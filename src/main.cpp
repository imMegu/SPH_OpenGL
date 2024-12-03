#include <GL/glew.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
GLFWwindow* window;

#include <glm/glm.hpp>

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <random>
#include <filesystem>

#include "camera.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Functions
void processInput(GLFWwindow *window);
void setMat4(GLuint ID, const std::string &name, const glm::mat4 &mat);
void setFloat(GLuint ID, const std::string &name, float f);
void setInt(GLuint ID, const std::string &name, int f);
void checkCompileErrors(GLuint shader, std::string type);
GLuint CompileShader(const char* path_to_file, GLenum shader_type, std::string type);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
glm::vec3 genRandomVector2d();
void runSimulationFrame();

// Settings
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 750;
const int WORKGROUP_SIZE = 512;
const int NUM_PARTICLES = 10000;
float botX = 10.0f;
float botY = 10.0f;
float topX = 12.0f;
float topY = 11.0f;
double fixedDeltaTime = 1 / 60.0f; // 60 fps
float smoothingRadius = 0.035f;
float targetDensity = 1.0f;
float pressureStrength = 5.0f;
float viscosityStrength = 0.0f;
//float viscosityStrength = 0.0f;
double frameTime = 0.0;
float gravity = 0.0f;
float mouseX = 0;
float mouseY = 0;
int mouseActive = 0;
bool running = true;
bool first = true;

