cmake_minimum_required(VERSION 3.31.0)
project(execute VERSION 0.1.0 LANGUAGES C CXX)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# Include directories for GLEW and OpenGL
include_directories(
    ${OPENGL_INCLUDE_DIRS}
)

# Link directories if necessary
link_directories(${CMAKE_SOURCE_DIR}/lib)

# Add executable and source files
add_executable(execute
    src/main.cpp
)

set(IMGUI_DIR include/imgui-master)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(execute
    glfw
    imgui
    ${OPENGL_LIBRARIES}
    GLEW::GLEW
)