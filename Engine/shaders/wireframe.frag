#version 450

// Per-Entity Wireframe Overlay Fragment Shader
// Paired with the main triangle.vert (for skinning support). Declares NO vertex
// inputs — triangle.vert's outputs (vec3 fragWorldPos at location 0, etc.) are
// simply unconsumed, which is legal; reading location 0 as vec4 like outline.frag
// did is not (VUID-08743). Flat color comes from push constants
// (MeshRendererComponent wireframeColor/wireframeOpacity).
// MRT: color + zero velocity, matching the main render pass.

layout(push_constant) uniform PushConstants {
    mat4 model;  // Used by vertex shader
    vec3 baseColor;
    float metallic;
    vec3 emissiveColor;
    float roughness;
    float emissiveStrength;
    float opacity;
    float alphaCutoff;
    int flags;
    float parallaxScale;
    float surfaceParam1;
    float surfaceParam2;
    float surfaceParam3;
} material;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outVelocity;  // MRT velocity attachment (RG16F)

void main() {
    outColor = vec4(material.baseColor, material.opacity);
    outVelocity = vec2(0.0);  // No motion vectors for the debug overlay
}