// Camera
Camera camera(glm::vec3((botX + topX) / 2, (botY + topY) / 2, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Shader sources 
const char* updateShaderSource = "/src/shader/update.compute";
const char* densityShaderSource = "/src/shader/density.compute";
const char* externalShaderSource = "/src/shader/external.compute";
const char* cellsShaderSource = "/src/shader/populateCells.compute";
const char* cellsStartShaderSource = "/src/shader/populateStart.compute";
const char* radixShaderSource = "/src/shader/radixSort.compute";
const char* vertexShaderSource = "/src/shader/vertex.vs";
const char* fragmentShaderSource = "/src/shader/fragment.fs";

// 0 is external forces (mouse and gravity)
// 1 is populate hash
// 2 is calculate density
// 3 is calculate forces (pressure and viscosity) + update positions + apply bounds
GLuint computeProgram[6] {0, 0, 0, 0, 0, 0};

// IMGUI
void setupGUI(GLFWwindow* window) {
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

void renderGUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(420, 160));
    ImGui::SetNextWindowPos(ImVec2(25, 25));
    // Create a GUI window
    ImGui::Begin("Simulation Controls", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::SliderFloat("Smoothing Radius", &smoothingRadius, 0.01f, 0.1f);
    ImGui::SliderFloat("Density", &targetDensity, 0.01f, 20000.0f);
    ImGui::SliderFloat("Pressure", &pressureStrength, 0.01f, 10.0f);
    ImGui::SliderFloat("Viscosity", &viscosityStrength, 0.0f, 1.0f);
    ImGui::SliderFloat("Gravity", &gravity, 0.0f, 20.0f);
    
    ImGui::Text("Application average %f ms/frame (%f FPS)", frameTime, 1/frameTime);
    ImGui::End();

    

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create GLFW window
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, 0);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSwapInterval(0); // set vertical sync off

    // Initialize glew
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // Initialize particles
    std::vector<glm::vec2> positions(NUM_PARTICLES);
    std::vector<glm::vec2> predictedPositions(NUM_PARTICLES);
    std::vector<glm::vec2> velocities(NUM_PARTICLES);
    std::vector<glm::vec2> forces(NUM_PARTICLES);
    std::vector<glm::vec3> cells(NUM_PARTICLES);
    std::vector<glm::vec3> tempCells(NUM_PARTICLES);
    float densities[NUM_PARTICLES];
    uint cellStart[NUM_PARTICLES];
    for (int i = 0; i < NUM_PARTICLES; ++i) {
        positions[i] = glm::vec2(genRandomVector2d());
        velocities[i] = glm::vec2(0);
        cells[i] = glm::vec3(0);
        tempCells[i] = glm::vec3(0);
        predictedPositions[i] = positions[i];
        forces[i] = glm::vec2(1, 1);
        densities[i] = 0;
        cellStart[i] = 4294967295u;
    }

    glEnable(GL_PROGRAM_POINT_SIZE);

    // this shader is responsible for rendering the particles
    uint32_t vertexShader = CompileShader(vertexShaderSource, GL_VERTEX_SHADER, "VERTEX");
    uint32_t fragmentShader = CompileShader(fragmentShaderSource, GL_FRAGMENT_SHADER, "FRAGMENT");
    GLuint renderProgram = glCreateProgram();
    glAttachShader(renderProgram, fragmentShader);
    glAttachShader(renderProgram, vertexShader);
    glLinkProgram(renderProgram);
    checkCompileErrors(renderProgram, "PROGRAM");
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // ------------------------------------------ Compute Shaders -------------------------------------------------------------
    
    uint32_t externalShader = CompileShader(externalShaderSource, GL_COMPUTE_SHADER, "EXTERNAL");
    computeProgram[0] = glCreateProgram();
    glAttachShader(computeProgram[0], externalShader);
    glLinkProgram(computeProgram[0]);
    checkCompileErrors(computeProgram[0], "EXTERNAL FORCES PROGRAM");
    glDeleteShader(externalShader);
    
    /*
    uint32_t populateShader = CompileShader(cellsShaderSource, GL_COMPUTE_SHADER, "CELLS");
    computeProgram[1] = glCreateProgram();
    glAttachShader(computeProgram[3], populateShader);
    glLinkProgram(computeProgram[3]);
    checkCompileErrors(computeProgram[3], "CELLS PROGRAM");
    glDeleteShader(populateShader);

    uint32_t radixShader = CompileShader(radixShaderSource, GL_COMPUTE_SHADER, "RADIX");
    computeProgram[2] = glCreateProgram();
    glAttachShader(computeProgram[2], radixShader);
    glLinkProgram(computeProgram[2]);
    checkCompileErrors(computeProgram[2], "RADIX PROGRAM");
    glDeleteShader(radixShader);

    uint32_t startShader = CompileShader(cellsStartShaderSource, GL_COMPUTE_SHADER, "START");
    computeProgram[3] = glCreateProgram();
    glAttachShader(computeProgram[3], startShader);
    glLinkProgram(computeProgram[3]);
    checkCompileErrors(computeProgram[3], "RADIX PROGRAM");
    glDeleteShader(startShader);
    */

    uint32_t densityShader = CompileShader(densityShaderSource, GL_COMPUTE_SHADER, "DENSITY");
    computeProgram[4] = glCreateProgram();
    glAttachShader(computeProgram[4], densityShader);
    glLinkProgram(computeProgram[4]);
    checkCompileErrors(computeProgram[4], "DENSITY PROGRAM");
    glDeleteShader(densityShader);

    uint32_t updateShader = CompileShader(updateShaderSource, GL_COMPUTE_SHADER, "UPDATE");
    computeProgram[5] = glCreateProgram();
    glAttachShader(computeProgram[5], updateShader);
    glLinkProgram(computeProgram[5]);
    checkCompileErrors(computeProgram[5], "UPDATE PROGRAM");
    glDeleteShader(updateShader);

    // --------------------------------------------------------------------------------------------------------------------------

    // Essentially creating these arrays in the GPU
    GLuint posBuffer;
    glGenBuffers(1, &posBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, posBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(glm::vec2),&positions[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer);

    GLuint velBuffer;
    glGenBuffers(1, &velBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(glm::vec2),&velocities[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velBuffer);

    GLuint densityBuffer;
    glGenBuffers(1, &densityBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, densityBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(float), &densities[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, densityBuffer);

    GLuint predBuffer;
    glGenBuffers(1, &predBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, predBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(glm::vec2),&predictedPositions[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, predBuffer);

    /*
    GLuint cellsBuffer;
    glGenBuffers(1, &cellsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(glm::vec3),&cells[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, cellsBuffer);

    GLuint startBuffer;
    glGenBuffers(1, &startBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, startBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(float), &cellStart[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, startBuffer);

    GLuint tempBuffer;
    glGenBuffers(1, &tempBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(glm::vec3),&tempCells[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, tempBuffer);
    */

    // ---------------------------------------- end of buffers ----------------------------------------------

    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(0);

    setupGUI(window);

    double currentTime = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        double newTime = glfwGetTime();
        frameTime = newTime - currentTime;
        currentTime = newTime;

        // Process mouse and keyboard inputs
        processInput(window);
        runSimulationFrame();

        // Background color
        glClearColor(0.173, 0.173, 0.173, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(renderProgram);


        // I wanted the ability to "move" inside the simulation, so I added a camera.
        // Mouse look is implemented but disabled, to facilitate ImGui's usage.

        // Projection matrix
        glm::mat4 Projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        setMat4(renderProgram, "u_proj", Projection);
        // Camera matrix
        glm::mat4 View       = camera.GetViewMatrix();
        setMat4(renderProgram, "u_view", View);

        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);

        renderGUI();

        // swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
        mouseActive = 0;
    }

    // Buffer and Program cleanup
    glDeleteBuffers(1, &posBuffer);
    glDeleteBuffers(1, &velBuffer);
    glDeleteBuffers(1, &densityBuffer);
    glDeleteProgram(computeProgram[0]);
    glDeleteProgram(renderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

uint nextPowerOfTwo(uint x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

void runSimulationFrame()
{
    if (running)
    {
        // ATTENTION!
        // The order IS important. The values should be calculated in this order
        // so the program behaves correctly.
        glUseProgram(computeProgram[0]);
        setFloat(computeProgram[0], "deltaTime", fixedDeltaTime);
        setFloat(computeProgram[0], "gravity", gravity);
        setFloat(computeProgram[0], "mouseX", mouseX);
        setFloat(computeProgram[0], "mouseY", mouseY);
        setInt(computeProgram[0], "mouseActive", mouseActive);
        glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        /*
        glUseProgram(computeProgram[1]);
        setInt(computeProgram[1], "numParticles", NUM_PARTICLES);
        setFloat(computeProgram[1], "smoothingRadius", smoothingRadius);
        glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);


        uint numPairs = nextPowerOfTwo(NUM_PARTICLES);
        int numStages = int(log2(float(numPairs)));

        for (int stageIndex = 0; stageIndex < numStages; stageIndex++) {
            for (int stepIndex = 0; stepIndex <= stageIndex; stepIndex++) {
                int groupWidth = 1 << (stageIndex - stepIndex);
                int groupHeight = 2 * groupWidth - 1;
                glUseProgram(computeProgram[2]);
                setInt(computeProgram[2], "numParticles", NUM_PARTICLES);
                setInt(computeProgram[2], "groupWidth", groupWidth);
                setInt(computeProgram[2], "groupHeight", groupHeight);
                setInt(computeProgram[2], "stepIndex", stepIndex);
                glDispatchCompute(1, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
        }
        

        glUseProgram(computeProgram[3]);
        setInt(computeProgram[3], "numParticles", NUM_PARTICLES);
        glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        */

        glUseProgram(computeProgram[4]);
        setInt(computeProgram[4], "numParticles", NUM_PARTICLES);
        setFloat(computeProgram[4], "smoothingRadius", smoothingRadius);
        setFloat(computeProgram[4], "SpikyPow2ScalingFactor", 6 / (M_PI * pow(smoothingRadius, 4)));
        glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1, 1);  // Dispatch compute shader
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);  // Ensure data is available for rendering


        glUseProgram(computeProgram[5]);
        setFloat(computeProgram[5], "deltaTime", fixedDeltaTime);
        setInt(computeProgram[5], "numParticles", NUM_PARTICLES);
        setFloat(computeProgram[5], "smoothingRadius", smoothingRadius);
        setFloat(computeProgram[5], "targetDensity", targetDensity);
        setFloat(computeProgram[5], "pressureStrength", pressureStrength);
        setFloat(computeProgram[5], "viscosityStrength", viscosityStrength);
        setFloat(computeProgram[5], "SpikyPow2DerivativeScalingFactor", 12 / (pow(smoothingRadius, 4) * M_PI));
        setFloat(computeProgram[5], "Poly6ScalingFactor", 4 / (M_PI * pow(smoothingRadius, 8)));
        glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1, 1);  // Dispatch compute shader
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);  // Ensure data is available for rendering
    }
}


// ------------------------ HELPER FUNCTIONS -------------------------------------

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_SPACE))
        running = !running;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, fixedDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, fixedDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, fixedDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, fixedDeltaTime);

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        mouseActive = 1;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        mouseActive = 2;
}

