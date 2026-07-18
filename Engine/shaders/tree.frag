#version 450

// Tree fragment shader - dual coloring: trunk vs canopy

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in float fragUVy;

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
    vec3 trunkColor;
    float trunkWidth;
    vec3 canopyTipColor;
    float canopyRadius;
    float trunkHeight;
    float canopyOffset;
    float canopyBaseR;
    int flags;
    float windSwayStrength;
    float canopyBaseG;
    float canopyBaseB;
    float seasonFactor;  // 0=bare, 1=full canopy
} material;

void main() {
    bool isCanopy = fragUVy >= 0.5;

    // Discard canopy fragments when season has stripped the leaves
    if (isCanopy && material.seasonFactor < 0.05) {
        discard;
    }

    vec3 albedo;
    if (isCanopy) {
        // Canopy: lerp base -> tip based on UV.y (0.5 = base, 1.0 = tip)
        float canopyHeight = (fragUVy - 0.5) * 2.0;  // Remap to 0..1
        vec3 canopyBaseColor = vec3(material.canopyBaseR, material.canopyBaseG, material.canopyBaseB);
        albedo = mix(canopyBaseColor, material.canopyTipColor, canopyHeight);

        // Snow on canopy
        float snowIntensity = lighting.fogColorSnow.w;
        if (snowIntensity > 0.0) {
            albedo = mix(albedo, vec3(0.92, 0.95, 0.98), snowIntensity * 0.7);
        }
    } else {
        // Trunk: solid trunk color
        albedo = material.trunkColor;
    }

    vec3 normal = normalize(fragNormal);

    // Ambient
    vec3 result = lighting.ambientColor * lighting.ambientIntensity * albedo;

    // Lambert diffuse
    for (uint i = 0u; i < lighting.directionalLightCount && i < MAX_DIRECTIONAL_LIGHTS; ++i) {
        vec3 lightDir = -normalize(lighting.directionalLights[i].direction);
        float diff = max(dot(normal, lightDir), 0.0);
        diff = diff * 0.5 + 0.5;
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
