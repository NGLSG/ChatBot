#include "Impls/CustomRule_Impl.h"

#include <regex>

#include "Configure.h"
static ResponseRole ROLE;
static vector<string> RESPONSE_PATH;
static const string MD = "${MODEL}";
static const string API_KEY = "${API_KEY}";

std::string replaceAll(std::string str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Move past the replacement
    }
    return str;
}

void resolveChainedVariables(std::vector<CustomVariable>& vars)
{
    if (vars.empty())
    {
        return;
    }

    int max_iterations = vars.size() * vars.size(); // Heuristic limit to prevent infinite loops
    int current_iteration = 0;
    bool changed_in_pass;

    do
    {
        changed_in_pass = false;
        current_iteration++;

        for (auto& target_var : vars)
        {
            std::string original_value = target_var.value; // Keep original for this pass's sources

            // Regex to find placeholders like ${VAR_NAME}
            // Using regex to correctly capture variable names, especially if they can have underscores, numbers etc.
            // This regex finds ${ followed by one or more word characters (alphanumeric + underscore), followed by }
            std::regex placeholder_regex("\\$\\{([\\w_]+)\\}");
            std::smatch match;
            std::string temp_value = target_var.value; // Work on a temporary copy for multiple replacements in one var

            // Iteratively replace placeholders in the current target_var.value
            // This inner loop handles multiple placeholders within a single variable's value string
            std::string current_processing_value = target_var.value;
            std::string last_iteration_value;

            // This inner loop is to ensure all placeholders in a single string are processed
            // e.g. VarC = ${VarA}${VarB}
            do
            {
                last_iteration_value = current_processing_value;
                std::string search_value = current_processing_value; // string to search placeholders in
                std::string new_value_for_target = ""; // build the new value incrementally
                size_t last_pos = 0;

                while (std::regex_search(search_value, match, placeholder_regex))
                {
                    std::string placeholder = match[0].str(); // e.g., "${Var1}"
                    std::string var_name_to_find = match[1].str(); // e.g., "Var1"

                    new_value_for_target += match.prefix().str(); // Add text before the placeholder

                    bool found_source = false;
                    for (const auto& source_var : vars)
                    {
                        if (source_var.name == var_name_to_find)
                        {
                            // Important: Only use source_var.value if it's already resolved
                            // OR if it's a literal. For simplicity in this iterative approach,
                            // we use its current value. The outer loop will eventually resolve it.
                            // However, to avoid direct self-reference in a non-productive way,
                            // ensure source_var is not target_var or that its value is different.
                            if (&source_var != &target_var || source_var.value != original_value)
                            {
                                new_value_for_target += source_var.value;
                                found_source = true;
                            }
                            break;
                        }
                    }
                    if (!found_source)
                    {
                        new_value_for_target += placeholder; // Keep placeholder if source not found
                    }

                    search_value = match.suffix().str();
                    last_pos += match.prefix().length() + placeholder.length();
                }
                new_value_for_target += search_value; // Add remaining part of the string
                current_processing_value = new_value_for_target;

                if (current_processing_value != last_iteration_value)
                {
                    if (target_var.value != current_processing_value)
                    {
                        target_var.value = current_processing_value;
                        changed_in_pass = true;
                    }
                }
                else
                {
                    // No change in this inner iteration, break
                    break;
                }
                // Continue inner loop if the value string changed, ensuring chained substitutions within one string resolve
            }
            while (true);
        } // end for each target_var

        if (current_iteration > max_iterations)
        {
            // Optional: Log a warning if max_iterations is reached,
            // indicating potential circular dependencies or unresolved variables.
            // std::cerr << "Warning: Variable resolution reached max iterations. Possible circular dependency." << std::endl;
            // for(const auto& var : vars) {
            //     if (var.value.find("${") != std::string::npos) {
            //         std::cerr << "Unresolved: " << var.name << " = " << var.value << std::endl;
            //     }
            // }
            break;
        }
    }
    while (changed_in_pass);
}

void replaceVariable(const std::string& varName, std::string& text, const std::string& value)
{
    std::string var = "${" + varName + "}";
    auto p = text.find(var);
    if (p != std::string::npos)
    {
        text.replace(p, var.length(), value);
    }
}

