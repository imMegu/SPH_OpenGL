#version 430 core

layout (location = 0) in vec2 quadVertex;     // Base quad vertex
layout (location = 1) in vec4 instancePos;    // Per-instance position
layout (location = 2) in vec4 instanceVel;    // Per-instance velocity

uniform mat4 u_proj;
uniform mat4 u_view;
uniform float sphereRadius;

out vec2 texCoord;
out vec3 viewCenter;
out vec4 particleColor;

void main() {
    // Calculate color based on velocity
    float speed = length(instanceVel.xyz);
    float t = clamp(speed / 0.15, 0.0, 1.0);

    vec4 color1 = vec4(23.0/255.0,  71.0/255.0, 162.0/255.0, 1.0);
    vec4 color2 = vec4(87.0/255.0, 254.0/255.0, 145.0/255.0, 1.0);
    vec4 color3 = vec4(0.9, 0.9, 0.9, 1.0);

    if (t <= 0.55) {
        particleColor = mix(color1, color2, t / 0.55);
    } else {
        particleColor = mix(color2, color3, (t - 0.55) / 0.45);
    }

    // Billboard directly in view space; the fragment shader reconstructs the
    // sphere surface from texCoord
    viewCenter = (u_view * vec4(instancePos.xyz, 1.0)).xyz;
    vec3 viewPos = viewCenter + vec3(quadVertex, 0.0) * sphereRadius;
    gl_Position = u_proj * vec4(viewPos, 1.0);

    texCoord = quadVertex;
}
