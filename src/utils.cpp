#define NOMINMAX
#include "utils.h"

#include "../sample/Application.h"
#include "ChatBot.h"
#include "Downloader.h"
bool NoRecord = false;
std::mutex mtx;
float DownloadProgress;
int FileSize;
int DownloadedSize;
double DownloadSpeed;
double RemainingTime;
std::string FileName;


bool UFile::Exists(const std::string& filename)
{
    std::ifstream file(filename);
    return file.good();
}

bool UDirectory::CreateDirIfNotExists(const std::string& dir)
{
    if (!Exists(dir))
    {
        return Create(dir);
    }
    return true;
}

bool UDirectory::Create(const std::string& dirname)
{
    try
    {
        std::filesystem::create_directories(dirname);
        return true;
    }
    catch (std::filesystem::filesystem_error& e)
    {
        LogError(fmt::format("creating directory: {0}" , e.what()));
        return false;
    }
}

bool UDirectory::Exists(const std::string& dirname)
{
    return std::filesystem::is_directory(dirname);
}

bool UDirectory::Remove(const std::string& dir)
{
    try
    {
        // 使用 std::filesystem::remove_all 删除目录及其所有内容
        if (std::filesystem::remove_all(dir))
        {
            return true;
        }
        else
        {
            LogError("Failed to remove directory '{0}{1}", dir, "'.");
            return false;
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LogError("Error occurred while removing directory: {0}", e.what());
        return false;
    }
}

std::vector<std::string> UDirectory::GetSubDirectories(const std::string& dirPath)
{
    std::filesystem::path path(dirPath);
    std::vector<std::string> directories;
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        if (std::filesystem::is_directory(entry))
        {
            directories.push_back(entry.path().filename().string());
            // 递归获取子文件夹下的子文件夹
            // auto subdirectories = ListDirectories(entry);
            // directories.insert(directories.end(), subdirectories.begin(), subdirectories.end());
        }
    }
    return directories;
}

std::string UEncrypt::ToMD5(const std::string& str)
{
    unsigned char md[16];
    MD5((const unsigned char*)str.c_str(), str.length(), md);
    char buf[33] = {'\0'};
    for (int i = 0; i < 16; ++i)
    {
        sprintf(buf + i * 2, "%02x", md[i]);
    }
    return std::string(buf);
}

std::string UEncrypt::GetMD5(const std::string& str)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((const unsigned char*)str.c_str(), str.size(), digest);
    char md5_str[2 * MD5_DIGEST_LENGTH + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(md5_str + 2 * i, "%02x", digest[i]);
    }
    md5_str[2 * MD5_DIGEST_LENGTH] = '\0';
    return std::string(md5_str);
}

std::string UEncrypt::GetMD5(const void* data, std::size_t size)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((const unsigned char*)data, size, digest);
    char md5_str[2 * MD5_DIGEST_LENGTH + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(md5_str + 2 * i, "%02x", digest[i]);
    }
    md5_str[2 * MD5_DIGEST_LENGTH] = '\0';
    return std::string(md5_str);
}

std::string UEncrypt::md5(const std::string& data)
{
    unsigned char md[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(data.data()), data.size(), md);

    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(md[i]);
    }
    return ss.str();
}

int Utils::paCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData)
{
    paUserData* data = (paUserData*)userData;
    const float* in = data->data + data->position;
    float* out = (float*)outputBuffer;
    int framesLeft = data->length - data->position;
    int framesToCopy = framesPerBuffer;
    if (framesToCopy > framesLeft)
    {
        framesToCopy = framesLeft;
    }
    for (int i = 0; i < framesToCopy; i++)
    {
        out[i] = in[i];
    }
    data->position += framesToCopy;
    return framesToCopy < framesPerBuffer ? paComplete : paContinue;
}

std::string Utils::GetAbsolutePath(const std::string& relativePath)
{
    // 将相对路径转换为std::filesystem::path类型的路径
    std::filesystem::path path(relativePath);

    // 将相对路径转换为绝对路径
    std::filesystem::path absolutePath = std::filesystem::absolute(path);

    return absolutePath.generic_string();
}

std::string Utils::execAsync(const std::string& command)
{
    // 异步执行命令并返回输出结果
    std::future<std::string> resultFuture = std::async(std::launch::async, [&command]()
    {
        return exec(command);
    });
    // 等待异步任务完成并返回结果
    return resultFuture.get();
}

std::string Utils::exec(const std::string& command)
{
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe)
    {
        throw std::runtime_error("Error: Failed to execute command " + command);
    }
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        output += buffer;
    }
    int status = pclose(pipe);
    if (status == -1)
    {
        throw std::runtime_error("Error: Failed to close pipe for command " + command);
    }
    return output;
}

void Utils::playAudioAsync(const std::string& filename, std::function<void()> callback)
{
    // 异步执行音频播放
    std::thread([&]()
    {
        // 打开音频文件
        NoRecord = true;
        SF_INFO info;
        SNDFILE* file = sf_open("tmp.wav", SFM_READ, &info);
        if (!file)
        {
            std::cerr << "Failed to open file: tmp.wav" << std::endl;
            return;
        }

        // 读取音频数据
        int sampleRate = info.samplerate;
        int channels = info.channels;
        int frames = info.frames;
        float* data = new float[frames * channels];
        sf_readf_float(file, data, frames);
        sf_close(file);

        // 初始化PortAudio
        PaError err = Pa_Initialize();
        if (err != paNoError)
        {
            std::cerr << "Failed to initialize PortAudio: " << Pa_GetErrorText(err) << std::endl;
            delete[] data;
            return;
        }

        // 打开音频流
        paUserData userData = {data, 0, frames * channels};
        PaStream* stream;
        err = Pa_OpenDefaultStream(&stream, channels, channels, paFloat32, sampleRate, FRAMES_PER_BUFFER, paCallback,
                                   &userData);
        if (err != paNoError)
        {
            std::cerr << "Failed to open audio stream: " << Pa_GetErrorText(err) << std::endl;
            delete[] data;
            Pa_Terminate();
            return;
        }

        // 开始播放音频
        err = Pa_StartStream(stream);
        if (err != paNoError)
        {
            std::cerr << "Failed to start audio stream: " << Pa_GetErrorText(err) << std::endl;
            delete[] data;
            Pa_CloseStream(stream);
            Pa_Terminate();
            return;
        }

        // 等待音频播放完成
        while (Pa_IsStreamActive(stream))
        {
            Pa_Sleep(100);
        }
        NoRecord = false;
        // 关闭音频流并清理资源
        err = Pa_StopStream(stream);
        if (err != paNoError)
        {
            std::cerr << "Failed to stop audio stream: " << Pa_GetErrorText(err) << std::endl;
        }
        Pa_CloseStream(stream);
        Pa_Terminate();
        delete[] data;

        // 调用回调函数
        if (callback)
        {
            callback();
        }
    }).join();
}

