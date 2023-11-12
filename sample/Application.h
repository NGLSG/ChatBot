#ifndef APP_H
#define APP_H
#define SOL_ALL_SAFETIES_ON 1

#include <iostream>
#include <string>
#include <vector>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <regex>
#include "stb_image_write.h"
#include "stb_image.h"
#include "ChatBot.h"
#include "VoiceToText.h"
#include "Translate.h"
#include "Recorder.h"
#include "Configure.h"
#include "utils.h"
#include "StableDiffusion.h"
#include "imfilebrowser.h"
#include "sol/sol.hpp"

#define TEXT_BUFFER 4096
const std::string VERSION = reinterpret_cast<const char *>(u8"CyberGirl v1.3");
extern std::vector<std::string> scommands;
extern bool cpshow;
// 定义一个委托类型，它接受一个空参数列表，返回类型为 void
typedef std::function<void()> ConfirmDelegate;

enum State {
    OK = 0,
    NO_OPENAI_KEY,
    NO_VITS,
    NO_TRANSLATOR,
    NO_WHISPER,
    NO_WHISPERMODEL,
    FIRST_USE,
};


class Application {
private:
#ifdef _WIN32
    const std::string VitsConvertUrl = "https://github.com/NGLSG/MoeGoe/releases/download/1.1/VitsConvertor-win64.tar.gz";
    const std::string whisperUrl = "https://github.com/ggerganov/whisper.cpp/releases/download/v1.3.0/whisper-bin-Win32.zip";
    const std::string vitsFile = "VitsConvertor.tar.gz";
    const std::string exeSuffix = ".exe";
#else
    const std::string whisperUrl = "https://github.com/ggerganov/whisperData.cpp/releases/download/v1.3.0/whisper-bin-x64.zip";
    const std::string VitsConvertUrl = "https://github.com/NGLSG/MoeGoe/releases/download/1.1/VitsConvertor-linux64.tar.xz";
    const std::string vitsFile = "VitsConvertor.tar.gz";
    const std::string exeSuffix = "";
#endif

    struct Chat {
        int flag = 0;//0=user;1=bot
        long long timestamp;
        std::string content;
        std::string image;
    };

    Ref<Translate> translator;
    Ref<ChatBot> bot;
    Ref<VoiceToText> voiceToText;
    Ref<Listener> listener;
    Ref<StableDiffusion> stableDiffusion;
    WhisperData whisperData;
    VITSData vitsData;
    Live2D live2D;

    std::vector<std::string> conversations;
    std::vector<Chat> chat_history;

    // 存储当前输入的文本
    std::string input_text;
    std::string last_input;
    std::string selected_text = "";
    std::string convid = "default";
    std::string role = "user";


    std::mutex submit_futures_mutex;
    std::mutex chat_history_mutex;
    std::mutex chat_mutex;


    State state = State::OK;
    Configure configure;
    LConfigure lConfigure;
    int Rnum = 0;
    int token;

    char input_buffer[4096 * 32];
    char api_buffer[4096];
    char Bapi_buffer[4096];
    const std::string bin = "bin/";
    const std::string VitsConvertor = "VitsConvertor/";
    const std::string model = "model/";
    const std::string Resources = "Resources/";
    const std::string VitsPath = "Vits/";
    const std::string WhisperPath = "Whisper/";
    const std::string ChatGLMPath = "ChatGLM/";
    const std::string Live2DPath = "Live2D/";
    const std::string PluginPath = "Plugins/";
    const std::string Conversation = "Conversations/ChatHistory/";
    const std::string special_chars = R"(~!@#$%^&*()_+`-={}|[]\:";'<>?,./)";

    const int sampleRate = 16000;
    const int framesPerBuffer = 512;

