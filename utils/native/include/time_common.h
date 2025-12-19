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

#ifndef SERVICES_INCLUDE_TIME_COMMON_H
#define SERVICES_INCLUDE_TIME_COMMON_H

#include <string>
#include <chrono>

#include "errors.h"
#include "time_hilog.h"

namespace OHOS {
namespace MiscServices {
#define TIME_SERVICE_NAME "TimeService"

struct TimerPara {
    std::string name;
    int timerType;
    int64_t windowLength;
    uint64_t interval;
    bool disposable;
    bool autoRestore;
    uint32_t flag;
};

enum TimeModule : uint8_t {
    TIME_MODULE_SERVICE_ID = 0x04,
};
// time error offset, used only in this file.
constexpr ErrCode TIME_ERR_OFFSET = ErrCodeOffset(SUBSYS_SMALLSERVICES, TIME_MODULE_SERVICE_ID);

enum TimeError : int32_t {
    E_TIME_OK = 0,
    E_TIME_SA_DIED = TIME_ERR_OFFSET,       // 77856768
    E_TIME_READ_PARCEL_ERROR,               // 77856769
    E_TIME_WRITE_PARCEL_ERROR,              // 77856770
    E_TIME_PUBLISH_FAIL,                    // 77856771
    E_TIME_TRANSACT_ERROR,                  // 77856772
    E_TIME_DEAL_FAILED,                     // 77856773
    E_TIME_PARAMETERS_INVALID,              // 77856774
    E_TIME_SET_RTC_FAILED,                  // 77856775
    E_TIME_NOT_FOUND,                       // 77856776
    E_TIME_NULLPTR,                         // 77856777
    E_TIME_NO_PERMISSION,                   // 77856778
    E_TIME_NOT_SYSTEM_APP,                  // 77856779
    E_TIME_NO_TIMER_ADJUST,                 // 77856780
    E_TIME_NTP_UPDATE_FAILED,               // 77856781
    E_TIME_NTP_NOT_UPDATE,                  // 77856782
    #ifdef MULTI_ACCOUNT_ENABLE
    E_TIME_ACCOUNT_NOT_MATCH,
    E_TIME_ACCOUNT_ERROR,
    #endif
    E_TIME_AUTO_RESTORE_ERROR,
};

enum DatabaseType : int8_t {
    NOT_STORE = 0,
    STORE,
};

enum APIVersion : int8_t {
    API_VERSION_7 = 0,
    API_VERSION_9 = 1,
};

class TimeUtils {
public:
    static int32_t GetWallTimeMs(int64_t &time);
    static int32_t GetBootTimeMs(int64_t &time);
    static int32_t GetBootTimeNs(int64_t &time);
    static std::chrono::steady_clock::time_point GetBootTimeNs();
    static bool GetTimeByClockId(clockid_t clockId, struct timespec &tv);
};
} // namespace MiscServices
} // namespace OHOS
#endif // SERVICES_INCLUDE_TIME_COMMON_H