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

#include "napi_system_date_time.h"

#include "parameters.h"
#include "napi_work.h"
#include "napi_utils.h"
#include "time_common.h"
#include "time_service_client.h"

using namespace OHOS::MiscServices;

namespace OHOS {
namespace MiscServices {
namespace Time {

constexpr int64_t SECONDS_TO_NANO = 1000000000;
constexpr int64_t SECONDS_TO_MILLI = 1000;
constexpr int64_t NANO_TO_MILLI = SECONDS_TO_NANO / SECONDS_TO_MILLI;
constexpr int32_t STARTUP = 0;
constexpr int32_t ACTIVE = 1;
constexpr const char *TIMEZONE_KEY = "persist.time.timezone";
constexpr const char *AUTOTIME_KEY = "persist.time.auto_time";

napi_value NapiSystemDateTime::SystemDateTimeInit(napi_env env, napi_value exports)
{
    napi_value timeType = nullptr;
    napi_value startup = nullptr;
    napi_value active = nullptr;
    TIME_SERVICE_NAPI_CALL(env, napi_create_int32(env, STARTUP, &startup), ERROR, "napi_create_int32 failed");
    TIME_SERVICE_NAPI_CALL(env, napi_create_int32(env, ACTIVE, &active), ERROR, "napi_create_int32 failed");
    TIME_SERVICE_NAPI_CALL(env, napi_create_object(env, &timeType), ERROR, "napi_create_object failed");
    TIME_SERVICE_NAPI_CALL(env, napi_set_named_property(env, timeType, "STARTUP", startup), ERROR,
        "napi_set_named_property failed");
    TIME_SERVICE_NAPI_CALL(env, napi_set_named_property(env, timeType, "ACTIVE", active), ERROR,
        "napi_set_named_property failed");
    napi_property_descriptor descriptors[] = {
        DECLARE_NAPI_STATIC_FUNCTION("setTime", SetTime),
        DECLARE_NAPI_STATIC_FUNCTION("getCurrentTime", GetCurrentTime),
        DECLARE_NAPI_STATIC_FUNCTION("getRealActiveTime", GetRealActiveTime),
        DECLARE_NAPI_STATIC_FUNCTION("getRealTime", GetRealTime),
        DECLARE_NAPI_STATIC_FUNCTION("getTime", GetTime),
        DECLARE_NAPI_STATIC_FUNCTION("updateNtpTime", UpdateNtpTime),
        DECLARE_NAPI_STATIC_FUNCTION("getNtpTime", GetNtpTime),
        DECLARE_NAPI_STATIC_FUNCTION("setDate", SetDate),
        DECLARE_NAPI_STATIC_FUNCTION("getDate", GetDate),
        DECLARE_NAPI_STATIC_FUNCTION("setTimezone", SetTimezone),
        DECLARE_NAPI_STATIC_FUNCTION("getTimezone", GetTimezone),
        DECLARE_NAPI_STATIC_FUNCTION("getUptime", GetUptime),
        DECLARE_NAPI_STATIC_FUNCTION("getTimezoneSync", GetTimezoneSync),
        DECLARE_NAPI_STATIC_FUNCTION("getAutoTimeStatus", GetAutoTimeStatus),
        DECLARE_NAPI_STATIC_FUNCTION("setAutoTimeStatus", SetAutoTimeStatus),
        DECLARE_NAPI_STATIC_PROPERTY("TimeType", timeType),
    };
    napi_status status =
        napi_define_properties(env, exports, sizeof(descriptors) / sizeof(napi_property_descriptor), descriptors);
    if (status != napi_ok) {
        TIME_HILOGE(TIME_MODULE_JS_NAPI, "define manager properties failed, status=%{public}d", status);
        return NapiUtils::GetUndefinedValue(env);
    }
    return exports;
}

napi_value NapiSystemDateTime::SetTime(napi_env env, napi_callback_info info)
{
    struct SetTimeContext : public ContextBase {
        int64_t time = 0;
    };
    auto *setTimeContext = new SetTimeContext();
    auto inputParser = [env, setTimeContext](size_t argc, napi_value *argv) {
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setTimeContext, argc >= ARGC_ONE,
            "Mandatory parameters are left unspecified", JsErrorCode::PARAMETER_ERROR);
        setTimeContext->status = napi_get_value_int64(env, argv[ARGV_FIRST], &setTimeContext->time);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setTimeContext, setTimeContext->status == napi_ok,
            "The type of 'time' must be number", JsErrorCode::PARAMETER_ERROR);
        setTimeContext->status = napi_ok;
    };
    setTimeContext->GetCbInfo(env, info, inputParser);
    auto executor = [setTimeContext]() {
        auto innerCode = TimeServiceClient::GetInstance()->SetTimeV9(setTimeContext->time);
        if (innerCode != JsErrorCode::ERROR_OK) {
            setTimeContext->errCode = innerCode;
            setTimeContext->status = napi_generic_failure;
        }
    };
    auto complete = [env](napi_value &output) { output = NapiUtils::GetUndefinedValue(env); };
    return NapiWork::AsyncEnqueue(env, setTimeContext, "SetTime", executor, complete);
}

napi_value NapiSystemDateTime::SetDate(napi_env env, napi_callback_info info)
{
    struct SetDateContext : public ContextBase {
        int64_t time = 0;
    };
    auto *setDateContext = new SetDateContext();
    auto inputParser = [env, setDateContext](size_t argc, napi_value *argv) {
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setDateContext, argc >= ARGC_ONE,
            "Mandatory parameters are left unspecified", JsErrorCode::PARAMETER_ERROR);
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[ARGV_FIRST], &valueType);
        if (valueType == napi_number) {
            napi_get_value_int64(env, argv[ARGV_FIRST], &setDateContext->time);
            CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setDateContext, setDateContext->time >= 0,
                "date number must >= 0", JsErrorCode::PARAMETER_ERROR);
        } else {
            bool hasProperty = false;
            napi_valuetype resValueType = napi_undefined;
            napi_has_named_property(env, argv[ARGV_FIRST], "getTime", &hasProperty);
            CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setDateContext, hasProperty,
                "The type of 'date' must be Date", JsErrorCode::PARAMETER_ERROR);
            napi_value getTimeFunc = nullptr;
            napi_get_named_property(env, argv[0], "getTime", &getTimeFunc);
            napi_value getTimeResult = nullptr;
            napi_call_function(env, argv[0], getTimeFunc, 0, nullptr, &getTimeResult);
            napi_typeof(env, getTimeResult, &resValueType);
            CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setDateContext, resValueType == napi_number,
                "The type of 'date' must be Date", JsErrorCode::PARAMETER_ERROR);
            setDateContext->status = napi_get_value_int64(env, getTimeResult, &setDateContext->time);
        }
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setDateContext, setDateContext->status == napi_ok,
            "The type of 'date' must be Date", JsErrorCode::PARAMETER_ERROR);
        setDateContext->status = napi_ok;
    };
    setDateContext->GetCbInfo(env, info, inputParser);
    auto executor = [setDateContext]() {
        auto innerCode = TimeServiceClient::GetInstance()->SetTimeV9(setDateContext->time);
        if (innerCode != JsErrorCode::ERROR_OK) {
            setDateContext->errCode = innerCode;
            setDateContext->status = napi_generic_failure;
        }
    };
    auto complete = [env](napi_value &output) { output = NapiUtils::GetUndefinedValue(env); };
    return NapiWork::AsyncEnqueue(env, setDateContext, "SetDate", executor, complete);
}

