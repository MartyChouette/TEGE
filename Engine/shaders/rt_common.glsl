// rt_common.glsl — Shared structs, RNG, BRDF, and sampling for RT shaders
//
// NOTE: This file must NOT contain layout(binding=...) declarations for UBOs/SSBOs.
// Each shader (.rgen, .rchit, .rmiss) declares its own bindings so they do not
// conflict when included across different pipeline stages.
// The ONE exception is the MaterialSSBO at binding 9, which is used by all
// closest-hit shaders and is safe to declare here.

#ifndef RT_COMMON_GLSL
#define RT_COMMON_GLSL

const float RT_PI = 3.14159265359;
const float RT_INV_PI = 0.31830988618;
const float RT_EPSILON = 1e-7;

// ============================================================================
// RNG (PCG)
// ============================================================================

uint pcg_hash(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand_float(inout uint seed) {
    seed = pcg_hash(seed);
    return float(seed) / 4294967296.0;
}

vec2 rand_vec2(inout uint seed) {
    return vec2(rand_float(seed), rand_float(seed));
}

uint init_rng(uvec2 pixel, uint frame) {
    return pcg_hash(pixel.x + pixel.y * 8192u + frame * 65536u);
}

// ============================================================================
// Sampling
// ============================================================================

// Cosine-weighted hemisphere sampling (returns direction in tangent space, z-up)
vec3 cosine_hemisphere_sample(vec2 u) {
    float r = sqrt(u.x);
    float theta = 2.0 * RT_PI * u.y;
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(max(0.0, 1.0 - u.x));
    return vec3(x, y, z);
}

// PDF for cosine-weighted hemisphere sampling
float cosine_hemisphere_pdf(float cosTheta) {
    return max(cosTheta * RT_INV_PI, RT_EPSILON);
}

// Uniform hemisphere sampling
vec3 uniform_hemisphere_sample(vec2 u) {
    float z = u.x;
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = 2.0 * RT_PI * u.y;
    return vec3(r * cos(phi), r * sin(phi), z);
}

// Build orthonormal basis from normal (Frisvad-style, robust for all normals)
void build_onb(vec3 N, out vec3 T, out vec3 B) {
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

// Transform direction from tangent space to world space
vec3 tangent_to_world(vec3 dir, vec3 N, vec3 T, vec3 B) {
    return dir.x * T + dir.y * B + dir.z * N;
}

// ============================================================================
// Material data
// ============================================================================

struct RTMaterial {
    vec3 baseColor;
    float metallic;
    vec3 emissive;
    float roughness;

    // Transmission / SSS
    float transmission;     // 0=opaque, 1=fully transmissive
    float ior;              // Index of refraction (1.0=vacuum, 1.5=glass)
    float thickness;        // Thin-surface thickness for translucency falloff
    float sssIntensity;     // Subsurface scattering strength

    vec3 sssColor;          // SSS scatter tint
    float sssRadius;        // SSS scatter radius in world units
};

// GPU material struct matching C++ MaterialGPU (std430 layout, 80 bytes)
struct MaterialGPU {
    vec3 baseColor;
    float metallic;
    vec3 emissiveColor;
    float roughness;
    float emissiveStrength;
    float opacity;
    float alphaCutoff;
    int flags;
    float transmission;
    float ior;
    float thickness;
    float sssIntensity;
    vec3 sssColor;
    float sssRadius;
};

// Per-entity material data uploaded from the CPU (binding 9)
layout(binding = 9, set = 0) readonly buffer MaterialSSBO {
    MaterialGPU materials[];
};

// Fetch material for the hit instance and convert to RTMaterial
RTMaterial fetchMaterial(uint instanceIndex) {
    MaterialGPU m = materials[instanceIndex];
    RTMaterial mat;
    mat.baseColor = m.baseColor;
    mat.metallic = m.metallic;
    mat.emissive = m.emissiveColor * m.emissiveStrength;
    mat.roughness = max(m.roughness, 0.04); // Clamp to avoid singularities
    mat.transmission = m.transmission;
    mat.ior = m.ior;
    mat.thickness = m.thickness;
    mat.sssIntensity = m.sssIntensity;
    mat.sssColor = m.sssColor;
    mat.sssRadius = m.sssRadius;
    return mat;
}

// ============================================================================
// Light data for path tracing
// ============================================================================

// Extended light struct with spot light cutoff angles (64 bytes)
struct RTLight {
    vec3 position;     // World position (point/spot) or unused (directional)
    float range;       // Attenuation range (point/spot), 0 = infinite (directional)
    vec3 direction;    // Light direction (directional/spot)
    float intensity;   // Light intensity multiplier
    vec3 color;        // RGB light color
    int type;          // 0=directional, 1=point, 2=spot
    float innerCutoff; // cos(innerConeAngle) for spot lights
    float outerCutoff; // cos(outerConeAngle) for spot lights
    float _pad0;
    float _pad1;
};

// Maximum lights supported in path tracer
#define RT_MAX_LIGHTS 32

// ============================================================================
// Firefly clamping utility
// ============================================================================

// Clamp radiance to suppress fireflies (luminance-based)
vec3 clampRadiance(vec3 radiance, float maxValue) {
    if (maxValue <= 0.0) return radiance;
    float lum = dot(radiance, vec3(0.2126, 0.7152, 0.0722));
    if (lum > maxValue) {
        radiance *= maxValue / lum;
    }
    return radiance;
}

// ============================================================================
// Russian Roulette utility
// ============================================================================

// Returns survival probability (0 = terminate path)
float russianRoulette(vec3 throughput, uint bounceIndex, uint minBounce, float minProb) {
    if (bounceIndex < minBounce) return 1.0;
    float lum = max(dot(throughput, vec3(0.2126, 0.7152, 0.0722)), minProb);
    return min(lum, 1.0);
}

// ============================================================================
// Cook-Torrance BRDF
// ============================================================================

// Fresnel-Schlick approximation
vec3 fresnel_schlick(float cosTheta, vec3 F0) {
    float t = 1.0 - cosTheta;
    float t2 = t * t;
    return F0 + (1.0 - F0) * (t2 * t2 * t); // t^5
}

// GGX/Trowbridge-Reitz normal distribution function
float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = RT_PI * denom * denom;
    return a2 / max(denom, RT_EPSILON);
}

// Smith's geometry function (Schlick-GGX)
float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometry_schlick_ggx(NdotV, roughness) * geometry_schlick_ggx(NdotL, roughness);
}

