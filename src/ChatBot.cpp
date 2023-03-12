#include "ChatBot.h"
#include <iostream>
#include <fstream>

ChatBot::ChatBot(ChatData chat_data) : chat_data_(std::move(chat_data)) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("chatbot.log", true);
    auto logger = std::make_shared<spdlog::logger>("Logger", spdlog::sinks_init_list{console_sink, file_sink});
    logger->set_level(spdlog::level::debug);
    logger_ = logger;
}

json ChatBot::sendRequest(const std::string &data) {
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

std::string ChatBot::submit(const std::string &prompt, const std::string &role) {
    std::string data = "{\n"
                       "  \"model\": \"gpt-3.5-turbo\",\n"
                       "  \"messages\": [{\"role\": \"" + role + R"(", "content": ")" + prompt + "\"}]\n"
                                                                                                 "}";

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
    std::string finish_reason = response["choices"][0]["finish_reason"];

    memory_ += text + "\n";
    return text;
}

void ChatBot::reset() {
    memory_ = "";
}

void ChatBot::setMode(std::string ModeName) {
    mode_name_ = std::move(ModeName);
}

std::string ChatBot::getMode() {
    return mode_name_;
}

void ChatBot::saveMode(const std::string &ModeName, const std::string &content) {
    std::ofstream mode_file(ModeName + ".txt");

    if (mode_file.is_open()) {
        mode_file << content;
        mode_file.close();
    } else {
        LogError("Error: Unable to save mode " + ModeName + ".");
    }
}

void ChatBot::delMode(const std::string &ModeName) {
    if (remove((ModeName + ".txt").c_str()) != 0) {
        LogError("Error: Unable to delete mode " + ModeName + ".");
    }
}

void ChatBot::load(const std::string &name) {
    std::ifstream session_file(name + ".txt");

    if (session_file.is_open()) {
        std::string session_content((std::istreambuf_iterator<char>(session_file)),
                                    std::istreambuf_iterator<char>());
        memory_ = session_content;
        session_file.close();
    } else {
        LogError("Error: Unable to load session " + name + ".");
    }
}

void ChatBot::save(const std::string &name) {
    std::ofstream session_file(name + ".txt");

    if (session_file.is_open()) {
        session_file << memory_;
        session_file.close();
    } else {
        log("Error: Unable to save session " + name + ".");
    }
}

void ChatBot::del(const std::string &name) {
    if (remove((name + ".txt").c_str()) != 0) {
        log("Error: Unable to delete session " + name + ".");
    }
}

void ChatBot::log(const std::string &message) {
    std::ofstream log_file(log_file_, std::ios_base::app);
    if (log_file.is_open()) {
        log_file << message << std::endl;
        log_file.close();
    } else {
        std::cerr << "Error: Unable to open log file." << std::endl;
    }
}

void ChatBot::LogError(const std::string &errorMessage) {
    // Output error message to console

    // Log error message to file
    logger_->error(errorMessage);
}