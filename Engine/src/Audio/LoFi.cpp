#include "Enjin/Audio/LoFi.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

namespace Enjin {
namespace Audio {

namespace {
// Guards the pending copy only. The audio thread never waits on it: when the
// game is mid-write it keeps the parameters it already has for one more block.
std::mutex g_ParamsMutex;
constexpr f32 kPi = 3.14159265f;
constexpr u32 kDelayFrames = 2048;          // ~43 ms at 48 kHz, room for the wobble
constexpr f32 kMaxWobbleSeconds = 0.004f;   // depth at wobble = 1
}

void LoFiProcessor::SetParams(const LoFiParams& p) {
    std::lock_guard<std::mutex> lock(g_ParamsMutex);
    m_Pending = p;
    m_HasPending = true;
}

void LoFiProcessor::Process(f32* io, u64 frames, u32 sampleRate) {
    if (m_HasPending && g_ParamsMutex.try_lock()) {
        m_Active = m_Pending;
        m_HasPending = false;
        g_ParamsMutex.unlock();
    }
    const LoFiParams& p = m_Active;
    if (!p.enabled || p.intensity <= 0.0f || !io || sampleRate == 0) return;
    if (m_Delay.empty()) m_Delay.assign(kDelayFrames * 2, 0.0f);

    const f32 sr = static_cast<f32>(sampleRate);
    const f32 mix = std::clamp(p.intensity, 0.0f, 1.0f);
    const f32 holdStep = std::clamp(p.sampleRateReduction, 0.01f, 1.0f);   // < 1 holds samples
    const f32 bits = std::clamp(16.0f * p.bitDepthReduction, 1.0f, 16.0f);
    const f32 levels = std::pow(2.0f, bits - 1.0f);
    const bool crush = p.bitDepthReduction < 0.999f;
    const f32 cutoff = std::clamp(p.lowPassCutoff, 20.0f, sr * 0.49f);
    const f32 lpA = 1.0f - std::exp(-2.0f * kPi * cutoff / sr);
    const bool lowPass = p.lowPassCutoff < 19999.0f;
    const f32 drive = 1.0f + 4.0f * std::clamp(p.saturation, 0.0f, 1.0f);
    const f32 driveNorm = 1.0f / std::tanh(drive);
    const f32 width = std::clamp(p.stereoWidth, 0.0f, 2.0f);
    const f32 wobbleDepth = std::clamp(p.wobble, 0.0f, 1.0f) * kMaxWobbleSeconds * sr;
    const f32 wobbleInc = 2.0f * kPi * std::max(p.wobbleSpeed, 0.0f) / sr;

    for (u64 f = 0; f < frames; ++f) {
        const f32 dryL = io[f * 2], dryR = io[f * 2 + 1];
        f32 l = dryL, r = dryR;

        // Wobble: a slowly swept delay, which is what a warped tape does to pitch
        if (wobbleDepth > 0.0f) {
            m_Delay[m_DelayPos * 2] = l;
            m_Delay[m_DelayPos * 2 + 1] = r;
            const f32 d = wobbleDepth * (1.0f + std::sin(m_WobblePhase)) * 0.5f + 1.0f;
            m_WobblePhase += wobbleInc;
            if (m_WobblePhase > 2.0f * kPi) m_WobblePhase -= 2.0f * kPi;
            f32 readPos = static_cast<f32>(m_DelayPos) - d;
            while (readPos < 0.0f) readPos += static_cast<f32>(kDelayFrames);
            const u32 i0 = static_cast<u32>(readPos) % kDelayFrames;
            const u32 i1 = (i0 + 1) % kDelayFrames;
            const f32 t = readPos - std::floor(readPos);
            l = m_Delay[i0 * 2] * (1.0f - t) + m_Delay[i1 * 2] * t;
            r = m_Delay[i0 * 2 + 1] * (1.0f - t) + m_Delay[i1 * 2 + 1] * t;
            m_DelayPos = (m_DelayPos + 1) % kDelayFrames;
        }

        // Sample-rate reduction: hold each sample for 1/rate frames
        if (holdStep < 0.999f) {
            m_HoldPhase += holdStep;
            if (m_HoldPhase >= 1.0f) { m_HoldPhase -= 1.0f; m_HoldL = l; m_HoldR = r; }
            l = m_HoldL; r = m_HoldR;
        }

        // Bit depth
        if (crush) {
            l = std::round(l * levels) / levels;
            r = std::round(r * levels) / levels;
        }

        // Low-pass
        if (lowPass) {
            m_LpL += lpA * (l - m_LpL);
            m_LpR += lpA * (r - m_LpR);
            l = m_LpL; r = m_LpR;
        }

        // Saturation
        if (drive > 1.0f) {
            l = std::tanh(l * drive) * driveNorm;
            r = std::tanh(r * drive) * driveNorm;
        }

        // Hiss
        if (p.noiseFloor > 0.0f) {
            m_Noise = m_Noise * 1664525u + 1013904223u;
            const f32 n = (static_cast<f32>(m_Noise >> 8) / 8388608.0f - 1.0f) * p.noiseFloor;
            l += n; r += n;
        }

        // Stereo width (mid/side)
        if (width != 1.0f) {
            const f32 mid = 0.5f * (l + r), side = 0.5f * (l - r) * width;
            l = mid + side; r = mid - side;
        }

        io[f * 2]     = dryL + (l - dryL) * mix;
        io[f * 2 + 1] = dryR + (r - dryR) * mix;
    }
}

} // namespace Audio
} // namespace Enjin
