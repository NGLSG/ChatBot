#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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

    std::string submit(const std::string& prompt, const std::string& role = Role::User);

    void reset();

    void setMode(std::string ModeName = "default");

    std::string getMode();

    void saveMode(const std::string& ModeName, const std::string& content);

    void delMode(const std::string& ModeName);

    void load(const std::string& name);

    void save(const std::string& name);

    void del(const std::string& name);

private:
    ChatData chat_data_;
    std::string mode_name_ = "default";
    std::string memory_;
    std::string log_file_ = "chatbot.log";
    std::shared_ptr<spdlog::logger> logger_;

    json sendRequest(const std::string& data);

    void log(const std::string& message);

    void LogError(const std::string &errorMessage);
};