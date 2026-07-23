#include "renderer.h"
#include "shader.h"
#include "simulation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

bool waterRendering = false;
float waterAbsorption = 9.0f;
bool shadowsEnabled = true;
float shadowStrength = 0.6f;

extern glm::mat4 boxTransform;

namespace {

// Bilateral range weight: rejects depth differences larger than a few
// particle diameters so the blur doesn't bleed across silhouettes
const float DEPTH_FALLOFF = 120.0f;

// World-space direction toward the light; shared by the shadow map, the
// composite specular and the floor shading so they all agree
const glm::vec3 LIGHT_DIR = glm::normalize(glm::vec3(0.4f, 0.8f, 0.6f));
const int SHADOW_RES = 1024;

int fbWidth = 0, fbHeight = 0;

GLuint depthProgram, thicknessProgram, blurProgram, compositeProgram;

GLuint shadowFBO, shadowTex, shadowDepthRB;
glm::mat4 lightView(1.0f), lightVP(1.0f);

GLuint fsVAO; // empty VAO for fullscreen-triangle passes

GLuint sceneFBO, sceneColorTex, sceneDepthTex;
GLuint fluidFBO, fluidDepthTex, fluidDepthRB;
GLuint blurTmpFBO, blurTmpTex;
GLuint fluidSmoothFBO, fluidSmoothTex;
GLuint thicknessFBO, thicknessTex;

struct {
  GLint proj, view, sphereRadius;
} depthU, thicknessU;
struct {
  GLint blurDir, depthFalloff;
} blurU;
struct {
  GLint proj, view, absorption, invView, lightView, lightVP, lightDir,
      shadowBias, shadowStrength;
} compositeU;

GLuint createTargetTexture(GLenum internalFormat, int width, int height) {
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return tex;
}

GLuint createColorFBO(GLuint colorTex) {
  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         colorTex, 0);
  return fbo;
}

void createTargets(int width, int height) {
  fbWidth = width;
  fbHeight = height;

  // Scene (background + cube), sampled by the composite pass for refraction
  sceneColorTex = createTargetTexture(GL_RGBA8, width, height);
  sceneDepthTex = createTargetTexture(GL_DEPTH_COMPONENT24, width, height);
  glGenFramebuffers(1, &sceneFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         sceneColorTex, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         sceneDepthTex, 0);

  // Fluid eye-space depth, with a depth attachment for impostor occlusion
  fluidDepthTex = createTargetTexture(GL_R32F, width, height);
  fluidFBO = createColorFBO(fluidDepthTex);
  glGenRenderbuffers(1, &fluidDepthRB);
  glBindRenderbuffer(GL_RENDERBUFFER, fluidDepthRB);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, fluidFBO);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, fluidDepthRB);

  // Blur ping-pong and thickness targets
  blurTmpTex = createTargetTexture(GL_R32F, width, height);
  blurTmpFBO = createColorFBO(blurTmpTex);
  fluidSmoothTex = createTargetTexture(GL_R32F, width, height);
  fluidSmoothFBO = createColorFBO(fluidSmoothTex);
  thicknessTex = createTargetTexture(GL_R32F, width, height);
  thicknessFBO = createColorFBO(thicknessTex);

  GLuint fbos[] = {sceneFBO, fluidFBO, blurTmpFBO, fluidSmoothFBO,
                   thicknessFBO};
  for (GLuint fbo : fbos) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
      std::cerr << "ERROR::RENDERER::FBO_INCOMPLETE: 0x" << std::hex << status
                << std::dec << std::endl;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void deleteTargets() {
  GLuint fbos[] = {sceneFBO, fluidFBO, blurTmpFBO, fluidSmoothFBO,
                   thicknessFBO};
  glDeleteFramebuffers(5, fbos);
  GLuint texs[] = {sceneColorTex, sceneDepthTex,  fluidDepthTex,
                   blurTmpTex,    fluidSmoothTex, thicknessTex};
  glDeleteTextures(6, texs);
  glDeleteRenderbuffers(1, &fluidDepthRB);
}

void drawParticles(GLuint particleVAO, GLuint program, const glm::mat4 &proj,
                   const glm::mat4 &view, float radius,
                   const decltype(depthU) &uniforms) {
  glUseProgram(program);
  glUniformMatrix4fv(uniforms.proj, 1, GL_FALSE, &proj[0][0]);
  glUniformMatrix4fv(uniforms.view, 1, GL_FALSE, &view[0][0]);
  glUniform1f(uniforms.sphereRadius, radius);
  glBindVertexArray(particleVAO);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, numParticles);
}

} // namespace