napi_value NapiSystemDateTime::GetRealActiveTime(napi_env env, napi_callback_info info)
{
    struct GetRealActiveTimeContext : public ContextBase {
        int64_t time = 0;
        bool isNano = false;
    };
    auto *getRealActiveTimeContext = new GetRealActiveTimeContext();
    auto inputParser = [env, getRealActiveTimeContext](size_t argc, napi_value *argv) {
        if (argc >= ARGC_ONE) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[ARGV_FIRST], &valueType);
            if (valueType == napi_boolean) {
                getRealActiveTimeContext->status =
                    napi_get_value_bool(env, argv[ARGV_FIRST], &getRealActiveTimeContext->isNano);
                CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, getRealActiveTimeContext,
                    getRealActiveTimeContext->status == napi_ok, "invalid isNano", JsErrorCode::PARAMETER_ERROR);
            }
        }
        getRealActiveTimeContext->status = napi_ok;
    };
    getRealActiveTimeContext->GetCbInfo(env, info, inputParser);
    auto executor = [getRealActiveTimeContext]() {
        int32_t innerCode;
        if (getRealActiveTimeContext->isNano) {
            innerCode = TimeServiceClient::GetInstance()->GetMonotonicTimeNs(getRealActiveTimeContext->time);
        } else {
            innerCode = TimeServiceClient::GetInstance()->GetMonotonicTimeMs(getRealActiveTimeContext->time);
        }
        if (innerCode != JsErrorCode::ERROR_OK) {
            getRealActiveTimeContext->errCode = NapiUtils::ConvertErrorCode(innerCode);
            getRealActiveTimeContext->status = napi_generic_failure;
        }
    };
    auto complete = [env, getRealActiveTimeContext](napi_value &output) {
        getRealActiveTimeContext->status = napi_create_int64(env, getRealActiveTimeContext->time, &output);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, getRealActiveTimeContext,
            "convert native object to javascript object failed", JsErrorCode::ERROR);
    };
    return NapiWork::AsyncEnqueue(env, getRealActiveTimeContext, "GetRealActiveTime", executor, complete);
}

