#define NOMINMAX
#include "Application.h"

#include "Downloader.h"
#include "SystemRole.h"
#define IMSPINNER_DEMO
#include "ImGuiSpinners.h"
#include "imgui_markdown.h"
std::vector<std::string> scommands;
bool cpshow = false;
ImGui::MarkdownConfig mdConfig;
ImFont* H1 = NULL;
ImFont* H2 = NULL;
ImFont* H3 = NULL;
Application* s_instance = nullptr;


ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData data)
{
    return s_instance->md_ImageCallback(data);
}

void LinkCallback(ImGui::MarkdownLinkCallbackData data)
{
    s_instance->md_LinkCallback(data);
}

Application::Application(const Configure& configure, bool setting)
{
    try
    {
        if (s_instance)
        {
            LogError("Application instance already exists!");
            return;
        }
        s_instance = this;
        scommands = commands;
        this->configure = configure;
        OnlySetting = setting;
        CreateBot();
        translator = CreateRef<Translate>(configure.baiDuTranslator);

        voiceToText = CreateRef<VoiceToText>(configure.openAi);
        listener = CreateRef<Listener>(sampleRate, framesPerBuffer);


        stableDiffusion = CreateRef<StableDiffusion>(configure.stableDiffusion);
        vitsData = configure.vits;
        whisperData = configure.whisper;
        live2D = configure.live2D;
        if (!Initialize())
        {
            LogWarn("Warning: Initialization failed!Maybe some function can not working");
        }
        if (live2D.enable)
        {
            Utils::OpenProgram(live2D.bin.c_str());
            lConfigure = Utils::LoadYaml<LConfigure>("Lconfig.yml").value();
        }
        if (whisperData.enable && whisper && mwhisper)
        {
            listener->listen();
            listener->ChangeDevice(configure.MicroPhoneID);
        }
        if (vitsData.enable && vits && vitsModel)
        {
            if (!vitsData.UseGptSovite)
            {
#ifdef WIN32
                Utils::OpenProgram("bin/VitsConvertor/VitsConvertor.exe");
#else
            std::thread([&]() {
                Utils::exec(Utils::GetAbsolutePath(bin + VitsConvertor + "VitsConvertor" + exeSuffix));
            }).detach();
#endif
            }
            Vits("欢迎使用本ChatBot");

            VitsAudioPlayer();
            VitsQueueHandler();
        }
        text_buffers.emplace_back("model");
        text_buffers.emplace_back("endPoint");
        text_buffers.emplace_back("proxy");
        text_buffers.emplace_back("api");
        text_buffers.emplace_back("lan");
        text_buffers.emplace_back("role");
        text_buffers.emplace_back("live2d");
        text_buffers.emplace_back("newName");
        text_buffers.emplace_back("apiPath");
        text_buffers.emplace_back("apiHost");
        text_buffers.emplace_back("search");
        text_buffers.emplace_back("outDevice");
        text_buffers.emplace_back("inputDevice");
        text_buffers.emplace_back("version");
        text_buffers.emplace_back("curl_parser");
    }
    catch
    (exception& e)
    {
        LogError(e.what());
    }
}

GLuint Application::LoadTexture(const std::string& path)
{
    return UImage::Img2Texture(path);
}

void Application::PreloadNextAudio()
{
    // 仅当没有预加载的音频时执行预加载
    std::lock_guard<std::mutex> lockAudio(audioMutex);
    if (nextAudio != nullptr)
    {
        return; // 已有预加载音频，不需要再加载
    }

    std::string fileToLoad;
    bool hasFile = false;
    bool needListen = false;

    {
        std::lock_guard<std::mutex> lock(vitsPlayQueueMutex);
        if (!vitsPlayQueue.empty())
        {
            fileToLoad = vitsPlayQueue.front();

            // 确认文件存在才处理
            if (UFile::Exists(fileToLoad))
            {
                vitsPlayQueue.pop();
                hasFile = true;
                // 设置是否需要语音识别回调
                needListen = whisperData.enable;
            }
        }
    }

    // 如果有文件可加载，开始异步预加载
    if (hasFile)
    {
        // 创建预加载音频对象
        auto newAudio = std::make_shared<PreloadedAudio>();
        newAudio->filename = fileToLoad;
        newAudio->isListen = needListen;

        // 在后台线程预加载音频数据
        std::thread([newAudio]()
        {
            // 打开音频文件
            SF_INFO info;
            SNDFILE* file = sf_open(newAudio->filename.c_str(), SFM_READ, &info);
            if (!file)
            {
                std::cerr << "无法打开音频文件: " << newAudio->filename << std::endl;
                return;
            }

            // 读取音频数据到内存
            newAudio->sampleRate = info.samplerate;
            newAudio->channels = info.channels;
            newAudio->frames = info.frames;
            newAudio->data = new float[info.frames * info.channels];
            sf_readf_float(file, newAudio->data, info.frames);
            sf_close(file);

            // 标记加载完成
            newAudio->ready = true;
            LogInfo("音频预加载完成: {0}", newAudio->filename);
        }).detach();

        // 设置为下一个要播放的音频
        nextAudio = newAudio;
    }
}

// 音频播放处理函数 - 支持无缝衔接播放
void Application::VitsAudioPlayer()
{
    std::thread([&]()
    {
        while (AppRunning)
        {
            // 检查是否需要预加载音频
            if (nextAudio == nullptr)
            {
                PreloadNextAudio();
            }

            // 如果当前没有播放音频，且有预加载好的音频，则开始播放
            bool startPlayback = false;
            {
                std::lock_guard<std::mutex> lockAudio(audioMutex);
                if (!isPlaying && nextAudio && nextAudio->ready)
                {
                    currentAudio = nextAudio;
                    nextAudio = nullptr;
                    isPlaying = true;
                    startPlayback = true;
                }
            }

            // 开始播放
            if (startPlayback)
            {
                // 捕获当前音频的智能指针，避免在线程中使用局部变量
                auto audioToPlay = currentAudio;

                // 在新线程中播放音频
                std::thread([this, audioToPlay]()
                {
                    // 设置不录音标志
                    NoRecord = true;

                    // 初始化PortAudio
                    PaError err = Pa_Initialize();
                    if (err != paNoError)
                    {
                        std::cerr << "初始化PortAudio失败: " << Pa_GetErrorText(err) << std::endl;
                        NoRecord = false;

                        std::lock_guard<std::mutex> lockAudio(audioMutex);
                        isPlaying = false;
                        return;
                    }

                    // 准备播放参数
                    paUserData userData = {audioToPlay->data, 0, audioToPlay->frames * audioToPlay->channels};
                    PaStream* stream;

                    // 打开音频流
                    err = Pa_OpenDefaultStream(&stream, 0, audioToPlay->channels, paFloat32,
                                               audioToPlay->sampleRate, FRAMES_PER_BUFFER, Utils::paCallback,
                                               &userData);
                    if (err != paNoError)
                    {
                        std::cerr << "打开音频流失败: " << Pa_GetErrorText(err) << std::endl;
                        Pa_Terminate();
                        NoRecord = false;

                        std::lock_guard<std::mutex> lockAudio(audioMutex);
                        isPlaying = false;
                        return;
                    }

                    // 开始播放音频
                    err = Pa_StartStream(stream);
                    if (err != paNoError)
                    {
                        std::cerr << "启动音频流失败: " << Pa_GetErrorText(err) << std::endl;
                        Pa_CloseStream(stream);
                        Pa_Terminate();
                        NoRecord = false;

                        std::lock_guard<std::mutex> lockAudio(audioMutex);
                        isPlaying = false;
                        return;
                    }

                    // 在播放开始后立即尝试预加载下一个音频
                    PreloadNextAudio();

                    // 等待音频播放完成
                    while (Pa_IsStreamActive(stream))
                    {
                        Pa_Sleep(100);
                    }

                    // 播放完成，清理资源
                    err = Pa_StopStream(stream);
                    Pa_CloseStream(stream);
                    Pa_Terminate();

                    // 恢复录音标志
                    NoRecord = false;

                    // 如果需要语音识别，则调用listen方法
                    if (audioToPlay->isListen && listener)
                    {
                        listener->listen();
                    }

                    // 记录日志
                    LogInfo("音频播放完成: {0}", audioToPlay->filename);

                    // 删除已播放的临时文件
                    std::remove(audioToPlay->filename.c_str());

                    // 标记播放结束
                    {
                        std::lock_guard<std::mutex> lockAudio(audioMutex);
                        isPlaying = false;
                    }
                }).detach();
            }

            // 短暂休眠
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }).detach();
}

// 在VITS任务处理函数中，当音频文件生成完成后立即触发预加载
void Application::VitsQueueHandler()
{
    std::thread([&]()
    {
        while (AppRunning)
        {
            // 从队列中获取任务
            VitsQueueItem item;
            bool hasTask = false;

            {
                std::lock_guard<std::mutex> lock(vitsQueueMutex);
                if (!vitsQueue.empty())
                {
                    item = vitsQueue.front();
                    vitsQueue.pop();
                    hasTask = true;
                }
            }

            // 处理任务
            if (hasTask)
            {
                if (item.isApi)
                {
                    // 处理API任务
                    std::string url = item.endPoint + "?text=" + Utils::UrlEncode(item.text) + "&text_language=zh";
                    auto res = Get(cpr::Url{url});

                    if (res.status_code != 200)
                    {
                        LogError("GPT-SoVits错误: " + res.text);
                        vits = false;
                    }
                    else
                    {
                        std::ofstream wavFile(item.outputFile, std::ios::binary);
                        if (wavFile)
                        {
                            wavFile.write(res.text.c_str(), res.text.size());
                            wavFile.close();
                            LogInfo("语音合成完成: {0} -> {1}", item.text, item.outputFile);

                            // 添加到播放队列
                            {
                                std::lock_guard<std::mutex> playLock(vitsPlayQueueMutex);
                                vitsPlayQueue.push(item.outputFile);
                            }

                            // 语音生成完成后立即触发预加载
                            PreloadNextAudio();
                        }
                        else
                        {
                            std::cerr << "无法打开文件进行写入: " << item.outputFile << std::endl;
                        }
                    }
                }
                else
                {
                    // 处理本地任务
                    VitsTask task;
                    task.model = Utils::GetAbsolutePath(vitsData.model);
                    task.config = Utils::GetAbsolutePath(vitsData.config);
                    task.text = item.text;
                    task.sid = vitsData.speaker_id;
                    task.outpath = item.outputFile;

                    // 保存任务
                    Utils::SaveYaml("task.yaml", Utils::toYaml(task));
                    LogInfo("本地语音合成任务已创建: {0}", item.text);

                    // 添加到播放队列
                    {
                        std::lock_guard<std::mutex> playLock(vitsPlayQueueMutex);
                        vitsPlayQueue.push(item.outputFile);
                    }

                    // 语音任务创建后尝试预加载（本地VITS可能需要额外的文件监听机制）
                    PreloadNextAudio();
                }
            }
            else
            {
                // 队列为空时休眠一段时间
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }).detach();
}

// 修改后的Vits方法 - 将语音合成任务添加到队列
void Application::Vits(std::string text)
{
    // 提取普通文本
    text = Utils::ExtractNormalText(text);

    // 如果启用了翻译，先进行翻译
    if (configure.baiDuTranslator.enable)
    {
        text = translator->translate(text, configure.vits.lanType);
    }

    // 生成唯一的输出文件名
    static int fileCounter = 0;
    std::string outputFile = "vits_" + std::to_string(fileCounter++) + ".wav";

    // 创建队列项并设置相关信息
    VitsQueueItem item;
    item.text = text;
    item.outputFile = outputFile;

    // 根据配置选择API或本地处理
    if (configure.vits.UseGptSovite)
    {
        item.isApi = true;
        item.endPoint = configure.vits.apiEndPoint;
    }
    else
    {
        item.isApi = false;
    }

    // 添加到队列
    {
        std::lock_guard<std::mutex> lock(vitsQueueMutex);
        vitsQueue.push(item);
    }

    LogInfo("语音合成任务已添加到队列: {0}", text);
}


void Application::_Draw(Ref<std::string> prompt, long long ts, bool callFromBot, const std::string& negative)
{
    if (is_valid_text(configure.stableDiffusion.apiPath))
    {
        std::string uid = stableDiffusion->Text2Img(*prompt, negative);

        if (callFromBot)
        {
            // 使用互斥锁保护共享资源 chat_history
            std::lock_guard<std::mutex> lock(chat_mutex);
            for (auto& it : chat_history)
            {
                if (it->flag == 1)
                {
                    if (it->timestamp >= ts)
                    {
                        it->image = uid;
                        break;
                    }
                }
                else
                {
                    continue;
                }
            }
        }
        else
        {
            Chat img;
            img.flag = 1;
            img.timestamp = Utils::GetCurrentTimestamp();
            img.content = "[Draw Finished]";
            img.image = uid;

            // 使用互斥锁保护共享资源 chat_history
            std::lock_guard<std::mutex> lock(chat_mutex);
            auto chat = CreateRef<Chat>(img);
            if (!chat_history.empty() && chat_history.back())
                chat_history.back()->addChild(chat);
            chat_history.emplace_back(chat);
            save(convid);
            bot->Save(convid);
        }
    }
    else
    {
        Chat img;
        img.flag = 1;
        img.timestamp = Utils::GetCurrentTimestamp() + 10;
        img.content = reinterpret_cast<const char*>(u8"抱歉,我不能为您生成图片,因为您的api地址为空");
        std::lock_guard<std::mutex> lock(chat_mutex);
        auto chat = CreateRef<Chat>(img);
        if (!chat_history.empty() && chat_history.back())
            chat_history.back()->addChild(chat);
        chat_history.emplace_back(chat);
        save(convid);
        bot->Save(convid);
    }
}

void Application::DisplayInputText(Ref<Chat> chat, bool edit)
{
    try
    {
        float max_width = ImGui::GetWindowContentRegionMax().x * 0.85f;
        int n = static_cast<int>(max_width / fontSize);

        n = max(n, 1);
        max_width = min(max_width, fontSize * n) + fontSize / 2;

        ImVec2 input_size = ImVec2(0, 0);
        std::vector<std::string> lines;
        std::string line;
        std::istringstream stream(chat->content);
        float max_line_width = 0;


        // 按行读取内容
        while (std::getline(stream, line))
        {
            lines.push_back(line);
            ImVec2 text_size = ImGui::CalcTextSize(line.c_str());
            max_line_width = max(max_line_width, text_size.x);
        }
        // 计算输入框的宽度
        input_size.x = min(max_line_width, max_width) + 16;
        static float orig = fontSize;
        input_size.y = 0;
        std::string processed_content;
        auto isChinese = [](uint32_t code_point) -> bool
        {
            return (code_point >= 0x4E00 && code_point <= 0x9FFF) ||
                (code_point >= 0x3400 && code_point <= 0x4DBF) ||
                (code_point >= 0x20000 && code_point <= 0x2A6DF) ||
                (code_point >= 0x2A700 && code_point <= 0x2B73F) ||
                (code_point >= 0x2B740 && code_point <= 0x2B81F) ||
                (code_point >= 0x2B820 && code_point <= 0x2CEAF) ||
                (code_point >= 0xF900 && code_point <= 0xFAFF) ||
                (code_point >= 0x2F800 && code_point <= 0x2FA1F);
        };
        auto processLine = [&](const std::string& line, int n)
        {
            std::string processed;
            int current_width = 0;
            size_t pos = 0;
            while (pos < line.size())
            {
                // 解析UTF-8字符长度和码点
                unsigned char c = static_cast<unsigned char>(line[pos]);
                int char_len = 1;
                if ((c & 0x80) == 0)
                {
                    char_len = 1;
                }
                else if ((c & 0xE0) == 0xC0)
                {
                    char_len = 2;
                }
                else if ((c & 0xF0) == 0xE0)
                {
                    char_len = 3;
                }
                else if ((c & 0xF8) == 0xF0)
                {
                    char_len = 4;
                }
                else
                {
                    // 无效字符，按单字节处理
                    char_len = 1;
                }

                // 提取码点
                uint32_t code_point = 0;
                bool valid = true;
                for (int j = 0; j < char_len; ++j)
                {
                    if (pos + j >= line.size())
                    {
                        valid = false;
                        break;
                    }
                    unsigned char ch = static_cast<unsigned char>(line[pos + j]);
                    if (j == 0)
                    {
                        switch (char_len)
                        {
                        case 1: code_point = ch;
                            break;
                        case 2: code_point = ch & 0x1F;
                            break;
                        case 3: code_point = ch & 0x0F;
                            break;
                        case 4: code_point = ch & 0x07;
                            break;
                        }
                    }
                    else
                    {
                        if ((ch & 0xC0) != 0x80)
                        {
                            valid = false;
                            break;
                        }
                        code_point = (code_point << 6) | (ch & 0x3F);
                    }
                }

                int char_width = 1;
                if (valid && isChinese(code_point))
                {
                    char_width = 2;
                }

                // 处理换行
                if (current_width + char_width > n)
                {
                    processed += '\n';
                    current_width = 0;
                }
                else
                {
                    for (int j = 0; j < char_len; ++j)
                    {
                        if (pos + j < line.size())
                        {
                            processed += line[pos + j];
                        }
                    }
                    pos += char_len;
                    current_width += char_width;
                }
            }
            return processed;
        };
        bool stop = false;
        bool inThink = false;
        for (size_t i = 0; i < lines.size(); ++i)
        {
            line = lines[i];
            if (line.find("<think>") != std::string::npos)
            {
                inThink = true;
                processed_content += line + "\n";
            }
            if (line.find("</think>") != std::string::npos)
            {
                inThink = false;
                processed_content += line + "\n";
            }
            if (!inThink && line.find("[Code]") != std::string::npos)
            {
                if (!stop)
                    stop = true;
                else
                {
                    stop = false;
                }
                processed_content += line + "\n";
                continue;
            }
            if (stop)
            {
                processed_content += line + "\n";
                continue;
            }
            if (line.empty())
            {
                continue;
            }
            /*int width = Utils::WStringSize(line);
            int idx = 1;
            while (width >= n)
            {
                line = Utils::WStringInsert(line, n * idx, "\n");
                width -= n;
                idx++;
            }*/
            line = processLine(line, (n - 1) * 2);

            processed_content += line; // 合并处理后的行
            if (i < lines.size() - 1)
            {
                processed_content += "\n"; // 添加换行符
            }
        }

        int enter_count = std::count(processed_content.begin(), processed_content.end(), '\n');
        input_size.y += orig * (enter_count + 1) + ImGui::GetStyle().ItemSpacing.y;
        input_size.x = max(input_size.x, 32.f);
        input_size.y = max(input_size.y, 16.f);

        std::vector<char> mutableBuffer(processed_content.begin(), processed_content.end());
        mutableBuffer.push_back('\0');

        auto& style = ImGui::GetStyle();
        float i = style.ScrollbarSize;
        style.ScrollbarSize = 0.00001;
        static std::string lastContent = "";
        if (edit)
        {
            // 编辑状态下的多行输入
            strcpy_s(input_buffer, mutableBuffer.data());
            last_input = input_buffer;
            if (ImGui::InputTextMultiline(
                ("##" + std::to_string(chat->timestamp) + std::to_string(chat->flag)).c_str(),
                input_buffer,
                sizeof(input_buffer),
                input_size,
                ImGuiInputTextFlags_AllowTabInput
            ))
            {
                chat->content = last_input = input_text = input_buffer;
                if (chat->content.empty())
                {
                    chat->content = " ";
                }
                input_text.clear();
            }
            bool isFocused = ImGui::IsItemFocused();

            // 处理回车键输入
            if (isFocused && ImGui::IsKeyPressed(ImGuiKey_Enter))
            {
                input_text += "\n";
            }

            // 显示时间戳
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), Utils::Stamp2Time(chat->timestamp).c_str());

            // 所有按钮统一放在对话下方一排
            ImGui::BeginGroup(); // 开始一个组，使所有按钮在同一行

            // 发送按钮
            ImGui::PushID(("send_button_" + std::to_string(chat->timestamp)).c_str());
            if (ImGui::ImageButton(("send_button_" + std::to_string(chat->timestamp)).c_str(), TextureCache["send"],
                                   ImVec2(16, 16)))
            {
                auto it = ranges::find(chat_history, chat);
                chat->content = last_input;
                chat->history.emplace_back(chat->content);
                chat->reedit = false;
                chat->currentVersionIndex++;
                // 如果找到了目标 chat
                if (it != chat_history.end())
                {
                    // 删除目标 chat 之后的所有元素（不包括目标 chat 本身）
                    chat_history.erase(std::next(it), chat_history.end());
                }
                bot->Save(convid);
                save(convid);

                AddSubmit();
            }
            ImGui::PopID();
            ImGui::SameLine();

            // 取消按钮
            ImGui::PushID(("cancel_button_" + std::to_string(chat->timestamp)).c_str());
            if (ImGui::ImageButton(("cancel_button_" + std::to_string(chat->timestamp)).c_str(),
                                   TextureCache["cancel"],
                                   ImVec2(16, 16)))
            {
                chat->reedit = false;
                chat->content = lastContent;
            }
            ImGui::PopID();
            ImGui::SameLine();

            ImGui::EndGroup(); // 结束按钮组
        }
        else
        {
            /*
            // 获取当前光标屏幕位置，用于绘制背景
            ImVec2 screenPos = ImGui::GetCursorScreenPos();

            // 获取绘制列表用于绘制背景
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // 根据消息类型选择背景颜色
            ImU32 bgColor;
            if (chat->flag == 0)
            {
                // 用户消息背景色（浅蓝色）
                bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.3f, 0.4f, 0.2f));
            }
            else
            {
                // AI消息背景色（浅灰色）
                bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.3f, 0.2f));
            }

            // 设置圆角半径
            float cornerRadius = 5.0f;

            // 绘制圆角矩形背景
            drawList->AddRectFilled(
                ImVec2(screenPos.x, screenPos.y), // 左上角（添加边距）
                ImVec2(screenPos.x + input_size.x, screenPos.y + input_size.y), // 右下角（完全匹配input_size）
                bgColor,
                cornerRadius
            );

            // 添加边框效果
            ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.5f, 0.3f));
            drawList->AddRect(
                ImVec2(screenPos.x, screenPos.y),
                ImVec2(screenPos.x + input_size.x, screenPos.y + input_size.y),
                borderColor,
                cornerRadius,
                0,
                1.0f
            );

            // 渲染Markdown内容
            ImGui::BeginChild(("md_" + std::to_string(chat->timestamp)).c_str(), input_size, false);
            RenderMarkdown(mutableBuffer, input_size); // 渲染Markdown内容
            ImGui::EndChild();*/
            ImGui::InputTextMultiline(
                ("##" + std::to_string(chat->timestamp) + std::to_string(chat->flag)).c_str(),
                mutableBuffer.data(),
                mutableBuffer.size(),
                input_size,
                ImGuiInputTextFlags_ReadOnly
            );

            // 显示时间戳
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), Utils::Stamp2Time(chat->timestamp).c_str());

            // 所有按钮统一放在对话下方一排
            ImGui::BeginGroup(); // 开始一个组，使所有按钮在同一行

            // 复制按钮 - 始终显示
            ImGui::PushID(("copy_button_" + std::to_string(chat->timestamp)).c_str());
            if (ImGui::ImageButton(("copy_button_" + std::to_string(chat->timestamp)).c_str(),
                                   TextureCache["copy"],
                                   ImVec2(16, 16)))
            {
                // 将当前聊天内容复制到系统剪贴板
                ImGui::SetClipboardText(chat->content.c_str());
            }
            ImGui::PopID();

            // 根据聊天类型显示不同按钮
            if (chat->flag == 0)
            {
                // 用户消息 - 添加编辑按钮
                ImGui::SameLine();
                ImGui::PushID(("edit_button_" + std::to_string(chat->timestamp)).c_str());
                if (ImGui::ImageButton(("edit_button_" + std::to_string(chat->timestamp)).c_str(),
                                       TextureCache["edit"],
                                       ImVec2(16, 16)))
                {
                    chat->reedit = true;
                    lastContent = chat->content;
                }
                ImGui::PopID();

                ImGui::SameLine();
                if (!chat_history.empty() && chat_history.back() && !chat_history.back()->talking)
                {
                    ImGui::PushID(("del_button_" + std::to_string(chat->timestamp)).c_str());
                    if (ImGui::ImageButton(("del_button_" + std::to_string(chat->timestamp)).c_str(),
                                           TextureCache["del"],
                                           ImVec2(16, 16)))
                    {
                        bool flag = true;
                        // 查找要删除的聊天记录在历史中的位置
                        auto it = ranges::find(chat_history, chat);
                        if (it != chat_history.end())
                        {
                            // 检查节点是否有历史记录
                            if (chat->history.size() > 1)
                            {
                                if (chat->currentVersionIndex < chat->children.size())
                                {
                                    auto childIt = chat->children.begin() + chat->currentVersionIndex;
                                    if (childIt != chat->children.end())
                                    {
                                        chat->children.erase(childIt);
                                    }
                                }

                                if (!chat->history.empty())
                                {
                                    int currentIndex = chat->currentVersionIndex;

                                    if (currentIndex < chat->history.size())
                                    {
                                        chat->history.erase(chat->history.begin() + currentIndex);
                                    }

                                    if (currentIndex > 0)
                                    {
                                        chat->currentVersionIndex--;
                                    }
                                    else if (chat->history.empty())
                                    {
                                        chat->currentVersionIndex = 0;
                                    }

                                    // 更新内容为当前索引对应的历史记录内容
                                    if (!chat->history.empty() && chat->currentVersionIndex < chat->history.size())
                                    {
                                        chat->content = chat->history[chat->currentVersionIndex];
                                    }
                                    // 如果没有历史记录了，则重置内容
                                    else if (chat->history.empty())
                                    {
                                        chat->content = "..."; // 默认内容
                                    }
                                }
                            }
                            else
                            {
                                if (chat->parent != nullptr)
                                {
                                    // 从父节点的子节点列表中移除
                                    auto& siblings = chat->parent->children;
                                    auto nodeIt = ranges::find(siblings, chat);
                                    if (nodeIt != siblings.end())
                                    {
                                        siblings.erase(nodeIt);
                                    }
                                }
                                else
                                {
                                    chat_history.clear();
                                    bot->Reset();
                                    codes.clear();
                                    if (configure.claude.enable)
                                    {
                                        FirstTime = Utils::GetCurrentTimestamp();
                                    }
                                    save(convid);
                                    flag = false;
                                }

                                // 无论是否为根节点，都清空子节点
                                chat->children.clear(); // 直接清空子节点列表，不再保留任何子节点
                            }
                            if (flag)
                                RebuildChatHistory();
                        }
                        else
                        {
                            // 如果在历史记录中找不到该聊天节点，记录错误
                            LogError("聊天记录未在历史中找到！");
                        }
                        save(convid);
                        bot->Save(convid);
                    }
                    ImGui::PopID();
                }
                ImGui::SameLine();
                ImGui::PushID(("left_button_" + std::to_string(chat->timestamp)).c_str());
                if (ImGui::ImageButton(("left_button_" + std::to_string(chat->timestamp)).c_str(),
                                       TextureCache["left"],
                                       ImVec2(16, 16)))
                {
                    if (chat->currentVersionIndex > 0)
                    {
                        chat->currentVersionIndex--;
                        chat->selectBranch(chat->currentVersionIndex);
                        RebuildChatHistory();
                    }
                }
                ImGui::PopID();

                ImGui::SameLine();
                ImGui::Text(" %d/%d", chat->currentVersionIndex + 1, chat->children.size());

                ImGui::SameLine();
                ImGui::PushID(("right_button_" + std::to_string(chat->timestamp)).c_str());
                if (ImGui::ImageButton(("right_button_" + std::to_string(chat->timestamp)).c_str(),
                                       TextureCache["right"],
                                       ImVec2(16, 16)))
                {
                    if (chat->currentVersionIndex < chat->children.size() - 1)
                    {
                        chat->currentVersionIndex++;
                        chat->selectBranch(chat->currentVersionIndex);
                        RebuildChatHistory();
                    }
                }
                ImGui::PopID();
            }

            if (chat->flag == 1 && chat->talking)
            {
                // AI回复中 - 添加停止按钮
                ImGui::SameLine();
                ImGui::PushID(("stop_button_" + std::to_string(chat->timestamp)).c_str());
                if (ImGui::ImageButton(("stop_button_" + std::to_string(chat->timestamp)).c_str(),
                                       TextureCache["pause"],
                                       ImVec2(16, 16)))
                {
                    bot->ForceStop();
                }
                ImGui::PopID();
            }

            ImGui::EndGroup(); // 结束按钮组
        }
        style.ScrollbarSize = i;
    }
    catch (...)
    {
    }
}

