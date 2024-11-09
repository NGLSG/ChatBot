#ifndef SCRIPTINTERPRETER_H
#define SCRIPTINTERPRETER_H

#include <sol/sol.hpp>

class Script {
public:
    friend class ScriptManager;

    Script();

    ~Script();

    bool Initialize(std::string scriptsPath);

    sol::protected_function getFunction(const std::string&funcName);

    template<typename... Args>
    sol::object Invoke(const std::string&funcName, Args&&... args);

    bool checkFunc(const std::string&funcName);

    void PrintAllFunctions();

    static std::shared_ptr<Script> Create() {
        return std::make_shared<Script>();
    }

private:
    void binding();

    bool loadScript();

    void buildPackagePath(const std::string&rootPath);

    sol::state lua;
    sol::protected_function_result result;
    std::string scriptPath;
};

template<typename... Args>
sol::object Script::Invoke(const std::string&funcName, Args&&... args) {
    if (checkFunc(funcName)) {
        sol::protected_function func = lua[funcName];
        auto ret = func(std::forward<Args>(args)...);
        if (ret.valid()) {
            return ret;
        }
        sol::error err = ret;
        std::cerr << "Error call function failed:" << funcName << " due to: " << err.what() << std::endl;
        return sol::nil;
    }
    std::cerr << "Function not found: " << funcName << std::endl;
    return sol::nil;
}


#endif //SCRIPTINTERPRETER_H