napi_value NapiSystemDateTime::GetCurrentTime(napi_env env, napi_callback_info info)
{
    struct GetCurrentTimeContext : public ContextBase {
        int64_t time = 0;
        bool isNano = false;
    };
    auto *getCurrentTimeContext = new GetCurrentTimeContext();
    auto inputParser = [env, getCurrentTimeContext](size_t argc, napi_value *argv) {
        if (argc >= ARGC_ONE) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[ARGV_FIRST], &valueType);
            if (valueType == napi_boolean) {
                getCurrentTimeContext->status =
                    napi_get_value_bool(env, argv[ARGV_FIRST], &getCurrentTimeContext->isNano);
                CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, getCurrentTimeContext,
                    getCurrentTimeContext->status == napi_ok, "invalid isNano", JsErrorCode::PARAMETER_ERROR);
            }
        }
        getCurrentTimeContext->status = napi_ok;
    };
    getCurrentTimeContext->GetCbInfo(env, info, inputParser);
    auto executor = [getCurrentTimeContext]() {
        int32_t innerCode;
        if (getCurrentTimeContext->isNano) {
            innerCode = TimeServiceClient::GetInstance()->GetWallTimeNs(getCurrentTimeContext->time);
        } else {
            innerCode = TimeServiceClient::GetInstance()->GetWallTimeMs(getCurrentTimeContext->time);
        }
        if (innerCode != JsErrorCode::ERROR_OK) {
            getCurrentTimeContext->errCode = innerCode;
            getCurrentTimeContext->status = napi_generic_failure;
        }
    };
    auto complete = [getCurrentTimeContext, env](napi_value &output) {
        getCurrentTimeContext->status = napi_create_int64(env, getCurrentTimeContext->time, &output);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, getCurrentTimeContext,
            "convert native object to javascript object failed", JsErrorCode::ERROR);
    };
    return NapiWork::AsyncEnqueue(env, getCurrentTimeContext, "GetCurrentTime", executor, complete);
}

napi_value NapiSystemDateTime::GetTime(napi_env env, napi_callback_info info)
{
    struct GetTimeContext : public ContextBase {
        int64_t time = 0;
        bool isNano = false;
    };
    auto *getTimeContext = new GetTimeContext();
    auto inputParser = [env, getTimeContext](size_t argc, napi_value *argv) {
        if (argc >= ARGC_ONE) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[ARGV_FIRST], &valueType);
            if (valueType == napi_boolean) {
                getTimeContext->status = napi_get_value_bool(env, argv[ARGV_FIRST], &getTimeContext->isNano);
                CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, getTimeContext, getTimeContext->status == napi_ok,
                                       "invalid isNano", JsErrorCode::PARAMETER_ERROR);
            }
        }
        getTimeContext->status = napi_ok;
    };
    getTimeContext->GetCbInfo(env, info, inputParser, true);
    auto executor = [getTimeContext]() {
        int32_t innerCode = GetDeviceTime(CLOCK_REALTIME, getTimeContext->isNano, getTimeContext->time);
        if (innerCode != JsErrorCode::ERROR_OK) {
            getTimeContext->errCode = innerCode;
            getTimeContext->status = napi_generic_failure;
        }
    };
    auto complete = [getTimeContext](napi_value &output) {
        getTimeContext->status = napi_create_int64(getTimeContext->env, getTimeContext->time, &output);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, getTimeContext,
                                 "convert native object to javascript object failed", JsErrorCode::ERROR);
    };
    return NapiWork::SyncEnqueue(env, getTimeContext, "GetTime", executor, complete);
}

