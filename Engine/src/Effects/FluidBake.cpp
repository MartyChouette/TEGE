#include "Enjin/Effects/FluidBake.h"
#include "Enjin/Effects/FluidObstacles.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Effects {

namespace {

constexpr char kMagic[8] = {'E', 'N', 'J', 'F', 'L', 'U', 'I', 'D'};
// 2 added per-frame DELTAS. Version 1 files still load: every frame in one is
// a keyframe, which is exactly what the old format was.
constexpr u32 kVersion = 2;
constexpr u32 kVersionNoDeltas = 1;

// A keyframe every this many frames. Not a header field: the per-frame kind
// byte says what each frame is, so a take written with a different interval
// loads without the reader needing to be told. It bounds the cost of a SEEK --
// a scrub to an arbitrary frame decodes at most this many.
constexpr u32 kKeyframeInterval = 30;

// A run of empty cells can be at most this long before a second run is
// started. 255 rather than 256 because the length shares a byte and 0 would be
// a run of nothing, which an encoder should never emit and a decoder should
// never have to reason about.
constexpr u32 kMaxRun = 255;

template <typename T>
void Write(std::ofstream& f, const T& v) {
    f.write(reinterpret_cast<const char*>(&v), sizeof(T));
}

template <typename T>
bool Read(std::ifstream& f, T& v) {
    f.read(reinterpret_cast<char*>(&v), sizeof(T));
    return static_cast<bool>(f);
}

// The file header, parsed in ONE place.
//
// Load() and ReadFluidBakeInfo() both start here, so the field order lives in
// a single function and `kHeaderBytes` below is derived from it rather than
// counted by hand -- the seek past the header used to be a literal sum, which
// is a number that goes wrong silently the first time a field is added.
bool ReadHeader(std::ifstream& f, FluidBakeInfo& out, u32& outVersion) {
    char magic[sizeof(kMagic)] = {};
    f.read(magic, sizeof(magic));
    if (!f || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) return false;

    if (!Read(f, outVersion) || (outVersion != kVersion && outVersion != kVersionNoDeltas)) {
        return false;
    }

    u8 is3DByte = 0, loopByte = 0;
    u16 pad = 0;
    if (!Read(f, out.gridSize) || !Read(f, is3DByte) || !Read(f, loopByte) || !Read(f, pad)
        || !Read(f, out.loopBlendFrames) || !Read(f, out.frameRate)
        || !Read(f, out.halfExtents.x) || !Read(f, out.halfExtents.y)
        || !Read(f, out.halfExtents.z)
        || !Read(f, out.maxDensity) || !Read(f, out.frameCount)) {
        return false;
    }
    out.is3D = is3DByte != 0;
    out.looping = loopByte != 0;
    return true;
}

// magic + version + gridSize + (is3D,looping,pad) + loopBlendFrames
// + frameRate + halfExtents.xyz + maxDensity + frameCount
constexpr std::streamoff kHeaderBytes = sizeof(kMagic) + 4 * 10;

// Quantise a density field to one byte per cell.
//
// Round, not truncate: truncating loses the faintest visible wisp of every
// plume, which is the part that reads as smoke rather than as a solid blob.
void QuantiseDensity(const std::vector<f32>& density, f32 maxDensity,
                     std::vector<u8>& out) {
    out.assign(density.size(), 0);
    // A take whose peak is zero has no smoke in it at all. Guarding here keeps
    // the divide out of the inner loop and stops a silent NaN reaching a file.
    const f32 scale = (maxDensity > 0.0f) ? (255.0f / maxDensity) : 0.0f;
    for (usize i = 0; i < density.size(); ++i) {
        const f32 q = density[i] * scale;
        const i32 v = (q <= 0.0f) ? 0 : static_cast<i32>(q + 0.5f);
        out[i] = static_cast<u8>(std::clamp(v, 0, 255));
    }
}

// Literal bytes with runs of ZEROES collapsed. This is where a keyframe's size
// comes from: most of a smoke field is empty, and it is empty in long stretches
// because the scan order walks whole rows. A quantised 0 can never appear as a
// literal, which is what makes 0x00 usable as the run marker.
void EncodeKeyframe(const std::vector<u8>& q, std::vector<u8>& out) {
    out.clear();
    out.reserve(q.size() / 4);   // sparse fields land well under this
    usize i = 0;
    while (i < q.size()) {
        if (q[i] != 0) {
            out.push_back(q[i]);
            ++i;
            continue;
        }
        u32 run = 0;
        while (i < q.size() && run < kMaxRun && q[i] == 0) { ++run; ++i; }
        out.push_back(0);
        out.push_back(static_cast<u8>(run));
    }
}

bool DecodeKeyframe(const std::vector<u8>& in, usize cellCount, std::vector<u8>& q) {
    q.assign(cellCount, 0);
    usize w = 0, r = 0;
    while (r < in.size()) {
        const u8 b = in[r++];
        if (b != 0) {
            if (w >= cellCount) return false;        // more data than cells
            q[w++] = b;
            continue;
        }
        if (r >= in.size()) return false;            // run length truncated
        const u32 run = in[r++];
        if (run == 0 || w + run > cellCount) return false;
        w += run;                                     // already zero-filled
    }
    return w == cellCount;
}

// A frame against the one before it.
//
//   0x00 + len   a run of `len` UNCHANGED cells. The whole point: a cell that
//                is not moving costs nothing, whether it is empty or is the
//                still middle of a pool -- which the zero-run scheme could
//                never express, because a settled pool is not zero.
//   0x01..0xFE   a zigzag delta: 1,2,3,4 -> +1,-1,+2,-2 ... up to +/-127.
//   0xFF + byte  an escape carrying the new value outright, for the jumps a
//                signed byte cannot reach.
void EncodeDelta(const std::vector<u8>& cur, const std::vector<u8>& prev,
                 std::vector<u8>& out) {
    out.clear();
    out.reserve(cur.size() / 8);
    usize i = 0;
    while (i < cur.size()) {
        if (cur[i] == prev[i]) {
            u32 run = 0;
            while (i < cur.size() && run < kMaxRun && cur[i] == prev[i]) { ++run; ++i; }
            out.push_back(0);
            out.push_back(static_cast<u8>(run));
            continue;
        }
        const i32 d = static_cast<i32>(cur[i]) - static_cast<i32>(prev[i]);
        if (d >= -127 && d <= 127) {
            const u32 zigzag = (d > 0) ? (2u * static_cast<u32>(d) - 1u)
                                       : (2u * static_cast<u32>(-d));
            out.push_back(static_cast<u8>(zigzag));   // 1..254, never 0 or 255
        } else {
            out.push_back(0xFF);
            out.push_back(cur[i]);
        }
        ++i;
    }
}

// In place: `q` arrives holding the PREVIOUS frame and leaves holding this one.
bool ApplyDelta(const std::vector<u8>& in, std::vector<u8>& q) {
    const usize cellCount = q.size();
    usize w = 0, r = 0;
    while (r < in.size()) {
        const u8 b = in[r++];
        if (b == 0) {
            if (r >= in.size()) return false;
            const u32 run = in[r++];
            if (run == 0 || w + run > cellCount) return false;
            w += run;                                  // unchanged: already correct
            continue;
        }
        if (w >= cellCount) return false;
        if (b == 0xFF) {
            if (r >= in.size()) return false;
            q[w++] = in[r++];
            continue;
        }
        const i32 d = (b & 1u) ? static_cast<i32>((b + 1u) / 2u)
                               : -static_cast<i32>(b / 2u);
        q[w] = static_cast<u8>(std::clamp(static_cast<i32>(q[w]) + d, 0, 255));
        ++w;
    }
    return w == cellCount;
}

} // namespace

