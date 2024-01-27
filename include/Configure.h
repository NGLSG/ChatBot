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

struct OpenAIBotCreateInfo {
    bool enable = false;
    bool useLocalModel = false;
    bool useWebProxy = true;
    std::string modelPath = "model/ChatGLM/";
    std::string api_key = "";
    std::string model = "gpt-3.5-turbo";
    std::string proxy = "";
    int webproxy = 0;
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

struct StableDiffusionData {
    std::string apiPath = "http://127.0.0.1:7860";
    std::string sampler_index = "Euler a";
    std::string negative_prompt =
            "lowres, bad anatomy, bad hands, text, error, missing fingers, extra digit, fewer digits, cropped, worst quality, low quality, normal quality, jpeg artifacts, signature, watermark, username, blurry, bad feet";
    int steps = 50;
    int denoising_strength = 0;
    int width = 5112;
    int height = 512;
    float cfg_scale = 7;
};

struct VITSData {
    bool enable = false;
    std::string modelName = "empty";
    std::string model;
    std::string config;
    std::string lanType = "jp";
    int speaker_id = 0;
};

struct WhisperCreateInfo {
    bool enable = false;
    bool useLocalModel = false;
    std::string model = "model/Whisper/ggml-small.bin";
};

struct LConfigure {
    std::string model = "Mao";
    float scaleX = 1.0;
    float scaleY = 1.0;
};

struct ClaudeBotCreateInfo {
    bool enable = true;
    std::string channelID;
    std::string slackToken;
    std::string userName;
    std::string cookies;
};

struct GeminiBotCreateInfo {
    bool enable = true;
    std::string _apiKey;
};


struct Live2D {
    bool enable = false;
    std::string model = "./model/Live2D/Mao";
    std::string bin = "Live2D.exe";
};

struct Configure {
    OpenAIBotCreateInfo openAi;
    TranslateData baiDuTranslator;
    VITSData vits;
    WhisperCreateInfo whisper;
    Live2D live2D;
    ClaudeBotCreateInfo claude;
    GeminiBotCreateInfo gemini;
    StableDiffusionData stableDiffusion;
};

namespace YAML {
    template<>
    struct convert<GeminiBotCreateInfo> {
        static Node encode(const GeminiBotCreateInfo&data) {
            Node node;
            node["enable"] = data.enable;
            node["api_Key"] = data._apiKey;
            return node;
        }

        static bool decode(const Node&node, GeminiBotCreateInfo&data) {
            if (!node["enable"]) {
                return false;
            }
            data._apiKey = node["api_Key"].as<std::string>();
            data.enable = node["enable"].as<bool>();
            return true;
        }
    };

    template<>
    struct convert<ClaudeBotCreateInfo> {
        static Node encode(const ClaudeBotCreateInfo&data) {
            Node node;
            node["enable"] = data.enable;
            node["channelID"] = data.channelID;
            node["userName"] = data.userName;
            node["cookies"] = data.cookies;
            node["slackToken"] = data.slackToken;
            return node;
        }

        static bool decode(const Node&node, ClaudeBotCreateInfo&data) {
            if (!node["channelID"]) {
                return false;
            }
            data.cookies = node["cookies"].as<std::string>();
            data.userName = node["userName"].as<std::string>();
            data.enable = node["enable"].as<bool>();
            data.channelID = node["channelID"].as<std::string>();
            data.slackToken = node["slackToken"].as<std::string>();
            return true;
        }
    };

    template<>
    struct convert<TranslateData> {
        static Node encode(const TranslateData&data) {
            Node node;
            node["appId"] = data.appId;
            node["APIKey"] = data.APIKey;
            node["proxy"] = data.proxy;
            return node;
        }

