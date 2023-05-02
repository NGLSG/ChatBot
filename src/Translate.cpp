//
// Created by 92703 on 2023/3/19.
//

#include "Translate.h"
#include <cpr/cpr.h>

Translate::Translate(const TranslateData &data) : _data(data) {
    Logger::Init();
}

std::string Translate::translate(std::string originText, std::string destLanguage, std::string originLanguage) {
    std::string Appid = _data.appId;
    std::string Secret = _data.APIKey;
    std::string salt = std::to_string(time(nullptr));
    std::string sign = UEncrypt::ToMD5(Appid + originText + salt + Secret);
    std::string url = "http://api.fanyi.baidu.com/api/trans/vip/translate";

    cpr::Proxies proxies;
    session.SetProxies(cpr::Proxies());
    if (!_data.proxy.empty()) {
        proxies = {
                {"http",  _data.proxy},
                {"https", _data.proxy}

        };
        session.SetProxies(proxies);
    }

    try {
        std::string quest = cpr::util::urlEncode(originText);
        std::string data =
                "q=" + quest + "&from=" + originLanguage + "&to=" + destLanguage + "&appid=" + Appid + "&salt=" +
                salt + "&sign=" + sign;
        cpr::Body body(data);
        session.SetUrl(cpr::Url{url});
        session.SetBody(body);
        session.SetHeader(cpr::Header{{"Content-Type", "application/x-www-form-urlencoded"}});
        session.SetVerifySsl(cpr::VerifySsl{false});
        // 发送HTTP请求
        cpr::Response response = session.Post();

        session.SetProxies(cpr::Proxies());

        if (response.status_code != 200) {
            LogError("Translation HTTP Error: {0}", std::to_string(response.status_code));
            LogError("Translation Request Error: {0}", Utils::GetErrorString(response.error.code));
        }
        auto json = nlohmann::json::parse(response.text);

        if (json.find("error_code") != json.end()) {
            LogError("Translation Error: {0}", json["error_msg"].get<std::string>());
        }
        std::string result;
        for (auto &item: json["trans_result"]) {
            result += item["dst"].get<std::string>() + "\n";
        }
        return result;
    } catch (const std::exception &e) {
        LogError("Translation Error: {0}", e.what());
        return "";
    }
}

