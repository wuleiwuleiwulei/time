/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

#include <charconv>

#include "cjson_helper.h"
#include "timer_manager_interface.h"

namespace OHOS {
namespace MiscServices {
namespace {
constexpr const char* DB_PATH = "/data/service/el1/public/database/time/time.json";
constexpr size_t INDEX_TWO = 2;
}
std::mutex CjsonHelper::mutex_;

CjsonHelper &CjsonHelper::GetInstance()
{
    static CjsonHelper cjsonHelper;
    return cjsonHelper;
}

CjsonHelper::CjsonHelper()
{
    std::ifstream file(DB_PATH);
    if (file.is_open()) {
        file.close();
        return;
    }

    std::ofstream newFile(DB_PATH);
    if (!newFile.is_open()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Create new file fail");
        return;
    }
    cJSON* root = cJSON_CreateObject();
    cJSON* holdTable = cJSON_CreateArray();
    cJSON* dropTable = cJSON_CreateArray();
    cJSON_AddItemToObject(root, HOLD_ON_REBOOT, holdTable);
    cJSON_AddItemToObject(root, DROP_ON_REBOOT, dropTable);
    char* jsonString = cJSON_Print(root);
    if (jsonString != nullptr) {
        newFile << jsonString;
        cJSON_free(jsonString);
    }
    cJSON_Delete(root);
    newFile.close();
}

bool CjsonHelper::LoadAndParseJsonFile(const std::string& tableName, cJSON* &db, cJSON* &table)
{
    db = nullptr;
    table = nullptr;
    std::ifstream file(DB_PATH);
    if (!file.good()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "open json file fail!");
        return false;
    }
    std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    db = cJSON_Parse(fileContent.c_str());
    table = cJSON_GetObjectItem(db, tableName.c_str());
    if (table == NULL) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "%{public}s get fail!", tableName.c_str());
        cJSON_Delete(db);
        db = nullptr;
        return false;
    }
    return true;
}

std::string CjsonHelper::QueryWant(std::string tableName, uint64_t timerId)
{
    std::string data = "";
    cJSON* db = NULL;
    cJSON* table = QueryTable(tableName, &db);
    if (table == NULL) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "QueryTable fail!");
        cJSON_Delete(db);
        return data;
    }

    int size = cJSON_GetArraySize(table);
    for (int i = 0; i < size; ++i) {
        cJSON* obj = cJSON_GetArrayItem(table, i);

        auto item = cJSON_GetObjectItem(obj, "timerId");
        if (!IsString(item)) {
            continue;
        }
        if (item->valuestring == std::to_string(timerId)) {
            item = cJSON_GetObjectItem(obj, "wantAgent");
            if (IsString(item)) {
                data = item->valuestring;
            }
            break;
        }
    }
    cJSON_Delete(db);
    return data;
}

// must cJSON_Delete(db) after use, avoid memory leak.
cJSON* CjsonHelper::QueryTable(std::string tableName, cJSON** db)
{
    cJSON* data = NULL;
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(DB_PATH);
    if (!file.good()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "open json file fail!");
        return data;
    }
    std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    *db = cJSON_Parse(fileContent.c_str());
    cJSON* table = cJSON_GetObjectItem(*db, tableName.c_str());
    if (table == NULL) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "%{public}s get fail!", tableName.c_str());
        return data;
    }

    data = table;
    return data;
}

bool CjsonHelper::StrToI64(std::string str, int64_t& value)
{
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    return ec == std::errc{} && ptr == str.data() + str.size();
}

bool Compare(const std::tuple<std::string, std::string, int64_t>& a,
             const std::tuple<std::string, std::string, int64_t>& b)
{
    return std::get<INDEX_TWO>(a) < std::get<INDEX_TWO>(b);
}

std::vector<std::tuple<std::string, std::string, int64_t>> CjsonHelper::QueryAutoReboot()
{
    std::vector<std::tuple<std::string, std::string, int64_t>> result;
    cJSON* db = NULL;
    cJSON* table = QueryTable(HOLD_ON_REBOOT, &db);
    if (table == NULL) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "QueryTable fail!");
        cJSON_Delete(db);
        return result;
    }
    int size = cJSON_GetArraySize(table);
    for (int i = 0; i < size; ++i) {
        cJSON* obj = cJSON_GetArrayItem(table, i);

        auto state = cJSON_GetObjectItem(obj, "state");
        if (!IsNumber(state)) {
            continue;
        }
        if (state->valueint == 1) {
            auto item = cJSON_GetObjectItem(obj, "bundleName");
            if (!IsString(item)) {
                continue;
            }
            std::string bundleName = item->valuestring;

            item = cJSON_GetObjectItem(obj, "name");
            if (!IsString(item)) {
                continue;
            }
            std::string name = item->valuestring;

            item = cJSON_GetObjectItem(obj, "triggerTime");
            if (!IsString(item)) {
                continue;
            }
            int64_t triggerTime;
            if (!StrToI64(item->valuestring, triggerTime)) {
                continue;
            }
            std::tuple<std::string, std::string, int64_t> tuple(bundleName, name, triggerTime);
            result.push_back(tuple);
        }
    }
    cJSON_Delete(db);
    std::sort(result.begin(), result.end(), Compare);
    return result;
}

