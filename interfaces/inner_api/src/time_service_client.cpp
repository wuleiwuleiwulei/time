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

#include "time_service_client.h"

#include "system_ability_definition.h"
#include "timer_call_back.h"
#include <cinttypes>

namespace OHOS {
namespace MiscServices {
namespace {
static constexpr int MILLI_TO_SEC = 1000LL;
static constexpr int NANO_TO_SEC = 1000000000LL;
constexpr int32_t NANO_TO_MILLI = NANO_TO_SEC / MILLI_TO_SEC;
}

std::mutex TimeServiceClient::instanceLock_;
sptr<TimeServiceClient> TimeServiceClient::instance_;

TimeServiceClient::TimeServiceListener::TimeServiceListener ()
{
}

void TimeServiceClient::TimeServiceListener::OnAddSystemAbility(
    int32_t saId, const std::string &deviceId)
{
    if (saId == TIME_SERVICE_ID) {
        TimeServiceClient::GetInstance()->ConnectService();
        auto proxy = TimeServiceClient::GetInstance()->GetProxy();
        if (proxy == nullptr) {
            return;
        }
        auto timerCallbackInfoObject = TimerCallback::GetInstance()->AsObject();
        if (!timerCallbackInfoObject) {
            TIME_HILOGE(TIME_MODULE_CLIENT, "New TimerCallback failed");
            return;
        }
        std::map<uint64_t, std::shared_ptr<RecoverTimerInfo>> recoverTimer;
        {
            auto timerServiceClient = TimeServiceClient::GetInstance();
            std::lock_guard<std::mutex> lock(timerServiceClient->recoverTimerInfoLock_);
            recoverTimer = timerServiceClient->recoverTimerInfoMap_;
        }
        TIME_HILOGW(TIME_MODULE_CLIENT, "recoverTimer count:%{public}zu", recoverTimer.size());
        auto iter = recoverTimer.begin();
        for (; iter != recoverTimer.end(); iter++) {
            auto timerId = iter->first;
            auto timerInfo = iter->second->timerInfo;
            TIME_HILOGW(TIME_MODULE_CLIENT, "recover cb-timer: %{public}" PRId64 "", timerId);
            if (timerInfo->wantAgent) {
                proxy->CreateTimer(timerInfo->name, timerInfo->type, timerInfo->repeat, timerInfo->disposable,
                    timerInfo->autoRestore, timerInfo->interval, *timerInfo->wantAgent,
                    timerCallbackInfoObject, timerId);
            } else {
                proxy->CreateTimerWithoutWA(timerInfo->name, timerInfo->type, timerInfo->repeat, timerInfo->disposable,
                    timerInfo->autoRestore, timerInfo->interval, timerCallbackInfoObject, timerId);
            }
            
            if (iter->second->state == 1) {
                proxy->StartTimer(timerId, iter->second->triggerTime);
            }
        }
        return;
    } else {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Id is not TIME_SERVICE_ID");
        return;
    }
}

void TimeServiceClient::TimeServiceListener::OnRemoveSystemAbility(
    int32_t saId, const std::string &deviceId)
{
}

TimeServiceClient::TimeServiceClient()
{
    listener_ = new (std::nothrow) TimeServiceListener();
    ConnectService();
}

TimeServiceClient::~TimeServiceClient()
{
    auto proxy = GetProxy();
    if (proxy != nullptr) {
        auto remoteObject = proxy->AsObject();
        if (remoteObject != nullptr) {
            remoteObject->RemoveDeathRecipient(deathRecipient_);
        }
    }
}

sptr<TimeServiceClient> TimeServiceClient::GetInstance()
{
    if (instance_ == nullptr) {
        std::lock_guard<std::mutex> autoLock(instanceLock_);
        if (instance_ == nullptr) {
            instance_ = new TimeServiceClient;
        }
    }
    return instance_;
}

bool TimeServiceClient::SubscribeSA(sptr<ISystemAbilityManager> systemAbilityManager)
{
    auto timeServiceListener = listener_;
    if (timeServiceListener == nullptr) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Get timeServiceListener failed");
        return false;
    }
    auto ret = systemAbilityManager->SubscribeSystemAbility(TIME_SERVICE_ID, timeServiceListener);
    if (ret != 0) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "SubscribeSystemAbility failed: %{public}d", ret);
        return false;
    }
    return true;
}