// 从 JSON 中按照指定路径提取内容
std::string ExtractContentFromJson(const json& jsonData, const std::vector<std::string>& path)
{
    // 使用指针遍历路径
    const json* current = &jsonData;

    // 逐级遍历路径
    for (size_t i = 0; i < path.size(); i++)
    {
        const auto& pathItem = path[i];

        // 检查当前节点是否为数组
        if (current->is_array())
        {
            // 检查路径项是否为数字索引
            bool isIndex = true;
            size_t index = 0;
            try
            {
                index = std::stoul(pathItem);
            }
            catch (...)
            {
                isIndex = false;
            }

            if (isIndex)
            {
                // 使用指定的索引访问数组
                if (index < current->size())
                {
                    current = &(*current)[index];
                }
                else
                {
                    // 索引越界
                    return "";
                }
            }
            else
            {
                // 不是索引，默认使用数组第一个元素，然后处理当前路径项
                if (current->size() > 0)
                {
                    current = &(*current)[0];

                    // 继续处理当前路径项（不要跳过）
                    i--; // 回退一步，下次循环仍处理当前路径项
                }
                else
                {
                    // 空数组
                    return "";
                }
            }
        }
        else if (current->is_object() && current->contains(pathItem) && !(*current)[pathItem].is_null())
        {
            // 对象属性访问
            current = &(*current)[pathItem];
        }
        else
        {
            // 路径无效
            return "";
        }
    }

    // 可能最后一级也是数组，需要再次检查并取第一个元素
    if (current->is_array() && current->size() > 0)
    {
        current = &(*current)[0];
    }

    // 获取最终节点的值
    if (current->is_string())
    {
        // 如果是字符串，直接返回
        return current->get<std::string>();
    }
    else
    {
        // 非字符串类型，返回空
        return "";
    }
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    // 基本参数转换
    DString* dstr = static_cast<DString*>(userp);
    size_t totalSize = size * nmemb;
    std::string dataChunk(static_cast<char*>(contents), totalSize);
    ChatBot* chatBot = static_cast<ChatBot*>(dstr->instance);

    // 检查是否需要停止请求
    if (chatBot && chatBot->forceStop.load())
    {
        return 0; // 返回0会导致libcurl中断请求
    }

    // 将数据添加到完整响应缓冲区
    dstr->response->append(dataChunk);

    // 临时存储接收的数据
    {
        std::lock_guard<std::mutex> lock(dstr->mtx);
        dstr->str1->append(dataChunk);
    }

    // 获取当前完整的缓冲区内容
    std::string buffer;
    {
        std::lock_guard<std::mutex> lock(dstr->mtx);
        buffer = *dstr->str1;
    }

    // 处理状态变量
    std::string processed;
    size_t processedLength = 0;

    // 根据ROLE配置进行响应处理
    if (ROLE.suffix.empty())
    {
        // 无分隔符模式：尝试解析整个缓冲区为单个JSON
        try
        {
            if (!buffer.empty())
            {
                // 检查是否包含结束标记
                if (!ROLE.stopFlag.empty() && buffer.find(ROLE.stopFlag) != std::string::npos)
                {
                    size_t flagPos = buffer.find(ROLE.stopFlag);
                    processedLength = flagPos + ROLE.stopFlag.length();
                }
                else
                {
                    // 解析JSON并使用RESPONSE_PATH提取内容
                    json jsonData = json::parse(buffer);

                    // 判断是否为数组
                    if (jsonData.is_array())
                    {
                        // 对数组中的每个元素应用路径提取
                        for (const auto& item : jsonData)
                        {
                            std::string extractedContent = ExtractContentFromJson(item, RESPONSE_PATH);
                            if (!extractedContent.empty())
                            {
                                processed += extractedContent;
                            }
                        }
                    }
                    else
                    {
                        // 单个JSON对象
                        std::string extractedContent = ExtractContentFromJson(jsonData, RESPONSE_PATH);
                        if (!extractedContent.empty())
                        {
                            processed += extractedContent;
                        }
                    }

                    // 标记缓冲区已处理
                    processedLength = buffer.length();
                }
            }
        }
        catch (json::parse_error&)
        {
            // JSON解析失败，可能数据不完整，保留缓冲区
            processedLength = 0;
        }
    }
    else
    {
        // 分隔符模式：处理流式数据
        size_t currentPos = 0;
        size_t nextPos = 0;

        while ((nextPos = buffer.find(ROLE.suffix, currentPos)) != std::string::npos)
        {
            // 找到下一个分隔符或缓冲区结尾
            size_t endPos = buffer.find(ROLE.suffix, nextPos + ROLE.suffix.length());
            if (endPos == std::string::npos)
            {
                endPos = buffer.length();
            }

            // 提取数据块
            std::string dataBlock = buffer.substr(nextPos, endPos - nextPos);

            // 检查是否是结束标记
            if (!ROLE.stopFlag.empty() && dataBlock.find(ROLE.stopFlag) != std::string::npos)
            {
                currentPos = endPos;
                processedLength = endPos;
                continue;
            }

            // 尝试解析JSON
            size_t jsonStart = dataBlock.find('{');
            if (jsonStart != std::string::npos)
            {
                std::string jsonStr = dataBlock.substr(jsonStart);

                try
                {
                    json jsonData = json::parse(jsonStr);

                    // 使用RESPONSE_PATH提取内容
                    std::string extractedContent = ExtractContentFromJson(jsonData, RESPONSE_PATH);
                    if (!extractedContent.empty())
                    {
                        processed += extractedContent;
                    }
                }
                catch (...)
                {
                    // 解析失败且是最后一个数据块，保留待下次处理
                    if (endPos == buffer.length())
                    {
                        break;
                    }
                }
            }

            // 移动到下一个位置
            currentPos = endPos;
            processedLength = endPos;
        }
    }

    // 更新未处理完的缓冲区
    {
        std::lock_guard<std::mutex> lock(dstr->mtx);
        if (processedLength > 0)
        {
            // 只保留未处理的部分
            if (processedLength < buffer.length())
            {
                *dstr->str1 = buffer.substr(processedLength);
            }
            else
            {
                dstr->str1->clear();
            }
        }

        // 将解析出的内容添加到输出
        if (!processed.empty())
        {
            dstr->str2->append(processed);
        }
    }

    return totalSize;
}

