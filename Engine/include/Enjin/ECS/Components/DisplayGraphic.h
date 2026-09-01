#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin {
namespace ECS {

// P2 of the unified display system: a vector graphic that lives in the scene
// as a real entity. The SVG source (VectorDrawing exports SVG; any SVG works)
// is tessellated ONCE into colored triangles and mounted as this entity's
// mesh - crisp at any scale, timeline/tween-animatable like any entity, and
// the same tessellation drives the UICanvas VectorGraphic element (one
// compositor, two mounts).
struct ENJIN_API DisplayGraphicComponent {
    std::string sourcePath;       // project-relative .svg
    // World height of the WHOLE document (the transform's scale multiplies
    // it). Same explicit px->world contract as TextComponent.worldHeight.
    f32 worldHeight = 1.0f;
    // Max curve-flattening error in SVG units; smaller = more triangles.
    f32 curveTolerance = 0.25f;
    bool dirty = true;            // set after any field change to rebuild the mesh

    DisplayGraphicComponent() = default;
    DisplayGraphicComponent(const std::string& path) : sourcePath(path) {}
};

} // namespace ECS
} // namespace Enjin
