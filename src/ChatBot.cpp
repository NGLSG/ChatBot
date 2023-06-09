#include "ChatBot.h"

ChatBot::ChatBot(const OpenAIData &chat_data) : chat_data_(chat_data) {
    Logger::Init();
    defaultJson["content"] = sys;
    defaultJson["role"] = "system";

    if (!UDirectory::Exists(ConversationPath)) {
        UDirectory::Create(ConversationPath);
        Add("default");
    }
}

string ChatBot::sendRequest(std::string data) {
    json parsed_response;
    std::string response_role = "";
    std::string full_response = "";
    int retry_count = 0;
    while (retry_count < 3) {
        try {
            LogInfo("ChatBot: Post request to openai...");
            std::string url = "";
            if (!chat_data_.useWebProxy) {
                url = "https://api.openai.com/";
            } else {
                url = WebProxies[chat_data_.webproxy];
            }

            CURL *curl;
            CURLcode res;
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, ("Authorization: Bearer " + chat_data_.api_key).c_str());
            headers = curl_slist_append(headers, "Transfer-Encoding: chunked");

            curl = curl_easy_init();
            if (curl) {
                std::string response = "";
                curl_easy_setopt(curl, CURLOPT_URL, (url + "v1/chat/completions").c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                if (!chat_data_.useWebProxy && !chat_data_.proxy.empty()) {
                    curl_easy_setopt(curl, CURLOPT_PROXY, chat_data_.proxy.c_str());
                }
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // disable SSL verification
                res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    LogError("OpenAI Error: Request failed with error code " + std::to_string(res));
                    parsed_response = {};
                    return "";
                }
                curl_easy_cleanup(curl);
                curl_slist_free_all(headers);
                std::stringstream stream(response);
                std::string line;
                while (std::getline(stream, line)) {
                    if (line.empty()) {
                        continue;
                    }
                    // Remove "data: "
                    line = line.substr(6);
                    if (line == "[DONE]") {
                        break;
                    }
                    json resp = json::parse(line);
                    json choices = resp.value("choices", json::array());
                    if (choices.empty()) {
                        continue;
                    }
                    json delta = choices[0].value("delta", json::object());
                    if (delta.empty()) {
                        continue;
                    }
                    if (delta.count("role")) {
                        response_role = delta["role"].get<std::string>();
                    }
                    if (delta.count("content")) {
                        std::string content = delta["content"].get<std::string>();
                        full_response += content;
                    }
                }
                return full_response;
            }
        }
        catch (json::exception &e) {
            LogError("OpenAI Error: " + std::string(e.what()));
            parsed_response = {};
            retry_count++;
        }
    }
    LogError("OpenAI Error: Request failed after three retries.");
    return "";
}

size_t ChatBot::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *) userp)->append((char *) contents, size * nmemb);
    return size * nmemb;
}

std::future<std::string> ChatBot::SubmitAsync(std::string prompt, std::string role, std::string convid) {
    return std::async(std::launch::async, &ChatBot::Submit, this, prompt, role, convid);
}

std::string ChatBot::Submit(std::string prompt, std::string role, std::string convid) {
    convid_ = convid;
    json ask;
    ask["content"] = prompt;
    ask["role"] = role;
    LogInfo("User asked: {0}", prompt);
    if (Conversation.find(convid) == Conversation.end()) {
        history.push_back(defaultJson);
        Conversation.insert({convid, history});
    }
    history.emplace_back(ask);
    Conversation[convid] = history;
    std::string data = "{\n"
                       "  \"model\": \"" + chat_data_.model + "\",\n"
                                                              "  \"stream\": true,\n"
                                                              "  \"messages\": " +
                       Conversation[convid].dump()
                       + "}\n";
    string text = sendRequest(data);

    std::string str;
    for (char c: text) {
        str += c;
        std::cout << "\rBot: " << str << flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
