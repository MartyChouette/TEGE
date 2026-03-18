#version 450

// Lit Mesh Fragment Shader with Multi-light, Cascaded Shadow, and Retro Effect Support

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in float fragViewDepth;  // View-space depth for cascade selection
layout(location = 4) in vec4 fragVertColor;
layout(location = 5) in float fragClipW;
layout(location = 6) in vec4 fragTangent;
layout(location = 7) in vec4 fragCurClipPos;  // Current clip position (for velocity)
layout(location = 8) in vec4 fragPrevClipPos; // Previous frame clip position (for velocity)

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outVelocity;    // Per-pixel screen-space motion vector (RG16F)

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
    float celDiffuseBands;     // 0 = disabled, 2-8 = quantization bands
    float celSpecularCutoff;   // 0 = disabled, >0 = hard specular cutoff
    uint shadingFlags;         // bit0=GGX, bit1=Fresnel, bit2=EnergyConserv, bit3=GeometryTerm, bit4=SphereEnvMap
    float sphereEnvStrength;   // Spherical environment map intensity (0=off)
    float posterizeLevels;     // Color posterization levels (0=disabled)
    float texturePageSize;  // PS1 VRAM page size in texels (0=off)
    vec4 windData;  // xyz = wind direction * strength, w = time (unused in frag, layout must match)
    vec4 fogParams;     // x=density, y=start, z=end, w=heightFalloff
    vec4 fogColorSnow;  // xyz=fog color, w=snow intensity
    vec4 playerPosition; // xyz = player world pos, w = step radius
    vec4 worldCurvature; // x = strength, yzw reserved (layout must match)
    vec4 skyReflectColor; // xyz = sky reflection color, w = reserved
    vec4 shProbeIrradiance; // xyz = SH probe irradiance, w = blend weight (0 or 1)
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
    float surfaceParam1;  // water: shoreWidth | artistic: reflectivity
    float surfaceParam2;  // water: foamIntensity | artistic: fresnelPower
    float surfaceParam3;  // water: foamScale | artistic: rimLightStrength
} material;

// Material SSBO (binding 2) — batched material data with per-entity dynamic offset
// Layout matches C++ MaterialGPU (std430, 80 bytes per entry)
layout(std430, binding = 2) readonly buffer MaterialSSBO {
    vec3  matBaseColor;
    float matMetallic;
    vec3  matEmissiveColor;
    float matRoughness;
    float matEmissiveStrength;
    float matOpacity;
    float matAlphaCutoff;
    int   matFlags;
    float matTransmission;
    float matIOR;
    float matThickness;
    float matSSSIntensity;
    vec3  matSSSColor;
    float matSSSRadius;
} materialData;

// Material flag bits
#define FLAG_DOUBLE_SIDED       (1 << 0)
#define FLAG_CAST_SHADOWS       (1 << 1)
#define FLAG_RECEIVE_SHADOWS    (1 << 2)
#define FLAG_EXCLUDE_CEL        (1 << 4)
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
// Shadow dither pattern (bits 29-31): 0=Bayer4x4, 1=Bayer8x8, 2=BlueNoise, 3=Halftone, 4=Crosshatch, 5=Overlook
#define FLAG_SHADOW_DITHER_PATTERN_SHIFT 29
#define FLAG_SHADOW_DITHER_PATTERN_MASK  (7 << FLAG_SHADOW_DITHER_PATTERN_SHIFT)

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

// --- Clustered forward lighting SSBOs (bindings 14-15) ---
#ifdef CLUSTERED_LIGHTING
#define CL_GRID_X 16u
#define CL_GRID_Y 9u
#define CL_GRID_Z 24u

struct ClusterCell {
    uint offset;
    uint count;
};

layout(std430, binding = 14) readonly buffer ClusterGridSSBO {
    float clScreenWidth;
    float clScreenHeight;
    float clNearPlane;
    float clFarPlane;
    ClusterCell clCells[];
};

// Packed light indices: bits 0-15 = UBO array index, bit 16 = type (0=point, 1=spot)
layout(std430, binding = 15) readonly buffer ClusterLightIndexSSBO {
    uint clLightIndices[];
};
#endif

