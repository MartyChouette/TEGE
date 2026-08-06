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

// ============================================================================
// Real triangle normals (binding 10 + buffer device addresses)
//
// Opt-in: define RT_USE_INSTANCE_GEOM before including this file, AND declare
//   #extension GL_EXT_buffer_reference : require
//   #extension GL_EXT_buffer_reference_uvec2 : require
// Closest-hit stages only (uses gl_InstanceCustomIndexEXT / gl_PrimitiveID).
// ============================================================================
#ifdef RT_USE_INSTANCE_GEOM

// Matches RTInstanceGeomGPU in RenderSystem::RebuildTLAS (32 bytes, std430)
struct RTInstanceGeom {
    uvec2 vertexAddr;         // u64 device address of the mesh's first vertex (lo, hi)
    uvec2 indexAddr;          // u64 device address of the mesh's first index (u32)
    uint strideFloats;        // Vertex stride in floats (0 = no entry, use fallback)
    uint normalOffsetFloats;  // Offset of the normal within a vertex, in floats
    uint _pad0;
    uint _pad1;
};

layout(binding = 10, set = 0) readonly buffer InstanceGeomBuffer {
    RTInstanceGeom instanceGeoms[];
};

layout(buffer_reference, buffer_reference_align = 4, std430) readonly buffer RTGeomFloats { float f[]; };
layout(buffer_reference, buffer_reference_align = 4, std430) readonly buffer RTGeomIndices { uint i[]; };

// Interpolated object-space vertex normal for the current hit triangle.
// Vertex formats without normals (vegetation templates: pos3 + uv2, encoded as
// normalOffsetFloats >= strideFloats) use the face normal from positions.
// Degenerate vertex normals also fall back to the face normal, and instances
// with no geometry entry fall back to object-space +Y.
vec3 rtHitObjectNormal(vec2 baryAttribs) {
    RTInstanceGeom geom = instanceGeoms[gl_InstanceCustomIndexEXT];
    if (geom.strideFloats == 0u) return vec3(0.0, 1.0, 0.0);

    RTGeomIndices ib = RTGeomIndices(geom.indexAddr);
    uint base = uint(gl_PrimitiveID) * 3u;
    uint i0 = ib.i[base + 0u];
    uint i1 = ib.i[base + 1u];
    uint i2 = ib.i[base + 2u];

    RTGeomFloats vb = RTGeomFloats(geom.vertexAddr);
    uint s = geom.strideFloats;
    uint n = geom.normalOffsetFloats;

    if (n + 3u <= s) {
        vec3 n0 = vec3(vb.f[i0 * s + n], vb.f[i0 * s + n + 1u], vb.f[i0 * s + n + 2u]);
        vec3 n1 = vec3(vb.f[i1 * s + n], vb.f[i1 * s + n + 1u], vb.f[i1 * s + n + 2u]);
        vec3 n2 = vec3(vb.f[i2 * s + n], vb.f[i2 * s + n + 1u], vb.f[i2 * s + n + 2u]);

        vec3 bary = vec3(1.0 - baryAttribs.x - baryAttribs.y, baryAttribs.x, baryAttribs.y);
        vec3 objNormal = n0 * bary.x + n1 * bary.y + n2 * bary.z;
        if (dot(objNormal, objNormal) >= 1e-8) return objNormal;
    }

    // Face normal from positions (no-normal vertex formats, degenerate normals)
    vec3 p0 = vec3(vb.f[i0 * s], vb.f[i0 * s + 1u], vb.f[i0 * s + 2u]);
    vec3 p1 = vec3(vb.f[i1 * s], vb.f[i1 * s + 1u], vb.f[i1 * s + 2u]);
    vec3 p2 = vec3(vb.f[i2 * s], vb.f[i2 * s + 1u], vb.f[i2 * s + 2u]);
    return cross(p1 - p0, p2 - p0);
}

// World-space hit normal: inverse-transpose transform (vec * WorldToObject ==
// transpose(WorldToObject) * vec), normalized, flipped to face the incoming ray.
vec3 rtHitWorldNormal(vec2 baryAttribs) {
    vec3 objNormal = rtHitObjectNormal(baryAttribs);
    vec3 worldNormal = normalize(objNormal * mat3(gl_WorldToObjectEXT));
    if (dot(worldNormal, gl_WorldRayDirectionEXT) > 0.0) worldNormal = -worldNormal;
    return worldNormal;
}