napi_value NapiSystemDateTime::GetRealTime(napi_env env, napi_callback_info info)
{
    struct GetRealTimeContext : public ContextBase {
        int64_t time = 0;
        bool isNano = false;
    };
    auto *getRealTimeContext = new GetRealTimeContext();
    auto inputParser = [env, getRealTimeContext](size_t argc, napi_value *argv) {
        if (argc >= ARGC_ONE) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[ARGV_FIRST], &valueType);
            if (valueType == napi_boolean) {
                getRealTimeContext->status = napi_get_value_bool(env, argv[ARGV_FIRST], &getRealTimeContext->isNano);
                CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, getRealTimeContext, getRealTimeContext->status == napi_ok,
                    "invalid isNano", JsErrorCode::PARAMETER_ERROR);
            }
        }
        getRealTimeContext->status = napi_ok;
    };
    getRealTimeContext->GetCbInfo(env, info, inputParser);
    auto executor = [getRealTimeContext]() {
        int32_t innerCode;
        if (getRealTimeContext->isNano) {
            innerCode = TimeServiceClient::GetInstance()->GetBootTimeNs(getRealTimeContext->time);
        } else {
            innerCode = TimeServiceClient::GetInstance()->GetBootTimeMs(getRealTimeContext->time);
        }
        if (innerCode != JsErrorCode::ERROR_OK) {
            getRealTimeContext->errCode = innerCode;
            getRealTimeContext->status = napi_generic_failure;
        }
    };
    auto complete = [getRealTimeContext](napi_value &output) {
        getRealTimeContext->status = napi_create_int64(getRealTimeContext->env, getRealTimeContext->time, &output);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, getRealTimeContext,
            "convert native object to javascript object failed", JsErrorCode::ERROR);
    };
    return NapiWork::AsyncEnqueue(env, getRealTimeContext, "GetRealTime", executor, complete);
}

napi_value NapiSystemDateTime::GetUptime(napi_env env, napi_callback_info info)
{
    struct GetUpTimeContext : public ContextBase {
        int64_t time = 0;
        int32_t timeType = STARTUP;
        bool isNanoseconds = false;
    };
    auto *getUpTimeContext = new GetUpTimeContext();
    auto inputParser = [env, getUpTimeContext](size_t argc, napi_value *argv) {
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, getUpTimeContext, argc >= ARGC_ONE,
            "Mandatory parameters are left unspecified", JsErrorCode::PARAMETER_ERROR);
        getUpTimeContext->status = napi_get_value_int32(env, argv[ARGV_FIRST], &getUpTimeContext->timeType);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, getUpTimeContext, getUpTimeContext->status == napi_ok,
            "The type of 'timeType' must be number or enum", JsErrorCode::PARAMETER_ERROR);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, getUpTimeContext,
            (getUpTimeContext->timeType >= STARTUP && getUpTimeContext->timeType <= ACTIVE),
            "The 'timeType' must be 'STARTUP' or 'ACTIVE' or 0 or 1", JsErrorCode::PARAMETER_ERROR);
        if (argc >= ARGC_TWO) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[ARGV_SECOND], &valueType);
            if (valueType == napi_boolean) {
                getUpTimeContext->status =
                    napi_get_value_bool(env, argv[ARGV_SECOND], &getUpTimeContext->isNanoseconds);
                CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, getUpTimeContext, getUpTimeContext->status == napi_ok,
                    "get isNanoseconds failed", JsErrorCode::PARAMETER_ERROR);
            }
        }
        getUpTimeContext->status = napi_ok;
    };
    getUpTimeContext->GetCbInfo(env, info, inputParser, true);
    auto executor = [getUpTimeContext]() {
        int32_t innerCode;
        innerCode = GetDeviceTime(getUpTimeContext->isNanoseconds, getUpTimeContext->timeType,
            getUpTimeContext->time);
        if (innerCode != JsErrorCode::ERROR_OK) {
            getUpTimeContext->errCode = innerCode;
            getUpTimeContext->status = napi_generic_failure;
        }
    };
    auto complete = [getUpTimeContext](napi_value &output) {
        getUpTimeContext->status = napi_create_int64(getUpTimeContext->env, getUpTimeContext->time, &output);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, getUpTimeContext,
                                 "convert native object to javascript object failed", JsErrorCode::ERROR);
    };
    return NapiWork::SyncEnqueue(env, getUpTimeContext, "GetUpTime", executor, complete);
}

