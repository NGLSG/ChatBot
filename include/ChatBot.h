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

class ChatBot
{
public:
    virtual std::string Submit(std::string prompt, size_t timeStamp, std::string role = Role::User,
                               std::string convid = "default") = 0;

    void SubmitAsync(std::string prompt, size_t timeStamp, std::string role = Role::User,
                     std::string convid = "default")
    {
        std::thread([=] { Submit(prompt, timeStamp, role, convid); }).detach();
    }

    virtual void Reset() = 0;

    virtual void Load(std::string name = "default") = 0;

    virtual void Save(std::string name = "default") = 0;

    virtual void Del(std::string name) = 0;

    virtual void Add(std::string name) = 0;

    std::string GetResponse(size_t uid)
    {
        return std::get<0>(Response[uid]);
    }

    bool Finished(size_t uid)
    {
        return std::get<1>(Response[uid]);
    }

    virtual map<long long, string> GetHistory() = 0;


    map<long long, string> History;

protected:
    std::mutex fileAccessMutex;
    std::mutex historyAccessMutex;
    std::unordered_map<size_t, std::tuple<std::string, bool>> Response; //ts,response,finished
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
    Submit(std::string prompt, size_t timeStamp, std::string role = Role::User,
           std::string convid = "defult") override;

    void Reset() override;

    void Load(std::string name = "default") override;

    void Save(std::string name = "default") override;

    void Del(std::string name) override;

    void Add(std::string name) override;

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

    std::string sendRequest(std::string data, size_t ts);
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
        chat_data_._endPoint = data.apiHost + data.apiPath;
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
    Submit(std::string text, size_t timeStamp, std::string role = Role::User,
           std::string convid = "defult") override;

    void Reset() override;;

    void Load(std::string name = "default") override;

    void Save(std::string name = "default") override;

    void Del(std::string id) override;

    void Add(std::string name = "default") override;

    map<long long, string> GetHistory() override;

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
    Submit(std::string prompt, size_t timeStamp, std::string role = Role::User,
           std::string convid = "defult") override;

    std::future<std::string> SubmitAsync(std::string prompt, std::string role, std::string convid)
    {
    };

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

class LLama : public ChatBot
{
public:
    LLama(const LLamaCreateInfo& data,
          const std::string& sysr =
              "You are LLama, a large language model trained by OpenSource. Respond conversationally.");

    ~LLama()
    {
        try
        {
            if (model)
                llama_model_free(model);
            if (ctx)
                llama_free(ctx);
        }
        catch (...)
        {
        }
    }

    std::string Submit(std::string prompt, size_t timeStamp, std::string role, std::string convid) override;
    void Reset() override;
    void Load(std::string name) override;
    void Save(std::string name) override;
    void Del(std::string name) override;
    void Add(std::string name) override;
    map<long long, string> GetHistory() override;

private:
    struct ChatMessage
    {
        std::string role;
        std::string content;

        llama_chat_message To()
        {
            return {role.c_str(), _strdup(content.c_str())};
        }
    };


    struct chatInfo
    {
        std::vector<ChatMessage> messages;
        int prev_len = 0;

        std::vector<llama_chat_message> To()
        {
            std::vector<llama_chat_message> msgs;
            for (auto& m : messages)
            {
                msgs.push_back(m.To());
            }
            return msgs;
        }
    };

    LLamaCreateInfo llamaData;
    llama_context* ctx;
    llama_model* model;
    llama_sampler* smpl;
    std::string convid_ = "default";
    const llama_vocab* vocab;
    std::vector<char> formatted;
    std::string sys = "You are LLama, a large language model trained by OpenSource. Respond conversationally.";
    const std::string ConversationPath = "Conversations/";
    const std::string suffix = ".dat";
    std::unordered_map<std::string, chatInfo> history;

    static uint16_t GetGPUMemory()
    {
        // Create Vulkan application info
        vk::ApplicationInfo appInfo(
            "GPU Memory Query", VK_MAKE_VERSION(1, 0, 0),
            "No Engine", VK_MAKE_VERSION(1, 0, 0),
            VK_API_VERSION_1_0
        );

        // Create Vulkan instance
        vk::InstanceCreateInfo instanceCreateInfo({}, &appInfo);
        vk::Instance instance = vk::createInstance(instanceCreateInfo);

        // Enumerate physical devices
        std::vector<vk::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
        if (physicalDevices.empty())
        {
            LogError("No GPU physical devices found");
            return -1;
        }

        // Use the first (default) physical device
        vk::PhysicalDevice physicalDevice = physicalDevices[0];

        // Get properties of the default physical device
        vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();
        std::cout << "Device Name: " << deviceProperties.deviceName << std::endl;

        // Retrieve memory properties for the default device
        vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

        vk::DeviceSize totalLocalMemory = 0;
        // Sum up all heaps flagged as device local (GPU memory)
        for (const auto& heap : memoryProperties.memoryHeaps)
        {
            if (heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
            {
                totalLocalMemory += heap.size;
            }
        }
        std::cout << "Total GPU Memory: " << totalLocalMemory / (1024 * 1024) << " MB" << std::endl;

        // Cleanup Vulkan instance
        instance.destroy();

        return static_cast<uint16_t>(totalLocalMemory / (1024 * 1024));
    }

    static uint16_t GetGPULayer()
    {
        //获取当前显卡的显存
        uint16_t gpu_memory = GetGPUMemory();
        if (gpu_memory >= 24000) //24GB
        {
            return 60;
        }
        else if (gpu_memory >= 16000) //16GB
        {
            return 40;
        }
        else if (gpu_memory >= 8000) //8GB
        {
            return 20;
        }
        else if (gpu_memory >= 4000) //4GB
        {
            return 10;
        }
        else if (gpu_memory >= 2000) //2GB
        {
            return 5;
        }
        else
        {
            return 0;
        }
    }
};


#endif