bool CjsonHelper::Insert(std::string tableName, std::shared_ptr<TimerEntry> timerInfo)
{
    cJSON* newLine = cJSON_CreateObject();

    cJSON_AddStringToObject(newLine, "name", timerInfo->name.c_str());
    cJSON_AddStringToObject(newLine, "timerId", std::to_string(timerInfo->id).c_str());
    cJSON_AddNumberToObject(newLine, "type", timerInfo->type);
    cJSON_AddNumberToObject(newLine, "flag", timerInfo->flag);
    cJSON_AddNumberToObject(newLine, "windowLength", timerInfo->windowLength);
    cJSON_AddNumberToObject(newLine, "interval", timerInfo->interval);
    cJSON_AddNumberToObject(newLine, "uid", timerInfo->uid);
    cJSON_AddNumberToObject(newLine, "pid", timerInfo->pid);
    cJSON_AddStringToObject(newLine, "bundleName", timerInfo->bundleName.c_str());
    cJSON_AddStringToObject(newLine, "wantAgent",
        OHOS::AbilityRuntime::WantAgent::WantAgentHelper::ToString(timerInfo->wantAgent).c_str());
    cJSON_AddNumberToObject(newLine, "state", 0);
    cJSON_AddStringToObject(newLine, "triggerTime", "0");

    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(DB_PATH);
    if (!file.good()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "open json file fail!");
        cJSON_Delete(newLine);
        return false;
    }
    std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    cJSON* db = cJSON_Parse(fileContent.c_str());
    cJSON* table = cJSON_GetObjectItem(db, tableName.c_str());
    if (table == NULL) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "%{public}s get fail!", tableName.c_str());
        cJSON_Delete(db);
        cJSON_Delete(newLine);
        return false;
    }
    cJSON_AddItemToArray(table, newLine);
    SaveJson(db);
    cJSON_Delete(db);
    return true;
}

bool CjsonHelper::UpdateTrigger(std::string tableName, int64_t timerId, int64_t triggerTime)
{
    std::lock_guard<std::mutex> lock(mutex_);

    cJSON* db = nullptr;
    cJSON* table = nullptr;
    if (!LoadAndParseJsonFile(tableName, db, table)) {
        return false;
    }

    int size = cJSON_GetArraySize(table);
    for (int i = 0; i < size; ++i) {
        cJSON* obj = cJSON_GetArrayItem(table, i);

        auto timerIdObj = cJSON_GetObjectItem(obj, "timerId");
        if (!IsString(timerIdObj)) {
            continue;
        }
        if (timerIdObj->valuestring == std::to_string(timerId)) {
            cJSON_ReplaceItemInObject(obj, "state", cJSON_CreateNumber(1));
            cJSON_ReplaceItemInObject(obj, "triggerTime", cJSON_CreateString(std::to_string(triggerTime).c_str()));
            SaveJson(db);
            break;
        }
    }
    cJSON_Delete(db);
    return true;
}

bool CjsonHelper::UpdateTriggerGroup(std::string tableName, std::vector<std::pair<uint64_t, uint64_t>> timerVec)
{
    if (timerVec.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);

    cJSON* db = nullptr;
    cJSON* table = nullptr;
    if (!LoadAndParseJsonFile(tableName, db, table)) {
        return false;
    }

    int size = cJSON_GetArraySize(table);
    for (int i = 0; i < size; ++i) {
        cJSON* obj = cJSON_GetArrayItem(table, i);

        auto timerIdObj = cJSON_GetObjectItem(obj, "timerId");
        if (!IsString(timerIdObj)) {
            continue;
        }
        for (auto it = timerVec.begin(); it != timerVec.end(); ++it) {
            std::string timerId = std::to_string(it->first);
            if (timerIdObj->valuestring == timerId) {
                cJSON_ReplaceItemInObject(obj, "state", cJSON_CreateNumber(1));
                std::string triggerTime = std::to_string(it->second);
                cJSON_ReplaceItemInObject(obj, "triggerTime", cJSON_CreateString(triggerTime.c_str()));
            }
        }
    }
    SaveJson(db);
    cJSON_Delete(db);
    return true;
}

