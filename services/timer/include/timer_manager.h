/*
 * Copyright (C) 2021-2023 Huawei Device Co., Ltd.
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
#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <random>
#include <thread>
#include <cinttypes>

#include "batch.h"
#include "timer_handler.h"

#ifdef POWER_MANAGER_ENABLE
#include "completed_callback.h"
#include "power_mgr_client.h"
#endif

#ifdef MULTI_ACCOUNT_ENABLE
#include <utility>
#endif

namespace OHOS {
namespace MiscServices {
static std::vector<std::string> NEED_RECOVER_ON_REBOOT = { "not_support" };

class TimerManager : public ITimerManager {
public:
    int32_t CreateTimer(TimerPara &paras,
                        std::function<int32_t (const uint64_t)> callback,
                        std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> wantAgent,
                        int uid,
                        int pid,
                        uint64_t &timerId,
                        DatabaseType type) override;
    void ReCreateTimer(uint64_t timerId, std::shared_ptr<TimerEntry> timerInfo);
    int32_t StartTimer(uint64_t timerId, uint64_t triggerTime) override;
    int32_t StopTimer(uint64_t timerId) override;
    int32_t DestroyTimer(uint64_t timerId) override;
    bool ProxyTimer(int32_t uid, std::set<int> pidList, bool isProxy, bool needRetrigger) override;
    bool AdjustTimer(bool isAdjust, uint32_t interval, uint32_t delta) override;
    void SetTimerExemption(const std::unordered_set<std::string> &nameArr, bool isExemption) override;
    void SetAdjustPolicy(const std::unordered_map<std::string, uint32_t> &policyMap) override;
    bool ResetAllProxy() override;
    #ifndef RDB_ENABLE
    int32_t StartTimerGroup(std::vector<std::pair<uint64_t, uint64_t>> timerVec, std::string tableName);
    #endif
    #ifdef HIDUMPER_ENABLE
    bool ShowTimerEntryMap(int fd);
    bool ShowTimerEntryById(int fd, uint64_t timerId);
    bool ShowTimerTriggerById(int fd, uint64_t timerId);
    bool ShowIdleTimerInfo(int fd);
    #endif
    #ifdef MULTI_ACCOUNT_ENABLE
    void OnUserRemoved(int userId);
    #endif
    void OnPackageRemoved(int uid);
    ~TimerManager() override;
    void HandleRSSDeath();
    static TimerManager* GetInstance();
    #ifdef SET_AUTO_REBOOT_ENABLE
    void ShutDownReschedulePowerOnTimer();
    #endif

private:
    explicit TimerManager(std::shared_ptr<TimerHandler> impl);
    void TimerLooper();

    void SetHandlerLocked(std::shared_ptr<TimerInfo> timer);
    void RemoveHandler(uint64_t id);
    void RemoveLocked(uint64_t id, bool needReschedule);
    void ReBatchAllTimers();
    void ReAddTimerLocked(std::shared_ptr<TimerInfo> timer,
                          std::chrono::steady_clock::time_point nowElapsed);
    void SetHandlerLocked(std::shared_ptr<TimerInfo> alarm, bool rebatching, bool isRebatched);
    void InsertAndBatchTimerLocked(std::shared_ptr<TimerInfo> alarm);
    int64_t AttemptCoalesceLocked(std::chrono::steady_clock::time_point whenElapsed,
                                  std::chrono::steady_clock::time_point maxWhen);
    void TriggerIdleTimer();
    bool ProcTriggerTimer(std::shared_ptr<TimerInfo> &alarm,
                          const std::chrono::steady_clock::time_point &nowElapsed);
    bool TriggerTimersLocked(std::vector<std::shared_ptr<TimerInfo>> &triggerList,
                             std::chrono::steady_clock::time_point nowElapsed);
    void RescheduleKernelTimerLocked();
    void DeliverTimersLocked(const std::vector<std::shared_ptr<TimerInfo>> &triggerList);
    void NotifyWantAgentRetry(std::shared_ptr<TimerInfo> timer);
    std::shared_ptr<Batch> FindFirstWakeupBatchLocked();
    void SetLocked(int type, std::chrono::nanoseconds when, std::chrono::steady_clock::time_point bootTime);
    int32_t StopTimerInner(uint64_t timerNumber, bool needDestroy);
    int32_t StopTimerInnerLocked(bool needDestroy, uint64_t timerNumber, bool &needRecover);
    void UpdateOrDeleteDatabase(bool needDestroy, uint64_t timerNumber, bool needRecover);
    #ifdef MULTI_ACCOUNT_ENABLE
    int32_t CheckUserIdForNotify(const std::shared_ptr<TimerInfo> &timer);
    #endif
    bool NotifyWantAgent(const std::shared_ptr<TimerInfo> &timer);
    bool CheckAllowWhileIdle(const std::shared_ptr<TimerInfo> &alarm);
    bool AdjustDeliveryTimeBasedOnDeviceIdle(const std::shared_ptr<TimerInfo> &alarm);
    bool AdjustTimersBasedOnDeviceIdle();
    void HandleRepeatTimer(const std::shared_ptr<TimerInfo> &timer, std::chrono::steady_clock::time_point nowElapsed);
    inline bool CheckNeedRecoverOnReboot(std::string bundleName, int type, bool autoRestore);
    #ifdef POWER_MANAGER_ENABLE
    void HandleRunningLock(const std::shared_ptr<Batch> &firstWakeup);
    void AddRunningLock(long long holdLockTime);
    void AddRunningLockRetry(long long holdLockTime);
    #endif

    void UpdateTimersState(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger);
    bool AdjustSingleTimer(std::shared_ptr<TimerInfo> timer);
    bool AdjustSingleTimerLocked(std::shared_ptr<TimerInfo> timer);
    void IncreaseTimerCount(int uid);
    void DecreaseTimerCount(int uid);
    void CheckTimerCount();
    void ShowTimerCountByUid(int count);
    void AddTimerName(int uid, std::string name, uint64_t timerId);
    void DeleteTimerName(int uid, std::string name, uint64_t timerId);
    #ifdef SET_AUTO_REBOOT_ENABLE
    bool IsPowerOnTimer(std::shared_ptr<TimerInfo> timerInfo);
    void DeleteTimerFromPowerOnTimerListById(uint64_t timerId);
    void ReschedulePowerOnTimerLocked(bool isShutDown);
    #endif

    std::map<uint64_t, std::shared_ptr<TimerEntry>> timerEntryMap_;
    // vector<uid, count>
    std::vector<std::pair<int32_t, int32_t>> timerCount_;
    // <uid, <name, timerid>>
    std::map<int32_t, std::map<std::string, uint64_t>> timerNameMap_;
    std::default_random_engine random_;
    std::atomic_bool runFlag_;
    std::shared_ptr<TimerHandler> handler_;
    std::unique_ptr<std::thread> alarmThread_;
    std::vector<std::shared_ptr<Batch>> alarmBatches_;
    std::mutex mutex_;
    std::mutex entryMapMutex_;
    std::mutex timerMapMutex_;
    std::chrono::system_clock::time_point lastTimeChangeClockTime_;
    std::chrono::steady_clock::time_point lastTimeChangeRealtime_;
    static std::mutex instanceLock_;
    static TimerManager* instance_;

    std::vector<std::shared_ptr<TimerInfo>> pendingDelayTimers_;
    // map<timerId, original trigger time> for delayed timers
    std::map<uint64_t, std::chrono::steady_clock::time_point> delayedTimers_;
    // idle timer
    std::shared_ptr<TimerInfo> mPendingIdleUntil_;
    std::array<int64_t, TIMER_TYPE_BUTT> lastSetTime_ = {0};
    bool adjustPolicy_ = false;
    uint32_t adjustInterval_ = 0;
    uint32_t adjustDelta_ = 0;
    int64_t timerOutOfRangeTimes_ = 0;
    std::chrono::steady_clock::time_point lastTimerOutOfRangeTime_;
    #ifdef SET_AUTO_REBOOT_ENABLE
    std::vector<std::shared_ptr<TimerInfo>> powerOnTriggerTimerList_;
    std::vector<std::string> powerOnApps_;
    #endif
    #ifdef POWER_MANAGER_ENABLE
    std::mutex runningLockMutex_;
    std::shared_ptr<PowerMgr::RunningLock> runningLock_;
    int64_t lockExpiredTime_ = 0;
    #endif
}; // timer_manager
} // MiscServices
} // OHOS

#endif