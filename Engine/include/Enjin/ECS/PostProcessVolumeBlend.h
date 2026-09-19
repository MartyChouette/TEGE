#pragma once

#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Renderer/PostProcessing.h"

namespace Enjin {
namespace ECS {

class World;

// Blend every active post-process volume the camera is inside, on top of `base`,
// into `out`. Returns true if any volume contributed (false leaves `out` untouched,
// so a caller can skip its own restore work).
//
// `base` is a separate parameter, and that is the whole point of this function.
// All three runtimes used to blend the LIVE settings into themselves each frame:
//
//     blended = live;  lerp(blended, volume, w);  live = blended;
//
// which re-lerps an already-lerped value every frame. A volume with weight 0.5
// reaches 0.75 of its own settings on frame two, 0.875 on frame three, and is
// indistinguishable from weight 1.0 within about ten frames -- so `weight` and
// the blendRadius falloff both stopped meaning anything a sixth of a second
// after you entered. Worse, a volume that stops contributing is simply skipped,
// and nothing puts the old values back: walking through one tinted corridor
// tinted the rest of the session. In the editor it also overwrote the
// PostProcessing panel's authored values in place.
//
// Passing the base explicitly makes the caller hold the un-blended settings, so
// each frame starts where the author left off. `out` and `base` may not alias.
ENJIN_API bool BlendPostProcessVolumes(World* world,
                                       const Math::Vector3& cameraPosition,
                                       const Renderer::PostProcessSettings& base,
                                       Renderer::PostProcessSettings& out);

} // namespace ECS
} // namespace Enjin
