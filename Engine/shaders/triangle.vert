#version 450

// Lit Mesh Vertex Shader with Cascaded Shadow Support, Retro Effects, and Skeletal Animation

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inTangent;
layout(location = 5) in vec4 inBoneWeights;
layout(location = 6) in uvec4 inBoneIndices;
layout(location = 7) in vec2 inUV1;           // second UV channel (lightmap/detail)
layout(location = 8) in vec4 inBoneWeights2;  // bone influences 5-8
layout(location = 9) in uvec4 inBoneIndices2;

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
    mat4 prevViewProj;   // Previous frame's view*proj for velocity computation
    vec4 jitterOffset;   // xy = current jitter (NDC), zw = previous jitter
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
    float celDiffuseBands;
    float celSpecularCutoff;
    uint shadingFlags;
    float sphereEnvStrength;
    float posterizeLevels;
    float texturePageSize;  // PS1 VRAM page size in texels (0=off)
    vec4 windData;  // xyz = wind direction * strength, w = time
    vec4 fogParams;     // x=density, y=start, z=end, w=heightFalloff
    vec4 fogColorSnow;  // xyz=fog color, w=snow intensity
    vec4 playerPosition; // xyz = player world pos, w = step radius
    vec4 worldCurvature; // x = strength, yzw reserved
    vec4 skyReflectColor; // xyz = sky reflection color, w = reserved
    vec4 shProbeIrradiance; // xyz = SH probe irradiance, w = blend weight
    vec4 reflectionProbePosition;
    vec4 reflectionProbeBoxMin;
    vec4 reflectionProbeBoxMax;
    vec4 fogScreenParams;         // xy = screen size px (froxel UV), z = camera near, w = reserved
    vec4 ddgiGridOrigin;         // xyz = probe grid origin, w = spacing
    ivec4 ddgiProbeCounts;       // xyz = probes per axis, w = oct resolution
    vec4 ddgiAtlasParams;        // x = atlas W, y = atlas H, z = enabled, w = intensity
    // Note: light arrays follow but we only need windData in vertex shader
} lighting;

// Bone matrix SSBO for skeletal animation
layout(std430, binding = 7) readonly buffer BoneMatrixSSBO {
    mat4 boneMatrices[];
};

