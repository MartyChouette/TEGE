#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <atomic>
#include <vector>

namespace Enjin {
namespace Audio {

// The lo-fi master effect behind AudioFidelityComponent (SD-27): the numbers
// that make a game sound like the machine it looks like. Everything is a
// fraction or a frequency so a preset can be written as data.
struct LoFiParams {
    bool enabled = false;
    f32 intensity = 1.0f;            // 0 = untouched, 1 = fully processed
    f32 sampleRateReduction = 1.0f;  // 1 = native, 0.5 = half the rate (sample and hold)
    f32 bitDepthReduction = 1.0f;    // 1 = full, 0.5 = 8-bit
    f32 lowPassCutoff = 20000.0f;    // Hz
    f32 noiseFloor = 0.0f;           // hiss level, 0-0.1
    f32 wobble = 0.0f;               // tape/vinyl pitch wobble, 0-1
    f32 wobbleSpeed = 0.5f;          // Hz
    f32 saturation = 0.0f;           // warm drive, 0-1
    f32 stereoWidth = 1.0f;          // 0 = mono, 1 = as mixed
};

// Runs on interleaved stereo float, in place. Owned by the audio thread; the
// game hands it new parameters with SetParams from any thread.
class ENJIN_API LoFiProcessor {
public:
    void SetParams(const LoFiParams& p);
    void Process(f32* interleavedStereo, u64 frames, u32 sampleRate);

private:
    LoFiParams m_Pending;
    LoFiParams m_Active;
    std::atomic<bool> m_HasPending{false};

    // State
    f32 m_HoldL = 0.0f, m_HoldR = 0.0f, m_HoldPhase = 1.0f;
    f32 m_LpL = 0.0f, m_LpR = 0.0f;
    f32 m_WobblePhase = 0.0f;
    u32 m_Noise = 22222u;
    std::vector<f32> m_Delay;       // stereo ring for the wobble
    u32 m_DelayPos = 0;
};

} // namespace Audio
} // namespace Enjin
