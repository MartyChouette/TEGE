#include "Enjin/Acoustics/ReverbDSP.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Acoustics {

namespace {

// Mutually prime, so the delay lines do not line up and produce a ringing
// pitch. These are the ratios; the actual lengths are scaled by room size.
constexpr f32 kLineRatios[FeedbackDelayNetwork::kLines] = {
    1.000f, 1.147f, 1.319f, 1.471f, 1.637f, 1.801f, 1.973f, 2.131f,
};

// The largest prime not greater than n.
//
// Prime lengths are the standard trick: two delay lines whose lengths share a
// factor repeat together, and what that sounds like is a note in the tail.
usize PrimeAtMost(usize n) {
    if (n < 2) return 2;
    auto isPrime = [](usize v) {
        if (v < 2) return false;
        if (v % 2 == 0) return v == 2;
        for (usize d = 3; d * d <= v; d += 2) {
            if (v % d == 0) return false;
        }
        return true;
    };
    while (n > 2 && !isPrime(n)) --n;
    return n;
}

// Feedback gain for a 60 dB decay over `rt60` seconds, for a line this long.
//
// This IS the definition of RT60 rearranged: after rt60 seconds the signal has
// gone round the line rt60/delay times, and the product of the gains must be
// -60 dB.
f32 DecayGain(f32 delaySeconds, f32 rt60) {
    if (rt60 <= 1.0e-4f || delaySeconds <= 0.0f) return 0.0f;
    return std::pow(10.0f, -3.0f * delaySeconds / rt60);
}

} // namespace

// ---------------------------------------------------------------------------
// DelayLine
// ---------------------------------------------------------------------------

void DelayLine::Resize(usize samples) {
    m_Buffer.assign(std::max<usize>(samples, 1), 0.0f);
    m_Write = 0;
}

void DelayLine::Clear() {
    std::fill(m_Buffer.begin(), m_Buffer.end(), 0.0f);
    m_Write = 0;
}

void DelayLine::Write(f32 value) {
    if (m_Buffer.empty()) return;
    m_Buffer[m_Write] = value;
    m_Write = (m_Write + 1) % m_Buffer.size();
}

f32 DelayLine::Read(usize delaySamples) const {
    if (m_Buffer.empty()) return 0.0f;
    // Clamped rather than wrapped. A delay longer than the buffer is a caller
    // error, and wrapping would return a sample from an unrelated moment --
    // an echo at a time nothing happened.
    const usize d = std::min(delaySamples, m_Buffer.size() - 1);
    const usize index = (m_Write + m_Buffer.size() - d - 1) % m_Buffer.size();
    return m_Buffer[index];
}

// ---------------------------------------------------------------------------
// EarlyReflectionRenderer
// ---------------------------------------------------------------------------

void EarlyReflectionRenderer::Prepare(u32 sampleRate, f32 maxDelaySeconds) {
    m_SampleRate = std::max(sampleRate, 1u);
    m_Line.Resize(static_cast<usize>(maxDelaySeconds * static_cast<f32>(m_SampleRate)) + 2);
    m_Taps.clear();
}

void EarlyReflectionRenderer::Clear() {
    m_Line.Clear();
    for (Tap& t : m_Taps) t.tone.Reset();
}

void EarlyReflectionRenderer::SetTaps(const EarlyReflectionResult& reflections) {
    m_Taps.clear();
    m_Taps.reserve(reflections.taps.size());

    for (const EarlyReflection& r : reflections.taps) {
        const usize delay = static_cast<usize>(r.delay * static_cast<f32>(m_SampleRate) + 0.5f);
        if (delay + 1 >= m_Line.Capacity()) continue;   // beyond what we can hold

        Tap tap;
        tap.delaySamples = delay;
        tap.gain = r.gain[1];   // mid band carries the level

        // The tilt: how much duller this reflection is than it is quiet. A
        // surface that returns the highs as strongly as the mids needs no
        // filtering; carpet needs a lot.
        const f32 mid = std::max(r.gain[1], 1.0e-9f);
        const f32 high = std::max(r.gain[2], 0.0f);
        const f32 ratio = std::min(high / mid, 1.0f);
        // ratio 1 -> no damping; ratio 0 -> as dull as a one-pole gets.
        tap.tone.SetCoefficient(std::min(0.95f, 1.0f - ratio));
        tap.tone.Reset();

        m_Taps.push_back(tap);
    }
}

f32 EarlyReflectionRenderer::Process(f32 input) {
    m_Line.Write(input);
    f32 sum = 0.0f;
    for (Tap& tap : m_Taps) {
        sum += tap.tone.Process(m_Line.Read(tap.delaySamples)) * tap.gain;
    }
    return sum;
}

// ---------------------------------------------------------------------------
// FeedbackDelayNetwork
// ---------------------------------------------------------------------------

void FeedbackDelayNetwork::Prepare(u32 sampleRate) {
    m_SampleRate = std::max(sampleRate, 1u);
    // Room for the longest tail anyone will ask for.
    const usize capacity = static_cast<usize>(0.25f * static_cast<f32>(m_SampleRate)) + 2;
    for (u32 i = 0; i < kLines; ++i) {
        m_Lines[i].Resize(capacity);
        m_Damping[i].Reset();
    }
    m_Configured = false;
}

