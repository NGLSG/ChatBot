#ifndef VOICETOTEXT_H
#define VOICETOTEXT_H

#include "utils.h"

using json = nlohmann::json;


class VoiceToText {
public:
    VoiceToText(const OpenAIData &voiceData);

    std::string Convert(std::string voicePath);

    json sendRequest(std::string data);

    std::future<std::string> ConvertAsync(std::string voicePath);

private:

    cpr::Session session;
    OpenAIData _voiceData;
};

#endif