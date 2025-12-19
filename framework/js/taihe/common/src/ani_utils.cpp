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

#include "ani_utils.h"

#include "time_common.h"

namespace OHOS {
namespace MiscServices {
namespace Time {
static constexpr const char *SYSTEM_ERROR = "system error";
int32_t AniUtils::ConvertErrorCode(int32_t timeErrorCode)
{
    switch (timeErrorCode) {
        case MiscServices::E_TIME_NOT_SYSTEM_APP:
            return JsErrorCode::SYSTEM_APP_ERROR;
        case MiscServices::E_TIME_NO_PERMISSION:
            return JsErrorCode::PERMISSION_ERROR;
        case MiscServices::E_TIME_PARAMETERS_INVALID:
            return JsErrorCode::PARAMETER_ERROR;
        case MiscServices::E_TIME_NTP_UPDATE_FAILED:
            return JsErrorCode::NTP_UPDATE_ERROR;
        case MiscServices::E_TIME_NTP_NOT_UPDATE:
            return JsErrorCode::NTP_NOT_UPDATE_ERROR;
        default:
            return JsErrorCode::ERROR;
    }
}

std::string AniUtils::GetErrorMessage(int32_t errCode)
{
    auto it = CODE_TO_MESSAGE.find(errCode);
    if (it != CODE_TO_MESSAGE.end()) {
        return it->second;
    }
    return std::string(SYSTEM_ERROR);
}

} // namespace Time
} // namespace MiscServices
} // namespace OHOS