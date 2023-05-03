cmake_minimum_required(VERSION 3.18)

project(ChatBot)

set(CMAKE_CXX_STANDARD 20)
add_definitions(-w)

# 添加依赖库
find_package(sndfile REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PortAudio CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(LibArchive REQUIRED)
# 添加源文件
set(SOURCES
        ../../3rdPart/ImGuiColorTextEdit/TextEditor.cpp
        ../src/utils.cpp
        ../src/ChatBot.cpp
        ../src/Logger.cpp
        ../src/VoiceToText.cpp
        ../src/Translate.cpp
        ../src/Recorder.cpp
        ../src/stb_image.cpp
        main.cpp
        Application.cpp
        )

# 添加头文件
set(HEADERS
        ../../3rdPart/ImGuiColorTextEdit/TextEditor.h
        ../include/utils.h
        ../include/ChatBot.h
        ../include/Logger.h
        ../include/VoiceToText.h
        ../include/Translate.h
        ../include/Recorder.h
        ../include/Recorder.h
        ../src/stb_image.cpp
        ../include/stb_image.h
        ../include/imgui_markdown.h
        ../include/Configure.h
        ../include/Progress.hpp
        Application.h
        ../include/pch.h)

# 添加可执行文件
add_executable(ChatBot ${SOURCES} ${HEADERS})
target_precompile_headers(ChatBot PRIVATE ../include/pch.h)
# 添加头文件目录
target_include_directories(ChatBot PRIVATE ../include)
# 链接依赖库
target_link_libraries(ChatBot
        PRIVATE portaudio
        PRIVATE ${SNDFILE_LIBRARIES}
        PRIVATE OpenGL::GL
        PRIVATE LibArchive::LibArchive
        PRIVATE yaml-cpp
        PRIVATE nlohmann_json::nlohmann_json
        PRIVATE imgui::imgui
        PRIVATE cpr::cpr
        PRIVATE spdlog::spdlog spdlog::spdlog_header_only
        PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# 添加资源文件
target_sources(ChatBot PRIVATE
        Resources/Info.rc
        Resources/Info.plist
        Resources/icon.png)

# 指定资源文件输出目录
set_target_properties(ChatBot PROPERTIES
        RESOURCE "Resources/Info.rc"
        RESOURCE "Resources/Info.plist"
        RESOURCE "Resources/icon.png"
        )

# 指定资源文件输出目录
set_target_properties(ChatBot PROPERTIES
        VS_RESOURCE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Resources"
        XCODE_ATTRIBUTE_RESOURCE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Resources"
        )