bool Application::ContainsCommand(std::string& str, std::string& cmd, std::string& args) const
{
    for (const auto& it : commands)
    {
        std::regex cmd_regex(R"([CMD](\w+)[CMD])"); // 定义匹配命令的正则表达式
        std::regex args_regex(R"((\[[^\]]*\]))"); // 定义匹配参数的正则表达式
        std::smatch cmd_match, args_match;

        // 匹配命令
        if (std::regex_search(str, cmd_match, cmd_regex))
        {
            cmd = cmd_match[1]; // 第一个括号内的内容即为命令
            str = cmd_match.prefix().str() + cmd_match.suffix().str(); // 删除匹配到的命令部分

            // 匹配参数
            if (std::regex_search(str, args_match, args_regex))
            {
                args = args_match[1]; // 第一个括号内的内容即为参数
                args = args.substr(1, args.length() - 2); // 去除参数中的方括号
                str = args_match.prefix().str() + args_match.suffix().str(); // 删除匹配到的参数部分
                return true;
            }
            return true;
        }
    }
    return false;
}

void Application::InlineCommand(const std::string& cmd, const std::string& args, long long ts)
{
    if (cmd == "draw" || cmd == reinterpret_cast<const char*>(u8"绘图"))
    {
        Draw(args, ts);
    }
    else if (cmd == "help" || reinterpret_cast<const char*>(u8"帮助"))
    {
        Chat help;
        help.flag = 1;
        help.timestamp = Utils::GetCurrentTimestamp() + 10;
        help.content = reinterpret_cast<const char*>(
            u8"内置命令:\n[CMD]draw[CMD]或者[CMD]绘图[CMD] [prompt] 绘制一张图片\n[CMD]help[CMD]或者[CMD]帮助[CMD] 唤起帮助");
        chat_history.emplace_back(CreateRef<Chat>(help));
    }
}

void Application::CreateBot()
{
    loadingBot = true;
    std::thread([&]()
    {
        OnlySetting = false;
        auto isMissingKey = [](const std::string& key)
        {
            return key.empty();
        };

        // 检查所有模型的API密钥是否缺失
        if ((configure.openAi.enable && isMissingKey(configure.openAi.api_key)) ||
            (configure.claude.enable && isMissingKey(configure.claude.slackToken)) ||
            (configure.gemini.enable && isMissingKey(configure.gemini._apiKey)) ||
            (configure.grok.enable && isMissingKey(configure.grok.api_key)) ||
            (configure.mistral.enable && isMissingKey(configure.mistral.api_key)) ||
            (configure.qianwen.enable && isMissingKey(configure.qianwen.api_key)) ||
            (configure.sparkdesk.enable && isMissingKey(configure.sparkdesk.api_key)) ||
            (configure.chatglm.enable && isMissingKey(configure.chatglm.api_key)) ||
            (configure.hunyuan.enable && isMissingKey(configure.hunyuan.api_key)) ||
            (configure.baichuan.enable && isMissingKey(configure.baichuan.api_key)) ||
            (configure.claudeAPI.enable && isMissingKey(configure.claudeAPI.apiKey)) ||
            (configure.huoshan.enable && isMissingKey(configure.huoshan.api_key))
        )
        {
            OnlySetting = true;
            state = State::NO_BOT_KEY;
        }

        // 检查是否没有选择任何模型
        bool hasAnyModelEnabled = configure.openAi.enable || configure.claude.enable ||
            configure.gemini.enable || configure.grok.enable ||
            configure.mistral.enable || configure.qianwen.enable ||
            configure.sparkdesk.enable || configure.chatglm.enable ||
            configure.hunyuan.enable || configure.baichuan.enable ||
            configure.claudeAPI.enable || configure.huoshan.enable;

        bool hasCustomGPT = false;
        for (const auto& [name, cconfig] : configure.customGPTs)
        {
            if (cconfig.enable)
            {
                hasCustomGPT = true;
                break;
            }
        }

        // 检查是否有启用的自定义规则
        bool hasCustomRule = false;
        for (const auto& rule : configure.customRules)
        {
            if (rule.enable)
            {
                hasCustomRule = true;
                break;
            }
        }

        // 如果没有启用任何类型的模型，设置错误状态
        if (!hasAnyModelEnabled && !hasCustomGPT && !hasCustomRule)
        {
            OnlySetting = true;
            state = State::NO_BOT_KEY;
            bot = CreateRef<GPTLike>(GPTLikeCreateInfo());
        }

        // 检查自定义模型配置
        for (const auto& [name, cconfig] : configure.customGPTs)
        {
            if (cconfig.enable)
            {
                if (!cconfig.useLocalModel)
                {
                    if (isMissingKey(cconfig.api_key))
                    {
                        OnlySetting = true;
                        state = State::NO_BOT_KEY;
                    }
                    else if (!cconfig.model.empty())
                    {
                        OnlySetting = false;
                        state = State::OK;
                    }
                }
                else
                {
                    if (!UFile::Exists(cconfig.llamaData.model))
                    {
                        OnlySetting = true;
                        state = State::NO_BOT_KEY;
                    }
                }
            }
        }

        // 检查自定义规则配置
        for (const auto& rule : configure.customRules)
        {
            if (rule.enable)
            {
                // 检查自定义规则是否有有效的API密钥
                if (isMissingKey(rule.apiKeyRole.key))
                {
                    OnlySetting = true;
                    state = State::NO_BOT_KEY;
                }
                else
                {
                    OnlySetting = false;
                    state = State::OK;
                }
            }
        }

        // 创建所选的聊天机器人实例
        if (configure.claude.enable)
        {
            bot = CreateRef<ClaudeInSlack>(configure.claude);
            GetClaudeHistory();
        }
        else if (configure.claudeAPI.enable)
        {
            // 创建Claude API聊天机器人
            bot = CreateRef<Claude>(configure.claudeAPI, SYSTEMROLE + SYSTEMROLE_EX);
            ConversationPath /= "ClaudeAPI/";
            if (!std::filesystem::exists(ConversationPath))
            {
                std::filesystem::create_directories(ConversationPath);
            }
        }
        else if (configure.openAi.enable)
        {
            bot = CreateRef<ChatGPT>(configure.openAi, SYSTEMROLE + SYSTEMROLE_EX);
        }
        else if (configure.gemini.enable)
        {
            bot = CreateRef<Gemini>(configure.gemini, SYSTEMROLE + SYSTEMROLE_EX);
            ConversationPath /= "Gemini/";
            if (!std::filesystem::exists(ConversationPath))
            {
                std::filesystem::create_directories(ConversationPath);
            }
        }
        else if (configure.grok.enable)
        {
            bot = CreateRef<Grok>(configure.grok, SYSTEMROLE + SYSTEMROLE_EX);
        }
        // 创建Mistral聊天机器人
        else if (configure.mistral.enable)
        {
            bot = CreateRef<Mistral>(configure.mistral, SYSTEMROLE + SYSTEMROLE_EX);
            ConversationPath /= "Mistral/";
            if (!std::filesystem::exists(ConversationPath))
            {
                std::filesystem::create_directories(ConversationPath);
            }
        }
        // 创建通义千问聊天机器人
        else if (configure.qianwen.enable)
        {
            bot = CreateRef<TongyiQianwen>(configure.qianwen, SYSTEMROLE + SYSTEMROLE_EX);
            ConversationPath /= "QianWen/";
            if (!std::filesystem::exists(ConversationPath))
            {
                std::filesystem::create_directories(ConversationPath);
            }
        }
        // 创建讯飞星火聊天机器人
        else if (configure.sparkdesk.enable)
        {
            bot = CreateRef<SparkDesk>(configure.sparkdesk, SYSTEMROLE + SYSTEMROLE_EX);
            ConversationPath /= "SparkDesk/";
            if (!std::filesystem::exists(ConversationPath))
            {
                std::filesystem::create_directories(ConversationPath);
            }
        }
        // 创建ChatGLM聊天机器人
        else if (configure.chatglm.enable)
        {
            bot = CreateRef<ChatGLM>(configure.chatglm, SYSTEMROLE + SYSTEMROLE_EX);
            ConversationPath /= "ChatGLM/";
            if (!std::filesystem::exists(ConversationPath))
            {
                std::filesystem::create_directories(ConversationPath);
            }
        }
        // 创建腾讯混元聊天机器人
        else if (configure.hunyuan.enable)
        {
            bot = CreateRef<HunyuanAI>(configure.hunyuan, SYSTEMROLE + SYSTEMROLE_EX);
            ConversationPath /= "Hunyuan/";
            if (!std::filesystem::exists(ConversationPath))
            {
                std::filesystem::create_directories(ConversationPath);
            }
        }
        // 创建百川AI聊天机器人
        else if (configure.baichuan.enable)
        {
            bot = CreateRef<BaichuanAI>(configure.baichuan, SYSTEMROLE + SYSTEMROLE_EX);
            ConversationPath /= "Baichuan/";
            if (!std::filesystem::exists(ConversationPath))
            {
                std::filesystem::create_directories(ConversationPath);
            }
        }
        else if (configure.huoshan.enable)
        {
            bot = CreateRef<HuoshanAI>(configure.huoshan, SYSTEMROLE + SYSTEMROLE_EX);
            ConversationPath /= "Huoshan/";
            if (!std::filesystem::exists(ConversationPath))
            {
                std::filesystem::create_directories(ConversationPath);
            }
        }
        else
        {
            bool botCreated = false;

            // 首先尝试使用自定义规则创建机器人
            for (const auto& rule : configure.customRules)
            {
                if (rule.enable)
                {
                    // 使用自定义规则创建机器人
                    bot = CreateRef<CustomRule_Impl>(rule, SYSTEMROLE + SYSTEMROLE_EX);

                    // 创建存储会话的目录
                    std::string dirName = "CustomRule_" + rule.name + "/";
                    ConversationPath /= dirName;
                    if (!std::filesystem::exists(ConversationPath))
                    {
                        std::filesystem::create_directories(ConversationPath);
                    }

                    botCreated = true;
                    break; // 使用第一个启用的自定义规则
                }
            }

            // 如果没有通过自定义规则创建机器人，尝试创建自定义GPT或本地模型
            if (!botCreated)
            {
                for (const auto& [name, cconfig] : configure.customGPTs)
                {
                    if (cconfig.enable)
                    {
                        if (!cconfig.useLocalModel)
                            bot = CreateRef<GPTLike>(cconfig, SYSTEMROLE + SYSTEMROLE_EX);
                        else
                        {
                            bot = CreateRef<LLama>(cconfig.llamaData, SYSTEMROLE + SYSTEMROLE_EX);
                        }

                        // 创建自定义GPT的会话存储路径
                        std::string dirName = "CustomGPT_" + name + "/";
                        ConversationPath /= dirName;
                        if (!std::filesystem::exists(ConversationPath))
                        {
                            std::filesystem::create_directories(ConversationPath);
                        }

                        break; // 使用第一个可用的自定义GPT
                    }
                }
            }
        }

        // 清理会话列表
        conversations.clear();
        for (const auto& entry : std::filesystem::directory_iterator(ConversationPath))
        {
            if (entry.is_regular_file())
            {
                const auto& file_path = entry.path();
                std::string file_name = file_path.stem().string();
                if (!file_name.empty() && UFile::EndsWith(file_path.string(), ".dat"))
                {
                    conversations.emplace_back(file_name);
                }
                if (!conversations.size() > 0)
                {
                    bot->Add(convid);
                    conversations.emplace_back(convid);
                }
            }
        }

        // 如果不是使用Claude Slack接口，确保至少有一个会话
        if (!configure.claude.enable)
        {
            if (conversations.empty())
            {
                bot->Add(convid);
                save(convid, true);
                conversations.emplace_back(convid);
            }
            convid = conversations[0];
        }

        // 加载当前会话
        bot->Load(convid);
        chat_history = load(convid);
        loadingBot = false;
    }).detach();
}

