#ifndef GEMINI_IMPL_H
#define GEMINI_IMPL_H

#include "ChatBot.h"

class Gemini : public ChatBot
{
public:
    Gemini(const GeminiBotCreateInfo& data, const std::string sys) : geminiData(data)
    {
        json _t;

        _t["role"] = "user";
        _t["parts"] = json::array();
        _t["parts"].push_back(json::object());
        _t["parts"][0]["text"] = sys;

        json _t2;
        _t2["role"] = "model";
        _t2["parts"] = json::array();
        _t2["parts"].push_back(json::object());
        _t2["parts"][0]["text"] = "Yes I am here to help you.";
        SystemPrompt.push_back(_t);
        SystemPrompt.push_back(_t2);
    }

    std::string sendRequest(std::string data, size_t ts) override;
    std::string Submit(std::string prompt, size_t timeStamp, std::string role,
                       std::string convid, float temp,
                       float top_p,
                       uint32_t top_k,
                       float pres_pen,
                       float freq_pen, bool async) override;

    void Reset() override;

    void Load(std::string name) override;

    void Save(std::string name) override;

    void Del(std::string name) override;

    void Add(std::string name) override;

    map<long long, string> GetHistory() override { return map<long long, string>(); }
    void BuildHistory(const std::vector<std::pair<std::string, std::string>>& history) override;

    std::string GetModel() override
    {
        return geminiData.model;
    }

private:
    GeminiBotCreateInfo geminiData;
    const std::string ConversationPath = "Conversations/Gemini/";
    json SystemPrompt;
    const std::string suffix = ".dat";
    std::string convid_ = "default";
    std::map<std::string, json> Conversation;
    json history;
};

#endif //GEMINI_IMPL_H
