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

#ifndef TIMER_INFO_H
#define TIMER_INFO_H

#include "timer_manager_interface.h"

namespace OHOS {
namespace MiscServices {

class TimerInfo {
public:
    const std::string name;
    const uint64_t id;
    const int type;
    const std::chrono::milliseconds origWhen;
    const bool wakeup;
    const bool autoRestore;
    const std::function<int32_t (const uint64_t)> callback;
    const std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> wantAgent;
    const uint32_t flags;
    const int uid;
    const int pid;

    enum TimerState : int {
        INIT = 0,
        ADJUST,
        PROXY,
    };

    enum PolicyState : int {
        NORMAL = 0,
        FORWARD,
        BACKWARD,
    };
    std::chrono::milliseconds when;
    std::chrono::milliseconds windowLength;
    std::chrono::steady_clock::time_point originWhenElapsed;
    std::chrono::steady_clock::time_point originMaxWhenElapsed;
    std::chrono::steady_clock::time_point whenElapsed;
    std::chrono::steady_clock::time_point maxWhenElapsed;
    std::chrono::milliseconds repeatInterval;
    std::chrono::milliseconds offset;
    std::string bundleName;
    int state;

    TimerInfo(std::string name, uint64_t id, int type,
        std::chrono::milliseconds when,
        std::chrono::steady_clock::time_point whenElapsed,
        std::chrono::milliseconds windowLength,
        std::chrono::steady_clock::time_point maxWhen,
        std::chrono::milliseconds interval,
        std::function<int32_t (const uint64_t)> callback,
        std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> wantAgent,
        uint32_t flags,
        bool autoRestore,
        int uid,
        int pid,
        const std::string &bundleName);
    virtual ~TimerInfo() = default;
    bool operator==(const TimerInfo &other) const;
    static std::shared_ptr<TimerInfo> CreateTimerInfo(std::string _name, uint64_t _id, int _type,
        uint64_t _triggerAtTime,
        int64_t _windowLength,
        uint64_t _interval,
        uint32_t _flag,
        bool _autoRestore,
        std::function<int32_t (const uint64_t)> _callback,
        std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> _wantAgent,
        int _uid,
        int _pid,
        const std::string &_bundleName);
    static std::chrono::steady_clock::time_point ConvertToElapsed(std::chrono::milliseconds when, int type);
    static std::chrono::steady_clock::time_point MaxTriggerTime(std::chrono::steady_clock::time_point now,
        std::chrono::steady_clock::time_point triggerAtTime,
        std::chrono::milliseconds interval);
    bool Matches(const std::string &packageName) const;
    void CalculateWhenElapsed(std::chrono::steady_clock::time_point nowElapsed);
    void CalculateOriWhenElapsed();
    bool UpdateWhenElapsedFromNow(std::chrono::steady_clock::time_point now, std::chrono::nanoseconds offset);
    bool ProxyTimer(const std::chrono::steady_clock::time_point &now, std::chrono::nanoseconds deltaTime);
    bool RestoreProxyTimer();
    bool AdjustTimer(const std::chrono::steady_clock::time_point &now, const uint32_t interval,
                     const uint32_t delta, const uint32_t policy);
    bool RestoreAdjustTimer();
    bool ChangeStatusToAdjust();
private:
    bool RestoreTimer();
    std::chrono::seconds ConvertAdjustPolicy(const uint32_t interval, const uint32_t policy);
};
} // MiscService
} // OHOS
#endif