void Application::RenderCodeBox()
{
    UniversalStyle();

    ImGui::Begin("Code Box", nullptr,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    static bool show_codes = false;
    for (auto& it : codes)
    {
        show_codes = ImGui::CollapsingHeader(it.first.c_str());
        if (show_codes)
        {
            for (const auto& code : it.second)
            {
                if (is_valid_text(code))
                {
                    if (ImGui::Button(code.c_str(), ImVec2(-1, 0)))
                    {
                        glfwSetClipboardString(nullptr, code.c_str());
                    }
                }
            }
        }
    }

    ImGui::PopStyleVar();
    ImGui::End();
}

void Application::RenderPopupBox()
{
    UniversalStyle();
    if (show_input_box)
    {
        ImGui::OpenPopup(reinterpret_cast<const char*>(u8"新建对话##popup"));
    }
    // 显示输入框并获取用户输入
    char buf[256] = {0};
    if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"新建对话##popup"), &show_input_box,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        static std::string text;
        if (ImGui::InputText("##input", buf, 256))
        {
            text = buf;
        };
        ImGui::SameLine();
        ImGui::Text(reinterpret_cast<const char*>(u8"请输入对话名称："));
        ImGui::Spacing();
        ImVec2 button_size(120, 30);
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
            ImGui::Button(reinterpret_cast<const char*>(u8"确定"), button_size))
        {
            if (is_valid_text(text))
            {
                bot->Add(text);
                conversations.emplace_back(text);
                chat_history.clear();
                auto iter = std::find(conversations.begin(), conversations.end(), text);
                if (iter != conversations.end())
                {
                    SelectIndices["conversation"] = std::distance(conversations.begin(), iter);
                }
                convid = conversations[SelectIndices["conversation"]];
                ImGui::CloseCurrentPopup();
                save(convid);
                bot->Load(convid);
                load(convid);
                show_input_box = false;
            }
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - button_size.x * 2);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
            ImGui::Button(reinterpret_cast<const char*>(u8"取消"), button_size))
        {
            ImGui::CloseCurrentPopup();
            show_input_box = false;
        }
        ImGui::EndPopup();
    }
}

void Application::RenderDownloadBox()
{
    UniversalStyle();
    ImGui::Begin("Downloader", nullptr,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    for (auto& downloader : downloaders)
    {
        if (downloader != nullptr && downloader->GetStatus() != UInitializing && downloader->GetStatus() !=
            UFinished)
        {
            std::string dA = downloader->GetBasicInfo().url;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(32.f / 256.f, 126.f / 256.f, 177.f / 256.f, 1.0f));
            ImGui::Text(downloader->GetBasicInfo().files[0].c_str());
            ImGui::PopStyleColor();
            ImGui::ProgressBar(downloader->GetProgress(), ImVec2(400, 32));
            ImGui::SameLine();
            ImGui::PushID(reinterpret_cast<const char*>(&downloader));
            ImSpinner::SpinnerFilledArcFade(reinterpret_cast<const char*>(&downloader), 16,
                                            ImColor(.2f, .2f, .2f, 1.0f), 6, 6);
            ImGui::PopID();
            if (downloader->IsRunning())
            {
                ImGui::SameLine();
                ImGui::PushID(("button_" + TextureCache["pause"] + dA).c_str());
                if (ImGui::ImageButton(("button_" + TextureCache["pause"] + dA).c_str(), TextureCache["pause"],
                                       ImVec2(32, 32)))
                {
                    downloader->Pause();
                }
                ImGui::PopID();
            }
            if (downloader->GetStatus() == UPaused)
            {
                ImGui::SameLine();
                ImGui::PushID(("button_" + TextureCache["play"] + dA).c_str());
                if (ImGui::ImageButton(("button_" + TextureCache["play"] + dA).c_str(), TextureCache["play"],
                                       ImVec2(32, 32)))
                {
                    downloader->Resume();
                }
                ImGui::PopID();
            }
            ImGui::SameLine();
            ImGui::PushID(("button_" + TextureCache["del"] + dA).c_str());
            if (ImGui::ImageButton(("button_" + TextureCache["del"] + dA).c_str(), TextureCache["del"],
                                   ImVec2(32, 32)))
            {
                std::thread([downloader]()
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1)); //延时析构
                    downloader->Stop();
                }).detach();
            }
            ImGui::PopID();
        }
    }
    Downloading = false;
    for (auto& downloader : downloaders)
    {
        if (downloader != nullptr && downloader->GetStatus() != UFinished)
        {
            Downloading = true;
            break;
        }
    }

    ImGui::PopStyleVar();
    ImGui::End();
}

