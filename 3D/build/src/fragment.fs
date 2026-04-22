#version 430 core

in vec2 texCoord;
in vec4 particleColor;
out vec4 FragColor;

void main() {
    float dist = length(texCoord);
    if (dist > 1.0) discard;
    
    // Optional: Add some shading for better sphere appearance
    float alpha = 1.0 - smoothstep(0.8, 1.0, dist);
    FragColor = vec4(particleColor.rgb, particleColor.a * alpha);
}
