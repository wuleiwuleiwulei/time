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

#include "time_permission.h"

#include "accesstoken_kit.h"
#include "ipc_skeleton.h"
#include "tokenid_kit.h"

namespace OHOS {
namespace MiscServices {
const std::string TimePermission::setTime = "ohos.permission.SET_TIME";
const std::string TimePermission::setTimeZone = "ohos.permission.SET_TIME_ZONE";
bool TimePermission::CheckCallingPermission(const std::string &permissionName)
{
    if (permissionName.empty()) {
        TIME_HILOGE(TIME_MODULE_COMMON, "permission check failed, permission name is empty");
        return false;
    }
    auto callerToken = IPCSkeleton::GetCallingTokenID();
    int result = Security::AccessToken::AccessTokenKit::VerifyAccessToken(callerToken, permissionName);
    if (result != Security::AccessToken::PERMISSION_GRANTED) {
        TIME_HILOGE(TIME_MODULE_COMMON, "permission check failed, result:%{public}d, permission:%{public}s",
            result, permissionName.c_str());
        return false;
    }
    return true;
}

bool TimePermission::CheckProxyCallingPermission()
{
    auto callerToken = IPCSkeleton::GetCallingTokenID();
    auto tokenType = Security::AccessToken::AccessTokenKit::GetTokenTypeFlag(callerToken);
    return (tokenType == Security::AccessToken::ATokenTypeEnum::TOKEN_NATIVE ||
            tokenType == Security::AccessToken::ATokenTypeEnum::TOKEN_SHELL);
}

bool TimePermission::CheckSystemUidCallingPermission(uint64_t tokenId)
{
    if (CheckProxyCallingPermission()) {
        return true;
    }
    return Security::AccessToken::TokenIdKit::IsSystemAppByFullTokenID(tokenId);
}
} // namespace MiscServices
} // namespace OHOS