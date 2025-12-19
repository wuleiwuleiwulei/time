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

#include "napi_system_timer.h"

#include "napi_utils.h"
#include "timer_type.h"

using namespace OHOS::MiscServices;

namespace OHOS {
namespace MiscServices {
namespace Time {
static constexpr size_t STR_MAX_LENGTH = 64;
ITimerInfoInstance::ITimerInfoInstance() : callbackInfo_{}
{
}

ITimerInfoInstance::~ITimerInfoInstance()
{
    auto *callback = new (std::nothrow) CallbackInfo(callbackInfo_.env, callbackInfo_.ref);
    if (callback == nullptr) {
        return;
    }
    ITimerInfoInstance::Call(callbackInfo_.env, reinterpret_cast<void *>(callback));
}

void ITimerInfoInstance::Call(napi_env env, void *data)
{
    auto task = [data]() {
        auto *callback = reinterpret_cast<CallbackInfo *>(data);
        if (callback != nullptr) {
            napi_delete_reference(callback->env, callback->ref);
            delete callback;
        }
    };
    auto ret = napi_send_event(env, task, napi_eprio_immediate, "time:systemTimer.createTimer");
    if (ret != 0) {
        delete static_cast<CallbackInfo *>(data);
        TIME_HILOGE(TIME_MODULE_JS_NAPI, "napi_send_event failed retCode:%{public}d", ret);
    }
}

void ITimerInfoInstance::OnTrigger()
{
    if (callbackInfo_.ref == nullptr) {
        return;
    }
    auto callbackInfo = callbackInfo_;

    auto task = [callbackInfo]() {
        TIME_HILOGD(TIME_MODULE_JS_NAPI, "timerCallback success");
        napi_value undefined = nullptr;
        napi_get_undefined(callbackInfo.env, &undefined);
        napi_value callback = nullptr;
        napi_get_reference_value(callbackInfo.env, callbackInfo.ref, &callback);
        napi_call_function(callbackInfo.env, undefined, callback, ARGC_ZERO, &undefined, &undefined);
    };
    auto ret = napi_send_event(callbackInfo.env, task, napi_eprio_immediate, "time:systemTimer.createTimer");
    if (ret != 0) {
        TIME_HILOGE(TIME_MODULE_JS_NAPI, "napi_send_event failed retCode:%{public}d", ret);
    }
}

void ITimerInfoInstance::SetCallbackInfo(const napi_env &env, const napi_ref &ref)
{
    callbackInfo_.env = env;
    callbackInfo_.ref = ref;
}

void ITimerInfoInstance::SetType(const int &_type)
{
    type = _type;
}

void ITimerInfoInstance::SetRepeat(bool _repeat)
{
    repeat = _repeat;
}
void ITimerInfoInstance::SetInterval(const uint64_t &_interval)
{
    interval = _interval;
}
void ITimerInfoInstance::SetWantAgent(std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> _wantAgent)
{
    wantAgent = _wantAgent;
}

napi_value NapiSystemTimer::SystemTimerInit(napi_env env, napi_value exports)
{
    napi_property_descriptor descriptors[] = {
        DECLARE_NAPI_STATIC_FUNCTION("createTimer", CreateTimer),
        DECLARE_NAPI_STATIC_FUNCTION("startTimer", StartTimer),
        DECLARE_NAPI_STATIC_FUNCTION("stopTimer", StopTimer),
        DECLARE_NAPI_STATIC_FUNCTION("destroyTimer", DestroyTimer),
        DECLARE_NAPI_PROPERTY("TIMER_TYPE_REALTIME", NapiUtils::CreateNapiNumber(env, 1 << TIMER_TYPE_REALTIME)),
        DECLARE_NAPI_PROPERTY("TIMER_TYPE_WAKEUP", NapiUtils::CreateNapiNumber(env, 1 << TIMER_TYPE_WAKEUP)),
        DECLARE_NAPI_PROPERTY("TIMER_TYPE_EXACT", NapiUtils::CreateNapiNumber(env, 1 << TIMER_TYPE_EXACT)),
        DECLARE_NAPI_PROPERTY("TIMER_TYPE_IDLE", NapiUtils::CreateNapiNumber(env, 1 << TIMER_TYPE_IDLE)),
    };

    napi_status status =
        napi_define_properties(env, exports, sizeof(descriptors) / sizeof(napi_property_descriptor), descriptors);
    if (status != napi_ok) {
        TIME_HILOGE(TIME_MODULE_JS_NAPI, "define manager properties failed, status=%{public}d", status);
        return NapiUtils::GetUndefinedValue(env);
    }
    return exports;
}

std::map<std::string, napi_valuetype> PARA_NAPI_TYPE_MAP = {
    { "name", napi_string },
    { "type", napi_number },
    { "repeat", napi_boolean },
    { "autoRestore", napi_boolean },
    { "interval", napi_number },
    { "wantAgent", napi_object },
    { "callback", napi_function },
};

std::map<std::string, std::string> NAPI_TYPE_STRING_MAP = {
    { "name", "string" },
    { "type", "number" },
    { "repeat", "boolean" },
    { "autoRestore", "boolean" },
    { "interval", "number" },
    { "wantAgent", "object" },
    { "callback", "function" },
};

void ParseTimerOptions(napi_env env, ContextBase *context, std::string paraType,
    const napi_value &value, std::shared_ptr<ITimerInfoInstance> &iTimerInfoInstance)
{
    napi_value result = nullptr;
    OHOS::AbilityRuntime::WantAgent::WantAgent *wantAgent = nullptr;
    napi_valuetype valueType = napi_undefined;
    napi_get_named_property(env, value, paraType.c_str(), &result);
    napi_typeof(env, result, &valueType);
    CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, valueType == PARA_NAPI_TYPE_MAP[paraType],
        paraType + ": incorrect parameter types, must be " + NAPI_TYPE_STRING_MAP[paraType],
        JsErrorCode::PARAMETER_ERROR);
    if (paraType == "type") {
        int type = 0;
        napi_get_value_int32(env, result, &type);
        iTimerInfoInstance->SetType(type);
    } else if (paraType == "repeat") {
        bool repeat = false;
        napi_get_value_bool(env, result, &repeat);
        iTimerInfoInstance->SetRepeat(repeat);
    } else if (paraType == "autoRestore") {
        bool autoRestore = false;
        napi_get_value_bool(env, result, &autoRestore);
        iTimerInfoInstance->SetAutoRestore(autoRestore);
    } else if (paraType == "interval") {
        int64_t interval = 0;
        napi_get_value_int64(env, result, &interval);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, interval >= 0,
            "interval number must >= 0.", JsErrorCode::PARAMETER_ERROR);
        iTimerInfoInstance->SetInterval((uint64_t)interval);
    } else if (paraType == "wantAgent") {
        napi_unwrap(env, result, (void **)&wantAgent);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, wantAgent != nullptr,
            "wantAgent can not be nullptr.", JsErrorCode::PARAMETER_ERROR);
        std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> sWantAgent =
            std::make_shared<OHOS::AbilityRuntime::WantAgent::WantAgent>(*wantAgent);
        iTimerInfoInstance->SetWantAgent(sWantAgent);
    } else if (paraType == "callback") {
        napi_ref onTriggerCallback;
        napi_create_reference(env, result, 1, &onTriggerCallback);
        iTimerInfoInstance->SetCallbackInfo(env, onTriggerCallback);
    } else if (paraType == "name") {
        std::string name = "";
        auto ret = NapiUtils::GetValue(env, result, name);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, ret == napi_ok,
            "The type of 'name' must be string", JsErrorCode::PARAMETER_ERROR);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, name.size() <= STR_MAX_LENGTH,
            "timer name must <= 64.", JsErrorCode::PARAMETER_ERROR);
        iTimerInfoInstance->SetName(name);
    }
}

