#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin {
namespace ECS {

// An effect authored in the Effekseer editor (.efkefc, or the older .efk),
// played at this entity. The entity's world transform places, aims and scales
// it every frame, so a parented effect follows its parent.
//
// The effect file names its own textures, models and materials by path
// relative to itself; they are read through the same asset filesystem as the
// effect, so a packed build finds them in the pak.
struct EffekseerEffectComponent {
    std::string effectPath;        // .efkefc / .efk, project-relative or absolute
    bool playOnStart = true;       // start when the scene starts playing
    bool loop = true;              // start again when the effect has finished
    f32  speed = 1.0f;             // playback rate, 1 = as authored
    f32  magnification = 1.0f;     // size of the whole effect, baked when the file loads
    bool visible = true;

    // Runtime (not serialized)
    bool dirty = true;             // path or magnification changed -> reload
    bool playRequested = false;    // set by Play(); consumed by the system
    bool stopRequested = false;    // set by Stop(); consumed by the system
    bool playing = false;          // an instance is alive in the manager
    i32  handle = -1;              // Effekseer handle of the live instance
    std::string loadError;         // last loader error, for the inspector

    void Play() { playRequested = true; stopRequested = false; }
    void Stop() { stopRequested = true; playRequested = false; }
};

} // namespace ECS
} // namespace Enjin