napi_value NapiSystemDateTime::GetDate(napi_env env, napi_callback_info info)
{
    struct GetDateContext : public ContextBase {
        int64_t time = 0;
    };
    auto *getDateContext = new GetDateContext();
    auto inputParser = [getDateContext](size_t argc, napi_value *argv) { getDateContext->status = napi_ok; };
    getDateContext->GetCbInfo(env, info, inputParser);
    auto executor = [getDateContext]() {
        auto innerCode = TimeServiceClient::GetInstance()->GetWallTimeMs(getDateContext->time);
        if (innerCode != JsErrorCode::ERROR_OK) {
            getDateContext->errCode = innerCode;
            getDateContext->status = napi_generic_failure;
        }
    };
    auto complete = [env, getDateContext](napi_value &output) {
        getDateContext->status = napi_create_date(env, getDateContext->time, &output);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, getDateContext,
            "convert native object to javascript object failed", JsErrorCode::ERROR);
    };

    return NapiWork::AsyncEnqueue(env, getDateContext, "GetDate", executor, complete);
}

napi_value NapiSystemDateTime::SetTimezone(napi_env env, napi_callback_info info)
{
    struct SetTimezoneContext : public ContextBase {
        std::string timezone;
    };
    auto *setTimezoneContext = new SetTimezoneContext();
    auto inputParser = [env, setTimezoneContext](size_t argc, napi_value *argv) {
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setTimezoneContext, argc >= ARGC_ONE,
            "Mandatory parameters are left unspecified", JsErrorCode::PARAMETER_ERROR);
        setTimezoneContext->status = NapiUtils::GetValue(env, argv[ARGV_FIRST], setTimezoneContext->timezone);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setTimezoneContext, setTimezoneContext->status == napi_ok,
            "The type of 'timezone' must be string", JsErrorCode::PARAMETER_ERROR);
        setTimezoneContext->status = napi_ok;
    };
    setTimezoneContext->GetCbInfo(env, info, inputParser);
    auto executor = [setTimezoneContext]() {
        auto innerCode = TimeServiceClient::GetInstance()->SetTimeZoneV9(setTimezoneContext->timezone);
        if (innerCode != JsErrorCode::ERROR_OK) {
            setTimezoneContext->errCode = innerCode;
            setTimezoneContext->status = napi_generic_failure;
        }
    };
    auto complete = [env](napi_value &output) { output = NapiUtils::GetUndefinedValue(env); };
    return NapiWork::AsyncEnqueue(env, setTimezoneContext, "SetTimezone", executor, complete);
}

napi_value NapiSystemDateTime::GetTimezone(napi_env env, napi_callback_info info)
{
    struct GetTimezoneContext : public ContextBase {
        std::string timezone;
    };
    auto *getTimezoneContext = new GetTimezoneContext();
    auto inputParser = [getTimezoneContext](size_t argc, napi_value *argv) {
        getTimezoneContext->status = napi_ok;
    };
    getTimezoneContext->GetCbInfo(env, info, inputParser);

    auto executor = [getTimezoneContext]() {
        auto innerCode = TimeServiceClient::GetInstance()->GetTimeZone(getTimezoneContext->timezone);
        if (innerCode != JsErrorCode::ERROR_OK) {
            getTimezoneContext->errCode = innerCode;
            getTimezoneContext->status = napi_generic_failure;
        }
    };
    auto complete = [env, getTimezoneContext](napi_value &output) {
        getTimezoneContext->status = napi_create_string_utf8(env, getTimezoneContext->timezone.c_str(),
            getTimezoneContext->timezone.size(), &output);
        TIME_HILOGI(TIME_MODULE_JS_NAPI, "%{public}s, ", getTimezoneContext->timezone.c_str());
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, getTimezoneContext,
            "convert native object to javascript object failed", JsErrorCode::ERROR);
    };
    return NapiWork::AsyncEnqueue(env, getTimezoneContext, "GetTimezone", executor, complete);
}

