#version 430 core

layout(std430, binding = 1) buffer Velocities {
    vec2 velocities[];
};

layout(location = 0) in vec2 position;

uniform mat4 u_proj;
uniform mat4 u_view;

out vec2 vel;

void main() {
    int id = gl_VertexID;
    vel = velocities[id];
    // MATRIX MULTIPLICATION IS NOT COMMUTATIVE
    gl_Position = u_proj * u_view * vec4(position, 0.0, 1.0);
    gl_PointSize = 3;
}