// --- Virtual texturing samplers (bindings 16-17) ---
#ifdef VIRTUAL_TEXTURING
layout(binding = 16) uniform sampler2D vtIndirectionTex;
layout(binding = 17) uniform sampler2D vtPhysicalAtlas;
#endif

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
// Sample shadow from a single cascade (helper for blending)
float sampleShadowCascade(int cascadeIdx, vec3 worldPos, vec2 texelSize) {
    vec4 lightSpacePos = lighting.cascadeViewProj[cascadeIdx] * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Out-of-bounds: no shadow
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0;
    }

    float currentDepth = projCoords.z;
    float shadow = 0.0;

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

    return shadow;
}

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

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0).xy);

    // Sample primary cascade
    float shadow = sampleShadowCascade(cascadeIdx, worldPos, texelSize);

    // Blend with next cascade near boundaries to eliminate seam lines
    if (cascadeIdx < 3) {
        float splitDist = lighting.cascadeSplits[cascadeIdx];
        float blendRange = splitDist * 0.1;  // 10% blend zone
        if (viewDepth > splitDist - blendRange) {
            float nextShadow = sampleShadowCascade(cascadeIdx + 1, worldPos, texelSize);
            float blend = smoothstep(splitDist - blendRange, splitDist, viewDepth);
            shadow = mix(shadow, nextShadow, blend);
        }
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

// Shading flag bits (must match C++ packing)
#define SHADING_GGX              (1u << 0)
#define SHADING_FRESNEL          (1u << 1)
#define SHADING_ENERGY_CONSERV   (1u << 2)
#define SHADING_GEOMETRY_TERM    (1u << 3)
#define SHADING_SPHERE_ENV       (1u << 4)
#define SHADING_HALF_LAMBERT     (1u << 5)  // NdotL * 0.5 + 0.5 (softer falloff)

const float PI = 3.14159265359;

// --- Fresnel-Schlick approximation ---
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// --- GGX/Trowbridge-Reitz normal distribution ---
float distributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// --- Smith GGX geometry function (single direction) ---
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// --- Smith geometry (combined view + light) ---
float geometrySmith(float NdotV, float NdotL, float roughness) {
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

// Procedural light ramp: remap NdotL for art-style lighting
// Mode 0=off, 1=smooth step (TF2/hand-painted), 2=warm shadow, 3=cool shadow, 4=anime (hard 2-band)
// Returns modified diffuse contribution (vec3 to allow shadow tinting)
vec3 lightRamp(float NdotL) {
    float mode = lighting.skyReflectColor.w; // packed in reserved field
    if (mode < 0.5) return vec3(NdotL); // off

    if (mode < 1.5) {
        // Smooth step ramp: soft band transition (hand-painted look)
        float ramp = smoothstep(0.0, 0.4, NdotL) * 0.5 + smoothstep(0.4, 0.8, NdotL) * 0.5;
        return vec3(ramp);
    } else if (mode < 2.5) {
        // Warm shadow: shadow areas shift to warm amber
        float ramp = smoothstep(0.0, 0.5, NdotL);
        vec3 shadowColor = vec3(0.35, 0.2, 0.1); // warm amber shadow
        return mix(shadowColor, vec3(1.0), ramp);
    } else if (mode < 3.5) {
        // Cool shadow: shadow areas shift to blue/purple
        float ramp = smoothstep(0.0, 0.5, NdotL);
        vec3 shadowColor = vec3(0.15, 0.12, 0.3); // cool purple shadow
        return mix(shadowColor, vec3(1.0), ramp);
    } else {
        // Anime: hard 2-band with warm shadow tint
        float ramp = step(0.35, NdotL);
        vec3 shadowColor = vec3(0.6, 0.45, 0.5); // muted pink-purple shadow
        return mix(shadowColor, vec3(1.0), ramp);
    }
}

// Calculate Blinn-Phong lighting contribution
vec3 calcBlinnPhong(vec3 lightDir, vec3 lightColor, float lightIntensity, vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float shininess) {
    float rawNdotL = dot(normal, lightDir);
    // Light wrap: 0.0 = standard clamped, 0.5 = half-Lambert, 1.0 = full wrap
    float wrapAmount = ((lighting.shadingFlags & SHADING_HALF_LAMBERT) != 0u) ? 0.5 : 0.0;
    float NdotL = max(rawNdotL * (1.0 - wrapAmount) + wrapAmount, 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfwayDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.001);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    uint flags = lighting.shadingFlags;

    // Fresnel
    vec3 F = ((flags & SHADING_FRESNEL) != 0u)
        ? fresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), F0)
        : F0;

    // Specular distribution: GGX or Blinn-Phong
    float spec;
    if ((flags & SHADING_GGX) != 0u) {
        float roughness = clamp(sqrt(2.0 / (shininess + 2.0)), 0.04, 1.0);
        spec = distributionGGX(NdotH, roughness);

        // Geometry term
        if ((flags & SHADING_GEOMETRY_TERM) != 0u) {
            spec *= geometrySmith(NdotV, NdotL, roughness);
        }

        // Cook-Torrance denominator
        spec /= max(4.0 * NdotV * NdotL, 0.001);
    } else {
        spec = pow(NdotH, shininess);
    }

    vec3 specular = spec * F * lightColor * lightIntensity;

    // Diffuse with optional energy conservation
    vec3 kD = vec3(1.0 - metallic);
    if ((flags & SHADING_ENERGY_CONSERV) != 0u) {
        kD *= (vec3(1.0) - F);
    }
    // Apply light ramp to diffuse (replaces raw NdotL with stylized ramp)
    vec3 rampedDiffuse = lightRamp(NdotL);
    vec3 diffuse = rampedDiffuse * lightColor * lightIntensity;

    return diffuse * albedo * kD + specular;
}

