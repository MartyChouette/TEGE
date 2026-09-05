#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "Enjin/Renderer/TextRasterizer.h"
#include "Enjin/Renderer/TextEncoding.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Accessibility/TextFont.h"  // bundled default font (embedded bytes)
#include "Enjin/Assets/MeshAssetCache.h"           // game-root resolution for relative font paths

#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace Enjin {
namespace Renderer {

TextRasterizer::~TextRasterizer() {
    ClearFontCache();
}

const TextRasterizer::FontData* TextRasterizer::GetOrLoadFont(const std::string& fontPath) {
    // Keyed by the accessibility choice as well as the path: the same path
    // answers with a different face once the dyslexia font is on, and a cache
    // keyed on the path alone would keep returning the face baked before it.
    const std::string cacheKey = Accessibility::FontCacheKey(fontPath);
    auto it = m_FontCache.find(cacheKey);
    if (it != m_FontCache.end()) {
        return &it->second;
    }

    // An embedded face answers for an empty path (so authored text renders on
    // load without the author picking a font first) and for any path at all
    // while the dyslexia font is on.
    {
        usize embeddedSize = 0;
        const u8* embedded = Accessibility::ResolveFontBytes(fontPath, embeddedSize);
        if (embedded && embeddedSize > 0) {
            FontData fontData;
            fontData.fileData.assign(embedded, embedded + embeddedSize);
            auto [inserted, success] = m_FontCache.emplace(cacheKey, std::move(fontData));
            if (!success) return nullptr;
            return &inserted->second;
        }
    }

    // Read font file from disk. Project-relative paths resolve against the
    // same game root textures/models use (the CWD is the exe dir, not the
    // project, so a raw relative open silently fails).
    std::string loadPath = fontPath;
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        if (fs::path(fontPath).is_relative() && !fs::exists(fontPath, ec)) {
            const std::string& root = Assets::MeshAssetCache::Get().GetSearchRoot();
            if (!root.empty()) {
                std::string joined = (fs::path(root) / fontPath).string();
                if (fs::exists(joined, ec)) loadPath = joined;
            }
        }
    }
    std::ifstream file(loadPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Renderer, "TextRasterizer: Failed to open font file: %s", fontPath.c_str());
        return nullptr;
    }

    auto fileSize = file.tellg();
    if (fileSize <= 0) {
        ENJIN_LOG_ERROR(Renderer, "TextRasterizer: Empty font file: %s", fontPath.c_str());
        return nullptr;
    }

    FontData fontData;
    fontData.fileData.resize(static_cast<usize>(fileSize));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(fontData.fileData.data()), fileSize);
    file.close();

    auto [inserted, success] = m_FontCache.emplace(cacheKey, std::move(fontData));
    if (!success) {
        return nullptr;
    }

    ENJIN_LOG_INFO(Renderer, "TextRasterizer: Cached font: %s", fontPath.c_str());
    return &inserted->second;
}

