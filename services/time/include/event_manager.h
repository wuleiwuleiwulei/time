/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "common_event_manager.h"
#include "common_event_support.h"

namespace OHOS {
namespace MiscServices {
using namespace OHOS::EventFwk;
class EventManager : public CommonEventSubscriber {
public:
    explicit EventManager(const CommonEventSubscribeInfo &subscriberInfo);
    ~EventManager() = default;
    virtual void OnReceiveEvent(const CommonEventData &data);

private:
    enum EventType : int8_t {
        UNKNOWN_BROADCAST_EVENT = 0,
        CONNECTED,
        POWER_BROADCAST_EVENT,
        NITZ_TIME_CHANGED_BROADCAST_EVENT,
        PACKAGE_REMOVED_EVENT,
    };

    using broadcastSubscriberFunc = std::function<void(const CommonEventData &data)>;

    void NetConnStateConnected(const CommonEventData &data);
    void PowerBroadcast(const CommonEventData &data);
    void NITZTimeChangeBroadcast(const CommonEventData &data);
    void PackageRemovedBroadcast(const CommonEventData &data);
    std::map<uint32_t, broadcastSubscriberFunc> memberFuncMap_;
};
} // namespace MiscServices
} // namespace OHOS

#endif // EVENT_MANAGER_H
