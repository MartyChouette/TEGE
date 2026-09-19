#include "Enjin/ECS/PostProcessVolumeBlend.h"

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"

#include <algorithm>
#include <vector>

namespace Enjin {
namespace ECS {

bool BlendPostProcessVolumes(World* world,
                             const Math::Vector3& cameraPosition,
                             const Renderer::PostProcessSettings& base,
                             Renderer::PostProcessSettings& out)
{
    if (!world) return false;

    auto volumeEntities = world->GetEntitiesWithComponent<PostProcessVolumeComponent>();
    if (volumeEntities.empty()) return false;

    struct VolumeEntry {
        const PostProcessVolumeComponent* vol;
        f32 blendWeight;
    };
    std::vector<VolumeEntry> active;
    active.reserve(volumeEntities.size());

    for (auto entity : volumeEntities) {
        auto* vol = world->GetComponent<PostProcessVolumeComponent>(entity);
        if (!vol || !vol->isActive) continue;

        Math::Vector3 center(0.0f, 0.0f, 0.0f);
        if (!vol->isGlobal) {
            auto* transform = world->GetComponent<TransformComponent>(entity);
            if (!transform) continue;
            center = transform->position;
        }

        const f32 w = vol->GetBlendWeight(center, cameraPosition);
        if (w <= 0.001f) continue;

        active.push_back({vol, w});
    }

    if (active.empty()) return false;

    // Lowest priority first: later volumes blend on top and so win.
    std::sort(active.begin(), active.end(),
        [](const VolumeEntry& a, const VolumeEntry& b) {
            return a.vol->priority < b.vol->priority;
        });

    Renderer::PostProcessSettings blended = base;
    for (const auto& entry : active) {
        BlendPostProcessSettings(blended, blended, entry.vol->settings,
                                 entry.blendWeight, entry.vol->overrideMask);
    }

    out = blended;
    return true;
}

} // namespace ECS
} // namespace Enjin
