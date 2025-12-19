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

#include "time_cmd_parse.h"

TimeCmdParse::TimeCmdParse(
    const std::vector<std::string> &argsFormat, const std::string &strHelp, const TimeCmdParse::Action &action)
    : format(argsFormat), help(strHelp), action(action)
{
}
std::string TimeCmdParse::ShowHelp()
{
    return help;
}

void TimeCmdParse::DoAction(int fd, const std::vector<std::string> &input)
{
    action(fd, input);
}

std::string TimeCmdParse::GetOption()
{
    std::string formatTitle;
    if (format.size() == 0) {
        return formatTitle;
    }
    for (unsigned long i = 0; i < format.size() - 1; i++) {
        formatTitle += format.at(i);
        formatTitle += " ";
    }
    if (formatTitle.length() == 0) {
        return formatTitle;
    }
    return formatTitle.substr(0, formatTitle.length() - 1);
}

std::string TimeCmdParse::GetFormat()
{
    std::string formatStr;
    for (auto &seg : format) {
        formatStr += seg;
        formatStr += " ";
    }
    return formatStr;
}