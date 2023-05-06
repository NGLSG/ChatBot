#include "VoiceToText.h"

VoiceToText::VoiceToText(const OpenAIData &voiceData) : _voiceData(voiceData) {

}

json VoiceToText::sendRequest(std::string data) {
    json parsed_response;
    if (!_voiceData.proxy.empty()) {
        cpr::Proxies proxies = {
                {"http",  _voiceData.proxy},
                {"https", _voiceData.proxy}

        };
        session.SetUrl(cpr::Url{"https://api.openai.com/v1/audio/transcriptions"});
        session.SetHeader(cpr::Header{{"Authorization", "Bearer " + _voiceData.api_key},
                                      {"Content-Type",  "multipart/form-data"}});
        session.SetMultipart(cpr::Multipart{{"file",  cpr::File{data}},
                                            {"model", "whisper-1"}});
        session.SetProxies(proxies);
        session.SetVerifySsl(cpr::VerifySsl{false});
        // 发送HTTP请求
        cpr::Response response = session.Post();

        // 清除代理设置
        session.SetProxies(cpr::Proxies());

        if (response.status_code != 200) {
            LogError("Whisper Error: Request failed with status code " + std::to_string(response.status_code) +
                     ". Because " +
                     response.reason);
            parsed_response = {};
        }

        parsed_response = json::parse(response.text);
    } else {
        session.SetUrl(cpr::Url{"https://api.openai.com/v1/audio/transcriptions"});
        session.SetHeader(cpr::Header{{"Authorization", "Bearer " + _voiceData.api_key},
                                      {"Content-Type",  "multipart/form-data"}});
        session.SetMultipart(cpr::Multipart{{"file",  cpr::File{data}},
                                            {"model", "whisperData-1"}});
        session.SetVerifySsl(cpr::VerifySsl{false});
        // 发送HTTP请求
        cpr::Response response = session.Post();
        // 清除代理设置
        session.SetProxies(cpr::Proxies());
        if (response.status_code != 200) {
            LogError("Whisper Error: Request failed with status code " + std::to_string(response.status_code) +
                     ". Because " +
                     response.reason);
            parsed_response = {};
        }

        parsed_response = json::parse(response.text);
    }
    return parsed_response;
}

std::string VoiceToText::Convert(std::string voicePath) {
    json response = sendRequest(voicePath);

    if (response.is_null()) {
        LogError("Whisper Error: Response is null.");
        return "";
    }

    std::string res = response["text"];
    return res;
}

std::future<std::string> VoiceToText::ConvertAsync(std::string voicePath) {
    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::async(std::launch::async, [this, voicePath, promise = std::move(promise)]() mutable {
        json response = sendRequest(voicePath);
        std::string res;
        if (response.is_null()) {
            LogError("Whisper Error: Response is null.");
            promise.set_value("");
        } else {
            res = response["text"];
            promise.set_value(res);
        }
    });

    return future;
}
