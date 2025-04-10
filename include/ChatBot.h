#ifndef CHATBOT_H
#define CHATBOT_H

#include "VoiceToText.h"
#include "utils.h"
#include <llama.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

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
                               std::string convid = "default", bool async = false) = 0;

    void SubmitAsync(std::string prompt, size_t timeStamp, std::string role = Role::User,
                     std::string convid = "default")
    {
        {
            std::lock_guard<std::mutex> lock(forceStopMutex);
            forceStop = false;
        }
        lastFinalResponse = "";
        std::get<1>(Response[timeStamp]) = false;
        std::thread([=] { Submit(prompt, timeStamp, role, convid, true); }).detach();
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

protected:
    long long lastTimeStamp = 0;
    std::mutex fileAccessMutex;
    std::mutex historyAccessMutex;
    std::string lastFinalResponse;
    std::unordered_map<size_t, std::tuple<std::string, bool>> Response; //ts,response,finished

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