napi_value NapiSystemDateTime::GetTimezoneSync(napi_env env, napi_callback_info info)
{
    struct GetTimezoneContext : public ContextBase {
        std::string timezone;
    };
    auto *getTimezoneContext = new GetTimezoneContext();
    auto inputParser = [getTimezoneContext](size_t argc, napi_value *argv) { getTimezoneContext->status = napi_ok; };
    getTimezoneContext->GetCbInfo(env, info, inputParser, true);

    auto executor = [getTimezoneContext]() {
        auto innerCode = GetTimezone(getTimezoneContext->timezone);
        if (innerCode != JsErrorCode::ERROR_OK) {
            getTimezoneContext->errCode = innerCode;
            getTimezoneContext->status = napi_generic_failure;
        }
    };
    auto complete = [env, getTimezoneContext](napi_value &output) {
        getTimezoneContext->status = napi_create_string_utf8(env, getTimezoneContext->timezone.c_str(),
            getTimezoneContext->timezone.size(), &output);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, getTimezoneContext,
            "convert native object to javascript object failed", JsErrorCode::ERROR);
    };
    return NapiWork::SyncEnqueue(env, getTimezoneContext, "GetTimezone", executor, complete);
}

napi_value NapiSystemDateTime::GetAutoTimeStatus(napi_env env, napi_callback_info info)
{
    struct GetAutoTimeContext : public ContextBase {
        bool autotime;
    };
    auto *getAutoTimeContext = new GetAutoTimeContext();
    auto inputParser = [env, getAutoTimeContext](size_t argc, napi_value *argv) {
        getAutoTimeContext->status = napi_ok;
    };
    getAutoTimeContext->GetCbInfo(env, info, inputParser, true);
    auto executor = [getAutoTimeContext]() {
        auto innerCode = GetAutoTime(getAutoTimeContext->autotime);
            if (innerCode != JsErrorCode::ERROR_OK) {
            getAutoTimeContext->errCode = innerCode;
            getAutoTimeContext->status = napi_generic_failure;
        }
    };
    auto complete = [env, getAutoTimeContext](napi_value &output) {
        getAutoTimeContext->status =
            napi_get_boolean(env, getAutoTimeContext->autotime, &output);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, getAutoTimeContext,
            "convert native object to javascript object failed", JsErrorCode::ERROR);
    };
    return NapiWork::SyncEnqueue(env, getAutoTimeContext, "GetAutoTimeStatus", executor, complete);
}

napi_value NapiSystemDateTime::SetAutoTimeStatus(napi_env env, napi_callback_info info)
{
    struct SetAutoTimeContext : public ContextBase {
        bool autotime;
    };
    auto *setAutoTimeContext = new SetAutoTimeContext();
    auto inputParser = [env, setAutoTimeContext](size_t argc, napi_value *argv) {
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setAutoTimeContext, argc >= ARGC_ONE,
            "Mandatory parameters are left unspecified", JsErrorCode::PARAMETER_ERROR);
        setAutoTimeContext->status = napi_get_value_bool(env, argv[ARGV_FIRST], &setAutoTimeContext->autotime);
        CHECK_ARGS_RETURN_VOID(TIME_MODULE_JS_NAPI, setAutoTimeContext, setAutoTimeContext->status == napi_ok,
            "The type of 'time' must be bool", JsErrorCode::PARAMETER_ERROR);
        setAutoTimeContext->status = napi_ok;
    };
    setAutoTimeContext->GetCbInfo(env, info, inputParser);
    auto executor = [setAutoTimeContext]() {
        auto innerCode = TimeServiceClient::GetInstance()->SetAutoTime(setAutoTimeContext->autotime);
        if (innerCode != JsErrorCode::ERROR_OK) {
            if (innerCode != E_TIME_NOT_SYSTEM_APP && innerCode != E_TIME_NO_PERMISSION) {
                setAutoTimeContext->errCode = E_TIME_NTP_UPDATE_FAILED;
            } else {
                setAutoTimeContext->errCode = innerCode;
            }
            setAutoTimeContext->status = napi_generic_failure;
        }
    };
    auto complete = [env](napi_value &output) { output = NapiUtils::GetUndefinedValue(env); };
    return NapiWork::AsyncEnqueue(env, setAutoTimeContext, "SetAutoTimeStatus", executor, complete);
}

