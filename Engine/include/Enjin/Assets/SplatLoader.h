#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Assets {

// One Gaussian splat, decoded to linear values and laid out exactly as the
// renderer's per-instance vertex buffer expects (64 bytes, 4x vec4).
struct SplatInstance {
    f32 px, py, pz, opacity;     // world position + opacity [0,1]
    f32 r, g, b, pad0;           // base color [0,1] (SH DC band evaluated)
    f32 sx, sy, sz, pad1;        // gaussian scale (world units, already exp())
    f32 qx, qy, qz, qw;          // rotation quaternion (normalized, engine order)
};
static_assert(sizeof(SplatInstance) == 64, "renderer instance layout");

struct SplatData {
    std::vector<SplatInstance> splats;
    std::string error;           // non-empty on failure
    bool Valid() const { return error.empty() && !splats.empty(); }
};

// Gaussian splat file loading. Two formats:
// - .ply in the INRIA 3DGS layout (binary_little_endian; identified by the
//   f_dc_0 property - a plain mesh .ply is rejected with a clear error).
//   Higher-order SH bands (f_rest_*) are skipped: splats are baked radiance,
//   and the engine applies its styling in post (docs/OPENNESS.md spirit: the
//   file is readable, the import is lossy only in view-dependence).
// - .spz (Niantic's gzipped packed format, ~10x smaller).
//
// maxSplats caps memory; flipYZ converts the common COLMAP frame (Y down,
// Z forward) into the engine's Y-up right-handed frame by conjugating with
// diag(1,-1,-1) - positions flip and rotations re-extract through a matrix.
class ENJIN_API SplatLoader {
public:
    static SplatData LoadFromFile(const std::string& path, u32 maxSplats = 2000000,
                                  bool flipYZ = true);

    // Format entry points (also used directly by tests)
    static SplatData ParsePly(const u8* data, usize size, u32 maxSplats, bool flipYZ);
    static SplatData ParseSpz(const u8* data, usize size, u32 maxSplats, bool flipYZ);
    // The spz payload AFTER gzip decompression (tests feed this directly)
    static SplatData ParseSpzRaw(const u8* data, usize size, u32 maxSplats, bool flipYZ);
};

} // namespace Assets
} // namespace Enjin
