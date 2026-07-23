#version 430 core

in vec2 texCoord;
in vec3 viewCenter;
in vec4 particleColor;
out vec4 FragColor;

uniform mat4 u_proj;
uniform float sphereRadius;

void main() {
    float r2 = dot(texCoord, texCoord);
    if (r2 > 1.0) discard;

    // Reconstruct the sphere surface at this fragment (view space). The
    // billboard is view-aligned, so the normal is simply (texCoord, z)
    vec3 normal = vec3(texCoord, sqrt(1.0 - r2));
    vec3 viewPos = viewCenter + normal * sphereRadius;

    // Write the true sphere depth so overlapping impostors intersect like
    // solid spheres instead of flat discs
    vec4 clipPos = u_proj * vec4(viewPos, 1.0);
    gl_FragDepth = (clipPos.z / clipPos.w) * 0.5 + 0.5;

    // Simple diffuse shading with a fixed view-space light
    vec3 lightDir = normalize(vec3(0.4, 0.6, 0.8));
    float diffuse = max(dot(normal, lightDir), 0.0);
    FragColor = vec4(particleColor.rgb * (0.35 + 0.65 * diffuse), 1.0);
}
