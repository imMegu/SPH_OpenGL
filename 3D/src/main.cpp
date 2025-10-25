#include <GL/glew.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>

#include "camera.h"
#include "gui.h"
#include "shader.h"
#include "simulation.h"
#include "utilities.h"

float angle;
extern float botX, botY, topX, topY, botZ, topZ;
extern glm::mat4 boxTransform;
extern glm::mat4 boxTransformInverse;
glm::vec3 boxCenter =
    glm::vec3((topX + botX) / 2.0f, (topY + botY) / 2.0f, (topZ + botZ) / 2.0f);
bool singleAxis = true;

// Globals - Definitions moved to their respective files
GLFWwindow *window;
Camera camera(glm::vec3((topX - botX) / 2, topY - (topY - botY) / 2,
                        topZ * 3)); // Middle view
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.016f;
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 750;
const float fixedDeltaTime = 1.0 / 120.0; // the simulation one
bool altPressed = false;
bool cameraMode = false;
double frameTime = 0.0;
float mouseX = 0;
float mouseY = 0;
int mouseActive = 0;
bool running = true;
float sphereRadius = 3.5f;
float targetDensity = 3.0f;
float pressureStrength = 20.5f;
float viscosityStrength = 0.7f;
float gravity = 9.8f;
bool mouse = false;
float boundSpeed = 1.0f;

// Particle rendering
GLuint renderProgram;
GLuint VAO;
GLuint posBuffer, velBuffer, densityBuffer, predBuffer;
GLuint particleIndexBuffer, cellIndexBuffer, cellOffsetBuffer;
// Cube rendering
GLuint cubeProgram;
GLuint cubeVAO, cubeVBO, cubeEBO;
GLuint quadVAO, quadVBO;

// Function Prototypes - Implementations moved to their respective files
void processInput(GLFWwindow *window);
void mouse_callback_camera(GLFWwindow *window, double xposIn, double yposIn);
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);

int main() {
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return -1;
  }

  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, 0);

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
  glfwSetScrollCallback(window, scroll_callback);
  glfwSwapInterval(0);

  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW" << std::endl;
    return -1;
  }

  // Particle Data Initialization
  std::vector<glm::vec4> positions(NUM_PARTICLES);
  std::vector<glm::vec4> predictedPositions(NUM_PARTICLES);
  std::vector<glm::vec4> velocities(NUM_PARTICLES);
  std::vector<uint32_t> particleIndices(NUM_PARTICLES);
  std::vector<uint32_t> cellIndices(NUM_PARTICLES);
  std::vector<uint32_t> cellOffsets(TOTAL_GRID_CELL_COUNT, 0xFFFFFFFF);
  float densities[NUM_PARTICLES];
  for (int i = 0; i < NUM_PARTICLES; ++i) {
    positions[i] = genRandomVector3d();
    velocities[i] = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    particleIndices[i] = i;
  }

  glEnable(GL_PROGRAM_POINT_SIZE);

  // Shader Compilation
  renderProgram = createRenderProgram(vertexShaderSource, fragmentShaderSource);

  cubeProgram = createLineProgram(cubeVertexSource, cubeFragmentSource);
  setupComputeShaders();

  // Buffer Creation
  setupBuffers(positions, velocities, predictedPositions, densities,
               particleIndices, cellIndices, cellOffsets, &posBuffer,
               &velBuffer, &densityBuffer, &predBuffer, &particleIndexBuffer,
               &cellIndexBuffer, &cellOffsetBuffer);

  // VAO setup for particles
  /*
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribDivisor(0, 1);
  glBindBuffer(GL_ARRAY_BUFFER, velBuffer);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribDivisor(1, 1);
  */

  // VAO setup for cube
  setupCubeBuffers(&cubeVAO, &cubeVBO, &cubeEBO);
  setupQuadBuffers(&quadVAO, &quadVBO, &posBuffer, &velBuffer);

  setupGUI(window);

  double currentTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    double newTime = glfwGetTime();
    frameTime = newTime - currentTime;
    currentTime = newTime;
    deltaTime = std::min((float)frameTime, 0.008f);

    processInput(window);

    runSimulationFrame(cellOffsetBuffer);

    glClearColor(0.173, 0.173, 0.173, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Render particles
    glUseProgram(renderProgram);
    glm::mat4 Projection =
        glm::perspective(glm::radians(camera.Zoom),
                         (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 10.0f);
    setMat4(renderProgram, "u_proj", Projection);
    glm::mat4 View = camera.GetViewMatrix();
    setMat4(renderProgram, "u_view", View);
    glm::vec3 cameraPos = camera.GetPosition();
    setVec3(renderProgram, "u_cameraPos", cameraPos);
    setFloat(renderProgram, "sphereRadius", sphereRadius / 1000);
    glBindVertexArray(quadVAO);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, NUM_PARTICLES);

    // Render cube
    glUseProgram(cubeProgram);
    setMat4(cubeProgram, "u_model", boxTransform);
    setMat4(cubeProgram, "u_proj", Projection);
    setMat4(cubeProgram, "u_view", View);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);

    // Render GUI
    renderGUI();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Cleanup
  glDeleteVertexArrays(1, &quadVAO);
  glDeleteBuffers(1, &quadVBO);
  glDeleteBuffers(1, &posBuffer);
  glDeleteBuffers(1, &velBuffer);
  glDeleteBuffers(1, &densityBuffer);
  glDeleteBuffers(1, &predBuffer);
  glDeleteBuffers(1, &particleIndexBuffer);
  glDeleteBuffers(1, &cellIndexBuffer);
  glDeleteBuffers(1, &cellOffsetBuffer);
  glDeleteVertexArrays(1, &cubeVAO);
  glDeleteBuffers(1, &cubeVBO);
  glDeleteBuffers(1, &cubeEBO);
  glDeleteProgram(renderProgram);
  glDeleteProgram(cubeProgram);
  for (int i = 0; i < 6; i++)
    glDeleteProgram(computeProgram[i]);

  return 0;
}

// Handles general application input, including gizmo and camera modes
void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    running = !running;

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.ProcessKeyboard(FORWARD, fixedDeltaTime);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.ProcessKeyboard(BACKWARD, fixedDeltaTime);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.ProcessKeyboard(LEFT, fixedDeltaTime);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.ProcessKeyboard(RIGHT, fixedDeltaTime);

  float realBoundSpeed = boundSpeed / 1000;

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
    angle += 0.01f;
    // Calculate the new box center
    boxCenter = glm::vec3((topX + botX) / 2.0f, (topY + botY) / 2.0f,
                          (topZ + botZ) / 2.0f);
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
      glfwSetCursorPosCallback(window, mouse_callback_camera);
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      glfwSetCursorPosCallback(window, mouse_callback);
      firstMouse = true;
    }
    altPressed = true;
  } else if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_RELEASE) {
    altPressed = false;
  }
}

void mouse_callback_camera(GLFWwindow *window, double xposIn, double yposIn) {
  if (!cameraMode)
    return;
  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);
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
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn) {
  ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureMouse)
    return;

  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);
  float normalizedX = xpos / SCR_WIDTH;
  float normalizedY = 1.0 - ypos / SCR_HEIGHT;
  mouseX = normalizedX * (2.5 - (-0.5)) + (-0.5);
  mouseY = normalizedY * (1.5 - (-0.5)) + (-0.5);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureMouse)
    return;
  camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}