static std::vector<std::string> split(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);

    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }

    return tokens;
}

void JsonPathBuilder::addValueAtPath(json& jsonObj, const vector<string>& path, const string& value)
{
    if (path.empty()) return;

    json* current = &jsonObj;
    for (size_t i = 0; i < path.size() - 1; ++i)
    {
        const string& segment = path[i];
        bool isArrayIndex = !segment.empty() &&
            std::all_of(segment.begin(), segment.end(), [](char c) { return isdigit(c); });

        if (isArrayIndex)
        {
            size_t index = std::stoul(segment);
            if (!current->is_array())
            {
                (*current) = json::array();
            }
            while (current->size() <= index)
            {
                current->push_back(json::object());
            }

            current = &(*current)[index];
        }
        else
        {
            if (!current->contains(segment))
            {
                bool nextIsArray = (i < path.size() - 2) &&
                    !path[i + 1].empty() &&
                    std::all_of(path[i + 1].begin(), path[i + 1].end(), [](char c) { return isdigit(c); });

                (*current)[segment] = nextIsArray ? json::array() : json::object();
            }
            current = &(*current)[segment];
        }
    }

    if (!path.empty())
    {
        try
        {
            (*current)[path.back()] = json::parse(value);
        }
        catch (const json::parse_error&)
        {
            (*current)[path.back()] = value;
        }
    }
}

void JsonPathBuilder::addPath(const string& pathStr, const string& value)
{
    vector<string> pathParts = split(pathStr, '/');
    erase_if(pathParts, [](const string& s) { return s.empty(); });
    addValueAtPath(rootJson, pathParts, value);
}

json JsonPathBuilder::getJson()
{
    return rootJson;
}

