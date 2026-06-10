#version 430 core

// Writes the eye-space depth of the sphere impostor surface. 0 marks pixels
// with no fluid (the target is cleared to 0).

in vec2 texCoord;
in vec3 viewCenter;
in vec4 particleColor; // unused, produced by the shared vertex shader
out float eyeDepth;

uniform mat4 u_proj;
uniform float sphereRadius;

void main() {
    float r2 = dot(texCoord, texCoord);
    if (r2 > 1.0) discard;

    vec3 viewPos = viewCenter + vec3(texCoord, sqrt(1.0 - r2)) * sphereRadius;

    vec4 clipPos = u_proj * vec4(viewPos, 1.0);
    gl_FragDepth = (clipPos.z / clipPos.w) * 0.5 + 0.5;
    eyeDepth = -viewPos.z;
}
