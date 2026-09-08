#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Renderer/LightCookie.h"

#include <string>

namespace Enjin {
namespace ECS {

// Light types
enum class LightType : u32 {
    Directional = 0,
    Point = 1,
    Spot = 2
};

// Light component - attach to entities to create light sources
struct ENJIN_API LightComponent {
    LightType type = LightType::Point;

    // Color and intensity
    Math::Vector3 color = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 intensity = 1.0f;

    // Point/Spot light attenuation
    f32 range = 10.0f;           // Maximum range
    f32 constantAttenuation = 1.0f;
    f32 linearAttenuation = 0.09f;
    f32 quadraticAttenuation = 0.032f;

    // Spot light specific
    f32 innerConeAngle = 12.5f;  // Degrees
    f32 outerConeAngle = 17.5f;  // Degrees

    // Shadow casting
    bool castShadows = true;

    // --- Cookie (gobo): a shape in the light's projection ---
    // Window mullions, blinds, leaf dapple. Spot lights only for now, because a
    // cookie needs a cone to project through.
    //
    // The RECIPE is stored, not just the image, so the cookie stays re-editable
    // after a save and can be regenerated instead of shipping a texture. A baked
    // path is optional and wins when set, which is how a hand-painted cookie gets
    // in without the generator having to be able to draw it.
    bool cookieEnabled = false;
    Renderer::CookieParams cookie;
    std::string cookieTexturePath;   // project-relative; empty = generate from `cookie`

    // How much of the cone the cookie covers. 1 = the pattern exactly fills the
    // outer cone; smaller zooms in, larger tiles it out toward the edge.
    f32 cookieScale = 1.0f;
    // Blend between the plain light (0) and the full cookie (1). Not the same as
    // dimming the light: at 0 the light is unchanged, not off.
    f32 cookieIntensity = 1.0f;

    // Helper to calculate attenuation at a distance
    f32 CalculateAttenuation(f32 distance) const {
        if (distance > range) return 0.0f;
        return 1.0f / (constantAttenuation +
                       linearAttenuation * distance +
                       quadraticAttenuation * distance * distance);
    }
};

// Maximum lights supported in a single draw
constexpr u32 MAX_DIRECTIONAL_LIGHTS = 4;
constexpr u32 MAX_POINT_LIGHTS = 64;
constexpr u32 MAX_SPOT_LIGHTS = 32;

// Maximum shadow-casting point/spot lights (cubemap faces are expensive)
constexpr u32 MAX_SHADOW_POINT_LIGHTS = 4;
constexpr u32 MAX_SHADOW_SPOT_LIGHTS = 4;

// GPU-side directional light data (aligned for UBO)
struct alignas(16) DirectionalLightData {
    Math::Vector3 direction;
    f32 _pad0;
    Math::Vector3 color;
    f32 intensity;
};

// GPU-side point light data (aligned for UBO)
struct alignas(16) PointLightData {
    Math::Vector3 position;
    f32 range;
    Math::Vector3 color;
    f32 intensity;
    f32 constantAttenuation;
    f32 linearAttenuation;
    f32 quadraticAttenuation;
    f32 _pad0;
};

// GPU-side spot light data (aligned for UBO)
struct alignas(16) SpotLightData {
    Math::Vector3 position;
    f32 range;
    Math::Vector3 direction;
    f32 intensity;
    Math::Vector3 color;
    f32 innerCutoff;  // cos(innerConeAngle)
    f32 outerCutoff;  // cos(outerConeAngle)
    f32 constantAttenuation;
    f32 linearAttenuation;
    f32 quadraticAttenuation;

    // --- Cookie (gobo) ---
    // cookieRight is the light's local +X taken from its transform, so the
    // pattern keeps a stable orientation when the light rotates. The shader
    // derives up as cross(direction, right) rather than carrying a second
    // vector. Deriving BOTH from the direction alone would be cheaper still, but
    // any stable-perpendicular trick flips as the direction crosses its
    // reference axis, and a gobo that spins when a lamp is rotated past vertical
    // is worse than one extra row.
    Math::Vector3 cookieRight;
    f32 cookieIndex;      // bindless texture index; < 0 means no cookie
    f32 cookieScale;      // 1 = the pattern fills the outer cone
    f32 cookieIntensity;  // blend between plain light (0) and full cookie (1)
    f32 _cookiePad0;
    f32 _cookiePad1;
};

// std140 rows: 6 x 16 bytes. The GLSL SpotLight struct in triangle.frag,
// grass.frag, shrub.frag, tree.frag and sprite_lit.frag must match exactly --
// they all index the same UBO array, so a stride mismatch in any one of them
// reads every light after the first from the wrong offset.
static_assert(sizeof(SpotLightData) == 96, "SpotLightData must stay 96 bytes; update all 5 shaders together");

// GPU shadow data SSBO for point/spot light shadow maps (binding 12)
struct alignas(16) ShadowDataSSBO {
    Math::Matrix4 pointFaceViewProj[MAX_SHADOW_POINT_LIGHTS * 6]; // 24 matrices
    Math::Vector4 pointLightParams[MAX_SHADOW_POINT_LIGHTS];       // xyz=pos, w=range
    Math::Matrix4 spotViewProj[MAX_SHADOW_SPOT_LIGHTS];             // 4 matrices
    i32 pointShadowCount;
    i32 spotShadowCount;
    i32 _pad[2];
};

// Lighting uniform buffer object
struct alignas(16) LightingUBO {
    // Scene ambient
    Math::Vector3 ambientColor;
    f32 ambientIntensity;

