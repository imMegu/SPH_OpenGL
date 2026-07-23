# Sources

References for the techniques used in this project.

## SPH fluid simulation
- M. Müller, D. Charypar, M. Gross — *Particle-Based Fluid Simulation for Interactive Applications* (SCA 2003).
  The foundational interactive-SPH paper: smoothing kernels (Poly6, Spiky), pressure and viscosity forces.
  https://matthias-research.github.io/pages/publications/sca03.pdf
- S. Clavet, P. Beaudoin, P. Poulin — *Particle-based Viscoelastic Fluid Simulation* (SCA 2005).
  Source of the **near-pressure / near-density** term: a purely repulsive short-range pressure with no
  rest density that prevents particle clumping (double-density relaxation, §4).
  https://www.ligum.umontreal.ca/Clavet-2005-PVFS/pvfs.pdf
- S. Lague — *Coding Adventure: Simulating Fluids* (2023). The kernel formulation
  (SpikyPow2/SpikyPow3 density kernels, shared-pressure symmetrization) this implementation descends from.
  https://www.youtube.com/watch?v=rSKMYc1CQHE

## Neighborhood search: grid, counting sort, data reorder
- S. Green — *Particle Simulation using CUDA* (NVIDIA, 2010). Uniform grid construction by sorting
  particles by cell index, and the **"reorder data and find cell start"** step: scattering particle data
  into cell order so neighbor loops read contiguous (coalesced) memory.
  https://web.archive.org/web/20140725014123/https://docs.nvidia.com/cuda/samples/5_Simulations/particles/doc/particles.pdf
- R. Hoetzlein — *Fast Fixed-Radius Nearest Neighbors: Interactive Million-Particle Fluids* (GTC 2014).
  The **counting sort** pipeline used here: per-cell atomic histogram, prefix sum (which doubles as the
  cell-start table), then scatter — O(n), replacing comparison sorts entirely.
  https://on-demand.gputechconf.com/gtc/2014/presentations/S4117-fast-fixed-radius-nearest-neighbor-gpu.pdf

## Parallel prefix sum (scan)
- M. Harris, S. Sengupta, J. D. Owens — *Parallel Prefix Sum (Scan) with CUDA* (GPU Gems 3, ch. 39, 2007).
  The work-efficient Blelloch block scan in `scan_blocks.compute` (up-sweep/down-sweep in shared memory)
  and the scan-of-block-sums + uniform-add structure of the multi-level scan.
  https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda
- G. E. Blelloch — *Prefix Sums and Their Applications* (CMU technical report, 1990). The underlying
  work-efficient scan algorithm.
  https://www.cs.cmu.edu/~guyb/papers/Ble93.pdf

## Sphere impostor rendering
- C. Sigg, T. Weyrich, M. Botsch, M. Gross — *GPU-Based Ray-Casting of Quadratic Surfaces* (PBG 2006).
  Rendering analytic spheres from screen-aligned quads, reconstructing the surface normal and writing
  per-fragment depth (`gl_FragDepth`) so impostors intersect correctly.
  https://dl.acm.org/doi/10.5555/2386388.2386396
- W. J. van der Laan, S. Green, M. Sainz — *Screen Space Fluid Rendering with Curvature Flow* (I3D 2009).
  The screen-space water pipeline implemented in `renderer.cpp`: impostor depth pass, bilateral
  depth smoothing, normal reconstruction from depth gradients, additive thickness pass, and
  compositing with Fresnel reflection and Beer-Lambert absorption of the refracted background.
  (This implementation uses a separable bilateral blur rather than the paper's curvature flow.)
  https://dl.acm.org/doi/10.1145/1507149.1507164
