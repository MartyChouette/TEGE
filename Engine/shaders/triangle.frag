#version 450

// Lit Mesh Fragment Shader with Multi-light, Cascaded Shadow, and Retro Effect Support

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in float fragViewDepth;  // View-space depth for cascade selection
layout(location = 4) in vec4 fragVertColor;
layout(location = 5) in float fragClipW;
layout(location = 6) in vec4 fragTangent;

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
    mat4 cascadeViewProj[4];
    vec4 cascadeSplits;
    float shadowSoftness;
    int shadowEnabled;
    float shadowStrength;
    float shadowMaxDistance;
    int pointShadowCount;
    int spotShadowCount;
    vec2 _pointSpotPad;
    vec4 windData;  // xyz = wind direction * strength, w = time (unused in frag, layout must match)
    vec4 fogParams;     // x=density, y=start, z=end, w=heightFalloff
    vec4 fogColorSnow;  // xyz=fog color, w=snow intensity
    vec4 playerPosition; // xyz = player world pos, w = step radius
    vec4 worldCurvature; // x = strength, yzw reserved (layout must match)
    vec4 skyReflectColor; // xyz = sky reflection color, w = reserved
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
    int flags;  // bits 0-7: render flags, 8-9: alpha mode, 10: height tex, 16-19: texture flags, 20-31: retro
    float parallaxScale;
    float shoreWidth;
    float foamIntensity;
    float foamScale;
} material;

// Material flag bits
#define FLAG_DOUBLE_SIDED       (1 << 0)
#define FLAG_CAST_SHADOWS       (1 << 1)
#define FLAG_RECEIVE_SHADOWS    (1 << 2)
#define FLAG_HAS_BASE_COLOR_TEX (1 << 16)
#define FLAG_HAS_NORMAL_TEX     (1 << 17)
#define FLAG_HAS_METALLIC_TEX   (1 << 18)
#define FLAG_HAS_EMISSIVE_TEX   (1 << 19)

// Height texture flag
#define FLAG_HAS_HEIGHT_TEX     (1 << 10)

// Water/rain flag bits
#define FLAG_WATER_SURFACE      (1 << 5)
#define FLAG_RAIN_RIPPLES       (1 << 6)
#define FLAG_WATER_SHORE        (1 << 7)
#define FLAG_WATER_OCEAN        (1 << 11)

// Retro flag bits (must match vertex shader and C++ Material.h)
#define FLAG_FLAT_SHADING       (1 << 20)
#define FLAG_AFFINE_TEXTURING   (1 << 21)
#define FLAG_VERTEX_SNAPPING    (1 << 22)
#define FLAG_STIPPLE_TRANS      (1 << 23)
#define FLAG_UV_QUANTIZE        (1 << 12)
#define FLAG_GOURAUD_ONLY       (1 << 13)

// Shadow dither mode (bits 14-15): 0=None, 1=By Darkness, 2=By Distance, 3=By Angle
#define FLAG_SHADOW_DITHER_SHIFT 14
#define FLAG_SHADOW_DITHER_MASK  (3 << FLAG_SHADOW_DITHER_SHIFT)

// Base color texture sampler (binding 3)
layout(binding = 3) uniform sampler2D baseColorTexture;

// Shadow map sampler - 2D array for cascaded shadows (binding 4)
layout(binding = 4) uniform sampler2DArrayShadow shadowMap;

// Height map sampler for parallax mapping (binding 5)
layout(binding = 5) uniform sampler2D heightMap;

// Normal map sampler (binding 6)
layout(binding = 6) uniform sampler2D normalMap;

// Metallic-roughness map sampler (binding 8)
layout(binding = 8) uniform sampler2D metallicRoughnessMap;

// Emissive map sampler (binding 9)
layout(binding = 9) uniform sampler2D emissiveMap;

// Point light shadow cubemap array (binding 10)
layout(binding = 10) uniform samplerCubeArrayShadow pointShadowMaps;

// Spot light shadow 2D array (binding 11)
layout(binding = 11) uniform sampler2DArrayShadow spotShadowMaps;

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

