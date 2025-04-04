

#ifndef GEMINI_IMPL_H
#define GEMINI_IMPL_H

#include "ChatBot.h"

class Gemini : public ChatBot
{
public:
    Gemini(const GeminiBotCreateInfo& data) : geminiData(data)
    {
    }

    std::string
    Submit(std::string prompt, size_t timeStamp, std::string role = Role::User,
           std::string convid = "defult") override;

    void Reset() override;

    void Load(std::string name = "default") override;

    void Save(std::string name = "default") override;

    void Del(std::string name) override;

    void Add(std::string name) override;

    map<long long, string> GetHistory() override { return map<long long, string>(); }

private:
    GeminiBotCreateInfo geminiData;
    const std::string ConversationPath = "Conversations/Gemini/";
    const std::string suffix = ".dat";
    std::string convid_ = "default";
    std::map<std::string, json> Conversation;
    json history;
};

#endif //GEMINI_IMPL_H
