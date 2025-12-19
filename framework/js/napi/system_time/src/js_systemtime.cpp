/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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

#include "napi_utils.h"
#include "time_common.h"
#include "time_service_client.h"

namespace OHOS {
namespace MiscServicesNapi {
using namespace OHOS::MiscServices;
using namespace OHOS::MiscServices::Time;
typedef struct AsyncContext {
    ~AsyncContext()
    {
        if (callbackRef != nullptr) {
            napi_delete_reference(env, callbackRef);
        }
    }
    napi_env env = nullptr;
    napi_async_work work = nullptr;
    int64_t time = INVALID_TIME;
    std::string timeZone = "";
    napi_deferred deferred = nullptr;
    napi_ref callbackRef = nullptr;
    bool isCallback = false;
    bool isOK = false;
    bool isNano = false;
    int32_t errorCode = E_TIME_OK;
    std::string message = "system error";
} AsyncContext;

void FreeWorkIfFail(napi_status status, napi_env env, AsyncContext *asyncContext)
{
    if (status != napi_ok) {
        napi_delete_async_work(env, asyncContext->work);
        delete asyncContext;
        TIME_SERVICE_NAPI_CALL_RETURN_VOID(env, status, ERROR, "free work failed");
    }
}

void TimePaddingAsyncCallbackInfo(const napi_env &env, AsyncContext *&asynccallbackinfo, const napi_ref &callback,
    napi_value &promise)
{
    if (callback) {
        asynccallbackinfo->callbackRef = callback;
        asynccallbackinfo->isCallback = true;
    } else {
        napi_deferred deferred = nullptr;
        TIME_SERVICE_NAPI_CALL_RETURN_VOID(env, napi_create_promise(env, &deferred, &promise), ERROR,
            "napi_create_promise failed");
        asynccallbackinfo->deferred = deferred;
        asynccallbackinfo->isCallback = false;
    }
}

napi_value JSSystemTimeSetTime(napi_env env, napi_callback_info info)
{
    size_t argc = SET_TIME_MAX_PARA;
    napi_value argv[SET_TIME_MAX_PARA] = { 0 };
    napi_value thisVar = nullptr;
    TIME_SERVICE_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, NULL), ERROR,
        "napi_get_cb_info failed");
    int64_t times = INVALID_TIME;
    napi_ref callback = nullptr;
    if (NapiUtils::ParseParametersBySetTime(env, argv, argc, times, callback) == nullptr) {
        return NapiUtils::GetUndefinedValue(env);
    }
    AsyncContext *asyncContext = new (std::nothrow) AsyncContext{ .env = env, .time = times };
    if (!asyncContext) {
        return NapiUtils::JSParaError(env, callback);
    }
    napi_value promise = nullptr;
    TimePaddingAsyncCallbackInfo(env, asyncContext, callback, promise);
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSSystemTimeSetTime", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            int32_t errorCode = E_TIME_OK;
            asyncContext->isOK = TimeServiceClient::GetInstance()->SetTime(asyncContext->time, errorCode);
            if (!asyncContext->isOK) {
                auto jsErrorCode = NapiUtils::ConvertErrorCode(errorCode);
                asyncContext->message = NapiUtils::GetErrorMessage(jsErrorCode);
                asyncContext->errorCode = JsErrorCode::ERROR;
            }
        },
        [](napi_env env, napi_status status, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            if (asyncContext == nullptr) {
                return;
            }
            CallbackPromiseInfo info{ asyncContext->callbackRef, asyncContext->deferred, asyncContext->isCallback,
                asyncContext->errorCode, asyncContext->message };
            napi_value result = 0;
            napi_get_null(env, &result);
            NapiUtils::ReturnCallbackPromise(env, info, result);
            napi_delete_async_work(env, asyncContext->work);
            delete asyncContext;
        },
        (void *)asyncContext, &asyncContext->work);
    bool isCallback = asyncContext->isCallback;
    FreeWorkIfFail(
        napi_queue_async_work_with_qos(env, asyncContext->work, napi_qos_user_initiated), env, asyncContext);
    return isCallback ? NapiUtils::NapiGetNull(env) : promise;
}