bool TimeServiceClient::ConnectService()
{
    if (GetProxy() != nullptr) {
        return true;
    }
    sptr<ISystemAbilityManager> systemAbilityManager =
        SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (systemAbilityManager == nullptr) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Getting SystemAbilityManager failed");
        return false;
    }
    auto systemAbility = systemAbilityManager->GetSystemAbility(TIME_SERVICE_ID);
    if (systemAbility == nullptr) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Get SystemAbility failed");
        return false;
    }
    IPCObjectProxy *ipcProxy = reinterpret_cast<IPCObjectProxy*>(systemAbility.GetRefPtr());
    if (ipcProxy->IsObjectDead()) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Time service is dead");
        return false;
    }
    std::lock_guard<std::mutex> autoLock(deathLock_);
    if (deathRecipient_ == nullptr) {
        deathRecipient_ = new (std::nothrow) TimeSaDeathRecipient();
        if (deathRecipient_ == nullptr) {
            return false;
        }
    }
    systemAbility->AddDeathRecipient(deathRecipient_);
    sptr<ITimeService> proxy = iface_cast<ITimeService>(systemAbility);
    if (proxy == nullptr) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Get TimeServiceProxy from SA failed");
        return false;
    }
    SetProxy(proxy);
    if (!SubscribeSA(systemAbilityManager)) {
        return false;
    }
    return true;
}

bool TimeServiceClient::GetTimeByClockId(clockid_t clockId, struct timespec &tv)
{
    if (clock_gettime(clockId, &tv) < 0) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Failed clock_gettime, errno: %{public}s", strerror(errno));
        return false;
    }
    return true;
}

bool TimeServiceClient::SetTime(int64_t time)
{
    if (!ConnectService()) {
        return false;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return false;
    }
    return proxy->SetTime(time, APIVersion::API_VERSION_7) == ERR_OK;
}

bool TimeServiceClient::SetTime(int64_t milliseconds, int32_t &code)
{
    if (!ConnectService()) {
        code = E_TIME_SA_DIED;
        return false;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        code = E_TIME_NULLPTR;
        return false;
    }
    code = proxy->SetTime(milliseconds, APIVersion::API_VERSION_7);
    code = ConvertErrCode(code);
    return code == ERR_OK;
}

int32_t TimeServiceClient::SetTimeV9(int64_t time)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    int32_t code = proxy->SetTime(time, APIVersion::API_VERSION_9);
    code = ConvertErrCode(code);
    return code;
}

int32_t TimeServiceClient::SetAutoTime(bool autoTime)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    int32_t code = proxy->SetAutoTime(autoTime);
    code = ConvertErrCode(code);
    return code;
}

bool TimeServiceClient::SetTimeZone(const std::string &timezoneId)
{
    if (!ConnectService()) {
        return false;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return false;
    }
    return proxy->SetTimeZone(timezoneId, APIVersion::API_VERSION_7) == ERR_OK;
}

bool TimeServiceClient::SetTimeZone(const std::string &timezoneId, int32_t &code)
{
    if (!ConnectService()) {
        code = E_TIME_SA_DIED;
        return false;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        code =  E_TIME_NULLPTR;
        return false;
    }
    code = proxy->SetTimeZone(timezoneId, APIVersion::API_VERSION_7);
    code = ConvertErrCode(code);
    TIME_HILOGD(TIME_MODULE_CLIENT, "settimezone end");
    return code == ERR_OK;
}

int32_t TimeServiceClient::SetTimeZoneV9(const std::string &timezoneId)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    int32_t code = proxy->SetTimeZone(timezoneId, APIVersion::API_VERSION_9);
    code = ConvertErrCode(code);
    return code;
}

uint64_t TimeServiceClient::CreateTimer(std::shared_ptr<ITimerInfo> timerOptions)
{
    uint64_t timerId = 0;
    auto errCode = CreateTimerV9(timerOptions, timerId);
    TIME_HILOGD(TIME_MODULE_SERVICE, "CreateTimer id: %{public}" PRId64 "", timerId);
    if (errCode != E_TIME_OK) {
        return 0;
    }
    if (timerId == 0) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Create timer failed");
        return 0;
    }
    return timerId;
}

