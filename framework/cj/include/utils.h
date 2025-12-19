/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#ifndef OHOS_SYSTEM_DATE_TIME_UTILS_H
#define OHOS_SYSTEM_DATE_TIME_UTILS_H

namespace OHOS {
namespace CJSystemapi {
namespace SystemDateTime {

enum CjErrorCode : int16_t {
    ERROR_OK = 0,
    ERROR = -1,
    PERMISSION_ERROR = 201,
    SYSTEM_APP_ERROR = 202,
    PARAMETER_ERROR = 401,
};

} // SystemDateTime
} // CJSystemapi
} // OHOS

#endif // OHOS_SYSTEM_DATE_TIME_UTILS_H