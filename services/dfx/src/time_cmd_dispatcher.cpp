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

#include "time_cmd_dispatcher.h"

namespace OHOS {
namespace MiscServices {
bool TimeCmdDispatcher::Dispatch(int fd, const std::vector<std::string> &args)
{
    if (args.empty() || args.at(0) == "-h") {
        dprintf(fd, "\n%-15s: %-20s", "Option", "Description");
        for (auto &[key, handler] : cmdHandler) {
            dprintf(fd, "\n%-15s: %-20s", handler->GetFormat().c_str(), handler->ShowHelp().c_str());
        }
        return false;
    }
    std::string cmdTitle = getCmdTitle(fd, args);
    auto handler = cmdHandler.find(cmdTitle);
    if (handler != cmdHandler.end()) {
        handler->second->DoAction(fd, args);
        return true;
    }
    return false;
}

void TimeCmdDispatcher::RegisterCommand(std::shared_ptr<TimeCmdParse> &cmd)
{
    cmdHandler.insert(std::make_pair(cmd->GetOption(), cmd));
}

std::string TimeCmdDispatcher::getCmdTitle(int fd, const std::vector<std::string> &args)
{
    std::string formatTitle;
    if (args.size() == 0) {
        return formatTitle;
    }
    for (unsigned long i = 0; i < args.size() - 1; i++) {
        formatTitle += args.at(i);
        formatTitle += " ";
    }
    return formatTitle.substr(0, formatTitle.length() - 1);
    dprintf(fd, "TimeCmdDispatcher::getCmdTitle formatTitle:%s.\n", formatTitle.c_str());
}

TimeCmdDispatcher &TimeCmdDispatcher::GetInstance()
{
    static TimeCmdDispatcher instance;
    return instance;
}
} // namespace MiscServices
} // namespace OHOS