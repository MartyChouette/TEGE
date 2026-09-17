#pragma once
// Play a recorded fluid simulation on this entity's fluid volume.
//
// Sits alongside FluidVolumeComponent, which keeps carrying the volume's
// extents and its look (colour, opacity, density threshold) -- so a recording
// can be recoloured or resized without re-baking, and so everything that
// already knows how to draw a volume keeps working unchanged.
//
// The volume is NOT simulated while a recording drives it. Solving a
// played-back frame would overwrite it with a frame of real simulation before
// anything could draw it, so the recording would never be seen at all.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#include <string>

namespace Enjin {
namespace ECS {

struct ENJIN_API FluidPlaybackComponent {
    // Project-relative path to the .enjfluid recording. Empty means nothing to
    // play, which is the state a freshly added component is in and is not an
    // error -- it is the prompt to go and bake something.
    std::string bakePath;

    bool playing = true;
    // Negative runs the take backwards, which is a legitimate effect (smoke
    // being sucked in) and costs nothing to allow.
    f32 speed = 1.0f;

    // Playback head in seconds. Serialized so a scene can open mid-take for a
    // screenshot or a cinematic that starts partway in.
    f32 time = 0.0f;

    // Runtime only: set once the file has been read, so a missing or broken
    // recording is reported once rather than retried every frame.
    bool loadAttempted = false;
    bool loadFailed = false;
};

} // namespace ECS
} // namespace Enjin
