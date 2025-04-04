#ifndef CONFIGURE_H
#define CONFIGURE_H

#include <yaml-cpp/yaml.h>
#include <string>
#include <iostream>

struct TranslateData
{
    bool enable = false;
    std::string appId;
    std::string APIKey;
    std::string proxy = "";
};

struct LLamaCreateInfo
{
    std::string model = "";
    int contextSize = 32000; //32k
    int maxTokens = 4096;
};

struct ClaudeAPICreateInfo
{
    bool enable = false;
    std::string apiKey;
    std::string model = "claude-3.5";
    std::string apiVersion = "2023-06-01";
    std::string _endPoint = "https://api.anthropic.com/v1/complete";
};

struct OpenAIBotCreateInfo
{
    bool enable = true;
    bool useLocalModel = false;
    bool useWebProxy = false;
    std::string modelPath = "model/ChatGLM/";
    std::string api_key = "";
    std::string model = "gpt-4o";
    std::string proxy = "";
    std::string _endPoint = "";
};

struct GPTLikeCreateInfo
{
    bool enable;
    bool useLocalModel = false;
    std::string api_key;
    std::string model = ""; //Must be set
    std::string apiHost = "";
    std::string apiPath = "";
    LLamaCreateInfo llamaData;

    GPTLikeCreateInfo()
    {
        enable = false;
    }


    GPTLikeCreateInfo& operator=(const GPTLikeCreateInfo& data)
    {
        this->enable = data.enable;
        this->api_key = data.api_key;
        this->model = data.model;
        this->apiHost = data.apiHost;
        this->apiPath = data.apiPath;
        this->llamaData = data.llamaData;
        this->useLocalModel = data.useLocalModel;

        return *this;
    }
};

struct VitsTask
{
    std::string model;
    std::string config;
    std::string text;
    std::string outpath = "tmp.wav";
    char choice = 't';
    int sid = 0;
    bool escape = false;
};

struct StableDiffusionData
{
    std::string apiPath = "http://127.0.0.1:7860";
    std::string sampler_index = "Euler a";
    std::string negative_prompt =
        "lowres, bad anatomy, bad hands, text, error, missing fingers, extra digit, fewer digits, cropped, worst quality, low quality, normal quality, jpeg artifacts, signature, watermark, username, blurry, bad feet";
    int steps = 50;
    int denoising_strength = 0;
    int width = 512;
    int height = 512;
    float cfg_scale = 7;
};

struct VITSData
{
    bool enable = false;
    bool UseGptSovite = false;
    std::string apiEndPoint = "http://127.0.0.1:9880";
    std::string modelName = "empty";
    std::string model;
    std::string config;
    std::string lanType = "jp";
    int speaker_id = 0;
};

struct WhisperCreateInfo
{
    bool enable = false;
    bool useLocalModel = false;
    std::string model = "model/Whisper/ggml-small.bin";
};

struct LConfigure
{
    std::string model = "Mao";
    float scaleX = 1.0;
    float scaleY = 1.0;
};

struct ClaudeBotCreateInfo
{
    bool enable = false;
    std::string channelID;
    std::string slackToken;
    std::string userName;
    std::string cookies;
};

struct GeminiBotCreateInfo
{
    bool enable = false;
    std::string _apiKey;
    std::string _endPoint;
    std::string model = "gemini-2.0-flash";
};

struct Live2D
{
    bool enable = false;
    std::string model = "./model/Live2D/Mao";
    std::string bin = "bin/Live2D/DesktopPet.exe";
};

struct Configure
{
    std::string MicroPhoneID = "default";
    std::string OutDeviceID = "default";
    OpenAIBotCreateInfo openAi;
    TranslateData baiDuTranslator;
    VITSData vits;
    WhisperCreateInfo whisper;
    Live2D live2D;
    ClaudeBotCreateInfo claude;
    GeminiBotCreateInfo gemini;
    GPTLikeCreateInfo grok;
    GPTLikeCreateInfo mistral;
    GPTLikeCreateInfo qianwen;
    GPTLikeCreateInfo sparkdesk;
    GPTLikeCreateInfo chatglm;
    GPTLikeCreateInfo hunyuan;
    GPTLikeCreateInfo baichuan;
    GPTLikeCreateInfo huoshan;
    ClaudeAPICreateInfo claudeAPI;
    std::unordered_map<std::string, GPTLikeCreateInfo> customGPTs;
    StableDiffusionData stableDiffusion;

};

