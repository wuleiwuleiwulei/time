/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "napi_work.h"

#include "napi_utils.h"

namespace OHOS {
namespace MiscServices {
namespace Time {
ContextBase::~ContextBase()
{
    TIME_HILOGD(TIME_MODULE_JS_NAPI, "no memory leak after callback or promise[resolved/rejected]");
    if (env != nullptr) {
        if (work != nullptr) {
            napi_delete_async_work(env, work);
        }
        if (callbackRef != nullptr) {
            napi_delete_reference(env, callbackRef);
        }
        napi_delete_reference(env, selfRef);
        env = nullptr;
    }
}

void ContextBase::GetCbInfo(napi_env envi, napi_callback_info info, NapiCbInfoParser parser, bool sync)
{
    env = envi;
    size_t argc = ARGC_MAX;
    napi_value argv[ARGC_MAX] = { nullptr };
    status = napi_get_cb_info(env, info, &argc, argv, &self, nullptr);
    CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, this, "napi_get_cb_info failed!", JsErrorCode::PARAMETER_ERROR);
    CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, this, argc <= ARGC_MAX, "too many arguments!",
        JsErrorCode::PARAMETER_ERROR);
    CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, this, self != nullptr, "no JavaScript this argument!",
        JsErrorCode::PARAMETER_ERROR);
    napi_create_reference(env, self, 1, &selfRef);

    if (!sync && (argc > 0)) {
        // get the last arguments :: <callback>
        size_t index = argc - 1;
        napi_valuetype type = napi_undefined;
        napi_status tyst = napi_typeof(env, argv[index], &type);
        if ((tyst == napi_ok) && (type == napi_function)) {
            status = napi_create_reference(env, argv[index], 1, &callbackRef);
            CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, this, "ref callback failed!", JsErrorCode::PARAMETER_ERROR);
            argc = index;
            TIME_HILOGD(TIME_MODULE_JS_NAPI, "async callback, no promise");
        } else {
            TIME_HILOGD(TIME_MODULE_JS_NAPI, "no callback, async promise");
        }
    }

    if (parser) {
        parser(argc, argv);
    } else {
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, this, argc == 0, "required no arguments!",
            JsErrorCode::PARAMETER_ERROR);
    }
}

