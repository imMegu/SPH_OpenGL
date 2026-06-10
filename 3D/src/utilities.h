#pragma once
#include "simulation.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

glm::vec4 genRandomVector3d();
// Generates initial particle data for the current `numParticles` and
// (re)creates the particle SSBOs (bindings 0-5 and 7)
void createParticleBuffers(GLuint *posBuffer, GLuint *velBuffer,
                           GLuint *densityBuffer, GLuint *nearDensityBuffer,
                           GLuint *predBuffer, GLuint *particleIndexBuffer,
                           GLuint *cellIndexBuffer);
// Creates the grid cell offset table (binding 6)
void createGridBuffer(GLuint *cellOffsetBuffer);
void setupCubeBuffers(GLuint *cubeVAO, GLuint *cubeVBO, GLuint *cubeEBO);
void setupQuadBuffers(GLuint *quadVAO, GLuint *quadVBO, GLuint *posBuffer,
                      GLuint *velBuffer);
void updateCubeBounds(GLuint cubeVBO);
