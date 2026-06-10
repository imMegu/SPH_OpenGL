#include "simulation.h"
#include "shader.h"
#define _USE_MATH_DEFINES
#include <cfloat>
#include <cmath>

GLuint computeProgram[7]{};
int numParticles = 32768 * 2 * 2;
int pendingNumParticles = 32768 * 2 * 2;
float smoothingRadius = 0.011f;
float botX = 0.0f;
float botY = 0.0f;
float topX = 0.4f;
float topY = 0.5;
float botZ = 0.0f;
float topZ = 0.3f;
glm::mat4 boxTransform = glm::mat4(1.0f);
glm::mat4 boxTransformInverse = glm::mat4(1.0f);

const char *gpuPassNames[GPU_PASS_COUNT] = {"External", "Partition", "Sort",
                                            "Offsets",  "Density",   "Update"};
double gpuPassMs[GPU_PASS_COUNT] = {};

namespace {

// Per-frame uniform locations, fetched once at setup. Constant uniforms
// (numParticles, deltaTime) are uploaded once and on particle-count changes.
struct {
  GLint gravity;
} externalU;
struct {
  GLint smoothingRadius, gridMin, gridDims;
} partitionU;
struct {
  GLint stage, step;
} sortU;
struct {
  GLint stage, startStep;
} sortLocalU;
struct {
  GLint smoothingRadius, gridMin, gridDims;
  GLint spikyPow2, spikyPow3;
} densityU;
struct {
  GLint smoothingRadius, gridMin, gridDims;
  GLint targetDensity, pressureStrength, nearPressureStrength,
      viscosityStrength;
  GLint botX, botY, topX, topY, botZ, topZ;
  GLint spikyPow2Derivative, spikyPow3Derivative, poly6;
  GLint boxTransform, boxTransformInverse;
} updateU;

int gridCellCapacity = GRID_CELL_CAPACITY;

// Double-buffered GL_TIME_ELAPSED queries; results are read one frame late so
// the CPU never stalls waiting on the GPU.
GLuint timerQueries[2][GPU_PASS_COUNT];
long timedFrames = 0;

GLuint createComputeProgram(const std::string &source, const char *name) {
  GLuint program = glCreateProgram();
  glAttachShader(program, CompileShader(source, GL_COMPUTE_SHADER, name));
  glLinkProgram(program);
  checkCompileErrors(program, std::string(name) + " PROGRAM");
  return program;
}

} // namespace

void updateNumParticlesUniform() {
  for (int i = 0; i < 7; i++) {
    GLint loc = glGetUniformLocation(computeProgram[i], "numParticles");
    if (loc != -1) {
      glUseProgram(computeProgram[i]);
      glUniform1i(loc, numParticles);
    }
  }
}

void setupComputeShaders() {
  computeProgram[0] = createComputeProgram(externalShaderSource, "EXTERNAL");
  computeProgram[1] = createComputeProgram(densityShaderSource, "DENSITY");
  computeProgram[2] = createComputeProgram(updateShaderSource, "UPDATE");
  computeProgram[3] = createComputeProgram(partitionShaderSource, "PARTITION");
  computeProgram[4] = createComputeProgram(sortShaderSource, "SORT");
  computeProgram[5] = createComputeProgram(offsetsShaderSource, "OFFSETS");
  computeProgram[6] = createComputeProgram(sortLocalShaderSource, "SORT LOCAL");

  // Constant uniforms, set once
  updateNumParticlesUniform();
  for (int i = 0; i < 7; i++) {
    GLint deltaTimeLoc = glGetUniformLocation(computeProgram[i], "deltaTime");
    if (deltaTimeLoc != -1) {
      glUseProgram(computeProgram[i]);
      glUniform1f(deltaTimeLoc, fixedDeltaTime);
    }
  }

  externalU.gravity = glGetUniformLocation(computeProgram[0], "gravity");

  partitionU.smoothingRadius =
      glGetUniformLocation(computeProgram[3], "smoothingRadius");
  partitionU.gridMin = glGetUniformLocation(computeProgram[3], "gridMin");
  partitionU.gridDims = glGetUniformLocation(computeProgram[3], "gridDims");

  sortU.stage = glGetUniformLocation(computeProgram[4], "stage");
  sortU.step = glGetUniformLocation(computeProgram[4], "step");

  sortLocalU.stage = glGetUniformLocation(computeProgram[6], "stage");
  sortLocalU.startStep = glGetUniformLocation(computeProgram[6], "startStep");

  densityU.smoothingRadius =
      glGetUniformLocation(computeProgram[1], "smoothingRadius");
  densityU.gridMin = glGetUniformLocation(computeProgram[1], "gridMin");
  densityU.gridDims = glGetUniformLocation(computeProgram[1], "gridDims");
  densityU.spikyPow2 =
      glGetUniformLocation(computeProgram[1], "SpikyPow2ScalingFactor");
  densityU.spikyPow3 =
      glGetUniformLocation(computeProgram[1], "SpikyPow3ScalingFactor");

  updateU.smoothingRadius =
      glGetUniformLocation(computeProgram[2], "smoothingRadius");
  updateU.gridMin = glGetUniformLocation(computeProgram[2], "gridMin");
  updateU.gridDims = glGetUniformLocation(computeProgram[2], "gridDims");
  updateU.targetDensity =
      glGetUniformLocation(computeProgram[2], "targetDensity");
  updateU.pressureStrength =
      glGetUniformLocation(computeProgram[2], "pressureStrength");
  updateU.nearPressureStrength =
      glGetUniformLocation(computeProgram[2], "nearPressureStrength");
  updateU.viscosityStrength =
      glGetUniformLocation(computeProgram[2], "viscosityStrength");
  updateU.botX = glGetUniformLocation(computeProgram[2], "botX");
  updateU.botY = glGetUniformLocation(computeProgram[2], "botY");
  updateU.topX = glGetUniformLocation(computeProgram[2], "topX");
  updateU.topY = glGetUniformLocation(computeProgram[2], "topY");
  updateU.botZ = glGetUniformLocation(computeProgram[2], "botZ");
  updateU.topZ = glGetUniformLocation(computeProgram[2], "topZ");
  updateU.spikyPow2Derivative = glGetUniformLocation(
      computeProgram[2], "SpikyPow2DerivativeScalingFactor");
  updateU.spikyPow3Derivative = glGetUniformLocation(
      computeProgram[2], "SpikyPow3DerivativeScalingFactor");
  updateU.poly6 = glGetUniformLocation(computeProgram[2], "Poly6ScalingFactor");
  updateU.boxTransform =
      glGetUniformLocation(computeProgram[2], "boxTransform");
  updateU.boxTransformInverse =
      glGetUniformLocation(computeProgram[2], "boxTransformInverse");

  glGenQueries(2 * GPU_PASS_COUNT, &timerQueries[0][0]);
}

