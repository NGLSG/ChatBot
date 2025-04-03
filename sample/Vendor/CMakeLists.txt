project(libs)
add_library(libs INTERFACE)

find_package(OpenGL REQUIRED)
find_package(glad REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

project(ImGUI)
file(GLOB IMGUI_SOURCES ./imgui/*.cpp ./imgui/*.h ./imgui/backends/imgui_impl_opengl3.* ./imgui/backends/imgui_impl_glfw.*)
add_library(ImGUI STATIC
        ${IMGUI_SOURCES}
)
target_link_libraries(ImGUI PUBLIC OpenGL::GL glad::glad glfw)
target_include_directories(ImGUI PUBLIC ./imgui ./imgui/backends)
