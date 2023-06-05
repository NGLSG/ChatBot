
#include "utils.h"

bool NoRecord = false;
std::mutex mtx;

bool UFile::Exists(const std::string &filename) {
    std::ifstream file(filename);
    return file.good();
}

bool UDirectory::Create(const std::string &dirname) {
    try {
        std::filesystem::create_directory(dirname);
        return true;
    } catch (std::filesystem::filesystem_error &e) {
        std::cerr << "LogError creating directory: " << e.what() << std::endl;
        return false;
    }
}

bool UDirectory::Exists(const std::string &dirname) {
    return std::filesystem::is_directory(dirname);
}

std::string UEncrypt::ToMD5(const std::string &str) {
    unsigned char md[16];
    MD5((const unsigned char *) str.c_str(), str.length(), md);
    char buf[33] = {'\0'};
    for (int i = 0; i < 16; ++i) {
        sprintf(buf + i * 2, "%02x", md[i]);
    }
    return std::string(buf);
}

std::string UEncrypt::GetMD5(const std::string &str) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((const unsigned char *) str.c_str(), str.size(), digest);
    char md5_str[2 * MD5_DIGEST_LENGTH + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(md5_str + 2 * i, "%02x", digest[i]);
    }
    md5_str[2 * MD5_DIGEST_LENGTH] = '\0';
    return std::string(md5_str);
}

std::string UEncrypt::GetMD5(const void *data, std::size_t size) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((const unsigned char *) data, size, digest);
    char md5_str[2 * MD5_DIGEST_LENGTH + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(md5_str + 2 * i, "%02x", digest[i]);
    }
    md5_str[2 * MD5_DIGEST_LENGTH] = '\0';
    return std::string(md5_str);
}

int Utils::paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
    paUserData *data = (paUserData *) userData;
    const float *in = data->data + data->position;
    float *out = (float *) outputBuffer;
    int framesLeft = data->length - data->position;
    int framesToCopy = framesPerBuffer;
    if (framesToCopy > framesLeft) {
        framesToCopy = framesLeft;
    }
    for (int i = 0; i < framesToCopy; i++) {
        out[i] = in[i];
    }
    data->position += framesToCopy;
    return framesToCopy < framesPerBuffer ? paComplete : paContinue;
}

std::string Utils::GetAbsolutePath(const std::string &relativePath) {
    // 将相对路径转换为std::filesystem::path类型的路径
    std::filesystem::path path(relativePath);

    // 将相对路径转换为绝对路径
    std::filesystem::path absolutePath = std::filesystem::absolute(path);

    return absolutePath.generic_string();
}

std::string Utils::execAsync(const std::string &command) {
    // 异步执行命令并返回输出结果
    std::future<std::string> resultFuture = std::async(std::launch::async, [&command]() {
        return exec(command);
    });
    // 等待异步任务完成并返回结果
    return resultFuture.get();
}

std::string Utils::exec(const std::string &command) {
    std::string output;
    FILE *pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Error: Failed to execute command " + command);
        return "";
    }
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    _pclose(pipe);
    return output;
}

