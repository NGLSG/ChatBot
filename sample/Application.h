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
#include "imgui_markdown.h"
#include "Script.h"
#include "sol/sol.hpp"

#define TEXT_BUFFER 4096
const std::string VERSION = reinterpret_cast<const char*>(u8"CyberGirl v1.6.3");
extern std::vector<std::string> scommands;
extern bool cpshow;
// 定义一个委托类型，它接受一个空参数列表，返回类型为 void
typedef std::function<void()> ConfirmDelegate;

enum State
{
    OK = 0,
    NO_BOT_KEY,
    NO_VITS,
    NO_TRANSLATOR,
    NO_WHISPER,
    NO_WHISPERMODEL,
    FIRST_USE,
};


class Application
{
private:
    inline static const std::string OllamaLink = "https://ollama.com/download";
    inline static const std::string DefaultModelInstallLink =
        "https://hf-mirror.com/unsloth/DeepSeek-R1-Distill-Qwen-1.5B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf?download=true";
#ifdef _WIN32
    inline static const std::string VitsConvertUrl =
        "https://github.com/NGLSG/MoeGoe/releases/download/1.1/VitsConvertor-win64.tar.gz";
    inline static const std::string whisperUrl =
        "https://github.com/ggerganov/whisper.cpp/releases/download/v1.3.0/whisper-bin-Win32.zip";
    inline static const std::string vitsFile = "VitsConvertor.tar.gz";
    inline static const std::string exeSuffix = ".exe";
    inline static const std::string PythonHome = "bin\\Python\\";
    inline static const std::string PythonExecute = "python.exe";
    inline static const bool usePackageManagerInstaller = false;
    inline static const std::string PythonInstallCMD = "";
    inline static const std::string PythonGetPip = "https://bootstrap.pypa.io/get-pip.py";
    inline static const std::string PythonLink =
        "https://mirrors.aliyun.com/python-release/windows/python-3.10.9-embed-amd64.zip";

#else
    inline static const std::string PythonGetPip="";
    inline static const std::string whisperUrl = "https://github.com/ggerganov/whisperData.cpp/releases/download/v1.3.0/whisper-bin-x64.zip";
    inline static const std::string VitsConvertUrl = "https://github.com/NGLSG/MoeGoe/releases/download/1.1/VitsConvertor-linux64.tar.xz";
    inline static const std::string vitsFile = "VitsConvertor.tar.gz";
    inline static const std::string exeSuffix = "";
    inline static const std::string PythonHome = "";
#ifdef __linux__
    inline static const bool usePackageManagerInstaller = true;

#ifdef __debian__
    inline static const std::string PythonInstallCMD = "yes | sudo apt install python3";
#elif defined(__fedora__)
    inline static const std::string PythonInstallCMD = "yes | sudo dnf install python3";
#elif defined(__centos__) || defined(__rhel__)
    inline static const std::string PythonInstallCMD = "yes | sudo yum install python3";
#elif defined(__arch__)
    inline static const std::string PythonInstallCMD = "yes | sudo pacman -S python";
#else
    inline static const std::string PythonInstallCMD = "";
    inline static const bool usePackageManagerInstaller = false;
#endif

    inline static std::string detectPythonExecutable() {
        std::string pythonExecutable;
        try {
            std::string output = exec("which python3");
            if (!output.empty()) {
                pythonExecutable = "python3";
            } else {
                output = exec("which python");
                if (!output.empty()) {
                    pythonExecutable = "python";
                }
            }
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }
        return pythonExecutable;
    }

    inline static const std::string PythonExecute = detectPythonExecutable();
#endif
#ifdef __APPLE__
    inline static const std::string PythonExecute="python3";
    inline static const bool usePackageManagerInstaller = true;
    inline static const PythonInstallCMD="yes | brew install python";
#endif

#endif

    struct Chat
    {
        int flag = 0; // 0=user;1=bot
        size_t timestamp;
        std::string thinkContent;
        std::string content = "...";
        std::string image;
        bool reedit = false;
        bool newMessage = true;
        bool talking = false;

        std::vector<Ref<Chat>> children; // 子节点列表
        std::vector<std::string> history; // 历史记录
        Chat* parent = nullptr; // 父节点指针
        int currentVersionIndex = 0; // 当前激活的子节点索引

