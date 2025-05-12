#include "Impls/Claude_Impl.h"

size_t ClaudeWriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
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

    // 将数据添加到完整响应缓冲区（用于最终验证）
    dstr->response->append(dataChunk);

    // 尝试解析 JSON 响应
    try
    {
        json jsonData = json::parse(dataChunk);

        // 从 Claude API 响应中提取内容
        if (jsonData.contains("content") && jsonData["content"].is_array() && !jsonData["content"].empty())
        {
            for (const auto& contentItem : jsonData["content"])
            {
                if (contentItem.contains("type") && contentItem["type"] == "text" &&
                    contentItem.contains("text") && contentItem["text"].is_string())
                {
                    std::string text = contentItem["text"].get<std::string>();
                    dstr->str2->append(text);
                }
            }
        }
    }
    catch (...)
    {
        // JSON 解析失败，可能是流式响应的一部分
        // 简单存储数据以备后续完整处理
        dstr->str1->append(dataChunk);
    }

    return totalSize;
}

// 构造函数 - 带系统角色提示
Claude::Claude(std::string systemrole)
{
    Logger::Init();
    if (!systemrole.empty())
        defaultJson["content"] = systemrole;
    else
        defaultJson["content"] = sys;
    defaultJson["role"] = "system";

    // 确保对话目录存在
    if (!UDirectory::Exists(ConversationPath))
    {
        UDirectory::Create(ConversationPath);
        Add("default");
    }
}

// 构造函数 - 带 Claude API 配置信息
Claude::Claude(const ClaudeAPICreateInfo& claude_data, std::string systemrole) : claude_data_(claude_data)
{
    Logger::Init();
    if (!systemrole.empty())
        defaultJson["content"] = systemrole;
    else
        defaultJson["content"] = sys;
    defaultJson["role"] = "system";

    // 确保对话目录存在
    if (!UDirectory::Exists(ConversationPath))
    {
        UDirectory::Create(ConversationPath);
        Add("default");
    }
}

// 获取历史记录
map<long long, string> Claude::GetHistory()
{
    return map<long long, string>(); // 实际实现可能需要根据需求改进
}

// 时间戳转换为可读时间
std::string Claude::Stamp2Time(long long timestamp)
{
    time_t tick = (time_t)(timestamp / 1000); // 转换时间
    tm tm;
    char s[40];
    tm = *localtime(&tick);
    strftime(s, sizeof(s), "%Y-%m-%d", &tm);
    std::string str(s);
    return str;
}

// 检查是否已保存
bool Claude::IsSaved()
{
    return LastHistory == history;
}

// 获取当前时间戳
long long Claude::getCurrentTimestamp()
{
    auto currentTime = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count();
}

// 获取指定天数前的时间戳
long long Claude::getTimestampBefore(const int daysBefore)
{
    auto currentTime = std::chrono::system_clock::now();
    auto days = std::chrono::hours(24 * daysBefore);
    auto targetTime = currentTime - days;
    return std::chrono::duration_cast<std::chrono::milliseconds>(targetTime.time_since_epoch()).count();
}

void Claude::BuildHistory(const std::vector<std::pair<std::string, std::string>>& history)
{
    this->history.clear();
    this->history.push_back(defaultJson);
    for (const auto& it : history)
    {
        json ask = defaultJson;
        ask["content"] = it.first;
        ask["role"] = it.second;
        this->history.push_back(ask);
    }
}

