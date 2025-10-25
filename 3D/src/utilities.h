#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

extern const unsigned int SCR_WIDTH;
extern const unsigned int SCR_HEIGHT;
extern const float fixedDeltaTime;
extern float botX, botY, topX, topY, botZ, topZ;
extern const int NUM_PARTICLES;
extern std::vector<unsigned int> cubeIndices;

glm::vec2 genRandomVector2d();
glm::vec4 genRandomVector3d();
void setupBuffers(const std::vector<glm::vec4> &positions,
                  const std::vector<glm::vec4> &velocities,
                  const std::vector<glm::vec4> &predictedPositions,
                  const float *densities,
                  const std::vector<uint32_t> &particleIndices,
                  const std::vector<uint32_t> &cellIndices,
                  const std::vector<uint32_t> &cellOffsets, GLuint *posBuffer,
                  GLuint *velBuffer, GLuint *densityBuffer, GLuint *predBuffer,
                  GLuint *particleIndexBuffer, GLuint *cellIndexBuffer,
                  GLuint *cellOffsetBuffer);
void setupCubeBuffers(GLuint *cubeVAO, GLuint *cubeVBO, GLuint *cubeEBO);
void setupQuadBuffers(GLuint *quadVAO, GLuint *quadVBO, GLuint *posBuffer,
                      GLuint *velBuffer);
void updateCubeBounds(GLuint cubeVBO);
