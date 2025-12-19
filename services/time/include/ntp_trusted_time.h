
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

#ifndef SNTP_CLIENT_NTP_TRUSTED_TIME_H
#define SNTP_CLIENT_NTP_TRUSTED_TIME_H

#include "time_common.h"
#include <mutex>

namespace OHOS {
namespace MiscServices {
class NtpTrustedTime {
public:
    static NtpTrustedTime &GetInstance();
    bool ForceRefresh(const std::string &ntpServer);
    bool ForceRefreshTrusted(const std::string &ntpServer);
    int64_t GetCacheAge();
    int64_t CurrentTimeMillis();
    int64_t ElapsedRealtimeMillis();
    std::chrono::steady_clock::time_point GetBootTimeNs();
    bool FindBestTimeResult();
    void ClearTimeResultCandidates();
    class TimeResult : std::enable_shared_from_this<TimeResult> {
    public:
        TimeResult();
        TimeResult(int64_t mTimeMillis, int64_t mElapsedRealtimeMills, int64_t mCertaintyMillis,
            std::string ntpServer);
        ~TimeResult();
        int64_t GetTimeMillis();
        int64_t GetElapsedRealtimeMillis();
        int64_t CurrentTimeMillis(int64_t bootTime);
        int64_t GetAgeMillis(int64_t bootTime);
        std::string GetNtpServer();
        void Clear();

    private:
        // Calculated network time from NTP server.
        int64_t mTimeMillis;
        // Boot time when getting time from NTP server.
        int64_t mElapsedRealtimeMillis;
        int64_t mCertaintyMillis;
        std::string mNtpServer;
    };
    bool IsTimeResultTrusted(std::shared_ptr<TimeResult> timeResult);
    int32_t GetSameTimeResultCount(std::shared_ptr<TimeResult> candidateTimeResult);

private:
    std::shared_ptr<TimeResult> mTimeResult {};
    std::vector<std::shared_ptr<TimeResult>> TimeResultCandidates_ {};
    static std::mutex mTimeResultMutex_;
};
} // namespace MiscServices
} // namespace OHOS

#endif // SNTP_CLIENT_NTP_TRUSTED_TIME_H