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
    ChatBot(const OpenAIData &chat_data);

    std::string
    Submit(std::string prompt, std::string role = Role::User, std::string convid = "defult");

    std::future<std::string> SubmitAsync(std::string prompt, std::string role, std::string convid);

    void Reset();

    void Load(std::string name = "default");

    void Save(std::string name = "default");

    void Del(std::string name);

    void Add(std::string name);

    Billing GetBilling() {
        std::string url;
        Billing billing;

        if (!chat_data_.useWebProxy) {
            url = "https://api.openai.com/";
            if (!chat_data_.proxy.empty()) {
                session.SetProxies(cpr::Proxies{
                        {"http",  chat_data_.proxy},
                        {"https", chat_data_.proxy}
                });
            }
        } else {
            url = WebProxies[chat_data_.webproxy];
        }

        auto t1 = std::async(std::launch::async, [&] { // execute the first API request asynchronously
            cpr::Session session;
            session.SetUrl(cpr::Url{url + "v1/dashboard/billing/subscription"});
            session.SetHeader(cpr::Header{
                    {"Authorization", "Bearer " + chat_data_.api_key},
            });
            session.SetVerifySsl(cpr::VerifySsl{false});

            auto response = session.Get();
            if (response.status_code != 200) {
                LogError("OpenAI Error: Request failed with status code " + std::to_string(response.status_code));
                return;
            }

            json data = json::parse(response.text);
            billing.total = data["system_hard_limit_usd"];
            billing.date = data["access_until"];
        });

        auto t2 = std::async(std::launch::async, [&] { // execute the second API request asynchronously
            cpr::Session session;
            string start = Stamp2Time(getTimestampBefore(100));
            string end = Stamp2Time(getCurrentTimestamp());
            url = url + "v1/dashboard/billing/usage?start_date=" + start + "&end_date=" + end;
            session.SetUrl(cpr::Url{url});
            session.SetHeader(cpr::Header{
                    {"Authorization", "Bearer " + chat_data_.api_key}
            });
            session.SetVerifySsl(cpr::VerifySsl{false});

            auto response = session.Get();
            if (response.status_code != 200) {
                LogError("OpenAI Error: Request failed with status code " + std::to_string(response.status_code));
                return;
            }

            json data = json::parse(response.text);
            billing.used = float(data["total_usage"]) / 100;
            billing.available = billing.total - billing.used;
        });

        t1.wait(); // wait for both tasks to finish
        t2.wait();

        return billing;
    }

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
    const std::string sys = "You are ChatGPT, a large language model trained by OpenAI. Respond conversationally";
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
};

#endif