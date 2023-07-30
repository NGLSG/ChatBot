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

struct Billing {
    float total = -1;
    float available = -1;
    float used = -1;
    long long date = -1;
};

class ChatBot {
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

class ChatGPT : public ChatBot {

public:

    ChatGPT(const OpenAIData &chat_data);

    virtual std::string
    Submit(std::string prompt, std::string role = Role::User, std::string convid = "defult") override;

    std::future<std::string> SubmitAsync(std::string prompt, std::string role, std::string convid);

    virtual void Reset() override;

    virtual void Load(std::string name = "default") override;

    virtual void Save(std::string name = "default") override;

    virtual void Del(std::string name) override;

    virtual void Add(std::string name) override;

    virtual Billing GetBilling() override;

    virtual map<long long, string> GetHistory() override { return map<long long, string>(); }

    std::string Stamp2Time(long long timestamp) {
        time_t tick = (time_t) (timestamp / 1000);//转换时间
        struct tm tm;
        char s[40];
        tm = *localtime(&tick);
        strftime(s, sizeof(s), "%Y-%m-%d", &tm);
        std::string str(s);
        return str;
    }

    json history;
private:

    cpr::Session session;
    OpenAIData chat_data_;
    std::string mode_name_ = "default";
    std::string convid_ = "default";
    std::map<std::string, json> Conversation;
    std::mutex print_mutex;
    const std::string ConversationPath = "Conversations/";
    const std::string sys = "You are ChatGPT, a large language model trained by OpenAI. Respond conversationally.";
    const std::string suffix = ".dat";
    const std::vector<std::string> WebProxies{"https://nglsg.ml/",
                                              "https://service-hbv9ql2m-1306800451.sg.apigw.tencentcs.com/"};
    json LastHistory;
    json defaultJson;

    bool IsSaved() {
        return LastHistory == history;
    }

    long long getCurrentTimestamp() {
        auto currentTime = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count();
    }

    long long getTimestampBefore(const int daysBefore) {
        auto currentTime = std::chrono::system_clock::now();
        auto days = std::chrono::hours(24 * daysBefore);
        auto targetTime = currentTime - days;
        return std::chrono::duration_cast<std::chrono::milliseconds>(targetTime.time_since_epoch()).count();
    }

    string sendRequest(std::string data);

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

};


class Claude : public ChatBot {
public:
    Claude(const ClaudeData &data) : claudeData(data) {
    }

    virtual std::string
    Submit(std::string text, std::string role = Role::User, std::string convid = "defult") override {
        cpr::Payload payload{
                {"token",   claudeData.slackToken},
                {"channel", claudeData.channelID},
                {"text",    text}};

        cpr::Response r = cpr::Post(cpr::Url{"https://slack.com/api/chat.postMessage"},
                                    payload);
        if (r.status_code == 200) {
            // 发送成功
        } else {
            // 发送失败,打印错误信息
            LogError(r.error.message);
        }
        json response = json::parse(r.text);

        return "";
    }

    virtual void Reset() override {
/*        cpr::Payload payload{
                {"token",   claudeData.slackToken},
                {"channel", claudeData.channelID},
                {"command", "/reset"}};
        cpr::Header header{{"Cookie", claudeData.cookies}};
        std::string url = "https://" + claudeData.userName + ".slack.com/api/chat.command";
        cpr::Response r = cpr::Post(cpr::Url{url},
                                    payload, header);
        if (r.status_code == 200) {
            // 发送成功
        } else {
            // 发送失败,打印错误信息
            LogError(r.error.message);
        }*/
        Submit("请忘记上面的会话内容");
        LogInfo("Claude : 重置成功");
    };

    virtual void Load(std::string name = "default") override {
        LogInfo("Claude : 不支持的操作");
    }

    virtual void Save(std::string name = "default") override {
        LogInfo("Claude : 不支持的操作");
    }

    virtual void Del(std::string id) override {

        LogInfo("Claude : 不支持的操作");
    }

    virtual void Add(std::string name = "default") override {
        LogInfo("Claude : 不支持的操作");
    }

    virtual Billing GetBilling() override {
        return {999, 999, 0, Utils::getCurrentTimestamp()};
    };

    virtual map<long long, string> GetHistory() override {
        try {
            History.clear();
            auto _ts = to_string(Logger::getCurrentTimestamp());
            cpr::Payload payload = {
                    {"channel", claudeData.channelID},
                    {"latest",  _ts},
                    {"token",   claudeData.slackToken}
            };
            cpr::Response r2 = cpr::Post(cpr::Url{"https://slack.com/api/conversations.history"},
                                         payload);
            json j = json::parse(r2.text);
            if (j["ok"].get<bool>()) {
                for (auto &message: j["messages"]) {
                    if (message["bot_profile"]["name"] == "Claude") {

                        long long ts = (long long) (atof(message["ts"].get<string>().c_str()) * 1000);
                        std::string text = message["blocks"][0]["elements"][0]["elements"][0]["text"].get<string>();

                        History[ts] = text;
                    }
                }
            }
        } catch (const std::exception &e) {
            // 捕获并处理异常
            LogError("获取历史记录失败:" + string(e.what()));
        }
        return History;
    };
private:
    map <string, string> ChannelListName;
    map <string, string> ChannelListID;
    ClaudeData claudeData;
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