void NapiSystemTimer::GetTimerOptions(const napi_env &env, ContextBase *context,
    const napi_value &value, std::shared_ptr<ITimerInfoInstance> &iTimerInfoInstance)
{
    bool hasProperty = false;

    // type: number
    napi_has_named_property(env, value, "type", &hasProperty);
    CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, hasProperty,
        "Mandatory parameters are left unspecified, type expected.", JsErrorCode::PARAMETER_ERROR);
    ParseTimerOptions(env, context, "type", value, iTimerInfoInstance);
    CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, context->errMessage, JsErrorCode::PARAMETER_ERROR);

    // repeat: boolean
    napi_has_named_property(env, value, "repeat", &hasProperty);
    CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, hasProperty,
        "Mandatory parameters are left unspecified, repeat expected.", JsErrorCode::PARAMETER_ERROR);
    ParseTimerOptions(env, context, "repeat", value, iTimerInfoInstance);
    CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, context->errMessage, JsErrorCode::PARAMETER_ERROR);

    // autoRestore?: boolean
    napi_has_named_property(env, value, "autoRestore", &hasProperty);
    if (hasProperty) {
        ParseTimerOptions(env, context, "autoRestore", value, iTimerInfoInstance);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, context->errMessage, JsErrorCode::PARAMETER_ERROR);
    }

    // interval?: number
    napi_has_named_property(env, value, "interval", &hasProperty);
    if (hasProperty) {
        ParseTimerOptions(env, context, "interval", value, iTimerInfoInstance);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, context->errMessage, JsErrorCode::PARAMETER_ERROR);
    }

    // wantAgent?: WantAgent
    napi_has_named_property(env, value, "wantAgent", &hasProperty);
    if (hasProperty) {
        ParseTimerOptions(env, context, "wantAgent", value, iTimerInfoInstance);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, context->errMessage, JsErrorCode::PARAMETER_ERROR);
    }

    // callback?: () => void
    napi_has_named_property(env, value, "callback", &hasProperty);
    if (hasProperty) {
        ParseTimerOptions(env, context, "callback", value, iTimerInfoInstance);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, context->errMessage, JsErrorCode::PARAMETER_ERROR);
    }

    // name?: string
    napi_has_named_property(env, value, "name", &hasProperty);
    if (hasProperty) {
        ParseTimerOptions(env, context, "name", value, iTimerInfoInstance);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, context, context->errMessage, JsErrorCode::PARAMETER_ERROR);
    }
}