napi_value NapiSystemDateTime::UpdateNtpTime(napi_env env, napi_callback_info info)
{
    struct UpdateNtpTime : public ContextBase {
        int64_t time = 0;
    };
    auto *updateNtpTimeContext = new UpdateNtpTime();
    auto inputParser = [env, updateNtpTimeContext](size_t argc, napi_value *argv) {
        updateNtpTimeContext->status = napi_ok;
    };
    updateNtpTimeContext->GetCbInfo(env, info, inputParser);

    auto executor = [updateNtpTimeContext]() {
        int32_t innerCode = TimeServiceClient::GetInstance()->GetNtpTimeMs(updateNtpTimeContext->time);
        if (innerCode != JsErrorCode::ERROR_OK) {
            updateNtpTimeContext->errCode = innerCode;
            updateNtpTimeContext->status = napi_generic_failure;
        }
    };
    auto complete = [env](napi_value &output) {
        output = NapiUtils::GetUndefinedValue(env);
    };
    return NapiWork::AsyncEnqueue(env, updateNtpTimeContext, "UpdateNtpTime", executor, complete);
}

napi_value NapiSystemDateTime::GetNtpTime(napi_env env, napi_callback_info info)
{
    struct GetNtpTimeContext : public ContextBase {
        int64_t time = 0;
    };
    auto *getNtpTimeContext = new GetNtpTimeContext();
    auto inputParser = [env, getNtpTimeContext](size_t argc, napi_value *argv) {
        getNtpTimeContext->status = napi_ok;
    };
    getNtpTimeContext->GetCbInfo(env, info, inputParser);

    auto executor = [getNtpTimeContext]() {
        int32_t innerCode = TimeServiceClient::GetInstance()->GetRealTimeMs(getNtpTimeContext->time);
        if (innerCode != JsErrorCode::ERROR_OK) {
            getNtpTimeContext->errCode = innerCode;
            getNtpTimeContext->status = napi_generic_failure;
        }
    };
    auto complete = [getNtpTimeContext](napi_value &output) {
        getNtpTimeContext->status = napi_create_int64(getNtpTimeContext->env, getNtpTimeContext->time, &output);
        CHECK_STATUS_RETURN_VOID(TIME_MODULE_JS_NAPI, getNtpTimeContext,
            "convert native object to javascript object failed", JsErrorCode::ERROR);
    };
    return NapiWork::SyncEnqueue(env, getNtpTimeContext, "GetNtpTime", executor, complete);
}

int32_t NapiSystemDateTime::GetDeviceTime(clockid_t clockId, bool isNano, int64_t &time)
{
    struct timespec tv {};
    if (clock_gettime(clockId, &tv) < 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "failed clock_gettime, errno: %{public}s", strerror(errno));
        return ERROR;
    }

    if (isNano) {
        time = tv.tv_sec * SECONDS_TO_NANO + tv.tv_nsec;
    } else {
        time = tv.tv_sec * SECONDS_TO_MILLI + tv.tv_nsec / NANO_TO_MILLI;
    }
    return ERROR_OK;
}

int32_t NapiSystemDateTime::GetTimezone(std::string &timezone)
{
    timezone = system::GetParameter(TIMEZONE_KEY, "Asia/Shanghai");
    if (timezone.empty()) {
        TIME_HILOGW(TIME_MODULE_SERVICE, "No found timezone from system parameter.");
        return ERROR;
    }
    return ERROR_OK;
}

int32_t NapiSystemDateTime::GetAutoTime(bool &autoTime)
{
    auto res = system::GetParameter(AUTOTIME_KEY, "ON");
    autoTime = (res == "ON");
    return ERROR_OK;
}

int32_t NapiSystemDateTime::GetDeviceTime(bool isNano, int32_t timeType, int64_t &time)
{
#ifdef IOS_PLATFORM
    int64_t deviceTime = 0;
    if (timeType == STARTUP) {
        deviceTime = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
    } else {
        deviceTime = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    }
    if (deviceTime == 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "failed clock_gettime_nsec_np.");
        return ERROR;
    }

    if (isNano) {
        time = deviceTime;
    } else {
        time = deviceTime / NANO_TO_MILLI;
    }
    return ERROR_OK;
#else
    if (timeType == STARTUP) {
        return GetDeviceTime(CLOCK_BOOTTIME, isNano, time);
    } else {
        return GetDeviceTime(CLOCK_MONOTONIC, isNano, time);
    }
#endif
}
} // namespace Time
} // namespace MiscServices
} // namespace OHOS