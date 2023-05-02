#include "Application.h"

using namespace std;

int main() {
    try {
        Logger::Init();
        if (!UFile::Exists("config.yaml")) {
            Utils::SaveYaml("config.yaml", Utils::toYaml(Configure()));
            LogError("Application Error: Please configure config.yaml! Then run this program again");
            return 0;
        }
        auto configure = Utils::LoadYaml<Configure>("config.yaml");

        Application app(configure);
        app.Renderer();
    }
    catch (const std::exception &e) {
        LogError(e.what());
    }

    system("pause");
    return 0;
}

