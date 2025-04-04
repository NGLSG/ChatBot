#ifndef LOGGER_H
#define LOGGER_H

#include <chrono>
#include <spdlog/spdlog.h>
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

    static std::string Stamp2Time(size_t timestamp, bool nospace = false) {
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
    inline static std::shared_ptr<spdlog::logger> s_CoreLogger; //内核日志
};

//日志
#define LogTrace(...) SPDLOG_LOGGER_TRACE(::Logger::GetCoreLogger(), __VA_ARGS__)
#define LogInfo(...) SPDLOG_LOGGER_INFO(::Logger::GetCoreLogger(), __VA_ARGS__)
#define LogWarn(...) SPDLOG_LOGGER_WARN(::Logger::GetCoreLogger(), __VA_ARGS__)
#define LogError(...) SPDLOG_LOGGER_ERROR(::Logger::GetCoreLogger(), __VA_ARGS__)
#define LogFatal(...) SPDLOG_LOGGER_CRITICAL(::Logger::GetCoreLogger(), __VA_ARGS__)


#endif