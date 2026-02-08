#version 450

// Shadow Map Depth-Only Vertex Shader
// Uses push constants for the pre-multiplied cascadeViewProj * model matrix.
// This avoids the HOST_COHERENT UBO race where the main pass overwrites
// shadow cascade data before the GPU executes.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inTangent;
layout(location = 5) in vec4 inBoneWeights;
layout(location = 6) in uvec4 inBoneIndices;

// Pre-multiplied cascadeViewProj * model stored in push constants (first 64 bytes)
layout(push_constant) uniform PushConstants {
    mat4 mvp;
} push;

void main() {
    gl_Position = push.mvp * vec4(inPosition, 1.0);
}
