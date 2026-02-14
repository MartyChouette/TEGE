#version 450

// Lit Mesh Vertex Shader with Cascaded Shadow Support, Retro Effects, and Skeletal Animation

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inTangent;
layout(location = 5) in vec4 inBoneWeights;
layout(location = 6) in uvec4 inBoneIndices;

// Push constant for per-object model matrix + material flags
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
    float shoreWidth;
    float foamIntensity;
    float foamScale;
} pushConstants;

// Uniform buffer for view/projection (shared across all objects)
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Access lighting UBO for wind data and cascade matrices
// Must match C++ LightingUBO structure exactly
layout(binding = 1) uniform LightingUBO {
    vec3 ambientColor;
    float ambientIntensity;
    vec3 cameraPos;
    float _pad0;
    uint directionalLightCount;
    uint pointLightCount;
    uint spotLightCount;
    uint _pad1;
    mat4 cascadeViewProj[4];
    vec4 cascadeSplits;
    float shadowSoftness;
    int shadowEnabled;
    float shadowStrength;
    float shadowMaxDistance;
    int pointShadowCount;
    int spotShadowCount;
    vec2 _pointSpotPad;
    vec4 windData;  // xyz = wind direction * strength, w = time
    vec4 fogParams;     // x=density, y=start, z=end, w=heightFalloff
    vec4 fogColorSnow;  // xyz=fog color, w=snow intensity
    vec4 playerPosition; // xyz = player world pos, w = step radius
    vec4 worldCurvature; // x = strength, yzw reserved
    vec4 skyReflectColor; // xyz = sky reflection color, w = reserved
    vec4 shProbeIrradiance; // xyz = SH probe irradiance, w = blend weight
    // Note: light arrays follow but we only need windData in vertex shader
} lighting;

// Bone matrix SSBO for skeletal animation
layout(std430, binding = 7) readonly buffer BoneMatrixSSBO {
    mat4 boneMatrices[];
};

// Per-object data SSBO for indirect draws (binding 13)
// Indexed by gl_InstanceIndex (set to original object index via firstInstance)
struct ObjectData {
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
    float _objPad[3];
};
layout(std430, binding = 13) readonly buffer ObjectDataSSBO {
    ObjectData objectData[];
};

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out float fragViewDepth;  // View-space depth for cascade selection
layout(location = 4) out vec4 fragVertColor;
layout(location = 5) out float fragClipW;
layout(location = 6) out vec4 fragTangent;

// Flag bits (must match C++ Material.h and RenderSystem.cpp)
#define FLAG_SKINNED          (1 << 3)
#define FLAG_WIND_SWAY        (1 << 4)
#define FLAG_WATER_SURFACE    (1 << 5)
#define FLAG_RAIN_RIPPLES     (1 << 6)
#define FLAG_WATER_SHORE      (1 << 7)
#define FLAG_WATER_OCEAN      (1 << 11)
#define FLAG_FLAT_SHADING     (1 << 20)
#define FLAG_AFFINE_TEXTURING (1 << 21)
#define FLAG_VERTEX_SNAPPING  (1 << 22)
#define FLAG_STIPPLE_TRANS    (1 << 23)
#define FLAG_UV_QUANTIZE      (1 << 12)
#define FLAG_GOURAUD_ONLY     (1 << 13)

