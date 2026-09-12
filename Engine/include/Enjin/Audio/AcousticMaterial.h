#pragma once

// What a surface does to sound that hits it.
//
// Every triangle in the audio scene was handed the same material -- one default
// described in a comment as "roughly concrete/wood" -- so a carpeted basement
// and a tiled kitchen reflected identically, and no amount of tuning the reverb
// could separate them because nothing about them differed.
//
// The numbers here are absorption coefficients: the fraction of energy a
// surface swallows rather than returns, per frequency band. They are the
// standard published figures used in room acoustics, and the interesting thing
// about them is how UNLIKE each other the common materials are. Concrete
// returns about 98% of everything. Heavy carpet returns 92% of the lows and 40%
// of the highs, which is why a carpeted room sounds dull rather than quiet.
// That difference is the whole reason a basement and a kitchen sound different,
// and it was the thing being thrown away.
//
// Keyed on ECS::SurfaceMaterial, which already exists and is already authored:
// the enum's own comment says it is "shared between collision audio and
// material interaction". An entity should say what it is made of ONCE and have
// that drive what it sounds like when struck AND what it does to sound passing
// by. A second parallel acoustic enum would be a second place to disagree.
//
// Plain data on purpose. No phonon.h, no IPL types, nothing behind a build
// flag: this compiles and is testable whether or not the Steam Audio SDK is
// present, and the conversion to IPLMaterial happens at the one place that
// talks to the SDK.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Components/Gameplay.h"   // SurfaceMaterial, AudioCollisionComponent
#include "Enjin/ECS/Components/Material.h"

#include <vector>

namespace Enjin {
namespace Audio {

// The three bands Steam Audio works in: roughly low / mid / high.
constexpr u32 kAcousticBands = 3;

struct ENJIN_API AcousticProperties {
    // Fraction of energy absorbed rather than reflected, per band. 0 returns
    // everything (a mirror), 1 returns nothing (an open window).
    f32 absorption[kAcousticBands] = {0.1f, 0.2f, 0.3f};

    // How much a reflection is spread rather than mirrored. Rough surfaces
    // scatter; polished ones do not. This is what separates a brick wall from
    // a pane of glass even though both are hard.
    f32 scattering = 0.05f;

    // Fraction that passes THROUGH the surface, per band. What you hear of the
    // room next door. Lows pass through walls far more readily than highs,
    // which is why a distant party is audible as bass.
    f32 transmission[kAcousticBands] = {0.04f, 0.01f, 0.005f};
};

// The acoustic behaviour of a surface kind.
//
// Values are the ordinary published ranges for these materials, not invented
// ones. Where a real material varies a lot with construction (wood on studs
// versus solid timber) the figure is the common case for a room you would
// stand in.
ENJIN_API AcousticProperties AcousticsFor(ECS::SurfaceMaterial surface);

// True when two surfaces would behave identically, which is what lets the
// scene builder collapse a hundred concrete walls into one material entry
// instead of a hundred.
ENJIN_API bool AcousticsEqual(const AcousticProperties& a, const AcousticProperties& b);

// What an entity is made of, acoustically.
//
// Precedence, written down in ONE place because two places would eventually
// disagree and the symptom would be a wall that sounds like concrete when
// struck and like carpet when reflecting:
//
//   1. AudioCollisionComponent::material -- already authored, already means
//      exactly this, and already appears in the inspector.
//   2. MaterialComponent::surfaceMaterial -- for the ordinary case of a wall
//      that has a look but no impact sounds. Most geometry in a room is this.
//   3. Default -- deliberately unremarkable, so an unlabelled room does not
//      ring like a cathedral.
//
// Takes the component pointers rather than a World so it stays callable from
// the audio path without dragging the ECS in, and so it can be tested without
// building a world.
ENJIN_API ECS::SurfaceMaterial ResolveSurface(const ECS::AudioCollisionComponent* collision,
                                              const ECS::MaterialComponent* material);

// A list of distinct acoustic materials, plus which one each triangle uses.
//
// Steam Audio wants a material ARRAY and a per-triangle index into it, so a
// scene of a hundred concrete walls should carry one concrete entry and a
// hundred indices -- not a hundred identical entries. This builds that mapping
// without knowing anything about Steam Audio, which is what makes it testable
// on a machine with no SDK.
class ENJIN_API AcousticMaterialTable {
public:
    // Returns the index for this surface, adding it if new.
    u32 IndexFor(ECS::SurfaceMaterial surface);

    usize Count() const { return m_Properties.size(); }
    const AcousticProperties& At(usize i) const { return m_Properties[i]; }
    ECS::SurfaceMaterial SurfaceAt(usize i) const { return m_Surfaces[i]; }

private:
    std::vector<AcousticProperties> m_Properties;
    std::vector<ECS::SurfaceMaterial> m_Surfaces;
};

} // namespace Audio
} // namespace Enjin
