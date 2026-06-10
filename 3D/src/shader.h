#pragma once
#include <GL/glew.h>
#include <string>

// Shader paths, relative to the executable's directory. CMake copies the
// shader sources next to the binary at build time.
const std::string externalShaderSource = "src/shaders/external.compute";
const std::string countShaderSource = "src/shaders/count.compute";
const std::string scanBlocksShaderSource = "src/shaders/scan_blocks.compute";
const std::string scanSumsShaderSource = "src/shaders/scan_sums.compute";
const std::string scanAddShaderSource = "src/shaders/scan_add.compute";
const std::string scatterShaderSource = "src/shaders/scatter.compute";
const std::string densityShaderSource = "src/shaders/density.compute";
const std::string updateShaderSource = "src/shaders/update.compute";

const std::string vertexShaderSource = "src/shaders/vertex.vs";
const std::string fragmentShaderSource = "src/shaders/fragment.fs";

const std::string cubeFragmentSource = "src/shaders/cube_f.fs";
const std::string cubeVertexSource = "src/shaders/cube_v.vs";

// Screen-space fluid rendering
const std::string fullscreenVertexSource = "src/shaders/fullscreen.vs";
const std::string fluidDepthFragmentSource = "src/shaders/fluid_depth.fs";
const std::string thicknessFragmentSource = "src/shaders/thickness.fs";
const std::string blurFragmentSource = "src/shaders/blur.fs";
const std::string compositeFragmentSource = "src/shaders/composite.fs";

GLuint createRenderProgram(const std::string &vertexPath,
                           const std::string &fragmentPath);
GLuint createLineProgram(const std::string &vertexPath,
                         const std::string &fragmentPath);
GLuint CompileShader(const std::string &relative_path, GLenum shader_type,
                     const std::string &shader_name);
void checkCompileErrors(GLuint shader, std::string type);