// Cel-shaded Blinn-Phong: quantize diffuse into bands, hard-cutoff specular
// Respects shading flags (GGX, Fresnel, energy conservation, geometry term)
vec3 calcBlinnPhongCel(vec3 lightDir, vec3 lightColor, float lightIntensity, vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float shininess) {
    float NdotL = max(dot(normal, lightDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.001);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfwayDir), 0.0);

    // Diffuse with band quantization
    float bands = lighting.celDiffuseBands;
    float diff = floor(NdotL * bands + 0.5) / bands;

    // Colored shadows: tint dark areas instead of pure black
    float celShadowMode = lighting.worldCurvature.w;
    vec3 shadowTint = vec3(0.0);
    if (celShadowMode > 0.5) {
        if (celShadowMode < 1.5) shadowTint = vec3(0.25, 0.15, 0.35);      // purple
        else if (celShadowMode < 2.5) shadowTint = vec3(0.15, 0.2, 0.35);  // blue
        else if (celShadowMode < 3.5) shadowTint = vec3(0.35, 0.2, 0.15);  // warm
        else shadowTint = vec3(0.25, 0.25, 0.3);                            // neutral cool
    }
    vec3 diffuse = mix(shadowTint, vec3(1.0), diff) * lightColor * lightIntensity;

    // Rim lighting (edge glow using view angle)
    float rimStrength = material.surfaceParam3; // rimLightStrength from push constants
    if (rimStrength > 0.0) {
        float rim = pow(1.0 - NdotV, 3.0) * rimStrength;
        diffuse += lightColor * lightIntensity * rim;
    }

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    uint flags = lighting.shadingFlags;

    // Fresnel
    vec3 F = ((flags & SHADING_FRESNEL) != 0u)
        ? fresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), F0)
        : F0;

    // Specular distribution: GGX or Blinn-Phong
    float spec;
    if ((flags & SHADING_GGX) != 0u) {
        float roughness = clamp(sqrt(2.0 / (shininess + 2.0)), 0.04, 1.0);
        spec = distributionGGX(NdotH, roughness);
        if ((flags & SHADING_GEOMETRY_TERM) != 0u) {
            spec *= geometrySmith(NdotV, NdotL, roughness);
        }
        spec /= max(4.0 * NdotV * NdotL, 0.001);
    } else {
        spec = pow(NdotH, shininess);
    }

    // Cel specular hard cutoff
    float cutoff = lighting.celSpecularCutoff;
    spec = (cutoff > 0.0 && spec > cutoff) ? 1.0 : ((cutoff > 0.0) ? 0.0 : spec);

    vec3 specular = spec * F * lightColor * lightIntensity;

    // Diffuse with optional energy conservation
    vec3 kD = vec3(1.0 - metallic);
    if ((flags & SHADING_ENERGY_CONSERV) != 0u) {
        kD *= (vec3(1.0) - F);
    }

    return diffuse * albedo * kD + specular;
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

// 8x8 Bayer dither matrix — finer grain than 4x4
float bayerDither8x8(ivec2 pos) {
    const int pattern[64] = int[64](
         0, 32,  8, 40,  2, 34, 10, 42,
        48, 16, 56, 24, 50, 18, 58, 26,
        12, 44,  4, 36, 14, 46,  6, 38,
        60, 28, 52, 20, 62, 30, 54, 22,
         3, 35, 11, 43,  1, 33,  9, 41,
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47,  7, 39, 13, 45,  5, 37,
        63, 31, 55, 23, 61, 29, 53, 21
    );
    int idx = (pos.x % 8) + (pos.y % 8) * 8;
    return float(pattern[idx]) / 64.0;
}