void EncodeFluidDensity(const std::vector<f32>& density, f32 maxDensity,
                        std::vector<u8>& out) {
    out.clear();
    if (density.empty()) return;
    std::vector<u8> q;
    QuantiseDensity(density, maxDensity, q);
    EncodeKeyframe(q, out);
}

bool DecodeFluidDensity(const std::vector<u8>& in, f32 maxDensity,
                        usize cellCount, std::vector<f32>& out) {
    std::vector<u8> q;
    // Refused rather than partially filled: a short frame is smoke with a
    // corrupt tail, which on screen reads as a simulation bug rather than as a
    // damaged file, and would be debugged in entirely the wrong place.
    if (!DecodeKeyframe(in, cellCount, q)) {
        out.assign(cellCount, 0.0f);
        return false;
    }
    const f32 scale = maxDensity / 255.0f;
    out.assign(cellCount, 0.0f);
    for (usize i = 0; i < cellCount; ++i) out[i] = static_cast<f32>(q[i]) * scale;
    return true;
}

// Walk to the nearest keyframe at or before `index` and replay the deltas.
//
// The cache turns sequential playback into one delta apply per frame, which is
// what playback always is; a seek pays at most kKeyframeInterval.
bool FluidBake::DecodeQuantisedFrame(i64 index, std::vector<u8>& out) const {
    if (index < 0 || index >= static_cast<i64>(frames.size())) return false;
    if (frameIsKey.size() != frames.size()) return false;

    const usize cellCount = CellCount();

    if (m_CacheIndex == index && m_CacheQuantised.size() == cellCount) {
        out = m_CacheQuantised;
        return true;
    }

    // Continue from the cache when it is exactly the frame before this one.
    if (m_CacheIndex == index - 1 && !frameIsKey[static_cast<usize>(index)]
        && m_CacheQuantised.size() == cellCount) {
        if (!ApplyDelta(frames[static_cast<usize>(index)], m_CacheQuantised)) {
            m_CacheIndex = -1;
            return false;
        }
        m_CacheIndex = index;
        out = m_CacheQuantised;
        return true;
    }

    i64 start = index;
    while (start > 0 && !frameIsKey[static_cast<usize>(start)]) --start;
    if (!frameIsKey[static_cast<usize>(start)]) return false;   // no keyframe to stand on

    std::vector<u8> q;
    if (!DecodeKeyframe(frames[static_cast<usize>(start)], cellCount, q)) return false;
    for (i64 n = start + 1; n <= index; ++n) {
        if (!ApplyDelta(frames[static_cast<usize>(n)], q)) return false;
    }

    m_CacheQuantised = q;
    m_CacheIndex = index;
    out = std::move(q);
    return true;
}

