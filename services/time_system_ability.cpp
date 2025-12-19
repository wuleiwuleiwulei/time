/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#include "time_system_ability.h"

#include <dirent.h>
#include <linux/rtc.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/timerfd.h>

#include "iservice_registry.h"
#include "ntp_update_time.h"
#include "ntp_trusted_time.h"
#include "system_ability_definition.h"
#include "time_tick_notify.h"
#include "time_zone_info.h"
#include "timer_proxy.h"
#include "time_file_utils.h"
#include "time_xcollie.h"
#include "parameters.h"
#include "event_manager.h"
#include "simple_timer_info.h"

#ifdef MULTI_ACCOUNT_ENABLE
#include "os_account.h"
#include "os_account_manager.h"
#endif

using namespace std::chrono;
using namespace OHOS::EventFwk;
using namespace OHOS::HiviewDFX;

namespace OHOS {
namespace MiscServices {
namespace {
// Unit of measure conversion , BASE: second
static constexpr int MILLI_TO_BASE = 1000LL;
static constexpr int MICR_TO_BASE = 1000000LL;
static constexpr int NANO_TO_BASE = 1000000000LL;
static constexpr std::int32_t INIT_INTERVAL = 10L;
static constexpr uint32_t TIMER_TYPE_REALTIME_MASK = 1 << 0;
static constexpr uint32_t TIMER_TYPE_REALTIME_WAKEUP_MASK = 1 << 1;
static constexpr uint32_t TIMER_TYPE_EXACT_MASK = 1 << 2;
static constexpr uint32_t TIMER_TYPE_IDLE_MASK = 1 << 3;
static constexpr uint32_t TIMER_TYPE_INEXACT_REMINDER_MASK = 1 << 4;
static constexpr int32_t STR_MAX_LENGTH = 64;
constexpr int32_t MILLI_TO_MICR = MICR_TO_BASE / MILLI_TO_BASE;
constexpr int32_t NANO_TO_MILLI = NANO_TO_BASE / MILLI_TO_BASE;
constexpr int32_t ONE_MILLI = 1000;
constexpr int AUTOTIME_OK = 0;
static const std::vector<std::string> ALL_DATA = { "timerId", "type", "flag", "windowLength", "interval", \
                                                   "uid", "bundleName", "wantAgent", "state", "triggerTime", \
                                                   "pid", "name"};
constexpr const char* BOOTEVENT_PARAMETER = "bootevent.boot.completed";
constexpr const char* AUTOTIME_KEY = "persist.time.auto_time";
static constexpr int MAX_PID_LIST_SIZE = 1024;
static constexpr uint32_t MAX_EXEMPTION_SIZE = 1000;

#ifdef MULTI_ACCOUNT_ENABLE
constexpr const char* SUBSCRIBE_REMOVED = "UserRemoved";
#endif
} // namespace

REGISTER_SYSTEM_ABILITY_BY_ID(TimeSystemAbility, TIME_SERVICE_ID, true);

std::mutex TimeSystemAbility::instanceLock_;
sptr<TimeSystemAbility> TimeSystemAbility::instance_;

#ifdef MULTI_ACCOUNT_ENABLE
class UserRemovedSubscriber : public AccountSA::OsAccountSubscriber {
public:
    explicit UserRemovedSubscriber(const AccountSA::OsAccountSubscribeInfo &subscribeInfo)
        : AccountSA::OsAccountSubscriber(subscribeInfo)
    {}

    void OnAccountsChanged(const int &id)
    {
        auto timerManager = TimerManager::GetInstance();
        if (timerManager == nullptr) {
            return;
        }
        timerManager->OnUserRemoved(id);
    }

    void OnAccountsSwitch(const int &newId, const int &oldId) {}
};
#endif

TimeSystemAbility::TimeSystemAbility(int32_t systemAbilityId, bool runOnCreate)
    : SystemAbility(systemAbilityId, runOnCreate), state_(ServiceRunningState::STATE_NOT_START),
      rtcId(GetWallClockRtcId())
{
    TIME_HILOGD(TIME_MODULE_SERVICE, " TimeSystemAbility Start");
}

TimeSystemAbility::TimeSystemAbility() : state_(ServiceRunningState::STATE_NOT_START), rtcId(GetWallClockRtcId())
{
}

TimeSystemAbility::~TimeSystemAbility(){};

sptr<TimeSystemAbility> TimeSystemAbility::GetInstance()
{
    if (instance_ == nullptr) {
        std::lock_guard<std::mutex> autoLock(instanceLock_);
        if (instance_ == nullptr) {
            instance_ = new TimeSystemAbility;
        }
    }
    return instance_;
}

#ifdef HIDUMPER_ENABLE
void TimeSystemAbility::InitDumpCmd()
{
    auto cmdTime = std::make_shared<TimeCmdParse>(std::vector<std::string>({ "-time" }),
        "dump current time info,include localtime,timezone info",
        [this](int fd, const std::vector<std::string> &input) { DumpAllTimeInfo(fd, input); });
    TimeCmdDispatcher::GetInstance().RegisterCommand(cmdTime);

    auto cmdTimerAll = std::make_shared<TimeCmdParse>(std::vector<std::string>({ "-timer", "-a" }),
        "dump all timer info", [this](int fd, const std::vector<std::string> &input) { DumpTimerInfo(fd, input); });
    TimeCmdDispatcher::GetInstance().RegisterCommand(cmdTimerAll);

    auto cmdTimerInfo = std::make_shared<TimeCmdParse>(std::vector<std::string>({ "-timer", "-i", "[n]" }),
        "dump the timer info with timer id",
        [this](int fd, const std::vector<std::string> &input) { DumpTimerInfoById(fd, input); });
    TimeCmdDispatcher::GetInstance().RegisterCommand(cmdTimerInfo);

    auto cmdTimerTrigger = std::make_shared<TimeCmdParse>(std::vector<std::string>({ "-timer", "-s", "[n]" }),
        "dump current time info,include localtime,timezone info",
        [this](int fd, const std::vector<std::string> &input) { DumpTimerTriggerById(fd, input); });
    TimeCmdDispatcher::GetInstance().RegisterCommand(cmdTimerTrigger);

    auto cmdTimerIdle = std::make_shared<TimeCmdParse>(std::vector<std::string>({ "-idle", "-a" }),
        "dump idle state and timer info, include pending delay timers and delayed info.",
        [this](int fd, const std::vector<std::string> &input) { DumpIdleTimerInfo(fd, input); });
    TimeCmdDispatcher::GetInstance().RegisterCommand(cmdTimerIdle);

    auto cmdProxyTimer = std::make_shared<TimeCmdParse>(std::vector<std::string>({ "-ProxyTimer", "-l" }),
        "dump proxy timer info.",
        [this](int fd, const std::vector<std::string> &input) { DumpProxyTimerInfo(fd, input); });
    TimeCmdDispatcher::GetInstance().RegisterCommand(cmdProxyTimer);

    auto cmdUidTimer = std::make_shared<TimeCmdParse>(std::vector<std::string>({ "-UidTimer", "-l" }),
        "dump uid timer map.",
        [this](int fd, const std::vector<std::string> &input) { DumpUidTimerMapInfo(fd, input); });
    TimeCmdDispatcher::GetInstance().RegisterCommand(cmdUidTimer);

    auto cmdShowDelayTimer = std::make_shared<TimeCmdParse>(std::vector<std::string>({ "-ProxyDelayTime", "-l" }),
        "dump proxy delay time.",
        [this](int fd, const std::vector<std::string> &input) { DumpProxyDelayTime(fd, input); });
    TimeCmdDispatcher::GetInstance().RegisterCommand(cmdShowDelayTimer);

    auto cmdAdjustTimer = std::make_shared<TimeCmdParse>(std::vector<std::string>({ "-adjust", "-a" }),
        "dump adjust time.",
        [this](int fd, const std::vector<std::string> &input) { DumpAdjustTime(fd, input); });
    TimeCmdDispatcher::GetInstance().RegisterCommand(cmdAdjustTimer);
}
#endif

void TimeSystemAbility::OnStart()
{
    TIME_HILOGI(TIME_MODULE_SERVICE, "TimeSystemAbility OnStart");
    if (state_ == ServiceRunningState::STATE_RUNNING) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "TimeSystemAbility is already running");
        return;
    }
    TimerManager::GetInstance();
    TimeTickNotify::GetInstance().Init();
    TimeZoneInfo::GetInstance().Init();
    NtpUpdateTime::GetInstance().Init();
    // This parameter is set to true by init only after all services have been started,
    // and is automatically set to false after shutdown. Otherwise it will not be modified.
    std::string bootCompleted = system::GetParameter(BOOTEVENT_PARAMETER, "");
    TIME_HILOGI(TIME_MODULE_SERVICE, "bootCompleted:%{public}s", bootCompleted.c_str());
    if (bootCompleted != "true") {
        #ifdef RDB_ENABLE
        TimeDatabase::GetInstance().ClearDropOnReboot();
        #else
        CjsonHelper::GetInstance().Clear(DROP_ON_REBOOT);
        #endif
    }
    AddSystemAbilityListener(ABILITY_MGR_SERVICE_ID);
    AddSystemAbilityListener(COMMON_EVENT_SERVICE_ID);
    AddSystemAbilityListener(DEVICE_STANDBY_SERVICE_SYSTEM_ABILITY_ID);
    #ifdef SET_AUTO_REBOOT_ENABLE
    AddSystemAbilityListener(POWER_MANAGER_SERVICE_ID);
    #endif
    AddSystemAbilityListener(COMM_NET_CONN_MANAGER_SYS_ABILITY_ID);
    #ifdef MULTI_ACCOUNT_ENABLE
    AddSystemAbilityListener(SUBSYS_ACCOUNT_SYS_ABILITY_ID_BEGIN);
    #endif
    #ifdef HIDUMPER_ENABLE
    InitDumpCmd();
    #endif
    if (Init() != ERR_OK) {
        auto callback = [this]() {
            sleep(INIT_INTERVAL);
            Init();
        };
        std::thread thread(callback);
        thread.detach();
        TIME_HILOGE(TIME_MODULE_SERVICE, "Init failed. Try again 10s later");
    }
}

