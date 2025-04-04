project(libs)
add_library(libs INTERFACE)

find_package(OpenGL REQUIRED)
find_package(glad REQUIRED)
find_package(Vulkan REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

project(ImGUI)
file(GLOB IMGUI_SOURCES ./imgui/*.cpp ./imgui/*.h ./imgui/backends/imgui_impl_opengl3.* ./imgui/backends/imgui_impl_glfw.* ./imgui/backends/imgui_impl_sdl2.* ./imgui/backends/imgui_impl_vulkan.*)
add_library(ImGUI STATIC
        ${IMGUI_SOURCES}
)
target_link_libraries(ImGUI PUBLIC OpenGL::GL glad::glad glfw ${Vulkan_LIBRARIES}
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
)
target_include_directories(ImGUI PUBLIC ./imgui ./imgui/backends ${Vulkan_INCLUDE_DIRS})
