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

#include "timer_info.h"

#include <cinttypes>
#include <memory>

namespace OHOS {
namespace MiscServices {
using namespace std::chrono;
static constexpr uint32_t HALF_SECOND = 2;
const auto INTERVAL_HOUR = hours(1);
const auto INTERVAL_HALF_DAY = hours(12);
constexpr int64_t MAX_MILLISECOND = std::numeric_limits<int64_t>::max() / 1000000;
const auto MIN_INTERVAL_ONE_SECONDS = seconds(1);
const auto MAX_INTERVAL = hours(24 * 365);
const auto MIN_FUZZABLE_INTERVAL = milliseconds(10000);
constexpr float_t BATCH_WINDOW_COE = 0.75;

bool TimerInfo::operator==(const TimerInfo &other) const
{
    return this->id == other.id;
}

bool TimerInfo::Matches(const std::string &packageName) const
{
    return false;
}

TimerInfo::TimerInfo(std::string _name, uint64_t _id, int _type,
                     std::chrono::milliseconds _when,
                     std::chrono::steady_clock::time_point _whenElapsed,
                     std::chrono::milliseconds _windowLength,
                     std::chrono::steady_clock::time_point _maxWhen,
                     std::chrono::milliseconds _interval,
                     std::function<int32_t (const uint64_t)> _callback,
                     std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> _wantAgent,
                     uint32_t _flags,
                     bool _autoRestore,
                     int _uid,
                     int _pid,
                     const std::string &_bundleName)
    : name {_name},
      id {_id},
      type {_type},
      origWhen {_when},
      wakeup {_type == ITimerManager::ELAPSED_REALTIME_WAKEUP || _type == ITimerManager::RTC_WAKEUP},
      autoRestore {_autoRestore},
      callback {std::move(_callback)},
      wantAgent {_wantAgent},
      flags {_flags},
      uid {_uid},
      pid {_pid},
      when {_when},
      windowLength {_windowLength},
      whenElapsed {_whenElapsed},
      maxWhenElapsed {_maxWhen},
      repeatInterval {_interval},
      bundleName {_bundleName}
{
    originWhenElapsed = _whenElapsed;
    originMaxWhenElapsed = _maxWhen;
    state = TimerState::INIT;
}

std::shared_ptr<TimerInfo> TimerInfo::CreateTimerInfo(std::string _name, uint64_t _id, int _type,
    uint64_t _triggerAtTime,
    int64_t _windowLength,
    uint64_t _interval,
    uint32_t _flag,
    bool _autoRestore,
    std::function<int32_t (const uint64_t)> _callback,
    std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> _wantAgent,
    int _uid,
    int _pid,
    const std::string &_bundleName)
{
    auto windowLengthDuration = milliseconds(_windowLength);
    if (windowLengthDuration > INTERVAL_HALF_DAY) {
        windowLengthDuration = INTERVAL_HOUR;
    }
    auto intervalDuration = milliseconds(_interval > MAX_MILLISECOND ? MAX_MILLISECOND : _interval);
    if (intervalDuration > milliseconds::zero() && intervalDuration < MIN_INTERVAL_ONE_SECONDS) {
        intervalDuration = MIN_INTERVAL_ONE_SECONDS;
    } else if (intervalDuration > MAX_INTERVAL) {
        intervalDuration = MAX_INTERVAL;
    }

    auto triggerTime = milliseconds(_triggerAtTime > MAX_MILLISECOND ? MAX_MILLISECOND : _triggerAtTime);
    auto nominalTrigger = ConvertToElapsed(triggerTime, _type);

    steady_clock::time_point maxElapsed;
    if (windowLengthDuration == milliseconds::zero()) {
        maxElapsed = nominalTrigger;
    } else if (windowLengthDuration < milliseconds::zero()) {
        maxElapsed = MaxTriggerTime(nominalTrigger, nominalTrigger, intervalDuration);
        windowLengthDuration = duration_cast<milliseconds>(maxElapsed - nominalTrigger);
    } else {
        maxElapsed = nominalTrigger + windowLengthDuration;
    }
    return std::make_shared<TimerInfo>(_name, _id, _type, triggerTime, nominalTrigger, windowLengthDuration, maxElapsed,
        intervalDuration, std::move(_callback), _wantAgent, _flag, _autoRestore, _uid,
        _pid, _bundleName);
}

void TimerInfo::CalculateOriWhenElapsed()
{
    auto nowElapsed = TimeUtils::GetBootTimeNs();
    auto elapsed = ConvertToElapsed(origWhen, type);
    steady_clock::time_point maxElapsed;
    if (windowLength == milliseconds::zero()) {
        maxElapsed = elapsed;
    } else {
        maxElapsed = (windowLength > milliseconds::zero()) ?
                     (elapsed + windowLength) :
                     MaxTriggerTime(nowElapsed, elapsed, repeatInterval);
    }
    originWhenElapsed = elapsed;
    originMaxWhenElapsed = maxElapsed;
}

void TimerInfo::CalculateWhenElapsed(std::chrono::steady_clock::time_point nowElapsed)
{
    auto Elapsed = ConvertToElapsed(when, type);
    steady_clock::time_point maxElapsed;
    if (windowLength == milliseconds::zero()) {
        maxElapsed = Elapsed;
    } else {
        maxElapsed = (windowLength > milliseconds::zero()) ?
                     (Elapsed + windowLength) :
                     MaxTriggerTime(nowElapsed, Elapsed, repeatInterval);
    }
    whenElapsed = Elapsed;
    maxWhenElapsed = maxElapsed;
}

/* Please make sure that the first param is current boottime */
bool TimerInfo::UpdateWhenElapsedFromNow(std::chrono::steady_clock::time_point now, std::chrono::nanoseconds offset)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "Update whenElapsed, id=%{public}" PRId64 "", id);
    auto oldWhenElapsed = whenElapsed;
    whenElapsed = now + offset;
    auto oldMaxWhenElapsed = maxWhenElapsed;
    maxWhenElapsed = whenElapsed + windowLength;
    std::chrono::milliseconds currentTime;
    if (type == ITimerManager::RTC || type == ITimerManager::RTC_WAKEUP) {
        currentTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    } else {
        currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    }
    auto offsetMill = std::chrono::duration_cast<std::chrono::milliseconds>(offset);
    when = currentTime + offsetMill;
    return (oldWhenElapsed != whenElapsed) || (oldMaxWhenElapsed != maxWhenElapsed);
}

