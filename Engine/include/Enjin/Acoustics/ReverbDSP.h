#pragma once

// Turning a measured room into sound.
//
// The measurement half of this system produces an RT60 per band and a list of
// early reflections. Neither is any use until something renders them, and the
// reverb already in the engine cannot be told either: Freeverb's controls are
// room size and damping, which relate to decay time only by feel. You cannot
// hand it "1.24 seconds" and get 1.24 seconds back.
//
// So the late reverb here is a feedback delay network, where the decay time IS
// the parameter. Each delay line's feedback gain is
//
//     g = 10 ^ (-3 * delaySeconds / RT60)
//
// which is the definition of a 60 dB decay rearranged. That makes the whole
// chain closed-loop testable: measure a room, ask for what was measured, and
// measure the reverb back. A test does exactly that, and it is the only way to
// know the geometry work reaches anybody's ears intact.
//
// Everything here is plain C++ over float buffers. No device, no miniaudio, no
// platform: it runs on the audio thread on desktop and in a browser, and in a
// test with no sound card at all.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Audio/AcousticMaterial.h"
#include "Enjin/Acoustics/EarlyReflections.h"
#include "Enjin/Acoustics/RoomResponse.h"

#include <vector>

namespace Enjin {
namespace Acoustics {

// A circular buffer with an integer tap. Fractional delay would buy sub-sample
// accuracy nobody can hear at these path lengths, at the cost of an
// interpolation per tap per sample.
class ENJIN_API DelayLine {
public:
    void Resize(usize samples);
    void Clear();

    void Write(f32 value);
    f32 Read(usize delaySamples) const;
    usize Capacity() const { return m_Buffer.size(); }

private:
    std::vector<f32> m_Buffer;
    usize m_Write = 0;
};

// One-pole lowpass, the damping in a feedback path.
//
// This is what makes high frequencies decay faster than low ones, which is the
// difference between a carpeted room and a tiled one heard as a TAIL rather
// than as a level.
class ENJIN_API OnePoleLowpass {
public:
    // `coefficient` is the pole, 0 = no filtering, approaching 1 = very dull.
    void SetCoefficient(f32 coefficient) { m_A = coefficient; }
    void Reset() { m_Z = 0.0f; }
    f32 Process(f32 x) {
        m_Z = x * (1.0f - m_A) + m_Z * m_A;
        return m_Z;
    }

private:
    f32 m_A = 0.0f;
    f32 m_Z = 0.0f;
};

// The early reflections, rendered as a tap delay line.
//
// Each tap carries ONE gain and one damping coefficient rather than a full
// three-band filter. Per-band filtering for two dozen taps is seventy-two
// filters running per sample for a difference that lands under the tail; a gain
// plus a tilt captures the audible part, which is that a reflection off carpet
// comes back duller as well as quieter. The simplification is here rather than
// hidden because it is the kind of thing someone later measures and wonders
// about.
class ENJIN_API EarlyReflectionRenderer {
public:
    void Prepare(u32 sampleRate, f32 maxDelaySeconds = 0.5f);
    void Clear();

    // Rebuild the taps from a traced result. Safe to call between blocks, not
    // during one.
    void SetTaps(const EarlyReflectionResult& reflections);

    // One sample in, one sample of early reflections out. The direct sound is
    // NOT included: the caller mixes it, because whether the direct path is
    // audible at all is an occlusion question this does not answer.
    f32 Process(f32 input);

    usize TapCount() const { return m_Taps.size(); }

private:
    struct Tap {
        usize delaySamples = 0;
        f32 gain = 0.0f;
        OnePoleLowpass tone;
    };

    DelayLine m_Line;
    std::vector<Tap> m_Taps;
    u32 m_SampleRate = 48000;
};

// The tail.
//
// Eight delay lines with mutually prime lengths, mixed through a Householder
// matrix -- lossless, so the only energy lost is the energy the feedback gains
// remove, which is what makes the decay time predictable rather than emergent.
class ENJIN_API FeedbackDelayNetwork {
public:
    static constexpr u32 kLines = 8;

    void Prepare(u32 sampleRate);
    void Clear();

    // `rt60` is per band, `meanFreePath` sizes the delay lines so a cathedral
    // and a cupboard do not share a texture even at the same decay time.
    void Configure(const f32 rt60[Audio::kAcousticBands], f32 meanFreePath);

    // One in, stereo out. The two outputs are different combinations of the
    // same lines rather than two networks, which is both cheaper and what makes
    // the tail sound like one space rather than two.
    void Process(f32 input, f32& outLeft, f32& outRight);

    bool IsConfigured() const { return m_Configured; }
    f32 CurrentRT60() const { return m_RT60Mid; }

private:
    DelayLine m_Lines[kLines];
    OnePoleLowpass m_Damping[kLines];
    usize m_Delays[kLines] = {};
    f32 m_Gains[kLines] = {};
    u32 m_SampleRate = 48000;
    f32 m_RT60Mid = 0.0f;
    bool m_Configured = false;
};

// Early plus late, configured from what the tracer measured.
class ENJIN_API RoomReverb {
public:
    void Prepare(u32 sampleRate);
    void Clear();

    // Everything the geometry said, in one call.
    void Configure(const RoomResponse& room, const EarlyReflectionResult& reflections);

    // The wet signal only. The caller decides how much dry to mix, because that
    // is a distance and occlusion question rather than a room one.
    void Process(f32 input, f32& outLeft, f32& outRight);

    bool IsConfigured() const { return m_Fdn.IsConfigured(); }

private:
    EarlyReflectionRenderer m_Early;
    FeedbackDelayNetwork m_Fdn;
    f32 m_LateLevel = 1.0f;
};

} // namespace Acoustics
} // namespace Enjin
