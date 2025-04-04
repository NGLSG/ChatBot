
#include "Impls/Gemini_Impl.h"
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