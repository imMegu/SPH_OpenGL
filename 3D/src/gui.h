#pragma once
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

extern float sphereRadius;
extern float targetDensity;
extern float pressureStrength;
extern float viscosityStrength;
extern float gravity;
extern float smoothingRadius;
extern double frameTime;
extern const int NUM_PARTICLES;
extern float boundSpeed;
extern bool singleAxis;

void setupGUI(GLFWwindow *window);
void renderGUI();
