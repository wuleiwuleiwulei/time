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

#ifndef SERVICES_INCLUDE_TIME_SERVICES_H
#define SERVICES_INCLUDE_TIME_SERVICES_H

#include "system_ability.h"
#ifdef HIDUMPER_ENABLE
#include "time_cmd_dispatcher.h"
#include "time_cmd_parse.h"
#endif
#include "time_service_notify.h"
#include "time_service_stub.h"
#include "timer_manager.h"
#include "shutdown/sync_shutdown_callback_stub.h"
#include "shutdown/shutdown_client.h"
#include "cjson_helper.h"
#include "ipc_skeleton.h"
#include "time_permission.h"
#include "time_sysevent.h"
#include "itimer_info.h"
#ifdef RDB_ENABLE
#include "rdb_helper.h"
#include "timer_database.h"
#endif

namespace OHOS {
namespace MiscServices {
enum class ServiceRunningState : int8_t { STATE_NOT_START, STATE_RUNNING };

class TimeSystemAbility : public SystemAbility, public TimeServiceStub {
    DECLARE_SYSTEM_ABILITY(TimeSystemAbility);

public:
    #ifdef SET_AUTO_REBOOT_ENABLE
    class TimePowerStateListener : public OHOS::PowerMgr::SyncShutdownCallbackStub {
    public:
        ~TimePowerStateListener() override = default;
        void OnSyncShutdown() override;
    };
    #endif
    DISALLOW_COPY_AND_MOVE(TimeSystemAbility);
    TimeSystemAbility(int32_t systemAbilityId, bool runOnCreate);
    TimeSystemAbility();
    ~TimeSystemAbility();
    static sptr<TimeSystemAbility> GetInstance();
    int32_t SetTime(int64_t time, int8_t apiVersion = APIVersion::API_VERSION_7) override;
    int32_t SetTimeInner(int64_t time, int8_t apiVersion = APIVersion::API_VERSION_7);
    bool SetRealTime(int64_t time);
    int32_t SetAutoTime(bool autoTime) override;
    int32_t SetTimeZone(const std::string &timeZoneId, int8_t apiVersion = APIVersion::API_VERSION_7) override;
    int32_t SetTimeZoneInner(const std::string &timeZoneId, int8_t apiVersion = APIVersion::API_VERSION_7);
    int32_t GetTimeZone(std::string &timeZoneId) override;
    int32_t GetThreadTimeMs(int64_t &time) override;
    int32_t GetThreadTimeNs(int64_t &time) override;
    int32_t CreateTimer(const std::string &name, int type, bool repeat, bool disposable, bool autoRestore,
                        uint64_t interval, const OHOS::AbilityRuntime::WantAgent::WantAgent &wantAgent,
                        const sptr<IRemoteObject> &timerCallback, uint64_t &timerId) override;
    int32_t CreateTimerWithoutWA(const std::string &name, int type, bool repeat, bool disposable, bool autoRestore,
                        uint64_t interval, const sptr<IRemoteObject> &timerCallback, uint64_t &timerId) override;
    int32_t CreateTimer(const std::shared_ptr<ITimerInfo> &timerOptions, const sptr<IRemoteObject> &obj,
        uint64_t &timerId);
    int32_t CreateTimer(TimerPara &paras, std::function<int32_t (const uint64_t)> callback, uint64_t &timerId);
    int32_t StartTimer(uint64_t timerId, uint64_t triggerTime) override;
    int32_t StopTimer(uint64_t timerId) override;
    int32_t DestroyTimer(uint64_t timerId) override;
    int32_t DestroyTimerAsync(uint64_t timerId) override;
    int32_t ProxyTimer(int32_t uid, const std::vector<int>& pidList, bool isProxy, bool needRetrigger) override;
    int32_t AdjustTimer(bool isAdjust, uint32_t interval, uint32_t delta) override;
    int32_t SetTimerExemption(const std::vector<std::string> &nameArr, bool isExemption) override;
    int32_t ResetAllProxy() override;
    int32_t GetNtpTimeMs(int64_t &time) override;
    int32_t GetRealTimeMs(int64_t &time) override;
    int32_t SetAdjustPolicy(const std::unordered_map<std::string, uint32_t> &policyMap) override;
    #ifdef HIDUMPER_ENABLE
    int Dump(int fd, const std::vector<std::u16string> &args) override;
    void DumpAllTimeInfo(int fd, const std::vector<std::string> &input);
    void DumpTimerInfo(int fd, const std::vector<std::string> &input);
    void DumpTimerInfoById(int fd, const std::vector<std::string> &input);
    void DumpTimerTriggerById(int fd, const std::vector<std::string> &input);
    void DumpIdleTimerInfo(int fd, const std::vector<std::string> &input);
    void DumpProxyTimerInfo(int fd, const std::vector<std::string> &input);
    void DumpUidTimerMapInfo(int fd, const std::vector<std::string> &input);
    void DumpPidTimerMapInfo(int fd, const std::vector<std::string> &input);
    void DumpProxyDelayTime(int fd, const std::vector<std::string> &input);
    void DumpAdjustTime(int fd, const std::vector<std::string> &input);
    void InitDumpCmd();
    #endif
    void RegisterCommonEventSubscriber();
    bool RecoverTimer();
    void RecoverTimerCjson(std::string tableName);

protected:
    void OnStart() override;
    void OnStop() override;
    void OnAddSystemAbility(int32_t systemAbilityId, const std::string &deviceId) override;

private:
    class RSSSaDeathRecipient : public IRemoteObject::DeathRecipient {
    public:
        explicit RSSSaDeathRecipient()= default;;
        ~RSSSaDeathRecipient() override = default;
        void OnRemoteDied(const wptr<IRemoteObject> &object) override;
    };

    int32_t Init();
    void ParseTimerPara(const std::shared_ptr<ITimerInfo> &timerOptions, TimerPara &paras);
    int32_t CheckTimerPara(const DatabaseType type, const TimerPara &paras);
    bool GetTimeByClockId(clockid_t clockId, struct timespec &tv);
    int SetRtcTime(time_t sec);
    bool CheckRtc(const std::string &rtcPath, uint64_t rtcId);
    int GetWallClockRtcId();
    void RegisterRSSDeathCallback();
    void RegisterSubscriber();
    std::shared_ptr<TimerEntry> GetEntry(cJSON* obj, bool autoRestore);
    #ifdef MULTI_ACCOUNT_ENABLE
    void RegisterOsAccountSubscriber();
    #endif
    bool IsValidTime(int64_t time);
    #ifdef RDB_ENABLE
    void CjsonIntoDatabase(cJSON* resultSet, bool autoRestore, const std::string &table);
    void RecoverTimerInner(std::shared_ptr<OHOS::NativeRdb::ResultSet> resultSet, bool autoRestore);
    #else
    void RecoverTimerInnerCjson(cJSON* resultSet, bool autoRestore, std::string tableName);
    #endif
    #ifdef SET_AUTO_REBOOT_ENABLE
    void RegisterPowerStateListener();
    #endif

    ServiceRunningState state_;
    static std::mutex instanceLock_;
    static sptr<TimeSystemAbility> instance_;
    const int rtcId;
    sptr<RSSSaDeathRecipient> deathRecipient_ {};
};
} // namespace MiscServices
} // namespace OHOS
#endif // SERVICES_INCLUDE_TIME_SERVICES_H