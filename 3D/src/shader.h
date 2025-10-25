#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>

const std::string updateShaderSource = "/src/shaders/update.compute";
const std::string densityShaderSource = "/src/shaders/density.compute";
const std::string externalShaderSource = "/src/shaders/external.compute";
const std::string partitionShaderSource = "/src/shaders/partition.compute";
const std::string sortShaderSource = "/src/shaders/sort.compute";
const std::string offsetsShaderSource = "/src/shaders/offsets.compute";

const std::string vertexShaderSource = "/src/shaders/vertex.vs";
const std::string fragmentShaderSource = "/src/shaders/fragment.fs";
const std::string geometryShaderSource = "/src/shaders/geometry.gs";

const std::string cubeFragmentSource = "/src/shaders/cube_f.fs";
const std::string cubeVertexSource = "/src/shaders/cube_v.vs";

GLuint createRenderProgram(const std::string &vertexPath,
                           const std::string &fragmentPath,
                           const std::string &geometryPath);
GLuint createRenderProgram(const std::string &vertexPath,
                           const std::string &fragmentPath);
GLuint createLineProgram(const std::string &vertexPath,
                         const std::string &fragmentPath);
GLuint CompileShader(const std::string &relative_path, GLenum shader_type,
                     const std::string &shader_name);
void checkCompileErrors(GLuint shader, std::string type);
void setMat4(GLuint ID, const std::string &name, const glm::mat4 &mat);
void setFloat(GLuint ID, const std::string &name, float f);
void setInt(GLuint ID, const std::string &name, int f);
void setVec3(GLuint ID, const std::string &name, const glm::vec3 &value);
