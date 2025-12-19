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

#ifndef TIME_HILOG_WRAPPER_H
#define TIME_HILOG_WRAPPER_H

#include "hilog/log.h"
#include <cstdint>

namespace OHOS {
namespace MiscServices {
// param of log interface, such as TIME_HILOGF.
enum TimeSubModule : int8_t {
    TIME_MODULE_INNERKIT = 0,
    TIME_MODULE_CLIENT,
    TIME_MODULE_SERVICE,
    TIME_MODULE_JAVAKIT, // java kit, defined to avoid repeated use of domain.
    TIME_MODULE_JNI,
    TIME_MODULE_COMMON,
    TIME_MODULE_JS_NAPI,
    TIME_MODULE_JS_ANI,
    TIME_MODULE_BUTT,
};

static constexpr unsigned int BASE_TIME_DOMAIN_ID = 0xD001C40;

enum TimeDomainId : int32_t {
    TIME_INNERKIT_DOMAIN = BASE_TIME_DOMAIN_ID + TIME_MODULE_INNERKIT,
    TIME_CLIENT_DOMAIN,
    TIME_SERVICE_DOMAIN,
    TIME_JAVAKIT_DOMAIN,
    TIME_JNI_DOMAIN,
    TIME_COMMON_DOMAIN,
    TIME_JS_NAPI,
    TIME_JS_ANI,
    TIME_BUTT,
};

static constexpr OHOS::HiviewDFX::HiLogLabel TIME_MODULE_LABEL[TIME_MODULE_BUTT] = {
    { LOG_CORE, TIME_INNERKIT_DOMAIN, "TimeInnerKit" },
    { LOG_CORE, TIME_CLIENT_DOMAIN, "TimeClient" },
    { LOG_CORE, TIME_SERVICE_DOMAIN, "TimeService" },
    { LOG_CORE, TIME_JAVAKIT_DOMAIN, "TimeJavaKit" },
    { LOG_CORE, TIME_JNI_DOMAIN, "TimeJni" },
    { LOG_CORE, TIME_COMMON_DOMAIN, "TimeCommon" },
    { LOG_CORE, TIME_JS_NAPI, "TimeJSNAPI" },
    { LOG_CORE, TIME_JS_ANI, "TimeJSANI" },
};

#define R_FORMATED(fmt, ...) "%{public}s# " fmt, __FUNCTION__, ##__VA_ARGS__

// In order to improve performance, do not check the module range.
// Besides, make sure module is less than TIME_MODULE_BUTT.
#define TIME_HILOGF(module, fmt, ...) (void)HILOG_IMPL(LOG_CORE, LOG_FATAL, TIME_MODULE_LABEL[module].domain, \
    TIME_MODULE_LABEL[module].tag, "%{public}s# " fmt, __FUNCTION__, ##__VA_ARGS__)
#define TIME_HILOGE(module, fmt, ...) (void)HILOG_IMPL(LOG_CORE, LOG_ERROR, TIME_MODULE_LABEL[module].domain, \
    TIME_MODULE_LABEL[module].tag, "%{public}s# " fmt, __FUNCTION__, ##__VA_ARGS__)
#define TIME_HILOGW(module, fmt, ...) (void)HILOG_IMPL(LOG_CORE, LOG_WARN, TIME_MODULE_LABEL[module].domain, \
    TIME_MODULE_LABEL[module].tag, "%{public}s# " fmt, __FUNCTION__, ##__VA_ARGS__)
#define TIME_HILOGI(module, fmt, ...) (void)HILOG_IMPL(LOG_CORE, LOG_INFO, TIME_MODULE_LABEL[module].domain, \
    TIME_MODULE_LABEL[module].tag, "%{public}s# " fmt, __FUNCTION__, ##__VA_ARGS__)
#ifdef DEBUG_ENABLE
#define TIME_HILOGD(module, fmt, ...) (void)HILOG_IMPL(LOG_CORE, LOG_DEBUG, TIME_MODULE_LABEL[module].domain, \
    TIME_MODULE_LABEL[module].tag, "%{public}s# " fmt, __FUNCTION__, ##__VA_ARGS__)
#else
#define TIME_HILOGD(module, fmt, ...)
#endif

#define TIME_SIMPLIFY_HILOGI(module, fmt, ...) (void)HILOG_IMPL(LOG_CORE, LOG_INFO, TIME_MODULE_LABEL[module].domain, \
    TIME_MODULE_LABEL[module].tag, fmt, ##__VA_ARGS__)
#define TIME_SIMPLIFY_HILOGW(module, fmt, ...) (void)HILOG_IMPL(LOG_CORE, LOG_WARN, TIME_MODULE_LABEL[module].domain, \
    TIME_MODULE_LABEL[module].tag, fmt, ##__VA_ARGS__)
#define TIME_SIMPLIFY_HILOGE(module, fmt, ...) (void)HILOG_IMPL(LOG_CORE, LOG_ERROR, TIME_MODULE_LABEL[module].domain, \
    TIME_MODULE_LABEL[module].tag, fmt, ##__VA_ARGS__)

#define CHECK_AND_RETURN_RET_LOG(module, cond, ret, ...)  \
    do {                                                  \
        if (!(cond)) {                                    \
            TIME_HILOGE(module, R_FORMATED(__VA_ARGS__)); \
            return ret;                                   \
        }                                                 \
    } while (0)

#define CHECK_AND_RETURN_LOG(module, cond, ...)           \
    do {                                                  \
        if (!(cond)) {                                    \
            TIME_HILOGE(module, R_FORMATED(__VA_ARGS__)); \
            return;                                       \
        }                                                 \
    } while (0)

} // namespace MiscServices
} // namespace OHOS
#endif // TIME_HILOG_WRAPPER_H