#ifndef CLAUDE_IMPL_H
#define CLAUDE_IMPL_H

#include "ChatBot.h"

class ClaudeInSlack : public ChatBot
{
public:
    ClaudeInSlack(const ClaudeBotCreateInfo& data) : claudeData(data)
    {
    }

    std::string Submit(std::string prompt, size_t timeStamp, std::string role = Role::User,
                       std::string convid = "default", float temp = 0.7f,
                       float top_p = 0.9f,
                       uint32_t top_k = 40u,
                       float pres_pen = 0.0f,
                       float freq_pen = 0.0f, bool async = false) override;

    void Reset() override;;

    void Load(std::string name) override;

    void Save(std::string name) override;

    void Del(std::string id) override;

    void Add(std::string name) override;

    map<long long, string> GetHistory() override;

    std::string sendRequest(std::string data, size_t ts) override
    {
        return "";
    }

    std::string GetModel() override
    {
        return "Claude";
    }

private:
    map<string, string> ChannelListName;
    map<string, string> ChannelListID;
    ClaudeBotCreateInfo claudeData;
};

class Claude : public ChatBot
{
public:
    // 构造函数 - 带系统角色提示
    Claude(std::string systemrole);

    // 构造函数 - 带 Claude API 配置信息
    Claude(const ClaudeAPICreateInfo& claude_data, std::string systemrole = "");

    // 提交用户消息并获取响应
    std::string Submit(std::string prompt, size_t timeStamp, std::string role = Role::User,
                       std::string convid = "default", float temp = 0.7f,
                       float top_p = 0.9f,
                       uint32_t top_k = 40u,
                       float pres_pen = 0.0f,
                       float freq_pen = 0.0f, bool async = false) override;

    // 重置当前对话
    void Reset() override;

    // 加载指定对话
    void Load(std::string name) override;

    // 保存当前对话
    void Save(std::string name) override;

    // 删除指定对话
    void Del(std::string name) override;

    // 添加新对话
    void Add(std::string name) override;

    // 获取历史记录
    map<long long, string> GetHistory() override;

    // 时间戳转换为可读时间
    static std::string Stamp2Time(long long timestamp);
    // 发送请求到 Claude API
    std::string sendRequest(std::string data, size_t ts) override;
    // 历史记录
    json history;

protected:
    ClaudeAPICreateInfo claude_data_;
    std::string mode_name_ = "default";
    std::string convid_ = "default";
    std::map<std::string, json> Conversation;
    std::mutex print_mutex;
    const std::string ConversationPath = "Conversations/Claude/";
    const std::string sys = "You are Claude, an AI assistant developed by Anthropic. Please respond in Chinese.";
    const std::string suffix = ".dat";
    json LastHistory;
    json defaultJson;

    // 检查是否已保存
    bool IsSaved();

    // 获取当前时间戳
    static long long getCurrentTimestamp();

    // 获取指定天数前的时间戳
    static long long getTimestampBefore(int daysBefore);

public:
    void BuildHistory(const std::vector<std::pair<std::string, std::string>>& history) override;

    std::string GetModel() override
    {
        return claude_data_.model;
    }
};


#endif //CLAUDE_IMPL_H