// Shadow data SSBO for point/spot light shadow matrices (binding 12)
#define MAX_SHADOW_POINT_LIGHTS 4
#define MAX_SHADOW_SPOT_LIGHTS 4

layout(std430, binding = 12) readonly buffer ShadowDataSSBO {
    mat4 pointFaceViewProj[MAX_SHADOW_POINT_LIGHTS * 6];
    vec4 pointLightParams[MAX_SHADOW_POINT_LIGHTS];  // xyz=pos, w=range
    mat4 spotViewProj[MAX_SHADOW_SPOT_LIGHTS];
    int pointShadowCountSSBO;
    int spotShadowCountSSBO;
    int _ssbo_pad[2];
} shadowData;

// 16-sample Poisson disk for soft shadow PCF
const vec2 poissonDisk[16] = vec2[16](
    vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790)
);

// Calculate cascaded shadow factor using PCF (Percentage Closer Filtering)
float calcShadowCSM(float viewDepth, vec3 worldPos, vec3 normal, vec3 lightDir) {
    if (lighting.shadowEnabled == 0) return 1.0;
    if ((material.flags & FLAG_RECEIVE_SHADOWS) == 0) return 1.0;

    // Select cascade based on view-space depth
    int cascadeIdx = 3;  // default to furthest
    for (int i = 0; i < 4; ++i) {
        if (viewDepth < lighting.cascadeSplits[i]) {
            cascadeIdx = i;
            break;
        }
    }

    // Transform world position to this cascade's light space
    vec4 lightSpacePos = lighting.cascadeViewProj[cascadeIdx] * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Out-of-bounds check
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0;
    }

    float currentDepth = projCoords.z;

    // PCF filtering
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0).xy);

    if (lighting.shadowSoftness <= 0.0) {
        // Hard shadows: fast 3x3 grid (9 samples)
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                vec2 offset = vec2(float(x), float(y)) * texelSize;
                shadow += texture(shadowMap, vec4(projCoords.xy + offset,
                                                  float(cascadeIdx), currentDepth));
            }
        }
        shadow /= 9.0;
    } else {
        // Soft shadows: 16-sample Poisson disk with per-pixel rotation
        float angle = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
        float s = sin(angle);
        float c = cos(angle);
        mat2 rotation = mat2(c, s, -s, c);

        float radius = lighting.shadowSoftness;
        for (int i = 0; i < 16; ++i) {
            vec2 offset = rotation * poissonDisk[i] * radius * texelSize;
            shadow += texture(shadowMap, vec4(projCoords.xy + offset,
                                              float(cascadeIdx), currentDepth));
        }
        shadow /= 16.0;
    }

    // Distance fade: smoothly fade shadows near max distance
    float fadeStart = lighting.shadowMaxDistance * 0.8;
    float fadeFactor = 1.0 - smoothstep(fadeStart, lighting.shadowMaxDistance, viewDepth);
    shadow = mix(1.0, shadow, fadeFactor);

    // Apply shadow strength
    shadow = mix(1.0, shadow, lighting.shadowStrength);

    return shadow;
}

