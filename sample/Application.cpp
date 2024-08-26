#include "Application.h"

std::vector<std::string> scommands;
bool cpshow = false;

Application::Application(const Configure&configure, bool setting) {
    try {
        scommands = commands;
        this->configure = configure;
        OnlySetting = setting;
        if (!configure.claude.enable && (configure.openAi.api_key.empty() ||
                                         configure.openAi.api_key == "")) {
            OnlySetting = true;
            state = State::NO_BOT_KEY;
        }
        else if (configure.claude.enable &&
                 (configure.claude.slackToken.empty() ||
                  configure.claude.slackToken == "") && configure.gptLike.api_key == "") {
            OnlySetting = true;
            state = State::NO_BOT_KEY;
        }
        else if (configure.gemini.enable && configure.gemini._apiKey.empty()) {
            OnlySetting = true;
            state = State::NO_BOT_KEY;
        }
        translator = CreateRef<Translate>(configure.baiDuTranslator);
        if (configure.claude.enable) {
            bot = CreateRef<Claude>(configure.claude);
            convid = "Claude";
            GetClaudeHistory();
        }
        if (configure.openAi.enable) {
            bot = CreateRef<ChatGPT>(configure.openAi);
        }
        if (configure.gemini.enable) {
            bot = CreateRef<Gemini>(configure.gemini);
            ConversationPath = std::filesystem::path(ConversationPath.string() + "Gemini/");
        }
        if (configure.gptLike.enable) {
            bot = CreateRef<GPTLike>(configure.gptLike);
        }
        //billing = bot->GetBilling();
        voiceToText = CreateRef<VoiceToText>(configure.openAi);
        listener = CreateRef<Listener>(sampleRate, framesPerBuffer);
        stableDiffusion = CreateRef<StableDiffusion>(configure.stableDiffusion);
        vitsData = configure.vits;
        whisperData = configure.whisper;
        live2D = configure.live2D;
        if (!Initialize()) {
            LogWarn("Warning: Initialization failed!Maybe some function can not working");
        }
        if (live2D.enable) {
            Utils::OpenProgram(live2D.bin.c_str());
            lConfigure = Utils::LoadYaml<LConfigure>("Lconfig.yml");
        }
        if (whisperData.enable && whisper && mwhisper) {
            listener->listen();
        }
        if (vitsData.enable && vits && vitsModel) {
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
    }
    catch (exception&e) {
        LogError(e.what());
    }
}

GLuint Application::LoadTexture(const std::string&path) {
    return UImage::Img2Texture(path);
}

void Application::Vits(std::string text) {
    if (configure.vits.UseGptSovite) {
        std::string endPoint = configure.vits.apiEndPoint;
        std::thread([endPoint,text] {
            std::string url = endPoint + "?text=" + Utils::UrlEncode(text) + "&text_language=zh";
            auto res = Get(cpr::Url{url});
            if (res.status_code != 200) {
                LogError("GPT-SoVits Error: " + res.text);
            }
            else {
                std::ofstream wavFile("tmp.wav", std::ios::binary);
                if (wavFile) {
                    // 将响应体写入文件
                    wavFile.write(res.text.c_str(), res.text.size());
                    wavFile.close();
                    std::cout << "WAV file downloaded successfully." << std::endl;
                }
                else {
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

void Application::VitsListener() {
    static bool is_playing = false;
    std::thread([&]() {
        VitsTask task;
        while (AppRunning) {
            //std::string result = Utils::exec(Utils::GetAbsolutePath(bin + VitsConvertor + "VitsConvertor" + exeSuffix));

            //if (result.find("Synthesized and saved!")) {
            if (UFile::Exists(task.outpath)) {
                // 等待音频完成后再播放下一个
                while (is_playing) {
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

void Application::Draw(Ref<std::string> prompt, long long ts, bool callFromBot) {
    if (is_valid_text(configure.stableDiffusion.apiPath)) {
        std::string uid = stableDiffusion->Text2Img(*prompt);

        if (callFromBot) {
            // 使用互斥锁保护共享资源 chat_history
            std::lock_guard<std::mutex> lock(chat_mutex);
            for (auto&it: chat_history) {
                if (it.flag == 1) {
                    if (it.timestamp >= ts) {
                        it.image = uid;
                        break;
                    }
                }
                else {
                    continue;
                }
            }
        }
        else {
            Chat img;
            img.flag = 1;
            img.timestamp = Utils::GetCurrentTimestamp();
            img.content = "Finished!";
            img.image = uid;

            // 使用互斥锁保护共享资源 chat_history
            std::lock_guard<std::mutex> lock(chat_mutex);
            chat_history.emplace_back(img);
        }
    }
    else {
        Chat img;
        img.flag = 1;
        img.timestamp = Utils::GetCurrentTimestamp() + 10;
        img.content = reinterpret_cast<const char *>(u8"抱歉,我不能为您生成图片,因为您的api地址为空");
        std::lock_guard<std::mutex> lock(chat_mutex);
        chat_history.emplace_back(img);
    }
}

bool Application::ContainsCommand(std::string&str, std::string&cmd, std::string&args) const {
    for (const auto&it: commands) {
        std::regex cmd_regex(R"(/(\w+))"); // 定义匹配命令的正则表达式
        std::regex args_regex(R"((\[[^\]]*\]))"); // 定义匹配参数的正则表达式
        std::smatch cmd_match, args_match;

        // 匹配命令
        if (std::regex_search(str, cmd_match, cmd_regex)) {
            cmd = cmd_match[1]; // 第一个括号内的内容即为命令
            str = cmd_match.prefix().str() + cmd_match.suffix().str(); // 删除匹配到的命令部分

            // 匹配参数
            if (std::regex_search(str, args_match, args_regex)) {
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

void Application::RenderCodeBox() {
    UniversalStyle();

    ImGui::Begin("Code Box", nullptr,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    static bool show_codes = false;
    for (auto&it: codes) {
        show_codes = ImGui::CollapsingHeader(it.first.c_str());
        if (show_codes) {
            for (const auto&code: it.second) {
                if (is_valid_text(code)) {
                    if (ImGui::Button(code.c_str(), ImVec2(-1, 0))) {
                        glfwSetClipboardString(nullptr, code.c_str());
                    }
                }
            }
        }
    }

    ImGui::PopStyleVar();
    ImGui::End();
}

void Application::RenderPopupBox() {
    UniversalStyle();
    if (show_input_box) {
        ImGui::OpenPopup(reinterpret_cast<const char *>(u8"新建对话##popup"));
    }
    // 显示输入框并获取用户输入
    char buf[256] = {0};
    if (ImGui::BeginPopupModal(reinterpret_cast<const char *>(u8"新建对话##popup"), &show_input_box,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        static std::string text;
        if (ImGui::InputText("##input", buf, 256)) {
            text = buf;
        };
        ImGui::SameLine();
        ImGui::Text(reinterpret_cast<const char *>(u8"请输入对话名称："));
        ImGui::Spacing();
        ImVec2 button_size(120, 30);
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
            ImGui::Button(reinterpret_cast<const char *>(u8"确定"), button_size)) {
            if (is_valid_text(text)) {
                bot->Add(text);
                conversations.emplace_back(text);
                chat_history.clear();
                auto iter = std::find(conversations.begin(), conversations.end(), text);
                if (iter != conversations.end()) {
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
            ImGui::Button(reinterpret_cast<const char *>(u8"取消"), button_size)) {
            ImGui::CloseCurrentPopup();
            show_input_box = false;
        }
        ImGui::EndPopup();
    }
}

void Application::RenderChatBox() {
    UniversalStyle();
    // 创建一个子窗口
    ImGui::Begin(reinterpret_cast<const char *>(u8"对话"), NULL,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav);

    ImGui::BeginChild("Chat Box Content", ImVec2(0, -ImGui::GetTextLineHeightWithSpacing()), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoSavedSettings);
    std::sort(chat_history.begin(), chat_history.end(), compareByTimestamp);
    // 显示聊天记录
    for (auto&chat: chat_history) {
        Chat userAsk;
        Chat botAnswer;
        if (chat.flag == 0)
            userAsk = chat;
        else
            botAnswer = chat;
        ImVec2 text_size, text_pos;

        if (!userAsk.content.empty()) {
            // Calculate the size of the input field
            ImVec2 input_size = ImVec2(0, 0);
            ImVec2 text_size = ImGui::CalcTextSize(userAsk.content.c_str());
            float max_width = ImGui::GetWindowContentRegionMax().x * 0.9f;

            input_size.x = min(text_size.x, max_width) + 10;
            input_size.y = text_size.y + ImGui::GetStyle().ItemSpacing.y * 2;
            // Set the style of the input field
            float rounding = 4.0f;

            // Display the input field with the text and automatic line breaks
            ImGui::InputTextMultiline(("##" + std::to_string(userAsk.timestamp)).c_str(),
                                      const_cast<char *>(userAsk.content.c_str()), userAsk.content.size() + 1,
                                      input_size,
                                      ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_CtrlEnterForNewLine |
                                      ImGuiInputTextFlags_AutoSelectAll);

            // Display the timestamp below the chat record
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), Utils::Stamp2Time(userAsk.timestamp).c_str());

            // Restore the style of the input field
        }
        if (!botAnswer.content.empty()) {
            // Display the bot's answer
            text_pos = ImVec2(ImGui::GetWindowContentRegionMin().x, ImGui::GetCursorPosY());
            ImGui::SetCursorPosY(text_pos.y + ImGui::GetStyle().ItemSpacing.y);

            // Set the maximum height and width of the multi-line input field
            float max_width = ImGui::GetWindowContentRegionMax().x * 0.9f;

            // Calculate the size of the multi-line input field
            ImVec2 input_size = ImVec2(0, 0);
            ImVec2 text_size = ImGui::CalcTextSize(botAnswer.content.c_str());
            input_size.x = min(text_size.x, max_width) + 10;
            input_size.y = text_size.y + ImGui::GetStyle().ItemSpacing.y * 2;

            // Modify the style of the multi-line input field
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
            ImGui::PushID(botAnswer.timestamp);
            if (ImGui::ImageButton(reinterpret_cast<void *>(TextureCache["avatar"]), ImVec2(24, 24))) {
                fileBrowser = true;
                title = reinterpret_cast<const char *>(u8"图片选择");
                typeFilters = {".png", ".jpg"};
                PathOnConfirm = [this]() {
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
            // Display the multi-line input field
            ImGui::InputTextMultiline(("##" + to_string(botAnswer.timestamp)).c_str(),
                                      const_cast<char *>(botAnswer.content.c_str()), botAnswer.content.size() + 1,
                                      input_size,
                                      ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

            // 加载纹理的线程

            if (!botAnswer.image.empty()) {
                static bool wait = false;
                if (!SDCache.contains(botAnswer.image)) {
                    // 如果没有加载过该纹理
                    if (!wait) {
                        wait = true;
                        LogInfo("开始加载图片: {0}", Resources + "Images/" + botAnswer.image);
                        auto t = LoadTexture(Resources + "Images/" + botAnswer.image);
                        SDCache[botAnswer.image] = t;
                        wait = false;
                    }
                }
                else {
                    // 如果纹理已加载,显示
                    if (ImGui::ImageButton(reinterpret_cast<void *>(SDCache[botAnswer.image]),
                                           ImVec2(256, 256))) {
                        Utils::OpenFileManager(Utils::GetAbsolutePath(Resources + "Images/" + botAnswer.image));
                    }
                }
            }

            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), Utils::Stamp2Time(botAnswer.timestamp).c_str());

            // Restore the style
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(8);
        }
    }
    // 滚动到底部
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - ImGui::GetStyle().ItemSpacing.x - 20) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    // 显示菜单栏
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(reinterpret_cast<const char *>(u8"选项"))) {
            if (ImGui::Button(reinterpret_cast<const char *>(u8"保存"), ImVec2(200, 30))) {
                bot->Save(convid);
                save(convid);
            }
            if (ImGui::Button(reinterpret_cast<const char *>(u8"新建"), ImVec2(200, 30))) {
                // 显示输入框
                show_input_box = true;
            }
            if (ImGui::Button(reinterpret_cast<const char *>(u8"清空记录"), ImVec2(200, 30))) {
                chat_history.clear();
                save(convid);
            }
            if (ImGui::Button(reinterpret_cast<const char *>(u8"重置会话"), ImVec2(200, 30))) {
                chat_history.clear();
                bot->Reset();
                if (configure.claude.enable) {
                    FirstTime = Utils::GetCurrentTimestamp();
                }
                save(convid);
            }
            if (ImGui::Button(reinterpret_cast<const char *>(u8"删除当前会话"), ImVec2(200, 30)) &&
                conversations.size() > 1) {
                bot->Del(convid);
                del(convid);
                bot->Load(convid);
                load(convid);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(reinterpret_cast<const char *>(u8"会话"))) {
            ImGuiStyle&item_style = ImGui::GetStyle();
            item_style.Colors[ImGuiCol_Text] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
            if (!configure.claude.enable) {
                for (int i = 0; i < conversations.size(); i++) {
                    const bool is_selected = (SelectIndices["conversation"] == i);
                    if (ImGui::MenuItem(reinterpret_cast<const char *>(conversations[i].c_str()), nullptr,
                                        is_selected)) {
                        if (SelectIndices["conversation"] != i) {
                            SelectIndices["conversation"] = i;
                            convid = conversations[SelectIndices["conversation"]];
                            bot->Load(convid);
                            load(convid);
                        }
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(reinterpret_cast<const char *>(u8"模式"))) {
            ImGuiStyle&item_style = ImGui::GetStyle();
            item_style.Colors[ImGuiCol_Text] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
            for (int i = 0; i < roles.size(); i++) {
                const bool is_selected = (SelectIndices["role"] == i);
                if (ImGui::MenuItem(reinterpret_cast<const char *>(roles[i].c_str()), nullptr, is_selected)) {
                    if (SelectIndices["role"] != i) {
                        SelectIndices["role"] = i;
                        role = roles[SelectIndices["role"]];
                    }
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::End();
}

void Application::RenderInputBox() {
    static std::vector<std::shared_future<std::string>> submit_futures;
    if (listener && listener->IsRecorded()) {
        std::thread([&]() {
            listener->EndListen();
            listener->ResetRecorded();
            std::string path = "recorded" + std::to_string(Rnum) + ".wav";
            Rnum += 1;
            if (voiceToText && !path.empty()) {
                std::future<std::string> conversion_future;
                if (!whisperData.useLocalModel) {
                    // 启动异步任务
                    conversion_future = std::async(std::launch::async, [&]() {
                        return voiceToText->ConvertAsync(path).get();
                    });
                }
                else {
                    conversion_future = std::async(std::launch::async, [&]() {
                        return WhisperConvertor(path);
                    });
                }
                // 在异步任务完成后执行回调函数
                auto callback = [&](std::future<std::string>&future) {
                    std::string text = future.get();

                    if (!is_valid_text(text)) {
                        // 显示错误消息
                        LogWarn("Bot Warning: Your question is empty");
                        if (whisperData.enable)
                            listener->listen();
                    }
                    else {
                        text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
                        last_input = text;
                        Chat user;
                        user.timestamp = Utils::GetCurrentTimestamp();
                        user.content = last_input;
                        AddChatRecord(user);

                        Rnum -= 1;

                        std::remove(("recorded" + std::to_string(Rnum) + ".wav").c_str());
                        LogInfo("whisper: 删除录音{0}", "recorded" + std::to_string(Rnum) + ".wav");
                        std::shared_future<std::string> submit_future = std::async(std::launch::async, [&](void) {
                            return bot->Submit(last_input, role);
                        }).share(); {
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
    } {
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
        ImGui::Begin("Input filed", NULL,
                     ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoTitleBar);
        /*        ImGui::BeginGroup();
                // 设置子窗口的背景颜色为浅灰色
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        // 设置子窗口的圆角半径为10像素
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        // 创建一个子窗口，注意要设置大小和标志
        // 大小要和你的控件一致，标志要去掉边框和滚动条
                ImGui::BeginChild("background", ImVec2(500, 20), false,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                  ImGuiWindowFlags_NoDecoration);
        // 恢复默认的样式
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
        // 显示你的控件
                ImGui::SameLine();
                ImGui::Text(reinterpret_cast<const char *>(u8"总额: %.2f"), billing.total);
                ImGui::SameLine();
                ImGui::Text(reinterpret_cast<const char *>(u8"可用: %.2f"), billing.available);
                ImGui::SameLine();
                ImGui::Text(reinterpret_cast<const char *>(u8"剩余: %d 天"),
                            Utils::Stamp2Day(billing.date * 1000 - Utils::getCurrentTimestamp()));
                ImGui::SameLine();
        // 设置进度条的背景颜色为透明
                ImGui::PushStyleColor(ImGuiCol_PlotHistogramHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        // 设置进度条的前景颜色为白色
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.00f, 1.0f, 1.0f, 1.0f));
        // 设置进度条的圆角半径为10像素
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
        // 显示进度条，注意要设置frame_padding为-1，否则会有边框
                ImGui::ProgressBar(billing.used / billing.total, ImVec2(100, 20), NULL);
        // 恢复默认的样式
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
        // 结束子窗口
                ImGui::EndChild();
                ImGui::EndGroup();*/
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu(reinterpret_cast<const char *>(u8"指令"))) {
                if (ImGui::Button(reinterpret_cast<const char *>(u8"绘图"))) {
                    input_text += "/draw [ ]";
                }

                if (ImGui::Button(reinterpret_cast<const char *>(u8"帮助"))) {
                    input_text += "/help";
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
        token = countTokens(input_buffer);
        ImGui::Text(reinterpret_cast<const char *>(u8"字符数: %d"), token);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }


    strcpy_s(input_buffer, input_text.c_str());
    if (ImGui::InputTextMultiline("##Input Text", input_buffer, sizeof(input_buffer), ImVec2(-1, -1),
                                  ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CtrlEnterForNewLine)) {
        last_input = input_text = input_buffer;
        input_text.clear();
    }

    if ((strlen(last_input.c_str()) > 0 && ImGui::IsKeyPressed(ImGuiKey_Enter) && (!ImGui::GetIO().KeyCtrl))) {
        Chat user;
        std::string cmd, args;
        user.timestamp = Utils::GetCurrentTimestamp();

        user.content = last_input;
        if (ContainsCommand(last_input, cmd, args)) {
            InlineCommand(cmd, args, user.timestamp);
        }
        if (is_valid_text(user.content)) {
            AddChatRecord(user);
        }
        if (is_valid_text(last_input)) {
            // 使用last_input作为键
            if (whisperData.enable)
                listener->ResetRecorded();
            std::shared_future<std::string> submit_future = std::async(std::launch::async, [&]() {
                LogInfo(last_input);
                return bot->Submit(last_input, role);
            }).share();


            submit_futures.push_back(std::move(submit_future));
        }
        input_text.clear();
        save(convid);
        bot->Save(convid);
    }
    else if ((ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl) ||
             (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyShift)) {
        input_text += "\n";
    }
    ImGui::End();

    // 处理已完成的submit_future
    for (auto it = submit_futures.begin(); it != submit_futures.end();) {
        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            if (!configure.claude.enable) {
                std::string response = it->get();
                std::lock_guard<std::mutex> lock(chat_history_mutex);
                Chat botR;
                botR.flag = 1;

                botR.timestamp = Utils::GetCurrentTimestamp();
                botR.content = response;
                AddChatRecord(botR);

                it = submit_futures.erase(it);
                auto tmp_code = Utils::GetAllCodesFromText(response);
                save(convid);
                bot->Save(convid);
                for (auto&i: tmp_code) {
                    std::size_t pos = i.find('\n'); // 查找第一个换行符的位置
                    std::string codetype;
                    if (pos != std::string::npos) {
                        // 如果找到了换行符
                        codetype = i.substr(0, pos);
                        i = i.substr(pos + 1); // 删除第一行
                    }
                    if (!is_valid_text(codetype))
                        codetype = "Unknown";
                    if (codes.contains(codetype)) {
                        codes[codetype].emplace_back(i);
                    }
                    else {
                        codes.insert({codetype, {i}});
                    }
                    ImGui::SetClipboardText(i.c_str());
                }
                if (vits && vitsData.enable) {
                    std::string VitsText = translator->translate(Utils::ExtractNormalText(response),
                                                                 vitsData.lanType);
                    Vits(VitsText);
                }
                else if (whisperData.enable) {
                    listener->listen();
                }
            }
            else {
                it = submit_futures.erase(it);
            }
        }
        else {
            ++it;
        }
    }
    ImGui::PopStyleVar(11);
}

void Application::RenderConversationBox() {
    UniversalStyle();
    ImGui::Begin(reinterpret_cast<const char *>(u8"会话"), NULL,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav);
    if (ImGui::ImageButton(reinterpret_cast<void *>(TextureCache["add"]), ImVec2(16, 16))) {
        // 显示输入框
        show_input_box = true;
    }
    ImGui::SameLine();
    ImGui::Text(reinterpret_cast<const char *>(u8"新建会话"));
    for (const auto&conversation: conversations) {
        ImGui::BeginGroup();
        // 消息框
        ImGui::Image(reinterpret_cast<void *>(TextureCache["message"]), ImVec2(32, 32),
                     ImVec2(0, 0),
                     ImVec2(1, 1));

        // 消息文本
        ImGui::SameLine();
        if (ImGui::Button(conversation.c_str(), ImVec2(100, 30))) {
            convid = conversation;
            bot->Load(convid);
            load(convid);
        }

        // 删除按钮
        ImGui::SameLine();
        ImGui::PushID(conversation.c_str());
        if (ImGui::ImageButton(reinterpret_cast<void *>(TextureCache["del"]), ImVec2(16, 16)) &&
            conversations.size() > 1) {
            convid = conversation;
            bot->Del(convid);
            del(convid);
            bot->Load(convid);
            load(convid);
            ImGui::PopID();
            ImGui::EndGroup();
            break;
        }
        ImGui::PopID();
        ImGui::EndGroup();
    }
    ImGui::End();
}

void Application::RenderConfigBox() {
    UniversalStyle();
    static bool no_key = false;
    static bool first = false;
    switch (state) {
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

    ImGui::Begin(reinterpret_cast<const char *>(u8"配置"), NULL,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);

    // 显示 ChatBot 配置
    if (ImGui::CollapsingHeader("ChatBot")) {
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用Claude (实验性功能)"), &configure.claude.enable);
        if (configure.claude.enable) {
            configure.gemini.enable = false;
            configure.openAi.enable = false;
            configure.gptLike.enable = false;
        }
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用Gemini"), &configure.gemini.enable);
        if (configure.gemini.enable) {
            configure.claude.enable = false;
            configure.openAi.enable = false;
            configure.gptLike.enable = false;
        }
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用OpenAI"), &configure.openAi.enable);
        if (configure.openAi.enable) {
            configure.claude.enable = false;
            configure.gemini.enable = false;
            configure.gptLike.enable = false;
        }
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用GPT-Like"), &configure.gptLike.enable);
        if (configure.gptLike.enable) {
            configure.claude.enable = false;
            configure.gemini.enable = false;
            configure.openAi.enable = false;
        }


        if (configure.openAi.enable) {
            static bool showPassword = false, clicked = false;
            static double lastInputTime = 0.0;
            double currentTime = ImGui::GetTime();
            strcpy_s(GetBufferByName("api").buffer, configure.openAi.api_key.c_str());
            strcpy_s(GetBufferByName("endPoint").buffer, configure.openAi._endPoint.c_str());
            strcpy_s(GetBufferByName("model").buffer, configure.openAi.model.c_str());
            strcpy_s(GetBufferByName("proxy").buffer, configure.openAi.proxy.c_str());
            if (currentTime - lastInputTime > 0.5) {
                showPassword = false;
            }
            if (showPassword || clicked) {
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"OpenAI API Key"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer))) {
                    configure.openAi.api_key = GetBufferByName("api").buffer;
                }
            }
            else {
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"OpenAI API Key"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer),
                                     ImGuiInputTextFlags_Password)) {
                    configure.openAi.api_key = GetBufferByName("api").buffer;
                    showPassword = true;
                    lastInputTime = ImGui::GetTime();
                }
            }
            ImGui::SameLine();
            if (ImGui::ImageButton(reinterpret_cast<void *>(TextureCache["eye"]),
                                   ImVec2(16, 16),
                                   ImVec2(0, 0),
                                   ImVec2(1, 1),
                                   -1,
                                   ImVec4(0, 0, 0, 0),
                                   ImVec4(1, 1, 1, 1))) {
                clicked = !clicked;
            }
            if (ImGui::InputText(reinterpret_cast<const char *>(u8"OpenAI 模型"), GetBufferByName("model").buffer,
                                 TEXT_BUFFER)) {
                configure.openAi.model = GetBufferByName("model").buffer;
            }
            ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用远程代理"), &configure.openAi.useWebProxy);
            if (!configure.openAi.useWebProxy)
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"对OpenAI使用的代理"),
                                     GetBufferByName("proxy").buffer,
                                     TEXT_BUFFER)) {
                    configure.openAi.proxy = GetBufferByName("proxy").buffer;
                }
                else {
                    if (ImGui::InputText(reinterpret_cast<const char *>(u8"远程接入点"), GetBufferByName("endPoint").buffer,
                                         TEXT_BUFFER)) {
                        configure.openAi._endPoint = GetBufferByName("endPoint").buffer;
                    }
                }
        }
        else if (configure.claude.enable) {
            /*            ImGui::InputText(reinterpret_cast<const char *>(u8"ChatGLM的位置"),
                                         configure.openAi.modelPath.data(),
                                         TEXT_BUFFER);*/
            static bool showPassword = false, clicked = false;
            static double lastInputTime = 0.0;
            double currentTime = ImGui::GetTime();
            strcpy_s(GetBufferByName("api").buffer, configure.claude.slackToken.c_str());
            if (currentTime - lastInputTime > 0.5) {
                showPassword = false;
            }
            if (showPassword || clicked) {
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"Claude Token"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer))) {
                    configure.claude.slackToken = GetBufferByName("api").buffer;
                }
            }
            else {
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"Claude Token"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer),
                                     ImGuiInputTextFlags_Password)) {
                    configure.claude.slackToken = GetBufferByName("api").buffer;
                    showPassword = true;
                    lastInputTime = ImGui::GetTime();
                }
            }
            ImGui::SameLine();
            if (ImGui::ImageButton(reinterpret_cast<void *>(TextureCache["eye"]),
                                   ImVec2(16, 16),
                                   ImVec2(0, 0),
                                   ImVec2(1, 1),
                                   -1,
                                   ImVec4(0, 0, 0, 0),
                                   ImVec4(1, 1, 1, 1))) {
                clicked = !clicked;
            }
            /*            ImGui::InputText(reinterpret_cast<const char *>(u8"用户名"), configure.claude.userName.data(),
                                         TEXT_BUFFER);


                        ImGui::InputText("Claude Cookie",
                                         configure.claude.cookies.data(),
                                         TEXT_BUFFER);*/

            if (ImGui::InputText(reinterpret_cast<const char *>(u8"Claude ID"), configure.claude.channelID.data(),
                                 TEXT_BUFFER)) {
            }
        }
        else if (configure.gemini.enable) {
            static bool showPassword = false, clicked = false;
            static double lastInputTime = 0.0;
            double currentTime = ImGui::GetTime();
            strcpy_s(GetBufferByName("api").buffer, configure.gemini._apiKey.c_str());
            strcpy_s(GetBufferByName("endPoint").buffer, configure.gemini._endPoint.c_str());
            if (currentTime - lastInputTime > 0.5) {
                showPassword = false;
            }
            if (showPassword || clicked) {
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"ApiKey"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer))) {
                    configure.gemini._apiKey = GetBufferByName("api").buffer;
                }
            }
            else {
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"ApiKey"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer),
                                     ImGuiInputTextFlags_Password)) {
                    configure.gemini._apiKey = GetBufferByName("api").buffer;
                    showPassword = true;
                    lastInputTime = ImGui::GetTime();
                }
            }
            ImGui::SameLine();
            if (ImGui::ImageButton(reinterpret_cast<void *>(TextureCache["eye"]),
                                   ImVec2(16, 16),
                                   ImVec2(0, 0),
                                   ImVec2(1, 1),
                                   -1,
                                   ImVec4(0, 0, 0, 0),
                                   ImVec4(1, 1, 1, 1))) {
                clicked = !clicked;
            }
            if (ImGui::InputText(reinterpret_cast<const char *>(u8"远程接入点"), GetBufferByName("endPoint").buffer,
                                 TEXT_BUFFER)) {
                configure.gemini._endPoint = GetBufferByName("endPoint").buffer;
            }
        }
        else if (configure.gptLike.enable) {
            static bool showPassword = false, clicked = false;
            static double lastInputTime = 0.0;
            double currentTime = ImGui::GetTime();
            strcpy_s(GetBufferByName("api").buffer, configure.gptLike.api_key.c_str());
            strcpy_s(GetBufferByName("endPoint").buffer, configure.gptLike.apiEndPoint.c_str());
            strcpy_s(GetBufferByName("model").buffer, configure.gptLike.model.c_str());
            if (currentTime - lastInputTime > 0.5) {
                showPassword = false;
            }
            if (showPassword || clicked) {
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"ApiKey"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer))) {
                    configure.gptLike.api_key = GetBufferByName("api").buffer;
                }
            }
            else {
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"ApiKey"),
                                     GetBufferByName("api").buffer,
                                     sizeof(input_buffer),
                                     ImGuiInputTextFlags_Password)) {
                    configure.gptLike.api_key = GetBufferByName("api").buffer;
                    showPassword = true;
                    lastInputTime = ImGui::GetTime();
                }
            }
            ImGui::SameLine();
            if (ImGui::ImageButton(reinterpret_cast<void *>(TextureCache["eye"]),
                                   ImVec2(16, 16),
                                   ImVec2(0, 0),
                                   ImVec2(1, 1),
                                   -1,
                                   ImVec4(0, 0, 0, 0),
                                   ImVec4(1, 1, 1, 1))) {
                clicked = !clicked;
            }
            if (ImGui::InputText(reinterpret_cast<const char *>(u8"API地址"), GetBufferByName("endPoint").buffer,
                                 TEXT_BUFFER)) {
                configure.gptLike.apiEndPoint = GetBufferByName("endPoint").buffer;
            }
            if (ImGui::InputText(reinterpret_cast<const char *>(u8"模型名称"), GetBufferByName("model").buffer,
                                 TEXT_BUFFER)) {
                configure.gptLike.model = GetBufferByName("model").buffer;
            }
        }
    }

    // 显示百度翻译配置
    if (ImGui::CollapsingHeader(reinterpret_cast<const char *>(u8"百度翻译"))) {
        ImGui::InputText("BaiDu App ID", configure.baiDuTranslator.appId.data(),
                         TEXT_BUFFER);
        static bool showbaiduPassword = false, baiduclicked = false;
        static double baidulastInputTime = 0.0;

        double baiducurrentTime = ImGui::GetTime();
        strcpy_s(GetBufferByName("api").buffer, configure.baiDuTranslator.APIKey.c_str());
        if (baiducurrentTime - baidulastInputTime > 0.5) {
            showbaiduPassword = false;
        }
        if (showbaiduPassword || baiduclicked) {
            if (ImGui::InputText("BaiDu API Key", GetBufferByName("api").buffer,
                                 sizeof(GetBufferByName("api").buffer))) {
                configure.baiDuTranslator.APIKey = GetBufferByName("api").buffer;
            }
        }
        else {
            if (ImGui::InputText("BaiDu API Key", GetBufferByName("api").buffer,
                                 sizeof(GetBufferByName("api").buffer), ImGuiInputTextFlags_Password)) {
                configure.baiDuTranslator.APIKey = GetBufferByName("api").buffer;
                showbaiduPassword = true;
                baidulastInputTime = ImGui::GetTime();
            }
        }
        ImGui::SameLine();
        if (ImGui::ImageButton(reinterpret_cast<void *>(TextureCache["eye"]),
                               ImVec2(16, 16),
                               ImVec2(0, 0),
                               ImVec2(1, 1),
                               1,
                               ImVec4(0, 0, 0, 0),
                               ImVec4(1, 1, 1, 1))) {
            baiduclicked = !baiduclicked;
        }

        if (ImGui::InputText(reinterpret_cast<const char *>(u8"对百度使用的代理"),
                             GetBufferByName("proxy").buffer,
                             TEXT_BUFFER)) {
            configure.baiDuTranslator.proxy = GetBufferByName("proxy").buffer;
        }
    }

    // 显示 VITS 配置
    if (ImGui::CollapsingHeader("VITS")) {
        strcpy_s(GetBufferByName("lan").buffer, configure.vits.lanType.c_str());
        strcpy_s(GetBufferByName("endPoint").buffer, configure.vits.apiEndPoint.c_str());
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"启用Vits"), &configure.vits.enable);
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"GPT-SoVITS"), &configure.vits.UseGptSovite);
        if (ImGui::InputText(reinterpret_cast<const char *>(u8"语言类型"), GetBufferByName("lan").buffer, TEXT_BUFFER)) {
            configure.vits.lanType = GetBufferByName("lan").buffer;
        }
        if (!configure.vits.UseGptSovite) {
            vitsModels = Utils::GetDirectories(model + VitsPath);
            auto it = std::find(vitsModels.begin(), vitsModels.end(), configure.vits.modelName);
            if (it != vitsModels.end()) {
                // 找到了,计算索引
                SelectIndices["vits"] = std::distance(vitsModels.begin(), it);
                if (UFile::Exists(configure.vits.config)) {
                    std::string config;
                    config = Utils::ReadFile(configure.vits.config);
                    if (!config.empty()) {
                        json _config = json::parse(config);
                        if (_config["speakers"] != nullptr)
                            speakers = Utils::JsonArrayToStringVector(_config["speakers"]);
                        else if (_config["data"]["spk2id"] != nullptr)
                            speakers = Utils::JsonDictToStringVector(_config["data"]["spk2id"]);
                    }
                }
            }
            else {
                // 没找到,设置为0
                SelectIndices["vits"] = 0;
            }

            static char search_text[256] = "";
            /*        ImGui::InputText(reinterpret_cast<const char *>(u8"Vits 模型位置"), configure.vits.model.data(),
                                     TEXT_BUFFER);*/
            // 开始下拉列表
            if (ImGui::BeginCombo(reinterpret_cast<const char *>(u8"模型"),
                                  Utils::GetFileName(vitsModels[SelectIndices["vits"]]).c_str())) {
                // 在下拉框中添加一个文本输入框
                ImGui::InputTextWithHint("##Search1", reinterpret_cast<const char *>(u8"搜索"), search_text,
                                         sizeof(search_text));

                // 遍历所有选项
                for (int i = 0; i < vitsModels.size(); i++) {
                    // 如果搜索关键字为空，或者当前选项匹配搜索关键字
                    if (search_text[0] == '\0' ||
                        strstr(Utils::GetFileName(vitsModels[i]).c_str(), search_text) != nullptr) {
                        bool is_selected = (SelectIndices["vits"] == i);
                        if (ImGui::Selectable(Utils::GetFileName(vitsModels[i]).c_str(), is_selected)) {
                            SelectIndices["vits"] = i;
                        }
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                }
                configure.vits.modelName = vitsModels[SelectIndices["vits"]].c_str();
                // 检查vitsModels数组不为空并且当前项不为"empty"
                if (vitsModels.size() > 0 && vitsModels[SelectIndices["vits"]] != "empty") {
                    // 获取.pth和.json文件
                    std::vector<std::string> pth_files = Utils::GetFilesWithExt(vitsModels[SelectIndices["vits"]] + "/",
                        ".pth");
                    std::vector<std::string> json_files = Utils::GetFilesWithExt(
                        vitsModels[SelectIndices["vits"]] + "/",
                        ".json");

                    // 设置模型和配置文件
                    configure.vits.model = Utils::GetDefaultIfEmpty(pth_files, "model.pth");
                    configure.vits.config = Utils::GetDefaultIfEmpty(json_files, "config.json");

                    // 检查配置文件是否存在
                    if (UFile::Exists(configure.vits.config)) {
                        // 读取配置文件
                        std::string config = Utils::ReadFile(configure.vits.config);

                        if (!config.empty()) {
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
            if (vitsModels[SelectIndices["vits"]] != "empty") {
                if (ImGui::BeginCombo(reinterpret_cast<const char *>(u8"角色"),
                                      speakers[configure.vits.speaker_id].c_str())) {
                    // 在下拉框中添加一个文本输入框
                    ImGui::InputTextWithHint("##Search", reinterpret_cast<const char *>(u8"搜索"), search_text,
                                             sizeof(search_text));

                    // 遍历所有选项
                    for (int i = 0; i < speakers.size(); i++) {
                        // 如果搜索关键字为空，或者当前选项匹配搜索关键字
                        if (search_text[0] == '\0' || strstr(speakers[i].c_str(), search_text) != nullptr) {
                            bool is_selected = (configure.vits.speaker_id == i);
                            if (ImGui::Selectable(speakers[i].c_str(), is_selected)) {
                                configure.vits.speaker_id = i;
                            }
                            if (is_selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }
        else {
            if (ImGui::InputText(reinterpret_cast<const char *>(u8"GPT-SoVITS Web API地址"),
                                 GetBufferByName("endPoint").buffer,
                                 TEXT_BUFFER)) {
                configure.vits.apiEndPoint = GetBufferByName("endPoint").buffer;
            }
        }
    }

#ifdef WIN32
    if (ImGui::CollapsingHeader("Live2D")) {
        live2dModel = Utils::GetDirectories(model + Live2DPath);
        auto it = std::find(live2dModel.begin(), live2dModel.end(), configure.live2D.model);
        if (it != live2dModel.end() && live2dModel.size() > 1) {
            SelectIndices["Live2D"] = std::distance(live2dModel.begin(), it);
            lConfigure.model = configure.live2D.model;
        }
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"启用Live2D"), &configure.live2D.enable);
        ImGui::InputText(reinterpret_cast<const char *>(u8"Live2D 可执行文件"), configure.live2D.bin.data(),
                         TEXT_BUFFER);
        ImGui::InputFloat(reinterpret_cast<const char *>(u8"宽度缩放(400 * ScaleX)"), &lConfigure.scaleX,
                          0.1f, 1.0f, "%.2f");
        ImGui::InputFloat(reinterpret_cast<const char *>(u8"高度缩放(400 * ScaleY)"), &lConfigure.scaleY,
                          0.1f, 1.0f, "%.2f");

        if (ImGui::BeginCombo(reinterpret_cast<const char *>(u8"Live2D 模型"),
                              Utils::GetFileName(live2dModel[SelectIndices["Live2D"]]).c_str())) // 开始下拉列表
        {
            for (int i = 0; i < live2dModel.size(); i++) {
                bool is_selected = (SelectIndices["Live2D"] == i);
                if (ImGui::Selectable(Utils::GetFileName(live2dModel[i]).c_str(), is_selected)) {
                    SelectIndices["Live2D"] = i;
                    if (SelectIndices["conversation"] = 0) {
                        configure.live2D.enable = false;
                    }
                    else {
                        configure.live2D.enable = true;
                    }
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus(); // 默认选中项
                }
            }
            configure.live2D.model = live2dModel[SelectIndices["Live2D"]].c_str();
            ImGui::EndCombo(); // 结束下拉列表
        }
    }
#endif
    // 显示 Whisper 配置
    if (ImGui::CollapsingHeader("Whisper")) {
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"启用Whisper"), &configure.whisper.enable);
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用本地模型"), &configure.whisper.useLocalModel);
        auto it = std::find(tmpWhisperModel.begin(), tmpWhisperModel.end(), configure.whisper.model);
        if (it != tmpWhisperModel.end() && tmpWhisperModel.size() > 1) {
            SelectIndices["whisper"] = std::distance(tmpWhisperModel.begin(), it);
        }
        if (ImGui::BeginCombo(reinterpret_cast<const char *>(u8"Whisper 模型"),
                              Utils::GetFileName(whisperModel[SelectIndices["whisper"]]).c_str())) // 开始下拉列表
        {
            for (int i = 0; i < whisperModel.size(); i++) {
                bool is_selected = (SelectIndices["whisper"] == i);
                if (ImGui::Selectable(Utils::GetFileName(whisperModel[i]).c_str(), is_selected)) {
                    SelectIndices["whisper"] = i;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus(); // 默认选中项
                }
            }
            configure.whisper.model = tmpWhisperModel[SelectIndices["whisper"]];
            ImGui::EndCombo(); // 结束下拉列表
        }
    }

    //显示Stable Diffusion配置
    if (ImGui::CollapsingHeader("Stable Diffusion")) {
        static char search_text[256] = "";
        ImGui::InputText(reinterpret_cast<const char *>(u8"Http API"), configure.stableDiffusion.apiPath.data(),
                         TEXT_BUFFER);
        ImGui::InputText(reinterpret_cast<const char *>(u8"屏蔽词汇"), configure.stableDiffusion.negative_prompt.data(),
                         TEXT_BUFFER);
        ImGui::InputInt(reinterpret_cast<const char *>(u8"迭代次数(与质量相关)"), &configure.stableDiffusion.steps);
        ImGui::InputInt(reinterpret_cast<const char *>(u8"高"), &configure.stableDiffusion.height);
        ImGui::InputInt(reinterpret_cast<const char *>(u8"宽"), &configure.stableDiffusion.width);
        ImGui::InputFloat(reinterpret_cast<const char *>(u8"相关度"), &configure.stableDiffusion.cfg_scale, 0.1f, 1.0f,
                          "%.2f");
        auto it = std::find(sampleIndices.begin(), sampleIndices.end(), configure.stableDiffusion.sampler_index);
        if (it != sampleIndices.end()) {
            // 找到了,计算索引
            SelectIndices["stableDiffusion"] = std::distance(sampleIndices.begin(), it);
        }
        if (ImGui::BeginCombo(reinterpret_cast<const char *>(u8"采样方法"),
                              sampleIndices[SelectIndices["stableDiffusion"]].c_str())) {
            // 在下拉框中添加一个文本输入框
            ImGui::InputTextWithHint("##Search", reinterpret_cast<const char *>(u8"搜索"), search_text,
                                     sizeof(search_text));

            // 遍历所有选项
            for (int i = 0; i < sampleIndices.size(); i++) {
                // 如果搜索关键字为空，或者当前选项匹配搜索关键字
                if (search_text[0] == '\0' || strstr(sampleIndices[i].c_str(), search_text) != nullptr) {
                    bool is_selected = (SelectIndices["stableDiffusion"] == i);
                    if (ImGui::Selectable(sampleIndices[i].c_str(), is_selected)) {
                        configure.stableDiffusion.sampler_index = sampleIndices[i];
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndCombo();
        }
    }

    // 保存配置
    if (ImGui::Button(reinterpret_cast<const char *>(u8"保存配置"))) {
        Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
        Utils::SaveYaml("Lconfig.yml", Utils::toYaml(lConfigure));
        should_restart = true;
    }

    ImGui::End(); {
        if (should_restart) {
            ImGui::OpenPopup(reinterpret_cast<const char *>(u8"需要重启应用"));
            should_restart = false;
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char *>(u8"需要重启应用"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(reinterpret_cast<const char *>(u8"您需要重新启动应用程序以使更改生效。"));
            if (ImGui::Button(reinterpret_cast<const char *>(u8"确定"), ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
                std::string executable = "CyberGirl" + exeSuffix;
                const char* argv[] = {executable.c_str(), NULL};
                execvp(executable.c_str(), const_cast<char *const *>(argv));
                //std::exit(0);
            }

            ImGui::SameLine(ImGui::GetWindowSize().x - 120 - ImGui::GetStyle().ItemSpacing.x);
            if (ImGui::Button(reinterpret_cast<const char *>(u8"取消"), ImVec2(80, 0))) {
                // 点击取消按钮时，关闭Popup
                ImGui::CloseCurrentPopup();
            }


            ImGui::EndPopup();
        }

        if (first) {
            ImGui::OpenPopup(reinterpret_cast<const char *>(u8"初始化提醒"));
            first = false;
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char *>(u8"初始化提醒"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(reinterpret_cast<const char *>(u8"您是第一次启动本应用,请完成下方的配置填写。"));
            if (ImGui::Button(reinterpret_cast<const char *>(u8"确定"), ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (no_key) {
            ImGui::OpenPopup(reinterpret_cast<const char *>(u8"需要配置OpenAI Key或者填写Claude token"));
            no_key = false;
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char *>(u8"需要配置OpenAI Key或者填写Claude token"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(reinterpret_cast<const char *>(u8"您需要填写OpenAI API key 或Claude token 以正常启用本程序。"));
            if (ImGui::Button(reinterpret_cast<const char *>(u8"确定"), ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

void Application::RenderUI() {
    ImGui::DockSpaceOverViewport();
    RuntimeDetector();
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!OnlySetting) {
        RenderInputBox();
        RenderPopupBox();
        RenderConfigBox();
        RenderChatBox();
        RenderCodeBox();
        RenderConversationBox();
        FileChooser();
    }
    else {
        RenderConfigBox();
    }
}

void Application::initImGuiBindings(sol::state_view lua) {
    lua.set_function("ImVec4", sol::overload(
                         []() { return ImVec4(0, 0, 0, 0); },
                         [](float x) { return ImVec4(x, 0, 0, 0); },
                         [](float x, float y) { return ImVec4(x, y, 0, 0); },
                         [](float x, float y, float z) { return ImVec4(x, y, z, 0); },
                         [](float x, float y, float z, float w) { return ImVec4(x, y, z, w); }
                     ));


    lua.set_function("ImVec2", sol::overload(
                         []() { return ImVec2(0, 0); },
                         [](float x) { return ImVec2(x, 0); },
                         [](float x, float y) { return ImVec2(x, y); }
                     ));

    lua.set_function("UIBegin", sol::overload(
                         [](const char* name) { return ImGui::Begin(name); },
                         [](const char* name, bool* p_open) { return ImGui::Begin(name, p_open); },
                         [](const char* name, bool* p_open, ImGuiWindowFlags flags) {
                             return ImGui::Begin(name, p_open, flags);
                         }
                     ));
    lua.set_function("UIEnd", ImGui::End);

    // ImGui Text 函数的绑定
    lua.set_function("UIText", &ImGui::Text);

    lua.set_function("UItextUnformatted", sol::overload(
                         [](const char* text) { ImGui::TextUnformatted(text); },
                         [](const char* text, const char* text_end) { ImGui::TextUnformatted(text, text_end); }
                     ));

    lua.set_function("UItextColored", &ImGui::TextColored);

    lua.set_function("UItextColoredV", &ImGui::TextColored);

    lua.set_function("UItextDisabled", &ImGui::TextDisabled);

    lua.set_function("UItextWrapped", &ImGui::TextWrapped);

    lua.set_function("UIlabelText", &ImGui::LabelText);

    lua.set_function("UIbulletText", &ImGui::BulletText);

    lua.set_function("UIseparatorText", &ImGui::SeparatorText);

    lua.set_function("UIDragFloat", sol::overload(
                         [](const char* label, float* v, float v_speed, float v_min, float v_max,
                            const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[2], float v_speed, float v_min, float v_max,
                            const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragFloat2(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[3], float v_speed, float v_min, float v_max,
                            const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[4], float v_speed, float v_min, float v_max,
                            const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragFloat4(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, float* v_current_min, float* v_current_max, float v_speed,
                            float v_min, float v_max, const char* format, const char* format_max,
                            ImGuiSliderFlags flags) {
                             return ImGui::DragFloatRange2(label, v_current_min, v_current_max, v_speed, v_min,
                                                           v_max, format, format_max, flags);
                         }
                     ));

    lua.set_function("UIDragInt", sol::overload(
                         [](const char* label, int* v, float v_speed, int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[2], float v_speed, int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::DragInt2(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[3], float v_speed, int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::DragInt3(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[4], float v_speed, int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::DragInt4(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, int* v_current_min, int* v_current_max, float v_speed, int v_min,
                            int v_max, const char* format, const char* format_max, ImGuiSliderFlags flags) {
                             return ImGui::DragIntRange2(label, v_current_min, v_current_max, v_speed, v_min, v_max,
                                                         format, format_max, flags);
                         }
                     ));

    lua.set_function("UIDragScalar", sol::overload(
                         [](const char* label, ImGuiDataType data_type, void* p_data, float v_speed,
                            const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragScalar(label, data_type, p_data, v_speed, p_min, p_max, format,
                                                      flags);
                         },
                         [](const char* label, ImGuiDataType data_type, void* p_data, int components, float v_speed,
                            const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragScalarN(label, data_type, p_data, components, v_speed, p_min, p_max,
                                                       format, flags);
                         }
                     ));

    lua.set_function("UISliderFloat", sol::overload(
                         [](const char* label, float* v, float v_min, float v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[2], float v_min, float v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderFloat2(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[3], float v_min, float v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderFloat3(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[4], float v_min, float v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderFloat4(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, float* v, float v_min, float v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderAngle(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, int* v, int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderInt(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[2], int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderInt2(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[3], int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderInt3(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[4], int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderInt4(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, ImGuiDataType data_type, void* p_data, const void* p_min,
                            const void* p_max, const char* format, ImGuiSliderFlags flags) {
                             return ImGui::SliderScalar(label, data_type, p_data, p_min, p_max, format, flags);
                         },
                         [](const char* label, ImGuiDataType data_type, void* p_data, int components,
                            const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                             return ImGui::SliderScalarN(label, data_type, p_data, components, p_min, p_max, format,
                                                         flags);
                         }
                     ));

    // 绑定 Button 函数
    lua.set_function("UIButton", sol::overload(
                         [](const char* label) { return ImGui::Button(label); },
                         [](const char* label, const ImVec2&size) { return ImGui::Button(label, size); }
                     ));

    // 绑定 Checkbox 函数
    lua.set_function("UICheckbox", &ImGui::Checkbox);

    // 绑定 RadioButton 函数
    lua.set_function("UIRadioButton", sol::overload(
                         [](const char* label, bool active) { return ImGui::RadioButton(label, active); },
                         [](const char* label, int* v, int v_button) {
                             return ImGui::RadioButton(label, v, v_button);
                         }
                     ));

    // 绑定 ProgressBar 函数
    lua.set_function("ProgressBar", sol::overload(
                         [](float fraction, const ImVec2&size_arg, const char* overlay) {
                             ImGui::ProgressBar(fraction, size_arg, overlay);
                         },
                         [](float fraction, const ImVec2&size_arg) {
                             ImGui::ProgressBar(fraction, size_arg);
                         },
                         [](float fraction) {
                             ImGui::ProgressBar(fraction);
                         }
                     ));

    // 绑定 Bullet 函数
    lua.set_function("UIBullet", &ImGui::Bullet);

    // 绑定 Image 函数
    lua.set_function("UIImage", sol::overload(
                         [](ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0, const ImVec2&uv1,
                            const ImVec4&tint_col, const ImVec4&border_col) {
                             ImGui::Image(user_texture_id, size, uv0, uv1, tint_col, border_col);
                         },
                         [](ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0, const ImVec2&uv1,
                            const ImVec4&tint_col) {
                             ImGui::Image(user_texture_id, size, uv0, uv1, tint_col);
                         },
                         [](ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0, const ImVec2&uv1) {
                             ImGui::Image(user_texture_id, size, uv0, uv1);
                         },
                         [](ImTextureID user_texture_id, const ImVec2&size) {
                             ImGui::Image(user_texture_id, size);
                         }
                     ));
    lua.set_function("UIImageButton", sol::overload(
                         [](const char* str_id, ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0,
                            const ImVec2&uv1, const ImVec4&bg_col, const ImVec4&tint_col) {
                             return ImGui::ImageButton(str_id, user_texture_id, size, uv0, uv1, bg_col, tint_col);
                         },
                         [](const char* str_id, ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0,
                            const ImVec2&uv1, const ImVec4&bg_col) {
                             return ImGui::ImageButton(str_id, user_texture_id, size, uv0, uv1, bg_col);
                         },
                         [](const char* str_id, ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0,
                            const ImVec2&uv1) {
                             return ImGui::ImageButton(str_id, user_texture_id, size, uv0, uv1);
                         },
                         [](const char* str_id, ImTextureID user_texture_id, const ImVec2&size) {
                             return ImGui::ImageButton(str_id, user_texture_id, size);
                         }
                     ));

    // 绑定 Combo 函数
    lua.set_function("UICombo", sol::overload(
                         [](const char* label, int* current_item, const char* const items[], int items_count,
                            int popup_max_height_in_items) {
                             return ImGui::Combo(label, current_item, items, items_count,
                                                 popup_max_height_in_items);
                         },
                         [](const char* label, int* current_item, const char* items_separated_by_zeros,
                            int popup_max_height_in_items) {
                             return ImGui::Combo(label, current_item, items_separated_by_zeros,
                                                 popup_max_height_in_items);
                         }
                     ));

    lua.set_function("UIBeginCombo", sol::overload(
                         [](const char* label, const char* preview_value, ImGuiComboFlags flags) {
                             return ImGui::BeginCombo(label, preview_value, flags);
                         },
                         [](const char* label, const char* preview_value) {
                             return ImGui::BeginCombo(label, preview_value);
                         }
                     ));

    lua.set_function("UIEndCombo", &ImGui::EndCombo);

    lua.set_function("UISameLine", sol::overload(
                         [](float offset_from_start_x, float spacing) {
                             ImGui::SameLine(offset_from_start_x, spacing);
                         },
                         [](float offset_from_start_x) {
                             ImGui::SameLine(offset_from_start_x);
                         },
                         []() {
                             ImGui::SameLine();
                         }
                     ));

    lua.set_function("UINewLine", &ImGui::NewLine);

    lua.set_function("UISeparator", &ImGui::Separator);

    lua.set_function("UISpacing", &ImGui::Spacing);

    lua.set_function("UIDummy", &ImGui::Dummy);

    lua.set_function("UIIndent", sol::overload(
                         [](float indent_w) { ImGui::Indent(indent_w); },
                         []() { ImGui::Indent(); }
                     ));

    lua.set_function("UIUnindent", sol::overload(
                         [](float indent_w) { ImGui::Unindent(indent_w); },
                         []() { ImGui::Unindent(); }
                     ));

    lua.set_function("UIBeginGroup", &ImGui::BeginGroup);

    lua.set_function("UIEndGroup", &ImGui::EndGroup);

    lua.set_function("UIGetCursorPos", &ImGui::GetCursorPos);

    lua.set_function("UIGetCursorPosX", &ImGui::GetCursorPosX);

    lua.set_function("UIGetCursorPosY", &ImGui::GetCursorPosY);

    lua.set_function("UISetCursorPos", &ImGui::SetCursorPos);

    lua.set_function("UISetCursorPosX", &ImGui::SetCursorPosX);

    lua.set_function("UISetCursorPosY", &ImGui::SetCursorPosY);

    lua.set_function("UIGetCursorStartPos", &ImGui::GetCursorStartPos);

    lua.set_function("UIGetCursorScreenPos", &ImGui::GetCursorScreenPos);

    lua.set_function("UISetCursorScreenPos", &ImGui::SetCursorScreenPos);

    lua.set_function("UIAlignTextToFramePadding", &ImGui::AlignTextToFramePadding);

    lua.set_function("UIGetTextLineHeight", &ImGui::GetTextLineHeight);

    lua.set_function("UIGetTextLineHeightWithSpacing", &ImGui::GetTextLineHeightWithSpacing);

    lua.set_function("UIGetFrameHeight", &ImGui::GetFrameHeight);

    lua.set_function("UIGetFrameHeightWithSpacing", &ImGui::GetFrameHeightWithSpacing);

    lua.set_function("UISliderAngle",
                     [](const char* label, float* v_rad, float v_degrees_min, float v_degrees_max,
                        const char* format, ImGuiSliderFlags flags) {
                         return ImGui::SliderAngle(label, v_rad, v_degrees_min, v_degrees_max, format, flags);
                     });

    lua.set_function("UIVSliderFloat",
                     [](const char* label, const ImVec2&size, float* v, float v_min, float v_max,
                        const char* format, ImGuiSliderFlags flags) {
                         return ImGui::VSliderFloat(label, size, v, v_min, v_max, format, flags);
                     });

    lua.set_function("UIVSliderInt",
                     [](const char* label, const ImVec2&size, int* v, int v_min, int v_max, const char* format,
                        ImGuiSliderFlags flags) {
                         return ImGui::VSliderInt(label, size, v, v_min, v_max, format, flags);
                     });

    lua.set_function("UIVSliderScalar",
                     [](const char* label, const ImVec2&size, ImGuiDataType data_type, void* p_data,
                        const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                         return ImGui::VSliderScalar(label, size, data_type, p_data, p_min, p_max, format, flags);
                     });

    lua.set_function("UIInputText", [](const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
        return ImGui::InputText(label, buf, buf_size, flags);
    });

    lua.set_function("UIInputTextMultiline",
                     [](const char* label, char* buf, size_t buf_size, const ImVec2&size,
                        ImGuiInputTextFlags flags) {
                         return ImGui::InputTextMultiline(label, buf, buf_size, size, flags);
                     });

    lua.set_function("UIInputTextWithHint",
                     [](const char* label, const char* hint, char* buf, size_t buf_size,
                        ImGuiInputTextFlags flags) {
                         return ImGui::InputTextWithHint(label, hint, buf, buf_size, flags);
                     });

    lua.set_function("UIInputFloat",
                     [](const char* label, float* v, float step, float step_fast, const char* format,
                        ImGuiInputTextFlags flags) {
                         return ImGui::InputFloat(label, v, step, step_fast, format, flags);
                     });

    lua.set_function("UIInputFloat2",
                     [](const char* label, float v[2], const char* format, ImGuiInputTextFlags flags) {
                         return ImGui::InputFloat2(label, v, format, flags);
                     });

    lua.set_function("UIInputFloat3",
                     [](const char* label, float v[3], const char* format, ImGuiInputTextFlags flags) {
                         return ImGui::InputFloat3(label, v, format, flags);
                     });

    lua.set_function("UIInputFloat4",
                     [](const char* label, float v[4], const char* format, ImGuiInputTextFlags flags) {
                         return ImGui::InputFloat4(label, v, format, flags);
                     });

    lua.set_function("UIInputInt",
                     [](const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags) {
                         return ImGui::InputInt(label, v, step, step_fast, flags);
                     });

    lua.set_function("UIInputInt2", [](const char* label, int v[2], ImGuiInputTextFlags flags) {
        return ImGui::InputInt2(label, v, flags);
    });

    lua.set_function("UIInputInt3", [](const char* label, int v[3], ImGuiInputTextFlags flags) {
        return ImGui::InputInt3(label, v, flags);
    });

    lua.set_function("UIInputInt4", [](const char* label, int v[4], ImGuiInputTextFlags flags) {
        return ImGui::InputInt4(label, v, flags);
    });

    lua.set_function("UIInputDouble",
                     [](const char* label, double* v, double step, double step_fast, const char* format,
                        ImGuiInputTextFlags flags) {
                         return ImGui::InputDouble(label, v, step, step_fast, format, flags);
                     });

    lua.set_function("UIInputScalar",
                     [](const char* label, ImGuiDataType data_type, void* p_data, const void* p_step,
                        const void* p_step_fast, const char* format, ImGuiInputTextFlags flags) {
                         return ImGui::InputScalar(label, data_type, p_data, p_step, p_step_fast, format, flags);
                     });

    lua.set_function("UIInputScalarN",
                     [](const char* label, ImGuiDataType data_type, void* p_data, int components,
                        const void* p_step, const void* p_step_fast, const char* format,
                        ImGuiInputTextFlags flags) {
                         return ImGui::InputScalarN(label, data_type, p_data, components, p_step, p_step_fast,
                                                    format, flags);
                     });

    lua.set_function("UIColorEdit3", [](const char* label, float col[3], ImGuiColorEditFlags flags) {
        return ImGui::ColorEdit3(label, col, flags);
    });

    lua.set_function("UIColorEdit4", [](const char* label, float col[4], ImGuiColorEditFlags flags) {
        return ImGui::ColorEdit4(label, col, flags);
    });

    lua.set_function("UIColorPicker3", [](const char* label, float col[3], ImGuiColorEditFlags flags) {
        return ImGui::ColorPicker3(label, col, flags);
    });

    lua.set_function("UIColorPicker4",
                     [](const char* label, float col[4], ImGuiColorEditFlags flags, const float* ref_col) {
                         return ImGui::ColorPicker4(label, col, flags, ref_col);
                     });

    lua.set_function("UIBeginListBox", [](const char* label, const ImVec2&size) {
        return ImGui::BeginListBox(label, size);
    });

    lua.set_function("UIEndListBox", []() {
        ImGui::EndListBox();
    });

    lua.set_function("UIListBox",
                     [](const char* label, int* current_item, const sol::table&items, int height_in_items) {
                         std::vector<const char *> item_array;
                         for (auto&item: items) {
                             item_array.push_back(item.second.as<const char *>());
                         }
                         return ImGui::ListBox(label, current_item, item_array.data(),
                                               static_cast<int>(item_array.size()), height_in_items);
                     });

    lua.set_function("LoadImage", &Application::LoadTexture);
}

int Application::Renderer() {
    // 初始化GLFW
    if (!glfwInit()) {
        // 处理初始化失败
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 创建窗口
    GLFWwindow* window = glfwCreateWindow(800, 600, VERSION.c_str(), NULL, NULL);
    if (!window) {
        // 处理窗口创建失败
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
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
    ImGuiIO&io = ImGui::GetIO();

    ImFontConfig config;
    config.OversampleH = 1;
    config.OversampleV = 1;
    config.PixelSnapH = true;
    config.GlyphExtraSpacing = ImVec2(0.0f, 1.0f);
    ImFont* font = io.Fonts->AddFontFromFileTTF("Resources/font/default.otf", 16.0f, &config,
                                                io.Fonts->GetGlyphRangesChineseFull());
    IM_ASSERT(font != NULL);
    io.DisplayFramebufferScale = ImVec2(0.8f, 0.8f);
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // 启用Docking
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // 设置 ImGui 全局样式属性

    ImGuiStyle&style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(240 / 255.f, 248 / 255.f, 1.f, 1.f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(240 / 255.f, 248 / 255.f, 1.f, 1.f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(240 / 255.f, 248 / 255.f, 1.f, 1.f);
    //初始化插件
    // 初始化Lua
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    initImGuiBindings(L);
    // 加载Lua脚本
    auto dirs = UDirectory::GetSubDirectories(PluginPath);
    for (auto&dir: dirs) {
        std::string path = PluginPath + dir + "/Plugin.lua";
        if (!UFile::Exists(path)) {
            LogWarn("Found plugin directory but Plugin.lua can not found");
            continue;
        }
        std::string luaScript = Utils::ReadFile(path);
        if (luaL_dostring(L, luaScript.c_str())) {
            lua_error(L);
            return -1;
        }
        lua_getglobal(L, "Initialize");
        if (lua_isfunction(L, -1)) {
            lua_call(L, 0, 0);
        }
        PluginsScript.push_back(luaScript);
    }
#pragma omp parallel for
    for (auto&it: TextureCache) {
        it.second = LoadTexture(Resources + it.first + ".png");
    }

    // 渲染循环
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        RenderUI();
        // 在Lua中调用ImGui函数
        for (auto&script: PluginsScript) {
            if (luaL_dostring(L, script.c_str())) {
                lua_error(L);
                return -1;
            }
            lua_getglobal(L, "UIRenderer");
            if (lua_isfunction(L, -1)) {
                lua_call(L, 0, 0);
            }
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // 清理资源
    AppRunning = false;
    listener.reset();
    voiceToText.reset();
    bot.reset();
    translator.reset();
    // 关闭Lua
    lua_close(L);
    stbi_image_free(images[0].pixels);
    for (auto&it: TextureCache) {
        glDeleteTextures(1, &it.second);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

bool Application::CheckFileExistence(const std::string&filePath, const std::string&fileType,
                                     const std::string&executableFile, bool isExecutable) {
    if (isExecutable) {
        if (!UFile::Exists(filePath + executableFile + exeSuffix)) {
            LogWarn("Warning: {0} won't work because you don't have an executable \"{1}\" for {0} !",
                    fileType, filePath + executableFile + exeSuffix);
            LogWarn("Warning: Please download or compile it as an executable file and put it in this folder: {0} !",
                    filePath);
            return false;
        }
    }
    else {
        if (!UFile::Exists(filePath)) {
            LogWarn("Warning: {0} won't work because you don't have an file \"{1}\" for {0} !",
                    fileType, filePath);
            return false;
        }
    }

    return true;
}


std::string Application::WhisperConvertor(const std::string&file) {
    if (!CheckFileExistence(whisperData.model, "Whisper Model")) {
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

void Application::WhisperModelDownload(const std::string&model) {
    std::map<std::string, std::string> tasks = {
        {
            "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/" + model,
            this->model + WhisperPath + model
        }
    };
    whisper = Installer(tasks);
    //return whisper;
}

void Application::WhisperExeInstaller() {
    if (UFile::Exists(bin + WhisperPath + "Whisper.zip")) {
        whisper = Utils::Decompress(bin + WhisperPath + "Whisper.zip");
    }
    else {
        std::map<std::string, std::string> tasks = {
            {whisperUrl, bin + WhisperPath + "Whisper.zip"}
        };
        whisper = Installer(tasks) && Utils::Decompress(bin + WhisperPath + "Whisper.zip");
    }
    //return whisper;
}

void Application::VitsExeInstaller() {
    if (UFile::Exists(bin + VitsConvertor + vitsFile)) {
        vits = Utils::Decompress(bin + VitsConvertor + vitsFile);
    }
    else {
        std::map<std::string, std::string> tasks = {
            {VitsConvertUrl, bin + VitsConvertor + vitsFile}
        };
        vits = Installer(tasks) && Utils::Decompress(bin + VitsConvertor + vitsFile);
    }
    //return vits;
}

bool Application::Installer(std::map<std::string, std::string> tasks) {
    std::future<bool> download_future = Utils::UDownloads(tasks);
    bool success = download_future.get();
    if (success) {
        LogInfo("Application: Download completed successfully.");
        return true;
    }
    else {
        LogError("Application Error: Download failed.");
        return false;
    }
}


void Application::AddChatRecord(const Chat&data) {
    chat_history.push_back(data);
}

void Application::DeleteAllBotChat() {
    chat_history.erase(
        std::remove_if(chat_history.begin(), chat_history.end(),
                       [](const Chat&c) {
                           return c.flag == 1;
                       }),
        chat_history.end());
}

vector<Application::Chat> Application::load(std::string name) {
    if (UFile::Exists(ConversationPath.string() + name + ".yaml")) {
        std::ifstream session_file(ConversationPath.string() + name + ".yaml");
        session_file.imbue(std::locale("en_US.UTF-8"));
        chat_history.clear();
        if (session_file.is_open()) {
            // 从文件中读取 YAML 节点
            YAML::Node node = YAML::Load(session_file);

            // 将 YAML 节点映射到 chat_history
            for (const auto&record_node: node) {
                // 从 YAML 映射节点中读取 ChatRecord 对象的成员变量
                int flag = record_node["flag"].as<int>();
                long long timestamp;
                std::string content;
                Chat record;
                if (flag == 0) {
                    timestamp = record_node["user"]["timestamp"].as<long long>();
                    content = record_node["user"]["content"].as<std::string>();
                }
                else {
                    timestamp = record_node["bot"]["timestamp"].as<long long>();
                    content = record_node["bot"]["content"].as<std::string>();
                    record.image = record_node["bot"]["image"].as<std::string>();
                }

                record.flag = flag;
                record.content = content;
                record.timestamp = timestamp;

                // 创建一个新的 ChatRecord 对象，并将其添加到 chat_history 中
                chat_history.push_back(record);
            }

            session_file.close();
            LogInfo("Application : Load {0} successfully", name);
        }
        else {
            LogError("Application Error: Unable to load session {0},{1}", name, ".");
        }
    }
    else {
        LogError("Application Error: Unable to load session {0},{1}", name, ".");
    }
    return chat_history;
}

void Application::del(std::string name) {
    if (remove((ConversationPath.string() + name + ".yaml").c_str()) != 0) {
        LogError("Bot Error: Unable to delete session {0},{1}", name, ".");
    }
    conversations.erase(ranges::find(conversations, name));
    SelectIndices["conversation"] = 0;
    convid = conversations[SelectIndices["conversation"]];

    LogInfo("Bot : 删除 {0} 成功", name);
}

void Application::save(std::string name, bool out) {
    std::ofstream session_file(ConversationPath.string() + name + ".yaml");
    session_file.imbue(std::locale("en_US.UTF-8"));
    if (session_file.is_open()) {
        // 创建 YAML 节点
        YAML::Node node;

        // 将 chat_history 映射到 YAML 节点
        for (const auto&record: chat_history) {
            // 创建一个新的 YAML 映射节点
            YAML::Node record_node(YAML::NodeType::Map);

            // 将 ChatRecord 对象的成员变量映射到 YAML 映射节点中
            record_node["flag"] = record.flag;
            if (record.flag == 0) {
                record_node["user"]["timestamp"] = record.timestamp;
                record_node["user"]["content"] = record.content;
            }
            else {
                record_node["bot"]["timestamp"] = record.timestamp;
                record_node["bot"]["content"] = record.content;
                record_node["bot"]["image"] = record.image;
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
    else {
        if (out)
            LogError("Application Error: Unable to save session {0},{1}", name, ".");
    }
}

bool Application::Initialize() {
    bool success = true;
    if (!UDirectory::Exists(ConversationPath.string())) {
        UDirectory::Create(ConversationPath.string());
    }
    for (const auto&entry: std::filesystem::directory_iterator(ConversationPath)) {
        if (entry.is_regular_file()) {
            const auto&file_path = entry.path();
            std::string file_name = file_path.stem().string();
            if (!file_name.empty() && UFile::EndsWith(file_path.string(), ".dat")) {
                conversations.emplace_back(file_name);
            }
            if (!conversations.size() > 0) {
                bot->Add(convid);
                conversations.emplace_back(convid);
            }
        }
    }
    if (!configure.claude.enable) {
        if (conversations.empty()) {
            bot->Add(convid);
            save(convid, true);
            conversations.emplace_back(convid);
        }
        convid = conversations[0];
    }
    bot->Load(convid);
    load(convid);
    whisper = true;
    mwhisper = true;

    // 遍历文件夹数组
    for (const auto&dir: dirs) {
        // 检查文件夹是否存在
        if (!UDirectory::Exists(dir)) {
            // 如果文件夹不存在，则输出警告信息
            LogWarn("Initialize Warning: Folder {0} does not exist!", dir);
            UDirectory::Create(dir);
        }
    }
    if (whisperData.enable) {
        if (whisperData.useLocalModel) {
            if (!CheckFileExistence(bin + WhisperPath, "Whisper", "main", true)) {
                state = State::NO_WHISPER;
                whisper = false;
                LogInfo(
                    "Initialize: It is detected that you do not have the \"Whisper Executable file\" and the \"useLocalModel\" option is enabled.");
                success = false;
            }
            if (!CheckFileExistence(whisperData.model, "Whisper Model")) {
                mwhisper = false;
                state = State::NO_WHISPER;
                LogInfo(
                    "Initialize: It is detected that you do not have the \"Whisper model\" and the \"useLocalModel\" option is enabled.");
                success = false;
            }
        }
    }
    if (vitsData.enable) {
        // 检查VITS可执行文件是否存在
        if (!CheckFileExistence(bin + VitsConvertor, "VITS", "VitsConvertor", true)) {
            state = State::NO_VITS;
            vits = false;
            LogInfo(
                "Initialize: It is detected that you do not have the \"VITS Executable file\" and the \"vits.enable\" option is enabled.");
            success = false;
        }

        // 检查VITS模型文件是否存在
        if (!CheckFileExistence(vitsData.model, "model file") ||
            !CheckFileExistence(vitsData.config, "Configuration file")) {
            vitsModel = false;
            success = false;
        }
    }

    if (live2D.enable) {
        if (!CheckFileExistence(live2D.bin, "Live2D executable file")) {
            LogWarn(
                "Initialize Warning: Since you don't have a \"Live2D Executable file\", the Live2D function isn't working properly!");
            live2D.enable = false;
            success = false;
        }
        if (!CheckFileExistence(live2D.model + "/" + Utils::GetFileName(live2D.model) + ".model3.json",
                                "Live2D model")) {
            live2D.enable = false;
            success = false;
        }
    }

    LogInfo("Successful initialization!");
    return success;
}

bool Application::is_valid_text(const std::string&text) const {
    if (text.empty()) return false;

    bool is_space = text.find_first_not_of(" \t\r\n") == std::string::npos;
    bool is_special = text.find_first_not_of(special_chars) == std::string::npos;

    if (is_space || is_special) return false;

    if (is_space && is_special) return false;

    return true;
}

int Application::countTokens(const string&str) {
    // 单一字符的token数量，包括回车
    const int singleCharTokenCount = 1;

    // 按照中文、英文、数字、特殊字符、emojis将字符串分割
    std::vector<std::string> tokens;
    std::string token;

    for (char c: str) {
        if ((c >= 0 && c <= 127) || (c >= -64 && c <= -33)) {
            // 遇到ASCII码或者中文字符的第一个部分，加入前一个token
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }

            // 加入当前字符所对应的token数量，根据字节数判断是否是中文字符
            if ((c >= 0 && c <= 127) || c == '\n') {
                tokens.push_back(std::string(singleCharTokenCount, c));
            }
            else {
                tokens.push_back(std::string(2, c));
            }
        }
        else if (c >= -128 && c <= -65) {
            // 中文字符第二个部分，加入前一个token
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }

            // 加入当前字符所对应的token数量
            token = std::string(2, c);
            tokens.push_back(token);
            token.clear();
        }
        else if ((c >= '0' && c <= '9')
                 || (c >= 'a' && c <= 'z')
                 || (c >= 'A' && c <= 'Z')) {
            // 数字或者字母的一段，加入前一个token
            if (token.empty()) {
                token += c;
            }
            else if (isdigit(c) == isdigit(token[0])) {
                token += c;
            }
            else {
                tokens.push_back(token);
                token = c;
            }
        }
        else {
            // 特殊字符或者emoji等，加入前一个token
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }

            // 根据不同类型的字符加入当前字符所对应的token数量
            if (c == '\n') {
                tokens.push_back(std::string(singleCharTokenCount, c));
            }
            else if (c == 0xF0) {
                tokens.push_back(std::string(6, ' '));
            }
            else if (c == '[' || c == ']') {
                tokens.push_back(std::string(singleCharTokenCount, c));
            }
            else if (c == ' ' || c == '\t' || c == '.' || c == ',' || c == ';' || c == ':' || c == '!' ||
                     c == '?' || c == '(' || c == ')' || c == '{' || c == '}' || c == '/' || c == '\\' ||
                     c == '+' || c == '-' || c == '*' || c == '=' || c == '<' || c == '>' || c == '|' ||
                     c == '&' || c == '^' || c == '%' || c == '$' || c == '#' || c == '@') {
                tokens.push_back(std::string(singleCharTokenCount, c));
            }
            else {
                tokens.push_back(std::string(singleCharTokenCount, c));
            }
        }
    }

    // 加入最后一个token
    if (!token.empty()) {
        tokens.push_back(token);
    }

    return tokens.size();
}

void Application::
ShowConfirmationDialog(const char* title, const char* content, bool&mstate,
                       ConfirmDelegate on_confirm,
                       const char* failure,
                       const char* success,
                       const char* confirm,
                       const char* cancel) {
    static bool Start = false;
    static bool reject = false;
    if (!reject) {
        ImGui::OpenPopup(title);
        if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", content);
            ImGui::Separator();

            if (ImGui::Button(confirm)) {
                mstate = true;
                if (on_confirm != nullptr) {
                    std::thread t(on_confirm);
                    t.detach();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(cancel)) {
                mstate = false;
                reject = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

void Application::RuntimeDetector() {
    if (!whisper) {
        ShowConfirmationDialog(reinterpret_cast<const char *>(u8"下载通知"),
                               reinterpret_cast<const char *>(u8"检测到你并没安装Whisper,是否安装?"), whisper,
                               [this]() { WhisperExeInstaller(); },
                               reinterpret_cast<const char *>(u8"安装Whisper失败"),
                               reinterpret_cast<const char *>(u8"安装Whisper成功"));
    }
    else if (!vits) {
        ShowConfirmationDialog(reinterpret_cast<const char *>(u8"下载通知"),
                               reinterpret_cast<const char *>(u8"检测到你并没安装VitsConvertor,是否安装?"), vits,
                               [this]() { VitsExeInstaller(); },
                               reinterpret_cast<const char *>(u8"安装VitsConvertor失败"),
                               reinterpret_cast<const char *>(u8"安装VitsConvertor成功"));
    }
    else if (!mwhisper) {
        ShowConfirmationDialog(reinterpret_cast<const char *>(u8"下载通知"),
                               reinterpret_cast<const char *>(u8"检测到你并没安装Whisper Model,是否下载?"),
                               mwhisper,
                               [this]() { WhisperModelDownload(Utils::GetFileName(whisperData.model)); },
                               reinterpret_cast<const char *>(u8"安装Whisper Model失败"),
                               reinterpret_cast<const char *>(u8"安装Whisper Model成功"));
    }
}

void Application::GetClaudeHistory() {
    thread([&]() {
        FirstTime = Utils::GetCurrentTimestamp();
        while (AppRunning) {
            Chat botR;
            botR.flag = 1;
            auto _history = bot->GetHistory();
            DeleteAllBotChat();
            for (auto&it: _history) {
                if (it.second != "Please note:") {
                    botR.timestamp = it.first;
                    botR.content = it.second;
                    AddChatRecord(botR);
                }
            }
            // 按时间戳排序
            std::sort(chat_history.begin(), chat_history.end(), compareByTimestamp);

            for (const auto&chat: chat_history) {
                if (chat.flag == 0) {
                    FirstTime = chat.timestamp;
                    break;
                }
            }

            // 删除早于FirstTime的对话
            chat_history.erase(
                std::remove_if(chat_history.begin(), chat_history.end(),
                               [&](const Chat&c) {
                                   return c.timestamp < FirstTime;
                               }),
                chat_history.end());
            save(convid, false);
            // 延迟0.002s
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }).detach();
}
