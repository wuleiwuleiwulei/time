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

#include "time_service.h"
#include "parameter.h"
#include "sysparam_errno.h"

const char * const TIMEZONE_KEY = "persist.time.timezone";

TimeService_ErrCode OH_TimeService_GetTimeZone(char *timeZone, uint32_t len)
{
    int ret = GetParameter(TIMEZONE_KEY, "", timeZone, len);
    if (ret == EC_INVALID) {
        return TIMESERVICE_ERR_INVALID_PARAMETER;
    }
    if (ret < 0) {
        return TIMESERVICE_ERR_INTERNAL_ERROR;
    }
    return TIMESERVICE_ERR_OK;
}