napi_value NapiSystemTimer::CreateTimer(napi_env env, napi_callback_info info)
{
    struct CreateTimerContext : public ContextBase {
        uint64_t timerId = 0;
        std::shared_ptr<ITimerInfoInstance> iTimerInfoInstance = std::make_shared<ITimerInfoInstance>();
    };
    CreateTimerContext *createTimerContext = new CreateTimerContext();
    auto inputParser = [env, createTimerContext](size_t argc, napi_value *argv) {
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, createTimerContext, argc >= ARGC_ONE,
            "Mandatory parameters are left unspecified", JsErrorCode::PARAMETER_ERROR);
        GetTimerOptions(env, createTimerContext, argv[ARGV_FIRST], createTimerContext->iTimerInfoInstance);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, createTimerContext, createTimerContext->status == napi_ok,
            createTimerContext->errMessage, JsErrorCode::PARAMETER_ERROR);
        createTimerContext->status = napi_ok;
    };
    createTimerContext->GetCbInfo(env, info, inputParser);
    auto executor = [createTimerContext]() {
        auto innerCode = TimeServiceClient::GetInstance()->CreateTimerV9(createTimerContext->iTimerInfoInstance,
            createTimerContext->timerId);
        if (innerCode != JsErrorCode::ERROR_OK) {
            createTimerContext->errCode = innerCode;
            createTimerContext->status = napi_generic_failure;
        }
    };
    auto complete = [createTimerContext](napi_value &output) {
        uint64_t timerId = static_cast<uint64_t>(createTimerContext->timerId);
        createTimerContext->status = napi_create_int64(createTimerContext->env, timerId, &output);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, createTimerContext,
            "convert native object to javascript object failed", ERROR);
    };
    return NapiWork::AsyncEnqueue(env, createTimerContext, "SetTime", executor, complete);
}