std::vector<u8> TextRasterizer::Rasterize(const ECS::TextComponent& textComp) {
    u32 width = textComp.textureWidth;
    u32 height = textComp.textureHeight;

    // Allocate RGBA output buffer
    std::vector<u8> pixels(width * height * 4);

    // Fill background
    u8 bgR = static_cast<u8>(std::clamp(textComp.bgColor.x, 0.0f, 1.0f) * 255.0f);
    u8 bgG = static_cast<u8>(std::clamp(textComp.bgColor.y, 0.0f, 1.0f) * 255.0f);
    u8 bgB = static_cast<u8>(std::clamp(textComp.bgColor.z, 0.0f, 1.0f) * 255.0f);
    u8 bgA = static_cast<u8>(std::clamp(textComp.bgOpacity, 0.0f, 1.0f) * 255.0f);

    for (u32 i = 0; i < width * height; i++) {
        pixels[i * 4 + 0] = bgR;
        pixels[i * 4 + 1] = bgG;
        pixels[i * 4 + 2] = bgB;
        pixels[i * 4 + 3] = bgA;
    }

    if (textComp.text.empty()) {
        return pixels;
    }

    // Load font (empty path uses the bundled default font)
    const FontData* fontData = GetOrLoadFont(textComp.fontPath);
    if (!fontData || fontData->fileData.empty()) {
        return pixels;
    }

    // Initialize stb_truetype
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, fontData->fileData.data(),
                        stbtt_GetFontOffsetForIndex(fontData->fileData.data(), 0))) {
        ENJIN_LOG_ERROR(Renderer, "TextRasterizer: Failed to init font: %s", textComp.fontPath.c_str());
        return pixels;
    }

    f32 scale = stbtt_ScaleForPixelHeight(&font, textComp.fontSize);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

    f32 scaledAscent = ascent * scale;
    f32 scaledLineHeight = (ascent - descent + lineGap) * scale;

    u8 textR = static_cast<u8>(std::clamp(textComp.textColor.x, 0.0f, 1.0f) * 255.0f);
    u8 textG = static_cast<u8>(std::clamp(textComp.textColor.y, 0.0f, 1.0f) * 255.0f);
    u8 textB = static_cast<u8>(std::clamp(textComp.textColor.z, 0.0f, 1.0f) * 255.0f);

    f32 padX = textComp.paddingX;
    f32 padY = textComp.paddingY;
    f32 maxWidth = textComp.wrapWidth - padX * 2.0f;
    if (maxWidth <= 0.0f) maxWidth = static_cast<f32>(width) - padX * 2.0f;

    // Split text into lines, then word-wrap each line
    // Layout runs over CODEPOINTS. This used to walk the std::string a byte at
    // a time and pass each one to stb_truetype as a codepoint -- and `char` is
    // signed on MSVC, so every byte above 0x7F arrived NEGATIVE and drew
    // nothing at all. Accented and localised text was unrenderable here.
    const std::vector<u32> allCps = DecodeUTF8All(textComp.text);

    struct TextLine {
        std::vector<u32> cps;
        f32 width;
    };
    std::vector<TextLine> lines;

    // Split by newlines first
    std::vector<std::vector<u32>> paragraphs;
    {
        std::vector<u32> paragraph;
        for (u32 cp : allCps) {
            if (cp == 10u) {                 // newline
                paragraphs.push_back(paragraph);
                paragraph.clear();
            } else {
                paragraph.push_back(cp);
            }
        }
        paragraphs.push_back(paragraph);
    }

    // Word-wrap each paragraph
    for (const auto& para : paragraphs) {
        if (para.empty()) {
            lines.push_back(TextLine{ std::vector<u32>(), 0.0f });
            continue;
        }

        // Split into words
        std::vector<std::vector<u32>> words;
        {
            std::vector<u32> word;
            for (u32 cp : para) {
                if (cp == 32u || cp == 9u) {          // space or tab
                    if (!word.empty()) {
                        words.push_back(word);
                        word.clear();
                    }
                } else {
                    word.push_back(cp);
                }
            }
            if (!word.empty()) {
                words.push_back(word);
            }
        }

        std::vector<u32> currentLine;
        f32 currentWidth = 0.0f;

        // Measure space width
        int spaceAdvance, spaceLeftBearing;
        stbtt_GetCodepointHMetrics(&font, ' ', &spaceAdvance, &spaceLeftBearing);
        f32 spaceWidth = spaceAdvance * scale;

        for (const auto& word : words) {
            // Measure word width
            f32 wordWidth = 0.0f;
            for (usize i = 0; i < word.size(); i++) {
                int advance, lsb;
                stbtt_GetCodepointHMetrics(&font, static_cast<int>(word[i]), &advance, &lsb);
                wordWidth += advance * scale;
                if (i + 1 < word.size()) {
                    int kern = stbtt_GetCodepointKernAdvance(&font, static_cast<int>(word[i]),
                                                             static_cast<int>(word[i + 1]));
                    wordWidth += kern * scale;
                }
            }

            f32 testWidth = currentWidth;
            if (!currentLine.empty()) {
                testWidth += spaceWidth;
            }
            testWidth += wordWidth;

            if (!currentLine.empty() && testWidth > maxWidth) {
                // Wrap: emit current line and start new one
                lines.push_back({currentLine, currentWidth});
                currentLine = word;
                currentWidth = wordWidth;
            } else {
                if (!currentLine.empty()) {
                    currentLine.push_back(32u);
                    currentWidth += spaceWidth;
                }
                currentLine.insert(currentLine.end(), word.begin(), word.end());
                currentWidth += wordWidth;
            }
        }

        lines.push_back({currentLine, currentWidth});
    }

    // Render each line
    f32 cursorY = padY + scaledAscent;

    for (const auto& line : lines) {
        if (line.cps.empty()) {
            cursorY += scaledLineHeight;
            continue;
        }

        // Compute x offset based on alignment
        f32 cursorX = padX;
        if (textComp.horizontalAlign == ECS::TextAlign::Center) {
            cursorX = padX + (maxWidth - line.width) * 0.5f;
        } else if (textComp.horizontalAlign == ECS::TextAlign::Right) {
            cursorX = padX + maxWidth - line.width;
        }

        // Render each character
        for (usize i = 0; i < line.cps.size(); i++) {
            const int ch = static_cast<int>(line.cps[i]);

            // Get glyph bitmap
            int x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBox(&font, ch, scale, scale, &x0, &y0, &x1, &y1);

            int glyphW = x1 - x0;
            int glyphH = y1 - y0;

            if (glyphW > 0 && glyphH > 0) {
                // Rasterize glyph to temporary buffer
                std::vector<u8> glyphBitmap(glyphW * glyphH);
                stbtt_MakeCodepointBitmap(&font, glyphBitmap.data(), glyphW, glyphH, glyphW, scale, scale, ch);

                // Composite glyph into output buffer with alpha blending
                int destX = static_cast<int>(cursorX) + x0;
                int destY = static_cast<int>(cursorY) + y0;

                for (int gy = 0; gy < glyphH; gy++) {
                    int py = destY + gy;
                    if (py < 0 || py >= static_cast<int>(height)) continue;

                    for (int gx = 0; gx < glyphW; gx++) {
                        int px = destX + gx;
                        if (px < 0 || px >= static_cast<int>(width)) continue;

                        u8 alpha = glyphBitmap[gy * glyphW + gx];
                        if (alpha == 0) continue;

                        u32 idx = (py * width + px) * 4;
                        f32 a = alpha / 255.0f;
                        f32 invA = 1.0f - a;

                        pixels[idx + 0] = static_cast<u8>(textR * a + pixels[idx + 0] * invA);
                        pixels[idx + 1] = static_cast<u8>(textG * a + pixels[idx + 1] * invA);
                        pixels[idx + 2] = static_cast<u8>(textB * a + pixels[idx + 2] * invA);
                        pixels[idx + 3] = static_cast<u8>(std::min(255.0f, pixels[idx + 3] + alpha * a));
                    }
                }
            }

            // Advance cursor
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&font, ch, &advance, &lsb);
            cursorX += advance * scale;

            // Kerning
            if (i + 1 < line.cps.size()) {
                int kern = stbtt_GetCodepointKernAdvance(&font, ch, static_cast<int>(line.cps[i + 1]));
                cursorX += kern * scale;
            }
        }

        cursorY += scaledLineHeight;

        // Stop rendering if we've gone past the texture bottom
        if (cursorY > static_cast<f32>(height) - padY) {
            break;
        }
    }

    return pixels;
}

void TextRasterizer::ClearFontCache() {
    m_FontCache.clear();
}

} // namespace Renderer
} // namespace Enjin
