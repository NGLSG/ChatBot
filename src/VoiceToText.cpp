#include "VoiceToText.h"
#include "vcruntime_exception.h"

VoiceToText::VoiceToText(const OpenAIBotCreateInfo&voiceData) : _voiceData(voiceData) {
}

json VoiceToText::sendRequest(std::string data) {
    try {
        json parsed_response;
        std::string url = "";
        if (!_voiceData.useWebProxy) {
            url = "https://api.openai.com/";
            if (!_voiceData.proxy.empty()) {
                session.SetProxies(cpr::Proxies{
                    {"http", _voiceData.proxy},
                    {"https", _voiceData.proxy}
                });
            }
        }
        else {
            url = _voiceData._endPoint;
        }
        session.SetUrl(cpr::Url{url + "v1/audio/transcriptions"});
        session.SetHeader(cpr::Header{
            {"Authorization", "Bearer " + _voiceData.api_key},
            {"Content-Type", "multipart/form-data"}
        });
        session.SetMultipart(cpr::Multipart{
            {"file", cpr::File{data}},
            {"model", "whisper-1"}
        });
        session.SetVerifySsl(cpr::VerifySsl{false});
        cpr::Response response = session.Post();
        session.SetProxies(cpr::Proxies());
        if (response.status_code != 200) {
            LogError("Whisper Error: Request failed with status code " + std::to_string(response.status_code) +
                ". Because " + response.reason);
            return {};
        }
        parsed_response = json::parse(response.text);
        return parsed_response;
    }
    catch (std::exception&e) {
        LogError(e.what());
    }
}

std::string VoiceToText::Convert(std::string voicePath) {
    try {
        json response = sendRequest(voicePath);

        if (response.is_null()) {
            LogError("Whisper Error: Response is null.");
            return "";
        }

        std::string res = response["text"];
        return res;
    }
    catch (std::exception&e) {
        LogError(e.what());
    }
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
        }
        else {
            res = response["text"];
            promise.set_value(res);
        }
    });

    return future;
}