// 发送请求到 Claude API
std::string Claude::sendRequest(std::string data, size_t ts)
{
    try
    {
        int retry_count = 0;
        while (retry_count < 3)
        {
            // 检查是否要求强制停止
            {
                std::lock_guard<std::mutex> stopLock(forceStopMutex);
                if (forceStop)
                {
                    std::get<0>(Response[ts]) = "操作已被取消";
                    std::get<1>(Response[ts]) = true;
                    return "操作已被取消";
                }
            }

            try
            {
                LogInfo("ClaudeBot: 发送请求...");

                // 设置请求URL
                std::string url = claude_data_._endPoint;

                // 设置CURL请求
                CURL* curl;
                CURLcode res;
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, "Content-Type: application/json");
                headers = curl_slist_append(headers, ("x-api-key: " + claude_data_.apiKey).c_str());

                // Claude API 必须指定版本号
                headers = curl_slist_append(headers, ("anthropic-version: " + claude_data_.apiVersion).c_str());

                curl = curl_easy_init();
                if (curl)
                {
                    curl_easy_setopt(curl, CURLOPT_URL, (url).c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ClaudeWriteCallback);

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

                    // 处理请求被中断的情况
                    if (res == CURLE_ABORTED_BY_CALLBACK || (res == CURLE_WRITE_ERROR && forceStop))
                    {
                        LogInfo("ClaudeBot: 请求被用户取消");
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
                        LogError("ClaudeBot 错误: 请求失败，错误代码 " + std::to_string(res));
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
                        // 请求成功
                        // 释放资源
                        curl_easy_cleanup(curl);
                        curl_slist_free_all(headers);

                        // 如果响应不为空，解析完整响应
                        if (!dstr.response->empty())
                        {
                            try
                            {
                                json responseJson = json::parse(*dstr.response);

                                // 从响应中提取内容
                                if (responseJson.contains("content") && responseJson["content"].is_array() && !
                                    responseJson["content"].empty())
                                {
                                    std::string fullContent;

                                    for (const auto& contentItem : responseJson["content"])
                                    {
                                        if (contentItem.contains("type") && contentItem["type"] == "text" &&
                                            contentItem.contains("text") && contentItem["text"].is_string())
                                        {
                                            fullContent += contentItem["text"].get<std::string>();
                                        }
                                    }

                                    // 更新响应内容
                                    if (!fullContent.empty())
                                    {
                                        std::get<0>(Response[ts]) = fullContent;
                                    }
                                }
                            }
                            catch (...)
                            {
                                // JSON解析失败，使用已处理的回复
                                LogWarn("ClaudeBot: 响应解析失败，使用已处理的内容");
                            }
                        }

                        delete dstr.str1;
                        delete dstr.response;

                        // 返回已处理的响应
                        std::cout << std::get<0>(Response[ts]) << std::endl;
                        return std::get<0>(Response[ts]);
                    }
                }
            }
            catch (std::exception& e)
            {
                // 处理异常
                LogError("ClaudeBot 错误: " + std::string(e.what()));
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
        LogError("ClaudeBot 错误: 三次尝试后请求仍然失败。");
    }
    catch (exception& e)
    {
        LogError(e.what());
    };
    return "";
}

// 提交用户消息并获取响应
std::string Claude::Submit(std::string prompt, size_t timeStamp, std::string role, std::string convid, float temp,
                           float top_p, uint32_t top_k, float pres_pen, float freq_pen, bool async)
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
        lastFinalResponse = "";
        lastTimeStamp = timeStamp;
        json ask;
        ask["content"] = prompt;
        ask["role"] = role;
        convid_ = convid;
        if (Conversation.find(convid) == Conversation.end())
        {
            std::lock_guard<std::mutex> lock(historyAccessMutex);
            history.push_back(defaultJson);
            Conversation.insert({convid_, history});
        }
        history.emplace_back(ask);
        Conversation[convid_] = history;

        // 构建 Claude API 请求
        std::string data = "{\n"
            "  \"model\": \"" + claude_data_.model + "\",\n"
            "  \"max_tokens\": 4096,\n"
            "\"temperature\":" + std::to_string(temp) + ",\n"
            "\"top_k\":" + std::to_string(top_k) + ",\n"
            "\"top_p\":" + std::to_string(top_p) + ",\n"
            "\"presence_penalty\":" + std::to_string(pres_pen) + "\n"
            "\"frequency_penalty\":" + std::to_string(freq_pen) + "\n"
            "  \"messages\": " +
            Conversation[convid_].dump()
            + "}\n";

        Response[timeStamp] = std::make_tuple("", false);
        std::string res = sendRequest(data, timeStamp);

        // 再次检查是否要求强制停止
        {
            std::lock_guard<std::mutex> stopLock(forceStopMutex);
            if (forceStop && !res.empty() && res != "操作已被取消")
            {
                // 如果生成过程中被取消但已有部分结果
                json response;
                response["content"] = res;
                response["role"] = "assistant";
                history.emplace_back(response);
                std::get<1>(Response[timeStamp]) = true;
                LogInfo("ClaudeBot: 生成被取消，但保留了部分结果");
                return res;
            }
        }

        // 如果没有被取消，正常处理结果
        if (!res.empty() && res != "操作已被取消")
        {
            json response;
            response["content"] = res;
            response["role"] = "assistant";
            history.emplace_back(response);
            std::get<1>(Response[timeStamp]) = true;
            LogInfo("ClaudeBot: 请求完成");
        }
        if (async)
            while (!std::get<0>(Response[timeStamp]).empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                //等待被处理完成
            }
        json response;
        response["content"] = lastFinalResponse;
        response["role"] = "assistant";
        history.emplace_back(response);
        std::get<1>(Response[timeStamp]) = true;
        return lastFinalResponse;
    }
    catch (exception& e)
    {
        LogError(e.what());
        std::get<0>(Response[timeStamp]) = "发生错误: " + std::string(e.what());
        std::get<1>(Response[timeStamp]) = true;
        return std::get<0>(Response[timeStamp]);
    }
}