void setupWaterRenderer(int width, int height) {
  depthProgram =
      createRenderProgram(vertexShaderSource, fluidDepthFragmentSource);
  thicknessProgram =
      createRenderProgram(vertexShaderSource, thicknessFragmentSource);
  blurProgram = createRenderProgram(fullscreenVertexSource, blurFragmentSource);
  compositeProgram =
      createRenderProgram(fullscreenVertexSource, compositeFragmentSource);

  depthU.proj = glGetUniformLocation(depthProgram, "u_proj");
  depthU.view = glGetUniformLocation(depthProgram, "u_view");
  depthU.sphereRadius = glGetUniformLocation(depthProgram, "sphereRadius");

  thicknessU.proj = glGetUniformLocation(thicknessProgram, "u_proj");
  thicknessU.view = glGetUniformLocation(thicknessProgram, "u_view");
  thicknessU.sphereRadius =
      glGetUniformLocation(thicknessProgram, "sphereRadius");

  blurU.blurDir = glGetUniformLocation(blurProgram, "blurDir");
  blurU.depthFalloff = glGetUniformLocation(blurProgram, "depthFalloff");
  glUseProgram(blurProgram);
  glUniform1i(glGetUniformLocation(blurProgram, "source"), 0);

  compositeU.proj = glGetUniformLocation(compositeProgram, "u_proj");
  compositeU.view = glGetUniformLocation(compositeProgram, "u_view");
  compositeU.absorption = glGetUniformLocation(compositeProgram, "absorption");
  compositeU.invView = glGetUniformLocation(compositeProgram, "u_invView");
  compositeU.lightView = glGetUniformLocation(compositeProgram, "u_lightView");
  compositeU.lightVP = glGetUniformLocation(compositeProgram, "u_lightVP");
  compositeU.lightDir = glGetUniformLocation(compositeProgram, "u_lightDir");
  compositeU.shadowBias =
      glGetUniformLocation(compositeProgram, "u_shadowBias");
  compositeU.shadowStrength =
      glGetUniformLocation(compositeProgram, "u_shadowStrength");
  glUseProgram(compositeProgram);
  glUniform1i(glGetUniformLocation(compositeProgram, "fluidDepthTex"), 0);
  glUniform1i(glGetUniformLocation(compositeProgram, "thicknessTex"), 1);
  glUniform1i(glGetUniformLocation(compositeProgram, "sceneColorTex"), 2);
  glUniform1i(glGetUniformLocation(compositeProgram, "sceneDepthTex"), 3);
  glUniform1i(glGetUniformLocation(compositeProgram, "shadowMapTex"), 4);

  // Light-space fluid depth, fixed resolution (independent of window resizes).
  // NEAREST: the shaders compare depths per tap, interpolating across the
  // 0 = "no fluid" sentinel would produce bogus values at silhouettes.
  shadowTex = createTargetTexture(GL_R32F, SHADOW_RES, SHADOW_RES);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  shadowFBO = createColorFBO(shadowTex);
  glGenRenderbuffers(1, &shadowDepthRB);
  glBindRenderbuffer(GL_RENDERBUFFER, shadowDepthRB);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SHADOW_RES,
                        SHADOW_RES);
  glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, shadowDepthRB);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glGenVertexArrays(1, &fsVAO);
  createTargets(width, height);
}

void renderShadowMap(GLuint particleVAO, float sphereRadiusWorld) {
  glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
  glViewport(0, 0, SHADOW_RES, SHADOW_RES);
  float zero[4] = {0, 0, 0, 0};
  glClearBufferfv(GL_COLOR, 0, zero);
  glClear(GL_DEPTH_BUFFER_BIT);
  if (!shadowsEnabled || shadowStrength <= 0.0f)
    return; // empty map reads as "no fluid", i.e. fully lit

  // Fit the ortho frustum around the (possibly rotated/resized) box; the
  // particles are clamped inside it
  glm::vec3 center = glm::vec3(
      boxTransform * glm::vec4(0.5f * (botX + topX), 0.5f * (botY + topY),
                               0.5f * (botZ + topZ), 1.0f));
  float radius =
      0.5f * glm::length(glm::vec3(topX - botX, topY - botY, topZ - botZ)) +
      4.0f * sphereRadiusWorld;
  lightView =
      glm::lookAt(center + LIGHT_DIR * 2.0f * radius, center, glm::vec3(0, 1, 0));
  glm::mat4 lightProj =
      glm::ortho(-radius, radius, -radius, radius, 0.5f * radius, 3.5f * radius);
  lightVP = lightProj * lightView;

  glEnable(GL_DEPTH_TEST);
  drawParticles(particleVAO, depthProgram, lightProj, lightView,
                sphereRadiusWorld, depthU);
}

