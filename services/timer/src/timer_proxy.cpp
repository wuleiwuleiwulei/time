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

#include <cinttypes>

#include "timer_proxy.h"

namespace OHOS {
namespace MiscServices {
using namespace std::chrono;
using namespace OHOS::AppExecFwk;

namespace {
constexpr int UID_PROXY_OFFSET = 32;
}

IMPLEMENT_SINGLE_INSTANCE(TimerProxy)

uint64_t GetProxyKey(int uid, int pid)
{
    uint64_t key = (static_cast<uint64_t>(uid) << UID_PROXY_OFFSET) | static_cast<uint64_t>(pid);
    return key;
}

std::pair<int, int> ParseProxyKey(uint64_t key)
{
    auto uid = static_cast<uint32_t>(key >> UID_PROXY_OFFSET);
    auto pid = key & ((static_cast<uint64_t>(1) << UID_PROXY_OFFSET) - 1);
    return std::make_pair(uid, pid);
}

int32_t TimerProxy::CallbackAlarmIfNeed(const std::shared_ptr<TimerInfo> &alarm)
{
    if (alarm == nullptr) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "callback alarm is nullptr!");
        return E_TIME_NULLPTR;
    }
    int32_t ret = alarm->callback(alarm->id);
    if (ret != 0) {
        TIME_SIMPLIFY_HILOGE(TIME_MODULE_SERVICE, "cb:%{public}" PRId64 " ret:%{public}d",
            alarm->id, ret);
    }
    return ret;
}

bool TimerProxy::ProxyTimer(int32_t uid, int pid, bool isProxy, bool needRetrigger,
    const std::chrono::steady_clock::time_point &now,
    std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start. pid=%{public}d, isProxy=%{public}u, needRetrigger=%{public}u",
        pid, isProxy, needRetrigger);

    std::lock_guard<std::mutex> lockProxy(proxyMutex_);
    if (isProxy) {
        UpdateProxyWhenElapsedForProxyTimers(uid, pid, now, insertAlarmCallback, needRetrigger);
        return true;
    }

    if (!RestoreProxyWhenElapsedForProxyTimers(uid, pid, now, insertAlarmCallback, needRetrigger)) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Pid%{public}d doesn't exist in the proxy list" PRId64 "", pid);
        return false;
    }
    return true;
}

bool TimerProxy::AdjustTimer(bool isAdjust, uint32_t interval,
    const std::chrono::steady_clock::time_point &now, uint32_t delta,
    std::function<void(AdjustTimerCallback adjustTimer)> updateTimerDeliveries)
{
    std::lock_guard<std::mutex> lockProxy(adjustMutex_);
    TIME_HILOGD(TIME_MODULE_SERVICE, "adjust timer state:%{public}d, interval:%{public}d, delta:%{public}d",
        isAdjust, interval, delta);
    auto callback = [=] (std::shared_ptr<TimerInfo> timer) {
        if (timer == nullptr) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "adjust timer is nullptr!");
            return false;
        }
        bool isOverdue = (now > timer->originWhenElapsed);
        if (isAdjust && !isOverdue) {
            return UpdateAdjustWhenElapsed(now, interval, delta, timer);
        }
        return RestoreAdjustWhenElapsed(timer);
    };
    updateTimerDeliveries(callback);
    if (!isAdjust) {
        adjustTimers_.clear();
    }
    return true;
}

bool TimerProxy::UpdateAdjustWhenElapsed(const std::chrono::steady_clock::time_point &now,
    uint32_t interval, uint32_t delta, std::shared_ptr<TimerInfo> &timer)
{
    if (IsTimerExemption(timer)) {
        TIME_HILOGD(TIME_MODULE_SERVICE, "adjust exemption timer bundleName:%{public}s",
            timer->bundleName.c_str());
        return false;
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "adjust single time id:%{public}" PRId64 ", "
        "uid:%{public}d, bundleName:%{public}s",
        timer->id, timer->uid, timer->bundleName.c_str());
    uint32_t policy = 0;
    auto it = adjustPolicyList_.find(timer->bundleName);
    if (it != adjustPolicyList_.end()) {
        policy = it->second;
    }
    auto ret = timer->AdjustTimer(now, interval, delta, policy);
    if (ret) {
        adjustTimers_.insert(timer->id);
    }
    return ret;
}

bool TimerProxy::RestoreAdjustWhenElapsed(std::shared_ptr<TimerInfo> &timer)
{
    if (adjustTimers_.find(timer->id) == adjustTimers_.end()) {
        return false;
    }
    return timer->RestoreAdjustTimer();
}