// Evaluate Cook-Torrance BRDF
// Returns: vec3 BRDF value (diffuse + specular)
vec3 eval_cook_torrance(RTMaterial mat, vec3 N, vec3 V, vec3 L) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    if (NdotL <= 0.0 || NdotV <= 0.0) return vec3(0.0);

    vec3 H = normalize(V + L);

    // F0: reflectance at normal incidence
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, mat.baseColor, mat.metallic);

    // Specular (Cook-Torrance)
    float D = distribution_ggx(N, H, mat.roughness);
    float G = geometry_smith(N, V, L, mat.roughness);
    vec3  F = fresnel_schlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL;
    vec3 specular = numerator / max(denominator, RT_EPSILON);

    // Diffuse (Lambertian)
    vec3 kD = (1.0 - F) * (1.0 - mat.metallic);
    vec3 diffuse = kD * mat.baseColor * RT_INV_PI;

    return diffuse + specular;
}

// ============================================================================
// GGX importance sampling
// ============================================================================

// Sample GGX distribution: returns half-vector in tangent space
vec3 sample_ggx_tangent(vec2 u, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;

    float cosTheta = sqrt((1.0 - u.x) / (1.0 + (a2 - 1.0) * u.x));
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    float phi = 2.0 * RT_PI * u.y;

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// PDF for GGX sampling (in terms of reflected direction L, not half-vector H)
float pdf_ggx_reflection(float NdotH, float HdotV, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float cosTheta = NdotH;
    float cos2 = cosTheta * cosTheta;
    float denom = cos2 * (a2 - 1.0) + 1.0;
    denom = RT_PI * denom * denom;
    float D_val = a2 / max(denom, RT_EPSILON);

    // Jacobian of reflection: |dH/dL| = 1 / (4 * HdotV)
    return D_val * cosTheta / max(4.0 * HdotV, RT_EPSILON);
}

// Sample a BRDF direction and return the direction, PDF, and whether it's specular
// Returns: sampled world-space direction L
// Out: pdf - sampling PDF, isSpecular - true if GGX sample was chosen
vec3 sample_brdf(RTMaterial mat, vec3 N, vec3 V, inout uint rng,
                 out float pdf, out bool isSpecular) {
    vec3 T, B;
    build_onb(N, T, B);

    // Decide between diffuse and specular sampling based on metallic + roughness
    // Higher metallic or lower roughness -> more specular sampling
    float specProb = mix(0.5, 1.0, mat.metallic);
    // For very rough surfaces, favor diffuse sampling
    specProb = mix(specProb, 0.3, mat.roughness * mat.roughness * (1.0 - mat.metallic));
    specProb = clamp(specProb, 0.1, 0.9);

    vec2 u = rand_vec2(rng);
    float choice = rand_float(rng);

    if (choice < specProb) {
        // GGX specular sampling
        isSpecular = true;
        vec3 H_tangent = sample_ggx_tangent(u, mat.roughness);
        vec3 H = tangent_to_world(H_tangent, N, T, B);
        vec3 L = reflect(-V, H);

        if (dot(L, N) <= 0.0) {
            pdf = 1.0; // Will be rejected by caller
            return L;
        }

        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);
        float specPdf = pdf_ggx_reflection(NdotH, HdotV, mat.roughness);

        // Cosine hemisphere PDF for the diffuse lobe
        float NdotL = max(dot(N, L), 0.0);
        float diffPdf = cosine_hemisphere_pdf(NdotL);

        // Combined PDF (mixture)
        pdf = specProb * specPdf + (1.0 - specProb) * diffPdf;
        return L;
    } else {
        // Cosine-weighted diffuse sampling
        isSpecular = false;
        vec3 localDir = cosine_hemisphere_sample(u);
        vec3 L = tangent_to_world(localDir, N, T, B);

        float NdotL = max(dot(N, L), 0.0);
        float diffPdf = cosine_hemisphere_pdf(NdotL);

        // GGX PDF for this direction
        vec3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);
        float specPdf = pdf_ggx_reflection(NdotH, HdotV, mat.roughness);

        // Combined PDF (mixture)
        pdf = specProb * specPdf + (1.0 - specProb) * diffPdf;
        return L;
    }
}

