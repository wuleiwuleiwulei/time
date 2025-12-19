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

#include <thread>
#include "event_manager.h"
#include "ntp_update_time.h"
#include "time_tick_notify.h"
#include "timer_manager.h"

namespace OHOS {
namespace MiscServices {
using namespace OHOS::EventFwk;
using namespace OHOS::AAFwk;
static constexpr uint32_t CONNECTED_EVENT = 3;

EventManager::EventManager(const OHOS::EventFwk::CommonEventSubscribeInfo &subscriberInfo)
    : CommonEventSubscriber(subscriberInfo)
{
    memberFuncMap_ = {
        { CONNECTED,
            [this] (const CommonEventData &data) { NetConnStateConnected(data); } },
        { POWER_BROADCAST_EVENT,
            [this] (const CommonEventData &data) { PowerBroadcast(data); } },
        { NITZ_TIME_CHANGED_BROADCAST_EVENT,
            [this] (const CommonEventData &data) { NITZTimeChangeBroadcast(data); } },
        { PACKAGE_REMOVED_EVENT,
            [this] (const CommonEventData &data) { PackageRemovedBroadcast(data); } },
    };
}

void EventManager::OnReceiveEvent(const CommonEventData &data)
{
    uint32_t code = UNKNOWN_BROADCAST_EVENT;
    std::string action = data.GetWant().GetAction();
    TIME_HILOGD(TIME_MODULE_SERVICE, "receive one broadcast:%{public}s", action.c_str());

    if (action == CommonEventSupport::COMMON_EVENT_CONNECTIVITY_CHANGE && data.GetCode() == CONNECTED_EVENT) {
        code = CONNECTED;
    } else if (action == CommonEventSupport::COMMON_EVENT_SCREEN_ON) {
        code = POWER_BROADCAST_EVENT;
    } else if (action == CommonEventSupport::COMMON_EVENT_NITZ_TIME_CHANGED) {
        code = NITZ_TIME_CHANGED_BROADCAST_EVENT;
    } else if (action == CommonEventSupport::COMMON_EVENT_PACKAGE_REMOVED ||
               action == CommonEventSupport::COMMON_EVENT_BUNDLE_REMOVED ||
               action == CommonEventSupport::COMMON_EVENT_PACKAGE_FULLY_REMOVED) {
        code = PACKAGE_REMOVED_EVENT;
    }

    auto itFunc = memberFuncMap_.find(code);
    if (itFunc != memberFuncMap_.end()) {
        auto memberFunc = itFunc->second;
        if (memberFunc != nullptr) {
            return memberFunc(data);
        }
    }
}

void EventManager::NetConnStateConnected(const CommonEventData &data)
{
    TIME_HILOGI(TIME_MODULE_SERVICE, "Internet ready");
    if (NtpUpdateTime::IsInUpdateInterval()) {
        NtpUpdateTime::SetSystemTime(NtpUpdateSource::NET_CONNECTED);
    } else {
        auto setSystemTime = [this]() { NtpUpdateTime::SetSystemTime(NtpUpdateSource::NET_CONNECTED); };
        std::thread thread(setSystemTime);
        thread.detach();
    }
}

void EventManager::PowerBroadcast(const CommonEventData &data)
{
    TimeTickNotify::GetInstance().Callback();
}

void EventManager::NITZTimeChangeBroadcast(const CommonEventData &data)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "NITZ Time changed broadcast code:%{public}d", data.GetCode());
    NtpUpdateTime::GetInstance().UpdateNITZSetTime();
}

void EventManager::PackageRemovedBroadcast(const CommonEventData &data)
{
    auto uid = data.GetWant().GetIntParam(std::string("uid"), -1);
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return;
    }
    timerManager->OnPackageRemoved(uid);
}
} // namespace MiscServices
} // namespace OHOS