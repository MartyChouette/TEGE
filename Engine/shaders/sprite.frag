#version 450

// Simple 2D sprite fragment shader — texture + tint, no lighting

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragTintAlpha;

layout(location = 0) out vec4 outColor;

// Base color texture sampler (same binding as main pipeline)
layout(binding = 3) uniform sampler2D baseColorTexture;

// Push constants — minimal, just need to match pipeline layout
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor;
    float metallic;
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
    vec4 texColor = texture(baseColorTexture, fragUV);

    vec3 color = texColor.rgb * fragTintAlpha.rgb;
    float alpha = texColor.a * fragTintAlpha.a;

    // Alpha discard
    if (alpha < 0.01) {
        discard;
    }

    outColor = vec4(color, alpha);
}
