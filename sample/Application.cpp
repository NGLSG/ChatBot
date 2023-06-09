#include "Application.h"


Application::Application(const OpenAIData &chat_data, const TranslateData &data, const VITSData &VitsData,
                         const WhisperData &WhisperData, const Live2D &Live2D, bool setting) {
    configure = Configure(chat_data, data, VitsData, whisperData);
    OnlySetting = setting;
    if (chat_data.api_key.empty()) {
        OnlySetting = true;
        state = State::NO_OPENAI_KEY;
    }
    translator = CreateRef<Translate>(data);
    bot = CreateRef<ChatBot>(chat_data);
    billing = bot->GetBilling();
    voiceToText = CreateRef<VoiceToText>(chat_data);
    listener = CreateRef<Listener>(sampleRate, framesPerBuffer);
    vitsData = VitsData;
    whisperData = WhisperData;
    live2D = Live2D;
    if (!Initialize()) {
        LogWarn("Warning: Initialization failed!Maybe some function can not working");
    }
    if (live2D.enable) {
        Utils::SaveFile(live2D.model, "Lconfig.txt");
        Utils::OpenProgram(live2D.bin.c_str());
    }
    if (whisperData.enable)
        listener->listen();
}

Application::Application(const Configure &configure, bool setting) {
    this->configure = configure;
    OnlySetting = setting;
    if (configure.openAi.api_key.empty() || configure.openAi.api_key == "") {
        OnlySetting = true;
        state = State::NO_OPENAI_KEY;
    }
    translator = CreateRef<Translate>(configure.baiDuTranslator);
    bot = CreateRef<ChatBot>(configure.openAi);
    billing = bot->GetBilling();
    voiceToText = CreateRef<VoiceToText>(configure.openAi);
    listener = CreateRef<Listener>(sampleRate, framesPerBuffer);
    vitsData = configure.vits;
    whisperData = configure.whisper;
    live2D = configure.live2D;
    if (!Initialize()) {
        LogWarn("Warning: Initialization failed!Maybe some function can not working");
    }
    if (live2D.enable) {

        Utils::SaveFile(live2D.model, "Lconfig.txt");
        Utils::OpenProgram(live2D.bin.c_str());
    }
    if (whisperData.enable)
        listener->listen();
}

GLuint Application::load_texture(const char *path) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path, &width, &height, &channels, 0);
    if (data == NULL) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return texture;
}

void Application::Vits(std::string text) {
    VitsTask task;
    task.model = Utils::GetAbsolutePath(vitsData.model);
    task.config = Utils::GetAbsolutePath(vitsData.config);
    task.text = text;
    static bool is_playing = false;
    Utils::SaveYaml("task.yaml", Utils::toYaml(task));

    std::thread([&]() {
        std::string result = Utils::exec(Utils::GetAbsolutePath(bin + VitsConvertor + "VitsConvertor" + exeSuffix));

        if (result.find("Successfully saved!")) {
            // 等待音频完成后再播放下一个
            while (is_playing) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            is_playing = true;
            listener->playRecorded(whisperData.enable);
            is_playing = false;
        }
    }).detach();
}