GLuint shadowMapTexture() { return shadowTex; }
const glm::mat4 &lightViewMatrix() { return lightView; }
const glm::mat4 &lightViewProjMatrix() { return lightVP; }

void resizeWaterRenderer(int width, int height) {
  if (width == fbWidth && height == fbHeight)
    return;
  deleteTargets();
  createTargets(width, height);
}

void beginScenePass() {
  glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
  glViewport(0, 0, fbWidth, fbHeight);
  // glClearColor(0.173, 0.173, 0.173, 1.0f);
  glClearColor(0.5, 0.44, 0.5, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
}

void renderWater(GLuint particleVAO, const glm::mat4 &proj,
                 const glm::mat4 &view, float sphereRadiusWorld) {
  // Fluid depth
  glBindFramebuffer(GL_FRAMEBUFFER, fluidFBO);
  glViewport(0, 0, fbWidth, fbHeight);
  float zero[4] = {0, 0, 0, 0};
  glClearBufferfv(GL_COLOR, 0, zero);
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  drawParticles(particleVAO, depthProgram, proj, view, sphereRadiusWorld,
                depthU);

  // Bilateral blur, horizontal then vertical
  glDisable(GL_DEPTH_TEST);
  glUseProgram(blurProgram);
  glUniform1f(blurU.depthFalloff, DEPTH_FALLOFF);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(fsVAO);

  glBindFramebuffer(GL_FRAMEBUFFER, blurTmpFBO);
  glBindTexture(GL_TEXTURE_2D, fluidDepthTex);
  glUniform2f(blurU.blurDir, 1.0f / fbWidth, 0.0f);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  glBindFramebuffer(GL_FRAMEBUFFER, fluidSmoothFBO);
  glBindTexture(GL_TEXTURE_2D, blurTmpTex);
  glUniform2f(blurU.blurDir, 0.0f, 1.0f / fbHeight);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Thickness (additive, no depth test)
  glBindFramebuffer(GL_FRAMEBUFFER, thicknessFBO);
  glClearBufferfv(GL_COLOR, 0, zero);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  drawParticles(particleVAO, thicknessProgram, proj, view, sphereRadiusWorld,
                thicknessU);
  glDisable(GL_BLEND);

  // Composite to the default framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, fbWidth, fbHeight);
  glUseProgram(compositeProgram);
  glUniformMatrix4fv(compositeU.proj, 1, GL_FALSE, &proj[0][0]);
  glUniformMatrix4fv(compositeU.view, 1, GL_FALSE, &view[0][0]);
  glUniform1f(compositeU.absorption, waterAbsorption);
  glm::mat4 invView = glm::inverse(view);
  glm::vec3 lightDirView = glm::normalize(glm::mat3(view) * LIGHT_DIR);
  glUniformMatrix4fv(compositeU.invView, 1, GL_FALSE, &invView[0][0]);
  glUniformMatrix4fv(compositeU.lightView, 1, GL_FALSE, &lightView[0][0]);
  glUniformMatrix4fv(compositeU.lightVP, 1, GL_FALSE, &lightVP[0][0]);
  glUniform3fv(compositeU.lightDir, 1, &lightDirView[0]);
  glUniform1f(compositeU.shadowBias, 3.0f * sphereRadiusWorld);
  glUniform1f(compositeU.shadowStrength,
              shadowsEnabled ? shadowStrength : 0.0f);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, shadowTex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fluidSmoothTex);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, thicknessTex);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, sceneColorTex);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
  glBindVertexArray(fsVAO);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glActiveTexture(GL_TEXTURE0);
}

void shutdownWaterRenderer() {
  deleteTargets();
  glDeleteFramebuffers(1, &shadowFBO);
  glDeleteTextures(1, &shadowTex);
  glDeleteRenderbuffers(1, &shadowDepthRB);
  glDeleteVertexArrays(1, &fsVAO);
  glDeleteProgram(depthProgram);
  glDeleteProgram(thicknessProgram);
  glDeleteProgram(blurProgram);
  glDeleteProgram(compositeProgram);
}