CustomRule_Impl::CustomRule_Impl(const CustomRule& data, std::string systemrole): CustomRuleData(data)
{
    resolveChainedVariables(CustomRuleData.vars);
    paths = split(CustomRuleData.promptRole.prompt.path, '/');
    paths2 = split(CustomRuleData.promptRole.role.path, '/');
    RESPONSE_PATH = split(CustomRuleData.responseRole.content, '/');
    ROLE = CustomRuleData.responseRole;
    JsonPathBuilder builder;

    // 添加第一个路径
    builder.addPath(CustomRuleData.promptRole.prompt.path, "${PROMPT}");

    // 添加第二个路径
    builder.addPath(CustomRuleData.promptRole.role.path, "${ROLE}");

    templateJson = builder.getJson();


    if (CustomRuleData.supportSystemRole && !CustomRuleData.roles["system"].empty())
    {
        auto js = templateJson;
        /*js[paths2.front()].back() = CustomRuleData.roles["system"];
        js[paths.front()].back() = systemrole;*/
        js = buildRequest(systemrole, CustomRuleData.roles["system"]);

        SystemPrompt.push_back(js);
    }
    else
    {
        auto js = templateJson;
        std::string data1 = js.dump();
        /*js[paths2.front()].back() = CustomRuleData.roles["assistant"];
        js[paths.front()].back() = "Yes I am here to help you.";*/
        js = buildRequest("Yes I am here to help you.", CustomRuleData.roles["assistant"]);
        auto js2 = templateJson;
        /*js2[paths2.front()].back() = CustomRuleData.roles["user"];
        js2[paths.front()].back() = systemrole;*/
        js2 = buildRequest(systemrole, CustomRuleData.roles["user"]);
        SystemPrompt.push_back(js2);
        SystemPrompt.push_back(js);
    }
    if (!UDirectory::Exists(ConversationPath))
    {
        UDirectory::Create(ConversationPath);
        Add("default");
    }
}

std::string CustomRule_Impl::sendRequest(std::string data, size_t ts)
{
    try
    {
        int retry_count = 0;
        while (retry_count < 3)
        {
            std::string url = CustomRuleData.apiPath;
            if (url.find(MD) != std::string::npos)
                url.replace(url.find(MD), MD.size(), CustomRuleData.model);
            if (CustomRuleData.apiKeyRole.role == "URL" && url.find(API_KEY) != std::string::npos)
                url.replace(url.find(API_KEY), API_KEY.size(), CustomRuleData.apiKeyRole.key);
            for (auto& it : CustomRuleData.vars)
            {
                replaceVariable(it.name, url, it.value);
            }

            // 检查强制停止标志
            {
                std::lock_guard<std::mutex> stopLock(forceStopMutex);
                if (forceStop)
                {
                    // 设置响应状态并返回
                    std::get<0>(Response[ts]) = "操作已被取消";
                    std::get<1>(Response[ts]) = true;
                    return "操作已被取消";
                }
            }
            try
            {
                CURLcode res;
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, "Content-Type: application/json");
                for (const auto& [key, value] : CustomRuleData.headers)
                {
                    std::string v = value;
                    for (const auto& it : CustomRuleData.vars)
                    {
                        if (v.find(it.name) != std::string::npos)
                        {
                            v.replace(v.find(it.name), it.name.size(), it.value);
                        }
                    }
                    headers = curl_slist_append(headers, (key + ": " + v).c_str());
                }
                if (CustomRuleData.apiKeyRole.role == "HEADERS")
                {
                    headers = curl_slist_append(
                        headers, (CustomRuleData.apiKeyRole.header + CustomRuleData.apiKeyRole.key).c_str());
                }

                headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
                CURL* curl = curl_easy_init();
                if (curl)
                {
                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

                    // 添加进度回调，用于检查停止标志
                    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
                    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);

                    // 初始化数据结构
                    DString dstr;
                    dstr.str1 = new string(); // 用于处理缓冲区
                    dstr.str2 = &std::get<0>(Response[ts]); // 最终输出结果
                    dstr.response = new string(); // 保存完整响应
                    dstr.instance = this;
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dstr);

                    // 执行请求
                    res = curl_easy_perform(curl);
                    lastRawResponse = *dstr.response;

                    // 处理请求被中断的情况
                    if (res == CURLE_ABORTED_BY_CALLBACK || (res == CURLE_WRITE_ERROR && forceStop))
                    {
                        LogInfo("ChatBot: Request canceled by user");
                        std::get<0>(Response[ts]) = std::get<0>(Response[ts]) + "\n[生成被中断]";
                        std::get<1>(Response[ts]) = true;

                        // 释放资源
                        curl_easy_cleanup(curl);
                        curl_slist_free_all(headers);
                        delete dstr.str1;
                        delete dstr.response;

                        return std::get<0>(Response[ts]);
                    }
                    else if (res != CURLE_OK)
                    {
                        // 请求失败，准备重试
                        LogError("ChatBot Error: Request failed with error code " + std::to_string(res));
                        retry_count++;

                        // 检查是否要求强制停止
                        std::lock_guard<std::mutex> stopLock(forceStopMutex);
                        if (forceStop)
                        {
                            std::get<0>(Response[ts]) = "操作已被取消";
                            std::get<1>(Response[ts]) = true;

                            // 释放资源
                            curl_easy_cleanup(curl);
                            curl_slist_free_all(headers);
                            delete dstr.str1;
                            delete dstr.response;

                            return "操作已被取消";
                        }
                    }
                    else
                    {
                        // 释放资源
                        curl_easy_cleanup(curl);
                        curl_slist_free_all(headers);

                        delete dstr.str1;
                        delete dstr.response;

                        // 返回已在WriteCallback中处理好的响应
                        std::cout << std::get<0>(Response[ts]) << std::endl;
                        return std::get<0>(Response[ts]);
                    }
                }
            }
            catch (std::exception& e)
            {
                // 处理异常
                LogError("ChatBot Error: " + std::string(e.what()));
                retry_count++;

                // 检查是否要求强制停止
                std::lock_guard<std::mutex> stopLock(forceStopMutex);
                if (forceStop)
                {
                    std::get<0>(Response[ts]) = "操作已被取消";
                    std::get<1>(Response[ts]) = true;
                    return "操作已被取消";
                }
            }
        }
        LogError("ChatBot Error: Request failed after three retries.");
    }
    catch (exception& e)
    {
        LogError(e.what());
    };
    return "";
}

