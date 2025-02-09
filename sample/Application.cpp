#include "Application.h"

#include "Downloader.h"
#include "SystemRole.h"
#define IMSPINNER_DEMO
#include "ImGuiSpinners.h"

std::vector<std::string> scommands;
bool cpshow = false;

Application::Application(const Configure& configure, bool setting)
{
    try
    {
        scommands = commands;
        this->configure = configure;
        OnlySetting = setting;
        CreateBot();
        translator = CreateRef<Translate>(configure.baiDuTranslator);

        voiceToText = CreateRef<VoiceToText>(configure.openAi);
        listener = CreateRef<Listener>(sampleRate, framesPerBuffer);
        listener->ChangeDevice(configure.MicroPhoneID);
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
        }
        if (vitsData.enable && vits && vitsModel)
        {
            Vits("欢迎使用本ChatBot");
#ifdef WIN32
            Utils::OpenProgram("bin/VitsConvertor/VitsConvertor.exe");
#else
            std::thread([&]() {
                Utils::exec(Utils::GetAbsolutePath(bin + VitsConvertor + "VitsConvertor" + exeSuffix));
            }).detach();
#endif
            VitsListener();
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
    }
    catch (exception& e)
    {
        LogError(e.what());
    }
}

GLuint Application::LoadTexture(const std::string& path)
{
    return UImage::Img2Texture(path);
}

void Application::Vits(std::string text)
{
    if (configure.vits.UseGptSovite)
    {
        std::string endPoint = configure.vits.apiEndPoint;
        std::thread([endPoint,text]
        {
            std::string url = endPoint + "?text=" + Utils::UrlEncode(text) + "&text_language=zh";
            auto res = Get(cpr::Url{url});
            if (res.status_code != 200)
            {
                LogError("GPT-SoVits Error: " + res.text);
            }
            else
            {
                std::ofstream wavFile("tmp.wav", std::ios::binary);
                if (wavFile)
                {
                    // 将响应体写入文件
                    wavFile.write(res.text.c_str(), res.text.size());
                    wavFile.close();
                    std::cout << "WAV file downloaded successfully." << std::endl;
                }
                else
                {
                    std::cerr << "Failed to open file for writing." << std::endl;
                }
            }
        }).detach();
        return;
    }
    VitsTask task;
    task.model = Utils::GetAbsolutePath(vitsData.model);
    task.config = Utils::GetAbsolutePath(vitsData.config);
    task.text = text;
    task.sid = vitsData.speaker_id;
    Utils::SaveYaml("task.yaml", Utils::toYaml(task));
}

void Application::VitsListener()
{
    static bool is_playing = false;
    std::thread([&]()
    {
        VitsTask task;
        while (AppRunning)
        {
            //std::string result = Utils::exec(Utils::GetAbsolutePath(bin + VitsConvertor + "VitsConvertor" + exeSuffix));

            //if (result.find("Synthesized and saved!")) {
            if (UFile::Exists(task.outpath))
            {
                // 等待音频完成后再播放下一个
                while (is_playing)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                is_playing = true;
                listener->playRecorded(whisperData.enable);
                is_playing = false;

                std::remove(task.outpath.c_str());
            }
            //}
        }
    }).detach();
}

void Application::Draw(Ref<std::string> prompt, long long ts, bool callFromBot)
{
    if (is_valid_text(configure.stableDiffusion.apiPath))
    {
        std::string uid = stableDiffusion->Text2Img(*prompt);

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
            img.content = "Finished!";
            img.image = uid;

            // 使用互斥锁保护共享资源 chat_history
            std::lock_guard<std::mutex> lock(chat_mutex);
            chat_history.emplace_back(CreateRef<Chat>(img));
        }
    }
    else
    {
        Chat img;
        img.flag = 1;
        img.timestamp = Utils::GetCurrentTimestamp() + 10;
        img.content = reinterpret_cast<const char*>(u8"抱歉,我不能为您生成图片,因为您的api地址为空");
        std::lock_guard<std::mutex> lock(chat_mutex);
        chat_history.emplace_back(CreateRef<Chat>(img));
    }
}

