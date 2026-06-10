#include <GL/glew.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

#include "camera.h"
#include "gui.h"
#include "shader.h"
#include "simulation.h"
#include "utilities.h"

float angle;
extern glm::mat4 boxTransform;
extern glm::mat4 boxTransformInverse;
bool singleAxis = true;

// Globals - Definitions moved to their respective files
GLFWwindow *window;
Camera camera(glm::vec3((topX - botX) / 2, topY - (topY - botY) / 2,
                        topZ * 3)); // Middle view
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 750;
int scrWidth = SCR_WIDTH;
int scrHeight = SCR_HEIGHT;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
const float fixedDeltaTime = 1.0f / 120.0f; // simulation timestep
bool altPressed = false;
bool spacePressed = false;
bool cameraMode = false;
double frameTime = 0.0;
bool running = true;
bool resetRequested = false;
bool vsyncEnabled = false;
float sphereRadius = 3.5f;
float targetDensity = 3.0f;
float pressureStrength = 20.5f;
float nearPressureStrength = 0.1f;
float viscosityStrength = 0.7f;
float gravity = 9.8f;
float boundSpeed = 1.0f;

// Orbit camera (left-drag when not in fly mode)
bool orbiting = false;
float orbitYaw = 0.0f;
float orbitPitch = 0.0f;
float orbitRadius = 1.0f;

// Particle rendering
GLuint renderProgram;
GLuint posBuffer, velBuffer, densityBuffer, nearDensityBuffer, predBuffer;
GLuint particleIndexBuffer, cellIndexBuffer, cellOffsetBuffer;
// Cube rendering
GLuint cubeProgram;
GLuint cubeVAO, cubeVBO, cubeEBO;
GLuint quadVAO, quadVBO;

// Function Prototypes - Implementations moved to their respective files
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);
void mouse_button_callback(GLFWwindow *window, int button, int action,
                           int mods);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);

static glm::vec3 currentBoxCenter() {
  // The box rotates around its own center, so the local center is also the
  // world-space one
  return glm::vec3((topX + botX) / 2.0f, (topY + botY) / 2.0f,
                   (topZ + botZ) / 2.0f);
}

static void rebuildParticleBuffers() {
  GLuint oldBuffers[] = {posBuffer,         velBuffer,  densityBuffer,
                         nearDensityBuffer, predBuffer, particleIndexBuffer,
                         cellIndexBuffer,   quadVBO};
  glDeleteBuffers(8, oldBuffers);
  glDeleteVertexArrays(1, &quadVAO);
  createParticleBuffers(&posBuffer, &velBuffer, &densityBuffer,
                        &nearDensityBuffer, &predBuffer, &particleIndexBuffer,
                        &cellIndexBuffer);
  setupQuadBuffers(&quadVAO, &quadVBO, &posBuffer, &velBuffer);
  updateNumParticlesUniform();
}

int main() {
  // Prefer X11, but fall back to whatever platform GLFW picks (e.g. on a
  // Wayland-only session)
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  if (!glfwInit()) {
    glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
    if (!glfwInit()) {
      std::cerr << "Failed to initialize GLFW" << std::endl;
      return -1;
    }
  }

  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSwapInterval(vsyncEnabled ? 1 : 0);

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW" << std::endl;
    const char *err;
    int code = glfwGetError(&err);
    std::cerr << "GLFW error: " << code << " - " << (err ? err : "none")
              << std::endl;
    glfwTerminate();
    return -1;
  }

  glEnable(GL_PROGRAM_POINT_SIZE);

  // Shader Compilation
  renderProgram = createRenderProgram(vertexShaderSource, fragmentShaderSource);
  cubeProgram = createLineProgram(cubeVertexSource, cubeFragmentSource);
  setupComputeShaders();

  // Uniform locations, fetched once
  GLint locProj = glGetUniformLocation(renderProgram, "u_proj");
  GLint locView = glGetUniformLocation(renderProgram, "u_view");
  GLint locCameraPos = glGetUniformLocation(renderProgram, "u_cameraPos");
  GLint locSphereRadius = glGetUniformLocation(renderProgram, "sphereRadius");
  GLint locCubeModel = glGetUniformLocation(cubeProgram, "u_model");
  GLint locCubeProj = glGetUniformLocation(cubeProgram, "u_proj");
  GLint locCubeView = glGetUniformLocation(cubeProgram, "u_view");

  // Buffer Creation
  createParticleBuffers(&posBuffer, &velBuffer, &densityBuffer,
                        &nearDensityBuffer, &predBuffer, &particleIndexBuffer,
                        &cellIndexBuffer);
  createGridBuffer(&cellOffsetBuffer);

  setupCubeBuffers(&cubeVAO, &cubeVBO, &cubeEBO);
  setupQuadBuffers(&quadVAO, &quadVBO, &posBuffer, &velBuffer);

  setupGUI(window);

  double currentTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    double newTime = glfwGetTime();
    frameTime = newTime - currentTime;
    currentTime = newTime;

    processInput(window);

    // Apply GUI-driven reset / particle count changes between frames
    if (resetRequested || pendingNumParticles != numParticles) {
      numParticles = pendingNumParticles;
      rebuildParticleBuffers();
      resetRequested = false;
    }

    // One fixed-size step per rendered frame: the GPU is the bottleneck, so
    // stepping multiple times per frame to track real time only lowers the
    // framerate. The step size stays fixed for stability, which means sim
    // speed follows the framerate.
    runSimulationFrame(cellOffsetBuffer);

    glClearColor(0.173, 0.173, 0.173, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Render particles
    glUseProgram(renderProgram);
    glm::mat4 Projection =
        glm::perspective(glm::radians(camera.Zoom),
                         (float)scrWidth / (float)scrHeight, 0.1f, 10.0f);
    glUniformMatrix4fv(locProj, 1, GL_FALSE, &Projection[0][0]);
    glm::mat4 View = camera.GetViewMatrix();
    glUniformMatrix4fv(locView, 1, GL_FALSE, &View[0][0]);
    glm::vec3 cameraPos = camera.GetPosition();
    glUniform3f(locCameraPos, cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform1f(locSphereRadius, sphereRadius / 1000);
    glBindVertexArray(quadVAO);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, numParticles);

    // Render cube
    glUseProgram(cubeProgram);
    glUniformMatrix4fv(locCubeModel, 1, GL_FALSE, &boxTransform[0][0]);
    glUniformMatrix4fv(locCubeProj, 1, GL_FALSE, &Projection[0][0]);
    glUniformMatrix4fv(locCubeView, 1, GL_FALSE, &View[0][0]);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);

    // Render GUI
    renderGUI();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Cleanup
  shutdownGUI();
  glDeleteVertexArrays(1, &quadVAO);
  glDeleteBuffers(1, &quadVBO);
  glDeleteBuffers(1, &posBuffer);
  glDeleteBuffers(1, &velBuffer);
  glDeleteBuffers(1, &densityBuffer);
  glDeleteBuffers(1, &nearDensityBuffer);
  glDeleteBuffers(1, &predBuffer);
  glDeleteBuffers(1, &particleIndexBuffer);
  glDeleteBuffers(1, &cellIndexBuffer);
  glDeleteBuffers(1, &cellOffsetBuffer);
  glDeleteVertexArrays(1, &cubeVAO);
  glDeleteBuffers(1, &cubeVBO);
  glDeleteBuffers(1, &cubeEBO);
  glDeleteProgram(renderProgram);
  glDeleteProgram(cubeProgram);
  for (int i = 0; i < 7; i++)
    glDeleteProgram(computeProgram[i]);
  glfwTerminate();

  return 0;
}

