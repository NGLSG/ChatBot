cmake_minimum_required(VERSION 3.18)

project(Chatbot)
# 设置CUDA编译器标志和C++标准
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -allow-unsupported-compiler")
set(CMAKE_CXX_STANDARD 20)

# 设置统一输出目录
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/Chatbot")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/lib")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${OUTPUT_DIR}/pdb")

# GPU 加速配置（CUDA > Vulkan > CPU）
option(GGML_CUDA_USE_CUBLASLT "使用 cuBLASLt (需要 CUDA 12+)" ON)
option(GGML_CUDA_FORCE_MMQ "强制使用 CUDA MMQ (如可用)" ON)

# 检测 CUDA
include(CheckLanguage)
check_language(CUDA)
if (CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    # 简化的CUDA版本检测
    if (CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 11.0)
        message(STATUS "【GPU加速】检测到 CUDA (${CMAKE_CUDA_COMPILER_VERSION})")
        set(GGML_CUDA ON CACHE BOOL "启用 CUDA 支持" FORCE)
        set(GGML_CUDA_MMQ_SUPPORT ON CACHE BOOL "启用 CUDA MMQ 支持" FORCE)
    endif ()
    # 获取CUDA路径信息
    get_filename_component(CUDA_BIN_DIR ${CMAKE_CUDA_COMPILER} DIRECTORY)
    get_filename_component(CUDA_ROOT_DIR ${CUDA_BIN_DIR} DIRECTORY)
    set(CUDA_DLL_DIR "${CUDA_ROOT_DIR}/bin")
else ()
    # 尝试使用Vulkan
    find_package(Vulkan QUIET)
    if (Vulkan_FOUND)
        message(STATUS "【GPU加速】未检测到 CUDA，将使用 Vulkan")
        set(GGML_VULKAN ON CACHE BOOL "启用 Vulkan 支持" FORCE)
    else ()
        message(STATUS "【GPU加速】将使用 CPU 模式")
    endif ()
endif ()

# 添加子目录
add_subdirectory(Vendor/libarchive)
add_subdirectory(Vendor/llama.cpp)
add_subdirectory(Vendor/spdlog)
add_subdirectory(Vendor)

# 查找依赖库
find_package(sndfile REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(PortAudio CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(sol2 REQUIRED)
find_package(Lua REQUIRED)
find_package(OpenSSL REQUIRED)

# 收集源文件和头文件
file(GLOB SOURCES
        ../src/*.c*
        *.c*
        ../src/Impls/*.c*
)

file(GLOB HEADERS
        *.h*
        ../include/*.h*
        ../include/Impls/*.h*
)

add_definitions(-DNOCRYPT)

# 创建可执行文件
add_executable(ChatBot ${SOURCES} ${HEADERS})
target_include_directories(ChatBot PRIVATE
        ../include
        Vendor/llava
        ${LUA_INCLUDE_DIR}
)

# 设置llama静态库属性
set_target_properties(llama ggml ggml-base PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF
        ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

if (TARGET ggml-cuda)
    # 设置ggml-cuda的输出路径
    set_target_properties(ggml-cuda PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    )
endif ()

if (TARGET ggml-cpu)
    # 设置ggml-cpu的输出路径
    set_target_properties(ggml-cpu PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    )
endif ()

if (TARGET ggml-vulkan)
    # 设置ggml-vulkan的输出路径
    set_target_properties(ggml-vulkan PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    )
endif ()

# 设置编译定义
if (GGML_CUDA)
    target_compile_definitions(ChatBot PRIVATE HAVE_CUDA=1)
    if (CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
        target_compile_definitions(ChatBot PRIVATE HAVE_CUDA12=1)
    endif ()
elseif (GGML_VULKAN)
    target_compile_definitions(ChatBot PRIVATE HAVE_VULKAN=1)
else ()
    target_compile_definitions(ChatBot PRIVATE CPU_ONLY=1)
endif ()

# 链接依赖库
target_link_libraries(ChatBot PRIVATE
        ggml
        llama
        portaudio
        ${SNDFILE_LIBRARIES}
        archive
        yaml-cpp::yaml-cpp
        nlohmann_json::nlohmann_json
        ImGUI
        cpr::cpr
        ${LUA_LIBRARIES} sol2
        OpenSSL::SSL OpenSSL::Crypto
        spdlog::spdlog
        CURL::libcurl
)

# 简化的资源文件处理
if (WIN32)
    # Windows资源处理
    target_sources(ChatBot PRIVATE Resources/Info.rc)
    if (MSVC)
        target_sources(ChatBot PRIVATE Resources/icon.ico)
    endif ()
elseif (APPLE)
    # macOS资源处理
    set_target_properties(ChatBot PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST Resources/Info.plist
            MACOSX_BUNDLE_ICON_FILE "icon.icns"
    )
    target_sources(ChatBot PRIVATE Resources/icon.icns)
endif ()

# MSVC编译选项
if (MSVC)
    # 简化的MSVC选项
    target_compile_options(ChatBot PRIVATE "/W0" "/bigobj" "/utf-8")

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(ChatBot PRIVATE "/MDd" "/Od" "/Zi")
    else ()
        target_compile_options(ChatBot PRIVATE "/MD" "/O2")
    endif ()
endif ()

# 资源处理
file(GLOB_RECURSE RESOURCES "Resources/*")
set_target_properties(ChatBot PROPERTIES
        RESOURCE "${RESOURCES}"
        RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

# Windows平台下复制MSVC运行时库到输出目录
if(MSVC)
    # 获取Visual Studio安装路径
    include(InstallRequiredSystemLibraries)

    # 添加自定义命令复制MSVC运行库
    add_custom_command(
            TARGET ChatBot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "正在复制MSVC运行库到 ${OUTPUT_DIR} 目录..."
    )

    # 根据不同的构建类型和平台，复制相应的MSVC运行库
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # 复制Debug版本的运行库
        set(MSVC_REDIST_DIR "$ENV{VCToolsRedistDir}/debug_nonredist/x64/Microsoft.VC${MSVC_TOOLSET_VERSION}.DebugCRT")
        set(MSVC_DLLS
                "${MSVC_REDIST_DIR}/msvcp*.dll"
                "${MSVC_REDIST_DIR}/vcruntime*.dll"
                "${MSVC_REDIST_DIR}/ucrtbased.dll"
        )
    else()
        # 复制Release版本的运行库
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            # 64位
            set(MSVC_REDIST_DIR "$ENV{VCToolsRedistDir}/x64/Microsoft.VC${MSVC_TOOLSET_VERSION}.CRT")
        else()
            # 32位
            set(MSVC_REDIST_DIR "$ENV{VCToolsRedistDir}/x86/Microsoft.VC${MSVC_TOOLSET_VERSION}.CRT")
        endif()

        set(MSVC_DLLS
                "${MSVC_REDIST_DIR}/msvcp*.dll"
                "${MSVC_REDIST_DIR}/vcruntime*.dll"
                "${MSVC_REDIST_DIR}/concrt*.dll"
        )
    endif()

    # 复制通用C运行库
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64位UCRT
        set(UCRT_DLL_DIR "$ENV{UniversalCRTSdkDir}/Redist/ucrt/DLLs/x64")
    else()
        # 32位UCRT
        set(UCRT_DLL_DIR "$ENV{UniversalCRTSdkDir}/Redist/ucrt/DLLs/x86")
    endif()

    # 对每个DLL模式执行复制
    foreach(MSVC_DLL_PATTERN ${MSVC_DLLS})
        file(GLOB MATCHING_DLLS "${MSVC_DLL_PATTERN}")
        if(MATCHING_DLLS)
            foreach(DLL_FILE ${MATCHING_DLLS})
                if(EXISTS "${DLL_FILE}")
                    add_custom_command(
                            TARGET ChatBot POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E echo "复制 ${DLL_FILE} 到 ${OUTPUT_DIR}"
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL_FILE}" "${OUTPUT_DIR}"
                    )
                endif()
            endforeach()
        else()
            message(STATUS "未找到匹配的MSVC运行库文件: ${MSVC_DLL_PATTERN}")
        endif()
    endforeach()

    # 添加诊断信息帮助调试
    if(DEFINED ENV{VCToolsRedistDir})
        add_custom_command(
                TARGET ChatBot POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "MSVC重发行目录: $ENV{VCToolsRedistDir}"
        )
    endif()
endif()

if (CMAKE_CUDA_COMPILER)
    # Windows特定处理：复制必要的CUDA DLL到bin目录
    if (WIN32)

        # 定义需要复制的CUDA相关DLL列表
        set(CUDA_DLLS
                "${CUDA_DLL_DIR}/cudart64_*.dll"
                "${CUDA_DLL_DIR}/cublas64_*.dll"
                "${CUDA_DLL_DIR}/cublasLt64_*.dll"
        )

        # 添加自定义命令，在构建后复制CUDA DLL到bin目录
        add_custom_command(
                TARGET ChatBot POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "正在复制CUDA DLL到 ${OUTPUT_DIR} 目录..."
        )

        # 对每个DLL模式执行复制
        foreach (CUDA_DLL_PATTERN ${CUDA_DLLS})
            file(GLOB MATCHING_DLLS "${CUDA_DLL_PATTERN}")
            if (MATCHING_DLLS)
                foreach (DLL_FILE ${MATCHING_DLLS})
                    if (EXISTS "${DLL_FILE}")
                        add_custom_command(
                                TARGET ChatBot POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E echo "复制 ${DLL_FILE} 到 ${OUTPUT_DIR}"
                                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL_FILE}" "${OUTPUT_DIR}"
                        )
                    endif ()
                endforeach ()
            else ()
                message(WARNING "未找到匹配的DLL文件: ${CUDA_DLL_PATTERN}")
            endif ()
        endforeach ()

        # 添加诊断信息帮助调试
        add_custom_command(
                TARGET ChatBot POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "CUDA_DLL_DIR: ${CUDA_DLL_DIR}"
                COMMAND ${CMAKE_COMMAND} -E echo "OUTPUT_DIR: ${OUTPUT_DIR}"
        )
    endif ()
endif ()
# 复制资源文件
add_custom_command(
        TARGET ChatBot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "${OUTPUT_DIR}/Resources"
)