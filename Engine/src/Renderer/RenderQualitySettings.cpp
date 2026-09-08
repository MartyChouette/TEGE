#include "Enjin/Renderer/RenderQualitySettings.h"

#include <nlohmann/json.hpp>
#include <algorithm>

namespace Enjin {
namespace Renderer {

namespace {

template <typename T>
T CapTo(T value, T ceiling) { return value > ceiling ? ceiling : value; }

u32 FloorTo(u32 value, u32 floorValue) { return value < floorValue ? floorValue : value; }

} // namespace

const char* QualityTierName(QualityTier t) {
    switch (t) {
        case QualityTier::Low:    return "Low";
        case QualityTier::Medium: return "Medium";
        case QualityTier::High:   return "High";
        case QualityTier::Ultra:  return "Ultra";
        case QualityTier::Custom: return "Custom";
    }
    return "High";
}

QualityTier QualityTierFromName(const std::string& name, QualityTier fallback) {
    if (name == "Low")    return QualityTier::Low;
    if (name == "Medium") return QualityTier::Medium;
    if (name == "High")   return QualityTier::High;
    if (name == "Ultra")  return QualityTier::Ultra;
    if (name == "Custom") return QualityTier::Custom;
    return fallback;
}

RenderQualitySettings::RenderQualitySettings() {
    // Low: no ray tracing at all. This is the tier that has to run on the
    // machine that cannot afford any of it, so the master gates go off rather
    // than the counts going to 1 -- a cheap RT pass is still an RT pass.
    RenderQualityCaps& low = tiers[static_cast<u32>(QualityTier::Low)];
    low.allowRayTracing = false;
    low.allowPathTracing = false;
    low.allowRestirSpatialReuse = false;
    low.allowSurfelCache = false;
    low.allowRadianceCache = false;
    low.maxPathTracerSPP = 1;
    low.maxGIBounces = 0;
    low.maxDenoiserIterations = 1;
    low.maxRestirInitialCandidates = 1;
    low.maxRestirSpatialNeighbors = 0;
    low.maxSurfelCount = 0;
    low.maxAdaptiveRaysPerPixel = 1;
    low.maxDDGIRaysPerProbe = 32;
    low.minDDGIAmortizationRate = 16;

    // Medium: real-time RT effects are allowed, offline-shaped ones are not.
    RenderQualityCaps& med = tiers[static_cast<u32>(QualityTier::Medium)];
    med.allowRayTracing = true;
    med.allowPathTracing = false;
    med.allowRestirSpatialReuse = false;
    med.allowSurfelCache = false;
    med.allowRadianceCache = true;
    med.maxPathTracerSPP = 64;
    med.maxGIBounces = 1;
    med.maxDenoiserIterations = 3;
    med.maxRestirInitialCandidates = 8;
    med.maxRestirSpatialNeighbors = 4;
    med.maxSurfelCount = 16384;
    med.maxAdaptiveRaysPerPixel = 2;
    med.maxDDGIRaysPerProbe = 64;
    med.minDDGIAmortizationRate = 8;

    // High: the default. Everything on, at costs a current machine can hold.
    RenderQualityCaps& high = tiers[static_cast<u32>(QualityTier::High)];
    high.maxPathTracerSPP = 256;
    high.maxGIBounces = 2;
    high.maxDenoiserIterations = 5;
    high.maxRestirInitialCandidates = 16;
    high.maxRestirSpatialNeighbors = 8;
    high.maxSurfelCount = 65536;
    high.maxAdaptiveRaysPerPixel = 4;
    high.maxDDGIRaysPerProbe = 128;
    high.minDDGIAmortizationRate = 4;

    // Ultra and Custom keep the struct defaults, which clamp nothing. Ultra
    // meaning "do not get in the way" is the point: the authored scene value is
    // the artistic intent, and a tier exists to go cheaper, never richer.
}

const RenderQualityCaps& RenderQualitySettings::CapsFor(QualityTier t) const {
    const u32 i = static_cast<u32>(t);
    return tiers[i < kQualityTierCount ? i : static_cast<u32>(QualityTier::High)];
}

RenderQualityCaps& RenderQualitySettings::CapsFor(QualityTier t) {
    const u32 i = static_cast<u32>(t);
    return tiers[i < kQualityTierCount ? i : static_cast<u32>(QualityTier::High)];
}

void RenderQualitySettings::ApplyTo(SceneRenderSettings& s, QualityTier t) const {
    if (!enabled) return;
    const RenderQualityCaps& c = CapsFor(t);

    // Master gates first: a feature turned off here does not need its counts
    // clamped, and turning it off is the only thing that reliably saves the
    // whole pass rather than a fraction of it.
    if (!c.allowRayTracing) {
        s.rtEnabled = false;
    }
    if (!c.allowPathTracing && s.rtMode == 1) {
        s.rtMode = 0;   // fall back to hybrid rather than refusing to render
    }
    if (!c.allowRadianceCache) s.radianceCacheEnabled = false;
    if (!c.allowSurfelCache)   s.surfelCacheEnabled = false;
    if (!c.allowRestirSpatialReuse) s.restirSpatialReuse = false;

    // Ceilings. Every one of these is a min(), so a scene authored cheaper than
    // the tier keeps its own value.
    s.rtPathTracerTargetSPP   = CapTo(s.rtPathTracerTargetSPP,   c.maxPathTracerSPP);
    s.rtGIBounces             = CapTo(s.rtGIBounces,             c.maxGIBounces);
    s.rtDenoiserIterations    = CapTo(s.rtDenoiserIterations,    c.maxDenoiserIterations);
    s.restirInitialCandidates = CapTo(s.restirInitialCandidates, c.maxRestirInitialCandidates);
    s.restirSpatialNeighbors  = CapTo(s.restirSpatialNeighbors,  c.maxRestirSpatialNeighbors);
    s.surfelCacheMaxSurfels   = CapTo(s.surfelCacheMaxSurfels,   c.maxSurfelCount);
    s.adaptiveRayMaxPerPixel  = CapTo(s.adaptiveRayMaxPerPixel,  c.maxAdaptiveRaysPerPixel);
    s.ddgiRaysPerProbe        = CapTo(s.ddgiRaysPerProbe,        c.maxDDGIRaysPerProbe);

    // The minimum ray count has to follow the maximum down, or a Low tier that
    // caps the max at 1 leaves min > max and the budget range is inverted.
    s.adaptiveRayMinPerPixel  = CapTo(s.adaptiveRayMinPerPixel,  s.adaptiveRayMaxPerPixel);

    // Amortization is "one probe in N", so cheaper is BIGGER and this is a floor.
    s.ddgiAmortizationRate = FloorTo(s.ddgiAmortizationRate, c.minDDGIAmortizationRate);
}

namespace {

nlohmann::json CapsToJson(const RenderQualityCaps& c) {
    nlohmann::json j;
    j["allowRayTracing"]           = c.allowRayTracing;
    j["allowPathTracing"]          = c.allowPathTracing;
    j["allowRestirSpatialReuse"]   = c.allowRestirSpatialReuse;
    j["allowSurfelCache"]          = c.allowSurfelCache;
    j["allowRadianceCache"]        = c.allowRadianceCache;
    j["maxPathTracerSPP"]          = c.maxPathTracerSPP;
    j["maxGIBounces"]              = c.maxGIBounces;
    j["maxDenoiserIterations"]     = c.maxDenoiserIterations;
    j["maxRestirInitialCandidates"]= c.maxRestirInitialCandidates;
    j["maxRestirSpatialNeighbors"] = c.maxRestirSpatialNeighbors;
    j["maxSurfelCount"]            = c.maxSurfelCount;
    j["maxAdaptiveRaysPerPixel"]   = c.maxAdaptiveRaysPerPixel;
    j["maxDDGIRaysPerProbe"]       = c.maxDDGIRaysPerProbe;
    j["minDDGIAmortizationRate"]   = c.minDDGIAmortizationRate;
    return j;
}

void CapsFromJson(const nlohmann::json& j, RenderQualityCaps& c) {
    if (!j.is_object()) return;
    auto B = [&](const char* k, bool& dst) {
        if (j.contains(k) && j[k].is_boolean()) dst = j[k].get<bool>();
    };
    auto U = [&](const char* k, u32& dst) {
        if (j.contains(k) && j[k].is_number()) dst = j[k].get<u32>();
    };
    B("allowRayTracing", c.allowRayTracing);
    B("allowPathTracing", c.allowPathTracing);
    B("allowRestirSpatialReuse", c.allowRestirSpatialReuse);
    B("allowSurfelCache", c.allowSurfelCache);
    B("allowRadianceCache", c.allowRadianceCache);
    U("maxPathTracerSPP", c.maxPathTracerSPP);
    U("maxGIBounces", c.maxGIBounces);
    U("maxDenoiserIterations", c.maxDenoiserIterations);
    U("maxRestirInitialCandidates", c.maxRestirInitialCandidates);
    U("maxRestirSpatialNeighbors", c.maxRestirSpatialNeighbors);
    U("maxSurfelCount", c.maxSurfelCount);
    U("maxAdaptiveRaysPerPixel", c.maxAdaptiveRaysPerPixel);
    U("maxDDGIRaysPerProbe", c.maxDDGIRaysPerProbe);
    U("minDDGIAmortizationRate", c.minDDGIAmortizationRate);
}

} // namespace

nlohmann::json SerializeRenderQuality(const RenderQualitySettings& q) {
    nlohmann::json j;
    j["enabled"]         = q.enabled;
    j["defaultTier"]     = QualityTierName(q.defaultTier);
    j["playerCanChange"] = q.playerCanChange;

    nlohmann::json tiersJson = nlohmann::json::object();
    for (u32 i = 0; i < kQualityTierCount; ++i) {
        const QualityTier t = static_cast<QualityTier>(i);
        tiersJson[QualityTierName(t)] = CapsToJson(q.tiers[i]);
    }
    j["tiers"] = tiersJson;
    return j;
}

RenderQualitySettings DeserializeRenderQuality(const nlohmann::json& j) {
    // Starts from the built-in presets, so a project that authored only one tier
    // still gets sane values for the rest.
    RenderQualitySettings q;
    if (!j.is_object()) return q;

    if (j.contains("enabled") && j["enabled"].is_boolean()) {
        q.enabled = j["enabled"].get<bool>();
    }
    if (j.contains("playerCanChange") && j["playerCanChange"].is_boolean()) {
        q.playerCanChange = j["playerCanChange"].get<bool>();
    }
    if (j.contains("defaultTier") && j["defaultTier"].is_string()) {
        q.defaultTier = QualityTierFromName(j["defaultTier"].get<std::string>(), q.defaultTier);
    }
    if (j.contains("tiers") && j["tiers"].is_object()) {
        const auto& tj = j["tiers"];
        for (u32 i = 0; i < kQualityTierCount; ++i) {
            const char* name = QualityTierName(static_cast<QualityTier>(i));
            if (tj.contains(name)) CapsFromJson(tj[name], q.tiers[i]);
        }
    }
    return q;
}

} // namespace Renderer
} // namespace Enjin
