#include "Impls/ChatGPT_Impl.h"

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
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

    // 临时存储所有已接收但未处理的数据
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

    // 声明变量用于存储各种处理状态
    std::string processed;
    std::string remainingBuffer;
    size_t processedLength = 0;

    // 按照"data: "标记分割数据流
    size_t currentPos = 0;
    size_t nextPos = 0;

    while ((nextPos = buffer.find("data:", currentPos)) != std::string::npos)
    {
        // 定位此数据块的结束位置（下一个data:或缓冲区结束）
        size_t endPos = buffer.find("data:", nextPos + 5);
        if (endPos == std::string::npos)
        {
            endPos = buffer.length();
        }

        // 提取完整的数据块
        std::string dataBlock = buffer.substr(nextPos, endPos - nextPos);

        // 检查是否为结束标记
        if (dataBlock.find("[DONE]") != std::string::npos)
        {
            currentPos = endPos;
            continue;
        }

        // 尝试找到并解析JSON内容
        size_t jsonStart = dataBlock.find('{');
        if (jsonStart != std::string::npos)
        {
            std::string jsonStr = dataBlock.substr(jsonStart);

            // 尝试解析JSON
            try
            {
                json jsonData = json::parse(jsonStr);

                // 从JSON中提取文本内容
                if (jsonData.contains("choices") && !jsonData["choices"].empty())
                {
                    auto& choices = jsonData["choices"];

                    if (choices[0].contains("delta") &&
                        choices[0]["delta"].contains("content") &&
                        !choices[0]["delta"]["content"].is_null())
                    {
                        std::string content = choices[0]["delta"]["content"].get<std::string>();
                        processed += content;
                    }
                }
            }
            catch (...)
            {
                // JSON解析失败，这个数据块可能不完整
                // 如果不是最后一个数据块，则忽略并继续
                if (endPos != buffer.length())
                {
                    currentPos = endPos;
                    continue;
                }

                // 如果是最后一个数据块，保留以待下次处理
                remainingBuffer = buffer.substr(nextPos);
                break;
            }
        }

        // 移动到下一个位置
        currentPos = endPos;
        processedLength = endPos;
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

ChatGPT::ChatGPT(std::string systemrole)
{
    Logger::Init();
    if (!systemrole.empty())
        defaultJson["content"] = systemrole;
    else
        defaultJson["content"] = sys;
    defaultJson["role"] = "system";


    if (!UDirectory::Exists(ConversationPath))
    {
        UDirectory::Create(ConversationPath);
        Add("default");
    }
}

map<long long, string> ChatGPT::GetHistory()
{
    return map<long long, string>();
}

std::string ChatGPT::Stamp2Time(long long timestamp)
{
    time_t tick = (time_t)(timestamp / 1000); //转换时间
    tm tm;
    char s[40];
    tm = *localtime(&tick);
    strftime(s, sizeof(s), "%Y-%m-%d", &tm);
    std::string str(s);
    return str;
}

bool ChatGPT::IsSaved()
{
    return LastHistory == history;
}

ChatGPT::ChatGPT(const OpenAIBotCreateInfo& chat_data, std::string systemrole) : chat_data_(chat_data)
{
    Logger::Init();
    if (!systemrole.empty())
        defaultJson["content"] = systemrole;
    else
        defaultJson["content"] = sys;
    defaultJson["role"] = "system";

    if (!UDirectory::Exists(ConversationPath))
    {
        UDirectory::Create(ConversationPath);
        Add("default");
    }
}

long long ChatGPT::getCurrentTimestamp()
{
    auto currentTime = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count();
}

long long ChatGPT::getTimestampBefore(const int daysBefore)
{
    auto currentTime = std::chrono::system_clock::now();
    auto days = std::chrono::hours(24 * daysBefore);
    auto targetTime = currentTime - days;
    return std::chrono::duration_cast<std::chrono::milliseconds>(targetTime.time_since_epoch()).count();
}

std::string ChatGPT::sendRequest(std::string data, size_t ts)
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
                LogInfo("ChatBot: Post request...");
                // 确定请求URL
                std::string url = "";
                if (!chat_data_.useWebProxy)
                {
                    url = "https://api.openai.com/v1/chat/completions";
                }
                else
                {
                    url = chat_data_._endPoint;
                }

                // 设置CURL请求
                CURL* curl;
                CURLcode res;
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, "Content-Type: application/json");
                headers = curl_slist_append(headers, ("Authorization: Bearer " + chat_data_.api_key).c_str());
                headers = curl_slist_append(headers, ("api-key: " + chat_data_.api_key).c_str());
                headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
                curl = curl_easy_init();
                if (curl)
                {
                    curl_easy_setopt(curl, CURLOPT_URL, (url).c_str());
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

                    // 设置代理
                    if (!chat_data_.useWebProxy && !chat_data_.proxy.empty())
                    {
                        curl_easy_setopt(curl, CURLOPT_PROXY, chat_data_.proxy.c_str());
                    }
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // 禁用SSL验证

                    // 执行请求
                    res = curl_easy_perform(curl);

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

                        std::stringstream stream(*dstr.response);
                        std::string line;
                        std::string full_response;

                        while (std::getline(stream, line))
                        {
                            // 在处理每一行之前检查是否应该停止
                            {
                                std::lock_guard<std::mutex> stopLock(forceStopMutex);
                                if (forceStop)
                                {
                                    delete dstr.str1;
                                    delete dstr.response;
                                    std::get<0>(Response[ts]) = full_response + "\n[生成被中断]";
                                    std::get<1>(Response[ts]) = true;
                                    return std::get<0>(Response[ts]);
                                }
                            }

                            if (line.empty())
                            {
                                continue;
                            }
                            // Remove "data: "
                            line = line.substr(6);
                            if (line == "[DONE]")
                            {
                                break;
                            }
                            json resp = json::parse(line);
                            json choices = resp.value("choices", json::array());
                            if (choices.empty())
                            {
                                continue;
                            }
                            json delta = choices[0].value("delta", json::object());
                            if (delta.empty())
                            {
                                continue;
                            }
                            if (delta.count("role"))
                            {
                            }
                            if (delta.count("content"))
                            {
                                if (delta["content"].is_string())
                                {
                                    std::string content = delta["content"].get<std::string>();
                                    full_response += content;
                                }
                            }
                        }
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

std::string ChatGPT::Submit(std::string prompt, size_t timeStamp, std::string role, std::string convid)
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
        json ask;
        ask["content"] = prompt;
        ask["role"] = role;
        convid_ = convid;
        if (Conversation.find(convid) == Conversation.end())
        {
            std::lock_guard<std::mutex> lock(historyAccessMutex);
            history.push_back(defaultJson);
            //history.push_back(defaultJson2);
            Conversation.insert({convid_, history});
        }
        history.emplace_back(ask);
        Conversation[convid_] = history;
        std::string data = "{\n"
            "  \"model\": \"" + chat_data_.model + "\",\n"
            "  \"stream\": true,\n"
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
        return res;
    }
    catch (exception& e)
    {
        LogError(e.what());
        std::get<0>(Response[timeStamp]) = "发生错误: " + std::string(e.what());
        std::get<1>(Response[timeStamp]) = true;
        return std::get<0>(Response[timeStamp]);
    }
}

void ChatGPT::Reset()
{
    history.clear();
    history.push_back(defaultJson);
    Conversation[convid_] = history;
    Del(convid_);
    Save(convid_);
}

void ChatGPT::Load(std::string name)
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
        LogError("ChatBot Error: Unable to load session " + name + ".");
    }
    LogInfo("Bot: 加载 {0} 成功", name);
}

void ChatGPT::Save(std::string name)
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
        LogError("ChatBot Error: Unable to save session {0},{1}", name, ".");
    }
}

void ChatGPT::Del(std::string name)
{
    if (remove((ConversationPath + name + suffix).c_str()) != 0)
    {
        LogError("ChatBot Error: Unable to delete session {0},{1}", name, ".");
    }
    LogInfo("Bot : 删除 {0} 成功", name);
}

void ChatGPT::Add(std::string name)
{
    history.clear();
    history.emplace_back(defaultJson);
    Save(name);
}
