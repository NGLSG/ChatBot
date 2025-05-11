#ifndef CUSTOMROLE_IMPL_H
#define CUSTOMROLE_IMPL_H
#include "ChatBot.h"

class JsonPathBuilder
{
private:
    json rootJson;

    // 向指定路径添加值
    void addValueAtPath(json& jsonObj, const vector<string>& path, const string& value);

public:
    // 添加路径和值
    void addPath(const string& pathStr, const string& value);

    // 获取构建的JSON
    json getJson();
};

class CustomRule_Impl : public ChatBot
{
private:
    CustomRule CustomRuleData;
    const std::string ConversationPath = "Conversations/CustomRule/";
    json SystemPrompt;
    const std::string suffix = ".dat";
    std::string convid_ = "default";
    std::map<std::string, json> Conversation;
    json history;
    json templateJson;
    vector<string> paths;
    vector<string> paths2;
    std::string sendRequest(std::string data, size_t ts) override;

    json buildRequest(const std::string& prompt, const std::string& role);

public:
    CustomRule_Impl(const CustomRule& data,
                    std::string systemrole = "You are a ai assistant made by Artiverse Studio.");
    std::string Submit(std::string prompt, size_t timeStamp, std::string role = Role::User,
                       std::string convid = "default", float temp = 0.7f,
                       float top_p = 0.9f,
                       uint32_t top_k = 40u,
                       float pres_pen = 0.0f,
                       float freq_pen = 0.0f, bool async = false) override;
    void Reset() override;
    void Load(std::string name) override;
    void Save(std::string name) override;
    void Del(std::string name) override;
    void Add(std::string name) override;
    map<long long, string> GetHistory() override;
    void BuildHistory(const std::vector<std::pair<std::string, std::string>>& history) override;


    std::string GetModel() override
    {
        return CustomRuleData.model;
    }
};


#endif //CUSTOMROLE_IMPL_H
