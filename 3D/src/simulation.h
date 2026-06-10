#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>

const int WORKGROUP_SIZE = 256;
// Initial allocation for the grid cell table; runSimulationFrame grows it on
// demand when the box is enlarged or the smoothing radius is reduced.
const int GRID_CELL_CAPACITY = 32768 * 2 * 2 * 2;

// Runtime particle count. The bitonic sort requires a power of two (it skips
// comparisons whose partner is out of range) and whole workgroups, so the GUI
// only offers power-of-two counts >= WORKGROUP_SIZE.
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
extern GLuint computeProgram[7];
extern bool running;

// Per-pass GPU times of the previous simulation step (GL_TIME_ELAPSED)
enum {
  GPU_PASS_EXTERNAL,
  GPU_PASS_PARTITION,
  GPU_PASS_SORT,
  GPU_PASS_OFFSETS,
  GPU_PASS_DENSITY,
  GPU_PASS_UPDATE,
  GPU_PASS_COUNT
};
extern const char *gpuPassNames[GPU_PASS_COUNT];
extern double gpuPassMs[GPU_PASS_COUNT];

void setupComputeShaders();
void runSimulationFrame(GLuint offbuf);
void updateNumParticlesUniform();
