#include "Impls/CustomRule_Impl.h"
static ResponseRole ROLE;
static vector<string> RESPONSE_PATH;
static const string MD = "${MODEL}";
static const string API_KEY = "${API_KEY}";
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

std::vector<std::string> split(const std::string& str, char delimiter)
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
        if (!current->contains(path[i]))
        {
            (*current)[path[i]] = json::object();
        }
        current = &(*current)[path[i]];
    }

    (*current)[path.back()] = value;
}

void JsonPathBuilder::addPath(const string& pathStr, const string& value)
{
    vector<string> pathParts = split(pathStr, '/');
    addValueAtPath(rootJson, pathParts, value);
}

json JsonPathBuilder::getJson()
{
    return rootJson;
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
                    headers = curl_slist_append(headers, (key + ": " + value).c_str());
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


CustomRule_Impl::CustomRule_Impl(const CustomRule& data, std::string systemrole): CustomRuleData(data)
{
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
        js[paths2.front()].back() = CustomRuleData.roles["system"];
        js[paths.front()].back() = systemrole;
        SystemPrompt.push_back(js);
    }
    else
    {
        auto js = templateJson;
        js[paths2.front()].back() = CustomRuleData.roles["assistant"];
        js[paths.front()].back() = "Yes I am here to help you.";
        auto js2 = templateJson;
        js2[paths2.front()].back() = CustomRuleData.roles["user"];
        js2[paths.front()].back() = systemrole;
        SystemPrompt.push_back(js2);
        SystemPrompt.push_back(js);
    }
    if (!UDirectory::Exists(ConversationPath))
    {
        UDirectory::Create(ConversationPath);
        Add("default");
    }
}

std::string CustomRule_Impl::Submit(std::string prompt, size_t timeStamp, std::string role, std::string convid,
                                    bool async)
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
        json ask = templateJson;
        ask[paths.front()].back() = prompt;
        ask[paths2.front()].back() = CustomRuleData.roles[role];
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
        std::string data = "{\n";
        for (auto& param : CustomRuleData.params)
        {
            if (param.content == MD)
                param.content = CustomRuleData.model;
            if (param.isStr)
                data += "\"" + param.suffix + "\":\"" + param.content + "\",\n";
            else
                data += "\"" + param.suffix + "\":" + param.content + ",\n";
        }
        data += "\"" + CustomRuleData.promptRole.prompt.suffix + "\":" + Conversation[convid_].dump() + "\n}";
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
