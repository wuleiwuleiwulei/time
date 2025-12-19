/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd.
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

#include "time_sysevent.h"

#include <cinttypes>

#include "hisysevent.h"
#include "time_file_utils.h"
#include "ipc_skeleton.h"

namespace OHOS {
namespace MiscServices {
namespace {
using HiSysEventNameSpace = OHOS::HiviewDFX::HiSysEvent;
} // namespace

std::string GetBundleOrProcessName()
{
    std::string bundleOrProcessName = TimeFileUtils::GetBundleNameByTokenID(IPCSkeleton::GetCallingTokenID());
    if (bundleOrProcessName.empty()) {
        bundleOrProcessName = TimeFileUtils::GetNameByPid(IPCSkeleton::GetCallingPid());
    }
    return bundleOrProcessName;
}

void StatisticReporter(int32_t size, std::shared_ptr<TimerInfo> timer)
{
    if (timer == nullptr) {
        return;
    }
    int32_t callerUid = timer->uid;
    int32_t callerPid = timer->pid;
    std::string bundleOrProcessName = timer->bundleName;
    std::string timerName = timer->name;
    int32_t type = timer->type;
    int64_t triggerTime = timer->whenElapsed.time_since_epoch().count();
    auto interval = static_cast<uint64_t>(timer->repeatInterval.count());
    struct HiSysEventParam params[] = {
        {"CALLER_PID",             HISYSEVENT_INT32,  {.i32 = callerPid},                                    0},
        {"CALLER_UID",             HISYSEVENT_INT32,  {.i32 = callerUid},                                    0},
        {"BUNDLE_OR_PROCESS_NAME", HISYSEVENT_STRING, {.s = const_cast<char*>(bundleOrProcessName.c_str())}, 0},
        {"TIMER_NAME",             HISYSEVENT_STRING, {.s = const_cast<char*>(timerName.c_str())},           0},
        {"TIMER_SIZE",             HISYSEVENT_INT32,  {.i32 = size},                                         0},
        {"TIMER_TYPE",             HISYSEVENT_INT32,  {.i32 = type},                                         0},
        {"TRIGGER_TIME",           HISYSEVENT_INT64,  {.i64 = triggerTime},                                  0},
        {"INTERVAL",               HISYSEVENT_UINT64, {.ui64 = interval},                                    0}
    };
    int ret = OH_HiSysEvent_Write("TIME", "MISC_TIME_STATISTIC_REPORT", HISYSEVENT_STATISTIC, params,
        sizeof(params)/sizeof(params[0]));
    if (ret != 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE,
            "hisysevent Statistic failed! pid %{public}d,uid %{public}d,timer type %{public}d", callerPid, callerUid,
            type);
    }
}

void TimeBehaviorReport(ReportEventCode eventCode, const std::string &originTime, const std::string &newTime,
    int64_t ntpTime)
{
    std::string bundleOrProcessName = GetBundleOrProcessName();
    struct HiSysEventParam params[] = {
        {"EVENT_CODE",    HISYSEVENT_INT32,  {.i32 = eventCode},                                    0},
        {"CALLER_UID",    HISYSEVENT_INT32,  {.i32 = IPCSkeleton::GetCallingUid()},                 0},
        {"CALLER_NAME",   HISYSEVENT_STRING, {.s = const_cast<char*>(bundleOrProcessName.c_str())}, 0},
        {"ORIGINAL_TIME", HISYSEVENT_STRING, {.s = const_cast<char*>(originTime.c_str())},          0},
        {"SET_TIME",      HISYSEVENT_STRING, {.s = const_cast<char*>(newTime.c_str())},             0},
        {"NTP_TIME",      HISYSEVENT_INT64,  {.i64 = ntpTime},                                      0}
    };
    int ret = OH_HiSysEvent_Write("TIME", "BEHAVIOR_TIME", HISYSEVENT_BEHAVIOR, params,
        sizeof(params)/sizeof(params[0]));
    if (ret != 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "TimeBehaviorReport failed! eventCode %{public}d, name:%{public}s,"
            "ret:%{public}d", eventCode, bundleOrProcessName.c_str(), ret);
    }
}

