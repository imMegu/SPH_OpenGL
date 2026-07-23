#version 430 core

// One direction of a separable bilateral blur over the fluid depth texture
// (van der Laan et al., "Screen Space Fluid Rendering with Curvature Flow").
// The range weight stops the blur from bleeding across silhouettes.

in vec2 uv;
out float result;

uniform sampler2D source;
uniform vec2 blurDir;       // one texel step in x or y
uniform float depthFalloff; // bilateral range weight (1 / eye-space units)

const int RADIUS = 10;

void main() {
    float center = texture(source, uv).r;
    if (center <= 0.0) {
        result = 0.0;
        return;
    }

    float sum = 0.0;
    float wsum = 0.0;
    for (int i = -RADIUS; i <= RADIUS; i++) {
        float s = texture(source, uv + float(i) * blurDir).r;
        if (s <= 0.0) continue; // empty pixel

        float x = float(i) / float(RADIUS);
        float spatial = exp(-x * x * 2.0);

        float d = (s - center) * depthFalloff;
        float range = exp(-d * d);

        sum += s * spatial * range;
        wsum += spatial * range;
    }
    result = sum / max(wsum, 1e-5);
}
