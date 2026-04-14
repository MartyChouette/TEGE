#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Texture.h"
#include <string>
#include <memory>
#include <vector>

namespace Enjin {
namespace Renderer {

// SVG image data (rasterized)
struct SVGImage {
    std::vector<u8> pixels;  // RGBA
    u32 width = 0;
    u32 height = 0;
};

// Loads SVG files using nanosvg, rasterizes to pixel data
class ENJIN_API SVGLoader {
public:
    // Load and rasterize an SVG file to pixel data
    static SVGImage LoadAndRasterize(const std::string& path, f32 scale = 1.0f);

    // Load an SVG file directly as a GPU texture
    static std::shared_ptr<Texture> LoadAsTexture(VulkanContext* ctx, const std::string& path, f32 scale = 1.0f);

    // Load an SVG and generate an SDF texture for resolution-independent rendering
    static SVGImage LoadAsSDF(const std::string& path, f32 scale = 1.0f, f32 spread = 8.0f);

    // Check if a file path has an SVG extension
    static bool IsSVGFile(const std::string& path);
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
