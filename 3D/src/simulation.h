#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>

const int WORKGROUP_SIZE = 256;
// Initial allocation for the grid cell tables; runSimulationFrame grows them
// on demand when the box is enlarged or the smoothing radius is reduced.
const int GRID_CELL_CAPACITY = 32768 * 2 * 2 * 2;

// Runtime particle count (the counting sort puts no constraints on it)
extern int numParticles;
extern int pendingNumParticles; // edited by the GUI, applied between frames

extern const float fixedDeltaTime;
extern float smoothingRadius;
extern float targetDensity;
extern float pressureStrength;
extern float nearPressureStrength;
extern float viscosityStrength;
extern float gravity;
extern float botX, botY, topX, topY, botZ, topZ;
extern GLuint computeProgram[8];
extern bool running;

// Per-pass GPU times of the previous simulation step (GL_TIME_ELAPSED)
enum {
  GPU_PASS_EXTERNAL,
  GPU_PASS_COUNTING,
  GPU_PASS_SCAN,
  GPU_PASS_SCATTER,
  GPU_PASS_DENSITY,
  GPU_PASS_UPDATE,
  GPU_PASS_COUNT
};
extern const char *gpuPassNames[GPU_PASS_COUNT];
extern double gpuPassMs[GPU_PASS_COUNT];

void setupComputeShaders();
void runSimulationFrame();
void updateNumParticlesUniform();
void shutdownSimulation();
