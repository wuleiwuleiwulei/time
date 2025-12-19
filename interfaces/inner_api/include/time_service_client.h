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

#ifndef SERVICES_INCLUDE_TIME_SERVICES_MANAGER_H
#define SERVICES_INCLUDE_TIME_SERVICES_MANAGER_H

#include <sstream>

#include "itimer_info.h"
#include "itime_service.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#include "system_ability_status_change_stub.h"

namespace OHOS {
namespace MiscServices {
class TimeServiceClient : public RefBase {
public:
    DISALLOW_COPY_AND_MOVE(TimeServiceClient);
    TIME_API static sptr<TimeServiceClient> GetInstance();

    /**
     * @brief Set time
     *
     * This api is used to set system time.
     *
     * @param UTC time in milliseconds.
     * @return true on success, false on failure.
     */
    TIME_API bool SetTime(int64_t milliseconds);

    /**
     * @brief Set system time
     *
     * This api is used to set system time.
     *
     * @param UTC time in milliseconds.
     * @param error code.
     * @return true on success, false on failure.
     */
    TIME_API bool SetTime(int64_t milliseconds, int32_t &code);

    /**
     * @brief Set system time
     *
     * This api is used to set system time.
     *
     * @param UTC time in milliseconds.
     * @return error code.
     */
    TIME_API int32_t SetTimeV9(int64_t time);

    /**
     * @brief Set autotime status
     *
     * This api is used to set autotime status.
     *
     * @param status status of autotime.
     * @return error code.
     */
    TIME_API int32_t SetAutoTime(bool autoTime);

    /**
     * @brief Set Timezone
     *
     * This api is used to set timezone.
     *
     * @param const std::string time zone. example: "Beijing, China".
     * @return true on success, false on failure.
     */
    TIME_API bool SetTimeZone(const std::string &timeZoneId);

    /**
     * @brief Set Timezone
     *
     * This api is used to set timezone.
     *
     * @param const std::string time zone. example: "Beijing, China".
     * @param error code.
     * @return true on success, false on failure.
     */
    TIME_API bool SetTimeZone(const std::string &timezoneId, int32_t &code);

    /**
     * @brief Set Timezone
     *
     * This api is used to set timezone.
     *
     * @param const std::string time zone. example: "Beijing, China".
     * @return error code.
     */
    TIME_API int32_t SetTimeZoneV9(const std::string &timezoneId);

    /**
     * @brief Get Timezone
     *
     * This api is used to get current system timezone.
     *
     * @return time zone example: "Beijing, China", if result length == 0 on failed.
     */
    TIME_API std::string GetTimeZone();

    /**
     * @brief Get Timezone
     *
     * This api is used to get current system timezone.
     *
     * @param The current system time zone, example: "Beijing, China", if failed the value is nullptr.
     * @return error code.
     */
    TIME_API int32_t GetTimeZone(std::string &timezoneId);

    /**
     * @brief GetWallTimeMs
     *
     * Get the wall time(the UTC time from 1970 0H:0M:0S) in milliseconds
     *
     * @return milliseconds in wall time, ret < 0 on failed.
     */
    TIME_API int64_t GetWallTimeMs();

    /**
     * @brief GetWallTimeMs
     *
     * Get the wall time(the UTC time from 1970 0H:0M:0S) in milliseconds.
     *
     * @param milliseconds in wall time.
     * @return error code.
     */
    TIME_API int32_t GetWallTimeMs(int64_t &time);

    /**
     * @brief GetWallTimeNs
     *
     * Get the wall time(the UTC time from 1970 0H:0M:0S) in nanoseconds.
     *
     * @return nanoseconds in wall time, ret < 0 on failed.
     */
    TIME_API int64_t GetWallTimeNs();

    /**
     * @brief GetWallTimeNs
     *
     * Get the wall time(the UTC time from 1970 0H:0M:0S) in nanoseconds.
     *
     * @param nanoseconds in wall time.
     * @return error code.
     */
    TIME_API int32_t GetWallTimeNs(int64_t &time);

    /**
     * @brief GetBootTimeMs
     *
     * Get the time since boot(include time spent in sleep) in milliseconds.
     *
     * @return milliseconds in boot time, ret < 0 on failed.
     */
    TIME_API int64_t GetBootTimeMs();

    /**
     * @brief GetBootTimeMs
     *
     * Get the time since boot(include time spent in sleep) in milliseconds.
     *
     * @param milliseconds in boot time.
     * @param error code.
     */
    TIME_API int32_t GetBootTimeMs(int64_t &time);

    /**
     * @brief GetBootTimeNs
     *
     * Get the time since boot(include time spent in sleep) in nanoseconds.
     *
     * @return nanoseconds in boot time, ret < 0 on failed.
     */
    TIME_API int64_t GetBootTimeNs();