namespace YAML
{
    template <>
    struct convert<ClaudeAPICreateInfo>
    {
        static Node encode(const ClaudeAPICreateInfo& data)
        {
            Node node;
            node["enable"] = data.enable;
            node["apiKey"] = data.apiKey;
            node["model"] = data.model;
            node["apiVersion"] = data.apiVersion;
            node["endPoint"] = data._endPoint;
            return node;
        }

        static bool decode(const Node& node, ClaudeAPICreateInfo& data)
        {
            data.enable = node["enable"].as<bool>();
            data.apiKey = node["apiKey"].as<std::string>();
            data.model = node["model"].as<std::string>();
            data.apiVersion = node["apiVersion"].as<std::string>();
            data._endPoint = node["endPoint"].as<std::string>();
            return true;
        }
    };

    template <>
    struct convert<LLamaCreateInfo>
    {
        static Node encode(const LLamaCreateInfo& data)
        {
            Node node;
            node["model"] = data.model;
            node["contextSize"] = data.contextSize;
            node["maxTokens"] = data.maxTokens;
            return node;
        }

        static bool decode(const Node& node, LLamaCreateInfo& data)
        {
            data.model = node["model"].as<std::string>();
            data.contextSize = node["contextSize"].as<int>();
            data.maxTokens = node["maxTokens"].as<int>();
            return true;
        }
    };

    template <>
    struct convert<GPTLikeCreateInfo>
    {
        static Node encode(const GPTLikeCreateInfo& data)
        {
            Node node;
            node["enable"] = data.enable;
            node["api_key"] = data.api_key;
            node["model"] = data.model;
            node["apiHost"] = data.apiHost;
            node["apiPath"] = data.apiPath;
            node["useLocalModel"] = data.useLocalModel;
            node["llamaData"] = data.llamaData;
            return node;
        }

        static bool decode(const Node& node, GPTLikeCreateInfo& data)
        {
            data.enable = node["enable"].as<bool>();
            data.api_key = node["api_key"].as<std::string>();
            data.model = node["model"].as<std::string>();
            if (node["apiHost"])
                data.apiHost = node["apiHost"].as<std::string>();
            if (node["apiPath"])
                data.apiPath = node["apiPath"].as<std::string>();
            if (node["useLocalModel"])
                data.useLocalModel = node["useLocalModel"].as<bool>();
            if (node["llamaData"])
                data.llamaData = node["llamaData"].as<LLamaCreateInfo>();
            return true;
        }
    };

    template <>
    struct convert<GeminiBotCreateInfo>
    {
        static Node encode(const GeminiBotCreateInfo& data)
        {
            Node node;
            node["enable"] = data.enable;
            node["api_Key"] = data._apiKey;
            node["endPoint"] = data._endPoint;
            node["model"] = data.model;
            return node;
        }

        static bool decode(const Node& node, GeminiBotCreateInfo& data)
        {
            data._apiKey = node["api_Key"].as<std::string>();
            data.enable = node["enable"].as<bool>();
            data._endPoint = node["endPoint"].as<std::string>();
            if (node["model"])
            {
                data.model = node["model"].as<std::string>();
            }
            else
            {
                data.model = "gemini-2.0-flash";
            }
            return true;
        }
    };

    template <>
    struct convert<ClaudeBotCreateInfo>
    {
        static Node encode(const ClaudeBotCreateInfo& data)
        {
            Node node;
            node["enable"] = data.enable;
            node["channelID"] = data.channelID;
            node["userName"] = data.userName;
            node["cookies"] = data.cookies;
            node["slackToken"] = data.slackToken;
            return node;
        }