void TimeSystemAbility::OnAddSystemAbility(int32_t systemAbilityId, const std::string &deviceId)
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "OnAddSystemAbility systemAbilityId:%{public}d added", systemAbilityId);
    switch (systemAbilityId) {
        case COMMON_EVENT_SERVICE_ID:
            RegisterCommonEventSubscriber();
            RemoveSystemAbilityListener(COMMON_EVENT_SERVICE_ID);
            break;
        case DEVICE_STANDBY_SERVICE_SYSTEM_ABILITY_ID:
            RegisterRSSDeathCallback();
            break;
        #ifdef SET_AUTO_REBOOT_ENABLE
        case POWER_MANAGER_SERVICE_ID:
            RegisterPowerStateListener();
            break;
        #endif
        case ABILITY_MGR_SERVICE_ID:
            AddSystemAbilityListener(BUNDLE_MGR_SERVICE_SYS_ABILITY_ID);
            RemoveSystemAbilityListener(ABILITY_MGR_SERVICE_ID);
            break;
        case BUNDLE_MGR_SERVICE_SYS_ABILITY_ID:
            RecoverTimer();
            RemoveSystemAbilityListener(BUNDLE_MGR_SERVICE_SYS_ABILITY_ID);
            break;
        #ifdef MULTI_ACCOUNT_ENABLE
        case SUBSYS_ACCOUNT_SYS_ABILITY_ID_BEGIN:
            RegisterOsAccountSubscriber();
            break;
        #endif
        default:
            TIME_HILOGE(TIME_MODULE_SERVICE, "OnAddSystemAbility systemAbilityId is not valid, id is %{public}d",
                systemAbilityId);
    }
}

void TimeSystemAbility::RegisterSubscriber()
{
    MatchingSkills matchingSkills;
    matchingSkills.AddEvent(CommonEventSupport::COMMON_EVENT_CONNECTIVITY_CHANGE);
    matchingSkills.AddEvent(CommonEventSupport::COMMON_EVENT_SCREEN_ON);
    matchingSkills.AddEvent(CommonEventSupport::COMMON_EVENT_NITZ_TIME_CHANGED);
    matchingSkills.AddEvent(CommonEventSupport::COMMON_EVENT_PACKAGE_REMOVED);
    matchingSkills.AddEvent(CommonEventSupport::COMMON_EVENT_BUNDLE_REMOVED);
    matchingSkills.AddEvent(CommonEventSupport::COMMON_EVENT_PACKAGE_FULLY_REMOVED);
    CommonEventSubscribeInfo subscriberInfo(matchingSkills);
    std::shared_ptr<EventManager> subscriberPtr = std::make_shared<EventManager>(subscriberInfo);
    bool subscribeResult = CommonEventManager::SubscribeCommonEvent(subscriberPtr);
    TIME_HILOGI(TIME_MODULE_SERVICE, "Register com event res:%{public}d", subscribeResult);
}

void TimeSystemAbility::RegisterCommonEventSubscriber()
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "RegisterCommonEventSubscriber Started");
    bool subRes = TimeServiceNotify::GetInstance().RepublishEvents();
    if (!subRes) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "failed to RegisterCommonEventSubscriber");
        auto callback = [this]() {
            sleep(INIT_INTERVAL);
            TimeServiceNotify::GetInstance().RepublishEvents();
        };
        std::thread thread(callback);
        thread.detach();
    }
    RegisterSubscriber();
    NtpUpdateTime::SetSystemTime(NtpUpdateSource::REGISTER_SUBSCRIBER);
}

