#pragma once
// A tree volume's canopy for the season: colour and how full it is.
//
// One copy for both backends. It lived inside the Vulkan TreeRenderer, so web
// trees kept their authored summer canopy all year while desktop trees turned,
// thinned and went bare.

#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/Effects/WorldTime.h"

namespace Enjin {
namespace Effects {

struct SeasonalCanopy {
    Math::Vector3 base;
    Math::Vector3 tip;
    f32 scale = 1.0f;   // 0 = bare branches, 1 = full crown
};

// Deciduous trees grow back in spring, are full in summer, thin and turn in
// fall and are bare in winter. Evergreens keep their authored canopy.
inline SeasonalCanopy ComputeSeasonalCanopy(const ECS::TreeVolumeComponent& tree,
                                            Season season, f32 progress) {
    SeasonalCanopy c;
    c.base = tree.canopyBaseColor;
    c.tip = tree.canopyTipColor;
    if (tree.treeType != ECS::TreeType::Deciduous) return c;

    switch (season) {
        case Season::Spring:
            c.scale = 0.3f + 0.5f * progress;   // growing back
            c.base = tree.springCanopyColor;
            c.tip = tree.springCanopyColor * 1.2f;
            break;
        case Season::Summer:
            c.scale = 1.0f;
            c.base = tree.summerCanopyColor;
            c.tip = tree.summerCanopyColor * 1.3f;
            break;
        case Season::Fall:
            c.scale = 1.0f - 0.7f * progress;   // thinning
            c.base = tree.summerCanopyColor + (tree.fallCanopyColor - tree.summerCanopyColor) * progress;
            c.tip = c.base * 1.2f;
            break;
        case Season::Winter:
            c.scale = 0.0f;   // bare branches
            break;
    }
    return c;
}

} // namespace Effects
} // namespace Enjin
