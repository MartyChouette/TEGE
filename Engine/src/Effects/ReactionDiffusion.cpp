#include "Enjin/Effects/ReactionDiffusion.h"
#include <algorithm>
#include <cmath>
#include <chrono>

namespace Enjin {
namespace Effects {

// xorshift32 PRNG (same pattern used elsewhere in the engine)
static u32 XorShift32(u32& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void ReactionDiffusion::Initialize(const RDConfig& config) {
    m_Config = config;

    // Clamp dimensions to sane range
    m_Config.width = std::clamp(m_Config.width, 8u, 2048u);
    m_Config.height = std::clamp(m_Config.height, 8u, 2048u);

    u32 w = m_Config.width;
    u32 h = m_Config.height;
    u32 totalCells = w * h;

    // Allocate grids
    m_State.width = w;
    m_State.height = h;
    m_State.stepCount = 0;
    m_State.chemU.assign(totalCells, 1.0f);
    m_State.chemV.assign(totalCells, 0.0f);
    m_TempU.resize(totalCells);
    m_TempV.resize(totalCells);

    // Apply preset parameters if not Custom
    if (m_Config.preset != RDPreset::Custom) {
        RDConfig presetCfg = GetPresetConfig(m_Config.preset);
        m_Config.feedRate = presetCfg.feedRate;
        m_Config.killRate = presetCfg.killRate;
        m_Config.diffusionU = presetCfg.diffusionU;
        m_Config.diffusionV = presetCfg.diffusionV;
    }

    // Seed initial perturbation based on preset type
    u32 seedVal = m_Config.seed;
    if (seedVal == 0) {
        seedVal = static_cast<u32>(
            std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
    }
    // Ensure seed is nonzero for xorshift
    if (seedVal == 0) seedVal = 1;

    switch (m_Config.preset) {
        case RDPreset::Spirals: {
            // Single off-center seed for spiral formation
            f32 cx = static_cast<f32>(w) * 0.5f + static_cast<f32>(w) * 0.05f;
            f32 cy = static_cast<f32>(h) * 0.5f;
            SeedCircle(cx, cy, static_cast<f32>(std::min(w, h)) * 0.04f);
            break;
        }
        case RDPreset::BubblePacking:
        case RDPreset::MitosisSpots:
        case RDPreset::Leopard: {
            // Scattered random seeds
            u32 count = std::max(5u, (w * h) / 2000);
            f32 spotR = static_cast<f32>(std::min(w, h)) * 0.02f;
            u32 rng = seedVal;
            for (u32 i = 0; i < count; ++i) {
                f32 sx = static_cast<f32>(XorShift32(rng) % w);
                f32 sy = static_cast<f32>(XorShift32(rng) % h);
                SeedCircle(sx, sy, spotR);
            }
            break;
        }
        default: {
            // Central rectangular region with some noise for most presets
            f32 cx = static_cast<f32>(w) * 0.5f;
            f32 cy = static_cast<f32>(h) * 0.5f;
            f32 regionR = static_cast<f32>(std::min(w, h)) * 0.1f;
            SeedCircle(cx, cy, regionR);

            // Add a few random satellite seeds
            u32 rng = seedVal;
            for (u32 i = 0; i < 5; ++i) {
                f32 sx = cx + static_cast<f32>(static_cast<i32>(XorShift32(rng) % (w / 3)) - static_cast<i32>(w / 6));
                f32 sy = cy + static_cast<f32>(static_cast<i32>(XorShift32(rng) % (h / 3)) - static_cast<i32>(h / 6));
                SeedCircle(sx, sy, regionR * 0.3f);
            }
            break;
        }
    }
}

void ReactionDiffusion::Update(f32 dt) {
    // dt is passed for API consistency but we use config.deltaTime for simulation stability
    (void)dt;
    u32 steps = std::clamp(m_Config.stepsPerFrame, 1u, 100u);
    for (u32 i = 0; i < steps; ++i) {
        Step();
    }
}

void ReactionDiffusion::Step() {
    u32 w = m_State.width;
    u32 h = m_State.height;
    if (w == 0 || h == 0) return;

    f32 f = m_Config.feedRate;
    f32 k = m_Config.killRate;
    f32 dU = m_Config.diffusionU;
    f32 dV = m_Config.diffusionV;
    f32 dt = m_Config.deltaTime;

    const auto& U = m_State.chemU;
    const auto& V = m_State.chemV;

    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            u32 idx = y * w + x;
            f32 u = U[idx];
            f32 v = V[idx];
            f32 uvv = u * v * v;

            f32 lapU = LaplacianU(x, y);
            f32 lapV = LaplacianV(x, y);

            // Gray-Scott equations:
            //   dU/dt = Du * laplacian(U) - U*V^2 + f*(1-U)
            //   dV/dt = Dv * laplacian(V) + U*V^2 - (f+k)*V
            f32 newU = u + (dU * lapU - uvv + f * (1.0f - u)) * dt;
            f32 newV = v + (dV * lapV + uvv - (f + k) * v) * dt;

            m_TempU[idx] = std::clamp(newU, 0.0f, 1.0f);
            m_TempV[idx] = std::clamp(newV, 0.0f, 1.0f);
        }
    }

    // Swap buffers
    std::swap(m_State.chemU, m_TempU);
    std::swap(m_State.chemV, m_TempV);

    ++m_State.stepCount;
}

void ReactionDiffusion::SeedCircle(f32 cx, f32 cy, f32 radius) {
    u32 w = m_State.width;
    u32 h = m_State.height;
    if (w == 0 || h == 0) return;

    f32 r2 = radius * radius;
    i32 minX = static_cast<i32>(cx - radius) - 1;
    i32 maxX = static_cast<i32>(cx + radius) + 1;
    i32 minY = static_cast<i32>(cy - radius) - 1;
    i32 maxY = static_cast<i32>(cy + radius) + 1;

    for (i32 iy = minY; iy <= maxY; ++iy) {
        for (i32 ix = minX; ix <= maxX; ++ix) {
            f32 dx = static_cast<f32>(ix) - cx;
            f32 dy = static_cast<f32>(iy) - cy;
            if (dx * dx + dy * dy <= r2) {
                // Handle wrapping or clamping
                i32 px = ix;
                i32 py = iy;
                if (m_Config.wrapEdges) {
                    px = ((px % static_cast<i32>(w)) + static_cast<i32>(w)) % static_cast<i32>(w);
                    py = ((py % static_cast<i32>(h)) + static_cast<i32>(h)) % static_cast<i32>(h);
                } else {
                    if (px < 0 || px >= static_cast<i32>(w) || py < 0 || py >= static_cast<i32>(h))
                        continue;
                }
                u32 idx = static_cast<u32>(py) * w + static_cast<u32>(px);
                m_State.chemU[idx] = 0.5f;
                m_State.chemV[idx] = 1.0f;
            }
        }
    }
}

void ReactionDiffusion::SeedRandom(u32 count, f32 spotRadius) {
    u32 w = m_State.width;
    u32 h = m_State.height;
    if (w == 0 || h == 0) return;

    u32 rng = m_Config.seed;
    if (rng == 0) {
        rng = static_cast<u32>(
            std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
    }
    if (rng == 0) rng = 1;

    count = std::min(count, 10000u);
    for (u32 i = 0; i < count; ++i) {
        f32 cx = static_cast<f32>(XorShift32(rng) % w);
        f32 cy = static_cast<f32>(XorShift32(rng) % h);
        SeedCircle(cx, cy, spotRadius);
    }
}

RDConfig ReactionDiffusion::GetPresetConfig(RDPreset preset) {
    RDConfig cfg;
    switch (preset) {
        case RDPreset::MitosisSpots:
            cfg.feedRate = 0.0367f;
            cfg.killRate = 0.0649f;
            cfg.diffusionU = 1.0f;
            cfg.diffusionV = 0.5f;
            break;
        case RDPreset::CoralGrowth:
            cfg.feedRate = 0.0545f;
            cfg.killRate = 0.062f;
            cfg.diffusionU = 1.0f;
            cfg.diffusionV = 0.5f;
            break;
        case RDPreset::Fingerprints:
            cfg.feedRate = 0.0545f;
            cfg.killRate = 0.062f;
            cfg.diffusionU = 0.21f;
            cfg.diffusionV = 0.105f;
            break;
        case RDPreset::Leopard:
            cfg.feedRate = 0.026f;
            cfg.killRate = 0.051f;
            cfg.diffusionU = 1.0f;
            cfg.diffusionV = 0.5f;
            break;
        case RDPreset::Labyrinth:
            cfg.feedRate = 0.029f;
            cfg.killRate = 0.057f;
            cfg.diffusionU = 1.0f;
            cfg.diffusionV = 0.5f;
            break;
        case RDPreset::WormHoles:
            cfg.feedRate = 0.039f;
            cfg.killRate = 0.058f;
            cfg.diffusionU = 1.0f;
            cfg.diffusionV = 0.5f;
            break;
        case RDPreset::BubblePacking:
            cfg.feedRate = 0.012f;
            cfg.killRate = 0.047f;
            cfg.diffusionU = 1.0f;
            cfg.diffusionV = 0.5f;
            break;
        case RDPreset::Spirals:
            cfg.feedRate = 0.014f;
            cfg.killRate = 0.045f;
            cfg.diffusionU = 1.0f;
            cfg.diffusionV = 0.5f;
            break;
        case RDPreset::Custom:
        default:
            // Return default values
            break;
    }
    cfg.preset = preset;
    return cfg;
}

const char* ReactionDiffusion::GetPresetName(RDPreset preset) {
    switch (preset) {
        case RDPreset::MitosisSpots:  return "Mitosis Spots";
        case RDPreset::CoralGrowth:   return "Coral Growth";
        case RDPreset::Fingerprints:  return "Fingerprints";
        case RDPreset::Leopard:       return "Leopard";
        case RDPreset::Labyrinth:     return "Labyrinth";
        case RDPreset::WormHoles:     return "Worm Holes";
        case RDPreset::BubblePacking: return "Bubble Packing";
        case RDPreset::Spirals:       return "Spirals";
        case RDPreset::Custom:        return "Custom";
        default:                      return "Unknown";
    }
}

std::vector<u8> ReactionDiffusion::BakeToRGBA8(const Math::Vector3& colorLow,
                                                const Math::Vector3& colorHigh) const {
    u32 w = m_State.width;
    u32 h = m_State.height;
    u32 totalPixels = w * h;
    std::vector<u8> pixels(totalPixels * 4);

    if (m_State.chemV.size() != totalPixels) return pixels;

    for (u32 i = 0; i < totalPixels; ++i) {
        f32 t = std::clamp(m_State.chemV[i], 0.0f, 1.0f);

        // Linear interpolation between low and high colors
        f32 r = colorLow.x + (colorHigh.x - colorLow.x) * t;
        f32 g = colorLow.y + (colorHigh.y - colorLow.y) * t;
        f32 b = colorLow.z + (colorHigh.z - colorLow.z) * t;

        u32 pixelBase = i * 4;
        pixels[pixelBase + 0] = static_cast<u8>(std::clamp(r * 255.0f, 0.0f, 255.0f));
        pixels[pixelBase + 1] = static_cast<u8>(std::clamp(g * 255.0f, 0.0f, 255.0f));
        pixels[pixelBase + 2] = static_cast<u8>(std::clamp(b * 255.0f, 0.0f, 255.0f));
        pixels[pixelBase + 3] = 255;
    }

    return pixels;
}

std::vector<f32> ReactionDiffusion::BakeToHeightmap() const {
    return m_State.chemV;
}

f32 ReactionDiffusion::LaplacianU(u32 x, u32 y) const {
    u32 w = m_State.width;
    u32 idx = y * w + x;

    // 5-point stencil: weights = [0, 1, 0; 1, -4, 1; 0, 1, 0] (normalized)
    u32 xLeft  = WrapX(static_cast<i32>(x) - 1);
    u32 xRight = WrapX(static_cast<i32>(x) + 1);
    u32 yUp    = WrapY(static_cast<i32>(y) - 1);
    u32 yDown  = WrapY(static_cast<i32>(y) + 1);

    f32 center = m_State.chemU[idx];
    f32 left   = m_State.chemU[y * w + xLeft];
    f32 right  = m_State.chemU[y * w + xRight];
    f32 up     = m_State.chemU[yUp * w + x];
    f32 down   = m_State.chemU[yDown * w + x];

    return left + right + up + down - 4.0f * center;
}

f32 ReactionDiffusion::LaplacianV(u32 x, u32 y) const {
    u32 w = m_State.width;
    u32 idx = y * w + x;

    u32 xLeft  = WrapX(static_cast<i32>(x) - 1);
    u32 xRight = WrapX(static_cast<i32>(x) + 1);
    u32 yUp    = WrapY(static_cast<i32>(y) - 1);
    u32 yDown  = WrapY(static_cast<i32>(y) + 1);

    f32 center = m_State.chemV[idx];
    f32 left   = m_State.chemV[y * w + xLeft];
    f32 right  = m_State.chemV[y * w + xRight];
    f32 up     = m_State.chemV[yUp * w + x];
    f32 down   = m_State.chemV[yDown * w + x];

    return left + right + up + down - 4.0f * center;
}

u32 ReactionDiffusion::WrapX(i32 x) const {
    i32 w = static_cast<i32>(m_State.width);
    if (m_Config.wrapEdges) {
        return static_cast<u32>(((x % w) + w) % w);
    }
    return static_cast<u32>(std::clamp(x, 0, w - 1));
}

u32 ReactionDiffusion::WrapY(i32 y) const {
    i32 h = static_cast<i32>(m_State.height);
    if (m_Config.wrapEdges) {
        return static_cast<u32>(((y % h) + h) % h);
    }
    return static_cast<u32>(std::clamp(y, 0, h - 1));
}

} // namespace Effects
} // namespace Enjin