#ifdef MULTI_ACCOUNT_ENABLE
void TimeSystemAbility::RegisterOsAccountSubscriber()
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "RegisterOsAccountSubscriber Started");
    AccountSA::OsAccountSubscribeInfo subscribeInfo(AccountSA::OS_ACCOUNT_SUBSCRIBE_TYPE::REMOVED, SUBSCRIBE_REMOVED);
    auto userChangedSubscriber = std::make_shared<UserRemovedSubscriber>(subscribeInfo);
    int err = AccountSA::OsAccountManager::SubscribeOsAccount(userChangedSubscriber);
    if (err != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Subscribe user removed event failed, errcode:%{public}d", err);
    }
}
#endif

int32_t TimeSystemAbility::Init()
{
    bool ret = Publish(TimeSystemAbility::GetInstance());
    if (!ret) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Init Failed");
        return E_TIME_PUBLISH_FAIL;
    }
    TIME_HILOGI(TIME_MODULE_SERVICE, "Init success");
    state_ = ServiceRunningState::STATE_RUNNING;
    return ERR_OK;
}

void TimeSystemAbility::OnStop()
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "OnStop Started");
    if (state_ != ServiceRunningState::STATE_RUNNING) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "state is running");
        return;
    }
    TimeTickNotify::GetInstance().Stop();
    state_ = ServiceRunningState::STATE_NOT_START;
    TIME_HILOGI(TIME_MODULE_SERVICE, "OnStop End");
}

void TimeSystemAbility::ParseTimerPara(const std::shared_ptr<ITimerInfo> &timerOptions, TimerPara &paras)
{
    auto uIntType = static_cast<uint32_t>(timerOptions->type);
    bool isRealtime = (uIntType & TIMER_TYPE_REALTIME_MASK) > 0;
    bool isWakeup = (uIntType & TIMER_TYPE_REALTIME_WAKEUP_MASK) > 0;
    paras.windowLength = (uIntType & TIMER_TYPE_EXACT_MASK) > 0 ? 0 : -1;
    paras.flag = (uIntType & TIMER_TYPE_EXACT_MASK) > 0 ? 1 : 0;
    paras.autoRestore = timerOptions->autoRestore;
    if (isRealtime && isWakeup) {
        paras.timerType = ITimerManager::TimerType::ELAPSED_REALTIME_WAKEUP;
    } else if (isRealtime) {
        paras.timerType = ITimerManager::TimerType::ELAPSED_REALTIME;
    } else if (isWakeup) {
        paras.timerType = ITimerManager::TimerType::RTC_WAKEUP;
    } else {
        paras.timerType = ITimerManager::TimerType::RTC;
    }
    if ((uIntType & TIMER_TYPE_IDLE_MASK) > 0) {
        paras.flag |= ITimerManager::TimerFlag::IDLE_UNTIL;
    }
    if ((uIntType & TIMER_TYPE_INEXACT_REMINDER_MASK) > 0) {
        paras.flag |= ITimerManager::TimerFlag::INEXACT_REMINDER;
    }
    if (timerOptions->disposable) {
        paras.flag |= ITimerManager::TimerFlag::IS_DISPOSABLE;
    }
    if (timerOptions->name != "") {
        paras.name = timerOptions->name;
    }
    paras.interval = timerOptions->repeat ? timerOptions->interval : 0;
}

int32_t TimeSystemAbility::CheckTimerPara(const DatabaseType type, const TimerPara &paras)
{
    if (paras.autoRestore && (paras.timerType == ITimerManager::TimerType::ELAPSED_REALTIME ||
        paras.timerType == ITimerManager::TimerType::ELAPSED_REALTIME_WAKEUP || type == DatabaseType::NOT_STORE)) {
        return E_TIME_AUTO_RESTORE_ERROR;
    }
    if (paras.name.size() > STR_MAX_LENGTH) {
        return E_TIME_PARAMETERS_INVALID;
    }
    return E_TIME_OK;
}

int32_t TimeSystemAbility::CreateTimer(const std::string &name, int type, bool repeat, bool disposable,
                                       bool autoRestore, uint64_t interval,
                                       const OHOS::AbilityRuntime::WantAgent::WantAgent &wantAgent,
                                       const sptr<IRemoteObject> &timerCallback, uint64_t &timerId)
{
    TimeXCollie timeXCollie("TimeService::CreateTimer");
    if (!TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "not system applications");
        return E_TIME_NOT_SYSTEM_APP;
    }
    std::shared_ptr<SimpleTimerInfo> timerInfo = std::make_shared<SimpleTimerInfo>(name, type, repeat, disposable,
        autoRestore, interval, std::make_shared<OHOS::AbilityRuntime::WantAgent::WantAgent>(wantAgent));
    auto ret = CreateTimer(timerInfo, timerCallback, timerId);
    if (ret != E_TIME_OK) {
        return E_TIME_DEAL_FAILED;
    }
    return E_TIME_OK;
}

int32_t TimeSystemAbility::CreateTimerWithoutWA(const std::string &name, int type, bool repeat, bool disposable,
    bool autoRestore, uint64_t interval,
    const sptr<IRemoteObject> &timerCallback, uint64_t &timerId)
{
    TimeXCollie timeXCollie("TimeService::CreateTimer");
    if (!TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "not system applications");
        return E_TIME_NOT_SYSTEM_APP;
    }
    std::shared_ptr<SimpleTimerInfo> timerInfo = std::make_shared<SimpleTimerInfo>(name, type, repeat, disposable,
    autoRestore, interval, nullptr);
    auto ret = CreateTimer(timerInfo, timerCallback, timerId);
    if (ret != E_TIME_OK) {
        return E_TIME_DEAL_FAILED;
    }
    return E_TIME_OK;
}

int32_t TimeSystemAbility::CreateTimer(const std::shared_ptr<ITimerInfo> &timerOptions, const sptr<IRemoteObject> &obj,
    uint64_t &timerId)
{
    if (obj == nullptr) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Input nullptr");
        return E_TIME_NULLPTR;
    }
    sptr<ITimerCallback> timerCallback = iface_cast<ITimerCallback>(obj);
    if (timerCallback == nullptr) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "ITimerCallback nullptr");
        return E_TIME_NULLPTR;
    }
    auto type = DatabaseType::NOT_STORE;
    if (timerOptions->wantAgent != nullptr) {
        type = DatabaseType::STORE;
    }
    int uid = IPCSkeleton::GetCallingUid();
    int pid = IPCSkeleton::GetCallingPid();
    struct TimerPara paras {};
    ParseTimerPara(timerOptions, paras);
    int32_t res = CheckTimerPara(type, paras);
    if (res != E_TIME_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "check para err:%{public}d,uid:%{public}d", res, uid);
        return res;
    }
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return E_TIME_NULLPTR;
    }
    auto callbackFunc = [timerCallback](uint64_t id) -> int32_t {
        return timerCallback->NotifyTimer(id);
    };
    if ((paras.flag & ITimerManager::TimerFlag::IDLE_UNTIL) > 0 &&
        !TimePermission::CheckProxyCallingPermission()) {
        TIME_HILOGW(TIME_MODULE_SERVICE, "App not support create idle timer");
        paras.flag &= ~ITimerManager::TimerFlag::IDLE_UNTIL;
    }
    return timerManager->CreateTimer(paras, callbackFunc, timerOptions->wantAgent,
                                     uid, pid, timerId, type);
}

