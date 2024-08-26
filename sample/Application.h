#ifndef APP_H
#define APP_H
#define SOL_ALL_SAFETIES_ON 1

#include "utils.h"
#include <iostream>
#include <string>
#include <vector>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <regex>
#include <GLFW/glfw3.h>
#include "stb_image_write.h"
#include "stb_image.h"
#include "ChatBot.h"
#include "VoiceToText.h"
#include "Translate.h"
#include "Recorder.h"
#include "Configure.h"
#include "StableDiffusion.h"
#include "imfilebrowser.h"
#include "sol/sol.hpp"

#define TEXT_BUFFER 4096
const std::string VERSION = reinterpret_cast<const char *>(u8"CyberGirl v1.4");
extern std::vector<std::string> scommands;
extern bool cpshow;
// 定义一个委托类型，它接受一个空参数列表，返回类型为 void
typedef std::function<void()> ConfirmDelegate;

enum State {
    OK = 0,
    NO_BOT_KEY,
    NO_VITS,
    NO_TRANSLATOR,
    NO_WHISPER,
    NO_WHISPERMODEL,
    FIRST_USE,
};


class Application {
private:
#ifdef _WIN32
    const std::string VitsConvertUrl =
            "https://github.com/NGLSG/MoeGoe/releases/download/1.1/VitsConvertor-win64.tar.gz";
    const std::string whisperUrl =
            "https://github.com/ggerganov/whisper.cpp/releases/download/v1.3.0/whisper-bin-Win32.zip";
    const std::string vitsFile = "VitsConvertor.tar.gz";
    const std::string exeSuffix = ".exe";
#else
    const std::string whisperUrl = "https://github.com/ggerganov/whisperData.cpp/releases/download/v1.3.0/whisper-bin-x64.zip";
    const std::string VitsConvertUrl = "https://github.com/NGLSG/MoeGoe/releases/download/1.1/VitsConvertor-linux64.tar.xz";
    const std::string vitsFile = "VitsConvertor.tar.gz";
    const std::string exeSuffix = "";
#endif

    struct Chat {
        int flag = 0; //0=user;1=bot
        long long timestamp;
        std::string content;
        std::string image;
    };

    Ref<Translate> translator;
    Ref<ChatBot> bot;
    Ref<VoiceToText> voiceToText;
    Ref<Listener> listener;
    Ref<StableDiffusion> stableDiffusion;
    WhisperCreateInfo whisperData;
    VITSData vitsData;
    Live2D live2D;
    GLFWwindow* window = nullptr;

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

    struct TextBuffer {
        std::string VarName = "";
        char buffer[4096] = "";

        TextBuffer(std::string varName = "") {
            VarName = varName;
        }
    };

    std::vector<TextBuffer> text_buffers;
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

    const std::vector<std::string> dirs = {
        PluginPath, Conversation, bin, bin + VitsConvertor, bin + WhisperPath,
        bin + ChatGLMPath, bin + Live2DPath, model, model + VitsPath,
        model + WhisperPath, model + Live2DPath,
        Resources
    };
    const std::vector<std::string> whisperModel = {"tiny", "base", "small", "medium", "large"};
    const std::vector<std::string> tmpWhisperModel = {
        "model/Whisper/ggml-tiny.bin", "model/Whisper/ggml-base.bin",
        "model/Whisper/ggml-small.bin", "model/Whisper/ggml-medium.bin",
        "model/Whisper/ggml-large.bin"
    };
    const std::vector<std::string> sampleIndices = {
        "Euler a", "Euler", "LMS", "Heun", "DPM2", "DPM2 a", "DPM++ 2S a",
        "DPM++ 2M", "DPM++ SDE", "DPM fast", "DPM adaptive", "LMS Karras",
        "DPM2 Karras", "DPM2 a Karras", "DPM++ 2S a Karras",
        "DPM++ 2M Karras", "DPM++ SDE Karras", "DDIM", "PLMS", "UniPC"
    };
    const std::vector<std::string> roles = {"user", "system", "assistant"};
    const std::vector<std::string> proxies = {"Cloudflare", "Tencent Cloud"};
    const std::vector<std::string> commands = {
        "/draw", reinterpret_cast<const char *>(u8"/绘图"),
        "/help", reinterpret_cast<const char *>(u8"/帮助")
    };


