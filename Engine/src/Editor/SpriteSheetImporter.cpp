#include "Enjin/Editor/SpriteSheetImporter.h"
#include "Enjin/Logging/Log.h"

#include <stb_image.h>

#include <algorithm>
#include <queue>

namespace Enjin {
namespace Editor {

SpriteSheetImporter::~SpriteSheetImporter() {
    if (m_Pixels) {
        stbi_image_free(m_Pixels);
        m_Pixels = nullptr;
    }
}

bool SpriteSheetImporter::LoadImage(const std::string& path) {
    if (m_Pixels) {
        stbi_image_free(m_Pixels);
        m_Pixels = nullptr;
    }

    int w, h, channels;
    m_Pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!m_Pixels) {
        ENJIN_LOG_ERROR(Editor, "SpriteSheetImporter: Failed to load '%s'", path.c_str());
        return false;
    }

    m_Width = static_cast<u32>(w);
    m_Height = static_cast<u32>(h);
    m_Channels = 4; // Force RGBA
    m_Path = path;

    ENJIN_LOG_INFO(Editor, "SpriteSheetImporter: Loaded '%s' (%ux%u)", path.c_str(), m_Width, m_Height);
    return true;
}

SpriteSheetImportResult SpriteSheetImporter::SliceGrid(u32 cellW, u32 cellH, u32 padding) const {
    SpriteSheetImportResult result;
    result.texturePath = m_Path;
    result.imageWidth = m_Width;
    result.imageHeight = m_Height;

    if (!m_Pixels || cellW == 0 || cellH == 0) return result;

    u32 cols = m_Width / (cellW + padding);
    u32 rows = m_Height / (cellH + padding);

    for (u32 row = 0; row < rows; row++) {
        for (u32 col = 0; col < cols; col++) {
            SpriteSheetSlice slice;
            slice.name = "frame_" + std::to_string(row * cols + col);
            slice.x = static_cast<f32>(col * (cellW + padding));
            slice.y = static_cast<f32>(row * (cellH + padding));
            slice.width = static_cast<f32>(cellW);
            slice.height = static_cast<f32>(cellH);
            result.slices.push_back(slice);
        }
    }

    ENJIN_LOG_INFO(Editor, "SpriteSheetImporter: Grid slice %ux%u -> %zu frames",
                   cols, rows, result.slices.size());
    return result;
}

SpriteSheetImportResult SpriteSheetImporter::SliceAutoDetect(u8 alphaThreshold) const {
    SpriteSheetImportResult result;
    result.texturePath = m_Path;
    result.imageWidth = m_Width;
    result.imageHeight = m_Height;

    if (!m_Pixels || m_Width == 0 || m_Height == 0) return result;

    // Create visited mask
    std::vector<bool> visited(m_Width * m_Height, false);

    auto isOpaque = [&](u32 x, u32 y) -> bool {
        if (x >= m_Width || y >= m_Height) return false;
        u32 idx = (y * m_Width + x) * 4 + 3; // Alpha channel
        return m_Pixels[idx] > alphaThreshold;
    };

    // Scan for connected non-transparent regions using flood fill
    u32 frameIndex = 0;
    for (u32 y = 0; y < m_Height; y++) {
        for (u32 x = 0; x < m_Width; x++) {
            if (visited[y * m_Width + x] || !isOpaque(x, y)) continue;

            // BFS flood fill to find bounding rect of connected region
            u32 minX = x, maxX = x, minY = y, maxY = y;
            std::queue<std::pair<u32, u32>> q;
            q.push({x, y});
            visited[y * m_Width + x] = true;

            while (!q.empty()) {
                auto [cx, cy] = q.front();
                q.pop();

                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);

                // 4-connected neighbors
                const i32 dx[] = {-1, 1, 0, 0};
                const i32 dy[] = {0, 0, -1, 1};
                for (int d = 0; d < 4; d++) {
                    i32 nx = static_cast<i32>(cx) + dx[d];
                    i32 ny = static_cast<i32>(cy) + dy[d];
                    if (nx < 0 || ny < 0) continue;
                    u32 unx = static_cast<u32>(nx);
                    u32 uny = static_cast<u32>(ny);
                    if (unx >= m_Width || uny >= m_Height) continue;
                    if (visited[uny * m_Width + unx]) continue;
                    if (!isOpaque(unx, uny)) continue;

                    visited[uny * m_Width + unx] = true;
                    q.push({unx, uny});
                }
            }

            // Only add regions that are at least 4x4 pixels (filter noise)
            u32 regionW = maxX - minX + 1;
            u32 regionH = maxY - minY + 1;
            if (regionW >= 4 && regionH >= 4) {
                SpriteSheetSlice slice;
                slice.name = "region_" + std::to_string(frameIndex++);
                slice.x = static_cast<f32>(minX);
                slice.y = static_cast<f32>(minY);
                slice.width = static_cast<f32>(regionW);
                slice.height = static_cast<f32>(regionH);
                result.slices.push_back(slice);
            }
        }
    }

    // Sort slices by position (top-left to bottom-right)
    std::sort(result.slices.begin(), result.slices.end(),
        [](const SpriteSheetSlice& a, const SpriteSheetSlice& b) {
            if (std::abs(a.y - b.y) > 4.0f) return a.y < b.y;
            return a.x < b.x;
        });

    // Rename after sorting
    for (usize i = 0; i < result.slices.size(); i++) {
        result.slices[i].name = "frame_" + std::to_string(i);
    }

    ENJIN_LOG_INFO(Editor, "SpriteSheetImporter: Auto-detect found %zu regions", result.slices.size());
    return result;
}

Animation::SpriteAnimation SpriteSheetImporter::CreateAnimation(const std::string& name,
                                                                  const SpriteSheetImportResult& result,
                                                                  f32 fps) {
    Animation::SpriteAnimation anim;
    anim.name = name;
    anim.texturePath = result.texturePath;
    anim.playMode = Animation::PlayMode::Loop;

    f32 frameDuration = (fps > 0.0f) ? (1.0f / fps) : 0.1f;

    for (const auto& slice : result.slices) {
        Animation::SpriteFrame frame;
        frame.srcX = slice.x;
        frame.srcY = slice.y;
        frame.srcWidth = slice.width;
        frame.srcHeight = slice.height;
        frame.duration = frameDuration;
        frame.pivot = Math::Vector2(0.5f, 0.5f);
        anim.frames.push_back(frame);
    }

    return anim;
}

} // namespace Editor
} // namespace Enjin
