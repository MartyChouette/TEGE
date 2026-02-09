#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Animation/Animation.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Editor {

// A single slice region within a sprite sheet
struct SpriteSheetSlice {
    std::string name;
    f32 x = 0, y = 0, width = 0, height = 0;
};

// Result of importing and slicing a sprite sheet
struct SpriteSheetImportResult {
    std::string texturePath;
    u32 imageWidth = 0;
    u32 imageHeight = 0;
    std::vector<SpriteSheetSlice> slices;
};

// Imports and slices sprite sheets for animation and sprite creation
class ENJIN_API SpriteSheetImporter {
public:
    SpriteSheetImporter() = default;
    ~SpriteSheetImporter();

    // Load a sprite sheet image from disk
    bool LoadImage(const std::string& path);

    // Slice into a uniform grid
    SpriteSheetImportResult SliceGrid(u32 cellW, u32 cellH, u32 padding = 0) const;

    // Auto-detect non-transparent regions as slices
    SpriteSheetImportResult SliceAutoDetect(u8 alphaThreshold = 10) const;

    // Convert slices into a SpriteAnimation
    static Animation::SpriteAnimation CreateAnimation(const std::string& name,
                                                       const SpriteSheetImportResult& result,
                                                       f32 fps = 12.0f);

    // State
    bool IsLoaded() const { return m_Pixels != nullptr; }
    const std::string& GetPath() const { return m_Path; }
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    const u8* GetPixels() const { return m_Pixels; }

private:
    std::string m_Path;
    u8* m_Pixels = nullptr;
    u32 m_Width = 0;
    u32 m_Height = 0;
    i32 m_Channels = 0;
};

} // namespace Editor
} // namespace Enjin
