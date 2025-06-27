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
GLuint CompileShader(const std::string& relative_path, GLenum shader_type, const std::string& shader_name);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
glm::vec3 genRandomVector2d();
void runSimulationFrame(GLuint offbuf);
uint NextPowerOfTwo(int x);

// Settings
const int TOTAL_GRID_CELL_COUNT = 32768; // Power of 2 for better hashing
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 750;
const int WORKGROUP_SIZE = 256;
const int NUM_PARTICLES = 65536;
float botX = 0.0f;
float botY = 0.0f;
float topX = 2.0f;
float topY = 1.0f;
double fixedDeltaTime = 1.0 / 60.0; // 60 fps
float smoothingRadius = 0.005f;
float targetDensity = 1.0f;
float pressureStrength = 0.5f;
float viscosityStrength = 0.0f;
double frameTime = 0.0;
float gravity = 0.0f;
float mouseX = 0;
float mouseY = 0;
int mouseActive = 0;
bool running = true;
bool first = true;
float deltaTime = 0.016f;

// Camera
Camera camera(glm::vec3((botX + topX) / 2, (botY + topY) / 2, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Shader sources 
const std::string updateShaderSource = "/src/shader/update.compute";
const std::string densityShaderSource = "/src/shader/density.compute";
const std::string externalShaderSource = "/src/shader/external.compute";
const std::string vertexShaderSource = "/src/shader/vertex.vs";
const std::string fragmentShaderSource = "/src/shader/fragment.fs";

GLuint computeProgram[10] {0, 0, 0, 0, 0, 0};

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
    ImGui::SetNextWindowSize(ImVec2(420, 175));
    ImGui::SetNextWindowPos(ImVec2(25, 25));
    // Create a GUI window
    ImGui::Begin("Simulation Controls", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::SliderFloat("Smoothing Radius", &smoothingRadius, 0.01f, 0.1f);
    ImGui::SliderFloat("Density", &targetDensity, 0.01f, 20000.0f);
    ImGui::SliderFloat("Pressure", &pressureStrength, 0.01f, 1.0f);
    ImGui::SliderFloat("Viscosity", &viscosityStrength, 0.0f, 1.0f);
    ImGui::SliderFloat("Gravity", &gravity, 0.0f, 20.0f);
    ImGui::Text("Number of particles: %i", NUM_PARTICLES);
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
    std::vector<uint32_t> particleIndices(NUM_PARTICLES);
    std::vector<uint32_t> cellIndices(NUM_PARTICLES);
    std::vector<uint32_t> cellOffsets(TOTAL_GRID_CELL_COUNT, 0xFFFFFFFF);
    float densities[NUM_PARTICLES];
    for (int i = 0; i < NUM_PARTICLES; ++i) {
      positions[i] = glm::vec2(genRandomVector2d());
      particleIndices[i] = i;
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

    uint32_t densityShader = CompileShader(densityShaderSource, GL_COMPUTE_SHADER, "DENSITY");
    computeProgram[1] = glCreateProgram();
    glAttachShader(computeProgram[1], densityShader);
    glLinkProgram(computeProgram[1]);
    checkCompileErrors(computeProgram[1], "DENSITY PROGRAM");
    glDeleteShader(densityShader);

    uint32_t updateShader = CompileShader(updateShaderSource, GL_COMPUTE_SHADER, "UPDATE");
    computeProgram[2] = glCreateProgram();
    glAttachShader(computeProgram[2], updateShader);
    glLinkProgram(computeProgram[2]);
    checkCompileErrors(computeProgram[2], "UPDATE PROGRAM");
    glDeleteShader(updateShader);

    uint32_t partitionShader = CompileShader("/src/shader/partition.compute", GL_COMPUTE_SHADER, "PARTITION");
    computeProgram[3] = glCreateProgram();
    glAttachShader(computeProgram[3], partitionShader);
    glLinkProgram(computeProgram[3]);
    checkCompileErrors(computeProgram[3], "PARTITION PROGRAM");
    glDeleteShader(partitionShader);

    uint32_t sortShader = CompileShader("/src/shader/sort.compute", GL_COMPUTE_SHADER, "SORT");
    computeProgram[4] = glCreateProgram();
    glAttachShader(computeProgram[4], sortShader);
    glLinkProgram(computeProgram[4]);
    checkCompileErrors(computeProgram[4], "SORT PROGRAM");
    glDeleteShader(sortShader);

    uint32_t offsetsShader = CompileShader("/src/shader/offsets.compute", GL_COMPUTE_SHADER, "OFFSETS");
    computeProgram[5] = glCreateProgram();
    glAttachShader(computeProgram[5], offsetsShader);
    glLinkProgram(computeProgram[5]);
    checkCompileErrors(computeProgram[5], "OFFSETS PROGRAM");
    glDeleteShader(offsetsShader);

    // --------------------------------------------------------------------------------------------------------------------------

    // Position of particle
    GLuint posBuffer;
    glGenBuffers(1, &posBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, posBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(glm::vec2),&positions[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer);

    // Velocity of particle
    GLuint velBuffer;
    glGenBuffers(1, &velBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(glm::vec2),&velocities[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velBuffer);

    // Density of particle
    GLuint densityBuffer;
    glGenBuffers(1, &densityBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, densityBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(float), &densities[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, densityBuffer);

    // Prediction of particle position
    GLuint predBuffer;
    glGenBuffers(1, &predBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, predBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(glm::vec2), &predictedPositions[0],GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, predBuffer);

    GLuint particleIndexBuffer;  // For sorting particles by cell
    GLuint cellIndexBuffer;     // Cell index for each particle
    GLuint cellOffsetBuffer;    // Start offset for each cell
    // Particle indices buffer
    glGenBuffers(1, &particleIndexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleIndexBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(uint32_t), &particleIndices[0], GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particleIndexBuffer);

    // Cell indices buffer  
    glGenBuffers(1, &cellIndexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellIndexBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(uint32_t), &cellIndices[0], GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, cellIndexBuffer);

    // Cell offsets buffer
    glGenBuffers(1, &cellOffsetBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellOffsetBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, TOTAL_GRID_CELL_COUNT * sizeof(uint32_t), &cellOffsets[0], GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, cellOffsetBuffer);

    // ---------------------------------------- end of buffers ----------------------------------------------

    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribDivisor(0, 1);

    glBindBuffer(GL_ARRAY_BUFFER, velBuffer);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);


    setupGUI(window);

    double currentTime = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        double newTime = glfwGetTime();
        frameTime = newTime - currentTime;
        currentTime = newTime;
        deltaTime = std::min((float)frameTime, 0.008f);

        // Process mouse and keyboard inputs
        processInput(window);
        runSimulationFrame(cellOffsetBuffer);

        // Background color
        glClearColor(0.173, 0.173, 0.173, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(renderProgram);


        // I wanted the ability to move inside the simulation, so I added a camera.
        // Mouse look is implemented but disabled, to facilitate ImGui's usage.

        // Projection matrix
        glm::mat4 Projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 10.0f);
        setMat4(renderProgram, "u_proj", Projection);
        // Camera matrix
        glm::mat4 View       = camera.GetViewMatrix();
        setMat4(renderProgram, "u_view", View);

        glBindVertexArray(VAO);
        glDrawArraysInstanced(GL_POINTS, 0, 1, NUM_PARTICLES);

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

// Updated runSimulationFrame function
void runSimulationFrame(GLuint offbuf)
{
    if (running)
    {
        // 1. External forces (gravity, etc.)
        glUseProgram(computeProgram[0]);
        setFloat(computeProgram[0], "deltaTime", deltaTime);
        setFloat(computeProgram[0], "gravity", gravity);
        setFloat(computeProgram[0], "mouseX", mouseX);
        setFloat(computeProgram[0], "mouseY", mouseY); 
        setInt(computeProgram[0], "mouseActive", mouseActive);
        glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE) / WORKGROUP_SIZE, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // 2. Clear cell offsets
        uint32_t clearValue = 0xFFFFFFFF;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, offbuf);
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &clearValue);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // 3. Grid partitioning
        glUseProgram(computeProgram[3]);
        setInt(computeProgram[3], "numParticles", NUM_PARTICLES);
        setFloat(computeProgram[3], "smoothingRadius", smoothingRadius);
        glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE) / WORKGROUP_SIZE, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // 4. Sort particles by cell indices (simplified bitonic sort or countingSort)
        uint sortSize = NextPowerOfTwo(NUM_PARTICLES);
        for (uint stage = 2; stage <= sortSize; stage <<= 1) {
            for (uint step = stage >> 1; step > 0; step >>= 1) {
                glUseProgram(computeProgram[4]);
                setInt(computeProgram[4], "numParticles", NUM_PARTICLES);
                setInt(computeProgram[4], "stage", stage);
                setInt(computeProgram[4], "step", step);
                glDispatchCompute(ceil(NUM_PARTICLES / WORKGROUP_SIZE), 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
        }

        // 5. Calculate cell offsets
        glUseProgram(computeProgram[5]);
        setInt(computeProgram[5], "numParticles", NUM_PARTICLES);
        glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE) / WORKGROUP_SIZE, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // 6. Density computation
        glUseProgram(computeProgram[1]);
        setInt(computeProgram[1], "numParticles", NUM_PARTICLES);
        setFloat(computeProgram[1], "smoothingRadius", smoothingRadius);
        setFloat(computeProgram[1], "SpikyPow2ScalingFactor", 6 / (M_PI * pow(smoothingRadius, 4)));
        glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE) / WORKGROUP_SIZE, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // 7. Force computation and integration
        glUseProgram(computeProgram[2]);
        setFloat(computeProgram[2], "deltaTime", deltaTime);
        setInt(computeProgram[2], "numParticles", NUM_PARTICLES);
        setFloat(computeProgram[2], "smoothingRadius", smoothingRadius);
        setFloat(computeProgram[2], "targetDensity", targetDensity);
        setFloat(computeProgram[2], "pressureStrength", pressureStrength);
        setFloat(computeProgram[2], "viscosityStrength", viscosityStrength);
        setFloat(computeProgram[2], "botX", botX);
        setFloat(computeProgram[2], "botY", botY);
        setFloat(computeProgram[2], "topX", topX);
        setFloat(computeProgram[2], "topY", topY);
        setFloat(computeProgram[2], "SpikyPow2DerivativeScalingFactor", -30 / (M_PI * pow(smoothingRadius, 4)));
        setFloat(computeProgram[2], "Poly6ScalingFactor", 4 / (M_PI * pow(smoothingRadius, 8)));
        glDispatchCompute((NUM_PARTICLES + WORKGROUP_SIZE) / WORKGROUP_SIZE, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
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
            std::cout << "ERROR::SHADER_ERROR of type: "<< type << "\n" << infoLog << std::endl;
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

GLuint CompileShader(const std::string& relative_path, GLenum shader_type, const std::string& shader_name)
{
    // Step 1: Load shader source code from file
    std::string shader_code;
    try
    {
        std::filesystem::path shader_path = std::filesystem::current_path().parent_path().string() + relative_path;
        std::ifstream shader_file(shader_path);

        if (!shader_file.is_open())
        {
            throw std::ios_base::failure("Could not open file: " + shader_path.string());
        }

        std::stringstream shader_stream;
        shader_stream << shader_file.rdbuf();
        shader_code = shader_stream.str();
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
        return 0; // Return an invalid shader ID
    }

    const char* shader_code_cstr = shader_code.c_str();

    // Step 2: Create and compile the shader
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &shader_code_cstr, nullptr);
    glCompileShader(shader);

    // Step 3: Check for compilation errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char info_log[1024];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << shader_name << "\n"
                  << info_log << std::endl;
        glDeleteShader(shader); // Clean up the shader object
        return 0; // Return an invalid shader ID
    }

    return shader; // Return the compiled shader ID
}


// glfw: whenever the mouse moves, this callback is called
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    float normalizedX = xpos / SCR_WIDTH;        // Normalize x
    float normalizedY = 1.0 - ypos / SCR_HEIGHT; // Normalize y and flip vertically


    mouseX = normalizedX * (2.5 - (-0.5)) + (-0.5); // Map x to [-2, 2]
    mouseY = normalizedY * (1.5 - (-0.5)) + (-0.5); // Map y to [-1, 1]
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
    std::uniform_real_distribution<float> dist_x(((botX + 0.20f * (topX - botX))), (botX + 0.80f * (topX - botX)));
    std::uniform_real_distribution<float> dist_y(((botY + 0.30f * (topY - botY))), (botY + 0.70f * (topY - botY)));
    return glm::vec3(dist_x(gen),dist_y(gen),0);
}
