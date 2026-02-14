#include "Enjin/Effects/FramebufferFeedback.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Effects {

// ===========================================================================
// Initialization / Resize
// ===========================================================================

void FramebufferFeedbackSystem::Initialize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;

    m_Width = width;
    m_Height = height;

    usize bufSize = static_cast<usize>(width) * height * 4;
    m_BufferA.assign(bufSize, 0);
    m_BufferB.assign(bufSize, 0);
    m_CurrentIsA = true;
    m_Time = 0.0f;
    m_Initialized = true;
}

void FramebufferFeedbackSystem::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;
    if (width == m_Width && height == m_Height) return;

    m_Width = width;
    m_Height = height;

    usize bufSize = static_cast<usize>(width) * height * 4;
    m_BufferA.assign(bufSize, 0);
    m_BufferB.assign(bufSize, 0);
    m_CurrentIsA = true;
}

// ===========================================================================
// Update
// ===========================================================================

void FramebufferFeedbackSystem::Update(f32 dt) {
    m_Time += dt;
}

// ===========================================================================
// Preset application
// ===========================================================================

void FramebufferFeedbackSystem::ApplyPreset(FeedbackPreset preset) {
    // Reset to defaults first
    m_Config = FeedbackConfig{};
    m_Config.preset = preset;

    switch (preset) {
        case FeedbackPreset::None:
            m_Config.blendFactor = 0.0f;
            break;

        case FeedbackPreset::Echo:
            m_Config.blendFactor = 0.8f;
            m_Config.decayRate = 0.1f;
            m_Config.blendMode = FeedbackConfig::BlendMode::Alpha;
            break;

        case FeedbackPreset::Melt:
            m_Config.blendFactor = 0.9f;
            m_Config.uvOffset = Math::Vector2(0.0f, 0.002f);
            m_Config.blurRadius = 1.0f;
            m_Config.saturation = 0.95f;
            break;

        case FeedbackPreset::InfiniteMirror:
            m_Config.blendFactor = 0.85f;
            m_Config.uvScale = 0.98f;
            m_Config.uvPivot = Math::Vector2(0.5f, 0.5f);
            break;

        case FeedbackPreset::VHSTracking:
            m_Config.blendFactor = 0.7f;
            m_Config.scanlineOffset = 3.0f;
            m_Config.scanlineWidth = 4;
            m_Config.saturation = 0.8f;
            m_Config.colorTint = Math::Vector3(1.0f, 0.95f, 0.9f);
            break;

        case FeedbackPreset::Kaleidoscope:
            m_Config.blendFactor = 0.75f;
            m_Config.uvRotation = 0.05f;
            m_Config.uvScale = 0.99f;
            break;

        case FeedbackPreset::Phosphor:
            m_Config.blendFactor = 0.95f;
            m_Config.decayRate = 0.02f;
            m_Config.blendMode = FeedbackConfig::BlendMode::Max;
            m_Config.saturation = 0.9f;
            break;

        case FeedbackPreset::DreamSequence:
            m_Config.blendFactor = 0.8f;
            m_Config.noiseStrength = 0.003f;
            m_Config.blurRadius = 1.5f;
            m_Config.brightness = 1.02f;
            m_Config.saturation = 0.9f;
            break;

        case FeedbackPreset::Custom:
            // Leave defaults in place for custom configuration
            break;
    }
}

// ===========================================================================
// Ping-pong buffer helpers
// ===========================================================================

u8* FramebufferFeedbackSystem::GetPreviousBuffer() {
    return m_CurrentIsA ? m_BufferA.data() : m_BufferB.data();
}

u8* FramebufferFeedbackSystem::GetNextBuffer() {
    return m_CurrentIsA ? m_BufferB.data() : m_BufferA.data();
}

void FramebufferFeedbackSystem::SwapBuffers() {
    m_CurrentIsA = !m_CurrentIsA;
}

// ===========================================================================
// PRNG
// ===========================================================================

f32 FramebufferFeedbackSystem::NextRandom() {
    m_RngState ^= m_RngState << 13;
    m_RngState ^= m_RngState >> 17;
    m_RngState ^= m_RngState << 5;
    return static_cast<f32>(m_RngState & 0xFFFF) / 65535.0f;
}