void Application::RenderChatBox()
{
    UniversalStyle();
    // 创建一个子窗口
    ImGui::Begin(reinterpret_cast<const char*>(u8"对话"), NULL,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav);
    if (!loadingBot)
    {
        ImGui::BeginChild("Chat Box Content", ImVec2(0, -ImGui::GetTextLineHeightWithSpacing()), false,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoSavedSettings);
        std::sort(chat_history.begin(), chat_history.end(), compareByTimestamp);
        bool new_chat = false;
        // 显示聊天记录
        for (auto& chat : chat_history)
        {
            Ref<Chat> userAsk;
            Ref<Chat> botAnswer;
            if (!chat)
                continue;
            if (chat->flag == 0)
                userAsk = chat;
            else
                botAnswer = chat;

            if (userAsk && !userAsk->content.empty())
            {
                DisplayInputText(userAsk, userAsk->reedit);
            }
            if (botAnswer)
            {
                // Modify the style of the multi-line input field
                new_chat = botAnswer->newMessage;
                botAnswer->newMessage = false;
                float rounding = 4.0f;
                ImVec4 bg_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                ImVec4 text_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
                ImVec4 cursor_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
                ImVec4 selection_color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_color);
                ImGui::PushStyleColor(ImGuiCol_Text, text_color);
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, selection_color);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, selection_color);
                ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, bg_color);
                ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, text_color);
                ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, cursor_color);
                ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, cursor_color);
                ImGui::PushID(("Avatar" + std::to_string(botAnswer->timestamp)).c_str());
                if (ImGui::ImageButton(("Avatar" + std::to_string(botAnswer->timestamp)).c_str(),
                                       TextureCache["avatar"],
                                       ImVec2(24, 24)))
                {
                    fileBrowser = true;
                    title = reinterpret_cast<const char*>(u8"图片选择");
                    typeFilters = {".png", ".jpg"};
                    PathOnConfirm = [this]()
                    {
                        LogInfo(BrowserPath.c_str());
                        TextureCache["avatar"] = LoadTexture(BrowserPath);
                        UFile::UCopyFile(Resources + "avatar.png", Resources + "avatar.png.bak");
                        std::remove((Resources + "avatar.png").c_str());
                        std::remove((Resources + "avatar.png.meta").c_str());
                        UFile::UCopyFile(BrowserPath, Resources + "avatar.png");
                    };
                }
                ImGui::PopID();

                ImGui::SameLine();
                if (botAnswer->content.empty())
                {
                    if (new_chat)
                    {
                        ImGui::SameLine();
                        ImGui::PushID(reinterpret_cast<const char*>(&botAnswer));
                        ImSpinner::SpinnerFilledArcFade(reinterpret_cast<const char*>(&botAnswer), 8,
                                                        ImColor(.8f, .8f, .8f, 1.0f), 6, 6);
                        ImGui::PopID();
                    }
                }
                else
                    DisplayInputText(botAnswer);
                // 加载纹理的线程
                if (!botAnswer->image.empty())
                {
                    static bool wait = false;
                    if (SDCache.find(botAnswer->image) == SDCache.end())
                    {
                        // 如果没有加载过该纹理
                        if (!wait)
                        {
                            wait = true;
                            LogInfo("开始加载图片: {0}", Resources + "Images/" + botAnswer->image);
                            auto t = LoadTexture(Resources + "Images/" + botAnswer->image);
                            SDCache[botAnswer->image] = t;
                            wait = false;
                        }
                    }
                    else
                    {
                        // 如果纹理已加载,显示
                        if (ImGui::ImageButton("##" + SDCache[botAnswer->image], (SDCache[botAnswer->image]),
                                               ImVec2(256, 256)))
                        {
                            Utils::OpenFileManager(
                                Utils::GetAbsolutePath(Resources + "Images/" + botAnswer->image));
                        }
                    }
                }

                ImGui::PopStyleVar();
                ImGui::PopStyleColor(8);
            }
        }
        // 滚动到底部
        if (new_chat)
        {
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - ImGui::GetStyle().ItemSpacing.x - 20)
            {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu(reinterpret_cast<const char*>(u8"选项")))
            {
                if (ImGui::Button(reinterpret_cast<const char*>(u8"保存"), ImVec2(200, 30)))
                {
                    bot->Save(convid);
                    save(convid);
                }
                if (ImGui::Button(reinterpret_cast<const char*>(u8"新建"), ImVec2(200, 30)))
                {
                    // 显示输入框
                    show_input_box = true;
                }
                if (ImGui::Button(reinterpret_cast<const char*>(u8"清空记录"), ImVec2(200, 30)))
                {
                    chat_history.clear();
                    save(convid);
                }
                if (ImGui::Button(reinterpret_cast<const char*>(u8"重置会话"), ImVec2(200, 30)))
                {
                    chat_history.clear();
                    bot->Reset();
                    codes.clear();
                    if (configure.claude.enable)
                    {
                        FirstTime = Utils::GetCurrentTimestamp();
                    }
                    save(convid);
                }
                if (ImGui::Button(reinterpret_cast<const char*>(u8"删除当前会话"), ImVec2(200, 30)) &&
                    conversations.size() > 1)
                {
                    bot->Del(convid);
                    del(convid);
                    bot->Load(convid);
                    load(convid);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(reinterpret_cast<const char*>(u8"会话")))
            {
                ImGuiStyle& item_style = ImGui::GetStyle();
                item_style.Colors[ImGuiCol_Text] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
                if (!configure.claude.enable)
                {
                    for (int i = 0; i < conversations.size(); i++)
                    {
                        const bool is_selected = (SelectIndices["conversation"] == i);
                        ImGui::PushID("conversation");
                        if (ImGui::MenuItem(reinterpret_cast<const char*>(conversations[i].c_str()), nullptr,
                                            is_selected))
                        {
                            if (SelectIndices["conversation"] != i)
                            {
                                SelectIndices["conversation"] = i;
                                convid = conversations[SelectIndices["conversation"]];
                                bot->Load(convid);
                                load(convid);
                            }
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(reinterpret_cast<const char*>(u8"模式")))
            {
                ImGuiStyle& item_style = ImGui::GetStyle();
                item_style.Colors[ImGuiCol_Text] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
                for (int i = 0; i < roles.size(); i++)
                {
                    const bool is_selected = (SelectIndices["role"] == i);
                    if (ImGui::MenuItem(reinterpret_cast<const char*>(roles[i].c_str()), nullptr, is_selected))
                    {
                        if (SelectIndices["role"] != i)
                        {
                            SelectIndices["role"] = i;
                            role = roles[SelectIndices["role"]];
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }

    if (loadingBot)
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetCursorPos(center);
        font->Scale = 2;


        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"正在加载"));
        ImGui::SameLine();
        ImSpinner::SpinnerRotateDots("##loading", 8, 3, ImVec4(0.1f, 0.1f, 0.15f, 1.0f), 6, 4);


        font->Scale = 1;
    }

    ImGui::End();
}

void Application::RenderInputBox()
{
    if (listener && listener->IsRecorded())
    {
        int focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
        if (!focused)
        {
            glfwShowWindow(window);
            glfwRestoreWindow(window);
        }
        std::thread([&]()
        {
            listener->EndListen();
            listener->ResetRecorded();
            std::string path = "recorded" + std::to_string(Rnum) + ".wav";
            Rnum += 1;
            if (voiceToText && !path.empty())
            {
                std::future<std::string> conversion_future;
                if (!whisperData.useLocalModel)
                {
                    // 启动异步任务
                    conversion_future = std::async(std::launch::async, [&]()
                    {
                        return voiceToText->ConvertAsync(path).get();
                    });
                }
                else
                {
                    conversion_future = std::async(std::launch::async, [&]()
                    {
                        return WhisperConvertor(path);
                    });
                }
                // 在异步任务完成后执行回调函数
                auto callback = [&](std::future<std::string>& future)
                {
                    std::string text = future.get();

                    if (!is_valid_text(text))
                    {
                        // 显示错误消息
                        LogWarn("Bot Warning: Your question is empty");
                        if (whisperData.enable)
                            listener->listen();
                    }
                    else
                    {
                        text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
                        last_input = text;
                        Ref<Chat> user = CreateRef<Chat>();
                        user->timestamp = Utils::GetCurrentTimestamp();
                        user->content = last_input;
                        user->history.emplace_back(user->content);
                        if (!chat_history.empty() && chat_history.back())
                            chat_history.back()->addChild(user);
                        AddChatRecord(user);

                        Rnum -= 1;

                        std::remove(("recorded" + std::to_string(Rnum) + ".wav").c_str());
                        LogInfo("whisper: 删除录音{0}", "recorded" + std::to_string(Rnum) + ".wav");
                        /*std::shared_future<std::string> submit_future = std::async(std::launch::async, [&]()
                        {
                            Ref<Chat> botR = CreateRef<Chat>();
                            botR->flag = 1;
                            botR->timestamp = Utils::GetCurrentTimestamp() + 1;
                            std::lock_guard<std::mutex> lock(chat_history_mutex);
                            AddChatRecord(botR);
                            bot->SubmitAsync(last_input, botR->timestamp, role, convid);
                            while (!bot->Finished(botR->timestamp))
                            {
                                botR->content = bot->GetResponse(botR->timestamp);
                                botR->newMessage = true;
                                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                            }
                            botR->content = bot->GetResponse(botR->timestamp);
                            botR->content = StringExecutor::AutoExecute(botR->content, bot);
                            return botR->content;
                        }).share();
                        {
                            std::lock_guard<std::mutex> lock(submit_futures_mutex);
                            submit_futures.push_back(std::move(submit_future));
                        }*/
                        AddSubmit();
                    }
                };

                // 启动异步任务，并将回调函数作为参数传递
                std::async(std::launch::async, callback, std::ref(conversion_future));
            }

            listener->changeFile("recorded" + std::to_string(Rnum) + ".wav");
        }).detach();
    }
    {
        UniversalStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::Begin("##", NULL,
                     ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoTitleBar);
        /*if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu(reinterpret_cast<const char*>(u8"指令")))
            {
                if (ImGui::Button(reinterpret_cast<const char*>(u8"绘图")))
                {
                    input_text += "[CMD]draw[CMD] [ ]";
                }

                if (ImGui::Button(reinterpret_cast<const char*>(u8"帮助")))
                {
                    input_text += "[CMD]help[CMD]";
                }

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }*/
        ImVec2 size(50, 0);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        token = Utils::WStringSize(input_buffer);
        ImGui::Text(reinterpret_cast<const char*>(u8"字符数: %d"), token);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }


    strcpy_s(input_buffer, input_text.c_str());
    if (ImGui::InputTextMultiline("##Input Text", input_buffer, sizeof(input_buffer), ImVec2(-1, -1),
                                  ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CtrlEnterForNewLine))
    {
        last_input = input_text = input_buffer;
        input_text.clear();
    }

    // 保存输入框的聚焦状态
    bool input_focused = ImGui::IsItemFocused();

    // 只有当输入框被聚焦且满足提交条件时才能提交
    if (input_focused && (strlen(last_input.c_str()) > 0 && ImGui::IsKeyPressed(ImGuiKey_Enter) && (!ImGui::GetIO().
        KeyCtrl)))
    {
        Ref<Chat> user = CreateRef<Chat>();
        std::string cmd, args;
        user->timestamp = Utils::GetCurrentTimestamp();

        user->content = last_input;
        user->history.emplace_back(user->content);
        if (!chat_history.empty() && chat_history.back())
            chat_history.back()->addChild(user);
        if (is_valid_text(user->content))
        {
            AddChatRecord(user);
        }
        /*if (ContainsCommand(last_input, cmd, args))
        {
            InlineCommand(cmd, args, user->timestamp);
        }
        else
        {*/
        if (is_valid_text(last_input))
        {
            // 使用last_input作为键
            if (whisperData.enable)
                listener->ResetRecorded();

            AddSubmit();
        }
        //}

        input_text.clear();
        save(convid);
        bot->Save(convid);
    }
    else if (input_focused && ((ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl) ||
        (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyShift)))
    {
        input_text += "\n";
    }
    ImGui::End();

    // 处理已完成的submit_future
    for (auto it = submit_futures.begin(); it != submit_futures.end();)
    {
        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            if (!configure.claude.enable)
            {
                std::string response = it->get();

                it = submit_futures.erase(it);
                auto tmp_code = Utils::GetAllCodesFromText(response);
                save(convid);
                bot->Save(convid);
                for (auto& i : tmp_code)
                {
                    std::size_t pos = i.find('\n'); // 查找第一个换行符的位置
                    std::string codetype;
                    if (pos != std::string::npos)
                    {
                        // 如果找到了换行符
                        codetype = i.substr(0, pos);
                        i = i.substr(pos + 1); // 删除第一行
                    }
                    if (!is_valid_text(codetype))
                        codetype = "Unknown";
                    if (codes.find(codetype) != codes.end())
                    {
                        codes[codetype].emplace_back(i);
                    }
                    else
                    {
                        codes.insert({codetype, {i}});
                    }

                    ImGui::SetClipboardText(i.c_str());
                }
                auto tcodes = StringExecutor::GetCodes(response);
                for (auto& code : tcodes)
                {
                    for (auto& it : code.Content)
                    {
                        codes[code.Type].emplace_back(it);
                    }
                }
                /*if (vits && vitsData.enable)
                {
                    if (configure.baiDuTranslator.enable)
                    {
                        std::string VitsText = translator->translate(Utils::ExtractNormalText(response),
                                                                     vitsData.lanType);
                        Vits(VitsText);
                    }
                    else
                    {
                        std::string VitsText = Utils::ExtractNormalText(response);
                        Vits(VitsText);
                    }
                }*/
                if (whisperData.enable)
                {
                    listener->listen();
                }
            }
            else
            {
                it = submit_futures.erase(it);
            }
        }
        else
        {
            ++it;
        }
    }
    ImGui::PopStyleVar(11);
}

void Application::RenderConversationBox()
{
    UniversalStyle();
    ImGui::Begin(reinterpret_cast<const char*>(u8"会话"), NULL,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav);
    ImGui::PushID(("button_" + std::to_string(TextureCache["add"])).c_str());
    if (ImGui::ImageButton(("button_" + std::to_string(TextureCache["add"])).c_str(), TextureCache["add"],
                           ImVec2(16, 16)))
    {
        // 显示输入框
        show_input_box = true;
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Text(reinterpret_cast<const char*>(u8"新建会话"));
    for (const auto& conversation : conversations)
    {
        ImGui::BeginGroup();
        // 消息框
        ImGui::Image(TextureCache["message"], ImVec2(32, 32),
                     ImVec2(0, 0),
                     ImVec2(1, 1));


        // 消息文本
        ImGui::SameLine();
        ImGui::PushID(conversation.c_str());
        if (ImGui::Button(conversation.c_str(), ImVec2(100, 32)))
        {
            convid = conversation;
            bot->Load(convid);
            load(convid);
        }
        ImGui::PopID();
        // 删除按钮
        ImGui::SameLine();
        ImGui::PushID(conversation.c_str());
        if (ImGui::ImageButton("##" + TextureCache["del"], TextureCache["del"], ImVec2(24, 24)) &&
            conversations.size() > 1)
        {
            convid = conversation;
            bot->Del(convid);
            del(convid);
            bot->Load(convid);
            load(convid);
            ImGui::EndGroup();
            break;
        }
        ImGui::PopID();
        ImGui::EndGroup();
    }
    ImGui::End();
}

void Application::RenderConfigBox()
{
    UniversalStyle();
    static bool no_key = false;
    static bool first = false;
    switch (state)
    {
    case State::NO_BOT_KEY:
        no_key = true;
        state = State::OK;
        break;
    case State::FIRST_USE:
        first = true;
        state = State::OK;
        break;

    default:
        break;
    }
    static bool should_restart = false;

    ImGui::Begin(reinterpret_cast<const char*>(u8"配置"), NULL,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);
    if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"音频设置")))
    {
        static std::vector<std::string> devices = Utils::GetMicrophoneDevices();
        if (ImGui::BeginCombo("##输入设备", configure.MicroPhoneID.c_str()))
        {
            for (const auto& device : devices)
            {
                bool is_selected = (configure.MicroPhoneID == device);
                if (ImGui::Selectable(device.c_str(), is_selected))
                {
                    configure.MicroPhoneID = device;
                    if (listener && whisper)
                        listener->ChangeDevice(configure.MicroPhoneID);
                    Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
                }
                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    // 显示 ChatBot 配置
    // LLM 功能配置界面
    if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"LLM功能")))
    {
        // ========== 第一部分：LLM 选择区域 ==========
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.6f, 1.0f, 1.0f));
        ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"选择大语言模型"));
        ImGui::PopStyleColor();
        ImGui::Separator();

        // LLM 选择的辅助函数
        auto SelectLLM = [&](const char* name, bool& enabled, bool isBeta = false)
        {
            // 添加 Beta 标签
            if (isBeta)
            {
                ImGui::Checkbox(name, &enabled);
                if (enabled)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(实验性功能)");
                }
            }
            else
            {
                ImGui::Checkbox(name, &enabled);
            }

            // 如果当前模型启用，禁用其它所有模型
            if (enabled)
            {
                // 禁用所有内置模型
                if (&enabled != &configure.claude.enable) configure.claude.enable = false;
                if (&enabled != &configure.gemini.enable) configure.gemini.enable = false;
                if (&enabled != &configure.openAi.enable) configure.openAi.enable = false;
                if (&enabled != &configure.mistral.enable) configure.mistral.enable = false;
                if (&enabled != &configure.grok.enable) configure.grok.enable = false;
                if (&enabled != &configure.claudeAPI.enable) configure.claudeAPI.enable = false;
                if (&enabled != &configure.qianwen.enable) configure.qianwen.enable = false;
                if (&enabled != &configure.sparkdesk.enable) configure.sparkdesk.enable = false;
                if (&enabled != &configure.chatglm.enable) configure.chatglm.enable = false;
                if (&enabled != &configure.hunyuan.enable) configure.hunyuan.enable = false;
                if (&enabled != &configure.baichuan.enable) configure.baichuan.enable = false;
                if (&enabled != &configure.huoshan.enable) configure.huoshan.enable = false;

                // 禁用所有自定义模型
                for (auto& it : configure.customGPTs)
                {
                    it.second.enable = false;
                }

                // 禁用所有自定义规则
                for (auto& rule : configure.customRules)
                {
                    rule.enable = false;
                }
            }
        };

        // 使用两列布局来显示模型选择
        const float columnWidth = ImGui::GetContentRegionAvail().x * 0.5f - 10.0f;

        ImGui::BeginChild("##ModelSelectLeft", ImVec2(columnWidth, 140), false);
        // 第一列内置模型选择
        SelectLLM(reinterpret_cast<const char*>(u8"使用OpenAI"), configure.openAi.enable);
        //SelectLLM(reinterpret_cast<const char*>(u8"使用Claude (Slack接口)"), configure.claude.enable);
        SelectLLM(reinterpret_cast<const char*>(u8"使用Claude"), configure.claudeAPI.enable);
        SelectLLM(reinterpret_cast<const char*>(u8"使用Gemini"), configure.gemini.enable);
        SelectLLM(reinterpret_cast<const char*>(u8"使用Grok"), configure.grok.enable);
        SelectLLM(reinterpret_cast<const char*>(u8"使用Mistral AI"), configure.mistral.enable);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##ModelSelectRight", ImVec2(columnWidth, 140), false);
        // 第二列内置模型选择 - 国内模型
        SelectLLM(reinterpret_cast<const char*>(u8"使用通义千问"), configure.qianwen.enable);
        SelectLLM(reinterpret_cast<const char*>(u8"使用讯飞星火"), configure.sparkdesk.enable);
        SelectLLM(reinterpret_cast<const char*>(u8"使用ChatGLM"), configure.chatglm.enable);
        SelectLLM(reinterpret_cast<const char*>(u8"使用腾讯混元"), configure.hunyuan.enable);
        SelectLLM(reinterpret_cast<const char*>(u8"使用百川AI"), configure.baichuan.enable);
        SelectLLM(reinterpret_cast<const char*>(u8"使用火山引擎"), configure.huoshan.enable);
        ImGui::EndChild();

        // ========== 第二部分：自定义API选择区域 ==========
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.6f, 1.0f, 1.0f));
        ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"自定义API"));
        ImGui::PopStyleColor();
        ImGui::Separator();

        // 添加自定义API按钮和下拉菜单
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));

        // 添加API下拉菜单
        if (ImGui::Button(reinterpret_cast<const char*>(u8"+ 添加自定义API"), ImVec2(150, 28)))
        {
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"添加API选项"));
        }

        // 创建下拉菜单
        static bool showAddAPIMenu = false;
        static bool showAddAPITemplateMenu = false;
        if (ImGui::BeginPopup(reinterpret_cast<const char*>(u8"添加API选项")))
        {
            if (ImGui::MenuItem(reinterpret_cast<const char*>(u8"创建标准GPT接口")))
            {
                showAddAPIMenu = true;
            }

            if (ImGui::MenuItem(reinterpret_cast<const char*>(u8"从规则模板创建")))
            {
                showAddAPITemplateMenu = true;
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        // 应用按钮
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
        if (ImGui::Button(reinterpret_cast<const char*>(u8"应用当前模型"), ImVec2(150, 28)))
        {
            CreateBot();
            Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
        }
        ImGui::PopStyleColor(3);

        // ========== 第三部分：APIs列表（标准GPT和自定义规则） ==========

        // 创建两个标签页：标准GPT接口和自定义规则
        static int currentTab = 0;
        ImGui::BeginTabBar("##APITabs", ImGuiTabBarFlags_None);

        // ============ 标准GPT接口标签页 ============
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"标准GPT接口")))
        {
            // 计算动态高度
            static int filteredItemCount = 0;
            float lineHeight = ImGui::GetTextLineHeightWithSpacing() * 1.5;
            float childHeight = (filteredItemCount < 8 ? filteredItemCount : 8) * lineHeight + lineHeight;

            // 搜索框
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
            ImGui::InputTextWithHint("##搜索", reinterpret_cast<const char*>(u8"搜索API..."),
                                     GetBufferByName("search").buffer, TEXT_BUFFER);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();

            std::string searchStr(GetBufferByName("search").buffer);

            // 过滤函数
            auto filterMatch = [&searchStr](const std::string& name) -> bool
            {
                if (searchStr.empty())
                    return true;
                return name.find(searchStr) != std::string::npos;
            };

            // 计算过滤后的项目数
            filteredItemCount = 0;
            for (const auto& pair : configure.customGPTs)
            {
                if (filterMatch(pair.first))
                {
                    filteredItemCount++;
                }
            }

            // 自定义API列表
            ImGui::BeginChild("##自定义API", ImVec2(0, childHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            {
                // 显示自定义API选项
                for (auto& pair : configure.customGPTs)
                {
                    // 只显示匹配搜索条件的项目
                    if (!filterMatch(pair.first))
                        continue;

                    // 添加选择高亮（修复：先存储状态，只有真正推送样式时才弹出）
                    bool isEnabled = pair.second.enable;
                    if (isEnabled)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));

                    if (ImGui::Checkbox(pair.first.c_str(), &pair.second.enable))
                    {
                        if (pair.second.enable)
                        {
                            // 当启用某个自定义GPT时，禁用所有其他模型
                            configure.claude.enable = false;
                            configure.claudeAPI.enable = false;
                            configure.gemini.enable = false;
                            configure.openAi.enable = false;
                            configure.mistral.enable = false;
                            configure.qianwen.enable = false;
                            configure.sparkdesk.enable = false;
                            configure.chatglm.enable = false;
                            configure.hunyuan.enable = false;
                            configure.baichuan.enable = false;
                            configure.huoshan.enable = false;
                            configure.grok.enable = false;

                            // 禁用其他自定义GPT
                            for (auto& pair2 : configure.customGPTs)
                            {
                                if (pair2.first != pair.first)
                                {
                                    pair2.second.enable = false;
                                }
                            }

                            // 禁用所有自定义规则
                            for (auto& rule : configure.customRules)
                            {
                                rule.enable = false;
                            }
                        }
                    }

                    // 确保只有当真正推送了样式时才弹出
                    if (isEnabled)
                        ImGui::PopStyleColor();
                }
            }
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        // ============ 自定义规则标签页 ============
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"自定义规则")))
        {
            // 搜索框
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
            ImGui::InputTextWithHint("##规则搜索", reinterpret_cast<const char*>(u8"搜索规则..."),
                                     GetBufferByName("ruleSearch").buffer, TEXT_BUFFER);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();

            std::string ruleSearchStr(GetBufferByName("ruleSearch").buffer);

            // 过滤函数
            auto ruleFilterMatch = [&ruleSearchStr](const std::string& name) -> bool
            {
                if (ruleSearchStr.empty())
                    return true;
                return name.find(ruleSearchStr) != std::string::npos;
            };

            // 自定义规则列表
            ImGui::BeginChild("##自定义规则列表", ImVec2(0, 120), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            {
                for (int i = 0; i < configure.customRules.size(); i++)
                {
                    auto& rule = configure.customRules[i];

                    // 应用搜索过滤
                    if (!ruleFilterMatch(rule.name))
                        continue;

                    ImGui::PushID(i);

                    // 创建一行包含选择框和基本信息
                    ImGui::BeginGroup();

                    // 添加选择高亮
                    bool isEnabled = rule.enable;
                    if (isEnabled)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));

                    // 复选框和规则名
                    if (ImGui::Checkbox("##选择", &rule.enable))
                    {
                        if (rule.enable)
                        {
                            // 当启用某个自定义规则时，禁用所有其他模型
                            configure.claude.enable = false;
                            configure.claudeAPI.enable = false;
                            configure.gemini.enable = false;
                            configure.openAi.enable = false;
                            configure.mistral.enable = false;
                            configure.qianwen.enable = false;
                            configure.sparkdesk.enable = false;
                            configure.chatglm.enable = false;
                            configure.hunyuan.enable = false;
                            configure.baichuan.enable = false;
                            configure.huoshan.enable = false;
                            configure.grok.enable = false;

                            // 禁用所有自定义API
                            for (auto& pair2 : configure.customGPTs)
                            {
                                pair2.second.enable = false;
                            }

                            // 禁用其他自定义规则
                            for (int j = 0; j < configure.customRules.size(); j++)
                            {
                                if (j != i)
                                {
                                    configure.customRules[j].enable = false;
                                }
                            }
                        }
                    }
                    ImGui::SameLine();

                    // 规则名称
                    ImGui::Text("%s", rule.name.c_str());
                    ImGui::SameLine();

                    // 添加模型信息标签
                    if (!rule.model.empty())
                    {
                        ImGui::TextDisabled("[%s]", rule.model.c_str());
                    }

                    // 确保只有当真正推送了样式时才弹出
                    if (isEnabled)
                        ImGui::PopStyleColor();

                    // 添加右侧编辑和删除按钮
                    float buttonWidth = 60.0f;
                    float rightAlignPos = ImGui::GetContentRegionAvail().x - buttonWidth * 2 - 8.0f;
                    ImGui::SameLine(rightAlignPos);

                    // 编辑按钮
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 0.9f));
                    if (ImGui::Button(reinterpret_cast<const char*>(u8"编辑##edit"), ImVec2(buttonWidth, 0)))
                    {
                        ImGui::OpenPopup(
                            (std::string(reinterpret_cast<const char*>(u8"编辑规则##")) + std::to_string(i)).c_str());
                    }
                    ImGui::PopStyleColor(2);

                    // 编辑弹窗
                    if (ImGui::BeginPopupModal(
                        (std::string(reinterpret_cast<const char*>(u8"编辑规则##")) + std::to_string(i)).c_str(), NULL,
                        ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        // 获取屏幕高度的70%作为滚动区域的高度
                        float availableHeight = ImGui::GetIO().DisplaySize.y * 0.7f;

                        // 创建一个可滚动区域
                        ImGui::BeginChild("##滚动区域编辑", ImVec2(500, availableHeight),
                                          true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

                        ImGui::Indent(20.0f);
                        EditCustomRule(rule);
                        ImGui::Unindent(20.0f);

                        ImGui::EndChild();

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
                        if (ImGui::Button(reinterpret_cast<const char*>(u8"保存"), ImVec2(120, 28)))
                        {
                            Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::PopStyleColor(2);

                        ImGui::SameLine();

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.9f));
                        if (ImGui::Button(reinterpret_cast<const char*>(u8"取消"), ImVec2(120, 28)))
                        {
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::PopStyleColor(2);

                        ImGui::EndPopup();
                    }

                    ImGui::SameLine();

                    // 删除按钮
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 0.9f));
                    if (ImGui::Button(reinterpret_cast<const char*>(u8"删除##delete"), ImVec2(buttonWidth, 0)))
                    {
                        rule.enable = false;
                        configure.customRules.erase(configure.customRules.begin() + i);
                        Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
                        ImGui::PopStyleColor(2);
                        ImGui::EndGroup();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopStyleColor(2);

                    ImGui::EndGroup();
                    ImGui::PopID();

                    // 添加分隔线
                    ImGui::Separator();
                }
            }
            ImGui::EndChild();

            // 操作按钮区域
            float buttonWidth = ImGui::GetContentRegionAvail().x * 0.48f;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"从模板创建规则"), ImVec2(buttonWidth, 28)))
            {
                showAddAPITemplateMenu = true;
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"刷新规则列表"), ImVec2(buttonWidth, 28)))
            {
                // 保存配置并加载
                Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
            }
            ImGui::PopStyleColor(2);

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();

        // ========== 第四部分：API配置区域 ==========
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.6f, 1.0f, 1.0f));
        ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"API配置"));
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImGui::BeginChild("##API配置", ImVec2(0, 180), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        // 显示密码输入框的辅助函数
        auto ShowPasswordInput = [&](const char* label, std::string& value, const char* buffer_name)
        {
            static std::map<std::string, bool> showPasswords;
            static std::map<std::string, bool> clicked;
            static std::map<std::string, double> lastInputTimes;

            std::string key = label; // 用标签作为唯一键
            double currentTime = ImGui::GetTime();

            // 初始化状态（如果不存在）
            if (showPasswords.find(key) == showPasswords.end())
            {
                showPasswords[key] = false;
                clicked[key] = false;
                lastInputTimes[key] = 0.0;
            }

            // 复制值到缓冲区
            strcpy_s(GetBufferByName(buffer_name).buffer, value.c_str());

            // 自动隐藏密码
            if (currentTime - lastInputTimes[key] > 0.5)
            {
                showPasswords[key] = false;
            }

            // 设置输入框的样式
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));

            // 根据状态显示输入框
            if (showPasswords[key] || clicked[key])
            {
                if (ImGui::InputText(label, GetBufferByName(buffer_name).buffer, sizeof(input_buffer)))
                {
                    value = GetBufferByName(buffer_name).buffer;
                }
            }
            else
            {
                if (ImGui::InputText(label, GetBufferByName(buffer_name).buffer, sizeof(input_buffer),
                                     ImGuiInputTextFlags_Password))
                {
                    value = GetBufferByName(buffer_name).buffer;
                    showPasswords[key] = true;
                    lastInputTimes[key] = currentTime;
                }
            }

            ImGui::PopStyleVar();

            // 眼睛按钮（切换密码显示）
            ImGui::SameLine();
            ImGui::PushID(("button_eye_" + key).c_str());
            if (ImGui::ImageButton("##" + TextureCache["eye"], TextureCache["eye"],
                                   ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1),
                                   ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)))
            {
                clicked[key] = !clicked[key];
            }
            ImGui::PopID();
        };

        // 显示普通输入框的辅助函数
        auto ShowTextInput = [&](const char* label, std::string& value, const char* buffer_name)
        {
            strcpy_s(GetBufferByName(buffer_name).buffer, value.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(label, GetBufferByName(buffer_name).buffer, TEXT_BUFFER))
            {
                value = GetBufferByName(buffer_name).buffer;
            }
            ImGui::PopStyleVar();
        };

        auto ModelController = [&](const char* label, std::string& model, std::vector<std::string>& models)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));

            // Check if current model is in the list
            bool modelInList = std::find(models.begin(), models.end(), model) != models.end();

            // Create a horizontal group for the combo and button
            ImGui::BeginGroup();

            // Display combo box with model list
            if (ImGui::BeginCombo(reinterpret_cast<const char*>(u8"可用模型"), model.c_str()))
            {
                for (const auto& m : models)
                {
                    bool isSelected = (model == m);
                    if (ImGui::Selectable(m.c_str(), isSelected))
                    {
                        model = m;
                    }
                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            strcpy_s(GetBufferByName("model").buffer, model.c_str());

            if (ImGui::InputText(label, GetBufferByName("model").buffer, TEXT_BUFFER))

            {
                model = GetBufferByName("model").buffer;
            }

            if (!modelInList && model.length() > 0)
            {
                ImGui::SameLine();
                if (ImGui::Button(reinterpret_cast<const char*>(u8"添加"), ImVec2(60, 28)))
                {
                    models.push_back(model);
                }
            }

            ImGui::EndGroup();

            ImGui::PopStyleVar();
        };

        // 根据当前选择的模型显示相应配置
        bool configDisplayed = false;

        // 检查是否有自定义规则被启用
        for (auto& rule : configure.customRules)
        {
            if (rule.enable)
            {
                // 自定义规则配置UI
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
                ImGui::TextUnformatted((reinterpret_cast<const char*>(u8"自定义规则: ") + rule.name).c_str());
                ImGui::PopStyleColor();

                /*// 基本信息部分
                if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"基本信息")))
                {
                    // 允许修改名称
                    ShowTextInput(reinterpret_cast<const char*>(u8"规则名称"), rule.name, "ruleName");
                                        ShowTextInput(reinterpret_cast<const char*>(u8"API路径"), rule.apiPath, "ruleApiPath");

                    ImGui::Checkbox(reinterpret_cast<const char*>(u8"支持系统角色"), &rule.supportSystemRole);
                }*/
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), rule.model, "ruleModel");
                ShowTextInput(reinterpret_cast<const char*>(u8"API地址"), rule.apiPath, "api");
                ModelController(reinterpret_cast<const char*>(u8"模型"), rule.model, rule.supportModels);

                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), rule.apiKeyRole.key, "ruleApiKey");
                if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"变量管理")))
                {
                    ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.2f, 1.0f), reinterpret_cast<const char*>(u8"变量:"));

                    ImGui::BeginChild("##VarsList", ImVec2(0, 150), true);
                    if (rule.vars.empty())
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"暂无变量"));
                    int idx = 0;
                    for (auto& var : rule.vars)
                    {
                        ImGui::PushID(idx++);
                        strcpy_s(GetBufferByName("varName").buffer, TEXT_BUFFER, var.name.c_str());
                        strcpy_s(GetBufferByName("varValue").buffer, TEXT_BUFFER, var.value.c_str());

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
                        ImGui::TextUnformatted((var.name+": ").c_str());


                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
                        if (ImGui::InputText(reinterpret_cast<const char*>(u8"变量值"), GetBufferByName("varValue").buffer,
                                             TEXT_BUFFER))
                            var.value = GetBufferByName("varValue").buffer;
                        ImGui::PopStyleVar();
                        ImGui::PopID();
                    }

                    ImGui::EndChild();
                }
                /*// API密钥配置部分
                if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"API密钥配置")))
                {
                                        ShowTextInput(reinterpret_cast<const char*>(u8"角色"), rule.apiKeyRole.role, "ruleApiRole");
                    ShowTextInput(reinterpret_cast<const char*>(u8"请求头"), rule.apiKeyRole.header, "ruleApiHeader");
                }*/

                /*
                // 响应配置部分
                if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"响应配置")))
                {
                    ShowTextInput(reinterpret_cast<const char*>(u8"后缀"), rule.responseRole.suffix, "ruleRespSuffix");
                    ShowTextInput(reinterpret_cast<const char*>(u8"内容路径"), rule.responseRole.content,
                                  "ruleRespContent");
                    ShowTextInput(reinterpret_cast<const char*>(u8"回调"), rule.responseRole.callback,
                                  "ruleRespCallback");
                    ShowTextInput(reinterpret_cast<const char*>(u8"停止标记"), rule.responseRole.stopFlag,
                                  "ruleRespStopFlag");
                }

                // 提示角色配置部分
                if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"提示角色配置")))
                {
                    ShowTextInput(reinterpret_cast<const char*>(u8"提示路径"), rule.promptRole.prompt.path,
                                  "rulePromptPath");
                    ShowTextInput(reinterpret_cast<const char*>(u8"提示后缀"), rule.promptRole.prompt.suffix,
                                  "rulePromptSuffix");
                    ShowTextInput(reinterpret_cast<const char*>(u8"角色路径"), rule.promptRole.role.path, "ruleRolePath");
                    ShowTextInput(reinterpret_cast<const char*>(u8"角色后缀"), rule.promptRole.role.suffix,
                                  "ruleRoleSuffix");
                }
                */

                // 删除按钮
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 0.9f));
                if (ImGui::Button(reinterpret_cast<const char*>(u8"删除此规则"), ImVec2(120, 28)))
                {
                    rule.enable = false;
                    // 标记要删除的规则索引
                    int ruleIndexToDelete = &rule - &configure.customRules[0];
                    if (ruleIndexToDelete >= 0 && ruleIndexToDelete < configure.customRules.size())
                    {
                        configure.customRules.erase(configure.customRules.begin() + ruleIndexToDelete);
                    }
                }
                ImGui::PopStyleColor(2);

                configDisplayed = true;
                break; // 只显示当前启用的规则
            }
        }

        // 如果没有自定义规则被启用，检查是否有标准API被启用
        if (!configDisplayed)
        {
            if (configure.openAi.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.openAi.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.openAi.model,
                                configure.openAi.supportModels);

                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.openAi.api_key, "apiKey");
                ImGui::Checkbox(reinterpret_cast<const char*>(u8"使用远程接入点"), &configure.openAi.useWebProxy);
                if (configure.openAi.useWebProxy)
                {
                    ShowTextInput(reinterpret_cast<const char*>(u8"接入点"), configure.openAi._endPoint, "endPoint");
                }
                else
                {
                    ShowTextInput(reinterpret_cast<const char*>(u8"代理"), configure.openAi.proxy, "proxy");
                }
            }
            else if (configure.claudeAPI.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.claudeAPI.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.claudeAPI.model,
                                configure.claudeAPI.supportModels);
                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.claudeAPI.apiKey, "apiKey");
                ShowTextInput(reinterpret_cast<const char*>(u8"API版本"), configure.claudeAPI.apiVersion, "apiVersion");
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"API版本必填不可留空"));
            }
            else if (configure.gemini.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.gemini.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.gemini.model,
                                configure.gemini.supportModels);
                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.gemini._apiKey, "apiKey");
            }
            else if (configure.grok.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.grok.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.grok.model,
                                configure.grok.supportModels);
                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.grok.api_key, "apiKey");
            }

            else if (configure.mistral.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.mistral.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.mistral.model,
                                configure.mistral.supportModels);
                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.mistral.api_key, "apiKey");
            }

            else if (configure.qianwen.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.qianwen.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.qianwen.model,
                                configure.qianwen.supportModels);
                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.qianwen.api_key, "apiKey");
            }

            else if (configure.baichuan.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.baichuan.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.baichuan.model,
                                configure.baichuan.supportModels);
                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.baichuan.api_key, "apiKey");
            }

            else if (configure.chatglm.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.chatglm.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.chatglm.model,
                                configure.chatglm.supportModels);
                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.chatglm.api_key, "apiKey");
            }

            else if (configure.huoshan.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.huoshan.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.huoshan.model,
                                configure.huoshan.supportModels);
                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.huoshan.api_key, "apiKey");
            }

            else if (configure.hunyuan.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.hunyuan.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.hunyuan.model,
                                configure.hunyuan.supportModels);
                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.hunyuan.api_key, "apiKey");
            }

            else if (configure.sparkdesk.enable)
            {
                //ShowTextInput(reinterpret_cast<const char*>(u8"模型"), configure.sparkdesk.model, "model");
                ModelController(reinterpret_cast<const char*>(u8"模型"), configure.sparkdesk.model,
                                configure.sparkdesk.supportModels);
                ShowPasswordInput(reinterpret_cast<const char*>(u8"API密钥"), configure.sparkdesk.api_key, "apiKey");
            }
            else
            {
                for (auto& [name, cdata] : configure.customGPTs)
                {
                    if (cdata.enable)
                    {
                        // 自定义GPT标题
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
                        ImGui::TextUnformatted((reinterpret_cast<const char*>(u8"自定义API: ") + name).c_str());
                        ImGui::PopStyleColor();

                        // 本地模型切换
                        ImGui::Checkbox(reinterpret_cast<const char*>(u8"使用本地模型"), &cdata.useLocalModel);

                        if (!cdata.useLocalModel)
                        {
                            // 远程API配置
                            static int currentItem = 0;
                            const char* items[] = {
                                reinterpret_cast<const char*>(u8"手动配置"),
                                reinterpret_cast<const char*>(u8"从Curl命令解析")
                            };

                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                            ImGui::Combo(reinterpret_cast<const char*>(u8"配置方式"), &currentItem, items,
                                         IM_ARRAYSIZE(items));
                            ImGui::PopStyleVar();

                            if (currentItem == 1) // 从Curl命令解析
                            {
                                static std::string value;
                                strcpy_s(GetBufferByName("curl_parser").buffer, value.c_str());

                                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                                if (ImGui::InputTextMultiline("##CurlParser", GetBufferByName("curl_parser").buffer,
                                                              TEXT_BUFFER, ImVec2(-1, 64),
                                                              ImGuiInputTextFlags_AllowTabInput |
                                                              ImGuiInputTextFlags_CtrlEnterForNewLine))
                                {
                                    value = GetBufferByName("curl_parser").buffer;
                                }
                                ImGui::PopStyleVar();

                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 0.8f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 0.9f));
                                if (ImGui::Button(reinterpret_cast<const char*>(u8"解析Curl命令"), ImVec2(120, 28)))
                                {
                                    auto parserFromCurl = [](std::string& cmd)
                                    {
                                        GPTLikeCreateInfo gpt;
                                        gpt.enable = true;
                                        gpt.useLocalModel = false;

                                        // 解析URL
                                        size_t urlStart = cmd.find("\"") + 1;
                                        size_t urlEnd = cmd.find("\"", urlStart);
                                        if (urlStart != std::string::npos && urlEnd != std::string::npos)
                                        {
                                            std::string url = cmd.substr(urlStart, urlEnd - urlStart);

                                            // 分离主机名和API路径
                                            size_t protocolEnd = url.find("://");
                                            size_t pathStart = std::string::npos;
                                            if (protocolEnd != std::string::npos)
                                            {
                                                pathStart = url.find("/", protocolEnd + 3);
                                            }

                                            if (pathStart != std::string::npos)
                                            {
                                                gpt.apiHost = url.substr(0, pathStart);
                                                gpt.apiPath = url.substr(pathStart);
                                            }
                                        }

                                        // 解析Authorization头部获取API密钥
                                        std::string authMarker = "Authorization: Bearer ";
                                        size_t authStart = cmd.find(authMarker);
                                        if (authStart != std::string::npos)
                                        {
                                            authStart += authMarker.length();
                                            size_t authEnd = cmd.find("\"", authStart);
                                            if (authEnd != std::string::npos)
                                            {
                                                gpt.api_key = cmd.substr(authStart, authEnd - authStart);
                                            }
                                        }

                                        // 解析JSON数据中的model字段
                                        std::string modelMarker = "\"model\":\"";
                                        size_t modelStart = cmd.find(modelMarker);
                                        if (modelStart != std::string::npos)
                                        {
                                            modelStart += modelMarker.length();
                                            size_t modelEnd = cmd.find("\"", modelStart);
                                            if (modelEnd != std::string::npos)
                                            {
                                                gpt.model = cmd.substr(modelStart, modelEnd - modelStart);
                                            }
                                        }

                                        return gpt;
                                    };
                                    cdata = parserFromCurl(value);
                                    cdata.enable = true;
                                    cdata.useLocalModel = false;
                                    configure.customGPTs[name] = cdata;
                                    currentItem = 0; // 解析完成后切换回手动配置模式显示结果
                                }
                                ImGui::PopStyleColor(2);
                            }

                            // 无论哪种配置方式，都显示API配置信息
                            ShowPasswordInput(reinterpret_cast<const char*>(u8"API Key"), cdata.api_key, "api");
                            ShowTextInput(reinterpret_cast<const char*>(u8"API 主机"), cdata.apiHost, "apiHost");
                            ShowTextInput(reinterpret_cast<const char*>(u8"API 路径"), cdata.apiPath, "apiPath");
                            //ShowTextInput(reinterpret_cast<const char*>(u8"模型名称"), cdata.model, "model");
                            ModelController(reinterpret_cast<const char*>(u8"模型"), cdata.model, cdata.supportModels);
                        }
                        else
                        {
                            // 本地模型配置
                            ShowTextInput(reinterpret_cast<const char*>(u8"模型名称"), cdata.llamaData.model, "model");

                            // 上下文大小设置
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                            ImGui::InputInt(reinterpret_cast<const char*>(u8"上下文大小"), &cdata.llamaData.contextSize);
                            ImGui::PopStyleVar();

                            if (cdata.llamaData.contextSize < 512)
                            {
                                cdata.llamaData.contextSize = 512;
                            }

                            // 最大生成长度设置
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                            ImGui::InputInt(reinterpret_cast<const char*>(u8"最大生成长度"), &cdata.llamaData.maxTokens);
                            ImGui::PopStyleVar();

                            if (cdata.llamaData.maxTokens < 256)
                            {
                                cdata.llamaData.maxTokens = 256;
                            }
                        }

                        // 删除按钮
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 0.9f));
                        if (ImGui::Button(reinterpret_cast<const char*>(u8"删除此API"), ImVec2(120, 28)))
                        {
                            cdata.enable = false;
                            configure.customGPTs.erase(name);
                        }
                        ImGui::PopStyleColor(2);

                        configDisplayed = true;
                        break; // 只显示当前启用的自定义API
                    }
                }
            }
        }

        // 如果没有任何配置被启用，显示提示
        if (!configDisplayed)
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               reinterpret_cast<const char*>(u8"请选择一个模型或自定义API以查看配置"));
        }

        ImGui::EndChild();

        // 标准GPT接口创建弹窗
        if (showAddAPIMenu)
        {
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"请输入新的配置名字"));
            showAddAPIMenu = false;
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"请输入新的配置名字"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText(reinterpret_cast<const char*>(u8"配置名字"), GetBufferByName("newName").buffer, TEXT_BUFFER);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"确定"), ImVec2(120, 28)))
            {
                std::string name = GetBufferByName("newName").buffer;
                if (name.empty())
                {
                    name = reinterpret_cast<const char*>(u8"新配置_") + std::to_string(configure.customGPTs.size());
                }
                configure.customGPTs[name] = GPTLikeCreateInfo();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"取消"), ImVec2(120, 28)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::EndPopup();
        }

        // 从模板创建自定义规则弹窗
        if (showAddAPITemplateMenu)
        {
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"从模板创建API"));
            showAddAPITemplateMenu = false;
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"从模板创建API"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            static int selectedTemplateIdx = -1;
            static char newApiName[256] = {0};

            ImGui::InputText(reinterpret_cast<const char*>(u8"新API名称"), newApiName, sizeof(newApiName));

            ImGui::Text(reinterpret_cast<const char*>(u8"选择模板:"));
            ImGui::BeginChild("##模板选择列表", ImVec2(400, 200), true);

            int idx = 0;
            for (const auto& [name, rule] : customRuleTemplates)
            {
                if (ImGui::Selectable(name.c_str(), selectedTemplateIdx == idx))
                {
                    selectedTemplateIdx = idx;
                }
                idx++;
            }

            ImGui::EndChild();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"创建"), ImVec2(120, 28)))
            {
                if (selectedTemplateIdx >= 0)
                {
                    // 获取选中的模板
                    idx = 0;
                    for (const auto& [tplName, rule] : customRuleTemplates)
                    {
                        if (idx == selectedTemplateIdx)
                        {
                            // 获取新API名称
                            std::string apiName = newApiName;
                            if (apiName.empty())
                            {
                                apiName = reinterpret_cast<const char*>(u8"从模板创建_") + std::to_string(
                                    configure.customRules.size());
                            }

                            // 直接复制模板并添加到自定义规则
                            CustomRule newRule = rule;
                            newRule.name = apiName; // 设置新的名称

                            // 添加到自定义规则集
                            configure.customRules.push_back(newRule);

                            // 保存配置
                            Utils::SaveYaml("config.yaml", Utils::toYaml(configure));

                            // 清空输入
                            memset(newApiName, 0, sizeof(newApiName));
                            break;
                        }
                        idx++;
                    }

                    selectedTemplateIdx = -1;
                }

                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"取消"), ImVec2(120, 28)))
            {
                memset(newApiName, 0, sizeof(newApiName));
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::EndPopup();
        }

        // ========== 第五部分：自定义API模板管理 ==========
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.6f, 1.0f, 1.0f));
        ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"自定义API模板管理"));
        ImGui::PopStyleColor();
        ImGui::Separator();

        // 左右两列布局
        const float columnWidthTemplates = ImGui::GetContentRegionAvail().x * 0.55f;
        ImGui::BeginChild("##模板管理左侧", ImVec2(columnWidthTemplates, 250), false);

        // 模板列表标题
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"模板列表"));
        ImGui::PopStyleColor();

        // 显示当前模板列表
        ImGui::BeginChild("##模板列表", ImVec2(0, 180), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        static int selectedTemplateIndex = -1;
        int index = 0;
        static std::vector<std::string> templateNames;
        templateNames.clear();

        for (const auto& [name, rule] : customRuleTemplates)
        {
            templateNames.push_back(name);

            // 如果是选中项，使用高亮样式（修复：只有真正选中时才推送和弹出样式）
            bool isSelected = selectedTemplateIndex == index;
            if (isSelected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));

            if (ImGui::Selectable(name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                selectedTemplateIndex = index;

                // 双击应用模板
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    std::string selectedName = templateNames[selectedTemplateIndex];
                    configure.customRules.push_back(customRuleTemplates[selectedName]);
                    Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
                }
            }

            // 确保只有当真正推送样式时才弹出
            if (isSelected)
                ImGui::PopStyleColor();

            index++;
        }

        ImGui::EndChild();

        // 模板操作按钮 - 使用水平均匀布局
        const float buttonWidth = (ImGui::GetContentRegionAvail().x - 10.0f) / 3.0f;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 0.9f));
        if (ImGui::Button(reinterpret_cast<const char*>(u8"导入模板"), ImVec2(buttonWidth, 28)))
        {
            typeFilters = {".bin"};
            title = reinterpret_cast<const char*>(u8"选择模板导入文件");
            fileBrowser = true;
            PathOnConfirm = [this]()
            {
                std::string selectedFile = BrowserPath;
                if (!selectedFile.empty())
                {
                    ImportCustomRuleTemplates(selectedFile);
                }
            };
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
        if (ImGui::Button(reinterpret_cast<const char*>(u8"导出模板"), ImVec2(buttonWidth, 28)))
        {
            typeFilters = {".bin"};
            title = reinterpret_cast<const char*>(u8"选择模板导出位置");
            fileBrowser = true;
            PathOnConfirm = [this]()
            {
                std::string selectedFile = BrowserPath;
                if (!selectedFile.empty())
                {
                    // 确保文件有.bin后缀
                    if (selectedFile.find(".bin") == std::string::npos)
                    {
                        selectedFile += ".bin";
                    }
                    ExportCustomRuleTemplates(selectedFile);
                }
            };
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.5f, 0.1f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.6f, 0.2f, 0.9f));
        static bool isCreatingTemplate = false;
        if (ImGui::Button(reinterpret_cast<const char*>(u8"添加模板"), ImVec2(buttonWidth, 28)))
        {
            isCreatingTemplate = true;
        }
        ImGui::PopStyleColor(2);

        ImGui::EndChild();

        ImGui::SameLine();

        // 右侧部分
        ImGui::BeginChild("##模板管理右侧", ImVec2(0, 250), false);

        // 显示模板详情
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"模板详情"));
        ImGui::PopStyleColor();

        ImGui::BeginChild("##模板详情", ImVec2(0, 180), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        // 显示选中模板的详情 - 仅展示核心信息
        if (selectedTemplateIndex >= 0 && selectedTemplateIndex < templateNames.size())
        {
            std::string selectedName = templateNames[selectedTemplateIndex];
            const auto& rule = customRuleTemplates[selectedName];

            // 显示模板名称
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.3f, 1.0f));
            ImGui::TextUnformatted((reinterpret_cast<const char*>(u8"模板名称: ") + selectedName).c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();

            // 只展示三个核心属性
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                               reinterpret_cast<const char*>(u8"作者: %s"), rule.author.c_str());
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                               reinterpret_cast<const char*>(u8"版本: %s"), rule.version.c_str());

            // 描述信息使用 TextWrapped 支持多行显示
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                               reinterpret_cast<const char*>(u8"描述:"));
            ImGui::TextWrapped("%s", rule.description.c_str());

            // 添加分隔线
            ImGui::Separator();

            // 显示模型和API路径作为额外参考信息
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.8f),
                               reinterpret_cast<const char*>(u8"模型: %s"), rule.model.c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               reinterpret_cast<const char*>(u8"请选择一个模板查看详情"));
        }

        ImGui::EndChild();

        // 如果有选择的模板，显示操作按钮
        if (selectedTemplateIndex >= 0 && selectedTemplateIndex < templateNames.size())
        {
            const float actionButtonWidth = (ImGui::GetContentRegionAvail().x - 10.0f) / 2.0f;

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"删除选中模板"), ImVec2(actionButtonWidth, 28)))
            {
                std::string selectedName = templateNames[selectedTemplateIndex];
                customRuleTemplates.erase(selectedName);
                selectedTemplateIndex = -1;
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"应用选中模板"), ImVec2(actionButtonWidth, 28)))
            {
                std::string selectedName = templateNames[selectedTemplateIndex];
                // 将模板添加到已激活的自定义规则
                configure.customRules.push_back(customRuleTemplates[selectedName]);
                // 保存配置
                Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::EndChild();

        // 新建模板弹窗
        if (isCreatingTemplate)
        {
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"新建模板"));
            isCreatingTemplate = false;
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"新建模板"), NULL))
        {
            static CustomRule newRule;

            // 获取屏幕高度的70%作为滚动区域的高度
            float availableHeight = ImGui::GetIO().DisplaySize.y * 0.7f;

            // 创建一个可滚动区域，使用鼠标滚轮滑动
            ImGui::BeginChild("##滚动区域", ImVec2(ImGui::GetContentRegionAvail().x, availableHeight),
                              true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            // 继续显示详细配置
            ImGui::Indent(20.0f);
            EditCustomRule(newRule);
            ImGui::Unindent(20.0f);

            ImGui::EndChild();

            // 按钮区域保持在滚动区域外部
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"确定"), ImVec2(120, 28)))
            {
                // 检查模板名称是否为空
                if (newRule.name.empty())
                {
                    // 如果名称为空，生成默认名称
                    newRule.name = reinterpret_cast<const char*>(u8"模板_") +
                        std::to_string(customRuleTemplates.size() + 1);
                }

                // 添加到模板库
                customRuleTemplates[newRule.name] = newRule;

                // 重置并关闭弹窗
                newRule = CustomRule();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"取消"), ImVec2(120, 28)))
            {
                // 直接关闭弹窗，不保存任何内容
                newRule = CustomRule(); // 重置规则内容
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::EndPopup();
        }

        // 从现有配置创建模板弹窗
        static bool isCreatingFromExisting = false;
        if (ImGui::Button(reinterpret_cast<const char*>(u8"从现有配置创建模板"), ImVec2(ImGui::GetContentRegionAvail().x, 28)))
        {
            isCreatingFromExisting = true;
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"选择配置来源"));
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"选择配置来源"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"请选择要转换为模板的配置："));

            static int selectedConfigIndex = -1;
            int idx = 0;

            // 显示所有自定义GPT配置
            ImGui::BeginChild("##ConfigList", ImVec2(300, 200), true);
            for (const auto& [name, gpt] : configure.customGPTs)
            {
                if (!gpt.useLocalModel) // 只显示远程API配置
                {
                    if (ImGui::Selectable(name.c_str(), selectedConfigIndex == idx))
                    {
                        selectedConfigIndex = idx;
                    }
                    idx++;
                }
            }
            ImGui::EndChild();

            // 额外的元数据输入字段
            static char metaAuthor[256] = "Ryoshi";
            static char metaVersion[256] = "1.0";
            static char metaDescription[1024] = "自定义规则";

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            ImGui::InputText(reinterpret_cast<const char*>(u8"作者"), metaAuthor, sizeof(metaAuthor));
            ImGui::InputText(reinterpret_cast<const char*>(u8"版本"), metaVersion, sizeof(metaVersion));
            ImGui::InputTextMultiline(reinterpret_cast<const char*>(u8"描述"), metaDescription, sizeof(metaDescription),
                                      ImVec2(ImGui::GetContentRegionAvail().x, 60));
            ImGui::PopStyleVar();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"创建模板"), ImVec2(120, 28)))
            {
                if (selectedConfigIndex >= 0)
                {
                    // 找到选中的配置并创建模板
                    idx = 0;
                    for (const auto& [name, gpt] : configure.customGPTs)
                    {
                        if (!gpt.useLocalModel)
                        {
                            if (idx == selectedConfigIndex)
                            {
                                // 创建新的模板
                                CustomRule newTemplate;
                                newTemplate.enable = true;
                                newTemplate.name = name;
                                newTemplate.model = gpt.model;
                                newTemplate.supportSystemRole = true;

                                // 设置元数据
                                newTemplate.author = metaAuthor;
                                newTemplate.version = metaVersion;
                                newTemplate.description = metaDescription;

                                // 构建API路径
                                newTemplate.apiPath = gpt.apiHost + gpt.apiPath;

                                // API密钥配置
                                newTemplate.apiKeyRole.key = gpt.api_key;
                                newTemplate.apiKeyRole.role = "HEADERS";
                                newTemplate.apiKeyRole.header = "Authorization: Bearer ";

                                // 响应配置 - 默认OpenAI格式
                                newTemplate.responseRole.suffix = "data: ";
                                newTemplate.responseRole.content = "choices/delta/content";
                                newTemplate.responseRole.callback = "RESPONSE";
                                newTemplate.responseRole.stopFlag = "[DONE]";

                                // 设置提示角色配置
                                newTemplate.promptRole.prompt.path = "content";
                                newTemplate.promptRole.prompt.suffix = "messages";
                                newTemplate.promptRole.role.path = "role";
                                newTemplate.promptRole.role.suffix = "messages";

                                // 添加到模板库
                                customRuleTemplates[name + "_模板"] = newTemplate;
                                break;
                            }
                            idx++;
                        }
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"取消"), ImVec2(120, 28)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::EndPopup();
        }
    }

    // 显示百度翻译配置
    if
    (ImGui::CollapsingHeader
        (
            reinterpret_cast
            <
                const char*>
            (
                u8"百度翻译(如果需要)"
            )
        )
    )
    {
        ImGui::Checkbox(reinterpret_cast<const char*>(u8"启用百度翻译"), &configure.baiDuTranslator.enable);
        ImGui::InputText("BaiDu App ID", configure.baiDuTranslator.appId.data(),
                         TEXT_BUFFER);
        static bool showbaiduPassword = false, baiduclicked = false;
        static double baidulastInputTime = 0.0;

        double baiducurrentTime = ImGui::GetTime();
        strcpy_s(GetBufferByName("api").buffer, configure.baiDuTranslator.APIKey.c_str());
        if (baiducurrentTime - baidulastInputTime > 0.5)
        {
            showbaiduPassword = false;
        }
        if (showbaiduPassword || baiduclicked)
        {
            if (ImGui::InputText("BaiDu API Key", GetBufferByName("api").buffer,
                                 sizeof(GetBufferByName("api").buffer)))
            {
                configure.baiDuTranslator.APIKey = GetBufferByName("api").buffer;
            }
        }
        else
        {
            if (ImGui::InputText("BaiDu API Key", GetBufferByName("api").buffer,
                                 sizeof(GetBufferByName("api").buffer), ImGuiInputTextFlags_Password))
            {
                configure.baiDuTranslator.APIKey = GetBufferByName("api").buffer;
                showbaiduPassword = true;
                baidulastInputTime = ImGui::GetTime();
            }
        }
        ImGui::SameLine();
        ImGui::PushID(("button_" + std::to_string(TextureCache["eye"])).c_str());
        if (ImGui::ImageButton("##" + TextureCache["eye"], (TextureCache["eye"]),
                               ImVec2(16, 16),
                               ImVec2(0, 0),
                               ImVec2(1, 1),
                               ImVec4(0, 0, 0, 0),
                               ImVec4(1, 1, 1, 1)))
        {
            baiduclicked = !baiduclicked;
        }
        ImGui::PopID();

        if (ImGui::InputText(reinterpret_cast<const char*>(u8"对百度使用的代理"),
                             GetBufferByName("proxy").buffer,
                             TEXT_BUFFER))
        {
            configure.baiDuTranslator.proxy = GetBufferByName("proxy").buffer;
        }
    }

    // 显示 VITS 配置
    if
    (ImGui::CollapsingHeader
        (
            reinterpret_cast
            <
                const char*>
            (
                u8"语音聊天功能(基于VITS)"
            )
        )
    )
    {
        strcpy_s(GetBufferByName("lan").buffer, configure.vits.lanType.c_str());
        strcpy_s(GetBufferByName("endPoint").buffer, configure.vits.apiEndPoint.c_str());
        ImGui::Checkbox(reinterpret_cast<const char*>(u8"启用Vits"), &configure.vits.enable);
        ImGui::Checkbox(reinterpret_cast<const char*>(u8"GPT-SoVITS"), &configure.vits.UseGptSovite);
        if (ImGui::InputText(reinterpret_cast<const char*>(u8"语言类型"), GetBufferByName("lan").buffer, TEXT_BUFFER))
        {
            configure.vits.lanType = GetBufferByName("lan").buffer;
        }
        if (!configure.vits.UseGptSovite)
        {
            vitsModels = Utils::GetDirectories(model + VitsPath);
            auto it = std::find(vitsModels.begin(), vitsModels.end(), configure.vits.modelName);
            if (it != vitsModels.end())
            {
                // 找到了,计算索引
                SelectIndices["vits"] = std::distance(vitsModels.begin(), it);
                if (UFile::Exists(configure.vits.config))
                {
                    std::string config;
                    config = Utils::ReadFile(configure.vits.config);
                    if (!config.empty())
                    {
                        json _config = json::parse(config);
                        if (_config["speakers"] != nullptr)
                            speakers = Utils::JsonArrayToStringVector(_config["speakers"]);
                        else if (_config["data"]["spk2id"] != nullptr)
                            speakers = Utils::JsonDictToStringVector(_config["data"]["spk2id"]);
                    }
                }
            }
            else
            {
                // 没找到,设置为0
                SelectIndices["vits"] = 0;
            }

            static char search_text[256] = "";
            /*        ImGui::InputText(reinterpret_cast<const char *>(u8"Vits 模型位置"), configure.vits.model.data(),
                                     TEXT_BUFFER);*/
            // 开始下拉列表
            if (ImGui::BeginCombo(reinterpret_cast<const char*>(u8"模型"),
                                  Utils::GetFileName(vitsModels[SelectIndices["vits"]]).c_str()))
            {
                // 在下拉框中添加一个文本输入框
                ImGui::InputTextWithHint("##Search1", reinterpret_cast<const char*>(u8"搜索"), search_text,
                                         sizeof(search_text));

                // 遍历所有选项
                for (int i = 0; i < vitsModels.size(); i++)
                {
                    // 如果搜索关键字为空，或者当前选项匹配搜索关键字
                    if (search_text[0] == '\0' ||
                        strstr(Utils::GetFileName(vitsModels[i]).c_str(), search_text) != nullptr)
                    {
                        bool is_selected = (SelectIndices["vits"] == i);
                        if (ImGui::Selectable(Utils::GetFileName(vitsModels[i]).c_str(), is_selected))
                        {
                            SelectIndices["vits"] = i;
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                }
                configure.vits.modelName = vitsModels[SelectIndices["vits"]].c_str();
                // 检查vitsModels数组不为空并且当前项不为"empty"
                if (vitsModels.size() > 0 && vitsModels[SelectIndices["vits"]] != "empty")
                {
                    // 获取.pth和.json文件
                    std::vector<std::string> pth_files = Utils::GetFilesWithExt(
                        vitsModels[SelectIndices["vits"]] + "/",
                        ".pth");
                    std::vector<std::string> json_files = Utils::GetFilesWithExt(
                        vitsModels[SelectIndices["vits"]] + "/",
                        ".json");

                    // 设置模型和配置文件
                    configure.vits.model = Utils::GetDefaultIfEmpty(pth_files, "model.pth");
                    configure.vits.config = Utils::GetDefaultIfEmpty(json_files, "config.json");

                    // 检查配置文件是否存在
                    if (UFile::Exists(configure.vits.config))
                    {
                        // 读取配置文件
                        std::string config = Utils::ReadFile(configure.vits.config);

                        if (!config.empty())
                        {
                            // 解析配置,获取speakers
                            json _config = json::parse(config);
                            if (_config["speakers"] != nullptr)
                                speakers = Utils::JsonArrayToStringVector(_config["speakers"]);
                            else if (_config["data"]["spk2id"] != nullptr)
                                speakers = Utils::JsonDictToStringVector(_config["data"]["spk2id"]);
                        }
                        configure.vits.speaker_id = 0;
                    }
                }
                ImGui::EndCombo();
            }
            // 开始下拉列表
            if (vitsModels[SelectIndices["vits"]] != "empty")
            {
                if (ImGui::BeginCombo(reinterpret_cast<const char*>(u8"角色"),
                                      speakers[configure.vits.speaker_id].c_str()))
                {
                    // 在下拉框中添加一个文本输入框
                    ImGui::InputTextWithHint("##Search", reinterpret_cast<const char*>(u8"搜索"), search_text,
                                             sizeof(search_text));

                    // 遍历所有选项
                    for (int i = 0; i < speakers.size(); i++)
                    {
                        // 如果搜索关键字为空，或者当前选项匹配搜索关键字
                        if (search_text[0] == '\0' || strstr(speakers[i].c_str(), search_text) != nullptr)
                        {
                            bool is_selected = (configure.vits.speaker_id == i);
                            if (ImGui::Selectable(speakers[i].c_str(), is_selected))
                            {
                                configure.vits.speaker_id = i;
                            }
                            if (is_selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }
        else
        {
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"GPT-SoVITS Web API地址"),
                                 GetBufferByName("endPoint").buffer,
                                 TEXT_BUFFER))
            {
                configure.vits.apiEndPoint = GetBufferByName("endPoint").buffer;
            }
        }
    }

#ifdef WIN32
    if
    (ImGui::CollapsingHeader
        (
            reinterpret_cast
            <
                const char*>
            (
                u8"桌面宠物(基于Live2D)"
            )
        )
    )
    {
        //live2dModel = Utils::GetDirectories(model + Live2DPath);
        /*auto it = std::find(live2dModel.begin(), live2dModel.end(), configure.live2D.model);
        if (it != live2dModel.end() && live2dModel.size() > 1)
        {
            SelectIndices["Live2D"] = std::distance(live2dModel.begin(), it);
            lConfigure.model = configure.live2D.model;
        }*/
        strcpy_s(GetBufferByName("live2d").buffer, configure.live2D.bin.c_str());
        ImGui::Checkbox(reinterpret_cast<const char*>(u8"启用Live2D"), &configure.live2D.enable);
        if (ImGui::InputText(reinterpret_cast<const char*>(u8"Live2D 可执行文件"), GetBufferByName("live2d").buffer,
                             TEXT_BUFFER))
        {
            configure.live2D.bin = GetBufferByName("live2d").buffer;
        }

        /*ImGui::InputFloat(reinterpret_cast<const char*>(u8"宽度缩放(400 * ScaleX)"), &lConfigure.scaleX,
                          0.1f, 1.0f, "%.2f");
        ImGui::InputFloat(reinterpret_cast<const char*>(u8"高度缩放(400 * ScaleY)"), &lConfigure.scaleY,
                          0.1f, 1.0f, "%.2f");

        if (ImGui::BeginCombo(reinterpret_cast<const char*>(u8"Live2D 模型"),
                              Utils::GetFileName(live2dModel[SelectIndices["Live2D"]]).c_str())) // 开始下拉列表
        {
            for (int i = 0; i < live2dModel.size(); i++)
            {
                bool is_selected = (SelectIndices["Live2D"] == i);
                if (ImGui::Selectable(Utils::GetFileName(live2dModel[i]).c_str(), is_selected))
                {
                    SelectIndices["Live2D"] = i;
                    if (SelectIndices["conversation"] = 0)
                    {
                        configure.live2D.enable = false;
                    }
                    else
                    {
                        configure.live2D.enable = true;
                    }
                }
                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus(); // 默认选中项
                }
            }
            configure.live2D.model = live2dModel[SelectIndices["Live2D"]].c_str();
            ImGui::EndCombo(); // 结束下拉列表
        }*/
    }
#endif
    // 显示 Whisper 配置
    if
    (ImGui::CollapsingHeader
        (
            reinterpret_cast
            <
                const char*>
            (
                u8"语音识别(基于Whisper)"
            )
        )
    )
    {
        ImGui::Checkbox(reinterpret_cast<const char*>(u8"启用Whisper"), &configure.whisper.enable);
        ImGui::Checkbox(reinterpret_cast<const char*>(u8"使用本地模型"), &configure.whisper.useLocalModel);
        auto it = std::find(tmpWhisperModel.begin(), tmpWhisperModel.end(), configure.whisper.model);
        if (it != tmpWhisperModel.end() && tmpWhisperModel.size() > 1)
        {
            SelectIndices["whisper"] = std::distance(tmpWhisperModel.begin(), it);
        }
        if (ImGui::BeginCombo(reinterpret_cast<const char*>(u8"Whisper 模型"),
                              Utils::GetFileName(whisperModel[SelectIndices["whisper"]]).c_str())) // 开始下拉列表
        {
            for (int i = 0; i < whisperModel.size(); i++)
            {
                bool is_selected = (SelectIndices["whisper"] == i);
                if (ImGui::Selectable(Utils::GetFileName(whisperModel[i]).c_str(), is_selected))
                {
                    SelectIndices["whisper"] = i;
                }
                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus(); // 默认选中项
                }
            }
            configure.whisper.model = tmpWhisperModel[SelectIndices["whisper"]];
            ImGui::EndCombo(); // 结束下拉列表
        }
    }

    //显示Stable Diffusion配置
    if
    (ImGui::CollapsingHeader
        (
            reinterpret_cast
            <
                const char*>
            (
                u8"AI绘图(基于Stable Diffusion)"
            )
        )
    )
    {
        static char search_text[256] = "";
        ImGui::InputText(reinterpret_cast<const char*>(u8"Http API"), configure.stableDiffusion.apiPath.data(),
                         TEXT_BUFFER);
        ImGui::InputText(reinterpret_cast<const char*>(u8"屏蔽词汇"), configure.stableDiffusion.negative_prompt.data(),
                         TEXT_BUFFER);
        ImGui::InputInt(reinterpret_cast<const char*>(u8"迭代次数(与质量相关)"), &configure.stableDiffusion.steps);
        ImGui::InputInt(reinterpret_cast<const char*>(u8"高"), &configure.stableDiffusion.height);
        ImGui::InputInt(reinterpret_cast<const char*>(u8"宽"), &configure.stableDiffusion.width);
        ImGui::InputFloat(reinterpret_cast<const char*>(u8"相关度"), &configure.stableDiffusion.cfg_scale, 0.1f, 1.0f,
                          "%.2f");
        auto it = std::find(sampleIndices.begin(), sampleIndices.end(), configure.stableDiffusion.sampler_index);
        if (it != sampleIndices.end())
        {
            // 找到了,计算索引
            SelectIndices["stableDiffusion"] = std::distance(sampleIndices.begin(), it);
        }
        if (ImGui::BeginCombo(reinterpret_cast<const char*>(u8"采样方法"),
                              sampleIndices[SelectIndices["stableDiffusion"]].c_str()))
        {
            // 在下拉框中添加一个文本输入框
            ImGui::InputTextWithHint("##Search", reinterpret_cast<const char*>(u8"搜索"), search_text,
                                     sizeof(search_text));

            // 遍历所有选项
            for (int i = 0; i < sampleIndices.size(); i++)
            {
                // 如果搜索关键字为空，或者当前选项匹配搜索关键字
                if (search_text[0] == '\0' || strstr(sampleIndices[i].c_str(), search_text) != nullptr)
                {
                    bool is_selected = (SelectIndices["stableDiffusion"] == i);
                    if (ImGui::Selectable(sampleIndices[i].c_str(), is_selected))
                    {
                        configure.stableDiffusion.sampler_index = sampleIndices[i];
                    }
                    if (is_selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndCombo();
        }
    }

    if
    (ImGui::CollapsingHeader
        (
            reinterpret_cast
            <
                const char*>
            (
                u8"插件列表"
            )
        )
    )
    {
        auto dirs = UDirectory::GetSubDirectories(PluginPath);
        static std::vector<bool> enablePlugins(dirs.size(), true);
        forbidLuaPlugins.clear();
        forbidLuaPlugins.resize(dirs.size());

        ImGui::BeginChild("PluginsList", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 10), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);
        for (int i = 0; i < dirs.size(); i++)
        {
            bool t = enablePlugins[i];
            ImGui::Checkbox(dirs[i].c_str(), &t);
            enablePlugins[i] = t;
        }
        ImGui::EndChild();

        for (int i = 0; i < dirs.size(); i++)
        {
            forbidLuaPlugins[i] = [&, i]() -> std::string
            {
                if (!enablePlugins[i])
                {
                    return dirs[i];
                }
                return "NULL";
            }();
        }
    }

    // 保存配置
    if
    (ImGui::Button
        (
            reinterpret_cast
            <
                const char*>
            (
                u8"保存配置"
            )
        )
    )
    {
        Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
        Utils::SaveYaml("Lconfig.yml", Utils::toYaml(lConfigure));
        for (auto& it : customRuleTemplates)
        {
            Utils::SaveYaml(CustomRulesPath + "/" + it.first + ".yaml", Utils::toYaml(it.second));
        }
        should_restart = true;
    }


    ImGui::End();
    {
        if (should_restart)
        {
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"需要重启应用"));
            should_restart = false;
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"需要重启应用"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(reinterpret_cast<const char*>(u8"您需要重新启动应用程序以使更改生效。"));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"确定"), ImVec2(80, 0)))
            {
                ImGui::CloseCurrentPopup();
                std::string executable = "CyberGirl" + exeSuffix;
                const char* argv[] = {executable.c_str(), NULL};
                execvp(executable.c_str(), const_cast<char*const *>(argv));
                //std::exit(0);
            }

            ImGui::SameLine(ImGui::GetWindowSize().x - 120 - ImGui::GetStyle().ItemSpacing.x);
            if (ImGui::Button(reinterpret_cast<const char*>(u8"取消"), ImVec2(80, 0)))
            {
                // 点击取消按钮时，关闭Popup
                ImGui::CloseCurrentPopup();
            }


            ImGui::EndPopup();
        }

        if (first)
        {
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"初始化提醒"));
            first = false;
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"初始化提醒"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(reinterpret_cast<const char*>(u8"您是第一次启动本应用,请完成下方的配置填写。"));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"确定"), ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (no_key)
        {
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"需要配置LLM服务的API Key,否则此应用无法正常使用"));
            no_key = false;
        }
        static bool showPopup = false;
        static bool showPopup2 = false;
        if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"需要配置LLM服务的API Key,否则此应用无法正常使用"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(reinterpret_cast<const char*>(u8"您配置任意一项LLM服务的API Key,或者安装本地模型 以正常启用本程序。"));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"配置API"), ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(reinterpret_cast<const char*>(u8"安装本地模型"), ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
                showPopup2 = true;
            }
            ImGui::EndPopup();
        }
        if (showPopup2)
        {
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"推理方式"));
            showPopup2 = false;
        }
        if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"推理方式"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(reinterpret_cast<const char*>(u8"选择使用的推理方式："));
            ImGui::Text(reinterpret_cast<const char*>(u8"1.使用Ollama进行推理需要额外安装Ollama并下载模型"));
            ImGui::Text(reinterpret_cast<const char*>(u8"2.使用本程序推理只需要下载模型"));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"使用Ollama"), ImVec2(120, 0)))
            {
                if (Utils::CheckCMDExist("ollama"))
                {
                    showPopup = true;
                    Utils::AsyncExecuteShell("ollama", {"run", "qwen2.5:3b"});
                    configure.openAi.enable = false;
                    configure.gemini.enable = false;
                    configure.claude.enable = false;
                    configure.grok.enable = false;
                    configure.chatglm.enable = false;
                    configure.hunyuan.enable = false;
                    configure.huoshan.enable = false;
                    configure.baichuan.enable = false;
                    configure.sparkdesk.enable = false;
                    configure.mistral.enable = false;
                    for (auto& cdata : configure.customGPTs | views::values)
                    {
                        cdata.enable = false;
                    }
                    for (auto& it : configure.customRules)
                    {
                        it.enable = false;
                    }
                    configure.customGPTs.insert({"qwen2.5:3b", GPTLikeCreateInfo()});
                    configure.customGPTs["qwen2.5:3b"].enable = true;
                    configure.customGPTs["qwen2.5:3b"].useLocalModel = false;
                    configure.customGPTs["qwen2.5:3b"].apiHost = "http://localhost:11434";
                    configure.customGPTs["qwen2.5:3b"].apiPath = "/v1/chat/completions";
                    configure.customGPTs["qwen2.5:3b"].api_key = "null";
                    configure.customGPTs["qwen2.5:3b"].model = "qwen2.5:3b";
                    Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
                }
                else
                {
                    Utils::OpenURL(OllamaLink);
                    LogWarn("ollama not found, please install it first");
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(reinterpret_cast<const char*>(u8"本程序推理"), ImVec2(120, 0)))
            {
                Installer({
                              DefaultModelInstallLink, model + "DeepSeek_R1_1_5b.gguf"
                          }, [&](Downloader*)
                          {
                              CreateBot();
                          });

                configure.customGPTs.insert({"DeepSeek_R1_1_5b", GPTLikeCreateInfo()});
                configure.customGPTs["DeepSeek_R1_1_5b"].enable = true;
                configure.customGPTs["DeepSeek_R1_1_5b"].useLocalModel = true;
                configure.customGPTs["DeepSeek_R1_1_5b"].llamaData.model = model + "DeepSeek_R1_1_5b.gguf";
                configure.customGPTs["DeepSeek_R1_1_5b"].llamaData.contextSize = 10000;
                configure.customGPTs["DeepSeek_R1_1_5b"].llamaData.maxTokens = 4096;

                Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (showPopup)
        {
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"通知"));
            showPopup = false;
        }
        if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"通知"),NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(reinterpret_cast<const char*>(u8"当控制台模型安装成功时,您可以重启此程序以开始使用"));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"重启"), ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
                std::string executable = "CyberGirl" + exeSuffix;
                const char* argv[] = {executable.c_str(), NULL};
                execvp(executable.c_str(), const_cast<char*const *>(argv));
            }
            ImGui::EndPopup();
        }
    }
}

