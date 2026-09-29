#pragma once
// A terrain's heights to and from a greyscale image.
//
// A terrain could only be shaped with the brush inside the editor: no way to
// bring in a heightmap made elsewhere, and no way to take one out. Heights map
// linearly onto grey, black = the terrain's Min Height and white = its Max
// Height, so an exported image imported into the same terrain gives back the
// same ground to within one 16-bit step.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS { struct TerrainComponent; }
namespace Assets {

// Largest grid an import makes; bigger images are resampled down to it. The
// same ceiling as the inspector's Grid Width and Grid Height.
constexpr u32 kHeightmapMaxGrid = 512;

// Row-major 16-bit grey, one sample per cell, rows along the terrain's Z.
ENJIN_API std::vector<u16> TerrainToGray16(const ECS::TerrainComponent& terrain);

// Sets the grid to the image's size (resampled down past kHeightmapMaxGrid) and
// its heights from the grey. Min and Max Height and the cell size are kept. A
// changed grid size resets the paint and hole masks; the same size keeps them.
ENJIN_API void TerrainFromGray16(ECS::TerrainComponent& terrain, const u16* pixels, u32 width, u32 height);

// A 16-bit greyscale PNG. stb_image_write has no 16-bit PNG, so this writes one
// with stored (uncompressed) deflate blocks: larger than a compressed file, and
// readable by every PNG reader.
ENJIN_API std::vector<u8> EncodeGray16PNG(const u16* pixels, u32 width, u32 height);

// File versions. Import reads anything stb_image reads, 8 or 16 bit, colour
// taken as its luminance. Export writes a 16-bit PNG.
ENJIN_API bool ImportHeightmapImage(ECS::TerrainComponent& terrain, const std::string& path, std::string& error);
ENJIN_API bool ExportHeightmapImage(const ECS::TerrainComponent& terrain, const std::string& path, std::string& error);

} // namespace Assets
} // namespace Enjin
