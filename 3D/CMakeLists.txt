cmake_minimum_required(VERSION 3.13.0)
project(execute VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For LSP configuration

# Default to an optimized build
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Find necessary libraries
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

set(IMGUI_DIR include/imgui-master)

add_executable(execute
    src/main.cpp
    src/gui.cpp
    src/renderer.cpp
    src/shader.cpp
    src/simulation.cpp
    src/utilities.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Copy shaders next to the binary at build time; the executable loads them
# relative to its own location.
add_custom_command(TARGET execute POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/src/shaders" "${CMAKE_BINARY_DIR}/src/shaders"
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
    glm::glm
)
