#ifndef UTILS_H
#define UTILS_H

#include <codecvt>

#include "pch.h"
#include <Logger.h>
#include <Progress.hpp>
#include <Configure.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <GL/glext.h>

#ifdef _WIN32
#define popen(arg1,arg2) _popen(arg1,arg2)
#define pclose(arg1) _pclose(arg1)
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

class ChatBot;
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
class Downloader;

class UFile
{
public:
    static bool Exists(const std::string& filename);

    // 获取目录下所有文件
    static std::vector<std::string> GetFilesInDirectory(const std::string& folder)
    {
        std::vector<std::string> result;

        // 使用filesystem遍历目录
        for (const auto& entry : std::filesystem::directory_iterator(folder))
        {
            result.push_back(entry.path().string());
        }

        return result;
    }

    static std::string PlatformPath(std::string path)
    {
        return std::filesystem::path(path).make_preferred().string();
    }

    // 检查是否以某后缀结尾
    static bool EndsWith(const std::string& str, const std::string& suffix)
    {
        return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
    }

    static bool UCopyFile(const std::string& src, const std::string& dst)
    {
        try
        {
            std::filesystem::copy_file(
                std::filesystem::path(src),
                std::filesystem::path(dst)
            );
        }
        catch (std::filesystem::filesystem_error& e)
        {
            // 处理错误
            return false;
        }

        return true;
    }
};

class UDirectory
{
public:
    static bool Create(const std::string& dirname);

    static bool CreateDirIfNotExists(const std::string& dir);

    static bool Exists(const std::string& dirname);

    static bool Remove(const std::string& dir);


    static std::vector<std::string> GetSubDirectories(const std::string& dirPath);
};

class UEncrypt
{
public:
    static std::string ToMD5(const std::string& str);

    static std::string GetMD5(const std::string& str);

    static std::string GetMD5(const void* data, std::size_t size);

    static std::string md5(const std::string& data);
};

class UCompression
{
public:
    static bool Decompress7z(std::string file, std::string path);

    static bool DecompressZip(std::string file, std::string path);

    static bool DecompressGz(std::string file, std::string path);

    static bool DecompressXz(std::string file, std::string path);

    static bool DecompressRar(std::string file, std::string path);
};

class Utils
{
private:
    typedef struct
    {
        const float* data;
        int position;
        int length;
    } paUserData;

    static int paCallback(const void* inputBuffer, void* outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData);

    static bool CheckFileSize(const std::string& file_path, int expected_size);

public:
#ifdef WIN32
    inline static std::atomic<HANDLE> childProcessHandle{nullptr};

    static void CleanUpChildProcess()
    {
        if (childProcessHandle != nullptr)
        {
            TerminateProcess(childProcessHandle.load(), 0); // 强制终止子进程
            CloseHandle(childProcessHandle.load()); // 关闭进程句柄
            std::cout << "Child process terminated" << std::endl;
        }
    }

    static void OpenProgram(const char* path);
#else
    std::atomic<pid_t> childPid{-1};

    void signal_handler(int signum)
    {
        if (childPid != -1)
        {
            std::cerr << "Terminating child process" << std::endl;
            kill(childPid.load(), SIGTERM);  // 发送终止信号给子进程
        }
    }

    void OpenProgram(const char* path);
#endif
    template <typename... Args>
    static std::string ExecuteShell(const std::string& cmd, Args... args);

