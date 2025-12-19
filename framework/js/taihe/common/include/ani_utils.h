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

#ifndef ANI_UTILS_H
#define ANI_UTILS_H

#include <cstdint>
#include <string>
#include <map>

namespace OHOS {
namespace MiscServices {
namespace Time {

enum JsErrorCode : int32_t {
    ERROR_OK = 0,
    ERROR = -1,
    PERMISSION_ERROR = 201,
    SYSTEM_APP_ERROR = 202,
    PARAMETER_ERROR = 401,
    NTP_UPDATE_ERROR = 13000001,
    NTP_NOT_UPDATE_ERROR = 13000002,
};

const std::map<int32_t, std::string> CODE_TO_MESSAGE = {
    { JsErrorCode::SYSTEM_APP_ERROR, "Permission verification failed. A non-system application calls a system API" },
    { JsErrorCode::PARAMETER_ERROR, "Parameter error" },
    { JsErrorCode::PERMISSION_ERROR, "Permission denied" },
    { JsErrorCode::ERROR, "Parameter check failed, permission denied, or system error." },
    { JsErrorCode::NTP_UPDATE_ERROR, "Ntp update error" },
    { JsErrorCode::NTP_NOT_UPDATE_ERROR, "Ntp not update error" },
};

class AniUtils {
    public:
    static int32_t ConvertErrorCode(int32_t timeErrorCode);
    static std::string GetErrorMessage(int32_t errCode);
};

} // namespace Time
} // namespace MiscServices
} // namespace OHOS

#endif // NAPI_UTILS_H