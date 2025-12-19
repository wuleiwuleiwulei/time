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

#include "ani_system_timer.h"
using namespace OHOS::MiscServices;

namespace OHOS {
namespace MiscServices {
namespace Time {

ITimerInfoInstance::ITimerInfoInstance()
{
}

ITimerInfoInstance::~ITimerInfoInstance()
{
}

void ITimerInfoInstance::SetType(const int &_type)
{
    type = _type;
}

void ITimerInfoInstance::SetRepeat(bool _repeat)
{
    repeat = _repeat;
}

void ITimerInfoInstance::SetInterval(const uint64_t &_interval)
{
    interval = _interval;
}

void ITimerInfoInstance::SetWantAgent(std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> _wantAgent)
{
    wantAgent = _wantAgent;
}

void ITimerInfoInstance::SetCallbackInfo(const std::function<void()> &_callBack)
{
    callBack = _callBack;
}

void ITimerInfoInstance::OnTrigger()
{
    if (callBack) {
        callBack();
    }
}

} // namespace Time
} // namespace MiscServices
} // namespace OHOS