#version 450

// Shadow Map Alpha-Cutout Vertex Shader
// shadow.vert plus a UV passthrough so the fragment stage can sample the
// material's base color alpha and punch cutouts (foliage cards, masked hair)
// into the shadow map instead of casting a full-quad shadow.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inTangent;
layout(location = 5) in vec4 inBoneWeights;
layout(location = 6) in uvec4 inBoneIndices;
layout(location = 7) in vec2 inUV1;           // unused here; declared to match the shared vertex layout
layout(location = 8) in vec4 inBoneWeights2;  // bone influences 5-8
layout(location = 9) in uvec4 inBoneIndices2;

// Push constants layout must match the full PushConstants struct (128 bytes).
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

layout(location = 0) out vec2 fragUV;

void main() {
    vec3 pos = inPosition;

    // Apply skeletal skinning if FLAG_SKINNED is set
    if ((push.flags & FLAG_SKINNED) != 0) {
        float weightSum = dot(inBoneWeights, vec4(1.0)) + dot(inBoneWeights2, vec4(1.0));
        if (weightSum > 0.0) {
            mat4 skinMatrix = inBoneWeights.x * boneMatrices[inBoneIndices.x]
                            + inBoneWeights.y * boneMatrices[inBoneIndices.y]
                            + inBoneWeights.z * boneMatrices[inBoneIndices.z]
                            + inBoneWeights.w * boneMatrices[inBoneIndices.w]
                            + inBoneWeights2.x * boneMatrices[inBoneIndices2.x]
                            + inBoneWeights2.y * boneMatrices[inBoneIndices2.y]
                            + inBoneWeights2.z * boneMatrices[inBoneIndices2.z]
                            + inBoneWeights2.w * boneMatrices[inBoneIndices2.w];
            pos = (skinMatrix * vec4(inPosition, 1.0)).xyz;
        }
    }

    gl_Position = push.model * vec4(pos, 1.0);
    fragUV = inUV;
}
