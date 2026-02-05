#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Math/Noise.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

// ============================================================================
// Base noise wrappers (2D)
// ============================================================================

static f32 Noise_Value2D(f32 x, f32 y, u32 seed) {
    return ValueNoise2D(x, y, seed);
}

static f32 Noise_Perlin2D(f32 x, f32 y, u32 seed) {
    return PerlinNoise2D(x, y, seed);
}

static f32 Noise_Simplex2D(f32 x, f32 y, u32 seed) {
    return SimplexNoise2D(x, y, seed);
}

static f32 Noise_Worley2D(f32 x, f32 y, u32 seed) {
    return WorleyNoise2D(x, y, seed);
}

// ============================================================================
// Base noise wrappers (3D)
// ============================================================================

static f32 Noise_Value3D(f32 x, f32 y, f32 z, u32 seed) {
    return ValueNoise3D(x, y, z, seed);
}

static f32 Noise_Perlin3D(f32 x, f32 y, f32 z, u32 seed) {
    return PerlinNoise3D(x, y, z, seed);
}

static f32 Noise_Simplex3D(f32 x, f32 y, f32 z, u32 seed) {
    return SimplexNoise3D(x, y, z, seed);
}

static f32 Noise_Worley3D(f32 x, f32 y, f32 z, u32 seed) {
    return WorleyNoise3D(x, y, z, seed);
}

// ============================================================================
// Fractal noise wrappers (2D)
// ============================================================================

static f32 Noise_FBM2D(f32 x, f32 y, i32 octaves, f32 lacunarity, f32 persistence, f32 frequency, u32 seed) {
    return FBM2D(x, y, octaves, lacunarity, persistence, frequency, seed);
}

static f32 Noise_Ridged2D(f32 x, f32 y, i32 octaves, f32 lacunarity, f32 gain, f32 frequency, u32 seed) {
    return RidgedMultifractal2D(x, y, octaves, lacunarity, gain, frequency, seed);
}

static f32 Noise_Billow2D(f32 x, f32 y, i32 octaves, f32 lacunarity, f32 persistence, f32 frequency, u32 seed) {
    return BillowNoise2D(x, y, octaves, lacunarity, persistence, frequency, seed);
}

// ============================================================================
// Fractal noise wrappers (3D)
// ============================================================================

static f32 Noise_FBM3D(f32 x, f32 y, f32 z, i32 octaves, f32 lacunarity, f32 persistence, f32 frequency, u32 seed) {
    return FBM3D(x, y, z, octaves, lacunarity, persistence, frequency, seed);
}

static f32 Noise_Ridged3D(f32 x, f32 y, f32 z, i32 octaves, f32 lacunarity, f32 gain, f32 frequency, u32 seed) {
    return RidgedMultifractal3D(x, y, z, octaves, lacunarity, gain, frequency, seed);
}

static f32 Noise_Billow3D(f32 x, f32 y, f32 z, i32 octaves, f32 lacunarity, f32 persistence, f32 frequency, u32 seed) {
    return BillowNoise3D(x, y, z, octaves, lacunarity, persistence, frequency, seed);
}

// ============================================================================
// Domain warp wrappers (return warped value, don't modify coords in script)
// ============================================================================

static f32 Noise_DomainWarp2D(f32 x, f32 y, f32 warpStrength, f32 frequency, u32 seed) {
    return DomainWarp2D(x, y, warpStrength, frequency, seed);
}

static f32 Noise_DomainWarp3D(f32 x, f32 y, f32 z, f32 warpStrength, f32 frequency, u32 seed) {
    return DomainWarp3D(x, y, z, warpStrength, frequency, seed);
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterNoiseBindings(asIScriptEngine* engine) {
    // Base noise (2D)
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Value2D(float, float, uint)",
        asFUNCTION(Noise_Value2D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Perlin2D(float, float, uint)",
        asFUNCTION(Noise_Perlin2D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Simplex2D(float, float, uint)",
        asFUNCTION(Noise_Simplex2D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Worley2D(float, float, uint)",
        asFUNCTION(Noise_Worley2D), asCALL_CDECL));

    // Base noise (3D)
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Value3D(float, float, float, uint)",
        asFUNCTION(Noise_Value3D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Perlin3D(float, float, float, uint)",
        asFUNCTION(Noise_Perlin3D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Simplex3D(float, float, float, uint)",
        asFUNCTION(Noise_Simplex3D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Worley3D(float, float, float, uint)",
        asFUNCTION(Noise_Worley3D), asCALL_CDECL));

    // Fractal (2D)
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_FBM2D(float, float, int, float, float, float, uint)",
        asFUNCTION(Noise_FBM2D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Ridged2D(float, float, int, float, float, float, uint)",
        asFUNCTION(Noise_Ridged2D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Billow2D(float, float, int, float, float, float, uint)",
        asFUNCTION(Noise_Billow2D), asCALL_CDECL));

    // Fractal (3D)
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_FBM3D(float, float, float, int, float, float, float, uint)",
        asFUNCTION(Noise_FBM3D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Ridged3D(float, float, float, int, float, float, float, uint)",
        asFUNCTION(Noise_Ridged3D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_Billow3D(float, float, float, int, float, float, float, uint)",
        asFUNCTION(Noise_Billow3D), asCALL_CDECL));

    // Domain warp
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_DomainWarp2D(float, float, float, float, uint)",
        asFUNCTION(Noise_DomainWarp2D), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Noise_DomainWarp3D(float, float, float, float, float, uint)",
        asFUNCTION(Noise_DomainWarp3D), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
