#include "ChatBot.h"

static inline void TrimLeading(std::string& s)
{
    size_t pos = 0;
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])))
        pos++;
    if (pos > 0)
        s.erase(0, pos);
}