namespace {

// Quantised back to density, the one place the scale is applied on read.
void Dequantise(const std::vector<u8>& q, f32 maxDensity, std::vector<f32>& out) {
    const f32 scale = maxDensity / 255.0f;
    out.assign(q.size(), 0.0f);
    for (usize i = 0; i < q.size(); ++i) out[i] = static_cast<f32>(q[i]) * scale;
}

} // namespace

bool FluidBake::SampleAt(f32 timeSeconds, std::vector<f32>& out) const {
    if (frames.empty() || frameRate <= 0.0f) return false;

    i64 index = static_cast<i64>(timeSeconds * frameRate);
    const i64 count = static_cast<i64>(frames.size());

    if (index < 0) index = 0;
    if (index >= count) {
        // A loop wraps; a one-shot holds its last frame rather than vanishing,
        // because a cinematic whose water blinks out on the final frame is
        // worse than one that simply stops moving.
        index = looping ? (index % count) : (count - 1);
    }

    std::vector<u8> q;
    if (!DecodeQuantisedFrame(index, q)) return false;
    Dequantise(q, maxDensity, out);

    // Cross-blend the tail of a loop into its head so the wrap does not pop.
    // Frame N never matches frame 0 in a fluid take, and a hard cut is visible
    // every single cycle, which is exactly the kind of thing that reads as
    // cheap.
    if (looping && loopBlendFrames > 0 && count > static_cast<i64>(loopBlendFrames) * 2) {
        const i64 blendStart = count - static_cast<i64>(loopBlendFrames);
        if (index >= blendStart) {
            const f32 t = static_cast<f32>(index - blendStart + 1)
                        / static_cast<f32>(loopBlendFrames + 1);
            std::vector<f32> head;
            std::vector<u8> headQ;
            const i64 headIndex = index - blendStart;
            if (DecodeQuantisedFrame(headIndex, headQ)) Dequantise(headQ, maxDensity, head);
            // The blend decodes a SECOND frame, which leaves the cache holding
            // the head rather than the frame playback is on. Restoring it keeps
            // the next frame one delta away instead of a walk from a keyframe.
            if (head.size() == out.size()) {
                for (usize n = 0; n < out.size(); ++n) {
                    out[n] = out[n] * (1.0f - t) + head[n] * t;
                }
            }
        }
    }
    return true;
}