// Interleaved gradient noise — blue-noise-like spatial distribution
float blueNoiseDither(ivec2 pos) {
    return fract(52.9829189 * fract(0.06711056 * float(pos.x) + 0.00583715 * float(pos.y)));
}

// Halftone — circular dot pattern on a grid
float halftoneDither(ivec2 pos) {
    float scale = 6.0;
    vec2 p = mod(vec2(pos), scale) / scale - 0.5;
    return length(p) * 1.414;
}

// Crosshatch — diagonal line pattern
float crosshatchDither(ivec2 pos) {
    float s1 = mod(float(pos.x + pos.y), 8.0) / 8.0;
    float s2 = mod(float(pos.x - pos.y), 6.0) / 6.0;
    return min(s1, s2);
}

// Overlook — hexagonal geometric pattern inspired by The Shining carpet
float overlookDither(ivec2 pos) {
    vec2 p = vec2(pos) / 10.0;
    float a = mod(floor(p.x) + floor(p.y), 2.0);
    vec2 f = fract(p) - 0.5;
    float d = abs(f.x) + abs(f.y); // diamond distance
    return mix(d * 1.5, 1.0 - d * 1.5, a);
}

// Select dither threshold based on pattern index
float getDitherThreshold(ivec2 pos, int pattern) {
    if (pattern == 1) return bayerDither8x8(pos);
    if (pattern == 2) return blueNoiseDither(pos);
    if (pattern == 3) return halftoneDither(pos);
    if (pattern == 4) return crosshatchDither(pos);
    if (pattern == 5) return overlookDither(pos);
    return bayerDither4x4(pos); // default (pattern 0)
}

#ifdef CLUSTERED_LIGHTING
// Compute cluster index from screen position and view-space depth
uint getClusterIndex() {
    uint cx = min(uint(gl_FragCoord.x * float(CL_GRID_X) / clScreenWidth), CL_GRID_X - 1u);
    uint cy = min(uint(gl_FragCoord.y * float(CL_GRID_Y) / clScreenHeight), CL_GRID_Y - 1u);
    float depth = max(abs(fragViewDepth), clNearPlane);
    float logRatio = log2(clFarPlane / clNearPlane);
    uint cz = uint(log2(depth / clNearPlane) / logRatio * float(CL_GRID_Z));
    cz = min(cz, CL_GRID_Z - 1u);
    return cx + cy * CL_GRID_X + cz * CL_GRID_X * CL_GRID_Y;
}
#endif

#ifdef VIRTUAL_TEXTURING
// Sample through virtual texture indirection
vec4 sampleVirtualTexture(vec2 uv) {
    vec4 ind = texture(vtIndirectionTex, uv);
    if (ind.w < 0.5) return vec4(1.0); // Page not loaded — fallback white
    // ind.xy = physical page origin in atlas UV, ind.z = page size in atlas UV
    vec2 withinPage = fract(uv * vec2(textureSize(vtIndirectionTex, 0)));
    vec2 physicalUV = ind.xy + withinPage * ind.z;
    return texture(vtPhysicalAtlas, physicalUV);
}
#endif

