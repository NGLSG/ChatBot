#include "ChatBot.h"


ChatBot::ChatBot(const OpenAIData &chat_data) : chat_data_(chat_data) {
    Logger::Init();
    defaultJson["content"] = sys;
    defaultJson["role"] = "system";

    if (!UDirectory::Exists(ConversationPath)) {
        UDirectory::Create(ConversationPath);
    }

}

json ChatBot::sendRequest(std::string data) {
    bool ForceStop = false;
    json parsed_response;


    while (!ForceStop) {
        if (!chat_data_.proxy.empty()) {
            cpr::Proxies proxies = {
                    {"http",  chat_data_.proxy},
                    {"https", chat_data_.proxy}
            };

            try {
                session.SetProxies(proxies);

                session.SetUrl(cpr::Url{"https://api.openai.com/v1/chat/completions"});
                session.SetBody(cpr::Body{data});
                session.SetHeader(cpr::Header{{"Content-Type",  "application/json"},
                                              {"Authorization", "Bearer " + chat_data_.api_key}});
                session.SetVerifySsl(cpr::VerifySsl{false});

                // 发送HTTP请求
                cpr::Response response = session.Post();

                if (response.status_code != 200) {
                    LogError("OpenAI Error: Request failed with status code " + std::to_string(response.status_code));
                    parsed_response = {};
                    ForceStop = true;
                }

                parsed_response = json::parse(response.text);
                if (parsed_response["choices"][0]["finish_reason"] == "stop") {
                    ForceStop = true;
                }
            } catch (json::exception &e) {
                LogError("OpenAI Error: " + std::string(e.what()));
                parsed_response = {};
                ForceStop = true;
            }

        } else {
            try {
                session.SetUrl(cpr::Url{"https://api.openai.com/v1/chat/completions"});
                session.SetBody(cpr::Body{data});
                session.SetHeader(cpr::Header{{"Content-Type",  "application/json"},
                                              {"Authorization", "Bearer " + chat_data_.api_key}});
                session.SetVerifySsl(cpr::VerifySsl{false});

                // 发送HTTP请求
                cpr::Response response = session.Post();

                if (response.status_code != 200) {
                    LogError("OpenAI Error: Request failed with status code " + std::to_string(response.status_code));
                    parsed_response = {};
                    ForceStop = true;
                }

                parsed_response = json::parse(response.text);
                if (parsed_response["choices"][0]["finish_reason"] == "stop") {
                    ForceStop = true;
                }
            } catch (json::exception &e) {
                LogError("OpenAI Error: " + std::string(e.what()));
                parsed_response = {};
                ForceStop = true;
            }
        }
    }

    return parsed_response;
}

std::future<std::string> ChatBot::SubmitAsync(std::string prompt, std::string role, std::string convid) {
    return std::async(std::launch::async, &ChatBot::Submit, this, prompt, role, convid);
}

std::string ChatBot::Submit(std::string prompt, std::string role, std::string convid) {
    convid_ = convid;
    json ask;
    ask["content"] = prompt;
    ask["role"] = role;

    if (Conversation.find(convid) == Conversation.end()) {
        history.push_back(defaultJson);
        Conversation.insert({convid, history});
    }
    history.emplace_back(ask);
    Conversation[convid] = history;
    std::string data = "{\n"
                       "  \"model\": \"gpt-3.5-turbo\",\n"
                       "  \"messages\": " + Conversation[convid].dump()
                       + "}\n";
    json response = sendRequest(data);

    if (response.is_null()) {
        LogError("OpenAI Error: Response is null.");
        return "";
    }

    if (response.find("choices") == response.end() || response["choices"].empty()) {
        LogError("OpenAI Error: No choices found in response.");
        return "";
    }

    std::string text = response["choices"][0]["message"]["content"];
    //std::string finish_reason = response["choices"][0]["finish_reason"];

    if (!text.empty())
        while (text[0] == '\n') {
            text.erase(0, 1);
        }
    else {
        LogWarn("OpenAI Warning: Result is null");
        return "";
    }
    return text;
}

void ChatBot::Reset() {

    history.clear();
    history.push_back(defaultJson);
    Conversation[convid_] = history;
}

void ChatBot::Load(std::string name) {

    std::stringstream buffer;
    std::ifstream session_file(ConversationPath + name + suffix);
    if (session_file.is_open()) {
        buffer << session_file.rdbuf();
        history = json::parse(buffer.str());
    } else {
        LogError("OpenAI Error: Unable to load session " + name + ".");
    }
    LogInfo("Bot: 加载 {0} 成功", name);
}

void ChatBot::Save(std::string name) {
    std::ofstream session_file(ConversationPath + name + suffix);

    if (session_file.is_open()) {
        session_file << history.dump();
        session_file.close();
        LogInfo("Bot : Save {0} successfully", name);
    } else {
        LogError("OpenAI Error: Unable to save session {0},{1}", name, ".");
    }
}

void ChatBot::Del(std::string name) {
    if (remove((ConversationPath + name + suffix).c_str()) != 0) {
        LogError("OpenAI Error: Unable to delete session {0},{1}", name, ".");
    }
    LogInfo("Bot : 删除 {0} 成功", name);
}

void ChatBot::Add(std::string name) {
    history.clear();
    history.emplace_back(defaultJson);
    Save(name);
}