void Application::render_code_box() {
    ImGuiStyle &style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
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

    ImGui::Begin("Code Box", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoTitleBar);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    static bool show_codes = false;
    for (auto &it: codes) {

        show_codes = ImGui::CollapsingHeader(it.first.c_str());
        if (show_codes) {
            for (const auto &code: it.second) {
                if (!is_valid_text(code)) {
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

void Application::render_popup_box() {
    // 定制样式
    ImVec4 button_color(0.8f, 0.8f, 0.8f, 1.0f); // 将按钮颜色设置为浅白色
    ImVec4 button_hovered_color(0.15f, 0.45f, 0.65f, 1.0f);
    ImGuiStyle &style = ImGui::GetStyle();
    style.Colors[ImGuiCol_PopupBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.WindowRounding = 5.0f;
    style.Colors[ImGuiCol_Button] = button_color;
    style.Colors[ImGuiCol_ButtonHovered] = button_hovered_color;
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
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
            if (!is_valid_text(text)) {
                bot->Add(text);
                conversations.emplace_back(text);
                chat_history.clear();
                auto iter = std::find(conversations.begin(), conversations.end(), text);
                if (iter != conversations.end()) {
                    select_id = std::distance(conversations.begin(), iter);
                }
                convid = conversations[select_id];
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
    // 恢复默认样式
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.27f, 0.27f, 0.27f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.0f);
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.WindowRounding = 9.0f;
}

void Application::render_chat_box() {
    ImGuiStyle &style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
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
    // 创建一个子窗口
    ImGui::Begin("Chat", NULL, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoTitleBar);

    ImGui::BeginChild("Chat Box Content", ImVec2(0, -ImGui::GetTextLineHeightWithSpacing()), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoSavedSettings);

    // 显示聊天记录
    for (auto &chat: chat_history) {
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
            float max_width = ImGui::GetWindowContentRegionWidth() * 0.9f;
            input_size.x = min(text_size.x, max_width) + 10;
            input_size.y = text_size.y + ImGui::GetStyle().ItemSpacing.y * 2;
            // Set the style of the input field
            float rounding = 4.0f;
            ImVec4 bg_color = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
            ImVec4 text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            ImVec4 cursor_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
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

            // Display the input field with the text and automatic line breaks
            ImGui::InputTextMultiline(("##" + std::to_string(userAsk.timestamp)).c_str(),
                                      const_cast<char *>(userAsk.content.c_str()), userAsk.content.size() + 1,
                                      input_size,
                                      ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_CtrlEnterForNewLine);

            // Display the timestamp below the chat record
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), Utils::Stamp2Time(userAsk.timestamp).c_str());

            // Restore the style of the input field
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(8);
        }
        if (!botAnswer.content.empty()) {
            // Display the bot's answer
            text_pos = ImVec2(ImGui::GetWindowContentRegionMin().x, ImGui::GetCursorPosY());
            ImGui::SetCursorPosY(text_pos.y + ImGui::GetStyle().ItemSpacing.y);

            // Set the maximum height and width of the multi-line input field
            float max_width = ImGui::GetWindowContentRegionWidth() * 0.9f;

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


            // Display the multi-line input field
            ImGui::InputTextMultiline(("##" + to_string(botAnswer.timestamp)).c_str(),
                                      const_cast<char *>(botAnswer.content.c_str()), botAnswer.content.size() + 1,
                                      input_size,
                                      ImGuiInputTextFlags_ReadOnly);


            // Display the timestamp below the chat record
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
            ImGuiStyle &item_style = ImGui::GetStyle();
            item_style.Colors[ImGuiCol_Text] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
            for (int i = 0; i < conversations.size(); i++) {
                const bool is_selected = (select_id == i);
                if (ImGui::MenuItem(reinterpret_cast<const char *>(conversations[i].c_str()), nullptr, is_selected)) {
                    if (select_id != i) {
                        select_id = i;
                        convid = conversations[select_id];
                        bot->Load(convid);
                        load(convid);
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(reinterpret_cast<const char *>(u8"模式"))) {
            ImGuiStyle &item_style = ImGui::GetStyle();
            item_style.Colors[ImGuiCol_Text] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
            for (int i = 0; i < roles.size(); i++) {
                const bool is_selected = (role_id == i);
                if (ImGui::MenuItem(reinterpret_cast<const char *>(roles[i].c_str()), nullptr, is_selected)) {
                    if (role_id != i) {
                        role_id = i;
                        role = roles[role_id];
                    }
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::End();
}

//Optimized code
void Application::render_input_box() {
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
                } else {
                    conversion_future = std::async(std::launch::async, [&]() {
                        return WhisperConvertor(path);
                    });
                }
                // 在异步任务完成后执行回调函数
                auto callback = [&](std::future<std::string> &future) {
                    std::string text = future.get();

                    if (is_valid_text(text)) {
                        // 显示错误消息
                        LogWarn("Bot Warning: Your question is empty");
                        if (whisperData.enable)
                            listener->listen();
                    } else {
                        last_input = text;
                        Chat user;
                        user.timestamp = Utils::getCurrentTimestamp();
                        user.content = last_input;
                        add_chat_record(user);

                        Rnum -= 1;

                        std::remove(("recorded" + std::to_string(Rnum) + ".wav").c_str());
                        LogInfo("whisper: 删除录音{0}", "recorded" + std::to_string(Rnum) + ".wav");
                        std::shared_future<std::string> submit_future = std::async(std::launch::async, [&](void) {
                            return bot->Submit(last_input, role);
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
        ImGuiStyle &style = ImGui::GetStyle();
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0);

        ImGui::Begin("Input filed", NULL,
                     ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoTitleBar);
        ImGui::BeginGroup();
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
        ImGui::EndGroup();

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

    if ((strlen(last_input.c_str()) > 0 && ImGui::IsKeyPressed(ImGuiKey_Enter) && !ImGui::GetIO().KeyCtrl)) {
        Chat user;
        user.timestamp = Utils::getCurrentTimestamp();
        user.content = last_input;
        add_chat_record(user); // 使用last_input作为键
        if (whisperData.enable)
            listener->ResetRecorded();
        std::shared_future<std::string> submit_future = std::async(std::launch::async, [&]() {
            return bot->Submit(last_input, role);
        }).share();
        submit_futures.push_back(std::move(submit_future));
        input_text.clear();
    } else if ((ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl) ||
               (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyShift)) {
        input_text += "\n";
    }
    ImGui::End();

    // 处理已完成的submit_future
    for (auto it = submit_futures.begin(); it != submit_futures.end();) {
        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            std::string response = it->get();
            std::lock_guard<std::mutex> lock(chat_history_mutex);
            Chat bot;
            bot.flag = 1;
            bot.timestamp = Utils::getCurrentTimestamp();
            bot.content = response;
            add_chat_record(bot);
            it = submit_futures.erase(it);
            auto tmp_code = Utils::GetAllCodesFromText(response);
            for (auto &i: tmp_code) {
                std::size_t pos = i.find('\n'); // 查找第一个换行符的位置
                std::string codetype;
                if (pos != std::string::npos) { // 如果找到了换行符
                    codetype = i.substr(0, pos);
                    i = i.substr(pos + 1); // 删除第一行
                }
                if (is_valid_text(codetype))
                    codetype = "Unknown";
                if (codes.contains(codetype)) {
                    codes[codetype].emplace_back(i);
                } else {
                    codes.insert({codetype, {i}});
                }
            }
            if (vits && vitsData.enable) {
                std::string VitsText = translator->translate(Utils::ExtractNormalText(response), vitsData.lanType);
                Vits(VitsText);
            } else if (whisperData.enable) {
                listener->listen();
            }
        } else {
            ++it;
        }
    }
    ImGui::PopStyleVar(9);
}

void Application::render_setting_box() {
    static bool no_key = false;
    static bool first = false;
    switch (state) {
        case State::NO_OPENAI_KEY:
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

    ImGui::Begin(reinterpret_cast<const char *>(u8"设置"), NULL,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);

    // 显示 OpenAI 配置
    if (ImGui::CollapsingHeader("OpenAI")) {
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用ChatGLM"), &configure.openAi.useLocalModel);

        if (!configure.openAi.useLocalModel) {
            static bool showPassword = false, clicked = false;
            static double lastInputTime = 0.0;
            double currentTime = ImGui::GetTime();
            strcpy_s(api_buffer, configure.openAi.api_key.c_str());
            if (currentTime - lastInputTime > 0.5) {
                showPassword = false;
            }
            if (showPassword || clicked) {
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"OpenAI API Key"),
                                     api_buffer,
                                     sizeof(input_buffer))) {
                    configure.openAi.api_key = api_buffer;
                }

            } else {
                if (ImGui::InputText(reinterpret_cast<const char *>(u8"OpenAI API Key"),
                                     api_buffer,
                                     sizeof(input_buffer),
                                     ImGuiInputTextFlags_Password)) {
                    configure.openAi.api_key = api_buffer;
                    showPassword = true;
                    lastInputTime = ImGui::GetTime();
                }
            }
            ImGui::SameLine();
            if (ImGui::ImageButton(reinterpret_cast<void *>(eye_texture),
                                   ImVec2(16, 16),
                                   ImVec2(0, 0),
                                   ImVec2(1, -1),
                                   -1,
                                   ImVec4(0, 0, 0, 0),
                                   ImVec4(1, 1, 1, 1))) {
                clicked = !clicked;
            }
            ImGui::InputText(reinterpret_cast<const char *>(u8"OpenAI 模型"), configure.openAi.model.data(),
                             TEXT_BUFFER);
            ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用远程代理"), &configure.openAi.useWebProxy);
            if (!configure.openAi.useWebProxy)
                ImGui::InputText(reinterpret_cast<const char *>(u8"对OpenAI使用的代理"), configure.openAi.proxy.data(),
                                 TEXT_BUFFER);
            else {
                if (ImGui::BeginCombo(reinterpret_cast<const char *>(u8"远程代理"),
                                      Utils::GetFileName(proxies[configure.openAi.webproxy]).c_str())) // 开始下拉列表
                {
                    for (int i = 0; i < proxies.size(); i++) {
                        bool is_selected = (configure.openAi.webproxy == i);
                        if (ImGui::Selectable(Utils::GetFileName(proxies[i]).c_str(), is_selected)) {
                            configure.openAi.webproxy = i;
                        }
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus(); // 默认选中项
                        }
                    }
                    ImGui::EndCombo(); // 结束下拉列表
                }
            }
        } else {
            ImGui::InputText(reinterpret_cast<const char *>(u8"ChatGLM的位置"),
                             configure.openAi.modelPath.data(),
                             TEXT_BUFFER);
        }
    }

    // 显示百度翻译配置
    if (ImGui::CollapsingHeader(reinterpret_cast<const char *>(u8"百度翻译"))) {
        ImGui::InputText("BaiDu App ID", configure.baiDuTranslator.appId.data(),
                         TEXT_BUFFER);
        static bool showbaiduPassword = false, baiduclicked = false;
        static double baidulastInputTime = 0.0;

        double baiducurrentTime = ImGui::GetTime();
        strcpy_s(Bapi_buffer, configure.baiDuTranslator.APIKey.c_str());
        if (baiducurrentTime - baidulastInputTime > 0.5) {
            showbaiduPassword = false;
        }
        if (showbaiduPassword || baiduclicked) {
            if (ImGui::InputText("BaiDu API Key", Bapi_buffer,
                                 sizeof(Bapi_buffer))) {
                configure.baiDuTranslator.APIKey = Bapi_buffer;
            }
        } else {
            if (ImGui::InputText("BaiDu API Key", Bapi_buffer,
                                 sizeof(Bapi_buffer), ImGuiInputTextFlags_Password)) {
                configure.baiDuTranslator.APIKey = Bapi_buffer;
                showbaiduPassword = true;
                baidulastInputTime = ImGui::GetTime();
            }
        }
        ImGui::SameLine();
        if (ImGui::ImageButton(reinterpret_cast<void *>(eye_texture),
                               ImVec2(16, 16),
                               ImVec2(0, 0),
                               ImVec2(1, -1),
                               1,
                               ImVec4(0, 0, 0, 0),
                               ImVec4(1, 1, 1, 1))) {
            baiduclicked = !baiduclicked;
        }

        ImGui::InputText(reinterpret_cast<const char *>(u8"对百度使用的代理"),
                         configure.baiDuTranslator.proxy.data(),
                         TEXT_BUFFER);
    }

    // 显示 VITS 配置
    if (ImGui::CollapsingHeader("VITS")) {
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"启用Vits"), &configure.vits.enable);
        ImGui::InputText(reinterpret_cast<const char *>(u8"Vits 模型位置"), configure.vits.model.data(), TEXT_BUFFER);
        ImGui::InputText(reinterpret_cast<const char *>(u8"Vits 配置文件位置"), configure.vits.config.data(),
                         TEXT_BUFFER);
        ImGui::InputText(reinterpret_cast<const char *>(u8"Vits的语言类型"), configure.vits.lanType.data(),
                         TEXT_BUFFER);
    }

#ifdef WIN32
    if (ImGui::CollapsingHeader("Live2D")) {
        mdirs = Utils::GetDirectories(model + Live2DPath);
        auto it = std::find(mdirs.begin(), mdirs.end(), configure.live2D.model);
        if (it != mdirs.end() && mdirs.size() > 1) {
            selected_dir = std::distance(mdirs.begin(), it);
        }
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"启用Live2D"), &configure.live2D.enable);
        ImGui::InputText(reinterpret_cast<const char *>(u8"Live2D 可执行文件"), configure.live2D.bin.data(),
                         TEXT_BUFFER);

        if (ImGui::BeginCombo(reinterpret_cast<const char *>(u8"Live2D 模型"),
                              Utils::GetFileName(mdirs[selected_dir]).c_str())) // 开始下拉列表
        {
            for (int i = 0; i < mdirs.size(); i++) {
                bool is_selected = (selected_dir == i);
                if (ImGui::Selectable(Utils::GetFileName(mdirs[i]).c_str(), is_selected)) {
                    selected_dir = i;
                    if (select_id = 0) {
                        configure.live2D.enable = false;
                    } else {
                        configure.live2D.enable = true;
                    }
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus(); // 默认选中项
                }
            }
            configure.live2D.model = mdirs[selected_dir].c_str();
            ImGui::EndCombo(); // 结束下拉列表
        }

    }
#endif
    // 显示 Whisper 配置
    if (ImGui::CollapsingHeader("Whisper")) {
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"启用Whisper"), &configure.whisper.enable);
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用本地模型"), &configure.whisper.useLocalModel);
        ImGui::InputText(reinterpret_cast<const char *>(u8"Whisper 模型位置"), configure.whisper.model.data(),
                         TEXT_BUFFER);
    }

    // 保存配置
    if (ImGui::Button(reinterpret_cast<const char *>(u8"保存配置"))) {
        Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
        should_restart = true;
    }

    ImGui::End();
    {
        if (should_restart) {
            ImGui::OpenPopup(reinterpret_cast<const char *>(u8"需要重启应用"));
            should_restart = false;
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char *>(u8"需要重启应用"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(reinterpret_cast<const char *>(u8"您需要重新启动应用程序以使更改生效。"));
            if (ImGui::Button(reinterpret_cast<const char *>(u8"确定"), ImVec2(120, 0))) {
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
            ImGui::OpenPopup(reinterpret_cast<const char *>(u8"需要配置OpenAI Key"));
            no_key = false;
        }

        if (ImGui::BeginPopupModal(reinterpret_cast<const char *>(u8"需要配置OpenAI Key"), NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(reinterpret_cast<const char *>(u8"您需要填写OpenAI API key 以启用本程序。"));
            if (ImGui::Button(reinterpret_cast<const char *>(u8"确定"), ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

bool Application::Initialize() {
    std::filesystem::path path_to_directory = "Conversations/";
    for (const auto &entry: std::filesystem::directory_iterator(path_to_directory)) {
        if (entry.is_regular_file()) {
            const auto &file_path = entry.path();
            std::string file_name = file_path.stem().string();
            if (!file_name.empty()) {
                conversations.emplace_back(file_name);
            }
            if (!conversations.size() > 0) {
                bot->Add(convid);
                conversations.emplace_back(convid);
            }
        }
    }
    convid = conversations[0];

    bot->Load(convid);
    load(convid);

    // 遍历文件夹数组
    for (const auto &dir: dirs) {
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
                std::string yn;
                LogInfo("Initialize: It is detected that you do not have the \"Whisper Executable file\" and the \"useLocalModel\" option is enabled. Do you want to download the executable file?(y\\n)");
                std::cin >> yn;
                if (yn == "y") {
                    if (!WhisperExeInstaller()) {
                        LogWarn("Initialize Waring: \"Executable file\" download failure!Please download {0} \"Executable\" file by yourself",
                                VitsConvertUrl);
                        LogWarn("Initialize Warning: Since you don't have a \"Executable file\", the voice conversation function isn't working properly!");
                        return false;
                    }
                } else {
                    LogWarn("Initialize Warning: Since you don't have a \"Whisper Executable file\", the voice conversation function isn't working properly!");
                    return false;
                }
            }
            if (!CheckFileExistence(whisperData.model, "Whisper")) {
                std::string yn;
                LogInfo("Initialize: It is detected that you do not have the \"Whisper model\" and the \"useLocalModel\" option is enabled. Do you want to download the model?(y\\n)");
                std::cin >> yn;
                if (yn == "y") {
                    if (!WhisperModelDownload(Utils::GetFileName(whisperData.model))) {
                        LogWarn("Initialize Warning: Model download failure!Please download model by yourself");
                        LogWarn("Initialize Warning: Since you don't have a \"Whisper model\", the voice conversation function isn't working properly!");
                        return false;
                    }
                } else {
                    LogWarn("Initialize Warning: Since you don't have a \"Whisper model\", the voice conversation function isn't working properly!");
                    return false;
                }
            }
        }
    }

    if (vitsData.enable) {
        // 检查VITS可执行文件是否存在
        if (!CheckFileExistence(bin + VitsConvertor, "VITS", "VitsConvertor", true)) {
            std::string yn;
            LogInfo("Initialize: It is detected that you do not have the \"VITS Executable file\" and the \"vits.enable\" option is enabled. Do you want to download the executable file?(y\\n)");
            std::cin >> yn;
            if (yn == "y") {
                if (!VitsExeInstaller()) {
                    LogWarn("Initialize Waring: \"Executable file\" download failure!Please download \"Executable\" file by yourself");
                    LogWarn("Initialize Warning: Since you don't have a \"Executable file\", the voice conversation function isn't working properly!");
                    vits = false;
                    return false;
                }
            } else {
                LogWarn("Initialize Warning: Since you don't have a \"Vits Executable file\", the voice conversation function isn't working properly!");
                vits = false;
                return false;
            }
            return false;
        }

        // 检查VITS模型文件是否存在
        if (!CheckFileExistence(vitsData.model, "model file") ||
            !CheckFileExistence(vitsData.config, "Configuration file")) {
            vits = false;
            return false;
        }
    }

    if (live2D.enable) {
        if (!CheckFileExistence(live2D.bin, "Live2D executable file")) {
            LogWarn("Initialize Warning: Since you don't have a \"Live2D Executable file\", the Live2D function isn't working properly!");
            live2D.enable = false;
            return false;
        }
        if (!CheckFileExistence(live2D.model + "/" + Utils::GetFileName(live2D.model) + ".model3.json",
                                "Live2D model")) {
            live2D.enable = false;
            return false;
        }

    }

    LogInfo("Successful initialization!");
    return true;
}

bool Application::CheckFileExistence(const std::string &filePath, const std::string &fileType,
                                     const std::string &executableFile, bool isExecutable) {
    if (isExecutable) {
        if (!UFile::Exists(filePath + executableFile + exeSuffix)) {
            LogWarn("Warning: {0} won't work because you don't have an executable \"{1}\" for {0} !",
                    fileType, filePath + executableFile + exeSuffix);
            LogWarn("Warning: Please download or compile it as an executable file and put it in this folder: {0} !",
                    filePath);
            return false;
        }
    } else {
        if (!UFile::Exists(filePath)) {
            LogWarn("Warning: {0} won't work because you don't have an file \"{1}\" for {0} !",
                    fileType, filePath);
            return false;
        }
    }

    return true;
}

bool Application::WhisperModelDownload(const std::string &model) {
    std::map<std::string, std::string> tasks = {
            {"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/" + model,
             this->model + WhisperPath + model}
    };
    return Installer(tasks);
}

std::string Application::WhisperConvertor(const std::string &file) {
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

bool Application::WhisperExeInstaller() {
    std::map<std::string, std::string> tasks = {
            {whisperUrl, bin + "Whisper.zip"}
    };
    return Installer(tasks) && Utils::Decompress(bin + "Whisper.zip");
}

bool Application::VitsExeInstaller() {
    std::map<std::string, std::string> tasks = {
            {VitsConvertUrl, bin + vitsFile}
    };
    return Installer(tasks) && Utils::Decompress(bin + vitsFile);
}

bool Application::Installer(std::map<std::string, std::string> tasks) {
    bool success = Utils::UDownloads(tasks, 16);
    if (success) {
        LogInfo("Application: Download completed successfully.");
        return true;
    } else {
        LogError("Application Error: Download failed.");
        return false;
    }
}

void Application::render_ui() {
    ImGui::DockSpaceOverViewport();
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!OnlySetting) {
        render_input_box();
        render_popup_box();
        render_setting_box();
        render_chat_box();
        render_code_box();
    } else {
        render_setting_box();
    }
}

int Application::Renderer() {
    // 初始化GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 创建窗口
    GLFWwindow *window = glfwCreateWindow(800, 600, VERSION.c_str(), NULL, NULL);
    glfwMakeContextCurrent(window);

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
    ImGuiIO &io = ImGui::GetIO();

    ImFontConfig config;
    config.OversampleH = 1;
    config.OversampleV = 1;
    config.PixelSnapH = true;
    config.GlyphExtraSpacing = ImVec2(0.0f, 1.0f);
    ImFont *font = io.Fonts->AddFontFromFileTTF("Resources/font/default.otf", 16.0f, &config,
                                                io.Fonts->GetGlyphRangesChineseFull());
    IM_ASSERT(font != NULL);
    io.DisplayFramebufferScale = ImVec2(0.8f, 0.8f);
    (void) io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // 启用Docking
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // 设置 ImGui 全局样式属性

    ImGuiStyle &style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
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

    //send_texture = load_texture("Resources/send.png");
    eye_texture = load_texture("Resources/eye.png");
    // 渲染循环
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        render_ui();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // 清理资源

    listener.reset();
    voiceToText.reset();
    bot.reset();
    translator.reset();

    stbi_image_free(images[0].pixels);
    glDeleteTextures(1, &send_texture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

void Application::del(std::string name) {
    if (remove((Conversation + name + ".yaml").c_str()) != 0) {
        LogError("OpenAI Error: Unable to delete session {0},{1}", name, ".");
    }
    conversations.erase(conversations.begin() + select_id);
    select_id = conversations.size() - 1;
    convid = conversations[select_id];

    LogInfo("Bot : 删除 {0} 成功", name);
}

vector <Application::Chat> Application::load(std::string name) {
    if (UFile::Exists(Conversation + name + ".yaml")) {
        std::ifstream session_file(Conversation + name + ".yaml");
        session_file.imbue(std::locale("en_US.UTF-8"));
        chat_history.clear();
        if (session_file.is_open()) {
            // 从文件中读取 YAML 节点
            YAML::Node node = YAML::Load(session_file);

            // 将 YAML 节点映射到 chat_history
            for (const auto &record_node: node) {
                // 从 YAML 映射节点中读取 ChatRecord 对象的成员变量
                int flag = record_node["flag"].as<int>();
                long long timestamp;
                std::string content;
                Chat record;
                if (flag == 0) {
                    timestamp = record_node["user"]["timestamp"].as<long long>();
                    content = record_node["user"]["content"].as<std::string>();
                } else {
                    timestamp = record_node["bot"]["timestamp"].as<long long>();
                    content = record_node["bot"]["content"].as<std::string>();
                }

                record.flag = flag;
                record.content = content;
                record.timestamp = timestamp;

                // 创建一个新的 ChatRecord 对象，并将其添加到 chat_history 中
                chat_history.push_back(record);
            }

            session_file.close();
            LogInfo("Application : Load {0} successfully", name);
        } else {
            LogError("Application Error: Unable to load session {0},{1}", name, ".");
        }
    }

    return chat_history;
}

void Application::save(std::string name) {
    std::ofstream session_file(Conversation + name + ".yaml");
    session_file.imbue(std::locale("en_US.UTF-8"));
    if (session_file.is_open()) {
        // 创建 YAML 节点
        YAML::Node node;

        // 将 chat_history 映射到 YAML 节点
        for (const auto &record: chat_history) {
            // 创建一个新的 YAML 映射节点
            YAML::Node record_node(YAML::NodeType::Map);

            // 将 ChatRecord 对象的成员变量映射到 YAML 映射节点中
            record_node["flag"] = record.flag;
            if (record.flag == 0) {
                record_node["user"]["timestamp"] = record.timestamp;
                record_node["user"]["content"] = record.content;
            } else {
                record_node["bot"]["timestamp"] = record.timestamp;
                record_node["bot"]["content"] = record.content;
            }

            // 将 YAML 映射节点添加到主节点中
            node.push_back(record_node);
        }

        // 将 YAML 节点写入文件
        session_file << node;

        session_file.close();
        LogInfo("Application : Save {0} successfully", name);
    } else {
        LogError("Application Error: Unable to save session {0},{1}", name, ".");
    }
}
