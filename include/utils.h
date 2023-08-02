#ifndef UTILS_H
#define UTILS_H

#include "pch.h"
#include <Logger.h>
#include <Progress.hpp>
#include <Configure.h>
#include <gl/GL.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>

using json = nlohmann::json;

#ifndef UTILS
#define UTILS

extern bool NoRecord;
extern std::mutex mtx;
extern float DownloadProgress;
extern int FileSize;
extern int DownloadedSize;
extern double DownloadSpeed;
extern double RemainingTime;
extern std::string FileName;

#endif

class UFile {
public:
    static bool Exists(const std::string &filename);

    // 获取目录下所有文件
    static std::vector<std::string> GetFilesInDirectory(const std::string &folder) {

        std::vector<std::string> result;

        // 使用filesystem遍历目录
        for (const auto &entry: std::filesystem::directory_iterator(folder)) {
            result.push_back(entry.path().string());
        }

        return result;

    }

    // 检查是否以某后缀结尾
    static bool EndsWith(const std::string &str, const std::string &suffix) {

        return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());

    }

    static bool UCopyFile(const std::string &src, const std::string &dst) {

        try {
            std::filesystem::copy_file(
                    std::filesystem::path(src),
                    std::filesystem::path(dst)
            );
        } catch (std::filesystem::filesystem_error &e) {
            // 处理错误
            return false;
        }

        return true;
    }
};

class UDirectory {
public:
    static bool Create(const std::string &dirname);

    static bool Exists(const std::string &dirname);
};

class UEncrypt {
public:
    static std::string ToMD5(const std::string &str);

    static std::string GetMD5(const std::string &str);

    static std::string GetMD5(const void *data, std::size_t size);

    static std::string md5(const std::string &data);

};

class UCompression {
public:
    static bool Decompress7z(std::string file, std::string path);

    static bool DecompressZip(std::string file, std::string path);

    static bool DecompressGz(std::string file, std::string path);

    static bool DecompressXz(std::string file, std::string path);

    static bool DecompressRar(std::string file, std::string path);
};

class Utils {
private:

    typedef struct {
        const float *data;
        int position;
        int length;
    } paUserData;

    static int paCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData);

    static bool DownloadThread(const std::string &url, const std::string &file_path, int start, int end, int id,
                               std::vector<int> &progress);

    static bool CheckFileSize(const std::string &file_path, int expected_size);


public:

    static std::string GetAbsolutePath(const std::string &relativePath);

    static std::string exec(const std::string &command);

    static std::string GetErrorString(cpr::ErrorCode code);

    static std::string execAsync(const std::string &command);

    static std::string GetLatestUrl(const std::string &url);

    static std::string GetFileName(const std::string &dir);

    static std::string GetDirName(const std::string &dir);

    static std::string GetFileExt(std::string file);

    static void playAudio(const char *filename = "tmp.wav");

    static void playAudioAsync(const std::string &filename, std::function<void()> callback);

    static void SaveYaml(const std::string &filename, const YAML::Node &node);

    // 下载文件
    static bool UDownload(const std::pair<std::string, std::string> &task, int num_threads = 8);

    static std::future<bool> UDownloads(const std::map<std::string, std::string> &tasks, int num_threads = 8);

    static bool Decompress(std::string file, std::string path = "");

    static void ExecuteInBackGround(const std::string &appName);

    static void SaveFile(const std::string &content, const std::string &filename) {
        std::ofstream outfile(filename);
        if (outfile) {
            outfile << content << std::endl;
            outfile.close();
            LogInfo("数据已保存到文件 {0}", filename);
        } else {
            LogError("无法打开文件 {0}", filename);
        }
    }

    template<typename T>
    static T LoadYaml(const std::string &file) {
        // 从文件中读取YAML数据
        YAML::Node node = YAML::LoadFile(file);

        // 将整个YAML节点转换为指定类型的对象
        return node.as<T>();
    }

    template<typename T>
    static YAML::Node toYaml(const T &value) {
        YAML::Node node;
        node = value;
        return node;
    }

    static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);

    static void OpenProgram(const char *path);

    static std::vector<std::string> GetDirectories(const std::string &path);

    static std::vector<std::string> GetAllCodesFromText(const std::string &text);

    static std::string ExtractNormalText(const std::string &text);

    static long long GetCurrentTimestamp() { return Logger::getCurrentTimestamp(); }

    static std::string Stamp2Time(long long timestamp) { return Logger::Stamp2Time(timestamp); }

    static int Stamp2Day(long long timestamp) {
        return timestamp / 86400000;
    }

    static std::wstring Unicode2String(const std::string &str);

    static std::string ReadFile(const std::string &filename);

    static std::vector<std::string> JsonArrayToStringVector(const json &array) {
        std::vector<std::string> result;
        for (json::const_iterator it = array.begin(); it != array.end(); ++it) {
            if (it->is_string()) {
                result.push_back(it->get<std::string>());
            }
        }
        return result;
    }

    // 获取文件夹下所有指定后缀的文件
    static std::vector<std::string> GetFilesWithExt(const std::string &folder,
                                                    const std::string &ext = ".txt") {

        std::vector<std::string> files = UFile::GetFilesInDirectory(folder);

        std::vector<std::string> result;

        // 找到所有匹配的文件
        for (const auto &file: files) {
            if (UFile::EndsWith(file, ext)) {
                result.push_back(file);
            }
        }

        return result;

    }

    // 如果vector为空,返回默认值。否则返回第一个元素。
    static std::string GetDefaultIfEmpty(const std::vector<std::string> &vec,
                                         const std::string &defaultValue) {

        if (vec.empty()) {
            return defaultValue;

        } else {
            return vec[0];
        }

    }

    static void OpenFileManager(const std::string &path) {
        std::string command;

#ifdef _WIN32
        // Windows
        std::string winPath = path;
        std::replace(winPath.begin(), winPath.end(), '/', '\\');
        command = "explorer.exe  /select,\"" + winPath + "\"";
#elif defined(__APPLE__)
        // macOS
    command = "open \"" + path + "\"";
#elif defined(__linux__)
    // Linux
    command = "xdg-open \"" + path + "\"";
#else
    // Unsupported platform
    std::cerr << "Error: Unsupported platform. Cannot open file manager." << std::endl;
    return;
#endif

        // Execute the command to open the file manager
        int result = std::system(command.c_str());

        if (result != 0) {
            // Failed to open the file manager
            std::cerr << "Error: Failed to open file manager." << std::endl;
        }
    }

    static std::vector<unsigned char> Str2VUChar(const std::string &str) {
        std::vector<unsigned char> vuchar(str.begin(), str.end());
        return vuchar;
    }
};

class UImage {
public:
    static void Base64ToImage(const std::string &str_base64, const std::string &dstPath);

    static GLuint Base64ToTextureFromPath(const std::string &imgPath);

    static GLuint Base64ToTexture(const std::string &base64_str);

    static std::string Img2Base64(const std::string &path);

    static GLuint Img2Texture(const std::string &path);

    static void ImgResize(const std::string &base64_str, float scale, const std::string &path);

private:
    static GLuint CreateGLTexture(const unsigned char *image_data, int width, int height, int channels);

    static GLuint CreateGLTexture(const unsigned char *image_data, int width, int height, int channels, int newWidth,
                                  int newHeight);

};

#endif