int32_t TimeSystemAbility::CreateTimer(TimerPara &paras, std::function<int32_t (const uint64_t)> callback,
    uint64_t &timerId)
{
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return E_TIME_NULLPTR;
    }
    return timerManager->CreateTimer(paras, std::move(callback), nullptr, 0, 0, timerId, NOT_STORE);
}

int32_t TimeSystemAbility::StartTimer(uint64_t timerId, uint64_t triggerTime)
{
    TimeXCollie timeXCollie("TimeService::StartTimer");
    if (!TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "not system applications");
        return E_TIME_NOT_SYSTEM_APP;
    }
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return E_TIME_NULLPTR;
    }
    auto ret = timerManager->StartTimer(timerId, triggerTime);
    if (ret != E_TIME_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Failed to start timer");
        return E_TIME_DEAL_FAILED;
    }
    return ret;
}

int32_t TimeSystemAbility::StopTimer(uint64_t timerId)
{
    TimeXCollie timeXCollie("TimeService::StopTimer");
    if (!TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "not system applications");
        return E_TIME_NOT_SYSTEM_APP;
    }
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return E_TIME_NULLPTR;
    }
    auto ret = timerManager->StopTimer(timerId);
    if (ret != E_TIME_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Failed to stop timer");
        return E_TIME_DEAL_FAILED;
    }
    return ret;
}

int32_t TimeSystemAbility::DestroyTimer(uint64_t timerId)
{
    TimeXCollie timeXCollie("TimeService::DestroyTimer");
    if (!TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "not system applications");
        return E_TIME_NOT_SYSTEM_APP;
    }
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return E_TIME_NULLPTR;
    }
    auto ret = timerManager->DestroyTimer(timerId);
    if (ret != E_TIME_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Failed to destory timer");
        return E_TIME_DEAL_FAILED;
    }
    return ret;
}

int32_t TimeSystemAbility::DestroyTimerAsync(uint64_t timerId)
{
    return DestroyTimer(timerId);
}

bool TimeSystemAbility::IsValidTime(int64_t time)
{
#if __SIZEOF_POINTER__ == 4
    if (time / MILLI_TO_BASE > LONG_MAX) {
        return false;
    }
#endif
    return true;
}

bool TimeSystemAbility::SetRealTime(int64_t time)
{
    if (!IsValidTime(time)) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "time is invalid:%{public}s", std::to_string(time).c_str());
        return false;
    }
    sptr<TimeSystemAbility> instance = TimeSystemAbility::GetInstance();
    int64_t beforeTime = 0;
    TimeUtils::GetWallTimeMs(beforeTime);
    int64_t bootTime = 0;
    TimeUtils::GetBootTimeMs(bootTime);
    TIME_HILOGW(TIME_MODULE_SERVICE,
        "Before Current Time:%{public}s"
        " Set time:%{public}s"
        " Difference:%{public}s"
        " uid:%{public}d pid:%{public}d ",
        std::to_string(beforeTime).c_str(), std::to_string(time).c_str(),
        std::to_string(std::abs(time - beforeTime)).c_str(),
        IPCSkeleton::GetCallingUid(), IPCSkeleton::GetCallingPid());
    if (time < 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "input param error %{public}" PRId64 "", time);
        return false;
    }
    int64_t currentTime = 0;
    if (TimeUtils::GetWallTimeMs(currentTime) != ERR_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "currentTime get failed");
        return false;
    }
    struct timeval tv {};
    tv.tv_sec = (time_t)(time / MILLI_TO_BASE);
    tv.tv_usec = (suseconds_t)((time % MILLI_TO_BASE) * MILLI_TO_MICR);
    int result = settimeofday(&tv, nullptr);
    if (result < 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "settimeofday time fail:%{public}d. error:%{public}s", result,
            strerror(errno));
        return false;
    }
    auto ret = SetRtcTime(tv.tv_sec);
    if (ret == E_TIME_SET_RTC_FAILED) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "set rtc fail:%{public}d", ret);
        return false;
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "getting currentTime to milliseconds:%{public}" PRId64 "", currentTime);
    if (currentTime < (time - ONE_MILLI) || currentTime > (time + ONE_MILLI)) {
        TimeServiceNotify::GetInstance().PublishTimeChangeEvents(currentTime);
    }
    TimeTickNotify::GetInstance().Callback();
    int64_t curtime = NtpTrustedTime::GetInstance().CurrentTimeMillis();
    TimeBehaviorReport(ReportEventCode::SET_TIME, std::to_string(beforeTime), std::to_string(time), curtime);
    return true;
}

int32_t TimeSystemAbility::SetTime(int64_t time, int8_t apiVersion)
{
    TimeXCollie timeXCollie("TimeService::SetTime");
    if (apiVersion == APIVersion::API_VERSION_9) {
        if (!TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "not system applications");
            return E_TIME_NOT_SYSTEM_APP;
        }
    }
    if (!TimePermission::CheckCallingPermission(TimePermission::setTime)) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "permission check setTime failed");
        return E_TIME_NO_PERMISSION;
    }
    return SetTimeInner(time, apiVersion);
}

int32_t TimeSystemAbility::SetTimeInner(int64_t time, int8_t apiVersion)
{
    if (!SetRealTime(time)) {
        return E_TIME_DEAL_FAILED;
    }
    return E_TIME_OK;
}

#ifdef HIDUMPER_ENABLE
int TimeSystemAbility::Dump(int fd, const std::vector<std::u16string> &args)
{
    int uid = static_cast<int>(IPCSkeleton::GetCallingUid());
    const int maxUid = 10000;
    if (uid > maxUid) {
        return E_TIME_DEAL_FAILED;
    }

    std::vector<std::string> argsStr;
    for (auto &item : args) {
        argsStr.emplace_back(Str16ToStr8(item));
    }

    TimeCmdDispatcher::GetInstance().Dispatch(fd, argsStr);
    return ERR_OK;
}

void TimeSystemAbility::DumpAllTimeInfo(int fd, const std::vector<std::string> &input)
{
    dprintf(fd, "\n - dump all time info :\n");
    struct timespec ts{};
    struct tm timestr{};
    char date_time[64];
    if (GetTimeByClockId(CLOCK_BOOTTIME, ts)) {
        auto localTime = localtime_r(&ts.tv_sec, &timestr);
        if (localTime == nullptr) {
            return;
        }
        strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", localTime);
        dprintf(fd, " * date time = %s\n", date_time);
    } else {
        dprintf(fd, " * dump date time error.\n");
    }
    dprintf(fd, " - dump the time Zone:\n");
    std::string timeZone;
    int32_t bRet = GetTimeZone(timeZone);
    if (bRet == ERR_OK) {
        dprintf(fd, " * time zone = %s\n", timeZone.c_str());
    } else {
        dprintf(fd, " * dump time zone error,is %s\n", timeZone.c_str());
    }
}