void Utils::playAudioAsync(const std::string &filename, std::function<void()> callback) {
    // 异步执行音频播放
    std::thread([&]() {
        // 打开音频文件
        NoRecord = true;
        SF_INFO info;
        SNDFILE *file = sf_open("tmp.wav", SFM_READ, &info);
        if (!file) {
            std::cerr << "Failed to open file: tmp.wav" << std::endl;
            return;
        }

        // 读取音频数据
        int sampleRate = info.samplerate;
        int channels = info.channels;
        int frames = info.frames;
        float *data = new float[frames * channels];
        sf_readf_float(file, data, frames);
        sf_close(file);

        // 初始化PortAudio
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "Failed to initialize PortAudio: " << Pa_GetErrorText(err) << std::endl;
            delete[] data;
            return;
        }

        // 打开音频流
        paUserData userData = {data, 0, frames * channels};
        PaStream *stream;
        err = Pa_OpenDefaultStream(&stream, channels, channels, paFloat32, sampleRate, FRAMES_PER_BUFFER, paCallback,
                                   &userData);
        if (err != paNoError) {
            std::cerr << "Failed to open audio stream: " << Pa_GetErrorText(err) << std::endl;
            delete[] data;
            Pa_Terminate();
            return;
        }

        // 开始播放音频
        err = Pa_StartStream(stream);
        if (err != paNoError) {
            std::cerr << "Failed to start audio stream: " << Pa_GetErrorText(err) << std::endl;
            delete[] data;
            Pa_CloseStream(stream);
            Pa_Terminate();
            return;
        }

        // 等待音频播放完成
        while (Pa_IsStreamActive(stream)) {
            Pa_Sleep(100);
        }
        NoRecord = false;
        // 关闭音频流并清理资源
        err = Pa_StopStream(stream);
        if (err != paNoError) {
            std::cerr << "Failed to stop audio stream: " << Pa_GetErrorText(err) << std::endl;
        }
        Pa_CloseStream(stream);
        Pa_Terminate();
        delete[] data;

        // 调用回调函数
        if (callback) {
            callback();
        }
    }).join();
}

void Utils::playAudio(const char *filename) {
    // 打开音频文件
    SF_INFO info;
    SNDFILE *file = sf_open(filename, SFM_READ, &info);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    // 读取音频数据
    int sampleRate = info.samplerate;
    int channels = info.channels;
    int frames = info.frames;
    float *data = new float[frames * channels];
    sf_readf_float(file, data, frames);
    sf_close(file);

    // 初始化PortAudio

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "Failed to initialize PortAudio: " << Pa_GetErrorText(err) << std::endl;
        delete[] data;
        return;
    }

    // 打开音频流
    paUserData userData = {data, 0, frames * channels};
    PaStream *stream;
    err = Pa_OpenDefaultStream(&stream, channels, channels, paFloat32, sampleRate, FRAMES_PER_BUFFER, paCallback,
                               &userData);
    if (err != paNoError) {
        std::cerr << "Failed to open audio stream: " << Pa_GetErrorText(err) << std::endl;
        delete[] data;
        Pa_Terminate();
        return;
    }

    // 开始播放音频
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Failed to start audio stream: " << Pa_GetErrorText(err) << std::endl;
        delete[] data;
        Pa_CloseStream(stream);
        Pa_Terminate();
        return;
    }

    // 等待音频播放完毕
    while (Pa_IsStreamActive(stream)) {
        Pa_Sleep(100);
    }

    // 停止音频流并清理资源
    err = Pa_StopStream(stream);
    if (err != paNoError) {
        std::cerr << "Failed to stop audio stream: " << Pa_GetErrorText(err) << std::endl;
    }
    Pa_CloseStream(stream);
    Pa_Terminate();
    delete[] data;
}

void Utils::SaveYaml(const std::string &filename, const YAML::Node &node) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("LogError: Failed to open file " + filename);
        return;
    }
    file << node;
    file.close();
}

size_t Utils::write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t realsize = size * nmemb;
    std::string *response_data = (std::string *) userdata;
    response_data->append(ptr, realsize);
    return realsize;
}

