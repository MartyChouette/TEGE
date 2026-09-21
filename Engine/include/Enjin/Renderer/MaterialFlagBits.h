#pragma once

// The material flag word's bit map, in ONE place (adr-0008 phase 3).
//
// WHY THIS FILE EXISTS. triangle.vert and triangle.frag declare the SAME
// `layout(push_constant)` block, so they read one 32-bit `flags` word -- and
// they each carried their own hand-written `#define FLAG_*` list. Two bits
// ended up meaning different things per stage:
//
//   bit 3: FLAG_SKINNED in the vert, FLAG_SDF_TEXT in the frag
//   bit 4: FLAG_WIND_SWAY in the vert, FLAG_EXCLUDE_CEL in the frag
//
// A skinned mesh therefore took the SDF text path (which hard-discards every
// fragment under 180/255 alpha: silent on an opaque texture, destructive on an
// alpha-tested one) and wind-swaying vegetation was silently excluded from cel
// shading. Nothing caught it because nothing put the two lists side by side.
//
// This repo already solved the same class of defect once: ObjectData is
// generated for C++ and WGSL from ENJIN_WEB_OBJECTDATA_FIELDS, with
// tools/gpu_layout_parity.py --strict in CI, and it has not drifted since. The
// material flag word was the one GPU layout that never got the treatment, and
// it is the one that drifted.
//
// HOW TO CHANGE IT. Edit the table below, then run:
//     python tools/gen_material_flags.py
// which rewrites the generated block in both shaders. CI runs the same tool
// with --check and fails if a shader has drifted from this table.
//
// COLUMNS: name, bit, used-by-vert, used-by-frag.
// A bit may legitimately be read by both stages -- WATER_SURFACE is -- but it
// then means the SAME thing in both, which is the invariant this file exists to
// hold. Two different meanings on one bit cannot be expressed here at all.
//
// NOT IN THIS TABLE, deliberately: anything material-STATIC belongs in a
// specialization constant, not a bit (adr-0008's rate rule). SDF text and
// exclude-cel left this word for exactly that reason and freed bits 3 and 4.
// Packed numeric fields are also not flags and are listed separately below.

#include "Enjin/Platform/Types.h"

// clang-format off
#define ENJIN_MATERIAL_FLAG_BITS(X)                 \
    /*    NAME,             bit, vert, frag */      \
    X(DOUBLE_SIDED,          0,   0,    1)          \
    X(CAST_SHADOWS,          1,   0,    1)          \
    X(RECEIVE_SHADOWS,       2,   0,    1)          \
    X(SKINNED,               3,   1,    0)          \
    X(WIND_SWAY,             4,   1,    0)          \
    X(WATER_SURFACE,         5,   1,    1)          \
    X(RAIN_RIPPLES,          6,   1,    1)          \
    X(WATER_SHORE,           7,   1,    1)          \
    X(HAS_HEIGHT_TEX,       10,   0,    1)          \
    X(WATER_OCEAN,          11,   1,    1)          \
    X(UV_QUANTIZE,          12,   1,    1)          \
    X(GOURAUD_ONLY,         13,   1,    1)          \
    X(HAS_BASE_COLOR_TEX,   16,   0,    1)          \
    X(HAS_NORMAL_TEX,       17,   0,    1)          \
    X(HAS_METALLIC_TEX,     18,   0,    1)          \
    X(HAS_EMISSIVE_TEX,     19,   0,    1)          \
    X(FLAT_SHADING,         20,   1,    1)          \
    X(AFFINE_TEXTURING,     21,   1,    1)          \
    X(VERTEX_SNAPPING,      22,   1,    1)          \
    X(STIPPLE_TRANS,        23,   1,    1)
// clang-format on

// Packed numeric fields in the PUSH-CONSTANT word. Not flags, listed here so
// the word's occupancy is readable in one place:
//   14-15 shadowDitherMode, 24-28 vertexSnapResolution, 29-31 shadowDitherPattern
//
// Bits 8-9 (alphaMode) are FREE as of adr-0008 phase 4: it is material-static,
// so it is SPEC_ALPHA_MODE now.
//
// It still occupies bits 8-9 of the MATERIAL SSBO word, and that is deliberate
// rather than drift. rt_shadow.rchit and rt_ao.rchit read
// `materials[gl_InstanceCustomIndexEXT].flags >> 8`; a ray-tracing hit shader
// indexes materials from a buffer and cannot be specialized per material. So
// the rate rule has an exception worth stating: material-static data still
// needs a BUFFER representation wherever the consumer has no pipeline of its
// own to bake it into.
//
// With SDF text and exclude-cel gone, bits 9 and 15 are the only gaps inside
// the packed ranges and are NOT free. The genuinely free bits are none: see
// adr-0008 phase 4, which moves the packed numerics into the material buffer
// and returns twelve.

namespace Enjin {
namespace Renderer {
namespace MaterialFlags {

#define ENJIN_MFB_CONSTANT(name, bit, vert, frag) \
    inline constexpr i32 name = (1 << (bit));
ENJIN_MATERIAL_FLAG_BITS(ENJIN_MFB_CONSTANT)
#undef ENJIN_MFB_CONSTANT

// Every bit the table claims, for tests that want to assert the word's shape
// without restating it.
inline constexpr i32 kClaimedMask = []{
    i32 m = 0;
#define ENJIN_MFB_OR(name, bit, vert, frag) m |= (1 << (bit));
    ENJIN_MATERIAL_FLAG_BITS(ENJIN_MFB_OR)
#undef ENJIN_MFB_OR
    return m;
}();

} // namespace MaterialFlags
} // namespace Renderer
} // namespace Enjin