// 重置当前对话
void Claude::Reset()
{
    history.clear();
    history.push_back(defaultJson);
    Conversation[convid_] = history;
    Del(convid_);
    Save(convid_);
}

// 加载指定对话
void Claude::Load(std::string name)
{
    std::lock_guard<std::mutex> lock(fileAccessMutex);
    std::stringstream buffer;
    std::ifstream session_file(ConversationPath + name + suffix);
    if (session_file.is_open())
    {
        buffer << session_file.rdbuf();
        history = json::parse(buffer.str());
        convid_ = name;
        Conversation[name] = history;
    }
    else
    {
        LogError("ClaudeBot 错误: 无法加载会话 " + name + "。");
    }
    LogInfo("ClaudeBot: 加载 {0} 成功", name);
}

// 保存当前对话
void Claude::Save(std::string name)
{
    std::ofstream session_file(ConversationPath + name + suffix);

    if (session_file.is_open())
    {
        session_file << history.dump();
        session_file.close();
        LogInfo("ClaudeBot: 保存 {0} 成功", name);
    }
    else
    {
        LogError("ClaudeBot 错误: 无法保存会话 {0},{1}", name, "。");
    }
}

// 删除指定对话
void Claude::Del(std::string name)
{
    if (remove((ConversationPath + name + suffix).c_str()) != 0)
    {
        LogError("ClaudeBot 错误: 无法删除会话 {0},{1}", name, "。");
    }
    LogInfo("ClaudeBot: 删除 {0} 成功", name);
}

// 添加新对话
void Claude::Add(std::string name)
{
    history.clear();
    history.emplace_back(defaultJson);
    Save(name);
}

std::string ClaudeInSlack::Submit(std::string text, size_t timeStamp, std::string role, std::string convid, float temp,
                                  float top_p, uint32_t top_k, float pres_pen, float freq_pen, bool async)
{
    try
    {
        std::lock_guard<std::mutex> lock(historyAccessMutex);
        Response[timeStamp] = std::make_tuple("", false);
        lastFinalResponse = "";
        lastTimeStamp = timeStamp;

        // 初始化curl
        CURL* curl = curl_easy_init();
        if (!curl)
        {
            LogError("CURL初始化失败");
            std::get<0>(Response[timeStamp]) = "CURL初始化失败";
            std::get<1>(Response[timeStamp]) = true;
            return "CURL初始化失败";
        }

        // 构建POST数据
        std::string postFields = std::string("token=") + curl_easy_escape(curl, claudeData.slackToken.c_str(), 0) +
            std::string("&channel=") + curl_easy_escape(curl, claudeData.channelID.c_str(), 0) +
            std::string("&text=") + curl_easy_escape(curl, text.c_str(), 0);

        // 设置URL和POST数据
        curl_easy_setopt(curl, CURLOPT_URL, "https://slack.com/api/chat.postMessage");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());

        // 设置接收响应的回调函数和数据
        std::string responseData;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                         [](char* contents, size_t size, size_t nmemb, std::string* userp) -> size_t
                         {
                             userp->append(contents, size * nmemb);
                             return size * nmemb;
                         });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

        // 创建一个线程来执行请求，以支持异步和中断
        std::thread requestThread([curl, &responseData, timeStamp, this]()
        {
            // 执行请求
            CURLcode res = CURLE_OK;

            // 分段执行，允许中断
            while (true)
            {
                // 检查是否请求停止
                {
                    std::lock_guard<std::mutex> stopLock(forceStopMutex);
                    if (forceStop)
                    {
                        std::get<0>(Response[timeStamp]) = "操作已被取消";
                        std::get<1>(Response[timeStamp]) = true;
                        curl_easy_cleanup(curl);
                        return;
                    }
                }

                // 执行请求，超时设置为100ms
                curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 100);
                res = curl_easy_perform(curl);

                // 如果请求完成或出错，就退出循环
                if (res != CURLE_OPERATION_TIMEDOUT)
                {
                    break;
                }
            }

            // 处理请求结果
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            if (res == CURLE_OK && httpCode == 200)
            {
                try
                {
                    json response = json::parse(responseData);
                    // 处理响应数据
                }
                catch (const std::exception& e)
                {
                    std::get<0>(Response[timeStamp]) = "解析响应失败: " + std::string(e.what());
                }
            }
            else
            {
                std::string errorMsg = "请求失败: ";
                if (res != CURLE_OK)
                {
                    errorMsg += curl_easy_strerror(res);
                }
                else
                {
                    errorMsg += "HTTP错误 " + std::to_string(httpCode);
                }
                LogError(errorMsg);
                std::get<0>(Response[timeStamp]) = errorMsg;
            }

            // 清理curl
            curl_easy_cleanup(curl);
        });

        // 分离线程，让它在背景运行
        requestThread.detach();
    }
    catch (std::exception& e)
    {
        LogError(e.what());
        std::get<0>(Response[timeStamp]) = "发生错误: " + std::string(e.what());
    }

    if (async)
        while (!std::get<0>(Response[timeStamp]).empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            // 等待被处理完成
        }
    else
    {
        lastFinalResponse = GetResponse(timeStamp);
    }
    std::get<1>(Response[timeStamp]) = true;
    return lastFinalResponse;
}

