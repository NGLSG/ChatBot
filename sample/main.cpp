#include "Application.h"

using namespace std;

int main(int argc, char *argv[]) {
    try {
        if (!UDirectory::Exists("Logs")) {
            UDirectory::Create("Logs");
        }
        bool setting = false;
        Logger::Init();
        if (!UFile::Exists("config.yaml")) {
            Utils::SaveYaml("config.yaml", Utils::toYaml(Configure()));
            LogWarn("Application Warning: Please configure config.yaml! Then run this program again");
            setting = true;
        }

        auto configure = Utils::LoadYaml<Configure>("config.yaml");

        Application app(configure, setting);
        app.Renderer();
    }
    catch (const std::exception &e) {
        LogError(e.what());
    }

    return 0;
}