// Handles general application input, including gizmo and camera modes
void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !spacePressed) {
    running = !running;
    spacePressed = true;
  } else if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) {
    spacePressed = false;
  }

  float camDeltaTime = (float)frameTime;
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.ProcessKeyboard(FORWARD, camDeltaTime);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.ProcessKeyboard(BACKWARD, camDeltaTime);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.ProcessKeyboard(LEFT, camDeltaTime);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.ProcessKeyboard(RIGHT, camDeltaTime);

  // Scaled so the slider feels like the old per-frame speed did at ~120 FPS,
  // but independent of the actual framerate
  float realBoundSpeed = boundSpeed * 0.12f * (float)frameTime;

  if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
    topX += realBoundSpeed;
    if (!singleAxis)
      botX -= realBoundSpeed;
    updateCubeBounds(cubeVBO);
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
    topX -= realBoundSpeed;
    if (!singleAxis)
      botX += realBoundSpeed;
    updateCubeBounds(cubeVBO);
  }
  if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
    botZ -= realBoundSpeed;
    if (!singleAxis)
      topZ += realBoundSpeed;
    updateCubeBounds(cubeVBO);
  }
  if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
    botZ += realBoundSpeed;
    if (!singleAxis)
      topZ -= realBoundSpeed;
    updateCubeBounds(cubeVBO);
  }

  if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
    angle += 1.2f * (float)frameTime;
    glm::vec3 boxCenter = currentBoxCenter();
    // Build the transformation matrix to rotate around the center of the box
    boxTransform =
        glm::translate(glm::mat4(1.0f), boxCenter) *
        glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::translate(glm::mat4(1.0f), -boxCenter);
    // Recalculate the inverse transform for the compute shader
    boxTransformInverse = glm::inverse(boxTransform);
  }

  if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS && !altPressed) {
    cameraMode = !cameraMode;
    if (cameraMode) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      firstMouse = true;
      orbiting = false;
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    altPressed = true;
  } else if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_RELEASE) {
    altPressed = false;
  }
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn) {
  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);

  // Fly mode: free look
  if (cameraMode) {
    if (firstMouse) {
      lastX = xpos;
      lastY = ypos;
      firstMouse = false;
      return;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
    return;
  }

  // Default mode: left-drag orbits around the box center
  if (orbiting) {
    float xoffset = xpos - lastX;
    float yoffset = ypos - lastY;
    lastX = xpos;
    lastY = ypos;
    orbitYaw += xoffset * 0.4f;
    orbitPitch = glm::clamp(orbitPitch + yoffset * 0.4f, -89.0f, 89.0f);
    camera.OrbitAround(currentBoxCenter(), orbitYaw, orbitPitch, orbitRadius);
  }
}

void mouse_button_callback(GLFWwindow *window, int button, int action,
                           int mods) {
  if (button != GLFW_MOUSE_BUTTON_LEFT)
    return;
  ImGuiIO &io = ImGui::GetIO();
  if (action == GLFW_PRESS && !io.WantCaptureMouse && !cameraMode) {
    // Derive the orbit state from the current camera position so the orbit
    // starts wherever the camera happens to be
    glm::vec3 rel = camera.GetPosition() - currentBoxCenter();
    orbitRadius = glm::length(rel);
    if (orbitRadius < 0.001f)
      return;
    orbitYaw = glm::degrees(atan2(rel.z, rel.x));
    orbitPitch =
        glm::degrees(asin(glm::clamp(rel.y / orbitRadius, -1.0f, 1.0f)));
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    lastX = (float)x;
    lastY = (float)y;
    orbiting = true;
  } else if (action == GLFW_RELEASE) {
    orbiting = false;
  }
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureMouse)
    return;
  camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  if (width > 0 && height > 0) {
    scrWidth = width;
    scrHeight = height;
  }
  glViewport(0, 0, width, height);
}