        static bool decode(const Node& node, ClaudeBotCreateInfo& data)
        {
            if (!node["channelID"])
            {
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

    template <>
    struct convert<TranslateData>
    {
        static Node encode(const TranslateData& data)
        {
            Node node;
            node["appId"] = data.appId;
            node["APIKey"] = data.APIKey;
            node["proxy"] = data.proxy;
            node["enable"] = data.enable;
            return node;
        }

        static bool decode(const Node& node, TranslateData& data)
        {
            data.proxy = node["proxy"].as<std::string>();
            data.appId = node["appId"].as<std::string>();
            data.APIKey = node["APIKey"].as<std::string>();
            if (node["enable"])
            {
                data.enable = node["enable"].as<bool>();
            }
            else
            {
                data.enable = false;
            }
            return true;
        }
    };

    template <>
    struct convert<OpenAIBotCreateInfo>
    {
        static Node encode(const OpenAIBotCreateInfo& data)
        {
            Node node;
            node["enable"] = data.enable;
            node["useLocalModel"] = data.useLocalModel;
            node["modelPath"] = data.modelPath;
            node["api_key"] = data.api_key;
            node["model"] = data.model;
            node["proxy"] = data.proxy;
            node["useWebProxy"] = data.useWebProxy;
            node["endPoint"] = data._endPoint;

            return node;
        }

        static bool decode(const Node& node, OpenAIBotCreateInfo& data)
        {
            data.enable = node["enable"].as<bool>();
            data.api_key = node["api_key"].as<std::string>();
            data.modelPath = node["modelPath"].as<std::string>();
            data.useLocalModel = node["useLocalModel"].as<bool>();
            data.useWebProxy = node["useWebProxy"].as<bool>();
            if (node["model"])
            {
                data.model = node["model"].as<std::string>();
            }
            data.proxy = node["proxy"].as<std::string>();
            data._endPoint = node["endPoint"].as<std::string>();
            return true;
        }
    };

    template <>
    struct convert<VitsTask>
    {
        static Node encode(const VitsTask& task)
        {
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

        static bool decode(const Node& node, VitsTask& task)
        {
            if (!node["model"] || !node["config"] || !node["text"])
            {
                return false;
            }
            task.model = node["model"].as<std::string>();
            task.config = node["config"].as<std::string>();
            task.text = node["text"].as<std::string>();
            if (node["choice"])
            {
                task.choice = node["choice"].as<char>();
            }
            if (node["outpath"])
            {
                task.outpath = node["outpath"].as<std::string>();
            }
            if (node["sid"])
            {
                task.sid = node["sid"].as<int>();
            }
            if (node["escape"])
            {
                task.escape = node["escape"].as<bool>();
            }
            return true;
        }
    };

    template <>
    struct convert<VITSData>
    {
        static Node encode(const VITSData& data)
        {
            Node node;
            node["modelName"] = data.modelName;
            node["model"] = data.model;
            node["config"] = data.config;
            node["lanType"] = data.lanType;
            node["enable"] = data.enable;
            node["speaker_id"] = data.speaker_id;
            node["useGptSoVits"] = data.UseGptSovite;
            node["apiEndPoint"] = data.apiEndPoint;
            return node;
        }

        static bool decode(const Node& node, VITSData& data)
        {
            if (!node["model"] || !node["config"])
            {
                return false;
            }
            data.enable = node["enable"].as<bool>();
            data.model = node["model"].as<std::string>();
            data.modelName = node["modelName"].as<std::string>();
            data.config = node["config"].as<std::string>();
            data.speaker_id = node["speaker_id"].as<int>();
            if (node["lanType"])
            {
                data.lanType = node["lanType"].as<std::string>();
            }
            if (node["useGptSoVits"])
                data.UseGptSovite = node["useGptSoVits"].as<bool>();
            if (node["apiEndPoint"])
                data.apiEndPoint = node["apiEndPoint"].as<std::string>();
            return true;
        }
    };

    template <>
    struct convert<WhisperCreateInfo>
    {
        static Node encode(const WhisperCreateInfo& data)
        {
            Node node;
            node["enable"] = data.enable;
            node["useLocalModel"] = data.useLocalModel;
            node["model"] = data.model;
            return node;
        }

        static bool decode(const Node& node, WhisperCreateInfo& data)
        {
            data.useLocalModel = node["useLocalModel"].as<bool>();
            data.enable = node["enable"].as<bool>();
            if (node["model"])
            {
                data.model = node["model"].as<std::string>();
            }
            return true;
        }
    };

    template <>
    struct convert<Live2D>
    {
        static Node encode(const Live2D& data)
        {
            Node node;
            node["enable"] = data.enable;
            node["model"] = data.model;
            node["bin"] = data.bin;
            return node;
        }

        static bool decode(const Node& node, Live2D& data)
        {
            data.enable = node["enable"].as<bool>();
            if (node["model"])
            {
                data.model = node["model"].as<std::string>();
            }
            data.bin = node["bin"].as<std::string>();
            return true;
        }
    };

    template <>
    struct convert<LConfigure>
    {
        static Node encode(const LConfigure& data)
        {
            Node node;
            node["model"] = data.model;
            node["scaleX"] = data.scaleX;
            node["scaleY"] = data.scaleY;
            return node;
        }

        static bool decode(const Node& node, LConfigure& data)
        {
            if (node["model"])
            {
                data.model = node["model"].as<std::string>();
            }
            data.scaleX = node["scaleX"].as<float>();
            data.scaleY = node["scaleY"].as<float>();
            return true;
        }
    };

    template <>
    struct convert<StableDiffusionData>
    {
        static Node encode(const StableDiffusionData& data)
        {
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

        static bool decode(const Node& node, StableDiffusionData& data)
        {
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

    template <>
    struct convert<Configure>
    {
        static Node encode(const Configure& config)
        {
            Node node;
            node["MicroPhoneID"] = config.MicroPhoneID;
            node["OutDeviceID"] = config.OutDeviceID;
            node["openAi"] = config.openAi;
            node["claude"] = config.claude;
            node["gemini"] = config.gemini;
            node["baiDuTranslator"] = config.baiDuTranslator;
            node["vits"] = config.vits;
            node["whisper"] = config.whisper;
            node["live2D"] = config.live2D;
            node["stableDiffusion"] = config.stableDiffusion;
            node["grok"] = config.grok;
            node["mistral"] = config.mistral;
            node["qwen"] = config.qianwen;
            node["chatglm"] = config.chatglm;
            node["hunyuan"] = config.hunyuan;
            node["baichuan"] = config.baichuan;
            node["sparkdesk"] = config.sparkdesk;
            node["huoshan"] = config.huoshan;
            node["claudeAPI"] = config.claudeAPI;
            node["customGPTs"] = config.customGPTs;
            return node;
        }

        static bool decode(const Node& node, Configure& config)
        {
            if (!node["openAi"] || !node["baiDuTranslator"] || !node["vits"])
            {
                return false;
            }
            if (node["MicroPhoneID"])
            {
                config.MicroPhoneID = node["MicroPhoneID"].as<std::string>();
            }
            if (node["OutDeviceID"])
            {
                config.OutDeviceID = node["OutDeviceID"].as<std::string>();
            }
            if (node["claudeAPI"])
            {
                config.claudeAPI = node["claudeAPI"].as<ClaudeAPICreateInfo>();
            }
            if (node["mistral"])
            {
                config.mistral = node["mistral"].as<GPTLikeCreateInfo>();
            }
            if (node["qwen"])
            {
                config.qianwen = node["qwen"].as<GPTLikeCreateInfo>();
            }
            if (node["sparkdesk"])
            {
                config.sparkdesk = node["sparkdesk"].as<GPTLikeCreateInfo>();
            }
            if (node["chatglm"])
            {
                config.chatglm = node["chatglm"].as<GPTLikeCreateInfo>();
            }
            if (node["hunyuan"])
            {
                config.hunyuan = node["hunyuan"].as<GPTLikeCreateInfo>();
            }
            if (node["baichuan"])
            {
                config.baichuan = node["baichuan"].as<GPTLikeCreateInfo>();
            }
            if (node["huoshan"])
            {
                config.huoshan = node["huoshan"].as<GPTLikeCreateInfo>();
            }
            config.openAi = node["openAi"].as<OpenAIBotCreateInfo>();
            config.gemini = node["gemini"].as<GeminiBotCreateInfo>();
            config.claude = node["claude"].as<ClaudeBotCreateInfo>();
            if (node["grok"])
                config.grok = node["grok"].as<GPTLikeCreateInfo>();
            if (node["customGPTs"])
                config.customGPTs = node["customGPTs"].as<std::unordered_map<std::string, GPTLikeCreateInfo>>();
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
