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

#include "simple_timer_info.h"
#include "time_hilog.h"

namespace OHOS {
namespace MiscServices {
SimpleTimerInfo::SimpleTimerInfo(std::string _name,
                                 int _type,
                                 bool _repeat,
                                 bool _disposable,
                                 bool _autoRestore,
                                 uint64_t _interval,
                                 std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> _wantAgent)
{
    name = _name;
    type = _type;
    repeat = _repeat;
    disposable = _disposable;
    autoRestore = _autoRestore;
    interval = _interval;
    wantAgent = _wantAgent;
}
SimpleTimerInfo::~SimpleTimerInfo()
{
}
void SimpleTimerInfo::SetType(const int &_type)
{
    type = _type;
}

void SimpleTimerInfo::SetRepeat(bool _repeat)
{
    repeat = _repeat;
}
void SimpleTimerInfo::SetInterval(const uint64_t &_interval)
{
    interval = _interval;
}
void SimpleTimerInfo::SetWantAgent(std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> _wantAgent)
{
    wantAgent = _wantAgent;
}
void SimpleTimerInfo::OnTrigger()
{
}

// LCOV_EXCL_START
// Non-public interfaces, do not require fuzz testing coverage.
bool SimpleTimerInfo::Marshalling(Parcel& parcel) const
{
    if (!parcel.WriteString(name)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Failed to write name");
        return false;
    }
    if (!parcel.WriteInt32(type)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Failed to write type");
        return false;
    }
    if (!parcel.WriteBool(repeat)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Failed to write repeat");
        return false;
    }
    if (!parcel.WriteBool(disposable)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Failed to write disposable");
        return false;
    }
    if (!parcel.WriteBool(autoRestore)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Failed to write autoRestore");
        return false;
    }
    if (!parcel.WriteUint64(interval)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Failed to write interval");
        return false;
    }
    if (!parcel.WriteBool(wantAgent != nullptr)) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Failed to write wantAgent status");
        return false;
    }
    if (wantAgent != nullptr && !parcel.WriteParcelable(&(*wantAgent))) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Failed to write wantAgent");
        return false;
    }
    return true;
}

SimpleTimerInfo *SimpleTimerInfo::Unmarshalling(Parcel& parcel)
{
    auto name = parcel.ReadString();
    auto type = parcel.ReadInt32();
    auto repeat = parcel.ReadBool();
    auto disposable = parcel.ReadBool();
    auto autoRestore = parcel.ReadBool();
    auto interval = parcel.ReadUint64();
    std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent> wantAgent{ nullptr };
    if (parcel.ReadBool()) {
        wantAgent = std::shared_ptr<OHOS::AbilityRuntime::WantAgent::WantAgent>(
                parcel.ReadParcelable<OHOS::AbilityRuntime::WantAgent::WantAgent>());
        if (!wantAgent) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "Input wantagent nullptr");
            return nullptr;
        }
    }
    SimpleTimerInfo *simpleTimerInfo = new (std::nothrow) SimpleTimerInfo(name, type, repeat, disposable, autoRestore,
                                                                          interval, wantAgent);
    if (simpleTimerInfo == nullptr) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "SimpleTimerInfo nullptr");
        return nullptr;
    }
    return simpleTimerInfo;
}
// LCOV_EXCL_STOP
} // namespace MiscServices
} // namespace OHOS