// needs to acquire the lock `recoverTimerInfoLock_` before calling this method
void TimeServiceClient::CheckNameLocked(std::string name)
{
    auto it = std::find(timerNameList_.begin(), timerNameList_.end(), name);
    if (it == timerNameList_.end()) {
        timerNameList_.push_back(name);
        return;
    }
    auto recoverIter = std::find_if(recoverTimerInfoMap_.begin(), recoverTimerInfoMap_.end(),
                                    [name](const auto& pair) {
                                        return pair.second->timerInfo->name == name;
                                    });
    if (recoverIter != recoverTimerInfoMap_.end()) {
        recoverIter = recoverTimerInfoMap_.erase(recoverIter);
    }
}

int32_t TimeServiceClient::CreateTimerV9(std::shared_ptr<ITimerInfo> timerOptions, uint64_t &timerId)
{
    if (timerOptions == nullptr) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Input nullptr");
        return E_TIME_NULLPTR;
    }
    if (!ConnectService()) {
        return E_TIME_NULLPTR;
    }
    auto timerCallbackInfoObject = TimerCallback::GetInstance()->AsObject();
    if (!timerCallbackInfoObject) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "New TimerCallback failed");
        return E_TIME_NULLPTR;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    int32_t errCode = E_TIME_OK;
    if (timerOptions->wantAgent) {
        errCode = proxy->CreateTimer(timerOptions->name, timerOptions->type, timerOptions->repeat,
            timerOptions->disposable, timerOptions->autoRestore, timerOptions->interval,
            *timerOptions->wantAgent, timerCallbackInfoObject, timerId);
    } else {
        errCode = proxy->CreateTimerWithoutWA(timerOptions->name, timerOptions->type, timerOptions->repeat,
            timerOptions->disposable, timerOptions->autoRestore, timerOptions->interval,
            timerCallbackInfoObject, timerId);
    }
    if (errCode != E_TIME_OK) {
        errCode = ConvertErrCode(errCode);
        TIME_HILOGE(TIME_MODULE_CLIENT, "create timer failed, errCode=%{public}d", errCode);
        return errCode;
    }
    if (timerOptions->wantAgent == nullptr) {
        auto ret = RecordRecoverTimerInfoMap(timerOptions, timerId);
        if (ret != E_TIME_OK) {
            return ret;
        }
    }
    TIME_HILOGD(TIME_MODULE_CLIENT, "CreateTimer id: %{public}" PRId64 "", timerId);
    if (!TimerCallback::GetInstance()->InsertTimerCallbackInfo(timerId, timerOptions)) {
        return E_TIME_DEAL_FAILED;
    }
    return errCode;
}

int32_t TimeServiceClient::RecordRecoverTimerInfoMap(std::shared_ptr<ITimerInfo> timerOptions, uint64_t timerId)
{
    std::lock_guard<std::mutex> lock(recoverTimerInfoLock_);
    if (timerOptions->name != "") {
        CheckNameLocked(timerOptions->name);
    }
    auto info = recoverTimerInfoMap_.find(timerId);
    if (info != recoverTimerInfoMap_.end()) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "recover timer info already insert");
        return E_TIME_DEAL_FAILED;
    } else {
        auto recoverTimerInfo = std::make_shared<RecoverTimerInfo>();
        recoverTimerInfo->timerInfo = timerOptions;
        recoverTimerInfo->state = 0;
        recoverTimerInfo->triggerTime = 0;
        recoverTimerInfoMap_[timerId] = recoverTimerInfo;
    }
    return E_TIME_OK;
}

bool TimeServiceClient::StartTimer(uint64_t timerId, uint64_t triggerTime)
{
    int32_t errCode = StartTimerV9(timerId, triggerTime);
    if (errCode != E_TIME_OK) {
        return false;
    }
    return true;
}

int32_t TimeServiceClient::StartTimerV9(uint64_t timerId, uint64_t triggerTime)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    auto startRet = proxy->StartTimer(timerId, triggerTime);
    if (startRet != 0) {
        startRet = ConvertErrCode(startRet);
        TIME_HILOGE(TIME_MODULE_CLIENT, "start timer failed: %{public}d", startRet);
        return startRet;
    }
    std::lock_guard<std::mutex> lock(recoverTimerInfoLock_);
    auto info = recoverTimerInfoMap_.find(timerId);
    if (info != recoverTimerInfoMap_.end()) {
        info->second->state = 1;
        info->second->triggerTime = triggerTime;
    }
    return startRet;
}

