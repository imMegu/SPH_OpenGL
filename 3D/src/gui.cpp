#include "gui.h"

void setupGUI(GLFWwindow *window) {
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 460");
}

void renderGUI() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(420, 175));
  ImGui::SetNextWindowPos(ImVec2(25, 25));
  ImGui::Begin("Simulation Controls", nullptr, ImGuiWindowFlags_NoResize);
  ImGui::SliderFloat("Smoothing Radius", &smoothingRadius, 0.003f, 0.02f);
  ImGui::SliderFloat("Density", &targetDensity, 0.01f, 6.0f);
  ImGui::SliderFloat("Pressure", &pressureStrength, 0.05f, 30.0f);
  ImGui::SliderFloat("Viscosity", &viscosityStrength, 0.0f, 2.0f);
  ImGui::SliderFloat("Gravity", &gravity, 0.0f, 20.0f);
  ImGui::Text("Number of particles: %i", NUM_PARTICLES);
  ImGui::Text("Application average %f ms/frame (%f FPS)", frameTime,
              1 / frameTime);
  ImGui::End();

  ImGui::SetNextWindowSize(ImVec2(330, 100));
  ImGui::SetNextWindowPos(ImVec2(550, 25));
  ImGui::Begin("Other", nullptr, ImGuiWindowFlags_NoResize);
  ImGui::SliderFloat("Sphere Radius", &sphereRadius, 0.1f, 5.0f);
  ImGui::SliderFloat("Bounds Speed", &boundSpeed, 0.1f, 5.0f);
  ImGui::Checkbox("Single Axis", &singleAxis);
  ImGui::End();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
