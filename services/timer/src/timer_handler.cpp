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

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sstream>
#include "timer_handler.h"
#include "timer_manager_interface.h"

namespace OHOS {
namespace MiscServices {
namespace {
static constexpr uint32_t ALARM_TIME_CHANGE_MASK = 1 << 16;
constexpr int CLOCK_POWEROFF_ALARM = 12;
static constexpr clockid_t alarm_to_clock_id[N_TIMER_FDS] = {
    CLOCK_REALTIME_ALARM,
    CLOCK_REALTIME,
    CLOCK_BOOTTIME_ALARM,
    CLOCK_BOOTTIME,
    CLOCK_MONOTONIC,
    CLOCK_REALTIME,
    #ifdef SET_AUTO_REBOOT_ENABLE
    CLOCK_POWEROFF_ALARM,
    #endif
};
}

std::shared_ptr<TimerHandler> TimerHandler::Create()
{
    TimerFds fds;
    int epollfd = epoll_create(fds.size());
    if (epollfd < 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "epoll_create %{public}d failed:%{public}s",
            static_cast<int>(fds.size()), strerror(errno));
        return nullptr;
    }
    std::string fdStr = "";
    std::string typStr = "";
    for (size_t i = 0; i < fds.size(); i++) {
        if (alarm_to_clock_id[i] != CLOCK_POWEROFF_ALARM) {
            fds[i] = timerfd_create(alarm_to_clock_id[i], 0);
        } else {
            fds[i] = timerfd_create(CLOCK_POWEROFF_ALARM, TFD_NONBLOCK);
        }
        if (fds[i] < 0) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "timerfd_create %{public}d  failed:%{public}s",
                static_cast<int>(i), strerror(errno));
            close(epollfd);
            for (size_t j = 0; j < i; j++) {
                close(fds[j]);
            }
            return nullptr;
        }
        fdStr += std::to_string(fds[i]) + " ";
        typStr += std::to_string(i) + " ";
    }
    TIME_HILOGW(TIME_MODULE_SERVICE, "create fd:[%{public}s], typ:[%{public}s]", fdStr.c_str(), typStr.c_str());
    std::shared_ptr<TimerHandler> handler = std::shared_ptr<TimerHandler>(
        new (std::nothrow)TimerHandler(fds, epollfd));
    #ifdef SET_AUTO_REBOOT_ENABLE
    for (size_t i = 0; i < fds.size() - 1; i++) {
    #else
    for (size_t i = 0; i < fds.size(); i++) {
    #endif
        epoll_event event {};
        event.events = EPOLLIN | EPOLLWAKEUP;
        event.data.u32 = i;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fds[i], &event) < 0) {
            TIME_HILOGE(TIME_MODULE_SERVICE, "epoll_ctl(EPOLL_CTL_ADD) failed:%{public}s", strerror(errno));
            return nullptr;
        }
    }
    if (SetRealTimeFd(fds) < 0 && errno != ECANCELED) {
        return nullptr;
    }
    return handler;
}

int TimerHandler::SetRealTimeFd(TimerFds fds)
{
    itimerspec spec {};
    #ifdef SET_AUTO_REBOOT_ENABLE
    int err = timerfd_settime(fds[ALARM_TYPE_COUNT - 1], TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &spec, nullptr);
    if (err) {
        TIME_HILOGW(TIME_MODULE_SERVICE, "settime fd:%{public}d, res:%{public}d, errno:%{public}s",
            fds[ALARM_TYPE_COUNT - 1], err, strerror(errno));
    }
    #else
    int err = timerfd_settime(fds[ALARM_TYPE_COUNT], TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &spec, nullptr);
    if (err) {
        TIME_HILOGW(TIME_MODULE_SERVICE, "settime fd:%{public}d, res:%{public}d, errno:%{public}s",
            fds[ALARM_TYPE_COUNT], err, strerror(errno));
    }
    #endif
    return err;
}

TimerHandler::TimerHandler(const TimerFds &fds, int epollfd)
    : fds_ {fds}, epollFd_ {epollfd}
{
}

TimerHandler::~TimerHandler()
{
    for (auto fd : fds_) {
        epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
    }
    close(epollFd_);
}

int TimerHandler::Set(uint32_t type, std::chrono::nanoseconds when, std::chrono::steady_clock::time_point bootTime)
{
    if (static_cast<size_t>(type) > ALARM_TYPE_COUNT) {
        errno = EINVAL;
        return -1;
    }

    auto second = std::chrono::duration_cast<std::chrono::seconds>(when);
    auto milliSecond = std::chrono::duration_cast<std::chrono::milliseconds>(when);
    auto bootTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(bootTime.time_since_epoch());
    if (type == static_cast<uint32_t>(ITimerManager::TimerType::ELAPSED_REALTIME_WAKEUP)) {
        TIME_SIMPLIFY_HILOGW(TIME_MODULE_SERVICE, "t%{public}d %{public}lld bt:%{public}lld", type,
            milliSecond.count(), bootTimeMs.count());
    } else {
        TIME_SIMPLIFY_HILOGW(TIME_MODULE_SERVICE, "t%{public}d %{public}lld", type, milliSecond.count());
    }
    timespec ts {second.count(), (when - second).count()};
    itimerspec spec {timespec {}, ts};
    int ret = timerfd_settime(fds_[type], TFD_TIMER_ABSTIME, &spec, nullptr);
    if (ret != 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Set timer to kernel ret:%{public}d error:%{public}s",
                    ret, strerror(errno));
    }
    return ret;
}

uint32_t TimerHandler::WaitForAlarm()
{
    epoll_event events[N_TIMER_FDS];

    int nevents = 0;
    do {
        nevents = epoll_wait(epollFd_, events, N_TIMER_FDS, -1);
    } while (nevents < 0 && errno == EINTR);

    uint32_t result = 0;
    for (int i = 0; i < nevents; i++) {
        uint32_t alarm_idx = events[i].data.u32;
        uint64_t unused;
        ssize_t err = read(fds_[alarm_idx], &unused, sizeof(unused));
        if (err < 0) {
            if (alarm_idx == ALARM_TYPE_COUNT && errno == ECANCELED) {
                result |= ALARM_TIME_CHANGE_MASK;
            } else {
                return err;
            }
        } else {
            result |= (1 << alarm_idx);
        }
    }
    return result;
}
} // MiscServices
} // OHOS