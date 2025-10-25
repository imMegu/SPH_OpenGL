cmake_minimum_required(VERSION 3.31.0)
project(execute VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For LSP configuration

file(COPY "${CMAKE_SOURCE_DIR}/src/shaders/" DESTINATION "${CMAKE_BINARY_DIR}/src/")

# Find necessary libraries
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

set(IMGUI_DIR include/imgui-master)

add_executable(execute
    src/main.cpp
    src/gui.cpp
    src/shader.cpp
    src/simulation.cpp
    src/utilities.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Specify include directories for all headers
target_include_directories(execute PUBLIC
    . # local headers
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${OPENGL_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
)

target_link_libraries(execute PRIVATE
    glfw
    ${OPENGL_LIBRARIES}
    GLEW::GLEW
)
