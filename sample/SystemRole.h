#ifndef SYSTEMROLE_H
#define SYSTEMROLE_H
#include <string>

#include "AES_Crypto.h"
#include "encrypted_systemrole.h"
#include "encrypted_systemrole_ex.h"


namespace SystemPrompt
{
    static std::string s_systemRole;
    static std::string s_systemRoleEx;
    static bool s_initialized = false;
    static std::mutex s_mutex;

    static std::string replacePlaceholders(const std::string& templateString,
                                           const std::string& platform,
                                           const std::string& pythonVersion,
                                           const std::string& pythonPath,
                                           const std::string& pythonPackages)
    {
        std::string result = templateString;

        size_t position;

        position = result.find("{0}");
        while (position != std::string::npos)
        {
            result.replace(position, 3, platform);
            position = result.find("{0}", position + platform.length());
        }

        position = result.find("{1}");
        while (position != std::string::npos)
        {
            result.replace(position, 3, pythonVersion);
            position = result.find("{1}", position + pythonVersion.length());
        }

        position = result.find("{2}");
        while (position != std::string::npos)
        {
            result.replace(position, 3, pythonPath);
            position = result.find("{2}", position + pythonPath.length());
        }

        position = result.find("{3}");
        while (position != std::string::npos)
        {
            result.replace(position, 3, pythonPackages);
            position = result.find("{3}", position + pythonPackages.length());
        }

        position = result.find("{4}");
        while (position != std::string::npos)
        {
            result.replace(position, 3, StringExecutor::GetTools());
            position = result.find("{4}", position + StringExecutor::GetTools().length());
        }

        return result;
    }

    static void initialize()
    {
        std::lock_guard<std::mutex> lock(s_mutex);

        if (s_initialized) return;

        try
        {
            SecureAES::X1();

            std::string rawSystemRole = SecureAES::X5(ENCRYPTED_SYSTEMROLE);

            s_systemRole = replacePlaceholders(rawSystemRole,
                                               Utils::GetPlatform(),
                                               Application::GetPythonVersion(),
                                               Application::GetPythonHome(),
                                               Application::GetPythonPackage());

            s_systemRoleEx = SecureAES::X5(ENCRYPTED_SYSTEMROLE_EX);

            SecureAES::X2();

            s_initialized = true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "解密系统角色时发生错误: " << e.what() << std::endl;
            s_systemRole = "系统角色解密失败";
            s_systemRoleEx = "示例对话解密失败";
            s_initialized = true;
        }
    }

    // 获取系统角色
    static const std::string& getSystemRole()
    {
        if (!s_initialized)
        {
            initialize();
        }
        return s_systemRole;
    }

    // 获取示例对话
    static const std::string& getSystemRoleEx()
    {
        if (!s_initialized)
        {
            initialize();
        }
        return s_systemRoleEx;
    }
}

#define SYSTEMROLE SystemPrompt::getSystemRole()
#define SYSTEMROLE_EX SystemPrompt::getSystemRoleEx()

#endif //SYSTEMROLE_H
