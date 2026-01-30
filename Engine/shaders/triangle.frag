#version 450

// Lit Mesh Fragment Shader with Multi-light, Shadow, and Retro Effect Support

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec4 fragPosLightSpace;
layout(location = 4) in vec4 fragVertColor;
layout(location = 5) in float fragClipW;

layout(location = 0) out vec4 outColor;

// Light limits (must match C++ constants)
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

// Lighting UBO - must match C++ LightingUBO structure
layout(binding = 1) uniform LightingUBO {
    vec3 ambientColor;
    float ambientIntensity;
    vec3 cameraPos;
    float _pad0;
    uint directionalLightCount;
    uint pointLightCount;
    uint spotLightCount;
    uint _pad1;
    mat4 lightSpaceMatrix;
    float shadowBias;
    int shadowEnabled;
    vec2 _shadowPad;
    DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
} lighting;

// Material via push constants (per-object data)
layout(push_constant) uniform PushConstants {
    mat4 model;  // Used by vertex shader
    vec3 baseColor;
    float metallic;
    vec3 emissiveColor;
    float roughness;
    float emissiveStrength;
    float opacity;
    float alphaCutoff;
    int flags;  // bits 0-7: render flags, 8-9: alpha mode, 16-19: texture flags, 20-31: retro
} material;

// Material flag bits
#define FLAG_DOUBLE_SIDED       (1 << 0)
#define FLAG_CAST_SHADOWS       (1 << 1)
#define FLAG_RECEIVE_SHADOWS    (1 << 2)
#define FLAG_HAS_BASE_COLOR_TEX (1 << 16)
#define FLAG_HAS_NORMAL_TEX     (1 << 17)
#define FLAG_HAS_METALLIC_TEX   (1 << 18)
#define FLAG_HAS_EMISSIVE_TEX   (1 << 19)

// Retro flag bits (must match vertex shader and C++ Material.h)
#define FLAG_FLAT_SHADING       (1 << 20)
#define FLAG_AFFINE_TEXTURING   (1 << 21)
#define FLAG_VERTEX_SNAPPING    (1 << 22)
#define FLAG_STIPPLE_TRANS      (1 << 23)

// Base color texture sampler (binding 3)
layout(binding = 3) uniform sampler2D baseColorTexture;

// Shadow map sampler (binding 4)
layout(binding = 4) uniform sampler2DShadow shadowMap;

// Calculate shadow factor using PCF (Percentage Closer Filtering)
float calcShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Check if shadows are enabled
    if (lighting.shadowEnabled == 0) {
        return 1.0;
    }

    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform from [-1,1] to [0,1] range for texture sampling
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Check if outside shadow map bounds
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0;
    }

    // Apply bias to reduce shadow acne
    float bias = max(lighting.shadowBias * (1.0 - dot(normal, lightDir)), lighting.shadowBias * 0.1);
    float currentDepth = projCoords.z - bias;

    // PCF (3x3 kernel)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            // sampler2DShadow returns 0.0 or 1.0 based on comparison
            shadow += texture(shadowMap, vec3(projCoords.xy + offset, currentDepth));
        }
    }
    shadow /= 9.0;

    return shadow;
}

// Calculate Blinn-Phong lighting contribution
vec3 calcBlinnPhong(vec3 lightDir, vec3 lightColor, float lightIntensity, vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float shininess) {
    // Diffuse (Lambert)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * lightIntensity;

    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    vec3 specularColor = mix(vec3(0.04), albedo, metallic);
    vec3 specular = spec * specularColor * lightColor * lightIntensity;

    return diffuse * albedo * (1.0 - metallic) + specular;
}

// Calculate attenuation for point/spot lights
float calcAttenuation(float distance, float constant, float linear, float quadratic) {
    return 1.0 / (constant + linear * distance + quadratic * distance * distance);
}

// 4x4 Bayer dither matrix for stipple transparency
float bayerDither4x4(ivec2 pos) {
    const int pattern[16] = int[16](
         0,  8,  2, 10,
        12,  4, 14,  6,
         3, 11,  1,  9,
        15,  7, 13,  5
    );
    int idx = (pos.x % 4) + (pos.y % 4) * 4;
    return float(pattern[idx]) / 16.0;
}

