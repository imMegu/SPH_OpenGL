// Shared shader definitions, pulled in via #include "common.glsl" (expanded
// by CompileShader). WORKGROUP_SIZE is injected by CompileShader from the
// constant in simulation.h.
//
// Dense uniform grid over the world-space AABB of the simulation box. Cells
// are indexed directly, so distinct cells never collide (unlike hashing).

ivec3 GetCellCoord(vec3 pos, vec3 gridMin, float cellSize, ivec3 gridDims)
{
    return clamp(ivec3(floor((pos - gridMin) / cellSize)),
                 ivec3(0), gridDims - 1);
}

uint GetFlatCellIndex(ivec3 cellCoord, ivec3 gridDims)
{
    return uint(cellCoord.x + gridDims.x * (cellCoord.y + gridDims.y * cellCoord.z));
}