    // Camera position (for specular calculations)
    Math::Vector3 cameraPosition;
    f32 _pad0;

    // Light counts
    u32 directionalLightCount;
    u32 pointLightCount;
    u32 spotLightCount;
    u32 _pad1;

    // Cascaded shadow mapping data
    Math::Matrix4 cascadeViewProj[4];  // Per-cascade light view-projection matrices
    Math::Vector4 cascadeSplits;       // View-space far distance of each cascade (x,y,z,w)
    f32 shadowSoftness;                // 0 = hard (3x3 PCF), >0 = soft (Poisson disk radius in texels)
    i32 shadowEnabled;                 // 1 = shadows enabled
    f32 shadowStrength;                // 0..1 shadow strength (0 = no shadow, 1 = full)
    f32 shadowMaxDistance;             // Maximum shadow distance (for fade-out)

    // Point/spot shadow counts (how many UBO lights at indices 0..N-1 have shadow maps)
    i32 pointShadowCount;
    i32 spotShadowCount;
    f32 celDiffuseBands;       // 0 = disabled, 2-8 = number of quantized bands
    f32 celSpecularCutoff;     // 0 = disabled, >0 = hard cutoff threshold for specular highlights

    // Shading model flags: bit0=GGX, bit1=Fresnel, bit2=EnergyConserv, bit3=GeometryTerm, bit4=SphereEnvMap
    u32 shadingFlags;
    f32 sphereEnvStrength;     // Spherical environment map intensity (0=off, 0.5=subtle, 1.0=full)
    f32 posterizeLevels;       // Color posterization: 0=disabled, 4-256 = discrete color levels per channel
    f32 texturePageSize;   // Retro: PS1 texture page size in texels (0=disabled, 64/128 typical)

    // Wind data for vegetation/weather shaders
    alignas(16) Math::Vector4 windData;  // xyz = wind direction * strength, w = time

    // Fog parameters
    alignas(16) Math::Vector4 fogParams;     // x=density, y=start, z=end, w=heightFalloff
    alignas(16) Math::Vector4 fogColorSnow;  // xyz=fog color, w=snow intensity

    // Player position for vegetation stepping (xyz = world pos, w = step radius)
    alignas(16) Math::Vector4 playerPosition;

    // World curvature (x = strength, yzw reserved)
    alignas(16) Math::Vector4 worldCurvature;

    // Sky reflection color for water/ice surfaces (xyz = color, w = lightRampMode: 0=off, 1=smooth, 2=warm, 3=cool, 4=anime)
    alignas(16) Math::Vector4 skyReflectColor;

    // SH light probe irradiance (xyz = RGB irradiance from nearest probe, w = blend weight 0 or 1)
    alignas(16) Math::Vector4 shProbeIrradiance;

    // Reflection probe data (box-projected environment reflections)
    // probePosition: xyz = world-space center, w = intensity (0 = no probe active)
    alignas(16) Math::Vector4 reflectionProbePosition;
    // probeBoxMin: xyz = world-space AABB min, w = blend distance
    alignas(16) Math::Vector4 reflectionProbeBoxMin;
    // probeBoxMax: xyz = world-space AABB max, w = isBaked (1.0 = baked cubemap at binding 19)
    alignas(16) Math::Vector4 reflectionProbeBoxMax;

    // Volumetric fog screen mapping: xy = swapchain size in pixels (froxel UV =
    // fragCoord / xy), z = camera near plane (froxel depth-slice mapping must
    // match the compute pass), w = reserved. xy == 0 means "not provided" and
    // the shader falls back to legacy behavior.
    alignas(16) Math::Vector4 fogScreenParams;

    // DDGI (software global illumination) — the PBR shader samples the probe
    // irradiance atlas (binding 22) directly via world position + normal.
    alignas(16) Math::Vector4 ddgiGridOrigin;   // xyz = probe grid origin, w = spacing
    alignas(16) i32 ddgiProbeCounts[4];         // xyz = probes per axis, w = oct resolution
    alignas(16) Math::Vector4 ddgiAtlasParams;  // x = atlas width, y = atlas height, z = enabled (0/1), w = intensity

    // Light data arrays
    DirectionalLightData directionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLightData pointLights[MAX_POINT_LIGHTS];
    SpotLightData spotLights[MAX_SPOT_LIGHTS];

    // Cloud shadows: passing sky clouds darken the sun's contribution.
    // x = coverage, y = scale, z = shadow strength, w = drift speed.
    // Appended LAST so every shader declaring the old block stays a valid
    // prefix of the bigger buffer (only triangle.frag reads this field).
    alignas(16) Math::Vector4 cloudShadowParams;

    // Accessibility color transform, applied at the end of the PBR shader so it
    // works WITHOUT the post-process pass (which exported desktop games lack).
    // x = colorblind mode (0=off, 1..8 per postprocess.frag's CB_* order),
    // y = colorblind strength, z = brightness (-0.5..0.5), w = contrast (0.5..2).
    // Same append-last prefix rule as cloudShadowParams; only triangle.frag reads it.
    alignas(16) Math::Vector4 accessibilityParams;
};

} // namespace ECS
} // namespace Enjin