void Utils::playAudio(const char* filename)
{
    // 打开音频文件
    SF_INFO info;
    SNDFILE* file = sf_open(filename, SFM_READ, &info);
    if (!file)
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    // 读取音频数据
    int sampleRate = info.samplerate;
    int channels = info.channels;
    int frames = info.frames;
    float* data = new float[frames * channels];
    sf_readf_float(file, data, frames);
    sf_close(file);

    // 初始化PortAudio

    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        std::cerr << "Failed to initialize PortAudio: " << Pa_GetErrorText(err) << std::endl;
        delete[] data;
        return;
    }

    // 打开音频流
    paUserData userData = {data, 0, frames * channels};
    PaStream* stream;
    err = Pa_OpenDefaultStream(&stream, channels, channels, paFloat32, sampleRate, FRAMES_PER_BUFFER, paCallback,
                               &userData);
    if (err != paNoError)
    {
        std::cerr << "Failed to open audio stream: " << Pa_GetErrorText(err) << std::endl;
        delete[] data;
        Pa_Terminate();
        return;
    }

    // 开始播放音频
    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        std::cerr << "Failed to start audio stream: " << Pa_GetErrorText(err) << std::endl;
        delete[] data;
        Pa_CloseStream(stream);
        Pa_Terminate();
        return;
    }

    // 等待音频播放完毕
    while (Pa_IsStreamActive(stream))
    {
        Pa_Sleep(100);
    }

    // 停止音频流并清理资源
    err = Pa_StopStream(stream);
    if (err != paNoError)
    {
        std::cerr << "Failed to stop audio stream: " << Pa_GetErrorText(err) << std::endl;
    }
    Pa_CloseStream(stream);
    Pa_Terminate();
    delete[] data;
}

void Utils::SaveYaml(const std::string& filename, const YAML::Node& node)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        throw std::runtime_error("LogError: Failed to open file " + filename);
        return;
    }
    file << node;
    file.close();
}

size_t Utils::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t realsize = size * nmemb;
    std::string* response_data = (std::string*)userdata;
    response_data->append(ptr, realsize);
    return realsize;
}

std::string Utils::GetErrorString(cpr::ErrorCode code)
{
    static const std::map<cpr::ErrorCode, std::string> error_map = {
        {cpr::ErrorCode::OK, "OK"},
        {cpr::ErrorCode::OPERATION_TIMEDOUT, "OPERATION_TIMEDOUT"},
        {cpr::ErrorCode::SSL_CONNECT_ERROR, "SSL_CONNECT_ERROR"},
        {cpr::ErrorCode::UNSUPPORTED_PROTOCOL, "UNSUPPORTED_PROTOCOL"},
        {cpr::ErrorCode::TOO_MANY_REDIRECTS, "TOO_MANY_REDIRECTS"},
        {cpr::ErrorCode::UNKNOWN_ERROR, "UNKNOWN_ERROR"}
    };

    auto it = error_map.find(code);
    if (it != error_map.end())
    {
        return it->second;
    }
    else
    {
        return "UNKNOWN_ERROR";
    }
}

std::string Utils::GetLatestUrl(const std::string& url)
{
    // 发送HTTP请求
    cpr::Response response = cpr::Get(cpr::Url{url});

    // 解析版本号和URL
    std::string tag, download_url;
    if (response.status_code == 200)
    {
        size_t pos = response.text.find("\"tag_name\":");
        if (pos != std::string::npos)
        {
            pos += 11;
            size_t endpos = response.text.find_first_of(",\n", pos);
            if (endpos != std::string::npos)
            {
                tag = response.text.substr(pos, endpos - pos);
            }
            else
            {
                tag = response.text.substr(pos);
            }
            tag = tag.substr(1, tag.size() - 2);
        }

        pos = response.text.find("\"browser_download_url\":");
        if (pos != std::string::npos)
        {
            pos += 23;
            size_t endpos = response.text.find_first_of(",\n", pos);
            if (endpos != std::string::npos)
            {
                download_url = response.text.substr(pos, endpos - pos);
            }
            else
            {
                download_url = response.text.substr(pos);
            }
            download_url = download_url.substr(1, download_url.size() - 2);
        }
    }

    // 返回URL或者空字符串
    if (!download_url.empty())
    {
        return download_url;
    }
    else
    {
        return "";
    }
}

std::string Utils::GetFileName(const std::string& dir)
{
    // 使用 C++17 标准库中的文件系统库
    std::filesystem::path path(dir);
    // 获取文件夹中最后一个文件名
    return path.filename().string();
}

std::future<bool> Utils::UDownloads(const std::map<std::string, std::string>& tasks, int num_threads)
{
    return std::async(std::launch::async, [tasks, num_threads]()
    {
        bool success = true;
        std::vector<std::future<bool>> futures;
        for (const auto& url_file_pair : tasks)
        {
            futures.push_back(std::async(std::launch::async, [url_file_pair, num_threads]()
            {
                return UDownload(url_file_pair, num_threads);
            }));
        }
        for (auto& future : futures)
        {
            if (!future.get())
            {
                success = false;
            }
        }
        return success;
    });
}

std::vector<Ref<Downloader>> Utils::UDownloadsAsync(const std::map<std::string, std::string>& tasks, int num_threads)
{
    std::vector<Ref<Downloader>> downloaders;
    for (const auto& url_file_pair : tasks)
    {
        downloaders.push_back(UDownloadAsync(url_file_pair, num_threads));
    }
    return downloaders;
}

bool Utils::UDownload(const std::pair<std::string, std::string>& task, int num_threads)
{
    try
    {
        Downloader downloader(task.first, num_threads, task.second);
        downloader.Start();
        while (downloader.IsRunning())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            LogInfo("\rDownloading {0}: {1}% {2} {3}", task.second, downloader.GetProgress(),
                    downloader.GetSpeed().downloadSpeed.content, downloader.GetSpeed().downloadSpeed.unit);
            std::cout << std::flush;
        }
        return downloader.IsFinished();
    }
    catch (const std::exception& e)
    {
        LogError("Exception in Download: " + std::string(e.what()));
        return false;
    }
}

