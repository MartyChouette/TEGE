#pragma once

// Getting a distance field into and out of a scene file.
//
// A modest volume is 48 x 32 x 48 samples, which is 73,728 numbers. Written as
// a JSON array of floats that is roughly a megabyte of text per cave, most of
// it the same value repeated -- because most of a volume is either well inside
// rock or well outside it, and only the thin shell near the surface carries any
// detail.
//
// So the field is stored three transformations away from its floats:
//
//   1. QUANTIZED to one byte. Values are already clamped to +/-band, and the
//      only place precision matters is within a voxel or two of the surface.
//      One byte over that band is finer than the mesher can resolve, so this
//      loses nothing anybody can see.
//   2. RUN-LENGTH ENCODED. The long stretches of "solid" and "air" collapse to
//      a handful of bytes, which is what turns a megabyte into kilobytes.
//   3. BASE64, because JSON has no way to hold bytes.
//
// Round-tripping is tested rather than assumed: a field that decodes to
// something slightly different every save would drift a cave out of shape over
// a week of editing, one reload at a time.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#include <string>
#include <vector>

namespace Enjin {
namespace Geometry {

// Encode a field. `band` is the clamp range the values were written with;
// anything outside it is clamped rather than wrapped.
ENJIN_API std::string EncodeVoxelField(const std::vector<f32>& field, f32 band);

// Decode a field. Returns false, leaving `out` untouched, when the text is not
// valid or does not decode to exactly `expectedCount` values.
//
// Strict about the count on purpose: a field that is the wrong size for its
// dimensions is a corrupt scene, and filling in the difference would produce a
// cave with a wall in the wrong place rather than an error anyone could see.
ENJIN_API bool DecodeVoxelField(const std::string& text, f32 band, usize expectedCount,
                                std::vector<f32>& out);

} // namespace Geometry
} // namespace Enjin