bool CjsonHelper::UpdateState(std::string tableName, int64_t timerId)
{
    std::lock_guard<std::mutex> lock(mutex_);

    cJSON* db = nullptr;
    cJSON* table = nullptr;
    if (!LoadAndParseJsonFile(tableName, db, table)) {
        return false;
    }

    int size = cJSON_GetArraySize(table);
    for (int i = 0; i < size; ++i) {
        cJSON* obj = cJSON_GetArrayItem(table, i);

        auto timerIdObj = cJSON_GetObjectItem(obj, "timerId");
        auto stateObj = cJSON_GetObjectItem(obj, "state");
        if (!IsString(timerIdObj) || !IsNumber(stateObj)) {
            continue;
        }
        if (timerIdObj->valuestring == std::to_string(timerId) && stateObj->valueint == 1) {
            cJSON_ReplaceItemInObject(obj, "state", cJSON_CreateNumber(0));
            SaveJson(db);
            break;
        }
    }
    cJSON_Delete(db);
    return true;
}

bool CjsonHelper::Delete(std::string tableName, int64_t timerId)
{
    std::lock_guard<std::mutex> lock(mutex_);

    cJSON* db = nullptr;
    cJSON* table = nullptr;
    if (!LoadAndParseJsonFile(tableName, db, table)) {
        return false;
    }

    int size = cJSON_GetArraySize(table);
    for (int i = 0; i < size; ++i) {
        cJSON* obj = cJSON_GetArrayItem(table, i);

        auto timerIdObj = cJSON_GetObjectItem(obj, "timerId");
        if (!IsString(timerIdObj)) {
            continue;
        }
        if (timerIdObj->valuestring == std::to_string(timerId)) {
            cJSON_DeleteItemFromArray(table, i);
            SaveJson(db);
            break;
        }
    }
    cJSON_Delete(db);
    return true;
}

bool CjsonHelper::ClearInvaildDataInHoldOnReboot()
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(DB_PATH);
    if (!file.good()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "open json file fail!");
        return false;
    }
    std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    cJSON* db = cJSON_Parse(fileContent.c_str());
    cJSON* table = cJSON_GetObjectItem(db, HOLD_ON_REBOOT);
    if (table == NULL) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "HOLD_ON_REBOOT get fail!");
        cJSON_Delete(db);
        return false;
    }

    int size = cJSON_GetArraySize(table);
    for (int i = size - 1; i >= 0; --i) {
        cJSON* obj = cJSON_GetArrayItem(table, i);

        auto stateObj = cJSON_GetObjectItem(obj, "state");
        auto typeObj = cJSON_GetObjectItem(obj, "type");
        if (!IsNumber(stateObj) || !IsNumber(typeObj)) {
            continue;
        }
        if (stateObj->valueint == 0
            || typeObj->valueint == ITimerManager::ELAPSED_REALTIME_WAKEUP
            || typeObj->valueint == ITimerManager::ELAPSED_REALTIME) {
            cJSON_DeleteItemFromArray(table, i);
        }
    }
    SaveJson(db);
    cJSON_Delete(db);
    return true;
}

void CjsonHelper::Clear(std::string tableName)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(DB_PATH);
    if (!file.good()) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "open json file fail!");
        return;
    }
    std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    cJSON* db = cJSON_Parse(fileContent.c_str());
    cJSON* table = cJSON_GetObjectItem(db, tableName.c_str());
    if (table == NULL) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "%{public}s get json fail!", tableName.c_str());
        cJSON_Delete(db);
        return;
    }

    int size = cJSON_GetArraySize(table);
    for (int i = size - 1; i >= 0; --i) {
        cJSON_DeleteItemFromArray(table, i);
    }
    SaveJson(db);
    cJSON_Delete(db);
}

void CjsonHelper::SaveJson(cJSON* data)
{
    std::ofstream outFile(DB_PATH);
    char* jsonString = cJSON_Print(data);
    if (jsonString != nullptr) {
        outFile << jsonString;
        cJSON_free(jsonString);
    }
    outFile.close();
}

bool CjsonHelper::IsNumber(cJSON* item)
{
    return (item != NULL && cJSON_IsNumber(item));
}

bool CjsonHelper::IsString(cJSON* item)
{
    return (item != NULL && cJSON_IsString(item));
}
}
}
