#pragma once

// What a surface is made of.
//
// One enum, read by three unrelated systems: collision audio picks an impact
// sound from it, material interaction picks a response from it, and the audio
// scene builder picks absorption and scattering from it. An entity says what it
// is made of ONCE.
//
// It lives in its own header because both Gameplay.h and Material.h need it and
// neither should have to include the other. Gameplay.h carries dialogue trees
// and rewind ring buffers; Material.h is included by most of the renderer. The
// enum is twenty bytes of vocabulary and should cost neither of them anything.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS {

// APPEND ONLY. The ordinal is what a scene file stores, so inserting a value in
// the middle turns every saved Glass into Flesh. The six after Ice were added
// for room acoustics: the audio scene needs to tell a carpeted basement from a
// tiled kitchen, and the original ten had no word for either.
enum class SurfaceMaterial : u8 {
    Default, Metal, Wood, Stone, Glass, Flesh, Water, Dirt, Grass, Ice,
    Concrete, Carpet, Drywall, Tile, Brick, Fabric,
    Count
};

inline const char* SurfaceMaterialName(SurfaceMaterial m) {
    static const char* names[] = {
        "Default","Metal","Wood","Stone","Glass","Flesh","Water","Dirt","Grass","Ice",
        "Concrete","Carpet","Drywall","Tile","Brick","Fabric",
    };
    // Bounded by the enum rather than by a literal. The previous version tested
    // against a hardcoded 10, so appending to the enum would have returned a
    // name from past the end of the array -- reading whatever followed it in
    // memory and calling that a material.
    static_assert(sizeof(names) / sizeof(names[0]) == static_cast<usize>(SurfaceMaterial::Count),
                  "every SurfaceMaterial needs a name, in enum order");
    return (static_cast<u8>(m) < static_cast<u8>(SurfaceMaterial::Count))
               ? names[static_cast<u8>(m)] : "Unknown";
}

} // namespace ECS
} // namespace Enjin
