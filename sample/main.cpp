#include "Application.h"
#include "StableDiffusion.h"

using namespace std;

int main(int argc, char* argv[]) {
    try {
        std::locale::global(std::locale(""));
        if (!UDirectory::Exists("Logs")) {
            UDirectory::Create("Logs");
        }

        bool setting = false;
        Logger::Init();
        Utils::SaveYaml("template.yaml", Utils::toYaml(Configure()));
        if (!UFile::Exists("config.yaml")) {
            LogWarn("Application Warning: Please configure config.yaml! Then run this program again");
            setting = true;
        }
        auto configure = Utils::LoadYaml<Configure>("config.yaml");
        Application app(configure, setting);
        app.Renderer();
    }
    catch (const std::exception&e) {
        LogError(e.what());
    }

    return 0;
}
