#version 450

// GPU-instanced tree vertex shader
// Trunk (2 tapered quads) + canopy (3 intersecting quads)
// UV.y < 0.5 = trunk, >= 0.5 = canopy

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 trunkColor;       // baseColor
    float trunkWidth;      // metallic
    vec3 canopyTipColor;   // emissiveColor
    float canopyRadius;    // roughness
    float trunkHeight;     // emissiveStrength
    float canopyOffset;    // opacity
    float canopyBaseR;     // alphaCutoff (canopyBaseColor.r)
    int flags;             // density
    float windSwayStrength; // parallaxScale
    float canopyBaseG;     // shoreWidth (canopyBaseColor.g)
    float canopyBaseB;     // foamIntensity (canopyBaseColor.b)
    float seasonFactor;    // foamScale -> 0=winter/bare, 1=full canopy
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
    float _padShading1;
    vec4 windData;
    vec4 fogParams;
    vec4 fogColorSnow;
    vec4 playerPosition; // xyz = player world pos, w = step radius
    vec4 worldCurvature; // x = strength, yzw reserved
    vec4 skyReflectColor;
    vec4 shProbeIrradiance;
} lighting;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out float fragUVy;

float hash(uint n) {
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / float(0x7fffffff);
}

void main() {
    uint instanceID = gl_InstanceIndex;

    float px = hash(instanceID * 3u + 0u) * 2.0 - 1.0;
    float pz = hash(instanceID * 3u + 1u) * 2.0 - 1.0;
    float sizeVar = hash(instanceID * 3u + 2u) * 0.8 + 0.6;  // 0.6 - 1.4 size multiplier (wider variance)
    float rotAngle = hash(instanceID * 7u + 5u) * 6.28318;

    vec3 volumeCenter = pushConstants.model[3].xyz;
    float halfX = length(pushConstants.model[0].xyz);
    float halfZ = length(pushConstants.model[2].xyz);

    vec3 treeOrigin = volumeCenter + vec3(px * halfX, 0.0, pz * halfZ);

    float tHeight = pushConstants.trunkHeight * sizeVar;
    float tWidth = pushConstants.trunkWidth * sizeVar;
    float cRadius = pushConstants.canopyRadius * sizeVar;
    float cOffset = pushConstants.canopyOffset * sizeVar;

    float cosR = cos(rotAngle);
    float sinR = sin(rotAngle);

    vec3 localPos = inPosition;
    bool isCanopy = inUV.y >= 0.5;

    // Season-driven canopy scale (0 = bare branches, 1 = full canopy)
    float canopyScale = pushConstants.seasonFactor;

    if (isCanopy) {
        // Scale canopy quads by canopy radius and seasonal canopy scale
        localPos.x *= cRadius * 2.0 * canopyScale;
        localPos.z *= cRadius * 2.0 * canopyScale;
        localPos.y = (localPos.y - 0.5) * cRadius * 2.0 * canopyScale + cOffset;
    } else {
        // Scale trunk by width and height
        localPos.x *= tWidth * 2.0;
        localPos.z *= tWidth * 2.0;
        localPos.y *= tHeight;
    }

    // Rotate around Y
    vec3 rotatedPos;
    rotatedPos.x = localPos.x * cosR - localPos.z * sinR;
    rotatedPos.y = localPos.y;
    rotatedPos.z = localPos.x * sinR + localPos.z * cosR;

    // Wind: trunk bends with smooth quadratic curve, canopy sways
    vec3 windDir = lighting.windData.xyz;
    float windTime = lighting.windData.w;
    float windSway = pushConstants.windSwayStrength;

    float windPhase = dot(treeOrigin.xz, vec2(0.08, 0.15)) + windTime * 1.0;
    float windAngle = sin(windPhase) * windSway;

    vec3 windDisplacement;
    if (isCanopy) {
        // Canopy sways with full wind
        windDisplacement = windDir * windAngle;
    } else {
        // Trunk: height-based quadratic bend (smooth curve, not rigid sway)
        float trunkHeight = localPos.y / max(tHeight, 0.01);
        float bendAmount = trunkHeight * trunkHeight * windAngle * 0.5;
        windDisplacement = windDir * bendAmount;
    }

    vec3 worldPos = treeOrigin + rotatedPos + windDisplacement;

    // World curvature: bend geometry downward at distance from camera
    if (lighting.worldCurvature.x > 0.0) {
        vec2 delta = worldPos.xz - lighting.cameraPos.xz;
        worldPos.y -= lighting.worldCurvature.x * dot(delta, delta);
    }

    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);

    fragWorldPos = worldPos;
    fragNormal = normalize(vec3(sinR, isCanopy ? 0.7 : 0.3, cosR));
    fragUVy = inUV.y;
}
