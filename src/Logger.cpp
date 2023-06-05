#include <Logger.h>
#include <spdlog/sinks/basic_file_sink.h>

std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;

void Logger::Init() {
    static bool isInit = false;
    if (!isInit) {
        isInit = true;
        spdlog::set_pattern("%^[%T] %n: %v%$");
        auto console_sink = CreateRef<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = CreateRef<spdlog::sinks::basic_file_sink_mt>(
                "Logs/" + Stamp2Time(getCurrentTimestamp(), true) + ".log", false);
        s_CoreLogger = CreateRef<spdlog::logger>("Logger", spdlog::sinks_init_list{console_sink, file_sink});
        s_CoreLogger->set_level(spdlog::level::debug);
    }
}
