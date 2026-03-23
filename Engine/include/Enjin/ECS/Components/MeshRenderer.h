#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>

namespace Enjin {
namespace ECS {

// Render layer bitmask — controls which cameras/passes render this entity.
// Default layer 0 is rendered by all cameras. Layers 1-31 can be used for
// selective rendering (e.g., layer 1 = minimap only, layer 2 = reflection probe only).
enum class RenderLayer : u32 {
    Default     = (1 << 0),
    Layer1      = (1 << 1),
    Layer2      = (1 << 2),
    Layer3      = (1 << 3),
    Layer4      = (1 << 4),
    UI          = (1 << 5),
    Minimap     = (1 << 6),
    Reflection  = (1 << 7),
    All         = 0xFFFFFFFF
};

// Mesh rendering control — attached to entities that need render behavior
// beyond what MeshComponent + MaterialComponent provide. Controls visibility,
// draw order, LOD bias, shadow behavior, and render layer assignment.
struct MeshRendererComponent {
    // Visibility & culling
    bool enabled = true;                  // Master on/off (skips draw entirely)
    bool frustumCull = true;              // Participate in frustum culling
    bool occlusionCull = true;            // Participate in HiZ occlusion culling
    f32 maxDrawDistance = 0.0f;           // 0 = infinite, >0 = fade out beyond this distance

    // Render order
    i32 renderQueue = 0;                  // Sort priority within same pipeline bucket
                                          // Negative = draw earlier, positive = draw later
                                          // 0 = default, -1000 = skybox, 1000 = overlay

    // Layers
    u32 renderLayerMask = static_cast<u32>(RenderLayer::Default);

    // LOD control
    f32 lodBias = 0.0f;                   // -1 = force higher detail, +1 = force lower detail
    bool forceLowestLOD = false;          // Debug: always use lowest LOD

    // Shadow control (overrides MaterialComponent settings)
    enum class ShadowMode : u8 {
        FromMaterial,   // Use MaterialComponent::castShadows
        Off,            // Never cast shadows
        On,             // Always cast shadows
        TwoSided        // Cast shadows with two-sided geometry
    };
    ShadowMode shadowMode = ShadowMode::FromMaterial;

    // Motion vectors (for TAA / motion blur)
    bool contributeMotionVectors = true;

    // Custom shader override (empty = use default pipeline)
    // When set, the render system looks up a named shader variant
    // and uses it instead of the standard PBR pipeline for this entity.
    std::string customShaderName;

    // Lightmap UV channel (0 = primary UVs, 1 = lightmap UVs)
    u8 lightmapUVChannel = 0;

    // Instancing hint — entities with identical mesh + material + renderer
    // settings are batched into instanced draw calls when this is true.
    bool allowInstancing = true;

    // Wireframe override (debug/editor)
    bool wireframe = false;
};

} // namespace ECS
} // namespace Enjin
