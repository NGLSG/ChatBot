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

enum State {
    OK = 0,
    NO_OPENAI_KEY,
    NO_VITS,
    NO_TRANSLATOR,
    NO_WHISPER,
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

    State state = state = State::OK;
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
    std::map<std::string, std::vector<std::string>> codes;


    bool vits = true;
    bool show_input_box = false;
    bool OnlySetting = false;

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

    int countTokens(const std::string &str) {
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
                } else {
                    tokens.push_back(std::string(2, c));
                }
            } else if (c >= -128 && c <= -65) {
                // 中文字符第二个部分，加入前一个token
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }

                // 加入当前字符所对应的token数量
                token = std::string(2, c);
                tokens.push_back(token);
                token.clear();
            } else if ((c >= '0' && c <= '9')
                       || (c >= 'a' && c <= 'z')
                       || (c >= 'A' && c <= 'Z')) {
                // 数字或者字母的一段，加入前一个token
                if (token.empty()) {
                    token += c;
                } else if (isdigit(c) == isdigit(token[0])) {
                    token += c;
                } else {
                    tokens.push_back(token);
                    token = c;
                }
            } else {
                // 特殊字符或者emoji等，加入前一个token
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }

                // 根据不同类型的字符加入当前字符所对应的token数量
                if (c == '\n') {
                    tokens.push_back(std::string(singleCharTokenCount, c));
                } else if (c == 0xF0) {
                    tokens.push_back(std::string(6, ' '));
                } else if (c == '[' || c == ']') {
                    tokens.push_back(std::string(singleCharTokenCount, c));
                } else if (c == ' ' || c == '\t' || c == '.' || c == ',' || c == ';' || c == ':' || c == '!' ||
                           c == '?' || c == '(' || c == ')' || c == '{' || c == '}' || c == '/' || c == '\\' ||
                           c == '+' || c == '-' || c == '*' || c == '=' || c == '<' || c == '>' || c == '|' ||
                           c == '&' || c == '^' || c == '%' || c == '$' || c == '#' || c == '@') {
                    tokens.push_back(std::string(singleCharTokenCount, c));
                } else {
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

    Billing billing;
public:
    std::string WhisperConvertor(const std::string &file);

    bool WhisperModelDownload(const std::string &model = "ggml-base.bin");

    bool WhisperExeInstaller();

    bool VitsExeInstaller();

    bool Installer(std::map<std::string, std::string> tasks);

    Application(const OpenAIData &chat_data, const TranslateData &data, const VITSData &VitsData,
                const WhisperData &WhisperData, const Live2D &Live2D, bool setting = false);

    explicit Application(const Configure &configure, bool setting = false);

    int Renderer();

    bool CheckFileExistence(const std::string &filePath, const std::string &fileType,
                            const std::string &executableFile = "", bool isExecutable = false);
};

#endif