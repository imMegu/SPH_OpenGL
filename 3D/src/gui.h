#pragma once
#include "simulation.h" // must come first: GLEW before any GL header
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

extern float sphereRadius;
extern double frameTime;
extern float boundSpeed;
extern bool singleAxis;
extern bool resetRequested;
extern bool vsyncEnabled;
extern bool waterRendering;
extern float waterAbsorption;

void setupGUI(GLFWwindow *window);
void renderGUI();
void shutdownGUI();
