#pragma once

#include <fstream>
#include <string>
#include <filesystem>
#include <iostream>
#include <Logger.h>

template<typename T>
using Scope = std::unique_ptr<T>;

template<typename T, typename... Args>
constexpr Scope<T> CreateScope(Args &&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T>
using Ref = std::shared_ptr<T>;

template<typename T, typename... Args>
constexpr Ref<T> CreateRef(Args &&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

struct Proxy {
    std::string proxy = "";
    std::string ip = "";
    std::string port = "";
};

struct OpenAIData {
    std::string api_key;
    std::string model = "gpt-3.5-turbo";
    Proxy proxy = Proxy();
};

class UFile {
public:
    static bool Exists(const std::string &filename) {
        std::ifstream file(filename);
        return file.good();
    }
};

class UDirectory {
public:
    static bool Create(const std::string &dirname) {
        try {
            std::filesystem::create_directory(dirname);
            return true;
        } catch (std::filesystem::filesystem_error &e) {
            std::cerr << "Error creating directory: " << e.what() << std::endl;
            return false;
        }
    }

    static bool Exists(const std::string &dirname) {
        return std::filesystem::is_directory(dirname);
    }
};