bool TimeServiceClient::StopTimer(uint64_t timerId)
{
    int32_t errCode = StopTimerV9(timerId);
    if (errCode != E_TIME_OK) {
        return false;
    }
    return true;
}

int32_t TimeServiceClient::StopTimerV9(uint64_t timerId)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    auto stopRet = proxy->StopTimer(timerId);
    if (stopRet != 0) {
        stopRet = ConvertErrCode(stopRet);
        TIME_HILOGE(TIME_MODULE_CLIENT, "stop timer failed: %{public}d", stopRet);
        return stopRet;
    }
    std::lock_guard<std::mutex> lock(recoverTimerInfoLock_);
    auto info = recoverTimerInfoMap_.find(timerId);
    if (info != recoverTimerInfoMap_.end()) {
        info->second->state = 0;
    }
    return stopRet;
}

bool TimeServiceClient::DestroyTimer(uint64_t timerId)
{
    int32_t errCode = DestroyTimerV9(timerId);
    if (errCode != E_TIME_OK) {
        return false;
    }
    return true;
}

int32_t TimeServiceClient::DestroyTimerV9(uint64_t timerId)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    auto errCode = proxy->DestroyTimer(timerId);
    if (errCode != 0) {
        errCode = ConvertErrCode(errCode);
        TIME_HILOGE(TIME_MODULE_CLIENT, "destroy timer failed: %{public}d", errCode);
        return errCode;
    }
    TimerCallback::GetInstance()->RemoveTimerCallbackInfo(timerId);
    std::lock_guard<std::mutex> lock(recoverTimerInfoLock_);
    auto info = recoverTimerInfoMap_.find(timerId);
    if (info != recoverTimerInfoMap_.end()) {
        if (info->second->timerInfo->name != "") {
            auto it = std::find(timerNameList_.begin(), timerNameList_.end(), info->second->timerInfo->name);
            if (it != timerNameList_.end()) {
                timerNameList_.erase(it);
            }
        }
        recoverTimerInfoMap_.erase(timerId);
    }
    return errCode;
}

bool TimeServiceClient::DestroyTimerAsync(uint64_t timerId)
{
    int32_t errCode = DestroyTimerAsyncV9(timerId);
    if (errCode != E_TIME_OK) {
        return false;
    }
    return true;
}

int32_t TimeServiceClient::DestroyTimerAsyncV9(uint64_t timerId)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }

    auto errCode = proxy->DestroyTimerAsync(timerId);
    if (errCode != 0) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "destroy timer failed: %{public}d", errCode);
        return errCode;
    }
    TimerCallback::GetInstance()->RemoveTimerCallbackInfo(timerId);
    std::lock_guard<std::mutex> lock(recoverTimerInfoLock_);
    auto info = recoverTimerInfoMap_.find(timerId);
    if (info != recoverTimerInfoMap_.end()) {
        recoverTimerInfoMap_.erase(timerId);
    }
    return errCode;
}

void TimeServiceClient::HandleRecoverMap(uint64_t timerId)
{
    std::lock_guard<std::mutex> lock(recoverTimerInfoLock_);
    auto info = recoverTimerInfoMap_.find(timerId);
    if (info == recoverTimerInfoMap_.end()) {
        TIME_HILOGD(TIME_MODULE_CLIENT, "timer:%{public}" PRId64 "is not in map", timerId);
        return;
    }
    if (info->second->timerInfo->repeat == true) {
        return;
    }
    if (info->second->timerInfo->disposable == true) {
        TIME_HILOGD(TIME_MODULE_CLIENT, "timer:%{public}" PRId64 "is disposable", timerId);
        recoverTimerInfoMap_.erase(timerId);
        return;
    }
    TIME_HILOGD(TIME_MODULE_CLIENT, "timer:%{public}" PRId64 "change state by trigger", timerId);
    info->second->state = 0;
}

std::string TimeServiceClient::GetTimeZone()
{
    std::string timeZoneId;
    if (!ConnectService()) {
        return std::string("");
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return std::string("");
    }
    if (proxy->GetTimeZone(timeZoneId) != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return std::string("");
    }
    return timeZoneId;
}