std::string Utils::GetErrorString(cpr::ErrorCode code) {
    static const std::map<cpr::ErrorCode, std::string> error_map = {
            {cpr::ErrorCode::OK,                           "OK"},
            {cpr::ErrorCode::CONNECTION_FAILURE,           "CONNECTION_FAILURE"},
            {cpr::ErrorCode::EMPTY_RESPONSE,               "EMPTY_RESPONSE"},
            {cpr::ErrorCode::HOST_RESOLUTION_FAILURE,      "HOST_RESOLUTION_FAILURE"},
            {cpr::ErrorCode::INTERNAL_ERROR,               "INTERNAL_ERROR"},
            {cpr::ErrorCode::INVALID_URL_FORMAT,           "INVALID_URL_FORMAT"},
            {cpr::ErrorCode::NETWORK_RECEIVE_ERROR,        "NETWORK_RECEIVE_ERROR"},
            {cpr::ErrorCode::NETWORK_SEND_FAILURE,         "NETWORK_SEND_FAILURE"},
            {cpr::ErrorCode::OPERATION_TIMEDOUT,           "OPERATION_TIMEDOUT"},
            {cpr::ErrorCode::PROXY_RESOLUTION_FAILURE,     "PROXY_RESOLUTION_FAILURE"},
            {cpr::ErrorCode::SSL_CONNECT_ERROR,            "SSL_CONNECT_ERROR"},
            {cpr::ErrorCode::SSL_LOCAL_CERTIFICATE_ERROR,  "SSL_LOCAL_CERTIFICATE_ERROR"},
            {cpr::ErrorCode::SSL_REMOTE_CERTIFICATE_ERROR, "SSL_REMOTE_CERTIFICATE_ERROR"},
            {cpr::ErrorCode::SSL_CACERT_ERROR,             "SSL_CACERT_ERROR"},
            {cpr::ErrorCode::GENERIC_SSL_ERROR,            "GENERIC_SSL_ERROR"},
            {cpr::ErrorCode::UNSUPPORTED_PROTOCOL,         "UNSUPPORTED_PROTOCOL"},
            {cpr::ErrorCode::REQUEST_CANCELLED,            "REQUEST_CANCELLED"},
            {cpr::ErrorCode::TOO_MANY_REDIRECTS,           "TOO_MANY_REDIRECTS"},
            {cpr::ErrorCode::UNKNOWN_ERROR,                "UNKNOWN_ERROR"}
    };

    auto it = error_map.find(code);
    if (it != error_map.end()) {
        return it->second;
    } else {
        return "UNKNOWN_ERROR";
    }

}

std::string Utils::GetLatestUrl(const std::string &url) {
    // 发送HTTP请求
    cpr::Response response = cpr::Get(cpr::Url{url});

    // 解析版本号和URL
    std::string tag, download_url;
    if (response.status_code == 200) {
        size_t pos = response.text.find("\"tag_name\":");
        if (pos != std::string::npos) {
            pos += 11;
            size_t endpos = response.text.find_first_of(",\n", pos);
            if (endpos != std::string::npos) {
                tag = response.text.substr(pos, endpos - pos);
            } else {
                tag = response.text.substr(pos);
            }
            tag = tag.substr(1, tag.size() - 2);
        }

        pos = response.text.find("\"browser_download_url\":");
        if (pos != std::string::npos) {
            pos += 23;
            size_t endpos = response.text.find_first_of(",\n", pos);
            if (endpos != std::string::npos) {
                download_url = response.text.substr(pos, endpos - pos);
            } else {
                download_url = response.text.substr(pos);
            }
            download_url = download_url.substr(1, download_url.size() - 2);
        }
    }

    // 返回URL或者空字符串
    if (!download_url.empty()) {
        return download_url;
    } else {
        return "";
    }
}

std::string Utils::GetFileName(const std::string &dir) {
    // 使用 C++17 标准库中的文件系统库
    std::filesystem::path path(dir);
    // 获取文件夹中最后一个文件名
    return path.filename().string();
}

bool Utils::UDownloads(const std::map<std::string, std::string> &tasks, int num_threads) {
    bool success = true;
    for (const auto &url_file_pair: tasks) {
        if (!UDownload(url_file_pair, num_threads)) {
            success = false;
        }
    }
    return success;
}