void main() {
    // Resolve UV for affine texturing (undo the w-multiply from vertex shader)
    vec2 uv = fragUV / fragClipW;

    // Choose normal: flat shading uses face normal from derivatives
    vec3 normal;
    if ((material.flags & FLAG_FLAT_SHADING) != 0) {
        vec3 dFdxPos = dFdx(fragWorldPos);
        vec3 dFdyPos = dFdy(fragWorldPos);
        normal = normalize(cross(dFdxPos, dFdyPos));
    } else {
        normal = normalize(fragNormal);
    }

    vec3 viewDir = normalize(lighting.cameraPos - fragWorldPos);

    // Material properties - sample textures when available
    vec3 albedo = material.baseColor;
    float metallic = material.metallic;
    float roughness = material.roughness;

    // Sample base color texture if available
    if ((material.flags & FLAG_HAS_BASE_COLOR_TEX) != 0) {
        vec4 texColor = texture(baseColorTexture, uv);
        albedo *= texColor.rgb;
    }

    // Multiply with vertex color (baked shadows / per-vertex lighting)
    albedo *= fragVertColor.rgb;

    // Convert roughness to shininess for Blinn-Phong
    float shininess = max(2.0, (2.0 / (roughness * roughness + 0.0001)) - 2.0);
    shininess = clamp(shininess, 2.0, 256.0);

    // Start with ambient
    vec3 result = lighting.ambientColor * lighting.ambientIntensity * albedo;

    // Process directional lights
    for (uint i = 0u; i < lighting.directionalLightCount && i < MAX_DIRECTIONAL_LIGHTS; ++i) {
        // Negate direction: stored as "where light points", we need "toward light source"
        vec3 lightDir = -normalize(lighting.directionalLights[i].direction);
        vec3 lightColor = lighting.directionalLights[i].color;
        float intensity = lighting.directionalLights[i].intensity;

        // Apply shadow only to first directional light
        float shadow = 1.0;
        if (i == 0u) {
            shadow = calcShadow(fragPosLightSpace, normal, lightDir);
        }

        result += shadow * calcBlinnPhong(lightDir, lightColor, intensity, normal, viewDir, albedo, metallic, shininess);
    }

    // Process point lights
    for (uint i = 0u; i < lighting.pointLightCount && i < MAX_POINT_LIGHTS; ++i) {
        vec3 lightPos = lighting.pointLights[i].position;
        vec3 lightVec = lightPos - fragWorldPos;
        float distance = length(lightVec);

        // Skip if beyond range
        if (distance > lighting.pointLights[i].range) continue;

        vec3 lightDir = normalize(lightVec);
        vec3 lightColor = lighting.pointLights[i].color;
        float intensity = lighting.pointLights[i].intensity;

        // Attenuation
        float atten = calcAttenuation(distance,
            lighting.pointLights[i].constantAtten,
            lighting.pointLights[i].linearAtten,
            lighting.pointLights[i].quadraticAtten);

        result += calcBlinnPhong(lightDir, lightColor, intensity * atten, normal, viewDir, albedo, metallic, shininess);
    }

    // Process spot lights
    for (uint i = 0u; i < lighting.spotLightCount && i < MAX_SPOT_LIGHTS; ++i) {
        vec3 lightPos = lighting.spotLights[i].position;
        vec3 lightVec = lightPos - fragWorldPos;
        float distance = length(lightVec);

        // Skip if beyond range
        if (distance > lighting.spotLights[i].range) continue;

        vec3 lightDir = normalize(lightVec);
        vec3 spotDir = normalize(lighting.spotLights[i].direction);

        // Spotlight cone attenuation
        float theta = dot(lightDir, -spotDir);
        float innerCutoff = lighting.spotLights[i].innerCutoff;
        float outerCutoff = lighting.spotLights[i].outerCutoff;
        float epsilon = innerCutoff - outerCutoff;
        float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);

        if (spotIntensity > 0.0) {
            vec3 lightColor = lighting.spotLights[i].color;
            float intensity = lighting.spotLights[i].intensity;

            // Distance attenuation
            float atten = calcAttenuation(distance,
                lighting.spotLights[i].constantAtten,
                lighting.spotLights[i].linearAtten,
                lighting.spotLights[i].quadraticAtten);

            result += calcBlinnPhong(lightDir, lightColor, intensity * atten * spotIntensity, normal, viewDir, albedo, metallic, shininess);
        }
    }

    // Add emission
    result += material.emissiveColor * material.emissiveStrength;

    // Gamma correction
    result = pow(result, vec3(1.0 / 2.2));

    // Alpha handling
    float alpha = material.opacity * fragVertColor.a;
    int alphaMode = (material.flags >> 8) & 0x3;
    if (alphaMode == 1) { // Mask mode
        if (alpha < material.alphaCutoff) {
            discard;
        }
        alpha = 1.0;
    }

    // Stipple transparency: screen-door dither pattern for alpha
    if ((material.flags & FLAG_STIPPLE_TRANS) != 0 && alpha < 1.0) {
        float threshold = bayerDither4x4(ivec2(gl_FragCoord.xy));
        if (alpha < threshold) {
            discard;
        }
        alpha = 1.0; // surviving fragments are fully opaque
    }

    outColor = vec4(result, alpha);
}