// ============================================================================
// MIS (Multiple Importance Sampling) - Power heuristic
// ============================================================================

float mis_power_heuristic(float pdfA, float pdfB) {
    float a2 = pdfA * pdfA;
    float b2 = pdfB * pdfB;
    return a2 / max(a2 + b2, RT_EPSILON);
}

// ============================================================================
// Light sampling utilities
// ============================================================================

// Sample a point on a directional light (treat as distant disk)
vec3 sample_directional_light(RTLight light, out vec3 lightDir, out float lightDist,
                               out float lightPdf) {
    lightDir = -light.direction;
    lightDist = 10000.0;  // Far away
    lightPdf = 1.0;       // Delta distribution
    return light.color * light.intensity;
}

// Sample a point light
vec3 sample_point_light(RTLight light, vec3 hitPos, out vec3 lightDir, out float lightDist,
                         out float lightPdf) {
    vec3 toLight = light.position - hitPos;
    lightDist = length(toLight);
    lightDir = toLight / max(lightDist, RT_EPSILON);

    // Inverse-square attenuation with range cutoff
    float attenuation = 1.0 / max(lightDist * lightDist, RT_EPSILON);
    if (light.range > 0.0 && lightDist > light.range) attenuation = 0.0;

    lightPdf = 1.0;  // Delta distribution (point source)
    return light.color * light.intensity * attenuation;
}

// Sample a spot light
vec3 sample_spot_light(RTLight light, vec3 hitPos, out vec3 lightDir, out float lightDist,
                        out float lightPdf) {
    vec3 toLight = light.position - hitPos;
    lightDist = length(toLight);
    lightDir = toLight / max(lightDist, RT_EPSILON);

    float attenuation = 1.0 / max(lightDist * lightDist, RT_EPSILON);
    if (light.range > 0.0 && lightDist > light.range) attenuation = 0.0;

    // Spot cone falloff
    float cosAngle = dot(-lightDir, light.direction);
    float spotFactor = 0.0;
    if (cosAngle > light.outerCutoff) {
        if (cosAngle > light.innerCutoff) {
            spotFactor = 1.0;
        } else {
            spotFactor = (cosAngle - light.outerCutoff) /
                         max(light.innerCutoff - light.outerCutoff, RT_EPSILON);
            spotFactor = spotFactor * spotFactor; // Smooth falloff
        }
    }

    lightPdf = 1.0;
    return light.color * light.intensity * attenuation * spotFactor;
}

#endif // RT_COMMON_GLSL