void runSimulationFrame(GLuint offbuf) {
  if (!running)
    return;

  int numWorkGroups = numParticles / WORKGROUP_SIZE;

  // Read the previous frame's timer queries (one frame of latency)
  int writeSet = (int)(timedFrames & 1);
  if (timedFrames > 0) {
    int readSet = writeSet ^ 1;
    for (int i = 0; i < GPU_PASS_COUNT; i++) {
      GLuint64 elapsed = 0;
      glGetQueryObjectui64v(timerQueries[readSet][i], GL_QUERY_RESULT,
                            &elapsed);
      gpuPassMs[i] = elapsed / 1.0e6;
    }
  }
  timedFrames++;

  // Dense uniform grid over the world-space AABB of the (possibly rotated)
  // box, padded by one cell so predicted positions that overshoot the bounds
  // still land in a border cell. Direct indexing - no hash collisions.
  glm::vec3 gridMin(FLT_MAX), gridMax(-FLT_MAX);
  for (int corner = 0; corner < 8; corner++) {
    glm::vec3 local((corner & 1) ? topX : botX, (corner & 2) ? topY : botY,
                    (corner & 4) ? topZ : botZ);
    glm::vec3 world = glm::vec3(boxTransform * glm::vec4(local, 1.0f));
    gridMin = glm::min(gridMin, world);
    gridMax = glm::max(gridMax, world);
  }
  gridMin -= smoothingRadius;
  gridMax += smoothingRadius;
  glm::ivec3 gridDims = glm::max(
      glm::ivec3(glm::ceil((gridMax - gridMin) / smoothingRadius)),
      glm::ivec3(1));
  int cellCount = gridDims.x * gridDims.y * gridDims.z;
  if (cellCount > gridCellCapacity) {
    gridCellCapacity = cellCount + cellCount / 2;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, offbuf);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 gridCellCapacity * sizeof(uint32_t), nullptr,
                 GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, offbuf);
  }

  // 3D smoothing kernel normalization factors
  double h = smoothingRadius;
  float spikyPow2Scale = (float)(15.0 / (2.0 * M_PI * pow(h, 5)));
  float spikyPow3Scale = (float)(15.0 / (M_PI * pow(h, 6)));
  float spikyPow2DerivScale = (float)(-15.0 / (M_PI * pow(h, 5)));
  float spikyPow3DerivScale = (float)(-45.0 / (M_PI * pow(h, 6)));
  float poly6Scale = (float)(315.0 / (64.0 * M_PI * pow(h, 9)));

  // Slider calibration: the switch from the old 2D kernel normalization to
  // proper 3D kernels rescales densities and forces by large, h-dependent
  // factors. These conversions keep the GUI sliders in the effective range
  // they were tuned for (and track h, so re-tuning isn't needed when the
  // smoothing radius slider moves).
  float targetDensityEff = targetDensity * 12500.0f / smoothingRadius;
  float pressureStrengthEff = pressureStrength * smoothingRadius;
  float viscosityStrengthEff = viscosityStrength * 250.0f;

  // External forces (gravity) + predicted positions
  glBeginQuery(GL_TIME_ELAPSED, timerQueries[writeSet][GPU_PASS_EXTERNAL]);
  glUseProgram(computeProgram[0]);
  glUniform1f(externalU.gravity, gravity);
  glDispatchCompute(numWorkGroups, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  glEndQuery(GL_TIME_ELAPSED);

  // Reset the cell offset table and assign each particle to a grid cell
  glBeginQuery(GL_TIME_ELAPSED, timerQueries[writeSet][GPU_PASS_PARTITION]);
  uint32_t clearValue = 0xFFFFFFFF;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, offbuf);
  glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER,
                    GL_UNSIGNED_INT, &clearValue);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  glUseProgram(computeProgram[3]);
  glUniform1f(partitionU.smoothingRadius, smoothingRadius);
  glUniform3fv(partitionU.gridMin, 1, &gridMin[0]);
  glUniform3iv(partitionU.gridDims, 1, &gridDims[0]);
  glDispatchCompute(numWorkGroups, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  glEndQuery(GL_TIME_ELAPSED);

  // Bitonic merge sort of (cellIndices, particleIndices). Steps that cross
  // workgroup boundaries run one global dispatch each; once the step fits
  // inside a workgroup, the rest of the stage runs in a single
  // shared-memory dispatch.
  glBeginQuery(GL_TIME_ELAPSED, timerQueries[writeSet][GPU_PASS_SORT]);
  for (uint32_t stage = 2; stage <= (uint32_t)numParticles; stage <<= 1) {
    uint32_t step = stage >> 1;
    if (step >= WORKGROUP_SIZE) {
      glUseProgram(computeProgram[4]);
      glUniform1i(sortU.stage, (int)stage);
      for (; step >= WORKGROUP_SIZE; step >>= 1) {
        glUniform1i(sortU.step, (int)step);
        glDispatchCompute(numWorkGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
      }
    }
    glUseProgram(computeProgram[6]);
    glUniform1i(sortLocalU.stage, (int)stage);
    glUniform1i(sortLocalU.startStep, (int)step);
    glDispatchCompute(numWorkGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }
  glEndQuery(GL_TIME_ELAPSED);

  // Find the first sorted index of each cell
  glBeginQuery(GL_TIME_ELAPSED, timerQueries[writeSet][GPU_PASS_OFFSETS]);
  glUseProgram(computeProgram[5]);
  glDispatchCompute(numWorkGroups, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  glEndQuery(GL_TIME_ELAPSED);

  // Density + near density
  glBeginQuery(GL_TIME_ELAPSED, timerQueries[writeSet][GPU_PASS_DENSITY]);
  glUseProgram(computeProgram[1]);
  glUniform1f(densityU.smoothingRadius, smoothingRadius);
  glUniform3fv(densityU.gridMin, 1, &gridMin[0]);
  glUniform3iv(densityU.gridDims, 1, &gridDims[0]);
  glUniform1f(densityU.spikyPow2, spikyPow2Scale);
  glUniform1f(densityU.spikyPow3, spikyPow3Scale);
  glDispatchCompute(numWorkGroups, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  glEndQuery(GL_TIME_ELAPSED);

  // Pressure + near-pressure + viscosity forces, integration, collisions
  glBeginQuery(GL_TIME_ELAPSED, timerQueries[writeSet][GPU_PASS_UPDATE]);
  glUseProgram(computeProgram[2]);
  glUniform1f(updateU.smoothingRadius, smoothingRadius);
  glUniform3fv(updateU.gridMin, 1, &gridMin[0]);
  glUniform3iv(updateU.gridDims, 1, &gridDims[0]);
  glUniform1f(updateU.targetDensity, targetDensityEff);
  glUniform1f(updateU.pressureStrength, pressureStrengthEff);
  glUniform1f(updateU.nearPressureStrength, nearPressureStrength);
  glUniform1f(updateU.viscosityStrength, viscosityStrengthEff);
  glUniform1f(updateU.botX, botX);
  glUniform1f(updateU.botY, botY);
  glUniform1f(updateU.topX, topX);
  glUniform1f(updateU.topY, topY);
  glUniform1f(updateU.botZ, botZ);
  glUniform1f(updateU.topZ, topZ);
  glUniform1f(updateU.spikyPow2Derivative, spikyPow2DerivScale);
  glUniform1f(updateU.spikyPow3Derivative, spikyPow3DerivScale);
  glUniform1f(updateU.poly6, poly6Scale);
  glUniformMatrix4fv(updateU.boxTransform, 1, GL_FALSE, &boxTransform[0][0]);
  glUniformMatrix4fv(updateU.boxTransformInverse, 1, GL_FALSE,
                     &boxTransformInverse[0][0]);
  glDispatchCompute(numWorkGroups, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  glEndQuery(GL_TIME_ELAPSED);
}
