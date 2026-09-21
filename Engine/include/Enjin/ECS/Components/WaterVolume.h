#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/BoundaryPolygon.h"
#include <cmath>

namespace Enjin {
namespace ECS {

// Water type presets
enum class WaterType : u32 {
    Lake = 0,
    Ocean,
    River,
    Pond
};

// Water volume component - attach to an empty game object to define a water area
// The entity's TransformComponent position is the origin of the water surface
// Y position = water surface level, halfExtents define the horizontal area and depth
struct ENJIN_API WaterVolumeComponent {
    // Bounding box half-extents (local space)
    // X/Z define horizontal area, Y defines depth below surface
    Math::Vector3 halfExtents = Math::Vector3(50.0f, 5.0f, 50.0f);

    // Water type preset
    WaterType waterType = WaterType::Lake;

    // Water visual settings
    Math::Vector3 waterColor = Math::Vector3(0.1f, 0.3f, 0.5f);
    f32 opacity = 0.7f;

    // Planar reflection: how strongly the scene above the surface is mirrored into
    // it. 0 disables the pass entirely, which is what you want for muddy or deep
    // water where a mirror would look wrong.
    //
    // This is the same mechanic Water3D's Reflective style uses -- real geometry
    // redrawn upside down under the surface, not a screen-space trace -- so it
    // only shows through a surface with opacity < 1, and only from a camera above
    // the water line. Both of those are handled in MirrorSceneAcrossPlane.
    f32 reflectionStrength = 0.4f;

    // Wave animation
    f32 waveSpeed = 1.0f;
    f32 waveHeight = 0.2f;

    // Shore foam settings
    bool enableShore = true;
    f32 shoreWidth = 0.15f;       // 0-0.5, normalized edge distance for foam
    f32 foamIntensity = 0.6f;     // 0-1
    f32 foamScale = 8.0f;         // Noise scale for foam pattern
    Math::Vector3 shoreColor = Math::Vector3(0.3f, 0.6f, 0.7f);  // Shallow water tint

    // Buoyancy: dynamic rigidbodies inside the volume and below the surface get pushed
    // up so they float. On by default so "things float in water" works out of the box.
    bool enableBuoyancy = true;
    f32 buoyancyStrength = 1.6f;   // >1 = net upward at full submersion (floats); 1 = neutral
    f32 buoyancyDrag = 2.5f;       // water resistance (damps bobbing / horizontal drift)

    // Higher priority volumes override lower ones when overlapping
    i32 priority = 0;

    // Freeze state (runtime — driven by temperature zones each frame)
    f32 freezeProgress = 0.0f;      // 0 = liquid, 1 = frozen solid
    bool isFrozen = false;           // Convenience: true when freezeProgress >= 0.99

    // Freeze tuning (authored, serialized)
    f32 freezeRate = 0.3f;          // How fast it freezes (per second)
    f32 thawRate = 0.5f;            // How fast it thaws (per second)
    Math::Vector3 iceColor = Math::Vector3(0.7f, 0.85f, 0.95f);  // Frozen surface color
    f32 iceOpacity = 0.95f;         // Frozen surface opacity

    // Dirty flag: set to false to force mesh regeneration
    bool meshCreated = false;

    // Is this XZ inside the water's horizontal footprint?
    //
    // THE footprint rule, in one place. It is the halfExtents box, narrowed by
    // the drag-editable outline when the entity carries one.
    //
    // It had been written out inline at every site that asks -- swimming and
    // floating in ControllerSystem, buoyancy in JoltBackend, the surface mesh in
    // RenderSystem -- and the copies did not agree. Only the renderer learned
    // about BoundaryPolygonComponent, so a lake dragged into a kidney rendered
    // as a kidney and behaved as the rectangle it was seeded from. Four copies
    // of a rule is four chances for the next one to be missed, so there is one.
    //
    // `outline` is the entity's BoundaryPolygonComponent, or null when it has
    // none. A caller that does not look one up gets the old box behaviour, which
    // is what every scene authored before the outline existed wants.
    //
    // center = entity's world position (from TransformComponent). Rotation and
    // scale are ignored here, as they are everywhere else in the water path.
    bool FootprintContainsXZ(const Math::Vector3& center,
                             const BoundaryPolygonComponent* outline,
                             f32 x, f32 z) const {
        if (std::abs(x - center.x) > halfExtents.x) return false;
        if (std::abs(z - center.z) > halfExtents.z) return false;
        return outline ? outline->ContainsXZ(center, x, z) : true;
    }