void TimeSystemAbility::DumpTimerInfo(int fd, const std::vector<std::string> &input)
{
    dprintf(fd, "\n - dump all timer info :\n");
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return;
    }
    timerManager->ShowTimerEntryMap(fd);
}

void TimeSystemAbility::DumpTimerInfoById(int fd, const std::vector<std::string> &input)
{
    dprintf(fd, "\n - dump the timer info with timer id:\n");
    int paramNumPos = 2;
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return;
    }
    timerManager->ShowTimerEntryById(fd, std::atoi(input.at(paramNumPos).c_str()));
}

void TimeSystemAbility::DumpTimerTriggerById(int fd, const std::vector<std::string> &input)
{
    dprintf(fd, "\n - dump timer trigger statics with timer id:\n");
    int paramNumPos = 2;
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return;
    }
    timerManager->ShowTimerTriggerById(fd, std::atoi(input.at(paramNumPos).c_str()));
}

void TimeSystemAbility::DumpIdleTimerInfo(int fd, const std::vector<std::string> &input)
{
    dprintf(fd, "\n - dump idle timer info :\n");
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return;
    }
    timerManager->ShowIdleTimerInfo(fd);
}

void TimeSystemAbility::DumpProxyTimerInfo(int fd, const std::vector<std::string> &input)
{
    dprintf(fd, "\n - dump proxy map:\n");
    int64_t times;
    TimeUtils::GetBootTimeNs(times);
    TimerProxy::GetInstance().ShowProxyTimerInfo(fd, times);
}

void TimeSystemAbility::DumpUidTimerMapInfo(int fd, const std::vector<std::string> &input)
{
    dprintf(fd, "\n - dump uid timer map:\n");
    int64_t times;
    TimeUtils::GetBootTimeNs(times);
    TimerProxy::GetInstance().ShowUidTimerMapInfo(fd, times);
}

void TimeSystemAbility::DumpProxyDelayTime(int fd, const std::vector<std::string> &input)
{
    dprintf(fd, "\n - dump proxy delay time:\n");
    TimerProxy::GetInstance().ShowProxyDelayTime(fd);
}

void TimeSystemAbility::DumpAdjustTime(int fd, const std::vector<std::string> &input)
{
    dprintf(fd, "\n - dump adjust timer info:\n");
    TimerProxy::GetInstance().ShowAdjustTimerInfo(fd);
}
#endif

int TimeSystemAbility::SetRtcTime(time_t sec)
{
    struct rtc_time rtc {};
    struct tm tm {};
    struct tm *gmtime_res = nullptr;
    FILE* fd = nullptr;
    int res;
    if (rtcId < 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "invalid rtc id:%{public}s:", strerror(ENODEV));
        return E_TIME_SET_RTC_FAILED;
    }
    std::stringstream strs;
    strs << "/dev/rtc" << rtcId;
    auto rtcDev = strs.str();
    TIME_HILOGI(TIME_MODULE_SERVICE, "rtc_dev :%{public}s:", rtcDev.data());
    auto rtcData = rtcDev.data();
    fd = fopen(rtcData, "r+");
    if (fd == NULL) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "open failed %{public}s:%{public}s", rtcDev.data(), strerror(errno));
        return E_TIME_SET_RTC_FAILED;
    }
    gmtime_res = gmtime_r(&sec, &tm);
    if (gmtime_res) {
        rtc.tm_sec = tm.tm_sec;
        rtc.tm_min = tm.tm_min;
        rtc.tm_hour = tm.tm_hour;
        rtc.tm_mday = tm.tm_mday;
        rtc.tm_mon = tm.tm_mon;
        rtc.tm_year = tm.tm_year;
        rtc.tm_wday = tm.tm_wday;
        rtc.tm_yday = tm.tm_yday;
        rtc.tm_isdst = tm.tm_isdst;
        int fd_int = fileno(fd);
        res = ioctl(fd_int, RTC_SET_TIME, &rtc);
        if (res < 0) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "ioctl RTC_SET_TIME failed,errno:%{public}s, res:%{public}d",
                strerror(errno), res);
        }
    } else {
        TIME_HILOGE(TIME_MODULE_SERVICE, "convert rtc time failed:%{public}s", strerror(errno));
        res = E_TIME_SET_RTC_FAILED;
    }
    int result = fclose(fd);
    if (result != 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "file close failed:%{public}d", result);
    }
    return res;
}

bool TimeSystemAbility::CheckRtc(const std::string &rtcPath, uint64_t rtcId)
{
    std::stringstream strs;
    strs << rtcPath << "/rtc" << rtcId << "/hctosys";
    auto hctosys_path = strs.str();

    std::fstream file(hctosys_path.data(), std::ios_base::in);
    if (file.is_open()) {
        return true;
    } else {
        TIME_HILOGE(TIME_MODULE_SERVICE, "failed to open %{public}s", hctosys_path.data());
        return false;
    }
}

int TimeSystemAbility::GetWallClockRtcId()
{
    std::string rtcPath = "/sys/class/rtc";

    std::unique_ptr<DIR, int (*)(DIR *)> dir(opendir(rtcPath.c_str()), closedir);
    if (!dir.get()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "failed to open %{public}s:%{public}s", rtcPath.c_str(), strerror(errno));
        return -1;
    }

    struct dirent *dirent;
    std::string s = "rtc";
    while (errno = 0, dirent = readdir(dir.get())) {
        std::string name(dirent->d_name);
        unsigned long rtcId = 0;
        auto index = name.find(s);
        if (index == std::string::npos) {
            continue;
        } else {
            auto rtcIdStr = name.substr(index + s.length());
            rtcId = std::stoul(rtcIdStr);
        }
        if (CheckRtc(rtcPath, rtcId)) {
            TIME_HILOGD(TIME_MODULE_SERVICE, "found wall clock rtc %{public}ld", rtcId);
            return rtcId;
        }
    }

    if (errno == 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "no wall clock rtc found");
    } else {
        TIME_HILOGE(TIME_MODULE_SERVICE, "failed to check rtc:%{public}s", strerror(errno));
    }
    return -1;
}

int32_t TimeSystemAbility::SetAutoTime(bool autoTime)
{
    TimeXCollie timeXCollie("TimeService::SetAutoTime");
    if (!TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "not system applications");
        return E_TIME_NOT_SYSTEM_APP;
    }
    if (!TimePermission::CheckCallingPermission(TimePermission::setTime)) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "permission check setTime failed");
        return E_TIME_NO_PERMISSION;
    }
    auto errNo = SetParameter(AUTOTIME_KEY, autoTime ? "ON" : "OFF");
    if (errNo != AUTOTIME_OK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "SetAutoTime FAIL,errNo: %{public}d", errNo);
        return E_TIME_DEAL_FAILED;
    } else {
        return E_TIME_OK;
    }
}

