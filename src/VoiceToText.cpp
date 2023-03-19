//
// Created by 92703 on 2023/3/19.
//

#include "VoiceToText.h"

VoiceToText::VoiceToText(const OpenAIData &voiceData) : _voiceData(voiceData) {

}

json VoiceToText::sendRequest(std::string data) {
    json parsed_response;
    if (!_voiceData.proxy.proxy.empty()) {
        cpr::Proxies proxies = {
                {"http",  _voiceData.proxy.proxy},
                {"https", _voiceData.proxy.proxy}

        };
        cpr::Response response = cpr::Post(cpr::Url{"https://api.openai.com/v1/audio/transcriptions"},
                                           cpr::Bearer{_voiceData.api_key},
                                           cpr::Header{{"Content-Type", "multipart/form-data"}},
                                           cpr::Multipart{{"file",  cpr::File{data}},
                                                          {"model", "whisper-1"}},
                                           proxies);
        if (response.status_code != 200) {
            Log::Error("Error: Request failed with status code " + std::to_string(response.status_code));
            parsed_response = {};
        }

        parsed_response = json::parse(response.text);
    } else {
        cpr::Response response = cpr::Post(cpr::Url{"https://api.openai.com/v1/audio/transcriptions"},
                                           cpr::Bearer{_voiceData.api_key},
                                           cpr::Header{{"Content-Type", "multipart/form-data"}},
                                           cpr::Multipart{{"file",  cpr::File{data}},
                                                          {"model", "whisper-1"}});
        if (response.status_code != 200) {
            Log::Error("Error: Request failed with status code " + std::to_string(response.status_code));
            parsed_response = {};
        }

        parsed_response = json::parse(response.text);
    }
    return parsed_response;
}

std::string VoiceToText::Convert(std::string voicePath) {
    json response = sendRequest(voicePath);

    if (response.is_null()) {
        Log::Error("Error: Response is null.");
        return "";
    }

    std::string res = response["text"];
    return res;
}