napi_value JSSystemTimeSetTimeZone(napi_env env, napi_callback_info info)
{
    size_t argc = SET_TIMEZONE_MAX_PARA;
    napi_value argv[SET_TIMEZONE_MAX_PARA] = { 0 };
    napi_value thisVar = nullptr;
    TIME_SERVICE_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, NULL), ERROR,
        "napi_get_cb_info failed");
    std::string timezoneId;
    napi_ref callback = nullptr;
    if (NapiUtils::ParseParametersBySetTimezone(env, argv, argc, timezoneId, callback) == nullptr) {
        return NapiUtils::GetUndefinedValue(env);
    }
    AsyncContext *asyncContext = new (std::nothrow) AsyncContext{ .env = env, .timeZone = timezoneId };
    if (!asyncContext) {
        return NapiUtils::JSParaError(env, callback);
    }
    napi_value promise = nullptr;
    TimePaddingAsyncCallbackInfo(env, asyncContext, callback, promise);
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSSystemTimeSetTimeZone", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            int32_t errorCode = E_TIME_OK;
            asyncContext->isOK = TimeServiceClient::GetInstance()->SetTimeZone(asyncContext->timeZone, errorCode);
            if (!asyncContext->isOK) {
                auto jsErrorCode = NapiUtils::ConvertErrorCode(errorCode);
                asyncContext->message = NapiUtils::GetErrorMessage(jsErrorCode).c_str();
                asyncContext->errorCode = JsErrorCode::ERROR;
            }
        },
        [](napi_env env, napi_status status, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            if (asyncContext == nullptr) {
                return;
            }
            CallbackPromiseInfo info{ asyncContext->callbackRef, asyncContext->deferred, asyncContext->isCallback,
                asyncContext->errorCode, asyncContext->message };
            napi_value result = 0;
            napi_get_null(env, &result);
            NapiUtils::ReturnCallbackPromise(env, info, result);
            napi_delete_async_work(env, asyncContext->work);
            delete asyncContext;
        },
        (void *)asyncContext, &asyncContext->work);
    bool isCallback = asyncContext->isCallback;
    FreeWorkIfFail(
        napi_queue_async_work_with_qos(env, asyncContext->work, napi_qos_user_initiated), env, asyncContext);
    return isCallback ? NapiUtils::NapiGetNull(env) : promise;
}

napi_value JSSystemTimeGetCurrentTime(napi_env env, napi_callback_info info)
{
    size_t argc = SET_TIMEZONE_MAX_PARA;
    napi_value argv[SET_TIMEZONE_MAX_PARA] = { 0 };
    napi_value thisVar = nullptr;
    TIME_SERVICE_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, NULL), ERROR,
        "napi_get_cb_info failed");
    napi_ref callback = nullptr;
    bool isNano = false;
    if (NapiUtils::ParseParametersGetNA(env, argv, argc, callback, &isNano) == nullptr) {
        return NapiUtils::GetUndefinedValue(env);
    }
    AsyncContext *asyncContext = new (std::nothrow) AsyncContext{ .env = env };
    if (!asyncContext) {
        return NapiUtils::JSParaError(env, callback);
    }
    napi_value promise = nullptr;
    TimePaddingAsyncCallbackInfo(env, asyncContext, callback, promise);
    asyncContext->isNano = isNano;
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSSystemTimeGetCurrentTime", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(env, nullptr, resource,
        [](napi_env env, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            if (asyncContext->isNano) {
                asyncContext->time = TimeServiceClient::GetInstance()->GetWallTimeNs();
            } else {
                asyncContext->time = TimeServiceClient::GetInstance()->GetWallTimeMs();
            }
        },
        [](napi_env env, napi_status status, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            if (asyncContext == nullptr) {
                return;
            }
            if (asyncContext->time < 0) {
                asyncContext->errorCode = JsErrorCode::ERROR;
            }
            CallbackPromiseInfo info{ asyncContext->callbackRef, asyncContext->deferred, asyncContext->isCallback,
                asyncContext->errorCode, asyncContext->message };
            napi_value result = nullptr;
            napi_create_int64(env, asyncContext->time, &result);
            NapiUtils::ReturnCallbackPromise(env, info, result);
            napi_delete_async_work(env, asyncContext->work);
            delete asyncContext;
        },
        (void *)asyncContext, &asyncContext->work);
    bool isCallback = asyncContext->isCallback;
    FreeWorkIfFail(
        napi_queue_async_work_with_qos(env, asyncContext->work, napi_qos_user_initiated), env, asyncContext);
    return isCallback ? NapiUtils::NapiGetNull(env) : promise;
}

