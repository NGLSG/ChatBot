#include <Logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>
#include <mutex>

static std::mutex s_InitMutex;

void Logger::Init() {
    static bool initialized = []() {
        std::lock_guard<std::mutex> lock(s_InitMutex);

        try {
            // 创建日志目录
            std::filesystem::create_directories("Logs");

            // 创建控制台输出接收器
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

            // 创建文件输出接收器
            std::string logFileName = "Logs/" + Stamp2Time(getCurrentTimestamp(), true) + ".log";
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName, false);

            // 创建日志记录器，包含两个接收器
            s_CoreLogger = std::make_shared<spdlog::logger>("Logger", spdlog::sinks_init_list{console_sink, file_sink});

            // 设置日志级别
            s_CoreLogger->set_level(spdlog::level::debug);

            // 设置格式模板，添加堆栈跟踪信息
            // %@ - 源文件路径
            // %# - 行号
            // %! - 函数名
            s_CoreLogger->set_pattern("%^[%T] %n: [%@:%#] [%!] %v%$");

            // 注册日志记录器
            spdlog::register_logger(s_CoreLogger);
        }
        catch (const std::exception& e) {
            s_CoreLogger = spdlog::stdout_color_mt("Logger");
            s_CoreLogger->critical("日志系统初始化失败: {}", e.what());
        }

        return true;
    }();
}