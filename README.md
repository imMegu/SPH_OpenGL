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
git clone https://github.com/imMegu/SPH_OpenGL.git
cd SPH_OpenGL

# Choose 2D or 3D
cd 3D

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
## Controls (3D)
| Input | Action |
| --- | --- |
| Left mouse drag | Orbit the camera around the box |
| Scroll wheel | Zoom (field of view) |
| Alt | Toggle fly camera (mouse look + WASD) |
| W / A / S / D | Move the camera (fly mode) |
| Space | Pause / resume the simulation |
| Arrow keys | Resize the box (X and Z axes) |
| R | Rotate the box around the Y axis |
| Esc | Quit |

The GUI exposes the simulation parameters (smoothing radius, density,
pressure, near pressure, viscosity, gravity), a particle count selector,
pause/reset buttons, a VSync toggle, and live per-pass GPU timings measured
with `GL_TIME_ELAPSED` queries.

# Neighborhood Search
SPH simulation calculates particle properties based on **neighboring particles** within an influence radius.
The naive approach is to "brute force" these calculations by, ```for each particle, iterating through every other particle, and only then determine if they are inside the particle's influence radius.``` This turns out to be a ```O(n^2)``` algorithm, which scales very poorly when more particles are added.
This implementation follows the particle-grid approach described in Nvidia's paper https://web.archive.org/web/20140725014123/https://docs.nvidia.com/cuda/samples/5_Simulations/particles/doc/particles.pdf: particles are binned into grid cells, sorted by cell index with a GPU Bitonic Merge Sort (using shared memory for the steps that fit inside a workgroup), and looked up through a per-cell offset table.
- The **3D version** uses a dense uniform grid over the simulation box, sized each frame from the box bounds and smoothing radius. Cells are indexed directly, so there are no hash collisions.
- The **2D version** uses the spatial hashing variant from the paper.

The 3D version also adds a near-pressure term (Clavet et al., "Particle-based Viscoelastic Fluid Simulation") to prevent particle clumping.
# Roadmap
- **Counting sort + data reorder:** Replace the bitonic sort with an O(n) counting sort and scatter the particle data into cell order for coalesced memory access (the density/update passes dominate the frame time).
- **Visual Enhancements:** Add shaders for realistic fluid rendering (e.g., reflections, refractions).
