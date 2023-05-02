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
#include "imgui_markdown.h"
#include "Configure.h"
#include "utils.h"

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
	struct ChatRecord {
		std::string userAsk;
		std::string botAnswer;
	};
	Ref<Translate> translator;
	Ref<ChatBot> bot;
	Ref<VoiceToText> voiceToText;
	Ref<Listener> listener;
	WhisperData whisperData;
	VITSData vitsData;

	std::vector<std::string> conversations;
	std::vector<ChatRecord> chat_history;

	// 存储当前输入的文本
	std::string input_text;
	std::string last_input;
	std::string selected_text = "";
	std::string convid = "default";
	std::string role = "user";
	GLuint send_texture;

	std::mutex submit_futures_mutex;
	std::mutex chat_history_mutex;

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

	const std::vector<std::string> dirs = { Conversation, bin, bin + VitsConvertor, bin + WhisperPath,
										   bin + ChatGLMPath, model, model + VitsPath,
										   model + WhisperPath,
										   Resources };
	const std::vector<std::string> roles = { "user", "system", "assistant" };

	bool vits = true;
	bool show_input_box = false;
	bool OnlySetting = false;

	std::string remove_spaces(const std::string& str) {
		std::string result = str;
		remove(result.begin(), result.end(), ' ');
		return result;
	}

	//保存对话历史
	void save(std::string name = "default") {
		std::ofstream session_file(Conversation + name + ".yaml");

		if (session_file.is_open()) {
			// 创建 YAML 节点
			YAML::Node node;

			// 将 chat_history 映射到 YAML 节点
			for (const auto& record : chat_history) {
				// 创建一个新的 YAML 映射节点
				YAML::Node record_node(YAML::NodeType::Map);

				// 将 ChatRecord 对象的成员变量映射到 YAML 映射节点中
				record_node["userAsk"] = record.userAsk;
				record_node["botAnswer"] = record.botAnswer;

				// 将 YAML 映射节点添加到主节点中
				node.push_back(record_node);
			}

			// 将 YAML 节点写入文件
			session_file << node;

			session_file.close();
			LogInfo("Application : Save {0} successfully", name);
		}
		else {
			LogError("Application Error: Unable to save session {0},{1}", name, ".");
		}
	}

	void load(std::string name = "default") {
		chat_history.clear();
		if (UFile::Exists(Conversation + name + ".yaml")) {
			std::ifstream session_file(Conversation + name + ".yaml");

			if (session_file.is_open()) {
				// 从文件中读取 YAML 节点
				YAML::Node node = YAML::Load(session_file);

				// 将 YAML 节点映射到 chat_history
				for (const auto& record_node : node) {
					// 从 YAML 映射节点中读取 ChatRecord 对象的成员变量
					std::string userAsk = record_node["userAsk"].as<std::string>();
					std::string botAnswer = record_node["botAnswer"].as<std::string>();

					// 创建一个新的 ChatRecord 对象，并将其添加到 chat_history 中
					chat_history.push_back({ userAsk, botAnswer });
				}

				session_file.close();
				LogInfo("Application : Load {0} successfully", name);
			}
			else {
				LogError("Application Error: Unable to load session {0},{1}", name, ".");
			}
		}
	}

	void del(std::string name = "default") {
		if (remove((Conversation + name + ".yaml").c_str()) != 0) {
			LogError("OpenAI Error: Unable to delete session {0},{1}", name, ".");
		}
		LogInfo("Bot : 删除 {0} 成功", name);
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

	// 添加新的聊天记录
	void add_chat_record(const std::string& userAsk, const std::string& botAnswer) {
		chat_history.push_back({ userAsk, botAnswer });
	}

	static void TextMarkdown(const char* text);

	GLuint load_texture(const char* path);

	void Vits(std::string text);

	bool Initialize();

	static bool is_valid_text(const std::string& text) {
		// 定义正则表达式，匹配所有非特殊字符和空格
		std::regex pattern("[^\\p{P}\\s]+");

		// 检查字符串是否匹配正则表达式
		return !std::regex_match(text, pattern);
	}

public:
	std::string WhisperConvertor(const std::string& file);

	bool WhisperModelDownload(const std::string& model = "ggml-base.bin");

	bool WhisperExeInstaller();

	bool VitsExeInstaller();

	bool Installer(std::map<std::string, std::string> tasks);

	Application(const OpenAIData& chat_data, const TranslateData& data, const VITSData& VitsData,
		const WhisperData& WhisperData, bool setting = false);

	explicit Application(const Configure& configure, bool setting = false);

	int Renderer();

	bool CheckFileExistence(const std::string& filePath, const std::string& fileType,
		const std::string& executableFile = "", bool isExecutable = false);
};

#endif