    // Check if a point is inside this volume
    // center = entity's world position (from TransformComponent)
    bool ContainsPoint(const Math::Vector3& center, const Math::Vector3& point,
                       const BoundaryPolygonComponent* outline = nullptr) const {
        return std::abs(point.y - center.y) <= halfExtents.y &&
               FootprintContainsXZ(center, outline, point.x, point.z);
    }
};

// The surface material a water volume renders with.
//
// Free function rather than inline in RenderSystem::EnsureWaterMeshes, because
// everything it decides is a pure function of the component and the render loop
// around it is not testable without a GPU. The one field that matters most here
// -- alphaMode -- was wrong for the entire life of the feature and nothing could
// have caught it, since reaching it meant standing up a Vulkan device.
inline MaterialComponent MakeWaterSurfaceMaterial(const WaterVolumeComponent& vol) {
    MaterialComponent m;
    m.baseColor = vol.waterColor;
    m.opacity = vol.opacity;
    m.doubleSided = true;
    m.castShadows = false;

    // Blend, so you can see INTO the water.
    //
    // This was Opaque, commented "writes depth", and that conflated two decisions
    // made in different places. Depth writing comes from the PIPELINE, and the
    // pipeline choice already special-cases water: BindGeometryPipelineForMaterial
    // and the RenderToTarget loop both compute `wantTransparent = ... &&
    // !isWaterSurf`, so a water surface stays on the depth-writing pipeline no
    // matter what this field says. That is what keeps overlapping wave fragments
    // depth-culling instead of stacking up and washing the surface white.
    //
    // What alphaMode actually drives is the SORT BUCKET (ComputeSortKey puts Blend
    // in bucket 2, after all opaque geometry) and the shader, which clamps
    // `alpha = 1.0` for Opaque and Mask so an opacity value can never make them
    // see-through.
    //
    // Opaque therefore did three things at once: threw away the opacity the author
    // set on the volume, threw away iceOpacity with it, and put the surface in the
    // front-to-back bucket where it could draw before the bed it is meant to blend
    // over. It also hid planar reflections, which are mirrored geometry drawn BELOW
    // the surface and only read as a reflection through a translucent one.
    m.alphaMode = MaterialComponent::AlphaMode::Blend;

    switch (vol.waterType) {
        case WaterType::Ocean: m.metallic = 0.4f;  m.roughness = 0.05f; break;
        case WaterType::River: m.metallic = 0.25f; m.roughness = 0.15f; break;
        case WaterType::Pond:  m.metallic = 0.2f;  m.roughness = 0.2f;  break;
        case WaterType::Lake:
        default:               m.metallic = 0.3f;  m.roughness = 0.1f;  break;
    }
    return m;
}

// Vertex colour for a point on the water surface. edgeDist is 0 at the rim and
// 1 at the centre; the shader reads it out of .g to place shoreline foam.
//
// Extracted for the same reason as the material: it encodes a decision (what
// goes in .a) that was wrong and could not be reached without a GPU.
//
// .a is 1, NOT vol.opacity. The shader computes
// `alpha = mat_opacity * fragVertColor.a * texAlpha`, and mat_opacity is already
// the volume's opacity, so carrying it here squared it -- an authored 0.78
// rendered as 0.61. Worse when frozen: the freeze lerp swaps mat_opacity for
// iceOpacity but cannot touch a vertex buffer, so a 0.95 ice sheet rendered at
// 0.74 and never read as solid. The material is the one carrier.
inline Math::Vector4 MakeWaterVertexColor(const WaterVolumeComponent& vol, f32 edgeDist) {
    return Math::Vector4(vol.waterColor.x, edgeDist, vol.waterColor.z, 1.0f);
}

} // namespace ECS
} // namespace Enjin