bool TimerProxy::SetTimerExemption(const std::unordered_set<std::string> &nameArr, bool isExemption)
{
    std::lock_guard<std::mutex> lockProxy(adjustMutex_);
    bool isChanged = false;
    if (!isExemption) {
        for (const auto &name : nameArr) {
            adjustExemptionList_.erase(name);
        }
        return isChanged;
    }
    adjustExemptionList_.insert(nameArr.begin(), nameArr.end());
    return isChanged;
}

bool TimerProxy::IsTimerExemption(std::shared_ptr<TimerInfo> timer)
{
    auto key = timer->bundleName + "|" + timer->name;
    TIME_HILOGD(TIME_MODULE_SERVICE, "key is:%{public}s", key.c_str());
    if ((adjustExemptionList_.find(timer->bundleName) != adjustExemptionList_.end()
        || adjustExemptionList_.find(key) != adjustExemptionList_.end())
        && timer->windowLength == milliseconds::zero()) {
        return true;
    }
    return false;
}

void TimerProxy::SetAdjustPolicy(const std::unordered_map<std::string, uint32_t> &policyMap)
{
    std::lock_guard<std::mutex> lockProxy(adjustMutex_);
    for (const auto& policy : policyMap) {
        adjustPolicyList_[policy.first] = policy.second;
    }
}

bool TimerProxy::ResetAllProxy(const std::chrono::steady_clock::time_point &now,
    std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start");
    ResetAllProxyWhenElapsed(now, insertAlarmCallback);
    return true;
}

void TimerProxy::EraseTimerFromProxyTimerMap(const uint64_t id, const int uid, const int pid)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "erase timer from proxy timer map, id=%{public}" PRId64 ", pid=%{public}u",
        id, pid);
    std::lock_guard<std::mutex> lock(proxyMutex_);
    uint64_t key = GetProxyKey(uid, pid);
    auto it = proxyTimers_.find(key);
    if (it != proxyTimers_.end()) {
        it->second.erase(std::remove_if(it->second.begin(), it->second.end(),
            [id](uint64_t timerId) { return timerId == id; }), it->second.end());
    }
}

void TimerProxy::EraseAlarmItem(
    const uint64_t id, std::unordered_map<uint64_t, std::shared_ptr<TimerInfo>> &idAlarmsMap)
{
    auto itAlarms = idAlarmsMap.find(id);
    if (itAlarms != idAlarmsMap.end()) {
        TIME_HILOGD(TIME_MODULE_SERVICE, "timer already exists, id=%{public}" PRId64 "", id);
        idAlarmsMap.erase(itAlarms);
    }
}

int32_t TimerProxy::CountUidTimerMapByUid(int32_t uid)
{
    std::lock_guard<std::mutex> lock(uidTimersMutex_);
    auto it = uidTimersMap_.find(uid);
    if (it == uidTimersMap_.end()) {
        return 0;
    }
    return it->second.size();
}

void TimerProxy::RecordUidTimerMap(const std::shared_ptr<TimerInfo> &alarm, const bool isRebatched)
{
    if (isRebatched) {
        TIME_HILOGD(TIME_MODULE_SERVICE, "Record uid timer info map, isRebatched:%{public}d", isRebatched);
        return;
    }
    if (alarm == nullptr) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "record uid timer map alarm is nullptr!");
        return;
    }

    std::lock_guard<std::mutex> lock(uidTimersMutex_);
    auto it = uidTimersMap_.find(alarm->uid);
    if (it == uidTimersMap_.end()) {
        std::unordered_map<uint64_t, std::shared_ptr<TimerInfo>> idAlarmsMap;
        idAlarmsMap.insert(std::make_pair(alarm->id, alarm));
        uidTimersMap_.insert(std::make_pair(alarm->uid, idAlarmsMap));
        return;
    }

    EraseAlarmItem(alarm->id, it->second);
    it->second.insert(std::make_pair(alarm->id, alarm));
}

void TimerProxy::RemoveUidTimerMap(const std::shared_ptr<TimerInfo> &alarm)
{
    if (alarm == nullptr) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "remove uid timer map alarm is nullptr!");
        return;
    }

    std::lock_guard<std::mutex> lock(uidTimersMutex_);
    auto it = uidTimersMap_.find(alarm->uid);
    if (it == uidTimersMap_.end()) {
        return;
    }

    EraseAlarmItem(alarm->id, it->second);
    if (it->second.empty()) {
        uidTimersMap_.erase(it);
    }
}

void TimerProxy::RemoveUidTimerMapLocked(const std::shared_ptr<TimerInfo> &alarm)
{
    if (alarm == nullptr) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "remove uid timer map alarm is nullptr!");
        return;
    }
    auto it = uidTimersMap_.find(alarm->uid);
    if (it == uidTimersMap_.end()) {
        return;
    }
    EraseAlarmItem(alarm->id, it->second);
    if (it->second.empty()) {
        uidTimersMap_.erase(it);
    }
}