// Morph target SSBO (binding 20)
// Layout: [vertexCount as uint bits, targetCount as uint bits, weights[targetCount], deltas[targetCount * vertexCount * 6]]
// Deltas: [px, py, pz, nx, ny, nz] per vertex, repeated per target
layout(std430, binding = 20) readonly buffer MorphTargetSSBO {
    float morphData[];
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
    uint teleported;       // 1 = network snap/spawn (zero velocity), 0 = normal
    float _objPad[2];
    mat4 prevModel;        // Previous frame model matrix for velocity
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
layout(location = 7) out vec4 fragCurClipPos;  // Current clip position (for velocity)
layout(location = 8) out vec4 fragPrevClipPos; // Previous frame clip position (for velocity)
layout(location = 9) flat out int v_ObjectIndex; // >=0: indirect draw (SSBO index), -1: per-entity
layout(location = 10) out vec2 fragUV1;          // second UV channel (passthrough; ready for detail/lightmap)
layout(location = 11) flat out int v_MaterialIndex; // material SSBO index (direct draws set it via firstInstance)

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
    // Multi-draw indirect mode: when parallaxScale == -1.0, per-object data comes from
    // ObjectData SSBO (binding 13) indexed by gl_InstanceIndex (set via firstInstance).
    // Otherwise, per-entity push constants are used (traditional path).
    bool indirectMode = (pushConstants.parallaxScale == -1.0);
    v_ObjectIndex = indirectMode ? gl_InstanceIndex : -1;
    // Direct draws encode the material SSBO index in firstInstance (adr-0003);
    // in indirect mode gl_InstanceIndex is the OBJECT index and the fragment
    // shader ignores this varying (falls back to material entry 0).
    v_MaterialIndex = gl_InstanceIndex;

    // Select data source: ObjectData SSBO for indirect draws, push constants otherwise
    mat4 objModel;
    int  objFlags;
    float objParallaxScale;
    mat4 objPrevModel;
    if (indirectMode) {
        ObjectData od = objectData[gl_InstanceIndex];
        objModel = od.model;
        objFlags = od.flags;
        objParallaxScale = od.parallaxScale;
        objPrevModel = od.prevModel;
    } else {
        objModel = pushConstants.model;
        objFlags = pushConstants.flags;
        objParallaxScale = pushConstants.parallaxScale;
        objPrevModel = pushConstants.model; // fallback: use current model
    }

    vec3 skinnedPos = inPosition;
    vec3 skinnedNormal = inNormal;
    vec3 skinnedTangent = inTangent.xyz;

    // Apply morph targets (blend shapes) before skeletal skinning
    // Morph targets modify the bind-pose mesh, then skinning transforms the result
    {
        uint mVertCount = floatBitsToUint(morphData[0]);
        uint mTargetCount = floatBitsToUint(morphData[1]);
        if (mTargetCount > 0u && uint(gl_VertexIndex) < mVertCount) {
            uint headerSize = 2u + mTargetCount;
            for (uint t = 0u; t < mTargetCount && t < 64u; t++) {
                float w = morphData[2u + t];
                if (abs(w) < 0.001) continue;
                uint base = headerSize + t * mVertCount * 6u + uint(gl_VertexIndex) * 6u;
                skinnedPos.x += morphData[base + 0u] * w;
                skinnedPos.y += morphData[base + 1u] * w;
                skinnedPos.z += morphData[base + 2u] * w;
                skinnedNormal.x += morphData[base + 3u] * w;
                skinnedNormal.y += morphData[base + 4u] * w;
                skinnedNormal.z += morphData[base + 5u] * w;
            }
            skinnedNormal = normalize(skinnedNormal);
        }
    }

    // Apply skeletal skinning if enabled and vertex has bone weights.
    // Up to 8 influences: the first 4 (inBoneWeights/Indices) plus 5-8 (inBoneWeights2/Indices2).
    if ((objFlags & FLAG_SKINNED) != 0) {
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
            skinnedTangent = mat3(skinMatrix) * inTangent.xyz;
        }
    }

    // Transform vertex position to world space
    vec4 worldPos = objModel * vec4(skinnedPos, 1.0);

    // Wind sway for vegetation (trees, bushes)
    // Uses vertex color red channel as sway weight (trunk=0, leaves=1)
    if ((objFlags & FLAG_WIND_SWAY) != 0) {
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
    if ((objFlags & FLAG_WATER_SURFACE) != 0) {
        float freezeProgress = objParallaxScale;
        float waveFactor = 1.0 - freezeProgress; // 1=full waves, 0=frozen still

        float waterTime = lighting.windData.w;
        vec3 windDir = lighting.windData.xyz;
        float windMag = length(windDir) + 0.01;

        // Shore dampening: vertex color G channel encodes edge distance (0=edge, 1=center)
        float edgeDist = inColor.g;
        float waveDampen = 1.0;
        if ((objFlags & FLAG_WATER_SHORE) != 0) {
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
        if ((objFlags & FLAG_WATER_OCEAN) != 0) {
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
    if ((objFlags & FLAG_VERTEX_SNAPPING) != 0) {
        // Extract snap resolution from bits 24-28 (5 bits, value*8 = actual resolution)
        int snapRes = (objFlags >> 24) & 0x1F;
        if (snapRes > 0) {
            float grid = float(snapRes) * 8.0;
            // Snap in clip space (after perspective divide)
            vec2 snapped = clipPos.xy / clipPos.w;           // NDC
            snapped = floor(snapped * grid + 0.5) / grid;    // snap to grid
            clipPos.xy = snapped * clipPos.w;                 // back to clip
        }
    }

    // PS1-style polygon sort jitter: add depth noise to simulate ordering table errors
    float depthJitter = lighting.worldCurvature.y; // packed in reserved field
    if (depthJitter > 0.0 && (objFlags & FLAG_VERTEX_SNAPPING) != 0) {
        // Hash based on world position to get stable per-triangle jitter
        float h = fract(sin(dot(worldPos.xyz, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
        clipPos.z += (h - 0.5) * 2.0 * depthJitter * clipPos.w;
    }

    gl_Position = clipPos;

    // Transform normal to world space (using normal matrix)
    mat3 normalMatrix = transpose(inverse(mat3(objModel)));
    fragNormal = normalize(normalMatrix * skinnedNormal);

    // Pass through UV - for affine texturing, multiply by w to undo perspective correction
    if ((objFlags & FLAG_AFFINE_TEXTURING) != 0) {
        fragUV = inUV * clipPos.w;
        fragClipW = clipPos.w;
    } else {
        fragUV = inUV;
        fragClipW = 1.0;  // no-op divisor in fragment shader
    }

    // UV quantization (PS1-style fixed-point UV): snap UV to low precision grid
    if ((objFlags & FLAG_UV_QUANTIZE) != 0) {
        fragUV = floor(fragUV * 128.0) / 128.0;
    }

    // Second UV channel passthrough (no consumer yet; available for detail/lightmap).
    fragUV1 = inUV1;

    // Pass through vertex color
    fragVertColor = inColor;

    // Transform tangent to world space and pass through (w = handedness)
    vec3 worldTangent = normalize(normalMatrix * skinnedTangent);
    fragTangent = vec4(worldTangent, inTangent.w);

    // Calculate view-space depth for cascaded shadow map selection
    fragViewDepth = -(ubo.view * worldPos).z;  // Positive distance from camera

    // Per-pixel velocity: pass current and previous clip positions to fragment shader.
    // Previous position uses current skinned position with previous frame's matrices.
    // This captures camera motion, object motion, and approximates skeletal motion.
    fragCurClipPos = clipPos;
    // Use previous-frame model matrix from ObjectData SSBO when available (indirect draws
    // and per-entity draws that have been uploaded). Falls back to current model for entities
    // without SSBO data (teleported = 1 zeroes velocity in that case anyway).
    vec4 prevWorldPos = objPrevModel * vec4(skinnedPos, 1.0);
    fragPrevClipPos = ubo.prevViewProj * prevWorldPos;
}
