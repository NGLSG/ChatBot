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
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

// 如果使用 Windows 平台，还需要这些额外包含
#ifdef _WIN32
#include <GLFW/glfw3native.h>
#endif
#include "stb_image_write.h"
#include "stb_image.h"
#include "Impls/Bots.h"
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
const std::string VERSION = reinterpret_cast<const char*>(u8"ChatBot v2.1.0");
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

        void addChild(Ref<Chat> child)
        {
            child->parent = this;
            children.push_back(child);
        }

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
    const std::string CustomRulesPath = "CustomRules/";
    const std::string special_chars = R"(~!@#$%^&*()_+`-={}|[]\:";'<>?,./)";

    const int sampleRate = 16000;
    const int framesPerBuffer = 512;

    const std::vector<std::string> dirs = {
        PluginPath, Conversation, bin, bin + VitsConvertor, bin + WhisperPath,
        bin + ChatGLMPath, bin + Live2DPath, model, model + VitsPath,
        model + WhisperPath, model + Live2DPath, CustomRulesPath,
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
    const std::vector<std::string> roles = {"user", "system"};
    const std::vector<std::string> proxies = {"Cloudflare", "Tencent Cloud"};
    const std::vector<std::string> commands = {
        "/draw", reinterpret_cast<const char*>(u8"/绘图"),
        "/help", reinterpret_cast<const char*>(u8"/帮助")
    };


    std::string title = reinterpret_cast<const char*>(u8"文件选择");
    std::vector<std::string> typeFilters;
    bool fileBrowser = false;
    ConfirmDelegate PathOnConfirm = nullptr;
    inline static bool PyInstalled = false;
    std::string BrowserPath;

    std::filesystem::path ConversationPath = "Conversations/";

    std::vector<std::string> forbidLuaPlugins;
    std::vector<std::string> live2dModel;
    std::vector<std::string> speakers = {reinterpret_cast<const char*>(u8"空空如也")};
    std::vector<std::string> vitsModels = {reinterpret_cast<const char*>(u8"空空如也")};
    std::map<std::string, std::vector<std::string>> codes;
    std::unordered_map<std::string, CustomRule> customRuleTemplates;
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

    struct VitsQueueItem
    {
        bool isApi; // 是否为API任务
        std::string text; // 文本内容
        std::string endPoint; // API端点（仅用于API任务）
        std::string outputFile; // 输出文件名
    };

    struct PreloadedAudio
    {
        std::string filename; // 音频文件名
        float* data; // 音频数据
        int sampleRate; // 采样率
        int channels; // 通道数
        int frames; // 总帧数
        bool isListen; // 是否需要监听
        bool ready; // 是否已加载完成

        PreloadedAudio() : data(nullptr), sampleRate(0), channels(0), frames(0), isListen(false), ready(false)
        {
        }

        ~PreloadedAudio()
        {
            if (data) delete[] data;
        }
    };

    // 添加新的队列和状态变量
    std::queue<VitsQueueItem> vitsQueue; // 任务队列
    std::mutex vitsQueueMutex; // 队列互斥锁
    std::queue<std::string> vitsPlayQueue; // 播放队列
    std::mutex vitsPlayQueueMutex; // 播放队列互斥锁
    inline static std::mutex audioMutex;

    bool isPlaying; // 当前是否正在播放
    std::shared_ptr<PreloadedAudio> nextAudio; // 下一个预加载的音频
    std::shared_ptr<PreloadedAudio> currentAudio; // 当前播放的音频

    vector<std::shared_ptr<Application::Chat>> load(std::string name = "default");

    void save(std::string name = "default", bool out = true);

    void del(std::string name = "default");

    void AddChatRecord(Ref<Chat> data);
    void DeleteAllBotChat();

    void VitsAudioPlayer();
    void VitsQueueHandler();

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

    template <typename... Args>
    void PluginRun(const std::string& method, Args... args)
    {
        for (auto& [name, script] : PluginsScript1)
        {
            // 检查是否在禁止插件列表中
            if (ranges::find(forbidLuaPlugins, name) != forbidLuaPlugins.end())
            {
                continue;
            }
            script->Invoke(method, std::forward<Args>(args)...);
        }
    }

    template <typename T, typename... Args>
    T PluginRunWithRes(const std::string& method, Args... args)
    {
        for (auto& [name, script] : PluginsScript1)
        {
            // 检查是否在禁止插件列表中
            if (ranges::find(forbidLuaPlugins, name) != forbidLuaPlugins.end())
            {
                continue;
            }
            sol::object t = script->Invoke(method, std::forward<Args>(args)...);
            if (t.is<T>())
            {
                return t.as<T>();
            }
            return T();
        }
        return T();
    }

    void EditCustomRule(CustomRule& rule)
    {
        if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"基本信息"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BeginGroup();

            ImGui::Checkbox(reinterpret_cast<const char*>(u8"启用规则"), &rule.enable);
            ImGui::SameLine(200);
            ImGui::Checkbox(reinterpret_cast<const char*>(u8"支持系统角色"), &rule.supportSystemRole);

            strcpy_s(GetBufferByName("ruleName").buffer, TEXT_BUFFER, rule.name.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"规则名称"), GetBufferByName("ruleName").buffer,
                                 TEXT_BUFFER))
                rule.name = GetBufferByName("ruleName").buffer;
            ImGui::PopStyleVar();

            strcpy_s(GetBufferByName("author").buffer, TEXT_BUFFER, rule.author.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"作者"), GetBufferByName("author").buffer, TEXT_BUFFER))
                rule.author = GetBufferByName("author").buffer;
            ImGui::PopStyleVar();

            strcpy_s(GetBufferByName("version").buffer, TEXT_BUFFER, rule.version.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"版本"), GetBufferByName("version").buffer, TEXT_BUFFER))
                rule.version = GetBufferByName("version").buffer;
            ImGui::PopStyleVar();

            ImGui::Text(reinterpret_cast<const char*>(u8"描述:"));
            strcpy_s(GetBufferByName("description").buffer, TEXT_BUFFER, rule.description.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputTextMultiline("##Description", GetBufferByName("description").buffer, TEXT_BUFFER,
                                          ImVec2(0, 80)))
                rule.description = GetBufferByName("description").buffer;
            ImGui::PopStyleVar();

            ImGui::EndGroup();
            ImGui::Separator();

            /*strcpy_s(GetBufferByName("apiPath").buffer, TEXT_BUFFER, rule.apiPath.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"API路径"), GetBufferByName("apiPath").buffer,
                                 TEXT_BUFFER))
                rule.apiPath = GetBufferByName("apiPath").buffer;
            ImGui::PopStyleVar();*/

            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               reinterpret_cast<const char*>(u8"支持变量: ${MODEL}, ${API_KEY}"));
        }
        if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"变量声明")))
        {
            ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.2f, 1.0f), reinterpret_cast<const char*>(u8"变量:"));

            ImGui::BeginChild("##VarsList", ImVec2(0, 150), true);
            if (rule.vars.empty())
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"暂无变量"));
            std::string varToDelete;
            int idx = 0;
            for (auto& var : rule.vars)
            {
                ImGui::PushID(idx++);
                strcpy_s(GetBufferByName("varName").buffer, TEXT_BUFFER, var.name.c_str());
                strcpy_s(GetBufferByName("varValue").buffer, TEXT_BUFFER, var.value.c_str());

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
                bool keyChanged = false;
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"变量名称"), GetBufferByName("varName").buffer,
                                     TEXT_BUFFER))
                {
                    keyChanged = true;
                    var.name = GetBufferByName("varName").buffer;
                }

                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"变量值"), GetBufferByName("varValue").buffer,
                                     TEXT_BUFFER))
                    var.value = GetBufferByName("varValue").buffer;
                ImGui::PopStyleVar();

                if (keyChanged)
                {
                    varToDelete = var.name;
                }

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 0.9f));
                if (ImGui::Button(reinterpret_cast<const char*>(u8"删除##var"), ImVec2(60, 0)))
                    varToDelete = var.name;
                ImGui::PopStyleColor(2);
                ImGui::PopID();
            }

            if (!varToDelete.empty())
                erase_if(rule.vars, [&varToDelete](const CustomVariable& var) { return var.name == varToDelete; });
            ImGui::EndChild();

            ImGui::BeginGroup();
            static char newHeaderKey[256] = {0};
            static char newHeaderValue[256] = {0};

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
            ImGui::InputText(reinterpret_cast<const char*>(u8"新请求头"), newHeaderKey, sizeof(newHeaderKey));

            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
            ImGui::InputText(reinterpret_cast<const char*>(u8"值"), newHeaderValue, sizeof(newHeaderValue));
            ImGui::PopStyleVar();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"添加"), ImVec2(60, 0)))
            {
                if (strlen(newHeaderKey) > 0)
                {
                    rule.vars.push_back(CustomVariable{newHeaderKey, false, newHeaderValue});
                    memset(newHeaderKey, 0, sizeof(newHeaderKey));
                    memset(newHeaderValue, 0, sizeof(newHeaderValue));
                }
            }
            ImGui::PopStyleColor(2);
            ImGui::EndGroup();
        }

        if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"API密钥配置")))
        {
            const char* roleTypes[] = {"HEADERS", "URL", "NONE"};
            int currentRoleIndex = 0;

            for (int i = 0; i < IM_ARRAYSIZE(roleTypes); i++)
                if (rule.apiKeyRole.role == roleTypes[i])
                {
                    currentRoleIndex = i;
                    break;
                }

            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"密钥位置:"));
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f),
                               reinterpret_cast<const char*>(u8"HEADERS=请求头 URL=URL参数 NONE=不使用"));

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::Combo(reinterpret_cast<const char*>(u8"密钥方式"), &currentRoleIndex, roleTypes,
                             IM_ARRAYSIZE(roleTypes)))
                rule.apiKeyRole.role = roleTypes[currentRoleIndex];
            ImGui::PopStyleVar();


            if (rule.apiKeyRole.role == "HEADERS")
            {
                strcpy_s(GetBufferByName("header").buffer, TEXT_BUFFER, rule.apiKeyRole.header.c_str());
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"请求头前缀"), GetBufferByName("header").buffer,
                                     TEXT_BUFFER))
                    rule.apiKeyRole.header = GetBufferByName("header").buffer;
                ImGui::PopStyleVar();
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                                   reinterpret_cast<const char*>(u8"常见格式: Authorization: Bearer "));
            }
        }

        if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"提示角色配置")))
        {
            strcpy_s(GetBufferByName("conversationSuffix").buffer, TEXT_BUFFER, rule.promptRole.prompt.suffix.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"轮次前缀"), GetBufferByName("conversationSuffix").buffer,
                                 TEXT_BUFFER))
                rule.promptRole.prompt.suffix = GetBufferByName("conversationSuffix").buffer;
            ImGui::PopStyleVar();
            ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.2f, 1.0f), reinterpret_cast<const char*>(u8"提示内容配置:"));


            strcpy_s(GetBufferByName("promptPath").buffer, TEXT_BUFFER, rule.promptRole.prompt.path.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"提示路径"), GetBufferByName("promptPath").buffer,
                                 TEXT_BUFFER))
                rule.promptRole.prompt.path = GetBufferByName("promptPath").buffer;
            ImGui::PopStyleVar();

            ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.2f, 1.0f), reinterpret_cast<const char*>(u8"角色配置:"));

            strcpy_s(GetBufferByName("rolePath").buffer, TEXT_BUFFER, rule.promptRole.role.path.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"角色路径"), GetBufferByName("rolePath").buffer,
                                 TEXT_BUFFER))
                rule.promptRole.role.path = GetBufferByName("rolePath").buffer;
            ImGui::PopStyleVar();

            ImGui::TextWrapped(
                reinterpret_cast<const char*>(u8"提示: 对于大多数API，提示轮次前缀设为'messages'，提示路径设为'content'，角色路径设为'role'即可"));
            ImGui::TextWrapped(
                reinterpret_cast<const char*>(u8"对应:{\"messages\":[\"content\":\"prompt\",\"role\":\"user\"}\"]}"));
        }
        if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"必须参数配置")))
        {
            int i = 0;
            for (auto& it : rule.extraMust)
            {
                ImGui::PushID(i++);
                ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.2f, 1.0f), (it.content + " 配置:").c_str());

                strcpy_s(GetBufferByName("path").buffer, TEXT_BUFFER, it.path.c_str());
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                if (ImGui::InputText("提示路径", GetBufferByName("path").buffer,
                                     TEXT_BUFFER))
                    it.path = GetBufferByName("path").buffer;
                ImGui::PopStyleVar();

                strcpy_s(GetBufferByName("suffix").buffer, TEXT_BUFFER, it.suffix.c_str());
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                if (ImGui::InputText("提示前缀", GetBufferByName("suffix").buffer,
                                     TEXT_BUFFER))
                    it.suffix = GetBufferByName("suffix").buffer;
                ImGui::PopStyleVar();
                ImGui::PopID();
            }
        }

        if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"参数配置")))
        {
            ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.2f, 1.0f), reinterpret_cast<const char*>(u8"添加请求参数:"));

            ImGui::BeginChild("##ParamsList", ImVec2(0, 150), true);
            if (rule.params.empty())
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                                   reinterpret_cast<const char*>(u8"暂无参数，点击下方添加按钮增加参数"));

            for (int i = 0; i < rule.params.size(); i++)
            {
                ImGui::PushID(i);
                ImGui::BeginGroup();
                ImGui::Text(reinterpret_cast<const char*>(u8"参数 %d:"), i + 1);

                strcpy_s(GetBufferByName("paramSuffix").buffer, TEXT_BUFFER, rule.params[i].suffix.c_str());
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"JSON键##suffix"),
                                     GetBufferByName("paramSuffix").buffer, TEXT_BUFFER))
                    rule.params[i].suffix = GetBufferByName("paramSuffix").buffer;
                ImGui::PopStyleVar();

                strcpy_s(GetBufferByName("paramPath").buffer, TEXT_BUFFER, rule.params[i].path.c_str());
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"JSON路径##path"),
                                     GetBufferByName("paramPath").buffer, TEXT_BUFFER))
                    rule.params[i].path = GetBufferByName("paramPath").buffer;
                ImGui::PopStyleVar();

                strcpy_s(GetBufferByName("paramContent").buffer, TEXT_BUFFER, rule.params[i].content.c_str());
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"内容##content"),
                                     GetBufferByName("paramContent").buffer, TEXT_BUFFER))
                    rule.params[i].content = GetBufferByName("paramContent").buffer;
                ImGui::PopStyleVar();

                ImGui::Checkbox(reinterpret_cast<const char*>(u8"是否为字符串值"), &rule.params[i].isStr);
                ImGui::Separator();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 0.9f));
                if (ImGui::Button(reinterpret_cast<const char*>(u8"删除参数"), ImVec2(100, 25)))
                {
                    rule.params.erase(rule.params.begin() + i);
                    ImGui::PopStyleColor(2);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    continue;
                }
                ImGui::PopStyleColor(2);
                ImGui::EndGroup();
                ImGui::PopID();
            }
            ImGui::EndChild();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"添加参数"), ImVec2(120, 28)))
            {
                ParamsRole newParam;
                newParam.suffix = "messages";
                newParam.path = "content";
                newParam.content = "content";
                newParam.isStr = false;
                rule.params.push_back(newParam);
            }
            ImGui::PopStyleColor(2);

            ImGui::BeginChild("##参数说明", ImVec2(0, 90), true);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"参数说明："));
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"JSON键: 表示JSON中的键名"));
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               reinterpret_cast<const char*>(u8"JSON路径: 指定在JSON中的访问路径"));
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"内容: 参数的实际内容值"));
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               reinterpret_cast<const char*>(u8"是否为字符串值: 决定内容是否需要使用引号"));
            ImGui::EndChild();
        }

        if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"请求头配置")))
        {
            ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.2f, 1.0f), reinterpret_cast<const char*>(u8"自定义HTTP请求头:"));
            std::string headerToDelete;

            ImGui::BeginChild("##HeadersList", ImVec2(0, 150), true);
            if (rule.headers.empty())
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"暂无自定义请求头"));

            int idx = 0;
            for (auto& header : rule.headers)
            {
                ImGui::PushID(idx++);
                strcpy_s(GetBufferByName("headerKey").buffer, TEXT_BUFFER, header.first.c_str());
                strcpy_s(GetBufferByName("headerValue").buffer, TEXT_BUFFER, header.second.c_str());

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
                bool keyChanged = false;
                std::string newKey = header.first;
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"请求头名"), GetBufferByName("headerKey").buffer,
                                     TEXT_BUFFER))
                {
                    keyChanged = true;
                    newKey = GetBufferByName("headerKey").buffer;
                }

                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
                if (ImGui::InputText(reinterpret_cast<const char*>(u8"请求头值"), GetBufferByName("headerValue").buffer,
                                     TEXT_BUFFER))
                    header.second = GetBufferByName("headerValue").buffer;
                ImGui::PopStyleVar();

                if (keyChanged)
                {
                    rule.headers[newKey] = header.second;
                    headerToDelete = header.first;
                }

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 0.9f));
                if (ImGui::Button(reinterpret_cast<const char*>(u8"删除##header"), ImVec2(60, 0)))
                    headerToDelete = header.first;
                ImGui::PopStyleColor(2);
                ImGui::PopID();
            }

            if (!headerToDelete.empty())
                rule.headers.erase(headerToDelete);
            ImGui::EndChild();

            ImGui::BeginGroup();
            static char newHeaderKey[256] = {0};
            static char newHeaderValue[256] = {0};

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
            ImGui::InputText(reinterpret_cast<const char*>(u8"新请求头"), newHeaderKey, sizeof(newHeaderKey));

            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
            ImGui::InputText(reinterpret_cast<const char*>(u8"值"), newHeaderValue, sizeof(newHeaderValue));
            ImGui::PopStyleVar();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 0.9f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"添加"), ImVec2(60, 0)))
            {
                if (strlen(newHeaderKey) > 0)
                {
                    rule.headers[newHeaderKey] = newHeaderValue;
                    memset(newHeaderKey, 0, sizeof(newHeaderKey));
                    memset(newHeaderValue, 0, sizeof(newHeaderValue));
                }
            }
            ImGui::PopStyleColor(2);
            ImGui::EndGroup();

            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               reinterpret_cast<const char*>(u8"常见请求头示例: Content-Type: application/json"));
        }

        if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"角色映射配置")))
        {
            ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.2f, 1.0f), reinterpret_cast<const char*>(u8"配置不同角色的映射:"));

            ImGui::BeginChild("##RoleMapping", ImVec2(0, 120), true);

            strcpy_s(GetBufferByName("systemRole").buffer, TEXT_BUFFER, rule.roles["system"].c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"系统角色"), GetBufferByName("systemRole").buffer,
                                 TEXT_BUFFER))
                rule.roles["system"] = GetBufferByName("systemRole").buffer;
            ImGui::PopStyleVar();

            strcpy_s(GetBufferByName("userRole").buffer, TEXT_BUFFER, rule.roles["user"].c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"用户角色"), GetBufferByName("userRole").buffer,
                                 TEXT_BUFFER))
                rule.roles["user"] = GetBufferByName("userRole").buffer;
            ImGui::PopStyleVar();

            strcpy_s(GetBufferByName("assistantRole").buffer, TEXT_BUFFER, rule.roles["assistant"].c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"助手角色"), GetBufferByName("assistantRole").buffer,
                                 TEXT_BUFFER))
                rule.roles["assistant"] = GetBufferByName("assistantRole").buffer;
            ImGui::PopStyleVar();

            ImGui::EndChild();

            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               reinterpret_cast<const char*>(u8"典型设置: 系统=system, 用户=user, 助手=assistant"));
        }

        if (ImGui::CollapsingHeader(reinterpret_cast<const char*>(u8"响应配置")))
        {
            ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.2f, 1.0f), reinterpret_cast<const char*>(u8"响应解析配置:"));

            strcpy_s(GetBufferByName("respSuffix").buffer, TEXT_BUFFER, rule.responseRole.suffix.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"响应前缀"), GetBufferByName("respSuffix").buffer,
                                 TEXT_BUFFER))
                rule.responseRole.suffix = GetBufferByName("respSuffix").buffer;
            ImGui::PopStyleVar();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"响应前缀示例: data: "));

            strcpy_s(GetBufferByName("respContent").buffer, TEXT_BUFFER, rule.responseRole.content.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"内容路径"), GetBufferByName("respContent").buffer,
                                 TEXT_BUFFER))
                rule.responseRole.content = GetBufferByName("respContent").buffer;
            ImGui::PopStyleVar();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               reinterpret_cast<const char*>(u8"内容路径示例: choices/delta/content"));

            const char* callbackTypes[] = {"RESPONSE", "NONE"};
            int currentCallbackIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(callbackTypes); i++)
                if (rule.responseRole.callback == callbackTypes[i])
                {
                    currentCallbackIndex = i;
                    break;
                }

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::Combo(reinterpret_cast<const char*>(u8"类型"), &currentCallbackIndex, callbackTypes,
                             IM_ARRAYSIZE(callbackTypes)))
                rule.responseRole.callback = callbackTypes[currentCallbackIndex];
            ImGui::PopStyleVar();

            strcpy_s(GetBufferByName("stopFlag").buffer, TEXT_BUFFER, rule.responseRole.stopFlag.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::InputText(reinterpret_cast<const char*>(u8"停止标记"), GetBufferByName("stopFlag").buffer,
                                 TEXT_BUFFER))
                rule.responseRole.stopFlag = GetBufferByName("stopFlag").buffer;
            ImGui::PopStyleVar();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), reinterpret_cast<const char*>(u8"停止标记示例: [DONE]"));
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), reinterpret_cast<const char*>(u8"注意：此规则只配置流式响应规则"));
    }

    void ExportCustomRuleTemplates(const std::string& filePath)
    {
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            LogError("无法打开导出文件: {}", filePath);
            return;
        }

        try
        {
            size_t templateCount = customRuleTemplates.size();
            file.write(reinterpret_cast<char*>(&templateCount), sizeof(templateCount));

            for (const auto& [name, rule] : customRuleTemplates)
            {
                size_t nameLen = name.size();
                file.write(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
                file.write(name.c_str(), nameLen);

                YAML::Node node = YAML::convert<CustomRule>::encode(rule);
                std::string yamlStr = YAML::Dump(node);

                size_t strLen = yamlStr.size();
                file.write(reinterpret_cast<char*>(&strLen), sizeof(strLen));
                file.write(yamlStr.c_str(), strLen);
            }

            file.close();
            LogInfo("成功导出{}个自定义API模板", templateCount);
        }
        catch (const std::exception& e)
        {
            LogError("导出模板时发生错误: {}", e.what());
        }
    }

    bool ImportCustomRuleTemplates(const std::string& filePath)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            LogError("无法打开导入文件: {}", filePath);
            return false;
        }

        try
        {
            size_t templateCount;
            file.read(reinterpret_cast<char*>(&templateCount), sizeof(templateCount));

            std::unordered_map<std::string, CustomRule> importedTemplates;

            for (size_t i = 0; i < templateCount; i++)
            {
                size_t nameLen;
                file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));

                std::string name(nameLen, '\0');
                file.read(&name[0], nameLen);

                size_t strLen;
                file.read(reinterpret_cast<char*>(&strLen), sizeof(strLen));

                std::string yamlStr(strLen, '\0');
                file.read(&yamlStr[0], strLen);

                YAML::Node node = YAML::Load(yamlStr);
                CustomRule rule = node.as<CustomRule>();

                importedTemplates[name] = rule;
            }

            file.close();

            for (const auto& [name, rule] : importedTemplates)
            {
                customRuleTemplates[name] = rule;
            }

            LogInfo("成功导入{}个自定义API模板", templateCount);
            return true;
        }
        catch (const std::exception& e)
        {
            LogError("导入模板时发生错误: {}", e.what());
            return false;
        }
    }

    static bool isNonNormalChar(char c)
    {
        return std::isspace(static_cast<unsigned char>(c));
    }

    static bool isContentInvalid(const std::string& content)
    {
        if (content.empty())
        {
            return true;
        }

        return std::all_of(content.begin(), content.end(), isNonNormalChar);
    }

    void AddSubmit()
    {
        std::shared_future<std::string> submit_future = std::async(std::launch::async, [&]()
        {
            Ref<Chat> botR = CreateRef<Chat>();
            botR->flag = 1;
            botR->timestamp = Utils::GetCurrentTimestamp() + 1;
            std::lock_guard<std::mutex> lock(chat_history_mutex);
            chat_history.back()->addChild(botR);
            AddChatRecord(botR);
            if (configure.vits.enable)
                last_input += "(VE)";
            if (packageUpdate)
            {
                last_input += "Python deps Updated: \n" + newPackage;
                newPackage = "";
                packageUpdate = false;
            }
            if (StringExecutor::ToolsUpdated)
            {
                last_input += "Tools Updated: \n" + StringExecutor::NewTools;
                StringExecutor::NewTools = "";
                StringExecutor::ToolsUpdated = false;
            }
            bot->SubmitAsync(last_input, botR->timestamp, role, convid);
            botR->talking = true;

            auto process = [this,botR]()
            {
                std::string _tmpText = "";
                while (!bot->Finished(botR->timestamp))
                {
                    std::string msg = bot->GetResponse(botR->timestamp);
                    std::string res;
                    if (msg != "")
                    {
                        res = PluginRunWithRes<std::string>("OnChat", msg);
                        if (!isContentInvalid(res))
                        {
                            res = msg;
                        }
                    }
                    _tmpText += res;
                    if (configure.vits.enable)
                    {
                        _tmpText = StringExecutor::TTS(_tmpText);
                    }
                    botR->content = _tmpText;
                    botR->newMessage = true;


                    std::this_thread::sleep_for(std::chrono::milliseconds(16));
                }
                //->content = bot->GetResponse(botR->timestamp);
                _tmpText += bot->GetResponse(botR->timestamp);
                if (configure.vits.enable)
                {
                    _tmpText = StringExecutor::TTS(_tmpText);
                }
                botR->content = _tmpText;
                botR->talking = false;
                botR->content = StringExecutor::AutoExecute(botR->content, bot);
                return botR->content;
            };

            StringExecutor::SetPreProcessCallback(process);
            process();
            if (isContentInvalid(botR->content))
            {
                botR->content = bot->GetLastRawResponse();
            }


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
    void PreloadNextAudio();

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
        text_buffers.emplace_back(name);
        return text_buffers.back();
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
        {
            std::string pythonCommand = PythonHome + PythonExecute;

            return Utils::ExecuteShell(pythonCommand, pyPath, std::forward<Args>(args)...);
        }
        return std::format("错误：在路径 {0} 中找不到Python可执行文件", PythonHome);
    }

    inline static bool packageUpdate = false;
    inline static std::string newPackage = "";

    template <typename... Args>
    static std::string InstallPythonPackage(Args&&... args)
    {
        if (IsPythonInstalled())
        {
            packageUpdate = true;
            newPackage = std::forward<std::string>(args...);
            return Utils::ExecuteShell(PythonHome + PythonExecute, "-m", "pip", "install", newPackage);
        }
        return "";
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
