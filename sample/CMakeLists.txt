cmake_minimum_required(VERSION 3.18)

project(CyberGirl)

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
find_package(Stb REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
# 添加源文件
set(SOURCES
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
        ../include/utils.h
        ../include/ChatBot.h
        ../include/Logger.h
        ../include/VoiceToText.h
        ../include/Translate.h
        ../include/Recorder.h
        ../include/Recorder.h
        ../src/stb_image.cpp
        ../include/stb_image.h
        ../include/Configure.h
        ../include/Progress.hpp
        Application.h
        ../include/pch.h
        ../include/imfilebrowser.h
        ../include/base64.h
        ../src/base64.cpp
        ../src/StableDiffusion.cpp
        ../include/StableDiffusion.h
        ../include/stb_image_resize.h
        ../src/stb_image_resize.cpp
        ../include/stb_image_write.h
        ../src/stb_image_wirte.cpp
)

# 添加可执行文件

add_executable(CyberGirl ${SOURCES} ${HEADERS})
target_precompile_headers(CyberGirl PRIVATE ../include/pch.h)
# 添加头文件目录
target_include_directories(CyberGirl PRIVATE ../include)
# 链接依赖库
target_link_libraries(CyberGirl
        PRIVATE portaudio
        PRIVATE ${SNDFILE_LIBRARIES}
        PRIVATE OpenGL::GL
        PRIVATE LibArchive::LibArchive
        PRIVATE yaml-cpp
        PRIVATE nlohmann_json::nlohmann_json
        PRIVATE imgui::imgui
        PRIVATE cpr::cpr
        PRIVATE spdlog::spdlog spdlog::spdlog_header_only
        PRIVATE OpenSSL::SSL OpenSSL::Crypto
        PRIVATE ${Stb_INCLUDE_DIR}
        PRIVATE
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        PRIVATE $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>)

# 添加资源文件
target_sources(CyberGirl PRIVATE
        Resources/Info.rc
        Resources/Info.plist
        Resources/icon.png)

# 指定资源文件输出目录
set_target_properties(CyberGirl PROPERTIES
        RESOURCE "Resources/Info.rc"
        RESOURCE "Resources/Info.plist"
        RESOURCE "Resources/icon.png"
        )

# 指定资源文件输出目录
set_target_properties(CyberGirl PROPERTIES
        VS_RESOURCE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Resources"
        XCODE_ATTRIBUTE_RESOURCE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Resources"
        )
