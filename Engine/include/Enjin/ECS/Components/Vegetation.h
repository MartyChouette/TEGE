#pragma once

#include "Enjin/Platform/Platform.h"

namespace Enjin {
namespace ECS {

// Vegetation tag component for trees/bushes
// When attached to a mesh entity, enables wind sway in the vertex shader
// Uses vertex color red channel as sway weight (trunk=0, leaves/branches=1)
struct ENJIN_API VegetationComponent {
};

} // namespace ECS
} // namespace Enjin