json CustomRule_Impl::buildRequest(const std::string& prompt, const std::string& role)
{
    auto js = templateJson;

    // 通用路径设置函数
    auto setValueAtPath = [](json& j, const std::vector<std::string>& path, const json& value)
    {
        if (path.empty()) return;

        json* current = &j;
        for (size_t i = 0; i < path.size() - 1; ++i)
        {
            const auto& segment = path[i];

            if (current->is_array())
            {
                // 处理数组索引
                if (segment.find_first_not_of("0123456789") == std::string::npos)
                {
                    size_t index = std::stoul(segment);
                    if (index >= current->size())
                    {
                        current->push_back(json::object());
                    }
                    current = &(*current)[index];
                }
                else
                {
                    // 非数字键，转换为对象
                    *current = json::object();
                    (*current)[segment] = json::object();
                    current = &(*current)[segment];
                }
            }
            else
            {
                // 处理对象
                if (!current->contains(segment))
                {
                    (*current)[segment] = json::object();
                }
                current = &(*current)[segment];
            }
        }

        if (!path.empty())
        {
            (*current)[path.back()] = value;
        }
    };

    // 设置role
    if (!paths2.empty())
    {
        setValueAtPath(js, paths2, role);
    }

    // 设置prompt
    if (!paths.empty())
    {
        setValueAtPath(js, paths, prompt);
    }

    return js;
}

