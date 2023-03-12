cmake_minimum_required(VERSION 3.10)

project(ChatBot)

set(CMAKE_CXX_STANDARD 17)

# 添加依赖库
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# 添加源文件
set(SOURCES
        src/ChatBot.cpp
        src/main.cpp
        )

# 添加头文件
set(HEADERS
        include/ChatBot.h
        include/utils.h)

# 添加可执行文件
add_executable(ChatBot ${SOURCES} ${HEADERS})

# 添加头文件目录
target_include_directories(ChatBot PRIVATE include)

# 链接依赖库

target_link_libraries(ChatBot PRIVATE nlohmann_json::nlohmann_json cpr::cpr spdlog::spdlog spdlog::spdlog_header_only)