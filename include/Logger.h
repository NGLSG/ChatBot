#ifndef LOGGER_H
#define LOGGER_H

#include "pch.h"
#include <memory.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

const std::string LogFile = "Bot.log";

class Logger {
public:
    static void Init();

    static std::shared_ptr<spdlog::logger> &GetCoreLogger() { return s_CoreLogger; } //返回内核日志

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