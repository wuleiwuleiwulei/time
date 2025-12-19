/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#ifndef TIME_PERMISSION_H
#define TIME_PERMISSION_H

#include "time_common.h"

namespace OHOS {
namespace MiscServices {
class TimePermission {
public:
    static const std::string setTime;
    static const std::string setTimeZone;

    static bool CheckCallingPermission(const std::string &permissionName);
    static bool CheckProxyCallingPermission();
    static bool CheckSystemUidCallingPermission(uint64_t tokenId);
};
} // namespace MiscServices
} // namespace OHOS
#endif // TIME_PERMISSION_H