#endif // RT_USE_INSTANCE_GEOM

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
// Simplified material (pre-baked on CPU to reduce hit shader divergence)
// ============================================================================

// Pre-computed material properties for RT shaders. CPU computes F0, kDiffuse,
// effectiveRoughness, and pre-multiplied emissive so hit shaders skip expensive
// per-ray material calculations (especially for secondary bounces).
struct RTSimplifiedMaterial {
    vec3 albedo;         float effectiveRoughness;  // 16 bytes
    vec3 f0;             float kDiffuse;             // 16 bytes
    vec3 emissive;       float opacity;              // 16 bytes
    float transmission;  float ior;  float _pad0;  float _pad1; // 16 bytes
};  // 64 bytes total

// Per-entity simplified material SSBO (binding 18, pre-baked on CPU)
layout(binding = 18, set = 0) readonly buffer SimplifiedMaterialBuffer {
    RTSimplifiedMaterial simplifiedMaterials[];
};

// Fetch pre-baked simplified material for the hit instance
RTSimplifiedMaterial fetchSimplifiedMaterial(uint instanceIndex) {
    return simplifiedMaterials[instanceIndex];
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

// ============================================================================
// SDF (Signed Distance Field) evaluation for RT fallback
// ============================================================================

// GPU-packed SDF object (48 bytes, matches C++ SDFObjectGPU)
struct SDFObjectGPU {
    vec3 position;       // World-space center
    float type;          // Primitive type (0=Sphere, 1=Box, 2=Cylinder, 3=Torus, 4=Plane, 5=RoundedBox)
    vec3 scale;          // Per-axis scale / half-extents
    float blendMode;     // Boolean op (0=Union, 1=Subtract, 2=Intersect, 3-5=Smooth variants)
    vec3 color;          // Albedo color
    float blendSmoothness; // Smooth blend radius
};

// Evaluate a single SDF primitive at local-space position p
float sdfPrimitive(vec3 p, float type, vec3 scale) {
    int t = int(type + 0.5);

    if (t == 0) {
        // Sphere: radius = scale.x
        return length(p) - scale.x;
    } else if (t == 1) {
        // Box: half-extents = scale
        vec3 d = abs(p) - scale;
        return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
    } else if (t == 2) {
        // Cylinder: radius = scale.x, half-height = scale.y
        float dx = length(p.xz) - scale.x;
        float dy = abs(p.y) - scale.y;
        return length(max(vec2(dx, dy), 0.0)) + min(max(dx, dy), 0.0);
    } else if (t == 3) {
        // Torus: major = scale.x, minor = scale.y
        float qx = length(p.xz) - scale.x;
        return length(vec2(qx, p.y)) - scale.y;
    } else if (t == 4) {
        // Plane: Y-up at origin
        return p.y;
    } else if (t == 5) {
        // Rounded box: half-extents = scale, corner radius = 0.05 (baked)
        float r = 0.05;
        vec3 d = abs(p) - scale + r;
        return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0) - r;
    }
    return 1e10;
}

// SDF boolean operations
float sdfOpUnion(float a, float b) { return min(a, b); }
float sdfOpSubtract(float a, float b) { return max(a, -b); }
float sdfOpIntersect(float a, float b) { return max(a, b); }

float sdfOpSmoothUnion(float a, float b, float k) {
    if (k <= 0.0) return min(a, b);
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25;
}

float sdfOpSmoothSubtract(float a, float b, float k) {
    if (k <= 0.0) return max(a, -b);
    float h = max(k - abs(a + b), 0.0) / k;
    return max(a, -b) + h * h * k * 0.25;
}

float sdfOpSmoothIntersect(float a, float b, float k) {
    if (k <= 0.0) return max(a, b);
    float h = max(k - abs(a - b), 0.0) / k;
    return max(a, b) + h * h * k * 0.25;
}

// NOTE: GLSL cannot pass unsized arrays as function parameters, so there is no
// shared sdfEvaluateScene/sdfSphereTrace helper here. Each rgen shader that
// supports the SDF fallback inlines the scene loop over its own SDF SSBO
// (see rt_reflect.rgen), built from sdfPrimitive and the sdfOp* functions above.

#endif // RT_COMMON_GLSL
