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
This implementation follows the particle-grid approach described in Nvidia's paper https://web.archive.org/web/20140725014123/https://docs.nvidia.com/cuda/samples/5_Simulations/particles/doc/particles.pdf.
- The **3D version** uses a dense uniform grid over the simulation box (sized each frame from the box bounds and smoothing radius; no hash collisions) and an O(n) GPU **counting sort**: a per-cell atomic histogram, a parallel exclusive prefix sum (which doubles as the cell-start table), and a scatter pass that reorders the particle data itself into cell order, so the neighbor loops read contiguous, coalesced memory.
- The **2D version** uses the spatial hashing + Bitonic Merge Sort variant from the paper.

The 3D version also adds a near-pressure term (Clavet et al., "Particle-based Viscoelastic Fluid Simulation") to prevent particle clumping.

Rendering uses screen-space fluid rendering (van der Laan et al.): sphere-impostor depth, a separable bilateral blur, normals reconstructed from the smoothed depth, and compositing with refraction, Beer-Lambert absorption, and a Fresnel-weighted sky reflection. The "Water Shading" checkbox switches between this and a plain shaded-impostor view.

See [sources.md](sources.md) for the papers behind each technique.
# Roadmap
- **Visual Enhancements:** Environment-map reflections, foam/spray particles.
