#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS { class World; struct LensComponent; }
namespace Renderer {

struct PostProcessSettings;

// The camera's LensComponent, applied to post-processing.
//
// The component had a preset, distortion, squeeze, chromatic aberration and a
// vignette in the inspector, saved with the scene, and nothing read it (SD-27).
//
// Writes into `live` for the camera the game renders through (camera zones
// apply). Distortion and squeeze belong to the lens alone. Chromatic
// aberration and vignette replace the scene's when the lens sets them above
// zero, and leave the scene's alone at zero, so a Standard lens does not switch
// off a vignette the scene asked for.
//
// Returns true when a lens was applied. Callers use it inside the per-frame
// post-process volume overlay, which is undone at the start of the next frame,
// so a lens that is removed or a camera that changes takes its values with it
// instead of leaving them in the scene's settings.
ENJIN_API bool ApplyCameraLens(ECS::World* world, PostProcessSettings& live);

// The mapping alone, for tests.
ENJIN_API void ApplyLensToSettings(const ECS::LensComponent& lens, PostProcessSettings& s);

} // namespace Renderer
} // namespace Enjin
