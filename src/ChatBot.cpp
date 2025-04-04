#include "ChatBot.h"

#include <llama-context.h>
#include <llama-sampling.h>

//userp:<string,string>
struct DString
{
    std::string* str1;
    std::string* str2;
    std::string* response;

    void* instance;
    std::mutex mtx;
};

static inline void TrimLeading(std::string& s)
{
    size_t pos = 0;
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])))
        pos++;
    if (pos > 0)
        s.erase(0, pos);
}

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

    while ((nextPos = buffer.find("data:", currentPos)) != std::string::npos) {
        // 定位此数据块的结束位置（下一个data:或缓冲区结束）
        size_t endPos = buffer.find("data:", nextPos + 5);
        if (endPos == std::string::npos) {
            endPos = buffer.length();
        }

        // 提取完整的数据块
        std::string dataBlock = buffer.substr(nextPos, endPos - nextPos);

        // 检查是否为结束标记
        if (dataBlock.find("[DONE]") != std::string::npos) {
            currentPos = endPos;
            continue;
        }

        // 尝试找到并解析JSON内容
        size_t jsonStart = dataBlock.find('{');
        if (jsonStart != std::string::npos) {
            std::string jsonStr = dataBlock.substr(jsonStart);

            // 尝试解析JSON
            try {
                json jsonData = json::parse(jsonStr);

                // 从JSON中提取文本内容
                if (jsonData.contains("choices") && !jsonData["choices"].empty()) {
                    auto& choices = jsonData["choices"];

                    if (choices[0].contains("delta") &&
                        choices[0]["delta"].contains("content") &&
                        !choices[0]["delta"]["content"].is_null()) {

                        std::string content = choices[0]["delta"]["content"].get<std::string>();
                        processed += content;
                    }
                }
            }
            catch (...) {
                // JSON解析失败，这个数据块可能不完整
                // 如果不是最后一个数据块，则忽略并继续
                if (endPos != buffer.length()) {
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
        if (processedLength > 0) {
            // 只保留未处理的部分
            if (processedLength < buffer.length()) {
                *dstr->str1 = buffer.substr(processedLength);
            } else {
                dstr->str1->clear();
            }
        }

        // 将解析出的内容添加到输出
        if (!processed.empty()) {
            dstr->str2->append(processed);
        }
    }

    return totalSize;
}

int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    ChatBot* chatBot = static_cast<ChatBot*>(clientp);
    if (chatBot && chatBot->forceStop.load())
    {
        return 1;
    }
    return 0;
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
                    dstr.str1 = new string();  // 用于处理缓冲区
                    dstr.str2 = &std::get<0>(Response[ts]);  // 最终输出结果
                    dstr.response = new string();  // 保存完整响应
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
                        std::cout<<std::get<0>(Response[ts])<<std::endl;
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
                // 如果生成过程中被取消但已有部分结果
                json response;
                response["content"] = res;
                response["role"] = "assistant";
                history.emplace_back(response);
                std::get<1>(Response[timeStamp]) = true;
                LogInfo("ChatBot: Post canceled but partial result saved");
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
            LogInfo("ChatBot: Post finished");
        }
        while (!std::get<0>(Response[timeStamp]).empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            //等待被处理完成
        }
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

std::string Claude::Submit(std::string text, size_t timeStamp, std::string role, std::string convid)
{
    try
    {
        std::lock_guard<std::mutex> lock(historyAccessMutex);
        Response[timeStamp] = std::make_tuple("", false);
        cpr::Payload payload{
            {"token", claudeData.slackToken},
            {"channel", claudeData.channelID},
            {"text", text}
        };

        cpr::Response r;
        auto future = cpr::PostAsync(cpr::Url{"https://slack.com/api/chat.postMessage"},
                                     payload);

        // 等待响应，但允许中断
        while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready)
        {
            // 每100毫秒检查一次是否要求停止
            {
                std::lock_guard<std::mutex> stopLock(forceStopMutex);
                if (forceStop)
                {
                    // 记录被中断的状态
                    std::get<0>(Response[timeStamp]) = "操作已被取消";
                    std::get<1>(Response[timeStamp]) = true;
                    return "操作已被取消";
                }
            }
        }

        // 获取结果
        r = future.get();

        if (r.status_code == 200)
        {
            json response = json::parse(r.text);
        }
        else
        {
            // 发送失败,打印错误信息
            LogError(r.error.message);
            std::get<0>(Response[timeStamp]) = "请求失败: " + r.error.message;
        }
    }
    catch (std::exception& e)
    {
        LogError(e.what());
        std::get<0>(Response[timeStamp]) = "发生错误: " + std::string(e.what());
    }

    while (!std::get<0>(Response[timeStamp]).empty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        //等待被处理完成
    }
    std::get<1>(Response[timeStamp]) = true;
    return std::get<0>(Response[timeStamp]);
}

