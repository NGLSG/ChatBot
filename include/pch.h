#pragma once
#ifndef PCH_H
#define PCH_H

#include <fstream>
#include <string>
#include <filesystem>
#include <iostream>
#include <map>
#include <sndfile.h>
#include <nlohmann/json.hpp>
#include <openssl/md5.h>
#include <portaudio.h>
#include <regex>
#include <archive.h>
#include <archive_entry.h>
#include "cpr/cpr.h"

#define FRAMES_PER_BUFFER (1024)
const int kMaxThreads = 10; // 最大线程数
const int kBlockSize = 1024 * 1024; // 每个块的大小
const int kRetryTimes = 3; // 下载失败的重试次数

typedef struct {
    float *buffer;
    int bufferSize;
    int readIndex;
} paUserData;


template<typename T>
T Min(T a, T b) {
    return a < b ? a : b;
}

template<typename T>
T Max(T a, T b) {
    return a > b ? a : b;
}

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

#endif
