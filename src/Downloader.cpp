#include "Downloader.h"

#include <filesystem>
#include <fstream>
#include <openssl/evp.h>
#include <openssl/types.h>
#include <curl/curl.h>
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif


#include "Logger.h"
#include "utils.h"
uint32_t MAX_RETRIES = 3;
uint32_t RETRY_INTERVAL = 100; //milliseconds
static bool RemoveFiles(const std::string& path)
{
    std::filesystem::path p(path);
    if (std::filesystem::exists(p))
    {
        return std::filesystem::remove_all(p);
    }
    return false;
}

UrlStatus operator|(UrlStatus a, UrlStatus b)
{
    return static_cast<UrlStatus>(static_cast<int>(a) | static_cast<int>(b));
}

UrlStatus operator&(UrlStatus a, UrlStatus b)
{
    return static_cast<UrlStatus>(static_cast<int>(a) & static_cast<int>(b));
}

UrlStatus operator~(UrlStatus a)
{
    return static_cast<UrlStatus>(~static_cast<int>(a));
}

UrlStatus operator|=(UrlStatus& a, UrlStatus b)
{
    return static_cast<UrlStatus>(static_cast<int>(a) | static_cast<int>(b));
}

void Speed::SetSpeed(float content)
{
    if (content < 0)
    {
        throw std::invalid_argument("Speed must be greater than or equal to 0");
        return;
    }
    if (content < 1024)
    {
        unit = "B/s";
    }
    else if (content < 1024 * 1024)
    {
        unit = "KB/s";
        content /= 1024;
    }
    else if (content < 1024 * 1024 * 1024)
    {
        unit = "MB/s";
        content /= 1024 * 1024;
    }
    else
    {
        unit = "GB/s";
        content /= 1024 * 1024 * 1024;
    }
    this->content = content;
}

Downloader::Downloader(const std::string& url, uint8_t numThreads, const std::string& savePath):
    numThreads(numThreads)
{
    basicInfo.totalSize = 0;
    basicInfo.supportRange = false;
    basicInfo.savePath = savePath.c_str();
    basicInfo.totalDownloaded = 0;
    basicInfo.url = url.c_str();

    curl = curl_easy_init();
    if (!curl)
    {
        LogError("Initialization failed");
        return;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // 发送 HEAD 请求
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // 启用重定向
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this); // 将 this 传递给 header callback
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &Downloader::HeaderCallback);
    std::thread([this]() { GetDownloadBasicInfo(); }).detach();
}

WebSpeed Downloader::GetSpeed() const
{
    return speed;
}

Downloader::~Downloader()
{
    LogInfo("Downloader destroyed.");
    status = UQuit;
    if (curl)
    {
        curl_easy_cleanup(curl);
    }
    for (auto& curl : multiCurl)
    {
        if (curl)
        {
            curl_easy_pause(curl, CURLPAUSE_ALL);
        }
    }
}

void Downloader::ForceStart()
{
    if (UFile::Exists(basicInfo.savePath))
    {
        RemoveFiles(basicInfo.savePath);
    }
    Start();
}

void Downloader::Start()
{
    while (status & UInitializing)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    if (UFile::Exists(basicInfo.savePath))
    {
        status = UFinished;
        LogInfo("File already exists: {0} you can call force start to cover it", basicInfo.savePath);
        return;
    }
    if (downloadThread.joinable())
    {
        LogError("Download thread is already running.");
        return;
    }
    status = URunning;
    startTime = std::chrono::system_clock::now();
    downloadThread = std::thread(&Downloader::Download, this);
    std::thread speedThread(&Downloader::SpeedMonitor, this);
    speedThread.detach();
}

void Downloader::Pause()
{
    if (!(status & UPaused))
    {
        status = UPaused;
        LogInfo("Download paused.");
        for (auto& curl : multiCurl)
        {
            curl_easy_pause(curl, CURLPAUSE_ALL);
        }
    }
}

BasicInfo Downloader::GetBasicInfo()
{
    return basicInfo;
}

void Downloader::Resume()
{
    if (status & UPaused)
    {
        status = URunning;
        pauseCondition.notify_all(); // 唤醒所有等待的线程
        LogInfo("Download resumed.");
        startTime = std::chrono::system_clock::now();
        for (auto& curl : multiCurl)
        {
            curl_easy_pause(curl, CURLPAUSE_CONT);
        }
    }
}

UrlStatus Downloader::GetStatus() const
{
    return status;
}

void Downloader::Stop()
{
    if (status == UFinished)
    {
        return;
    }
    if (downloadThread.joinable())
    {
        status = UStopped;
        pauseCondition.notify_all();
        downloadThread.join();
#ifdef WIN32
        TerminateThread(downloadThread.native_handle(), 0);
#else
        pthread_t pthread_id = downloadThread.native_handle();
        pthread_cancel(pthread_id);
#endif


        LogInfo("Download stopped.");
    }
}