    static void AsyncExecuteShell(const std::string& cmd, std::vector<std::string> args)
    {
        std::thread([cmd, args]()
        {
            std::string command = cmd;
            for (const auto& arg : args)
            {
                command += " " + arg;
            }
#ifdef _WIN32
            std::unique_ptr<FILE, decltype(&_pclose)> pipe(popen(command.c_str(), "r"), _pclose);
#else
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
#endif

            if (!pipe)
            {
                std::cerr << "Error opening pipe." << std::endl;
                return;
            }

            char buffer[128];
            std::mutex mtx; // 用于同步输出的互斥锁
            while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr)
            {
                std::lock_guard<std::mutex> lock(mtx); // 锁定互斥锁
                std::cout << buffer;
            }
        }).detach(); // 使用detach让线程在后台运行
    }

    static std::string GetAbsolutePath(const std::string& relativePath);
    static std::string exec(const std::string& command);

    static std::string GetErrorString(cpr::ErrorCode code);

    static std::string execAsync(const std::string& command);

    static std::string GetLatestUrl(const std::string& url);

    static std::string GetFileName(const std::string& dir);

    static std::string GetDirName(const std::string& dir);

    static std::string GetspecificPath(const std::string& path)
    {
        std::string specificPath = path;

#ifdef _WIN32
        // Replace all forward slashes with backslashes for Windows
        for (char& c : specificPath)
        {
            if (c == '/')
            {
                c = '\\';
            }
        }
#else
        // Replace all backslashes with forward slashes for non-Windows systems
        for (char& c : specificPath) {
            if (c == '\\') {
                c = '/';
            }
        }
#endif

        return specificPath;
    }

    static std::string UrlEncode(const std::string& value)
    {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (auto i = value.begin(), n = value.end(); i != n; ++i)
        {
            std::string::value_type c = (*i);

            // 保持字母和数字不变
            if (isalnum(c) || c == '-')
            {
                escaped << c;
                continue;
            }

            // 对特殊字符进行编码
            escaped << '%' << std::setw(2) << int((unsigned char)c);
        }

        return escaped.str();
    }


    static std::string GetFileExt(std::string file);

    static void playAudio(const char* filename = "tmp.wav");

    static void playAudioAsync(const std::string& filename, std::function<void()> callback);

    static void SaveYaml(const std::string& filename, const YAML::Node& node);

    // 下载文件
    static bool UDownload(const std::pair<std::string, std::string>& task, int num_threads = 8);
    static Ref<Downloader> UDownloadAsync(const std::pair<std::string, std::string>& task, int num_threads = 8);
    static std::future<bool> UDownloads(const std::map<std::string, std::string>& tasks, int num_threads = 8);
    static std::vector<Ref<Downloader>> UDownloadsAsync(const std::map<std::string, std::string>& tasks,
                                                        int num_threads = 8);
    static bool Decompress(std::string file, std::string path = "");

    static void OpenURL(const std::string& url);

    static bool CheckCMDExist(const std::string& cmd)
    {
#ifdef _WIN32
        std::string command = "where " + cmd;
#else
    std::string command = "which " + cmd;
#endif

        std::string result = Utils::exec(command);
        return !result.empty();
    }

    static void ExecuteInBackGround(const std::string& appName);

    static void SaveFile(const std::string& content, const std::string& filename)
    {
        std::fstream outfile(filename);
        if (outfile)
        {
            outfile << content << std::endl;
            outfile.close();
            LogInfo("数据已保存到文件 {0}", filename);
        }
        else
        {
            LogError("无法打开文件 {0}", filename);
        }
    }

    template <typename T>
    static std::optional<T> LoadYaml(const std::string& file)
    {
        try
        {
            // 从文件中读取YAML数据
            YAML::Node node = YAML::LoadFile(file);

            // 将整个YAML节点转换为指定类型的对象
            return node.as<T>();
        }
        catch (const std::exception& e)
        {
            // 解析异常处理
            LogError("{0}", e.what());
            return std::optional<T>();
        }
    }

    template <typename T>
    static YAML::Node toYaml(const T& value)
    {
        YAML::Node node;
        node = value;
        return node;
    }

    static std::vector<std::string> GetMicrophoneDevices()
    {
        std::vector<std::string> Devices;
        if (SDL_Init(SDL_INIT_AUDIO) != 0)
        {
            LogError("SDL_Init Error: {0}", SDL_GetError());
            return Devices;
        }
        int count = SDL_GetNumAudioDevices(1);
        if (count < 0)
        {
            LogError("SDL_GetNumAudioDevices Error: {0}", SDL_GetError());
        }
        else
        {
            for (int i = 0; i < count; i++)
            {
                const char* deviceName = SDL_GetAudioDeviceName(i, 1);
                Devices.push_back(deviceName);
            }
        }
        SDL_Quit();
        return Devices;
    }

    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);

    static std::vector<std::string> GetDirectories(const std::string& path);

    static std::vector<std::string> GetAllCodesFromText(const std::string& text);

    static std::string ExtractNormalText(const std::string& text);

    static long long GetCurrentTimestamp() { return Logger::getCurrentTimestamp(); }

    static std::string Stamp2Time(long long timestamp) { return Logger::Stamp2Time(timestamp); }

    static int Stamp2Day(long long timestamp)
    {
        return timestamp / 86400000;
    }

    static std::wstring Unicode2String(const std::string& str);

    static std::string ReadFile(const std::string& filename);

    static std::vector<std::string> JsonArrayToStringVector(const json& array);

    static std::vector<std::string> JsonDictToStringVector(const json& array);

    // 获取文件夹下所有指定后缀的文件
    static std::vector<std::string> GetFilesWithExt(const std::string& folder,
                                                    const std::string& ext = ".txt");

    // 如果vector为空,返回默认值。否则返回第一个元素。
    static std::string GetDefaultIfEmpty(const std::vector<std::string>& vec,
                                         const std::string& defaultValue);

    static void OpenFileManager(const std::string& path);

    static std::vector<unsigned char> Str2VUChar(const std::string& str)
    {
        std::vector<unsigned char> vuchar(str.begin(), str.end());
        return vuchar;
    }

    static std::string GetPlatform();

    static int WStringSize(const std::string& str);

    static std::string WStringInsert(const std::string& str, int pos, const std::string& insertStr);
};

