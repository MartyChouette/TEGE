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
    // Opt out of culling, for geometry whose bounds lie about where it draws: a
    // vertex-animated banner, a shader that pushes vertices outward, a skybox
    // shell. Each switches off only the test it names; they travel to the GPU
    // cull pass as CullableObject::cullFlags. Both apply only to meshes the GPU
    // pass draws indirectly; per-entity draws are not culled there at all.
    bool frustumCull = true;
    bool occlusionCull = true;
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

    // NOT WIRED, and superseded. There is no named-shader registry to look a name
    // up in, and nothing reads this field.
    //
    // Per-entity custom shaders already exist and work through
    // CustomShaderComponent, which holds the compiled GLSL and the node graph that
    // produced it, and which RenderSystem binds via GetEntityCustomPipeline. That
    // is the feature; this string is an older second way to ask for it that was
    // never built. Left in place rather than deleted so existing scenes keep
    // loading, and labelled so nobody types into it expecting an effect.
    std::string customShaderName;

    // NOT WIRED. Selecting the lightmap's UV set needs the choice to reach the
    // fragment shader, which means a bit in the per-object SSBO and a branch in
    // triangle.frag between fragUV and fragUV1. The second UV channel IS carried
    // through to the shader already; only the selection is missing.
    //
    // Until then the lightmap samples the primary UVs, which is what 0 means, so
    // the DEFAULT is honest and only a non-zero value is ignored.
    u8 lightmapUVChannel = 0;

    // Instancing hint — entities with identical mesh + material + renderer
    // settings are batched into instanced draw calls when this is true.
    bool allowInstancing = true;

    // Per-entity wireframe overlay (debug/editor)
    bool wireframe = false;
    Math::Vector3 wireframeColor = Math::Vector3(0.0f, 1.0f, 0.0f); // Green
    f32 wireframeOpacity = 1.0f;
};

} // namespace ECS
} // namespace Enjin