    const std::vector<std::string> dirs = {PluginPath, Conversation, bin, bin + VitsConvertor, bin + WhisperPath,
                                           bin + ChatGLMPath, bin + Live2DPath, model, model + VitsPath,
                                           model + WhisperPath, model + Live2DPath,
                                           Resources};
    const std::vector<std::string> whisperModel = {"tiny", "base", "small", "medium", "large"};
    const std::vector<std::string> tmpWhisperModel = {"model/Whisper/ggml-tiny.bin", "model/Whisper/ggml-base.bin",
                                                      "model/Whisper/ggml-small.bin", "model/Whisper/ggml-medium.bin",
                                                      "model/Whisper/ggml-large.bin"};
    const std::vector<std::string> sampleIndices = {"Euler a", "Euler", "LMS", "Heun", "DPM2", "DPM2 a", "DPM++ 2S a",
                                                    "DPM++ 2M", "DPM++ SDE", "DPM fast", "DPM adaptive", "LMS Karras",
                                                    "DPM2 Karras", "DPM2 a Karras", "DPM++ 2S a Karras",
                                                    "DPM++ 2M Karras", "DPM++ SDE Karras", "DDIM", "PLMS", "UniPC"
    };
    const std::vector<std::string> roles = {"user", "system", "assistant"};
    const std::vector<std::string> proxies = {"Cloudflare", "Tencent Cloud"};
    const std::vector<std::string> commands = {"/draw", reinterpret_cast<const char *>(u8"/绘图"),
                                               "/help", reinterpret_cast<const char *>(u8"/帮助")};

    std::vector<std::string> live2dModel;
    std::vector<std::string> speakers = {reinterpret_cast<const char *>(u8"空空如也")};
    std::vector<std::string> vitsModels = {reinterpret_cast<const char *>(u8"空空如也")};
    std::map<std::string, std::vector<std::string>> codes;
    map <string, GLuint> TextureCache = {{"eye",     0},
                                         {"del",     0},
                                         {"message", 0},
                                         {"add",     0},
                                         {"send",    0},
                                         {"avatar",  0}};
    map<string, int> SelectIndices = {{"Live2D",          0},
                                      {"whisper",         0},
                                      {"vits",            0},
                                      {"conversation",    0},
                                      {"stableDiffusion", 0},
                                      {"role",            0}};
    map <string, GLuint> SDCache;

    bool vits = true;
    bool vitsModel = true;
    bool show_input_box = false;
    bool OnlySetting = false;
    bool whisper = false;
    bool mwhisper = false;
    bool AppRunning = true;
    long long FirstTime = 0;

    std::vector<Chat> load(std::string name = "default");

    std::vector<std::string> PluginsScript;

    void save(std::string name = "default", bool out = true);

    void del(std::string name = "default");

    void AddChatRecord(const Chat &data) {
        chat_history.push_back(data);
    }

    void DeleteAllBotChat() {
        chat_history.erase(
                std::remove_if(chat_history.begin(), chat_history.end(),
                               [](const Chat &c) {
                                   return c.flag == 1;
                               }),
                chat_history.end());

    }

    void VitsListener();

    void _Draw(Ref<std::string> prompt, long long ts, bool callFromBot) {
        if (is_valid_text(configure.stableDiffusion.apiPath)) {
            std::string uid = stableDiffusion->Text2Img(*prompt);

            if (callFromBot) {
                // 使用互斥锁保护共享资源 chat_history
                std::lock_guard<std::mutex> lock(chat_mutex);
                for (auto &it: chat_history) {
                    if (it.flag == 1) {
                        if (it.timestamp >= ts) {
                            it.image = uid;
                            break;
                        }
                    } else {
                        continue;
                    }
                }
            } else {
                Chat img;
                img.flag = 1;
                img.timestamp = Utils::GetCurrentTimestamp();
                img.content = "Finished!";
                img.image = uid;

                // 使用互斥锁保护共享资源 chat_history
                std::lock_guard<std::mutex> lock(chat_mutex);
                chat_history.emplace_back(img);
            }

        } else {
            Chat img;
            img.flag = 1;
            img.timestamp = Utils::GetCurrentTimestamp() + 10;
            img.content = reinterpret_cast<const char *>(u8"抱歉,我不能为您生成图片,因为您的api地址为空");
            std::lock_guard<std::mutex> lock(chat_mutex);
            chat_history.emplace_back(img);
        }
    }

    void Draw(const std::string &prompt, long long ts, bool callFromBot = false) {
        auto prompt_ref = CreateRef<std::string>(prompt);
        std::thread t([=] { _Draw(prompt_ref, ts, callFromBot); });
        t.detach();
    }