napi_value JSSystemTimeGetRealActiveTime(napi_env env, napi_callback_info info)
{
    size_t argc = SET_TIMEZONE_MAX_PARA;
    napi_value argv[SET_TIMEZONE_MAX_PARA] = { 0 };
    napi_value thisVar = nullptr;
    TIME_SERVICE_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, NULL), ERROR,
        "napi_get_cb_info failed");
    napi_ref callback = nullptr;
    bool isNano = false;
    if (NapiUtils::ParseParametersGetNA(env, argv, argc, callback, &isNano) == nullptr) {
        return NapiUtils::GetUndefinedValue(env);
    }
    AsyncContext *asyncContext = new (std::nothrow) AsyncContext{ .env = env };
    if (!asyncContext) {
        return NapiUtils::JSParaError(env, callback);
    }
    napi_value promise = nullptr;
    TimePaddingAsyncCallbackInfo(env, asyncContext, callback, promise);
    asyncContext->isNano = isNano;
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSSystemTimeGetRealActiveTime", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(env, nullptr, resource,
        [](napi_env env, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            if (asyncContext->isNano) {
                asyncContext->time = TimeServiceClient::GetInstance()->GetMonotonicTimeNs();
            } else {
                asyncContext->time = TimeServiceClient::GetInstance()->GetMonotonicTimeMs();
            }
        },
        [](napi_env env, napi_status status, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            if (asyncContext == nullptr) {
                return;
            }
            if (asyncContext->time < 0) {
                asyncContext->errorCode = JsErrorCode::ERROR;
            }
            CallbackPromiseInfo info{ asyncContext->callbackRef, asyncContext->deferred, asyncContext->isCallback,
                asyncContext->errorCode, asyncContext->message };
            napi_value result = nullptr;
            napi_create_int64(env, asyncContext->time, &result);
            NapiUtils::ReturnCallbackPromise(env, info, result);
            napi_delete_async_work(env, asyncContext->work);
            delete asyncContext;
        },
        (void *)asyncContext, &asyncContext->work);
    bool isCallback = asyncContext->isCallback;
    FreeWorkIfFail(
        napi_queue_async_work_with_qos(env, asyncContext->work, napi_qos_user_initiated), env, asyncContext);
    return isCallback ? NapiUtils::NapiGetNull(env) : promise;
}

napi_value JSSystemTimeGetRealTime(napi_env env, napi_callback_info info)
{
    size_t argc = SET_TIMEZONE_MAX_PARA;
    napi_value argv[SET_TIMEZONE_MAX_PARA] = { 0 };
    napi_value thisVar = nullptr;
    TIME_SERVICE_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, NULL), ERROR,
        "napi_get_cb_info failed");
    napi_ref callback = nullptr;
    bool isNano = false;
    if (NapiUtils::ParseParametersGetNA(env, argv, argc, callback, &isNano) == nullptr) {
        return NapiUtils::GetUndefinedValue(env);
    }
    AsyncContext *asyncContext = new (std::nothrow) AsyncContext{ .env = env };
    if (!asyncContext) {
        return NapiUtils::JSParaError(env, callback);
    }
    napi_value promise = nullptr;
    TimePaddingAsyncCallbackInfo(env, asyncContext, callback, promise);
    asyncContext->isNano = isNano;
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSSystemTimeGetRealTime", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(env, nullptr, resource,
        [](napi_env env, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            if (asyncContext->isNano) {
                asyncContext->time = TimeServiceClient::GetInstance()->GetBootTimeNs();
            } else {
                asyncContext->time = TimeServiceClient::GetInstance()->GetBootTimeMs();
            }
        },
        [](napi_env env, napi_status status, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            if (asyncContext == nullptr) {
                return;
            }
            if (asyncContext->time < 0) {
                asyncContext->errorCode = JsErrorCode::ERROR;
            }
            CallbackPromiseInfo info{ asyncContext->callbackRef, asyncContext->deferred, asyncContext->isCallback,
                asyncContext->errorCode, asyncContext->message };
            napi_value result = nullptr;
            napi_create_int64(env, asyncContext->time, &result);
            NapiUtils::ReturnCallbackPromise(env, info, result);
            napi_delete_async_work(env, asyncContext->work);
            delete asyncContext;
        },
        (void *)asyncContext, &asyncContext->work);
    bool isCallback = asyncContext->isCallback;
    FreeWorkIfFail(
        napi_queue_async_work_with_qos(env, asyncContext->work, napi_qos_user_initiated), env, asyncContext);
    return isCallback ? NapiUtils::NapiGetNull(env) : promise;
}