    /**
     * @brief GetBootTimeNs
     *
     * Get the time since boot(include time spent in sleep) in nanoseconds.
     *
     * @param nanoseconds in boot time.
     * @return error code.
     */
    TIME_API int32_t GetBootTimeNs(int64_t &time);

    /**
     * @brief GetMonotonicTimeMs
     *
     * Get the time since boot(exclude time spent in sleep) in milliseconds.
     *
     * @return milliseconds in Monotonic time, ret < 0 on failed.
     */
    TIME_API int64_t GetMonotonicTimeMs();

    /**
     * @brief GetMonotonicTimeMs
     *
     * Get the time since boot(exclude time spent in sleep) in milliseconds.
     *
     * @param milliseconds in Monotonic time.
     * @return error code.
     */
    TIME_API int32_t GetMonotonicTimeMs(int64_t &time);

    /**
     * @brief GetMonotonicTimeNs
     *
     * Get the time since boot(exclude time spent in sleep) in nanoseconds.
     *
     * @return nanoseconds in Monotonic time, ret < 0 on failed.
     */
    TIME_API int64_t GetMonotonicTimeNs();

    /**
     * @brief GetMonotonicTimeNs
     *
     * Get the time since boot(exclude time spent in sleep) in nanoseconds.
     *
     * @param nanoseconds in Monotonic time.
     * @return error code.
     */
    TIME_API int32_t GetMonotonicTimeNs(int64_t &time);

    /**
     * @brief GetThreadTimeMs
     *
     * Get the thread time in milliseconds.
     *
     * @return milliseconds in Thread-specific CPU-time, ret < 0 on failed.
     */
    TIME_API int64_t GetThreadTimeMs();

    /**
     * @brief GetThreadTimeMs
     *
     * Get the thread time in milliseconds.
     *
     * @param the Thread-specific CPU-time in milliseconds.
     * @return error code.
     */
    TIME_API int32_t GetThreadTimeMs(int64_t &time);

    /**
     * @brief GetThreadTimeNs
     *
     * Get the thread time in nanoseconds.
     *
     * @return nanoseconds in Thread-specific CPU-time, ret < 0 on failed.
     */
    TIME_API int64_t GetThreadTimeNs();

    /**
     * @brief GetThreadTimeNs
     *
     * Get the thread time in nanoseconds.
     *
     * @param get the Thread-specific CPU-time in nanoseconds.
     * @return error code.
     */
    TIME_API int32_t GetThreadTimeNs(int64_t &time);

    /**
     * @brief CreateTimer
     *
     * Creates a timer.
     *
     * @param indicates the timer options.
     * @return timer id.
     */
    TIME_API uint64_t CreateTimer(std::shared_ptr<ITimerInfo> timerInfo);

    /**
     * @brief Create Timer
     *
     * Creates a timer.
     *
     * @param indicates the timer options.
     * @param timer id.
     * @return error code.
     */
    TIME_API int32_t CreateTimerV9(std::shared_ptr<ITimerInfo> timerOptions, uint64_t &timerId);

    /**
     * @brief StartTimer
     *
     * Starts a timer.
     *
     * @param indicate timerId
     * @param trigger time
     * @return true on success, false on failure.
     */
    TIME_API bool StartTimer(uint64_t timerId, uint64_t triggerTime);

    /**
     * @brief Start Timer
     *
     * Starts a timer.
     *
     * @param indicate timerId.
     * @param trigger time.
     * @return true on success, false on failure.
     */
    TIME_API int32_t StartTimerV9(uint64_t timerId, uint64_t triggerTime);

    /**
     * @brief Stop Timer
     *
     * Starts a timer.
     *
     * @param indicate timerId.
     * @return true on success, false on failure.
     */
    TIME_API bool StopTimer(uint64_t timerId);

    /**
     * @brief StopTimer
     *
     * Stops a timer.
     *
     * @param indicate timerId.
     * @return error code.
     */
    TIME_API int32_t StopTimerV9(uint64_t timerId);

    /**
     * @brief DestroyTimer
     *
     * Destroy a timer.
     *
     * @param indicate timerId.
     * @return true on success, false on failure.
     */
    TIME_API bool DestroyTimer(uint64_t timerId);

    /**
     * @brief DestroyTimerAsync
     *
     * Destroy a timer asynchronously.
     *
     * @param indicate timerId.
     * @return true on success, false on failure.
     */
    TIME_API bool DestroyTimerAsync(uint64_t timerId);

    /**
     * @brief DestroyTimer
     *
     * Destroy a timer.
     *
     * @param indicate timerId.
     * @return error code.
     */
    TIME_API int32_t DestroyTimerV9(uint64_t timerId);

