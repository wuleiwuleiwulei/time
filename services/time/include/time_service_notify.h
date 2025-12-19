/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef TIME_SERVICE_NOTIFY_H
#define TIME_SERVICE_NOTIFY_H
#include <common_event_publish_info.h>
#include <want.h>

#include "time_common.h"

namespace OHOS {
namespace MiscServices {
class TimeServiceNotify {

public:
    static TimeServiceNotify &GetInstance();;
    bool PublishTimeChangeEvents(int64_t eventTime);
    bool PublishTimeZoneChangeEvents(int64_t eventTime);
    bool PublishTimeTickEvents(int64_t eventTime);
    bool PublishTimerTriggerEvents();
    bool RepublishEvents();

private:
    using IntentWant = OHOS::AAFwk::Want;
    using PublishInfo = OHOS::EventFwk::CommonEventPublishInfo;

    bool PublishEvents(int64_t eventTime, const IntentWant &want, const PublishInfo &publishInfo);
};
} // namespace MiscServices
} // namespace OHOS

#endif // TIME_SERVICE_NOTIFY_H