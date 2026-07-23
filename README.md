# SPH Fluid Simulator (C++ / OpenGL)
A real-time Smoothed Particle Hydrodynamics (SPH) fluid simulation written in C++ with OpenGL. The whole simulation runs on the GPU in compute shaders.
![image](https://github.com/user-attachments/assets/519fe99c-ca88-4f5e-bc1e-ae67d5ad6bb1)
> [!NOTE]
> [Quick video showcasing the 2D version running.](https://youtu.be/x-2bFkBimAg)

<p width=100% align="center">
  <img src="https://github.com/user-attachments/assets/00361a18-c112-4a95-a7bf-c8add94cf652" width="45%" alt="No Shading" />
  <img src="https://github.com/user-attachments/assets/aca5ad0d-bc05-40c2-a2d4-1fc2b90e3623" width="45%" alt="Shading" />
</p>

> [!NOTE]
> The simulation is <u></u>paused</u> here, so FPS is higher than in real-time (1920x1060@144hz 60FPS with a laptop RTX 4050).

## Dependencies
To build the executable, there are a few required dependencies:
- GLEW
- GLFW
- GLM
- CMake >= 3.13.0

## How to install and build
```
git clone https://github.com/imMegu/SPH_OpenGL.git
cd SPH_OpenGL/3D          # (the 2D version lives in 2D/)

mkdir build && cd build
cmake ..
make
./execute
```

## Controls
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

## Simulation steps

| Pass | Shader | What it does |
| --- | --- | --- |
| External | `external.compute` | Applies gravity and computes each particle's *predicted position* (`pos + vel·dt`), which all neighbor queries use |
| Count | `count.compute` | Computes each particle's grid cell and atomically counts particles per cell |
| Scan | `scan_blocks/sums/add.compute` | Exclusive prefix sum over the cell counts — the result *is* the table of where each cell's particles start |
| Scatter | `scatter.compute` | Copies each particle's data into its sorted slot (see *data reorder* below) |
| Density | `density.compute` | Accumulates density and near-density from neighbors via the two spiky kernels |
| Update | `update.compute` | Pressure + near-pressure + viscosity forces, integration, and boundary collisions in the box's local space |

## Neighborhood search
SPH needs each particle's neighbors within the smoothing radius `h`. Brute force is `O(n²)`; this implementation bins particles into a grid of `h`-sized cells, so each particle only tests the 27 surrounding cells.

**Dense uniform grid.** The grid covers the world-space Axis Aligned Bounding Box of the box, recomputed every frame from the box bounds and `h`, and grown on demand. Cells are indexed directly (`x + nx·(y + ny·z)`) — unlike spatial hashing there are no hash collisions, so no double-counted neighbors. (The 2D version still uses the spatial-hashing variant from the NVIDIA paper.)

**Counting sort, O(n).** Because the sort keys are already small cell indices, no comparison sort is needed:
1. *Count* — every particle adds 1 to a counter for its cell.
2. *Scan* — turn those counts into starting positions with a running total. If cell 0 holds 3 particles and cell 1 holds 5, then cell 0's particles will occupy slots 0–2, cell 1's slots 3–7, cell 2 starts at slot 8, and so on.
3. *Scatter* — every particle moves to its slot: "my cell's starting position, plus the number of particles that got there before me". When this finishes, each cell's particles sit side by side in one contiguous range, `[cellStart, cellEnd)`.

This replaced a bitonic merge sort (`O(n log²n)`, ~60 dispatches/frame) with ~8 dispatches, and lifted the power-of-two particle count restriction.

**Data reorder.** The scatter pass moves the positions, velocities and predicted positions themselves into cell order. Neighbour loops then read *contiguous* memory. The Update pass writes its results back *at the sorted index*, so the sorted order simply becomes next frame's particle order.

## Rendering
Particles are camera-facing quads ("impostors"). In the plain view, the fragment shader reconstructs the sphere's view-space normal from the quad coordinates, shades it diffusely, and writes the true sphere depth to `gl_FragDepth` so overlapping particles intersect like solid spheres. Color encodes velocity.

The **Water Shading** mode is screen-space fluid rendering (van der Laan et al.), five passes per frame:
1. **Scene** — background and box render into an offscreen target (the water refracts it later).
2. **Fluid depth** — impostors write eye-space depth into an `R32F` target.
3. **Bilateral blur** — two separable passes smooth the depth into a continuous surface; a depth-difference weight stops the blur at silhouettes. This is what melts individual spheres into "a liquid".
4. **Thickness** — additive pass accumulating each impostor's chord length through the fluid.
5. **Composite** — surface normals come from the smoothed depth's gradients (taking the smaller one-sided difference near silhouettes). The background is sampled with a normal-based refraction offset and tinted by Beer–Lambert absorption through the accumulated thickness (deeper = bluer), then mixed with a procedural sky reflection by Schlick's Fresnel approximation, plus a specular highlight.

# Sources
- M. Müller, D. Charypar, M. Gross — [*Particle-Based Fluid Simulation for Interactive Applications*](https://matthias-research.github.io/pages/publications/sca03.pdf) (SCA 2003). The foundational interactive-SPH paper: smoothing kernels, pressure and viscosity forces.
- S. Clavet, P. Beaudoin, P. Poulin — [*Particle-based Viscoelastic Fluid Simulation*](https://www.ligum.umontreal.ca/Clavet-2005-PVFS/pvfs.pdf) (SCA 2005). The near-pressure / near-density term.
- S. Lague — [*Coding Adventure: Simulating Fluids*](https://www.youtube.com/watch?v=rSKMYc1CQHE) (2023). The kernel formulation this implementation descends from.
- S. Green — [*Particle Simulation using CUDA*](https://web.archive.org/web/20140725014123/https://docs.nvidia.com/cuda/samples/5_Simulations/particles/doc/particles.pdf) (NVIDIA, 2010). Uniform grid construction and the "reorder data and find cell start" coalescing technique.
- R. Hoetzlein — [*Fast Fixed-Radius Nearest Neighbors*](https://on-demand.gputechconf.com/gtc/2014/presentations/S4117-fast-fixed-radius-nearest-neighbor-gpu.pdf) (GTC 2014). The counting-sort pipeline.
- M. Harris, S. Sengupta, J. D. Owens — [*Parallel Prefix Sum (Scan) with CUDA*](https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda) (GPU Gems 3, ch. 39). The Blelloch block scan.
- C. Sigg, T. Weyrich, M. Botsch, M. Gross — [*GPU-Based Ray-Casting of Quadratic Surfaces*](https://dl.acm.org/doi/10.5555/2386388.2386396) (PBG 2006). Sphere impostors with per-fragment depth.
- W. J. van der Laan, S. Green, M. Sainz — [*Screen Space Fluid Rendering with Curvature Flow*](https://dl.acm.org/doi/10.1145/1507149.1507164) (I3D 2009). The screen-space water pipeline (this implementation substitutes a separable bilateral blur for curvature flow).
# Roadmap
- **Visual Enhancements:** Environment-map reflections, foam/spray particles.

