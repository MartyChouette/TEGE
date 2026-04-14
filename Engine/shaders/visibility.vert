#version 450

// Visibility buffer pass — vertex shader
// Outputs only gl_Position and flat instance/triangle IDs.
// No material evaluation, no texture sampling.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor; float metallic;
    vec3 emissiveColor; float roughness;
    float emissiveStrength;
    float opacity;
    float alphaCutoff;
    int flags;
    float parallaxScale;
    float surfaceParam1, surfaceParam2, surfaceParam3;
} pc;

layout(binding = 0) uniform ViewProjectionUBO {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) flat out uint outInstanceID;

void main() {
    gl_Position = ubo.proj * ubo.view * pc.model * vec4(inPosition, 1.0);
    outInstanceID = uint(gl_InstanceIndex);
}
