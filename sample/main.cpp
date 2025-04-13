#include "Application.h"
#include "StableDiffusion.h"
#include "AES_Crypto.h"

using namespace std;
namespace fs = std::filesystem;

#ifdef _WIN32
#include <Windows.h>

// 设置DLL搜索路径函数
bool SetupDllSearchPaths()
{
    // 获取程序的完整路径
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0)
    {
        // 获取失败时记录错误
        LogError("无法获取程序路径");
        return false;
    }

    // 转换为文件系统路径
    fs::path programPath(exePath);
    // 获取程序所在目录
    fs::path programDir = programPath.parent_path();
    // 构建bin目录路径
    fs::path binPath = programDir / "bin";

    // 检查bin目录是否存在
    bool binExists = fs::exists(binPath) && fs::is_directory(binPath);

    // 尝试设置DLL搜索目录
    if (binExists)
    {
        // 设置DLL目录为bin目录
        if (SetDllDirectoryA(binPath.string().c_str()))
        {
            LogInfo("已添加DLL搜索路径: " + binPath.string());
        }
        else
        {
            LogWarn("设置DLL目录失败: " + binPath.string());
        }
    }

    // 尝试使用AddDllDirectory API (Windows 8及更高版本)
    try
    {
        // 首先启用扩展DLL搜索
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

        // 添加程序目录到搜索路径
        AddDllDirectory(programDir.wstring().c_str());
        LogInfo("已添加程序目录到搜索路径: " + programDir.string());

        // 添加bin目录到搜索路径
        if (binExists)
        {
            AddDllDirectory(binPath.wstring().c_str());
            LogInfo("已添加bin目录到搜索路径: " + binPath.string());
        }
    }
    catch (...)
    {
        LogWarn("使用扩展DLL搜索功能失败");
    }

    return true;
}
#endif

int main(int argc, char* argv[])
{
    try
    {
        std::locale::global(std::locale(""));


        if (!UDirectory::Exists("Logs"))
        {
            UDirectory::Create("Logs");
        }

        Logger::Init();

#ifdef _WIN32
        SetupDllSearchPaths();
#endif

        bool setting = false;
        // 保存模板配置文件
        Utils::SaveYaml("template.yaml", Utils::toYaml(Configure()));

        // 检查配置文件是否存在
        if (!UFile::Exists("config.yaml"))
        {
            LogWarn("应用程序警告：请配置config.yaml！然后再次运行此程序");
            Utils::SaveYaml("config.yaml", Utils::toYaml(Configure()));
            setting = true;
        }

        // 加载配置文件
        auto configure = Utils::LoadYaml<Configure>("config.yaml");

        // 初始化应用程序并开始渲染
        Application app(configure.value(), setting);
        app.Renderer();
    }
    catch (const std::exception& e)
    {
        LogError(e.what());
    }

    return 0;
}
