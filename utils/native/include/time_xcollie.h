/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef TIME_XCOLLIE_H
#define TIME_XCOLLIE_H

#include <string>

namespace OHOS {
namespace MiscServices {

constexpr int TIME_OUT_SECONDS = 5;
constexpr int XCOLLIE_FLAG_LOG = 1;

class TimeXCollie {
public:
    TimeXCollie(const std::string &name, uint32_t timeoutSeconds = TIME_OUT_SECONDS,
        std::function<void(void *)> func = nullptr, void *arg = nullptr, uint32_t flag = XCOLLIE_FLAG_LOG);

    ~TimeXCollie();

    void CancelTimeXCollie();

private:
    int32_t id_;
    std::string name_;
    bool isCanceled_;
};
}
}
#endif // TIME_XCOLLIE_H
