/*
 * Copyright (C) 2023-2023 Huawei Device Co., Ltd.
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
#ifndef TIMER_PROXY_H
#define TIMER_PROXY_H

#include "single_instance.h"
#include "timer_info.h"

namespace OHOS {
namespace MiscServices {
using AdjustTimerCallback = std::function<bool(std::shared_ptr<TimerInfo> timer)>;
class TimerProxy {
    DECLARE_SINGLE_INSTANCE(TimerProxy)
public:
    int32_t CallbackAlarmIfNeed(const std::shared_ptr<TimerInfo> &alarm);
    bool ProxyTimer(int32_t uid, int pid, bool isProxy, bool needRetrigger,
        const std::chrono::steady_clock::time_point &now,
        std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback);
    bool AdjustTimer(bool isAdjust, uint32_t interval,
        const std::chrono::steady_clock::time_point &now, uint32_t delta,
        std::function<void(AdjustTimerCallback adjustTimer)> updateTimerDeliveries);
    bool SetTimerExemption(const std::unordered_set<std::string> &nameArr, bool isExemption);
    bool IsTimerExemption(std::shared_ptr<TimerInfo> time);
    void SetAdjustPolicy(const std::unordered_map<std::string, uint32_t> &policyMap);
    bool ResetAllProxy(const std::chrono::steady_clock::time_point &now,
        std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback);
    void EraseTimerFromProxyTimerMap(const uint64_t id, const int uid, const int pid);
    void RecordUidTimerMap(const std::shared_ptr<TimerInfo> &alarm, const bool isRebatched);
    void RecordProxyTimerMap(const std::shared_ptr<TimerInfo> &alarm, bool isPid);
    void RemoveUidTimerMap(const std::shared_ptr<TimerInfo> &alarm);
    void RemoveUidTimerMapLocked(const std::shared_ptr<TimerInfo> &alarm);
    void RemoveUidTimerMap(const uint64_t id);
    int32_t CountUidTimerMapByUid(int32_t uid);
    bool IsProxy(const int32_t uid, const int32_t pid);
    bool IsProxyLocked(const int32_t uid, const int32_t pid);
    #ifdef HIDUMPER_ENABLE
    bool ShowProxyTimerInfo(int fd, const int64_t now);
    bool ShowUidTimerMapInfo(int fd, const int64_t now);
    bool ShowProxyDelayTime(int fd);
    void ShowAdjustTimerInfo(int fd);
    #endif
    int64_t GetProxyDelayTime() const;

private:
    void EraseAlarmItem(
        const uint64_t id, std::unordered_map<uint64_t, std::shared_ptr<TimerInfo>> &idAlarmsMap);
    void UpdateProxyWhenElapsedForProxyTimers(const int32_t uid, const int32_t pid,
        const std::chrono::steady_clock::time_point &now,
        std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback,
        bool needRetrigger);
    bool UpdateAdjustWhenElapsed(const std::chrono::steady_clock::time_point &now,  uint32_t delta,
        uint32_t interval, std::shared_ptr<TimerInfo> &timer);
    bool RestoreAdjustWhenElapsed(std::shared_ptr<TimerInfo> &timer);
    bool RestoreProxyWhenElapsed(const int32_t uid, const int32_t pid,
        const std::chrono::steady_clock::time_point &now,
        std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback,
        bool needRetrigger);
    bool RestoreProxyWhenElapsedForProxyTimers(const int32_t uid, const int32_t pid,
        const std::chrono::steady_clock::time_point &now,
        std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback,
        bool needRetrigger);
    void ResetAllProxyWhenElapsed(const std::chrono::steady_clock::time_point &now,
        std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback);

    std::mutex uidTimersMutex_;
    /* <uid, <id, alarm ptr>> */
    std::unordered_map<int32_t, std::unordered_map<uint64_t, std::shared_ptr<TimerInfo>>> uidTimersMap_ {};
    std::mutex proxyMutex_;
    /* <(uid << 32) | pid, [timerid]> */
    std::unordered_map<uint64_t, std::vector<uint64_t>> proxyTimers_ {};
    std::mutex adjustMutex_;
    std::unordered_set<std::string> adjustExemptionList_ { "time_service" };
    std::unordered_set<uint64_t> adjustTimers_ {};
    std::unordered_map<std::string, uint32_t> adjustPolicyList_ {};
    /* ms for 3 days */
    int64_t proxyDelayTime_ = 3 * 24 * 60 * 60 * 1000;
}; // timer_proxy
} // MiscServices
} // OHOS

#endif // TIMER_PROXY_H