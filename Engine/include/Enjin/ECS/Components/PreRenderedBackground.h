#pragma once

// A pre-rendered background: a finished image of a room, plus the depth it was
// rendered at, standing in for geometry the hardware never has to draw.
//
// This lives on a CAMERA entity rather than on the scene, because a plate and a
// camera are one thing. The plate was rendered from exactly this viewpoint and
// is meaningless from any other, so binding them together makes the wrong setup
// unrepresentable -- and it gives room-to-room shot changes for free, which is
// how the games that used this worked: cut to another camera, get another
// painting.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#include <string>

namespace Enjin {
namespace ECS {

struct PreRenderedBackgroundComponent {
    bool enabled = true;

    // The finished image, drawn to fill the view. Rendered offline at whatever
    // quality and whatever render time you like -- that is the entire point.
    std::string platePath;

    // Per-pixel distance, packed across R, G and B (see Renderer/DepthPlate.h).
    // Empty means no depth: the plate is then just a backdrop and live geometry
    // always draws in front of it, which is a legitimate thing to want for a
    // skybox-like painting and useless for a room.
    std::string depthPath;

    // The world distances the depth image's 0 and 1 stand for. These come from
    // the bake and should not be edited by hand afterwards: they are half of
    // what the stored numbers MEAN, and changing one silently moves every wall.
    f32 depthNear = 0.5f;
    f32 depthFar = 100.0f;

    // Pushes the whole plate away from the camera, in world units. A plate is
    // sampled at screen resolution while a character is not, so a foot resting
    // exactly on a baked floor can flicker between the two; a small positive
    // bias settles it in the character's favour. Kept in WORLD units rather
    // than depth units so the number means the same thing at any distance.
    f32 depthBias = 0.0f;

    // Draw the plate at all. Off leaves the live scene visible, which is how a
    // person compares the bake against the geometry it came from.
    bool visible = true;
};

} // namespace ECS
} // namespace Enjin