int32_t TimeSystemAbility::SetTimeZone(const std::string &timeZoneId, int8_t apiVersion)
{
    TimeXCollie timeXCollie("TimeService::SetTimeZone");
    if (apiVersion == APIVersion::API_VERSION_9) {
        if (!TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "not system applications");
            return E_TIME_NOT_SYSTEM_APP;
        }
    }
    if (!TimePermission::CheckCallingPermission(TimePermission::setTimeZone)) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "permission check setTime failed");
        return E_TIME_NO_PERMISSION;
    }
    return SetTimeZoneInner(timeZoneId, apiVersion);
}

int32_t TimeSystemAbility::SetTimeZoneInner(const std::string &timeZoneId, int8_t apiVersion)
{
    if (!TimeZoneInfo::GetInstance().SetTimezone(timeZoneId)) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Set timezone failed :%{public}s", timeZoneId.c_str());
        return E_TIME_DEAL_FAILED;
    }
    int64_t currentTime = 0;
    TimeUtils::GetBootTimeMs(currentTime);
    TimeServiceNotify::GetInstance().PublishTimeZoneChangeEvents(currentTime);
    return E_TIME_OK;
}

int32_t TimeSystemAbility::GetTimeZone(std::string &timeZoneId)
{
    TimeXCollie timeXCollie("TimeService::GetTimeZone");
    if (!TimeZoneInfo::GetInstance().GetTimezone(timeZoneId)) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "get timezone failed");
        return E_TIME_DEAL_FAILED;
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "Current timezone :%{public}s", timeZoneId.c_str());
    return ERR_OK;
}

int32_t TimeSystemAbility::GetThreadTimeMs(int64_t &time)
{
    TimeXCollie timeXCollie("TimeService::GetThreadTimeMs");
    struct timespec tv {};
    clockid_t cid;
    int ret = pthread_getcpuclockid(pthread_self(), &cid);
    if (ret != E_TIME_OK) {
        return E_TIME_PARAMETERS_INVALID;
    }
    if (GetTimeByClockId(cid, tv)) {
        time = tv.tv_sec * MILLI_TO_BASE + tv.tv_nsec / NANO_TO_MILLI;
        return ERR_OK;
    }
    return E_TIME_DEAL_FAILED;
}

int32_t TimeSystemAbility::GetThreadTimeNs(int64_t &time)
{
    TimeXCollie timeXCollie("TimeService::GetThreadTimeNs");
    struct timespec tv {};
    clockid_t cid;
    int ret = pthread_getcpuclockid(pthread_self(), &cid);
    if (ret != E_TIME_OK) {
        return E_TIME_PARAMETERS_INVALID;
    }
    if (GetTimeByClockId(cid, tv)) {
        time = tv.tv_sec * NANO_TO_BASE + tv.tv_nsec;
        return ERR_OK;
    }
    return E_TIME_DEAL_FAILED;
}

bool TimeSystemAbility::GetTimeByClockId(clockid_t clockId, struct timespec &tv)
{
    if (clock_gettime(clockId, &tv) < 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Failed clock_gettime");
        return false;
    }
    return true;
}

int32_t TimeSystemAbility::AdjustTimer(bool isAdjust, uint32_t interval, uint32_t delta)
{
    TimeXCollie timeXCollie("TimeService::AdjustTimer");
    if (!TimePermission::CheckProxyCallingPermission()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Adjust Timer permission check failed");
        return E_TIME_NO_PERMISSION;
    }
    if (isAdjust && interval == 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "invalid parameter:interval");
        return E_TIME_READ_PARCEL_ERROR;
    }
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return E_TIME_NULLPTR;
    }
    if (!timerManager->AdjustTimer(isAdjust, interval, delta)) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Error adjust timer");
        return E_TIME_DEAL_FAILED;
    }
    return E_TIME_OK;
}

int32_t TimeSystemAbility::ProxyTimer(int32_t uid, const std::vector<int>& pidList, bool isProxy, bool needRetrigger)
{
    TimeXCollie timeXCollie("TimeService::TimerProxy");
    if (!TimePermission::CheckProxyCallingPermission()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "ProxyTimer permission check failed");
        return E_TIME_NO_PERMISSION;
    }
    if (pidList.size() < 0 || pidList.size() > MAX_PID_LIST_SIZE) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Error pid list size");
        return E_TIME_PARAMETERS_INVALID;
    }
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return E_TIME_NULLPTR;
    }
    std::set<int> pidSet;
    std::copy(pidList.begin(), pidList.end(), std::insert_iterator<std::set<int>>(pidSet, pidSet.begin()));
    auto ret = timerManager->ProxyTimer(uid, pidSet, isProxy, needRetrigger);
    if (!ret) {
        return E_TIME_DEAL_FAILED;
    }
    return ERR_OK;
}

int32_t TimeSystemAbility::SetTimerExemption(const std::vector<std::string> &nameArr, bool isExemption)
{
    TimeXCollie timeXCollie("TimeService::SetTimerExemption");
    if (!TimePermission::CheckProxyCallingPermission()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Set Timer Exemption permission check failed");
        return E_TIME_NO_PERMISSION;
    }
    if (nameArr.size() > MAX_EXEMPTION_SIZE) {
        return E_TIME_PARAMETERS_INVALID;
    }
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return E_TIME_NULLPTR;
    }
    std::unordered_set<std::string> nameSet;
    std::copy(nameArr.begin(), nameArr.end(), std::inserter(nameSet, nameSet.begin()));
    timerManager->SetTimerExemption(nameSet, isExemption);
    return E_TIME_OK;
}

int32_t TimeSystemAbility::SetAdjustPolicy(const std::unordered_map<std::string, uint32_t> &policyMap)
{
    TimeXCollie timeXCollie("TimeService::SetAdjustPolicy");
    if (!TimePermission::CheckProxyCallingPermission()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Set adjust policy permission check failed");
        return E_TIME_NO_PERMISSION;
    }
    if (policyMap.size() > MAX_EXEMPTION_SIZE) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Exceeded the maximum number");
        return E_TIME_PARAMETERS_INVALID;
    }
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return E_TIME_NULLPTR;
    }
    timerManager->SetAdjustPolicy(policyMap);
    return E_TIME_OK;
}

int32_t TimeSystemAbility::ResetAllProxy()
{
    if (!TimePermission::CheckProxyCallingPermission()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "ResetAllProxy permission check failed");
        return E_TIME_NO_PERMISSION;
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "ResetAllProxy service");
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return E_TIME_NULLPTR;
    }
    if (!timerManager->ResetAllProxy()) {
        return E_TIME_DEAL_FAILED;
    }
    return E_TIME_OK;
}

int32_t TimeSystemAbility::GetNtpTimeMs(int64_t &time)
{
    if (!TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "not system applications");
        return E_TIME_NOT_SYSTEM_APP;
    }
    auto ret = NtpUpdateTime::GetInstance().GetNtpTime(time);
    if (!ret) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "GetNtpTimeMs failed");
        return E_TIME_NTP_UPDATE_FAILED;
    }
    return E_TIME_OK;
}