        static bool decode(const Node&node, TranslateData&data) {
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
    struct convert<OpenAIBotCreateInfo> {
        static Node encode(const OpenAIBotCreateInfo&data) {
            Node node;
            node["enable"] = data.enable;
            node["useLocalModel"] = data.useLocalModel;
            node["modelPath"] = data.modelPath;
            node["api_key"] = data.api_key;
            node["model"] = data.model;
            node["proxy"] = data.proxy;
            node["useWebProxy"] = data.useWebProxy;
            node["webproxy"] = data.webproxy;

            return node;
        }

        static bool decode(const Node&node, OpenAIBotCreateInfo&data) {
            if (!node["api_key"] && !node["useLocalModel"]) {
                return false;
            }
            data.enable = node["enable"].as<bool>();
            data.api_key = node["api_key"].as<std::string>();
            data.modelPath = node["modelPath"].as<std::string>();
            data.useLocalModel = node["useLocalModel"].as<bool>();
            data.useWebProxy = node["useWebProxy"].as<bool>();
            if (node["model"]) {
                data.model = node["model"].as<std::string>();
            }
            data.proxy = node["proxy"].as<std::string>();
            data.webproxy = node["webproxy"].as<int>();
            return true;
        }
    };

    template<>
    struct convert<VitsTask> {
        static Node encode(const VitsTask&task) {
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

        static bool decode(const Node&node, VitsTask&task) {
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
        static Node encode(const VITSData&data) {
            Node node;
            node["modelName"] = data.modelName;
            node["model"] = data.model;
            node["config"] = data.config;
            node["lanType"] = data.lanType;
            node["enable"] = data.enable;
            node["speaker_id"] = data.speaker_id;
            return node;
        }

        static bool decode(const Node&node, VITSData&data) {
            if (!node["model"] || !node["config"]) {
                return false;
            }
            data.enable = node["enable"].as<bool>();
            data.model = node["model"].as<std::string>();
            data.modelName = node["modelName"].as<std::string>();
            data.config = node["config"].as<std::string>();
            data.speaker_id = node["speaker_id"].as<int>();
            if (node["lanType"]) {
                data.lanType = node["lanType"].as<std::string>();
            }
            return true;
        }
    };

    template<>
    struct convert<WhisperCreateInfo> {
        static Node encode(const WhisperCreateInfo&data) {
            Node node;
            node["enable"] = data.enable;
            node["useLocalModel"] = data.useLocalModel;
            node["model"] = data.model;
            return node;
        }

        static bool decode(const Node&node, WhisperCreateInfo&data) {
            data.useLocalModel = node["useLocalModel"].as<bool>();
            data.enable = node["enable"].as<bool>();
            if (node["model"]) {
                data.model = node["model"].as<std::string>();
            }
            return true;
        }
    };

    template<>
    struct convert<Live2D> {
        static Node encode(const Live2D&data) {
            Node node;
            node["enable"] = data.enable;
            node["model"] = data.model;
            node["bin"] = data.bin;
            return node;
        }

        static bool decode(const Node&node, Live2D&data) {
            data.enable = node["enable"].as<bool>();
            if (node["model"]) {
                data.model = node["model"].as<std::string>();
            }
            data.bin = node["bin"].as<std::string>();
            return true;
        }
    };

    template<>
    struct convert<LConfigure> {
        static Node encode(const LConfigure&data) {
            Node node;
            node["model"] = data.model;
            node["scaleX"] = data.scaleX;
            node["scaleY"] = data.scaleY;
            return node;
        }

        static bool decode(const Node&node, LConfigure&data) {
            if (node["model"]) {
                data.model = node["model"].as<std::string>();
            }
            data.scaleX = node["scaleX"].as<float>();
            data.scaleY = node["scaleY"].as<float>();
            return true;
        }
    };

    template<>
    struct convert<StableDiffusionData> {
        static Node encode(const StableDiffusionData&data) {
            Node node;
            node["apiPath"] = data.apiPath;
            node["sampler_index"] = data.sampler_index;
            node["negative_prompt"] = data.negative_prompt;
            node["steps"] = data.steps;
            node["denoising_strength"] = data.denoising_strength;
            node["width"] = data.width;
            node["height"] = data.height;
            node["cfg_scale"] = data.cfg_scale;
            return node;
        }

        static bool decode(const Node&node, StableDiffusionData&data) {
            data.apiPath = node["apiPath"].as<std::string>();
            data.sampler_index = node["sampler_index"].as<std::string>();
            data.negative_prompt = node["negative_prompt"].as<std::string>();
            data.steps = node["steps"].as<int>();
            data.width = node["width"].as<int>();
            data.height = node["height"].as<int>();
            data.denoising_strength = node["denoising_strength"].as<int>();
            data.cfg_scale = node["cfg_scale"].as<float>();
            return true;
        }
    };

    template<>
    struct convert<Configure> {
        static Node encode(const Configure&config) {
            Node node;
            node["openAi"] = config.openAi;
            node["claude"] = config.claude;
            node["gemini"] = config.gemini;
            node["baiDuTranslator"] = config.baiDuTranslator;
            node["vits"] = config.vits;
            node["whisper"] = config.whisper;
            node["live2D"] = config.live2D;
            node["stableDiffusion"] = config.stableDiffusion;
            return node;
        }

        static bool decode(const Node&node, Configure&config) {
            if (!node["openAi"] || !node["baiDuTranslator"] || !node["vits"]) {
                return false;
            }

            config.openAi = node["openAi"].as<OpenAIBotCreateInfo>();
            config.gemini = node["gemini"].as<GeminiBotCreateInfo>();
            config.claude = node["claude"].as<ClaudeBotCreateInfo>();
            config.baiDuTranslator = node["baiDuTranslator"].as<TranslateData>();
            config.vits = node["vits"].as<VITSData>();
            config.whisper = node["whisper"].as<WhisperCreateInfo>();
            config.live2D = node["live2D"].as<Live2D>();
            config.stableDiffusion = node["stableDiffusion"].as<StableDiffusionData>();
            return true;
        }
    };
}

#endif
