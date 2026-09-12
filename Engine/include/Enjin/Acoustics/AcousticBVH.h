#pragma once

// A spatial index over the room, so sound can be traced through it.
//
// Every part of this system is ray work. Early reflections mirror a source
// across surfaces and ask whether the path is clear. Late reverb fires
// thousands of rays and follows them until their energy is gone. Doing either
// against a flat triangle list is quadratic in the room and the whole thing
// stops being realtime at about one room.
//
// Its own index rather than the renderer's: the only BVH in the engine is
// LightBVH, which organises LIGHTS for importance sampling, not triangles. The
// physics backends have their own acceleration structures but they are inside
// Jolt and Box2D and speak their own types.
//
// Built over an AcousticScene, so every hit carries the material of the surface
// it struck. That is the whole reason the scene carries per-triangle materials:
// a reflection has to know it came off carpet rather than tile, and it has to
// know at the moment it bounces.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Audio/AcousticScene.h"

#include <vector>

namespace Enjin {
namespace Acoustics {

struct ENJIN_API RayHit {
    f32 distance = 0.0f;
    u32 triangle = 0;            // index into the scene's triangle list
    i32 material = 0;            // index into the scene's material table
    Math::Vector3 point;
    Math::Vector3 normal;        // unit, facing back along the incoming ray
    bool hit = false;
};

class ENJIN_API AcousticBVH {
public:
    // Build over a scene. The scene must outlive the BVH: the tree stores
    // triangle indices into it rather than copying the geometry, because a
    // carved cave is hundreds of thousands of triangles and duplicating them
    // for the audio thread is megabytes for no benefit.
    void Build(const Audio::AcousticScene& scene);

    void Clear();
    bool IsBuilt() const { return m_Scene != nullptr && !m_Nodes.empty(); }
    usize TriangleCount() const { return m_Triangles.size(); }
    usize NodeCount() const { return m_Nodes.size(); }

    // Nearest hit along the ray, or a miss.
    //
    // `maxDistance` bounds the search: a reverb ray that has run out of energy
    // should stop, not keep traversing the level.
    RayHit Raycast(const Math::Vector3& origin, const Math::Vector3& direction,
                   f32 maxDistance = 1.0e9f) const;

    // Is anything at all between these two points?
    //
    // Separate from Raycast because it can stop at the FIRST hit rather than
    // finding the nearest, which is most of the cost. Every image-source
    // candidate needs two of these and nothing else, so this is the hot path
    // for early reflections.
    bool Occluded(const Math::Vector3& from, const Math::Vector3& to,
                  f32 epsilon = 1.0e-3f) const;

    // The whole room's bounds, which is what a late-reverb estimate needs to
    // know how big the space is before it fires a single ray.
    Math::Vector3 BoundsMin() const { return m_BoundsMin; }
    Math::Vector3 BoundsMax() const { return m_BoundsMax; }

private:
    struct Node {
        Math::Vector3 lo, hi;
        u32 start = 0, count = 0;   // leaf: range into m_Triangles
        u32 left = 0, right = 0;    // interior: child indices (0 = none)
        bool IsLeaf() const { return count > 0; }
    };

    u32 BuildRange(u32 start, u32 count, u32 depth);

    const Audio::AcousticScene* m_Scene = nullptr;
    std::vector<u32> m_Triangles;       // permuted triangle indices
    std::vector<Node> m_Nodes;
    std::vector<Math::Vector3> m_Centroids;
    Math::Vector3 m_BoundsMin, m_BoundsMax;
};

} // namespace Acoustics
} // namespace Enjin
