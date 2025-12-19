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

#ifndef NTP_UPDATE_TIME_H
#define NTP_UPDATE_TIME_H

#include <mutex>

namespace OHOS {
namespace MiscServices {
struct AutoTimeInfo {
    std::string ntpServer;
    std::string ntpServerSpec;
    std::string status;
    int64_t lastUpdateTime;
};

enum NtpRefreshCode {
    NO_NEED_REFRESH,
    REFRESH_SUCCESS,
    REFRESH_FAILED
};

enum NtpUpdateSource {
    RETRY_BY_TIMER,
    REGISTER_SUBSCRIBER,
    NET_CONNECTED,
    NTP_SERVER_CHANGE,
    AUTO_TIME_CHANGE,
    INIT,
};

class NtpUpdateTime {
public:
    static NtpUpdateTime &GetInstance();
    static bool GetNtpTime(int64_t &time);
    static bool GetRealTime(int64_t &time);
    static void SetSystemTime(NtpUpdateSource code);
    static bool IsInUpdateInterval();
    void RefreshNetworkTimeByTimer(uint64_t timerId);
    void UpdateNITZSetTime();
    void Stop();
    void Init();
    bool IsValidNITZTime();
    uint64_t GetNITZUpdateTime();

private:
    NtpUpdateTime();
    static NtpRefreshCode GetNtpTimeInner();
    static bool CheckNeedSetTime(NtpRefreshCode code, int64_t time);
    static bool GetRealTimeInner(int64_t &time);
    static void ChangeNtpServerCallback(const char *key, const char *value, void *context);
    static std::vector<std::string> SplitNtpAddrs(const std::string &ntpStr);
    static void RefreshNextTriggerTime(NtpUpdateSource code, bool isSuccess, bool isSwitchOpen);
    bool CheckStatus();
    void RegisterSystemParameterListener();
    static void ChangeAutoTimeCallback(const char *key, const char *value, void *context);

    static AutoTimeInfo autoTimeInfo_;
    static uint64_t timerId_;
    uint64_t nitzUpdateTimeMilli_;
    static int64_t ntpRetryInterval_;
    static std::mutex requestMutex_;
    int64_t lastNITZUpdateTime_;
    static std::mutex ntpRetryMutex_;
};
} // namespace MiscServices
} // namespace OHOS
#endif