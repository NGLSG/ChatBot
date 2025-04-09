#ifndef CHATGPT_H
#define CHATGPT_H

#include "ChatBot.h"

class ChatGPT : public ChatBot
{
public:
    ChatGPT(std::string systemrole);

    ChatGPT(const OpenAIBotCreateInfo& chat_data, std::string systemrole = "");

    std::string
    Submit(std::string prompt, size_t timeStamp, std::string role = Role::User,
           std::string convid = "defult") override;

    void Reset() override;

    void Load(std::string name = "default") override;

    void Save(std::string name = "default") override;

    void Del(std::string name) override;

    void Add(std::string name) override;

    map<long long, string> GetHistory() override;

    static std::string Stamp2Time(long long timestamp);

    json history;

protected:
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

    bool IsSaved();

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

class Mistral : public ChatGPT
{
public:
    // 构造函数，接收 GPTLikeCreateInfo 并配置为 Mistral API
    Mistral(const GPTLikeCreateInfo& data, std::string systemrole = ""): ChatGPT(systemrole)
    {
        // 设置基本参数
        chat_data_.enable = data.enable;
        chat_data_.api_key = data.api_key;
        chat_data_.model = data.model;
        chat_data_.useLocalModel = false;
        chat_data_.useWebProxy = true;

        // 设置 Mistral API 端点
        chat_data_._endPoint = "https://api.mistral.ai/v1/chat/completions";
    }
};

class TongyiQianwen : public ChatGPT
{
public:
    TongyiQianwen(const GPTLikeCreateInfo& data, std::string systemrole = ""): ChatGPT(systemrole)
    {
        // 设置基本配置参数
        chat_data_.enable = data.enable;
        chat_data_.api_key = data.api_key;
        chat_data_.model = data.model;
        chat_data_.useLocalModel = false;
        chat_data_.useWebProxy = true;

        // 设置通义千问API端点
        chat_data_._endPoint = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";

        // 如果用户提供了自定义端点，则使用自定义端点
        if (!data.apiHost.empty())
        {
            chat_data_._endPoint = data.apiHost;
        }
    }
};

class SparkDesk : public ChatGPT
{
public:
    SparkDesk(const GPTLikeCreateInfo& data, std::string systemrole = ""): ChatGPT(systemrole)
    {
        // 设置基本配置参数
        chat_data_.enable = data.enable;
        chat_data_.api_key = data.api_key;
        chat_data_.model = data.model;
        chat_data_.useLocalModel = false;
        chat_data_.useWebProxy = true;

        // 设置星火API端点
        chat_data_._endPoint = "https://spark-api-open.xf-yun.com/v1/chat/completions";

        // 如果用户提供了自定义端点，则使用自定义端点
        if (!data.apiHost.empty())
        {
            chat_data_._endPoint = data.apiHost;
        }
    }
};

class BaichuanAI : public ChatGPT
{
public:
    /**
     * 构造函数
     * @param data 百川API配置信息
     * @param systemrole 系统角色提示信息
     */
    BaichuanAI(const GPTLikeCreateInfo& data, std::string systemrole = ""): ChatGPT(systemrole)
    {
        // 设置基本配置参数
        chat_data_.enable = data.enable;
        chat_data_.api_key = data.api_key;
        chat_data_.model = data.model;
        chat_data_.useLocalModel = false;
        chat_data_.useWebProxy = true;

        // 设置百川API端点
        chat_data_._endPoint = "https://api.baichuan-ai.com/v1/chat/completions";

        // 如果用户提供了自定义端点，则使用自定义端点
        if (!data.apiHost.empty())
        {
            chat_data_._endPoint = data.apiHost;
        }
    }
};

class HunyuanAI : public ChatGPT
{
public:
    /**
     * 构造函数
     * @param data 混元API配置信息
     * @param systemrole 系统角色提示信息
     */
    HunyuanAI(const GPTLikeCreateInfo& data, std::string systemrole = ""): ChatGPT(systemrole)
    {
        // 设置基本配置参数
        chat_data_.enable = data.enable;
        chat_data_.api_key = data.api_key;
        chat_data_.model = data.model;
        chat_data_.useLocalModel = false;
        chat_data_.useWebProxy = true;

        // 设置腾讯混元API端点
        chat_data_._endPoint = "https://api.hunyuan.cloud.tencent.com/v1/chat/completions";

        // 如果用户提供了自定义端点，则使用自定义端点
        if (!data.apiHost.empty())
        {
            chat_data_._endPoint = data.apiHost;
        }
    }
};
class HuoshanAI : public ChatGPT
{
public:
    /**
     * 构造函数
     * @param data 火山API配置信息
     * @param systemrole 系统角色提示信息
     */
    HuoshanAI(const GPTLikeCreateInfo& data, std::string systemrole = ""): ChatGPT(systemrole)
    {
        // 设置基本配置参数
        chat_data_.enable = data.enable;
        chat_data_.api_key = data.api_key;
        chat_data_.model = data.model;
        chat_data_.useLocalModel = false;
        chat_data_.useWebProxy = true;

        // 设置火山API端点
        chat_data_._endPoint = "https://ark.cn-beijing.volces.com/api/v3/chat/completions";

        // 如果用户提供了自定义端点，则使用自定义端点
        if (!data.apiHost.empty())
        {
            chat_data_._endPoint = data.apiHost;
        }
    }
};

class ChatGLM : public ChatGPT
{
public:
    /**
     * 构造函数
     * @param data ChatGLM API配置信息
     * @param systemrole 系统角色提示信息
     */
    ChatGLM(const GPTLikeCreateInfo& data, std::string systemrole = ""): ChatGPT(systemrole)
    {
        // 设置基本配置参数
        chat_data_.enable = data.enable;
        chat_data_.api_key = data.api_key;
        chat_data_.model = data.model;
        chat_data_.useLocalModel = false;
        chat_data_.useWebProxy = true;

        // 设置ChatGLM API端点
        chat_data_._endPoint = "https://open.bigmodel.cn/api/paas/v4/chat/completions";

        // 如果用户提供了自定义端点，则使用自定义端点
        if (!data.apiHost.empty())
        {
            chat_data_._endPoint = data.apiHost;
        }
    }
};


#endif //CHATGPT_H
