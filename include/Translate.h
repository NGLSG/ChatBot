#ifndef TRANSLATE_H
#define TRANSLATE_H
#include "utils.h"

class Translate {
public :
    Translate(const TranslateData &data);

    std::string translate(std::string originText, std::string destLanguage = "en", std::string originLanguage = "auto");

private:
    TranslateData _data;
    cpr::Session session;
};

#endif