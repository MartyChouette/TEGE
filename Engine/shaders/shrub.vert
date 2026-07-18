#version 450

// GPU-instanced shrub vertex shader
// 3 intersecting quads (star pattern) instanced for each shrub

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor;
    float metallic;
    vec3 emissiveColor;    // tipColor
    float roughness;
    float emissiveStrength; // shrubHeight
    float opacity;         // heightVariance
    float alphaCutoff;     // width
    int flags;             // density
    float parallaxScale;   // windSwayStrength
    float _pad0;
    float _pad1;
    float _pad2;
} pushConstants;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

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
    float texturePageSize;
    vec4 windData;
    vec4 fogParams;
    vec4 fogColorSnow;
    vec4 playerPosition; // xyz = player world pos, w = step radius
    vec4 worldCurvature; // x = strength, yzw reserved
    vec4 skyReflectColor;
    vec4 shProbeIrradiance;
    vec4 reflectionProbePosition;
    vec4 reflectionProbeBoxMin;
    vec4 reflectionProbeBoxMax;
    vec4 fogScreenParams;         // xy = screen size px (froxel UV), z = camera near, w = reserved
    vec4 ddgiGridOrigin;         // xyz = probe grid origin, w = spacing
    ivec4 ddgiProbeCounts;       // xyz = probes per axis, w = oct resolution
    vec4 ddgiAtlasParams;        // x = atlas W, y = atlas H, z = enabled, w = intensity
} lighting;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out float fragHeightFraction;

float hash(uint n) {
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / float(0x7fffffff);
}

void main() {
    uint instanceID = gl_InstanceIndex;

    // Procedural placement within volume
    float px = hash(instanceID * 3u + 0u) * 2.0 - 1.0;
    float pz = hash(instanceID * 3u + 1u) * 2.0 - 1.0;
    float heightVar = hash(instanceID * 3u + 2u) * 2.0 - 1.0;
    float rotAngle = hash(instanceID * 7u + 5u) * 6.28318;
    // Per-instance random darkening for depth variety
    float darkening = hash(instanceID * 11u + 3u) * 0.15;

    vec3 volumeCenter = pushConstants.model[3].xyz;
    float halfX = length(pushConstants.model[0].xyz);
    float halfZ = length(pushConstants.model[2].xyz);

    vec3 shrubOrigin = volumeCenter + vec3(px * halfX, 0.0, pz * halfZ);

    float shrubHeight = pushConstants.emissiveStrength + heightVar * pushConstants.opacity;
    float shrubWidth = pushConstants.alphaCutoff;

    // Apply random Y-axis rotation
    float cosR = cos(rotAngle);
    float sinR = sin(rotAngle);

    vec3 localPos = inPosition;
    localPos.x *= shrubWidth;
    localPos.y *= shrubHeight;

    // Rotate around Y
    vec3 rotatedPos;
    rotatedPos.x = localPos.x * cosR - localPos.z * sinR;
    rotatedPos.y = localPos.y;
    rotatedPos.z = localPos.x * sinR + localPos.z * cosR;

    float heightFraction = inUV.y;

    // Wind: bushier sway (height^1.5 keeps base more planted than grass)
    vec3 windDir = lighting.windData.xyz;
    float windTime = lighting.windData.w;
    float windSway = pushConstants.parallaxScale;

    float windPhase = dot(shrubOrigin.xz, vec2(0.12, 0.2)) + windTime * 1.5;
    float windPower = pow(heightFraction, 1.5);
    float windOffset = sin(windPhase) * windPower * windSway;

    float windPhase2 = dot(shrubOrigin.xz, vec2(0.35, 0.5)) + windTime * 3.5;
    float windOffset2 = sin(windPhase2) * windPower * windSway * 0.25;

    vec3 windDisplacement = windDir * (windOffset + windOffset2);

    vec3 worldPos = shrubOrigin + rotatedPos + windDisplacement;

    // Player stepping: shrub bends away from player position
    float stepRadius = lighting.playerPosition.w;
    if (stepRadius > 0.0) {
        vec2 toPlayer = worldPos.xz - lighting.playerPosition.xz;
        float dist = length(toPlayer);
        if (dist < stepRadius && dist > 0.001) {
            float bendFactor = (1.0 - dist / stepRadius) * pow(heightFraction, 1.5);
            worldPos.xz += normalize(toPlayer) * bendFactor * 0.5;
            worldPos.y -= bendFactor * 0.2;
        }
    }

    // World curvature: bend geometry downward at distance from camera
    if (lighting.worldCurvature.x > 0.0) {
        vec2 delta = worldPos.xz - lighting.cameraPos.xz;
        worldPos.y -= lighting.worldCurvature.x * dot(delta, delta);
    }

    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);

    fragWorldPos = worldPos;
    fragNormal = normalize(vec3(sinR, 0.6, cosR));
    fragHeightFraction = heightFraction - darkening;  // Encode darkening in height
}
