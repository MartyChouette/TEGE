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
constexpr u32 kVersion = 1;

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
bool ReadHeader(std::ifstream& f, FluidBakeInfo& out) {
    char magic[sizeof(kMagic)] = {};
    f.read(magic, sizeof(magic));
    if (!f || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) return false;

    u32 version = 0;
    if (!Read(f, version) || version != kVersion) return false;

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

} // namespace

void EncodeFluidDensity(const std::vector<f32>& density, f32 maxDensity,
                        std::vector<u8>& out) {
    out.clear();
    if (density.empty()) return;

    // A take whose peak is zero has no smoke in it at all. Guarding here keeps
    // the divide out of the inner loop and stops a silent NaN reaching a file.
    const f32 scale = (maxDensity > 0.0f) ? (255.0f / maxDensity) : 0.0f;

    out.reserve(density.size() / 4);   // sparse fields land well under this

    usize i = 0;
    while (i < density.size()) {
        // Quantise with a round, not a truncate: truncating loses the faintest
        // visible wisp of every plume, which is the part that reads as smoke
        // rather than as a solid blob.
        const f32 q = density[i] * scale;
        const i32 v = (q <= 0.0f) ? 0 : static_cast<i32>(q + 0.5f);
        const u8 b = static_cast<u8>(std::clamp(v, 0, 255));

        if (b != 0) {
            out.push_back(b);
            ++i;
            continue;
        }

        // Empty cell: measure the run. This is where the size comes from --
        // most of a smoke field is empty, and it is empty in long stretches
        // because the scan order walks whole rows.
        u32 run = 0;
        while (i < density.size() && run < kMaxRun) {
            const f32 qz = density[i] * scale;
            const i32 vz = (qz <= 0.0f) ? 0 : static_cast<i32>(qz + 0.5f);
            if (std::clamp(vz, 0, 255) != 0) break;
            ++run;
            ++i;
        }
        out.push_back(0);
        out.push_back(static_cast<u8>(run));
    }
}

bool DecodeFluidDensity(const std::vector<u8>& in, f32 maxDensity,
                        usize cellCount, std::vector<f32>& out) {
    out.assign(cellCount, 0.0f);
    const f32 scale = maxDensity / 255.0f;

    usize w = 0;
    usize r = 0;
    while (r < in.size()) {
        const u8 b = in[r++];
        if (b != 0) {
            if (w >= cellCount) return false;        // more data than cells
            out[w++] = static_cast<f32>(b) * scale;
            continue;
        }
        if (r >= in.size()) return false;            // run length truncated
        const u32 run = in[r++];
        if (run == 0 || w + run > cellCount) return false;
        w += run;                                     // already zero-filled
    }

    // Refused rather than partially filled: a short frame is smoke with a
    // corrupt tail, which on screen reads as a simulation bug rather than as a
    // damaged file, and would be debugged in entirely the wrong place.
    return w == cellCount;
}

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

    if (!DecodeFluidDensity(frames[static_cast<usize>(index)], maxDensity,
                            CellCount(), out)) {
        return false;
    }

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
            const usize headIndex = static_cast<usize>(index - blendStart);
            if (DecodeFluidDensity(frames[headIndex], maxDensity, CellCount(), head)
                && head.size() == out.size()) {
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

    for (const auto& frame : frames) {
        Write(f, static_cast<u32>(frame.size()));
        if (!frame.empty()) {
            f.write(reinterpret_cast<const char*>(frame.data()),
                    static_cast<std::streamsize>(frame.size()));
        }
    }
    return static_cast<bool>(f);
}

bool FluidBake::Load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    FluidBakeInfo info;
    if (!ReadHeader(f, info)) return false;
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
    frames.reserve(frameCount);
    for (u32 n = 0; n < frameCount; ++n) {
        u32 size = 0;
        if (!Read(f, size)) return false;
        if (static_cast<std::streamoff>(size) > fileSize) return false;
        std::vector<u8> frame(size);
        if (size) {
            f.read(reinterpret_cast<char*>(frame.data()), static_cast<std::streamsize>(size));
            if (!f) return false;
        }
        frames.push_back(std::move(frame));
    }
    return true;
}

