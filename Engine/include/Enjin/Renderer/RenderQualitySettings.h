#pragma once

// Project-level render quality tiers.
//
// ADR-0006 puts performance knobs in a project quality tier rather than in a
// scene, because a player picking "Medium" to make the game run should not be
// silently undone by whatever a scene happened to save. RT settings themselves
// stay per scene (they already serialize there); this sits on top of them.
//
// A tier is a CEILING on cost, never a replacement for a look. Every field here
// caps the corresponding scene value and nothing raises it, so:
//   - a player can always make the game cheaper than the author asked for
//   - Ultra never forces an intentionally cheap or stylized scene to be expensive
//   - artistic values (intensity, strength, colour, fog, sky) are never touched
//
// ApplyTo is a pure function on SceneRenderSettings, applied BEFORE
// SceneRenderSettings::ApplyToRuntime. That is deliberate: it means the tier
// needs no renderer, no Vulkan and no per-backend code, so desktop and web get
// identical behaviour from one implementation instead of two that drift.

#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/SceneRenderSettings.h"

#include <nlohmann/json_fwd.hpp>
#include <string>

namespace Enjin {
namespace Renderer {

enum class QualityTier : u8 {
    Low = 0,
    Medium = 1,
    High = 2,
    Ultra = 3,
    Custom = 4,
};

inline constexpr u32 kQualityTierCount = 5;

const char* QualityTierName(QualityTier t);
QualityTier QualityTierFromName(const std::string& name, QualityTier fallback);

// The cost ceiling for one tier. Defaults are the Ultra shape (clamp nothing),
// so a field someone forgets to author cannot accidentally throttle a project.
struct RenderQualityCaps {
    // Master gates. False forces the feature off no matter what a scene asked.
    bool allowRayTracing = true;
    bool allowPathTracing = true;
    bool allowRestirSpatialReuse = true;
    bool allowSurfelCache = true;
    bool allowRadianceCache = true;

    // Ceilings. Each caps the matching SceneRenderSettings value.
    u32 maxPathTracerSPP = 4096;
    u32 maxGIBounces = 8;
    u32 maxDenoiserIterations = 8;
    u32 maxRestirInitialCandidates = 32;
    u32 maxRestirSpatialNeighbors = 16;
    u32 maxSurfelCount = 262144;
    u32 maxAdaptiveRaysPerPixel = 16;
    u32 maxDDGIRaysPerProbe = 256;

    // A FLOOR, not a ceiling: DDGI amortization is "update 1 probe in N", so a
    // bigger number is cheaper. Low tiers raise it.
    u32 minDDGIAmortizationRate = 1;
};

struct RenderQualitySettings {
    // OFF by default. An existing project that has never heard of tiers must
    // render exactly as it did, so nothing clamps until someone opts in.
    bool enabled = false;

    // The tier a game starts on when the player has not chosen one.
    QualityTier defaultTier = QualityTier::High;

    // Whether the game's options menu offers the choice at all. A project that
    // ships one fixed target can turn it off.
    bool playerCanChange = true;

    RenderQualityCaps tiers[kQualityTierCount];

    RenderQualitySettings();   // seeds the built-in tier presets

    const RenderQualityCaps& CapsFor(QualityTier t) const;
    RenderQualityCaps& CapsFor(QualityTier t);

    // Clamp a scene's cost knobs to this tier. Pure, and a no-op when disabled.
    void ApplyTo(SceneRenderSettings& s, QualityTier t) const;
};

nlohmann::json SerializeRenderQuality(const RenderQualitySettings& q);
RenderQualitySettings DeserializeRenderQuality(const nlohmann::json& j);

} // namespace Renderer
} // namespace Enjin
