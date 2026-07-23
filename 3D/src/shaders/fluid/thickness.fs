#version 430 core

// Accumulates fluid thickness: each impostor adds its chord length at this
// pixel. Rendered with additive blending and no depth test.

in vec2 texCoord;
in vec3 viewCenter;
in vec4 particleColor; // unused, produced by the shared vertex shader
out float thickness;

uniform float sphereRadius;

void main() {
    float r2 = dot(texCoord, texCoord);
    if (r2 > 1.0) discard;

    thickness = 2.0 * sphereRadius * sqrt(1.0 - r2);
}
