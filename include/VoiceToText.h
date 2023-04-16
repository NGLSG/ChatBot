#pragma once

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
