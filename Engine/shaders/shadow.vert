#version 450

// Shadow Map Depth-Only Vertex Shader
// Uses push constants for the pre-multiplied cascadeViewProj * model matrix.
// Applies GPU skinning when FLAG_SKINNED is set so animated mesh shadows
// match the main pass's deformed geometry.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inTangent;
layout(location = 5) in vec4 inBoneWeights;
layout(location = 6) in uvec4 inBoneIndices;

// Push constants layout must match the full PushConstants struct (128 bytes).
// Shadow pass only uses the model matrix (bytes 0-63) and flags (byte 112).
layout(push_constant) uniform PushConstants {
    mat4 model;               // 0-63: cascadeVP * worldMatrix
    vec3 baseColor;           // 64-75
    float metallic;           // 76-79
    vec3 emissiveColor;       // 80-91
    float roughness;          // 92-95
    float emissiveStrength;   // 96-99
    float opacity;            // 100-103
    float alphaCutoff;        // 104-107
    uint flags;               // 108-111
    float parallaxScale;      // 112-115
} push;

#define FLAG_SKINNED (1 << 3)

// Bone matrix SSBO (binding 7, same as main pass)
layout(std430, binding = 7) readonly buffer BoneMatrixSSBO {
    mat4 boneMatrices[];
};

void main() {
    vec3 pos = inPosition;

    // Apply skeletal skinning if FLAG_SKINNED is set
    if ((push.flags & FLAG_SKINNED) != 0) {
        float weightSum = inBoneWeights.x + inBoneWeights.y + inBoneWeights.z + inBoneWeights.w;
        if (weightSum > 0.0) {
            mat4 skinMatrix = inBoneWeights.x * boneMatrices[inBoneIndices.x]
                            + inBoneWeights.y * boneMatrices[inBoneIndices.y]
                            + inBoneWeights.z * boneMatrices[inBoneIndices.z]
                            + inBoneWeights.w * boneMatrices[inBoneIndices.w];
            pos = (skinMatrix * vec4(inPosition, 1.0)).xyz;
        }
    }

    gl_Position = push.model * vec4(pos, 1.0);
}