bool FluidBake::Save(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        ENJIN_LOG_ERROR(Build, "Fluid bake: cannot open '%s' for writing", path.c_str());
        return false;
    }

    f.write(kMagic, sizeof(kMagic));
    Write(f, kVersion);
    Write(f, gridSize);
    Write(f, static_cast<u8>(is3D ? 1 : 0));
    Write(f, static_cast<u8>(looping ? 1 : 0));
    Write(f, static_cast<u16>(0));                 // padding, keeps floats aligned
    Write(f, loopBlendFrames);
    Write(f, frameRate);
    Write(f, halfExtents.x);
    Write(f, halfExtents.y);
    Write(f, halfExtents.z);
    Write(f, maxDensity);
    Write(f, static_cast<u32>(frames.size()));

    // A take whose kinds do not line up with its frames is a bug in whatever
    // built it, and writing it would produce a file that cannot be decoded.
    if (frameIsKey.size() != frames.size()) {
        ENJIN_LOG_ERROR(Build, "Fluid bake: %zu frames but %zu frame kinds",
                        frames.size(), frameIsKey.size());
        return false;
    }

    for (usize n = 0; n < frames.size(); ++n) {
        Write(f, frameIsKey[n]);                       // 1 = keyframe, 0 = delta
        Write(f, static_cast<u32>(frames[n].size()));
        if (!frames[n].empty()) {
            f.write(reinterpret_cast<const char*>(frames[n].data()),
                    static_cast<std::streamsize>(frames[n].size()));
        }
    }
    return static_cast<bool>(f);
}

bool FluidBake::Load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    FluidBakeInfo info;
    u32 version = 0;
    if (!ReadHeader(f, info, version)) return false;
    gridSize = info.gridSize;
    is3D = info.is3D;
    looping = info.looping;
    loopBlendFrames = info.loopBlendFrames;
    frameRate = info.frameRate;
    halfExtents = info.halfExtents;
    maxDensity = info.maxDensity;
    const u32 frameCount = info.frameCount;

    // A corrupt or hostile header must not turn into a multi-gigabyte reserve
    // before the read fails. The file is on disk and its frames cannot be
    // larger than it is.
    f.seekg(0, std::ios::end);
    const std::streamoff fileSize = f.tellg();
    f.seekg(kHeaderBytes, std::ios::beg);
    if (static_cast<std::streamoff>(frameCount) > fileSize) return false;

    frames.clear();
    frameIsKey.clear();
    frames.reserve(frameCount);
    frameIsKey.reserve(frameCount);
    m_CacheIndex = -1;

    for (u32 n = 0; n < frameCount; ++n) {
        // Version 1 wrote no kind byte because every frame was whole. Reading
        // one as a keyframe is not a compatibility shim, it is what the frame
        // actually is.
        u8 kind = 1;
        if (version == kVersion && !Read(f, kind)) return false;

        u32 size = 0;
        if (!Read(f, size)) return false;
        if (static_cast<std::streamoff>(size) > fileSize) return false;
        std::vector<u8> frame(size);
        if (size) {
            f.read(reinterpret_cast<char*>(frame.data()), static_cast<std::streamsize>(size));
            if (!f) return false;
        }
        frames.push_back(std::move(frame));
        frameIsKey.push_back(kind ? 1 : 0);
    }

    // A take whose first frame is a delta has nothing to stand on.
    if (!frames.empty() && !frameIsKey[0]) return false;
    return true;
}

bool ReadFluidBakeInfo(const std::string& path, FluidBakeInfo& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    u32 version = 0;
    return ReadHeader(f, out, version);
}