    bool ContainsCommand(std::string &str, std::string &cmd, std::string &args) {
        for (const auto &it: commands) {
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

    void InlineCommand(const std::string &cmd, const std::string &args, long long ts) {
        if (cmd == "draw" || cmd == reinterpret_cast<const char *>(u8"绘图")) {
            Draw(args, ts);
        } else if (cmd == "help" || reinterpret_cast<const char *>(u8"帮助")) {
            Chat help;
            help.flag = 1;
            help.timestamp = Utils::GetCurrentTimestamp() + 10;
            help.content = reinterpret_cast<const char *>(u8"内置命令:\n/draw或者/绘图 [prompt] 绘制一张图片\n/help或者/帮助 唤起帮助");
            chat_history.emplace_back(help);
        }
    }

    inline void UniversalStyle() {
        // 定义通用样式
        ImGuiStyle &style = ImGui::GetStyle();

        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);

        // popup 样式
        ImVec4 button_color(0.8f, 0.8f, 0.8f, 1.0f);
        ImVec4 button_hovered_color(0.15f, 0.45f, 0.65f, 1.0f);

        style.Colors[ImGuiCol_PopupBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        style.FrameRounding = 5.0f;
        style.GrabRounding = 5.0f;
        style.WindowRounding = 5.0f;
        style.Colors[ImGuiCol_Button] = button_color;
        style.Colors[ImGuiCol_ButtonHovered] = button_hovered_color;
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    }

    inline void RestoreDefaultStyle() {

        ImGuiStyle &style = ImGui::GetStyle();

        // 恢复默认样式
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.27f, 0.27f, 0.27f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.0f);
        style.FrameRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.WindowRounding = 0.0f;

        // 恢复默认颜色
        style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        // ......

    }

    // 渲染聊天框
    void RenderChatBox();

    // 渲染设置
    void RenderConfigBox();

    // 渲染输入框

    void RenderInputBox();

    //渲染弹窗
    void RenderPopupBox();

    void RenderCodeBox();

    void RenderConversationBox();

    // 渲染UI
    void RenderUI();

    void initImGuiBindings(sol::state_view lua) {


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
                [](const char *name) { return ImGui::Begin(name); },
                [](const char *name, bool *p_open) { return ImGui::Begin(name, p_open); },
                [](const char *name, bool *p_open, ImGuiWindowFlags flags) { return ImGui::Begin(name, p_open, flags); }
        ));
        lua.set_function("UIEnd", ImGui::End);

        // ImGui Text 函数的绑定
        lua.set_function("UIText", &ImGui::Text);

        lua.set_function("UItextUnformatted", sol::overload(
                [](const char *text) { ImGui::TextUnformatted(text); },
                [](const char *text, const char *text_end) { ImGui::TextUnformatted(text, text_end); }
        ));

        lua.set_function("UItextColored", &ImGui::TextColored);

        lua.set_function("UItextColoredV", &ImGui::TextColored);

        lua.set_function("UItextDisabled", &ImGui::TextDisabled);

        lua.set_function("UItextWrapped", &ImGui::TextWrapped);
        
        lua.set_function("UIlabelText", &ImGui::LabelText);

        lua.set_function("UIbulletText", &ImGui::BulletText);

        lua.set_function("UIseparatorText", &ImGui::SeparatorText);

        lua.set_function("UIDragFloat", sol::overload(
                [](const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
                },
                [](const char* label, float v[2], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::DragFloat2(label, v, v_speed, v_min, v_max, format, flags);
                },
                [](const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
                },
                [](const char* label, float v[4], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::DragFloat4(label, v, v_speed, v_min, v_max, format, flags);
                },
                [](const char* label, float* v_current_min, float* v_current_max, float v_speed, float v_min, float v_max, const char* format, const char* format_max, ImGuiSliderFlags flags) {
                    return ImGui::DragFloatRange2(label, v_current_min, v_current_max, v_speed, v_min, v_max, format, format_max, flags);
                }
        ));

