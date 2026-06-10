#include "shader.h"
#include "simulation.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

// Shader paths are resolved relative to the executable so the program works
// regardless of the current working directory.
std::filesystem::path ExecutableDir() {
  std::error_code ec;
  std::filesystem::path exe =
      std::filesystem::read_symlink("/proc/self/exe", ec);
  if (ec)
    return std::filesystem::current_path();
  return exe.parent_path();
}

std::string LoadFile(const std::filesystem::path &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::ios_base::failure("Could not open file: " + path.string());
  }
  std::stringstream stream;
  stream << file.rdbuf();
  return stream.str();
}

// Expands #include "file" directives (one level, relative to the shader's
// directory) and injects the C++-side constants right after the #version
// line so shaders and host code can never disagree on them.
std::string PreprocessShader(const std::filesystem::path &shader_path) {
  const std::string preamble =
      "#define WORKGROUP_SIZE " + std::to_string(WORKGROUP_SIZE) + "\n";

  std::istringstream input(LoadFile(shader_path));
  std::ostringstream output;
  std::string line;
  while (std::getline(input, line)) {
    if (line.rfind("#include", 0) == 0) {
      size_t first = line.find('"');
      size_t last = line.rfind('"');
      std::string include_name = line.substr(first + 1, last - first - 1);
      output << LoadFile(shader_path.parent_path() / include_name) << '\n';
    } else {
      output << line << '\n';
      if (line.rfind("#version", 0) == 0)
        output << preamble;
    }
  }
  return output.str();
}

} // namespace

GLuint CompileShader(const std::string &relative_path, GLenum shader_type,
                     const std::string &shader_name) {
  std::string shader_code;
  try {
    shader_code = PreprocessShader(ExecutableDir() / relative_path);
    if (shader_code.empty()) {
      std::cerr << "ERROR::SHADER::FILE_IS_EMPTY: " << relative_path
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
