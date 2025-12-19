/*
 * Copyright (C) 2022-2023 Huawei Device Co., Ltd.
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

#include <memory>
#include <chrono>
#include "timer_manager.h"

#include "time_file_utils.h"
#include "timer_proxy.h"
#include "time_tick_notify.h"

#ifdef RDB_ENABLE
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_predicates.h"
#include "rdb_store.h"
#include "timer_database.h"
#endif

#ifdef DEVICE_STANDBY_ENABLE
#include "allow_type.h"
#include "standby_service_client.h"
#endif

#ifdef POWER_MANAGER_ENABLE
#include "time_system_ability.h"
#endif

#ifdef MULTI_ACCOUNT_ENABLE
#include "os_account.h"
#include "os_account_manager.h"
#endif

namespace OHOS {
namespace MiscServices {
using namespace std::chrono;
using namespace OHOS::AppExecFwk;
namespace {
constexpr uint32_t TIME_CHANGED_BITS = 16;
constexpr uint32_t TIME_CHANGED_MASK = 1 << TIME_CHANGED_BITS;
constexpr int ONE_THOUSAND = 1000;
constexpr int NANO_TO_SECOND =  1000000000;
constexpr int WANTAGENT_CODE_ELEVEN = 11;
constexpr int WANT_RETRY_TIMES = 6;
constexpr int WANT_RETRY_INTERVAL = 1;
// an error code of ipc which means peer end is dead
constexpr int PEER_END_DEAD = 29189;
constexpr int TIMER_ALARM_COUNT = 50;
constexpr int MAX_TIMER_ALARM_COUNT = 100;
constexpr int TIMER_ALRAM_INTERVAL = 60;
constexpr int TIMER_COUNT_TOP_NUM = 5;
constexpr const char* AUTO_RESTORE_TIMER_APPS = "persist.time.auto_restore_timer_apps";
#ifdef SET_AUTO_REBOOT_ENABLE
constexpr const char* SCHEDULED_POWER_ON_APPS = "persist.time.scheduled_power_on_apps";
constexpr int64_t TEN_YEARS_TO_SECOND = 10 * 365 * 24 * 60 * 60;
constexpr uint64_t TWO_MINUTES_TO_MILLI = 120000;
#endif
constexpr std::array<const char*, 2> NO_LOG_APP_LIST = { "wifi_manager_service", "telephony" };

#ifdef RDB_ENABLE
static const std::vector<std::string> ALL_DATA = { "timerId", "type", "flag", "windowLength", "interval", \
                                                   "uid", "bundleName", "wantAgent", "state", "triggerTime" };
#endif

#ifdef MULTI_ACCOUNT_ENABLE
constexpr int SYSTEM_USER_ID  = 0;
constexpr const char* TIMER_ACROSS_ACCOUNTS = "persist.time.timer_across_accounts";
#endif

#ifdef POWER_MANAGER_ENABLE
constexpr int64_t USE_LOCK_ONE_SEC_IN_NANO = 1 * NANO_TO_SECOND;
constexpr int64_t USE_LOCK_TIME_IN_NANO = 2 * NANO_TO_SECOND;
constexpr int32_t NANO_TO_MILLI = 1000000;
constexpr int64_t ONE_HUNDRED_MILLI = 100000000; // 100ms
constexpr int POWER_RETRY_TIMES = 10;
constexpr int POWER_RETRY_INTERVAL = 10000;
constexpr const char* RUNNING_LOCK_DURATION_PARAMETER = "persist.time.running_lock_duration";
static int64_t RUNNING_LOCK_DURATION = 1 * NANO_TO_SECOND;
#endif

#ifdef DEVICE_STANDBY_ENABLE
constexpr int REASON_NATIVE_API = 0;
constexpr int REASON_APP_API = 1;
#endif
}

std::mutex TimerManager::instanceLock_;
TimerManager* TimerManager::instance_ = nullptr;

extern bool AddBatchLocked(std::vector<std::shared_ptr<Batch>> &list, const std::shared_ptr<Batch> &batch);

TimerManager::TimerManager(std::shared_ptr<TimerHandler> impl)
    : random_ {static_cast<uint64_t>(time(nullptr))},
      runFlag_ {true},
      handler_ {std::move(impl)},
      lastTimeChangeClockTime_ {system_clock::time_point::min()},
      lastTimeChangeRealtime_ {steady_clock::time_point::min()},
      lastTimerOutOfRangeTime_ {steady_clock::time_point::min()}
{
    alarmThread_.reset(new std::thread([this] { this->TimerLooper(); }));
    #ifdef SET_AUTO_REBOOT_ENABLE
    powerOnApps_ = TimeFileUtils::GetParameterList(SCHEDULED_POWER_ON_APPS);
    #endif
}

TimerManager* TimerManager::GetInstance()
{
    if (instance_ == nullptr) {
        std::lock_guard<std::mutex> autoLock(instanceLock_);
        if (instance_ == nullptr) {
            auto impl = TimerHandler::Create();
            if (impl == nullptr) {
                TIME_HILOGE(TIME_MODULE_SERVICE, "Create Timer handle failed");
                return nullptr;
            }
            instance_ = new TimerManager(impl);
            std::vector<std::string> bundleList = TimeFileUtils::GetParameterList(AUTO_RESTORE_TIMER_APPS);
            if (!bundleList.empty()) {
                NEED_RECOVER_ON_REBOOT = bundleList;
            }
            #ifdef POWER_MANAGER_ENABLE
            RUNNING_LOCK_DURATION = TimeFileUtils::GetIntParameter(RUNNING_LOCK_DURATION_PARAMETER,
                                                                   USE_LOCK_ONE_SEC_IN_NANO);
            #endif
        }
    }
    if (instance_ == nullptr) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Create Timer manager failed");
    }
    return instance_;
}

#ifdef RDB_ENABLE
OHOS::NativeRdb::ValuesBucket GetInsertValues(std::shared_ptr<TimerEntry> timerInfo, TimerPara &paras)
{
    OHOS::NativeRdb::ValuesBucket insertValues;
    insertValues.PutLong("timerId", timerInfo->id);
    insertValues.PutInt("type", paras.timerType);
    insertValues.PutInt("flag", paras.flag);
    insertValues.PutLong("windowLength", paras.windowLength);
    insertValues.PutLong("interval", paras.interval);
    insertValues.PutInt("uid", timerInfo->uid);
    insertValues.PutString("bundleName", timerInfo->bundleName);
    insertValues.PutString("wantAgent",
        OHOS::AbilityRuntime::WantAgent::WantAgentHelper::ToString(timerInfo->wantAgent));
    insertValues.PutInt("state", 0);
    insertValues.PutLong("triggerTime", 0);
    insertValues.PutInt("pid", timerInfo->pid);
    insertValues.PutString("name", timerInfo->name);
    return insertValues;
}
#endif

// needs to acquire the lock `entryMapMutex_` before calling this method
void TimerManager::AddTimerName(int uid, std::string name, uint64_t timerId)
{
    if (timerNameMap_.find(uid) == timerNameMap_.end() || timerNameMap_[uid].find(name) == timerNameMap_[uid].end()) {
        timerNameMap_[uid][name] = timerId;
        TIME_SIMPLIFY_HILOGI(TIME_MODULE_SERVICE, "%{public}s:%{public}" PRId64 "", name.c_str(), timerId);
        return;
    }
    auto oldTimerId = timerNameMap_[uid][name];
    if (timerId != oldTimerId) {
        bool needRecover =  false;
        StopTimerInnerLocked(true, oldTimerId, needRecover);
        UpdateOrDeleteDatabase(true, oldTimerId, needRecover);
        timerNameMap_[uid][name] = timerId;
        TIME_HILOGW(TIME_MODULE_SERVICE, "create:%{public}" PRId64 " name:%{public}s in %{public}d already exist "
            "destory:%{public}" PRId64 "", timerId, name.c_str(), uid, oldTimerId);
    }
    return;
}

// needs to acquire the lock `entryMapMutex_` before calling this method
void TimerManager::DeleteTimerName(int uid, std::string name, uint64_t timerId)
{
    auto nameIter = timerNameMap_.find(uid);
    if (nameIter == timerNameMap_.end()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "NameMap has no uid %{public}d", uid);
        return;
    }
    auto timerIter = nameIter->second.find(name);
    if (timerIter == nameIter->second.end()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "NameMap has no name:%{public}s uid:%{public}d", name.c_str(), uid);
        return;
    }
    if (timerIter->second == timerId) {
        timerNameMap_[uid].erase(timerIter);
        return;
    }
    TIME_HILOGW(TIME_MODULE_SERVICE,
        "timer %{public}" PRId64 " not exist in map, name:%{public}s uid%{public}d", timerId, name.c_str(), uid);
}

int32_t TimerManager::CreateTimer(TimerPara &paras,
                                  std::function<int32_t (const uint64_t)> callback,
                                  std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> wantAgent,
                                  int uid,
                                  int pid,
                                  uint64_t &timerId,
                                  DatabaseType type)
{
    TIME_HILOGD(TIME_MODULE_SERVICE,
                "Create timer:%{public}d windowLength:%{public}" PRId64 "interval:%{public}" PRId64 "flag:%{public}u"
                "uid:%{public}d pid:%{public}d timerId:%{public}" PRId64 "", paras.timerType, paras.windowLength,
                paras.interval, paras.flag, IPCSkeleton::GetCallingUid(), IPCSkeleton::GetCallingPid(), timerId);
    std::string bundleName = TimeFileUtils::GetBundleNameByTokenID(IPCSkeleton::GetCallingTokenID());
    if (bundleName.empty()) {
        bundleName = TimeFileUtils::GetNameByPid(IPCSkeleton::GetCallingPid());
    }
    auto timerName = paras.name;
    std::shared_ptr<TimerEntry> timerInfo;
    {
        std::lock_guard<std::mutex> lock(entryMapMutex_);
        while (timerId == 0) {
            // random_() needs to be protected in a lock.
            timerId = random_();
        }
        timerInfo = std::make_shared<TimerEntry>(TimerEntry {timerName, timerId, paras.timerType, paras.windowLength,
            paras.interval, paras.flag, paras.autoRestore, std::move(callback), wantAgent, uid, pid, bundleName});
        if (timerEntryMap_.find(timerId) == timerEntryMap_.end()) {
            IncreaseTimerCount(uid);
        }
        timerEntryMap_.insert(std::make_pair(timerId, timerInfo));
        if (timerName != "") {
            AddTimerName(uid, timerName, timerId);
        }
    }
    if (type == NOT_STORE) {
        return E_TIME_OK;
    }
    auto tableName = (CheckNeedRecoverOnReboot(bundleName, paras.timerType, paras.autoRestore)
                      ? HOLD_ON_REBOOT
                      : DROP_ON_REBOOT);
    #ifdef RDB_ENABLE
    TimeDatabase::GetInstance().Insert(tableName, GetInsertValues(timerInfo, paras));
    #else
    CjsonHelper::GetInstance().Insert(tableName, timerInfo);
    #endif
    return E_TIME_OK;
}

void TimerManager::ReCreateTimer(uint64_t timerId, std::shared_ptr<TimerEntry> timerInfo)
{
    std::lock_guard<std::mutex> lock(entryMapMutex_);
    timerEntryMap_.insert(std::make_pair(timerId, timerInfo));
    if (timerInfo->name != "") {
        AddTimerName(timerInfo->uid, timerInfo->name, timerId);
    }
    IncreaseTimerCount(timerInfo->uid);
}

int32_t TimerManager::StartTimer(uint64_t timerId, uint64_t triggerTime)
{
    std::shared_ptr<TimerEntry> timerInfo;
    {
        std::lock_guard<std::mutex> lock(entryMapMutex_);
        auto it = timerEntryMap_.find(timerId);
        if (it == timerEntryMap_.end()) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "id not found:%{public}" PRId64 "", timerId);
            return E_TIME_NOT_FOUND;
        }
        timerInfo = it->second;
        if (timerId != TimeTickNotify::GetInstance().GetTickTimerId()) {
            TIME_SIMPLIFY_HILOGI(TIME_MODULE_SERVICE, "start:%{public}" PRIu64 " typ:%{public}d "
                "int:%{public}" PRId64 " trig:%{public}s pid:%{public}d", timerId, timerInfo->type, timerInfo->interval,
                std::to_string(triggerTime).c_str(), IPCSkeleton::GetCallingPid());
        }
        {
            // To prevent the same ID from being started repeatedly,
            // the later start overwrites the earlier start.
            std::lock_guard<std::mutex> lock(mutex_);
            RemoveLocked(timerId, false);
        }
        auto alarm = TimerInfo::CreateTimerInfo(timerInfo->name, timerInfo->id, timerInfo->type, triggerTime,
            timerInfo->windowLength, timerInfo->interval, timerInfo->flag, timerInfo->autoRestore, timerInfo->callback,
            timerInfo->wantAgent, timerInfo->uid, timerInfo->pid, timerInfo->bundleName);
        std::lock_guard<std::mutex> lockGuard(mutex_);
        SetHandlerLocked(alarm);
    }
    if (timerInfo->wantAgent) {
        auto tableName = (CheckNeedRecoverOnReboot(timerInfo->bundleName, timerInfo->type, timerInfo->autoRestore)
            ? HOLD_ON_REBOOT
            : DROP_ON_REBOOT);
        #ifdef RDB_ENABLE
        OHOS::NativeRdb::ValuesBucket values;
        values.PutInt("state", 1);
        values.PutLong("triggerTime", static_cast<int64_t>(triggerTime));
        OHOS::NativeRdb::RdbPredicates rdbPredicates(tableName);
        rdbPredicates.EqualTo("state", 0)->And()->EqualTo("timerId", static_cast<int64_t>(timerId));
        TimeDatabase::GetInstance().Update(values, rdbPredicates);
        #else
        CjsonHelper::GetInstance().UpdateTrigger(tableName, static_cast<int64_t>(timerId),
            static_cast<int64_t>(triggerTime));
        #endif
    }
    return E_TIME_OK;
}

#ifndef RDB_ENABLE
int32_t TimerManager::StartTimerGroup(std::vector<std::pair<uint64_t, uint64_t>> timerVec, std::string tableName)
{
    std::lock_guard<std::mutex> lock(entryMapMutex_);
    for (auto iter = timerVec.begin(); iter != timerVec.end(); ++iter) {
        uint64_t timerId = iter->first;
        uint64_t triggerTime = iter->second;
        std::shared_ptr<TimerEntry> timerInfo;
        auto it = timerEntryMap_.find(timerId);
        if (it == timerEntryMap_.end()) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "Timer id not found:%{public}" PRId64 "", timerId);
            continue;
        }
        timerInfo = it->second;
        TIME_SIMPLIFY_HILOGI(TIME_MODULE_SERVICE, "start:%{public}" PRIu64 " typ:%{public}d "
            "int:%{public}" PRId64 " trig:%{public}s pid:%{public}d", timerId, timerInfo->type, timerInfo->interval,
            std::to_string(triggerTime).c_str(), IPCSkeleton::GetCallingPid());
        {
            // To prevent the same ID from being started repeatedly,
            // the later start overwrites the earlier start
            std::lock_guard<std::mutex> lock(mutex_);
            RemoveLocked(timerId, false);
        }
        auto alarm = TimerInfo::CreateTimerInfo(timerInfo->name, timerInfo->id, timerInfo->type, triggerTime,
            timerInfo->windowLength, timerInfo->interval, timerInfo->flag, timerInfo->autoRestore, timerInfo->callback,
            timerInfo->wantAgent, timerInfo->uid, timerInfo->pid, timerInfo->bundleName);
        std::lock_guard<std::mutex> lockGuard(mutex_);
        SetHandlerLocked(alarm);
    }
    CjsonHelper::GetInstance().UpdateTriggerGroup(tableName, timerVec);
    return E_TIME_OK;
}
#endif

void TimerManager::IncreaseTimerCount(int uid)
{
    auto it = std::find_if(timerCount_.begin(), timerCount_.end(),
        [uid](const std::pair<int32_t, size_t>& pair) {
            return pair.first == uid;
        });
    if (it == timerCount_.end()) {
        timerCount_.push_back(std::make_pair(uid, 1));
    } else {
        it->second++;
    }
    CheckTimerCount();
}

void TimerManager::DecreaseTimerCount(int uid)
{
    auto it = std::find_if(timerCount_.begin(), timerCount_.end(),
        [uid](const std::pair<int32_t, size_t>& pair) {
            return pair.first == uid;
        });
    if (it == timerCount_.end()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "uid:%{public}d has no timer", uid);
    } else {
        it->second--;
    }
}

void TimerManager::CheckTimerCount()
{
    steady_clock::time_point bootTimePoint = TimeUtils::GetBootTimeNs();
    int count = static_cast<int>(timerEntryMap_.size());
    if (count > (timerOutOfRangeTimes_ + 1) * TIMER_ALARM_COUNT) {
        timerOutOfRangeTimes_ += 1;
        ShowTimerCountByUid(count);
        lastTimerOutOfRangeTime_ = bootTimePoint;
        return;
    }
    auto currentBootTime = bootTimePoint;
    if (count > MAX_TIMER_ALARM_COUNT &&
        currentBootTime - lastTimerOutOfRangeTime_ > std::chrono::minutes(TIMER_ALRAM_INTERVAL)) {
        ShowTimerCountByUid(count);
        lastTimerOutOfRangeTime_ = currentBootTime;
        return;
    }
}

void TimerManager::ShowTimerCountByUid(int count)
{
    std::string uidStr = "";
    std::string countStr = "";
    int uidArr[TIMER_COUNT_TOP_NUM];
    int createTimerCountArr[TIMER_COUNT_TOP_NUM];
    int startTimerCountArr[TIMER_COUNT_TOP_NUM];
    auto size = static_cast<int>(timerCount_.size());
    std::sort(timerCount_.begin(), timerCount_.end(),
        [](const std::pair<int32_t, int32_t>& a, const std::pair<int32_t, int32_t>& b) {
            return a.second > b.second;
        });
    auto limitedSize = (size > TIMER_COUNT_TOP_NUM) ? TIMER_COUNT_TOP_NUM : size;
    int index = 0;
    for (auto it = timerCount_.begin(); it != timerCount_.begin() + limitedSize; ++it) {
        int uid = it->first;
        int createTimerCount = it->second;
        uidStr = uidStr + std::to_string(uid) + " ";
        countStr = countStr + std::to_string(createTimerCount) + " ";
        uidArr[index] = uid;
        createTimerCountArr[index] = createTimerCount;
        startTimerCountArr[index] = TimerProxy::GetInstance().CountUidTimerMapByUid(uid);
        ++index;
    }
    TimerCountStaticReporter(count, uidArr, createTimerCountArr, startTimerCountArr);
    TIME_HILOGI(TIME_MODULE_SERVICE, "Top uid:[%{public}s], nums:[%{public}s]", uidStr.c_str(), countStr.c_str());
}

int32_t TimerManager::StopTimer(uint64_t timerId)
{
    return StopTimerInner(timerId, false);
}

int32_t TimerManager::DestroyTimer(uint64_t timerId)
{
    return StopTimerInner(timerId, true);
}

int32_t TimerManager::StopTimerInner(uint64_t timerNumber, bool needDestroy)
{
    if (needDestroy) {
        TIME_SIMPLIFY_HILOGI(TIME_MODULE_SERVICE, "drop:%{public}" PRId64 "", timerNumber);
    } else {
        TIME_SIMPLIFY_HILOGI(TIME_MODULE_SERVICE, "stop:%{public}" PRId64 "", timerNumber);
    }
    int32_t ret;
    bool needRecover = false;
    {
        std::lock_guard<std::mutex> lock(entryMapMutex_);
        ret = StopTimerInnerLocked(needDestroy, timerNumber, needRecover);
    }
    UpdateOrDeleteDatabase(needDestroy, timerNumber, needRecover);
    return ret;
}

// needs to acquire the lock `entryMapMutex_` before calling this method
int32_t TimerManager::StopTimerInnerLocked(bool needDestroy, uint64_t timerNumber, bool &needRecover)
{
    auto it = timerEntryMap_.find(timerNumber);
    if (it == timerEntryMap_.end()) {
        TIME_HILOGW(TIME_MODULE_SERVICE, "timer not exist");
        return E_TIME_NOT_FOUND;
    }
    RemoveHandler(timerNumber);
    TimerProxy::GetInstance().EraseTimerFromProxyTimerMap(timerNumber, it->second->uid, it->second->pid);
    needRecover = CheckNeedRecoverOnReboot(it->second->bundleName, it->second->type, it->second->autoRestore);
    if (needDestroy) {
        auto uid = it->second->uid;
        auto name = it->second->name;
        timerEntryMap_.erase(it);
        DecreaseTimerCount(uid);
        if (name != "") {
            DeleteTimerName(uid, name, timerNumber);
        }
    }
    return E_TIME_OK;
}

void TimerManager::UpdateOrDeleteDatabase(bool needDestroy, uint64_t timerNumber, bool needRecover)
{
    auto tableName = (needRecover ? HOLD_ON_REBOOT : DROP_ON_REBOOT);
    if (needDestroy) {
        #ifdef RDB_ENABLE
        OHOS::NativeRdb::RdbPredicates rdbPredicatesDelete(tableName);
        rdbPredicatesDelete.EqualTo("timerId", static_cast<int64_t>(timerNumber));
        TimeDatabase::GetInstance().Delete(rdbPredicatesDelete);
        #else
        CjsonHelper::GetInstance().Delete(tableName, static_cast<int64_t>(timerNumber));
        #endif
    } else {
        #ifdef RDB_ENABLE
        OHOS::NativeRdb::ValuesBucket values;
        values.PutInt("state", 0);
        OHOS::NativeRdb::RdbPredicates rdbPredicates(tableName);
        rdbPredicates.EqualTo("state", 1)->And()->EqualTo("timerId", static_cast<int64_t>(timerNumber));
        TimeDatabase::GetInstance().Update(values, rdbPredicates);
        #else
        CjsonHelper::GetInstance().UpdateState(tableName, static_cast<int64_t>(timerNumber));
        #endif
    }
}

void TimerManager::SetHandlerLocked(std::shared_ptr<TimerInfo> alarm)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start id:%{public}" PRId64 "", alarm->id);
    auto bootTimePoint = TimeUtils::GetBootTimeNs();
    if (TimerProxy::GetInstance().IsProxy(alarm->uid, 0)) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "Timer already proxy, uid=%{public}d id=%{public}" PRId64 "",
            alarm->uid, alarm->id);
        TimerProxy::GetInstance().RecordProxyTimerMap(alarm, false);
        alarm->ProxyTimer(bootTimePoint, milliseconds(TimerProxy::GetInstance().GetProxyDelayTime()));
    }
    if (TimerProxy::GetInstance().IsProxy(alarm->uid, alarm->pid)) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "Timer already proxy, pid=%{public}d id=%{public}" PRId64 "",
            alarm->pid, alarm->id);
        TimerProxy::GetInstance().RecordProxyTimerMap(alarm, true);
        alarm->ProxyTimer(bootTimePoint, milliseconds(TimerProxy::GetInstance().GetProxyDelayTime()));
    }

    SetHandlerLocked(alarm, false, false);
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
}

void TimerManager::RemoveHandler(uint64_t id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    RemoveLocked(id, true);
    TimerProxy::GetInstance().RemoveUidTimerMap(id);
}

// needs to acquire the lock `mutex_` before calling this method
void TimerManager::RemoveLocked(uint64_t id, bool needReschedule)
{
    auto whichAlarms = [id](const TimerInfo &timer) {
        return timer.id == id;
    };
    bool didRemove = false;
    for (auto it = alarmBatches_.begin(); it != alarmBatches_.end();) {
        auto batch = *it;
        didRemove = batch->Remove(whichAlarms);
        if (didRemove) {
            TIME_SIMPLIFY_HILOGI(TIME_MODULE_SERVICE, "remove:%{public}" PRIu64 "", id);
            it = alarmBatches_.erase(it);
            if (batch->Size() != 0) {
                AddBatchLocked(alarmBatches_, batch);
            }
            break;
        }
        ++it;
    }
    pendingDelayTimers_.erase(remove_if(pendingDelayTimers_.begin(), pendingDelayTimers_.end(),
        [id](const std::shared_ptr<TimerInfo> &timer) { return timer->id == id; }), pendingDelayTimers_.end());
    delayedTimers_.erase(id);
    if (mPendingIdleUntil_ != nullptr && id == mPendingIdleUntil_->id) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "Idle alarm removed");
        mPendingIdleUntil_ = nullptr;
        bool isAdjust = AdjustTimersBasedOnDeviceIdle();
        delayedTimers_.clear();
        for (const auto &pendingTimer : pendingDelayTimers_) {
            TIME_HILOGI(TIME_MODULE_SERVICE, "Set timer from delay list, id=%{public}" PRId64 "", pendingTimer->id);
            auto bootTimePoint = TimeUtils::GetBootTimeNs();
            if (pendingTimer->whenElapsed <= bootTimePoint) {
                // 2 means the time of performing task.
                pendingTimer->UpdateWhenElapsedFromNow(bootTimePoint, milliseconds(2));
            } else {
                pendingTimer->UpdateWhenElapsedFromNow(bootTimePoint, pendingTimer->offset);
            }
            SetHandlerLocked(pendingTimer, false, false);
        }
        pendingDelayTimers_.clear();
        if (isAdjust) {
            ReBatchAllTimers();
            return;
        }
    }
    if (needReschedule && didRemove) {
        RescheduleKernelTimerLocked();
    }
    #ifdef SET_AUTO_REBOOT_ENABLE
    DeleteTimerFromPowerOnTimerListById(id);
    #endif
}

// needs to acquire the lock `mutex_` before calling this method
void TimerManager::SetHandlerLocked(std::shared_ptr<TimerInfo> alarm, bool rebatching, bool isRebatched)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start rebatching= %{public}d", rebatching);
    TimerProxy::GetInstance().RecordUidTimerMap(alarm, isRebatched);

    if (!isRebatched && mPendingIdleUntil_ != nullptr && !CheckAllowWhileIdle(alarm)) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "Pending not-allowed alarm in idle state, id=%{public}" PRId64 "",
            alarm->id);
        alarm->offset = duration_cast<milliseconds>(alarm->whenElapsed - TimeUtils::GetBootTimeNs());
        pendingDelayTimers_.push_back(alarm);
        return;
    }
    if (!rebatching) {
        AdjustSingleTimer(alarm);
    }
    bool isAdjust = false;
    if (!isRebatched && alarm->flags & static_cast<uint32_t>(IDLE_UNTIL)) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "Set idle timer, id=%{public}" PRId64 "", alarm->id);
        mPendingIdleUntil_ = alarm;
        isAdjust = AdjustTimersBasedOnDeviceIdle();
    }
    #ifdef SET_AUTO_REBOOT_ENABLE
    if (IsPowerOnTimer(alarm)) {
        auto timerId = alarm->id;
        auto timerInfo = std::find_if(powerOnTriggerTimerList_.begin(), powerOnTriggerTimerList_.end(),
                                      [timerId](const auto& triggerTimerInfo) {
                                          return triggerTimerInfo->id == timerId;
                                      });
        if (timerInfo == powerOnTriggerTimerList_.end()) {
            TIME_HILOGI(TIME_MODULE_SERVICE, "alarm needs power on, id=%{public}" PRId64 "", alarm->id);
            powerOnTriggerTimerList_.push_back(alarm);
            ReschedulePowerOnTimerLocked(false);
        }
    }
    #endif
    InsertAndBatchTimerLocked(std::move(alarm));
    if (isAdjust) {
        ReBatchAllTimers();
        rebatching = true;
    }
    if (!rebatching) {
        RescheduleKernelTimerLocked();
    }
}

// needs to acquire the lock `mutex_` before calling this method
void TimerManager::ReBatchAllTimers()
{
    auto oldSet = alarmBatches_;
    alarmBatches_.clear();
    auto nowElapsed = TimeUtils::GetBootTimeNs();
    for (const auto &batch : oldSet) {
        auto n = batch->Size();
        for (unsigned int i = 0; i < n; i++) {
            ReAddTimerLocked(batch->Get(i), nowElapsed);
        }
    }
    RescheduleKernelTimerLocked();
}

void TimerManager::ReAddTimerLocked(std::shared_ptr<TimerInfo> timer,
                                    std::chrono::steady_clock::time_point nowElapsed)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "ReAddTimerLocked start. uid= %{public}d, id=%{public}" PRId64 ""
        ", timer originMaxWhenElapsed=%{public}lld, whenElapsed=%{public}lld, now=%{public}lld",
        timer->uid, timer->id, timer->originWhenElapsed.time_since_epoch().count(),
        timer->whenElapsed.time_since_epoch().count(), nowElapsed.time_since_epoch().count());
    timer->CalculateWhenElapsed(nowElapsed);
    SetHandlerLocked(timer, true, true);
}

void TimerManager::TimerLooper()
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "Start timer wait loop");
    pthread_setname_np(pthread_self(), "timer_loop");
    std::vector<std::shared_ptr<TimerInfo>> triggerList;
    while (runFlag_) {
        uint32_t result = handler_->WaitForAlarm();
        auto nowRtc = std::chrono::system_clock::now();
        auto nowElapsed = TimeUtils::GetBootTimeNs();
        triggerList.clear();

        if ((result & TIME_CHANGED_MASK) != 0) {
            TIME_HILOGI(TIME_MODULE_SERVICE, "ret:%{public}u", result);
            system_clock::time_point lastTimeChangeClockTime;
            system_clock::time_point expectedClockTime;
            std::lock_guard<std::mutex> lock(mutex_);
            lastTimeChangeClockTime = lastTimeChangeClockTime_;
            expectedClockTime = lastTimeChangeClockTime +
                (duration_cast<milliseconds>(nowElapsed.time_since_epoch()) -
                duration_cast<milliseconds>(lastTimeChangeRealtime_.time_since_epoch()));
            if (lastTimeChangeClockTime == system_clock::time_point::min()
                || nowRtc < expectedClockTime
                || nowRtc > (expectedClockTime + milliseconds(ONE_THOUSAND))) {
                ReBatchAllTimers();
                lastTimeChangeClockTime_ = nowRtc;
                lastTimeChangeRealtime_ = nowElapsed;
            }
        }

        if (result != TIME_CHANGED_MASK) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                TriggerTimersLocked(triggerList, nowElapsed);
            }
            // in this function, timeservice apply a runninglock from powermanager
            // release mutex to prevent powermanager from using the interface of timeservice
            // which may cause deadlock
            DeliverTimersLocked(triggerList);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                RescheduleKernelTimerLocked();
            }
        }
    }
}

TimerManager::~TimerManager()
{
    if (alarmThread_ && alarmThread_->joinable()) {
        runFlag_ = false;
        alarmThread_->join();
    }
}

// needs to acquire the lock `mutex_` before calling this method
void TimerManager::TriggerIdleTimer()
{
    TIME_HILOGI(TIME_MODULE_SERVICE, "Idle alarm triggers");
    mPendingIdleUntil_ = nullptr;
    delayedTimers_.clear();
    std::for_each(pendingDelayTimers_.begin(), pendingDelayTimers_.end(),
        [this](const std::shared_ptr<TimerInfo> &pendingTimer) {
            TIME_HILOGI(TIME_MODULE_SERVICE, "Set timer from delay list, id=%{public}" PRId64 "", pendingTimer->id);
            auto bootTimePoint = TimeUtils::GetBootTimeNs();
            if (pendingTimer->whenElapsed > bootTimePoint) {
                pendingTimer->UpdateWhenElapsedFromNow(bootTimePoint, pendingTimer->offset);
            } else {
                // 2 means the time of performing task.
                pendingTimer->UpdateWhenElapsedFromNow(bootTimePoint, milliseconds(2));
            }
            SetHandlerLocked(pendingTimer, false, false);
        });
    pendingDelayTimers_.clear();
    ReBatchAllTimers();
}

// needs to acquire the lock `mutex_` before calling this method
bool TimerManager::ProcTriggerTimer(std::shared_ptr<TimerInfo> &alarm,
                                    const std::chrono::steady_clock::time_point &nowElapsed)
{
    if (mPendingIdleUntil_ != nullptr && mPendingIdleUntil_->id == alarm->id) {
        TriggerIdleTimer();
    }
    if (TimerProxy::GetInstance().IsProxy(alarm->uid, 0)
        || TimerProxy::GetInstance().IsProxy(alarm->uid, alarm->pid)) {
        alarm->ProxyTimer(nowElapsed, milliseconds(TimerProxy::GetInstance().GetProxyDelayTime()));
        SetHandlerLocked(alarm, false, false);
        return false;
    } else {
        #ifdef SET_AUTO_REBOOT_ENABLE
        DeleteTimerFromPowerOnTimerListById(alarm->id);
        #endif
        HandleRepeatTimer(alarm, nowElapsed);
        return true;
    }
}

bool IsNoLog(std::shared_ptr<TimerInfo> alarm)
{
    return (std::find(NO_LOG_APP_LIST.begin(), NO_LOG_APP_LIST.end(), alarm->bundleName) != NO_LOG_APP_LIST.end())
        && (alarm->repeatInterval != std::chrono::milliseconds(0))
        && (!alarm->wakeup);
}

// needs to acquire the lock `mutex_` before calling this method
bool TimerManager::TriggerTimersLocked(std::vector<std::shared_ptr<TimerInfo>> &triggerList,
                                       std::chrono::steady_clock::time_point nowElapsed)
{
    bool hasWakeup = false;
    int64_t bootTime = 0;
    TimeUtils::GetBootTimeNs(bootTime);
    TIME_HILOGD(TIME_MODULE_SERVICE, "current time %{public}" PRId64 "", bootTime);

    for (auto iter = alarmBatches_.begin(); iter != alarmBatches_.end();) {
        if (*iter == nullptr) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "alarmBatches_ has nullptr");
            iter = alarmBatches_.erase(iter);
            continue;
        }
        if ((*iter)->GetStart() > nowElapsed) {
            ++iter;
            continue;
        }
        auto batch = *iter;
        iter = alarmBatches_.erase(iter);
        TIME_HILOGD(
            TIME_MODULE_SERVICE, "batch size= %{public}d", static_cast<int>(alarmBatches_.size()));
        const auto n = batch->Size();
        for (unsigned int i = 0; i < n; ++i) {
            auto alarm = batch->Get(i);
            triggerList.push_back(alarm);
            if (!IsNoLog(alarm) && alarm->id != TimeTickNotify::GetInstance().GetTickTimerId()) {
                TIME_SIMPLIFY_HILOGW(TIME_MODULE_SERVICE, "uid:%{public}d id:%{public}" PRId64 " wk:%{public}u",
                    alarm->uid, alarm->id, alarm->wakeup);
            }
            if (alarm->wakeup) {
                hasWakeup = true;
            }
        }
    }
    for (auto iter = triggerList.begin(); iter != triggerList.end();) {
        auto alarm = *iter;
        if (!ProcTriggerTimer(alarm, nowElapsed)) {
            iter = triggerList.erase(iter);
        } else {
            ++iter;
        }
    }

    std::sort(triggerList.begin(), triggerList.end(),
        [](const std::shared_ptr<TimerInfo> &l, const std::shared_ptr<TimerInfo> &r) {
            return l->whenElapsed < r->whenElapsed;
        });

    return hasWakeup;
}

// needs to acquire the lock `mutex_` before calling this method
void TimerManager::RescheduleKernelTimerLocked()
{
    auto bootTime = TimeUtils::GetBootTimeNs();
    if (!alarmBatches_.empty()) {
        auto firstWakeup = FindFirstWakeupBatchLocked();
        auto firstBatch = alarmBatches_.front();
        if (firstWakeup != nullptr) {
            #ifdef POWER_MANAGER_ENABLE
            HandleRunningLock(firstWakeup);
            #endif
            auto setTimePoint = firstWakeup->GetStart().time_since_epoch();
            if (setTimePoint < bootTime.time_since_epoch() ||
                setTimePoint.count() != lastSetTime_[ELAPSED_REALTIME_WAKEUP]) {
                SetLocked(ELAPSED_REALTIME_WAKEUP, setTimePoint, bootTime);
                lastSetTime_[ELAPSED_REALTIME_WAKEUP] = setTimePoint.count();
            }
        }
        if (firstBatch != firstWakeup) {
            auto setTimePoint = firstBatch->GetStart().time_since_epoch();
            if (setTimePoint < bootTime.time_since_epoch() || setTimePoint.count() != lastSetTime_[ELAPSED_REALTIME]) {
                SetLocked(ELAPSED_REALTIME, setTimePoint, bootTime);
                lastSetTime_[ELAPSED_REALTIME] = setTimePoint.count();
            }
        }
    }
}

#ifdef SET_AUTO_REBOOT_ENABLE
bool TimerManager::IsPowerOnTimer(std::shared_ptr<TimerInfo> timerInfo)
{
    if (timerInfo != nullptr) {
        return (std::find(powerOnApps_.begin(), powerOnApps_.end(), timerInfo->name) != powerOnApps_.end() ||
            std::find(powerOnApps_.begin(), powerOnApps_.end(), timerInfo->bundleName) != powerOnApps_.end()) &&
            CheckNeedRecoverOnReboot(timerInfo->bundleName, timerInfo->type, timerInfo->autoRestore);
    }
    return false;
}

void TimerManager::DeleteTimerFromPowerOnTimerListById(uint64_t timerId)
{
    auto deleteTimerInfo = std::find_if(powerOnTriggerTimerList_.begin(), powerOnTriggerTimerList_.end(),
                                        [timerId](const auto& triggerTimerInfo) {
                                            return triggerTimerInfo->id == timerId;
                                        });
    if (deleteTimerInfo == powerOnTriggerTimerList_.end()) {
        return;
    }
    powerOnTriggerTimerList_.erase(deleteTimerInfo);
    ReschedulePowerOnTimerLocked(false);
}

void TimerManager::ReschedulePowerOnTimerLocked(bool isShutDown)
{
    auto bootTime = TimeUtils::GetBootTimeNs();
    int64_t currentTime = 0;
    if (TimeUtils::GetWallTimeMs(currentTime) != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "currentTime get failed");
        return;
    }
    if (powerOnTriggerTimerList_.size() == 0) {
        // current version cannot cancel a power-on timer
        // set trigger time to ten years as a never trigger timer to cancel the timer
        int64_t timeNeverTrigger = currentTime + TEN_YEARS_TO_SECOND * ONE_THOUSAND;
        SetLocked(POWER_ON_ALARM, std::chrono::milliseconds(timeNeverTrigger), bootTime);
        lastSetTime_[POWER_ON_ALARM] = timeNeverTrigger;
        return;
    }
    std::sort(powerOnTriggerTimerList_.begin(), powerOnTriggerTimerList_.end(),
        [](const std::shared_ptr<TimerInfo>& a, const std::shared_ptr<TimerInfo>& b) {
            return a->when < b->when;
        });
    auto timerInfo = powerOnTriggerTimerList_[0];
    auto setTimePoint = timerInfo->when;
    while (setTimePoint.count() < currentTime) {
        powerOnTriggerTimerList_.erase(powerOnTriggerTimerList_.begin());
        if (powerOnTriggerTimerList_.size() == 0) {
            // current version cannot cancel a power-on timer
            // set trigger time to ten years as a never trigger timer to cancel the timer
            int64_t timeNeverTrigger = currentTime + TEN_YEARS_TO_SECOND * ONE_THOUSAND;
            SetLocked(POWER_ON_ALARM, std::chrono::milliseconds(timeNeverTrigger), bootTime);
            lastSetTime_[POWER_ON_ALARM] = timeNeverTrigger;
            return;
        }
        timerInfo = powerOnTriggerTimerList_[0];
        setTimePoint = timerInfo->when;
    }
    if (isShutDown && static_cast<uint64_t>(currentTime) + TWO_MINUTES_TO_MILLI > setTimePoint.count()) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "interval less than 2min");
        auto triggerTime = static_cast<uint64_t>(currentTime) + TWO_MINUTES_TO_MILLI;
        setTimePoint = std::chrono::milliseconds(triggerTime);
    }
    if (setTimePoint.count() != lastSetTime_[POWER_ON_ALARM]) {
        SetLocked(POWER_ON_ALARM, setTimePoint, bootTime);
        lastSetTime_[POWER_ON_ALARM] = setTimePoint.count();
    }
}

void TimerManager::ShutDownReschedulePowerOnTimer()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ReschedulePowerOnTimerLocked(true);
}
#endif

// needs to acquire the lock `mutex_` before calling this method
std::shared_ptr<Batch> TimerManager::FindFirstWakeupBatchLocked()
{
    auto it = std::find_if(alarmBatches_.begin(),
                           alarmBatches_.end(),
                           [](const std::shared_ptr<Batch> &batch) {
                               return batch->HasWakeups();
                           });
    return (it != alarmBatches_.end()) ? *it : nullptr;
}

void TimerManager::SetLocked(int type, std::chrono::nanoseconds when, std::chrono::steady_clock::time_point bootTime)
{
    #ifdef SET_AUTO_REBOOT_ENABLE
    if (type != POWER_ON_ALARM && when.count() <= 0) {
        when = bootTime.time_since_epoch();
    }
    #else
    if (when.count() <= 0) {
        when = bootTime.time_since_epoch();
    }
    #endif
    handler_->Set(static_cast<uint32_t>(type), when, bootTime);
}

// needs to acquire the lock `mutex_` before calling this method
void TimerManager::InsertAndBatchTimerLocked(std::shared_ptr<TimerInfo> alarm)
{
    int64_t whichBatch = (alarm->flags & static_cast<uint32_t>(STANDALONE)) ?
        -1 :
        AttemptCoalesceLocked(alarm->whenElapsed, alarm->maxWhenElapsed);
    if (!IsNoLog(alarm)) {
        auto whenElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            alarm->whenElapsed.time_since_epoch()).count();
        auto maxWhenElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            alarm->maxWhenElapsed.time_since_epoch()).count();
        if (whenElapsedMs != maxWhenElapsedMs) {
            if (whichBatch == -1) {
                TIME_SIMPLIFY_HILOGW(TIME_MODULE_SERVICE, "id:%{public}" PRIu64 " we:%{public}lld mwe:%{public}lld",
                    alarm->id, whenElapsedMs, maxWhenElapsedMs);
            } else {
                TIME_SIMPLIFY_HILOGW(TIME_MODULE_SERVICE, "bat:%{public}" PRId64 " id:%{public}" PRIu64 " "
                    "we:%{public}lld mwe:%{public}lld", whichBatch, alarm->id, whenElapsedMs, maxWhenElapsedMs);
            }
        } else {
            if (whichBatch == -1) {
                TIME_SIMPLIFY_HILOGW(TIME_MODULE_SERVICE, "id:%{public}" PRIu64 " we:%{public}lld",
                    alarm->id, whenElapsedMs);
            } else {
                TIME_SIMPLIFY_HILOGW(TIME_MODULE_SERVICE, "bat:%{public}" PRId64 " id:%{public}" PRIu64 " "
                    "we:%{public}lld", whichBatch, alarm->id, whenElapsedMs);
            }
        }
    }
    if (whichBatch < 0) {
        AddBatchLocked(alarmBatches_, std::make_shared<Batch>(*alarm));
    } else {
        auto batch = alarmBatches_.at(whichBatch);
        if (batch->Add(alarm)) {
            alarmBatches_.erase(alarmBatches_.begin() + whichBatch);
            AddBatchLocked(alarmBatches_, batch);
        }
    }
}

// needs to acquire the lock `mutex_` before calling this method
int64_t TimerManager::AttemptCoalesceLocked(std::chrono::steady_clock::time_point whenElapsed,
                                            std::chrono::steady_clock::time_point maxWhen)
{
    auto it = std::find_if(alarmBatches_.begin(), alarmBatches_.end(),
        [whenElapsed, maxWhen](const std::shared_ptr<Batch> &batch) {
            return (batch->GetFlags() & static_cast<uint32_t>(STANDALONE)) == 0 &&
                   (batch->CanHold(whenElapsed, maxWhen));
        });
    if (it != alarmBatches_.end()) {
        return std::distance(alarmBatches_.begin(), it);
    }
    return -1;
}

void TimerManager::NotifyWantAgentRetry(std::shared_ptr<TimerInfo> timer)
{
    auto retryRegister = [timer]() {
        for (int i = 0; i < WANT_RETRY_TIMES; i++) {
            sleep(WANT_RETRY_INTERVAL << i);
            if (TimerManager::GetInstance()->NotifyWantAgent(timer)) {
                return;
            }
            TIME_HILOGI(TIME_MODULE_SERVICE, "retry trigWA:times:%{public}d id=%{public}" PRId64 "", i, timer->id);
        }
    };
    std::thread thread(retryRegister);
    thread.detach();
}

#ifdef MULTI_ACCOUNT_ENABLE
int32_t TimerManager::CheckUserIdForNotify(const std::shared_ptr<TimerInfo> &timer)
{
    auto bundleList = TimeFileUtils::GetParameterList(TIMER_ACROSS_ACCOUNTS);
    if (!bundleList.empty() &&
        std::find(bundleList.begin(), bundleList.end(), timer->bundleName) != bundleList.end()) {
        return E_TIME_OK;
    }
    int userIdOfTimer = -1;
    int foregroundUserId = -1;
    int getLocalIdErr = AccountSA::OsAccountManager::GetOsAccountLocalIdFromUid(timer->uid, userIdOfTimer);
    if (getLocalIdErr != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Get account id from uid failed, errcode:%{public}d", getLocalIdErr);
        return E_TIME_ACCOUNT_ERROR;
    }
    int getForegroundIdErr = AccountSA::OsAccountManager::GetForegroundOsAccountLocalId(foregroundUserId);
    if (getForegroundIdErr != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Get foreground account id failed, errcode:%{public}d", getForegroundIdErr);
        return E_TIME_ACCOUNT_ERROR;
    }
    if (userIdOfTimer == foregroundUserId || userIdOfTimer == SYSTEM_USER_ID) {
        return E_TIME_OK;
    } else {
        TIME_HILOGI(TIME_MODULE_SERVICE, "WA wait switch user, uid:%{public}d, timerId:%{public}" PRId64,
            timer->uid, timer->id);
        return E_TIME_ACCOUNT_NOT_MATCH;
    }
}
#endif

void TimerManager::DeliverTimersLocked(const std::vector<std::shared_ptr<TimerInfo>> &triggerList)
{
    auto wakeupNums = std::count_if(triggerList.begin(), triggerList.end(), [](auto timer) {return timer->wakeup;});
    if (wakeupNums > 0) {
        TimeServiceNotify::GetInstance().PublishTimerTriggerEvents();
    }
    for (const auto &timer : triggerList) {
        if (timer->wakeup) {
            #ifdef POWER_MANAGER_ENABLE
            AddRunningLock(RUNNING_LOCK_DURATION);
            #endif
            TimerBehaviorReport(timer, false);
            StatisticReporter(wakeupNums, timer);
        }
        if (timer->callback) {
            if (TimerProxy::GetInstance().CallbackAlarmIfNeed(timer) == PEER_END_DEAD
                && !timer->wantAgent) {
                DestroyTimer(timer->id);
                continue;
            }
        }
        if (timer->wantAgent) {
            if (!NotifyWantAgent(timer) &&
                CheckNeedRecoverOnReboot(timer->bundleName, timer->type, timer->autoRestore)) {
                NotifyWantAgentRetry(timer);
            }
            if (timer->repeatInterval != milliseconds::zero()) {
                continue;
            }
            auto tableName = (CheckNeedRecoverOnReboot(timer->bundleName, timer->type, timer->autoRestore)
                              ? HOLD_ON_REBOOT
                              : DROP_ON_REBOOT);
            #ifdef RDB_ENABLE
            OHOS::NativeRdb::ValuesBucket values;
            values.PutInt("state", 0);
            OHOS::NativeRdb::RdbPredicates rdbPredicates(tableName);
            rdbPredicates.EqualTo("state", 1)
                ->And()
                ->EqualTo("timerId", static_cast<int64_t>(timer->id));
            TimeDatabase::GetInstance().Update(values, rdbPredicates);
            #else
            CjsonHelper::GetInstance().UpdateState(tableName, static_cast<int64_t>(timer->id));
            #endif
        }
        if (((timer->flags & static_cast<uint32_t>(IS_DISPOSABLE)) > 0) &&
            (timer->repeatInterval == milliseconds::zero())) {
            DestroyTimer(timer->id);
        }
    }
}

std::string GetWantString(int64_t timerId)
{
    #ifdef RDB_ENABLE
    OHOS::NativeRdb::RdbPredicates holdRdbPredicates(HOLD_ON_REBOOT);
    holdRdbPredicates.EqualTo("timerId", timerId);
    auto holdResultSet = TimeDatabase::GetInstance().Query(holdRdbPredicates, ALL_DATA);
    if (holdResultSet == nullptr || holdResultSet->GoToFirstRow() != OHOS::NativeRdb::E_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "db query failed nullptr");
        if (holdResultSet != nullptr) {
            holdResultSet->Close();
        }
        return "";
    }
    // Line 7 is 'wantAgent'
    auto wantStr = GetString(holdResultSet, 7);
    holdResultSet->Close();
    #else
    auto wantStr = CjsonHelper::GetInstance().QueryWant(HOLD_ON_REBOOT, timerId);
    if (wantStr == "") {
        TIME_HILOGE(TIME_MODULE_SERVICE, "db query failed");
        return "";
    }
    #endif
    return wantStr;
}

bool TimerManager::NotifyWantAgent(const std::shared_ptr<TimerInfo> &timer)
{
    auto wantAgent = timer->wantAgent;
    std::shared_ptr<AAFwk::Want> want = OHOS::AbilityRuntime::WantAgent::WantAgentHelper::GetWant(wantAgent);
    if (want == nullptr) {
        #ifdef MULTI_ACCOUNT_ENABLE
        switch (CheckUserIdForNotify(timer)) {
            case E_TIME_ACCOUNT_NOT_MATCH:
                // No need to retry.
                return true;
            case E_TIME_ACCOUNT_ERROR:
                TIME_HILOGE(TIME_MODULE_SERVICE, "want is nullptr, id=%{public}" PRId64 "", timer->id);
                return false;
            default:
                break;
        }
        #endif
        auto wantStr = GetWantString(static_cast<int64_t>(timer->id));
        if (wantStr == "") {
            return false;
        }
        wantAgent = OHOS::AbilityRuntime::WantAgent::WantAgentHelper::FromString(wantStr);
        #ifdef MULTI_ACCOUNT_ENABLE
        switch (CheckUserIdForNotify(timer)) {
            case E_TIME_ACCOUNT_NOT_MATCH:
                TIME_HILOGI(TIME_MODULE_SERVICE, "user sw after FS, id=%{public}" PRId64 "", timer->id);
                // No need to retry.
                return true;
            case E_TIME_ACCOUNT_ERROR:
                TIME_HILOGE(TIME_MODULE_SERVICE, "want is nullptr, id=%{public}" PRId64 "", timer->id);
                return false;
            default:
                break;
        }
        #endif
        want = OHOS::AbilityRuntime::WantAgent::WantAgentHelper::GetWant(wantAgent);
        if (want == nullptr) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "want is nullptr,id=%{public}" PRId64 "", timer->id);
            return false;
        }
    }
    OHOS::AbilityRuntime::WantAgent::TriggerInfo paramsInfo("", nullptr, want, WANTAGENT_CODE_ELEVEN);
    sptr<AbilityRuntime::WantAgent::CompletedDispatcher> data;
    auto code = AbilityRuntime::WantAgent::WantAgentHelper::TriggerWantAgent(wantAgent, nullptr, paramsInfo,
        data, nullptr);
    if (code != ERR_OK) {
        TIME_SIMPLIFY_HILOGW(TIME_MODULE_SERVICE, "trigWA id:%{public}" PRId64 " ret:%{public}d", timer->id, code);
        auto extraInfo = "timer id:" + std::to_string(timer->id);
        TimeServiceFaultReporter(ReportEventCode::TIMER_WANTAGENT_FAULT_REPORT, code, timer->uid, timer->bundleName,
            extraInfo);
    } else {
        TIME_SIMPLIFY_HILOGW(TIME_MODULE_SERVICE, "trigWA id:%{public}" PRId64 "", timer->id);
    }
    return code == ERR_OK;
}

// needs to acquire the lock `mutex_` before calling this method
void TimerManager::UpdateTimersState(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)
{
    if (needRetrigger) {
        RemoveLocked(alarm->id, false);
        AdjustSingleTimerLocked(alarm);
        InsertAndBatchTimerLocked(alarm);
        RescheduleKernelTimerLocked();
    } else {
        RemoveLocked(alarm->id, true);
        TimerProxy::GetInstance().RemoveUidTimerMapLocked(alarm);
        bool needRecover = CheckNeedRecoverOnReboot(alarm->bundleName, alarm->type, alarm->autoRestore);
        UpdateOrDeleteDatabase(false, alarm->id, needRecover);
    }
}

bool TimerManager::AdjustTimer(bool isAdjust, uint32_t interval, uint32_t delta)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (adjustPolicy_ == isAdjust && adjustInterval_ == interval && adjustDelta_ == delta) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "already deal timer adjust, flag:%{public}d", isAdjust);
        return false;
    }
    std::chrono::steady_clock::time_point now = TimeUtils::GetBootTimeNs();
    adjustPolicy_ = isAdjust;
    adjustInterval_ = interval;
    adjustDelta_ = delta;
    auto callback = [this] (AdjustTimerCallback adjustTimer) {
        bool isChanged = false;
        for (const auto &batch : alarmBatches_) {
            if (!batch) {
                continue;
            }
            auto n = batch->Size();
            for (unsigned int i = 0; i < n; i++) {
                auto timer = batch->Get(i);
                isChanged = adjustTimer(timer) || isChanged;
            }
        }
        if (isChanged) {
            TIME_HILOGI(TIME_MODULE_SERVICE, "timer adjust executing, policy:%{public}d", adjustPolicy_);
            ReBatchAllTimers();
        }
    };

    return TimerProxy::GetInstance().AdjustTimer(isAdjust, interval, now, delta, callback);
}

bool TimerManager::ProxyTimer(int32_t uid, std::set<int> pidList, bool isProxy, bool needRetrigger)
{
    std::set<int> failurePid;
    auto bootTimePoint = TimeUtils::GetBootTimeNs();
    std::lock_guard<std::mutex> lock(mutex_);
    if (pidList.size() == 0) {
        return TimerProxy::GetInstance().ProxyTimer(uid, 0, isProxy, needRetrigger, bootTimePoint,
            [this] (std::shared_ptr<TimerInfo> &alarm, bool needRetrigger) {
                UpdateTimersState(alarm, needRetrigger);
            });
    }
    for (std::set<int>::iterator pid = pidList.begin(); pid != pidList.end(); ++pid) {
        if (!TimerProxy::GetInstance().ProxyTimer(uid, *pid, isProxy, needRetrigger, bootTimePoint,
            [this] (std::shared_ptr<TimerInfo> &alarm, bool needRetrigger) {
                UpdateTimersState(alarm, needRetrigger);
            })) {
            failurePid.insert(*pid);
        }
    }
    return (failurePid.size() == 0);
}

void TimerManager::SetTimerExemption(const std::unordered_set<std::string> &nameArr, bool isExemption)
{
    std::lock_guard<std::mutex> lock(mutex_);
    TimerProxy::GetInstance().SetTimerExemption(nameArr, isExemption);
}

void TimerManager::SetAdjustPolicy(const std::unordered_map<std::string, uint32_t> &policyMap)
{
    std::lock_guard<std::mutex> lock(mutex_);
    TimerProxy::GetInstance().SetAdjustPolicy(policyMap);
}

bool TimerManager::AdjustSingleTimer(std::shared_ptr<TimerInfo> timer)
{
    if (!adjustPolicy_ || TimerProxy::GetInstance().IsProxy(timer->uid, 0)
        || TimerProxy::GetInstance().IsProxy(timer->uid, timer->pid)) {
        return false;
    }
    return TimerProxy::GetInstance().AdjustTimer(adjustPolicy_, adjustInterval_, TimeUtils::GetBootTimeNs(),
        adjustDelta_, [this, timer] (AdjustTimerCallback adjustTimer) { adjustTimer(timer); });
}

// needs to acquire the lock `proxyMutex_` before calling this method
bool TimerManager::AdjustSingleTimerLocked(std::shared_ptr<TimerInfo> timer)
{
    if (!adjustPolicy_|| TimerProxy::GetInstance().IsProxyLocked(timer->uid, 0)
        || TimerProxy::GetInstance().IsProxyLocked(timer->uid, timer->pid)) {
        return false;
    }
    return TimerProxy::GetInstance().AdjustTimer(adjustPolicy_, adjustInterval_, TimeUtils::GetBootTimeNs(),
        adjustDelta_, [this, timer] (AdjustTimerCallback adjustTimer) { adjustTimer(timer); });
}

bool TimerManager::ResetAllProxy()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return TimerProxy::GetInstance().ResetAllProxy(TimeUtils::GetBootTimeNs(),
        [this] (std::shared_ptr<TimerInfo> &alarm, bool needRetrigger) { UpdateTimersState(alarm, true); });
}

bool TimerManager::CheckAllowWhileIdle(const std::shared_ptr<TimerInfo> &alarm)
{
    #ifdef DEVICE_STANDBY_ENABLE
    if (TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
        std::vector<DevStandbyMgr::AllowInfo> restrictList;
        DevStandbyMgr::StandbyServiceClient::GetInstance().GetRestrictList(DevStandbyMgr::AllowType::TIMER,
            restrictList, REASON_APP_API);
        auto it = std::find_if(restrictList.begin(), restrictList.end(),
            [&alarm](const DevStandbyMgr::AllowInfo &allowInfo) { return allowInfo.GetName() == alarm->bundleName; });
        if (it != restrictList.end()) {
            return false;
        }
    }

    if (TimePermission::CheckProxyCallingPermission()) {
        pid_t pid = IPCSkeleton::GetCallingPid();
        std::string procName = TimeFileUtils::GetNameByPid(pid);
        if (alarm->flags & static_cast<uint32_t>(INEXACT_REMINDER)) {
            return false;
        }
        std::vector<DevStandbyMgr::AllowInfo> restrictList;
        DevStandbyMgr::StandbyServiceClient::GetInstance().GetRestrictList(DevStandbyMgr::AllowType::TIMER,
            restrictList, REASON_NATIVE_API);
        auto it = std::find_if(restrictList.begin(), restrictList.end(),
            [procName](const DevStandbyMgr::AllowInfo &allowInfo) { return allowInfo.GetName() == procName; });
        if (it != restrictList.end()) {
            return false;
        }
    }
    #endif
    return true;
}

// needs to acquire the lock `mutex_` before calling this method
bool TimerManager::AdjustDeliveryTimeBasedOnDeviceIdle(const std::shared_ptr<TimerInfo> &alarm)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start adjust timer, uid=%{public}d, id=%{public}" PRId64 "",
        alarm->uid, alarm->id);
    if (mPendingIdleUntil_ == alarm) {
        return false;
    }
    if (mPendingIdleUntil_ == nullptr) {
        auto itMap = delayedTimers_.find(alarm->id);
        if (itMap != delayedTimers_.end()) {
            std::chrono::milliseconds currentTime;
            if (alarm->type == RTC || alarm->type == RTC_WAKEUP) {
                currentTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
            } else {
                currentTime = duration_cast<milliseconds>(TimeUtils::GetBootTimeNs().time_since_epoch());
            }

            if (alarm->origWhen > currentTime) {
                auto offset = alarm->origWhen - currentTime;
                return alarm->UpdateWhenElapsedFromNow(TimeUtils::GetBootTimeNs(), offset);
            }
            // 2 means the time of performing task.
            return alarm->UpdateWhenElapsedFromNow(TimeUtils::GetBootTimeNs(), milliseconds(2));
        }
        return false;
    }

    if (CheckAllowWhileIdle(alarm)) {
        TIME_HILOGD(TIME_MODULE_SERVICE, "Timer unrestricted, not adjust. id=%{public}" PRId64 "", alarm->id);
        return false;
    } else if (alarm->whenElapsed > mPendingIdleUntil_->whenElapsed) {
        TIME_HILOGD(TIME_MODULE_SERVICE, "Timer not allowed, not adjust. id=%{public}" PRId64 "", alarm->id);
        return false;
    } else {
        TIME_HILOGD(TIME_MODULE_SERVICE, "Timer not allowed, id=%{public}" PRId64 "", alarm->id);
        delayedTimers_[alarm->id] = alarm->whenElapsed;
        auto bootTimePoint = TimeUtils::GetBootTimeNs();
        auto offset = TimerInfo::ConvertToElapsed(mPendingIdleUntil_->when, mPendingIdleUntil_->type) - bootTimePoint;
        return alarm->UpdateWhenElapsedFromNow(bootTimePoint, offset);
    }
}

// needs to acquire the lock `mutex_` before calling this method
bool TimerManager::AdjustTimersBasedOnDeviceIdle()
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start adjust alarmBatches_.size=%{public}d",
        static_cast<int>(alarmBatches_.size()));
    bool isAdjust = false;
    for (const auto &batch : alarmBatches_) {
        auto n = batch->Size();
        for (unsigned int i = 0; i < n; i++) {
            auto alarm = batch->Get(i);
            isAdjust = AdjustDeliveryTimeBasedOnDeviceIdle(alarm) || isAdjust;
        }
    }
    return isAdjust;
}

// needs to acquire the lock `mutex_` before calling this method
bool AddBatchLocked(std::vector<std::shared_ptr<Batch>> &list, const std::shared_ptr<Batch> &newBatch)
{
    auto it = std::upper_bound(list.begin(),
                               list.end(),
                               newBatch,
                               [](const std::shared_ptr<Batch> &first, const std::shared_ptr<Batch> &second) {
                                   return first->GetStart() < second->GetStart();
                               });
    list.insert(it, newBatch);
    return it == list.begin();
}

#ifdef HIDUMPER_ENABLE
bool TimerManager::ShowTimerEntryMap(int fd)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start");
    std::lock_guard<std::mutex> lock(entryMapMutex_);
    auto iter = timerEntryMap_.begin();
    for (; iter != timerEntryMap_.end(); iter++) {
        dprintf(fd, " - dump timer number   = %lu\n", iter->first);
        dprintf(fd, " * timer name          = %s\n", iter->second->name.c_str());
        dprintf(fd, " * timer id            = %lu\n", iter->second->id);
        dprintf(fd, " * timer type          = %d\n", iter->second->type);
        dprintf(fd, " * timer flag          = %u\n", iter->second->flag);
        dprintf(fd, " * timer window Length = %lld\n", iter->second->windowLength);
        dprintf(fd, " * timer interval      = %lu\n", iter->second->interval);
        dprintf(fd, " * timer uid           = %d\n\n", iter->second->uid);
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
    return true;
}

bool TimerManager::ShowTimerEntryById(int fd, uint64_t timerId)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start");
    std::lock_guard<std::mutex> lock(entryMapMutex_);
    auto iter = timerEntryMap_.find(timerId);
    if (iter == timerEntryMap_.end()) {
        TIME_HILOGD(TIME_MODULE_SERVICE, "end");
        return false;
    } else {
        dprintf(fd, " - dump timer number   = %lu\n", iter->first);
        dprintf(fd, " * timer id            = %lu\n", iter->second->id);
        dprintf(fd, " * timer type          = %d\n", iter->second->type);
        dprintf(fd, " * timer window Length = %lld\n", iter->second->windowLength);
        dprintf(fd, " * timer interval      = %lu\n", iter->second->interval);
        dprintf(fd, " * timer uid           = %d\n\n", iter->second->uid);
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
    return true;
}

bool TimerManager::ShowTimerTriggerById(int fd, uint64_t timerId)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start");
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < alarmBatches_.size(); i++) {
        for (size_t j = 0; j < alarmBatches_[i]->Size(); j++) {
            if (alarmBatches_[i]->Get(j)->id == timerId) {
                dprintf(fd, " - dump timer id   = %lu\n", alarmBatches_[i]->Get(j)->id);
                dprintf(fd, " * timer trigger   = %lld\n", alarmBatches_[i]->Get(j)->origWhen);
            }
        }
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
    return true;
}

bool TimerManager::ShowIdleTimerInfo(int fd)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start");
    std::lock_guard<std::mutex> lock(mutex_);
    dprintf(fd, " - dump idle state         = %d\n", (mPendingIdleUntil_ != nullptr));
    if (mPendingIdleUntil_ != nullptr) {
        dprintf(fd, " - dump idle timer id  = %lu\n", mPendingIdleUntil_->id);
        dprintf(fd, " * timer type          = %d\n", mPendingIdleUntil_->type);
        dprintf(fd, " * timer flag          = %u\n", mPendingIdleUntil_->flags);
        dprintf(fd, " * timer window Length = %lu\n", mPendingIdleUntil_->windowLength);
        dprintf(fd, " * timer interval      = %lu\n", mPendingIdleUntil_->repeatInterval);
        dprintf(fd, " * timer whenElapsed   = %lu\n", mPendingIdleUntil_->whenElapsed);
        dprintf(fd, " * timer uid           = %d\n\n", mPendingIdleUntil_->uid);
    }
    for (const auto &pendingTimer : pendingDelayTimers_) {
        dprintf(fd, " - dump pending delay timer id  = %lu\n", pendingTimer->id);
        dprintf(fd, " * timer type          = %d\n", pendingTimer->type);
        dprintf(fd, " * timer flag          = %u\n", pendingTimer->flags);
        dprintf(fd, " * timer window Length = %lu\n", pendingTimer->windowLength);
        dprintf(fd, " * timer interval      = %lu\n", pendingTimer->repeatInterval);
        dprintf(fd, " * timer whenElapsed   = %lu\n", pendingTimer->whenElapsed);
        dprintf(fd, " * timer uid           = %d\n\n", pendingTimer->uid);
    }
    for (const auto &delayedTimer : delayedTimers_) {
        dprintf(fd, " - dump delayed timer id = %lu\n", delayedTimer.first);
        dprintf(fd, " * timer whenElapsed     = %lu\n", delayedTimer.second);
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
    return true;
}
#endif

#ifdef MULTI_ACCOUNT_ENABLE
void TimerManager::OnUserRemoved(int userId)
{
    TIME_HILOGI(TIME_MODULE_SERVICE, "Removed userId: %{public}d", userId);
    std::vector<std::shared_ptr<TimerEntry>> removeList;
    {
        std::lock_guard<std::mutex> lock(entryMapMutex_);
        for (auto it = timerEntryMap_.begin(); it != timerEntryMap_.end(); ++it) {
            int userIdOfTimer = -1;
            AccountSA::OsAccountManager::GetOsAccountLocalIdFromUid(it->second->uid, userIdOfTimer);
            if (userId == userIdOfTimer) {
                removeList.push_back(it->second);
            }
        }
    }
    for (auto it = removeList.begin(); it != removeList.end(); ++it) {
        DestroyTimer((*it)->id);
    }
}
#endif

void TimerManager::OnPackageRemoved(int uid)
{
    TIME_HILOGI(TIME_MODULE_SERVICE, "Removed uid: %{public}d", uid);
    std::vector<std::shared_ptr<TimerEntry>> removeList;
    {
        std::lock_guard<std::mutex> lock(entryMapMutex_);
        for (auto it = timerEntryMap_.begin(); it != timerEntryMap_.end(); ++it) {
            if (it->second->uid == uid) {
                removeList.push_back(it->second);
            }
        }
    }
    for (auto it = removeList.begin(); it != removeList.end(); ++it) {
        DestroyTimer((*it)->id);
    }
}

void TimerManager::HandleRSSDeath()
{
    TIME_HILOGI(TIME_MODULE_CLIENT, "RSSSaDeathRecipient died");
    uint64_t id = 0;
    {
        std::lock_guard <std::mutex> lock(mutex_);
        if (mPendingIdleUntil_ != nullptr) {
            id = mPendingIdleUntil_->id;
        } else {
            return;
        }
    }
    StopTimerInner(id, true);
}

void TimerManager::HandleRepeatTimer(
    const std::shared_ptr<TimerInfo> &timer, std::chrono::steady_clock::time_point nowElapsed)
{
    if (timer->repeatInterval > milliseconds::zero()) {
        uint64_t count = 1 + static_cast<uint64_t>(
            duration_cast<milliseconds>(nowElapsed - timer->whenElapsed) / timer->repeatInterval);
        auto delta = count * timer->repeatInterval;
        steady_clock::time_point nextElapsed = timer->whenElapsed + delta;
        steady_clock::time_point nextMaxElapsed = (timer->windowLength == milliseconds::zero()) ?
                                                  nextElapsed :
                                                  TimerInfo::MaxTriggerTime(nowElapsed, nextElapsed,
                                                                            timer->repeatInterval);
        auto alarm = std::make_shared<TimerInfo>(timer->name, timer->id, timer->type, timer->when + delta,
            timer->whenElapsed + delta, timer->windowLength, nextMaxElapsed, timer->repeatInterval, timer->callback,
            timer->wantAgent, timer->flags, timer->autoRestore, timer->uid, timer->pid, timer->bundleName);
        SetHandlerLocked(alarm);
    } else {
        TimerProxy::GetInstance().RemoveUidTimerMap(timer);
    }
}

inline bool TimerManager::CheckNeedRecoverOnReboot(std::string bundleName, int type, bool autoRestore)
{
    return ((std::find(NEED_RECOVER_ON_REBOOT.begin(), NEED_RECOVER_ON_REBOOT.end(), bundleName) !=
        NEED_RECOVER_ON_REBOOT.end() && (type == RTC || type == RTC_WAKEUP)) || autoRestore);
}

#ifdef POWER_MANAGER_ENABLE
// needs to acquire the lock `mutex_` before calling this method
void TimerManager::HandleRunningLock(const std::shared_ptr<Batch> &firstWakeup)
{
    int64_t currentTime = 0;
    TimeUtils::GetBootTimeNs(currentTime);
    auto nextTimerOffset =
        duration_cast<nanoseconds>(firstWakeup->GetStart().time_since_epoch()).count() - currentTime;
    auto lockOffset = currentTime - lockExpiredTime_;
    if (nextTimerOffset > 0 && nextTimerOffset <= USE_LOCK_TIME_IN_NANO &&
        ((lockOffset < 0 && std::abs(lockOffset) <= nextTimerOffset) || lockOffset >= 0)) {
        auto firstAlarm = firstWakeup->Get(0);
        if (firstAlarm == nullptr) {
            TIME_HILOGI(TIME_MODULE_SERVICE, "first alarm is null");
            return;
        }
        auto holdLockTime = nextTimerOffset + ONE_HUNDRED_MILLI;
        TIME_HILOGI(TIME_MODULE_SERVICE, "time:%{public}" PRIu64 ", timerId:%{public}" PRIu64"",
            static_cast<uint64_t>(holdLockTime), firstAlarm->id);
        lockExpiredTime_ = currentTime + holdLockTime;
        AddRunningLock(holdLockTime);
    }
}

void TimerManager::AddRunningLockRetry(long long holdLockTime)
{
    auto retryRegister = [this, holdLockTime]() {
        for (int i = 0; i < POWER_RETRY_TIMES; i++) {
            usleep(POWER_RETRY_INTERVAL);
            runningLock_->Lock(static_cast<int32_t>(holdLockTime / NANO_TO_MILLI));
            if (runningLock_->IsUsed()) {
                return;
            }
        }
    };
    std::thread thread(retryRegister);
    thread.detach();
}

void TimerManager::AddRunningLock(long long holdLockTime)
{
    if (runningLock_ == nullptr) {
        std::lock_guard<std::mutex> lock(runningLockMutex_);
        if (runningLock_ == nullptr) {
            TIME_HILOGI(TIME_MODULE_SERVICE, "runningLock is nullptr, create runningLock");
            runningLock_ = PowerMgr::PowerMgrClient::GetInstance().CreateRunningLock("timeServiceRunningLock",
                PowerMgr::RunningLockType::RUNNINGLOCK_BACKGROUND_NOTIFICATION);
        }
    }
    if (runningLock_ != nullptr) {
        runningLock_->Lock(static_cast<int32_t>(holdLockTime / NANO_TO_MILLI));
        if (!runningLock_->IsUsed()) {
            AddRunningLockRetry(holdLockTime);
        }
    }
}
#endif
} // MiscServices
} // OHOS