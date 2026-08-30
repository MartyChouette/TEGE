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

    // Default colors (used as summer reference). Shared palette: warm bark,
    // deep-ground canopy shadow -> light-green mid -> sun-catching tip.
    // See docs/art/PROCEDURAL_EFFECTS_DIRECTION.md.
    Math::Vector3 trunkColor = Math::Vector3(0.22f, 0.15f, 0.09f);        // warm bark brown
    Math::Vector3 canopyBaseColor = Math::Vector3(0.11f, 0.18f, 0.09f);   // canopy shadow (deep ground)
    Math::Vector3 canopyTipColor = Math::Vector3(0.52f, 0.68f, 0.32f);    // sun-catching tip

    // Seasonal canopy colors (for deciduous trees)
    Math::Vector3 springCanopyColor = Math::Vector3(0.42f, 0.62f, 0.24f);  // bright green
    Math::Vector3 summerCanopyColor = Math::Vector3(0.24f, 0.37f, 0.16f);  // mid green
    Math::Vector3 fallCanopyColor = Math::Vector3(0.68f, 0.42f, 0.12f);    // orange-brown

    // Wind response
    f32 windSwayStrength = 0.3f;

    // Generate a static capsule collider per trunk at PLAY START (opt-in) -
    // instanced trees aren't entities, so the runtime spawns transient
    // collider entities at the same hashed positions the shader uses. They
    // are never saved (created in play, discarded with the play world).
    bool generateColliders = false;

    // Number of intersecting quads for canopy
    u32 canopyQuads = 3;

    // Height variance range (size multiplier per-instance)
    f32 minHeightScale = 0.6f;
    f32 maxHeightScale = 1.4f;

    // External art (texture paths for bark and canopy)
    std::string barkTexturePath;
    std::string canopyTexturePath;

    // Runtime bindless texture indices for the paths above (-2 = unresolved,
    // -1 = none/failed). Not serialized; reset to -2 when a path changes.
    i32 cachedBarkTexIndex = -2;
    i32 cachedCanopyTexIndex = -2;
};

} // namespace ECS
} // namespace Enjin
