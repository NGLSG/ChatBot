#include "Impls/Gemini_Impl.h"

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

    // Gemini API返回的是JSON数组，每个元素是一个部分的响应
    try
    {
        // 尝试解析整个缓冲区作为一个完整的JSON数组
        auto jsonArray = json::parse(buffer);

        // 成功解析，处理每个元素
        for (const auto& item : jsonArray)
        {
            if (item.contains("candidates") && !item["candidates"].empty())
            {
                const auto& candidates = item["candidates"];

                // 提取文本内容
                if (candidates[0].contains("content") &&
                    candidates[0]["content"].contains("parts") &&
                    !candidates[0]["content"]["parts"].empty() &&
                    candidates[0]["content"]["parts"][0].contains("text"))
                {
                    std::string content = candidates[0]["content"]["parts"][0]["text"].get<std::string>();
                    processed += content;
                }
            }
        }

        // 解析成功，清空缓冲区
        processedLength = buffer.length();
    }
    catch (json::parse_error&)
    {
        // JSON解析失败，可能是数据不完整
        // 保留缓冲区内容以待下次处理
        processedLength = 0;
    }

    // 更新未处理完的缓冲区
    {
        std::lock_guard<std::mutex> lock(dstr->mtx);
        if (processedLength > 0)
        {
            // 清空已处理的内容
            dstr->str1->clear();
        }

        // 将解析出的内容添加到输出
        if (!processed.empty())
        {
            dstr->str2->append(processed);
        }
    }

    return totalSize;
}

std::string Gemini::sendRequest(std::string data, size_t ts)
{
    try
    {
        int retry_count = 0;
        while (retry_count < 3)
        {
            std::string endPoint = "";
            if (!geminiData._endPoint.empty() && geminiData._endPoint != "")
                endPoint = geminiData._endPoint;
            else
                endPoint = "https://generativelanguage.googleapis.com";
            std::string url = endPoint + "/v1beta/models/" + geminiData.model + ":streamGenerateContent?key="
                +
                geminiData._apiKey;
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
                CURL* curl;
                CURLcode res;
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, "Content-Type: application/json");
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


std::string Gemini::Submit(std::string prompt, size_t timeStamp, std::string role, std::string convid, bool async)
{
    try
    {
        std::lock_guard<std::mutex> lock(historyAccessMutex);
        convid_ = convid;
        lastFinalResponse = "";
        lastTimeStamp = timeStamp;
        Response[timeStamp] = std::make_tuple("", false);
        json ask;
        ask["role"] = role;
        ask["parts"] = json::array();
        ask["parts"].push_back(json::object());
        ask["parts"][0]["text"] = prompt;
        if (Conversation.find(convid) == Conversation.end())
        {
            Conversation.insert({convid, history});
        }
        if (history.empty())
        {
            history.push_back(SystemPrompt);
        }
        history.emplace_back(ask);
        Conversation[convid] = history;
        std::string data = "{\"contents\":" + Conversation[convid].dump() + "}";


        /*// 创建一个异步请求
        auto future = cpr::PostAsync(
            cpr::Url{url},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{data}
        );

        // 等待响应，但允许中断
        while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready)
        {
            // 每100毫秒检查一次是否要求停止
            std::lock_guard<std::mutex> stopLock(forceStopMutex);
            if (forceStop)
            {
                // 设置响应状态并返回
                std::get<0>(Response[timeStamp]) = "操作已被取消";
                std::get<1>(Response[timeStamp]) = true;
                return "操作已被取消";
            }
        }

        // 获取响应结果
        auto r = future.get();

        if (r.status_code != 200)
        {
            retry_count++;
            LogError("Gemini: {0}", r.text);

            // 检查是否应该停止重试
            std::lock_guard<std::mutex> stopLock(forceStopMutex);
            if (forceStop)
            {
                std::get<0>(Response[timeStamp]) = "操作已被取消";
                std::get<1>(Response[timeStamp]) = true;
                return "操作已被取消";
            }

            continue;
        }

        json response = json::parse(r.text);
        std::optional<std::string> res = response["candidates"][0]["content"]["parts"][0]["text"];*/

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
        }
        json ans;
        ans["role"] = "model";
        ans["parts"] = json::array();
        ans["parts"].push_back(json::object());
        ans["parts"][0]["text"] = lastFinalResponse;
        history.emplace_back(ans);
        Conversation[convid_] = history;
        std::get<0>(Response[timeStamp]) = lastFinalResponse;

        std::get<1>(Response[timeStamp]) = true;
        return lastFinalResponse;
    }
    catch (const std::exception& e)
    {
        LogError("获取历史记录失败:" + std::string(e.what()));
        std::get<0>(Response[timeStamp]) = "发生错误: " + std::string(e.what());
        std::get<1>(Response[timeStamp]) = true;
        return lastFinalResponse;
    }
}

void Gemini::Reset()
{
    history.clear();
    Conversation[convid_] = history;
}

void Gemini::Load(std::string name)
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
        LogError("Gemini Error: Unable to load session " + name + ".");
    }
    LogInfo("Bot: 加载 {0} 成功", name);
}

void Gemini::Save(std::string name)
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
        LogError("Gemini Error: Unable to save session {0},{1}", name, ".");
    }
}

void Gemini::Del(std::string name)
{
    if (remove((ConversationPath + name + suffix).c_str()) != 0)
    {
        LogError("Gemini Error: Unable to delete session {0},{1}", name, ".");
    }
    LogInfo("Bot : 删除 {0} 成功", name);
}

void Gemini::Add(std::string name)
{
    history.clear();
    Save(name);
}
