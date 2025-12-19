/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ani.h>
#include <array>
#include <iostream>
#include <ctime>
#include "time_hilog.h"
#include "time_service_client.h"

using namespace OHOS::MiscServices;

constexpr ani_long SECS_TO_NANO = 1000000000;
constexpr ani_long SECS_TO_MILLI = 1000;
constexpr ani_long NANO_TO_MILLI = 1000000;

static ani_long GetRealTime(ani_boolean isNano)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
        TIME_HILOGE(TIME_MODULE_JS_ANI, "failed clock_gettime, errno: %{public}s", strerror(errno));
        return 0;
    }

    if (isNano) {
        return ts.tv_sec * SECS_TO_NANO + ts.tv_nsec;
    }

    return ts.tv_sec * SECS_TO_MILLI + ts.tv_nsec / NANO_TO_MILLI;
}

static ani_double GetTime([[maybe_unused]] ani_env *env, ani_object booleanObject)
{
    ani_boolean isUndefined = false;
    env->Reference_IsUndefined(booleanObject, &isUndefined);
    if (isUndefined) {
        return GetRealTime(false);
    }
    ani_boolean isNano;
    if (ANI_OK !=env->Object_CallMethodByName_Boolean(booleanObject, "unboxed", nullptr, &isNano)) {
        TIME_HILOGE(TIME_MODULE_JS_ANI, "Object_CallMethodByName_Boolean Fail");
    }
    return GetRealTime(isNano);
}

static ani_string GetTimeZone([[maybe_unused]] ani_env *env)
{
    std::string timezone;
    auto status = TimeServiceClient::GetInstance()->GetTimeZone(timezone);
    if (ANI_OK != status) {
        TIME_HILOGE(TIME_MODULE_JS_ANI, "GetTimeZone failed, errno: %{public}d", status);
    }
    ani_string tzCstr;
    env->String_NewUTF8(timezone.c_str(), timezone.size(), &tzCstr);
    return tzCstr;
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    ani_env *env;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        TIME_HILOGE(TIME_MODULE_JS_ANI, "Unsupported ANI_VERSION_1");
        return ANI_ERROR;
    }

    static const char *namespaceName = "L@ohos/systemDateTime/systemDateTime;";
    ani_namespace ns;
    if (ANI_OK != env->FindNamespace(namespaceName, &ns)) {
        TIME_HILOGE(TIME_MODULE_JS_ANI, "Not found '%{public}s'", namespaceName);
        return ANI_ERROR;
    }

    std::array methods = {
        ani_native_function {"getTime", nullptr, reinterpret_cast<void *>(GetTime)},
        ani_native_function {"getTimezoneSync", ":Lstd/core/String;", reinterpret_cast<void *>(GetTimeZone)},
    };

    if (ANI_OK != env->Namespace_BindNativeFunctions(ns, methods.data(), methods.size())) {
        TIME_HILOGE(TIME_MODULE_JS_ANI, "Cannot bind native methods to '%{public}s'", namespaceName);
        return ANI_ERROR;
    };

    *result = ANI_VERSION_1;
    return ANI_OK;
}