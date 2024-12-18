#ifndef CHATBOT_H
#define CHATBOT_H

#include "VoiceToText.h"
#include "utils.h"

using json = nlohmann::json;
using namespace std;

namespace Role
{
    static std::string User = "user";
    static std::string System = "system";
    static std::string Assistant = "assistant";
};

struct Billing
{
    float total = -1;
    float available = -1;
    float used = -1;
    long long date = -1;
};

class ChatBot
{
public:
    virtual std::string Submit(std::string prompt, std::string role = Role::User, std::string convid = "defult") = 0;

    virtual void Reset() = 0;

    virtual void Load(std::string name = "default") = 0;

    virtual void Save(std::string name = "default") = 0;

    virtual void Del(std::string name) = 0;

    virtual void Add(std::string name) = 0;

    virtual Billing GetBilling() = 0;

    virtual map<long long, string> GetHistory() = 0;

    map<long long, string> History;
};

class ChatGPT : public ChatBot
{
public:
    ChatGPT(std::string systemrole)
    {
        Logger::Init();
        if (!systemrole.empty())
            defaultJson["content"] = systemrole;
        else
            defaultJson["content"] = sys;
        defaultJson["role"] = "system";


        defaultJson2["content"] = defaultJson["content"];
        defaultJson2["role"] = "user";


        if (!UDirectory::Exists(ConversationPath))
        {
            UDirectory::Create(ConversationPath);
            Add("default");
        }
    }

    ChatGPT(const OpenAIBotCreateInfo& chat_data, std::string systemrole = "");

    std::string
    Submit(std::string prompt, std::string role = Role::User, std::string convid = "defult") override;

    std::future<std::string> SubmitAsync(std::string prompt, std::string role, std::string convid);

    void Reset() override;

    void Load(std::string name = "default") override;

    void Save(std::string name = "default") override;

    void Del(std::string name) override;

    void Add(std::string name) override;

    Billing GetBilling() override;

    map<long long, string> GetHistory() override { return map<long long, string>(); }

    static std::string Stamp2Time(long long timestamp)
    {
        time_t tick = (time_t)(timestamp / 1000); //转换时间
        tm tm;
        char s[40];
        tm = *localtime(&tick);
        strftime(s, sizeof(s), "%Y-%m-%d", &tm);
        std::string str(s);
        return str;
    }

    json history;

protected:
    cpr::Session session;
    OpenAIBotCreateInfo chat_data_;
    std::string mode_name_ = "default";
    std::string convid_ = "default";
    std::map<std::string, json> Conversation;
    std::mutex print_mutex;
    const std::string ConversationPath = "Conversations/";
    const std::string sys = "You are ChatGPT, a large language model trained by OpenAI. Respond conversationally.";
    const std::string suffix = ".dat";
    const std::vector<std::string> WebProxies{

    };
    json LastHistory;
    json defaultJson;
    json defaultJson2;

    bool IsSaved()
    {
        return LastHistory == history;
    }

    static long long getCurrentTimestamp();

    static long long getTimestampBefore(const int daysBefore);

    string sendRequest(std::string data);

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

class GPTLike : public ChatGPT
{
public:
    GPTLike(const GPTLikeCreateInfo& data, std::string systemrole = ""): ChatGPT(systemrole)
    {
        chat_data_.enable = data.enable;
        chat_data_.api_key = data.api_key;
        chat_data_.model = data.model;
        chat_data_.useLocalModel = false;
        chat_data_.useWebProxy = true;
        chat_data_._endPoint = data.apiEndPoint;
    }
};

class Grok : public ChatGPT
{
public:
    Grok(const GPTLikeCreateInfo& data, std::string systemrole = ""): ChatGPT(systemrole)
    {
        chat_data_.enable = data.enable;
        chat_data_.api_key = data.api_key;
        chat_data_.model = data.model;
        chat_data_.useLocalModel = false;
        chat_data_.useWebProxy = true;
        chat_data_._endPoint = "https://api.x.ai/v1/chat/completions";
    }
};

class Claude : public ChatBot
{
public:
    Claude(const ClaudeBotCreateInfo& data) : claudeData(data)
    {
    }

    std::string
    Submit(std::string text, std::string role = Role::User, std::string convid = "defult") override;

    void Reset() override;;

    void Load(std::string name = "default") override;

    void Save(std::string name = "default") override;

    void Del(std::string id) override;

    void Add(std::string name = "default") override;

    Billing GetBilling() override;;

    map<long long, string> GetHistory() override;;

private:
    map<string, string> ChannelListName;
    map<string, string> ChannelListID;
    ClaudeBotCreateInfo claudeData;
};

class Gemini : public ChatBot
{
public:
    Gemini(const GeminiBotCreateInfo& data) : geminiData(data)
    {
    }

    std::string
    Submit(std::string prompt, std::string role = Role::User, std::string convid = "defult") override;

    std::future<std::string> SubmitAsync(std::string prompt, std::string role, std::string convid)
    {
    };

    void Reset() override;

    void Load(std::string name = "default") override;

    void Save(std::string name = "default") override;

    void Del(std::string name) override;

    void Add(std::string name) override;

    Billing GetBilling() override { return Billing(); };

    map<long long, string> GetHistory() override { return map<long long, string>(); };

private:
    GeminiBotCreateInfo geminiData;
    const std::string ConversationPath = "Conversations/Gemini/";
    const std::string suffix = ".dat";
    std::string convid_ = "default";
    std::map<std::string, json> Conversation;
    json history;
};

/*
class Claude2 {
public:
    Claude2(const std::string &sessionKey) : sessionKey_(sessionKey) {}

    std::string startConversation(const std::string &message) {
        // 1. 开始新对话
        cpr::Response r = cpr::Post(cpr::Url{"https://api.claude.ai/conversation"},
                                    cpr::Body{{"message", message}},
                                    cpr::Header{{"Content-Type", "application/json"},
                                                {"Cookie",       "sessionKey=" + sessionKey_}});

        // 2. 获取对话ID,使用 nlohmann::json 解析
        json response = json::parse(r.text);
        std::string conversationId = response["conversationId"];


        // 3. 发送消息
        r = cpr::Post(cpr::Url{"https://api.claude.ai/conversation/" + conversationId},
                      cpr::Body{{"message", message}},
                      cpr::Header{{"Content-Type", "application/json"},
                                  {"Cookie",       "sessionKey=" + sessionKey_}});

        // 4. 获取回复
        std::string reply = r.text;

        return reply;
    }

private:
    std::string sessionKey_;
};
*/


#endif
