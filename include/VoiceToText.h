#ifndef VOICETOTEXT_H
#define VOICETOTEXT_H

#include "utils.h"

using json = nlohmann::json;


class VoiceToText {
public:
    VoiceToText(const OpenAIBotCreateInfo &voiceData);

    std::string Convert(std::string voicePath);

    json sendRequest(std::string data);

    std::future<std::string> ConvertAsync(std::string voicePath);

private:
    const std::vector<std::string> WebProxies{"https://nglsg.ml/",
                                              "https://service-hbv9ql2m-1306800451.sg.apigw.tencentcs.com/"};
    cpr::Session session;
    OpenAIBotCreateInfo _voiceData;
};

#endif