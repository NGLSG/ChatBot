cmake_minimum_required(VERSION 3.10)

project(ChatBot)

set(CMAKE_CXX_STANDARD 20)
add_definitions(-w)

# 添加依赖库
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PortAudio  CONFIG REQUIRED)

# 添加源文件
set(SOURCES
        ../src/ChatBot.cpp
        ../src/Logger.cpp
        main.cpp
        )

# 添加头文件
set(HEADERS
        ../include/ChatBot.h
        ../include/utils.h
        ../include/Logger.h

        ../src/VoiceToText.cpp ../include/VoiceToText.h ../src/Translate.cpp ../include/Translate.h ../src/Recorder.cpp ../include/Recorder.h ../include/Recorder.h)

# 添加可执行文件
add_executable(ChatBot ${SOURCES} ${HEADERS})
# 添加头文件目录
target_include_directories(ChatBot PRIVATE ../include)
# 链接依赖库
target_link_libraries(ChatBot
        PRIVATE portaudio
        PRIVATE nlohmann_json::nlohmann_json
        PRIVATE cpr::cpr
        PRIVATE spdlog::spdlog spdlog::spdlog_header_only
        PRIVATE OpenSSL::SSL OpenSSL::Crypto)
