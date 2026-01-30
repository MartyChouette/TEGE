#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Components/Text.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace Enjin {
namespace Renderer {

// Rasterizes text into RGBA pixel buffers using stb_truetype
class ENJIN_API TextRasterizer {
public:
    TextRasterizer() = default;
    ~TextRasterizer();

    // Rasterize text component into an RGBA pixel buffer
    // Returns empty vector on failure
    std::vector<u8> Rasterize(const ECS::TextComponent& textComp);

    // Clear all cached font data
    void ClearFontCache();

private:
    // Cached font file data (read from disk once)
    struct FontData {
        std::vector<u8> fileData;
    };

    // Load or get cached font file data
    const FontData* GetOrLoadFont(const std::string& fontPath);

    std::unordered_map<std::string, FontData> m_FontCache;
};

} // namespace Renderer
} // namespace Enjin