bool Utils::UDownload(const std::pair<std::string, std::string> &task, int num_threads) {
    try {
        // 获取文件大小
        auto head = cpr::Head(cpr::Url{task.first});
        if (head.status_code != 200) {
            LogError("Downloading Error: Failed to get file size: " + head.error.message);
            return false;
        }
        int file_size = stoi(head.header["Content-Length"]);

        // 判断是否支持分块下载
        bool support_range =
                head.header.find("Accept-Ranges") != head.header.end() && head.header["Accept-Ranges"] == "bytes";
        if (!support_range) {
            LogWarn("Downloading Warning: The server does not support range requests, cannot download in multiple threads.");
            num_threads = 1;
        }

        // 下载文件
        std::string file_path = task.second;
        std::ofstream file(file_path, std::ios::binary | std::ios::out);
        if (!file.is_open()) {
            LogError("Downloading Error: Failed to open file " + file_path);
            return false;
        }

        std::vector<int> progress(num_threads, 0);
        std::vector<std::thread> threads(num_threads);
        int block_size = ceil((double) file_size / num_threads);

        for (int i = 0; i < num_threads; i++) {
            int start = i * block_size;
            int end = min((i + 1) * block_size - 1, file_size - 1);
            threads[i] = std::thread(DownloadThread, task.first, file_path, start, end, i, std::ref(progress));
        }

        // 显示进度条
        ProgressBar progressBar(file_size);
        LogInfo("Downloading {0} to {1} size:{2}", task.first, file_path, ProgressBar::formatSize(file_size));
        auto start_time = std::chrono::high_resolution_clock::now();
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            int total_progress = 0;
            for (int i = 0; i < num_threads; i++) {
                total_progress += progress[i];
            }
            auto current_time = std::chrono::high_resolution_clock::now();
            double elapsed_time =
                    std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() / 1000.0;
            double speed = total_progress / elapsed_time;
            double remaining_time = (file_size - total_progress) / speed;
            progressBar.update(total_progress, "Downloaded " + ProgressBar::formatSize(total_progress) + " of " +
                                               ProgressBar::formatSize(file_size) + " (" +
                                               ProgressBar::formatTime(remaining_time) + " left)");
            if (total_progress == file_size) {
                break;
            }
        }
        progressBar.end();
        for (int i = 0; i < num_threads; i++) {
            threads[i].join();
        }
        if (!CheckFileSize(file_path, file_size)) {
            LogFatal("Downloading Error: Failed to download file {0}", task.first);
            return false;
        }

        LogInfo("Downloaded {0} to {1} successful!", task.first, file_path);
        return true;
    } catch (const std::exception &e) {
        LogError("Exception in Download: " + std::string(e.what()));
        return false;
    }
}

bool Utils::DownloadThread(const std::string &url, const std::string &file_path, int start, int end, int id,
                           std::vector<int> &progress) {
    // 设置请求头
    cpr::Header headers = {{"Range", "bytes=" + std::to_string(start) + "-" + std::to_string(end)}};

    // 发送请求
    int retry = 0;
    while (retry < 3) {
        auto response = cpr::Get(cpr::Url{url}, headers, cpr::VerifySsl{false}, cpr::Redirect{true});
        if (response.status_code == 206) {
            std::ofstream file(file_path, std::ios::binary | std::ios::out | std::ios::in);
            if (!file.is_open()) {
                LogError("Downloading Error: Failed to open file " + file_path);
                return false;
            }
            file.seekp(start);
            file.write(response.text.c_str(), response.text.length());
            progress[id] = response.text.length();
            return true;
        }
        retry++;
    }

    LogError("Downloading Error: Failed to download file {0} in thread {1}", url, std::to_string(id));
    return false;
}

bool Utils::CheckFileSize(const std::string &file_path, int expected_size) {
    try {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            LogError("Failed to open file " + file_path);
            return false;
        }
        int file_size = file.tellg();
        if (file_size != expected_size) {
            LogError("Downloaded file size does not match expected size.");
            return false;
        }
        return true;
    } catch (const std::exception &e) {
        LogError("Exception in CheckFileSize: " + std::string(e.what()));
        return false;
    }
}

