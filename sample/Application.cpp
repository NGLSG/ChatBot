#include "Application.h"

Application::Application(const OpenAIData &chat_data, const TranslateData &data, const VITSData &VitsData,
                         const WhisperData &WhisperData) {

    translator = CreateRef<Translate>(data);
    bot = CreateRef<ChatBot>(chat_data);
    voiceToText = CreateRef<VoiceToText>(chat_data);
    listener = CreateRef<Listener>(sampleRate, framesPerBuffer);
    vitsData = VitsData;
    whisperData = WhisperData;
    if (!Initialize()) {
        LogWarn("Warning: Initialization failed!Maybe some function can not working");
    }
    listener->listen();
}

Application::Application(const Configure &configure) {

    translator = CreateRef<Translate>(configure.baiDuTranslator);
    bot = CreateRef<ChatBot>(configure.openAi);
    voiceToText = CreateRef<VoiceToText>(configure.openAi);
    listener = CreateRef<Listener>(sampleRate, framesPerBuffer);
    vitsData = configure.vits;
    whisperData = configure.whisper;

    if (!Initialize()) {
        LogWarn("Warning: Initialization failed!Maybe some function can not working");
    }
    listener->listen();
}

void Application::TextMarkdown(const char *text) {
    const ImVec4 link_color = ImVec4(0.0f, 0.5f, 1.0f, 1.0f);
    const ImVec4 code_color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
    const ImVec4 bold_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 italic_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 strike_color = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);

    const char *p = text;
    while (*p) {
        if (*p == '[' && *(p + 1) != ']') {
            const char *q = strchr(p, ']');
            if (q && *(q + 1) == '(') {
                const char *r = strchr(q, ')');
                if (r) {
                    ImGui::TextColored(link_color, "%.*s", (int) (q - p - 1), p + 1);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(" ");
                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        char url[1024];
                        strncpy(url, q + 2, r - q - 2);
                        url[r - q - 2] = '\0';
                        // Open link in browser or handle it as appropriate for your application
                    }
                    p = r + 1;
                    continue;
                }
            }
        } else if (*p == '`') {
            const char *q = strchr(p + 1, '`');
            if (q) {
                ImGui::TextColored(code_color, "%.*s", (int) (q - p - 1), p + 1);
                p = q + 1;
                continue;
            }
        } else if (*p == '*' && *(p + 1) == '*') {
            const char *q = strstr(p + 2, "**");
            if (q) {
                ImGui::TextColored(bold_color, "%.*s", (int) (q - p - 2), p + 2);
                p = q + 2;
                continue;
            }
        } else if (*p == '_') {
            const char *q = strchr(p + 1, '_');
            if (q) {
                ImGui::TextColored(italic_color, "%.*s", (int) (q - p - 1), p + 1);
                p = q + 1;
                continue;
            }
        } else if (*p == '~' && *(p + 1) == '~') {
            const char *q = strstr(p + 2, "~~");
            if (q) {
                ImGui::TextColored(strike_color, "%.*s", (int) (q - p - 2), p + 2);
                p = q + 2;
                continue;
            }
        }

        ImGui::TextUnformatted(p, strchr(p, '\n'));
        p = strchr(p, '\n');
        if (p)
            p++;
    }
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
            listener->playRecorded();
            is_playing = false;
        }
    }).detach();

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
        if (ImGui::InputText("##input", buf, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
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
                ImGui::CloseCurrentPopup();
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
    // 设置样式
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

    int num = 0;
    // 显示聊天记录
    for (auto chat: chat_history) {

        std::string userAsk = chat.userAsk;
        std::string botAnswer = chat.botAnswer;
        ImVec2 text_size, text_pos;

        if (!userAsk.empty()) {
            // 显示用户的问题
            text_size = ImGui::CalcTextSize(userAsk.c_str());
            text_pos = ImVec2(ImGui::GetWindowContentRegionMax().x - text_size.x - 16, ImGui::GetCursorPosY());
            ImGui::SetCursorPosY(text_pos.y + ImGui::GetStyle().ItemSpacing.y);

            // 移动光标到下一行的起始位置
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMin().x);

            // 增加光标的y坐标，为按钮留出足够的空间
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y * 2);

            // 计算按钮的大小
            ImVec2 button_size = ImVec2(text_size.x + 16, ImGui::GetFontSize() * 2);

            // 修改按钮的颜色和边框样式
            ImVec4 text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            ImVec4 bg_color = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
            ImVec4 border_color = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
            float rounding = 4.0f;
            ImGui::PushStyleColor(ImGuiCol_Text, text_color);
            ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg_color);
            ImGui::PushStyleColor(ImGuiCol_Border, border_color);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);

            // 显示按钮
            ImGui::Button(userAsk.c_str(), button_size);
            // 恢复样式
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(5);
        }

        if (!botAnswer.empty()) {
            // 显示机器人的回答
            text_pos = ImVec2(ImGui::GetWindowContentRegionMin().x, ImGui::GetCursorPosY());
            ImGui::SetCursorPosY(text_pos.y + ImGui::GetStyle().ItemSpacing.y);

            // 设置多行输入框的最大高度和最大宽度
            float max_width = ImGui::GetWindowContentRegionWidth() * 0.8f;

            // 计算多行输入框的大小
            ImVec2 input_size = ImVec2(0, 0);
            ImVec2 text_size = ImGui::CalcTextSize(botAnswer.c_str());
            input_size.x = min(text_size.x, max_width) + 10;
            input_size.y = text_size.y + ImGui::GetStyle().ItemSpacing.y * 2;

            // 修改多行输入框的样式
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

            // 显示多行输入框
            ImGui::InputTextMultiline(("##" + botAnswer + std::to_string(num)).c_str(),
                                      const_cast<char *>(botAnswer.c_str()), botAnswer.size() + 1, input_size,
                                      ImGuiInputTextFlags_ReadOnly);

            // 恢复样式
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(8);
        }
        num++;
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
            if (ImGui::Button(reinterpret_cast<const char *>(u8"删除当前会话"), ImVec2(200, 30))) {
                bot->Del(convid);
                del(convid);
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

void Application::render_input_box() {
    static std::vector<std::shared_future<std::string>> submit_futures;
    static int num = 0;
    if (listener && listener->IsRecorded()) {

        std::thread([&]() {
            listener->EndListen();

            std::string path = "recorded" + std::to_string(num) + ".wav";
            num += 1;
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
                        listener->listen();

                    } else {
                        last_input = text;
                        LogInfo("User ask: {0}", last_input);
                        add_chat_record(last_input, "");
                        num -= 1;
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
            listener->changeFile("recorded" + std::to_string(num) + ".wav");
        }).detach();

    }

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

    ImGui::Begin("Input Box", NULL, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                                    ImGuiWindowFlags_NoTitleBar);

    strcpy_s(input_buffer, input_text.c_str());

    if (ImGui::InputTextMultiline("##Input Text", input_buffer, sizeof(input_buffer), ImVec2(-1, -1),
                                  ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CtrlEnterForNewLine |
                                  ImGuiInputTextFlags_EnterReturnsTrue)) {

        last_input = input_text = input_buffer;
        input_text.clear();
    }
    if ((strlen(last_input.c_str()) > 0 && ImGui::IsKeyPressed(ImGuiKey_Enter) &&
         !((ImGui::GetIO().KeyCtrl) || (ImGui::GetIO().KeyShift)))) { // 检查last_input是否为空

        LogInfo("User ask: {0}", last_input);
        add_chat_record(last_input, ""); // 使用last_input作为键

        std::shared_future<std::string> submit_future = std::async(std::launch::async, [&]() {
            return bot->Submit(last_input, role);
        }).share();
        submit_futures.push_back(std::move(submit_future));
        input_text.clear();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl) {
        input_text += "\n";
    }
    ImGui::End();

    // 处理已完成的submit_future
    for (auto it = submit_futures.begin(); it != submit_futures.end();) {
        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            std::string response = it->get();
            LogInfo("Bot answer: {0}", response);
            std::lock_guard<std::mutex> lock(chat_history_mutex);
            for (auto &record: chat_history) {
                if (record.userAsk == last_input && record.botAnswer.empty()) { // 查找匹配的 ChatRecord
                    record.botAnswer = response; // 更新 botAnswer 字段
                    break;
                }
            }
            it = submit_futures.erase(it);
            if (vits) {
                std::string VitsText = translator->translate(response, vitsData.lanType);
                Vits(VitsText);
            }
        } else {
            ++it;
        }
    }

    ImGui::PopStyleVar(9);
}

