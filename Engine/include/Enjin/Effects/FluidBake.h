#pragma once
// A recorded fluid simulation: solve once at author time, play the recording
// back at runtime.
//
// This is what the grid solver is FOR. Solving in the frame costs about 56 ms
// for one 48^3 volume at the shipped defaults (Tests/Integration/FluidBench),
// which is 18 fps for a single campfire and does not get better by being
// clever about it -- a grid pays for its whole box whether or not there is
// smoke in it. Solving offline costs nothing at runtime, removes every reason
// to keep the resolution low, and lets the sim take as long as it likes to
// look right.
//
// Two shapes of recording, and the difference matters:
//
//   A LOOP -- a river through a town, a chimney that never stops. The sim is
//   run until it settles and then recorded; playback wraps. Frame N does not
//   match frame 0, so wrapping pops unless the loop is blended, which is what
//   `loopBlendFrames` is for.
//
//   A ONE-SHOT -- water pouring into a room for a cinematic. Plays once and
//   holds on the last frame. No wrap, no blend, no constraint but storage.
//
// STORAGE is the thing that decides what is possible, so it is worth being
// exact. A 48^3 volume is 125,000 cells counting the padding shell; at one
// float each that is 500 KB per frame and 15 MB per second at 30fps, which is
// not shippable for anything. Three things fix it and all three are here:
//
//   Only DENSITY is recorded. Playback draws, it does not re-simulate, so the
//   three velocity components and the pressure scratch never need storing.
//   That alone is most of the state.
//
//   Density QUANTISES to 8 bits against the take's own peak. Smoke has no
//   detail that survives being drawn as alpha but not 256 levels.
//
//   Most cells are EMPTY. Measured on a saturated 48^3 smoke volume, about a
//   third of cells are above the visible threshold, so a zero-run encoding
//   removes the other two thirds. The bake reports its real ratio rather than
//   this estimate -- see FluidBake::CompressionRatio.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/ECS/World.h"

#include <functional>
#include <string>
#include <vector>

namespace Enjin {
namespace Effects {

// One frame's density field, quantised and zero-run encoded.
//
// The scheme is deliberately dependency-free: a 0x00 byte introduces a run of
// empty cells and is followed by its length, and any other byte is one cell's
// density. A quantised 0 can therefore never appear as a literal, which is
// exactly the value that dominates a smoke field.
ENJIN_API void EncodeFluidDensity(const std::vector<f32>& density, f32 maxDensity,
                                  std::vector<u8>& out);

// Returns false if the stream is malformed or does not decode to `cellCount`
// values, rather than filling what it can: a half-decoded frame is a frame of
// smoke with a corrupt tail, which reads as a simulation bug.
ENJIN_API bool DecodeFluidDensity(const std::vector<u8>& in, f32 maxDensity,
                                  usize cellCount, std::vector<f32>& out);

struct ENJIN_API FluidBake {
    u32 gridSize = 0;                  // N, excluding the padding shell
    bool is3D = true;
    bool looping = false;
    // Frames cross-blended into the start on wrap. 0 = a hard cut, which pops
    // unless the take happens to be periodic. Ignored when `looping` is false.
    u32 loopBlendFrames = 0;
    f32 frameRate = 30.0f;
    Math::Vector3 halfExtents = Math::Vector3(5.0f, 5.0f, 5.0f);
    // The take's peak density, which is what the 8-bit quantisation is scaled
    // against. Stored because decoding without it returns the wrong values,
    // not merely differently scaled ones.
    f32 maxDensity = 1.0f;

    std::vector<std::vector<u8>> frames;

    usize CellCount() const {
        const usize s = static_cast<usize>(gridSize) + 2;
        return is3D ? s * s * s : s * s;
    }
    usize FrameCount() const { return frames.size(); }
    f32 Duration() const {
        return frameRate > 0.0f ? static_cast<f32>(frames.size()) / frameRate : 0.0f;
    }
    usize EncodedBytes() const {
        usize n = 0;
        for (const auto& f : frames) n += f.size();
        return n;
    }
    // Against the same take stored as raw floats, which is what the naive
    // version of this would have cost.
    f64 CompressionRatio() const {
        const usize raw = CellCount() * sizeof(f32) * frames.size();
        const usize enc = EncodedBytes();
        return enc ? static_cast<f64>(raw) / static_cast<f64>(enc) : 0.0;
    }

    // Density for a playback time in seconds, decoded into `out`.
    //
    // Past the end: a loop wraps, a one-shot holds its last frame. Holding
    // rather than vanishing is deliberate -- a cinematic whose water blinks
    // out of existence on the last frame is worse than one that stops moving.
    bool SampleAt(f32 timeSeconds, std::vector<f32>& out) const;

    bool Save(const std::string& path) const;
    bool Load(const std::string& path);
};

// How a take is recorded. The frame budget, the 48 grid clamp and the
// iteration clamp all exist because the solver normally runs inside a frame;
// a bake has no frame, so it overrides them.
struct ENJIN_API FluidBakeSettings {
    f32 frameRate = 30.0f;
    f32 duration = 4.0f;          // seconds of footage to keep
    f32 settleTime = 2.0f;        // simulated and DISCARDED first, so the take
                                  // starts from a developed plume rather than
                                  // from an empty grid filling up
    bool looping = false;
    u32 loopBlendFrames = 0;
    bool useSceneColliders = true;
};

// Run the solver headless and record it. `volume` must carry a
// FluidVolumeComponent; obstacles are voxelised once from `world` before the
// settle pass, because a bake runs against static geometry by definition.
//
// Progress is reported as 0..1 when `onProgress` is given -- a 128^3 take is
// minutes of solving and a tool that looks hung is a tool people kill.
ENJIN_API bool BakeFluid(ECS::World* world, ECS::Entity volume,
                         const FluidBakeSettings& settings,
                         FluidBake& out,
                         const std::function<void(f32)>& onProgress = {});

} // namespace Effects
} // namespace Enjin