int32_t TimeServiceClient::GetTimeZone(std::string &timezoneId)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    if (proxy->GetTimeZone(timezoneId) != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return E_TIME_SA_DIED;
    }
    return E_TIME_OK;
}

int64_t TimeServiceClient::GetWallTimeMs()
{
    int64_t time;
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_REALTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return -1;
    }
    time = tv.tv_sec * MILLI_TO_SEC + tv.tv_nsec / NANO_TO_MILLI;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return time;
}

int32_t TimeServiceClient::GetWallTimeMs(int64_t &time)
{
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_REALTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return E_TIME_SA_DIED;
    }
    time = tv.tv_sec * MILLI_TO_SEC + tv.tv_nsec / NANO_TO_MILLI;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return E_TIME_OK;
}

int64_t TimeServiceClient::GetWallTimeNs()
{
    int64_t time;
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_REALTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return -1;
    }
    time = tv.tv_sec * NANO_TO_SEC + tv.tv_nsec;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return time;
}

int32_t TimeServiceClient::GetWallTimeNs(int64_t &time)
{
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_REALTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return E_TIME_SA_DIED;
    }
    time = tv.tv_sec * NANO_TO_SEC + tv.tv_nsec;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return E_TIME_OK;
}

int64_t TimeServiceClient::GetBootTimeMs()
{
    int64_t time;
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_BOOTTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return -1;
    }
    time = tv.tv_sec * MILLI_TO_SEC + tv.tv_nsec / NANO_TO_MILLI;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return time;
}

int32_t TimeServiceClient::GetBootTimeMs(int64_t &time)
{
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_BOOTTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return E_TIME_SA_DIED;
    }
    time = tv.tv_sec * MILLI_TO_SEC + tv.tv_nsec / NANO_TO_MILLI;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return E_TIME_OK;
}

int64_t TimeServiceClient::GetBootTimeNs()
{
    int64_t time;
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_BOOTTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return -1;
    }
    time = tv.tv_sec * NANO_TO_SEC + tv.tv_nsec;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return time;
}

int32_t TimeServiceClient::GetBootTimeNs(int64_t &time)
{
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_BOOTTIME, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return E_TIME_SA_DIED;
    }
    time = tv.tv_sec * NANO_TO_SEC + tv.tv_nsec;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return E_TIME_OK;
}

int64_t TimeServiceClient::GetMonotonicTimeMs()
{
    int64_t time;
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_MONOTONIC, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return -1;
    }
    time = tv.tv_sec * MILLI_TO_SEC + tv.tv_nsec / NANO_TO_MILLI;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return time;
}

int32_t TimeServiceClient::GetMonotonicTimeMs(int64_t &time)
{
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_MONOTONIC, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return E_TIME_SA_DIED;
    }
    time = tv.tv_sec * MILLI_TO_SEC + tv.tv_nsec / NANO_TO_MILLI;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return E_TIME_OK;
}

int64_t TimeServiceClient::GetMonotonicTimeNs()
{
    int64_t time;
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_MONOTONIC, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return -1;
    }
    time = tv.tv_sec * NANO_TO_SEC + tv.tv_nsec;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return time;
}

int32_t TimeServiceClient::GetMonotonicTimeNs(int64_t &time)
{
    struct timespec tv {};
    if (!GetTimeByClockId(CLOCK_MONOTONIC, tv)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return E_TIME_SA_DIED;
    }
    time = tv.tv_sec * NANO_TO_SEC + tv.tv_nsec;
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return E_TIME_OK;
}