void FeedbackDelayNetwork::Clear() {
    for (u32 i = 0; i < kLines; ++i) {
        m_Lines[i].Clear();
        m_Damping[i].Reset();
    }
}

void FeedbackDelayNetwork::Configure(const f32 rt60[Audio::kAcousticBands], f32 meanFreePath) {
    const f32 mid = rt60[1];
    const f32 low = std::max(rt60[0], 1.0e-4f);
    const f32 high = std::max(rt60[2], 1.0e-4f);
    m_RT60Mid = mid;

    if (mid <= 1.0e-4f) {
        m_Configured = false;
        return;
    }

    // The shortest delay is the room's own mean free path -- the average time
    // between one surface and the next. That is what makes a cupboard and a
    // cathedral with the same decay time still sound like different sizes: the
    // echo density differs even when the tail length does not.
    const f32 baseSeconds =
        std::max(0.005f, std::min(meanFreePath, 60.0f) / kSpeedOfSound);

    for (u32 i = 0; i < kLines; ++i) {
        const f32 seconds = baseSeconds * kLineRatios[i];
        const usize wanted =
            static_cast<usize>(seconds * static_cast<f32>(m_SampleRate) + 0.5f);
        m_Delays[i] = PrimeAtMost(std::min(wanted, m_Lines[i].Capacity() - 1));

        const f32 actualSeconds =
            static_cast<f32>(m_Delays[i]) / static_cast<f32>(m_SampleRate);

        // Gain set from the LOW band, with the damping filter taking the highs
        // down the rest of the way. Setting it from the mid and damping around
        // that would make the low band decay too fast, and a room's low end is
        // the part that rings longest.
        const f32 gLow = DecayGain(actualSeconds, low);
        const f32 gHigh = DecayGain(actualSeconds, high);
        m_Gains[i] = gLow;

        // A one-pole with DC gain 1 has Nyquist gain (1-a)/(1+a). Solving that
        // for the ratio the two decay times ask for:
        const f32 ratio = std::min(std::max(gHigh / std::max(gLow, 1.0e-9f), 0.0f), 0.999f);
        const f32 a = (1.0f - ratio) / (1.0f + ratio);
        m_Damping[i].SetCoefficient(std::min(a, 0.95f));
        m_Damping[i].Reset();
        m_Lines[i].Clear();
    }
    m_Configured = true;
}

void FeedbackDelayNetwork::Process(f32 input, f32& outLeft, f32& outRight) {
    outLeft = 0.0f;
    outRight = 0.0f;
    if (!m_Configured) return;

    f32 read[kLines];
    f32 sum = 0.0f;
    for (u32 i = 0; i < kLines; ++i) {
        read[i] = m_Lines[i].Read(m_Delays[i]);
        sum += read[i];
    }

    // Householder: y = x - (2/N) * sum(x). Lossless, so the ONLY energy removed
    // is what the gains remove -- which is what makes the decay time the number
    // it was asked for rather than something that emerged from the mixing.
    const f32 correction = sum * (2.0f / static_cast<f32>(kLines));

    for (u32 i = 0; i < kLines; ++i) {
        const f32 mixed = read[i] - correction;
        const f32 damped = m_Damping[i].Process(mixed);
        m_Lines[i].Write(input + damped * m_Gains[i]);
    }

    // Two different combinations of the same lines, so it reads as one space
    // heard from two ears rather than as two rooms.
    for (u32 i = 0; i < kLines; ++i) {
        if (i % 2 == 0) outLeft += read[i];
        else            outRight += read[i];
    }
    const f32 norm = 2.0f / static_cast<f32>(kLines);
    outLeft *= norm;
    outRight *= norm;
}

// ---------------------------------------------------------------------------
// RoomReverb
// ---------------------------------------------------------------------------

void RoomReverb::Prepare(u32 sampleRate) {
    m_Early.Prepare(sampleRate);
    m_Fdn.Prepare(sampleRate);
}

void RoomReverb::Clear() {
    m_Early.Clear();
    m_Fdn.Clear();
}

void RoomReverb::Configure(const RoomResponse& room, const EarlyReflectionResult& reflections) {
    m_Early.SetTaps(reflections);
    m_Fdn.Configure(room.rt60, room.meanFreePath);

    // How much tail, from how much energy actually came back. This is the
    // difference between a field and a stairwell, and it is a thing the decay
    // time cannot say: an open field has almost no reflected energy at all, and
    // whatever tail it has should be nearly inaudible.
    m_LateLevel = std::min(1.0f, room.reflectedEnergy);
}

void RoomReverb::Process(f32 input, f32& outLeft, f32& outRight) {
    const f32 early = m_Early.Process(input);
    f32 lateL = 0.0f, lateR = 0.0f;
    m_Fdn.Process(input, lateL, lateR);
    outLeft = early + lateL * m_LateLevel;
    outRight = early + lateR * m_LateLevel;
}

} // namespace Acoustics
} // namespace Enjin