std::vector<FluidTakeEntry> FindFluidRecordings(const std::string& projectDir,
                                                std::string* searchedDir) {
    namespace fs = std::filesystem;
    std::vector<FluidTakeEntry> takes;
    if (searchedDir) searchedDir->clear();
    if (projectDir.empty()) return takes;

    const fs::path assetsDir = fs::path(projectDir) / "assets";
    if (searchedDir) *searchedDir = assetsDir.generic_string();

    std::error_code ec;
    if (!fs::exists(assetsDir, ec)) return takes;

    // skip_permission_denied and the error_code overloads throughout: a folder
    // the editor cannot read is a folder to walk past, not a reason to fail
    // the whole listing.
    for (fs::recursive_directory_iterator it(assetsDir, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;

        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".enjfluid") continue;

        FluidTakeEntry entry;
        entry.relativePath = fs::relative(it->path(), fs::path(projectDir), ec).generic_string();
        if (ec || entry.relativePath.empty()) { ec.clear(); continue; }
        entry.readable = ReadFluidBakeInfo(it->path().string(), entry.info);
        takes.push_back(std::move(entry));
    }

    std::sort(takes.begin(), takes.end(),
              [](const FluidTakeEntry& a, const FluidTakeEntry& b) {
                  return a.relativePath < b.relativePath;
              });
    return takes;
}

FluidBakeInput PrepareFluidBake(ECS::World* world, ECS::Entity volume,
                                const FluidBakeSettings& settings) {
    FluidBakeInput in;
    if (!world) return in;
    auto* vol = world->GetComponent<ECS::FluidVolumeComponent>(volume);
    if (!vol) return in;
    if (settings.frameRate <= 0.0f || settings.duration <= 0.0f) return in;

    auto* xf = world->GetComponent<ECS::TransformComponent>(volume);
    const Math::Vector3 centre = xf ? xf->position : Math::Vector3(0.0f, 0.0f, 0.0f);

    in.is3D = vol->dimension == ECS::FluidDimension::Mode3D;

    // The live solver's clamps, repeated here rather than discovered by
    // stepping a grid once: a take must record at the resolution the volume
    // will actually play at, or the recording is of a different volume.
    u32 clamped = vol->gridSize;
    clamped = in.is3D ? std::min(clamped, 48u) : std::min(clamped, 128u);
    in.gridSize = std::max(clamped, 8u);

    i32 iterations = vol->solverIterations;
    if (in.is3D && in.gridSize > 32) iterations = std::min(iterations, 10);

    in.halfExtents = vol->halfExtents;
    in.params.viscosity = vol->viscosity;
    in.params.diffusion = vol->diffusion;
    in.params.dissipation = vol->dissipation;
    in.params.velocityDissipation = vol->velocityDissipation;
    in.params.buoyancy = vol->buoyancy;
    in.params.iterations = iterations;
    in.params.sourceDensity = vol->sourceDensity;
    in.params.sourceRadius = vol->sourceRadius;
    in.params.sourceVelocityScale = vol->sourceVelocityScale;

    // The one thing that genuinely needs the world, taken once. A bake runs
    // against static geometry by definition, so a snapshot is not an
    // approximation here -- it is the definition.
    if (settings.useSceneColliders) {
        BuildFluidObstacleMask(world, centre, in.halfExtents, in.gridSize, in.is3D,
                               in.obstacles);
        bool any = false;
        for (u8 c : in.obstacles) { if (c) { any = true; break; } }
        if (!any) in.obstacles.clear();
    }

    in.valid = true;
    return in;
}

