#include <Logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>
#include <mutex>

// 修改后 - 使用函数内静态变量确保初始化
std::mutex& getInitMutex() {
    static std::mutex s_InitMutex;
    return s_InitMutex;
}

void Logger::Init() {
    static bool initialized = []() {
         std::lock_guard<std::mutex> lock(getInitMutex());

        try {
            // 确保日志目录存在
            std::filesystem::create_directories("Logs");

            // 创建日志接收器
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

            // 创建日志文件名
            std::string logFileName = "Logs/" + Stamp2Time(getCurrentTimestamp(), true) + ".log";
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName, false);

            // 创建日志器实例 - 先不设置pattern
            s_CoreLogger = std::make_shared<spdlog::logger>("Logger", spdlog::sinks_init_list{console_sink, file_sink});

            // 单独设置日志级别
            s_CoreLogger->set_level(spdlog::level::debug);

            // 直接对日志器设置pattern，避免使用全局设置
            // 这里是问题所在，不要使用全局的spdlog::set_pattern
            s_CoreLogger->set_pattern("%^[%T] %n: %v%$");

            // 注册到spdlog
            spdlog::register_logger(s_CoreLogger);
        }
        catch (const std::exception& e) {
            // 初始化失败时，尝试创建一个基本的控制台日志器
            s_CoreLogger = spdlog::stdout_color_mt("Logger");
            s_CoreLogger->critical("日志系统初始化失败: {}", e.what());
        }

        return true;
    }();
}