#version 450

// Lambert-only grass fragment shader
// Simple diffuse lighting, no specular/shadows (grass too small for shadow artifacts)

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
    float _padShading1;
    vec4 windData;
    vec4 fogParams;     // x=density, y=start, z=end, w=heightFalloff
    vec4 fogColorSnow;  // xyz=fog color, w=snow intensity
    vec4 playerPosition;
    vec4 worldCurvature;
    vec4 skyReflectColor;
    vec4 shProbeIrradiance;
    DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
} lighting;

// Base and tip colors via push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor;
    float metallic;
    vec3 tipColor;     // emissiveColor repurposed
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
    // Snow: blend tip color toward white based on snow intensity
    float snowIntensity = lighting.fogColorSnow.w;
    vec3 tipColor = material.tipColor;
    if (snowIntensity > 0.0) {
        tipColor = mix(tipColor, vec3(0.95, 0.97, 1.0), snowIntensity);
    }

    // Lerp between base and (possibly snowy) tip color
    vec3 albedo = mix(material.baseColor, tipColor, fragHeightFraction);

    // Also whiten the base color slightly when snow is heavy
    if (snowIntensity > 0.5) {
        float baseCoverage = (snowIntensity - 0.5) * 2.0; // 0..1 when intensity is 0.5..1.0
        albedo = mix(albedo, vec3(0.9, 0.92, 0.95), baseCoverage * 0.3);
    }

    vec3 normal = normalize(fragNormal);

    // Ambient
    vec3 result = lighting.ambientColor * lighting.ambientIntensity * albedo;

    // Lambert diffuse from directional lights only (grass is small, skip point/spot)
    for (uint i = 0u; i < lighting.directionalLightCount && i < MAX_DIRECTIONAL_LIGHTS; ++i) {
        vec3 lightDir = -normalize(lighting.directionalLights[i].direction);
        float diff = max(dot(normal, lightDir), 0.0);
        // Wrap lighting for softer grass look: remap [-1,1] to [0.3, 1]
        diff = diff * 0.5 + 0.5;
        result += diff * lighting.directionalLights[i].color * lighting.directionalLights[i].intensity * albedo;
    }

    // Default light if no directional lights
    if (lighting.directionalLightCount == 0u) {
        vec3 defaultLightDir = normalize(vec3(0.5, 0.8, 0.3));
        float diff = max(dot(normal, defaultLightDir), 0.0) * 0.5 + 0.5;
        result += diff * vec3(1.0, 0.95, 0.9) * 1.2 * albedo;
    }

    // Gamma correction
    result = pow(result, vec3(1.0 / 2.2));

    // Height-based distance fog
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
