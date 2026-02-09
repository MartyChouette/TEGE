#include "Enjin/Renderer/SVGLoader.h"
#include "Enjin/Renderer/SDFGenerator.h"
#include "Enjin/Logging/Log.h"

#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>

#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Enjin {
namespace Renderer {

SVGImage SVGLoader::LoadAndRasterize(const std::string& path, f32 scale) {
    SVGImage result;

    NSVGimage* svgImage = nsvgParseFromFile(path.c_str(), "px", 96.0f);
    if (!svgImage) {
        ENJIN_LOG_ERROR(Renderer, "SVGLoader: Failed to parse SVG file: %s", path.c_str());
        return result;
    }

    // Calculate rasterized dimensions
    u32 w = static_cast<u32>(std::ceil(svgImage->width * scale));
    u32 h = static_cast<u32>(std::ceil(svgImage->height * scale));

    if (w == 0 || h == 0) {
        ENJIN_LOG_WARN(Renderer, "SVGLoader: SVG has zero dimensions: %s", path.c_str());
        nsvgDelete(svgImage);
        return result;
    }

    // Cap to reasonable maximum
    constexpr u32 MAX_DIM = 4096;
    if (w > MAX_DIM || h > MAX_DIM) {
        f32 capScale = static_cast<f32>(MAX_DIM) / std::max(w, h);
        w = static_cast<u32>(w * capScale);
        h = static_cast<u32>(h * capScale);
        scale *= capScale;
        ENJIN_LOG_WARN(Renderer, "SVGLoader: Capped SVG dimensions to %ux%u: %s", w, h, path.c_str());
    }

    // Allocate pixel buffer
    result.pixels.resize(w * h * 4, 0);
    result.width = w;
    result.height = h;

    // Rasterize
    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
    if (!rasterizer) {
        ENJIN_LOG_ERROR(Renderer, "SVGLoader: Failed to create rasterizer");
        nsvgDelete(svgImage);
        result.pixels.clear();
        result.width = 0;
        result.height = 0;
        return result;
    }

    nsvgRasterize(rasterizer, svgImage, 0, 0, scale,
                  result.pixels.data(), static_cast<int>(w), static_cast<int>(h),
                  static_cast<int>(w * 4));

    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(svgImage);

    ENJIN_LOG_INFO(Renderer, "SVGLoader: Rasterized '%s' at scale %.1f -> %ux%u",
                   path.c_str(), scale, w, h);

    return result;
}

std::shared_ptr<Texture> SVGLoader::LoadAsTexture(VulkanContext* ctx, const std::string& path, f32 scale) {
    if (!ctx) return nullptr;

    SVGImage img = LoadAndRasterize(path, scale);
    if (img.pixels.empty() || img.width == 0 || img.height == 0) {
        return nullptr;
    }

    auto texture = std::make_shared<Texture>(ctx);
    if (!texture->CreateFromData(img.pixels.data(), img.width, img.height, 4)) {
        ENJIN_LOG_ERROR(Renderer, "SVGLoader: Failed to create GPU texture from SVG: %s", path.c_str());
        return nullptr;
    }

    return texture;
}

SVGImage SVGLoader::LoadAsSDF(const std::string& path, f32 scale, f32 spread) {
    // First rasterize normally
    SVGImage rasterized = LoadAndRasterize(path, scale);
    if (rasterized.pixels.empty() || rasterized.width == 0 || rasterized.height == 0) {
        return rasterized;
    }

    // Generate SDF from the rasterized alpha
    auto sdf = SDFGenerator::Generate(rasterized.pixels.data(),
                                      rasterized.width, rasterized.height, spread);

    // Convert SDF to RGBA (distance encoded in alpha)
    auto sdfPixels = SDFGenerator::ToRGBA(sdf);

    SVGImage result;
    result.width = rasterized.width;
    result.height = rasterized.height;
    result.pixels = std::move(sdfPixels);

    ENJIN_LOG_INFO(Renderer, "SVGLoader: Generated SDF from '%s' (%ux%u, spread=%.1f)",
                   path.c_str(), result.width, result.height, spread);
    return result;
}

bool SVGLoader::IsSVGFile(const std::string& path) {
    if (path.size() < 4) return false;
    std::string ext = path.substr(path.size() - 4);
    // Case-insensitive check
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".svg";
}

} // namespace Renderer
} // namespace Enjin