    std::string title = reinterpret_cast<const char *>(u8"文件选择");
    std::vector<std::string> typeFilters;
    bool fileBrowser = false;
    std::string BrowserPath;
    ConfirmDelegate PathOnConfirm = nullptr;
    std::filesystem::path ConversationPath = "Conversations/";

    std::vector<std::string> live2dModel;
    std::vector<std::string> speakers = {reinterpret_cast<const char *>(u8"空空如也")};
    std::vector<std::string> vitsModels = {reinterpret_cast<const char *>(u8"空空如也")};
    std::map<std::string, std::vector<std::string>> codes;
    map<string, GLuint> TextureCache = {
        {"eye", 0},
        {"del", 0},
        {"message", 0},
        {"add", 0},
        {"send", 0},
        {"avatar", 0}
    };
    map<string, int> SelectIndices = {
        {"Live2D", 0},
        {"whisper", 0},
        {"vits", 0},
        {"conversation", 0},
        {"stableDiffusion", 0},
        {"role", 0}
    };
    map<string, GLuint> SDCache;

    bool vits = true;
    bool vitsModel = true;
    bool show_input_box = false;
    bool OnlySetting = false;
    bool whisper = false;
    bool mwhisper = false;
    bool AppRunning = true;
    long long FirstTime = 0;
    std::vector<std::string> PluginsScript;


    std::vector<Chat> load(std::string name = "default");

    void save(std::string name = "default", bool out = true);

    void del(std::string name = "default");

    void AddChatRecord(const Chat&data);

    void DeleteAllBotChat();

    void VitsListener();

    void Draw(Ref<std::string> prompt, long long ts, bool callFromBot);

    void Draw(const std::string&prompt, long long ts, bool callFromBot = false) {
        auto prompt_ref = CreateRef<std::string>(prompt);
        std::thread t([=] { Draw(prompt_ref, ts, callFromBot); });
        t.detach();
    }

    bool ContainsCommand(std::string&str, std::string&cmd, std::string&args) const;

    void InlineCommand(const std::string&cmd, const std::string&args, long long ts) {
        if (cmd == "draw" || cmd == reinterpret_cast<const char *>(u8"绘图")) {
            Draw(args, ts);
        }
        else if (cmd == "help" || reinterpret_cast<const char *>(u8"帮助")) {
            Chat help;
            help.flag = 1;
            help.timestamp = Utils::GetCurrentTimestamp() + 10;
            help.content = reinterpret_cast<const char *>(u8"内置命令:\n/draw或者/绘图 [prompt] 绘制一张图片\n/help或者/帮助 唤起帮助");
            chat_history.emplace_back(help);
        }
    }

    inline void UniversalStyle() {
        // 定义通用样式
        ImGuiStyle&style = ImGui::GetStyle();

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
        ImGuiStyle&style = ImGui::GetStyle();

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

    void initImGuiBindings(sol::state_view lua);


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
    GLuint LoadTexture(const std::string&path);

    void Vits(std::string text);

    bool Initialize();

    bool is_valid_text(const std::string&text) const;

    void RuntimeDetector();

    int countTokens(const std::string&str);

    void GetClaudeHistory();

    static void EmptyFunction() {
        // Do nothing
    }

    TextBuffer& GetBufferByName(const std::string&name) {
        return *std::ranges::find_if(text_buffers, [&](const TextBuffer&buffer) {
            return buffer.VarName == name;
        });
    }

    void ShowConfirmationDialog(const char* title, const char* content, bool&mstate,
                                ConfirmDelegate on_confirm = nullptr,
                                const char* failure = nullptr,
                                const char* success = nullptr,
                                const char* confirm = reinterpret_cast<const char *>(u8"确定"),
                                const char* cancel = reinterpret_cast<const char *>(u8"取消")
    );

    static bool compareByTimestamp(const Chat&a, const Chat&b) {
        return a.timestamp < b.timestamp;
    }

    Billing billing;

public:
    std::string WhisperConvertor(const std::string&file);

    void WhisperModelDownload(const std::string&model = "ggml-base.bin");

    void WhisperExeInstaller();

    void VitsExeInstaller();

    bool Installer(std::map<std::string, std::string> tasks);

    explicit Application(const Configure&configure, bool setting = false);

    int Renderer();

    bool CheckFileExistence(const std::string&filePath, const std::string&fileType,
                            const std::string&executableFile = "", bool isExecutable = false);
};

#endif