bool Utils::Decompress(std::string file, std::string path) {
    if (path.empty()) {
        path = GetDirName(GetAbsolutePath(file));
    }

    // 获取文件扩展名
    std::string ext = GetFileExt(file);

    // 根据扩展名调用相应的解压缩函数
    if (ext == "7z") {
        return UCompression::Decompress7z(file, path);
    } else if (ext == "zip") {
        return UCompression::DecompressZip(file, path);
    } else if (ext == "gz") {
        return UCompression::DecompressGz(file, path);
    } else if (ext == "xz") {
        return UCompression::DecompressXz(file, path);
    } else if (ext == "rar") {
        return UCompression::DecompressRar(file, path);
    } else {
        LogError("Unsupported file extension: {0}", ext);
        return false;
    }
}

std::string Utils::GetDirName(const std::string &dir) {
    std::filesystem::path path(dir);
    return path.parent_path().string() + "/";
}

std::string Utils::GetFileExt(std::string file) {
    // 获取文件名中最后一个'.'的位置
    size_t dotPos = file.find_last_of('.');
    if (dotPos == std::string::npos) {
        // 没有找到'.'，返回空字符串
        return "";
    } else {
        // 返回'.'后面的字符串作为扩展名
        return file.substr(dotPos + 1);
    }
}