void Claude::Reset()
{
    /*        cpr::Payload payload{
                {"token",   claudeData.slackToken},
                {"channel", claudeData.channelID},
                {"command", "/reset"}};
        cpr::Header header{{"Cookie", claudeData.cookies}};
        std::string url = "https://" + claudeData.userName + ".slack.com/api/chat.command";
        cpr::Response r = cpr::Post(cpr::Url{url},
                                    payload, header);
        if (r.status_code == 200) {
            // 发送成功
        } else {
            // 发送失败,打印错误信息
            LogError(r.error.message);
        }*/
    Submit("请忘记上面的会话内容", Logger::getCurrentTimestamp());
    LogInfo("Claude : 重置成功");
}

void Claude::Load(std::string name)
{
    LogInfo("Claude : 不支持的操作");
}

void Claude::Save(std::string name)
{
    LogInfo("Claude : 不支持的操作");
}

void Claude::Del(std::string id)
{
    LogInfo("Claude : 不支持的操作");
}

void Claude::Add(std::string name)
{
    LogInfo("Claude : 不支持的操作");
}

map<long long, string> Claude::GetHistory()
{
    try
    {
        History.clear();
        auto _ts = to_string(Logger::getCurrentTimestamp());
        cpr::Payload payload = {
            {"channel", claudeData.channelID},
            {"latest", _ts},
            {"token", claudeData.slackToken}
        };
        cpr::Response r2 = cpr::Post(cpr::Url{"https://slack.com/api/conversations.history"},
                                     payload);
        json j = json::parse(r2.text);
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
    catch (const std::exception& e)
    {
        // 捕获并处理异常
        LogError("获取历史记录失败:" + string(e.what()));
    }
    return History;
}

std::string Gemini::Submit(std::string prompt, size_t timeStamp, std::string role, std::string convid)
{
    try
    {
        std::lock_guard<std::mutex> lock(historyAccessMutex);
        convid_ = convid;
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
        history.emplace_back(ask);
        Conversation[convid] = history;
        std::string data = "{\"contents\":" + Conversation[convid].dump() + "}";
        std::string endPoint = "";
        if (!geminiData._endPoint.empty() && geminiData._endPoint != "")
            endPoint = geminiData._endPoint;
        else
            endPoint = "https://generativelanguage.googleapis.com";
        std::string url = endPoint + "/v1beta/models/"+geminiData.model+":generateContent?key="
            +
            geminiData._apiKey;

        int retry_count = 0;
        while (retry_count < 3)
        {
            // 检查强制停止标志
            {
                std::lock_guard<std::mutex> stopLock(forceStopMutex);
                if (forceStop)
                {
                    // 设置响应状态并返回
                    std::get<0>(Response[timeStamp]) = "操作已被取消";
                    std::get<1>(Response[timeStamp]) = true;
                    return "操作已被取消";
                }
            }

            // 创建一个异步请求
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
            std::optional<std::string> res = response["candidates"][0]["content"]["parts"][0]["text"];
            json ans;
            ans["role"] = "model";
            ans["parts"] = json::array();
            ans["parts"].push_back(json::object());
            ans["parts"][0]["text"] = res.value();
            history.emplace_back(ans);
            Conversation[convid_] = history;
            std::get<0>(Response[timeStamp]) = res.value_or("NA");
            while (!std::get<0>(Response[timeStamp]).empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                //等待被处理完成
            }
            std::get<1>(Response[timeStamp]) = true;
            return res.value_or("NA");
        }

        // 所有重试都失败
        std::get<0>(Response[timeStamp]) = "所有请求尝试都失败了";
        std::get<1>(Response[timeStamp]) = true;
    }
    catch (const std::exception& e)
    {
        LogError("获取历史记录失败:" + std::string(e.what()));
        std::get<0>(Response[timeStamp]) = "发生错误: " + std::string(e.what());
        std::get<1>(Response[timeStamp]) = true;
    }

    return std::get<0>(Response[timeStamp]);
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


llama_chat_message LLama::ChatMessage::To()
{
    return {role.c_str(), _strdup(content.c_str())};
}

std::vector<llama_chat_message> LLama::chatInfo::To()
{
    std::vector<llama_chat_message> msgs;
    for (auto& m : messages)
    {
        msgs.push_back(m.To());
    }
    return msgs;
}

uint16_t LLama::GetGPUMemory()
{
    vk::ApplicationInfo appInfo(
        "GPU Memory Query", VK_MAKE_VERSION(1, 0, 0),
        "No Engine", VK_MAKE_VERSION(1, 0, 0),
        VK_API_VERSION_1_0
    );

    vk::InstanceCreateInfo instanceCreateInfo({}, &appInfo);
    vk::Instance instance = vk::createInstance(instanceCreateInfo);

    std::vector<vk::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
    if (physicalDevices.empty())
    {
        LogError("No GPU physical devices found");
        return -1;
    }

    vk::PhysicalDevice physicalDevice = physicalDevices[0];

    vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();
    std::cout << "Device Name: " << deviceProperties.deviceName << std::endl;

    vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

    vk::DeviceSize totalLocalMemory = 0;
    for (const auto& heap : memoryProperties.memoryHeaps)
    {
        if (heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
        {
            totalLocalMemory += heap.size;
        }
    }
    std::cout << "Total GPU Memory: " << totalLocalMemory / (1024 * 1024) << " MB" << std::endl;

    instance.destroy();

    return static_cast<uint16_t>(totalLocalMemory / (1024 * 1024));
}

uint16_t LLama::GetGPULayer()
{
    //获取当前显卡的显存
    uint16_t gpu_memory = GetGPUMemory();
    if (gpu_memory >= 24000) //24GB
    {
        return 60;
    }
    else if (gpu_memory >= 16000) //16GB
    {
        return 40;
    }
    else if (gpu_memory >= 8000) //8GB
    {
        return 20;
    }
    else if (gpu_memory >= 4000) //4GB
    {
        return 10;
    }
    else if (gpu_memory >= 2000) //2GB
    {
        return 5;
    }
    else
    {
        return 0;
    }
}
LLama::LLama(const LLamaCreateInfo& data, const std::string& sysr): llamaData(data), sys(sysr)
{
    // 初始化后端
    ggml_backend_load_all();

    // 设置日志回调
    llama_log_set([](enum ggml_log_level level, const char* text, void* /* user_data */)
    {
        if (level >= GGML_LOG_LEVEL_ERROR)
        {
            LogError("{0}", text);
        }
    }, nullptr);

    // 初始化默认参数
    llama_model_params params = llama_model_default_params();
    llama_context_params context_params = llama_context_default_params();

    // 配置上下文参数
    context_params.n_threads = 8;
    context_params.n_ctx = llamaData.contextSize;
    context_params.n_batch = llamaData.maxTokens;
    context_params.no_perf = false;

    // 尝试使用CPU模式，避免GPU问题
    int gpu_layers = GetGPULayer();
    params.n_gpu_layers = gpu_layers;

    LogInfo("LLama: 尝试加载模型，GPU层数: {0}", gpu_layers);

    // 检查文件是否存在
    if (!UFile::Exists(llamaData.model))
    {
        LogError("LLama: 模型文件未找到: {0}", llamaData.model);
        return;
    }

    // 检查文件权限
    FILE* test = fopen(llamaData.model.c_str(), "rb");
    if (!test) {
        LogError("LLama: 无法打开模型文件，请检查读取权限: {0}", strerror(errno));
        return;
    }
    fclose(test);

    // 尝试加载模型
    try {
        // 先尝试加载模型
        LogInfo("LLama: 开始加载模型文件 {0}", llamaData.model);
        model = llama_model_load_from_file(llamaData.model.c_str(), params);

        if (model == nullptr) {
            LogError("LLama: 模型加载失败！尝试降低GPU层数重新加载");
            // 尝试完全CPU模式
            params.n_gpu_layers = 0;
            model = llama_model_load_from_file(llamaData.model.c_str(), params);
        }

        if (model == nullptr) {
            LogError("LLama: 模型加载失败！尝试减小上下文大小");
            // 尝试减小上下文大小
            context_params.n_ctx =min(llamaData.contextSize, 2048);
            context_params.n_batch = min(llamaData.maxTokens, 512);
        } else {
            LogInfo("LLama: 模型加载成功！");
        }

        // 初始化上下文
        LogInfo("LLama: 开始初始化上下文");
        ctx = llama_init_from_model(model, context_params);

        if (ctx == nullptr) {
            LogError("LLama: 上下文初始化失败！可能是内存不足或参数设置不当");
        } else {
            LogInfo("LLama: 上下文初始化成功！");
        }
    } catch (const std::exception& e) {
        LogError("LLama: 加载模型过程中发生异常: {0}", e.what());
        return;
    }

    // 检查模型和上下文是否成功初始化
    if (model == nullptr || ctx == nullptr)
    {
        LogError("LLama: 模型或上下文初始化失败！");
        return;
    }

    // 初始化词汇表和采样器
    vocab = llama_model_get_vocab(model);
    llama_sampler_chain_params p = llama_sampler_chain_default_params();
    p.no_perf = true;
    smpl = llama_sampler_chain_init(p);
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
    formatted = std::vector<char>(llama_n_ctx(ctx));

    LogInfo("LLama: 初始化完成，上下文大小: {0}, 最大令牌数: {1}", llamaData.contextSize, llamaData.maxTokens);
}
std::string LLama::Submit(std::string prompt, size_t timeStamp, std::string role, std::string convid)
{
    // 检查上下文是否已初始化
    if (!ctx)
    {
        LogError("LLama: 上下文未初始化，无法处理请求");
        return "错误: LLama上下文未初始化。";
    }

    std::lock_guard<std::mutex> lock(historyAccessMutex);

    // 初始化对话历史
    if (!history.contains(convid))
    {
        LogInfo("LLama: 创建新的对话历史 ID: {0}", convid);
        history[convid].messages = std::vector<ChatMessage>();
        history[convid].messages.push_back({"system", sys});
        history[convid].prev_len = 0;
    }

    LogInfo("LLama: 开始生成响应");
    Response[timeStamp] = std::make_tuple("", false);

    // 应用聊天模板
    const char* tmpl = llama_model_chat_template(model, /* name */ nullptr);
    if (!tmpl) {
        LogWarn("LLama: 模型没有内置聊天模板，使用默认格式");
    }

    // 添加用户消息
    history[convid].messages.push_back({role, prompt});

    // 应用聊天模板
    int new_len = llama_chat_apply_template(tmpl, history[convid].To().data(),
                                        history[convid].To().size(), true, formatted.data(), formatted.size());
    if (new_len < 0)
    {
        LogError("LLama: 应用聊天模板失败");
        std::get<0>(Response[timeStamp]) = "错误: 应用聊天模板失败";
        std::get<1>(Response[timeStamp]) = true;
        return "错误: 应用聊天模板失败";
    }

    // 如果格式化后的长度超出缓冲区大小，则重新分配
    if (new_len > (int)formatted.size())
    {
        LogInfo("LLama: 重新分配格式化缓冲区大小为 {0}", new_len);
        formatted.resize(new_len);
        new_len = llama_chat_apply_template(tmpl, history[convid].To().data(),
                                        history[convid].To().size(), true, formatted.data(),
                                        formatted.size());
    }

    // 提取当前提示
    prompt = std::string(formatted.begin() + history[convid].prev_len, formatted.begin() + new_len);

    {
        // 检查是否要求强制停止
        {
            std::lock_guard<std::mutex> stopLock(forceStopMutex);
            if (forceStop)
            {
                LogInfo("LLama: 操作被用户取消");
                std::get<0>(Response[timeStamp]) = "操作已被取消";
                std::get<1>(Response[timeStamp]) = true;
                return "操作已被取消";
            }
        }

        // 对提示进行分词
        LogInfo("LLama: 对提示进行分词");
        int n_prompt = -llama_tokenize(vocab, prompt.c_str(), prompt.size(), NULL, 0, true, true);
        if (n_prompt <= 0) {
            LogError("LLama: 分词失败，返回的token数量为 {0}", n_prompt);
            std::get<0>(Response[timeStamp]) = "错误: 分词失败";
            std::get<1>(Response[timeStamp]) = true;
            return "错误: 分词失败";
        }

        std::vector<llama_token> prompt_tokens(n_prompt);
        if (llama_tokenize(vocab, prompt.c_str(), prompt.size(), prompt_tokens.data(), prompt_tokens.size(), true, true) < 0)
        {
            LogError("LLama: 对提示进行分词失败");
            std::get<0>(Response[timeStamp]) = "错误: 对提示进行分词失败";
            std::get<1>(Response[timeStamp]) = true;
            return "错误: 对提示进行分词失败";
        }

        LogInfo("LLama: 提示分词完成，共 {0} 个token", n_prompt);

        // 再次检查是否要求强制停止
        {
            std::lock_guard<std::mutex> stopLock(forceStopMutex);
            if (forceStop)
            {
                LogInfo("LLama: 操作被用户取消");
                std::get<0>(Response[timeStamp]) = "操作已被取消";
                std::get<1>(Response[timeStamp]) = true;
                return "操作已被取消";
            }
        }

        // 创建批处理
        llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
        int n_batch = llama_n_batch(ctx);
        int error = 0;

        // 检查是否超出批处理大小
        if (batch.n_tokens > n_batch)
        {
            LogError("LLama: token数量超出批处理大小，请减少token数量或增加maxTokens大小");
            std::get<0>(Response[timeStamp]) = "错误: token数量超出批处理大小，请减少token数量或增加maxTokens大小";
            std::get<1>(Response[timeStamp]) = true;
            error = 1;
        }

        if (error == 0)
        {
            // 检查上下文大小
            int n_ctx = llama_n_ctx(ctx);
            int n_ctx_used = llama_get_kv_cache_used_cells(ctx);

            if (n_ctx_used + batch.n_tokens > n_ctx) {
                LogError("LLama: 上下文大小超出限制，请打开新会话或重置");
                std::get<0>(Response[timeStamp]) = "错误: 上下文大小超出限制，请打开新会话或重置";
                std::get<1>(Response[timeStamp]) = true;
                error = 3;
            } else {
                // 开始生成
                LogInfo("LLama: 开始解码生成文本");

                // 首先处理提示
                if (llama_decode(ctx, batch)) {
                    LogError("LLama: 解码失败");
                    std::get<0>(Response[timeStamp]) = "错误: 解码失败";
                    std::get<1>(Response[timeStamp]) = true;
                    error = 2;
                } else {
                    // 逐个生成token
                    llama_token new_token_id;
                    while (true)
                    {
                        // 检查是否要求强制停止
                        {
                            std::lock_guard<std::mutex> stopLock(forceStopMutex);
                            if (forceStop)
                            {
                                LogInfo("LLama: 生成被强制中断");
                                std::get<0>(Response[timeStamp]) += "\n[生成被中断]";
                                std::get<1>(Response[timeStamp]) = true;
                                error = 5;
                                break;
                            }
                        }

                        // 采样新token
                        new_token_id = llama_sampler_sample(smpl, ctx, -1);

                        // 检查是否生成结束
                        if (llama_vocab_is_eog(vocab, new_token_id))
                        {
                            LogInfo("LLama: 生成完成，遇到结束标记");
                            break;
                        }

                        // 转换token为文本片段
                        char buf[256];
                        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
                        if (n < 0)
                        {
                            LogError("LLama: 转换token为文本片段失败");
                            std::get<0>(Response[timeStamp]) = "错误: 转换token为文本片段失败";
                            std::get<1>(Response[timeStamp]) = true;
                            error = 4;
                            break;
                        }

                        // 添加到结果中
                        std::string piece(buf, n);
                        std::get<0>(Response[timeStamp]) += piece;

                        // 创建新的批处理进行下一个token的生成
                        batch = llama_batch_get_one(&new_token_id, 1);
                        if (llama_decode(ctx, batch)) {
                            LogError("LLama: 解码失败");
                            std::get<0>(Response[timeStamp]) = "错误: 解码失败";
                            std::get<1>(Response[timeStamp]) = true;
                            error = 2;
                            break;
                        }
                    }
                }
            }
        }

        // 如果生成成功或被强制停止，更新聊天历史
        if (error == 0 || error == 5)
        {
            LogInfo("LLama: 生成完成，更新聊天历史");
            history[convid].messages.push_back({"assistant", std::get<0>(Response[timeStamp])});
            history[convid].prev_len = llama_chat_apply_template(tmpl, history[convid].To().data(),
                                                             history[convid].To().size(), false,
                                                             nullptr, 0);
            if (history[convid].prev_len < 0)
            {
                LogError("LLama: 应用聊天模板失败");
                std::get<0>(Response[timeStamp]) = "错误: 应用聊天模板失败";
                std::get<1>(Response[timeStamp]) = true;
                return "错误: 应用聊天模板失败";
            }
        }
    }

    // 等待响应被处理完成
    while (!std::get<0>(Response[timeStamp]).empty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::get<1>(Response[timeStamp]) = true;
    return std::get<0>(Response[timeStamp]);
}
LLama::~LLama()
{
    try
    {
        if (model)
            llama_model_free(model);
        if (ctx)
            llama_free(ctx);
    }
    catch (...)
    {
    }
}

void LLama::Reset()
{
    history[convid_].messages.clear();
    history[convid_].messages.push_back({"system", sys.c_str()});
    history[convid_].prev_len = 0;
    Del(convid_);
    Save(convid_);
}

void LLama::Load(std::string name)
{
    try
    {
        std::lock_guard<std::mutex> lock(fileAccessMutex);
        std::stringstream buffer;
        std::ifstream session_file(ConversationPath + name + suffix);
        if (session_file.is_open())
        {
            buffer << session_file.rdbuf();
            json j = json::parse(buffer.str());
            if (j.is_array())
            {
                for (auto& it : j)
                {
                    if (it.is_object())
                    {
                        history[name].messages.push_back({
                            it["role"].get<std::string>().c_str(),
                            it["content"].get<std::string>().c_str()
                        });
                    }
                }
            }
        }
        else
        {
            LogError("ChatBot Error: Unable to load session " + name + ".");
        }
    }
    catch (std::exception& e)
    {
        LogError(e.what());
        history[name].messages.clear();
        history[name].messages.push_back({"system", sys.c_str()});
    }
    convid_ = name;
    LogInfo("Bot: 加载 {0} 成功", name);
}

void LLama::Save(std::string name)
{
    json j;
    for (auto& m : history[name].messages)
    {
        j.push_back({
            {"role", m.role},
            {"content", m.content}
        });
    }
    std::ofstream session_file(ConversationPath + name + suffix);
    if (session_file.is_open())
    {
        session_file << j.dump();
        session_file.close();
        LogInfo("Bot : Save {0} successfully", name);
    }
    else
    {
        LogError("ChatBot Error: Unable to save session {0},{1}", name, ".");
    }
}

void LLama::Del(std::string name)
{
    if (remove((ConversationPath + name + suffix).c_str()) != 0)
    {
        LogError("ChatBot Error: Unable to delete session {0},{1}", name, ".");
    }
    LogInfo("Bot : 删除 {0} 成功", name);
}

void LLama::Add(std::string name)
{
    history[name].messages = std::vector<ChatMessage>();
    history[name].messages.push_back({"system", sys.c_str()});
    history[name].prev_len = 0;
    Save(name);
}

map<long long, string> LLama::GetHistory()
{
    return map<long long, string>();
}
