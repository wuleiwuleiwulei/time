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

#ifndef OHOS_SYSTEM_DATE_TIME_FFI_H
#define OHOS_SYSTEM_DATE_TIME_FFI_H

#include "native/ffi_remote_data.h"
#include "cj_ffi/cj_common_ffi.h"

#include <cstdint>

extern "C" {
    FFI_EXPORT RetCode FfiOHOSSysDateTimeSetTime(int64_t time);
    FFI_EXPORT RetDataI64 FfiOHOSSysDateTimegetCurrentTime(bool isNano);
    FFI_EXPORT RetDataI64 FfiOHOSSysDateTimegetRealActiveTime(bool isNano);
    FFI_EXPORT RetDataI64 FfiOHOSSysDateTimegetRealTime(bool isNano);
    FFI_EXPORT RetDataI64 FfiOHOSSysDateTimeGetTime(bool isNano);
    FFI_EXPORT RetDataI64 FfiOHOSSysDateTimeGetUptime(int32_t timeType, bool isNano);
    FFI_EXPORT RetCode FfiOHOSSysSetTimezone(char* timezone);
    FFI_EXPORT RetDataCString FfiOHOSSysGetTimezone();
}

#endif // OHOS_SYSTEM_DATE_TIME_FFI_H