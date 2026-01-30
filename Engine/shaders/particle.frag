#version 450

// Simple alpha particle fragment shader

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragAlpha;

layout(location = 0) out vec4 outColor;

// Push constants for particle color and type
layout(push_constant) uniform PushConstants {
    mat4 model;       // unused, but layout must match pipeline
    vec3 baseColor;
    float metallic;   // reused: 0 = rain (full fill), 1 = snow (radial falloff)
    vec3 emissiveColor;
    float roughness;
    float emissiveStrength;
    float opacity;
    float alphaCutoff;
    int flags;
    float parallaxScale;
    float _pad0;
    float _pad1;
    float _pad2;
} material;

void main() {
    float alpha = fragAlpha * material.opacity;

    // Snow: radial falloff from center
    if (material.metallic > 0.5) {
        float dist = length(fragUV - vec2(0.5));
        alpha *= smoothstep(0.5, 0.2, dist);
    }

    if (alpha < 0.01) {
        discard;
    }

    outColor = vec4(material.baseColor, alpha);
}
