#ifndef UTILS_H
#define UTILS_H

#include "pch.h"
#include <Logger.h>
#include <Progress.hpp>
#include <Configure.h>

using json = nlohmann::json;

#ifndef UTILS
#define UTILS

extern bool NoRecord;
extern std::mutex mtx;

#endif

class UFile {
public:
    static bool Exists(const std::string &filename);
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

    static bool UDownloads(const std::map<std::string, std::string> &tasks, int num_threads = 8);

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

    static long long getCurrentTimestamp() { return Logger::getCurrentTimestamp(); }

    static std::string Stamp2Time(long long timestamp) { return Logger::Stamp2Time(timestamp); }

};

#endif