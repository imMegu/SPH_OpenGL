# SPH Fluid Simulator (C++ / OpenGL)
A real-time Smoothed Particle Hydrodynamics (SPH) fluid simulation powered by C++ and OpenGL. Leverages compute shaders for high-performance parallel computing and features techniques for efficient particle-based fluid dynamics.
![image](https://github.com/user-attachments/assets/519fe99c-ca88-4f5e-bc1e-ae67d5ad6bb1)
> [!NOTE]
> A particle's color changes depending on it's velocity.
> [Quick video showcasing the app running.](https://youtu.be/x-2bFkBimAg)

# Features
- **Real-Time Fluid Simulation** 
- **Interactive Parameter Adjustments**
- **GPU Acceleration**
## Dependencies
To build the executable, there are a few required dependencies:
- GLEW
- GLFW
- GLM
- CMake >= 3.13.0
Make sure these libraries are installed before proceeding with the build.
## How to install and build:
This project uses CMake to generate build files.
Example of building in linux:
```
# Clone the repository
git clone https://github.com/your-repo/sph-fluid-simulator.git
cd sph-fluid-simulator

# Create and navigate to the build directory
mkdir build && cd build

# Generate build files
cmake ..

# Compile the project
make
```
### Running the Simulator

Once compiled, you can run the simulator with:
```
./execute
```
# Neighborhood Search
SPH simulation calculates particle properties based on **neighboring particles** within an influence radius.
The naive approach is to "brute force" these calculations by, ```for each particle, iterating through every other particle, and only then determine if they are inside the particle's influence radius.``` This turns out to be a ```O(n^2)``` algorithm, which scales very poorly when more particles are added.
This is the approach I have implemented, and a core improvement I need to make.
Better approaches include algorithms like Kd Trees and Spatial Hashing.
# Roadmap
- **Integrate Optimized Neighborhood Search:** Replace brute-force with a spatial data structure like Kd Trees or Spatial Hashing.
- **Improve Memory Efficiency:** Optimize GPU memory usage and data transfer.
- **Visual Enhancements:** Add shaders for realistic fluid rendering (e.g., reflections, refractions).
- **Cross-Platform Improvements:** Simplify setup and dependencies for Windows and macOS.
