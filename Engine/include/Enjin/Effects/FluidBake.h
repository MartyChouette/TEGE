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

#include <atomic>
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

// What a take IS, without reading a single frame of it.
//
// A browser row, a hover tooltip and a picker list all want the grid, the
// length and whether it loops. Loading the file to answer that reads megabytes
// per entry, so this reads only the 48-byte header -- and it is the SAME
// parser FluidBake::Load uses, so a format change cannot leave the two
// disagreeing about where a field lives.
struct ENJIN_API FluidBakeInfo {
    u32 gridSize = 0;
    bool is3D = true;
    bool looping = false;
    u32 loopBlendFrames = 0;
    f32 frameRate = 30.0f;
    Math::Vector3 halfExtents = Math::Vector3(5.0f, 5.0f, 5.0f);
    f32 maxDensity = 1.0f;
    u32 frameCount = 0;

    f32 Duration() const {
        return frameRate > 0.0f ? static_cast<f32>(frameCount) / frameRate : 0.0f;
    }
};

// False for a missing file, a file that is not a recording, and a recording
// written by a different format version -- the three cases a caller must not
// tell apart by guessing at the extension.
ENJIN_API bool ReadFluidBakeInfo(const std::string& path, FluidBakeInfo& out);

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

    // Per frame: 1 = a KEYFRAME, decodable on its own; 0 = a DELTA against the
    // frame before it. Kept exactly in step with `frames` -- Save and Load are
    // the only two places that build either, and both refuse a file where the
    // two disagree.
    //
    // Deltas exist because consecutive frames of a take are nearly identical
    // and the first version of this format exploited none of that: it stored
    // every frame whole, so a cell that never changed cost a byte in every
    // frame of the take. A run of UNCHANGED cells now costs two bytes however
    // long it is, which is what a settled pool or the still interior of a
    // plume actually is.
    std::vector<u8> frameIsKey;

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

private:
    // Decoded quantised frame, and which frame it is.
    //
    // A delta frame needs the one before it, so decoding frame N means walking
    // from the last keyframe at or before N. Sequential playback -- which is
    // every playback -- then costs ONE delta apply per frame because the cache
    // already holds N-1. A seek costs at most the keyframe interval.
    mutable std::vector<u8> m_CacheQuantised;
    mutable i64 m_CacheIndex = -1;

    // Quantised frame `index`, decoded through the chain. False on a malformed
    // take rather than a partially filled one.
    bool DecodeQuantisedFrame(i64 index, std::vector<u8>& out) const;
};

// One recording found on disk, as a picker needs it.
//
// `readable` false means the extension matched and the header did not -- a
// file being listed as a take it cannot be is worse than it not appearing,
// because the failure then happens later, in playback, with no clue attached.
struct ENJIN_API FluidTakeEntry {
    std::string relativePath;   // project-relative, forward slashes
    FluidBakeInfo info;
    bool readable = false;
};

// Every .enjfluid under `projectDir`/assets, sorted by path.
//
// Under assets/ only, because that is the one tree BuildPipeline copies into
// an exported game -- a recording anywhere else plays in the editor and is
// missing from every build, which is the worst shape of bug: it works
// everywhere you would test it.
//
// `searchedDir`, when given, comes back as the directory that was scanned, so
// an empty result can say WHERE it looked. "No recordings" on its own is
// indistinguishable from looking in the wrong project.
ENJIN_API std::vector<FluidTakeEntry> FindFluidRecordings(const std::string& projectDir,
                                                          std::string* searchedDir = nullptr);

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

// Everything a bake needs from the scene, COPIED, so the solve itself can run
// anywhere.
//
// A bake is seconds of solving for a grid-48 take and minutes for a 128, which
// is a frozen editor unless it moves off the main thread. It cannot simply be
// handed a World to do that: ECS reads are lock-free ONLY because structural
// mutation is owner-thread-only (adr-0004), so a solver walking the world from
// a worker would be reading components while the editor added and removed
// them. Taking the snapshot on the owner thread and solving from it is what
// makes the worker legal rather than merely lucky.
struct ENJIN_API FluidBakeInput {
    bool valid = false;
    u32 gridSize = 0;               // already clamped the way the live solver clamps
    bool is3D = true;
    Math::Vector3 halfExtents = Math::Vector3(5.0f, 5.0f, 5.0f);
    FluidSimulation::FluidStepParams params;
    std::vector<u8> obstacles;      // empty = none, as everywhere else
};

// Read the scene. MUST be called on the world's owner thread.
ENJIN_API FluidBakeInput PrepareFluidBake(ECS::World* world, ECS::Entity volume,
                                          const FluidBakeSettings& settings);

// Solve a prepared bake. Touches no world, so this is the half that may run on
// a worker thread.
//
// `cancel`, when given, is polled every frame: a bake a person cannot stop is
// a bake they will kill the editor to escape. A cancelled bake returns false
// and leaves `out` alone rather than handing back a truncated take, because a
// short recording looks like a bad simulation rather than an abandoned one.
ENJIN_API bool BakeFluidPrepared(const FluidBakeInput& input,
                                 const FluidBakeSettings& settings,
                                 FluidBake& out,
                                 const std::function<void(f32)>& onProgress = {},
                                 const std::atomic<bool>* cancel = nullptr);

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