napi_value NapiSystemTimer::StartTimer(napi_env env, napi_callback_info info)
{
    struct StartTimerContext : public ContextBase {
        uint64_t timerId = 0;
        uint64_t triggerTime = 0;
    };
    StartTimerContext *startTimerContext = new StartTimerContext();
    auto inputParser = [env, startTimerContext](size_t argc, napi_value *argv) {
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, startTimerContext, argc >= ARGC_TWO,
            "Mandatory parameters are left unspecified", JsErrorCode::PARAMETER_ERROR);
        int64_t timerId = 0;
        startTimerContext->status = napi_get_value_int64(env, argv[ARGV_FIRST], &timerId);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, startTimerContext, startTimerContext->status == napi_ok,
            "The type of 'timerId' must be number", JsErrorCode::PARAMETER_ERROR);
        startTimerContext->timerId = static_cast<uint64_t>(timerId);
        int64_t triggerTime = 0;
        startTimerContext->status = napi_get_value_int64(env, argv[ARGV_SECOND], &triggerTime);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, startTimerContext, startTimerContext->status == napi_ok,
            "The type of 'triggerTime' must be number", JsErrorCode::PARAMETER_ERROR);
        startTimerContext->triggerTime = static_cast<uint64_t>(triggerTime);
        startTimerContext->status = napi_ok;
    };
    startTimerContext->GetCbInfo(env, info, inputParser);
    auto executor = [startTimerContext]() {
        auto innerCode =
            TimeServiceClient::GetInstance()->StartTimerV9(startTimerContext->timerId, startTimerContext->triggerTime);
        if (innerCode != JsErrorCode::ERROR_OK) {
            startTimerContext->errCode = innerCode;
            startTimerContext->status = napi_generic_failure;
        }
    };
    auto complete = [env](napi_value &output) { output = NapiUtils::GetUndefinedValue(env); };
    return NapiWork::AsyncEnqueue(env, startTimerContext, "StartTimer", executor, complete);
}

napi_value NapiSystemTimer::StopTimer(napi_env env, napi_callback_info info)
{
    struct StopTimerContext : public ContextBase {
        uint64_t timerId = 0;
    };
    StopTimerContext *stopTimerContext = new StopTimerContext();
    auto inputParser = [env, stopTimerContext](size_t argc, napi_value *argv) {
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, stopTimerContext, argc >= ARGC_ONE,
            "Mandatory parameters are left unspecified", JsErrorCode::PARAMETER_ERROR);
        int64_t timerId = 0;
        stopTimerContext->status = napi_get_value_int64(env, argv[ARGV_FIRST], &timerId);
        stopTimerContext->timerId = static_cast<uint64_t>(timerId);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, stopTimerContext, stopTimerContext->status == napi_ok,
            "The type of 'timerId' must be number", JsErrorCode::PARAMETER_ERROR);
        stopTimerContext->status = napi_ok;
    };
    stopTimerContext->GetCbInfo(env, info, inputParser);
    auto executor = [stopTimerContext]() {
        auto innerCode = TimeServiceClient::GetInstance()->StopTimerV9(stopTimerContext->timerId);
        if (innerCode != JsErrorCode::ERROR_OK) {
            stopTimerContext->errCode = innerCode;
            stopTimerContext->status = napi_generic_failure;
        }
    };
    auto complete = [env](napi_value &output) { output = NapiUtils::GetUndefinedValue(env); };
    return NapiWork::AsyncEnqueue(env, stopTimerContext, "StopTimer", executor, complete);
}

napi_value NapiSystemTimer::DestroyTimer(napi_env env, napi_callback_info info)
{
    struct DestroyTimerContext : public ContextBase {
        uint64_t timerId = 0;
    };
    DestroyTimerContext *destroyTimerContext = new DestroyTimerContext();
    auto inputParser = [env, destroyTimerContext](size_t argc, napi_value *argv) {
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, destroyTimerContext, argc == ARGC_ONE,
            "Mandatory parameters are left unspecified", JsErrorCode::PARAMETER_ERROR);
        int64_t timerId = 0;
        destroyTimerContext->status = napi_get_value_int64(env, argv[ARGV_FIRST], &timerId);
        destroyTimerContext->timerId = static_cast<uint64_t>(timerId);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, destroyTimerContext, destroyTimerContext->status == napi_ok,
            "The type of 'timerId' must be number", JsErrorCode::PARAMETER_ERROR);
        destroyTimerContext->status = napi_ok;
    };
    destroyTimerContext->GetCbInfo(env, info, inputParser);
    auto executor = [destroyTimerContext]() {
        auto innerCode = TimeServiceClient::GetInstance()->DestroyTimerV9(destroyTimerContext->timerId);
        if (innerCode != ERROR_OK) {
            destroyTimerContext->errCode = innerCode;
            destroyTimerContext->status = napi_generic_failure;
        }
    };
    auto complete = [env](napi_value &output) { output = NapiUtils::GetUndefinedValue(env); };

    return NapiWork::AsyncEnqueue(env, destroyTimerContext, "DestroyTimer", executor, complete);
}
} // namespace Time
} // namespace MiscServices
} // namespace OHOS