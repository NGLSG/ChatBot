#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "utils.h"

using json = nlohmann::json;

struct Proxy {
    std::string proxy = "";
    std::string ip = "";
    std::string port = "";
};

struct ChatData {
    std::string api_key;
    std::string model = "gpt-3.5-turbo";
    Proxy proxy = Proxy();
};

namespace Role {
    static std::string User = "user";
    static std::string System = "system";
    static std::string Assistant = "assistant";
};

class ChatBot {
public:
    ChatBot(ChatData chat_data);

    std::string submit(std::string prompt, std::string role = Role::User, std::string convid = "defult");

    void reset();

    void load(std::string name = "default");

    void save(std::string name = "default");

    void del(std::string name);

    void log(std::string message);

    void LogError(std::string Message);

    void LogWarn(std::string Message);

    void LogInfo(std::string Message);


private:
    ChatData chat_data_;
    std::string mode_name_ = "default";
    std::string convid_ = "default";
    std::string log_file_ = "chatbot.log";
    std::shared_ptr<spdlog::logger> logger_;

    std::map<std::string, json> Conversation;
    json history;
    const std::string PresetPath = "Presets/";
    const std::string ConversationPath = "Conversations/";

    const std::string suffix = ".dat";

    json sendRequest(std::string data);


    const std::string sys = "You are ChatGPT, a large language model trained by OpenAI. Respond conversationally";
    json defaultJson;
};