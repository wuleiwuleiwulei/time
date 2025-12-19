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

#ifndef TIME_SYSEVENT_H
#define TIME_SYSEVENT_H

#include "timer_info.h"

namespace OHOS {
namespace MiscServices {
constexpr int32_t START_TIMER_OFFSET = 0x00000000;
constexpr int32_t TRIGGER_TIMER_OFFSET = 0x00000100;
constexpr int32_t TIMER_FAULT_OFFSET = 0x00000200;
constexpr int32_t MODIFY_TIME_OFFSET = 0x01000000;
constexpr int32_t EXACT_OFFSET = 4;
static const int COUNT_REPORT_ARRAY_LENGTH = 5;

enum ReportEventCode : int32_t {
    RTC_WAKEUP_EXACT_TIMER_START = START_TIMER_OFFSET,
    RTC_NONWAKEUP_EXACT_TIMER_START,
    REALTIME_WAKEUP_EXACT_TIMER_START,
    REALTIME_NONWAKEUP_EXACT_TIMER_START,
    RTC_WAKEUP_NONEXACT_TIMER_START,
    RTC_NONWAKEUP_NONEXACT_TIMER_START,
    REALTIME_WAKEUP_NONEXACT_TIMER_START,
    REALTIME_NONWAKEUP_NONEXACT_TIMER_START,
    RTC_WAKEUP_EXACT_TIMER_TRIGGER = TRIGGER_TIMER_OFFSET,
    RTC_NONWAKEUP_EXACT_TIMER_TRIGGER,
    REALTIME_WAKEUP_EXACT_TIMER_TRIGGER,
    REALTIME_NONWAKEUP_EXACT_TIMER_TRIGGER,
    RTC_WAKEUP_NONEXACT_TIMER_TRIGGER,
    RTC_NONWAKEUP_NONEXACT_TIMER_TRIGGER,
    REALTIME_WAKEUP_NONEXACT_TIMER_TRIGGER,
    REALTIME_NONWAKEUP_NONEXACT_TIMER_TRIGGER,
    TIMER_WANTAGENT_FAULT_REPORT = TIMER_FAULT_OFFSET,
    SET_TIME = MODIFY_TIME_OFFSET,
    NTP_REFRESH,
    SET_TIMEZONE,
    NTP_COMPARE_UNTRUSTED,
    NTP_VOTE_UNTRUSTED,
};
void StatisticReporter(int32_t size, std::shared_ptr<TimerInfo> timer);
void TimeBehaviorReport(ReportEventCode eventCode, const std::string &originTime, const std::string &newTime,
    int64_t ntpTime);
void TimerBehaviorReport(std::shared_ptr<TimerInfo> timer, bool isStart);
void TimerCountStaticReporter(int count, int (&uidArr)[COUNT_REPORT_ARRAY_LENGTH],
    int (&createTimerCountArr)[COUNT_REPORT_ARRAY_LENGTH], int (&startTimerCountArr)[COUNT_REPORT_ARRAY_LENGTH]);
void TimeServiceFaultReporter(ReportEventCode eventCode, int errCode, int uid, const std::string &bundleOrProcessName,
    const std::string &extraInfo);
} // namespace MiscServices
} // namespace OHOS
#endif // TIME_SYSEVENT_H
