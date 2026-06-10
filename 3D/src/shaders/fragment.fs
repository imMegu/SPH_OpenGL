#version 430 core

in vec2 texCoord;
in vec4 particleColor;
out vec4 FragColor;

void main() {
    if (length(texCoord) > 1.0) discard;
    FragColor = vec4(particleColor.rgb, 1.0);
}