std::string CustomRule_Impl::Submit(std::string prompt, size_t timeStamp, std::string role, std::string convid,
                                    float temp, float top_p, uint32_t top_k, float pres_pen, float freq_pen, bool async)
{
    try
    {
        // 检查是否要求强制停止
        {
            std::lock_guard<std::mutex> stopLock(forceStopMutex);
            if (forceStop)
            {
                std::get<0>(Response[timeStamp]) = "操作已被取消";
                std::get<1>(Response[timeStamp]) = true;
                return "操作已被取消";
            }
        }
        lastTimeStamp = timeStamp;
        lastFinalResponse = "";
        json ask = buildRequest(prompt, CustomRuleData.roles[role]); //templateJson;
        /*ask[paths.front()].back()[paths.back()] = prompt;
        ask[paths2.front()].back()[paths2.back()] = CustomRuleData.roles[role];*/
        convid_ = convid;
        if (Conversation.find(convid) == Conversation.end())
        {
            Conversation.insert({convid, history});
        }
        if (history.empty())
        {
            for (auto& i : SystemPrompt)
            {
                history.push_back(i);
            }
        }
        history.emplace_back(ask);
        Conversation[convid] = history;
        std::string d = Conversation[convid].dump();
        std::string data;
        JsonPathBuilder builder;

        // Handle params
        for (auto& param : CustomRuleData.params)
        {
            if (param.content == MD)
                param.content = CustomRuleData.model;
            for (auto& it : CustomRuleData.vars)
            {
                replaceVariable(it.name, param.content, it.value);
            }
            builder.addPath(param.path + "/" + param.suffix, param.content);
        }
        static const std::string VAR1 = "TOPK";
        static const std::string VAR2 = "TEMP";
        static const std::string VAR3 = "TOPP";
        static const std::string VAR4 = "PRES";
        static const std::string VAR5 = "FREQ";
        for (auto& must : CustomRuleData.extraMust)
        {
            replaceVariable(VAR1, must.content, std::to_string(top_k));
            replaceVariable(VAR2, must.content, std::to_string(temp));
            replaceVariable(VAR3, must.content, std::to_string(top_p));
            replaceVariable(VAR4, must.content, std::to_string(pres_pen));
            replaceVariable(VAR5, must.content, std::to_string(freq_pen));
            builder.addPath(must.path + "/" + must.suffix, must.content);
        }

        builder.addPath(CustomRuleData.promptRole.prompt.suffix, Conversation[convid].dump());

        json resultJson = builder.getJson();
        data += resultJson.dump();

        if (!data.empty() && data.back() == ',')
        {
            data.pop_back();
        }
        Response[timeStamp] = std::make_tuple("", false);
        std::string res = sendRequest(data, timeStamp);

        // 再次检查是否要求强制停止
        {
            std::lock_guard<std::mutex> stopLock(forceStopMutex);
            if (forceStop && !res.empty() && res != "操作已被取消")
            {
                std::get<1>(Response[timeStamp]) = true;
                LogInfo("ChatBot: Post canceled but partial result saved");
                return res;
            }
        }

        // 如果没有被取消，正常处理结果
        if (!res.empty() && res != "操作已被取消")
        {
            std::get<1>(Response[timeStamp]) = true;
            LogInfo("ChatBot: Post finished");
        }
        if (async)
            while (!std::get<0>(Response[timeStamp]).empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                //等待被处理完成
            }
        else
        {
            lastFinalResponse = GetResponse(timeStamp);
            if (lastFinalResponse.empty())
            {
                lastFinalResponse = lastRawResponse;
            }
        }
        json response = templateJson;
        response[paths.front()].back() = lastFinalResponse;
        response[paths2.front()].back() = CustomRuleData.roles["assistant"];

        history.emplace_back(response);
        std::get<1>(Response[timeStamp]) = true;
        return lastFinalResponse;
    }
    catch (exception& e)
    {
        LogError(e.what());
        std::get<0>(Response[timeStamp]) = "发生错误: " + std::string(e.what());
        std::get<1>(Response[timeStamp]) = true;
        return lastFinalResponse;
    }
}

void CustomRule_Impl::Reset()
{
    history.clear();
    Conversation[convid_] = history;
}

void CustomRule_Impl::Load(std::string name)
{
    std::lock_guard<std::mutex> lock(fileAccessMutex);
    std::stringstream buffer;
    std::ifstream session_file(ConversationPath + name + suffix);
    if (session_file.is_open())
    {
        buffer << session_file.rdbuf();
        history = json::parse(buffer.str());
    }
    else
    {
        LogError("CustomRule_Impl Error: Unable to load session " + name + ".");
    }
    LogInfo("Bot: 加载 {0} 成功", name);
}

void CustomRule_Impl::Save(std::string name)
{
    std::ofstream session_file(ConversationPath + name + suffix);

    if (session_file.is_open())
    {
        session_file << history.dump();
        session_file.close();
        LogInfo("Bot : Save {0} successfully", name);
    }
    else
    {
        LogError("CustomRule_Impl Error: Unable to save session {0},{1}", name, ".");
    }
}

void CustomRule_Impl::Del(std::string name)
{
    if (remove((ConversationPath + name + suffix).c_str()) != 0)
    {
        LogError("CustomRule_Impl Error: Unable to delete session {0},{1}", name, ".");
    }
    LogInfo("Bot : 删除 {0} 成功", name);
}

void CustomRule_Impl::Add(std::string name)
{
    history.clear();
    Save(name);
}

map<long long, string> CustomRule_Impl::GetHistory()
{
    return map<long long, string>();
}

void CustomRule_Impl::BuildHistory(const std::vector<std::pair<std::string, std::string>>& history)
{
    this->history.clear();
    for (const auto& it : history)
    {
        json js = templateJson;
        if (it.first == "system" && !CustomRuleData.supportSystemRole)
        {
            auto j1 = buildRequest(it.second, CustomRuleData.roles["user"]);
            auto j2 = buildRequest("Yes,i know that", CustomRuleData.roles["assistant"]);
            this->history.push_back(j1);
            this->history.push_back(j2);
        }
        else
        {
            this->history.push_back(buildRequest(it.second, CustomRuleData.roles[it.first]));
        }
    }
    std::string data = this->history.dump();
}