napi_value JSSystemTimeGetDate(napi_env env, napi_callback_info info)
{
    size_t argc = SET_TIMEZONE_MAX_PARA;
    napi_value argv[SET_TIMEZONE_MAX_PARA] = { 0 };
    napi_value thisVar = nullptr;
    TIME_SERVICE_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, NULL), ERROR,
        "napi_get_cb_info failed");
    napi_ref callback = nullptr;
    if (NapiUtils::ParseParametersGet(env, argv, argc, callback) == nullptr) {
        return NapiUtils::GetUndefinedValue(env);
    }
    AsyncContext *asyncContext = new (std::nothrow) AsyncContext{ .env = env };
    if (!asyncContext) {
        return NapiUtils::JSParaError(env, callback);
    }
    napi_value promise = nullptr;
    TimePaddingAsyncCallbackInfo(env, asyncContext, callback, promise);
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSSystemTimeGetDate", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            asyncContext->time = TimeServiceClient::GetInstance()->GetWallTimeMs();
        },
        [](napi_env env, napi_status status, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            if (asyncContext == nullptr) {
                return;
            }
            if (asyncContext->time < 0) {
                asyncContext->errorCode = JsErrorCode::ERROR;
            }
            CallbackPromiseInfo info{ asyncContext->callbackRef, asyncContext->deferred, asyncContext->isCallback,
                asyncContext->errorCode, asyncContext->message };
            napi_value result = nullptr;
            napi_create_date(env, asyncContext->time, &result);
            NapiUtils::ReturnCallbackPromise(env, info, result);
            napi_delete_async_work(env, asyncContext->work);
            delete asyncContext;
        },
        (void *)asyncContext, &asyncContext->work);
    bool isCallback = asyncContext->isCallback;
    FreeWorkIfFail(
        napi_queue_async_work_with_qos(env, asyncContext->work, napi_qos_user_initiated), env, asyncContext);
    return isCallback ? NapiUtils::NapiGetNull(env) : promise;
}

napi_value JSSystemTimeGetTimeZone(napi_env env, napi_callback_info info)
{
    size_t argc = SET_TIMEZONE_MAX_PARA;
    napi_value argv[SET_TIMEZONE_MAX_PARA] = { 0 };
    napi_value thisVar = nullptr;
    TIME_SERVICE_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, NULL), ERROR,
        "napi_get_cb_info failed");
    napi_ref callback = nullptr;
    if (NapiUtils::ParseParametersGet(env, argv, argc, callback) == nullptr) {
        return NapiUtils::GetUndefinedValue(env);
    }
    AsyncContext *asyncContext = new (std::nothrow) AsyncContext{ .env = env };
    if (!asyncContext) {
        return NapiUtils::JSParaError(env, callback);
    }
    napi_value promise = nullptr;
    TimePaddingAsyncCallbackInfo(env, asyncContext, callback, promise);
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSSystemTimeGetTimeZone", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            asyncContext->timeZone = TimeServiceClient::GetInstance()->GetTimeZone();
        },
        [](napi_env env, napi_status status, void *data) {
            AsyncContext *asyncContext = (AsyncContext *)data;
            if (asyncContext == nullptr) {
                return;
            }
            if (asyncContext->timeZone == "") {
                asyncContext->errorCode = JsErrorCode::ERROR;
            }
            CallbackPromiseInfo info{ asyncContext->callbackRef, asyncContext->deferred, asyncContext->isCallback,
                asyncContext->errorCode, asyncContext->message };
            napi_value result = nullptr;
            napi_create_string_utf8(env, asyncContext->timeZone.c_str(), asyncContext->timeZone.length(), &result);
            NapiUtils::ReturnCallbackPromise(env, info, result);
            napi_delete_async_work(env, asyncContext->work);
            delete asyncContext;
        },
        (void *)asyncContext, &asyncContext->work);
    bool isCallback = asyncContext->isCallback;
    FreeWorkIfFail(
        napi_queue_async_work_with_qos(env, asyncContext->work, napi_qos_user_initiated), env, asyncContext);
    return isCallback ? NapiUtils::NapiGetNull(env) : promise;
}

EXTERN_C_START
napi_value SystemTimeExport(napi_env env, napi_value exports)
{
    static napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("setTime", JSSystemTimeSetTime),
        DECLARE_NAPI_FUNCTION("setDate", JSSystemTimeSetTime),
        DECLARE_NAPI_FUNCTION("setTimezone", JSSystemTimeSetTimeZone),
        DECLARE_NAPI_FUNCTION("getCurrentTime", JSSystemTimeGetCurrentTime),
        DECLARE_NAPI_FUNCTION("getRealActiveTime", JSSystemTimeGetRealActiveTime),
        DECLARE_NAPI_FUNCTION("getRealTime", JSSystemTimeGetRealTime),
        DECLARE_NAPI_FUNCTION("getDate", JSSystemTimeGetDate),
        DECLARE_NAPI_FUNCTION("getTimezone", JSSystemTimeGetTimeZone),
    };
    TIME_SERVICE_NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc), ERROR,
        "napi_define_properties failed");
    return exports;
}
EXTERN_C_END

static napi_module system_time_module = { .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = SystemTimeExport,
    .nm_modname = "systemTime",
    .nm_priv = ((void *)0),
    .reserved = { 0 } };

extern "C" __attribute__((constructor)) void SystemTimeRegister()
{
    napi_module_register(&system_time_module);
}
} // namespace MiscServicesNapi
} // namespace OHOS