    /**
     * @brief DestroyTimerAsync
     *
     * Destroy a timer asynchronously.
     *
     * @param indicate timerId.
     * @return error code.
     */
    TIME_API int32_t DestroyTimerAsyncV9(uint64_t timerId);

    /**
     * @brief ProxyTimer
     *
     * Proxy timers when apps switch to background
     *
     * @param uid the uid.
     * @param pidList the pidlist. Passing an empty vector means proxy by uid.
     *                Max size is 1024.
     * @param true if set proxy, false if remove proxy.
     * @param true if need retrigger, false if not and stop timer.
     * @return true on success, false on failure.
     */
    TIME_API bool ProxyTimer(int32_t uid, std::set<int> pidList, bool isProxy, bool needRetrigger);

    /**
     * @brief AdjustTimer
     * adjust bundle or system process timer
     * @param isAdjust true if adjust, false if not adjust.
     * @param interval adjust period in seconds.
     * @return int32_t return error code.
     */
    TIME_API int32_t AdjustTimer(bool isAdjust, uint32_t interval, uint32_t delta);

    /**
     * @brief SetTimerExemption
     * set exemption list for adjust timer
     * @param nameArr list for bundle name or proccess name.
     * @param isExemption exemption or ctrl.
     * @return int32_t return error code.
     */
    TIME_API int32_t SetTimerExemption(const std::unordered_set<std::string> &nameArr, bool isExemption);

    /**
     * @brief SetAdjustPolicy
     * set policy for adjust timer
     * @param policyMap adjust policy map.
     * @return int32_t return error code.
     */
    TIME_API int32_t SetAdjustPolicy(const std::unordered_map<std::string, uint32_t> &policyMap);

    /**
     * @brief ResetAllProxy
     *
     * Wake up all timers by proxy.
     *
     * @return bool true on success, false on failure.
     */
    TIME_API bool ResetAllProxy();

    /**
     * @brief GetNtpTimeMs
     *
     * Obtain the wall time through ntp.
     *
     * @param time the wall time(the UTC time from 1970 0H:0M:0S) in milliseconds.
     * @return int32_t return error code.
     */
    TIME_API int32_t GetNtpTimeMs(int64_t &time);

    /**
     * @brief GetRealTimeMs
     *
     * Obtain the wall time based on the last ntp time.
     *
     * @param time the wall time(the UTC time from 1970 0H:0M:0S) in milliseconds.
     * @return int32_t return error code.
     */
    TIME_API int32_t GetRealTimeMs(int64_t &time);
    void HandleRecoverMap(uint64_t timerId);
private:
    class TimeSaDeathRecipient : public IRemoteObject::DeathRecipient {
    public:
        explicit TimeSaDeathRecipient(){};
        ~TimeSaDeathRecipient() = default;
        void OnRemoteDied(const wptr<IRemoteObject> &object) override
        {
            TimeServiceClient::GetInstance()->ClearProxy();
        };

    private:
        DISALLOW_COPY_AND_MOVE(TimeSaDeathRecipient);
    };

    class TimeServiceListener : public OHOS::SystemAbilityStatusChangeStub {
        public:
            TimeServiceListener();
            ~TimeServiceListener() = default;
            virtual void OnAddSystemAbility(int32_t saId, const std::string &deviceId) override;
            virtual void OnRemoveSystemAbility(int32_t asId, const std::string &deviceId) override;
    };

    struct RecoverTimerInfo {
        std::shared_ptr<ITimerInfo> timerInfo;
        uint8_t state;
        uint64_t triggerTime;
    };

    TimeServiceClient();
    ~TimeServiceClient();
    bool SubscribeSA(sptr<ISystemAbilityManager> systemAbilityManager);
    bool ConnectService();
    bool GetTimeByClockId(clockid_t clockId, struct timespec &tv);
    void ClearProxy();
    sptr<ITimeService> GetProxy();
    void SetProxy(sptr<ITimeService> proxy);
    void CheckNameLocked(std::string name);
    int32_t ConvertErrCode(int32_t errCode);
    int32_t RecordRecoverTimerInfoMap(std::shared_ptr<ITimerInfo> timerOptions, uint64_t timerId);

    sptr<TimeServiceListener> listener_;
    static std::mutex instanceLock_;
    static sptr<TimeServiceClient> instance_;
    TIME_API std::vector<std::string> timerNameList_;
    TIME_API std::map<uint64_t, std::shared_ptr<RecoverTimerInfo>> recoverTimerInfoMap_;
    TIME_API std::mutex recoverTimerInfoLock_;
    std::mutex proxyLock_;
    std::mutex deathLock_;
    sptr<ITimeService> timeServiceProxy_;
    sptr<TimeSaDeathRecipient> deathRecipient_ {};
};
} // MiscServices
} // OHOS
#endif // SERVICES_INCLUDE_TIME_SERVICES_MANAGER_H