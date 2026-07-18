#version 450

// Shrub fragment shader - base-to-tip color lerp with Lambert diffuse

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in float fragHeightFraction;

layout(location = 0) out vec4 outColor;

#define MAX_DIRECTIONAL_LIGHTS 4
#define MAX_POINT_LIGHTS 64
#define MAX_SPOT_LIGHTS 32

struct DirectionalLight {
    vec3 direction;
    float _pad0;
    vec3 color;
    float intensity;
};

struct PointLight {
    vec3 position;
    float range;
    vec3 color;
    float intensity;
    float constantAtten;
    float linearAtten;
    float quadraticAtten;
    float _pad0;
};

struct SpotLight {
    vec3 position;
    float range;
    vec3 direction;
    float intensity;
    vec3 color;
    float innerCutoff;
    float outerCutoff;
    float constantAtten;
    float linearAtten;
    float quadraticAtten;
};

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
    vec4 playerPosition;
    vec4 worldCurvature;
    vec4 skyReflectColor;
    vec4 shProbeIrradiance;
    vec4 reflectionProbePosition;
    vec4 reflectionProbeBoxMin;
    vec4 reflectionProbeBoxMax;
    vec4 fogScreenParams;         // xy = screen size px (froxel UV), z = camera near, w = reserved
    vec4 ddgiGridOrigin;         // xyz = probe grid origin, w = spacing
    ivec4 ddgiProbeCounts;       // xyz = probes per axis, w = oct resolution
    vec4 ddgiAtlasParams;        // x = atlas W, y = atlas H, z = enabled, w = intensity
    DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
} lighting;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor;
    float metallic;
    vec3 tipColor;
    float roughness;
    float emissiveStrength;
    float opacity;
    float alphaCutoff;
    int flags;
    float parallaxScale;
    float _pad0;
    float _pad1;
    float _pad2;
} material;

void main() {
    // Extract height and darkening from fragHeightFraction
    float height = clamp(fragHeightFraction, 0.0, 1.0);

    // Snow blending
    float snowIntensity = lighting.fogColorSnow.w;
    vec3 tipColor = material.tipColor;
    if (snowIntensity > 0.0) {
        tipColor = mix(tipColor, vec3(0.93, 0.95, 0.98), snowIntensity);
    }

    vec3 albedo = mix(material.baseColor, tipColor, height);

    // Per-instance darkening for depth variety
    float darkAmount = max(-fragHeightFraction, 0.0) * 2.0;
    albedo *= (1.0 - darkAmount);

    if (snowIntensity > 0.5) {
        float baseCoverage = (snowIntensity - 0.5) * 2.0;
        albedo = mix(albedo, vec3(0.88, 0.9, 0.93), baseCoverage * 0.4);
    }

    vec3 normal = normalize(fragNormal);

    // Ambient
    vec3 result = lighting.ambientColor * lighting.ambientIntensity * albedo;

    // Lambert diffuse from directional lights
    for (uint i = 0u; i < lighting.directionalLightCount && i < MAX_DIRECTIONAL_LIGHTS; ++i) {
        vec3 lightDir = -normalize(lighting.directionalLights[i].direction);
        float diff = max(dot(normal, lightDir), 0.0);
        diff = diff * 0.5 + 0.5;  // Wrap lighting
        result += diff * lighting.directionalLights[i].color * lighting.directionalLights[i].intensity * albedo;
    }

    if (lighting.directionalLightCount == 0u) {
        vec3 defaultLightDir = normalize(vec3(0.5, 0.8, 0.3));
        float diff = max(dot(normal, defaultLightDir), 0.0) * 0.5 + 0.5;
        result += diff * vec3(1.0, 0.95, 0.9) * 1.2 * albedo;
    }

    result = pow(result, vec3(1.0 / 2.2));

    // Fog
    float fogDensity = lighting.fogParams.x;
    if (fogDensity > 0.0) {
        float fogStart = lighting.fogParams.y;
        float fogEnd = lighting.fogParams.z;
        float fogHeightFalloff = lighting.fogParams.w;

        float dist = length(lighting.cameraPos - fragWorldPos);
        float fogRange = max(fogEnd - fogStart, 0.001);
        float fogFactor = clamp((dist - fogStart) / fogRange, 0.0, 1.0);
        fogFactor *= fogDensity;

        float heightFog = exp(-max(fragWorldPos.y, 0.0) * fogHeightFalloff);
        fogFactor *= heightFog;

        vec3 fogColor = lighting.fogColorSnow.xyz;
        result = mix(result, fogColor, fogFactor);
    }

    outColor = vec4(result, 1.0);
}