        // 选择当前激活的分支
        bool selectBranch(int index)
        {
            if (index >= 0 && index < children.size())
            {
                content = history[index];
                currentVersionIndex = index;
                return true;
            }
            return false;
        }
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
    std::vector<std::shared_ptr<Chat>> chat_history;
    std::vector<std::shared_future<std::string>> submit_futures;

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
    bool loadingBot;
    ImFont* font;
    int Rnum = 0;
    int token;

    char input_buffer[4096 * 32] = "";

    struct TextBuffer
    {
        std::string VarName = "";
        char buffer[4096] = "";

        explicit TextBuffer(std::string varName = "")
        {
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
        "/draw", reinterpret_cast<const char*>(u8"/绘图"),
        "/help", reinterpret_cast<const char*>(u8"/帮助")
    };


    std::string title = reinterpret_cast<const char*>(u8"文件选择");
    std::vector<std::string> typeFilters;
    bool fileBrowser = false;
    inline static bool PyInstalled = false;
    std::string BrowserPath;
    ConfirmDelegate PathOnConfirm = nullptr;
    std::filesystem::path ConversationPath = "Conversations/";

    std::vector<std::string> forbidLuaPlugins;
    std::vector<std::string> live2dModel;
    std::vector<std::string> speakers = {reinterpret_cast<const char*>(u8"空空如也")};
    std::vector<std::string> vitsModels = {reinterpret_cast<const char*>(u8"空空如也")};
    std::map<std::string, std::vector<std::string>> codes;
    map<string, GLuint> TextureCache = {
        {"eye", 0},
        {"del", 0},
        {"message", 0},
        {"add", 0},
        {"send", 0},
        {"avatar", 0},
        {"pause", 0},
        {"play", 0},
        {"edit", 0},
        {"cancel", 0},
        {"copy", 0},
        {"right", 0},
        {"left", 0}
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
    bool Downloading = false;
    std::vector<Ref<Downloader>> downloaders;
    long long FirstTime = 0;
    float fontSize = 16;
    std::unordered_map<std::string, std::shared_ptr<Script>> PluginsScript1;


    vector<std::shared_ptr<Application::Chat>> load(std::string name = "default");

    void save(std::string name = "default", bool out = true);

    void del(std::string name = "default");

    void AddChatRecord(Ref<Chat> data);
    void DeleteAllBotChat();

    void VitsListener();

    void _Draw(Ref<std::string> prompt, long long ts, bool callFromBot, const std::string& negative);

    void Draw(const std::string& prompt, long long ts, bool callFromBot = false, const std::string& negative = "")
    {
        auto prompt_ref = CreateRef<std::string>(prompt);
        std::thread t([=] { _Draw(prompt_ref, ts, callFromBot, negative); });
        t.detach();
    }

    void DisplayInputText(Ref<Chat> chat, bool edit = false);


    bool ContainsCommand(std::string& str, std::string& cmd, std::string& args) const;

    void InlineCommand(const std::string& cmd, const std::string& args, long long ts);

    void CreateBot();

    void AddSubmit()
    {
        std::shared_future<std::string> submit_future = std::async(std::launch::async, [&]()
        {
            Ref<Chat> botR = CreateRef<Chat>();
            botR->flag = 1;
            botR->timestamp = Utils::GetCurrentTimestamp() + 1;
            std::lock_guard<std::mutex> lock(chat_history_mutex);
            chat_history.back()->children.emplace_back(botR);
            AddChatRecord(botR);
            bot->SubmitAsync(last_input, botR->timestamp, role, convid);
            botR->talking = true;
            while (!bot->Finished(botR->timestamp))
            {
                botR->content = bot->GetResponse(botR->timestamp);
                botR->newMessage = true;

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
            botR->content = bot->GetResponse(botR->timestamp);
            botR->talking = false;
            botR->content = StringExecutor::AutoExecute(botR->content, bot);

            return botR->content;
        }).share();
        {
            std::lock_guard<std::mutex> lock(submit_futures_mutex);
            submit_futures.push_back(std::move(submit_future));
        }
    }

    void RebuildChatHistory()
    {
        std::lock_guard<std::mutex> lock(chat_history_mutex);
        std::shared_ptr<Chat> chat = chat_history.front();
        std::vector<std::shared_ptr<Chat>> chatList;
        while (chat)
        {
            chatList.push_back(chat);
            if (!chat->children.empty())
            {
                chat = chat->children[chat->currentVersionIndex];
            }
            else
            {
                chat = nullptr;
            }
        }
        chat_history.clear();
        for (auto& it : chatList)
        {
            chat_history.push_back(it);
        }
    }

    static inline void UniversalStyle()
    {
        // 定义通用样式
        ImGuiStyle& style = ImGui::GetStyle();

        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);

        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
        style.Colors[ImGuiCol_Text] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
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
        style.FrameRounding = 5.0f;
        style.GrabRounding = 5.0f;
        style.WindowRounding = 5.0f;
        style.Colors[ImGuiCol_Button] = button_color;
        style.Colors[ImGuiCol_ButtonHovered] = button_hovered_color;
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    }

    // 渲染聊天框
    void RenderChatBox();

    // 渲染设置
    void RenderConfigBox();

    // 渲染输入框

    void RenderInputBox();

    //渲染弹窗
    void RenderPopupBox();
    void RenderDownloadBox();
    void RenderCodeBox();

    void RenderConversationBox();

    // 渲染UI
    void RenderUI();


    inline void FileChooser();

    // 加载纹理,并保存元数据
    GLuint LoadTexture(const std::string& path);

    void Vits(std::string text);

    bool Initialize();

    bool is_valid_text(const std::string& text) const;

    void RuntimeDetector();

    void GetClaudeHistory();

    static void EmptyFunction()
    {
        // Do nothing
    }

    TextBuffer& GetBufferByName(const std::string& name)
    {
        for (auto& it : text_buffers)
        {
            if (it.VarName == name)
            {
                return it;
            }
        }
    }

    void ShowConfirmationDialog(const char* title, const char* content, bool& mstate,
                                ConfirmDelegate on_confirm = nullptr,
                                const char* failure = nullptr,
                                const char* success = nullptr,
                                const char* confirm = reinterpret_cast<const char*>(u8"确定"),
                                const char* cancel = reinterpret_cast<const char*>(u8"取消")
    );
    void InitializeMarkdownConfig();


    static bool compareByTimestamp(const Ref<Chat> a, const Ref<Chat> b)
    {
        return a->timestamp < b->timestamp;
    }

    void RenderMarkdown(vector<char>& markdown, ImVec2 input_size);

public:
    inline static bool IsPythonInstalled()
    {
        if (!PyInstalled)
        {
            try
            {
                return UFile::Exists(PythonHome + PythonExecute);
            }
            catch (const std::runtime_error& e)
            {
                std::cerr << e.what() << std::endl;
            }
            return false;
        }
        return true;
    }

    static bool IsPipInstalled()
    {
        if (IsPythonInstalled())
        {
            try
            {
                return UFile::Exists(PythonHome + "Scripts/pip.exe");
            }
            catch (const std::runtime_error& e)
            {
                std::cerr << e.what() << std::endl;
            }
            return false;
        }
        return false;
    }

    template <typename... Args>
    static std::string ExecutePython(const std::string& pyPath, Args&&... args)
    {
        if (IsPythonInstalled())
            return Utils::ExecuteShell(PythonHome + PythonExecute, pyPath, args...);
        return fmt::format("Wrong there is no python executable in your path {0}", PythonHome);
    }

    static std::string GetPythonPackage()
    {
        if (IsPythonInstalled())
        {
            return Utils::ExecuteShell(PythonHome + PythonExecute, "-m", "pip", "list");
        }
        return "";
    }

    static std::string GetPythonVersion()
    {
        if (IsPythonInstalled())
            return Utils::ExecuteShell(PythonHome + PythonExecute, "--version");
        return "";
    }

    static std::string GetPythonHome()
    {
        return PythonHome + PythonExecute;
    }

    std::string WhisperConvertor(const std::string& file);

    void WhisperModelDownload(const std::string& model = "ggml-base.bin");

    void WhisperExeInstaller();

    void VitsExeInstaller();

    void Installer(std::pair<std::string, std::string> task,
                   std::function<void(Downloader*)> callback = nullptr);

    explicit Application(const Configure& configure, bool setting = false);

    int Renderer();

    ImGui::MarkdownImageData md_ImageCallback(ImGui::MarkdownLinkCallbackData data);

    void md_LinkCallback(ImGui::MarkdownLinkCallbackData data);

    bool CheckFileExistence(const std::string& filePath, const std::string& fileType,
                            const std::string& executableFile = "", bool isExecutable = false) const;
};

#endif
