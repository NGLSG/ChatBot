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

    int select_id = 0;
    int role_id = 0;

    char input_buffer[4096 * 32];
    const std::string bin = "bin/";
    const std::string VitsConvertor = "VitsConvertor/";
    const std::string model = "model/";
    const std::string Resources = "Resources/";
    const std::string VitsPath = "Vits/";
    const std::string WhisperPath = "Whisper/";
    const std::string ChatGLMPath = "ChatGLM/";
    const std::string Conversation = "Conversations/ChatHistory/";

    const int sampleRate = 16000;
    const int framesPerBuffer = 512;

    const std::vector<std::string> dirs = {Conversation, bin, bin + VitsConvertor, bin + WhisperPath,
                                           bin + ChatGLMPath, model, model + VitsPath,
                                           model + WhisperPath,
                                           Resources};
    const std::vector<std::string> roles = {"user", "system", "assistant"};

    bool vits = true;
    bool show_input_box = false;
    bool OnlySetting = false;

    std::string remove_spaces(const std::string &str) {
        std::string result = str;
        remove(result.begin(), result.end(), ' ');
        return result;
    }


    void save(std::string name = "default") {
        std::ofstream session_file(Conversation + name + ".yaml");

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

    std::vector<Chat> load(std::string name = "default") {

        if (UFile::Exists(Conversation + name + ".yaml")) {
            std::ifstream session_file(Conversation + name + ".yaml");

            if (session_file.is_open()) {
                // 从文件中读取 YAML 节点
                YAML::Node node = YAML::Load(session_file);

                // 将 YAML 节点映射到 chat_history
                for (const auto &record_node: node) {
                    // 从 YAML 映射节点中读取 ChatRecord 对象的成员变量
                    int flag = record_node["flag"].as<int>();
                    long long timestamp;
                    std::string content;
                    if (flag == 0) {
                        timestamp = record_node["user"]["timestamp"].as<long long>();
                        content = record_node["user"]["content"].as<std::string>();
                    } else {
                        timestamp = record_node["bot"]["timestamp"].as<long long>();
                        content = record_node["bot"]["content"].as<std::string>();

                    }
                    Chat record;
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

    void del(std::string name = "default") {
        if (remove((Conversation + name + ".yaml").c_str()) != 0) {
            LogError("OpenAI Error: Unable to delete session {0},{1}", name, ".");
        }
        LogInfo("Bot : 删除 {0} 成功", name);
    }

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

    // 渲染UI
    void render_ui();

    GLuint load_texture(const char *path);

    void Vits(std::string text);

    bool Initialize();

    static bool is_valid_text(const std::string &text) {
        // 定义正则表达式，匹配所有非特殊字符和空格
        std::regex pattern("[^\\p{P}\\s]+");

        // 检查字符串是否匹配正则表达式
        return !std::regex_match(text, pattern);
    }


    struct Code {
        std::string codeType;
        std::string codeContent;
        std::string normalContent;
    };

    Code splitMarkdownCodeBlock(const std::string &markdown) {
        Code result;

        // 找到代码块的起始位置
        size_t start = markdown.find("```");
        if (start == std::string::npos) {
            // 没有找到代码块
            return result;
        }

        // 找到代码块的语言类型
        size_t langStart = start + 3;
        size_t langEnd = markdown.find("\n", langStart);
        if (langEnd == std::string::npos) {
            // 没有找到语言类型，使用默认值
            result.codeType = "text";
        } else {
            result.codeType = markdown.substr(langStart, langEnd - langStart);
        }

        // 找到代码块的结束位置
        size_t end = markdown.find("```", langEnd);
        if (end == std::string::npos) {
            // 没有找到代码块的结束
            return result;
        }

        // 截取代码块的内容
        result.codeContent = markdown.substr(langEnd + 1, end - langEnd - 1);

        return result;
    }


    long long getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }


    std::string Stamp2Time(long long timestamp) {
        int ms = timestamp % 1000;//取毫秒
        time_t tick = (time_t) (timestamp / 1000);//转换时间
        struct tm tm;
        char s[40];
        tm = *localtime(&tick);
        strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &tm);
        std::string str(s);
        str = str + " " + std::to_string(ms);
        return str;
    }

public:
    std::string WhisperConvertor(const std::string &file);

    bool WhisperModelDownload(const std::string &model = "ggml-base.bin");

    bool WhisperExeInstaller();

    bool VitsExeInstaller();

    bool Installer(std::map<std::string, std::string> tasks);

    Application(const OpenAIData &chat_data, const TranslateData &data, const VITSData &VitsData,
                const WhisperData &WhisperData, bool setting = false);

    explicit Application(const Configure &configure, bool setting = false);

    int Renderer();

    bool CheckFileExistence(const std::string &filePath, const std::string &fileType,
                            const std::string &executableFile = "", bool isExecutable = false);


};

#endif