void Application::RenderUI()
{
    ImGui::DockSpaceOverViewport();
    RuntimeDetector();
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (Downloading)
    {
        RenderDownloadBox();
    }
    if (!OnlySetting && !Downloading)
    {
        if (!loadingBot)
        {
            RenderInputBox();
            RenderPopupBox();
            RenderConfigBox();
            RenderCodeBox();
            RenderConversationBox();
        }
        RenderChatBox();
        FileChooser();
    }
    else
    {
        RenderConfigBox();
    }
}

void Application::FileChooser()
{
    static ImGui::FileBrowser fileDialog(ImGuiFileBrowserFlags_MultipleSelection | ImGuiFileBrowserFlags_NoModal);
    static std::string path;

    if (fileBrowser)
    {
        fileDialog.SetTitle(title);
        fileDialog.SetTypeFilters(typeFilters);
        fileDialog.Open();
        fileDialog.Display();
        if (fileDialog.IsOpened())
        {
            if (fileDialog.HasSelected())
            {
                // Get all selected files (if multiple selections are allowed)
                auto selectedFiles = fileDialog.GetMultiSelected();
                if (!selectedFiles.empty())
                {
                    // Return the first selected file's path
                    path = selectedFiles[0].string();
                    BrowserPath = path;
                    if (PathOnConfirm != nullptr)
                    {
                        PathOnConfirm();
                    }
                }
                fileBrowser = false;
                title = reinterpret_cast<const char*>(u8"文件选择");
                typeFilters.clear();
                PathOnConfirm = nullptr;
                fileDialog.Close();
            }
        }
        if (!fileDialog.IsOpened())
        {
            BrowserPath = path;
            fileBrowser = false;
            title = reinterpret_cast<const char*>(u8"文件选择");
            typeFilters.clear();
            PathOnConfirm = nullptr;
            fileDialog.Close();
        }
    }
}