void TimerProxy::RecordProxyTimerMap(const std::shared_ptr<TimerInfo> &alarm, bool isPid)
{
    std::lock_guard<std::mutex> lock(proxyMutex_);
    auto uid = alarm->uid;
    auto pid = alarm->pid;
    uint64_t key;
    if (isPid) {
        key =  GetProxyKey(uid, pid);
    } else {
        key = GetProxyKey(uid, 0);
    }
    auto it = proxyTimers_.find(key);
    if (it != proxyTimers_.end()) {
        proxyTimers_[key].push_back(alarm->id);
    } else {
        proxyTimers_.insert(std::make_pair(key, std::vector<uint64_t>{alarm->id}));
    }
}

void TimerProxy::RemoveUidTimerMap(const uint64_t id)
{
    std::lock_guard<std::mutex> lock(uidTimersMutex_);
    for (auto itUidsTimer = uidTimersMap_.begin(); itUidsTimer!= uidTimersMap_.end(); ++itUidsTimer) {
        for (auto itTimerId = itUidsTimer->second.begin(); itTimerId!= itUidsTimer->second.end(); ++itTimerId) {
            if (itTimerId->first != id) {
                continue;
            }

            itUidsTimer->second.erase(itTimerId);
            if (itUidsTimer->second.empty()) {
                uidTimersMap_.erase(itUidsTimer);
            }
            return;
        }
    }
}

bool TimerProxy::IsProxy(const int32_t uid, const int32_t pid)
{
    std::lock_guard<std::mutex> lock(proxyMutex_);
    uint64_t key = GetProxyKey(uid, pid);
    auto it = proxyTimers_.find(key);
    if (it == proxyTimers_.end()) {
        return false;
    }
    return true;
}

// needs to acquire the lock `proxyMutex_` before calling this method
bool TimerProxy::IsProxyLocked(const int32_t uid, const int32_t pid)
{
    uint64_t key = GetProxyKey(uid, pid);
    auto it = proxyTimers_.find(key);
    if (it == proxyTimers_.end()) {
        return false;
    }
    return true;
}

void TimerProxy::UpdateProxyWhenElapsedForProxyTimers(int32_t uid, int pid,
    const std::chrono::steady_clock::time_point &now,
    std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback,
    bool needRetrigger)
{
    uint64_t key = GetProxyKey(uid, pid);
    auto it = proxyTimers_.find(key);
    if (it != proxyTimers_.end()) {
        TIME_HILOGD(TIME_MODULE_SERVICE, "uid:%{public}d pid:%{public}d is already proxy", uid, pid);
        return;
    }
    std::lock_guard<std::mutex> lockUidTimers(uidTimersMutex_);
    std::vector<uint64_t> timerList;
    auto itUidTimersMap = uidTimersMap_.find(uid);
    if (itUidTimersMap == uidTimersMap_.end()) {
        TIME_HILOGD(TIME_MODULE_SERVICE, "uid:%{public}d in map not found", uid);
        proxyTimers_[key] = timerList;
        return;
    }

    for (auto itTimerInfo = itUidTimersMap->second.begin(); itTimerInfo!= itUidTimersMap->second.end();
        ++itTimerInfo) {
        if (pid == 0 || pid == itTimerInfo->second->pid) {
            if (!needRetrigger) {
                insertAlarmCallback(itTimerInfo->second, false);
                break;
            }
            itTimerInfo->second->ProxyTimer(now, milliseconds(proxyDelayTime_));
            TIME_HILOGD(TIME_MODULE_SERVICE, "Update proxy WhenElapsed for proxy pid map. "
                "pid= %{public}d, id=%{public}" PRId64 ", timer whenElapsed=%{public}lld, now=%{public}lld",
                itTimerInfo->second->pid, itTimerInfo->second->id,
                itTimerInfo->second->whenElapsed.time_since_epoch().count(),
                now.time_since_epoch().count());
            insertAlarmCallback(itTimerInfo->second, true);
            timerList.push_back(itTimerInfo->first);
        }
    }
    proxyTimers_[key] = timerList;
}

