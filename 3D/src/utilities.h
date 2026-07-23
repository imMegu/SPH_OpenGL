#pragma once
#include "simulation.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

// All per-particle SSBOs. positions/velocities hold the current state (and
// feed instanced rendering); the sorted* buffers are the cell-ordered copies
// produced by the scatter pass each frame.
struct ParticleBuffers {
  GLuint positions;         // binding 0
  GLuint velocities;        // binding 1
  GLuint densities;         // binding 2
  GLuint predicted;         // binding 3
  GLuint cellIndices;       // binding 4
  GLuint sortedPredicted;   // binding 5
  GLuint nearDensities;     // binding 7
  GLuint sortedPositions;   // binding 9
  GLuint sortedVelocities;  // binding 10
};

glm::vec4 genRandomVector3d();
// Generates initial particle data for the current `numParticles` and
// (re)creates the particle SSBOs
void createParticleBuffers(ParticleBuffers *buffers);
void deleteParticleBuffers(ParticleBuffers *buffers);
void setupCubeBuffers(GLuint *cubeVAO, GLuint *cubeVBO, GLuint *cubeEBO);
void setupFloorBuffers(GLuint *floorVAO, GLuint *floorVBO);
void setupQuadBuffers(GLuint *quadVAO, GLuint *quadVBO, GLuint posBuffer,
                      GLuint velBuffer);
void updateCubeBounds(GLuint cubeVBO);