napi_value NapiWork::AsyncEnqueue(napi_env env, ContextBase *ctxt, const std::string &name,
    NapiExecute execute, NapiComplete complete)
{
    if (ctxt->status != napi_ok) {
        auto message = NapiUtils::GetErrorMessage(ctxt->errCode) + ". Error message: " + ctxt->errMessage;
        NapiUtils::ThrowError(env, message.c_str(), ctxt->errCode);
        delete ctxt;
        return NapiUtils::GetUndefinedValue(env);
    }
    ctxt->execute = std::move(execute);
    ctxt->complete = std::move(complete);
    napi_value promise = nullptr;
    if (ctxt->callbackRef == nullptr) {
        napi_create_promise(ctxt->env, &ctxt->deferred, &promise);
        TIME_HILOGD(TIME_MODULE_JS_NAPI, "create deferred promise");
    } else {
        napi_get_undefined(ctxt->env, &promise);
    }
    napi_value resource = nullptr;
    napi_create_string_utf8(ctxt->env, name.c_str(), NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(
        ctxt->env, nullptr, resource,
        [](napi_env env, void *data) {
            CHECK_RETURN_VOID(TIME_MODULE_JS_NAPI, data != nullptr, "napi_async_execute_callback nullptr");
            auto ctxt = reinterpret_cast<ContextBase *>(data);
            TIME_HILOGD(TIME_MODULE_JS_NAPI, "napi_async_execute_callback ctxt->status=%{public}d", ctxt->status);
            if (ctxt->execute != nullptr && ctxt->status == napi_ok) {
                ctxt->execute();
            }
        },
        [](napi_env env, napi_status status, void *data) {
            CHECK_RETURN_VOID(TIME_MODULE_JS_NAPI, data != nullptr, "napi_async_complete_callback nullptr");
            auto ctxt = reinterpret_cast<ContextBase *>(data);
            TIME_HILOGD(TIME_MODULE_JS_NAPI, "status=%{public}d, ctxt->status=%{public}d", status, ctxt->status);
            if ((status != napi_ok) && (ctxt->status == napi_ok)) {
                ctxt->status = status;
            }
            if ((ctxt->complete) && (status == napi_ok) && (ctxt->status == napi_ok)) {
                ctxt->complete(ctxt->output);
            }
            GenerateOutput(ctxt);
            delete ctxt;
        },
        reinterpret_cast<void *>(ctxt), &ctxt->work);
    auto ret = napi_queue_async_work_with_qos(ctxt->env, ctxt->work, napi_qos_user_initiated);
    if (ret != napi_ok) {
        napi_delete_async_work(env, ctxt->work);
        delete ctxt;
        TIME_SERVICE_NAPI_CALL(env, ret, ERROR, "napi_queue_async_work_with_qos failed");
    }
    return promise;
}

void NapiWork::GenerateOutput(ContextBase *ctxt)
{
    napi_value result[RESULT_ALL] = { nullptr };
    if (ctxt->status == napi_ok) {
        napi_get_undefined(ctxt->env, &result[RESULT_ERROR]);
        if (ctxt->output == nullptr) {
            napi_get_undefined(ctxt->env, &ctxt->output);
        }
        result[RESULT_DATA] = ctxt->output;
    } else {
        napi_value message = nullptr;
        napi_value code = nullptr;
        int32_t jsErrorCode = NapiUtils::ConvertErrorCode(ctxt->errCode);
        napi_create_string_utf8(ctxt->env, NapiUtils::GetErrorMessage(jsErrorCode).c_str(), NAPI_AUTO_LENGTH,
            &message);
        napi_create_error(ctxt->env, nullptr, message, &result[RESULT_ERROR]);
        if (jsErrorCode != JsErrorCode::ERROR) {
            napi_create_int32(ctxt->env, jsErrorCode, &code);
            napi_set_named_property(ctxt->env, result[RESULT_ERROR], "code", code);
        }
        napi_get_undefined(ctxt->env, &result[RESULT_DATA]);
    }
    if (ctxt->deferred != nullptr) {
        if (ctxt->status == napi_ok) {
            TIME_HILOGD(TIME_MODULE_JS_NAPI, "deferred promise resolved");
            napi_resolve_deferred(ctxt->env, ctxt->deferred, result[RESULT_DATA]);
        } else {
            TIME_HILOGD(TIME_MODULE_JS_NAPI, "deferred promise rejected");
            napi_reject_deferred(ctxt->env, ctxt->deferred, result[RESULT_ERROR]);
        }
    } else {
        napi_value callback = nullptr;
        napi_get_reference_value(ctxt->env, ctxt->callbackRef, &callback);
        napi_value callbackResult = nullptr;
        TIME_HILOGD(TIME_MODULE_JS_NAPI, "call callback function");
        napi_call_function(ctxt->env, nullptr, callback, RESULT_ALL, result, &callbackResult);
    }
}

napi_value NapiWork::SyncEnqueue(napi_env env, ContextBase *ctxt, const std::string &name,
    NapiExecute execute, NapiComplete complete)
{
    if (ctxt->status != napi_ok) {
        auto message = NapiUtils::GetErrorMessage(ctxt->errCode) + ". Error message: " + ctxt->errMessage;
        NapiUtils::ThrowError(env, message.c_str(), ctxt->errCode);
        delete ctxt;
        return NapiUtils::GetUndefinedValue(env);
    }

    ctxt->execute = std::move(execute);
    ctxt->complete = std::move(complete);

    if (ctxt->execute != nullptr && ctxt->status == napi_ok) {
        ctxt->execute();
    }

    if (ctxt->complete != nullptr && (ctxt->status == napi_ok)) {
        ctxt->complete(ctxt->output);
    }

    return GenerateOutputSync(env, ctxt);
}

napi_value NapiWork::GenerateOutputSync(napi_env env, ContextBase *ctxt)
{
    napi_value result = nullptr;
    if (ctxt->status == napi_ok) {
        if (ctxt->output == nullptr) {
            napi_get_undefined(ctxt->env, &ctxt->output);
        }
        result = ctxt->output;
    } else {
        napi_value error = nullptr;
        napi_value message = nullptr;
        int32_t jsErrorCode = NapiUtils::ConvertErrorCode(ctxt->errCode);
        napi_create_string_utf8(ctxt->env, NapiUtils::GetErrorMessage(jsErrorCode).c_str(), NAPI_AUTO_LENGTH,
                                &message);
        napi_create_error(ctxt->env, nullptr, message, &error);
        if (jsErrorCode != JsErrorCode::ERROR) {
            napi_value code = nullptr;
            napi_create_int32(ctxt->env, jsErrorCode, &code);
            napi_set_named_property(ctxt->env, error, "code", code);
        }
        napi_throw(env, error);
    }
    delete ctxt;
    return result;
}
} // namespace Time
} // namespace MiscServices
} // namespace OHOS