void main() {
    vec3 skinnedPos = inPosition;
    vec3 skinnedNormal = inNormal;
    vec3 skinnedTangent = inTangent.xyz;

    // Apply skeletal skinning if enabled and vertex has bone weights
    if ((pushConstants.flags & FLAG_SKINNED) != 0) {
        float weightSum = inBoneWeights.x + inBoneWeights.y + inBoneWeights.z + inBoneWeights.w;
        if (weightSum > 0.0) {
            mat4 skinMatrix = inBoneWeights.x * boneMatrices[inBoneIndices.x]
                            + inBoneWeights.y * boneMatrices[inBoneIndices.y]
                            + inBoneWeights.z * boneMatrices[inBoneIndices.z]
                            + inBoneWeights.w * boneMatrices[inBoneIndices.w];

            skinnedPos = (skinMatrix * vec4(inPosition, 1.0)).xyz;
            skinnedNormal = mat3(skinMatrix) * inNormal;
            skinnedTangent = mat3(skinMatrix) * inTangent.xyz;
        }
    }

    // Transform vertex position to world space
    vec4 worldPos = pushConstants.model * vec4(skinnedPos, 1.0);

    // Wind sway for vegetation (trees, bushes)
    // Uses vertex color red channel as sway weight (trunk=0, leaves=1)
    if ((pushConstants.flags & FLAG_WIND_SWAY) != 0) {
        vec3 windDir = lighting.windData.xyz;
        float windTime = lighting.windData.w;
        float swayWeight = inColor.r;  // red = sway amplitude

        // Primary sway (slow, large movement)
        float phase = dot(worldPos.xz, vec2(0.1)) + windTime * 1.5;
        vec3 sway = windDir * swayWeight * sin(phase) * 0.5;

        // Secondary sway (faster, smaller — leaf/branch detail)
        float phase2 = dot(worldPos.xz, vec2(0.3, 0.7)) + windTime * 3.0;
        sway += windDir * swayWeight * sin(phase2) * 0.15;

        worldPos.xyz += sway;
    }

    // Water surface wave displacement (gentle Gerstner-lite)
    if ((pushConstants.flags & FLAG_WATER_SURFACE) != 0) {
        float freezeProgress = pushConstants.parallaxScale;
        float waveFactor = 1.0 - freezeProgress; // 1=full waves, 0=frozen still

        float waterTime = lighting.windData.w;
        vec3 windDir = lighting.windData.xyz;
        float windMag = length(windDir) + 0.01;

        // Shore dampening: vertex color G channel encodes edge distance (0=edge, 1=center)
        float edgeDist = inColor.g;
        float waveDampen = 1.0;
        if ((pushConstants.flags & FLAG_WATER_SHORE) != 0) {
            waveDampen = smoothstep(0.0, 0.3, edgeDist);
        }

        // Wave 1: slow, broad swell along wind direction
        float phase1 = dot(worldPos.xz, windDir.xz * 0.3) + waterTime * 1.2;
        float wave1 = sin(phase1) * 0.15 * windMag;

        // Wave 2: faster cross-wave for visual interest
        float phase2 = dot(worldPos.xz, vec2(-windDir.z, windDir.x) * 0.5) + waterTime * 2.0;
        float wave2 = sin(phase2) * 0.08 * windMag;

        // Ocean mode: add larger, slower swell
        float wave3 = 0.0;
        if ((pushConstants.flags & FLAG_WATER_OCEAN) != 0) {
            float phase3 = dot(worldPos.xz, windDir.xz * 0.08) + waterTime * 0.6;
            wave3 = sin(phase3) * 0.35 * windMag;
        }

        worldPos.y += (wave1 + wave2 + wave3) * waveDampen * waveFactor;
    }

    // World curvature: bend geometry downward at distance from camera
    if (lighting.worldCurvature.x > 0.0) {
        vec2 delta = worldPos.xz - lighting.cameraPos.xz;
        worldPos.y -= lighting.worldCurvature.x * dot(delta, delta);
    }

    fragWorldPos = worldPos.xyz;

    // Transform to clip space
    vec4 clipPos = ubo.proj * ubo.view * worldPos;

    // Vertex snapping (PS1-style): snap clip-space XY to a low-res grid
    if ((pushConstants.flags & FLAG_VERTEX_SNAPPING) != 0) {
        // Extract snap resolution from bits 24-28 (5 bits, value*8 = actual resolution)
        int snapRes = (pushConstants.flags >> 24) & 0x1F;
        if (snapRes > 0) {
            float grid = float(snapRes) * 8.0;
            // Snap in clip space (after perspective divide)
            vec2 snapped = clipPos.xy / clipPos.w;           // NDC
            snapped = floor(snapped * grid + 0.5) / grid;    // snap to grid
            clipPos.xy = snapped * clipPos.w;                 // back to clip
        }
    }

    gl_Position = clipPos;

    // Transform normal to world space (using normal matrix)
    mat3 normalMatrix = transpose(inverse(mat3(pushConstants.model)));
    fragNormal = normalize(normalMatrix * skinnedNormal);

    // Pass through UV - for affine texturing, multiply by w to undo perspective correction
    if ((pushConstants.flags & FLAG_AFFINE_TEXTURING) != 0) {
        fragUV = inUV * clipPos.w;
        fragClipW = clipPos.w;
    } else {
        fragUV = inUV;
        fragClipW = 1.0;  // no-op divisor in fragment shader
    }

    // UV quantization (PS1-style fixed-point UV): snap UV to low precision grid
    if ((pushConstants.flags & FLAG_UV_QUANTIZE) != 0) {
        fragUV = floor(fragUV * 128.0) / 128.0;
    }

    // Pass through vertex color
    fragVertColor = inColor;

    // Transform tangent to world space and pass through (w = handedness)
    vec3 worldTangent = normalize(normalMatrix * skinnedTangent);
    fragTangent = vec4(worldTangent, inTangent.w);

    // Calculate view-space depth for cascaded shadow map selection
    fragViewDepth = -(ubo.view * worldPos).z;  // Positive distance from camera
}