bool Downloader::IsRunning() const
{
    return status & URunning && !(status & UPaused || status & UStopped);
}

bool Downloader::IsPaused() const
{
    return status & UPaused;
}

bool Downloader::IsFinished() const
{
    return status & UFinished;
}

//0-1
float Downloader::GetProgress() const
{
    return static_cast<float>(basicInfo.totalDownloaded) /
        static_cast<float>(basicInfo.totalSize);
}

void Downloader::GetDownloadBasicInfo()
{
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        LogInfo("Failed to get basic info");
        return;
    }
    if (basicInfo.files.size() == 0)
    {
        std::string file = basicInfo.url.substr(basicInfo.url.find_last_of("/") + 1);
        size_t pos = file.find_first_of('?');

        if (pos != std::string::npos)
        {
            file = file.substr(0, pos);
        }
        basicInfo.files.push_back(file);
    }
    // 打印获取到的信息
    LogInfo("Basic info:");
    LogInfo("Url: {0}", basicInfo.url);
    LogInfo("Filename: {0}", basicInfo.savePath);
    LogInfo("Total Size: {0}", basicInfo.totalSize);
    LogInfo("Content Type: {0}", basicInfo.contentType);
    LogInfo("Support Range: {0}", basicInfo.supportRange ? "Yes" : "No");
    if (UFile::Exists(basicInfo.savePath))
    {
        basicInfo.totalDownloaded = std::filesystem::file_size(basicInfo.savePath);
        status = UFinished;
    }
    status = NO_STATUS;
}

void Downloader::SpeedMonitor()
{
    while (!(status & UFinished) && !(status & UQuit))
    {
        float duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - startTime).count();
        speed.downloadSpeed.SetSpeed(basicInfo.totalDownloaded / duration);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void Downloader::Download()
{
    if (status & UPaused)
    {
        std::unique_lock<std::mutex> lock(pauseMutex);
        pauseCondition.wait(lock);
    }
    while (status & UInitializing)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    if (basicInfo.supportRange)
    {
        DownloadMulti();
    }
    else
    {
        DownloadSingle();
    }
}

void Downloader::DownloadSingle(int retries)
{
    if (status == UQuit)
    {
        return;
    }
    if (retries >= MAX_RETRIES)
    {
        LogError("Failed to download file: {0}", basicInfo.savePath);
        return;
    }
    // 初始化文件流
    std::ofstream file(basicInfo.savePath, std::ios::binary);
    if (!file.is_open())
    {
        LogError("Failed to open file: {0}", basicInfo.savePath);
        LogInfo("Retry will start {0} microsecond later.Current retry count: {0}", retries);
        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL));
        DownloadSingle(retries + 1);
        return;
    }

    CURL* curl = curl_easy_init();
    multiCurl.push_back(curl);
    if (!curl)
    {
        LogError("Initialization failed");
        LogInfo("Retry will start {0} microsecond later.Current retry count: {0}", retries);
        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL));
        DownloadSingle(retries + 1);
        return;
    }

    // 设置 libcurl 选项
    curl_easy_setopt(curl, CURLOPT_URL, basicInfo.url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L); // 发送 GET 请求
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &Downloader::WriteData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &Downloader::ProgressCallback);
    // 启用进度回调
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

    // 启动下载线程
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        LogError("Failed to download file");
        LogInfo("Retry will start {0} microsecond later.Current retry count: {0}", retries);
        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL));
        basicInfo.totalDownloaded = 0;
        DownloadSingle(retries + 1);
    }
    file.close();
    status = UFinished;
    if (finishCallback)
    {
        finishCallback(this);
    }
}

bool Downloader::DownloadMulti()
{
    // 计算每个块的大小
    long long chunkSize = basicInfo.totalSize / numThreads;

    // 创建线程和块信息
    std::vector<std::thread> threads;
    std::vector<Chunck> chunks(numThreads);

    for (int i = 0; i < numThreads; ++i)
    {
        chunks[i].start = i * chunkSize;
        chunks[i].end = (i == numThreads - 1) ? basicInfo.totalSize - 1 : (i + 1) * chunkSize - 1;
        chunks[i].size = chunks[i].end - chunks[i].start + 1;

        threads.push_back(std::thread([this, &chunks, i]
        {
            DownloadChunk(chunks[i]);
        }));
    }

    // 等待所有线程完成
    for (auto& thread : threads)
    {
        thread.join();
    }

    // 合并所有块
    std::ofstream file(basicInfo.savePath, std::ios::binary);
    if (!file.is_open())
    {
        LogError("Failed to open file: {0}", basicInfo.savePath);
        return false;
    }

    for (const auto& chunk : chunks)
    {
        file.write(chunk.data.data(), chunk.data.size());
    }

    file.close();
    status = UFinished;
    status = UFinished;
    if (finishCallback)
    {
        finishCallback(this);
    }
    return true;
}

