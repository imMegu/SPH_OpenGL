#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>

// Screen-space fluid ("water") rendering. Pipeline per frame:
//   1. scene pass: background + cube into an offscreen FBO (beginScenePass,
//      then the caller draws the scene)
//   2. fluid depth: sphere impostor depth into an R32F target
//   3. separable bilateral blur of the depth (2 passes)
//   4. thickness: additive impostor chords
//   5. composite to the default framebuffer: refraction + absorption +
//      Fresnel sky reflection + specular
extern bool waterRendering;
extern float waterAbsorption;

void setupWaterRenderer(int width, int height);
void resizeWaterRenderer(int width, int height);
// Binds and clears the offscreen scene framebuffer; the caller then renders
// the background scene (cube) into it with depth testing
void beginScenePass();
// Runs passes 2-5; leaves the default framebuffer bound
void renderWater(GLuint particleVAO, const glm::mat4 &proj,
                 const glm::mat4 &view, float sphereRadiusWorld);
void shutdownWaterRenderer();
