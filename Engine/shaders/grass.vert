#version 450

// GPU-instanced grass blade vertex shader
// Single blade mesh (7 verts, triangle strip topology) instanced thousands of times
// Instance ID -> hash -> procedural (x,z) position within the volume

// Per-vertex blade geometry
layout(location = 0) in vec3 inPosition;   // Blade local position (base at origin, tip at y=1)
layout(location = 1) in vec2 inUV;         // UV (v=0 at base, v=1 at tip)

// Push constants: volume transform + grass parameters
layout(push_constant) uniform PushConstants {
    mat4 model;            // Volume world transform
    vec3 baseColor;        // Grass base color
    float metallic;        // unused
    vec3 emissiveColor;    // Grass tip color (repurposed)
    float roughness;       // unused
    float emissiveStrength; // bladeHeight
    float opacity;         // bladeHeightVariance
    float alphaCutoff;     // bladeWidth
    int flags;             // density (reinterpreted as int)
    float parallaxScale;   // windSwayStrength
    float _pad0;
    float _pad1;
    float _pad2;
} pushConstants;

// View/Projection UBO
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Lighting UBO (for wind data and light direction)
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
    vec4 windData;  // xyz = wind direction * strength, w = time
    vec4 fogParams;     // x=density, y=start, z=end, w=heightFalloff
    vec4 fogColorSnow;  // xyz=fog color, w=snow intensity
    vec4 playerPosition; // xyz = player world pos, w = step radius
    vec4 worldCurvature; // x = strength, yzw reserved
    vec4 skyReflectColor;
    vec4 shProbeIrradiance;
    vec4 reflectionProbePosition;
    vec4 reflectionProbeBoxMin;
    vec4 reflectionProbeBoxMax;
} lighting;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out float fragHeightFraction;

// Simple hash function for procedural placement
float hash(uint n) {
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / float(0x7fffffff);
}

void main() {
    uint instanceID = gl_InstanceIndex;

    // Procedural placement: hash instance ID to get (x, z) offset within volume
    float px = hash(instanceID * 3u + 0u) * 2.0 - 1.0;      // [-1, 1]
    float pz = hash(instanceID * 3u + 1u) * 2.0 - 1.0;      // [-1, 1]
    float heightVar = hash(instanceID * 3u + 2u) * 2.0 - 1.0; // [-1, 1]
    float rotAngle = hash(instanceID * 7u + 5u) * 6.28318;    // Random rotation [0, 2pi]

    // Get volume center and half-extents from model matrix
    // Column-major: model[0]=col0, model[1]=col1, model[2]=col2, model[3]=translation
    vec3 volumeCenter = pushConstants.model[3].xyz;

    // Half extents encoded as column lengths (set in GrassRenderer)
    float halfX = length(pushConstants.model[0].xyz);
    float halfZ = length(pushConstants.model[2].xyz);

    // World-space blade origin
    vec3 bladeOrigin = volumeCenter + vec3(px * halfX, 0.0, pz * halfZ);

    // Blade dimensions
    float bladeHeight = pushConstants.emissiveStrength + heightVar * pushConstants.opacity;
    float bladeWidth = pushConstants.alphaCutoff;

    // Apply random Y-axis rotation to blade
    float cosR = cos(rotAngle);
    float sinR = sin(rotAngle);

    vec3 localPos = inPosition;
    localPos.x *= bladeWidth;
    localPos.y *= bladeHeight;

    // Rotate around Y
    vec3 rotatedPos;
    rotatedPos.x = localPos.x * cosR - localPos.z * sinR;
    rotatedPos.y = localPos.y;
    rotatedPos.z = localPos.x * sinR + localPos.z * cosR;

    // Height fraction for wind and color
    float heightFraction = inUV.y;  // 0 at base, 1 at tip

    // Wind displacement: stronger at tip (heightFraction^2 keeps roots planted)
    vec3 windDir = lighting.windData.xyz;
    float windTime = lighting.windData.w;
    float windSway = pushConstants.parallaxScale; // windSwayStrength

    float windPhase = dot(bladeOrigin.xz, vec2(0.15, 0.25)) + windTime * 2.0;
    float windOffset = sin(windPhase) * heightFraction * heightFraction * windSway;

    // Secondary high-frequency detail
    float windPhase2 = dot(bladeOrigin.xz, vec2(0.4, 0.6)) + windTime * 4.5;
    float windOffset2 = sin(windPhase2) * heightFraction * heightFraction * windSway * 0.3;

    vec3 windDisplacement = windDir * (windOffset + windOffset2);

    vec3 worldPos = bladeOrigin + rotatedPos + windDisplacement;

    // Player stepping: grass bends away from player position
    float stepRadius = lighting.playerPosition.w;
    if (stepRadius > 0.0) {
        vec2 toPlayer = worldPos.xz - lighting.playerPosition.xz;
        float dist = length(toPlayer);
        if (dist < stepRadius && dist > 0.001) {
            float bendFactor = (1.0 - dist / stepRadius) * heightFraction;
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
    // Approximate normal: blade faces camera (billboard-ish) with slight upward bias
    fragNormal = normalize(vec3(sinR, 0.5, cosR));
    fragHeightFraction = heightFraction;
}
