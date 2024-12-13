cmake_minimum_required(VERSION 3.18)

project(CyberGirl)

set(CMAKE_CXX_STANDARD 20)
add_definitions(-w)
add_subdirectory(Vendor/libarchive)
# 添加依赖库
find_package(sndfile REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(PortAudio CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(sol2 REQUIRED)
find_package(Lua REQUIRED)
find_package(Stb REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(glad REQUIRED)
find_package(OpenSSL REQUIRED)
file(GLOB SOURCES
        ../src/*.c*
        *.c*
)

file(GLOB HEADERS
        *.h*
        ../include/*.h*
)
add_definitions(-DNOCRYPT)
add_executable(CyberGirl ${SOURCES} ${HEADERS})
target_include_directories(CyberGirl PRIVATE ../include Vendor/spdlog/include PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(CyberGirl
        PRIVATE portaudio
        PRIVATE ${SNDFILE_LIBRARIES}
        PRIVATE OpenGL::GL
        PRIVATE archive
        PRIVATE yaml-cpp::yaml-cpp
        PRIVATE nlohmann_json::nlohmann_json
        PRIVATE imgui::imgui
        PRIVATE glad::glad
        PRIVATE cpr::cpr
        PRIVATE ${Stb_INCLUDE_DIR}
        PRIVATE ${LUA_LIBRARIES} sol2
        PRIVATE OpenSSL::SSL OpenSSL::Crypto
        PRIVATE glfw
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        PRIVATE $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>)

# 添加资源文件
target_sources(CyberGirl PRIVATE
        Resources/Info.rc
        Resources/Info.plist
        Resources/icon.png)

if (MSVC)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE "/bigobj" "/utf-8")
endif ()


# 将Resources目录下的所有文件和子目录匹配并添加到资源变量中
file(GLOB_RECURSE RESOURCES "Resources/*")

# 为目标添加资源文件
set_target_properties(CyberGirl PROPERTIES
        RESOURCE "${RESOURCES}"
)
# 指定资源文件输出目录
set_target_properties(CyberGirl PROPERTIES
        VS_RESOURCE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Resources"
        XCODE_ATTRIBUTE_RESOURCE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Resources"
)
