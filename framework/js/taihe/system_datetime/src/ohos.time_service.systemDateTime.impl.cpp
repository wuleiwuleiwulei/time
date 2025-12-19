/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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

#include "ohos.time_service.systemDateTime.proj.hpp"
#include "ohos.time_service.systemDateTime.impl.hpp"
#include "taihe/runtime.hpp"
#include "stdexcept"
#include "time_service_client.h"
#include "ani_utils.h"
#include "parameters.h"

using namespace taihe;
using namespace ohos::time_service::systemDateTime;
using namespace OHOS::MiscServices;
using namespace OHOS::MiscServices::Time;

namespace {

constexpr int64_t SECONDS_TO_NANO = 1000000000;
constexpr int64_t SECONDS_TO_MILLI = 1000;
constexpr int64_t NANO_TO_MILLI = SECONDS_TO_NANO / SECONDS_TO_MILLI;
constexpr int32_t STARTUP = 0;
constexpr int32_t ACTIVE = 1;
constexpr const char *TIMEZONE_KEY = "persist.time.timezone";
constexpr const char *AUTOTIME_KEY = "persist.time.auto_time";

int32_t GetDeviceTime(clockid_t clockId, bool isNano, int64_t &time)
{
    struct timespec tv {};
    if (clock_gettime(clockId, &tv) < 0) {
        return JsErrorCode::ERROR;
    }

    if (isNano) {
        time = tv.tv_sec * SECONDS_TO_NANO + tv.tv_nsec;
    } else {
        time = tv.tv_sec * SECONDS_TO_MILLI + tv.tv_nsec / NANO_TO_MILLI;
    }
    return JsErrorCode::ERROR_OK;
}


string GetTimezoneSync()
{
    auto timeZone = OHOS::system::GetParameter(TIMEZONE_KEY, "Asia/Shanghai");
    if (timeZone.empty()) {
        set_business_error(JsErrorCode::ERROR, "convert native object to javascript object failed");
        return "";
    }
    return timeZone;
}

int64_t GetUptime(TimeType timeType, optional_view<bool> isNanoseconds)
{
    bool isNanosecondsValue = isNanoseconds.value_or(false);
    int innerCode;
    int64_t time = 0;
    if (timeType == STARTUP) {
        innerCode = GetDeviceTime(CLOCK_BOOTTIME, isNanosecondsValue, time);
    } else if (timeType == ACTIVE) {
        innerCode = GetDeviceTime(CLOCK_MONOTONIC, isNanosecondsValue, time);
    } else {
        set_business_error(JsErrorCode::PARAMETER_ERROR, "The 'timeType' must be 'STARTUP' or 'ACTIVE' or 0 or 1");
        return 0;
    }

    if (innerCode != JsErrorCode::ERROR_OK) {
        set_business_error(JsErrorCode::ERROR, "convert native object to javascript object failed");
        return 0;
    }
    return time;
}

int64_t GetTime(optional_view<bool> isNanoseconds)
{
    bool isNanosecondsValue = isNanoseconds.value_or(false);
    int64_t time = 0;
    int32_t innerCode = GetDeviceTime(CLOCK_REALTIME, isNanosecondsValue, time);
    if (innerCode != JsErrorCode::ERROR_OK) {
        set_business_error(JsErrorCode::ERROR, "convert native object to javascript object failed");
        return 0;
    }
    return time;
}

void SetTimeSync(int64_t time)
{
    auto innerCode = TimeServiceClient::GetInstance()->SetTimeV9(time);
    if (innerCode != JsErrorCode::ERROR_OK) {
        int32_t errorCode = AniUtils::ConvertErrorCode(innerCode);
        std::string errorMessage = AniUtils::GetErrorMessage(errorCode);
        set_business_error(errorCode, errorMessage);
    }
}

void SetTimezoneSync(::taihe::string_view timezone)
{
    std::string zone = std::string(timezone);
    auto innerCode = TimeServiceClient::GetInstance()->SetTimeZoneV9(zone);
    if (innerCode != JsErrorCode::ERROR_OK) {
        int32_t errorCode = AniUtils::ConvertErrorCode(innerCode);
        std::string errorMessage = AniUtils::GetErrorMessage(errorCode);
        set_business_error(errorCode, errorMessage);
    }
}

void UpdateNtpTimeSync()
{
    int64_t time = 0;
    int32_t innerCode = TimeServiceClient::GetInstance()->GetNtpTimeMs(time);
    if (innerCode != JsErrorCode::ERROR_OK) {
        int32_t errorCode = AniUtils::ConvertErrorCode(innerCode);
        std::string errorMessage = AniUtils::GetErrorMessage(errorCode);
        set_business_error(errorCode, errorMessage);
    }
}

int64_t GetNtpTime()
{
    int64_t time = 0;
    int32_t innerCode = TimeServiceClient::GetInstance()->GetRealTimeMs(time);
    if (innerCode != JsErrorCode::ERROR_OK) {
        int32_t errorCode = AniUtils::ConvertErrorCode(innerCode);
        std::string errorMessage = AniUtils::GetErrorMessage(errorCode);
        set_business_error(errorCode, errorMessage);
        return 0;
    }
    return time;
}

bool GetAutoTimeStatus()
{
    auto res = OHOS::system::GetParameter(AUTOTIME_KEY, "ON");
    bool autoTime = (res == "ON");
    return autoTime;
}

void SetAutoTimeStatusSync(bool status)
{
    int32_t innerCode = TimeServiceClient::GetInstance()->SetAutoTime(status);
    if (innerCode != JsErrorCode::ERROR_OK) {
        int32_t errorCode = AniUtils::ConvertErrorCode(innerCode);
        std::string errorMessage = AniUtils::GetErrorMessage(errorCode);
        set_business_error(errorCode, errorMessage);
    }
}
}  // namespace

TH_EXPORT_CPP_API_GetTimezoneSync(GetTimezoneSync);
TH_EXPORT_CPP_API_GetUptime(GetUptime);
TH_EXPORT_CPP_API_GetTime(GetTime);
TH_EXPORT_CPP_API_SetTimeSync(SetTimeSync);
TH_EXPORT_CPP_API_SetTimezoneSync(SetTimezoneSync);
TH_EXPORT_CPP_API_UpdateNtpTimeSync(UpdateNtpTimeSync);
TH_EXPORT_CPP_API_GetNtpTime(GetNtpTime);
TH_EXPORT_CPP_API_GetAutoTimeStatus(GetAutoTimeStatus);
TH_EXPORT_CPP_API_SetAutoTimeStatusSync(SetAutoTimeStatusSync);