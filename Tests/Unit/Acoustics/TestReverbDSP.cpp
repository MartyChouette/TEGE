// Turning a measured room into sound.
//
// The whole system is only worth anything if what was measured survives being
// rendered. So the central test here is closed-loop: ask the reverb for a decay
// time and then MEASURE the decay time of what it produces, with the same
// Schroeder integration a measurement microphone uses. If those two numbers
// agree, then geometry to materials to RT60 to ears is intact end to end.
//
// That is a test the reverb already in the engine could not pass, because
// Freeverb has no RT60 input -- you can ask it for "room size 0.7" and there is
// no number to compare the answer to.

#include "EnjinTest.h"
#include "Enjin/Acoustics/ReverbDSP.h"
#include "Enjin/Acoustics/RoomResponse.h"
#include "Enjin/Acoustics/EarlyReflections.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace Enjin;
using namespace Enjin::Acoustics;
using Enjin::Math::Vector3;

namespace {

constexpr u32 kRate = 48000;

// The decay time of a recorded impulse response, by Schroeder backward
// integration and a T30 fit -- the same method the room tracer uses on its own
// histogram, applied here to actual samples.
f32 MeasureRT60(const std::vector<f32>& impulse, u32 sampleRate) {
    if (impulse.empty()) return 0.0f;

    std::vector<f64> energy(impulse.size());
    for (usize i = 0; i < impulse.size(); ++i) {
        energy[i] = static_cast<f64>(impulse[i]) * static_cast<f64>(impulse[i]);
    }
    std::vector<f64> schroeder(energy.size(), 0.0);
    f64 running = 0.0;
    for (usize i = energy.size(); i-- > 0;) {
        running += energy[i];
        schroeder[i] = running;
    }
    if (running <= 0.0) return 0.0f;

    const f64 total = schroeder[0];
    f32 t5 = -1.0f, t35 = -1.0f;
    for (usize i = 0; i < schroeder.size(); ++i) {
        if (schroeder[i] <= 0.0) break;
        const f64 db = 10.0 * std::log10(schroeder[i] / total);
        if (t5 < 0.0f && db <= -5.0) t5 = static_cast<f32>(i) / static_cast<f32>(sampleRate);
        if (db <= -35.0) { t35 = static_cast<f32>(i) / static_cast<f32>(sampleRate); break; }
    }
    if (t5 < 0.0f || t35 < 0.0f || t35 <= t5) return 0.0f;
    return 2.0f * (t35 - t5);
}

std::vector<f32> ImpulseResponse(FeedbackDelayNetwork& fdn, f32 seconds) {
    const usize n = static_cast<usize>(seconds * static_cast<f32>(kRate));
    std::vector<f32> out(n, 0.0f);
    for (usize i = 0; i < n; ++i) {
        f32 l = 0.0f, r = 0.0f;
        fdn.Process(i == 0 ? 1.0f : 0.0f, l, r);
        out[i] = (l + r) * 0.5f;
    }
    return out;
}

} // namespace

// --------------------------------------------------------------------------
// The pieces
// --------------------------------------------------------------------------

