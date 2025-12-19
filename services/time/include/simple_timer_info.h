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

#ifndef SIMPLE_TIMER_INFO_H
#define SIMPLE_TIMER_INFO_H

#include "itimer_info.h"
namespace OHOS {
namespace MiscServices {
class SimpleTimerInfo : public ITimerInfo, public Parcelable {
public:
    SimpleTimerInfo(std::string _name,
                    int _type,
                    bool _repeat,
                    bool _disposable,
                    bool _autoRestore,
                    uint64_t _interval,
                    std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> _wantAgent);
    virtual ~SimpleTimerInfo();
    void OnTrigger() override;
    void SetType(const int &type) override;
    void SetRepeat(bool repeat) override;
    void SetInterval(const uint64_t &interval) override;
    void SetWantAgent(std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> wantAgent) override;

    bool Marshalling(Parcel& parcel) const override;
    static SimpleTimerInfo *Unmarshalling(Parcel& parcel);
};
} // namespace MiscServices
} // namespace OHOS

#endif // SIMPLE_TIMER_INFO_H