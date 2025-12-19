/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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

#ifndef ANI_SYSTEM_TIMER_H
#define ANI_SYSTEM_TIMER_H

#include "time_service_client.h"

namespace OHOS {
namespace MiscServices {
namespace Time {

constexpr int TIMER_TYPE_REALTIME = 0;
constexpr int TIMER_TYPE_WAKEUP = 1;
constexpr int TIMER_TYPE_EXACT = 2;
constexpr int TIMER_TYPE_IDLE = 3;

class ITimerInfoInstance : public OHOS::MiscServices::ITimerInfo {
public:
    ITimerInfoInstance();
    virtual ~ITimerInfoInstance();
    virtual void OnTrigger() override;
    virtual void SetType(const int &type) override;
    virtual void SetRepeat(bool repeat) override;
    virtual void SetInterval(const uint64_t &interval) override;
    virtual void SetWantAgent(std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> wantAgent) override;
    void SetCallbackInfo(const std::function<void()> &callBack);

    private:
    std::function<void()> callBack = nullptr;
};
}
}
}
#endif // ANI_SYSTEM_TIMER_H