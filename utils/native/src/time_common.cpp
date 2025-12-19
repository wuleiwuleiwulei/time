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

#include "time_common.h"


namespace OHOS {
namespace MiscServices {
namespace {
static constexpr int MILLI_TO_SEC = 1000LL;
static constexpr int NANO_TO_SEC = 1000000000LL;
constexpr int32_t NANO_TO_MILLI = NANO_TO_SEC / MILLI_TO_SEC;
}

int32_t TimeUtils::GetWallTimeMs(int64_t &time)
{
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_REALTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get rt failed");
        return E_TIME_SA_DIED;
    }
    time = tv.tv_sec * MILLI_TO_SEC + tv.tv_nsec / NANO_TO_MILLI;
    return E_TIME_OK;
}

int32_t TimeUtils::GetBootTimeNs(int64_t &time)
{
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_BOOTTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get bt failed");
        return E_TIME_SA_DIED;
    }
    time = tv.tv_sec * NANO_TO_SEC + tv.tv_nsec;
    return E_TIME_OK;
}

// LCOV_EXCL_START
// The method has no input parameters, impossible to construct fuzz test.
std::chrono::steady_clock::time_point TimeUtils::GetBootTimeNs()
{
    int64_t timeNow = -1;
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_BOOTTIME, tv)) {
        return std::chrono::steady_clock::now();
    }
    timeNow = tv.tv_sec * NANO_TO_SEC + tv.tv_nsec;
    std::chrono::steady_clock::time_point tp_epoch ((std::chrono::nanoseconds(timeNow)));
    return tp_epoch;
}
// LCOV_EXCL_STOP

int32_t TimeUtils::GetBootTimeMs(int64_t &time)
{
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_BOOTTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get bt failed");
        return E_TIME_SA_DIED;
    }
    time = tv.tv_sec * MILLI_TO_SEC + tv.tv_nsec / NANO_TO_MILLI;
    return E_TIME_OK;
}

bool TimeUtils::GetTimeByClockId(clockid_t clockId, struct timespec &tv)
{
    if (clock_gettime(clockId, &tv) < 0) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Failed clock_gettime, errno: %{public}s", strerror(errno));
        return false;
    }
    return true;
}

}
}