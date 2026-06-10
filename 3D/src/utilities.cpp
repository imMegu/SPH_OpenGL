#include "utilities.h"
#include <random>

std::vector<glm::vec2> quadVertices = {
    glm::vec2(-1.0f, -1.0f), // Bottom left
    glm::vec2(1.0f, -1.0f),  // Bottom right
    glm::vec2(-1.0f, 1.0f),  // Top left
    glm::vec2(1.0f, 1.0f)    // Top right
};

std::vector<glm::vec3> cubeVertices = {
    glm::vec3(botX, botY, botZ), glm::vec3(topX, botY, botZ),
    glm::vec3(topX, botY, topZ), glm::vec3(botX, botY, topZ),
    glm::vec3(botX, topY, botZ), glm::vec3(topX, topY, botZ),
    glm::vec3(topX, topY, topZ), glm::vec3(botX, topY, topZ)};

std::vector<unsigned int> cubeIndices = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6,
                                         6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};

glm::vec4 genRandomVector3d() {
  static std::mt19937 gen{std::random_device{}()};
  std::uniform_real_distribution<float> dist_x(((botX + 0.00f * (topX - botX))),
                                               (botX + 0.60f * (topX - botX)));
  std::uniform_real_distribution<float> dist_y(((botY + 0.30f * (topY - botY))),
                                               (botY + 0.70f * (topY - botY)));
  std::uniform_real_distribution<float> dist_z(((botZ + 0.20f * (topZ - botZ))),
                                               (botZ + 0.80f * (topZ - botZ)));
  return glm::vec4(dist_x(gen), dist_y(gen), dist_z(gen), 0.0f);
}

namespace {
void createStorageBuffer(GLuint *buffer, GLuint binding, GLsizeiptr size,
                         const void *data) {
  glGenBuffers(1, buffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_COPY);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, *buffer);
}
} // namespace

void createParticleBuffers(GLuint *posBuffer, GLuint *velBuffer,
                           GLuint *densityBuffer, GLuint *nearDensityBuffer,
                           GLuint *predBuffer, GLuint *particleIndexBuffer,
                           GLuint *cellIndexBuffer) {
  std::vector<glm::vec4> positions(numParticles);
  std::vector<glm::vec4> velocities(numParticles, glm::vec4(0.0f));
  std::vector<uint32_t> particleIndices(numParticles);
  std::vector<float> zeros(numParticles, 0.0f);
  for (int i = 0; i < numParticles; ++i) {
    positions[i] = genRandomVector3d();
    particleIndices[i] = i;
  }

  GLsizeiptr vec4Size = numParticles * sizeof(glm::vec4);
  GLsizeiptr uintSize = numParticles * sizeof(uint32_t);
  GLsizeiptr floatSize = numParticles * sizeof(float);

  createStorageBuffer(posBuffer, 0, vec4Size, positions.data());
  createStorageBuffer(velBuffer, 1, vec4Size, velocities.data());
  createStorageBuffer(densityBuffer, 2, floatSize, zeros.data());
  // overwritten by the external-forces pass before first use
  createStorageBuffer(predBuffer, 3, vec4Size, positions.data());
  createStorageBuffer(particleIndexBuffer, 4, uintSize,
                      particleIndices.data());
  createStorageBuffer(cellIndexBuffer, 5, uintSize, nullptr);
  createStorageBuffer(nearDensityBuffer, 7, floatSize, zeros.data());
}

void createGridBuffer(GLuint *cellOffsetBuffer) {
  // contents are cleared at the start of every simulation step
  createStorageBuffer(cellOffsetBuffer, 6,
                      GRID_CELL_CAPACITY * sizeof(uint32_t), nullptr);
}

void setupCubeBuffers(GLuint *cubeVAO, GLuint *cubeVBO, GLuint *cubeEBO) {
  glGenVertexArrays(1, cubeVAO);
  glGenBuffers(1, cubeVBO);
  glGenBuffers(1, cubeEBO);

  glBindVertexArray(*cubeVAO);

  glBindBuffer(GL_ARRAY_BUFFER, *cubeVBO);
  glBufferData(GL_ARRAY_BUFFER, cubeVertices.size() * sizeof(glm::vec3),
               &cubeVertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *cubeEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               cubeIndices.size() * sizeof(unsigned int), &cubeIndices[0],
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);
}

void updateCubeBounds(GLuint cubeVBO) {
  std::vector<glm::vec3> newCubeVertices = {
      glm::vec3(botX, botY, botZ), glm::vec3(topX, botY, botZ),
      glm::vec3(topX, botY, topZ), glm::vec3(botX, botY, topZ),
      glm::vec3(botX, topY, botZ), glm::vec3(topX, topY, botZ),
      glm::vec3(topX, topY, topZ), glm::vec3(botX, topY, topZ)};

  glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
  glBufferSubData(GL_ARRAY_BUFFER, 0,
                  newCubeVertices.size() * sizeof(glm::vec3),
                  &newCubeVertices[0]);
}

void setupQuadBuffers(GLuint *quadVAO, GLuint *quadVBO, GLuint *posBuffer,
                      GLuint *velBuffer) {
  glGenVertexArrays(1, quadVAO);
  glGenBuffers(1, quadVBO);

  glBindVertexArray(*quadVAO);
  glBindBuffer(GL_ARRAY_BUFFER, *quadVBO);
  glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(glm::vec2),
               &quadVertices[0], GL_STATIC_DRAW);

  // Quad vertices (per vertex, not per instance)
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribDivisor(0, 0); // Per vertex

  // Instance data - positions (per instance)
  glBindBuffer(GL_ARRAY_BUFFER, *posBuffer);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribDivisor(1, 1); // Per instance

  // Instance data - velocities (per instance)
  glBindBuffer(GL_ARRAY_BUFFER, *velBuffer);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void *)0);
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1); // Per instance
}
