#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#include <array>

namespace Enjin {
namespace ECS {

// How much animation work a skinned character is worth at a distance.
//
// Refreshing a distant animator at full rate and full fidelity is work nobody can see,
// and animation is usually the most expensive thing a crowd scene does. Some of this
// existed before as four `constexpr f32` distances and a 1/2/4/8 frame interval buried in
// RenderSystem, which meant it could not be authored, could not be measured per model, and
// existed on the Vulkan path only -- web, the platform least able to afford it, refreshed
// every animator every frame.
//
// BANDS rather than a curve, because the knobs are discrete: a blend tree is on or off.
// A band says "past here, this is what a character is worth", and a person can look at the
// list and know what a crowd costs.
//
// Bands must be ordered by beginDistance, and band 0 should begin at 0. ResolveBand walks
// forward and takes the last band whose beginDistance the entity is past, so an unsorted
// list does not crash, it just picks oddly.
struct AnimationLODComponent {
    // Four is what the hardcoded version had (full, half, quarter, eighth rate) and is
    // enough to say close / mid / far / very far without turning authoring into a chore.
    static constexpr int MAX_BANDS = 4;

    struct Band {
        // Metres from the camera at which this band takes over.
        f32 beginDistance = 0.0f;

        // Pose refreshes per second. 0 means every frame.
        //
        // In HERTZ, not frames. The old version skipped every Nth FRAME, so the animation
        // rate a player got depended on the frame rate they happened to be running at --
        // a distant character animated twice as smoothly at 120fps as at 60, which is
        // backwards, since the machine holding 60 is the one that needed the saving. Time
        // for skipped frames is banked, so a clip never drifts: it advances by the whole
        // elapsed time on the frame it does refresh.
        f32 updateHz = 0.0f;

        // Solve IK at this distance. A hand placed exactly on a door handle stops being
        // worth a solve long before the character stops being worth animating.
        bool ik = true;

        // Evaluate the blend tree. Off falls back to plain clip playback rather than
        // freezing: the character keeps moving, it just stops blending between variants.
        bool blendTrees = true;

        // Interpolate between keyframes. Off snaps to the preceding key, which at distance
        // is invisible and skips a slerp per bone per frame.
        bool interpolate = true;
    };

    // Defaults reproduce the behaviour the hardcoded version had at 60fps (full, half,
    // quarter, eighth) so switching a project to this component changes nothing by itself,
    // and then keep it at that rate on machines that are not running at 60.
    std::array<Band, MAX_BANDS> bands = { {
        { 0.0f,    0.0f, true,  true,  true  },
        { 30.0f,  30.0f, true,  true,  true  },
        { 70.0f,  15.0f, false, false, true  },
        { 140.0f,  7.5f, false, false, false },
    } };

    i32 bandCount = 4;
    bool enabled = true;

    // Past this, stop updating the pose at all; the character holds its last one. 0 means
    // never stop. Distinct from not drawing it -- a held pose still renders, and still
    // casts a shadow, which is why this is not just a draw distance.
    f32 cullDistance = 0.0f;

    // Which band applies at `distance`. Always returns a valid index when bandCount > 0.
    i32 ResolveBand(f32 distance) const {
        const i32 count = (bandCount < 1) ? 1
                        : (bandCount > MAX_BANDS ? MAX_BANDS : bandCount);
        i32 chosen = 0;
        for (i32 b = 1; b < count; ++b) {
            if (distance >= bands[b].beginDistance) chosen = b;
            else break;
        }
        return chosen;
    }
};

// What one animator is allowed to do on one frame. Resolved from the component (or from
// the engine defaults when an entity has none) and handed to the pose update.
//
// A struct rather than four arguments because it is passed through the parallel pose
// pass, where adding a parameter means touching the job record as well.
struct AnimationQuality {
    bool blendTrees = true;
    bool interpolate = true;
};

} // namespace ECS
} // namespace Enjin