int32_t TimeSystemAbility::GetRealTimeMs(int64_t &time)
{
    if (!TimePermission::CheckSystemUidCallingPermission(IPCSkeleton::GetCallingFullTokenID())) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "not system applications");
        return E_TIME_NOT_SYSTEM_APP;
    }
    auto ret = NtpUpdateTime::GetInstance().GetRealTime(time);
    if (!ret) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "GetRealTimeMs failed");
        return E_TIME_NTP_NOT_UPDATE;
    }
    return E_TIME_OK;
}

void TimeSystemAbility::RSSSaDeathRecipient::OnRemoteDied(const wptr<IRemoteObject> &object)
{
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return;
    }
    timerManager->HandleRSSDeath();
}

void TimeSystemAbility::RegisterRSSDeathCallback()
{
    TIME_HILOGD(TIME_MODULE_SERVICE, "register rss death callback");
    auto systemAbilityManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (systemAbilityManager == nullptr) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Getting SystemAbilityManager failed");
        return;
    }

    auto systemAbility = systemAbilityManager->GetSystemAbility(DEVICE_STANDBY_SERVICE_SYSTEM_ABILITY_ID);
    if (systemAbility == nullptr) {
        TIME_HILOGE(TIME_MODULE_CLIENT, "Get SystemAbility failed");
        return;
    }

    if (deathRecipient_ == nullptr) {
        deathRecipient_ = new (std::nothrow) RSSSaDeathRecipient();
        if (deathRecipient_ == nullptr) {
            return;
        }
    }

    systemAbility->AddDeathRecipient(deathRecipient_);
}

#ifdef SET_AUTO_REBOOT_ENABLE
void TimeSystemAbility::TimePowerStateListener::OnSyncShutdown()
{
    // Clears `drop_on_reboot` table.
    TIME_HILOGI(TIME_MODULE_SERVICE, "OnSyncShutdown");
    TimerManager::GetInstance()->ShutDownReschedulePowerOnTimer();
    #ifdef RDB_ENABLE
    TimeDatabase::GetInstance().ClearDropOnReboot();
    TimeDatabase::GetInstance().ClearInvaildDataInHoldOnReboot();
    #else
    CjsonHelper::GetInstance().Clear(DROP_ON_REBOOT);
    CjsonHelper::GetInstance().ClearInvaildDataInHoldOnReboot();
    #endif
}

void TimeSystemAbility::RegisterPowerStateListener()
{
    TIME_HILOGI(TIME_MODULE_CLIENT, "RegisterPowerStateListener");
    auto& powerManagerClient = OHOS::PowerMgr::ShutdownClient::GetInstance();
    sptr<OHOS::PowerMgr::ISyncShutdownCallback> syncShutdownCallback = new (std::nothrow) TimePowerStateListener();
    if (!syncShutdownCallback) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Get TimePowerStateListener failed");
        return;
    }
    powerManagerClient.RegisterShutdownCallback(syncShutdownCallback, PowerMgr::ShutdownPriority::HIGH);
    TIME_HILOGI(TIME_MODULE_CLIENT, "RegisterPowerStateListener end");
}
#endif

void TimeSystemAbility::RecoverTimerCjson(std::string tableName)
{
    cJSON* db = NULL;
    auto result = CjsonHelper::GetInstance().QueryTable(tableName, &db);
    if (result == NULL) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "%{public}s get table failed", tableName.c_str());
    } else {
        int count = cJSON_GetArraySize(result);
        TIME_HILOGI(TIME_MODULE_SERVICE, "%{public}s result rows count:%{public}d", tableName.c_str(), count);
        #ifdef RDB_ENABLE
        CjsonIntoDatabase(result, true, tableName);
        #else
        RecoverTimerInnerCjson(result, true, tableName);
        #endif
    }
    cJSON_Delete(db);
}

bool TimeSystemAbility::RecoverTimer()
{
    RecoverTimerCjson(HOLD_ON_REBOOT);
    RecoverTimerCjson(DROP_ON_REBOOT);

    #ifdef RDB_ENABLE
    auto database = TimeDatabase::GetInstance();
    OHOS::NativeRdb::RdbPredicates holdRdbPredicates(HOLD_ON_REBOOT);
    auto holdResultSet = database.Query(holdRdbPredicates, ALL_DATA);
    if (holdResultSet == nullptr || holdResultSet->GoToFirstRow() != OHOS::NativeRdb::E_OK) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "hold result set is nullptr or go to first row failed");
    } else {
        int count;
        holdResultSet->GetRowCount(count);
        TIME_HILOGI(TIME_MODULE_SERVICE, "hold result rows count:%{public}d", count);
        RecoverTimerInner(holdResultSet, true);
    }
    if (holdResultSet != nullptr) {
        holdResultSet->Close();
    }

    OHOS::NativeRdb::RdbPredicates dropRdbPredicates(DROP_ON_REBOOT);
    auto dropResultSet = database.Query(dropRdbPredicates, ALL_DATA);
    if (dropResultSet == nullptr || dropResultSet->GoToFirstRow() != OHOS::NativeRdb::E_OK) {
        TIME_HILOGI(TIME_MODULE_SERVICE, "drop result set is nullptr or go to first row failed");
    } else {
        int count;
        dropResultSet->GetRowCount(count);
        TIME_HILOGI(TIME_MODULE_SERVICE, "drop result rows count:%{public}d", count);
        RecoverTimerInner(dropResultSet, false);
    }
    if (dropResultSet != nullptr) {
        dropResultSet->Close();
    }
    #endif
    return true;
}

std::shared_ptr<TimerEntry> TimeSystemAbility::GetEntry(cJSON* obj, bool autoRestore)
{
    auto cjson = CjsonHelper::GetInstance();
    auto item = cJSON_GetObjectItem(obj, "name");
    if (!cjson.IsString(item)) return nullptr;
    auto name = item->valuestring;
    item = cJSON_GetObjectItem(obj, "timerId");
    if (!cjson.IsString(item)) return nullptr;
    int64_t tmp;
    if (!cjson.StrToI64(item->valuestring, tmp)) return nullptr;
    auto timerId = static_cast<uint64_t>(tmp);
    item = cJSON_GetObjectItem(obj, "type");
    if (!cjson.IsNumber(item)) return nullptr;
    int type = item->valueint;
    item = cJSON_GetObjectItem(obj, "windowLength");
    if (!cjson.IsNumber(item)) return nullptr;
    uint64_t windowLength = static_cast<uint64_t>(item->valueint);
    item = cJSON_GetObjectItem(obj, "interval");
    if (!cjson.IsNumber(item)) return nullptr;
    uint64_t interval = static_cast<uint64_t>(item->valueint);
    item = cJSON_GetObjectItem(obj, "flag");
    if (!cjson.IsNumber(item)) return nullptr;
    uint32_t flag = static_cast<uint32_t>(item->valueint);
    item = cJSON_GetObjectItem(obj, "wantAgent");
    if (!cjson.IsString(item)) return nullptr;
    auto wantAgent = OHOS::AbilityRuntime::WantAgent::WantAgentHelper::FromString(item->valuestring);
    item = cJSON_GetObjectItem(obj, "uid");
    if (!cjson.IsNumber(item)) return nullptr;
    int uid = item->valueint;
    item = cJSON_GetObjectItem(obj, "pid");
    if (!cjson.IsNumber(item)) return nullptr;
    int pid = item->valueint;
    item = cJSON_GetObjectItem(obj, "bundleName");
    if (!cjson.IsString(item)) return nullptr;
    auto bundleName = item->valuestring;
    return std::make_shared<TimerEntry>(TimerEntry {name, timerId, type, windowLength, interval, flag, autoRestore,
                                                    nullptr, wantAgent, uid, pid, bundleName});
}

