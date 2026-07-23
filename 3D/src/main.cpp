#include <GL/glew.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

#include "camera.h"
#include "gui.h"
#include "renderer.h"
#include "shader.h"
#include "simulation.h"
#include "src/gyro_source.h"
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
bool phoneGyro = false;
bool resetRequested = false;
bool vsyncEnabled = false;
float sphereRadius = 3.5f;
float targetDensity = 3.75f;
float pressureStrength = 27.0f;
float nearPressureStrength = 0.022f;
float viscosityStrength = 0.03f;
float gravity = 9.8f;
float boundSpeed = 1.0f;

// Orbit camera (left-drag when not in fly mode)
bool orbiting = false;
float orbitYaw = 0.0f;
float orbitPitch = 0.0f;
float orbitRadius = 1.0f;

// Particle rendering
GLuint renderProgram;
ParticleBuffers particleBuffers;
// Cube rendering
GLuint cubeProgram;
GLuint cubeVAO, cubeVBO, cubeEBO;
GLuint quadVAO, quadVBO;
// Checkered floor below the box
GLuint floorProgram;
GLuint floorVAO, floorVBO;

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
  deleteParticleBuffers(&particleBuffers);
  glDeleteBuffers(1, &quadVBO);
  glDeleteVertexArrays(1, &quadVAO);
  createParticleBuffers(&particleBuffers);
  setupQuadBuffers(&quadVAO, &quadVBO, particleBuffers.positions,
                   particleBuffers.velocities);
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
  floorProgram = createRenderProgram(floorVertexSource, floorFragmentSource);
  setupComputeShaders();

  // Uniform locations, fetched once
  GLint locProj = glGetUniformLocation(renderProgram, "u_proj");
  GLint locView = glGetUniformLocation(renderProgram, "u_view");
  GLint locSphereRadius = glGetUniformLocation(renderProgram, "sphereRadius");
  GLint locCubeModel = glGetUniformLocation(cubeProgram, "u_model");
  GLint locCubeProj = glGetUniformLocation(cubeProgram, "u_proj");
  GLint locCubeView = glGetUniformLocation(cubeProgram, "u_view");
  GLint locFloorModel = glGetUniformLocation(floorProgram, "u_model");
  GLint locFloorProj = glGetUniformLocation(floorProgram, "u_proj");
  GLint locFloorView = glGetUniformLocation(floorProgram, "u_view");
  GLint locFloorCenter = glGetUniformLocation(floorProgram, "u_floorCenter");
  GLint locFloorLightView = glGetUniformLocation(floorProgram, "u_lightView");
  GLint locFloorLightVP = glGetUniformLocation(floorProgram, "u_lightVP");
  GLint locFloorShadowStrength =
      glGetUniformLocation(floorProgram, "u_shadowStrength");
  glUseProgram(floorProgram);
  glUniform1f(glGetUniformLocation(floorProgram, "u_tileScale"), 8.0f);
  glUniform1f(glGetUniformLocation(floorProgram, "u_tileDarkOffset"), -0.07f);
  glUniform1f(glGetUniformLocation(floorProgram, "u_tileColVariation"), 0.2f);
  glUniform1i(glGetUniformLocation(floorProgram, "u_shadowMap"), 1);

  // Buffer Creation
  createParticleBuffers(&particleBuffers);

  setupCubeBuffers(&cubeVAO, &cubeVBO, &cubeEBO);
  setupFloorBuffers(&floorVAO, &floorVBO);
  setupQuadBuffers(&quadVAO, &quadVBO, particleBuffers.positions,
                   particleBuffers.velocities);

  setupWaterRenderer(scrWidth, scrHeight);
  setupGUI(window);

  GyroSource gyro("10.233.149.135", "8080");
  gyro.start();

  double currentTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    double newTime = glfwGetTime();
    frameTime = newTime - currentTime;
    currentTime = newTime;

    if (phoneGyro) {
      float dPitch, dYaw, dRoll;
      gyro.consume(dPitch, dYaw, dRoll); // radians accumulated since last frame

      camera.Yaw += glm::degrees(dYaw);
      camera.Pitch += glm::degrees(dPitch);
      camera.Pitch = glm::clamp(camera.Pitch, -89.0f, 89.0f);
      camera.updateCameraVectors();
    }

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
    runSimulationFrame();

    glm::mat4 Projection =
        glm::perspective(glm::radians(camera.Zoom),
                         (float)scrWidth / (float)scrHeight, 0.1f, 10.0f);
    glm::mat4 View = camera.GetViewMatrix();

    auto drawCube = [&]() {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glUseProgram(cubeProgram);
      glUniformMatrix4fv(locCubeModel, 1, GL_FALSE, &boxTransform[0][0]);
      glUniformMatrix4fv(locCubeProj, 1, GL_FALSE, &Projection[0][0]);
      glUniformMatrix4fv(locCubeView, 1, GL_FALSE, &View[0][0]);
      glBindVertexArray(cubeVAO);
      glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
      glDisable(GL_BLEND);
    };

    auto drawFloor = [&]() {
      glm::vec2 center(0.5f * (botX + topX), 0.5f * (botZ + topZ));
      // Slightly below botY to avoid z-fighting with the cube's bottom edges
      glm::mat4 model =
          glm::translate(glm::mat4(1.0f),
                         glm::vec3(center.x, botY - 0.001f, center.y)) *
          glm::scale(glm::mat4(1.0f), glm::vec3(3.0f, 1.0f, 3.0f));
      glUseProgram(floorProgram);
      glUniformMatrix4fv(locFloorModel, 1, GL_FALSE, &model[0][0]);
      glUniformMatrix4fv(locFloorProj, 1, GL_FALSE, &Projection[0][0]);
      glUniformMatrix4fv(locFloorView, 1, GL_FALSE, &View[0][0]);
      glUniform2f(locFloorCenter, center.x, center.y);
      glUniformMatrix4fv(locFloorLightView, 1, GL_FALSE,
                         &lightViewMatrix()[0][0]);
      glUniformMatrix4fv(locFloorLightVP, 1, GL_FALSE,
                         &lightViewProjMatrix()[0][0]);
      glUniform1f(locFloorShadowStrength,
                  shadowsEnabled ? shadowStrength : 0.0f);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, shadowMapTexture());
      glActiveTexture(GL_TEXTURE0);
      glBindVertexArray(floorVAO);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    };

    resizeWaterRenderer(scrWidth, scrHeight);
    // Fluid shadow map, sampled by the floor (and the water composite) in
    // both render modes
    renderShadowMap(quadVAO, sphereRadius / 1000);
    if (waterRendering) {
      // Background + cube go to the offscreen scene target; the water passes
      // then composite everything to the default framebuffer
      beginScenePass();
      drawFloor();
      drawCube();
      renderWater(quadVAO, Projection, View, sphereRadius / 1000);
    } else {
      // Plain sphere-impostor view
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, scrWidth, scrHeight);
      // glClearColor(0.173, 0.173, 0.173, 1.0f);
      glClearColor(0.5, 0.44, 0.5, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glEnable(GL_DEPTH_TEST);

      drawFloor();

      glUseProgram(renderProgram);
      glUniformMatrix4fv(locProj, 1, GL_FALSE, &Projection[0][0]);
      glUniformMatrix4fv(locView, 1, GL_FALSE, &View[0][0]);
      glUniform1f(locSphereRadius, sphereRadius / 1000);
      glBindVertexArray(quadVAO);
      glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, numParticles);

      drawCube();
    }

    // Render GUI
    renderGUI();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Cleanup
  shutdownGUI();
  shutdownWaterRenderer();
  shutdownSimulation();
  deleteParticleBuffers(&particleBuffers);
  glDeleteVertexArrays(1, &quadVAO);
  glDeleteBuffers(1, &quadVBO);
  glDeleteVertexArrays(1, &cubeVAO);
  glDeleteBuffers(1, &cubeVBO);
  glDeleteBuffers(1, &cubeEBO);
  glDeleteVertexArrays(1, &floorVAO);
  glDeleteBuffers(1, &floorVBO);
  glDeleteProgram(renderProgram);
  glDeleteProgram(cubeProgram);
  glDeleteProgram(floorProgram);
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
