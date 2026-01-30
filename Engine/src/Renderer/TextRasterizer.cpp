#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "Enjin/Renderer/TextRasterizer.h"
#include "Enjin/Logging/Log.h"

#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Renderer {

TextRasterizer::~TextRasterizer() {
    ClearFontCache();
}

const TextRasterizer::FontData* TextRasterizer::GetOrLoadFont(const std::string& fontPath) {
    auto it = m_FontCache.find(fontPath);
    if (it != m_FontCache.end()) {
        return &it->second;
    }

    // Read font file from disk
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
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

    auto [inserted, success] = m_FontCache.emplace(fontPath, std::move(fontData));
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

    if (textComp.text.empty() || textComp.fontPath.empty()) {
        return pixels;
    }

    // Load font
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
    struct TextLine {
        std::string text;
        f32 width;
    };
    std::vector<TextLine> lines;

    // Split by newlines first
    std::vector<std::string> paragraphs;
    {
        std::string paragraph;
        for (char c : textComp.text) {
            if (c == '\n') {
                paragraphs.push_back(paragraph);
                paragraph.clear();
            } else {
                paragraph += c;
            }
        }
        paragraphs.push_back(paragraph);
    }

    // Word-wrap each paragraph
    for (const auto& para : paragraphs) {
        if (para.empty()) {
            lines.push_back({"", 0.0f});
            continue;
        }

        // Split into words
        std::vector<std::string> words;
        {
            std::string word;
            for (char c : para) {
                if (c == ' ' || c == '\t') {
                    if (!word.empty()) {
                        words.push_back(word);
                        word.clear();
                    }
                } else {
                    word += c;
                }
            }
            if (!word.empty()) {
                words.push_back(word);
            }
        }

        std::string currentLine;
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
                stbtt_GetCodepointHMetrics(&font, word[i], &advance, &lsb);
                wordWidth += advance * scale;
                if (i + 1 < word.size()) {
                    int kern = stbtt_GetCodepointKernAdvance(&font, word[i], word[i + 1]);
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
                    currentLine += ' ';
                    currentWidth += spaceWidth;
                }
                currentLine += word;
                currentWidth += wordWidth;
            }
        }

        lines.push_back({currentLine, currentWidth});
    }

    // Render each line
    f32 cursorY = padY + scaledAscent;

    for (const auto& line : lines) {
        if (line.text.empty()) {
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
        for (usize i = 0; i < line.text.size(); i++) {
            int ch = line.text[i];

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
            if (i + 1 < line.text.size()) {
                int kern = stbtt_GetCodepointKernAdvance(&font, ch, line.text[i + 1]);
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
