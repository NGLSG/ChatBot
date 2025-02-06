#include "ChatBot.h"

//userp:<string,string>
struct DString
{
    std::string* str1;
    std::string* str2;
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
    DString* dstr = static_cast<DString*>(userp);
    size_t totalSize = size * nmemb;
    std::string dataChunk(static_cast<char*>(contents), totalSize);

    {
        // Lock while appending to str1.
        std::lock_guard<std::mutex> lock(dstr->mtx);
        dstr->str1->append(dataChunk);
    }

    std::stringstream ss;
    {
        // Lock access to copy data for processing.
        std::lock_guard<std::mutex> lock(dstr->mtx);
        ss.str(*dstr->str1);
    }
    std::string line;
    std::string remaining;

    // Process the buffer line-by-line.
    while (std::getline(ss, line))
    {
        // Skip empty or all-whitespace lines.
        if (line.find_first_not_of(" \t\r\n") == std::string::npos)
            continue;

        TrimLeading(line);

        const std::string dataPrefix = "data: ";
        if (line.compare(0, dataPrefix.length(), dataPrefix) == 0)
        {
            line = line.substr(dataPrefix.length());
        }

        size_t donePos = line.find("[DONE]");
        if (donePos != std::string::npos)
        {
            line.erase(donePos);
        }

        try
        {
            json resp = json::parse(line);
            json choices = resp.value("choices", json::array());
            if (!choices.empty())
            {
                json delta = choices[0].value("delta", json::object());
                if (!delta.empty() && delta.contains("content"))
                {
                    if (delta["content"].is_string())
                    {
                        // Lock while updating str2.
                        std::lock_guard<std::mutex> lock(dstr->mtx);
                        dstr->str2->append(delta["content"].get<std::string>());
                        std::cout << "\r" << dstr->str2 << std::flush;
                    }
                }
            }
        }
        catch (const json::exception& e)
        {
            remaining += line + "\n";
        }
        catch (const std::exception& e)
        {
        }
    }

    {
        std::lock_guard<std::mutex> lock(dstr->mtx);
        *dstr->str1 = remaining;
    }

    return totalSize;
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
            try
            {
                LogInfo("ChatBot: Post request...");
                std::string url = "";
                if (!chat_data_.useWebProxy)
                {
                    url = "https://api.openai.com/v1/chat/completions";
                }
                else
                {
                    url = chat_data_._endPoint;
                }

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
                    DString dstr;
                    dstr.str1 = new string();
                    dstr.str2 = &std::get<0>(Response[ts]);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dstr);
                    if (!chat_data_.useWebProxy && !chat_data_.proxy.empty())
                    {
                        curl_easy_setopt(curl, CURLOPT_PROXY, chat_data_.proxy.c_str());
                    }
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // disable SSL verification
                    res = curl_easy_perform(curl);
                    if (res != CURLE_OK)
                    {
                        LogError("ChatBot Error: Request failed with error code " + std::to_string(res));
                        retry_count++;
                    }
                    curl_easy_cleanup(curl);
                    curl_slist_free_all(headers);
                    return std::get<0>(Response[ts]);
                }
            }
            catch (std::exception& e)
            {
                LogError("ChatBot Error: " + std::string(e.what()));
                retry_count++;
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


std::string Claude::Submit(std::string text, size_t timeStamp, std::string role, std::string convid)
{
    try
    {
        Response[timeStamp] = std::make_tuple("", false);
        cpr::Payload payload{
            {"token", claudeData.slackToken},
            {"channel", claudeData.channelID},
            {"text", text}
        };

        cpr::Response r = cpr::Post(cpr::Url{"https://slack.com/api/chat.postMessage"},
                                    payload);
        if (r.status_code == 200)
        {
            // 发送成功
        }
        else
        {
            // 发送失败,打印错误信息
            LogError(r.error.message);
        }
        json response = json::parse(r.text);
    }
    catch (std::exception& e)
    {
        LogError(e.what());
    }
    std::get<1>(Response[timeStamp]) = true;
    return "";
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
        convid_ = convid;
        Response[timeStamp] = std::make_tuple("", false);
        json ask;
        /*构造[
        {"role":"user",
          "parts":[{
            "text": "Write the first line of a story about a magic backpack."}]}, json*/
        ask["role"] = role;
        ask["parts"] = json::array();
        ask["parts"].push_back(json::object());
        ask["parts"][0]["text"] = prompt;
        if (!Conversation.contains(convid))
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
        std::string url = endPoint + "/v1beta/models/gemini-pro:generateContent?key="
            +
            geminiData._apiKey;
        int retry_count = 0;
        while (retry_count < 3)
        {
            auto r = cpr::Post(cpr::Url{url}, cpr::Header{{"Content-Type", "application/json"}}, cpr::Body{data});
            if (r.status_code != 200)
            {
                retry_count++;
                LogError("Gemini: {0}", r.text);
                std::get<1>(Response[timeStamp]) = true;
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
            std::get<1>(Response[timeStamp]) = true;
            return res.value_or("NA");
        }
    }
    catch (const std::exception& e)
    {
        LogError("获取历史记录失败:" + string(e.what()));
    }
    return "";
}

void Gemini::Reset()
{
    history.clear();
    Conversation[convid_] = history;
}

void Gemini::Load(std::string name)
{
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

std::string ChatGPT::Submit(std::string prompt, size_t timeStamp, std::string role, std::string convid)
{
    try
    {
        json ask;
        ask["content"] = prompt;
        ask["role"] = role;
        if (!Conversation.contains(convid_))
        {
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
        json response;
        response["content"] = res;
        response["role"] = "assistant";
        std::get<1>(Response[timeStamp]) = true;
        LogInfo("ChatBot: Post finished");
        return res;
    }
    catch (exception& e)
    {
        LogError(e.what());
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
