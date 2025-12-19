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

#ifndef TIMER_MANAGER_INTERFACE_H
#define TIMER_MANAGER_INTERFACE_H

#include "want_agent_helper.h"
#include "time_common.h"

namespace OHOS {
namespace MiscServices {
struct TimerEntry {
    std::string name;
    uint64_t id;
    int type;
    int64_t windowLength;
    uint64_t interval;
    uint32_t flag;
    bool autoRestore;
    std::function<int32_t (const uint64_t)> callback;
    std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> wantAgent;
    int uid;
    int pid;
    std::string bundleName;
};

class ITimerManager {
public:
    enum TimerFlag : uint8_t {
        STANDALONE = 1 << 0,
        WAKE_FROM_IDLE = 1 << 1,
        ALLOW_WHILE_IDLE = 1 << 2,
        ALLOW_WHILE_IDLE_UNRESTRICTED = 1 << 3,
        IDLE_UNTIL = 1 << 4,
        INEXACT_REMINDER = 1 << 5,
        IS_DISPOSABLE = 1 << 6,
    };

    enum TimerType : uint8_t {
        RTC_WAKEUP = 0,
        RTC = 1,
        ELAPSED_REALTIME_WAKEUP = 2,
        ELAPSED_REALTIME = 3,
        #ifdef SET_AUTO_REBOOT_ENABLE
        POWER_ON_ALARM = 6,
        #endif
        TIMER_TYPE_BUTT
    };

    virtual int32_t CreateTimer(TimerPara &paras,
                                std::function<int32_t (const uint64_t)> callback,
                                std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> wantAgent,
                                int uid, int pid, uint64_t &timerId, DatabaseType type) = 0;

    virtual int32_t StartTimer(uint64_t timerId, uint64_t triggerTime) = 0;
    virtual int32_t StopTimer(uint64_t timerId) = 0;
    virtual int32_t DestroyTimer(uint64_t timerId) = 0;
    virtual ~ITimerManager() = default;
    virtual bool ProxyTimer(int32_t uid, std::set<int> pidList, bool isProxy, bool needRetrigger) = 0;
    virtual bool AdjustTimer(bool isAdjust, uint32_t interval, uint32_t delta) = 0;
    virtual void SetTimerExemption(const std::unordered_set<std::string> &nameArr, bool isExemption) = 0;
    virtual bool ResetAllProxy() = 0;
    virtual void SetAdjustPolicy(const std::unordered_map<std::string, uint32_t> &policyMap) = 0;
}; // ITimerManager
} // MiscService
} // OHOS

#endif