#pragma once
//
// Rooms built in code, so a test can change exactly one thing.
//
// Everything measured so far was measured through a scene file: entities,
// colliders, a serializer, the collider gatherer, and only then the tracer.
// That is the right thing to test ONCE -- it is the path a game takes -- but it
// is a terrible instrument. Six layers sit between the number you set and the
// number you read, the geometry is whatever somebody typed into a generator,
// and "the RT60 moved" never isolates WHY.
//
// These build an AcousticScene directly: exact vertices, exact materials, no
// ECS and no file. A volume sweep then really is a volume sweep, with the
// absorption held still to the last bit, and a disagreement with the closed
// form is the tracer's and nobody else's.
//
// Winding: every face is wound outward, i.e. normals point OUT of the box, the
// way a room's shell would be if you were outside it. The tracer faces normals
// back along the ray and the BVH is double-sided, so this is a convention
// rather than a correctness requirement -- but a convention nobody wrote down
// is one somebody eventually contradicts.

#include "Enjin/Audio/AcousticScene.h"
#include "Enjin/Audio/AcousticMaterial.h"
#include "Enjin/ECS/Components/SurfaceMaterial.h"
#include "Enjin/Math/Vector.h"

#include <array>

namespace RoomBuilder {

using Enjin::f32;
using Enjin::i32;
using Enjin::u32;
using Enjin::usize;
using Enjin::Math::Vector3;
using Enjin::ECS::SurfaceMaterial;

// The six faces of a room, named the way a person would name them.
struct Surfaces {
    SurfaceMaterial floor   = SurfaceMaterial::Concrete;
    SurfaceMaterial ceiling = SurfaceMaterial::Concrete;
    SurfaceMaterial walls   = SurfaceMaterial::Concrete;

    static Surfaces All(SurfaceMaterial m) { return Surfaces{ m, m, m }; }
};

struct Dimensions {
    f32 width = 10.0f;    // X
    f32 height = 3.0f;    // Y
    f32 depth = 8.0f;     // Z

