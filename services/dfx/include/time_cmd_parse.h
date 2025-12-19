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

#ifndef TIME_CMDPARSE_H
#define TIME_CMDPARSE_H

#include <string>

class TimeCmdParse {
public:
    using Action = std::function<void(int fd, const std::vector<std::string> &input)>;
    TimeCmdParse(const std::vector<std::string> &argsFormat, const std::string &strHelp, const Action &action);
    std::string ShowHelp();
    void DoAction(int fd, const std::vector<std::string> &input);
    std::string GetOption();
    std::string GetFormat();

private:
    std::vector<std::string> format;
    std::string help;
    Action action;
};
#endif // TIME_CMDPARSE_H