cmake_minimum_required(VERSION 3.18)

project(CyberGirl)
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -allow-unsupported-compiler")
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

# GPU 加速优先级配置 (CUDA12 > CUDA11 > Vulkan > CPU)
# 更新为使用新的选项名称
option(GGML_CUDA_USE_CUBLASLT "使用 cuBLASLt (需要 CUDA 12+)" ON)
option(GGML_CUDA_FORCE_MMQ "强制使用 CUDA MMQ (如可用)" ON)

# 检测 CUDA 并设置优先级
include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    # 启用 CUDA 语言支持
    enable_language(CUDA)

    # 检测 CUDA 版本
    if(CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
        # CUDA 12+
        message(STATUS "【GPU加速】检测到 CUDA 12+ (${CMAKE_CUDA_COMPILER_VERSION})，将优先使用")
        set(GGML_CUDA ON CACHE BOOL "启用 CUDA 支持" FORCE)  # 使用新的选项名称
        set(GGML_CUDA_MMQ_SUPPORT ON CACHE BOOL "启用 CUDA MMQ 支持" FORCE)
    elseif(CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 11.0)
        # CUDA 11+
        message(STATUS "【GPU加速】检测到 CUDA 11+ (${CMAKE_CUDA_COMPILER_VERSION})，将使用")
        set(GGML_CUDA ON CACHE BOOL "启用 CUDA 支持" FORCE)  # 使用新的选项名称
        set(GGML_CUDA_MMQ_SUPPORT ON CACHE BOOL "启用 CUDA MMQ 支持" FORCE)
    else()
        message(WARNING "【GPU加速】检测到较旧的 CUDA 版本 (${CMAKE_CUDA_COMPILER_VERSION})，性能可能不佳")
        set(GGML_CUDA ON CACHE BOOL "启用 CUDA 支持" FORCE)  # 使用新的选项名称
    endif()
else()
    # 如果没有 CUDA，检查是否可以使用 Vulkan
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        message(STATUS "【GPU加速】未检测到 CUDA，将使用 Vulkan 加速")
        set(GGML_VULKAN ON CACHE BOOL "启用 Vulkan 支持" FORCE)
        set(GGML_VULKAN_CHECK_RESULT ON CACHE BOOL "启用 Vulkan 结果检查" FORCE)
    else()
        message(STATUS "【GPU加速】未检测到 GPU 加速选项，将使用 CPU 模式")
    endif()
endif()

# 添加子目录（确保在设置完 GPU 选项之后）
add_subdirectory(Vendor/libarchive)
add_subdirectory(Vendor/llama.cpp)
add_subdirectory(Vendor/spdlog)
add_subdirectory(Vendor)

# 依赖库查找
find_package(sndfile REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(PortAudio CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(sol2 REQUIRED)
find_package(Lua REQUIRED)
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
target_include_directories(CyberGirl PRIVATE
        ../include
        Vendor/llava
        PRIVATE ${LUA_INCLUDE_DIR}
)

# 对于 llama 静态库部分 - 保留原有设置，但不再覆盖 GPU 设置
set_target_properties(llama ggml ggml-base PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF
        # 确保这些库也输出到正确位置
        ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Lib"
        LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Bin"
        RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

# 向主程序添加定义，以便程序知道使用何种后端
if(GGML_CUDA)  # 更新为使用新的选项名称
    target_compile_definitions(CyberGirl PRIVATE HAVE_CUDA=1)
    if(CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
        target_compile_definitions(CyberGirl PRIVATE HAVE_CUDA12=1)
    endif()
elseif(GGML_VULKAN)
    target_compile_definitions(CyberGirl PRIVATE HAVE_VULKAN=1)
else()
    target_compile_definitions(CyberGirl PRIVATE CPU_ONLY=1)
endif()

target_link_libraries(CyberGirl
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