bool TimerInfo::ProxyTimer(const std::chrono::steady_clock::time_point &now, std::chrono::nanoseconds deltaTime)
{
    auto res = UpdateWhenElapsedFromNow(now, deltaTime);
    //Change timer state
    state = TimerState::PROXY;
    return res;
}

bool TimerInfo::RestoreProxyTimer()
{
    //Change timer state
    switch (state) {
        case INIT:
            TIME_HILOGE(TIME_MODULE_SERVICE, "Restore timer in init state id:%{public}" PRIu64 "", id);
            break;
        case ADJUST:
            TIME_HILOGE(TIME_MODULE_SERVICE, "Restore timer in adjust state id:%{public}" PRIu64 "", id);
            state = INIT;
            break;
        case PROXY:
            state = INIT;
            break;
        default:
            TIME_HILOGE(TIME_MODULE_SERVICE, "Error state id:%{public}" PRIu64 ", state:%{public}d", id, state);
    }
    return RestoreTimer();
}

bool TimerInfo::ChangeStatusToAdjust()
{
    //Change timer state
    switch (state) {
        case INIT:
        case ADJUST:
            state = ADJUST;
            return true;
        case PROXY:
            TIME_HILOGD(TIME_MODULE_SERVICE, "Adjust timer in proxy state, id: %{public}" PRIu64 "", id);
            break;
        default:
            TIME_HILOGD(TIME_MODULE_SERVICE, "Error state, id: %{public}" PRIu64 ", state: %{public}d", id, state);
    }
    return false;
}

