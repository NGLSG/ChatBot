#ifndef LOGGER_H
#define LOGGER_H

#include "pch.h"
#include <memory.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class Logger {
public:
    static void Init();

    static std::shared_ptr<spdlog::logger> &GetCoreLogger() { return s_CoreLogger; } //返回内核日志

    static long long getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    static std::string Stamp2Time(long long int timestamp, bool nospace = false) {
        int ms = timestamp % 1000;//取毫秒
        time_t tick = (time_t) (timestamp / 1000);//转换时间
        struct tm tm;
        char s[40];
        tm = *localtime(&tick);
        if (!nospace) {
            strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &tm);
        } else {
            strftime(s, sizeof(s), "%Y%m%d%H%M%S", &tm);
        }
        std::string str(s);
        str = str + " " + std::to_string(ms);
        return str;
    }

private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger; //内核日志
};

//日志
#define LogTrace(...)        Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define LogInfo(...)         Logger::GetCoreLogger()->info(__VA_ARGS__)
#define LogWarn(...)         Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define LogError(...)        Logger::GetCoreLogger()->error(__VA_ARGS__)
#define LogFatal(...)        Logger::GetCoreLogger()->critical(__VA_ARGS__)

#endif