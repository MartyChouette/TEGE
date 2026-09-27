#include "Enjin/Renderer/CameraLens.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/CameraZones.h"
#include "Enjin/ECS/Components/Lens.h"
#include "Enjin/ECS/Components/Camera.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Renderer {

namespace {
constexpr f32 kSensorHeightMm = 24.0f;      // full frame, 36 x 24
constexpr f32 kCircleOfConfusionMm = 0.03f; // the full-frame convention
constexpr f32 kPi = 3.14159265358979f;
}

f32 FocalLengthFromFov(f32 fovDegrees) {
    const f32 half = std::clamp(fovDegrees, 1.0f, 179.0f) * 0.5f * kPi / 180.0f;
    return (kSensorHeightMm * 0.5f) / std::tan(half);
}

f32 FovFromFocalLength(f32 focalLengthMm) {
    const f32 f = std::max(focalLengthMm, 1.0f);
    return 2.0f * std::atan((kSensorHeightMm * 0.5f) / f) * 180.0f / kPi;
}

void ApplyLensToSettings(const ECS::LensComponent& lens, PostProcessSettings& s, f32 fovDegrees) {
    s.lensDistortion = lens.distortion;
    s.lensSqueeze = lens.anamorphicSqueeze > 0.01f ? lens.anamorphicSqueeze : 1.0f;
    if (lens.chromaticAberration > 0.0f) {
        s.chromaticAberrationEnabled = 1;
        s.chromaticAberrationIntensity = lens.chromaticAberration;
    }
    if (lens.vignetteIntensity > 0.0f) {
        s.vignetteEnabled = 1;
        s.vignetteIntensity = lens.vignetteIntensity;
        s.vignetteSmoothness = lens.vignetteSoftness;
    }
    if (lens.depthOfField) {
        // Thin-lens depth of field, 2 N c s^2 / f^2 in metres (valid while the
        // subject is well beyond the focal length, which a game camera is).
        // The post shader blurs fully at focus +/- dofFocalRange, so the
        // in-focus depth is that range.
        const f32 f = FocalLengthFromFov(fovDegrees) / 1000.0f;
        const f32 c = kCircleOfConfusionMm / 1000.0f;
        const f32 dist = std::max(lens.focusDistance, 0.01f);
        const f32 n = std::max(lens.apertureTStop, 0.5f);
        s.dofEnabled = 1;
        s.dofFocalDistance = dist;
        s.dofFocalRange = std::max(2.0f * n * c * dist * dist / (f * f), 0.01f);
        s.dofNearBlurStrength = 1.0f;
        s.dofFarBlurStrength = 1.0f;
    }
}

bool ApplyCameraLens(ECS::World* world, PostProcessSettings& live) {
    if (!world) return false;
    const ECS::Entity cam = ECS::ResolveGameCamera(world);
    if (cam == ECS::INVALID_ENTITY) return false;
    const auto* lens = world->GetComponent<ECS::LensComponent>(cam);
    if (!lens || !lens->enabled) return false;
    const auto* camera = world->GetComponent<ECS::CameraComponent>(cam);
    ApplyLensToSettings(*lens, live, camera ? camera->fieldOfView : 60.0f);
    return true;
}

} // namespace Renderer
} // namespace Enjin