        lua.set_function("UIDragInt", sol::overload(
                [](const char* label, int* v, float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);
                },
                [](const char* label, int v[2], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::DragInt2(label, v, v_speed, v_min, v_max, format, flags);
                },
                [](const char* label, int v[3], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::DragInt3(label, v, v_speed, v_min, v_max, format, flags);
                },
                [](const char* label, int v[4], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::DragInt4(label, v, v_speed, v_min, v_max, format, flags);
                },
                [](const char* label, int* v_current_min, int* v_current_max, float v_speed, int v_min, int v_max, const char* format, const char* format_max, ImGuiSliderFlags flags) {
                    return ImGui::DragIntRange2(label, v_current_min, v_current_max, v_speed, v_min, v_max, format, format_max, flags);
                }
        ));

        lua.set_function("UIDragScalar", sol::overload(
                [](const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::DragScalar(label, data_type, p_data, v_speed, p_min, p_max, format, flags);
                },
                [](const char* label, ImGuiDataType data_type, void* p_data, int components, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::DragScalarN(label, data_type, p_data, components, v_speed, p_min, p_max, format, flags);
                }
        ));

        lua.set_function("UISliderFloat", sol::overload(
                [](const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
                },
                [](const char* label, float v[2], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderFloat2(label, v, v_min, v_max, format, flags);
                },
                [](const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderFloat3(label, v, v_min, v_max, format, flags);
                },
                [](const char* label, float v[4], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderFloat4(label, v, v_min, v_max, format, flags);
                },
                [](const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderAngle(label, v, v_min, v_max, format, flags);
                },
                [](const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderInt(label, v, v_min, v_max, format, flags);
                },
                [](const char* label, int v[2], int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderInt2(label, v, v_min, v_max, format, flags);
                },
                [](const char* label, int v[3], int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderInt3(label, v, v_min, v_max, format, flags);
                },
                [](const char* label, int v[4], int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderInt4(label, v, v_min, v_max, format, flags);
                },
                [](const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderScalar(label, data_type, p_data, p_min, p_max, format, flags);
                },
                [](const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                    return ImGui::SliderScalarN(label, data_type, p_data, components, p_min, p_max, format, flags);
                }
        ));

        // 绑定 Button 函数
        lua.set_function("UIButton", sol::overload(
                [](const char *label) { return ImGui::Button(label); },
                [](const char *label, const ImVec2 &size) { return ImGui::Button(label, size); }
        ));

        // 绑定 Checkbox 函数
        lua.set_function("UICheckbox", &ImGui::Checkbox);

        // 绑定 RadioButton 函数
        lua.set_function("UIRadioButton", sol::overload(
                [](const char *label, bool active) { return ImGui::RadioButton(label, active); },
                [](const char *label, int *v, int v_button) { return ImGui::RadioButton(label, v, v_button); }
        ));

        // 绑定 ProgressBar 函数
        lua.set_function("ProgressBar", sol::overload(
                [](float fraction, const ImVec2 &size_arg, const char *overlay) {
                    ImGui::ProgressBar(fraction, size_arg, overlay);
                },
                [](float fraction, const ImVec2 &size_arg) {
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
                [](ImTextureID user_texture_id, const ImVec2 &size, const ImVec2 &uv0, const ImVec2 &uv1,
                   const ImVec4 &tint_col, const ImVec4 &border_col) {
                    ImGui::Image(user_texture_id, size, uv0, uv1, tint_col, border_col);
                },
                [](ImTextureID user_texture_id, const ImVec2 &size, const ImVec2 &uv0, const ImVec2 &uv1,
                   const ImVec4 &tint_col) {
                    ImGui::Image(user_texture_id, size, uv0, uv1, tint_col);
                },
                [](ImTextureID user_texture_id, const ImVec2 &size, const ImVec2 &uv0, const ImVec2 &uv1) {
                    ImGui::Image(user_texture_id, size, uv0, uv1);
                },
                [](ImTextureID user_texture_id, const ImVec2 &size) {
                    ImGui::Image(user_texture_id, size);
                }
        ));
        lua.set_function("UIImageButton", sol::overload(
                [](const char *str_id, ImTextureID user_texture_id, const ImVec2 &size, const ImVec2 &uv0,
                   const ImVec2 &uv1, const ImVec4 &bg_col, const ImVec4 &tint_col) {
                    return ImGui::ImageButton(str_id, user_texture_id, size, uv0, uv1, bg_col, tint_col);
                },
                [](const char *str_id, ImTextureID user_texture_id, const ImVec2 &size, const ImVec2 &uv0,
                   const ImVec2 &uv1, const ImVec4 &bg_col) {
                    return ImGui::ImageButton(str_id, user_texture_id, size, uv0, uv1, bg_col);
                },
                [](const char *str_id, ImTextureID user_texture_id, const ImVec2 &size, const ImVec2 &uv0,
                   const ImVec2 &uv1) {
                    return ImGui::ImageButton(str_id, user_texture_id, size, uv0, uv1);
                },
                [](const char *str_id, ImTextureID user_texture_id, const ImVec2 &size) {
                    return ImGui::ImageButton(str_id, user_texture_id, size);
                }
        ));

        // 绑定 Combo 函数
        lua.set_function("UICombo", sol::overload(
                [](const char *label, int *current_item, const char *const items[], int items_count,
                   int popup_max_height_in_items) {
                    return ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);
                },
                [](const char *label, int *current_item, const char *items_separated_by_zeros,
                   int popup_max_height_in_items) {
                    return ImGui::Combo(label, current_item, items_separated_by_zeros, popup_max_height_in_items);
                }
        ));

        lua.set_function("UIBeginCombo", sol::overload(
                [](const char *label, const char *preview_value, ImGuiComboFlags flags) {
                    return ImGui::BeginCombo(label, preview_value, flags);
                },
                [](const char *label, const char *preview_value) {
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

        lua.set_function("UISliderAngle", [](const char* label, float* v_rad, float v_degrees_min, float v_degrees_max, const char* format, ImGuiSliderFlags flags) {
            return ImGui::SliderAngle(label, v_rad, v_degrees_min, v_degrees_max, format, flags);
        });

        lua.set_function("UIVSliderFloat", [](const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
            return ImGui::VSliderFloat(label, size, v, v_min, v_max, format, flags);
        });

        lua.set_function("UIVSliderInt", [](const char* label, const ImVec2& size, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
            return ImGui::VSliderInt(label, size, v, v_min, v_max, format, flags);
        });

        lua.set_function("UIVSliderScalar", [](const char* label, const ImVec2& size, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
            return ImGui::VSliderScalar(label, size, data_type, p_data, p_min, p_max, format, flags);
        });

        lua.set_function("UIInputText", [](const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
            return ImGui::InputText(label, buf, buf_size, flags);
        });

        lua.set_function("UIInputTextMultiline", [](const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags) {
            return ImGui::InputTextMultiline(label, buf, buf_size, size, flags);
        });

        lua.set_function("UIInputTextWithHint", [](const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
            return ImGui::InputTextWithHint(label, hint, buf, buf_size, flags);
        });

        lua.set_function("UIInputFloat", [](const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags) {
            return ImGui::InputFloat(label, v, step, step_fast, format, flags);
        });

        lua.set_function("UIInputFloat2", [](const char* label, float v[2], const char* format, ImGuiInputTextFlags flags) {
            return ImGui::InputFloat2(label, v, format, flags);
        });

        lua.set_function("UIInputFloat3", [](const char* label, float v[3], const char* format, ImGuiInputTextFlags flags) {
            return ImGui::InputFloat3(label, v, format, flags);
        });

        lua.set_function("UIInputFloat4", [](const char* label, float v[4], const char* format, ImGuiInputTextFlags flags) {
            return ImGui::InputFloat4(label, v, format, flags);
        });

        lua.set_function("UIInputInt", [](const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags) {
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

        lua.set_function("UIInputDouble", [](const char* label, double* v, double step, double step_fast, const char* format, ImGuiInputTextFlags flags) {
            return ImGui::InputDouble(label, v, step, step_fast, format, flags);
        });

        lua.set_function("UIInputScalar", [](const char* label, ImGuiDataType data_type, void* p_data, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags) {
            return ImGui::InputScalar(label, data_type, p_data, p_step, p_step_fast, format, flags);
        });

        lua.set_function("UIInputScalarN", [](const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags) {
            return ImGui::InputScalarN(label, data_type, p_data, components, p_step, p_step_fast, format, flags);
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

        lua.set_function("UIColorPicker4", [](const char* label, float col[4], ImGuiColorEditFlags flags, const float* ref_col) {
            return ImGui::ColorPicker4(label, col, flags, ref_col);
        });

        lua.set_function("UIBeginListBox", [](const char* label, const ImVec2& size) {
            return ImGui::BeginListBox(label, size);
        });

        lua.set_function("UIEndListBox", []() {
            ImGui::EndListBox();
        });

        lua.set_function("UIListBox", [](const char* label, int* current_item, const sol::table& items, int height_in_items) {
            std::vector<const char*> item_array;
            for (auto& item : items) {
                item_array.push_back(item.second.as<const char*>());
            }
            return ImGui::ListBox(label, current_item, item_array.data(), static_cast<int>(item_array.size()), height_in_items);
        });
        
        lua.set_function("LoadImage", &Application::LoadTexture);

    }


    std::string title = reinterpret_cast<const char *>(u8"文件选择");
    std::vector<std::string> typeFilters;
    bool fileBrowser = false;
    std::string BrowserPath;
    ConfirmDelegate PathOnConfirm = nullptr;

    inline void FileChooser() {
        static ImGui::FileBrowser fileDialog(ImGuiFileBrowserFlags_MultipleSelection | ImGuiFileBrowserFlags_NoModal);
        static std::string path;

        if (fileBrowser) {
            fileDialog.SetTitle(title);
            fileDialog.SetTypeFilters(typeFilters);
            fileDialog.Open();
            fileDialog.Display();
            if (fileDialog.IsOpened()) {
                if (fileDialog.HasSelected()) {
                    // Get all selected files (if multiple selections are allowed)
                    auto selectedFiles = fileDialog.GetMultiSelected();
                    if (!selectedFiles.empty()) {
                        // Return the first selected file's path
                        path = selectedFiles[0].string();
                        BrowserPath = path;
                        if (PathOnConfirm != nullptr) {
                            PathOnConfirm();
                        }
                    }
                    fileBrowser = false;
                    title = reinterpret_cast<const char *>(u8"文件选择");
                    typeFilters.clear();
                    PathOnConfirm = nullptr;
                    fileDialog.Close();
                }
            }
            if (!fileDialog.IsOpened()) {
                BrowserPath = path;
                fileBrowser = false;
                title = reinterpret_cast<const char *>(u8"文件选择");
                typeFilters.clear();
                PathOnConfirm = nullptr;
                fileDialog.Close();
            }
        }
    }

    // 加载纹理,并保存元数据
    GLuint LoadTexture(const std::string &path);

    void Vits(std::string text);

    bool Initialize();

    bool is_valid_text(const std::string &text) {

        if (text.empty()) return false;

        bool is_space = text.find_first_not_of(" \t\r\n") == std::string::npos;
        bool is_special = text.find_first_not_of(special_chars) == std::string::npos;

        if (is_space || is_special) return false;

        if (is_space && is_special) return false;

        return true;
    }

    void RuntimeDetector();

    int countTokens(const std::string &str);

    void GetClaudeHistory();

    static void EmptyFunction() {
        // Do nothing
    }

    void ShowConfirmationDialog(const char *title, const char *content, bool &mstate,
                                ConfirmDelegate on_confirm = nullptr,
                                const char *failure = nullptr,
                                const char *success = nullptr,
                                const char *confirm = reinterpret_cast<const char *>(u8"确定"),
                                const char *cancel = reinterpret_cast<const char *>(u8"取消")
    );

    static bool compareByTimestamp(const Chat &a, const Chat &b) {
        return a.timestamp < b.timestamp;
    }

    Billing billing;
public:
    std::string WhisperConvertor(const std::string &file);

    void WhisperModelDownload(const std::string &model = "ggml-base.bin");

    void WhisperExeInstaller();

    void VitsExeInstaller();

    bool Installer(std::map<std::string, std::string> tasks);

    explicit Application(const Configure &configure, bool setting = false);

    int Renderer();

    bool CheckFileExistence(const std::string &filePath, const std::string &fileType,
                            const std::string &executableFile = "", bool isExecutable = false);
};

#endif