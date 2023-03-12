#pragma once

#include <fstream>
#include <string>
#include <filesystem>
#include <iostream>

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


