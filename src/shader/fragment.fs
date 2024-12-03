#version 430 core

out vec4 fragColor; // Output fragment color

in vec2 vel; // Input velocity from vertex shader 

void main() {

    float velocityMagnitude = length(vel);
    float normalizedVelocity = clamp(velocityMagnitude / 0.20, 0.0, 1.0);

    // Gradient stops
    vec4 color1 = vec4(23.0 / 255.0, 71.0 / 255.0, 162.0 / 255.0, 1.0);   // rgba(23,71,162,1) at 0%
    vec4 color2 = vec4(87.0 / 255.0, 254.0 / 255.0, 145.0 / 255.0, 1.0);  // rgba(87,254,145,1) at 55%
    vec4 color3 = vec4(248.0 / 255.0, 238.0 / 255.0, 35.0 / 255.0, 1.0);  // rgba(248,238,35,1) at 66%
    vec4 color4 = vec4(239.0 / 255.0, 81.0 / 255.0, 15.0 / 255.0, 1.0);   // rgba(239,81,15,1) at 100%

    // Map normalized velocity to the gradient
    vec4 color;
    if (normalizedVelocity < 0.55) {
        // Interpolate between color1 and color2 (0% to 55%)
        color = mix(color1, color2, normalizedVelocity / 0.55);
    } else if (normalizedVelocity < 0.66) {
        // Interpolate between color2 and color3 (55% to 66%)
        color = mix(color2, color3, (normalizedVelocity - 0.55) / (0.66 - 0.55));
    } else {
        // Interpolate between color3 and color4 (66% to 100%)
        color = mix(color3, color4, (normalizedVelocity - 0.66) / (1.0 - 0.66));
    }
    
    fragColor = color;
}