bool BakeFluidPrepared(const FluidBakeInput& input,
                       const FluidBakeSettings& settings,
                       FluidBake& out,
                       const std::function<void(f32)>& onProgress,
                       const std::atomic<bool>* cancel) {
    if (!input.valid || input.gridSize == 0) return false;
    if (settings.frameRate <= 0.0f || settings.duration <= 0.0f) return false;

    FluidGridData grid;
    grid.Allocate(input.gridSize, input.is3D);
    if (!input.obstacles.empty() && input.obstacles.size() == grid.density.size()) {
        grid.solid = input.obstacles;
    }

    const f32 dt = 1.0f / settings.frameRate;
    const u32 settleFrames =
        static_cast<u32>(std::max(0.0f, settings.settleTime) * settings.frameRate);
    const u32 keepFrames = std::max(1u, static_cast<u32>(settings.duration * settings.frameRate));
    const u32 totalFrames = settleFrames + keepFrames;

    auto cancelled = [&]() { return cancel && cancel->load(std::memory_order_relaxed); };

    // Simulated and thrown away, so the take opens on a developed plume rather
    // than on an empty grid filling up -- and so a LOOP is recorded from a
    // settled state, which is most of what makes one loopable at all.
    for (u32 n = 0; n < settleFrames; ++n) {
        if (cancelled()) return false;
        FluidSimulation::StepGrid(grid, input.params, dt);
        if (onProgress) onProgress(static_cast<f32>(n) / static_cast<f32>(totalFrames));
    }

    // Two passes over the kept frames, because the 8-bit quantisation is
    // scaled against the take's PEAK and the peak is not known until the take
    // is over. Quantising against a guess clips every plume that later grows
    // brighter than it, and rescaling afterwards would mean decoding and
    // re-encoding everything anyway.
    std::vector<std::vector<f32>> raw;
    raw.reserve(keepFrames);
    f32 peak = 0.0f;
    for (u32 n = 0; n < keepFrames; ++n) {
        if (cancelled()) return false;
        FluidSimulation::StepGrid(grid, input.params, dt);
        for (f32 d : grid.density) peak = std::max(peak, d);
        raw.push_back(grid.density);
        if (onProgress) {
            onProgress(static_cast<f32>(settleFrames + n) / static_cast<f32>(totalFrames));
        }
    }

    FluidBake result;
    result.gridSize = grid.N;
    result.is3D = grid.is3D;
    result.looping = settings.looping;
    result.loopBlendFrames = settings.looping ? settings.loopBlendFrames : 0;
    result.frameRate = settings.frameRate;
    result.halfExtents = input.halfExtents;
    result.maxDensity = (peak > 0.0f) ? peak : 1.0f;
    result.frames.reserve(raw.size());
    result.frameIsKey.reserve(raw.size());

    std::vector<u8> prevQ, curQ, asKey, asDelta;
    for (usize n = 0; n < raw.size(); ++n) {
        QuantiseDensity(raw[n], result.maxDensity, curQ);

        // A forced keyframe every interval, to bound what a SEEK costs.
        const bool forcedKey = (n % kKeyframeInterval) == 0;
        EncodeKeyframe(curQ, asKey);

        // Otherwise encode BOTH and keep the smaller. Measured on real takes,
        // a delta is not automatically the winner: in turbulent smoke nearly
        // every cell moves by a quantisation step each frame, so a delta byte
        // costs exactly what a literal does and the escapes make it worse. The
        // win is real where a field is STILL -- a settled pool, the quiet
        // interior of a plume -- and choosing per frame takes it without
        // betting on it.
        bool key = forcedKey;
        if (!forcedKey) {
            EncodeDelta(curQ, prevQ, asDelta);
            key = asDelta.size() >= asKey.size();
        }

        result.frames.push_back(key ? asKey : asDelta);
        result.frameIsKey.push_back(key ? 1 : 0);
        prevQ = curQ;
    }

    // Assigned only once the take is whole. A cancel partway through leaves the
    // caller's bake untouched rather than handing back a truncated one, which
    // would read as a bad simulation instead of an abandoned bake.
    out = std::move(result);
    if (onProgress) onProgress(1.0f);
    return true;
}

bool BakeFluid(ECS::World* world, ECS::Entity volume,
               const FluidBakeSettings& settings,
               FluidBake& out,
               const std::function<void(f32)>& onProgress) {
    // Snapshot then solve, on one thread. The split exists for the editor's
    // worker; a caller that does not need one should not have to know about it.
    const FluidBakeInput input = PrepareFluidBake(world, volume, settings);
    return BakeFluidPrepared(input, settings, out, onProgress, nullptr);
}

} // namespace Effects
} // namespace Enjin
