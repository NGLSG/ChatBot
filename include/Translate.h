#pragma once

#include "cpr/cpr.h"
#include "utils.h"

class Translate {
public :
    Translate(const TranslateData &data);

    std::string translate(std::string originText, std::string destLanguage = "en", std::string originLanguage = "auto");

private:
    TranslateData _data;

};

