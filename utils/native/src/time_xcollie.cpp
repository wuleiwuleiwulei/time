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

#include "time_xcollie.h"

#include "time_hilog.h"

#ifdef HICOLLIE_ENABLE
#include "xcollie/xcollie.h"
#endif

namespace OHOS {
namespace MiscServices {
TimeXCollie::TimeXCollie(const std::string &name, uint32_t timeoutSeconds,
    std::function<void(void *)> func, void *arg, uint32_t flag)
{
    name_ = name;
    #ifdef HICOLLIE_ENABLE
    id_ = HiviewDFX::XCollie::GetInstance().SetTimer(name_, timeoutSeconds, func, arg, flag);
    #else
    id_ = -1;
    #endif
    isCanceled_ = false;
    TIME_HILOGD(TIME_MODULE_SERVICE, "start TimeXCollie, name:%{public}s,timeout:%{public}u,flag:%{public}u,"
        "id:%{public}d", name_.c_str(), timeoutSeconds, flag, id_);
}

TimeXCollie::~TimeXCollie()
{
    CancelTimeXCollie();
}

void TimeXCollie::CancelTimeXCollie()
{
    if (!isCanceled_) {
        #ifdef HICOLLIE_ENABLE
        HiviewDFX::XCollie::GetInstance().CancelTimer(id_);
        #endif
        isCanceled_ = true;
        TIME_HILOGD(TIME_MODULE_SERVICE, "cancel TimeXCollie, tag:%{public}s,id:%{public}d", name_.c_str(), id_);
    }
}
}
}