ENJIN_TEST(ReverbDSP, ADelayLineReturnsWhatWasPutInWhenItWasPutIn) {
    // Arrange
    DelayLine line;
    line.Resize(16);

    // Act / Assert: a one-sample delay returns the previous sample.
    line.Write(1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(line.Read(0), 1.0f, 0.0001f);
    line.Write(2.0f);
    ENJIN_EXPECT_FLOAT_NEAR(line.Read(0), 2.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(line.Read(1), 1.0f, 0.0001f);
    for (u32 i = 0; i < 5; ++i) line.Write(0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(line.Read(6), 1.0f, 0.0001f);
}

ENJIN_TEST(ReverbDSP, ADelayLongerThanTheBufferIsClampedNotWrapped) {
    // Arrange
    DelayLine line;
    line.Resize(8);
    for (u32 i = 0; i < 8; ++i) line.Write(static_cast<f32>(i));

    // Act / Assert: a wrap would return a sample from an unrelated moment --
    // an echo at a time nothing happened. Clamping returns the oldest sample,
    // which is at least a time that existed.
    ENJIN_EXPECT_FLOAT_NEAR(line.Read(999), line.Read(7), 0.0001f);
}

ENJIN_TEST(ReverbDSP, EarlyTapsLandAtTheSampleTheirDelaySaysAndAtTheirGain) {
    // Arrange: two taps, at 10 ms and 25 ms, with known gains. No damping, so
    // the numbers are exact.
    EarlyReflectionResult reflections;
    EarlyReflection a;
    a.delay = 0.010f;
    a.gain[0] = a.gain[1] = a.gain[2] = 0.5f;
    EarlyReflection b;
    b.delay = 0.025f;
    b.gain[0] = b.gain[1] = b.gain[2] = 0.25f;
    reflections.taps = { a, b };

    EarlyReflectionRenderer renderer;
    renderer.Prepare(kRate);
    renderer.SetTaps(reflections);
    ENJIN_ASSERT_EQ(renderer.TapCount(), (usize)2);

    // Act: an impulse, then silence.
    std::vector<f32> out(static_cast<usize>(0.05f * kRate), 0.0f);
    for (usize i = 0; i < out.size(); ++i) out[i] = renderer.Process(i == 0 ? 1.0f : 0.0f);

    // Assert: the delay is a distance and the gain is a material, and both have
    // to survive to the sample. A tap at the wrong time is a room the wrong
    // size; a tap at the wrong gain is a room made of something else.
    const usize first = static_cast<usize>(0.010f * kRate);
    const usize second = static_cast<usize>(0.025f * kRate);
    ENJIN_EXPECT_FLOAT_NEAR(out[first], 0.5f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(out[second], 0.25f, 0.01f);
    // And nothing before the first one.
    for (usize i = 0; i + 1 < first; ++i) ENJIN_EXPECT_FLOAT_NEAR(out[i], 0.0f, 0.0001f);
}

ENJIN_TEST(ReverbDSP, ATapBeyondTheBufferIsDroppedRatherThanFolded) {
    // Arrange: a reflection from half a kilometre away.
    EarlyReflectionResult reflections;
    // Not `far`: windef.h defines it, and so does `near`. Second time in this
    // session, which is what the project's own trap list is for.
    EarlyReflection distant;
    distant.delay = 5.0f;
    distant.gain[0] = distant.gain[1] = distant.gain[2] = 0.5f;
    reflections.taps = { distant };

    EarlyReflectionRenderer renderer;
    renderer.Prepare(kRate, 0.5f);

    // Act
    renderer.SetTaps(reflections);

    // Assert: dropped. Folding it into the buffer would put an echo at a time
    // nothing happened, which is worse than not hearing it.
    ENJIN_EXPECT_EQ(renderer.TapCount(), (usize)0);
}

// --------------------------------------------------------------------------
// The closed loop
// --------------------------------------------------------------------------

ENJIN_TEST(ReverbDSP, TheTailDecaysForAsLongAsItWasAskedTo) {
    // Arrange / Act / Assert: ask for a decay, measure the decay. This is the
    // test Freeverb could not be given, because it has no RT60 to compare an
    // answer against.
    const f32 wanted[] = { 0.4f, 0.8f, 1.5f, 2.5f };
    for (f32 target : wanted) {
        FeedbackDelayNetwork fdn;
        fdn.Prepare(kRate);
        const f32 rt60[Audio::kAcousticBands] = { target, target, target };
        fdn.Configure(rt60, 8.0f);
        ENJIN_ASSERT_TRUE(fdn.IsConfigured());

        const std::vector<f32> ir = ImpulseResponse(fdn, target * 2.5f + 0.5f);
        const f32 measured = MeasureRT60(ir, kRate);

        std::printf("    asked %.2f s, measured %.2f s\n", target, measured);
        ENJIN_EXPECT_TRUE(measured > target * 0.75f);
        ENJIN_EXPECT_TRUE(measured < target * 1.25f);
    }
}

ENJIN_TEST(ReverbDSP, AskingForABrighterOrDullerTailChangesTheTail) {
    // Arrange: the same mid-band decay, once with the highs ringing as long as
    // the lows and once with them dying three times faster. The tails must
    // differ, or the per-band work never reaches anyone.
    FeedbackDelayNetwork even, damped;
    even.Prepare(kRate);
    damped.Prepare(kRate);
    const f32 flat[Audio::kAcousticBands] = { 1.2f, 1.2f, 1.2f };
    const f32 dull[Audio::kAcousticBands] = { 1.2f, 0.9f, 0.4f };
    even.Configure(flat, 8.0f);
    damped.Configure(dull, 8.0f);

    // Act
    const std::vector<f32> a = ImpulseResponse(even, 2.5f);
    const std::vector<f32> b = ImpulseResponse(damped, 2.5f);

    // Assert: the damped one is shorter, because a tail whose top end dies
    // early has less energy left late.
    const f32 rtA = MeasureRT60(a, kRate);
    const f32 rtB = MeasureRT60(b, kRate);
    std::printf("    flat tail %.2f s, damped tail %.2f s\n", rtA, rtB);
    ENJIN_EXPECT_TRUE(rtB < rtA);
}

ENJIN_TEST(ReverbDSP, ATailOfZeroLengthIsSilenceRatherThanNoise) {
    // Arrange: an unmeasurable room -- outdoors, or a trace that found nothing.
    FeedbackDelayNetwork fdn;
    fdn.Prepare(kRate);
    const f32 none[Audio::kAcousticBands] = { 0.0f, 0.0f, 0.0f };

    // Act
    fdn.Configure(none, 8.0f);

    // Assert: refused, and silent. A network configured with a zero decay would
    // have zero feedback gains and produce nothing useful; saying so is better
    // than running one.
    ENJIN_EXPECT_FALSE(fdn.IsConfigured());
    f32 l = 1.0f, r = 1.0f;
    fdn.Process(1.0f, l, r);
    ENJIN_EXPECT_FLOAT_NEAR(l, 0.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(r, 0.0f, 0.0001f);
}

ENJIN_TEST(ReverbDSP, TheTailIsStableAndDoesNotRunAway) {
    // Arrange: a long decay, which is where a feedback network with a gain
    // above one would blow up rather than ring.
    FeedbackDelayNetwork fdn;
    fdn.Prepare(kRate);
    const f32 rt60[Audio::kAcousticBands] = { 4.0f, 4.0f, 4.0f };
    fdn.Configure(rt60, 20.0f);

    // Act: ten seconds of impulse response.
    const std::vector<f32> ir = ImpulseResponse(fdn, 10.0f);

    // Assert: finite everywhere, never louder than the input, and quiet by the
    // end. A network that grew would be a sound that never stops, in a game
    // somebody shipped.
    f32 peak = 0.0f;
    for (f32 s : ir) {
        ENJIN_ASSERT_TRUE(std::isfinite(s));
        peak = std::max(peak, std::fabs(s));
    }
    ENJIN_EXPECT_TRUE(peak <= 1.0f);

    f32 tailPeak = 0.0f;
    for (usize i = ir.size() * 9 / 10; i < ir.size(); ++i) {
        tailPeak = std::max(tailPeak, std::fabs(ir[i]));
    }
    ENJIN_EXPECT_TRUE(tailPeak < peak * 0.01f);
}

ENJIN_TEST(ReverbDSP, TheTwoOutputsAreNotIdentical) {
    // Arrange
    FeedbackDelayNetwork fdn;
    fdn.Prepare(kRate);
    const f32 rt60[Audio::kAcousticBands] = { 1.5f, 1.5f, 1.5f };
    fdn.Configure(rt60, 10.0f);

    // Act
    usize differing = 0;
    for (usize i = 0; i < 20000; ++i) {
        f32 l = 0.0f, r = 0.0f;
        fdn.Process(i == 0 ? 1.0f : 0.0f, l, r);
        if (std::fabs(l - r) > 1.0e-6f) ++differing;
    }

    // Assert: a reverb whose two channels matched would collapse to the centre
    // of the head and stop sounding like a space at all.
    ENJIN_EXPECT_TRUE(differing > 1000);
}

ENJIN_TEST(ReverbDSP, ARoomWithTheSameDecayButADifferentSizeSoundsDifferent) {
    // Arrange: identical decay times, different mean free paths -- a cupboard
    // and a cathedral that happen to ring for the same length of time.
    FeedbackDelayNetwork tight, vast;
    tight.Prepare(kRate);
    vast.Prepare(kRate);
    const f32 rt60[Audio::kAcousticBands] = { 1.2f, 1.2f, 1.2f };
    tight.Configure(rt60, 2.0f);
    vast.Configure(rt60, 30.0f);

    // Act: how many samples in the first 50 ms are meaningfully non-zero, which
    // is a crude but honest measure of echo density.
    auto density = [&](FeedbackDelayNetwork& fdn) {
        usize active = 0;
        for (usize i = 0; i < kRate / 20; ++i) {
            f32 l = 0.0f, r = 0.0f;
            fdn.Process(i == 0 ? 1.0f : 0.0f, l, r);
            if (std::fabs(l) + std::fabs(r) > 1.0e-4f) ++active;
        }
        return active;
    };

    // Assert: the small room fills in far faster. Decay time alone cannot say
    // this, and it is why the mean free path is measured at all.
    const usize tightDensity = density(tight);
    const usize vastDensity = density(vast);
    std::printf("    echo density in 50 ms: small %zu, large %zu\n", tightDensity, vastDensity);
    ENJIN_EXPECT_TRUE(tightDensity > vastDensity * 2);
}

ENJIN_TEST(ReverbDSP, AnOpenFieldGetsAlmostNoTail) {
    // Arrange: a response like the one an outdoor trace produces -- a decay
    // time exists, but almost no energy came back.
    RoomResponse field;
    field.rt60[0] = field.rt60[1] = field.rt60[2] = 0.6f;
    field.meanFreePath = 30.0f;
    field.reflectedEnergy = 0.01f;
    field.raysTraced = 1024;

    RoomResponse room;
    room.rt60[0] = room.rt60[1] = room.rt60[2] = 0.6f;
    room.meanFreePath = 8.0f;
    room.reflectedEnergy = 1.0f;
    room.raysTraced = 1024;

    RoomReverb outdoors, indoors;
    outdoors.Prepare(kRate);
    indoors.Prepare(kRate);
    const EarlyReflectionResult none;
    outdoors.Configure(field, none);
    indoors.Configure(room, none);

    // Act
    auto energy = [&](RoomReverb& verb) {
        f64 total = 0.0;
        for (usize i = 0; i < kRate; ++i) {
            f32 l = 0.0f, r = 0.0f;
            verb.Process(i == 0 ? 1.0f : 0.0f, l, r);
            total += static_cast<f64>(l) * l + static_cast<f64>(r) * r;
        }
        return total;
    };

    // Assert: the field is nearly dry. Two places with the same decay time can
    // still differ enormously in how much room you hear, and the reflected
    // energy is the only thing that carries it.
    const f64 outside = energy(outdoors);
    const f64 inside = energy(indoors);
    ENJIN_EXPECT_TRUE(outside < inside * 0.1);
}

// --------------------------------------------------------------------------
// Geometry to ears, in one test
// --------------------------------------------------------------------------

ENJIN_TEST(ReverbDSP, ARoomMeasuredFromGeometryRendersWithTheDecayItWasMeasuredToHave) {
    // Arrange: build a real room, trace it, hand the result to the reverb.
    // Nothing in this test sets a reverb parameter.
    Audio::AcousticScene room;
    {
        const f32 x = 6.0f, y = 2.5f, z = 4.5f;
        const Vector3 corner[8] = {
            {-x,-y,-z}, { x,-y,-z}, { x, y,-z}, {-x, y,-z},
            {-x,-y, z}, { x,-y, z}, { x, y, z}, {-x, y, z},
        };
        for (const auto& c : corner) room.vertices.push_back(c);
        static const i32 kTris[36] = {
            0,1,2, 0,2,3,  5,4,7, 5,7,6,  4,5,1, 4,1,0,
            3,2,6, 3,6,7,  4,0,3, 4,3,7,  1,5,6, 1,6,2,
        };
        const i32 mat = static_cast<i32>(room.materials.IndexFor(ECS::SurfaceMaterial::Wood));
        for (i32 i = 0; i < 36; ++i) room.indices.push_back(kTris[i]);
        for (i32 t = 0; t < 12; ++t) room.materialIndices.push_back(mat);
    }
    AcousticBVH bvh;
    bvh.Build(room);

    RoomTraceSettings trace;
    trace.rayCount = 2048;
    const RoomResponse measured = TraceRoomResponse(bvh, room, Vector3(0, 0, 0), trace);
    ENJIN_ASSERT_TRUE(measured.Valid());

    const EarlyReflectionResult reflections =
        TraceEarlyReflections(bvh, room, Vector3(-3, 0, 0), Vector3(3, 0, 0));

    // Act
    RoomReverb verb;
    verb.Prepare(kRate);
    verb.Configure(measured, reflections);
    ENJIN_ASSERT_TRUE(verb.IsConfigured());

    std::vector<f32> ir(static_cast<usize>(measured.rt60[1] * 2.5f * kRate) + kRate);
    for (usize i = 0; i < ir.size(); ++i) {
        f32 l = 0.0f, r = 0.0f;
        verb.Process(i == 0 ? 1.0f : 0.0f, l, r);
        ir[i] = (l + r) * 0.5f;
    }
    const f32 rendered = MeasureRT60(ir, kRate);

    // Assert: geometry to materials to a measured RT60 to a rendered tail, and
    // the tail decays for as long as the room was measured to. Every step of
    // that chain had its own test; this is the one that says they connect.
    std::printf("    geometry says %.2f s, rendered tail measures %.2f s\n",
                measured.rt60[1], rendered);
    ENJIN_EXPECT_TRUE(rendered > measured.rt60[1] * 0.7f);
    ENJIN_EXPECT_TRUE(rendered < measured.rt60[1] * 1.3f);
}

ENJIN_TEST_MAIN()
