#pragma once

// Baking light into three basis textures.
//
// This is the expensive half of radiosity normal mapping, and the half that
// only ever runs offline. It answers, for every texel of every surface: how
// much light arrives here from each of the three basis directions? What ships
// is the answer -- three textures -- and the run-time cost of the whole system
// is reading them.
//
// Deliberately free of ECS and GPU. A bake is a pure function of geometry and
// lights, which is what lets it be tested against scenes whose answer is known
// by hand (a floor under a light is lit; a floor under a lid is not) instead of
// by looking at it.
//
// What it computes, and what it does not:
//   - Direct light from the scene's lights, shadowed by the scene's own
//     geometry. Shadows are the main product of baking; a lightmap without them
//     is barely worth the texture memory.
//   - Sky light, sampled around each basis direction, which is where the soft
//     ambient occlusion in corners comes from.
//   - NOT bounced light between surfaces yet. That is what the "radiosity" in
//     the name eventually means, and it is a second pass over this same
//     machinery rather than a different design.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Renderer/RadiosityNormalMap.h"

#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

// One triangle as the baker needs it: world positions, the shading frame at
// each corner, and where it lives in the lightmap atlas.
struct BakeTriangle {
    Math::Vector3 position[3];
    Math::Vector3 normal[3];
    // xyz = tangent, w = handedness, matching the mesh vertex convention. The
    // basis lives in TANGENT space, so without this there is no way to know
    // which direction "basis 0" points in the world.
    Math::Vector4 tangent[3];
    Math::Vector2 uv1[3];
};

struct BakeLight {
    enum class Type : u8 { Directional = 0, Point = 1 };
    Type type = Type::Directional;
    // Directional: the direction light TRAVELS. Point: world position.
    Math::Vector3 vector;
    Math::Vector3 color = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 intensity = 1.0f;
    f32 range = 50.0f;      // point lights only
    bool castsShadow = true;
};

struct LightmapBakeOptions {
    u32 atlasSize = 512;

    // Rays per texel per basis direction for the sky term. This is the entire
    // quality/time dial: the bake is O(texels * 3 * skySamples) ray casts. 16
    // is grainy and quick, 64 is smooth and slow, and because it runs offline
    // the honest default is the one that looks right rather than the one that
    // finishes fast.
    u32 skySamples = 32;

    Math::Vector3 skyColor = Math::Vector3(0.45f, 0.55f, 0.75f);
    Math::Vector3 groundColor = Math::Vector3(0.20f, 0.18f, 0.15f);
    f32 skyIntensity = 1.0f;

    // How far a shadow ray starts from the surface, in world units. Too small
    // and a surface shadows itself into a dark speckle; too large and contact
    // shadows lift off their corners.
    f32 rayBias = 0.002f;

    // Deterministic, so a rebake of unchanged geometry produces identical
    // bytes and a project's diff stays reviewable.
    u32 seed = 12345;

    // Texels of dilation around each island. Must be at least the unwrap's
    // padding or filtering pulls unlit black in from the margin.
    u32 dilate = 2;
};

struct LightmapBakeResult {
    bool ok = false;
    std::string error;
    // Three RGB8 atlases, atlasSize * atlasSize * 3 bytes each, in basis order.
    std::vector<u8> basis[kRNMBasisCount];
    u32 litTexels = 0;
    u32 rayCasts = 0;
};

// Bake. Returns three atlases, or an error and nothing.
LightmapBakeResult BakeLightmap(const std::vector<BakeTriangle>& triangles,
                                const std::vector<BakeLight>& lights,
                                const LightmapBakeOptions& options);

// --- Exposed for testing, and useful on its own -----------------------------

// A bounding-volume hierarchy over the bake geometry, which is what makes
// shadow rays affordable: a scene of a few thousand triangles is thousands of
// ray casts per texel otherwise.
class ENJIN_API BakeRayScene {
public:
    void Build(const std::vector<BakeTriangle>& triangles);
    // Anything hit between the origin and maxDistance. Occlusion only -- the
    // baker never needs to know WHAT it hit, just whether the light is blocked.
    bool Occluded(const Math::Vector3& origin, const Math::Vector3& direction, f32 maxDistance) const;
    u32 NodeCount() const { return static_cast<u32>(m_Nodes.size()); }
    u32 TriangleCount() const { return static_cast<u32>(m_Tris.size()); }

private:
    struct Tri { Math::Vector3 p0, e1, e2; };
    struct Node {
        Math::Vector3 lo, hi;
        u32 first = 0, count = 0, right = 0;   // count 0 = interior, use right
    };
    u32 BuildRange(u32 first, u32 count, u32 depth);

    std::vector<Tri> m_Tris;
    std::vector<Node> m_Nodes;
};

} // namespace Renderer
} // namespace Enjin
