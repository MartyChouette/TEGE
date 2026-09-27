#include "Enjin/Renderer/CameraLens.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/CameraZones.h"
#include "Enjin/ECS/Components/Lens.h"

namespace Enjin {
namespace Renderer {

void ApplyLensToSettings(const ECS::LensComponent& lens, PostProcessSettings& s) {
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
}

bool ApplyCameraLens(ECS::World* world, PostProcessSettings& live) {
    if (!world) return false;
    const ECS::Entity cam = ECS::ResolveGameCamera(world);
    if (cam == ECS::INVALID_ENTITY) return false;
    const auto* lens = world->GetComponent<ECS::LensComponent>(cam);
    if (!lens || !lens->enabled) return false;
    ApplyLensToSettings(*lens, live);
    return true;
}

} // namespace Renderer
} // namespace Enjin
