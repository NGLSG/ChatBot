//
// Created by 92703 on 2023/3/19.
//

#include "Translate.h"
#include <cpr/cpr.h>

Translate::Translate(const TranslateData &data) : _data(data) {
    Log::Logger::Init();
}

std::string Translate::translate(std::string originText, std::string destLanguage, std::string originLanguage) {
    std::string Appid = _data.appId;
    std::string Secret = _data.APIKey;
    std::string salt = std::to_string(rand());
    std::string sign = UEncrypt::ToMD5(Appid + originText + salt + Secret);
    std::string url =
            "http://api.fanyi.baidu.com/api/trans/vip/translate?q=" + originText + "&from=" + originLanguage + "&to=" +
            destLanguage + "&appid=" + Appid + "&salt=" + salt + "&sign=" + sign;
    try {
        auto r = cpr::Get(cpr::Url{url});
        if (r.status_code != 200) {
            throw std::runtime_error("HTTP Error: " + std::to_string(r.status_code));
        }
        auto json = nlohmann::json::parse(r.text);
        if (json.find("error_code") != json.end()) {
            throw std::runtime_error("Translation Error: " + json["error_msg"].get<std::string>());
        }
        std::string result;
        for (auto &item: json["trans_result"]) {
            result += item["dst"].get<std::string>() + "\n";
        }
        return result;
    } catch (const std::exception &e) {
        Log::Error("Error: {0}", e.what());
        return "";
    }
}

