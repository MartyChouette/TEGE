#pragma once

// The room, as sound sees it.
//
// Gathering the geometry and deciding what each surface is made of has nothing
// to do with Steam Audio. It was living inside `#ifdef ENJIN_AUDIO_STEAM_AUDIO`
// anyway, which had two costs: it was dead code on every build where the SDK is
// absent (which is the default, and this machine), and it could not be tested
// at all -- there is no way to assert that a carpeted floor came through as
// carpet when the function only exists on a machine with a proprietary SDK
// installed.
//
// So the gathering lives here, in plain engine types, and the part that talks
// to the SDK becomes a short conversion at the boundary.
//
// What it collects and why:
//
//   Box and sphere colliders     -- what the previous version collected.
//   MESH colliders               -- what it did not. Brush solids and voxel
//                                   caves produce mesh colliders and nothing
//                                   else, so an entire carved cave system was
//                                   acoustically invisible: a sound inside it
//                                   reflected off nothing.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Audio/AcousticMaterial.h"

#include <vector>

namespace Enjin {
namespace ECS { class World; }

namespace Audio {

struct ENJIN_API AcousticScene {
    std::vector<Math::Vector3> vertices;
    std::vector<i32> indices;            // triangle list
    std::vector<i32> materialIndices;    // one per TRIANGLE, into `materials`
    AcousticMaterialTable materials;

    usize TriangleCount() const { return indices.size() / 3; }
    bool Empty() const { return indices.empty(); }
};

// Options, so a big level can be gathered without producing a mesh the
// simulator chokes on.
struct ENJIN_API AcousticSceneOptions {
    // Mesh colliders can be enormous -- a carved cave is hundreds of thousands
    // of triangles, and sound does not care about a centimetre of surface
    // detail. Above this, a mesh contributes its bounding box instead of its
    // triangles: still the right room shape, at a cost the simulator can bear.
    //
    // Zero means never simplify, which is what a test wants.
    u32 meshTriangleBudget = 20000;

    bool includeBoxColliders = true;
    bool includeSphereColliders = true;
    bool includeMeshColliders = true;
};

// Walk the world and build the acoustic scene.
ENJIN_API AcousticScene BuildAcousticScene(ECS::World* world,
                                           const AcousticSceneOptions& options = {});

} // namespace Audio
} // namespace Enjin
