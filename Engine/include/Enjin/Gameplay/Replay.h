#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Gameplay {

// A shareable, replayable play session: the scene as it was when play started,
// a fixed timestep, and one input snapshot per simulated frame. The file format
// is plain JSON (see docs/OPENNESS.md) - a replay is data you own, not a blob.
//
// Determinism scope (v1): same build, same machine, same platform. The engine's
// procgen is fully seeded and physics steps at the fixed dt, but visual-only
// systems (GPU particles, weather drift) may diverge - the gameplay-relevant
// state follows the recorded inputs.
struct ReplayFrame {
    std::vector<u16> keysDown;   // KeyCode indices held this frame (sparse)
    u8  mouseMask = 0;           // bit i = mouse button i held
    f32 mouseX = 0.0f;
    f32 mouseY = 0.0f;
    f32 dt = 1.0f / 60.0f;       // the delta this frame was simulated with -
                                 // replay feeds the exact dt stream back, so
                                 // recording never alters live play speed
};

// Where a named entity ended up when the recording stopped. Playback snaps
// matching entities here when the stream ends, so a replay finishes exactly
// where the session did - without this, tiny simulation drift compounds over
// the run and the final frames land somewhere the original session never went
// (the recorded character kept walking off a ledge).
struct ReplayEndEntity {
    std::string name;            // matched by NameComponent in the replayed scene
    Math::Vector3 position{};
    f32 rotX = 0, rotY = 0, rotZ = 0, rotW = 1;   // quaternion
};

// A marked moment in the session: the playtester pressed the mark key, or a
// script exception fired. Carried in the replay file so the developer can jump
// straight to the frame where the bug happened.
struct ReplayBookmark {
    u32 frame = 0;               // index into frames[]
    f32 time = 0.0f;             // seconds from session start (sum of dt)
    std::string label;
};

struct ReplayData {
    u32 version = 1;
    std::string engineVersion;   // informational; mismatches warn, not reject
    f32 fixedDt = 1.0f / 60.0f;  // the timestep every frame was simulated with
    u32 rngSeed = 0;             // seed of the shared script-RNG stream for this
                                 // session (Math::SetRandomSeed); 0 = unseeded
                                 // legacy replay, stream state not reproduced
    std::string sceneJson;       // full scene snapshot at record start (self-contained)
    std::vector<ReplayFrame> frames;
    std::vector<ReplayEndEntity> endState;   // final transforms of named entities
    std::vector<ReplayBookmark> bookmarks;   // marked moments (F8 / script exceptions)
};

// JSON (de)serialization. Parse is tolerant of missing optional fields and
// returns false only for structurally unusable input.
ENJIN_API std::string SerializeReplay(const ReplayData& replay);
ENJIN_API bool ParseReplay(const std::string& text, ReplayData& out);

// Bridge to Input's 512-key / 8-button state buffers.
ENJIN_API void ReplayFrameFromBuffers(ReplayFrame& out, const bool* keys512,
                                      const bool* mouse8, Math::Vector2 mousePos);
ENJIN_API void ReplayFrameToBuffers(const ReplayFrame& frame, bool* keys512,
                                    bool* mouse8, Math::Vector2& mousePos);

} // namespace Gameplay
} // namespace Enjin
