#include "ChatBot.h"


ChatBot::ChatBot(ChatData chat_data) : chat_data_(std::move(chat_data)) {
    defaultJson["content"] = sys;
    defaultJson["role"] = "system";

    if (!UDirectory::Exists(PresetPath)) {
        UDirectory::Create(PresetPath);
    }
    if (!UDirectory::Exists(ConversationPath)) {
        UDirectory::Create(ConversationPath);
    }

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("chatbot.log", true);
    auto logger = std::make_shared<spdlog::logger>("Logger", spdlog::sinks_init_list{console_sink, file_sink});
    logger->set_level(spdlog::level::debug);
    logger_ = logger;
}

json ChatBot::sendRequest(std::string data) {
    if (!chat_data_.proxy.proxy.empty()) {
        cpr::Proxies proxies = {
                {"http",  chat_data_.proxy.proxy},
                {"https", chat_data_.proxy.proxy}
        };

        try {

            cpr::Response response = cpr::Post(cpr::Url{"https://api.openai.com/v1/chat/completions"},
                                               cpr::Body{data},
                                               cpr::Bearer({chat_data_.api_key}),
                                               cpr::Header{{"Content-Type", "application/json"}},
                                               proxies,
                                               cpr::VerifySsl{false});

            if (response.status_code != 200) {
                LogError("Error: Request failed with status code " + std::to_string(response.status_code));
                return {};
            }

            json parsed_response = json::parse(response.text);
            return parsed_response;
        } catch (json::exception &e) {
            LogError("Error: " + std::string(e.what()));

            return {};
        }
    } else {
        try {
            cpr::Response response = cpr::Post(cpr::Url{"https://api.openai.com/v1/chat/completions"},
                                               cpr::Bearer({chat_data_.api_key}),
                                               cpr::Header{{"Content-Type", "application/json"}},
                                               cpr::Body{data});

            if (response.status_code != 200) {
                LogError("Error: Request failed with status code " + std::to_string(response.status_code));
                return {};
            }

            json parsed_response = json::parse(response.text);
            return parsed_response;
        } catch (json::exception &e) {
            LogError("Error: " + std::string(e.what()));
            return {};
        }
    }
}

std::string ChatBot::submit(std::string prompt, std::string role, std::string convid) {
    convid_ = convid;
    json ask;
    ask["content"] = prompt;
    ask["role"] = role;

    if (Conversation.find(convid) == Conversation.end()) {

        history.push_back(defaultJson);
        Conversation.insert({convid, history});
    }
    history.push_back(ask);
    Conversation[convid] = history;
    std::string data = "{\n"
                       "  \"model\": \"gpt-3.5-turbo\",\n"
                       "  \"messages\": " + Conversation[convid].dump()
                       + "}\n";
    json response = sendRequest(data);

    if (response.is_null()) {
        LogError("Error: Response is null.");
        return "";
    }

    if (response.find("choices") == response.end() || response["choices"].empty()) {
        LogError("Error: No choices found in response.");
        return "";
    }

    std::string text = response["choices"][0]["message"]["content"];
    //std::string finish_reason = response["choices"][0]["finish_reason"];

    if (!text.empty())
        while (text[0] == '\n') {
            text.erase(0, 1);
        }
    else {
        LogWarn("Warning:Result is null");
        return "";
    }
    return text;
}

void ChatBot::reset() {

    history.clear();
    history.push_back(defaultJson);
    Conversation[convid_] = history;
}

void ChatBot::load(std::string name) {

    std::stringstream buffer;
    std::ifstream session_file(ConversationPath + name + suffix);
    if (session_file.is_open()) {
        buffer<<session_file.rdbuf();
        history=json::parse(buffer.str());
    } else {
        LogError("Error: Unable to load session " + name + ".");
    }
    log("加载成功");
}

void ChatBot::save(std::string name) {
    std::ofstream session_file(ConversationPath + name + suffix);

    if (session_file.is_open()) {
        session_file << history.dump();
        session_file.close();
    } else {
        log("Error: Unable to save session " + name + ".");
    }
}

void ChatBot::del(std::string name) {
    if (remove((ConversationPath + name + suffix).c_str()) != 0) {
        log("Error: Unable to delete session " + name + ".");
    }
}

void ChatBot::log(std::string message) {
    std::ofstream log_file(log_file_, std::ios_base::app);
    if (log_file.is_open()) {
        log_file << message << std::endl;
        log_file.close();
    } else {
        std::cerr << "Error: Unable to open log file." << std::endl;
    }
}

void ChatBot::LogError(std::string Message) {
    logger_->error(Message);
}

void ChatBot::LogWarn(std::string Message) {
    logger_->warn(Message);
}

void ChatBot::LogInfo(std::string Message) {
    logger_->info(Message);
}
