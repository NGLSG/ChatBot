#include "Impls/LLama_Impl.h"


#include <llama-context.h>
#include <llama-sampling.h>

llama_chat_message LLama::ChatMessage::To()
{
    return {role.c_str(), _strdup(content.c_str())};
}

std::vector<llama_chat_message> LLama::chatInfo::To()
{
    std::vector<llama_chat_message> msgs;
    for (auto& m : messages)
    {
        msgs.push_back(m.To());
    }
    return msgs;
}

uint16_t LLama::GetGPUMemory()
{
    vk::ApplicationInfo appInfo(
        "GPU Memory Query", VK_MAKE_VERSION(1, 0, 0),
        "No Engine", VK_MAKE_VERSION(1, 0, 0),
        VK_API_VERSION_1_0
    );

    vk::InstanceCreateInfo instanceCreateInfo({}, &appInfo);
    vk::Instance instance = vk::createInstance(instanceCreateInfo);

    std::vector<vk::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
    if (physicalDevices.empty())
    {
        LogError("No GPU physical devices found");
        return -1;
    }

    vk::PhysicalDevice physicalDevice = physicalDevices[0];

    vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();
    std::cout << "Device Name: " << deviceProperties.deviceName << std::endl;

    vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

    vk::DeviceSize totalLocalMemory = 0;
    for (const auto& heap : memoryProperties.memoryHeaps)
    {
        if (heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
        {
            totalLocalMemory += heap.size;
        }
    }
    std::cout << "Total GPU Memory: " << totalLocalMemory / (1024 * 1024) << " MB" << std::endl;

    instance.destroy();

    return static_cast<uint16_t>(totalLocalMemory / (1024 * 1024));
}

uint16_t LLama::GetGPULayer()
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

LLama::LLama(const LLamaCreateInfo& data, const std::string& sysr): llamaData(data), sys(sysr)
{
    // 初始化后端
    ggml_backend_load_all();

    // 设置日志回调
    llama_log_set([](enum ggml_log_level level, const char* text, void* /* user_data */)
    {
        if (level >= GGML_LOG_LEVEL_ERROR)
        {
            LogError("{0}", text);
        }
    }, nullptr);

    // 初始化默认参数
    llama_model_params params = llama_model_default_params();
    llama_context_params context_params = llama_context_default_params();

    // 配置上下文参数
    context_params.n_threads = 8;
    context_params.n_ctx = llamaData.contextSize;
    context_params.n_batch = llamaData.maxTokens;
    context_params.no_perf = false;

    // 尝试使用CPU模式，避免GPU问题
    int gpu_layers = GetGPULayer();
    params.n_gpu_layers = gpu_layers;

    LogInfo("LLama: 尝试加载模型，GPU层数: {0}", gpu_layers);

    // 检查文件是否存在
    if (!UFile::Exists(llamaData.model))
    {
        LogError("LLama: 模型文件未找到: {0}", llamaData.model);
        return;
    }

    // 检查文件权限
    FILE* test = fopen(llamaData.model.c_str(), "rb");
    if (!test)
    {
        LogError("LLama: 无法打开模型文件，请检查读取权限: {0}", strerror(errno));
        return;
    }
    fclose(test);

    // 尝试加载模型
    try
    {
        // 先尝试加载模型
        LogInfo("LLama: 开始加载模型文件 {0}", llamaData.model);
        model = llama_model_load_from_file(llamaData.model.c_str(), params);

        if (model == nullptr)
        {
            LogError("LLama: 模型加载失败！尝试降低GPU层数重新加载");
            // 尝试完全CPU模式
            params.n_gpu_layers = 0;
            model = llama_model_load_from_file(llamaData.model.c_str(), params);
        }

        if (model == nullptr)
        {
            LogError("LLama: 模型加载失败！尝试减小上下文大小");
            // 尝试减小上下文大小
            context_params.n_ctx = min(llamaData.contextSize, 2048);
            context_params.n_batch = min(llamaData.maxTokens, 512);
        }
        else
        {
            LogInfo("LLama: 模型加载成功！");
        }

        // 初始化上下文
        LogInfo("LLama: 开始初始化上下文");
        ctx = llama_init_from_model(model, context_params);

        if (ctx == nullptr)
        {
            LogError("LLama: 上下文初始化失败！可能是内存不足或参数设置不当");
        }
        else
        {
            LogInfo("LLama: 上下文初始化成功！");
        }
    }
    catch (const std::exception& e)
    {
        LogError("LLama: 加载模型过程中发生异常: {0}", e.what());
        return;
    }

    // 检查模型和上下文是否成功初始化
    if (model == nullptr || ctx == nullptr)
    {
        LogError("LLama: 模型或上下文初始化失败！");
        return;
    }

    // 初始化词汇表和采样器
    vocab = llama_model_get_vocab(model);
    llama_sampler_chain_params p = llama_sampler_chain_default_params();
    p.no_perf = true;
    smpl = llama_sampler_chain_init(p);
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
    formatted = std::vector<char>(llama_n_ctx(ctx));

    LogInfo("LLama: 初始化完成，上下文大小: {0}, 最大令牌数: {1}", llamaData.contextSize, llamaData.maxTokens);
}

std::string LLama::Submit(std::string prompt, size_t timeStamp, std::string role, std::string convid)
{
    // 检查上下文是否已初始化
    if (!ctx)
    {
        LogError("LLama: 上下文未初始化，无法处理请求");
        return "错误: LLama上下文未初始化。";
    }

    std::lock_guard<std::mutex> lock(historyAccessMutex);

    // 初始化对话历史
    if (!history.contains(convid))
    {
        LogInfo("LLama: 创建新的对话历史 ID: {0}", convid);
        history[convid].messages = std::vector<ChatMessage>();
        history[convid].messages.push_back({"system", sys});
        history[convid].prev_len = 0;
    }

    LogInfo("LLama: 开始生成响应");
    Response[timeStamp] = std::make_tuple("", false);

    // 应用聊天模板
    const char* tmpl = llama_model_chat_template(model, /* name */ nullptr);
    if (!tmpl)
    {
        LogWarn("LLama: 模型没有内置聊天模板，使用默认格式");
    }

    // 添加用户消息
    history[convid].messages.push_back({role, prompt});

    // 应用聊天模板
    int new_len = llama_chat_apply_template(tmpl, history[convid].To().data(),
                                            history[convid].To().size(), true, formatted.data(), formatted.size());
    if (new_len < 0)
    {
        LogError("LLama: 应用聊天模板失败");
        std::get<0>(Response[timeStamp]) = "错误: 应用聊天模板失败";
        std::get<1>(Response[timeStamp]) = true;
        return "错误: 应用聊天模板失败";
    }

    // 如果格式化后的长度超出缓冲区大小，则重新分配
    if (new_len > (int)formatted.size())
    {
        LogInfo("LLama: 重新分配格式化缓冲区大小为 {0}", new_len);
        formatted.resize(new_len);
        new_len = llama_chat_apply_template(tmpl, history[convid].To().data(),
                                            history[convid].To().size(), true, formatted.data(),
                                            formatted.size());
    }

    // 提取当前提示
    prompt = std::string(formatted.begin() + history[convid].prev_len, formatted.begin() + new_len);

    {
        // 检查是否要求强制停止
        {
            std::lock_guard<std::mutex> stopLock(forceStopMutex);
            if (forceStop)
            {
                LogInfo("LLama: 操作被用户取消");
                std::get<0>(Response[timeStamp]) = "操作已被取消";
                std::get<1>(Response[timeStamp]) = true;
                return "操作已被取消";
            }
        }

        // 对提示进行分词
        LogInfo("LLama: 对提示进行分词");
        int n_prompt = -llama_tokenize(vocab, prompt.c_str(), prompt.size(), NULL, 0, true, true);
        if (n_prompt <= 0)
        {
            LogError("LLama: 分词失败，返回的token数量为 {0}", n_prompt);
            std::get<0>(Response[timeStamp]) = "错误: 分词失败";
            std::get<1>(Response[timeStamp]) = true;
            return "错误: 分词失败";
        }

        std::vector<llama_token> prompt_tokens(n_prompt);
        if (llama_tokenize(vocab, prompt.c_str(), prompt.size(), prompt_tokens.data(), prompt_tokens.size(), true,
                           true) < 0)
        {
            LogError("LLama: 对提示进行分词失败");
            std::get<0>(Response[timeStamp]) = "错误: 对提示进行分词失败";
            std::get<1>(Response[timeStamp]) = true;
            return "错误: 对提示进行分词失败";
        }

        LogInfo("LLama: 提示分词完成，共 {0} 个token", n_prompt);

        // 再次检查是否要求强制停止
        {
            std::lock_guard<std::mutex> stopLock(forceStopMutex);
            if (forceStop)
            {
                LogInfo("LLama: 操作被用户取消");
                std::get<0>(Response[timeStamp]) = "操作已被取消";
                std::get<1>(Response[timeStamp]) = true;
                return "操作已被取消";
            }
        }

        // 创建批处理
        llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
        int n_batch = llama_n_batch(ctx);
        int error = 0;

        // 检查是否超出批处理大小
        if (batch.n_tokens > n_batch)
        {
            LogError("LLama: token数量超出批处理大小，请减少token数量或增加maxTokens大小");
            std::get<0>(Response[timeStamp]) = "错误: token数量超出批处理大小，请减少token数量或增加maxTokens大小";
            std::get<1>(Response[timeStamp]) = true;
            error = 1;
        }

        if (error == 0)
        {
            // 检查上下文大小
            int n_ctx = llama_n_ctx(ctx);
            int n_ctx_used = llama_get_kv_cache_used_cells(ctx);

            if (n_ctx_used + batch.n_tokens > n_ctx)
            {
                LogError("LLama: 上下文大小超出限制，请打开新会话或重置");
                std::get<0>(Response[timeStamp]) = "错误: 上下文大小超出限制，请打开新会话或重置";
                std::get<1>(Response[timeStamp]) = true;
                error = 3;
            }
            else
            {
                // 开始生成
                LogInfo("LLama: 开始解码生成文本");

                // 首先处理提示
                if (llama_decode(ctx, batch))
                {
                    LogError("LLama: 解码失败");
                    std::get<0>(Response[timeStamp]) = "错误: 解码失败";
                    std::get<1>(Response[timeStamp]) = true;
                    error = 2;
                }
                else
                {
                    // 逐个生成token
                    llama_token new_token_id;
                    while (true)
                    {
                        // 检查是否要求强制停止
                        {
                            std::lock_guard<std::mutex> stopLock(forceStopMutex);
                            if (forceStop)
                            {
                                LogInfo("LLama: 生成被强制中断");
                                std::get<0>(Response[timeStamp]) += "\n[生成被中断]";
                                std::get<1>(Response[timeStamp]) = true;
                                error = 5;
                                break;
                            }
                        }

                        // 采样新token
                        new_token_id = llama_sampler_sample(smpl, ctx, -1);

                        // 检查是否生成结束
                        if (llama_vocab_is_eog(vocab, new_token_id))
                        {
                            LogInfo("LLama: 生成完成，遇到结束标记");
                            break;
                        }

                        // 转换token为文本片段
                        char buf[256];
                        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
                        if (n < 0)
                        {
                            LogError("LLama: 转换token为文本片段失败");
                            std::get<0>(Response[timeStamp]) = "错误: 转换token为文本片段失败";
                            std::get<1>(Response[timeStamp]) = true;
                            error = 4;
                            break;
                        }

                        // 添加到结果中
                        std::string piece(buf, n);
                        std::get<0>(Response[timeStamp]) += piece;

                        // 创建新的批处理进行下一个token的生成
                        batch = llama_batch_get_one(&new_token_id, 1);
                        if (llama_decode(ctx, batch))
                        {
                            LogError("LLama: 解码失败");
                            std::get<0>(Response[timeStamp]) = "错误: 解码失败";
                            std::get<1>(Response[timeStamp]) = true;
                            error = 2;
                            break;
                        }
                    }
                }
            }
        }

        // 如果生成成功或被强制停止，更新聊天历史
        if (error == 0 || error == 5)
        {
            LogInfo("LLama: 生成完成，更新聊天历史");
            history[convid].messages.push_back({"assistant", std::get<0>(Response[timeStamp])});
            history[convid].prev_len = llama_chat_apply_template(tmpl, history[convid].To().data(),
                                                                 history[convid].To().size(), false,
                                                                 nullptr, 0);
            if (history[convid].prev_len < 0)
            {
                LogError("LLama: 应用聊天模板失败");
                std::get<0>(Response[timeStamp]) = "错误: 应用聊天模板失败";
                std::get<1>(Response[timeStamp]) = true;
                return "错误: 应用聊天模板失败";
            }
        }
    }

    // 等待响应被处理完成
    while (!std::get<0>(Response[timeStamp]).empty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::get<1>(Response[timeStamp]) = true;
    return std::get<0>(Response[timeStamp]);
}

LLama::~LLama()
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

void LLama::Reset()
{
    history[convid_].messages.clear();
    history[convid_].messages.push_back({"system", sys.c_str()});
    history[convid_].prev_len = 0;
    Del(convid_);
    Save(convid_);
}

void LLama::Load(std::string name)
{
    try
    {
        std::lock_guard<std::mutex> lock(fileAccessMutex);
        std::stringstream buffer;
        std::ifstream session_file(ConversationPath + name + suffix);
        if (session_file.is_open())
        {
            buffer << session_file.rdbuf();
            json j = json::parse(buffer.str());
            if (j.is_array())
            {
                for (auto& it : j)
                {
                    if (it.is_object())
                    {
                        history[name].messages.push_back({
                            it["role"].get<std::string>().c_str(),
                            it["content"].get<std::string>().c_str()
                        });
                    }
                }
            }
        }
        else
        {
            LogError("ChatBot Error: Unable to load session " + name + ".");
        }
    }
    catch (std::exception& e)
    {
        LogError(e.what());
        history[name].messages.clear();
        history[name].messages.push_back({"system", sys.c_str()});
    }
    convid_ = name;
    LogInfo("Bot: 加载 {0} 成功", name);
}

void LLama::Save(std::string name)
{
    json j;
    for (auto& m : history[name].messages)
    {
        j.push_back({
            {"role", m.role},
            {"content", m.content}
        });
    }
    std::ofstream session_file(ConversationPath + name + suffix);
    if (session_file.is_open())
    {
        session_file << j.dump();
        session_file.close();
        LogInfo("Bot : Save {0} successfully", name);
    }
    else
    {
        LogError("ChatBot Error: Unable to save session {0},{1}", name, ".");
    }
}

void LLama::Del(std::string name)
{
    if (remove((ConversationPath + name + suffix).c_str()) != 0)
    {
        LogError("ChatBot Error: Unable to delete session {0},{1}", name, ".");
    }
    LogInfo("Bot : 删除 {0} 成功", name);
}

void LLama::Add(std::string name)
{
    history[name].messages = std::vector<ChatMessage>();
    history[name].messages.push_back({"system", sys.c_str()});
    history[name].prev_len = 0;
    Save(name);
}

map<long long, string> LLama::GetHistory()
{
    return map<long long, string>();
}
