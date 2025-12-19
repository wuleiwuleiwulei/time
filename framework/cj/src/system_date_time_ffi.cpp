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

#include "time_hilog.h"
#include "cj_ffi/cj_common_ffi.h"
#include "system_date_time.h"
#include "system_date_time_ffi.h"

namespace OHOS {
namespace CJSystemapi {
namespace SystemDateTime {

extern "C" {
RetCode FfiOHOSSysDateTimeSetTime(int64_t time)
{
    RetCode ret = SystemDateTimeImpl::SetTime(time);
    if (ret != SUCCESS_CODE) {
        return ret;
    }
    return ret;
}

RetDataI64 FfiOHOSSysDateTimegetCurrentTime(bool isNano)
{
    RetDataI64 ret = { .code = INVALID_DATA_ID, .data = 0 };
    auto [state, time] = SystemDateTimeImpl::getCurrentTime(isNano);
    if (state != SUCCESS_CODE) {
        ret.code = state;
        ret.data = 0;
        return ret;
    }
    ret.code = state;
    ret.data = time;
    return ret;
}

RetDataI64 FfiOHOSSysDateTimegetRealActiveTime(bool isNano)
{
    RetDataI64 ret = { .code = INVALID_DATA_ID, .data = 0 };
    auto [state, time] = SystemDateTimeImpl::getRealActiveTime(isNano);
    if (state != SUCCESS_CODE) {
        ret.code = state;
        ret.data = 0;
        return ret;
    }
    ret.code = state;
    ret.data = time;
    return ret;
}

RetDataI64 FfiOHOSSysDateTimegetRealTime(bool isNano)
{
    RetDataI64 ret = { .code = INVALID_DATA_ID, .data = 0 };
    auto [state, time] = SystemDateTimeImpl::getRealTime(isNano);
    if (state != SUCCESS_CODE) {
        ret.code = state;
        ret.data = 0;
        return ret;
    }
    ret.code = state;
    ret.data = time;
    return ret;
}

RetDataI64 FfiOHOSSysDateTimeGetTime(bool isNano)
{
    RetDataI64 ret = { .code = INVALID_DATA_ID, .data = 0 };
    auto [state, time] = SystemDateTimeImpl::getTime(isNano);
    if (state != SUCCESS_CODE) {
        ret.code = state;
        ret.data = 0;
        return ret;
    }
    ret.code = state;
    ret.data = time;
    return ret;
}

RetDataI64 FfiOHOSSysDateTimeGetUptime(int32_t timeType, bool isNano)
{
    RetDataI64 ret = { .code = INVALID_DATA_ID, .data = 0 };
    auto [state, time] = SystemDateTimeImpl::getUpTime(timeType, isNano);
    if (state != SUCCESS_CODE) {
        ret.code = state;
        ret.data = 0;
        return ret;
    }
    ret.code = state;
    ret.data = time;
    return ret;
}

RetCode FfiOHOSSysSetTimezone(char* timezone)
{
    RetCode ret = SystemDateTimeImpl::SetTimeZone(timezone);
    if (ret != SUCCESS_CODE) {
        return ret;
    }
    return ret;
}

RetDataCString FfiOHOSSysGetTimezone()
{
    RetDataCString ret = { .code = INVALID_DATA_ID, .data = nullptr };
    auto [state, time] = SystemDateTimeImpl::getTimezone();
    if (state != SUCCESS_CODE) {
        ret.code = state;
        ret.data = nullptr;
        return ret;
    }
    ret.code = state;
    ret.data = time;
    return ret;
}
}
} // SystemDateTime
} // CJSystemapi
} // OHOS