bool TimerProxy::RestoreProxyWhenElapsed(const int uid, const int pid,
    const std::chrono::steady_clock::time_point &now,
    std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback,
    bool needRetrigger)
{
    uint64_t key = GetProxyKey(uid, pid);
    auto itProxy = proxyTimers_.find(key);
    if (itProxy == proxyTimers_.end()) {
        TIME_HILOGD(TIME_MODULE_SERVICE, "uid:%{public}d pid:%{public}d not in proxy", uid, pid);
        return false;
    }
    std::lock_guard<std::mutex> lockPidTimers(uidTimersMutex_);
    auto itTimer = uidTimersMap_.find(uid);
    if (uidTimersMap_.find(uid) == uidTimersMap_.end()) {
        TIME_HILOGD(TIME_MODULE_SERVICE, "uid:%{public}d timer info not found, erase proxy map", uid);
        return true;
    }

    for (auto elem : itProxy->second) {
        auto itTimerInfo = itTimer->second.find(elem);
        if (itTimerInfo == itTimer->second.end()) {
            continue;
        }
        itTimerInfo->second->RestoreProxyTimer();
        TIME_HILOGD(TIME_MODULE_SERVICE, "Restore proxy WhenElapsed by pid"
            "pid= %{public}d, id=%{public}" PRId64 ", timer whenElapsed=%{public}lld, now=%{public}lld",
            itTimerInfo->second->pid, itTimerInfo->second->id,
            itTimerInfo->second->whenElapsed.time_since_epoch().count(),
            now.time_since_epoch().count());
        insertAlarmCallback(itTimerInfo->second, needRetrigger);
    }
    return true;
}

bool TimerProxy::RestoreProxyWhenElapsedForProxyTimers(const int uid, const int pid,
    const std::chrono::steady_clock::time_point &now,
    std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback,
    bool needRetrigger)
{
    uint64_t key = GetProxyKey(uid, pid);
    bool ret = RestoreProxyWhenElapsed(uid, pid, now, insertAlarmCallback, needRetrigger);
    if (ret) {
        proxyTimers_.erase(key);
    }
    return ret;
}

void TimerProxy::ResetAllProxyWhenElapsed(const std::chrono::steady_clock::time_point &now,
    std::function<void(std::shared_ptr<TimerInfo> &alarm, bool needRetrigger)> insertAlarmCallback)
{
    std::lock_guard<std::mutex> lockProxy(proxyMutex_);
    for (auto it = proxyTimers_.begin(); it != proxyTimers_.end(); ++it) {
        auto resPair = ParseProxyKey(it->first);
        RestoreProxyWhenElapsed(resPair.first, resPair.second, now, insertAlarmCallback, true);
    }
    proxyTimers_.clear();
}

#ifdef HIDUMPER_ENABLE
bool TimerProxy::ShowProxyTimerInfo(int fd, const int64_t now)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start");
    std::lock_guard<std::mutex> lockPidProxy(proxyMutex_);
    dprintf(fd, "current time %lld\n", now);
    for (auto itProxyPids = proxyTimers_.begin(); itProxyPids != proxyTimers_.end(); ++itProxyPids) {
        auto resPair = ParseProxyKey(itProxyPids->first);
        dprintf(fd, " - proxy uid = %lu pid = %lu\n", resPair.first, resPair.second);
        for (auto elem : itProxyPids->second) {
            dprintf(fd, "   * save timer id          = %llu\n", elem);
        }
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
    return true;
}

bool TimerProxy::ShowUidTimerMapInfo(int fd, const int64_t now)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start");
    std::lock_guard<std::mutex> lockProxy(uidTimersMutex_);
    dprintf(fd, "current time %lld\n", now);
    for (auto itTimerInfoMap = uidTimersMap_.begin(); itTimerInfoMap != uidTimersMap_.end(); ++itTimerInfoMap) {
            dprintf(fd, " - uid = %lu\n", itTimerInfoMap->first);
        for (auto itTimerInfo = itTimerInfoMap->second.begin(); itTimerInfo != itTimerInfoMap->second.end();
            ++itTimerInfo) {
            dprintf(fd, "   * timer id          = %llu\n", itTimerInfo->second->id);
            dprintf(fd, "   * timer whenElapsed = %lld\n", itTimerInfo->second->whenElapsed.time_since_epoch().count());
        }
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
    return true;
}

void TimerProxy::ShowAdjustTimerInfo(int fd)
{
    std::lock_guard<std::mutex> lockProxy(adjustMutex_);
    dprintf(fd, "show adjust timer");
    for (auto timerId : adjustTimers_) {
        dprintf(fd, " * timer id            = %lu\n", timerId);
    }
}

bool TimerProxy::ShowProxyDelayTime(int fd)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start");
    dprintf(fd, "proxy delay time:%lld ms\n", proxyDelayTime_);
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
    return true;
}
#endif

int64_t TimerProxy::GetProxyDelayTime() const
{
    return proxyDelayTime_;
}
} // MiscServices
} // OHOS
