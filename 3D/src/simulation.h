#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>

const int TOTAL_GRID_CELL_COUNT = 262144;
const int NUM_PARTICLES = 32768;
const int WORKGROUP_SIZE = 256;

extern const std::string updateShaderSource;
extern const std::string densityShaderSource;
extern const std::string externalShaderSource;
extern const std::string partitionShaderSource;
extern const std::string sortShaderSource;
extern const std::string offsetsShaderSource;

extern const std::string vertexShaderSource;
extern const std::string fragmentShaderSource;
extern const std::string geometryShaderSource;

extern const std::string cubeFragmentSource;
extern const std::string cubeVertexSource;

extern float smoothingRadius;
extern float targetDensity;
extern float pressureStrength;
extern float viscosityStrength;
extern float gravity;
extern float deltaTime;
extern float mouseX;
extern float mouseY;
extern int mouseActive;
extern float botX, botY, topX, topY, botZ, topZ;
extern GLuint computeProgram[6];
extern bool running;

void setupComputeShaders();
void runSimulationFrame(GLuint offbuf);
uint NextPowerOfTwo(int x);
