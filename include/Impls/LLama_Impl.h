#ifndef LLAMA_IMPL_H
#define LLAMA_IMPL_H


#include "ChatBot.h"

class LLama : public ChatBot
{
public:
    LLama(const LLamaCreateInfo& data,
          const std::string& sysr =
              "You are LLama, a large language model trained by OpenSource. Respond conversationally.");

    ~LLama();

    std::string Submit(std::string prompt, size_t timeStamp, std::string role = Role::User,
                                   std::string convid = "default", float temp = 0.7f,
                                   float top_p = 0.9f,
                                   uint32_t top_k = 40u,
                                   float pres_pen = 0.0f,
                                   float freq_pen = 0.0f, bool async = false)override;
    void Reset() override;
    void Load(std::string name) override;
    void Save(std::string name) override;
    void Del(std::string name) override;
    void Add(std::string name) override;
    map<long long, string> GetHistory() override;
    std::string sendRequest(std::string data, size_t ts) override;

private:
    struct ChatMessage
    {
        std::string role;
        std::string content;

        llama_chat_message To();
    };


    struct chatInfo
    {
        std::vector<ChatMessage> messages;
        int prev_len = 0;

        std::vector<llama_chat_message> To();
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

    static uint16_t GetGPUMemory();

    static uint16_t GetGPULayer();

public:
    void BuildHistory(const std::vector<std::pair<std::string, std::string>>& history) override;
    std::string GetModel() override
    {
        return llamaData.model;
    }
};


#endif //LLAMA_IMPL_H
