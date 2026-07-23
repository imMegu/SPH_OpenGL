#pragma once
#include <GL/glew.h>
#include <string>

// Shader paths, relative to the executable's directory. CMake copies the
// shader sources next to the binary at build time.

// SPH compute pipeline
const std::string externalShaderSource = "src/shaders/simulation/external.compute";
const std::string countShaderSource = "src/shaders/simulation/count.compute";
const std::string scanBlocksShaderSource = "src/shaders/simulation/scan_blocks.compute";
const std::string scanSumsShaderSource = "src/shaders/simulation/scan_sums.compute";
const std::string scanAddShaderSource = "src/shaders/simulation/scan_add.compute";
const std::string scatterShaderSource = "src/shaders/simulation/scatter.compute";
const std::string densityShaderSource = "src/shaders/simulation/density.compute";
const std::string updateShaderSource = "src/shaders/simulation/update.compute";

// Scene geometry (particle impostors, bounding cube, floor)
const std::string vertexShaderSource = "src/shaders/scene/particle.vs";
const std::string fragmentShaderSource = "src/shaders/scene/particle.fs";

const std::string cubeVertexSource = "src/shaders/scene/cube.vs";
const std::string cubeFragmentSource = "src/shaders/scene/cube.fs";

const std::string floorVertexSource = "src/shaders/scene/floor.vs";
const std::string floorFragmentSource = "src/shaders/scene/floor.fs";

// Screen-space fluid rendering
const std::string fullscreenVertexSource = "src/shaders/fluid/fullscreen.vs";
const std::string fluidDepthFragmentSource = "src/shaders/fluid/depth.fs";
const std::string thicknessFragmentSource = "src/shaders/fluid/thickness.fs";
const std::string blurFragmentSource = "src/shaders/fluid/blur.fs";
const std::string compositeFragmentSource = "src/shaders/fluid/composite.fs";

GLuint createRenderProgram(const std::string &vertexPath,
                           const std::string &fragmentPath);
GLuint createLineProgram(const std::string &vertexPath,
                         const std::string &fragmentPath);
GLuint CompileShader(const std::string &relative_path, GLenum shader_type,
                     const std::string &shader_name);
void checkCompileErrors(GLuint shader, std::string type);