void TimerBehaviorReport(std::shared_ptr<TimerInfo> timer, bool isStart)
{
    if (timer == nullptr) {
        return;
    }
    int triggerOffset = isStart ? RTC_WAKEUP_EXACT_TIMER_START : RTC_WAKEUP_EXACT_TIMER_TRIGGER;
    int exactOffset = (timer->windowLength == std::chrono::milliseconds::zero()) ? 0 : EXACT_OFFSET;
    ReportEventCode eventCode = static_cast<ReportEventCode>(triggerOffset + timer->type + exactOffset);
    auto bundleOrProcessName = timer->bundleName;
    auto interval = static_cast<uint32_t>(timer->repeatInterval.count());
    struct HiSysEventParam params[] = {
        {"EVENT_CODE",   HISYSEVENT_INT32,  {.i32 = eventCode},                                    0},
        {"TIMER_ID",     HISYSEVENT_UINT32, {.ui32 = timer->id},                                   0},
        {"TRIGGER_TIME", HISYSEVENT_INT64,  {.i64 = timer->when.count()},                         0},
        {"CALLER_UID",   HISYSEVENT_INT32,  {.i32 = timer->uid},                                   0},
        {"CALLER_NAME",  HISYSEVENT_STRING, {.s = const_cast<char*>(bundleOrProcessName.c_str())}, 0},
        {"INTERVAL",     HISYSEVENT_UINT32, {.ui32 =interval},                                     0}
    };
    int ret = OH_HiSysEvent_Write("TIME", "BEHAVIOR_TIMER", HISYSEVENT_BEHAVIOR, params,
        sizeof(params)/sizeof(params[0]));
    if (ret != 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "TimerBehaviorReport failed! eventCode:%{public}d,"
            "id:%{public}" PRIu64 " name:%{public}s", static_cast<int32_t>(eventCode), timer->id,
            bundleOrProcessName.c_str());
    }
}

void TimerCountStaticReporter(int count, int (&uidArr)[COUNT_REPORT_ARRAY_LENGTH],
    int (&createTimerCountArr)[COUNT_REPORT_ARRAY_LENGTH], int (&startTimerCountArr)[COUNT_REPORT_ARRAY_LENGTH])
{
    struct HiSysEventParam params[] = {
        {"TIMER_NUM",     HISYSEVENT_INT32,       {.i32 = count},                 0},
        {"TOP_UID",       HISYSEVENT_INT32_ARRAY, {.array = uidArr},              COUNT_REPORT_ARRAY_LENGTH},
        {"TOP_NUM",       HISYSEVENT_INT32_ARRAY, {.array = createTimerCountArr}, COUNT_REPORT_ARRAY_LENGTH},
        {"TOP_STRAT_NUM", HISYSEVENT_INT32_ARRAY, {.array = startTimerCountArr},  COUNT_REPORT_ARRAY_LENGTH}
    };
    int ret = OH_HiSysEvent_Write("TIME", "ALARM_COUNT", HISYSEVENT_STATISTIC,
        params, sizeof(params)/sizeof(params[0]));
    if (ret != 0) {
        std::string uidStr = "";
        std::string createCountStr = "";
        std::string startCountStr = "";
        for (int i = 0; i < COUNT_REPORT_ARRAY_LENGTH; i++) {
            uidStr = uidStr + std::to_string(uidArr[i]) + " ";
            createCountStr = createCountStr + std::to_string(createTimerCountArr[i]) + " ";
            startCountStr = startCountStr + std::to_string(startTimerCountArr[i]) + " ";
        }
        TIME_HILOGE(TIME_MODULE_SERVICE, "TimerCountStaticReporter failed! count:%{public}d, uid:[%{public}s],"
            "create count:[%{public}s], startcount:[%{public}s]", count, uidStr.c_str(), createCountStr.c_str(),
            startCountStr.c_str());
    }
}

void TimeServiceFaultReporter(ReportEventCode eventCode, int errCode, int uid, const std::string &bundleOrProcessName,
    const std::string &extraInfo)
{
    struct HiSysEventParam params[] = {
        {"EVENT_CODE",  HISYSEVENT_INT32,  {.i32 = eventCode},                                    0},
        {"ERR_CODE",    HISYSEVENT_INT32,  {.i32 = errCode},                                      0},
        {"CALLER_UID",  HISYSEVENT_INT32,  {.i32 = uid},                                          0},
        {"CALLER_NAME", HISYSEVENT_STRING, {.s = const_cast<char*>(bundleOrProcessName.c_str())}, 0},
        {"EXTRA",       HISYSEVENT_STRING, {.s = const_cast<char*>(extraInfo.c_str())},           0}
    };
    int ret = OH_HiSysEvent_Write("TIME", "FUNC_FAULT", HISYSEVENT_FAULT,
        params, sizeof(params)/sizeof(params[0]));
    if (ret != 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "TimeServiceFaultReporter failed! eventCode:%{public}d errorcode:%{public}d"
            "callname:%{public}s", eventCode, errCode, bundleOrProcessName.c_str());
    }
}
} // namespace MiscServices
} // namespace OHOS