void Downloader::DownloadChunk(Chunck& chunk, int retries)
{
    if (status != UQuit)
    {
        if (retries >= MAX_RETRIES)
        {
            LogError("Reached max retries");
            return;
        }
        CURL* curl = curl_easy_init();
        multiCurl.push_back(curl);
        if (!curl)
        {
            LogError("Initialization failed");
            LogInfo("Retry will start {0} microsecond later.Current retry count: {0}", retries);
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL));
            DownloadChunk(chunk, retries + 1);
            return;
        }

        // 设置 libcurl 选项
        curl_easy_setopt(curl, CURLOPT_URL, basicInfo.url.c_str());
#ifndef NDEBUG
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif
        curl_easy_setopt(curl, CURLOPT_RANGE,
                         std::to_string(chunk.start).append("-").append(std::to_string(chunk.end)).c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &Downloader::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        auto t = std::make_tuple(&chunk, this);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &t);

        // 执行下载
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            LogError("Failed to download chunk");
            LogInfo("Retry will start {0} microsecond later.Current retry count: {0}", retries);
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL));
            basicInfo.totalDownloaded -= chunk.size;
            DownloadChunk(chunk, retries + 1);
        }

        curl_easy_cleanup(curl);
    }
}

size_t Downloader::WriteCallback(void* buffer, size_t size, size_t nitems, std::tuple<Chunck*, Downloader*>* parms)
{
    size_t real_size = size * nitems;

    auto [chunk, downloader] = *parms;
    if (chunk->data.size() + real_size > chunk->size)
    {
        real_size = chunk->size - chunk->data.size();
    }

    chunk->data.insert(chunk->data.end(), static_cast<char*>(buffer), static_cast<char*>(buffer) + real_size);

    downloader->basicInfo.totalDownloaded += real_size;
#ifndef NDEBUG
    if (downloader->basicInfo.totalSize > 0)
    {
        std::cout << "\rDownloaded: "
            << std::fixed << std::setprecision(2)
            << (static_cast<float>(downloader->basicInfo.totalDownloaded) /
                static_cast<float>(downloader->basicInfo.totalSize) * 100.0f)
            << "%" << std::flush;
    }
    else
    {
        std::cout << "\rDownloaded: " << downloader->basicInfo.totalDownloaded << " bytes" <<
            std::flush;
    }
#endif
    return real_size;
}


size_t Downloader::WriteData(void* ptr, size_t size, size_t nmemb, std::ofstream* stream)
{
    size_t written = 0;
    if (stream)
    {
        stream->write(static_cast<char*>(ptr), size * nmemb);
        written = size * nmemb;
    }
    return written;
}

size_t Downloader::HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata)
{
    Downloader* downloader = static_cast<Downloader*>(userdata);
    std::string header(buffer, size * nitems);

    // 解析响应头
    if (header.find("Content-Length:") != std::string::npos)
    {
        std::string value = header.substr(header.find(":") + 2);
        value.erase(value.find_last_not_of(" \t\n\r") + 1);
        downloader->basicInfo.totalSize = std::stoll(value);
    }
    else if (header.find("Content-Type:") != std::string::npos)
    {
        std::string value = header.substr(header.find(":") + 2);
        value.erase(value.find_last_not_of(" \t\n\r") + 1);
        downloader->basicInfo.contentType = value;
    }
    else if (header.find("Content-Disposition:") != std::string::npos)
    {
        if (downloader->basicInfo.files.size() == 0)
        {
            std::string value = header.substr(header.find("filename=") + 9);
            value.erase(value.find_last_not_of(" \t\n\r") + 1);
            downloader->basicInfo.files.emplace_back(value);
        }
    }
    else if (header.find("Accept-Ranges:") != std::string::npos)
    {
        std::string value = header.substr(header.find(":") + 2);
        value.erase(value.find_last_not_of(" \t\n\r") + 1);
        downloader->basicInfo.supportRange = (value == "bytes");
    }

    return size * nitems;
}

int Downloader::ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                                 curl_off_t ulnow)
{
    auto downloader = static_cast<Downloader*>(clientp);
    downloader->basicInfo.totalDownloaded = dlnow;
#ifndef NDEBUG
    if (downloader->basicInfo.totalSize > 0)
    {
        std::cout << "\rDownloaded: "
            << std::fixed << std::setprecision(2)
            << (static_cast<float>(downloader->basicInfo.totalDownloaded) /
                static_cast<float>(downloader->basicInfo.totalSize) * 100.0f)
            << "%" << std::flush;
    }
    else
    {
        std::cout << "\rDownloaded: " << downloader->basicInfo.totalDownloaded << " bytes" <<
            std::flush;
    }
#endif
    return (downloader->status & UStopped);
}