// ===========================================================================
// Bilinear sampling with UV transform
// ===========================================================================

void FramebufferFeedbackSystem::SampleTransformed(const u8* src,
                                                    f32 u, f32 v,
                                                    u8& outR, u8& outG,
                                                    u8& outB, u8& outA) const {
    // Clamp to [0, 1]
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    // Map to pixel coordinates
    f32 px = u * static_cast<f32>(m_Width - 1);
    f32 py = v * static_cast<f32>(m_Height - 1);

    i32 x0 = static_cast<i32>(std::floor(px));
    i32 y0 = static_cast<i32>(std::floor(py));
    i32 x1 = std::min(x0 + 1, static_cast<i32>(m_Width) - 1);
    i32 y1 = std::min(y0 + 1, static_cast<i32>(m_Height) - 1);

    x0 = std::clamp(x0, 0, static_cast<i32>(m_Width) - 1);
    y0 = std::clamp(y0, 0, static_cast<i32>(m_Height) - 1);

    f32 fx = px - static_cast<f32>(x0);
    f32 fy = py - static_cast<f32>(y0);

    // Fetch 4 texels
    auto fetch = [&](i32 cx, i32 cy, f32& r, f32& g, f32& b, f32& a) {
        usize idx = (static_cast<usize>(cy) * m_Width + cx) * 4;
        r = static_cast<f32>(src[idx + 0]);
        g = static_cast<f32>(src[idx + 1]);
        b = static_cast<f32>(src[idx + 2]);
        a = static_cast<f32>(src[idx + 3]);
    };

    f32 r00, g00, b00, a00;
    f32 r10, g10, b10, a10;
    f32 r01, g01, b01, a01;
    f32 r11, g11, b11, a11;

    fetch(x0, y0, r00, g00, b00, a00);
    fetch(x1, y0, r10, g10, b10, a10);
    fetch(x0, y1, r01, g01, b01, a01);
    fetch(x1, y1, r11, g11, b11, a11);

    f32 r0 = r00 * (1.0f - fx) + r10 * fx;
    f32 g0 = g00 * (1.0f - fx) + g10 * fx;
    f32 b0 = b00 * (1.0f - fx) + b10 * fx;
    f32 a0 = a00 * (1.0f - fx) + a10 * fx;

    f32 r1 = r01 * (1.0f - fx) + r11 * fx;
    f32 g1 = g01 * (1.0f - fx) + g11 * fx;
    f32 b1 = b01 * (1.0f - fx) + b11 * fx;
    f32 a1 = a01 * (1.0f - fx) + a11 * fx;

    outR = static_cast<u8>(std::clamp(r0 * (1.0f - fy) + r1 * fy, 0.0f, 255.0f));
    outG = static_cast<u8>(std::clamp(g0 * (1.0f - fy) + g1 * fy, 0.0f, 255.0f));
    outB = static_cast<u8>(std::clamp(b0 * (1.0f - fy) + b1 * fy, 0.0f, 255.0f));
    outA = static_cast<u8>(std::clamp(a0 * (1.0f - fy) + a1 * fy, 0.0f, 255.0f));
}

// ===========================================================================
// Gaussian blur (separable, in-place)
// ===========================================================================

