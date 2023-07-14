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
#include "stb_image.h"
#include "ChatBot.h"
#include "VoiceToText.h"
#include "Translate.h"
#include "Recorder.h"
#include "Configure.h"
#include "utils.h"

#define TEXT_BUFFER 1024
const std::string VERSION = reinterpret_cast<const char *>(u8"Listener v1.2");

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
    const std::string VitsConvertUrl = "https://github.com/NGLSG/MoeGoe/releases/download/1.0/VitsConvertor-win64.tar.gz";
    const std::string whisperUrl = "https://github.com/ggerganov/whisper.cpp/releases/download/v1.3.0/whisper-bin-Win32.zip";
    const std::string vitsFile = "VitsConvertor.tar.gz";
    const std::string exeSuffix = ".exe";
#else
    const std::string whisperUrl = "https://github.com/ggerganov/whisperData.cpp/releases/download/v1.3.0/whisper-bin-x64.zip";
    const std::string VitsConvertUrl = "https://github.com/NGLSG/MoeGoe/releases/download/1.0/VitsConvertor-linux64.tar.xz";
    const std::string vitsFile = "VitsConvertor.tar.gz";
    const std::string exeSuffix = "";
#endif

    struct Chat {
        int flag = 0;//0=user;1=bot
        long long timestamp;
        std::string content;
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
    GLuint send_texture;
    GLuint eye_texture;

    std::mutex submit_futures_mutex;
    std::mutex chat_history_mutex;

    State state = State::OK;
    Configure configure;
    int select_id = 0;
    int role_id = 0;
    int Rnum = 0;
    int selected_dir = 0;
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
    std::map<std::string, std::vector<std::string>> codes;


    bool vits = true;
    bool show_input_box = false;
    bool OnlySetting = false;
    bool whisper = false;
    bool mwhisper = false;

    void save(std::string name = "default");

    std::vector<Chat> load(std::string name = "default");

    void del(std::string name = "default");

    void add_chat_record(const Chat &data) {
        chat_history.push_back(data);
    }

    // 渲染聊天框
    void render_chat_box();

    // 渲染设置
    void render_setting_box();

    // 渲染输入框
    void render_input_box();

    //渲染弹窗
    void render_popup_box();

    void render_code_box();

    // 渲染UI
    void render_ui();

    void ImGuiCopyTextToClipboard(const char *text) {
        if (strlen(text) > 0) {
            ImGui::SetClipboardText(text);
        }
    }

    GLuint load_texture(const char *path);

    void Vits(std::string text);

    bool Initialize();

    static bool is_valid_text(const std::string &text) {
        return text.empty() || text == "";
    }

    void RuntimeDetector();

    int countTokens(const std::string &str);

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

    Billing billing;
public:
    std::string WhisperConvertor(const std::string &file);

    void WhisperModelDownload(const std::string &model = "ggml-base.bin");

    void WhisperExeInstaller();

    void VitsExeInstaller();

    bool Installer(std::map<std::string, std::string> tasks);

    Application(const OpenAIData &chat_data, const TranslateData &data, const VITSData &VitsData,
                const WhisperData &WhisperData, const Live2D &Live2D, bool setting = false);

    explicit Application(const Configure &configure, bool setting = false);

    int Renderer();

    bool CheckFileExistence(const std::string &filePath, const std::string &fileType,
                            const std::string &executableFile = "", bool isExecutable = false);
};

#endif