int Application::Renderer()
{
#ifdef _WIN32
    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL)
    {
        ShowWindow(hwnd, SW_HIDE);
    }
#endif
    // 初始化GLFW
    if (!glfwInit())
    {
        // 处理初始化失败
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 创建窗口
    window = glfwCreateWindow(800, 600, VERSION.c_str(), NULL, NULL);
    if (!window)
    {
        // 处理窗口创建失败
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        // 处理初始化失败
        glfwTerminate();
        return -1;
    }
    // 初始化OpenGL
    glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 初始化ImGui
    GLFWimage images[1];
    images[0].pixels = stbi_load("Resources/icon.png", &images[0].width, &images[0].height, 0, 4); //rgba channels
    glfwSetWindowIcon(window, 1, images);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig config;
    config.OversampleH = 1;
    config.OversampleV = 1;
    config.PixelSnapH = true;
    /*auto fontRanges = []()
    {
        // 基础范围构建器
        static ImVector<ImWchar> ranges;
        ImFontGlyphRangesBuilder builder;

        // 添加中文全字符集
        builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());

        // 添加特殊数学符号 (包括积分符号∫ - Unicode: 0x222B)
        static const ImWchar math_symbols[] = {
            0x2200, 0x22FF, // 数学运算符范围
            0x2600, 0x26FF, // 杂项符号
            0x2700, 0x27BF, // 装饰符号
            0
        };
        builder.AddRanges(math_symbols);

        // 也可以直接添加单个符号
        builder.AddChar(0x222B); // 积分符号 ∫
        builder.AddChar(0x221E); // 无穷符号 ∞
        builder.AddChar(0x2211); // 求和符号 ∑
        builder.AddChar(0x221A); // 平方根 √

        // 构建最终范围
        builder.BuildRanges(&ranges);
        return ranges;
    };*/
    font = io.Fonts->AddFontFromFileTTF("Resources/font/default.otf", fontSize, &config,
                                        io.Fonts->GetGlyphRangesChineseFull());
    H1 = io.Fonts->AddFontFromFileTTF("Resources/font/default.otf", fontSize, &config,
                                      io.Fonts->GetGlyphRangesChineseFull());
    H2 = io.Fonts->AddFontFromFileTTF("Resources/font/default.otf", fontSize, &config,
                                      io.Fonts->GetGlyphRangesChineseFull());
    H3 = mdConfig.headingFormats[1].font;


    IM_ASSERT(font != NULL);
    io.DisplayFramebufferScale = ImVec2(0.8f, 0.8f);
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // 启用Docking
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGuiStyle& style = ImGui::GetStyle();
    UniversalStyle();
    //初始化插件
    // 加载Lua脚本
    auto dirs = UDirectory::GetSubDirectories(PluginPath);
    for (auto& dir : dirs)
    {
        std::string path = PluginPath + dir + "/Plugin.lua";
        if (!UFile::Exists(path))
        {
            LogWarn("Found plugin directory but Plugin.lua can not found");
            continue;
        }

        std::string luaScript = Utils::ReadFile(path);
        auto _t = Script::Create();
        _t->Initialize(PluginPath + dir);
        _t->Invoke("Initialize");
        PluginsScript1[dir] = _t;
    }


    for (auto& it : TextureCache)
    {
        it.second = LoadTexture(Resources + it.first + ".png");
    }

    // 渲染循环
    while (!glfwWindowShouldClose(window))
    {
        try
        {
            PluginRun("PreRenderer");
            glfwPollEvents();
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            RenderUI();

            PluginRun("UIRenderer");
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
        catch (const std::exception& e)
        {
            LogError("Error: {0}", e.what());
        }
    }

    // 清理资源
    AppRunning = false;
    listener.reset();
    voiceToText.reset();
    bot.reset();
    translator.reset();
    // 关闭Lua
    //lua_close(L);
    stbi_image_free(images[0].pixels);
    for (auto& it : TextureCache)
    {
        glDeleteTextures(1, &it.second);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

ImGui::MarkdownImageData Application::md_ImageCallback(ImGui::MarkdownLinkCallbackData data)
{
    // 获取图片路径
    std::string imagePath(data.link, data.linkLength);

    // 加载图片纹理（这里应调用您的LoadTexture函数）
    GLuint textureID = LoadTexture(imagePath);

    // 创建并设置图片数据
    ImGui::MarkdownImageData imageData;
    imageData.isValid = textureID != 0; // 图片有效性取决于纹理是否加载成功
    imageData.useLinkCallback = false; // 不使用链接回调
    imageData.user_texture_id = textureID; // 设置纹理ID

    // 设置默认图片尺寸
    imageData.size = ImVec2(100.0f, 100.0f);

    // 自适应调整图片大小，确保不超过可用区域
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    if (imageData.size.x > contentSize.x)
    {
        float ratio = imageData.size.y / imageData.size.x;
        imageData.size.x = contentSize.x;
        imageData.size.y = contentSize.x * ratio;
    }

    return imageData;
}

void Application::md_LinkCallback(ImGui::MarkdownLinkCallbackData data)
{
    std::string url(data.link, data.linkLength);
    if (!data.isImage)
    {
        // 打开链接（Windows平台）
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
}

bool Application::CheckFileExistence(const std::string& filePath, const std::string& fileType,
                                     const std::string& executableFile, bool isExecutable) const
{
    if (isExecutable)
    {
        if (!UFile::Exists(filePath + executableFile + exeSuffix))
        {
            LogWarn("Warning: {0} won't work because you don't have an executable \"{1}\" for {0} !",
                    fileType, filePath + executableFile + exeSuffix);
            LogWarn("Warning: Please download or compile it as an executable file and put it in this folder: {0} !",
                    filePath);
            return false;
        }
    }
    else
    {
        if (!UFile::Exists(filePath))
        {
            LogWarn("Warning: {0} won't work because you don't have an file \"{1}\" for {0} !",
                    fileType, filePath);
            return false;
        }
    }

    return true;
}


std::string Application::WhisperConvertor(const std::string& file)
{
    if (!CheckFileExistence(whisperData.model, "Whisper Model"))
    {
        LogError("Whisper Error: Whisper doesn't work because you don't have a model file: {0}",
                 this->model + WhisperPath + whisperData.model);
        return "";
    }
    std::string res;

    res = Utils::exec(
        Utils::GetAbsolutePath(bin + WhisperPath) + "main -nt -l auto -t 8 -m " + whisperData.model +
        " -f ./recorded" + std::to_string(Rnum - 1) + ".wav");

    return res;
}

void Application::WhisperModelDownload(const std::string& model)
{
    std::pair<std::string, std::string> tasks =
    {
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/" + model,
        this->model + WhisperPath + model
    };
    Installer(tasks, [&](Downloader* downloader)
    {
        whisper = downloader->IsFinished();
    });
    //return whisper;
}

void Application::WhisperExeInstaller()
{
    if (UFile::Exists(bin + WhisperPath + "Whisper.zip"))
    {
        whisper = Utils::Decompress(bin + WhisperPath + "Whisper.zip");
    }
    else
    {
        std::pair tasks =
            {whisperUrl, bin + WhisperPath + "Whisper.zip"};
        Installer(tasks, [&](Downloader* d)
        {
            whisper = d->IsFinished() && Utils::Decompress(bin + WhisperPath + "Whisper.zip");
        });
    }
    //return whisper;
}

void Application::VitsExeInstaller()
{
    if (UFile::Exists(bin + VitsConvertor + vitsFile))
    {
        vits = Utils::Decompress(bin + VitsConvertor + vitsFile);
    }
    else
    {
        std::pair tasks = {VitsConvertUrl, bin + VitsConvertor + vitsFile};
        Installer(tasks, [&](Downloader* d)
        {
            vits = d->IsFinished() && Utils::Decompress(bin + VitsConvertor + vitsFile);
        });
    }
    //return vits;
}

void Application::Installer(std::pair<std::string, std::string> task,
                            std::function<void(Downloader*)> callback)
{
    auto ds = Utils::UDownloadAsync(task);
    Downloading = true;
    downloaders.emplace_back(ds);
    ds->AddCallback(callback);
}


void Application::AddChatRecord(Ref<Chat> data)
{
    chat_history.emplace_back(data);
}

void Application::DeleteAllBotChat()
{
    auto new_end = std::remove_if(chat_history.begin(), chat_history.end(),
                                  [](const Ref<Chat>& c)
                                  {
                                      return c->flag == 1;
                                  });
    chat_history.erase(new_end, chat_history.end());
}

vector<std::shared_ptr<Application::Chat>> Application::load(std::string name)
{
    if (UFile::Exists(ConversationPath.string() + name + ".yaml"))
    {
        std::ifstream session_file(ConversationPath.string() + name + ".yaml");
        session_file.imbue(std::locale("en_US.UTF-8"));
        chat_history.clear();

        if (session_file.is_open())
        {
            try
            {
                // 从文件中读取YAML根节点
                YAML::Node rootNode = YAML::Load(session_file);

                // 检查是否包含聊天历史节点
                if (rootNode["chat_history"])
                {
                    // 递归函数，用于加载节点及其所有子节点
                    std::function<Ref<Chat>(const YAML::Node&, Chat*)> deserializeChat =
                        [&deserializeChat,this](const YAML::Node& node, Chat* parentChat) -> Ref<Chat>
                    {
                        if (!node.IsMap())
                        {
                            return nullptr;
                        }

                        // 创建新的聊天记录对象
                        Ref<Chat> chat = CreateRef<Chat>();

                        // 设置父节点
                        chat->parent = parentChat;

                        // 加载基本信息
                        if (node["flag"])
                        {
                            chat->flag = node["flag"].as<int>();
                        }

                        if (node["timestamp"])
                        {
                            chat->timestamp = node["timestamp"].as<size_t>();
                        }

                        if (node["content"])
                        {
                            chat->content = node["content"].as<std::string>();
                        }

                        // 加载可选的图片路径
                        if (node["image"])
                        {
                            chat->image = node["image"].as<std::string>();
                        }

                        // 加载历史版本记录
                        if (node["history"] && node["history"].IsSequence())
                        {
                            for (const auto& version : node["history"])
                            {
                                chat->history.emplace_back(version.as<std::string>());
                            }
                        }

                        // 加载当前版本索引
                        if (node["current_version_index"])
                        {
                            chat->currentVersionIndex = node["current_version_index"].as<int>();
                        }

                        // 重置运行时状态
                        chat->reedit = false;
                        chat->newMessage = true;
                        chat->talking = false;

                        // 加载子节点
                        if (node["children"] && node["children"].IsSequence())
                        {
                            for (const auto& childNode : node["children"])
                            {
                                auto childChat = deserializeChat(childNode, chat.get());
                                if (childChat)
                                {
                                    chat->children.push_back(childChat);
                                }
                            }
                        }

                        // 如果是机器人回复，提取代码片段
                        if (chat->flag == 1)
                        {
                            auto tcodes = StringExecutor::GetCodes(chat->content);
                            for (auto& code : tcodes)
                            {
                                for (auto& it : code.Content)
                                {
                                    codes[code.Type].emplace_back(it);
                                }
                            }
                        }

                        return chat;
                    };

                    // 加载顶级聊天记录
                    for (const auto& chatNode : rootNode["chat_history"])
                    {
                        auto chat = deserializeChat(chatNode, nullptr);
                        if (chat)
                        {
                            chat_history.push_back(chat);
                        }
                    }
                }

                session_file.close();
                LogInfo("Application : 已成功加载对话 {0}", name);
            }
            catch (const YAML::Exception& e)
            {
                LogError("Application Error: YAML解析错误: {0}", e.what());
            }
        }
        else
        {
            LogError("Application Error: 无法打开对话文件 {0}", name);
        }
    }
    else
    {
        LogError("Application Error: 对话文件不存在 {0}", name);
    }

    return chat_history;
}

void Application::del(std::string name)
{
    if (remove((ConversationPath.string() + name + ".yaml").c_str()) != 0)
    {
        LogError("Bot Error: Unable to delete session {0},{1}", name, ".");
    }
    conversations.erase(std::find(conversations.begin(), conversations.end(), name));
    SelectIndices["conversation"] = 0;
    convid = conversations[SelectIndices["conversation"]];

    LogInfo("Bot : 删除 {0} 成功", name);
}

void Application::save(std::string name, bool out)
{
    std::ofstream session_file(ConversationPath.string() + name + ".yaml");
    session_file.imbue(std::locale("en_US.UTF-8"));
    if (session_file.is_open())
    {
        // 创建 YAML 根节点
        YAML::Node rootNode;

        // 保存所有聊天记录
        YAML::Node chatHistoryNode(YAML::NodeType::Sequence);

        // 递归函数，用于保存节点及其所有子节点
        std::function<void(const Ref<Chat>&, YAML::Node&)> serializeChat =
            [&serializeChat](const Ref<Chat>& chat, YAML::Node& parentNode)
        {
            // 创建当前节点的YAML映射
            YAML::Node chatNode(YAML::NodeType::Map);

            // 保存基本信息
            chatNode["flag"] = chat->flag;
            chatNode["timestamp"] = chat->timestamp;
            chatNode["content"] = chat->content;

            // 只保存非空的图片路径
            if (!chat->image.empty())
            {
                chatNode["image"] = chat->image;
            }

            // 保存历史版本记录
            if (!chat->history.empty())
            {
                YAML::Node historyNode(YAML::NodeType::Sequence);
                for (const auto& version : chat->history)
                {
                    historyNode.push_back(version);
                }
                chatNode["history"] = historyNode;
            }

            // 保存当前版本索引
            chatNode["current_version_index"] = chat->currentVersionIndex;

            // 保存子节点
            if (!chat->children.empty())
            {
                YAML::Node childrenNode(YAML::NodeType::Sequence);

                // 遍历并递归保存所有子节点
                for (const auto& child : chat->children)
                {
                    YAML::Node childNode;
                    serializeChat(child, childNode);
                    childrenNode.push_back(childNode);
                }

                chatNode["children"] = childrenNode;
            }

            // 将节点添加到父节点
            parentNode = chatNode;
        };

        // 保存顶层聊天记录节点
        for (const auto& chat : chat_history)
        {
            if (!chat)
            {
                continue;
            }
            YAML::Node chatNode;
            serializeChat(chat, chatNode);
            chatHistoryNode.push_back(chatNode);
        }

        rootNode["chat_history"] = chatHistoryNode;

        // 将 YAML 节点写入文件
        session_file << rootNode;

        session_file.close();
        if (out)
        {
            LogInfo("Application : 已成功保存对话 {0}", name);
        }
    }
    else
    {
        if (out)
        {
            LogError("Application Error: 无法保存对话 {0}", name);
        }
    }
}


bool Application::Initialize()
{
    isPlaying = false;
    nextAudio = nullptr;
    currentAudio = nullptr;
    LogInfo("Python installed: {0}", IsPythonInstalled());
    if (!IsPythonInstalled())
    {
        UDirectory::CreateDirIfNotExists(PythonHome);
        if (usePackageManagerInstaller)
        {
            Utils::exec(PythonInstallCMD);
        }
        else
        {
            LogInfo("下载Python...");
            const std::string pythonZip = PythonHome + "python.zip";
            Installer(
                {PythonLink, pythonZip}
                , [=](Downloader* d)
                {
                    if (d->IsFinished())
                    {
                        Utils::Decompress(pythonZip, PythonHome);
                        auto file = Utils::GetFilesWithExt(PythonHome, "._pth").front();
                        std::fstream fs(file, std::ios::out | std::ios::in);
                        if (!fs.is_open())
                        {
                            LogError("Error: Failed to open file : {0}", file);
                            return;
                        }

                        std::vector<std::string> lines;
                        std::string line;
                        while (std::getline(fs, line))
                        {
                            lines.push_back(line);
                        }
                        if (!lines.empty())
                        {
                            lines.back() = "import site";
                        }

                        fs.clear();
                        fs.seekp(0, std::ios::beg);
                        for (const auto& l : lines)
                        {
                            fs << l << std::endl;
                        }

                        fs.close();
                    }
                    else
                    {
                        LogError("Python 安装失败");
                    }
                });
        }
    }
    if (!IsPipInstalled())
    {
        const std::string getPipPython = PythonHome + "get-pip.py";
        Installer(
            {PythonGetPip, getPipPython}, [&](Downloader* d)
            {
                if (d->IsFinished())
                {
                    Utils::ExecuteShell(
                        Utils::GetAbsolutePath(
                            PythonHome + PythonExecute),
                        Utils::GetAbsolutePath(PythonHome + "get-pip.py"));
                    LogInfo("Python 安装成功");
                    CreateBot();
                }
            });
    }

    bool success = true;
    if (!UDirectory::Exists(ConversationPath.string()))
    {
        UDirectory::Create(ConversationPath.string());
    }

    whisper = true;
    mwhisper = true;

    // 遍历文件夹数组
    for (const auto& dir : dirs)
    {
        // 检查文件夹是否存在
        if (!UDirectory::Exists(dir))
        {
            // 如果文件夹不存在，则输出警告信息
            LogWarn("Initialize Warning: Folder {0} does not exist!", dir);
            UDirectory::Create(dir);
        }
    }
    if (whisperData.enable)
    {
        if (whisperData.useLocalModel)
        {
            if (!CheckFileExistence(bin + WhisperPath, "Whisper", "main", true))
            {
                state = State::NO_WHISPER;
                whisper = false;
                LogInfo(
                    "Initialize: It is detected that you do not have the \"Whisper Executable file\" and the \"useLocalModel\" option is enabled.");
                success = false;
            }
            if (!CheckFileExistence(whisperData.model, "Whisper Model"))
            {
                mwhisper = false;
                state = State::NO_WHISPER;
                LogInfo(
                    "Initialize: It is detected that you do not have the \"Whisper model\" and the \"useLocalModel\" option is enabled.");
                success = false;
            }
        }
    }
    if (vitsData.enable && !vitsData.UseGptSovite)
    {
        // 检查VITS可执行文件是否存在
        if (!CheckFileExistence(bin + VitsConvertor, "VITS", "VitsConvertor", true))
        {
            state = State::NO_VITS;
            vits = false;
            LogInfo(
                "Initialize: It is detected that you do not have the \"VITS Executable file\" and the \"vits.enable\" option is enabled.");
            success = false;
        }

        // 检查VITS模型文件是否存在
        if (!CheckFileExistence(vitsData.model, "model file") ||
            !CheckFileExistence(vitsData.config, "Configuration file"))
        {
            vitsModel = false;
            success = false;
        }
    }

    if (live2D.enable)
    {
        if (!CheckFileExistence(live2D.bin, "Live2D executable file"))
        {
            bool compressed = false;
            if (UFile::Exists("Resources/Live2D.zip"))
            {
                compressed = UCompression::DecompressZip("Resources/Live2D.zip", bin + Live2DPath);
                configure.live2D.bin = bin + Live2DPath + "DesktopPet.exe";
                Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
            }
            if (!compressed)
            {
                LogWarn(
                    "Initialize Warning: Since you don't have a \"Live2D Executable file\", the Live2D function isn't working properly!");
                live2D.enable = false;
                success = false;
            }
        }
        if (!CheckFileExistence(live2D.model + "/" + Utils::GetFileName(live2D.model) + ".model3.json",
                                "Live2D model"))
        {
            live2D.enable = false;
            success = false;
        }
    }
    if (is_valid_text(configure.stableDiffusion.apiPath))
    {
        StringExecutor::SetDrawCallback(
            [this](const std::string& _p, long long ts, bool fromBot, const std::string& _n)
            {
                Draw(_p, ts, fromBot, _n);
            });
    }
    if (vits)
    {
        StringExecutor::SetTTSCallback([this](const std::string& text)
        {
            Vits(text);
        });
    }
    auto files = UDirectory::GetSubFiles(CustomRulesPath);
    for (auto& file : files)
    {
        auto c = Utils::LoadYaml<CustomRule>(CustomRulesPath + file);
        if (c == nullopt)
        {
            LogError("Custom Role Error: Failed to load {0}", file);
            continue;
        }
        customRuleTemplates[c->name] = c.value();
    }
    mdConfig.linkCallback = LinkCallback;
    mdConfig.imageCallback = ImageCallback;

    mdConfig.linkIcon = "[链接]"; // 链接前缀
    mdConfig.headingFormats[0] = ImGui::MarkdownHeadingFormat{H1, true}; // H1
    mdConfig.headingFormats[1] = ImGui::MarkdownHeadingFormat{H2, true}; // H2
    mdConfig.headingFormats[2] = ImGui::MarkdownHeadingFormat{H3, false}; // H3
    mdConfig.formatCallback = [](const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start_)
    {
        // 根据不同的格式类型应用不同样式
        switch (markdownFormatInfo_.type)
        {
        case ImGui::MarkdownFormatType::NORMAL_TEXT:
            // 普通文本，不需要特殊处理
            break;

        case ImGui::MarkdownFormatType::HEADING:
            {
                // 处理标题格式
                if (start_)
                {
                    // 标题开始
                    int level = markdownFormatInfo_.level;

                    // 根据标题级别设置不同的样式
                    if (level == 1)
                    {
                        // 一级标题：添加空间并使用突出颜色
                        ImGui::Dummy(ImVec2(0.0f, 5.0f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
                    }
                    else if (level == 2)
                    {
                        // 二级标题：添加适当空间并使用不同颜色
                        ImGui::Dummy(ImVec2(0.0f, 4.0f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.9f, 1.0f));
                    }
                    else
                    {
                        // 三级标题：添加少量空间
                        ImGui::Dummy(ImVec2(0.0f, 3.0f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                    }
                }
                else
                {
                    // 标题结束
                    ImGui::PopStyleColor();

                    // 在标题后添加额外空间
                    int level = markdownFormatInfo_.level;
                    ImGui::Dummy(ImVec2(0.0f, 5.0f - level + 1)); // 根据级别调整空间大小
                }
                break;
            }

        case ImGui::MarkdownFormatType::UNORDERED_LIST:
            {
                // 无序列表的处理
                if (start_)
                {
                    // 列表项开始：添加缩进和少量垂直空间
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    ImGui::Indent(10.0f);
                }
                else
                {
                    // 列表项结束：取消缩进并添加空间
                    ImGui::Unindent(10.0f);
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));
                }
                break;
            }

        case ImGui::MarkdownFormatType::EMPHASIS:
            {
                // 强调文本（通常是斜体）
                if (start_)
                {
                    // 开始强调：使用不同颜色表示
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.7f, 1.0f));
                }
                else
                {
                    // 结束强调：恢复颜色
                    ImGui::PopStyleColor();
                }
                break;
            }

        case ImGui::MarkdownFormatType::LINK:
            {
                // 链接文本
                if (start_)
                {
                    // 链接开始：使用蓝色表示链接
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
                }
                else
                {
                    // 链接结束：恢复颜色
                    ImGui::PopStyleColor();
                }
                break;
            }

        default:
            // 其他未处理的类型
            break;
        }
    };
    LogInfo("Successful initialization!");
    return success;
}

bool Application::is_valid_text(const std::string& text) const
{
    if (text.empty()) return false;

    bool is_space = text.find_first_not_of(" \t\r\n") == std::string::npos;
    bool is_special = text.find_first_not_of(special_chars) == std::string::npos;

    if (is_space || is_special) return false;

    if (is_space && is_special) return false;

    return true;
}

void Application::
ShowConfirmationDialog(const char* title, const char* content, bool& mstate,
                       ConfirmDelegate on_confirm,
                       const char* failure,
                       const char* success,
                       const char* confirm,
                       const char* cancel)
{
    static bool Start = false;
    static bool reject = false;
    if (!reject)
    {
        ImGui::OpenPopup(title);
        if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("%s", content);
            ImGui::Separator();

            if (ImGui::Button(confirm))
            {
                mstate = true;
                if (on_confirm != nullptr)
                {
                    std::thread t(on_confirm);
                    t.detach();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(cancel))
            {
                mstate = false;
                reject = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

void Application::RenderMarkdown(vector<char>& markdown, ImVec2 input_size)
{
    // 添加左侧间距
    ImGui::Indent(4.0f);

    // 创建互相同步的双层滚动区域

    // 创建唯一ID，避免ImGui控件ID冲突
    ImGuiID id = ImGui::GetID("md_selection_layer");
    std::string childId = "md_content_" + std::to_string(id);

    // 获取滚动状态，用于同步两个层的滚动位置
    static float lastScrollY = 0.0f;
    float currentScrollY = 0.0f;
    bool scrollFromMarkdown = false;
    bool scrollFromInput = false;

    {
        // 记录渲染前的光标位置
        ImVec2 pos = ImGui::GetCursorPos();

        // 创建子窗口，允许滚动但禁止输入
        if (ImGui::BeginChild(childId.c_str(), input_size, false, ImGuiWindowFlags_NoInputs))
        {
            // 渲染Markdown格式的内容
            ImGui::Markdown(markdown.data(), markdown.size(), mdConfig);

            // 检测Markdown层是否被滚动
            currentScrollY = ImGui::GetScrollY();
            if (ImGui::IsWindowHovered() && fabsf(currentScrollY - lastScrollY) > 1.0f)
            {
                scrollFromMarkdown = true;
                lastScrollY = currentScrollY;
            }

            ImGui::EndChild();
        }

        // 将光标重置回原始位置，准备渲染第二层
        ImGui::SetCursorPos(pos);
    }

    {
        // 设置样式，使输入框内容不可见但保留选择高亮效果
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // 文本颜色设为透明
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // 背景颜色设为透明
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.2f, 0.4f, 0.8f, 0.3f)); // 文本选择背景设为蓝色半透明

        if (ImGui::InputTextMultiline(("##md_selector" + std::to_string(id)).c_str(),
                                      markdown.data(), markdown.size(),
                                      input_size,
                                      ImGuiInputTextFlags_ReadOnly))
        {
        }

        // 同步滚动位置
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window && window->ScrollbarY)
        {
            // 检测输入框是否被滚动
            float inputScrollY = ImGui::GetScrollY();
            if (ImGui::IsWindowHovered() && fabsf(inputScrollY - lastScrollY) > 1.0f)
            {
                scrollFromInput = true;
                lastScrollY = inputScrollY;
            }

            // 如果Markdown层被滚动，同步输入框的滚动位置
            if (scrollFromMarkdown)
            {
                ImGui::SetScrollY(currentScrollY);
            }
            // 如果输入框被滚动，记录新的滚动位置供下次渲染时同步到Markdown层
            else if (scrollFromInput)
            {
                lastScrollY = inputScrollY;
            }
        }

        // 恢复原始样式设置
        ImGui::PopStyleColor(3);
    }

    // 取消左侧缩进
    ImGui::Unindent(4.0f);
}

void Application::RuntimeDetector()
{
    if (!whisper)
    {
        ShowConfirmationDialog(reinterpret_cast<const char*>(u8"下载通知"),
                               reinterpret_cast<const char*>(u8"检测到你并没安装Whisper,是否安装?"), whisper,
                               [this]() { WhisperExeInstaller(); },
                               reinterpret_cast<const char*>(u8"安装Whisper失败"),
                               reinterpret_cast<const char*>(u8"安装Whisper成功"));
    }
    else if (!vits && (configure.vits.enable && !configure.vits.UseGptSovite))
    {
        ShowConfirmationDialog(reinterpret_cast<const char*>(u8"下载通知"),
                               reinterpret_cast<const char*>(u8"检测到你并没安装VitsConvertor,是否安装?"), vits,
                               [this]() { VitsExeInstaller(); },
                               reinterpret_cast<const char*>(u8"安装VitsConvertor失败"),
                               reinterpret_cast<const char*>(u8"安装VitsConvertor成功"));
    }
    else if (!mwhisper)
    {
        ShowConfirmationDialog(reinterpret_cast<const char*>(u8"下载通知"),
                               reinterpret_cast<const char*>(u8"检测到你并没安装Whisper Model,是否下载?"),
                               mwhisper,
                               [this]() { WhisperModelDownload(Utils::GetFileName(whisperData.model)); },
                               reinterpret_cast<const char*>(u8"安装Whisper Model失败"),
                               reinterpret_cast<const char*>(u8"安装Whisper Model成功"));
    }
}

void Application::GetClaudeHistory()
{
    thread([&]()
    {
        FirstTime = Utils::GetCurrentTimestamp();
        while (AppRunning)
        {
            Chat botR;
            botR.flag = 1;
            auto _history = bot->GetHistory();
            DeleteAllBotChat();
            for (auto& it : _history)
            {
                if (it.second != "Please note:")
                {
                    botR.timestamp = it.first;
                    botR.content = it.second;
                    AddChatRecord(CreateRef<Chat>(botR));
                }
            }
            // 按时间戳排序
            std::sort(chat_history.begin(), chat_history.end(), compareByTimestamp);

            for (const auto& chat : chat_history)
            {
                if (chat->flag == 0)
                {
                    FirstTime = chat->timestamp;
                    break;
                }
            }

            // 删除早于FirstTime的对话
            chat_history.erase(
                std::remove_if(chat_history.begin(), chat_history.end(),
                               [&](const Ref<Chat> c)
                               {
                                   return c->timestamp < FirstTime;
                               }),
                chat_history.end());
            save(convid, false);
            // 延迟0.002s
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }).detach();
}