// ----------------------------------- Set uniforms -------------------------------------------------
void setMat4(GLuint ID, const std::string &name, const glm::mat4 &mat)
{
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void setFloat(GLuint ID, const std::string &name, float f)
{
    glUniform1f(glGetUniformLocation(ID, name.c_str()), f);
}

void setInt(GLuint ID, const std::string &name, int f)
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), f);
}

void checkCompileErrors(GLuint shader, std::string type)
{
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << infoLog << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << std::endl;
        }
    }
}

GLuint CompileShader(const char* path_to_file, GLenum shader_type, std::string type)
{
    std::string shaderCode;
    std::ifstream shaderFile;
    shaderFile.exceptions( std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        std::string shaderPath = std::filesystem::current_path().parent_path().string() + std::string(path_to_file);
        shaderFile.open(shaderPath);
        std::stringstream shaderStream;
        shaderStream << shaderFile.rdbuf();
        shaderFile.close();
        shaderCode = shaderStream.str();
    }
    catch (std::ifstream::failure& e)
    {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
    }
    const char* ShaderCode = shaderCode.c_str();

    unsigned int shader;
    shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &ShaderCode, NULL);
    glCompileShader(shader);
    checkCompileErrors(shader, type);

    return shader;
}

// glfw: whenever the mouse moves, this callback is called
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    float normalizedX = xpos / SCR_WIDTH;        // Normalize x
    float normalizedY = 1.0 - ypos / SCR_HEIGHT; // Normalize y and flip vertically


    mouseX = normalizedX * (13 - (9)) + (9); // Map x to [-2, 2]
    mouseY = normalizedY * (12 - (9)) + (9); // Map y to [-1, 1]
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}


glm::vec3 genRandomVector2d()
{
    std::random_device rd;  // Seed
    std::mt19937 gen(rd()); // Mersenne Twister random number generator
    std::uniform_real_distribution<float> dist_x(((botX + 0.45f *(topX - botX))), (botX + 0.55f * (topX - botX)));
    std::uniform_real_distribution<float> dist_y(((botY + 0.3f *(topY - botY))), (botY + 0.7f * (topY - botY)));
    return glm::vec3(dist_x(gen),dist_y(gen),0);
}