#version 450

// Fluid cell fragment shader
// Simple alpha-blended color output

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragAlpha;
layout(location = 2) in vec3 fragColor;   // per-cell fluid colour

layout(location = 0) out vec4 outColor;

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
    float alpha = fragAlpha * material.opacity;

    if (alpha < 0.01) {
        discard;
    }

    // Use the per-cell fluid colour (from the Fluid Volume's Color knob). Falls back
    // to white only if the instance colour is unset (near-black).
    vec3 col = (dot(fragColor, fragColor) > 0.0001) ? fragColor : material.baseColor;
    outColor = vec4(col, alpha);
}