class StringExecutor
{
private:
    static std::string _Markdown(const std::string& text);

    static void _WriteToFile(std::string filename, const std::string& content);

    static std::string trim(const std::string& s);

public:
    struct Code
    {
        std::string Type;
        std::vector<std::string> Content;
    };

    static std::pair<std::string, std::string> EraseInRange(const std::string& str1, const std::string& str2,
                                                            const std::string& text);

    static std::string AutoExecute(std::string text, const std::shared_ptr<ChatBot>& bot);

    static std::string CMD(const std::string& text);

    static std::string File(const std::string& text);

    static std::string Python(const std::string& text);

    static std::string Process(const std::string& text);

    static std::string PreProcess(const std::string& text, const std::shared_ptr<ChatBot>& bot);

    static std::vector<Code> GetCodes(std::string text);
};

template <typename... Args>
std::string Utils::ExecuteShell(const std::string& cmd, Args... args)
{
    std::ostringstream command;
    command << cmd;

    // Append arguments
    std::vector<std::string> argList = {args...};
    for (const auto& arg : argList)
    {
        command << " " << arg;
    }
    std::string result;
    FILE* pipe = popen(command.str().c_str(), "r");
    if (!pipe)
    {
        return "Error opening pipe.";
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        result += buffer;
        std::cout << buffer;
    }
    pclose(pipe);

    return result;
}

class UImage
{
public:
    struct Meta
    {
        char MetaType[4];
        int MipmapLevel;
        int Width;
        int Height;
        int TextureFormat;
        int CompressSize;
    };

    static void Base64ToImage(const std::string& str_base64, const std::string& dstPath);

    static GLuint Base64ToTextureFromPath(const std::string& imgPath);

    static GLuint Base64ToTexture(const std::string& base64_str);

    static std::string Img2Base64(const std::string& path);

    static GLuint Img2Texture(const std::string& path);

    static void ImgResize(const std::string& base64_str, float scale, const std::string& path);

    static void CompressImageFile(std::string& image_file_path, std::string& save_image_file_path)
    {
    }

    static GLuint LoadFromMeta(const std::string& filename);

private:
    static GLuint CreateGLTexture(const unsigned char* image_data, int width, int height, int channels);

    static GLuint CreateGLTexture(const unsigned char* image_data, int width, int height, int channels, int newWidth,
                                  int newHeight);
};

#endif
