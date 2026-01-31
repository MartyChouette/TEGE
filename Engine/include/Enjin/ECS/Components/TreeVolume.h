#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>

namespace Enjin {
namespace ECS {

// Tree lifecycle type
enum class TreeType : u8 { Deciduous = 0, Evergreen = 1 };

// Tree volume component - defines a region filled with GPU-instanced procedural trees
struct ENJIN_API TreeVolumeComponent {
    // Bounding box half-extents (local space, Y ignored - trees sit on XZ plane)
    Math::Vector3 halfExtents = Math::Vector3(20.0f, 0.0f, 20.0f);

    // Number of tree instances
    u32 density = 100;

    // Tree lifecycle type
    TreeType treeType = TreeType::Deciduous;

    // Trunk geometry
    f32 trunkHeight = 2.0f;
    f32 trunkWidth = 0.15f;

    // Canopy geometry
    f32 canopyRadius = 1.0f;
    f32 canopyOffset = 1.5f;  // Y offset from base where canopy center is

    // Default colors (used as summer reference)
    Math::Vector3 trunkColor = Math::Vector3(0.35f, 0.22f, 0.1f);
    Math::Vector3 canopyBaseColor = Math::Vector3(0.1f, 0.35f, 0.08f);
    Math::Vector3 canopyTipColor = Math::Vector3(0.2f, 0.5f, 0.15f);

    // Seasonal canopy colors (for deciduous trees)
    Math::Vector3 springCanopyColor = Math::Vector3(0.3f, 0.6f, 0.2f);   // bright green
    Math::Vector3 summerCanopyColor = Math::Vector3(0.1f, 0.35f, 0.08f);  // deep green
    Math::Vector3 fallCanopyColor = Math::Vector3(0.7f, 0.4f, 0.1f);     // orange-brown

    // Wind response
    f32 windSwayStrength = 0.3f;

    // Number of intersecting quads for canopy
    u32 canopyQuads = 3;

    // Height variance range (size multiplier per-instance)
    f32 minHeightScale = 0.6f;
    f32 maxHeightScale = 1.4f;

    // External art (texture paths for bark and canopy)
    std::string barkTexturePath;
    std::string canopyTexturePath;
};

} // namespace ECS
} // namespace Enjin