// Calculate point light shadow factor using cubemap shadow sampling
float calcPointShadow(vec3 fragPos, int shadowIdx) {
    vec3 lightPos = shadowData.pointLightParams[shadowIdx].xyz;
    float range = shadowData.pointLightParams[shadowIdx].w;
    vec3 fragToLight = fragPos - lightPos;
    float dist = length(fragToLight);
    if (dist > range || dist < 0.001) return 1.0;

    // Non-linear reference depth matching custom Vulkan [0,1] perspective
    float nearP = 0.1;
    float d = max(max(abs(fragToLight.x), abs(fragToLight.y)), abs(fragToLight.z));
    float refDepth = range * (d - nearP) / ((range - nearP) * d);

    float shadow;
    if (lighting.shadowSoftness <= 0.0) {
        // Hard shadow: single sample
        shadow = texture(pointShadowMaps, vec4(fragToLight, float(shadowIdx)), refDepth);
    } else {
        // Soft shadow: perturb direction with Poisson disk offsets in 3D
        float angle = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
        float s = sin(angle);
        float c = cos(angle);

        float radius = lighting.shadowSoftness * 0.02;  // Scale for cubemap texels
        shadow = 0.0;
        // Build a tangent frame around fragToLight direction
        vec3 dir = normalize(fragToLight);
        vec3 up = abs(dir.y) < 0.99 ? vec3(0,1,0) : vec3(1,0,0);
        vec3 right = normalize(cross(up, dir));
        up = cross(dir, right);

        for (int i = 0; i < 16; ++i) {
            vec2 offset = vec2(
                c * poissonDisk[i].x - s * poissonDisk[i].y,
                s * poissonDisk[i].x + c * poissonDisk[i].y
            ) * radius;
            vec3 sampleDir = fragToLight + right * offset.x * dist + up * offset.y * dist;
            shadow += texture(pointShadowMaps, vec4(sampleDir, float(shadowIdx)), refDepth);
        }
        shadow /= 16.0;
    }

    // Distance fade
    float fadeStart = range * 0.8;
    shadow = mix(1.0, shadow, 1.0 - smoothstep(fadeStart, range, dist));
    return mix(1.0, shadow, lighting.shadowStrength);
}

// Calculate spot light shadow factor using 2D array shadow sampling
float calcSpotShadow(vec3 fragPos, int shadowIdx) {
    // Project fragment into spot light space
    vec4 lightSpacePos = shadowData.spotViewProj[shadowIdx] * vec4(fragPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Out-of-bounds check
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0;
    }

    float currentDepth = projCoords.z;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(spotShadowMaps, 0).xy);

    if (lighting.shadowSoftness <= 0.0) {
        // Hard shadows: 3x3 PCF
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                vec2 offset = vec2(float(x), float(y)) * texelSize;
                shadow += texture(spotShadowMaps, vec4(projCoords.xy + offset,
                                                        float(shadowIdx), currentDepth));
            }
        }
        shadow /= 9.0;
    } else {
        // Soft shadows: 16-sample Poisson disk
        float angle = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
        float s = sin(angle);
        float c = cos(angle);
        mat2 rotation = mat2(c, s, -s, c);

        float radius = lighting.shadowSoftness;
        for (int i = 0; i < 16; ++i) {
            vec2 offset = rotation * poissonDisk[i] * radius * texelSize;
            shadow += texture(spotShadowMaps, vec4(projCoords.xy + offset,
                                                    float(shadowIdx), currentDepth));
        }
        shadow /= 16.0;
    }

    // Apply shadow strength
    return mix(1.0, shadow, lighting.shadowStrength);
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

