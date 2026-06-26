#version 450

// Inverted-Hull Geometry Outline Vertex Shader
// Extrudes vertices along normals for backface-rendered outlines.
// Uses the same vertex layout, push constants, and descriptor bindings as triangle.vert.

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

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor;      // Repurposed: outlineColor
    float metallic;      // Repurposed: outlineWidth
    vec3 emissiveColor;
    float roughness;
    float emissiveStrength;
    float opacity;
    float alphaCutoff;
    int flags;
    float parallaxScale;
    float shoreWidth;
    float foamIntensity;
    float foamScale;
} pushConstants;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 prevViewProj;
    vec4 jitterOffset;
} ubo;

// Bone matrix SSBO for skeletal animation
layout(std430, binding = 7) readonly buffer BoneMatrixSSBO {
    mat4 boneMatrices[];
};

#define FLAG_SKINNED (1 << 3)

// MRT outputs (must match render pass attachment count)
layout(location = 0) out vec4 fragColor;

void main() {
    vec3 skinnedPos = inPosition;
    vec3 skinnedNormal = inNormal;

    // Apply skeletal skinning if enabled
    if ((pushConstants.flags & FLAG_SKINNED) != 0) {
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
            skinnedPos = (skinMatrix * vec4(inPosition, 1.0)).xyz;
            skinnedNormal = mat3(skinMatrix) * inNormal;
        }
    }

    // Extrude along object-space normal before model transform
    float outlineWidth = pushConstants.metallic;  // Repurposed field
    vec3 extrudedPos = skinnedPos + normalize(skinnedNormal) * outlineWidth;

    // Transform to world then clip space
    vec4 worldPos = pushConstants.model * vec4(extrudedPos, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;

    // Pass outline color to fragment shader
    fragColor = vec4(pushConstants.baseColor, 1.0);  // Repurposed: outlineColor
}
