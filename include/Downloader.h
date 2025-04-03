//
// Created by 92703 on 2025/2/8.
//

#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <curl/curl.h>
#include <string>
#include <thread>
#include <vector>

template <typename... Args>
static inline std::string FormatString(const std::string& fmt, Args... args)
{
    std::string result = fmt;
    std::stringstream ss;
    int index = 0;

    // 使用折叠表达式替换占位符
    ([&](const auto& arg)
    {
        std::string placeholder = "{" + std::to_string(index++) + "}";
        size_t pos = result.find(placeholder);
        if (pos != std::string::npos)
        {
            ss.str(""); // 清空 stringstream
            ss << arg; // 将参数写入 stringstream
            result.replace(pos, placeholder.length(), ss.str());
        }
    }(args), ...); // 折叠表达式展开参数包

    return result;
}

static bool verifyHash(const std::string& path, const std::string& hash);


enum UrlStatus
{
    URunning = 1 << 0, // 00001
    UPaused = 1 << 1, // 00010
    UFinished = 1 << 2, // 00100
    UStopped = 1 << 3, // 01000
    USeeding = 1 << 4, // 10000
    UError = 1 << 5, // 100000
    UQuit = 1 << 6, // 1000000
    UInitializing = 1 << 7, // 10000000
    NO_STATUS = 0,
};

static UrlStatus operator|(UrlStatus a, UrlStatus b);

static UrlStatus operator&(UrlStatus a, UrlStatus b);

static UrlStatus operator~(UrlStatus a);

static UrlStatus operator|=(UrlStatus& a, UrlStatus b);

struct Speed
{
    void SetSpeed(float content);

    float content = 0;
    std::string unit; // B/s, KB/s, MB/s, GB/s
};

struct WebSpeed
{
    Speed downloadSpeed;
    Speed uploadSpeed;
};

struct BasicInfo
{
    std::string savePath;
    std::string url;
    size_t totalSize;
    std::string contentType;
    bool supportRange;
    size_t totalDownloaded;
    std::vector<std::string> files;
    bool isTorrent;
};

class Downloader
{
public:
    Downloader(const std::string& url, uint8_t numThreads = 8, const std::string& savePath = "");

    WebSpeed GetSpeed() const;

    ~Downloader();

    void ForceStart();

    void AddCallback(std::function<void(Downloader*)> callback = nullptr)
    {
        finishCallback = callback;
    }

    void Start();

    void Pause();

    BasicInfo GetBasicInfo();
    void Resume();

    UrlStatus GetStatus() const;

    void Stop();

    bool IsRunning() const;

    bool IsPaused() const;

    bool IsFinished() const;

    float GetProgress() const;

private:
    struct Chunck
    {
        size_t start;
        size_t end;
        size_t size;
        std::vector<char> data;
    };

    void GetDownloadBasicInfo();

    void SpeedMonitor();

    void Download();

    void DownloadSingle(int retries = 0);

    bool DownloadMulti();

    void DownloadChunk(Chunck& chunk, int retries = 0);

    static size_t WriteCallback(void* buffer, size_t size, size_t nitems, std::tuple<Chunck*, Downloader*>* parms);

    static size_t WriteData(void* ptr, size_t size, size_t nmemb, std::ofstream* parm);

    static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata);

    static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                                curl_off_t ulnow);

    std::atomic<CURL*> curl;
    std::vector<CURL*> multiCurl;
    std::function<void(Downloader*)> finishCallback;
    BasicInfo basicInfo;
    std::thread downloadThread;
    uint8_t numThreads;
    WebSpeed speed;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    // 控制下载状态
    std::atomic<UrlStatus> status = UrlStatus::UInitializing;
    std::mutex pauseMutex;
    std::condition_variable pauseCondition;

    size_t previousDlnow = 0;
};


#endif //DOWNLOADER_H
