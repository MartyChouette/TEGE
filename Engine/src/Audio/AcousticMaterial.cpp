#include "Enjin/Audio/AcousticMaterial.h"

#include <cmath>

namespace Enjin {
namespace Audio {

namespace {

// absorption low / mid / high, scattering, transmission low / mid / high.
//
// Ordered to match ECS::SurfaceMaterial exactly; the static_assert below is
// what keeps them in step, because a table that silently shifted by one would
// make every metal surface sound like wood and nothing would look wrong.
struct Row {
    f32 absLow, absMid, absHigh;
    f32 scatter;
    f32 transLow, transMid, transHigh;
};

constexpr Row kTable[] = {
    // Default -- deliberately unremarkable, between plaster and wood. It is
    // what a surface nobody has labelled sounds like, and it should not be
    // the most reflective option or every unlabelled room rings.
    { 0.10f, 0.15f, 0.20f,  0.10f,  0.030f, 0.010f, 0.005f },

    // Metal -- hard, bright, almost nothing absorbed, and it passes very little.
    { 0.03f, 0.04f, 0.05f,  0.05f,  0.010f, 0.004f, 0.001f },

    // Wood -- panelling over a cavity absorbs LOW frequencies noticeably, which
    // is the opposite of what people expect and is why wooden rooms sound warm
    // rather than boomy.
    { 0.15f, 0.10f, 0.08f,  0.12f,  0.060f, 0.020f, 0.008f },

    // Stone -- rough masonry. Hard like concrete but it scatters far more.
    { 0.02f, 0.03f, 0.05f,  0.35f,  0.010f, 0.003f, 0.001f },

    // Glass -- hard and smooth, so it mirrors, and thin enough to pass a lot.
    { 0.18f, 0.06f, 0.02f,  0.02f,  0.120f, 0.050f, 0.020f },

    // Flesh -- soft and lossy across the band. Bodies in a room are why a full
    // room sounds deader than an empty one.
    { 0.25f, 0.45f, 0.55f,  0.30f,  0.020f, 0.008f, 0.003f },

    // Water -- a hard acoustic mirror from the air side.
    { 0.01f, 0.01f, 0.02f,  0.05f,  0.005f, 0.002f, 0.001f },

    // Dirt -- porous, swallows highs.
    { 0.15f, 0.40f, 0.60f,  0.40f,  0.020f, 0.006f, 0.002f },

    // Grass -- more porous still.
    { 0.20f, 0.50f, 0.70f,  0.50f,  0.030f, 0.010f, 0.004f },

    // Ice -- like water, hard and smooth.
    { 0.01f, 0.02f, 0.03f,  0.08f,  0.008f, 0.003f, 0.001f },

    // --- appended for acoustics ------------------------------------------
    // Appended rather than inserted: the ordinal is what a scene file stores,
    // so inserting in the middle would turn every saved Glass into Flesh.

    // Concrete, sealed -- the most reflective thing in an ordinary building,
    // and the reason a bare basement rings.
    { 0.01f, 0.02f, 0.02f,  0.05f,  0.005f, 0.002f, 0.001f },

    // Carpet, heavy, on concrete -- returns most of the low end and swallows
    // the top. This is the shape of "dull rather than quiet", and it is the
    // single biggest reason two rooms of the same size sound different.
    { 0.08f, 0.30f, 0.60f,  0.20f,  0.015f, 0.005f, 0.002f },

    // Drywall on studs -- a panel over a cavity, so it absorbs lows and passes
    // them too. Why you hear the bass through a bedroom wall and not the words.
    { 0.12f, 0.06f, 0.04f,  0.10f,  0.090f, 0.030f, 0.010f },

    // Tile -- hard, smooth, and the kitchen half of the acceptance case.
    { 0.01f, 0.01f, 0.02f,  0.03f,  0.006f, 0.002f, 0.001f },

    // Brick, unglazed -- hard but rough, so it scatters strongly.
    { 0.03f, 0.03f, 0.05f,  0.40f,  0.010f, 0.004f, 0.001f },

    // Fabric -- curtains, hangings, upholstery. Broadband absorber, the thing
    // you put in a room to stop it ringing.
    { 0.15f, 0.45f, 0.65f,  0.35f,  0.150f, 0.060f, 0.020f },
};

static_assert(sizeof(kTable) / sizeof(kTable[0]) ==
              static_cast<usize>(ECS::SurfaceMaterial::Count),
              "every SurfaceMaterial needs exactly one acoustic row, in the same "
              "order -- a table shifted by one makes every metal surface sound "
              "like wood and nothing looks wrong");

} // namespace

AcousticProperties AcousticsFor(ECS::SurfaceMaterial surface) {
    const usize i = static_cast<usize>(surface);
    const Row& r = (i < sizeof(kTable) / sizeof(kTable[0])) ? kTable[i] : kTable[0];

    AcousticProperties out;
    out.absorption[0] = r.absLow;
    out.absorption[1] = r.absMid;
    out.absorption[2] = r.absHigh;
    out.scattering = r.scatter;
    out.transmission[0] = r.transLow;
    out.transmission[1] = r.transMid;
    out.transmission[2] = r.transHigh;
    return out;
}

ECS::SurfaceMaterial ResolveSurface(const ECS::AudioCollisionComponent* collision,
                                    const ECS::MaterialComponent* material) {
    // Collision audio wins because it is the more specific statement: someone
    // who set it was describing this exact object, not the look it shares with
    // everything using the same material.
    if (collision && collision->material != ECS::SurfaceMaterial::Default) {
        return collision->material;
    }
    if (material && material->surfaceMaterial != ECS::SurfaceMaterial::Default) {
        return material->surfaceMaterial;
    }
    return ECS::SurfaceMaterial::Default;
}

u32 AcousticMaterialTable::IndexFor(ECS::SurfaceMaterial surface) {
    const AcousticProperties props = AcousticsFor(surface);
    for (usize i = 0; i < m_Properties.size(); ++i) {
        // Compared by PROPERTIES, not by enum value: two surface kinds that
        // happen to share a row (water and ice are nearly the same mirror) cost
        // one entry rather than two, and the scene stays as small as it can be.
        if (AcousticsEqual(m_Properties[i], props)) return static_cast<u32>(i);
    }
    m_Properties.push_back(props);
    m_Surfaces.push_back(surface);
    return static_cast<u32>(m_Properties.size() - 1);
}

bool AcousticsEqual(const AcousticProperties& a, const AcousticProperties& b) {
    // Exact rather than tolerant: these come from a table, so two surfaces are
    // either the same row or they are not. A tolerance here would quietly merge
    // tile and concrete, which are close on paper and are exactly the pair the
    // whole feature exists to tell apart.
    for (u32 i = 0; i < kAcousticBands; ++i) {
        if (a.absorption[i] != b.absorption[i]) return false;
        if (a.transmission[i] != b.transmission[i]) return false;
    }
    return a.scattering == b.scattering;
}

} // namespace Audio
} // namespace Enjin
