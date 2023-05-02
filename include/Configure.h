#ifndef CONFIGURE_H
#define CONFIGURE_H

#include <yaml-cpp/yaml.h>
#include <string>
#include <iostream>

struct TranslateData {
	std::string appId;
	std::string APIKey;
	std::string proxy = "";
};

struct OpenAIData {
	bool useLocalModel = false;
	std::string modelPath = "model/ChatGLM/";
	std::string api_key;
	std::string model = "gpt-3.5-turbo";
	std::string proxy = "";
};

struct VitsTask {
	std::string model;
	std::string config;
	std::string text;
	std::string outpath = "tmp.wav";
	char choice = 't';
	int sid = 0;
	bool escape = false;
};

struct VITSData {
	bool enable = false;
	std::string model;
	std::string config;
	std::string lanType = "jp";
};

struct WhisperData {
	bool enable = false;
	bool useLocalModel = false;
	std::string model = "model/Whisper/ggml-base.bin";
};

struct Configure {
	OpenAIData openAi;
	TranslateData baiDuTranslator;
	VITSData vits;
	WhisperData whisper;
};

namespace YAML {
	template<>
	struct convert<TranslateData> {
		static Node encode(const TranslateData& data) {
			Node node;
			node["appId"] = data.appId;
			node["APIKey"] = data.APIKey;
			node["proxy"] = data.proxy;
			return node;
		}

		static bool decode(const Node& node, TranslateData& data) {
			if (!node["appId"] || !node["APIKey"]) {
				return false;
			}
			data.proxy = node["proxy"].as<std::string>();
			data.appId = node["appId"].as<std::string>();
			data.APIKey = node["APIKey"].as<std::string>();
			return true;
		}
	};

	template<>
	struct convert<OpenAIData> {
		static Node encode(const OpenAIData& data) {
			Node node;
			node["useLocalModel"] = data.useLocalModel;
			node["modelPath"] = data.modelPath;
			node["api_key"] = data.api_key;
			node["model"] = data.model;
			node["proxy"] = data.proxy;
			return node;
		}

		static bool decode(const Node& node, OpenAIData& data) {
			if (!node["api_key"] && !node["useLocalModel"]) {
				return false;
			}
			data.api_key = node["api_key"].as<std::string>();
			data.modelPath = node["modelPath"].as<std::string>();
			data.useLocalModel = node["useLocalModel"].as<bool>();
			if (node["model"]) {
				data.model = node["model"].as<std::string>();
			}
			data.proxy = node["proxy"].as<std::string>();
			return true;
		}
	};

	template<>
	struct convert<VitsTask> {
		static Node encode(const VitsTask& task) {
			Node node;
			node["model"] = task.model;
			node["config"] = task.config;
			node["text"] = task.text;
			node["outpath"] = task.outpath;
			node["choice"] = task.choice;
			node["sid"] = task.sid;
			node["escape"] = task.escape;
			return node;
		}

		static bool decode(const Node& node, VitsTask& task) {
			if (!node["model"] || !node["config"] || !node["text"]) {
				return false;
			}
			task.model = node["model"].as<std::string>();
			task.config = node["config"].as<std::string>();
			task.text = node["text"].as<std::string>();
			if (node["choice"]) {
				task.choice = node["choice"].as<char>();
			}
			if (node["outpath"]) {
				task.outpath = node["outpath"].as<std::string>();
			}
			if (node["sid"]) {
				task.sid = node["sid"].as<int>();
			}
			if (node["escape"]) {
				task.escape = node["escape"].as<bool>();
			}
			return true;
		}
	};

	template<>
	struct convert<VITSData> {
		static Node encode(const VITSData& data) {
			Node node;
			node["model"] = data.model;
			node["config"] = data.config;
			node["lanType"] = data.lanType;
			node["enable"] = data.enable;
			return node;
		}

		static bool decode(const Node& node, VITSData& data) {
			if (!node["model"] || !node["config"]) {
				return false;
			}
			data.enable = node["enable"].as<bool>();
			data.model = node["model"].as<std::string>();
			data.config = node["config"].as<std::string>();
			if (node["lanType"]) {
				data.lanType = node["lanType"].as<std::string>();
			}
			return true;
		}
	};

	template<>
	struct convert<WhisperData> {
		static Node encode(const WhisperData& data) {
			Node node;
			node["enable"] = data.enable;
			node["useLocalModel"] = data.useLocalModel;
			node["model"] = data.model;
			return node;
		}

		static bool decode(const Node& node, WhisperData& data) {
			data.useLocalModel = node["useLocalModel"].as<bool>();
			data.enable = node["enable"].as<bool>();
			if (node["model"]) {
				data.model = node["model"].as<std::string>();
			}
			return true;
		}
	};

	template<>
	struct convert<Configure> {
		static Node encode(const Configure& config) {
			Node node;
			node["openAi"] = config.openAi;
			node["baiDuTranslator"] = config.baiDuTranslator;
			node["vits"] = config.vits;
			node["whisper"] = config.whisper;
			return node;
		}

		static bool decode(const Node& node, Configure& config) {
			if (!node["openAi"] || !node["baiDuTranslator"] || !node["vits"]) {
				return false;
			}
			config.openAi = node["openAi"].as<OpenAIData>();
			config.baiDuTranslator = node["baiDuTranslator"].as<TranslateData>();
			config.vits = node["vits"].as<VITSData>();
			config.whisper = node["whisper"].as<WhisperData>();
			return true;
		}
	};
}

#endif