#ifdef RDB_ENABLE
void TimeSystemAbility::CjsonIntoDatabase(cJSON* resultSet, bool autoRestore, const std::string &table)
{
    int size = cJSON_GetArraySize(resultSet);
    for (int i = 0; i < size; ++i) {
        // Get data row
        cJSON* obj = cJSON_GetArrayItem(resultSet, i);
        auto timerInfo = GetEntry(obj, autoRestore);
        if (timerInfo == nullptr) {
            continue;
        }
        if (timerInfo->wantAgent == nullptr) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "wantAgent is nullptr, uid=%{public}d, id=%{public}" PRId64 "",
                timerInfo->uid, timerInfo->id);
            continue;
        }
        auto item = cJSON_GetObjectItem(obj, "state");
        if (!CjsonHelper::GetInstance().IsNumber(item)) {
            continue;
        };
        auto state = item->valueint;
        item = cJSON_GetObjectItem(obj, "triggerTime");
        if (!CjsonHelper::GetInstance().IsString(item)) {
            continue;
        }
        int64_t triggerTime;
        if (!CjsonHelper::GetInstance().StrToI64(item->valuestring, triggerTime)) {
            continue;
        }
        OHOS::NativeRdb::ValuesBucket insertValues;
        insertValues.PutLong("timerId", timerInfo->id);
        insertValues.PutInt("type", timerInfo->type);
        insertValues.PutInt("flag", timerInfo->flag);
        insertValues.PutLong("windowLength", timerInfo->windowLength);
        insertValues.PutLong("interval", timerInfo->interval);
        insertValues.PutInt("uid", timerInfo->uid);
        insertValues.PutString("bundleName", timerInfo->bundleName);
        insertValues.PutString("wantAgent",
            OHOS::AbilityRuntime::WantAgent::WantAgentHelper::ToString(timerInfo->wantAgent));
        insertValues.PutInt("state", state);
        insertValues.PutLong("triggerTime", triggerTime);
        insertValues.PutInt("pid", timerInfo->pid);
        insertValues.PutString("name", timerInfo->name);
        TimeDatabase::GetInstance().Insert(table, insertValues);
    }
    CjsonHelper::GetInstance().Clear(std::string(table));
}

void TimeSystemAbility::RecoverTimerInner(std::shared_ptr<OHOS::NativeRdb::ResultSet> resultSet, bool autoRestore)
{
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return;
    }
    do {
        auto timerId = static_cast<uint64_t>(GetLong(resultSet, 0));
        auto timerInfo = std::make_shared<TimerEntry>(TimerEntry {
            // line 11 is 'name'
            GetString(resultSet, 11),
            // Line 0 is 'timerId'
            timerId,
            // Line 1 is 'type'
            GetInt(resultSet, 1),
            // Line 3 is 'windowLength'
            static_cast<uint64_t>(GetLong(resultSet, 3)),
            // Line 4 is 'interval'
            static_cast<uint64_t>(GetLong(resultSet, 4)),
            // Line 2 is 'flag'
            GetInt(resultSet, 2),
            // autoRestore depends on the table type
            autoRestore,
            // Callback can't recover.
            nullptr,
            // Line 7 is 'wantAgent'
            OHOS::AbilityRuntime::WantAgent::WantAgentHelper::FromString(GetString(resultSet, 7)),
            // Line 5 is 'uid'
            GetInt(resultSet, 5),
            // Line 10 is 'pid'
            GetInt(resultSet, 10),
            // Line 6 is 'bundleName'
            GetString(resultSet, 6)
        });
        if (timerInfo->wantAgent == nullptr) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "wantAgent is nullptr, uid=%{public}d, id=%{public}" PRId64 "",
                timerInfo->uid, timerInfo->id);
            continue;
        }
        timerManager->ReCreateTimer(timerId, timerInfo);
        // Line 8 is 'state'
        auto state = static_cast<uint8_t>(GetInt(resultSet, 8));
        if (state == 1) {
            // Line 9 is 'triggerTime'
            auto triggerTime = static_cast<uint64_t>(GetLong(resultSet, 9));
            timerManager->StartTimer(timerId, triggerTime);
        }
    } while (resultSet->GoToNextRow() == OHOS::NativeRdb::E_OK);
}

#else
void TimeSystemAbility::RecoverTimerInnerCjson(cJSON* resultSet, bool autoRestore, std::string tableName)
{
    auto timerManager = TimerManager::GetInstance();
    if (timerManager == nullptr) {
        return;
    }
    std::vector<std::pair<uint64_t, uint64_t>> timerVec;
    int size = cJSON_GetArraySize(resultSet);
    for (int i = 0; i < size; ++i) {
        // Get data row
        cJSON* obj = cJSON_GetArrayItem(resultSet, i);
        auto timerInfo = GetEntry(obj, autoRestore);
        if (timerInfo == nullptr) {
            return;
        }
        if (timerInfo->wantAgent == nullptr) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "wantAgent is nullptr, uid=%{public}d, id=%{public}" PRId64 "",
                timerInfo->uid, timerInfo->id);
            continue;
        }
        timerManager->ReCreateTimer(timerInfo->id, timerInfo);
        // 'state'
        auto item = cJSON_GetObjectItem(obj, "state");
        if (!CjsonHelper::GetInstance().IsNumber(item)) {
            continue;
        }
        auto state = static_cast<uint8_t>(item->valueint);
        if (state == 1) {
            // 'triggerTime'
            item = cJSON_GetObjectItem(obj, "triggerTime");
            if (!CjsonHelper::GetInstance().IsString(item)) {
                continue;
            }
            int64_t triggerTime;
            if (!CjsonHelper::GetInstance().StrToI64(item->valuestring, triggerTime)) {
                continue;
            }
            timerVec.push_back(std::make_pair(timerInfo->id, static_cast<uint64_t>(triggerTime)));
        }
    }
    timerManager->StartTimerGroup(timerVec, tableName);
}
#endif
} // namespace MiscServices
} // namespace OHOS