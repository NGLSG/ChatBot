//
// Created by 92703 on 2023/3/19.
//

#ifndef CHATBOT_VOICETOTEXT_H
#define CHATBOT_VOICETOTEXT_H


#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include "utils.h"

using json = nlohmann::json;


class VoiceToText {
public:
    VoiceToText(const OpenAIData &voiceData);

    std::string Convert(std::string voicePath);

    json sendRequest(std::string data);

private:
    OpenAIData _voiceData;
};


#endif //CHATBOT_VOICETOTEXT_H