void ClaudeInSlack::Reset()
{
    /*
    // curl版本的实现
    CURL* curl = curl_easy_init();
    if (curl) {
        std::string postFields = "token=" + curl_easy_escape(curl, claudeData.slackToken.c_str(), 0) +
                              "&channel=" + curl_easy_escape(curl, claudeData.channelID.c_str(), 0) +
                              "&command=/reset";

        // 设置URL和请求参数
        std::string url = "https://" + claudeData.userName + ".slack.com/api/chat.command";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());

        // 设置header
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Cookie: " + claudeData.cookies).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // 接收响应的回调
        std::string responseData;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](char* contents, size_t size, size_t nmemb, std::string* userp) -> size_t {
            userp->append(contents, size * nmemb);
            return size * nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

        // 执行请求
        CURLcode res = curl_easy_perform(curl);

        // 清理
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            LogError(curl_easy_strerror(res));
        }
    }
    */
    Submit("请忘记上面的会话内容", Logger::getCurrentTimestamp());
    LogInfo("Claude : 重置成功");
}

void ClaudeInSlack::Load(std::string name)
{
    LogInfo("Claude : 不支持的操作");
}

void ClaudeInSlack::Save(std::string name)
{
    LogInfo("Claude : 不支持的操作");
}

void ClaudeInSlack::Del(std::string id)
{
    LogInfo("Claude : 不支持的操作");
}

void ClaudeInSlack::Add(std::string name)
{
    LogInfo("Claude : 不支持的操作");
}

map<long long, string> ClaudeInSlack::GetHistory()
{
    try
    {
        History.clear();
        auto _ts = to_string(Logger::getCurrentTimestamp());

        // 初始化curl
        CURL* curl = curl_easy_init();
        if (!curl)
        {
            LogError("CURL初始化失败");
            return History;
        }

        // 构建POST数据
        std::string postFields = std::string("channel=") + curl_easy_escape(curl, claudeData.channelID.c_str(), 0) +
            std::string("&latest=") + curl_easy_escape(curl, _ts.c_str(), 0) +
            std::string("&token=") + curl_easy_escape(curl, claudeData.slackToken.c_str(), 0);

        // 设置URL和请求参数
        curl_easy_setopt(curl, CURLOPT_URL, "https://slack.com/api/conversations.history");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());

        // 接收响应的回调
        std::string responseData;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                         [](char* contents, size_t size, size_t nmemb, std::string* userp) -> size_t
                         {
                             userp->append(contents, size * nmemb);
                             return size * nmemb;
                         });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

        // 执行请求
        CURLcode res = curl_easy_perform(curl);

        // 清理curl
        curl_easy_cleanup(curl);

        if (res == CURLE_OK)
        {
            json j = json::parse(responseData);
            if (j["ok"].get<bool>())
            {
                for (auto& message : j["messages"])
                {
                    if (message["bot_profile"]["name"] == "Claude")
                    {
                        long long ts = (long long)(atof(message["ts"].get<string>().c_str()) * 1000);
                        std::string text = message["blocks"][0]["elements"][0]["elements"][0]["text"].get<string>();

                        History[ts] = text;
                    }
                }
            }
        }
        else
        {
            LogError("获取历史记录失败: " + std::string(curl_easy_strerror(res)));
        }
    }
    catch (const std::exception& e)
    {
        // 捕获并处理异常
        LogError("获取历史记录失败:" + string(e.what()));
    }
    return History;
}
