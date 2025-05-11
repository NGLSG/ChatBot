#ifndef CHATBOT_H
#define CHATBOT_H

#include "Utils.h"
#include <llama.h>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "Configure.h"
#include "Logger.h"
#include <curl/curl.h>
#include <fstream>
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

struct DString
{
    std::string* str1;
    std::string* str2;
    std::string* response;

    void* instance;
    std::mutex mtx;
};

class ChatBot
{
public:
    friend class StringExecutor;
    virtual std::string Submit(std::string prompt, size_t timeStamp, std::string role = Role::User,
                               std::string convid = "default", float temp = 0.7f,
                               float top_p = 0.9f,
                               uint32_t top_k = 40u,
                               float pres_pen = 0.0f,
                               float freq_pen = 0.0f, bool async = false) = 0;

    virtual void BuildHistory(const std::vector<std::pair<std::string, std::string>>& history) =0;
    virtual std::string GetModel() =0;

    void SubmitAsync(
        std::string prompt,
        size_t timeStamp,
        std::string role = Role::User,
        std::string convid = "default",
        float temp = 0.7f,
        float top_p = 0.9f,
        uint32_t top_k = 40u,
        float pres_pen = 0.0f,
        float freq_pen = 0.0f)
    {
        {
            std::lock_guard<std::mutex> lock(forceStopMutex);
            forceStop = false;
        }
        lastFinalResponse = "";
        std::get<1>(Response[timeStamp]) = false;
        std::thread([=]
        {
            Submit(prompt, timeStamp, role, convid, temp, top_p, top_k, pres_pen, freq_pen, true);
        }).detach();
    }


    std::string GetLastRawResponse()
    {
        std::string response = lastRawResponse;
        lastRawResponse = "";
        return response;
    }

    void ForceStop()
    {
        std::lock_guard<std::mutex> lock(forceStopMutex);
        forceStop = true;
    }

    virtual void Reset() = 0;

    virtual void Load(std::string name = "default") = 0;

    virtual void Save(std::string name = "default") = 0;

    virtual void Del(std::string name) = 0;

    virtual void Add(std::string name) = 0;


    std::string GetResponse(size_t uid)
    {
        std::string response;
        response = std::get<0>(Response[uid]);
        lastFinalResponse += response;
        std::get<0>(Response[uid]) = "";
        return response;
    }

    bool Finished(size_t uid)
    {
        return std::get<1>(Response[uid]);
    }

    virtual map<long long, string> GetHistory() = 0;


    map<long long, string> History;
    std::atomic<bool> forceStop{false};
    virtual std::string sendRequest(std::string data, size_t ts) =0;

protected:
    long long lastTimeStamp = 0;
    std::mutex fileAccessMutex;
    std::mutex historyAccessMutex;
    std::string lastFinalResponse;
    std::string lastRawResponse;
    unordered_map<size_t, std::tuple<std::string, bool>> Response; //ts,response,finished

    std::mutex forceStopMutex;
};

inline static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                                   curl_off_t ulnow)
{
    ChatBot* chatBot = static_cast<ChatBot*>(clientp);
    if (chatBot && chatBot->forceStop.load())
    {
        return 1;
    }
    return 0;
}

#endif