// Parallax Occlusion Mapping - ray marches through the height map
vec2 parallaxOcclusionMapping(vec2 texCoords, vec3 viewDirTangent) {
    // Adaptive layer count: more layers at grazing angles for quality
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0, 0, 1), viewDirTangent)));

    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;

    // Direction to shift UV per layer (scaled by parallax amount)
    vec2 P = viewDirTangent.xy * material.parallaxScale;
    vec2 deltaTexCoords = P / numLayers;

    vec2 currentTexCoords = texCoords;
    float currentDepthMapValue = texture(heightMap, currentTexCoords).r;

    // Step through layers until we find the intersection
    while (currentLayerDepth < currentDepthMapValue) {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(heightMap, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }

    // Interpolate between previous and current position for smoother result
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(heightMap, prevTexCoords).r - currentLayerDepth + layerDepth;
    float weight = afterDepth / (afterDepth - beforeDepth);

    return prevTexCoords * weight + currentTexCoords * (1.0 - weight);
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

    // Parallax Occlusion Mapping: offset UV using height map before any texture sampling
    // Skip for water surfaces — POM is expensive and adds little visual value on water
    if ((material.flags & FLAG_HAS_HEIGHT_TEX) != 0 && material.parallaxScale > 0.0
        && (material.flags & FLAG_WATER_SURFACE) == 0) {
        // Build TBN matrix from interpolated normal and tangent
        vec3 N = normalize(fragNormal);
        vec3 T = normalize(fragTangent.xyz);
        // Re-orthogonalize tangent (Gram-Schmidt)
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T) * fragTangent.w; // w = handedness
        mat3 TBN = mat3(T, B, N);
        mat3 TBN_inv = transpose(TBN); // transpose = inverse for orthonormal basis

        // View direction in tangent space
        vec3 viewDirWorld = normalize(lighting.cameraPos - fragWorldPos);
        vec3 viewDirTangent = normalize(TBN_inv * viewDirWorld);

        uv = parallaxOcclusionMapping(uv, viewDirTangent);
    }

    // Choose normal: flat shading, normal map, or interpolated vertex normal
    vec3 normal;
    if ((material.flags & FLAG_FLAT_SHADING) != 0) {
        vec3 dFdxPos = dFdx(fragWorldPos);
        vec3 dFdyPos = dFdy(fragWorldPos);
        normal = normalize(cross(dFdxPos, dFdyPos));
    } else if ((material.flags & FLAG_HAS_NORMAL_TEX) != 0) {
        // Sample normal map and transform from tangent space to world space
        vec3 N = normalize(fragNormal);
        vec3 T = normalize(fragTangent.xyz);
        T = normalize(T - dot(T, N) * N); // Re-orthogonalize
        vec3 B = cross(N, T) * fragTangent.w;
        mat3 TBN = mat3(T, B, N);

        // Normal map is stored as [0,1], remap to [-1,1]
        vec3 sampledNormal = texture(normalMap, uv).rgb * 2.0 - 1.0;
        normal = normalize(TBN * sampledNormal);
    } else {
        normal = normalize(fragNormal);
    }

    // Water surface: rain ripple normal perturbation — individual drop ripples
    // Optimized: 2 layers (down from 3), 5 neighbors (skip corners), early distance exit
    if ((material.flags & FLAG_WATER_SURFACE) != 0 && (material.flags & FLAG_RAIN_RIPPLES) != 0) {
        float waterTime = lighting.windData.w;
        vec2 worldXZ = fragWorldPos.xz;

        // Pseudo-random hash helpers
        #define HASH2(p) fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453)
        #define HASH1(p) fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453)

        vec2 rippleOffset = vec2(0.0);

        // 5-tap neighbor offsets: center + 4 cardinal directions (skip corners)
        const vec2 neighborOffsets[5] = vec2[5](
            vec2(0, 0), vec2(-1, 0), vec2(1, 0), vec2(0, -1), vec2(0, 1)
        );

        // 2 grid layers (was 3) — still looks dense enough with 2 overlapping scales
        for (int layer = 0; layer < 2; ++layer) {
            float scale = 1.2 + float(layer) * 0.9;
            vec2 gridUV = worldXZ * scale;
            vec2 cellId = floor(gridUV);

            // Check center cell + 4 cardinal neighbors (10 evaluations vs. 18)
            for (int n = 0; n < 5; ++n) {
                vec2 neighbor = cellId + neighborOffsets[n];

                // Random drop position within this cell
                vec2 rnd = HASH2(neighbor + float(layer) * 53.0);
                vec2 dropPos = (neighbor + rnd) / scale;

                // Early distance check — skip if fragment is too far from this drop
                float dist = length(worldXZ - dropPos);
                if (dist > 2.0) continue;

                // Random drop timing: each drop has its own phase
                float dropPhase = HASH1(neighbor + float(layer) * 71.0);
                float dropInterval = 1.8 + dropPhase * 1.4;
                float localTime = mod(waterTime + dropPhase * dropInterval, dropInterval);

                float age = localTime;
                float maxAge = dropInterval * 0.85;

                if (age < maxAge) {
                    // Expanding ring radius
                    float ringRadius = age * 1.5;
                    float ringDist = abs(dist - ringRadius);

                    // Sharp ring that fades with age
                    float ring = exp(-ringDist * 12.0) * (1.0 - age / maxAge);
                    ring *= smoothstep(2.0, 0.0, dist);

                    vec2 dir = (worldXZ - dropPos) / max(dist, 0.001);
                    rippleOffset += dir * ring * 0.12;
                }
            }
        }

        #undef HASH2
        #undef HASH1

        // Perturb the normal with accumulated ripple offset
        normal = normalize(normal + vec3(rippleOffset.x, 0.0, rippleOffset.y));
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

    // Sample metallic-roughness texture if available (glTF convention: G=roughness, B=metallic)
    if ((material.flags & FLAG_HAS_METALLIC_TEX) != 0) {
        vec4 mrSample = texture(metallicRoughnessMap, uv);
        roughness *= mrSample.g;
        metallic *= mrSample.b;
    }

    // Sample emissive texture if available
    vec3 emissiveTexColor = vec3(1.0);
    if ((material.flags & FLAG_HAS_EMISSIVE_TEX) != 0) {
        emissiveTexColor = texture(emissiveMap, uv).rgb;
    }

    // Multiply with vertex color (baked shadows / per-vertex lighting)
    // Skip for water surfaces: vertex color G channel stores edge distance, not color
    if ((material.flags & FLAG_WATER_SURFACE) == 0) {
        albedo *= fragVertColor.rgb;
    }

    // Gouraud-only mode: skip per-pixel lighting, use vertex color as pre-computed lighting
    if ((material.flags & FLAG_GOURAUD_ONLY) != 0) {
        vec3 result = albedo;
        // Add emission (modulated by emissive texture if present)
        result += material.emissiveColor * material.emissiveStrength * emissiveTexColor;
        // Gamma correction
        result = pow(result, vec3(1.0 / 2.2));
        // Alpha handling
        float alpha = material.opacity * fragVertColor.a;
        int alphaMode = (material.flags >> 8) & 0x3;
        if (alphaMode == 1) {
            if (alpha < material.alphaCutoff) discard;
            alpha = 1.0;
        }
        if ((material.flags & FLAG_STIPPLE_TRANS) != 0 && alpha < 1.0) {
            float threshold = bayerDither4x4(ivec2(gl_FragCoord.xy));
            if (alpha < threshold) discard;
            alpha = 1.0;
        }
        outColor = vec4(result, alpha);
        return;
    }

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

        // Apply cascaded shadow only to first directional light
        float shadow = 1.0;
        if (i == 0u) {
            shadow = calcShadowCSM(fragViewDepth, fragWorldPos, normal, lightDir);

            // Shadow dither: replace smooth shadow with binary dither pattern
            int shadowDitherMode = (material.flags & FLAG_SHADOW_DITHER_MASK) >> FLAG_SHADOW_DITHER_SHIFT;
            if (shadowDitherMode > 0 && shadow < 1.0) {
                float ditherThreshold = bayerDither4x4(ivec2(gl_FragCoord.xy));
                float ditherFactor;

                if (shadowDitherMode == 1) {
                    // By darkness: shadow factor drives dither density
                    ditherFactor = shadow;
                } else if (shadowDitherMode == 2) {
                    // By distance: farther from camera = denser dithering
                    ditherFactor = 1.0 - clamp(fragViewDepth / lighting.shadowMaxDistance, 0.0, 1.0);
                } else {
                    // By angle: grazing light angle = denser dithering
                    ditherFactor = max(dot(normal, lightDir), 0.0);
                }

                shadow = (ditherFactor > ditherThreshold) ? 1.0 : (1.0 - lighting.shadowStrength);
            }
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

        // Point light shadow (indices 0..N-1 have shadow maps)
        float shadow = 1.0;
        if (int(i) < lighting.pointShadowCount && (material.flags & FLAG_RECEIVE_SHADOWS) != 0) {
            shadow = calcPointShadow(fragWorldPos, int(i));
        }

        result += shadow * calcBlinnPhong(lightDir, lightColor, intensity * atten, normal, viewDir, albedo, metallic, shininess);
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

            // Spot light shadow (indices 0..N-1 have shadow maps)
            float shadow = 1.0;
            if (int(i) < lighting.spotShadowCount && (material.flags & FLAG_RECEIVE_SHADOWS) != 0) {
                shadow = calcSpotShadow(fragWorldPos, int(i));
            }

            result += shadow * calcBlinnPhong(lightDir, lightColor, intensity * atten * spotIntensity, normal, viewDir, albedo, metallic, shininess);
        }
    }

    // Water surface: fresnel-like reflective sheen with freeze transition
    if ((material.flags & FLAG_WATER_SURFACE) != 0) {
        float NdotV = max(dot(normal, viewDir), 0.0);
        float freezeProgress = material.parallaxScale;
        vec3 skyColor = lighting.skyReflectColor.xyz;

        // Fresnel transitions: water (low base, steep falloff) -> ice (higher base, broader)
        float fresnelBase = mix(0.02, 0.08, freezeProgress);
        float fresnelExp  = mix(5.0, 3.0, freezeProgress);
        float fresnel = fresnelBase + (1.0 - fresnelBase) * pow(1.0 - NdotV, fresnelExp);

        // Frozen surface tints reflection slightly blue-white
        vec3 reflectColor = mix(skyColor, skyColor * vec3(0.85, 0.9, 1.0) + vec3(0.1), freezeProgress * 0.4);

        // Reflection strength increases for ice
        float reflectStrength = mix(0.6, 0.85, freezeProgress);
        result = mix(result, reflectColor, fresnel * reflectStrength);

        // Shore foam: procedural noise foam near edges
        if ((material.flags & FLAG_WATER_SHORE) != 0 && material.foamIntensity > 0.0) {
            float edgeDist = fragVertColor.g;  // 0=edge, 1=center
            float shoreW = material.shoreWidth;

            // Shallow water color blend near edges
            float shallowBlend = 1.0 - smoothstep(0.0, shoreW * 2.0, edgeDist);
            // shoreColor is approximated from baseColor lightened — we use fragVertColor.r channel
            // for the shore color R and use a brighter tint of the water
            vec3 shoreColor = material.baseColor * 1.5 + vec3(0.1, 0.15, 0.1);
            result = mix(result, shoreColor, shallowBlend * 0.4);

            // Multi-octave hash noise for foam pattern
            float foamSc = material.foamScale;
            float waterTime = lighting.windData.w;
            vec2 wp = fragWorldPos.xz;

            // Simple hash-based noise
            #define FOAM_HASH(p) fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453)
            float n1 = FOAM_HASH(floor(wp * foamSc));
            float n2 = FOAM_HASH(floor(wp * foamSc * 2.3 + vec2(waterTime * 0.3)));
            float n3 = FOAM_HASH(floor(wp * foamSc * 4.7 + vec2(waterTime * 0.7)));
            float noise = (n1 * 0.5 + n2 * 0.3 + n3 * 0.2);
            #undef FOAM_HASH

            // Animated threshold: foam appears and disappears
            float foamThreshold = smoothstep(shoreW, 0.0, edgeDist);
            float foam = smoothstep(0.35, 0.65, noise) * foamThreshold * material.foamIntensity;

            // Blend white foam into result
            result = mix(result, vec3(0.9, 0.95, 1.0), foam);
        }
    }

    // Add emission (modulated by emissive texture if present)
    result += material.emissiveColor * material.emissiveStrength * emissiveTexColor;

    // Snow accumulation: whiten upward-facing surfaces based on snow intensity
    float snowIntensity = lighting.fogColorSnow.w;
    if (snowIntensity > 0.0) {
        float snowCoverage = snowIntensity * smoothstep(0.3, 0.8, normal.y);
        result = mix(result, vec3(0.95, 0.97, 1.0), snowCoverage);
    }

    // Gamma correction
    result = pow(result, vec3(1.0 / 2.2));

    // Height-based distance fog (volumetric feel: thicker near ground, thins at height)
    float fogDensity = lighting.fogParams.x;
    if (fogDensity > 0.0) {
        float fogStart = lighting.fogParams.y;
        float fogEnd = lighting.fogParams.z;
        float fogHeightFalloff = lighting.fogParams.w;

        float dist = length(lighting.cameraPos - fragWorldPos);
        float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
        fogFactor *= fogDensity;

        // Height falloff: fog thins as Y increases above ground
        float heightFog = exp(-max(fragWorldPos.y, 0.0) * fogHeightFalloff);
        fogFactor *= heightFog;

        vec3 fogColor = lighting.fogColorSnow.xyz;
        result = mix(result, fogColor, fogFactor);
    }

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
