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

#include "napi_utils.h"

#include "time_common.h"

namespace OHOS {
namespace MiscServices {
namespace Time {
static constexpr int32_t STR_MAX_LENGTH = 4096;
static constexpr size_t STR_TAIL_LENGTH = 1;
static constexpr const char *SYSTEM_ERROR = "system error";
int32_t NapiUtils::ConvertErrorCode(int32_t timeErrorCode)
{
    switch (timeErrorCode) {
        case MiscServices::E_TIME_NOT_SYSTEM_APP:
            return JsErrorCode::SYSTEM_APP_ERROR;
        case MiscServices::E_TIME_NO_PERMISSION:
            return JsErrorCode::PERMISSION_ERROR;
        case MiscServices::E_TIME_PARAMETERS_INVALID:
            return JsErrorCode::PARAMETER_ERROR;
        case MiscServices::E_TIME_NTP_UPDATE_FAILED:
            return JsErrorCode::NTP_UPDATE_ERROR;
        case MiscServices::E_TIME_NTP_NOT_UPDATE:
            return JsErrorCode::NTP_NOT_UPDATE_ERROR;
        default:
            return JsErrorCode::ERROR;
    }
}

napi_value NapiUtils::CreateNapiNumber(napi_env env, int32_t objName)
{
    napi_value prop = nullptr;
    napi_create_int32(env, objName, &prop);
    return prop;
}

napi_value NapiUtils::GetUndefinedValue(napi_env env)
{
    napi_value result{};
    napi_get_undefined(env, &result);
    return result;
}

napi_status NapiUtils::GetValue(napi_env env, napi_value in, std::string &out)
{
    napi_valuetype type = napi_undefined;
    napi_status status = napi_typeof(env, in, &type);
    CHECK_RETURN(TIME_MODULE_JS_NAPI, (status == napi_ok) && (type == napi_string), "invalid type", napi_invalid_arg);
    size_t maxLen = STR_MAX_LENGTH;
    napi_get_value_string_utf8(env, in, nullptr, 0, &maxLen);
    if (maxLen >= STR_MAX_LENGTH) {
        return napi_invalid_arg;
    }
    char buf[STR_MAX_LENGTH + STR_TAIL_LENGTH]{};
    size_t len = 0;
    status = napi_get_value_string_utf8(env, in, buf, maxLen + STR_TAIL_LENGTH, &len);
    if (status == napi_ok) {
        out = std::string(buf);
    }
    return status;
}

napi_status NapiUtils::ThrowError(napi_env env, const char *napiMessage, int32_t napiCode)
{
    napi_value message = nullptr;
    napi_value code = nullptr;
    napi_value result = nullptr;
    napi_create_string_utf8(env, napiMessage, NAPI_AUTO_LENGTH, &message);
    napi_create_error(env, nullptr, message, &result);
    napi_create_int32(env, napiCode, &code);
    napi_set_named_property(env, result, "code", code);
    napi_throw(env, result);
    return napi_ok;
}

std::string NapiUtils::GetErrorMessage(int32_t errCode)
{
    auto it = CODE_TO_MESSAGE.find(errCode);
    if (it != CODE_TO_MESSAGE.end()) {
        return it->second;
    }
    return std::string(SYSTEM_ERROR);
}

napi_value NapiUtils::GetCallbackErrorValue(napi_env env, int32_t errCode, const std::string &message)
{
    napi_value result = nullptr;
    napi_value eCode = nullptr;
    if (errCode == JsErrorCode::ERROR_OK) {
        napi_get_undefined(env, &result);
        return result;
    }
    TIME_SERVICE_NAPI_CALL(env, napi_create_object(env, &result), ERROR, "napi_create_object failed");
    if (errCode == JsErrorCode::ERROR) {
        TIME_SERVICE_NAPI_CALL(env, napi_create_int32(env, errCode, &eCode), ERROR, "napi_create_int32 failed");
        TIME_SERVICE_NAPI_CALL(env, napi_set_named_property(env, result, "code", eCode), ERROR,
            "napi_set_named_property failed");
        napi_value str;
        size_t str_len = strlen(message.c_str());
        TIME_SERVICE_NAPI_CALL(env, napi_create_string_utf8(env, message.c_str(), str_len, &str), ERROR,
            "napi_create_string_utf8 failed");
        TIME_SERVICE_NAPI_CALL(env, napi_set_named_property(env, result, "message", str), ERROR,
            "napi_set_named_property failed");
    }
    return result;
}

napi_value NapiUtils::NapiGetNull(napi_env env)
{
    napi_value result = nullptr;
    napi_get_null(env, &result);
    return result;
}

void NapiUtils::SetPromise(napi_env env, napi_deferred deferred, int32_t errorCode, const std::string &message,
    napi_value result)
{
    if (errorCode == JsErrorCode::ERROR_OK) {
        TIME_SERVICE_NAPI_CALL_RETURN_VOID(env, napi_resolve_deferred(env, deferred, result), ERROR,
            "napi_resolve_deferred failed");
        return;
    }
    TIME_SERVICE_NAPI_CALL_RETURN_VOID(env, napi_reject_deferred(env, deferred,
        GetCallbackErrorValue(env, errorCode, message)), ERROR, "napi_reject_deferred failed");
}

void NapiUtils::SetCallback(napi_env env, napi_ref callbackIn, int32_t errorCode, const std::string &message,
    napi_value result)
{
    napi_value undefined = nullptr;
    napi_get_undefined(env, &undefined);

    napi_value callback = nullptr;
    napi_value resultOut = nullptr;
    napi_get_reference_value(env, callbackIn, &callback);
    napi_value results[2] = { 0 };
    results[0] = GetCallbackErrorValue(env, errorCode, message);
    results[1] = result;
    TIME_SERVICE_NAPI_CALL_RETURN_VOID(env,
        napi_call_function(env, undefined, callback, ARGC_TWO, &results[0], &resultOut), ERROR,
        "napi_call_function failed");
}

void NapiUtils::ReturnCallbackPromise(napi_env env, const CallbackPromiseInfo &info, napi_value result)
{
    if (info.isCallback) {
        SetCallback(env, info.callback, info.errorCode, info.message, result);
    } else {
        SetPromise(env, info.deferred, info.errorCode, info.message, result);
    }
}

napi_value NapiUtils::JSParaError(napi_env env, napi_ref callback)
{
    if (callback) {
        return GetCallbackErrorValue(env, ERROR, NapiUtils::GetErrorMessage(JsErrorCode::PARAMETER_ERROR));
    } else {
        napi_value promise = nullptr;
        napi_deferred deferred = nullptr;
        napi_create_promise(env, &deferred, &promise);
        SetPromise(env, deferred, ERROR, NapiUtils::GetErrorMessage(JsErrorCode::PARAMETER_ERROR), NapiGetNull(env));
        return promise;
    }
}

napi_value NapiUtils::ParseParametersBySetTime(napi_env env, const napi_value (&argv)[SET_TIME_MAX_PARA], size_t argc,
    int64_t &times, napi_ref &callback)
{
    NAPI_ASSERTP_RETURN(env, argc >= SET_TIME_MAX_PARA - 1, "Wrong number of arguments");
    napi_valuetype valueType = napi_undefined;

    // argv[0]: times or date object
    TIME_SERVICE_NAPI_CALL(env, napi_typeof(env, argv[0], &valueType), ERROR, "napi_typeof failed");
    NAPI_ASSERTP_RETURN(env, valueType == napi_number || valueType == napi_object,
                        "Parameter error. The type of time must be number or date.");
    if (valueType == napi_number) {
        napi_get_value_int64(env, argv[0], &times);
        NAPI_ASSERTP_RETURN(env, times >= 0, "Wrong argument timer. Positive number expected.");
    } else {
        bool hasProperty = false;
        napi_valuetype resValueType = napi_undefined;
        TIME_SERVICE_NAPI_CALL(env, napi_has_named_property(env, argv[0], "getTime", &hasProperty), ERROR,
            "napi_has_named_property failed");
        NAPI_ASSERTP_RETURN(env, hasProperty, "type expected.");
        napi_value getTimeFunc = nullptr;
        napi_get_named_property(env, argv[0], "getTime", &getTimeFunc);
        napi_value getTimeResult = nullptr;
        napi_call_function(env, argv[0], getTimeFunc, 0, nullptr, &getTimeResult);
        TIME_SERVICE_NAPI_CALL(env, napi_typeof(env, getTimeResult, &resValueType), ERROR, "napi_typeof failed");
        NAPI_ASSERTP_RETURN(env, resValueType == napi_number, "type mismatch");
        napi_get_value_int64(env, getTimeResult, &times);
    }

    // argv[1]:callback
    if (argc >= SET_TIME_MAX_PARA) {
        TIME_SERVICE_NAPI_CALL(env, napi_typeof(env, argv[1], &valueType), ERROR, "napi_typeof failed");
        NAPI_ASSERTP_RETURN(env, valueType == napi_function,
            "Parameter error. The type of callback must be function.");
        napi_create_reference(env, argv[1], 1, &callback);
    }
    return NapiGetNull(env);
}

napi_value NapiUtils::ParseParametersBySetTimezone(napi_env env, const napi_value (&argv)[SET_TIMEZONE_MAX_PARA],
    size_t argc, std::string &timezoneId, napi_ref &callback)
{
    NAPI_ASSERTP_RETURN(env, argc >= SET_TIMEZONE_MAX_PARA - 1, "Wrong number of arguments");
    napi_valuetype valueType = napi_undefined;

    // argv[0]: timezoneid
    TIME_SERVICE_NAPI_CALL(env, napi_typeof(env, argv[0], &valueType), ERROR, "napi_typeof failed");
    NAPI_ASSERTP_RETURN(env, valueType == napi_string, "Parameter error. The type of timezone must be string.");
    char timeZoneChars[MAX_TIME_ZONE_ID];
    size_t copied;
    napi_get_value_string_utf8(env, argv[0], timeZoneChars, MAX_TIME_ZONE_ID - 1, &copied);
    TIME_HILOGD(TIME_MODULE_JNI, "timezone str: %{public}s", timeZoneChars);

    timezoneId = std::string(timeZoneChars);

    // argv[1]:callback
    if (argc >= SET_TIMEZONE_MAX_PARA) {
        TIME_SERVICE_NAPI_CALL(env, napi_typeof(env, argv[1], &valueType), ERROR, "napi_typeof failed");
        NAPI_ASSERTP_RETURN(env, valueType == napi_function,
            "Parameter error. The type of callback must be function.");
        napi_create_reference(env, argv[1], 1, &callback);
    }
    return NapiGetNull(env);
}

napi_value NapiUtils::ParseParametersGet(napi_env env, const napi_value (&argv)[SET_TIMEZONE_MAX_PARA], size_t argc,
    napi_ref &callback)
{
    napi_valuetype valueType = napi_undefined;
    if (argc == 1) {
        TIME_SERVICE_NAPI_CALL(env, napi_typeof(env, argv[0], &valueType), ERROR, "napi_typeof failed");
        NAPI_ASSERTP_RETURN(env, valueType == napi_function,
            "Parameter error. The type of callback must be function.");
        napi_create_reference(env, argv[0], 1, &callback);
    }
    return NapiGetNull(env);
}

napi_value NapiUtils::ParseParametersGetNA(napi_env env, const napi_value (&argv)[SET_TIMEZONE_MAX_PARA], size_t argc,
    napi_ref &callback, bool *isNano)
{
    napi_valuetype valueType = napi_undefined;
    if (argc == 1) {
        TIME_SERVICE_NAPI_CALL(env, napi_typeof(env, argv[0], &valueType), ERROR, "napi_typeof failed");
        if (valueType == napi_function) {
            napi_create_reference(env, argv[0], 1, &callback);
        } else if (valueType == napi_boolean) {
            napi_get_value_bool(env, argv[0], isNano);
        }
    } else if (argc == ARGC_TWO) {
        napi_get_value_bool(env, argv[0], isNano);
        napi_create_reference(env, argv[1], 1, &callback);
    }
    return NapiGetNull(env);
}
} // namespace Time
} // namespace MiscServices
} // namespace OHOS