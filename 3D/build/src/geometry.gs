#version 430 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

uniform mat4 u_proj;
uniform mat4 u_view;
uniform float sphereRadius = 0.02;

in vec4 worldPosition[];
in vec4 particleVelocity[];
out vec2 texCoord;
out vec4 particleColor;
out vec3 FragPos;
out float radius;



void main() {
    FragPos = worldPosition[0].xyz;
    
    // Calculate color based on velocity
    float speed = length(particleVelocity[0].xyz);
    float t = clamp(speed / 0.15, 0.0, 1.0);

    vec4 color1 = vec4(23.0/255.0,  71.0/255.0, 162.0/255.0, 1.0); // 0%
    vec4 color2 = vec4(87.0/255.0, 254.0/255.0, 145.0/255.0, 1.0); // 55%
    vec4 color3 = vec4(0.9, 0.9, 0.9, 1.0);
    //vec4 color3 = vec4(248.0/255.0,238.0/255.0,  35.0/255.0, 1.0); // 66%
    vec4 color4 = vec4(239.0/255.0, 81.0/255.0,  15.0/255.0, 1.0); // 100%

    if (t <= 0.55) {
        float localT = t / 0.55; // map [0,0.55] -> [0,1]
        particleColor = mix(color1, color2, localT);
    } else if (t <= 1.0) {
        float localT = (t - 0.55) / (1.00 - 0.55); // map [0.55,0.66] -> [0,1]
        particleColor = mix(color2, color3, localT);
    } else {
        float localT = (t - 0.66) / (1.0 - 0.66); // map [0.66,1] -> [0,1]
        particleColor = mix(color3, color4, localT);
    }
    
    radius = sphereRadius;
    
    // Create a billboard quad facing the camera
    vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]) * sphereRadius;
    vec3 up = vec3(u_view[0][1], u_view[1][1], u_view[2][1])  * sphereRadius;
    
    // Bottom left
    vec3 pos = FragPos - right - up;
    gl_Position = u_proj * u_view * vec4(pos, 1.0);
    texCoord = vec2(-1.0, -1.0);
    EmitVertex();
    
    // Bottom right
    pos = FragPos + right - up;
    gl_Position = u_proj * u_view * vec4(pos, 1.0);
    texCoord = vec2(1.0, -1.0);
    EmitVertex();
    
    // Top left
    pos = FragPos - right + up;
    gl_Position = u_proj * u_view * vec4(pos, 1.0);
    texCoord = vec2(-1.0, 1.0);
    EmitVertex();
    
    // Top right
    pos = FragPos + right + up;
    gl_Position = u_proj * u_view * vec4(pos, 1.0);
    texCoord = vec2(1.0, 1.0);
    EmitVertex();
    
    EndPrimitive();
}