void main() {
    // Resolve UV for affine texturing (undo the w-multiply from vertex shader)
    vec2 uv = fragUV / fragClipW;

    // PS1-style texture page warping: add UV discontinuities at VRAM page boundaries
    if (lighting.texturePageSize > 0.0 && (material.flags & FLAG_AFFINE_TEXTURING) != 0) {
        float pageSize = lighting.texturePageSize;
        // Compute distance to nearest page boundary (in UV space, assuming 256x256 VRAM)
        vec2 pageUV = uv * 256.0 / pageSize;
        vec2 pageFrac = fract(pageUV);
        // Create subtle seam at page edges: snap UVs near boundaries
        vec2 edgeDist = min(pageFrac, 1.0 - pageFrac);
        float edgeThreshold = 0.03;  // ~3% of page width
        vec2 warp = step(edgeDist, vec2(edgeThreshold)) * sign(pageFrac - 0.5) * 0.002;
        uv += warp;
    }

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

    // Normal quantization: snap normals to N cardinal directions (pixel art / 2D-in-3D look)
    float nqSteps = lighting.worldCurvature.z;
    if (nqSteps >= 4.0) {
        // Quantize each component to discrete steps
        float s = nqSteps;
        normal = normalize(floor(normal * s + 0.5) / s);
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
#ifdef VIRTUAL_TEXTURING
        vec4 texColor = sampleVirtualTexture(uv);
#else
        vec4 texColor = texture(baseColorTexture, uv);
#endif
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

    // Blend SH light probe irradiance when available
    if (lighting.shProbeIrradiance.w > 0.0) {
        result += lighting.shProbeIrradiance.xyz * albedo;
    }

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
                int ditherPattern = (material.flags & FLAG_SHADOW_DITHER_PATTERN_MASK) >> FLAG_SHADOW_DITHER_PATTERN_SHIFT;
                float ditherThreshold = getDitherThreshold(ivec2(gl_FragCoord.xy), ditherPattern);
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

        bool useCel = (lighting.celDiffuseBands >= 2.0) && ((material.flags & FLAG_EXCLUDE_CEL) == 0);
        result += shadow * (useCel
            ? calcBlinnPhongCel(lightDir, lightColor, intensity, normal, viewDir, albedo, metallic, shininess)
            : calcBlinnPhong(lightDir, lightColor, intensity, normal, viewDir, albedo, metallic, shininess));
    }

    // Process point and spot lights
#ifdef CLUSTERED_LIGHTING
    // Clustered forward lighting: only evaluate lights assigned to this fragment's cluster
    {
        uint clIdx = getClusterIndex();
        uint clOffset = clCells[clIdx].offset;
        uint clCount = clCells[clIdx].count;

        for (uint ci = 0u; ci < clCount; ++ci) {
            uint packed = clLightIndices[clOffset + ci];
            uint lightType = (packed >> 16u) & 1u;
            uint i = packed & 0xFFFFu;

            if (lightType == 0u) {
                // Point light
                vec3 lightPos = lighting.pointLights[i].position;
                vec3 lightVec = lightPos - fragWorldPos;
                float distance = length(lightVec);
                vec3 lightDir = normalize(lightVec);
                vec3 lightColor = lighting.pointLights[i].color;
                float intensity = lighting.pointLights[i].intensity;
                float atten = calcAttenuation(distance,
                    lighting.pointLights[i].constantAtten,
                    lighting.pointLights[i].linearAtten,
                    lighting.pointLights[i].quadraticAtten);
                float shadow = 1.0;
                if (int(i) < lighting.pointShadowCount && (material.flags & FLAG_RECEIVE_SHADOWS) != 0) {
                    shadow = calcPointShadow(fragWorldPos, int(i));
                }
                bool useCelPt = (lighting.celDiffuseBands >= 2.0) && ((material.flags & FLAG_EXCLUDE_CEL) == 0);
                result += shadow * (useCelPt
                    ? calcBlinnPhongCel(lightDir, lightColor, intensity * atten, normal, viewDir, albedo, metallic, shininess)
                    : calcBlinnPhong(lightDir, lightColor, intensity * atten, normal, viewDir, albedo, metallic, shininess));
            } else {
                // Spot light
                vec3 lightPos = lighting.spotLights[i].position;
                vec3 lightVec = lightPos - fragWorldPos;
                float distance = length(lightVec);
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
                    float shadow = 1.0;
                    if (int(i) < lighting.spotShadowCount && (material.flags & FLAG_RECEIVE_SHADOWS) != 0) {
                        shadow = calcSpotShadow(fragWorldPos, int(i));
                    }
                    bool useCelSp = (lighting.celDiffuseBands >= 2.0) && ((material.flags & FLAG_EXCLUDE_CEL) == 0);
                    result += shadow * (useCelSp
                        ? calcBlinnPhongCel(lightDir, lightColor, intensity * atten * spotIntensity, normal, viewDir, albedo, metallic, shininess)
                        : calcBlinnPhong(lightDir, lightColor, intensity * atten * spotIntensity, normal, viewDir, albedo, metallic, shininess));
                }
            }
        }
    }
#else
    // Brute-force point lights
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

        bool useCelPt = (lighting.celDiffuseBands >= 2.0) && ((material.flags & FLAG_EXCLUDE_CEL) == 0);
        result += shadow * (useCelPt
            ? calcBlinnPhongCel(lightDir, lightColor, intensity * atten, normal, viewDir, albedo, metallic, shininess)
            : calcBlinnPhong(lightDir, lightColor, intensity * atten, normal, viewDir, albedo, metallic, shininess));
    }

    // Brute-force spot lights
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

            bool useCelSp = (lighting.celDiffuseBands >= 2.0) && ((material.flags & FLAG_EXCLUDE_CEL) == 0);
            result += shadow * (useCelSp
                ? calcBlinnPhongCel(lightDir, lightColor, intensity * atten * spotIntensity, normal, viewDir, albedo, metallic, shininess)
                : calcBlinnPhong(lightDir, lightColor, intensity * atten * spotIntensity, normal, viewDir, albedo, metallic, shininess));
        }
    }
