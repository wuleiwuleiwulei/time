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

#ifndef OHOS_SYSTEM_DATE_TIME_H
#define OHOS_SYSTEM_DATE_TIME_H

#include <cstdint>
#include <string>

namespace OHOS {
namespace CJSystemapi {
namespace SystemDateTime {

class SystemDateTimeImpl {
public:
    static int SetTime(int64_t time);
    static std::tuple<int32_t, int64_t> getCurrentTime(bool isNano);
    static std::tuple<int32_t, int64_t> getRealActiveTime(bool isNano);
    static std::tuple<int32_t, int64_t> getRealTime(bool isNano);
    static std::tuple<int32_t, int64_t> getTime(bool isNano);
    static std::tuple<int32_t, int64_t> getUpTime(int32_t timeType, bool isNano);
    static int SetTimeZone(char* timezone);
    static std::tuple<int32_t, char*> getTimezone();
private:
    static int32_t GetDeviceTime(clockid_t clockId, bool isNano, int64_t &time);
};

char* MallocCString(const std::string& origin);
} // SystemDateTime
} // CJSystemapi
} // OHOS

#endif // OHOS_SYSTEM_DATE_TIME_H