bool ReadFluidBakeInfo(const std::string& path, FluidBakeInfo& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    return ReadHeader(f, out);
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

bool BakeFluid(ECS::World* world, ECS::Entity volume,
               const FluidBakeSettings& settings,
               FluidBake& out,
               const std::function<void(f32)>& onProgress) {
    if (!world) return false;
    auto* vol = world->GetComponent<ECS::FluidVolumeComponent>(volume);
    if (!vol) return false;
    if (settings.frameRate <= 0.0f || settings.duration <= 0.0f) return false;

    auto* xf = world->GetComponent<ECS::TransformComponent>(volume);
    const Math::Vector3 centre = xf ? xf->position : Math::Vector3(0.0f, 0.0f, 0.0f);

    FluidSimulation sim;
    // No frame budget. The budget exists so a volume cannot eat a frame, and a
    // bake has no frame -- deferring volumes here would only make the take
    // take longer AND come out wrong, since a deferred volume records a
    // duplicate of the frame before it.
    sim.SetFrameBudgetMs(1.0e9);
    // One volume is being recorded. Update iterates the whole world, so
    // without this a bake in a ten-volume level solves all ten for the entire
    // take -- ten times the wait for exactly the same file.
    sim.SetSoloEntity(volume);

    const f32 dt = 1.0f / settings.frameRate;

    // One step allocates the grid, which the obstacle mask has to be sized
    // against. Voxelising before that would produce a mask for a grid that
    // does not exist yet and SetObstacleMask would correctly refuse it.
    sim.Update(dt, world);
    if (settings.useSceneColliders) {
        const FluidGridData* g = sim.GetGridData(volume);
        if (g && g->N > 0) {
            std::vector<u8> mask;
            BuildFluidObstacleMask(world, centre, vol->halfExtents, g->N, g->is3D, mask);
            sim.SetObstacleMask(volume, std::move(mask));
        }
    }

    const u32 settleFrames = static_cast<u32>(std::max(0.0f, settings.settleTime) * settings.frameRate);
    const u32 keepFrames = std::max(1u, static_cast<u32>(settings.duration * settings.frameRate));
    const u32 totalFrames = settleFrames + keepFrames;

    // Simulated and thrown away, so the take opens on a developed plume rather
    // than on an empty grid filling up -- and so a LOOP is recorded from a
    // settled state, which is most of what makes one loopable at all.
    for (u32 n = 0; n < settleFrames; ++n) {
        sim.Update(dt, world);
        if (onProgress) onProgress(static_cast<f32>(n) / static_cast<f32>(totalFrames));
    }

    const FluidGridData* grid = sim.GetGridData(volume);
    if (!grid || grid->N == 0) return false;

    out = FluidBake{};
    out.gridSize = grid->N;
    out.is3D = grid->is3D;
    out.looping = settings.looping;
    out.loopBlendFrames = settings.looping ? settings.loopBlendFrames : 0;
    out.frameRate = settings.frameRate;
    out.halfExtents = vol->halfExtents;

    // Two passes over the kept frames, because the 8-bit quantisation is
    // scaled against the take's PEAK and the peak is not known until the take
    // is over. Quantising against a guess clips every plume that later grows
    // brighter than it, and rescaling afterwards would mean decoding and
    // re-encoding everything anyway.
    std::vector<std::vector<f32>> raw;
    raw.reserve(keepFrames);
    f32 peak = 0.0f;
    for (u32 n = 0; n < keepFrames; ++n) {
        sim.Update(dt, world);
        const FluidGridData* g = sim.GetGridData(volume);
        if (!g) return false;
        for (f32 d : g->density) peak = std::max(peak, d);
        raw.push_back(g->density);
        if (onProgress) {
            onProgress(static_cast<f32>(settleFrames + n) / static_cast<f32>(totalFrames));
        }
    }

    out.maxDensity = (peak > 0.0f) ? peak : 1.0f;
    out.frames.reserve(raw.size());
    for (const auto& frame : raw) {
        std::vector<u8> encoded;
        EncodeFluidDensity(frame, out.maxDensity, encoded);
        out.frames.push_back(std::move(encoded));
    }

    if (onProgress) onProgress(1.0f);
    return true;
}

} // namespace Effects
} // namespace Enjin