Ref<Downloader> Utils::UDownloadAsync(const std::pair<std::string, std::string>& task, int num_threads)
{
    Ref<Downloader> downloader = CreateRef<Downloader>(task.first, num_threads, task.second);
    std::thread([downloader]()
    {
        downloader->ForceStart();
        while (downloader->IsRunning())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }).detach();
    return downloader;
}

bool Utils::CheckFileSize(const std::string& file_path, int expected_size)
{
    try
    {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            LogError("Failed to open file " + file_path);
            return false;
        }
        int file_size = file.tellg();
        if (file_size != expected_size)
        {
            LogError("Downloaded file size does not match expected size.");
            return false;
        }
        return true;
    }
    catch (const std::exception& e)
    {
        LogError("Exception in CheckFileSize: " + std::string(e.what()));
        return false;
    }
}
#ifdef WIN32
void Utils::OpenProgram(const char* path)
{
    std::thread worker([=]()
    {
        STARTUPINFO si{};
        PROCESS_INFORMATION pi{};

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;

        HANDLE g_hChildStd_IN_Rd = nullptr;
        HANDLE g_hChildStd_IN_Wr = nullptr;
        HANDLE g_hChildStd_OUT_Rd = nullptr;
        HANDLE g_hChildStd_OUT_Wr = nullptr;

        if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0))
        {
            std::cerr << "Failed to create output pipe" << std::endl;
            return;
        }

        if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &sa, 0))
        {
            std::cerr << "Failed to create input pipe" << std::endl;
            return;
        }

        GetStartupInfo(&si);
        si.hStdInput = g_hChildStd_IN_Rd;
        si.hStdOutput = g_hChildStd_OUT_Wr;
        si.hStdError = g_hChildStd_OUT_Wr;
        si.dwFlags |= STARTF_USESTDHANDLES;

        std::string cmdLine = path;
        if (!CreateProcess(nullptr,
                           const_cast<char*>(cmdLine.c_str()),
                           nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi))
        {
            std::cerr << "Failed to open process" << std::endl;
            return;
        }

        childProcessHandle.store(pi.hProcess); // 存储子进程句柄

        std::cout << "Open process successfully" << std::endl;

        // 等待子进程完成
        WaitForSingleObject(pi.hProcess, INFINITE);

        std::cout << "Process finished" << std::endl;

        // 关闭句柄
        CloseHandle(g_hChildStd_IN_Rd);
        CloseHandle(g_hChildStd_IN_Wr);
        CloseHandle(g_hChildStd_OUT_Rd);
        CloseHandle(g_hChildStd_OUT_Wr);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    });

    worker.detach();

    // 注册退出时清理函数
    std::atexit(CleanUpChildProcess);
}
#else
void Utils::OpenProgram(const char* path)
{
    std::thread worker([=]()
            {
                pid_t pid = fork();
                if (pid == 0)
                {
                    // 在子进程中运行
                    execl(path, path, nullptr);
                    std::cerr << "Failed to start program" << std::endl;
                    exit(1);
                }
                else if (pid > 0)
                {
                    // 在父进程中
                    childPid.store(pid);  // 存储子进程的PID
                    std::cout << "Open process successfully" << std::endl;

                    // 等待子进程完成
                    waitpid(pid, nullptr, 0);

                    std::cout << "Process finished" << std::endl;
                }
                else
                {
                    std::cerr << "Failed to fork process" << std::endl;
                }
            });

    // 注册退出时清理函数
    std::signal(SIGTERM, signal_handler);
    std::atexit([](){
        if (childPid != -1)
        {
            std::cerr << "Automatically terminating child process due to main program exit" << std::endl;
            kill(childPid.load(), SIGTERM);  // 发送SIGTERM信号给子进程
        }
    });

    worker.detach();
}
#endif
bool Utils::Decompress(std::string file, std::string path)
{
    if (path.empty())
    {
        path = GetDirName(GetAbsolutePath(file));
    }

    // 获取文件扩展名
    std::string ext = GetFileExt(file);

    // 根据扩展名调用相应的解压缩函数
    if (ext == "7z")
    {
        return UCompression::Decompress7z(file, path);
    }
    else if (ext == "zip")
    {
        return UCompression::DecompressZip(file, path);
    }
    else if (ext == "gz")
    {
        return UCompression::DecompressGz(file, path);
    }
    else if (ext == "xz")
    {
        return UCompression::DecompressXz(file, path);
    }
    else if (ext == "rar")
    {
        return UCompression::DecompressRar(file, path);
    }
    else
    {
        LogError("Unsupported file extension: {0}", ext);
        return false;
    }
}

void Utils::OpenURL(const std::string& url)
{
#ifdef WIN32
    ExecuteShell("start " + url);
#elif __APPLE__
    ExecuteShell("open " + url);
#elif __linux__
    ExecuteShell("xdg-open " + url);
#endif
}

std::string Utils::GetDirName(const std::string& dir)
{
    std::filesystem::path path(dir);
    return path.parent_path().string() + "/";
}

std::string Utils::GetFileExt(std::string file)
{
    // 获取文件名中最后一个'.'的位置
    size_t dotPos = file.find_last_of('.');
    if (dotPos == std::string::npos)
    {
        // 没有找到'.'，返回空字符串
        return "";
    }
    else
    {
        // 返回'.'后面的字符串作为扩展名
        return file.substr(dotPos + 1);
    }
}

