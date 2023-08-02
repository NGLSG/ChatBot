#include "StableDiffusion.h"

StableDiffusion::StableDiffusion(StableDiffusionData data) : _data(data) {
    if (!UDirectory::Exists(ResPath)) {
        UDirectory::Create(ResPath);
    }
}

std::string StableDiffusion::Text2Img(std::string prompt) {
    cpr::Session session;
    session.SetUrl(cpr::Url{_data.apiPath+"/sdapi/v1/txt2img"});
    json body = {
            {"prompt",             prompt},
            {"sampler_index",      _data.sampler_index},
            {"negative_prompt",    _data.negative_prompt},
            {"steps",              _data.steps},
            {"width",              _data.width},
            {"height",             _data.height},
            {"cfg_scale",          _data.cfg_scale}
    };
    std::string json_str = body.dump();

    // Create the cpr::Body
    cpr::Body body_payload{json_str};
    session.SetBody(body_payload);
    session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
    session.SetVerifySsl(cpr::VerifySsl{false});
    cpr::Response response = session.Post();
    if (response.status_code != 200) {
        LogError("StableDiffusion Error: Request failed with status code " +
                 std::to_string(response.status_code) +
                 ". Because " + response.reason);
        return 0;
    }
    json parsed_response = json::parse(response.text);
    std::vector<std::string> images = parsed_response["images"].get<std::vector<std::string>>();
    std::string image = images[0];

    float x = _data.width / _maxSize;
    float y = _data.height / _maxSize;
    std::string uid = std::to_string(Utils::GetCurrentTimestamp()) + ".png";
    std::string path = ResPath + uid;
    if (x > 1) {
        UImage::ImgResize(image, 1 / x, path);
    } else if (y > 1) {
        UImage::ImgResize(image, 1 / y, path);
    } else {
        UImage::Base64ToImage(image, path);
    }

    return uid;
}
