#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>

namespace Enjin {
namespace ECS {

// Horizontal text alignment
enum class TextAlign : i32 {
    Left = 0,
    Center = 1,
    Right = 2
};

// Text component - rasterizes text to a texture that can be applied to any mesh surface
// Useful for books, signs, newspapers, UI panels in the 3D world
struct ENJIN_API TextComponent {
    std::string text;                   // Multi-line text content
    std::string fontPath;               // Path to TTF/OTF font file
    f32 fontSize = 32.0f;              // Font size in pixels
    f32 wrapWidth = 512.0f;            // Word wrap width in pixels
    u32 textureWidth = 512;            // Output texture width
    u32 textureHeight = 512;           // Output texture height
    Math::Vector3 textColor = Math::Vector3(1.0f, 1.0f, 1.0f);   // Text color (white)
    Math::Vector3 bgColor = Math::Vector3(0.0f, 0.0f, 0.0f);     // Background color (black)
    f32 bgOpacity = 1.0f;             // Background alpha (0 = transparent, 1 = opaque)
    TextAlign horizontalAlign = TextAlign::Left;
    f32 paddingX = 16.0f;             // Horizontal padding in pixels
    f32 paddingY = 16.0f;             // Vertical padding in pixels
    bool dirty = true;                 // Internal flag - triggers re-rasterization when true

    TextComponent() = default;
    TextComponent(const std::string& content) : text(content) {}
};

} // namespace ECS
} // namespace Enjin
