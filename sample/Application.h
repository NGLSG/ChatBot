#ifndef APP_H
#define APP_H

#include <iostream>
#include <string>
#include <vector>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <regex>
#include <stb_image_write.h>
#include "stb_image.h"
#include "ChatBot.h"
#include "VoiceToText.h"
#include "Translate.h"
#include "Recorder.h"
#include "Configure.h"
#include "utils.h"

#define TEXT_BUFFER 4096
const std::string VERSION = reinterpret_cast<const char *>(u8"CyberGirl v1.2.1");

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
    };

    // 纹理元数据
    struct TextureMetaData {
        std::string path;
        int width;
        int height;
        int channels;
    };

    Ref<Translate> translator;
    Ref<ChatBot> bot;
    Ref<VoiceToText> voiceToText;
    Ref<Listener> listener;
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
    map<string, GLuint> TextureCache = {{"eye",     0},
                                        {"del",     0},
                                        {"message", 0},
                                        {"add",     0},
                                        {"send",    0}};

    std::mutex submit_futures_mutex;
    std::mutex chat_history_mutex;

    State state = State::OK;
    Configure configure;
    LConfigure lConfigure;
    int select_id = 0;
    int role_id = 0;
    int Rnum = 0;
    int selected_dir = 0;
    int vselected_dir = 0;
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
    const std::string Conversation = "Conversations/ChatHistory/";

    const int sampleRate = 16000;
    const int framesPerBuffer = 512;

    const std::vector<std::string> dirs = {Conversation, bin, bin + VitsConvertor, bin + WhisperPath,
                                           bin + ChatGLMPath, bin + Live2DPath, model, model + VitsPath,
                                           model + WhisperPath, model + Live2DPath,
                                           Resources};
    const std::vector<std::string> roles = {"user", "system", "assistant"};
    const std::vector<std::string> proxies = {"Cloudflare", "Tencent Cloud"};

    std::vector<std::string> mdirs;
    std::vector<std::string> speakers = {reinterpret_cast<const char *>(u8"空空如也")};
    std::vector<std::string> vitsModels = {reinterpret_cast<const char *>(u8"空空如也")};
    std::map<std::string, std::vector<std::string>> codes;


    bool vits = true;
    bool vitsModel = true;
    bool show_input_box = false;
    bool OnlySetting = false;
    bool whisper = false;
    bool mwhisper = false;
    bool AppRunning = true;
    long long FirstTime = 0;

    void save(std::string name = "default", bool out = true);

    std::vector<Chat> load(std::string name = "default");

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

    void ImGuiCopyTextToClipboard(const char *text) {
        if (strlen(text) > 0) {
            ImGui::SetClipboardText(text);
        }
    }

    // 加载纹理,并保存元数据
    GLuint LoadTexture(const char *path);

    void Vits(std::string text);

    bool Initialize();

    static bool is_valid_text(const std::string &text) {
        return text.empty() || text == "";
    }

    void RuntimeDetector();

    int countTokens(const std::string &str);

    void claudeHistory();

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