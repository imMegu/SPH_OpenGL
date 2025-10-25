#include "simulation.h"
#include "shader.h"
#define _USE_MATH_DEFINES
#include <cmath>

GLuint computeProgram[6]{0, 0, 0, 0, 0, 0};
float smoothingRadius = 0.011f;
float botX = 0.0f;
float botY = 0.0f;
float topX = 0.4f;
float topY = 0.5;
float botZ = 0.0f;
float topZ = 0.3f;
glm::mat4 boxTransform = glm::mat4(1.0f);
glm::mat4 boxTransformInverse = glm::mat4(1.0f);

void setupComputeShaders() {
  computeProgram[0] = glCreateProgram();
  glAttachShader(
      computeProgram[0],
      CompileShader(externalShaderSource, GL_COMPUTE_SHADER, "EXTERNAL"));
  glLinkProgram(computeProgram[0]);
  checkCompileErrors(computeProgram[0], "EXTERNAL FORCES PROGRAM");

  computeProgram[1] = glCreateProgram();
  glAttachShader(
      computeProgram[1],
      CompileShader(densityShaderSource, GL_COMPUTE_SHADER, "DENSITY"));
  glLinkProgram(computeProgram[1]);
  checkCompileErrors(computeProgram[1], "DENSITY PROGRAM");

  computeProgram[2] = glCreateProgram();
  glAttachShader(computeProgram[2], CompileShader(updateShaderSource,
                                                  GL_COMPUTE_SHADER, "UPDATE"));
  glLinkProgram(computeProgram[2]);
  checkCompileErrors(computeProgram[2], "UPDATE PROGRAM");

  computeProgram[3] = glCreateProgram();
  glAttachShader(
      computeProgram[3],
      CompileShader(partitionShaderSource, GL_COMPUTE_SHADER, "PARTITION"));
  glLinkProgram(computeProgram[3]);
  checkCompileErrors(computeProgram[3], "PARTITION PROGRAM");

  computeProgram[4] = glCreateProgram();
  glAttachShader(computeProgram[4],
                 CompileShader(sortShaderSource, GL_COMPUTE_SHADER, "SORT"));
  glLinkProgram(computeProgram[4]);
  checkCompileErrors(computeProgram[4], "SORT PROGRAM");

  computeProgram[5] = glCreateProgram();
  glAttachShader(
      computeProgram[5],
      CompileShader(offsetsShaderSource, GL_COMPUTE_SHADER, "OFFSETS"));
  glLinkProgram(computeProgram[5]);
  checkCompileErrors(computeProgram[5], "OFFSETS PROGRAM");
}

void runSimulationFrame(GLuint offbuf) {
  if (running) {
    glUseProgram(computeProgram[0]);
    setFloat(computeProgram[0], "deltaTime", deltaTime);
    setFloat(computeProgram[0], "gravity", gravity);
    setFloat(computeProgram[0], "mouseX", mouseX);
    setFloat(computeProgram[0], "mouseY", mouseY);
    setInt(computeProgram[0], "mouseActive", mouseActive);
    setInt(computeProgram[0], "numParticles", NUM_PARTICLES);
    glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1,
                      1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    uint32_t clearValue = 0xFFFFFFFF;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, offbuf);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER,
                      GL_UNSIGNED_INT, &clearValue);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(computeProgram[3]);
    setInt(computeProgram[3], "numParticles", NUM_PARTICLES);
    setFloat(computeProgram[3], "smoothingRadius", smoothingRadius);
    glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1,
                      1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    uint sortSize = NextPowerOfTwo(NUM_PARTICLES);
    for (uint stage = 2; stage <= sortSize; stage <<= 1) {
      for (uint step = stage >> 1; step > 0; step >>= 1) {
        glUseProgram(computeProgram[4]);
        setInt(computeProgram[4], "numParticles", NUM_PARTICLES);
        setInt(computeProgram[4], "stage", (int)stage);
        setInt(computeProgram[4], "step", (int)step);
        uint numWorkGroups =
            (NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
        glDispatchCompute(numWorkGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
      }
    }

    glUseProgram(computeProgram[5]);
    setInt(computeProgram[5], "numParticles", NUM_PARTICLES);
    glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1,
                      1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(computeProgram[1]);
    setInt(computeProgram[1], "numParticles", NUM_PARTICLES);
    setFloat(computeProgram[1], "smoothingRadius", smoothingRadius);
    setFloat(computeProgram[1], "SpikyPow2ScalingFactor",
             6 / (M_PI * pow(smoothingRadius, 4)));
    glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1,
                      1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(computeProgram[2]);
    setFloat(computeProgram[2], "deltaTime", deltaTime);
    setInt(computeProgram[2], "numParticles", NUM_PARTICLES);
    setFloat(computeProgram[2], "smoothingRadius", smoothingRadius);
    setFloat(computeProgram[2], "targetDensity", targetDensity * 10000);
    setFloat(computeProgram[2], "pressureStrength", pressureStrength);
    setFloat(computeProgram[2], "viscosityStrength", viscosityStrength);
    setFloat(computeProgram[2], "botX", botX);
    setFloat(computeProgram[2], "botY", botY);
    setFloat(computeProgram[2], "topX", topX);
    setFloat(computeProgram[2], "topY", topY);
    setFloat(computeProgram[2], "botZ", botZ);
    setFloat(computeProgram[2], "topZ", topZ);
    setFloat(computeProgram[2], "SpikyPow2DerivativeScalingFactor",
             -30 / (M_PI * pow(smoothingRadius, 4)));
    setFloat(computeProgram[2], "Poly6ScalingFactor",
             4 / (M_PI * pow(smoothingRadius, 8)));
    setMat4(computeProgram[2], "boxTransform", boxTransform);
    setMat4(computeProgram[2], "boxTransformInverse", boxTransformInverse);
    glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1,
                      1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }
}

uint NextPowerOfTwo(int x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
}
