#version 450

// Lit 2D sprite fragment shader — full Blinn-Phong lighting from LightingUBO
// Supports optional normal mapping for per-pixel detail on sprites

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragTintAlpha;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

// Light limits (must match C++ constants and triangle.frag)
#define MAX_DIRECTIONAL_LIGHTS 4
#define MAX_POINT_LIGHTS 64
#define MAX_SPOT_LIGHTS 32

// Directional light data
struct DirectionalLight {
    vec3 direction;
    float _pad0;
    vec3 color;
    float intensity;
};

// Point light data
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

// Spot light data
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

// Lighting UBO — must match C++ LightingUBO structure exactly
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
    float shadowBias;
    int shadowEnabled;
    float shadowStrength;
    float shadowMaxDistance;
    vec4 windData;
    vec4 fogParams;
    vec4 fogColorSnow;
    vec4 playerPosition;
    vec4 worldCurvature;
    vec4 skyReflectColor;
    DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
} lighting;

// Base color texture sampler (same binding as main pipeline)
layout(binding = 3) uniform sampler2D baseColorTexture;

// Normal map sampler (same binding as main pipeline)
layout(binding = 6) uniform sampler2D normalMap;

// Push constants — same layout as main pipeline
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
    float _pad0;
    float _pad1;
    float _pad2;
} material;

// Normal map flag (bit 17, same as triangle.frag FLAG_HAS_NORMAL_TEX)
#define FLAG_HAS_NORMAL_TEX (1 << 17)

// Blinn-Phong lighting calculation (simplified from triangle.frag)
vec3 calcBlinnPhong(vec3 lightDir, vec3 lightColor, float lightIntensity, vec3 normal, vec3 viewDir, vec3 albedo, float shininess) {
    // Diffuse (Lambert)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * lightIntensity;

    // Specular (Blinn-Phong) — subtle for sprites
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    vec3 specular = spec * vec3(0.04) * lightColor * lightIntensity;

    return diffuse * albedo + specular;
}

// Attenuation for point/spot lights
float calcAttenuation(float distance, float constant, float linear, float quadratic) {
    return 1.0 / (constant + linear * distance + quadratic * distance * distance);
}

void main() {
    // Sample base color texture
    vec4 texColor = texture(baseColorTexture, fragUV);

    vec3 albedo = texColor.rgb * fragTintAlpha.rgb;
    float alpha = texColor.a * fragTintAlpha.a;

    // Alpha discard
    if (alpha < 0.01) {
        discard;
    }

    // Determine surface normal
    vec3 normal;
    if ((material.flags & FLAG_HAS_NORMAL_TEX) != 0) {
        // Sample normal map and transform via TBN matrix
        vec3 N = normalize(fragNormal);
        vec3 T = normalize(fragTangent.xyz);
        T = normalize(T - dot(T, N) * N); // Re-orthogonalize
        vec3 B = cross(N, T) * fragTangent.w;
        mat3 TBN = mat3(T, B, N);

        // Normal map stored as [0,1], remap to [-1,1]
        vec3 sampledNormal = texture(normalMap, fragUV).rgb * 2.0 - 1.0;
        normal = normalize(TBN * sampledNormal);
    } else {
        // Dome normal: sprite plane normal (gives basic directional light response)
        normal = normalize(fragNormal);
    }

    vec3 viewDir = normalize(lighting.cameraPos - fragWorldPos);

    // Shininess for sprites — moderate specular response
    float shininess = 32.0;

    // Start with ambient
    vec3 result = lighting.ambientColor * lighting.ambientIntensity * albedo;

    // Process directional lights (no shadows for sprites)
    for (uint i = 0u; i < lighting.directionalLightCount && i < MAX_DIRECTIONAL_LIGHTS; ++i) {
        vec3 lightDir = -normalize(lighting.directionalLights[i].direction);
        vec3 lightColor = lighting.directionalLights[i].color;
        float intensity = lighting.directionalLights[i].intensity;

        result += calcBlinnPhong(lightDir, lightColor, intensity, normal, viewDir, albedo, shininess);
    }

    // Process point lights
    for (uint i = 0u; i < lighting.pointLightCount && i < MAX_POINT_LIGHTS; ++i) {
        vec3 lightPos = lighting.pointLights[i].position;
        vec3 lightVec = lightPos - fragWorldPos;
        float distance = length(lightVec);

        if (distance > lighting.pointLights[i].range) continue;

        vec3 lightDir = normalize(lightVec);
        vec3 lightColor = lighting.pointLights[i].color;
        float intensity = lighting.pointLights[i].intensity;

        float atten = calcAttenuation(distance,
            lighting.pointLights[i].constantAtten,
            lighting.pointLights[i].linearAtten,
            lighting.pointLights[i].quadraticAtten);

        result += calcBlinnPhong(lightDir, lightColor, intensity * atten, normal, viewDir, albedo, shininess);
    }

    // Process spot lights
    for (uint i = 0u; i < lighting.spotLightCount && i < MAX_SPOT_LIGHTS; ++i) {
        vec3 lightPos = lighting.spotLights[i].position;
        vec3 lightVec = lightPos - fragWorldPos;
        float distance = length(lightVec);

        if (distance > lighting.spotLights[i].range) continue;

        vec3 lightDir = normalize(lightVec);
        vec3 spotDir = normalize(lighting.spotLights[i].direction);

        float theta = dot(lightDir, -spotDir);
        float innerCutoff = lighting.spotLights[i].innerCutoff;
        float outerCutoff = lighting.spotLights[i].outerCutoff;
        float epsilon = innerCutoff - outerCutoff;
        float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);

        if (spotIntensity > 0.0) {
            vec3 lightColor = lighting.spotLights[i].color;
            float intensity = lighting.spotLights[i].intensity;

            float atten = calcAttenuation(distance,
                lighting.spotLights[i].constantAtten,
                lighting.spotLights[i].linearAtten,
                lighting.spotLights[i].quadraticAtten);

            result += calcBlinnPhong(lightDir, lightColor, intensity * atten * spotIntensity, normal, viewDir, albedo, shininess);
        }
    }

    // Gamma correction
    result = pow(result, vec3(1.0 / 2.2));

    // Height-based distance fog (same as main pipeline)
    float fogDensity = lighting.fogParams.x;
    if (fogDensity > 0.0) {
        float fogStart = lighting.fogParams.y;
        float fogEnd = lighting.fogParams.z;
        float fogHeightFalloff = lighting.fogParams.w;

        float dist = length(lighting.cameraPos - fragWorldPos);
        float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
        fogFactor *= fogDensity;

        float heightFog = exp(-max(fragWorldPos.y, 0.0) * fogHeightFalloff);
        fogFactor *= heightFog;

        vec3 fogColor = lighting.fogColorSnow.xyz;
        result = mix(result, fogColor, fogFactor);
    }

    outColor = vec4(result, alpha);
}
