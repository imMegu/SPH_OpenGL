#pragma once
#include <GL/glew.h>
#include <string>

// Shader paths, relative to the executable's directory. CMake copies the
// shader sources next to the binary at build time.
const std::string updateShaderSource = "src/shaders/update.compute";
const std::string densityShaderSource = "src/shaders/density.compute";
const std::string externalShaderSource = "src/shaders/external.compute";
const std::string partitionShaderSource = "src/shaders/partition.compute";
const std::string sortShaderSource = "src/shaders/sort.compute";
const std::string sortLocalShaderSource = "src/shaders/sort_local.compute";
const std::string offsetsShaderSource = "src/shaders/offsets.compute";

const std::string vertexShaderSource = "src/shaders/vertex.vs";
const std::string fragmentShaderSource = "src/shaders/fragment.fs";

const std::string cubeFragmentSource = "src/shaders/cube_f.fs";
const std::string cubeVertexSource = "src/shaders/cube_v.vs";

GLuint createRenderProgram(const std::string &vertexPath,
                           const std::string &fragmentPath);
GLuint createLineProgram(const std::string &vertexPath,
                         const std::string &fragmentPath);
GLuint CompileShader(const std::string &relative_path, GLenum shader_type,
                     const std::string &shader_name);
void checkCompileErrors(GLuint shader, std::string type);