    f32 Volume() const { return width * height * depth; }
    f32 SurfaceArea() const {
        return 2.0f * (width * depth + width * height + height * depth);
    }
    // The mean distance between bounces in a convex enclosure. Not a fit and
    // not an approximation: 4V/S is exact for any convex shape, which makes it
    // the one number a ray tracer can be checked against with no fudge.
    f32 MeanFreePath() const { return 4.0f * Volume() / SurfaceArea(); }
    Vector3 Centre() const { return Vector3(0.0f, height * 0.5f, 0.0f); }
};

// A sealed rectangular room centred on the origin, floor at y = 0.
//
// `openCeiling` leaves the top face out entirely, which is a courtyard rather
// than a room: the energy that would have come back down leaves instead, and
// the difference is most of what "outdoors" means acoustically.
inline Enjin::Audio::AcousticScene Box(const Dimensions& dim,
                                       const Surfaces& surf,
                                       bool openCeiling = false) {
    Enjin::Audio::AcousticScene scene;

    const f32 hw = dim.width * 0.5f;
    const f32 hd = dim.depth * 0.5f;
    const f32 h = dim.height;

    const u32 mFloor = scene.materials.IndexFor(surf.floor);
    const u32 mCeil = scene.materials.IndexFor(surf.ceiling);
    const u32 mWall = scene.materials.IndexFor(surf.walls);

    auto quad = [&scene](Vector3 a, Vector3 b, Vector3 c, Vector3 d, u32 material) {
        const i32 base = static_cast<i32>(scene.vertices.size());
        scene.vertices.push_back(a);
        scene.vertices.push_back(b);
        scene.vertices.push_back(c);
        scene.vertices.push_back(d);
        const i32 tris[6] = { base, base + 1, base + 2, base, base + 2, base + 3 };
        for (i32 t : tris) scene.indices.push_back(t);
        scene.materialIndices.push_back(static_cast<i32>(material));
        scene.materialIndices.push_back(static_cast<i32>(material));
    };

    // Floor (normal down/out) and ceiling (normal up/out).
    quad(Vector3(-hw, 0, -hd), Vector3(hw, 0, -hd), Vector3(hw, 0, hd), Vector3(-hw, 0, hd), mFloor);
    if (!openCeiling) {
        quad(Vector3(-hw, h, hd), Vector3(hw, h, hd), Vector3(hw, h, -hd), Vector3(-hw, h, -hd), mCeil);
    }

    // Four walls.
    quad(Vector3(-hw, 0, -hd), Vector3(-hw, h, -hd), Vector3(hw, h, -hd), Vector3(hw, 0, -hd), mWall);
    quad(Vector3(hw, 0, hd), Vector3(hw, h, hd), Vector3(-hw, h, hd), Vector3(-hw, 0, hd), mWall);
    quad(Vector3(-hw, 0, hd), Vector3(-hw, h, hd), Vector3(-hw, h, -hd), Vector3(-hw, 0, -hd), mWall);
    quad(Vector3(hw, 0, -hd), Vector3(hw, h, -hd), Vector3(hw, h, hd), Vector3(hw, 0, hd), mWall);

    return scene;
}

// The same sealed box, with a rectangular hole of known area in one wall.
//
// This exists to answer a question the Range raised and could not settle: its
// hard rooms measure about 25% below their sealed twins, which is far more than
// a 3.52 m^2 doorway onto a dead corridor should cost. In a building there are
// a dozen candidate explanations and no way to isolate one. Here there is
// exactly one opening, of exactly one area, onto nothing at all -- so what a
// hole costs can be measured against what the closed form says it should.
//
// The hole is centred in the -Z wall and the wall is rebuilt as the four strips
// around it, so the opening is a true absence of geometry rather than a surface
// with a high coefficient. Those are not the same thing and it matters: a
// perfect absorber still ends a ray where it stands, while a hole lets it
// leave, and only the second one can also let it come back.
inline Enjin::Audio::AcousticScene BoxWithOpening(const Dimensions& dim,
                                                  const Surfaces& surf,
                                                  f32 openingWidth,
                                                  f32 openingHeight) {
    Enjin::Audio::AcousticScene scene;

    const f32 hw = dim.width * 0.5f;
    const f32 hd = dim.depth * 0.5f;
    const f32 h = dim.height;

    const u32 mFloor = scene.materials.IndexFor(surf.floor);
    const u32 mCeil = scene.materials.IndexFor(surf.ceiling);
    const u32 mWall = scene.materials.IndexFor(surf.walls);

    auto quad = [&scene](Vector3 a, Vector3 b, Vector3 c, Vector3 d, u32 material) {
        const i32 base = static_cast<i32>(scene.vertices.size());
        scene.vertices.push_back(a);
        scene.vertices.push_back(b);
        scene.vertices.push_back(c);
        scene.vertices.push_back(d);
        const i32 tris[6] = { base, base + 1, base + 2, base, base + 2, base + 3 };
        for (i32 t : tris) scene.indices.push_back(t);
        scene.materialIndices.push_back(static_cast<i32>(material));
        scene.materialIndices.push_back(static_cast<i32>(material));
    };

    quad(Vector3(-hw, 0, -hd), Vector3(hw, 0, -hd), Vector3(hw, 0, hd), Vector3(-hw, 0, hd), mFloor);
    quad(Vector3(-hw, h, hd), Vector3(hw, h, hd), Vector3(hw, h, -hd), Vector3(-hw, h, -hd), mCeil);

    // Three intact walls.
    quad(Vector3(hw, 0, hd), Vector3(hw, h, hd), Vector3(-hw, h, hd), Vector3(-hw, 0, hd), mWall);
    quad(Vector3(-hw, 0, hd), Vector3(-hw, h, hd), Vector3(-hw, h, -hd), Vector3(-hw, 0, -hd), mWall);
    quad(Vector3(hw, 0, -hd), Vector3(hw, h, -hd), Vector3(hw, h, hd), Vector3(hw, 0, hd), mWall);

    // The -Z wall, as four strips around a centred hole.
    const f32 ow = openingWidth * 0.5f;
    const f32 oh = openingHeight;
    if (ow > 0.0f && oh > 0.0f) {
        if (hw - ow > 0.001f) {
            quad(Vector3(-hw, 0, -hd), Vector3(-hw, h, -hd), Vector3(-ow, h, -hd), Vector3(-ow, 0, -hd), mWall);
            quad(Vector3(ow, 0, -hd), Vector3(ow, h, -hd), Vector3(hw, h, -hd), Vector3(hw, 0, -hd), mWall);
        }
        if (h - oh > 0.001f) {
            quad(Vector3(-ow, oh, -hd), Vector3(-ow, h, -hd), Vector3(ow, h, -hd), Vector3(ow, oh, -hd), mWall);
        }
    } else {
        quad(Vector3(-hw, 0, -hd), Vector3(-hw, h, -hd), Vector3(hw, h, -hd), Vector3(hw, 0, -hd), mWall);
    }

    return scene;
}

// Sabine's RT60 for a box of these dimensions and these surfaces.
//
// RT60 = 0.161 V / (S a), with `a` the area-weighted mean absorption. This is
// the closed form every measurement in here is checked against, per band, and
// it is worth being clear about what agreement does and does not prove: Sabine
// assumes a diffuse field and gets optimistic in very absorbent rooms (it
// cannot return 0 even at a = 1, where the true answer is instant). So it is a
// strong check in the middle of the range and a weak one at the dead end, and
// the tests say which they are relying on.
inline f32 SabineRT60(const Dimensions& dim, const Surfaces& surf, u32 band,
                      bool openCeiling = false) {
    using Enjin::Audio::AcousticsFor;

    const f32 floorArea = dim.width * dim.depth;
    const f32 wallArea = 2.0f * (dim.width * dim.height + dim.height * dim.depth);

    const f32 aFloor = AcousticsFor(surf.floor).absorption[band];
    const f32 aCeil = AcousticsFor(surf.ceiling).absorption[band];
    const f32 aWall = AcousticsFor(surf.walls).absorption[band];

    f32 absorbed = floorArea * aFloor + wallArea * aWall;
    f32 area = floorArea + wallArea;
    if (!openCeiling) {
        absorbed += floorArea * aCeil;
        area += floorArea;
    } else {
        // An opening is a perfect absorber: everything that reaches it leaves.
        absorbed += floorArea * 1.0f;
        area += floorArea;
    }

    if (absorbed < 1.0e-6f) return 0.0f;
    return 0.161f * dim.Volume() / absorbed;
}

} // namespace RoomBuilder