void Application::DisplayInputText(Ref<Chat> chat) const
{
    try
    {
        ImVec2 text_pos;
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

        std::vector<char> mutableBuffer(processed_content.begin(), processed_content.end());

        // Ensure the buffer is null-terminated.
        mutableBuffer.push_back('\0');

        auto& style = ImGui::GetStyle();
        float i = style.ScrollbarSize;
        style.ScrollbarSize = 0.00001;
        ImGui::InputTextMultiline(("##" + std::to_string(chat->timestamp) + std::to_string(chat->flag)).c_str(),
                                  mutableBuffer.data(), mutableBuffer.size(),
                                  input_size,
                                  ImGuiInputTextFlags_ReadOnly |
                                  ImGuiInputTextFlags_AutoSelectAll);
        style.ScrollbarSize = i;
        // 显示时间戳
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), Utils::Stamp2Time(chat->timestamp).c_str());
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

        if ((configure.openAi.enable && isMissingKey(configure.openAi.api_key)) ||
            (configure.claude.enable && isMissingKey(configure.claude.slackToken)) ||
            (configure.gemini.enable && isMissingKey(configure.gemini._apiKey)) ||
            (configure.grok.enable && isMissingKey(configure.grok.api_key)))
        {
            OnlySetting = true;
            state = State::NO_BOT_KEY;
        }

        if (!configure.openAi.enable && !configure.claude.enable &&
            !configure.gemini.enable && !configure.grok.enable)
        {
            bool hasCustomGPT = false;
            for (const auto& [name, cconfig] : configure.customGPTs)
            {
                if (cconfig.enable)
                {
                    hasCustomGPT = true;
                    break;
                }
            }
            if (!hasCustomGPT)
            {
                OnlySetting = true;
                state = State::NO_BOT_KEY;
                bot = CreateRef<GPTLike>(GPTLikeCreateInfo());
            }
        }

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

        if (configure.claude.enable)
        {
            bot = CreateRef<Claude>(configure.claude);
            GetClaudeHistory();
        }
        else if (configure.openAi.enable)
        {
            bot = CreateRef<ChatGPT>(configure.openAi, SYSTEMROLE + SYSTEMROLE_EX);
        }
        else if (configure.gemini.enable)
        {
            bot = CreateRef<Gemini>(configure.gemini);
            ConversationPath /= "Gemini/";
        }
        else if (configure.grok.enable)
        {
            bot = CreateRef<Grok>(configure.grok, SYSTEMROLE + SYSTEMROLE_EX);
        }
        else
        {
            for (const auto& [name, cconfig] : configure.customGPTs)
            {
                if (cconfig.enable)
                {
                    if (!cconfig.useLocalModel)
                        bot = CreateRef<GPTLike>(cconfig, SYSTEMROLE + SYSTEMROLE_EX);
                    else
                        bot = CreateRef<LLama>(cconfig.llamaData, SYSTEMROLE + SYSTEMROLE_EX);
                    break; // use the first available custom GPT
                }
            }
        }
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
        bot->Load(convid);
        load(convid);
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
        if (downloader != nullptr && downloader->GetStatus() != UInitializing && downloader->GetStatus() != UFinished)
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

                if (ImGui::ImageButton((dA + std::to_string(TextureCache["pause"])).c_str(), TextureCache["pause"],
                                       ImVec2(32, 32)))
                {
                    downloader->Pause();
                }
            }
            if (downloader->GetStatus() == UPaused)
            {
                ImGui::SameLine();
                if (ImGui::ImageButton((dA + std::to_string(TextureCache["play"])).c_str(), TextureCache["play"],
                                       ImVec2(32, 32)))
                {
                    downloader->Resume();
                }
            }
            ImGui::SameLine();
            if (ImGui::ImageButton((dA + std::to_string(TextureCache["del"])).c_str(), TextureCache["del"],
                                   ImVec2(32, 32)))
            {
                std::thread([downloader]()
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1)); //延时析构
                    downloader->Stop();
                }).detach();
            }
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
                DisplayInputText(userAsk);
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
                if (ImGui::ImageButton("##" + TextureCache["avatar"], TextureCache["avatar"],
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
                            Utils::OpenFileManager(Utils::GetAbsolutePath(Resources + "Images/" + botAnswer->image));
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
    static std::vector<std::shared_future<std::string>> submit_futures;
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
                        AddChatRecord(user);

                        Rnum -= 1;

                        std::remove(("recorded" + std::to_string(Rnum) + ".wav").c_str());
                        LogInfo("whisper: 删除录音{0}", "recorded" + std::to_string(Rnum) + ".wav");
                        std::shared_future<std::string> submit_future = std::async(std::launch::async, [&]()
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
                        }
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
        if (ImGui::BeginMenuBar())
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
        }
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

    if ((strlen(last_input.c_str()) > 0 && ImGui::IsKeyPressed(ImGuiKey_Enter) && (!ImGui::GetIO().KeyCtrl)))
    {
        Ref<Chat> user = CreateRef<Chat>();
        std::string cmd, args;
        user->timestamp = Utils::GetCurrentTimestamp();

        user->content = last_input;
        if (ContainsCommand(last_input, cmd, args))
        {
            InlineCommand(cmd, args, user->timestamp);
        }
        else
        {
            if (is_valid_text(last_input))
            {
                // 使用last_input作为键
                if (whisperData.enable)
                    listener->ResetRecorded();
                std::shared_future<std::string> submit_future = std::async(std::launch::async, [&]()
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
                submit_futures.push_back(std::move(submit_future));
            }
        }
        if (is_valid_text(user->content))
        {
            AddChatRecord(user);
        }
        input_text.clear();
        save(convid);
        bot->Save(convid);
    }
    else if ((ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl) ||
        (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyShift))
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
                if (vits && vitsData.enable)
                {
                    std::string VitsText = translator->translate(Utils::ExtractNormalText(response),
                                                                 vitsData.lanType);
                    Vits(VitsText);
                }
                else if (whisperData.enable)
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
    if (ImGui::ImageButton("##" + TextureCache["add"], TextureCache["add"], ImVec2(16, 16)))
    {
        // 显示输入框
        show_input_box = true;
    }
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
        if (ImGui::Button(conversation.c_str(), ImVec2(100, 30)))
        {
            convid = conversation;
            bot->Load(convid);
            load(convid);
        }
        ImGui::PopID();
        // 删除按钮
        ImGui::SameLine();
        ImGui::PushID(conversation.c_str());
        if (ImGui::ImageButton("##" + TextureCache["del"], TextureCache["del"], ImVec2(16, 16)) &&
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
                    if (listener)
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
    if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"LLM功能")))
    {
        ImGui::Checkbox(reinterpret_cast<const char*>(u8"使用Claude (实验性功能)"), &configure.claude.enable);
        if (configure.claude.enable)
        {
            configure.gemini.enable = false;
            configure.openAi.enable = false;
            configure.grok.enable = false;
            for (auto& it : configure.customGPTs)
            {
                it.second.enable = false;
            }
        }
        ImGui::Checkbox(reinterpret_cast<const char*>(u8"使用Gemini"), &configure.gemini.enable);
        if (configure.gemini.enable)
        {
            configure.claude.enable = false;
            configure.openAi.enable = false;
            configure.grok.enable = false;
            for (auto& it : configure.customGPTs)
            {
                it.second.enable = false;
            }
        }
        ImGui::Checkbox(reinterpret_cast<const char*>(u8"使用OpenAI"), &configure.openAi.enable);
        if (configure.openAi.enable)
        {
            configure.claude.enable = false;
            configure.gemini.enable = false;
            configure.grok.enable = false;
            for (auto& it : configure.customGPTs)
            {
                it.second.enable = false;
            }
        }

        ImGui::Checkbox(reinterpret_cast<const char*>(u8"使用GPT-Grok"), &configure.grok.enable);
        if (configure.grok.enable)
        {
            configure.claude.enable = false;
            configure.gemini.enable = false;
            configure.openAi.enable = false;
            for (auto& it : configure.customGPTs)
            {
                it.second.enable = false;
            }
        }
        static int filteredItemCount = 0;

        float lineHeight = ImGui::GetTextLineHeightWithSpacing() * 1.5;
        float childHeight = (filteredItemCount < 10 ? filteredItemCount : 10) * lineHeight + lineHeight;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange

        ImGui::Text("自定义API: ");

        ImGui::PopStyleColor();
        ImGui::BeginChild("##自定义API", ImVec2(0, childHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        {
            ImGui::InputText("##", GetBufferByName("search").buffer, TEXT_BUFFER);

            std::string searchStr(GetBufferByName("search").buffer);

            auto filterMatch = [&searchStr](const std::string& name) -> bool
            {
                if (searchStr.empty())
                    return true;
                return name.find(searchStr) != std::string::npos;
            };

            filteredItemCount = 0;
            for (const auto& pair : configure.customGPTs)
            {
                if (filterMatch(pair.first))
                {
                    filteredItemCount++;
                }
            }
            for (auto& pair : configure.customGPTs)
            {
                // Only display items that match the search criteria.
                if (!filterMatch(pair.first))
                    continue;

                // Display a checkbox for each custom GPT.
                if (ImGui::Checkbox(pair.first.c_str(), &pair.second.enable))
                {
                    // When a custom GPT is enabled, disable the other predefined providers.
                    if (pair.second.enable)
                    {
                        configure.claude.enable = false;
                        configure.gemini.enable = false;
                        configure.openAi.enable = false;
                        configure.grok.enable = false;

                        // Disable all other custom GPTs.
                        for (auto& pair2 : configure.customGPTs)
                        {
                            if (pair2.first != pair.first)
                            {
                                pair2.second.enable = false;
                            }
                        }
                    }
                }
            }
        }
        ImGui::EndChild();
        if (ImGui::Button(reinterpret_cast<const char*>(u8"添加自定义API"), ImVec2(120, 30)))
        {
            ImGui::OpenPopup(reinterpret_cast<const char*>(u8"请输入新的配置名字"));
        }
        ImGui::Text("API配置: ");
        ImGui::BeginChild("##API配置", ImVec2(0, 160), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        if (configure.openAi.enable)
        {
            static bool showPassword = false, clicked = false;
            static double lastInputTime = 0.0;
            double currentTime = ImGui::GetTime();
            strcpy_s(GetBufferByName("api").buffer, configure.openAi.api_key.c_str());
            strcpy_s(GetBufferByName("endPoint").buffer, configure.openAi._endPoint.c_str());
            strcpy_s(GetBufferByName("model").buffer, configure.openAi.model.c_str());
            strcpy_s(GetBufferByName("proxy").buffer, configure.openAi.proxy.c_str());
            if (currentTime - lastInputTime > 0.5)
            {
                showPassword = false;
            }
            if (showPassword || clicked)
            {
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"OpenAI API Key"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer)))
                {
                    configure.openAi.api_key = GetBufferByName("api").buffer;
                }
            }
            else
            {
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"OpenAI API Key"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer),
                                     ImGuiInputTextFlags_Password))
                {
                    configure.openAi.api_key = GetBufferByName("api").buffer;
                    showPassword = true;
                    lastInputTime = ImGui::GetTime();
                }
            }
            ImGui::SameLine();
            if (ImGui::ImageButton("##" + TextureCache["eye"], TextureCache["eye"],
                                   ImVec2(16, 16),
                                   ImVec2(0, 0),
                                   ImVec2(1, 1),
                                   ImVec4(0, 0, 0, 0),
                                   ImVec4(1, 1, 1, 1)))
            {
                clicked = !clicked;
            }
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"OpenAI 模型"), GetBufferByName("model").buffer,
                                 TEXT_BUFFER))
            {
                configure.openAi.model = GetBufferByName("model").buffer;
            }
            ImGui::Checkbox(reinterpret_cast<const char*>(u8"使用远程代理"), &configure.openAi.useWebProxy);
            if (configure.openAi.useWebProxy)
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"对OpenAI使用的代理"),
                                     GetBufferByName("proxy").buffer,
                                     TEXT_BUFFER))
                {
                    configure.openAi.proxy = GetBufferByName("proxy").buffer;
                }
                else
                {
                    if (ImGui::InputText(reinterpret_cast<const char*>(u8"远程接入点"),
                                         GetBufferByName("endPoint").buffer,
                                         TEXT_BUFFER))
                    {
                        configure.openAi._endPoint = GetBufferByName("endPoint").buffer;
                    }
                }
        }
        else if (configure.claude.enable)
        {
            static bool showPassword = false, clicked = false;
            static double lastInputTime = 0.0;
            double currentTime = ImGui::GetTime();
            strcpy_s(GetBufferByName("api").buffer, configure.claude.slackToken.c_str());
            if (currentTime - lastInputTime > 0.5)
            {
                showPassword = false;
            }
            if (showPassword || clicked)
            {
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"Claude Token"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer)))
                {
                    configure.claude.slackToken = GetBufferByName("api").buffer;
                }
            }
            else
            {
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"Claude Token"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer),
                                     ImGuiInputTextFlags_Password))
                {
                    configure.claude.slackToken = GetBufferByName("api").buffer;
                    showPassword = true;
                    lastInputTime = ImGui::GetTime();
                }
            }
            ImGui::SameLine();
            if (ImGui::ImageButton("##" + TextureCache["eye"], TextureCache["eye"],
                                   ImVec2(16, 16),
                                   ImVec2(0, 0),
                                   ImVec2(1, 1),
                                   ImVec4(0, 0, 0, 0),
                                   ImVec4(1, 1, 1, 1)))
            {
                clicked = !clicked;
            }
            /*            ImGui::InputText(reinterpret_cast<const char *>(u8"用户名"), configure.claude.userName.data(),
                                         TEXT_BUFFER);


                        ImGui::InputText("Claude Cookie",
                                         configure.claude.cookies.data(),
                                         TEXT_BUFFER);*/

            if (ImGui::InputText(reinterpret_cast<const char*>(u8"Claude ID"), configure.claude.channelID.data(),
                                 TEXT_BUFFER))
            {
            }
        }
        else if (configure.gemini.enable)
        {
            static bool showPassword = false, clicked = false;
            static double lastInputTime = 0.0;
            double currentTime = ImGui::GetTime();
            strcpy_s(GetBufferByName("api").buffer, configure.gemini._apiKey.c_str());
            strcpy_s(GetBufferByName("endPoint").buffer, configure.gemini._endPoint.c_str());
            if (currentTime - lastInputTime > 0.5)
            {
                showPassword = false;
            }
            if (showPassword || clicked)
            {
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"API Key"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer)))
                {
                    configure.gemini._apiKey = GetBufferByName("api").buffer;
                }
            }
            else
            {
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"API Key"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer),
                                     ImGuiInputTextFlags_Password))
                {
                    configure.gemini._apiKey = GetBufferByName("api").buffer;
                    showPassword = true;
                    lastInputTime = ImGui::GetTime();
                }
            }
            ImGui::SameLine();
            if (ImGui::ImageButton("##" + TextureCache["eye"], TextureCache["eye"],
                                   ImVec2(16, 16),
                                   ImVec2(0, 0),
                                   ImVec2(1, 1),
                                   ImVec4(0, 0, 0, 0),
                                   ImVec4(1, 1, 1, 1)))
            {
                clicked = !clicked;
            }
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"远程接入点"), GetBufferByName("endPoint").buffer,
                                 TEXT_BUFFER))
            {
                configure.gemini._endPoint = GetBufferByName("endPoint").buffer;
            }
        }
        else if (configure.grok.enable)
        {
            static bool showPassword = false, clicked = false;
            static double lastInputTime = 0.0;
            double currentTime = ImGui::GetTime();
            strcpy_s(GetBufferByName("api").buffer, configure.grok.api_key.c_str());
            strcpy_s(GetBufferByName("model").buffer, configure.grok.model.c_str());
            if (currentTime - lastInputTime > 0.5)
            {
                showPassword = false;
            }
            if (showPassword || clicked)
            {
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"API Key"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer)))
                {
                    configure.grok.api_key = GetBufferByName("api").buffer;
                }
            }
            else
            {
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"API Key"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer),
                                     ImGuiInputTextFlags_Password))
                {
                    configure.grok.api_key = GetBufferByName("api").buffer;
                    showPassword = true;
                    lastInputTime = ImGui::GetTime();
                }
            }
            ImGui::SameLine();
            if (ImGui::ImageButton("##" + (TextureCache["eye"]), (TextureCache["eye"]),
                                   ImVec2(16, 16),
                                   ImVec2(0, 0),
                                   ImVec2(1, 1),
                                   ImVec4(0, 0, 0, 0),
                                   ImVec4(1, 1, 1, 1)))
            {
                clicked = !clicked;
            }
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"模型名称"), GetBufferByName("model").buffer,
                                 TEXT_BUFFER))
            {
                configure.grok.model = GetBufferByName("model").buffer;
            }
        }

        for (auto& [name,cdata] : configure.customGPTs)
        {
            if (cdata.enable)
            {
                static bool showPassword = false, clicked = false;
                static double lastInputTime = 0.0;
                double currentTime = ImGui::GetTime();
                ImGui::Checkbox(reinterpret_cast<const char*>(u8"使用本地模型"), &cdata.useLocalModel);
                if (!cdata.useLocalModel)
                {
                    strcpy_s(GetBufferByName("api").buffer, cdata.api_key.c_str());
                    strcpy_s(GetBufferByName("apiHost").buffer, cdata.apiHost.c_str());
                    strcpy_s(GetBufferByName("apiPath").buffer, cdata.apiPath.c_str());
                    strcpy_s(GetBufferByName("model").buffer, cdata.model.c_str());
                    if (currentTime - lastInputTime > 0.5)
                    {
                        showPassword = false;
                    }
                    if (showPassword || clicked)
                    {
                        if (ImGui::InputText(reinterpret_cast<const char*>(u8"API Key"),
                                             GetBufferByName("api").buffer,
                                             sizeof(input_buffer)))
                        {
                            cdata.api_key = GetBufferByName("api").buffer;
                        }
                    }
                    else
                    {
                        if (ImGui::InputText(reinterpret_cast<const char*>(u8"API Key"),
                                             GetBufferByName("api").buffer,
                                             sizeof(input_buffer),
                                             ImGuiInputTextFlags_Password))
                        {
                            cdata.api_key = GetBufferByName("api").buffer;
                            showPassword = true;
                            lastInputTime = ImGui::GetTime();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::ImageButton("##" + TextureCache["eye"], TextureCache["eye"],
                                           ImVec2(16, 16),
                                           ImVec2(0, 0),
                                           ImVec2(1, 1),
                                           ImVec4(0, 0, 0, 0),
                                           ImVec4(1, 1, 1, 1)))
                    {
                        clicked = !clicked;
                    }
                    if (ImGui::InputText(reinterpret_cast<const char*>(u8"API 主机"),
                                         GetBufferByName("apiHost").buffer,
                                         TEXT_BUFFER))
                    {
                        cdata.apiHost = GetBufferByName("apiHost").buffer;
                    }
                    if (ImGui::InputText(reinterpret_cast<const char*>(u8"API 路径"),
                                         GetBufferByName("apiPath").buffer,
                                         TEXT_BUFFER))
                    {
                        cdata.apiPath = GetBufferByName("apiPath").buffer;
                    }
                    if (ImGui::InputText(reinterpret_cast<const char*>(u8"模型名称"), GetBufferByName("model").buffer,
                                         TEXT_BUFFER))
                    {
                        cdata.model = GetBufferByName("model").buffer;
                    }
                }
                else
                {
                    strcpy_s(GetBufferByName("model").buffer, cdata.llamaData.model.c_str());
                    if (ImGui::InputText(reinterpret_cast<const char*>(u8"模型名称"), GetBufferByName("model").buffer,
                                         TEXT_BUFFER))
                    {
                        cdata.llamaData.model = GetBufferByName("model").buffer;
                    }
                    ImGui::InputInt(reinterpret_cast<const char*>(u8"上下文大小"), &cdata.llamaData.contextSize);
                    if (cdata.llamaData.contextSize < 512)
                    {
                        cdata.llamaData.contextSize = 512;
                    }

                    ImGui::InputInt(reinterpret_cast<const char*>(u8"最大生成长度"), &cdata.llamaData.maxTokens);
                    if (cdata.llamaData.maxTokens < 256)
                    {
                        cdata.llamaData.maxTokens = 256;
                    }
                }
                if (ImGui::Button(reinterpret_cast<const char*>(u8"删除此API"), ImVec2(120, 30)))
                {
                    cdata.enable = false;
                    configure.customGPTs.erase(name);
                }
            }
        }
        ImGui::EndChild();

        if (ImGui::Button(reinterpret_cast<const char*>(u8"切换模型"), ImVec2(120, 30)))
        {
            CreateBot();
            Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char*>(u8"请输入新的配置名字"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText(reinterpret_cast<const char*>(u8"配置名字"), GetBufferByName("newName").buffer,
                             TEXT_BUFFER);
            if (ImGui::Button(reinterpret_cast<const char*>(u8"确定"), ImVec2(120, 0)))
            {
                std::string name = GetBufferByName("newName").buffer;
                if (name.empty())
                {
                    name = "NewConfig_" + std::to_string(configure.customGPTs.size());
                }
                configure.customGPTs[name] = GPTLikeCreateInfo();

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(reinterpret_cast<const char*>(u8"取消"), ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // 显示百度翻译配置
    if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"百度翻译(如果需要)")))
    {
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
        if (ImGui::ImageButton("##" + TextureCache["eye"], (TextureCache["eye"]),
                               ImVec2(16, 16),
                               ImVec2(0, 0),
                               ImVec2(1, 1),
                               ImVec4(0, 0, 0, 0),
                               ImVec4(1, 1, 1, 1)))
        {
            baiduclicked = !baiduclicked;
        }

        if (ImGui::InputText(reinterpret_cast<const char*>(u8"对百度使用的代理"),
                             GetBufferByName("proxy").buffer,
                             TEXT_BUFFER))
        {
            configure.baiDuTranslator.proxy = GetBufferByName("proxy").buffer;
        }
    }

    // 显示 VITS 配置
    if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"语音聊天功能(基于VITS)")))
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
    if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"桌面宠物(基于Live2D)")))
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
    if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"语音识别(基于Whisper)")))
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
    if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"AI绘图(基于Stable Diffusion)")))
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

    if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"插件列表")))
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
    if (ImGui::Button(reinterpret_cast<const char*>(u8"保存配置")))
    {
        Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
        Utils::SaveYaml("Lconfig.yml", Utils::toYaml(lConfigure));
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
                    for (auto& [name,cdata] : configure.customGPTs)
                    {
                        cdata.enable = false;
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
    config.GlyphExtraSpacing = ImVec2(0.0f, 1.0f);
    font = io.Fonts->AddFontFromFileTTF("Resources/font/default.otf", fontSize, &config,
                                        io.Fonts->GetGlyphRangesChineseFull());

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
            glfwPollEvents();
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            RenderUI();

            // 在Lua中调用ImGui函数
            for (auto& [name, script] : PluginsScript1)
            {
                // 检查是否在禁止插件列表中
                if (ranges::find(forbidLuaPlugins, name) != forbidLuaPlugins.end())
                {
                    continue;
                }
                script->Invoke("UIRenderer");
            }
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
            // 从文件中读取 YAML 节点
            YAML::Node node = YAML::Load(session_file);

            // 将 YAML 节点映射到 chat_history
            for (const auto& record_node : node)
            {
                // 从 YAML 映射节点中读取 ChatRecord 对象的成员变量
                Ref<Chat> record = CreateRef<Chat>();
                int flag = record_node["flag"].as<int>();
                if (record_node["newmsg"])
                {
                    record->newMessage = record_node["newmsg"].as<bool>();
                }
                long long timestamp;
                std::string content;

                if (flag == 0)
                {
                    timestamp = record_node["user"]["timestamp"].as<long long>();
                    content = record_node["user"]["content"].as<std::string>();
                }
                else
                {
                    timestamp = record_node["bot"]["timestamp"].as<long long>();
                    content = record_node["bot"]["content"].as<std::string>();
                    record->image = record_node["bot"]["image"].as<std::string>();
                }

                record->flag = flag;
                record->content = content;
                record->timestamp = timestamp;

                // 创建一个新的 ChatRecord 对象，并将其添加到 chat_history 中
                if (flag == 1)
                {
                    auto tcodes = StringExecutor::GetCodes(content);
                    for (auto& code : tcodes)
                    {
                        for (auto& it : code.Content)
                        {
                            codes[code.Type].emplace_back(it);
                        }
                    }
                }
                AddChatRecord(record);
            }

            session_file.close();
            LogInfo("Application : Load {0} successfully", name);
        }
        else
        {
            LogError("Application Error: Unable to load session {0},{1}", name, ".");
        }
    }
    else
    {
        LogError("Application Error: Unable to load session {0},{1}", name, ".");
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
        // 创建 YAML 节点
        YAML::Node node;

        // 将 chat_history 映射到 YAML 节点
        for (const auto& record : chat_history)
        {
            // 创建一个新的 YAML 映射节点
            YAML::Node record_node(YAML::NodeType::Map);

            // 将 ChatRecord 对象的成员变量映射到 YAML 映射节点中
            record_node["flag"] = record->flag;
            record_node["newmsg"] = record->newMessage;
            if (record->flag == 0)
            {
                record_node["user"]["timestamp"] = record->timestamp;
                record_node["user"]["content"] = record->content;
            }
            else
            {
                record_node["bot"]["timestamp"] = record->timestamp;
                record_node["bot"]["content"] = record->content;
                record_node["bot"]["image"] = record->image;
            }

            // 将 YAML 映射节点添加到主节点中
            node.push_back(record_node);
        }

        // 将 YAML 节点写入文件
        session_file << node;

        session_file.close();
        if (out)
            LogInfo("Application : Save {0} successfully", name);
    }
    else
    {
        if (out)
            LogError("Application Error: Unable to save session {0},{1}", name, ".");
    }
}

bool Application::Initialize()
{
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
                    SYSTEMROLE = InitSys();
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
            if (UFile::Exists(bin + "Live2D.zip"))
            {
                compressed = UCompression::DecompressZip(bin + "Live2D.zip", bin + Live2DPath);
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
    else if (!vits)
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