#endif

    // Subsurface scattering approximation (wrap lighting)
    // Uses extended material data from MaterialSSBO (binding 2, dynamic offset per entity)
    if (materialData.matSSSIntensity > 0.0) {
        float sssInt = materialData.matSSSIntensity;
        float sssRad = materialData.matSSSRadius;
        vec3  sssCol = materialData.matSSSColor;

        vec3 sssContrib = vec3(0.0);

        // Directional lights: wrap diffuse on the back side
        for (uint i = 0u; i < lighting.directionalLightCount && i < MAX_DIRECTIONAL_LIGHTS; ++i) {
            vec3 L = -normalize(lighting.directionalLights[i].direction);
            vec3 lc = lighting.directionalLights[i].color * lighting.directionalLights[i].intensity;
            float wrap = max(0.0, dot(normal, L) + sssRad) / (1.0 + sssRad);
            sssContrib += wrap * lc;
        }

        // Point lights: wrap diffuse with attenuation
        for (uint i = 0u; i < lighting.pointLightCount && i < MAX_POINT_LIGHTS; ++i) {
            vec3 lv = lighting.pointLights[i].position - fragWorldPos;
            float dist = length(lv);
            if (dist > lighting.pointLights[i].range) continue;
            vec3 L = normalize(lv);
            float atten = calcAttenuation(dist,
                lighting.pointLights[i].constantAtten,
                lighting.pointLights[i].linearAtten,
                lighting.pointLights[i].quadraticAtten);
            vec3 lc = lighting.pointLights[i].color * lighting.pointLights[i].intensity * atten;
            float wrap = max(0.0, dot(normal, L) + sssRad) / (1.0 + sssRad);
            sssContrib += wrap * lc;
        }

        // Spot lights: wrap diffuse with attenuation and cone falloff
        for (uint i = 0u; i < lighting.spotLightCount && i < MAX_SPOT_LIGHTS; ++i) {
            vec3 lv = lighting.spotLights[i].position - fragWorldPos;
            float dist = length(lv);
            if (dist > lighting.spotLights[i].range) continue;
            vec3 L = normalize(lv);
            vec3 spotDir = normalize(lighting.spotLights[i].direction);
            float theta = dot(L, -spotDir);
            float spotFalloff = clamp((theta - lighting.spotLights[i].outerCutoff)
                / (lighting.spotLights[i].innerCutoff - lighting.spotLights[i].outerCutoff), 0.0, 1.0);
            if (spotFalloff <= 0.0) continue;
            float atten = calcAttenuation(dist,
                lighting.spotLights[i].constantAtten,
                lighting.spotLights[i].linearAtten,
                lighting.spotLights[i].quadraticAtten);
            vec3 lc = lighting.spotLights[i].color * lighting.spotLights[i].intensity * atten * spotFalloff;
            float wrap = max(0.0, dot(normal, L) + sssRad) / (1.0 + sssRad);
            sssContrib += wrap * lc;
        }

        result += sssContrib * sssCol * albedo * sssInt;
    }

    // Dithered transparency: alternating pixels between fragment color and blend color
    // Encoded in surfaceParam1 >= 200: pattern in fractional part, opacity in surfaceParam2,
    // blend color packed as 10-bit RGB in surfaceParam3
    if (material.surfaceParam1 >= 199.5 && material.surfaceParam1 < 300.0) {
        float encoded = material.surfaceParam1 - 200.0;
        int dtPattern = int(floor(encoded + 0.5)); // 0=Checkerboard, 1=HStripe, 2=VStripe, 3=Bayer2x2
        float dtOpacity = material.surfaceParam2;

        // Unpack blend color from 10-bit packed float
        uint packedColor = floatBitsToUint(material.surfaceParam3);
        vec3 blendColor = vec3(
            float((packedColor >> 20) & 0x3FFu) / 1023.0,
            float((packedColor >> 10) & 0x3FFu) / 1023.0,
            float(packedColor & 0x3FFu) / 1023.0
        );

        // Generate pattern based on screen-space pixel coordinates
        ivec2 dtPos = ivec2(gl_FragCoord.xy);
        bool useBlend = false;

        if (dtPattern == 0) // Checkerboard
            useBlend = ((dtPos.x + dtPos.y) % 2) == 1;
        else if (dtPattern == 1) // H-Stripe
            useBlend = (dtPos.y % 2) == 1;
        else if (dtPattern == 2) // V-Stripe
            useBlend = (dtPos.x % 2) == 1;
        else // Bayer 2x2
        {
            int bx = dtPos.x % 2;
            int by = dtPos.y % 2;
            int val = bx + by * 2;
            useBlend = val >= 2;
        }

        if (useBlend)
            result = mix(blendColor, result, dtOpacity);
    }

    // Dithered gradient: quantize lighting into bands with dither transitions
    // Encoded in surfaceParam1 when > 1.0: surfaceParam1 = 100 + bands + pattern * 0.1
    bool isDitherGradient = false;
    if ((material.flags & FLAG_FLAT_SHADING) != 0 && material.surfaceParam1 > 1.5) {
        isDitherGradient = true;
        float encoded = material.surfaceParam1 - 100.0;
        float bands = floor(encoded);  // 2-8
        int pattern = int(floor((encoded - bands) * 10.0 + 0.5));  // 0-5

        // Compute luminance of the lit result
        float lum = dot(result, vec3(0.2126, 0.7152, 0.0722));

        // Quantize to discrete bands
        float quantized = floor(lum * bands + 0.5) / bands;
        float nextBand = min(quantized + 1.0 / bands, 1.0);

        // Dither at band boundaries: interpolate between adjacent bands
        float t = fract(lum * bands);  // position within current band (0-1)
        float dither = getDitherThreshold(ivec2(gl_FragCoord.xy), pattern);

        // Select band based on dither threshold
        float finalLum = (t > dither) ? nextBand : quantized;

        // Apply the quantized luminance while preserving hue
        if (lum > 0.001) {
            result *= finalLum / lum;
        } else {
            result = vec3(finalLum);
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
        if ((material.flags & FLAG_WATER_SHORE) != 0 && material.surfaceParam2 > 0.0) {
            float edgeDist = fragVertColor.g;  // 0=edge, 1=center
            float shoreW = material.surfaceParam1;

            // Shallow water color blend near edges
            float shallowBlend = 1.0 - smoothstep(0.0, shoreW * 2.0, edgeDist);
            // shoreColor is approximated from baseColor lightened — we use fragVertColor.r channel
            // for the shore color R and use a brighter tint of the water
            vec3 shoreColor = material.baseColor * 1.5 + vec3(0.1, 0.15, 0.1);
            result = mix(result, shoreColor, shallowBlend * 0.4);

            // Multi-octave hash noise for foam pattern
            float foamSc = material.surfaceParam3;
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
            float foam = smoothstep(0.35, 0.65, noise) * foamThreshold * material.surfaceParam2;

            // Blend white foam into result
            result = mix(result, vec3(0.9, 0.95, 1.0), foam);
        }
    }

    // Artistic surface: environment reflection + rim light (non-water, non-dither-gradient entities)
    if ((material.flags & FLAG_WATER_SURFACE) == 0 && !isDitherGradient &&
        (material.surfaceParam1 > 0.0 || material.surfaceParam3 > 0.0)) {
        float reflectivity = material.surfaceParam1;
        float fresnelPow   = material.surfaceParam2;
        float rimStrength   = material.surfaceParam3;

        float NdotV = max(dot(normal, viewDir), 0.0);
        float fresnel = reflectivity + (1.0 - reflectivity) * pow(1.0 - NdotV, fresnelPow);

        // Fake environment: gradient from dark ground to sky color via reflected direction
        vec3 reflectDir = reflect(-viewDir, normal);
        vec3 skyCol = lighting.skyReflectColor.xyz;
        vec3 envColor = mix(skyCol * 0.15, skyCol, clamp(reflectDir.y * 0.5 + 0.5, 0.0, 1.0));

        // Add specular highlight from directional lights
        for (int i = 0; i < int(lighting.directionalLightCount); i++) {
            float spec = pow(max(dot(reflectDir, normalize(lighting.directionalLights[i].direction)), 0.0), 64.0);
            envColor += lighting.directionalLights[i].color * lighting.directionalLights[i].intensity * spec * 0.5;
        }

        if (reflectivity > 0.0)
            result = mix(result, envColor, fresnel);

        // Additive rim light
        if (rimStrength > 0.0) {
            float rim = pow(1.0 - NdotV, fresnelPow) * rimStrength;
            result += rim * skyCol;
        }
    }

    // === ELEMENTAL SURFACE EFFECTS (PS1/PS2 style) ===
    if (material.surfaceParam1 >= 299.5 && material.surfaceParam1 < 400.0) {
        float charAmount = material.surfaceParam1 - 300.0;
        float wetness = fract(material.surfaceParam2);
        float snowCov = floor(material.surfaceParam2) / 256.0;
        float frost = material.surfaceParam3;

        // Fire charring: darken toward black/brown, ember glow pulses
        if (charAmount > 0.01) {
            vec3 charColor = vec3(0.08, 0.04, 0.02);
            result = mix(result, charColor, charAmount * 0.8);
            // Ember glow: pulsing orange in charred areas (PS2 lava-flow style)
            float ember = sin(lighting.windData.w * 4.0 + fragWorldPos.x * 3.0) * 0.5 + 0.5;
            result += vec3(1.0, 0.3, 0.0) * charAmount * ember * 0.15;
        }

        // Wetness: darken + subtle color shift (PS1 dark-when-wet trick)
        if (wetness > 0.01) {
            result *= mix(1.0, 0.6, wetness);
            result = mix(result, result * vec3(0.9, 0.9, 1.0), wetness * 0.3);
        }

        // Snow coverage: blend to white on upward-facing surfaces
        if (snowCov > 0.01) {
            float snowMask = smoothstep(0.3, 0.8, normal.y) * snowCov;
            // Dithered snow edge (very PS1)
            float dither = fract(sin(dot(floor(gl_FragCoord.xy / 2.0), vec2(12.9898, 78.233))) * 43758.5453);
            snowMask = step(dither, snowMask + 0.1) * snowCov;
            result = mix(result, vec3(0.93, 0.96, 1.0), snowMask);
        }

        // Frost: blue-white tint with dithered crystal pattern
        if (frost > 0.01) {
            float crystal = fract(sin(dot(fragUV * 16.0, vec2(12.9898, 78.233))) * 43758.5453);
            float frostPattern = step(crystal, frost);
            result = mix(result, vec3(0.75, 0.88, 1.0), frostPattern * 0.5);
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

    // Spherical environment mapping (Dreamcast-style matcap sheen)
    if ((lighting.shadingFlags & SHADING_SPHERE_ENV) != 0u && lighting.sphereEnvStrength > 0.0) {
        // Reconstruct view-space normal without view matrix
        vec3 vForward = normalize(lighting.cameraPos - fragWorldPos);
        vec3 vRight = normalize(cross(vec3(0.0, 1.0, 0.0), vForward));
        vec3 vUp = cross(vForward, vRight);
        vec2 matcapUV = vec2(dot(normal, vRight), dot(normal, vUp)) * 0.5 + 0.5;

        // Procedural matcap: metallic highlight ring + top-down gradient
        float rim = length(matcapUV - vec2(0.5));
        float highlight = smoothstep(0.45, 0.25, rim);   // bright center
        float ring = smoothstep(0.3, 0.48, rim) * smoothstep(0.5, 0.48, rim); // bright ring at edge
        float grad = matcapUV.y * 0.6 + 0.2;             // sky-ground gradient

        vec3 envColor = vec3(0.25, 0.28, 0.35) * grad     // base ambient gradient
                      + vec3(0.6, 0.65, 0.7) * highlight   // center specular
                      + vec3(0.4, 0.45, 0.5) * ring;       // edge ring (Dreamcast sheen)

        // Blend: metallic surfaces get full effect, dielectrics get subtle rim only
        float envBlend = mix(ring * 0.3, 1.0, metallic) * lighting.sphereEnvStrength;
        result = mix(result, result + envColor, envBlend);
    }

    // Gamma correction
    result = pow(result, vec3(1.0 / 2.2));

    // Color posterization (Dreamcast VQ-style banding)
    if (lighting.posterizeLevels > 1.5) {
        float levels = lighting.posterizeLevels;
        result = floor(result * levels + 0.5) / levels;
    }

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

    // Per-pixel velocity: screen-space motion vector for TAA / temporal upscaling
    vec2 curNDC  = fragCurClipPos.xy / fragCurClipPos.w;
    vec2 prevNDC = fragPrevClipPos.xy / fragPrevClipPos.w;
    outVelocity  = (curNDC - prevNDC) * 0.5;  // NDC is [-1,1], scale to [-0.5, 0.5] range
}