int64_t TimeServiceClient::GetThreadTimeMs()
{
    int64_t time;
    if (!ConnectService()) {
        return -1;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    if (proxy->GetThreadTimeMs(time) != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return -1;
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return time;
}

int32_t TimeServiceClient::GetThreadTimeMs(int64_t &time)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    if (proxy->GetThreadTimeMs(time) != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return E_TIME_SA_DIED;
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return E_TIME_OK;
}

int64_t TimeServiceClient::GetThreadTimeNs()
{
    int64_t time;
    if (!ConnectService()) {
        return -1;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    if (proxy->GetThreadTimeNs(time) != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return -1;
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return time;
}

int32_t TimeServiceClient::GetThreadTimeNs(int64_t &time)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    if (proxy->GetThreadTimeNs(time) != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "get failed");
        return E_TIME_SA_DIED;
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "Result: %{public}" PRId64 "", time);
    return E_TIME_OK;
}

bool TimeServiceClient::ProxyTimer(int32_t uid, std::set<int> pidList, bool isProxy, bool needRetrigger)
{
    if (!ConnectService()) {
        return false;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return false;
    }
    std::vector<int> pidVector;
    std::copy(pidList.begin(), pidList.end(), std::back_inserter(pidVector));
    auto errCode = proxy->ProxyTimer(uid, pidVector, isProxy, needRetrigger);
    return errCode == E_TIME_OK;
}

int32_t TimeServiceClient::AdjustTimer(bool isAdjust, uint32_t interval, uint32_t delta)
{
    TIME_HILOGD(TIME_MODULE_CLIENT, "Adjust Timer isAdjust: %{public}d", isAdjust);
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    auto code = proxy->AdjustTimer(isAdjust, interval, delta);
    code = ConvertErrCode(code);
    return code;
}

int32_t TimeServiceClient::SetTimerExemption(const std::unordered_set<std::string> &nameArr, bool isExemption)
{
    TIME_HILOGD(TIME_MODULE_CLIENT, "set time exemption size: %{public}zu", nameArr.size());
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    if (nameArr.empty()) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Nothing need cache");
        return E_TIME_NOT_FOUND;
    }
    std::vector<std::string> nameVector;
    std::copy(nameArr.begin(), nameArr.end(), std::back_inserter(nameVector));
    auto code = proxy->SetTimerExemption(nameVector, isExemption);
    code = ConvertErrCode(code);
    return code;
}

int32_t TimeServiceClient::SetAdjustPolicy(const std::unordered_map<std::string, uint32_t> &policyMap)
{
    TIME_HILOGD(TIME_MODULE_CLIENT, "set adjust policy size: %{public}zu", policyMap.size());
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    if (policyMap.empty()) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Nothing need cache");
        return E_TIME_NOT_FOUND;
    }
    auto code = proxy->SetAdjustPolicy(policyMap);
    code = ConvertErrCode(code);
    return code;
}

bool TimeServiceClient::ResetAllProxy()
{
    TIME_HILOGD(TIME_MODULE_CLIENT, "ResetAllProxy");
    if (!ConnectService()) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "ResetAllProxy ConnectService failed");
        return false;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return false;
    }
    auto code = proxy->ResetAllProxy();
    return code == ERR_OK;
}

int32_t TimeServiceClient::GetNtpTimeMs(int64_t &time)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    auto code = proxy->GetNtpTimeMs(time);
    code = ConvertErrCode(code);
    return code;
}

int32_t TimeServiceClient::GetRealTimeMs(int64_t &time)
{
    if (!ConnectService()) {
        return E_TIME_SA_DIED;
    }
    auto proxy = GetProxy();
    if (proxy == nullptr) {
        return E_TIME_NULLPTR;
    }
    auto code = proxy->GetRealTimeMs(time);
    code = ConvertErrCode(code);
    return code;
}

int32_t TimeServiceClient::ConvertErrCode(int32_t errCode)
{
    switch (errCode) {
        case ERR_INVALID_VALUE:
            return E_TIME_WRITE_PARCEL_ERROR;
        case ERR_INVALID_DATA:
            return E_TIME_WRITE_PARCEL_ERROR;
        case E_TIME_NULLPTR:
            return E_TIME_DEAL_FAILED;
        default:
            return errCode;
    }
}

sptr<ITimeService> TimeServiceClient::GetProxy()
{
    std::lock_guard<std::mutex> autoLock(proxyLock_);
    return timeServiceProxy_;
}

void TimeServiceClient::SetProxy(sptr<ITimeService> proxy)
{
    std::lock_guard<std::mutex> autoLock(proxyLock_);
    timeServiceProxy_ = proxy;
}

// LCOV_EXCL_START
// The method has no input parameters, impossible to construct fuzz test.
void TimeServiceClient::ClearProxy()
{
    std::lock_guard<std::mutex> autoLock(proxyLock_);
    timeServiceProxy_ = nullptr;
}
// LCOV_EXCL_STOP
} // namespace MiscServices
} // namespace OHOS