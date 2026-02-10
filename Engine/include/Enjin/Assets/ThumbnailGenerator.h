#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Enjin {
namespace Assets {

// Thumbnail size presets
enum class ThumbnailSize : u32 {
    Small  = 64,
    Medium = 128,
    Large  = 256
};

// Raw RGBA8 thumbnail data (CPU-side)
struct ThumbnailData {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> pixels; // RGBA8
    bool valid = false;
};

// Generates preview thumbnails for various asset types
// For images: downscales to thumbnail size
// For 3D models: renders a simple wireframe/icon representation
// For audio/scripts: returns type-specific placeholder icons
class ENJIN_API ThumbnailGenerator {
public:
    ThumbnailGenerator() = default;
    ~ThumbnailGenerator() = default;

    // Generate a thumbnail for any asset file
    ThumbnailData Generate(const std::string& path, ThumbnailSize size = ThumbnailSize::Medium);

    // Generate thumbnail for an image file (downsample)
    ThumbnailData GenerateImageThumbnail(const std::string& path, u32 targetSize);

    // Generate thumbnail for a 3D model file (isometric wireframe render)
    ThumbnailData GenerateModelThumbnail(const std::string& path, u32 targetSize);

    // Generate thumbnail for a scene file (scene icon)
    ThumbnailData GenerateSceneThumbnail(const std::string& path, u32 targetSize);

    // Generate a placeholder thumbnail with type label and color
    ThumbnailData GeneratePlaceholder(const char* label, u32 color, u32 targetSize);

    // Cache management
    void ClearCache();
    usize GetCacheSize() const { return m_Cache.size(); }

    // Check if a path has a cached thumbnail
    bool HasCachedThumbnail(const std::string& path) const;

    // Get cached thumbnail (returns invalid if not cached)
    const ThumbnailData& GetCachedThumbnail(const std::string& path) const;

private:
    // CPU-side software rasterizer for model thumbnails
    struct SoftVertex {
        f32 x, y, z;    // Position
        f32 nx, ny, nz; // Normal
    };

    // Simple orthographic projection for model preview
    void ProjectVertex(const SoftVertex& v, f32 scale, f32 cx, f32 cy,
                       f32 rotY, f32 rotX, f32& outX, f32& outY, f32& outDepth) const;

    // Draw a line into an RGBA8 buffer (Bresenham)
    static void DrawLine(std::vector<u8>& pixels, u32 w, u32 h,
                         i32 x0, i32 y0, i32 x1, i32 y1, u32 color);

    // Draw a filled triangle (simple scanline)
    static void DrawTriangle(std::vector<u8>& pixels, std::vector<f32>& depthBuf,
                             u32 w, u32 h,
                             f32 x0, f32 y0, f32 z0,
                             f32 x1, f32 y1, f32 z1,
                             f32 x2, f32 y2, f32 z2,
                             u32 color);

    // Draw text label into buffer (simple 5x7 bitmap font)
    static void DrawLabel(std::vector<u8>& pixels, u32 w, u32 h,
                          const char* text, i32 x, i32 y, u32 color);

    // File type helpers
    static bool IsImage(const std::string& ext);
    static bool IsModel(const std::string& ext);
    static bool IsScene(const std::string& ext);
    static bool IsAudio(const std::string& ext);
    static bool IsScript(const std::string& ext);

    // Thumbnail cache (path -> thumbnail data)
    std::unordered_map<std::string, ThumbnailData> m_Cache;
    static const ThumbnailData s_InvalidThumbnail;
};

} // namespace Assets
} // namespace Enjin
