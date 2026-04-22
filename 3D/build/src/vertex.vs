#version 430 core

layout (location = 0) in vec2 quadVertex;     // Base quad vertex
layout (location = 1) in vec4 instancePos;    // Per-instance position
layout (location = 2) in vec4 instanceVel;    // Per-instance velocity

uniform mat4 u_proj;
uniform mat4 u_view;
uniform float sphereRadius;

out vec2 texCoord;
out vec4 particleColor;

void main() {
    // Calculate color based on velocity
    float speed = length(instanceVel.xyz);
    float t = clamp(speed / 0.15, 0.0, 1.0);

    vec4 color1 = vec4(23.0/255.0,  71.0/255.0, 162.0/255.0, 1.0);
    vec4 color2 = vec4(87.0/255.0, 254.0/255.0, 145.0/255.0, 1.0);
    vec4 color3 = vec4(0.9, 0.9, 0.9, 1.0);
    vec4 color4 = vec4(239.0/255.0, 81.0/255.0,  15.0/255.0, 1.0);

    if (t <= 0.55) {
        float localT = t / 0.55;
        particleColor = mix(color1, color2, localT);
    } else { 
        float localT = (t - 0.55) / (1.00 - 0.55);
        particleColor = mix(color2, color3, localT);
    }     
    // Billboard transformation
    vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]) * sphereRadius;
    vec3 up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]) * sphereRadius;
    
    vec3 worldPos = instancePos.xyz + right * quadVertex.x + up * quadVertex.y;
    gl_Position = u_proj * u_view * vec4(worldPos, 1.0);
    
    texCoord = quadVertex;
}
