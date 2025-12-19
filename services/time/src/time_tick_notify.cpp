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
#include "time_tick_notify.h"

#include "time_system_ability.h"

using namespace std::chrono;

namespace OHOS {
namespace MiscServices {
namespace {
constexpr uint64_t MINUTE_TO_MILLISECOND = 60000;
constexpr uint64_t SECOND_TO_MILLISECOND = 1000;
constexpr int64_t SECOND_TO_NANO = 1000000000;
} // namespace

TimeTickNotify &TimeTickNotify::GetInstance()
{
    static TimeTickNotify instance;
    return instance;
}

TimeTickNotify::TimeTickNotify() = default;
TimeTickNotify::~TimeTickNotify() = default;

void TimeTickNotify::Init()
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "Tick notify start");
    TimerPara timerPara{};
    timerPara.timerType = static_cast<int>(ITimerManager::TimerType::RTC);
    timerPara.windowLength = 0;
    timerPara.interval = 0;
    timerPara.flag = 0;
    auto callback = [this](uint64_t id) -> int32_t {
        this->Callback();
        return E_TIME_OK;
    };
    std::lock_guard<std::mutex> lock(timeridMutex_);
    TimeSystemAbility::GetInstance()->CreateTimer(timerPara, callback, timerId_);
    auto trigger = RefreshNextTriggerTime();
    TimeSystemAbility::GetInstance()->StartTimer(timerId_, trigger.first);
    TIME_HILOGI(TIME_MODULE_SERVICE, "Tick timer timerId:%{public}" PRIu64 "", timerId_);
}

void TimeTickNotify::Callback()
{
    std::lock_guard<std::mutex> lock(timeridMutex_);
    auto trigger = RefreshNextTriggerTime();
    if (trigger.second) {
        auto currentTime = steady_clock::now().time_since_epoch().count();
        if (std::abs(currentTime - lastTriggerTime_) > SECOND_TO_NANO) {
            TimeServiceNotify::GetInstance().PublishTimeTickEvents(currentTime);
            lastTriggerTime_ = currentTime;
        }
    }
    TimeSystemAbility::GetInstance()->StartTimer(timerId_, trigger.first);
    TIME_SIMPLIFY_HILOGI(TIME_MODULE_SERVICE, "tick id:%{public}" PRIu64 " time:%{public}" PRIu64 "",
        timerId_, trigger.first / SECOND_TO_MILLISECOND);
}

std::pair<uint64_t, bool> TimeTickNotify::RefreshNextTriggerTime()
{
    int64_t time = 0;
    TimeUtils::GetWallTimeMs(time);
    uint64_t currTime = static_cast<uint64_t>(time);
    uint64_t timeMilliseconds = currTime % MINUTE_TO_MILLISECOND;
    bool isFirstSecond = timeMilliseconds < SECOND_TO_MILLISECOND;
    uint64_t nextTriggerTime = ((currTime / MINUTE_TO_MILLISECOND) + 1) * MINUTE_TO_MILLISECOND;
    return std::make_pair(nextTriggerTime, isFirstSecond);
}

void TimeTickNotify::Stop()
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "start");
    TimeSystemAbility::GetInstance()->DestroyTimer(timerId_);
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
}

uint64_t TimeTickNotify::GetTickTimerId()
{
    return timerId_;
}
} // namespace MiscServices
} // namespace OHOS