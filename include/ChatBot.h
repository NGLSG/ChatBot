#ifndef CHATBOT_H
#define CHATBOT_H

#include "VoiceToText.h"
#include "utils.h"

using json = nlohmann::json;
using namespace std;
namespace Role {
    static std::string User = "user";
    static std::string System = "system";
    static std::string Assistant = "assistant";
};

class ChatBot {
public:
    ChatBot(const OpenAIData &chat_data);

    std::string
    Submit(std::string prompt, std::string role = Role::User, std::string convid = "defult");

    std::future<std::string> SubmitAsync(std::string prompt, std::string role, std::string convid);

    void Reset();

    void Load(std::string name = "default");

    void Save(std::string name = "default");

    void Del(std::string name);

    void Add(std::string name);

    json history;

private:

    cpr::Session session;
    OpenAIData chat_data_;
    std::string mode_name_ = "default";
    std::string convid_ = "default";
    std::map<std::string, json> Conversation;
    std::mutex print_mutex;
    const std::string ConversationPath = "Conversations/";
    const std::string sys = "You are ChatGPT, a large language model trained by OpenAI. Respond conversationally";
    const std::string suffix = ".dat";

    json LastHistory;
    json defaultJson;

    bool IsSaved() {
        return LastHistory == history;
    }

    string sendRequest(std::string data);


};


#endif