void FramebufferFeedbackSystem::ApplyBlur(u8* buffer, f32 radius) {
    if (radius < 0.5f) return;

    // Choose kernel size based on radius: 3x3, 5x5, or 7x7
    i32 kernelRadius;
    if (radius < 1.0f)      kernelRadius = 1;  // 3x3
    else if (radius < 2.0f) kernelRadius = 2;  // 5x5
    else                    kernelRadius = 3;  // 7x7

    // Build normalized 1D Gaussian kernel
    f32 sigma = radius;
    if (sigma < 0.5f) sigma = 0.5f;
    i32 kernelSize = kernelRadius * 2 + 1;
    std::vector<f32> kernel(kernelSize);
    f32 kernelSum = 0.0f;

    for (i32 i = 0; i < kernelSize; ++i) {
        f32 x = static_cast<f32>(i - kernelRadius);
        kernel[i] = std::exp(-(x * x) / (2.0f * sigma * sigma));
        kernelSum += kernel[i];
    }
    for (i32 i = 0; i < kernelSize; ++i) {
        kernel[i] /= kernelSum;
    }

    usize bufSize = static_cast<usize>(m_Width) * m_Height * 4;
    std::vector<u8> temp(bufSize);

    // Horizontal pass
    for (u32 y = 0; y < m_Height; ++y) {
        for (u32 x = 0; x < m_Width; ++x) {
            f32 r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
            for (i32 k = -kernelRadius; k <= kernelRadius; ++k) {
                i32 sx = std::clamp(static_cast<i32>(x) + k, 0, static_cast<i32>(m_Width) - 1);
                usize srcIdx = (static_cast<usize>(y) * m_Width + sx) * 4;
                f32 w = kernel[k + kernelRadius];
                r += static_cast<f32>(buffer[srcIdx + 0]) * w;
                g += static_cast<f32>(buffer[srcIdx + 1]) * w;
                b += static_cast<f32>(buffer[srcIdx + 2]) * w;
                a += static_cast<f32>(buffer[srcIdx + 3]) * w;
            }
            usize dstIdx = (static_cast<usize>(y) * m_Width + x) * 4;
            temp[dstIdx + 0] = static_cast<u8>(std::clamp(r, 0.0f, 255.0f));
            temp[dstIdx + 1] = static_cast<u8>(std::clamp(g, 0.0f, 255.0f));
            temp[dstIdx + 2] = static_cast<u8>(std::clamp(b, 0.0f, 255.0f));
            temp[dstIdx + 3] = static_cast<u8>(std::clamp(a, 0.0f, 255.0f));
        }
    }

    // Vertical pass
    for (u32 y = 0; y < m_Height; ++y) {
        for (u32 x = 0; x < m_Width; ++x) {
            f32 r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
            for (i32 k = -kernelRadius; k <= kernelRadius; ++k) {
                i32 sy = std::clamp(static_cast<i32>(y) + k, 0, static_cast<i32>(m_Height) - 1);
                usize srcIdx = (static_cast<usize>(sy) * m_Width + x) * 4;
                f32 w = kernel[k + kernelRadius];
                r += static_cast<f32>(temp[srcIdx + 0]) * w;
                g += static_cast<f32>(temp[srcIdx + 1]) * w;
                b += static_cast<f32>(temp[srcIdx + 2]) * w;
                a += static_cast<f32>(temp[srcIdx + 3]) * w;
            }
            usize dstIdx = (static_cast<usize>(y) * m_Width + x) * 4;
            buffer[dstIdx + 0] = static_cast<u8>(std::clamp(r, 0.0f, 255.0f));
            buffer[dstIdx + 1] = static_cast<u8>(std::clamp(g, 0.0f, 255.0f));
            buffer[dstIdx + 2] = static_cast<u8>(std::clamp(b, 0.0f, 255.0f));
            buffer[dstIdx + 3] = static_cast<u8>(std::clamp(a, 0.0f, 255.0f));
        }
    }
}

// ===========================================================================
// Pixel blending
// ===========================================================================