bool TimerInfo::AdjustTimer(const std::chrono::steady_clock::time_point &now,
                            const uint32_t interval, const uint32_t delta, const uint32_t policy)
{
    if (!ChangeStatusToAdjust()) {
        return false;
    }
    CalculateOriWhenElapsed();
    auto oldWhenElapsed = whenElapsed;
    auto oldMaxWhenElapsed = maxWhenElapsed;
    std::chrono::seconds auxiliaryCalcuSec = ConvertAdjustPolicy(interval, policy);
    if (interval == 0) {
        return false;
    }
    std::chrono::duration<int, std::ratio<1, 1>> intervalSec(interval);
    std::chrono::duration<int, std::ratio<1, 1>> deltaSec(delta);
    auto oldTimeSec = std::chrono::duration_cast<std::chrono::seconds>(originWhenElapsed.time_since_epoch());
    auto timeSec = ((oldTimeSec + auxiliaryCalcuSec) / intervalSec) * intervalSec + deltaSec;
    whenElapsed = std::chrono::steady_clock::time_point(timeSec);
    if (windowLength == std::chrono::milliseconds::zero()) {
        maxWhenElapsed = whenElapsed;
    } else {
        auto oldMaxTimeSec = std::chrono::duration_cast<std::chrono::seconds>(originWhenElapsed.time_since_epoch());
        auto maxTimeSec = ((oldMaxTimeSec + auxiliaryCalcuSec) / intervalSec) * intervalSec + deltaSec;
        maxWhenElapsed = std::chrono::steady_clock::time_point(maxTimeSec);
    }
    if (whenElapsed < now) {
        whenElapsed += std::chrono::duration_cast<std::chrono::milliseconds>(intervalSec);
    }
    if (maxWhenElapsed < now) {
        maxWhenElapsed += std::chrono::duration_cast<std::chrono::milliseconds>(intervalSec);
    }
    auto elapsedDelta = std::chrono::duration_cast<std::chrono::milliseconds>(
        whenElapsed.time_since_epoch() - oldWhenElapsed.time_since_epoch());
    when = when + elapsedDelta;
    TIME_HILOGD(TIME_MODULE_SERVICE, "adjust timer id: %{public}" PRId64
                ", old elapsed: %{public}lld, when elapsed: %{public}lld"
                ", interval: %{public}u, policy: %{public}u",
                id, oldWhenElapsed.time_since_epoch().count(),
                whenElapsed.time_since_epoch().count(), interval, policy);
    return (oldWhenElapsed != whenElapsed) || (oldMaxWhenElapsed != maxWhenElapsed);
}

std::chrono::seconds TimerInfo::ConvertAdjustPolicy(const uint32_t interval, const uint32_t policy)
{
    switch (policy) {
        case FORWARD:
            return std::chrono::seconds(0);
        case BACKWARD: {
            std::chrono::duration<int, std::ratio<1, 1>> wholeSec(interval);
            return std::chrono::duration_cast<std::chrono::seconds>(wholeSec);
        }
        case NORMAL:
        default: {
            std::chrono::duration<int, std::ratio<1, HALF_SECOND>> halfSec(interval);
            return std::chrono::duration_cast<std::chrono::seconds>(halfSec);
        }
    }
}

bool TimerInfo::RestoreAdjustTimer()
{
    //Change timer state
    switch (state) {
        case INIT:
            TIME_HILOGE(TIME_MODULE_SERVICE, "Restore timer in init state, id: %{public}" PRIu64"", id);
            break;
        case ADJUST:
            state = INIT;
            break;
        case PROXY:
            return true;
        default:
            TIME_HILOGE(TIME_MODULE_SERVICE, "Error state, id: %{public}" PRIu64 ", state: %{public}d", id, state);
    }
    return RestoreTimer();
}

bool TimerInfo::RestoreTimer()
{
    CalculateOriWhenElapsed();
    auto oldWhenElapsed = whenElapsed;
    auto oldMaxWhenElapsed = maxWhenElapsed;
    auto oldWhen = when;
    whenElapsed = originWhenElapsed;
    maxWhenElapsed = originMaxWhenElapsed;
    when = origWhen;
    return (oldWhenElapsed != whenElapsed) || (oldMaxWhenElapsed != maxWhenElapsed) || (oldWhen != when);
}

std::chrono::steady_clock::time_point TimerInfo::ConvertToElapsed(std::chrono::milliseconds when, int type)
{
    if (type == ITimerManager::RTC || type == ITimerManager::RTC_WAKEUP) {
        auto systemTimeNow = system_clock::now().time_since_epoch();
        auto bootTimePoint = TimeUtils::GetBootTimeNs();
        auto offset = when - systemTimeNow;
        TIME_HILOGD(TIME_MODULE_SERVICE, "systemTimeNow : %{public}lld offset : %{public}lld",
                    systemTimeNow.count(), offset.count());
        return bootTimePoint + offset;
    }
    std::chrono::steady_clock::time_point elapsed (when);
    return elapsed;
}

steady_clock::time_point TimerInfo::MaxTriggerTime(steady_clock::time_point now,
                                                   steady_clock::time_point triggerAtTime,
                                                   milliseconds interval)
{
    milliseconds futurity = (interval == milliseconds::zero()) ?
                            (duration_cast<milliseconds>(triggerAtTime - now)) : interval;
    if (futurity < MIN_FUZZABLE_INTERVAL) {
        futurity = milliseconds::zero();
    }
    return triggerAtTime + milliseconds(static_cast<long>(BATCH_WINDOW_COE * futurity.count()));
}
} // MiscServices
} // OHOS