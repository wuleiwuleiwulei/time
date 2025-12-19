/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "time_hilog.h"
#include "system_date_time.h"
#include "cj_ffi/cj_common_ffi.h"
#include "time_service_client.h"
#include "utils.h"
#include "parameters.h"

namespace OHOS {
namespace CJSystemapi {
namespace SystemDateTime {

using namespace MiscServices;
constexpr const char *TIMEZONE_KEY = "persist.time.timezone";
constexpr int32_t STARTUP = 0;
constexpr int64_t SECONDS_TO_NANO = 1000000000;
constexpr int64_t SECONDS_TO_MILLI = 1000;
constexpr int64_t NANO_TO_MILLI = SECONDS_TO_NANO / SECONDS_TO_MILLI;

int SystemDateTimeImpl::SetTime(int64_t time)
{
    auto innerCode = TimeServiceClient::GetInstance()->SetTimeV9(time);
    if (innerCode != CjErrorCode::ERROR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "failed innerCode is %{public}d", innerCode);
        return ERROR;
    }

    return SUCCESS_CODE;
}

std::tuple<int32_t, int64_t> SystemDateTimeImpl::getCurrentTime(bool isNano)
{
    int32_t innerCode;
    int64_t time = 0;
    if (isNano) {
        innerCode = TimeServiceClient::GetInstance()->GetWallTimeNs(time);
    } else {
        innerCode = TimeServiceClient::GetInstance()->GetWallTimeMs(time);
    }
    if (innerCode != CjErrorCode::ERROR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "failed innerCode is %{public}d", innerCode);
        return {innerCode, ERROR};
    }
    return {SUCCESS_CODE, time};
}

std::tuple<int32_t, int64_t> SystemDateTimeImpl::getRealActiveTime(bool isNano)
{
    int32_t innerCode;
    int64_t time = 0;
    if (isNano) {
        innerCode = TimeServiceClient::GetInstance()->GetMonotonicTimeNs(time);
    } else {
        innerCode = TimeServiceClient::GetInstance()->GetMonotonicTimeMs(time);
    }
    if (innerCode != CjErrorCode::ERROR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "failed innerCode is %{public}d", innerCode);
        return {innerCode, ERROR};
    }
    return {SUCCESS_CODE, time};
}

std::tuple<int32_t, int64_t> SystemDateTimeImpl::getRealTime(bool isNano)
{
    int32_t innerCode;
    int64_t time = 0;
    if (isNano) {
        innerCode = TimeServiceClient::GetInstance()->GetBootTimeNs(time);
    } else {
        innerCode = TimeServiceClient::GetInstance()->GetBootTimeMs(time);
    }
    if (innerCode != CjErrorCode::ERROR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "failed innerCode is %{public}d", innerCode);
        return {innerCode, ERROR};
    }
    return {SUCCESS_CODE, time};
}

int32_t SystemDateTimeImpl::GetDeviceTime(clockid_t clockId, bool isNano, int64_t &time)
{
    struct timespec tv {};
    if (clock_gettime(clockId, &tv) < 0) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "failed clock_gettime");
        return ERROR;
    }

    if (isNano) {
        time = tv.tv_sec * SECONDS_TO_NANO + tv.tv_nsec;
    } else {
        time = tv.tv_sec * SECONDS_TO_MILLI + tv.tv_nsec / NANO_TO_MILLI;
    }
    return 0;
}

std::tuple<int32_t, int64_t> SystemDateTimeImpl::getTime(bool isNano)
{
    int32_t innerCode;
    int64_t time = 0;
    innerCode = GetDeviceTime(CLOCK_REALTIME, isNano, time);
    if (innerCode != CjErrorCode::ERROR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "failed innerCode is %{public}d", innerCode);
        return {innerCode, ERROR};
    }
    return {SUCCESS_CODE, time};
}

std::tuple<int32_t, int64_t> SystemDateTimeImpl::getUpTime(int32_t timeType, bool isNano)
{
    int32_t innerCode;
    int64_t time = 0;
    if (timeType == STARTUP) {
        innerCode = GetDeviceTime(CLOCK_BOOTTIME, isNano, time);
    } else {
        innerCode = GetDeviceTime(CLOCK_MONOTONIC, isNano, time);
    }
    if (innerCode != CjErrorCode::ERROR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "failed innerCode is %{public}d", innerCode);
        return {innerCode, ERROR};
    }
    return {SUCCESS_CODE, time};
}

int SystemDateTimeImpl::SetTimeZone(char* timezone)
{
    auto innerCode = TimeServiceClient::GetInstance()->SetTimeZoneV9(timezone);
    if (innerCode != CjErrorCode::ERROR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "failed innerCode is %{public}d", innerCode);
        return ERROR;
    }
    return SUCCESS_CODE;
}

char* MallocCString(const std::string& origin)
{
    if (origin.empty()) {
        return nullptr;
    }
    auto len = origin.length() + 1;
    char* res = static_cast<char*>(malloc(sizeof(char) * len));
    if (res == nullptr) {
        return nullptr;
    }
    return std::char_traits<char>::copy(res, origin.c_str(), len);
}

int32_t GetTimezone(std::string &timezone)
{
    timezone = OHOS::system::GetParameter(TIMEZONE_KEY, "Asia/Shanghai");
    if (timezone.empty()) {
        return ERROR;
    }
    return ERROR_OK;
}

std::tuple<int32_t, char*> SystemDateTimeImpl::getTimezone()
{
    int32_t innerCode;
    std::string time;
    innerCode = GetTimezone(time);
    if (innerCode != CjErrorCode::ERROR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "failed innerCode is %{public}d", innerCode);
        return {innerCode, nullptr};
    }
    return {SUCCESS_CODE, MallocCString(time)};
}
} // SystemDateTime
} // CJSystemapi
} // OHOS