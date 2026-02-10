#pragma once

#include "Enjin/Math/Noise.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Math {

// Curl noise 3D: computes curl of a 3D potential field built from FBM noise.
// Returns a divergence-free vector field suitable for fluid-like particle motion.
// Uses finite differences of three offset FBM samples to approximate partial derivatives.
inline Vector3 CurlNoise3D(f32 x, f32 y, f32 z,
                            i32 octaves = 4, f32 lacunarity = 2.0f,
                            f32 persistence = 0.5f, f32 frequency = 1.0f,
                            u32 seed = 0) {
    constexpr f32 eps = 0.001f;

    // Three scalar fields with different seed offsets to ensure decorrelation
    u32 seedA = seed;
    u32 seedB = seed + 31337;
    u32 seedC = seed + 71093;

    // Partial derivatives via central differences:
    // dFz/dy - dFy/dz  (curl x)
    // dFx/dz - dFz/dx  (curl y)
    // dFy/dx - dFx/dy  (curl z)

    f32 dFz_dy = (FBM3D(x, y + eps, z, octaves, lacunarity, persistence, frequency, seedC)
                - FBM3D(x, y - eps, z, octaves, lacunarity, persistence, frequency, seedC)) / (2.0f * eps);

    f32 dFy_dz = (FBM3D(x, y, z + eps, octaves, lacunarity, persistence, frequency, seedB)
                - FBM3D(x, y, z - eps, octaves, lacunarity, persistence, frequency, seedB)) / (2.0f * eps);

    f32 dFx_dz = (FBM3D(x, y, z + eps, octaves, lacunarity, persistence, frequency, seedA)
                - FBM3D(x, y, z - eps, octaves, lacunarity, persistence, frequency, seedA)) / (2.0f * eps);

    f32 dFz_dx = (FBM3D(x + eps, y, z, octaves, lacunarity, persistence, frequency, seedC)
                - FBM3D(x - eps, y, z, octaves, lacunarity, persistence, frequency, seedC)) / (2.0f * eps);

    f32 dFy_dx = (FBM3D(x + eps, y, z, octaves, lacunarity, persistence, frequency, seedB)
                - FBM3D(x - eps, y, z, octaves, lacunarity, persistence, frequency, seedB)) / (2.0f * eps);

    f32 dFx_dy = (FBM3D(x, y + eps, z, octaves, lacunarity, persistence, frequency, seedA)
                - FBM3D(x, y - eps, z, octaves, lacunarity, persistence, frequency, seedA)) / (2.0f * eps);

    return Vector3(dFz_dy - dFy_dz, dFx_dz - dFz_dx, dFy_dx - dFx_dy);
}

} // namespace Math
} // namespace Enjin