void FramebufferFeedbackSystem::BlendPixel(u8 srcR, u8 srcG, u8 srcB, u8 srcA,
                                            u8 dstR, u8 dstG, u8 dstB, u8 dstA,
                                            f32 factor,
                                            FeedbackConfig::BlendMode mode,
                                            u8& outR, u8& outG, u8& outB, u8& outA) {
    f32 sR = static_cast<f32>(srcR) / 255.0f;
    f32 sG = static_cast<f32>(srcG) / 255.0f;
    f32 sB = static_cast<f32>(srcB) / 255.0f;
    f32 sA = static_cast<f32>(srcA) / 255.0f;

    f32 dR = static_cast<f32>(dstR) / 255.0f;
    f32 dG = static_cast<f32>(dstG) / 255.0f;
    f32 dB = static_cast<f32>(dstB) / 255.0f;
    f32 dA = static_cast<f32>(dstA) / 255.0f;

    f32 rR, rG, rB, rA;

    switch (mode) {
        case FeedbackConfig::BlendMode::Alpha:
            // Standard alpha blend: src * (1-factor) + dst * factor
            rR = sR * (1.0f - factor) + dR * factor;
            rG = sG * (1.0f - factor) + dG * factor;
            rB = sB * (1.0f - factor) + dB * factor;
            rA = sA * (1.0f - factor) + dA * factor;
            break;

        case FeedbackConfig::BlendMode::Additive:
            rR = sR + dR * factor;
            rG = sG + dG * factor;
            rB = sB + dB * factor;
            rA = std::max(sA, dA);
            break;

        case FeedbackConfig::BlendMode::Multiply:
            rR = sR * (1.0f - factor) + sR * dR * factor;
            rG = sG * (1.0f - factor) + sG * dG * factor;
            rB = sB * (1.0f - factor) + sB * dB * factor;
            rA = sA;
            break;

        case FeedbackConfig::BlendMode::Screen: {
            f32 screenR = 1.0f - (1.0f - sR) * (1.0f - dR);
            f32 screenG = 1.0f - (1.0f - sG) * (1.0f - dG);
            f32 screenB = 1.0f - (1.0f - sB) * (1.0f - dB);
            rR = sR * (1.0f - factor) + screenR * factor;
            rG = sG * (1.0f - factor) + screenG * factor;
            rB = sB * (1.0f - factor) + screenB * factor;
            rA = sA;
            break;
        }

        case FeedbackConfig::BlendMode::Max:
            rR = sR * (1.0f - factor) + std::max(sR, dR) * factor;
            rG = sG * (1.0f - factor) + std::max(sG, dG) * factor;
            rB = sB * (1.0f - factor) + std::max(sB, dB) * factor;
            rA = std::max(sA, dA);
            break;

        default:
            rR = sR;
            rG = sG;
            rB = sB;
            rA = sA;
            break;
    }

    outR = static_cast<u8>(std::clamp(rR * 255.0f, 0.0f, 255.0f));
    outG = static_cast<u8>(std::clamp(rG * 255.0f, 0.0f, 255.0f));
    outB = static_cast<u8>(std::clamp(rB * 255.0f, 0.0f, 255.0f));
    outA = static_cast<u8>(std::clamp(rA * 255.0f, 0.0f, 255.0f));
}

// ===========================================================================
// Color transform (saturation, brightness, tint)
// ===========================================================================

void FramebufferFeedbackSystem::ApplyColorTransform(u8& r, u8& g, u8& b,
                                                     f32 saturation, f32 brightness,
                                                     const Math::Vector3& tint) {
    f32 fr = static_cast<f32>(r) / 255.0f;
    f32 fg = static_cast<f32>(g) / 255.0f;
    f32 fb = static_cast<f32>(b) / 255.0f;

    // Saturation: lerp toward luminance
    if (saturation < 1.0f - Math::EPSILON || saturation > 1.0f + Math::EPSILON) {
        f32 lum = fr * 0.2126f + fg * 0.7152f + fb * 0.0722f;
        fr = lum + (fr - lum) * saturation;
        fg = lum + (fg - lum) * saturation;
        fb = lum + (fb - lum) * saturation;
    }

    // Brightness
    fr *= brightness;
    fg *= brightness;
    fb *= brightness;

    // Tint
    fr *= tint.x;
    fg *= tint.y;
    fb *= tint.z;

    r = static_cast<u8>(std::clamp(fr * 255.0f, 0.0f, 255.0f));
    g = static_cast<u8>(std::clamp(fg * 255.0f, 0.0f, 255.0f));
    b = static_cast<u8>(std::clamp(fb * 255.0f, 0.0f, 255.0f));
}

// ===========================================================================
// Main compositing pipeline
// ===========================================================================

