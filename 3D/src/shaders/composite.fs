#version 430 core

// Screen-space fluid compositing: reconstructs the water surface from the
// blurred depth buffer, then shades it with refraction (background distorted
// by the surface normal, attenuated by Beer-Lambert absorption through the
// accumulated thickness), a procedural sky reflection weighted by Fresnel,
// and a specular highlight.

in vec2 uv;
out vec4 FragColor;

uniform sampler2D fluidDepthTex; // bilateral-blurred eye-space depth, 0 = no fluid
uniform sampler2D thicknessTex;
uniform sampler2D sceneColorTex;
uniform sampler2D sceneDepthTex; // hardware depth of the background scene
uniform mat4 u_proj;
uniform mat4 u_view;
uniform float absorption;

// Reconstruct view-space position from screen uv + eye-space depth
vec3 EyePos(vec2 coord, float eyeDepth) {
    vec2 ndc = coord * 2.0 - 1.0;
    return vec3(ndc.x * eyeDepth / u_proj[0][0],
                ndc.y * eyeDepth / u_proj[1][1],
                -eyeDepth);
}

// Hardware depth sample -> positive eye-space depth
float LinearizeSceneDepth(float d) {
    float zNdc = d * 2.0 - 1.0;
    return u_proj[3][2] / (zNdc + u_proj[2][2]);
}

void main() {
    vec3 scene = texture(sceneColorTex, uv).rgb;
    float depth = texture(fluidDepthTex, uv).r;
    if (depth <= 0.0) { // no fluid here
        FragColor = vec4(scene, 1.0);
        return;
    }
    float sceneEye = LinearizeSceneDepth(texture(sceneDepthTex, uv).r);
    if (sceneEye < depth) { // scene geometry in front of the water
        FragColor = vec4(scene, 1.0);
        return;
    }

    vec2 texel = 1.0 / vec2(textureSize(fluidDepthTex, 0));
    vec3 P = EyePos(uv, depth);

    // Surface normal from depth gradients; on each axis take the smaller
    // difference so silhouette edges don't produce wild normals
    float dR = texture(fluidDepthTex, uv + vec2(texel.x, 0.0)).r;
    float dL = texture(fluidDepthTex, uv - vec2(texel.x, 0.0)).r;
    float dT = texture(fluidDepthTex, uv + vec2(0.0, texel.y)).r;
    float dB = texture(fluidDepthTex, uv - vec2(0.0, texel.y)).r;

    vec3 ddxF = EyePos(uv + vec2(texel.x, 0.0), dR) - P;
    vec3 ddxB = P - EyePos(uv - vec2(texel.x, 0.0), dL);
    vec3 ddx = (dR <= 0.0 || (dL > 0.0 && abs(ddxB.z) < abs(ddxF.z))) ? ddxB : ddxF;

    vec3 ddyF = EyePos(uv + vec2(0.0, texel.y), dT) - P;
    vec3 ddyB = P - EyePos(uv - vec2(0.0, texel.y), dB);
    vec3 ddy = (dT <= 0.0 || (dB > 0.0 && abs(ddyB.z) < abs(ddyF.z))) ? ddyB : ddyF;

    vec3 N = normalize(cross(ddx, ddy));
    if (N.z < 0.0) N = -N; // always face the camera

    vec3 viewDir = normalize(-P);
    vec3 lightDir = normalize(vec3(0.4, 0.6, 0.8)); // view space

    float thickness = texture(thicknessTex, uv).r;

    // Refraction: sample the background offset along the surface normal, then
    // tint by Beer-Lambert absorption through the fluid
    vec2 refractUV = uv + N.xy * 0.025 * clamp(thickness * 6.0, 0.0, 1.0);
    vec3 transmit = exp(-absorption * thickness * vec3(1.0, 0.45, 0.3));
    vec3 refracted = texture(sceneColorTex, refractUV).rgb * transmit;

    // Procedural sky reflection (gradient along world-space up)
    vec3 reflWorld = transpose(mat3(u_view)) * reflect(-viewDir, N);
    vec3 sky = mix(vec3(0.08, 0.10, 0.12), vec3(0.45, 0.55, 0.70),
                   clamp(reflWorld.y * 0.5 + 0.5, 0.0, 1.0));

    // Schlick's Fresnel approximation
    float fresnel = 0.02 + 0.98 * pow(1.0 - max(dot(N, viewDir), 0.0), 5.0);

    float spec = pow(max(dot(N, normalize(lightDir + viewDir)), 0.0), 150.0);

    vec3 color = mix(refracted, sky, fresnel) + spec * vec3(0.6);
    FragColor = vec4(color, 1.0);
}
