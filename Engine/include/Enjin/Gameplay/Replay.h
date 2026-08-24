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

struct ReplayData {
    u32 version = 1;
    std::string engineVersion;   // informational; mismatches warn, not reject
    f32 fixedDt = 1.0f / 60.0f;  // the timestep every frame was simulated with
    u32 rngSeed = 0;             // seed of the shared script-RNG stream for this
                                 // session (Math::SetRandomSeed); 0 = unseeded
                                 // legacy replay, stream state not reproduced
    std::string sceneJson;       // full scene snapshot at record start (self-contained)
    std::vector<ReplayFrame> frames;
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
