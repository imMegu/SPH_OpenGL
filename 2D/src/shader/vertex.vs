#version 430 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 velocity;

uniform mat4 u_proj;
uniform mat4 u_view;

out vec2 vel;

void main() {
    // MATRIX MULTIPLICATION IS NOT COMMUTATIVE
    vel = velocity;
    gl_Position = u_proj * u_view * vec4(position, 0.0, 1.0);
    gl_PointSize = 3;
}