void FramebufferFeedbackSystem::CompositeWithFeedback(const u8* currentFrame, u8* outputBuffer) {
    if (!m_Initialized || !currentFrame || !outputBuffer) return;
    if (m_Width == 0 || m_Height == 0) return;

    usize bufSize = static_cast<usize>(m_Width) * m_Height * 4;

    // If no effect is active, just copy through
    if (m_Config.preset == FeedbackPreset::None && m_Config.blendFactor <= 0.0f) {
        std::memcpy(outputBuffer, currentFrame, bufSize);
        return;
    }

    const u8* prevBuffer = GetPreviousBuffer();
    u8* nextBuffer = GetNextBuffer();

    f32 effectiveFactor = m_Config.blendFactor * (1.0f - m_Config.decayRate);
    effectiveFactor = std::clamp(effectiveFactor, 0.0f, 1.0f);

    // Precompute UV transform parameters
    f32 cosR = std::cos(m_Config.uvRotation);
    f32 sinR = std::sin(m_Config.uvRotation);
    f32 pivotU = m_Config.uvPivot.x;
    f32 pivotV = m_Config.uvPivot.y;
    f32 scale = m_Config.uvScale;

    // Process each pixel
    for (u32 py = 0; py < m_Height; ++py) {
        for (u32 px = 0; px < m_Width; ++px) {
            usize pixelIdx = (static_cast<usize>(py) * m_Width + px) * 4;

            // Step 1: Compute UV for this pixel
            f32 u = static_cast<f32>(px) / static_cast<f32>(m_Width);
            f32 v = static_cast<f32>(py) / static_cast<f32>(m_Height);

            // Step 2: Apply UV transform (scale, rotate around pivot, offset)
            f32 cu = u - pivotU;
            f32 cv = v - pivotV;

            // Scale
            cu *= scale;
            cv *= scale;

            // Rotate
            f32 ru = cu * cosR - cv * sinR;
            f32 rv = cu * sinR + cv * cosR;

            // Offset + pivot restore
            ru += pivotU + m_Config.uvOffset.x;
            rv += pivotV + m_Config.uvOffset.y;

            // Step 3: Apply distortion (noise, scanlines)
            if (m_Config.noiseStrength > 0.0f) {
                ru += (NextRandom() - 0.5f) * 2.0f * m_Config.noiseStrength;
                rv += (NextRandom() - 0.5f) * 2.0f * m_Config.noiseStrength;
            }

            if (m_Config.scanlineOffset > 0.0f && m_Config.scanlineWidth > 0) {
                i32 scanY = static_cast<i32>(py) % (m_Config.scanlineWidth * 2);
                if (scanY < m_Config.scanlineWidth) {
                    ru += m_Config.scanlineOffset / static_cast<f32>(m_Width);
                }
            }

            // Step 4: Sample previous feedback buffer with transformed UVs
            u8 fbR, fbG, fbB, fbA;
            SampleTransformed(prevBuffer, ru, rv, fbR, fbG, fbB, fbA);

            // Step 5: Blend with current frame
            u8 curR = currentFrame[pixelIdx + 0];
            u8 curG = currentFrame[pixelIdx + 1];
            u8 curB = currentFrame[pixelIdx + 2];
            u8 curA = currentFrame[pixelIdx + 3];

            u8 outR, outG, outB, outA;
            BlendPixel(curR, curG, curB, curA,
                       fbR, fbG, fbB, fbA,
                       effectiveFactor,
                       m_Config.blendMode,
                       outR, outG, outB, outA);

            // Step 6: Apply color transform
            ApplyColorTransform(outR, outG, outB,
                                m_Config.saturation,
                                m_Config.brightness,
                                m_Config.colorTint);

            // Write to next buffer and output
            nextBuffer[pixelIdx + 0] = outR;
            nextBuffer[pixelIdx + 1] = outG;
            nextBuffer[pixelIdx + 2] = outB;
            nextBuffer[pixelIdx + 3] = outA;

            outputBuffer[pixelIdx + 0] = outR;
            outputBuffer[pixelIdx + 1] = outG;
            outputBuffer[pixelIdx + 2] = outB;
            outputBuffer[pixelIdx + 3] = outA;
        }
    }

    // Step 7: Apply optional blur on the next buffer (for next frame's feedback)
    if (m_Config.blurRadius > 0.0f) {
        ApplyBlur(nextBuffer, m_Config.blurRadius);
    }

    // Swap ping-pong buffers
    SwapBuffers();
}

} // namespace Effects
} // namespace Enjin