bool UCompression::Decompress7z(std::string file, std::string path)
{
    // 打开7z文件
    struct archive* a = archive_read_new();
    if (!a)
    {
        LogError("Failed to create archive object");
        return false;
    }
    archive_read_support_format_7zip(a);
    int r = archive_read_open_filename(a, file.c_str(), 10240);
    if (r != ARCHIVE_OK)
    {
        LogError("Failed to open 7z file: {0}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    // 解压缩7z文件中的每个文件
    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        std::string filename = archive_entry_pathname(entry);
        std::string outfile = path + filename;
        struct archive* ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
        archive_write_disk_set_standard_lookup(ext);
        archive_entry_set_pathname(entry, outfile.c_str());
        r = archive_read_extract(a, entry, 0);
        if (r != ARCHIVE_OK)
        {
            LogError("Failed to extract file from 7z archive: {0}", archive_error_string(a));
            archive_write_free(ext);
            archive_read_free(a);
            return false;
        }
        archive_write_free(ext);
    }

    // 关闭7z文件
    archive_read_close(a);
    archive_read_free(a);

    LogInfo("Successfully decompressed 7z file: {0}", file);
    return true;
}

// 解压缩zip文件
bool UCompression::DecompressZip(std::string file, std::string path)
{
    // 打开zip文件
    struct archive* a = archive_read_new();
    if (!a)
    {
        LogError("Failed to create archive object");
        return false;
    }
    archive_read_support_format_zip(a);
    int r = archive_read_open_filename(a, file.c_str(), 10240);
    if (r != ARCHIVE_OK)
    {
        LogError("Failed to open zip file: {0}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    // 解压缩zip文件中的每个文件
    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        std::string filename = archive_entry_pathname(entry);
        std::string outfile = path + filename;
        struct archive* ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
        archive_write_disk_set_standard_lookup(ext);
        archive_entry_set_pathname(entry, outfile.c_str());
        r = archive_read_extract(a, entry, 0);
        if (r != ARCHIVE_OK)
        {
            LogError("Failed to extract file from zip archive: {0}", archive_error_string(a));
            archive_write_free(ext);
            archive_read_free(a);
            return false;
        }
        archive_write_free(ext);
    }

    // 关闭zip文件
    archive_read_close(a);
    archive_read_free(a);

    LogInfo("Successfully decompressed zip file: {0}", file);
    return true;
}

// 解压缩gz文件
bool UCompression::DecompressGz(std::string file, std::string path)
{
    // 打开tar.gz文件
    struct archive* a = archive_read_new();
    if (!a)
    {
        LogError("Failed to create archive object");
        return false;
    }
    archive_read_support_filter_gzip(a);
    archive_read_support_format_tar(a);
    int r = archive_read_open_filename(a, file.c_str(), 10240);
    if (r != ARCHIVE_OK)
    {
        LogError("Failed to open tar.gz file: {0}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    // 解压缩tar.gz文件中的每个文件
    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        std::string filename = archive_entry_pathname(entry);
        std::string outfile = path + filename;
        struct archive* ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
        archive_write_disk_set_standard_lookup(ext);
        archive_entry_set_pathname(entry, outfile.c_str());
        r = archive_read_extract(a, entry, 0);
        if (r != ARCHIVE_OK)
        {
            LogError("Failed to extract file from tar.gz archive: {0}", archive_error_string(a));
            archive_write_free(ext);
            archive_read_free(a);
            return false;
        }
        archive_write_free(ext);
    }

    // 关闭tar.gz文件
    archive_read_close(a);
    archive_read_free(a);

    LogInfo("Successfully decompressed tar.gz file: {0}", file);
    return true;
}

// 解压缩xz文件
bool UCompression::DecompressXz(std::string file, std::string path)
{
    // 打开tar.xz文件
    struct archive* a = archive_read_new();
    if (!a)
    {
        LogError("Failed to create archive object");
        return false;
    }
    archive_read_support_filter_xz(a);
    archive_read_support_format_tar(a);
    int r = archive_read_open_filename(a, file.c_str(), 10240);
    if (r != ARCHIVE_OK)
    {
        LogError("Failed to open tar.xz file: {0}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    // 解压缩tar.xz文件中的每个文件
    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        std::string filename = archive_entry_pathname(entry);
        std::string outfile = path + filename;
        struct archive* ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
        archive_write_disk_set_standard_lookup(ext);
        archive_entry_set_pathname(entry, outfile.c_str());
        r = archive_read_extract(a, entry, 0);
        if (r != ARCHIVE_OK)
        {
            LogError("Failed to extract file from tar.xz archive: {0}", archive_error_string(a));
            archive_write_free(ext);
            archive_read_free(a);
            return false;
        }
        archive_write_free(ext);
    }

    // 关闭tar.xz文件
    archive_read_close(a);
    archive_read_free(a);

    LogInfo("Successfully decompressed tar.xz file: {0}", file);
    return true;
}

bool UCompression::DecompressRar(std::string file, std::string path)
{
    // 打开RAR文件
    struct archive* a = archive_read_new();
    if (!a)
    {
        LogError("Failed to create archive object");
        return false;
    }
    archive_read_support_format_rar(a);
    int r = archive_read_open_filename(a, file.c_str(), 10240);
    if (r != ARCHIVE_OK)
    {
        LogError("Failed to open RAR file: {0}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    // 解压缩RAR文件中的每个文件
    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        std::string filename = archive_entry_pathname(entry);
        std::string outfile = path + filename;
        struct archive* ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
        archive_write_disk_set_standard_lookup(ext);
        archive_entry_set_pathname(entry, outfile.c_str());
        r = archive_read_extract(a, entry, 0);
        if (r != ARCHIVE_OK)
        {
            LogError("Failed to extract file from RAR archive: {0}", archive_error_string(a));
            archive_write_free(ext);
            archive_read_free(a);
            return false;
        }
        archive_write_free(ext);
    }

    // 关闭RAR文件
    archive_read_close(a);
    archive_read_free(a);

    LogInfo("Successfully decompressed RAR file: {0}", file);
    return true;
}

std::vector<std::string> Utils::GetDirectories(const std::string& path)
{
    std::vector<std::string> dirs;
    dirs.push_back("empty");
    for (auto& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_directory())
        {
            dirs.push_back(GetAbsolutePath(entry.path().string()));
        }
    }
    return dirs;
}

std::vector<std::string> Utils::GetAllCodesFromText(const std::string& text)
{
    std::regex codeRegex("`{3}([\\s\\S]*?)`{3}");
    std::vector<std::string> codeStrings;
    std::smatch matchResult;
    std::string::const_iterator searchStart(text.cbegin());
    while (std::regex_search(searchStart, text.cend(), matchResult, codeRegex))
    {
        searchStart = matchResult.suffix().first;
        if (matchResult.size() >= 2)
        {
            std::string codeString = matchResult[1].str();
            codeStrings.push_back(codeString);
        }
    }

    return codeStrings;
}

std::string Utils::ExtractNormalText(const std::string& text)
{
    std::regex codeRegex("```[^`]*?\\n([\\s\\S]*?)\\n```"); // 匹配代码块
    std::string normalText = text;
    std::sregex_iterator codeBlockIterator(text.cbegin(), text.cend(), codeRegex);
    std::sregex_iterator endIterator;

    for (auto it = codeBlockIterator; it != endIterator; ++it)
    {
        std::string codeBlock = it->str();
        std::size_t position = normalText.find(codeBlock);
        if (position != std::string::npos) // 如果找到匹配的代码块，则将其替换为普通文本
        {
            normalText.replace(position, codeBlock.length(), ""); // 将代码块替换为空字符串
        }
    }

    return normalText;
}

std::wstring Utils::Unicode2String(const std::string& str)
{
    std::wstring result;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] == '\\' && i + 5 < str.size() && str[i + 1] == 'u')
        {
            // Found a Unicode escape sequence
            wchar_t code_point = 0;
            for (size_t j = 0; j < 4; j++)
            {
                char hex_digit = str[i + 2 + j];
                if (hex_digit >= '0' && hex_digit <= '9')
                {
                    code_point = (code_point << 4) | (hex_digit - '0');
                }
                else if (hex_digit >= 'a' && hex_digit <= 'f')
                {
                    code_point = (code_point << 4) | (hex_digit - 'a' + 10);
                }
                else if (hex_digit >= 'A' && hex_digit <= 'F')
                {
                    code_point = (code_point << 4) | (hex_digit - 'A' + 10);
                }
                else
                {
                    // Invalid hex digit
                    return L"";
                }
            }
            result += code_point;
            i += 5;
        }
        else
        {
            result += str[i];
        }
    }
    return result;
}

std::string Utils::ReadFile(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        // Failed to open file
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<std::string> Utils::JsonArrayToStringVector(const json& array)
{
    std::vector<std::string> result;
    for (json::const_iterator it = array.begin(); it != array.end(); ++it)
    {
        if (it->is_string())
        {
            result.push_back(it->get<std::string>());
        }
    }
    return result;
}

std::vector<std::string> Utils::JsonDictToStringVector(const json& array)
{
    std::vector<std::string> keys;
    try
    {
        // 遍历JSON对象，将所有键的名字存储在vector中
        for (auto it = array.begin(); it != array.end(); ++it)
        {
            keys.push_back(it.key());
        }
    }
    catch (const std::exception& e)
    {
        // 解析异常处理
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return keys;
}

std::vector<std::string> Utils::GetFilesWithExt(const std::string& folder, const std::string& ext)
{
    std::vector<std::string> files = UFile::GetFilesInDirectory(folder);

    std::vector<std::string> result;

    // 找到所有匹配的文件
    for (const auto& file : files)
    {
        if (UFile::EndsWith(file, ext))
        {
            result.push_back(file);
        }
    }

    return result;
}

std::string Utils::GetDefaultIfEmpty(const std::vector<std::string>& vec, const std::string& defaultValue)
{
    if (vec.empty())
    {
        return defaultValue;
    }
    else
    {
        return vec[0];
    }
}

void Utils::OpenFileManager(const std::string& path)
{
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

    if (result != 0)
    {
        // Failed to open the file manager
        std::cerr << "Error: Failed to open file manager." << std::endl;
    }
}

std::string Utils::GetPlatform()
{
#if defined(_WIN32) || defined(_WIN64)
    return "Windows";
#elif defined(__linux__)
        return "Linux";
#elif defined(__APPLE__) && defined(__MACH__)
        return "macOS";
#elif defined(__FreeBSD__)
        return "FreeBSD";
#elif defined(sunos) || defined(_SUNOS)
        return "Solaris";
#else
        return "Unknown";
#endif
}

int Utils::WStringSize(const std::string& str)
{
    // Convert UTF-8 std::string to std::wstring
    static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring wideStr = converter.from_bytes(str);

    // Return the size of the wide string
    return static_cast<int>(wideStr.size());
}

std::string Utils::WStringInsert(const std::string& str, int pos, const std::string& insertStr)
{
    static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring wideStr = converter.from_bytes(str);

    // Insert the wide string into the original string
    wideStr.insert(pos, converter.from_bytes(insertStr));

    // Convert the wide string back to UTF-8
    return converter.to_bytes(wideStr);
}

std::string StringExecutor::_Markdown(const std::string& text)
{
    static std::regex pattern(R"(```([\s\S]*?)```)", std::regex::icase); // 使用 [\s\S] 匹配任意字符
    std::string result;
    std::smatch match;
    std::string::const_iterator searchStart(text.cbegin());

    // 提取所有代码块内容
    while (std::regex_search(searchStart, text.cend(), match, pattern))
    {
        result += match[1].str(); // 提取代码块内容
        searchStart = match.suffix().first; // 移动到匹配结果之后的位置
    }

    // 如果没有找到代码块，返回原始文本
    if (result.empty())
    {
        return text;
    }

    return result; // 返回提取的代码块内容
}


void StringExecutor::_WriteToFile(std::string filename, const std::string& content)
{
    static std::regex newline_pattern("\n");
    static std::regex space_pattern(" ");

    // 替换换行符为指定字符串
    filename = std::regex_replace(filename, newline_pattern, "[Write to " + filename + "]");
    // 移除所有空格
    filename = std::regex_replace(filename, space_pattern, "");

    std::string fileParent = std::filesystem::path(filename).parent_path().string();
    if (!fileParent.empty())
        UDirectory::CreateDirIfNotExists(fileParent);
    std::ofstream outfile(filename, ios::out);
    if (outfile.is_open())
    {
        outfile << content;
        outfile.close();
        LogInfo("数据已保存到文件 {0}", filename);
    }
    else
    {
        LogError("无法打开文件 {0}", filename);
    }
}

std::string StringExecutor::trim(const std::string& s)
{
    const char* whitespace = " \t\n\r";
    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(whitespace);
    return s.substr(start, end - start + 1);
}

//res,erasePart
std::pair<std::string, std::string> StringExecutor::EraseInRange(const std::string& str1, const std::string& str2,
                                                                 const std::string& text)
{
    std::regex pattern(str1 + R"([\x01-\xFF]*?)" + str2);
    std::smatch match;
    std::string res = text;
    std::string erasePart;
    if (std::regex_search(text, match, pattern))
    {
        erasePart = match[0].str();
        res = std::regex_replace(text, pattern, "");
    }
    return {res, erasePart};
}

bool StringExecutor::HasMarkdown(const std::string& str)
{
    // 创建正则表达式，匹配以[Markdown]开始和结束的内容
    static const std::regex markdownPattern(R"(\[Markdown\]([\x01-\xFF]*?)\[Markdown\])", std::regex::icase);

    // 判断是否匹配正则表达式
    return std::regex_match(str, markdownPattern);
}

std::string StringExecutor::ExtractMarkdownContent(const std::string& str)
{
    // 创建正则表达式，捕获[Markdown]之间的内容
    static const std::regex contentPattern(R"(\[Markdown\]([\x01-\xFF]*?)\[Markdown\])", std::regex::icase);
    std::smatch matches;

    // 执行匹配并提取内容
    if (std::regex_search(str, matches, contentPattern) && matches.size() > 1)
    {
        return matches[1].str();
    }

    // 如果不匹配，返回原始字符串
    return str;
}

void StringExecutor::AddDrawCallback(const DrawCallback& callback)
{
    drawCallback = callback;
}

std::string StringExecutor::AutoExecute(std::string text, const std::shared_ptr<ChatBot>& bot)
{
    auto [res, part] = EraseInRange("<think>", "</think>", text);

    return part + "\n" + Python(Draw(CMDWithOutput(CMD(File(Process(PreProcess(res, bot)))))));
}

std::string StringExecutor::CMD(const std::string& text)
{
    static std::regex pattern(R"(\[Command\]([\x01-\xFF]*?)\[Command\])", std::regex::icase);
    std::vector<std::string> commands;
    std::smatch match;

    // 使用 std::regex_iterator 匹配所有的命令
    auto words_begin = std::sregex_iterator(text.begin(), text.end(), pattern);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
    {
        match = *i;
        commands.push_back(match[1].str());
    }

    // 依次执行命令并替换原始命令
    std::string replacedAnswer = text;
    for (const auto& command : commands)
    {
        std::string processedCommand = _Markdown(command);
        Utils::AsyncExecuteShell(processedCommand, {});

        replacedAnswer = std::regex_replace(replacedAnswer, pattern, "[Fin]",
                                            std::regex_constants::format_first_only);
    }

    return replacedAnswer;
}

std::string StringExecutor::CMDWithOutput(const std::string& text)
{
    static std::regex pattern(R"(\[CommandWithOutput\]([\x01-\xFF]*?)\[CommandWithOutput\])", std::regex::icase);
    std::vector<std::string> commands;
    std::smatch match;

    // 使用 std::regex_iterator 匹配所有的命令
    auto words_begin = std::sregex_iterator(text.begin(), text.end(), pattern);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
    {
        match = *i;
        commands.push_back(match[1].str());
    }

    // 依次执行命令并替换原始命令
    std::string replacedAnswer = text;
    for (const auto& command : commands)
    {
        std::string processedCommand = _Markdown(command);
        std::string res = Utils::ExecuteShell(processedCommand);

        replacedAnswer = std::regex_replace(replacedAnswer, pattern, "res",
                                            std::regex_constants::format_first_only);
    }

    return replacedAnswer;
}

std::string StringExecutor::Draw(const std::string& text)
{
    // 修改正则表达式，使用非贪婪匹配并支持多行模式
    static std::regex pattern1(R"(\[Draw\]([\s\S]*?)\[Draw\])");
    static std::regex pattern2(R"(\[Positive\]([\s\S]*?)\[Positive\])");
    static std::regex pattern3(R"(\[Negative\]([\s\S]*?)\[Negative\])");

    std::string replacedAnswer = text;
    if (!drawCallback)
        return replacedAnswer;

    try
    {
        smatch match;
        auto draw_begin = std::sregex_iterator(text.begin(), text.end(), pattern1);
        auto draw_end = std::sregex_iterator();

        // 遍历所有匹配的[Draw]...[Draw]块
        for (std::sregex_iterator i = draw_begin; i != draw_end; ++i)
        {
            match = *i;
            std::string drawBlock = match[1].str();

            // 初始化正负面提示词
            std::string positive;
            std::string negative;

            // 提取正面提示词
            smatch positiveMatch;
            if (std::regex_search(drawBlock, positiveMatch, pattern2))
            {
                positive = positiveMatch[1].str(); // 捕获正面内容
            }

            // 提取负面提示词
            smatch negativeMatch;
            if (std::regex_search(drawBlock, negativeMatch, pattern3))
            {
                negative = negativeMatch[1].str(); // 捕获负面内容
            }

            // 调用回调函数处理提取的内容
            if (drawCallback)
            {
                drawCallback(positive, Utils::GetCurrentTimestamp(), false, negative);
            }
        }
    }
    catch (std::exception& e)
    {
        LogError("Error: {0}", e.what());
    }

    // 替换已处理的[Draw]块
    replacedAnswer = std::regex_replace(replacedAnswer, pattern1, "[Drawing]",
                                        std::regex_constants::format_first_only);

    return replacedAnswer; // 添加返回语句，之前的代码缺少返回值
}

std::string StringExecutor::File(const std::string& text)
{
    static std::regex pattern1(R"(\[File\]([\x01-\xFF]*?)\[File\])");
    static std::regex pattern2(R"(\[Path\]([\x01-\xFF]*?)\[Path\])");
    static std::regex pattern3(R"(\[Content\]([\x01-\xFF]*?)\[Content\])");

    std::string replacedAnswer = text;

    try
    {
        auto file_begin = std::sregex_iterator(text.begin(), text.end(), pattern1);
        auto file_end = std::sregex_iterator();
        for (std::sregex_iterator file_iter = file_begin; file_iter != file_end; ++file_iter)
        {
            std::string fileContent = file_iter->str();

            // 匹配路径和内容
            std::vector<std::string> paths;
            std::vector<std::string> contents;

            auto path_begin = std::sregex_iterator(fileContent.begin(), fileContent.end(), pattern2);
            auto path_end = std::sregex_iterator();
            for (std::sregex_iterator path_iter = path_begin; path_iter != path_end; ++path_iter)
            {
                paths.push_back(path_iter->str(1)); // 捕获路径
            }

            auto content_begin = std::sregex_iterator(fileContent.begin(), fileContent.end(), pattern3);
            auto content_end = std::sregex_iterator();
            for (std::sregex_iterator content_iter = content_begin; content_iter != content_end; ++content_iter)
            {
                contents.push_back(content_iter->str(1)); // 捕获内容
            }

            // 确保路径和内容一一对应
            if (paths.size() == contents.size())
            {
                for (size_t i = 0; i < paths.size(); ++i)
                {
                    _WriteToFile(paths[i], contents[i]); // 写入文件
                }
            }
            else
            {
                LogError("Mismatch between paths and contents count");
            }

            // 移除匹配的 [File] 内容
            replacedAnswer = std::regex_replace(replacedAnswer, pattern1, "...",
                                                std::regex_constants::format_first_only);
        }
    }
    catch (const std::exception& e)
    {
        LogError("Error: {0}", e.what());
    }

    return replacedAnswer;
}


std::string StringExecutor::Python(const std::string& text)
{
    static std::regex pattern(R"(\[Python\]([\x01-\xFF]*?)\[Python\])", std::regex::icase);
    static std::string tmpPythonPath = "Runtime/pythons/";
    static int num = 0;
    if (num == 0)
    {
        if (UDirectory::Exists(tmpPythonPath))
            UDirectory::Remove(tmpPythonPath);
    }
    std::string replacedAnswer = text;
    std::smatch match;
    std::vector<std::string> pythonMatches;
    try
    {
        auto python_begin = std::sregex_iterator(text.begin(), text.end(), pattern);
        auto python_end = std::sregex_iterator();
        for (std::sregex_iterator i = python_begin; i != python_end; ++i)
        {
            match = *i;
            pythonMatches.push_back(match[1].str());
        }

        for (const auto& pythonMatch : pythonMatches)
        {
            std::string processedPy = _Markdown(pythonMatch);
            std::string pyPath = tmpPythonPath + fmt::format("temp_{0}.py", num++);
            _WriteToFile(pyPath, processedPy);
            auto res = Application::ExecutePython(pyPath);
            replacedAnswer = std::regex_replace(replacedAnswer, pattern, res,
                                                std::regex_constants::format_first_only);
        }
    }
    catch (const std::exception& e)
    {
        LogError("Error: {0}", e.what());
    }
    return replacedAnswer;
}

std::string StringExecutor::Process(const std::string& text)
{
    static std::regex pattern1(R"(\[Process\]([\x01-\xFF]*?)\[Process\])");
    static std::regex pattern2(R"(\[Output\]([\x01-\xFF]*?)\[Output\])");
    static std::regex pythonPattern(R"(\[Python\]([\x01-\xFF]*?)\[Python\])");
    std::string replacedAnswer = text;
    std::smatch match;


    try
    {
        auto process_begin = std::sregex_iterator(text.begin(), text.end(), pattern1);
        auto process_end = std::sregex_iterator();

        for (std::sregex_iterator i = process_begin; i != process_end; ++i)
        {
            match = *i;
            std::string processBlock = match[1].str();

            // 查找所有的 Output
            std::vector<std::string> outputs;
            auto output_begin = std::sregex_iterator(processBlock.begin(), processBlock.end(), pattern2);
            for (std::sregex_iterator j = output_begin; j != std::sregex_iterator(); ++j)
            {
                outputs.push_back(j->str(1));
            }

            // 查找所有的 Python 代码
            std::vector<std::string> pythonCommands;
            auto python_begin = std::sregex_iterator(processBlock.begin(), processBlock.end(), pythonPattern);
            for (std::sregex_iterator j = python_begin; j != std::sregex_iterator(); ++j)
            {
                pythonCommands.push_back(j->str(0));
            }

            // 确保每个 Output 和 Python 代码一一对应
            size_t count = min(outputs.size(), pythonCommands.size());
            for (size_t k = 0; k < count; ++k)
            {
                // 执行 Python 命令并获取结果
                auto res = Python(pythonCommands[k]);
                // 将结果写入相应的文件
                _WriteToFile(outputs[k], res);
            }

            // 替换已处理的内容
            replacedAnswer = std::regex_replace(replacedAnswer, pattern1, "...",
                                                std::regex_constants::format_first_only);
        }
    }
    catch (const std::exception& e)
    {
        LogError("Error: {0}", e.what());
    }

    return replacedAnswer;
}

std::string StringExecutor::PreProcess(const std::string& text, const std::shared_ptr<ChatBot>& bot)
{
    static std::regex pattern(R"(\[Reading\]([\x01-\xFF]*?)\[Reading\])");
    std::string replacedAnswer = text;
    std::smatch match;
    try
    {
        auto reading_begin = std::sregex_iterator(text.begin(), text.end(), pattern);
        auto reading_end = std::sregex_iterator();
        for (std::sregex_iterator i = reading_begin; i != reading_end; ++i)
        {
            match = *i;
            std::string processedReading = _Markdown(match[1].str());
            auto res = Python(processedReading);
            replacedAnswer = std::regex_replace(replacedAnswer, pattern, res,
                                                std::regex_constants::format_first_only);
            replacedAnswer = bot->Submit(replacedAnswer, Utils::GetCurrentTimestamp());
        }
    }
    catch (const std::exception& e)
    {
        LogError("Error: {0}", e.what());
    }
    return replacedAnswer;
}

std::vector<StringExecutor::Code> StringExecutor::GetCodes(std::string text)
{
    std::vector<Code> codes;
    // Regex to match the outer [Code] ... [Code] block.
    static const std::regex codeBlockPattern(R"(\[Code\]([\x01-\xFF]*?)\[Code\])", std::regex::optimize);

    std::smatch codeBlockMatch;
    auto [subText, erasePart] = EraseInRange("<think>", "</think>", text);
    text = subText;
    // Iterator to search through the text string.
    std::string::const_iterator searchStart(text.cbegin());
    auto removeSubstring = [](const std::string& str, const std::string& toRemove)
    {
        std::string result = str;
        size_t pos = 0;

        // 查找并去除指定的子字符串
        while ((pos = result.find(toRemove)) != std::string::npos)
        {
            result.erase(pos, toRemove.length());
        }

        return result;
    };
    try
    {
        while (std::regex_search(searchStart, text.cend(), codeBlockMatch, codeBlockPattern))
        {
            std::string codeBlock(codeBlockMatch[1].first, codeBlockMatch[1].second);
            std::istringstream iss(codeBlock);

            std::string line;

            // Temporary variables to hold current code segment values.
            Code currentCode;
            bool inContentBlock = false;
            std::ostringstream contentBuffer;

            while (std::getline(iss, line))
            {
                line = trim(line);
                if (line == "[Language]")
                {
                    if (!currentCode.Type.empty() || !currentCode.Content.empty())
                    {
                        if (inContentBlock)
                        {
                            currentCode.Content.push_back(contentBuffer.str());
                            contentBuffer.str("");
                            inContentBlock = false;
                        }
                        codes.push_back(currentCode);
                        currentCode = Code();
                    }
                    continue;
                }
                else if (line == "[Content]")
                {
                    if (inContentBlock)
                    {
                        currentCode.Content.push_back(contentBuffer.str());
                        contentBuffer.str("");
                        inContentBlock = false;
                    }
                    else
                    {
                        inContentBlock = true;
                    }
                    continue;
                }
                else
                {
                    if (currentCode.Type.empty())
                    {
                        currentCode.Type = removeSubstring(line, "[Language]");
                    }
                    else if (inContentBlock)
                    {
                        contentBuffer << line << "\n";
                    }
                }
            }
            // At the end of the inner block, if there is an incomplete content block, flush it.
            if (inContentBlock && !contentBuffer.str().empty())
            {
                currentCode.Content.push_back(contentBuffer.str());
            }
            if (!currentCode.Type.empty() || !currentCode.Content.empty())
            {
                codes.push_back(currentCode);
            }
            // Move the iterator past this match.
            searchStart = codeBlockMatch.suffix().first;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during parsing: " << e.what() << std::endl;
    }

    return codes;
}


void UImage::Base64ToImage(const std::string& str_base64, const std::string& dstPath)
{
    std::ofstream out(dstPath, std::ios::binary);
    std::vector<unsigned char> image_data = Utils::Str2VUChar(base64_decode(str_base64));
    out.write(reinterpret_cast<const char*>(image_data.data()), image_data.size());
    out.close();
}

GLuint UImage::Base64ToTextureFromPath(const std::string& imgPath)
{
    std::ifstream in(imgPath);
    if (!in)
    {
        std::cerr << "Failed to open file: " << imgPath << std::endl;
        return 0;
    }

    // Read the base64 data from the file
    std::string base64Str;
    in >> base64Str;

    return Base64ToTexture(base64Str);
}

GLuint UImage::Base64ToTexture(const std::string& base64_str)
{
    std::vector<unsigned char> image_decode = Utils::Str2VUChar(base64_decode(base64_str));
    int width, height, channels;
    unsigned char* pixels = stbi_load_from_memory(image_decode.data(), static_cast<int>(image_decode.size()), &width,
                                                  &height, &channels, 0);
    if (!pixels)
    {
        LogError("Failed to load image from base64 data");
        return 0;
    }

    GLuint texture_id = CreateGLTexture(pixels, width, height, channels);
    stbi_image_free(pixels);

    return texture_id;
}

std::string UImage::Img2Base64(const std::string& path)
{
    std::ifstream fin(path, std::ios::binary);
    if (!fin)
    {
        LogError("Failed to open file: {}", path);
        return "";
    }

    fin.seekg(0, std::ios::end);
    size_t file_size = fin.tellg();
    fin.seekg(0, std::ios::beg);

    std::vector<char> buffer(file_size);
    fin.read(buffer.data(), file_size);

    return base64_encode(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size());
}

GLuint UImage::Img2Texture(const std::string& imgPath)
{
    auto metaPath = imgPath + ".meta";
    std::optional<GLuint> texture_id;
    if (!UFile::Exists(metaPath))
    {
        int width, height, channels;
        unsigned char* pixels = stbi_load(imgPath.c_str(), &width, &height, &channels, 0);
        if (!pixels)
        {
            LogError("Failed to load image from path: {0}", imgPath);
            return 0;
        }

        Meta meta;
        GLuint imageFormat;
        //根据颜色通道数，判断颜色格式。
        switch (channels)
        {
        case 1:
            {
                imageFormat = GL_ALPHA;
                break;
            }
        case 3:
            {
                imageFormat = GL_RGB;
                meta.TextureFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
                break;
            }
        case 4:
            {
                imageFormat = GL_RGBA;
                meta.TextureFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                break;
            }
        }

        texture_id = CreateGLTexture(pixels, width, height, channels);

        glGenTextures(1, &texture_id.value());
        glBindTexture(GL_TEXTURE_2D, texture_id.value());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        //3. 将图片rgb数据上传到GPU;并进行压缩。
        glTexImage2D(GL_TEXTURE_2D, 0, meta.TextureFormat, width, height, 0, imageFormat, GL_UNSIGNED_BYTE,
                     pixels);
        //1. 获取压缩是否成功
        GLint compress_success = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &compress_success);
        //2. 获取压缩好的纹理数据尺寸
        GLint compress_size = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compress_size);
        //3. 获取具体的纹理压缩格式
        GLint compress_format = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &compress_format);
        //4. 从GPU中，将显存中保存的压缩好的纹理数据，下载到内存
        void* img = malloc(compress_size);
        glGetCompressedTexImage(GL_TEXTURE_2D, 0, img);
        //5. 保存为文件
        std::ofstream output_file_stream(metaPath, std::ios::out | std::ios::binary);


        meta.MetaType[0] = 'm';
        meta.MetaType[1] = 'e';
        meta.MetaType[2] = 't';
        meta.MetaType[3] = 'a';
        meta.MipmapLevel = 0;
        meta.Width = width;
        meta.Height = height;
        meta.TextureFormat = compress_format;
        meta.CompressSize = compress_size;

        output_file_stream.write((char*)&meta, sizeof(Meta));
        output_file_stream.write((char*)img, compress_size);
        output_file_stream.close();
        stbi_image_free(pixels);
    }
    else
    {
        return LoadFromMeta(metaPath);
    }

    return texture_id.value_or(0);
}

GLuint UImage::CreateGLTexture(const unsigned char* image_data, int width, int height, int channels, int newWidth,
                               int newHeight)
{
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image_data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return texture_id;
}

void UImage::ImgResize(const std::string& base64_str, float scale, const std::string& path)
{
    std::vector<unsigned char> image_decode = Utils::Str2VUChar(base64_decode(base64_str));
    int width, height, channels;
    unsigned char* pixels = stbi_load_from_memory(image_decode.data(), static_cast<int>(image_decode.size()), &width,
                                                  &height, &channels, 0);
    if (!pixels)
    {
        LogError("Failed to load image from path: {0}", path);
        return;
    }
    int new_width = width * scale;
    int new_height = height * scale;

    unsigned char* resized_data = new unsigned char[new_width * new_height * channels];

    stbir_resize_uint8(pixels, width, height, 0,
                       resized_data, new_width, new_height, 0, channels);

    stbi_write_png(path.c_str(), new_width, new_height, channels, resized_data, 0);

    stbi_image_free(pixels);
    delete[] resized_data;
}

GLuint UImage::LoadFromMeta(const std::string& filename)
{
    Meta* texture2d = new Meta();
    GLuint textureID;

    //读取 cpt 压缩纹理文件
    std::ifstream ifs(filename, std::ios::in | std::ios::binary);
    Meta meta;
    ifs.read((char*)&meta, sizeof(Meta));

    unsigned char* data = (unsigned char*)malloc(meta.CompressSize);
    ifs.read((char*)data, meta.CompressSize);
    ifs.close();

    texture2d->TextureFormat = meta.TextureFormat;
    texture2d->Width = meta.Width;
    texture2d->Height = meta.Height;


    //1. 通知显卡创建纹理对象，返回句柄;
    glGenTextures(1, &(textureID));

    //2. 将纹理绑定到特定纹理目标;
    glBindTexture(GL_TEXTURE_2D, textureID);
    {
        //3. 将压缩纹理数据上传到GPU;
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, texture2d->TextureFormat, texture2d->Width, texture2d->Height,
                               0, meta.CompressSize, data);
    }

    //4. 指定放大，缩小滤波方式，线性滤波，即放大缩小的插值方式;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    delete (data);

    return textureID;
}

GLuint UImage::CreateGLTexture(const unsigned char* image_data, int width, int height, int channels)
{
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image_data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return texture_id;
}
