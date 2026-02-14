#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Math.h"
#include <vector>
#include <cstring>

namespace Enjin {
namespace Effects {

// ---------------------------------------------------------------------------
// Feedback presets
// ---------------------------------------------------------------------------
enum class FeedbackPreset : u8 {
    None,
    Echo,           // Fading ghost trails
    Melt,           // Downward UV shift with blur
    InfiniteMirror, // Recursive scale-down toward center
    VHSTracking,    // Horizontal scan line offset with color bleed
    Kaleidoscope,   // Rotational symmetry feedback
    Phosphor,       // CRT phosphor persistence (slow decay)
    DreamSequence,  // Wavy UV distortion with soft glow
    Custom          // User-defined parameters
};

// ---------------------------------------------------------------------------
// FeedbackConfig — Full set of parameters controlling the feedback effect
// ---------------------------------------------------------------------------
struct FeedbackConfig {
    FeedbackPreset preset = FeedbackPreset::None;

    // Blend
    f32 blendFactor = 0.85f;    // How much of previous frame to keep (0=none, 1=full)
    enum class BlendMode : u8 { Alpha, Additive, Multiply, Screen, Max } blendMode = BlendMode::Alpha;
    f32 decayRate = 0.05f;      // Per-frame fade-out speed

    // UV Transform (applied to previous frame UVs before sampling)
    Math::Vector2 uvOffset = {0.0f, 0.0f};     // Per-frame UV shift
    f32 uvRotation = 0.0f;                      // Per-frame rotation (radians)
    f32 uvScale = 1.0f;                         // Per-frame scale (< 1 = zoom in = infinite mirror)
    Math::Vector2 uvPivot = {0.5f, 0.5f};       // Rotation/scale pivot

    // Distortion
    f32 noiseStrength = 0.0f;    // Random UV noise per frame
    f32 scanlineOffset = 0.0f;   // VHS-style horizontal shift
    i32 scanlineWidth = 2;       // Affected scan line height

    // Post
    f32 blurRadius = 0.0f;       // Gaussian blur on feedback buffer (0, 1, 2, or 3)
    f32 saturation = 1.0f;       // Color saturation of feedback (< 1 = desaturate over time)
    f32 brightness = 1.0f;       // Brightness multiplier per iteration
    Math::Vector3 colorTint = {1.0f, 1.0f, 1.0f};  // Color shift per iteration
};

// ---------------------------------------------------------------------------
// FeedbackComponent — ECS component that attaches feedback to an entity/camera
// ---------------------------------------------------------------------------
struct FeedbackComponent {
    FeedbackConfig config;
    bool enabled = true;
};

// ---------------------------------------------------------------------------
// FramebufferFeedbackSystem — CPU-side RGBA8 feedback compositing system
//
// Maintains two ping-pong buffers. Each frame:
//   1. Read previous feedback buffer
//   2. Apply UV transform (offset, rotation, scale around pivot)
//   3. Apply optional distortion (noise, scanlines)
//   4. Apply optional blur (separable Gaussian)
//   5. Blend with current frame using selected blend mode
//   6. Apply color transform (saturation, brightness, tint)
//   7. Write to output and swap ping-pong buffers
// ---------------------------------------------------------------------------
class ENJIN_API FramebufferFeedbackSystem {
public:
    FramebufferFeedbackSystem() = default;
    ~FramebufferFeedbackSystem() = default;

    // Allocate ping-pong buffers
    void Initialize(u32 width, u32 height);

    // Update time-based UV animation
    void Update(f32 dt);

    // Main compositing method — reads currentFrame (RGBA8, width*height*4 bytes),
    // writes to outputBuffer (same size). Applies the full feedback pipeline.
    void CompositeWithFeedback(const u8* currentFrame, u8* outputBuffer);

    // Fill FeedbackConfig with preset-specific values
    void ApplyPreset(FeedbackPreset preset);

    // Reallocate buffers on viewport resize
    void Resize(u32 width, u32 height);

    // Configuration access
    FeedbackConfig& GetConfig() { return m_Config; }
    const FeedbackConfig& GetConfig() const { return m_Config; }
    void SetConfig(const FeedbackConfig& config) { m_Config = config; }

    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    bool IsInitialized() const { return m_Initialized; }

private:
    FeedbackConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;
    bool m_Initialized = false;
    f32 m_Time = 0.0f;                 // Accumulated time for animation

    // Ping-pong RGBA8 buffers
    std::vector<u8> m_BufferA;
    std::vector<u8> m_BufferB;
    bool m_CurrentIsA = true;           // Which buffer holds the previous result

    // Internal helpers
    u8* GetPreviousBuffer();
    u8* GetNextBuffer();
    void SwapBuffers();

    // Sample the previous feedback buffer with UV transform applied.
    // Returns bilinear-interpolated RGBA at the transformed UV coordinate.
    void SampleTransformed(const u8* src, f32 u, f32 v, u8& outR, u8& outG, u8& outB, u8& outA) const;

    // Apply separable Gaussian blur in-place on an RGBA8 buffer
    void ApplyBlur(u8* buffer, f32 radius);

    // Blend two RGBA pixels according to blend mode
    static void BlendPixel(u8 srcR, u8 srcG, u8 srcB, u8 srcA,
                           u8 dstR, u8 dstG, u8 dstB, u8 dstA,
                           f32 factor,
                           FeedbackConfig::BlendMode mode,
                           u8& outR, u8& outG, u8& outB, u8& outA);

    // Apply color transforms (saturation, brightness, tint) to a single pixel
    static void ApplyColorTransform(u8& r, u8& g, u8& b,
                                    f32 saturation, f32 brightness,
                                    const Math::Vector3& tint);

    // Simple xorshift PRNG for noise
    u32 m_RngState = 12345;
    f32 NextRandom();
};

} // namespace Effects
} // namespace Enjin
