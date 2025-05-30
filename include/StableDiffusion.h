#ifndef STABLEDIFFUSION_H
#define STABLEDIFFUSION_H

#include "utils.h"

class StableDiffusion {
public :
    const std::string ResPath = "Resources/Images/";

    StableDiffusion(StableDiffusionData data);

    std::string Text2Img(std::string prompt, std::string negative_prompt="");
StableDiffusionData _data;
private:

    const int _maxSize = 256;
    std::string t2iurl;
};


#endif //STABLEDIFFUSION_H
