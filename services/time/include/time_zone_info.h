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
#ifndef SERVICES_INCLUDE_TIME_ZONE_INFO_H
#define SERVICES_INCLUDE_TIME_ZONE_INFO_H

#include <sys/time.h>

#include "parameter.h"
#include "set"

namespace OHOS {
namespace MiscServices {
class TimeZoneInfo {
public:
    static TimeZoneInfo &GetInstance();
    bool GetTimezone(std::string &timezoneId);
    bool SetTimezone(const std::string &timezoneId);
    bool SetTimezoneToKernel();
    void Init();

private:
    std::string curTimezoneId_;
    std::mutex timezoneMutex_;
    std::set<std::string> GetTimeZoneAvailableIDs();
    std::string ConvertTimeZone(const std::string &timezoneId);
};
} // namespace MiscServices
} // namespace OHOS

#endif // SERVICES_INCLUDE_TIME_ZONE_INFO_H