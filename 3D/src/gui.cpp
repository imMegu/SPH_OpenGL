#include "gui.h"

void setupGUI(GLFWwindow *window) {
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 430");
}

void renderGUI() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(25, 25), ImGuiCond_FirstUseEver);
  ImGui::Begin("Simulation Controls", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::SliderFloat("Smoothing Radius", &smoothingRadius, 0.003f, 0.02f);
  ImGui::SliderFloat("Density", &targetDensity, 0.01f, 6.0f);
  ImGui::SliderFloat("Pressure", &pressureStrength, 0.05f, 30.0f);
  ImGui::SliderFloat("Near Pressure", &nearPressureStrength, 0.0f, 0.3f);
  ImGui::SliderFloat("Viscosity", &viscosityStrength, 0.0f, 1.0f);
  ImGui::SliderFloat("Gravity", &gravity, 0.0f, 20.0f);

  if (ImGui::Button(running ? "Pause" : "Resume"))
    running = !running;
  ImGui::SameLine();
  if (ImGui::Button("Reset"))
    resetRequested = true;
  if (ImGui::Button(phoneGyro ? "Deactivate PhoneGyro" : "Activate PhoneGyro"))
    phoneGyro = !phoneGyro;

  static const int particleCounts[] = {16384,  32768,  65536,  131072,
                                       262144, 524288, 1048576};
  static const char *particleCountLabels[] = {
      "16384", "32768", "65536", "131072", "262144", "524288", "1048576"};
  int countIndex = 0;
  for (int i = 0; i < 7; i++)
    if (particleCounts[i] == pendingNumParticles)
      countIndex = i;
  if (ImGui::Combo("Particles", &countIndex, particleCountLabels, 7))
    pendingNumParticles = particleCounts[countIndex];

  if (ImGui::Checkbox("VSync", &vsyncEnabled))
    glfwSwapInterval(vsyncEnabled ? 1 : 0);

  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              frameTime * 1000.0, 1 / frameTime);

  if (ImGui::CollapsingHeader("GPU timings", ImGuiTreeNodeFlags_DefaultOpen)) {
    double total = 0.0;
    for (int i = 0; i < GPU_PASS_COUNT; i++) {
      ImGui::Text("%-9s %6.3f ms", gpuPassNames[i], gpuPassMs[i]);
      total += gpuPassMs[i];
    }
    ImGui::Separator();
    ImGui::Text("%-9s %6.3f ms", "Total", total);
  }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(550, 25), ImGuiCond_FirstUseEver);
  ImGui::Begin("Other", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::SliderFloat("Sphere Radius", &sphereRadius, 0.1f, 5.0f);
  ImGui::SliderFloat("Bounds Speed", &boundSpeed, 0.1f, 5.0f);
  ImGui::Checkbox("Single Axis", &singleAxis);
  ImGui::Checkbox("Water Shading", &waterRendering);
  ImGui::SliderFloat("Absorption", &waterAbsorption, 0.0f, 20.0f);
  ImGui::Checkbox("Shadows", &shadowsEnabled);
  ImGui::SliderFloat("Shadow Strength", &shadowStrength, 0.0f, 1.0f);
  ImGui::End();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void shutdownGUI() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
