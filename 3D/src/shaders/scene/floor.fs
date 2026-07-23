#version 430 core

// Checkered floor: one colour per quadrant around the box centre, every other
// tile darkened, plus a small random hue/sat/val tweak per tile.

in vec3 worldPos;
out vec4 FragColor;

uniform vec2 u_floorCenter;       // xz of the box centre: quadrants split here
uniform float u_tileScale;        // tiles per world unit
uniform float u_tileDarkOffset;   // value offset for the dark checker tiles
uniform float u_tileColVariation; // strength of the per-tile colour tweak

uniform sampler2D u_shadowMap; // light-space depth of the frontmost fluid, 0 = none
uniform mat4 u_lightView;
uniform mat4 u_lightVP;
uniform float u_shadowStrength;

const vec3 tileCol1 = vec3(0.800, 0.518, 0.475); // #cc8479 (-x +z)
const vec3 tileCol2 = vec3(0.565, 0.890, 0.643); // #90e3a4 (+x +z)
const vec3 tileCol3 = vec3(0.961, 0.604, 0.996); // #f59afe (-x -z)
const vec3 tileCol4 = vec3(0.502, 0.537, 0.929); // #8089ed (+x -z)

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 tweakHSV(vec3 col, vec3 delta) {
    vec3 hsv = rgb2hsv(col) + delta;
    return hsv2rgb(vec3(fract(hsv.x), clamp(hsv.yz, 0.0, 1.0)));
}

// PCG-style hash of the tile coordinate -> 3 floats in [-1, 1]
vec3 randomSNorm3(ivec2 tile) {
    uint s = uint(tile.x) * 1973u + uint(tile.y) * 9277u + 26699u;
    vec3 r;
    for (int i = 0; i < 3; i++) {
        s = s * 747796405u + 2891336453u;
        uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
        w = (w >> 22u) ^ w;
        r[i] = float(w) / 2147483648.0 - 1.0;
    }
    return r;
}

// 3x3-PCF shadow from the fluid above. Darkness scales with how much fluid
// hangs over (depth from light past the fluid front), so thin splashes cast
// fainter shadows than the bulk - a cheap stand-in for absorption. The floor
// itself is never in the shadow map, so no acne/bias issues.
float fluidShadow(vec3 wp) {
    vec2 lightUV = (u_lightVP * vec4(wp, 1.0)).xy * 0.5 + 0.5; // ortho: w=1
    if (any(lessThan(lightUV, vec2(0.0))) || any(greaterThan(lightUV, vec2(1.0))))
        return 0.0;
    float depthFromLight = -(u_lightView * vec4(wp, 1.0)).z;
    vec2 texel = 1.0 / vec2(textureSize(u_shadowMap, 0));
    float sum = 0.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            float front = texture(u_shadowMap, lightUV + vec2(x, y) * texel).r;
            if (front > 0.0)
                sum += clamp((depthFromLight - front) / 0.05, 0.0, 1.0);
        }
    return sum / 9.0;
}

void main() {
    vec2 p = worldPos.xz - u_floorCenter;

    vec3 tileCol = p.x < 0.0 ? tileCol1 : tileCol2;
    if (p.y < 0.0) tileCol = p.x < 0.0 ? tileCol3 : tileCol4;

    ivec2 tileCoord = ivec2(floor(p * u_tileScale));
    bool isDarkTile = (tileCoord.x & 1) == (tileCoord.y & 1);
    vec3 col =
        tweakHSV(tileCol, vec3(0.0, 0.0, u_tileDarkOffset * float(isDarkTile)));

    col = tweakHSV(col, randomSNorm3(tileCoord) * u_tileColVariation * 0.1);

    // Fade the pattern to the base colour where a tile shrinks toward pixel
    // size, so the distance doesn't moire
    float footprint = length(fwidth(p)) * u_tileScale;
    col = mix(col, tileCol * (1.0 + u_tileDarkOffset * 0.5),
              clamp(footprint * 2.0 - 0.5, 0.0, 1.0));

    col *= 1.0 - fluidShadow(worldPos) * u_shadowStrength;

    FragColor = vec4(col, 1.0);
}
