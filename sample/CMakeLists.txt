cmake_minimum_required(VERSION 3.18)

project(CyberGirl)

set(CMAKE_CXX_STANDARD 20)

# 设置输出目录，将所有文件放在编译目录的Chatbot文件夹下
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/Chatbot")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")  # 可执行文件输出位置
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Bin")  # 动态库输出位置
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Lib")  # 静态库输出位置
set(CMAKE_PDB_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Bin")  # 调试符号文件位置

# 确保各种构建模式下的输出路径一致
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}/Bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}/Lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}/Bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}/Lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_DIR}/Bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_DIR}/Lib")

# 添加子目录
add_subdirectory(Vendor/libarchive)
add_subdirectory(Vendor/llama.cpp)
add_subdirectory(Vendor/spdlog)
add_subdirectory(Vendor)

# 依赖库查找
find_package(sndfile REQUIRED)
find_package(Vulkan REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(PortAudio CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(sol2 REQUIRED)
find_package(Lua REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
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
include_directories(${Vulkan_INCLUDE_DIRS})
target_include_directories(CyberGirl PRIVATE
        ../include
        Vendor/llava
        PRIVATE ${LUA_INCLUDE_DIR}
)

# 对于 llama 静态库部分
set_target_properties(llama ggml ggml-base PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF
        GGML_VULKAN ON
        # 确保这些库也输出到正确位置
        ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Lib"
        LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Bin"
        RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

target_link_libraries(CyberGirl
        PRIVATE ${Vulkan_LIBRARIES}
        PRIVATE ggml
        PRIVATE llama
        PRIVATE portaudio
        PRIVATE ${SNDFILE_LIBRARIES}
        PRIVATE archive
        PRIVATE yaml-cpp::yaml-cpp
        PRIVATE nlohmann_json::nlohmann_json
        PRIVATE ImGUI
        PRIVATE cpr::cpr
        PRIVATE ${LUA_LIBRARIES} sol2
        PRIVATE OpenSSL::SSL OpenSSL::Crypto
        PRIVATE spdlog::spdlog
        PRIVATE CURL::libcurl
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        PRIVATE $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
)

# 添加资源文件
# 根据不同系统和编译器处理资源文件
if(WIN32)
    # Windows系统资源处理
    if(MSVC)
        # Visual Studio编译器
        target_sources(CyberGirl PRIVATE
                Resources/Info.rc
                Resources/icon.ico
        )
        # 为Visual Studio设置图标
        set_property(TARGET CyberGirl PROPERTY VS_DEBUGGER_COMMAND_ARGUMENTS "")
    else()
        # MinGW或其他Windows编译器
        target_sources(CyberGirl PRIVATE
                Resources/Info.rc
        )
        # MinGW需要特殊处理资源编译
        set(CMAKE_RC_COMPILER_INIT windres)
        enable_language(RC)
    endif()
elseif(APPLE)
    # macOS系统资源处理
    set_target_properties(CyberGirl PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Info.plist
            MACOSX_BUNDLE_ICON_FILE "icon.icns"
            MACOSX_BUNDLE_BUNDLE_NAME "CyberGirl"
    )
    # 添加icns图标到应用包
    target_sources(CyberGirl PRIVATE
            Resources/icon.icns
    )
    # 确保图标文件被正确复制到应用包中
    set_source_files_properties(
            Resources/icon.icns PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
    )
else()
    # Linux系统资源处理
    # Linux通常使用安装规则来处理资源文件
    # 创建.desktop文件
    configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/Resources/CyberGirl.desktop.in
            ${CMAKE_CURRENT_BINARY_DIR}/CyberGirl.desktop
            @ONLY
    )
    # 安装图标和桌面快捷方式
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png
            DESTINATION share/icons/hicolor/128x128/apps
            RENAME cybergirl.png
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CyberGirl.desktop
            DESTINATION share/applications
    )
endif()

# MSVC特定配置
if (MSVC)
    if(WIN32)
        # 根据构建模式设置不同的子系统
        if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
        else()
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
        endif()
    endif()

    # 编译选项
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Debug模式
        target_compile_options(CyberGirl PRIVATE "/W0" "/bigobj" "/utf-8" "/MDd" "/Od" "/Zi" "/RTC1")
    elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        # RelWithDebInfo模式
        target_compile_options(CyberGirl PRIVATE "/W0" "/bigobj" "/utf-8" "/MD" "/O2" "/Zi" "/DEBUG:FULL")
        target_link_options(CyberGirl PRIVATE "/DEBUG:FULL" "/INCREMENTAL:NO")
    else ()
        # Release模式
        target_compile_options(CyberGirl PRIVATE "/W0" "/bigobj" "/utf-8" "/MD" "/O2")
    endif ()
endif ()

# 资源处理
file(GLOB_RECURSE RESOURCES "Resources/*")
set_target_properties(CyberGirl PROPERTIES
        RESOURCE "${RESOURCES}"
        # 确保主可执行文件输出到正确位置
        RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

# 资源文件输出路径设置
set_target_properties(CyberGirl PROPERTIES
        VS_RESOURCE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Resources"
        XCODE_ATTRIBUTE_RESOURCE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Resources"
)

# 创建启动器脚本，添加DLL搜索路径
if(WIN32)
    file(GENERATE OUTPUT "${OUTPUT_DIR}/启动.bat" CONTENT
            "@echo off\nrem 设置DLL搜索路径\nset PATH=%~dp0Bin;%PATH%\nrem 启动应用程序\n\"%~dp0CyberGirl.exe\" %*\n"
    )
endif()

# 在程序中添加代码，使其能够找到Bin目录中的DLL
target_compile_definitions(CyberGirl PRIVATE
        DLL_PATH="${CMAKE_BINARY_DIR}/Chatbot/Bin"
)