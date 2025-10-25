#include "shader.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

GLuint CompileShader(const std::string &relative_path, GLenum shader_type,
                     const std::string &shader_name) {
  std::string shader_code;
  try {
    std::filesystem::path shader_path =
        std::filesystem::current_path().parent_path().string() + relative_path;

    std::ifstream shader_file(shader_path);
    if (!shader_file.is_open()) {
      throw std::ios_base::failure("Could not open file: " +
                                   shader_path.string());
    }
    std::stringstream shader_stream;
    shader_stream << shader_file.rdbuf();
    shader_code = shader_stream.str();
    if (shader_code.empty()) {
      std::cerr << "ERROR::SHADER::FILE_IS_EMPTY: " << shader_path.string()
                << std::endl;
      return 0;
    }
  } catch (const std::exception &e) {
    std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what()
              << std::endl;
    return 0;
  }
  const char *shader_code_cstr = shader_code.c_str();
  GLuint shader = glCreateShader(shader_type);
  glShaderSource(shader, 1, &shader_code_cstr, nullptr);
  glCompileShader(shader);
  checkCompileErrors(shader, shader_name);
  return shader;
}

GLuint createRenderProgram(const std::string &vertexPath,
                           const std::string &fragmentPath,
                           const std::string &geometryPath) {
  GLuint vertexShader = CompileShader(vertexPath, GL_VERTEX_SHADER, "VERTEX");
  GLuint fragmentShader =
      CompileShader(fragmentPath, GL_FRAGMENT_SHADER, "FRAGMENT");
  GLuint geometryShader =
      CompileShader(geometryPath, GL_GEOMETRY_SHADER, "GEOMETRY");
  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glAttachShader(program, geometryShader);
  glLinkProgram(program);
  checkCompileErrors(program, "RENDER PROGRAM");
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  glDeleteShader(geometryShader);
  return program;
}

GLuint createRenderProgram(const std::string &vertexPath,
                           const std::string &fragmentPath) {
  GLuint vertexShader = CompileShader(vertexPath, GL_VERTEX_SHADER, "VERTEX");
  GLuint fragmentShader =
      CompileShader(fragmentPath, GL_FRAGMENT_SHADER, "FRAGMENT");

  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);
  checkCompileErrors(program, "RENDER PROGRAM");

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}

GLuint createLineProgram(const std::string &vertexPath,
                         const std::string &fragmentPath) {
  GLuint lineVertexShader =
      CompileShader(vertexPath, GL_VERTEX_SHADER, "WIREFRAME VERTEX");
  GLuint lineFragmentShader =
      CompileShader(fragmentPath, GL_FRAGMENT_SHADER, "WIREFRAME FRAGMENT");
  GLuint program = glCreateProgram();
  glAttachShader(program, lineVertexShader);
  glAttachShader(program, lineFragmentShader);
  glLinkProgram(program);
  checkCompileErrors(program, "WIREFRAME PROGRAM");
  glDeleteShader(lineVertexShader);
  glDeleteShader(lineFragmentShader);
  return program;
}

void checkCompileErrors(GLuint shader, std::string type) {
  GLint success;
  GLchar infoLog[1024];
  if (type.find("PROGRAM") != std::string::npos) {
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(shader, 1024, NULL, infoLog);
      std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type
                << std::endl;
      std::cout << "Program linking error:" << std::endl;
      std::cout << infoLog << std::endl;
      std::cout << "------------------------" << std::endl;
    }
  } else {
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(shader, 1024, NULL, infoLog);
      std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type
                << std::endl;
      std::cout << "Shader compilation error:" << std::endl;
      std::cout << infoLog << std::endl;
      std::cout << "------------------------" << std::endl;
    }
  }
}

void setMat4(GLuint ID, const std::string &name, const glm::mat4 &mat) {
  glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE,
                     &mat[0][0]);
}

void setFloat(GLuint ID, const std::string &name, float f) {
  glUniform1f(glGetUniformLocation(ID, name.c_str()), f);
}

void setInt(GLuint ID, const std::string &name, int i) {
  glUniform1i(glGetUniformLocation(ID, name.c_str()), i);
}

void setVec3(GLuint ID, const std::string &name, const glm::vec3 &vec) {
  glUniform3f(glGetUniformLocation(ID, name.c_str()), vec.x, vec.y, vec.z);
}
