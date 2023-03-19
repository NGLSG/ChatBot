#pragma once

#include "utils.h"
#include <memory.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

const std::string LogFile = "Bot.log";

namespace Log {
    class Logger {
    public:
        static void Init();

        static std::shared_ptr<spdlog::logger> &GetCoreLogger() { return s_CoreLogger; } //返回内核日志
    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger; //内核日志
    };

    //日志
#define Trace(...)        Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define Info(...)         Logger::GetCoreLogger()->info(__VA_ARGS__)
#define Warn(...)         Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define Error(...)        Logger::GetCoreLogger()->error(__VA_ARGS__)
#define Fatal(...)        Logger::GetCoreLogger()->critical(__VA_ARGS__)
}
