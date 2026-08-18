#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vector>

namespace Enjin {
namespace ECS {

// Which cloth points are fixed in place (they follow the entity's transform,
// so moving the entity drags the cloth with it).
enum class ClothPin : u8 { TopEdge, TopCorners, LeftEdge, AllCorners, None };

// Seam layout for sewn-fabric tearing: links ON a seam are stitched (stronger),
// links right NEXT to a seam are slightly weaker (stress concentrates beside
// stitching), so tears run along the panel lines instead of spreading evenly.
enum class ClothSeams : u8 { None, Vertical, Horizontal, Grid };

// Grid cloth (flags, capes, curtains, banners). The system generates an
// resX x resY grid mesh hanging down from the entity and simulates it with
// position-based dynamics: gravity + wind -> distance constraints -> optional
// tearing. The entity's MeshComponent is (re)written every frame.
//
// tearable is the "destructible or not" switch: when on, any constraint
// stretched past tearThreshold x its rest length snaps, and the triangles on
// that edge are removed — the fabric visibly rips.
struct ClothComponent {
    // --- Authoring (serialized) ---
    f32 width = 2.0f;              // world width of the sheet
    f32 height = 2.0f;             // world height (hangs down from the entity)
    i32 resX = 16;                 // grid points across (>= 2)
    i32 resY = 16;                 // grid points down (>= 2)
    i32 iterations = 6;            // constraint solver passes (more = stiffer)
    f32 damping = 0.02f;           // velocity damping per step (0-0.2 sane)
    f32 gravityScale = 1.0f;
    Math::Vector3 wind = {0, 0, 0};  // world-space wind force
    ClothPin pin = ClothPin::TopEdge;
    bool tearable = false;
    f32 tearThreshold = 1.6f;      // stretch factor that snaps a constraint
    bool collide = true;           // push points out of Box/Sphere/Capsule colliders
    f32 collisionSkin = 0.04f;     // clearance kept off collider surfaces
    f32 friction = 0.5f;           // 0 = slide freely on contact, 1 = stick

    // Sewn-fabric tear tuning (bakes into per-link strength at build time).
    f32 pinStrength = 1.0f;        // links touching pinned points: >1 reinforced, <1 rips off the pins first
    ClothSeams seams = ClothSeams::None;
    i32 seamSpacing = 6;           // grid cells between seam lines
    f32 seamStrength = 1.8f;       // stitched links on the seam (neighbors get ~0.85 automatically)

    // --- Runtime state (owned by ClothSystem; not serialized) ---
    struct Constraint { i32 a; i32 b; f32 rest; f32 strength = 1.0f; };
    struct Tri { u32 i0, i1, i2; bool alive; };
    bool initialized = false;      // set false (or press Reset) to rebuild
    std::vector<Math::Vector3> positions;      // world space
    std::vector<Math::Vector3> prevPositions;  // Verlet
    std::vector<f32> invMass;                  // 0 = pinned
    std::vector<Math::Vector3> restLocal;      // grid rest positions (local)
    std::vector<Constraint> constraints;
    std::vector<Tri> tris;
    bool meshDirty = false;        // vertices changed -> re-upload
    bool topologyDirty = false;    // a tear removed triangles -> rebuild buffers
};

} // namespace ECS
} // namespace Enjin
