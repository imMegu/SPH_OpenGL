# SPH Fluid Simulator (C++ / OpenGL)
A real-time Smoothed Particle Hydrodynamics (SPH) fluid simulation written in C++ with OpenGL. Uses compute shaders for parallel computing.
![image](https://github.com/user-attachments/assets/519fe99c-ca88-4f5e-bc1e-ae67d5ad6bb1)
> [!NOTE]
> A particle's color changes depending on it's velocity.
> [Quick video showcasing the app running.](https://youtu.be/x-2bFkBimAg)

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
cd SPH_OpenGL

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
This implementation utilizes the Spatial Hashing technique by Nvidia as described in this paper https://web.archive.org/web/20140725014123/https://docs.nvidia.com/cuda/samples/5_Simulations/particles/doc/particles.pdf, which improves particle lookup to ```O(nlogn)``` with Bitonic Mrge Sort and ```O(n)``` with Radix Sort (Worst case is still ```O(n^2)``` when all particles lie in the same cell). The sorting algorithm I chose is Bitonic Merge Sort, although I plan to switch to Radix Sort in the future for better efficiency.
# Roadmap
- **Improve Memory Efficiency:** Optimize GPU memory usage and data transfer.
- **Visual Enhancements:** Add shaders for realistic fluid rendering (e.g., reflections, refractions).