void Application::render_setting_box() {
    static Configure configure = Utils::LoadYaml<Configure>("config.yaml");
    static bool should_restart = false;

    // 设置 ImGui 样式
    ImGuiStyle &style = ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

    ImGui::Begin(reinterpret_cast<const char *>(u8"设置"), NULL,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);

    // 显示 OpenAI 配置
    if (ImGui::CollapsingHeader("OpenAI")) {
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用ChatGLM"), &configure.openAi.useLocalModel);
        if (!configure.openAi.useLocalModel) {
            ImGui::InputText(reinterpret_cast<const char *>(u8"OpenAI API Key"), configure.openAi.api_key.data(),
                             configure.openAi.api_key.size() + 1);
            ImGui::InputText(reinterpret_cast<const char *>(u8"OpenAI 模型"), configure.openAi.model.data(),
                             configure.openAi.model.size() + 1);
        } else {
            ImGui::InputText(reinterpret_cast<const char *>(u8"ChatGLM的位置"), configure.openAi.modelPath.data(),
                             configure.openAi.modelPath.size() + 1);
        }
        ImGui::InputText(reinterpret_cast<const char *>(u8"对OpenAI使用的代理"), configure.openAi.proxy.data(),
                         configure.openAi.proxy.size() + 1);
    }

    // 显示百度翻译配置
    if (ImGui::CollapsingHeader(reinterpret_cast<const char *>(u8"百度翻译"))) {
        ImGui::InputText("BaiDu App ID", configure.baiDuTranslator.appId.data(),
                         configure.baiDuTranslator.appId.size() + 1);
        ImGui::InputText("BaiDu API Key", configure.baiDuTranslator.APIKey.data(),
                         configure.baiDuTranslator.APIKey.size() + 1);
        ImGui::InputText(reinterpret_cast<const char *>(u8"对百度使用的代理"), configure.baiDuTranslator.proxy.data(),
                         configure.baiDuTranslator.proxy.size() + 1);
    }

    // 显示 VITS 配置
    if (ImGui::CollapsingHeader("VITS")) {
        ImGui::InputText(reinterpret_cast<const char *>(u8"Vits 模型位置"), configure.vits.model.data(),
                         configure.vits.model.size() + 1);
        ImGui::InputText(reinterpret_cast<const char *>(u8"Vits 配置文件位置"), configure.vits.config.data(),
                         configure.vits.config.size() + 1);
        ImGui::InputText(reinterpret_cast<const char *>(u8"Vits的语言类型"), configure.vits.lanType.data(),
                         configure.vits.lanType.size() + 1);
    }

    // 显示 Whisper 配置
    if (ImGui::CollapsingHeader("Whisper")) {
        ImGui::Checkbox(reinterpret_cast<const char *>(u8"使用本地模型"), &configure.whisper.useLocalModel);
        ImGui::InputText(reinterpret_cast<const char *>(u8"Whisper 模型位置"), configure.whisper.model.data(),
                         configure.whisper.model.size() + 1);
    }

    // 保存配置
    if (ImGui::Button(reinterpret_cast<const char *>(u8"保存配置"))) {
        Utils::SaveYaml("config.yaml", Utils::toYaml(configure));
        should_restart = true;
    }

    ImGui::End();
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
                Initialize();
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

    if (whisperData.useLocalModel) {
        if (!CheckFileExistence(bin + WhisperPath, "Whisper", "main", true)) {
            std::string yn;
            LogInfo("Initialize: It is detected that you do not have the \"Whisper Executable file\" and the \"useLocalModel\" option is enabled. Do you want to download the executable file?(y\\n)");
            std::cin >> yn;
            if (yn == "y") {
                if (!WhisperExeInstaller()) {
                    LogWarn("Initialize Waring: \"Executable file\" download failure!Please download \"Executable\" file by yourself");
                    LogWarn("Initialize Warning: Since you don't have a \"Executable file\", the voice conversation function isn't working properly!");
                    return false;
                }
            } else {
                LogWarn("Initialize Warning: Since you don't have a \"Whisper model\", the voice conversation function isn't working properly!");
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

    // 检查VITS可执行文件是否存在
    if (!CheckFileExistence(bin + VitsConvertor, "VITS", "VitsConvertor", true)) {
        vits = false;
        return false;
    }

    // 检查VITS模型文件是否存在
    if (!CheckFileExistence(vitsData.model, "model file") ||
        !CheckFileExistence(vitsData.config, "Configuration file")) {
        vits = false;
        return false;
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
    std::string result;

    result = Utils::exec(
            Utils::GetAbsolutePath(bin + WhisperPath) + "main -l auto -m " + whisperData.model +
            " -f ./recorded0.wav");

    if (is_valid_text(result)) {
        return "";
    }
    std::regex pattern(
            "\\[.*\\]\\s*(.*)\\n");
    std::sregex_token_iterator it(result.begin(), result.end(), pattern, 1);
    std::sregex_token_iterator end;

    return it->str();
}

bool Application::WhisperExeInstaller() {
    std::map<std::string, std::string> tasks = {
            {whisperUrl, bin + "Whisper.zip"}
    };
    return Installer(tasks) && Utils::Decompress(bin + "Whisper.zip");
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
    render_input_box();
    render_setting_box();
    render_popup_box();

    render_chat_box();
}

int Application::Renderer() {
    // 初始化GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 创建窗口
    GLFWwindow *window = glfwCreateWindow(800, 600, reinterpret_cast<const char *>(u8"倾听者"), NULL, NULL);
    glfwMakeContextCurrent(window);

    // 初始化OpenGL
    glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 初始化ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    ImFontAtlas *atlas = io.Fonts;

    ImFontConfig config;
    config.OversampleH = 1;
    config.OversampleV = 1;
    config.PixelSnapH = true;
    config.GlyphExtraSpacing = ImVec2(0.0f, 1.0f);
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2000, 0x206F, 0x3000, 0x30FF, 0x4E00, 0x9FFF, 0};
    ImFont *font = io.Fonts->AddFontFromFileTTF("Resources/font/default.otf", 16.0f, &config, ranges);
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
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);


    send_texture = load_texture("Resources/send.png");
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

    glDeleteTextures(1, &send_texture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

