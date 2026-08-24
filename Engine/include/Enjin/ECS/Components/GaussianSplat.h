#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin {
namespace ECS {

// Gaussian splat cloud: a photoreal capture (phone scan, drone survey) placed
// in the scene like any other entity. Points at a .ply (INRIA 3DGS layout) or
// .spz (Niantic packed) file; the renderer draws it as sorted, alpha-blended
// gaussians. Splats are baked radiance - lights don't affect them, but every
// post effect and art style preset applies, since they render into the same
// color buffer as everything else.
struct GaussianSplatComponent {
    std::string sourcePath;        // .ply or .spz, project-relative or absolute
    f32  opacityScale = 1.0f;      // fades the whole cloud
    f32  splatScale = 1.0f;        // multiplies gaussian extents (1 = as captured)
    bool flipYZ = true;            // COLMAP frame -> engine Y-up (most captures need it)
    u32  maxSplats = 2000000;      // memory cap; loading warns when it truncates
    bool visible = true;

    // Runtime (not serialized)
    bool dirty = true;             // path/settings changed -> reload
    u32  loadedCount = 0;          // splats actually resident (0 = not loaded)
    std::string loadError;         // last loader error for the inspector
};

} // namespace ECS
} // namespace Enjin