bool UCompression::Decompress7z(std::string file, std::string path) {
    // 打开7z文件
    struct archive *a = archive_read_new();
    if (!a) {
        LogError("Failed to create archive object");
        return false;
    }
    archive_read_support_format_7zip(a);
    int r = archive_read_open_filename(a, file.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        LogError("Failed to open 7z file: {0}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    // 解压缩7z文件中的每个文件
    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string filename = archive_entry_pathname(entry);
        std::string outfile = path + filename;
        struct archive *ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
        archive_write_disk_set_standard_lookup(ext);
        archive_entry_set_pathname(entry, outfile.c_str());
        r = archive_read_extract(a, entry, 0);
        if (r != ARCHIVE_OK) {
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
bool UCompression::DecompressZip(std::string file, std::string path) {
    // 打开zip文件
    struct archive *a = archive_read_new();
    if (!a) {
        LogError("Failed to create archive object");
        return false;
    }
    archive_read_support_format_zip(a);
    int r = archive_read_open_filename(a, file.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        LogError("Failed to open zip file: {0}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    // 解压缩zip文件中的每个文件
    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string filename = archive_entry_pathname(entry);
        std::string outfile = path + filename;
        struct archive *ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
        archive_write_disk_set_standard_lookup(ext);
        archive_entry_set_pathname(entry, outfile.c_str());
        r = archive_read_extract(a, entry, 0);
        if (r != ARCHIVE_OK) {
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
bool UCompression::DecompressGz(std::string file, std::string path) {
    // 打开tar.gz文件
    struct archive *a = archive_read_new();
    if (!a) {
        LogError("Failed to create archive object");
        return false;
    }
    archive_read_support_filter_gzip(a);
    archive_read_support_format_tar(a);
    int r = archive_read_open_filename(a, file.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        LogError("Failed to open tar.gz file: {0}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    // 解压缩tar.gz文件中的每个文件
    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string filename = archive_entry_pathname(entry);
        std::string outfile = path + filename;
        struct archive *ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
        archive_write_disk_set_standard_lookup(ext);
        archive_entry_set_pathname(entry, outfile.c_str());
        r = archive_read_extract(a, entry, 0);
        if (r != ARCHIVE_OK) {
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
bool UCompression::DecompressXz(std::string file, std::string path) {
    // 打开tar.xz文件
    struct archive *a = archive_read_new();
    if (!a) {
        LogError("Failed to create archive object");
        return false;
    }
    archive_read_support_filter_xz(a);
    archive_read_support_format_tar(a);
    int r = archive_read_open_filename(a, file.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        LogError("Failed to open tar.xz file: {0}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    // 解压缩tar.xz文件中的每个文件
    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string filename = archive_entry_pathname(entry);
        std::string outfile = path + filename;
        struct archive *ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
        archive_write_disk_set_standard_lookup(ext);
        archive_entry_set_pathname(entry, outfile.c_str());
        r = archive_read_extract(a, entry, 0);
        if (r != ARCHIVE_OK) {
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

bool UCompression::DecompressRar(std::string file, std::string path) {
    // 打开RAR文件
    struct archive *a = archive_read_new();
    if (!a) {
        LogError("Failed to create archive object");
        return false;
    }
    archive_read_support_format_rar(a);
    int r = archive_read_open_filename(a, file.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        LogError("Failed to open RAR file: {0}", archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    // 解压缩RAR文件中的每个文件
    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string filename = archive_entry_pathname(entry);
        std::string outfile = path + filename;
        struct archive *ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
        archive_write_disk_set_standard_lookup(ext);
        archive_entry_set_pathname(entry, outfile.c_str());
        r = archive_read_extract(a, entry, 0);
        if (r != ARCHIVE_OK) {
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

#ifdef WIN32

#include <windows.h>
#include <tchar.h>
#include <string>
#include <thread>

void Utils::OpenProgram(const char *path) {
    std::thread worker([=]() {
        STARTUPINFO si{};
        PROCESS_INFORMATION pi{};

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;

        HANDLE g_hChildStd_IN_Rd = nullptr;
        HANDLE g_hChildStd_IN_Wr = nullptr;
        HANDLE g_hChildStd_OUT_Rd = nullptr;
        HANDLE g_hChildStd_OUT_Wr = nullptr;

        if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0)) {
            std::cerr << "Failed to create output pipe" << std::endl;
            return;
        }

        if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &sa, 0)) {
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
                           const_cast<char *>(cmdLine.c_str()),
                           nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
            std::cerr << "Failed to open process" << std::endl;
            return;
        }

        std::cout << "Open process successfully" << std::endl;

        WaitForSingleObject(pi.hProcess, INFINITE);

        std::cout << "Process finished" << std::endl;

        CloseHandle(g_hChildStd_IN_Rd);
        CloseHandle(g_hChildStd_IN_Wr);
        CloseHandle(g_hChildStd_OUT_Rd);
        CloseHandle(g_hChildStd_OUT_Wr);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    });
    worker.detach();
}

std::vector<std::string> Utils::GetDirectories(const std::string &path) {
    std::vector<std::string> dirs;
    dirs.push_back("empty");
    for (auto &entry: std::filesystem::directory_iterator(path)) {
        if (entry.is_directory()) {
            dirs.push_back(GetAbsolutePath(entry.path().string()));
        }
    }
    return dirs;
}

std::vector<std::string> Utils::GetAllCodesFromText(const std::string &text) {
    std::regex codeRegex("`{3}([\\s\\S]*?)`{3}");
    std::vector<std::string> codeStrings;
    std::smatch matchResult;
    std::string::const_iterator searchStart(text.cbegin());
    while (std::regex_search(searchStart, text.cend(), matchResult, codeRegex)) {
        searchStart = matchResult.suffix().first;
        if (matchResult.size() >= 2) {
            std::string codeString = matchResult[1].str();
            codeStrings.push_back(codeString);
        }
    }

    return codeStrings;
}

std::string Utils::ExtractNormalText(const std::string &text) {
    std::regex codeRegex("```[^`]*?\\n([\\s\\S]*?)\\n```"); // 匹配代码块
    std::string normalText = text;
    std::sregex_iterator codeBlockIterator(text.cbegin(), text.cend(), codeRegex);
    std::sregex_iterator endIterator;

    for (auto it = codeBlockIterator; it != endIterator; ++it) {
        std::string codeBlock = it->str();
        std::size_t position = normalText.find(codeBlock);
        if (position != std::string::npos) // 如果找到匹配的代码块，则将其替换为普通文本
        {
            normalText.replace(position, codeBlock.length(), ""); // 将代码块替换为空字符串
        }
    }

    return normalText;
}


#else
void Utils::OpenProgram(const char *path){}
#endif