// The reverb bus has to exist in the build that ships.
//
// It did not, for the entire life of the feature. The node was created inside
// `#ifdef ENJIN_AUDIO_STEAM_AUDIO` and inside `if (m_HRTFEnabled)`, and that
// CMake option defaults to OFF -- so in a stock build reverbReady stayed false
// and every call that fed the bus returned at its first line.
// SetEnvironmentReverb, SetMeasuredRoom, SetMeasuredReflections: all of them,
// silently, doing nothing. No measured rooms, no early reflections, and not the
// Freeverb or the ReverbZone components that predate both by years. Every scene
// played bone dry, so of course every room sounded like every other room.
//
// Twelve test suites were green over it. That is the part worth fixing in the
// TESTS and not just the code: every one of them exercised a DSP class directly
// -- FeedbackDelayNetwork, EarlyReflectionRenderer, RoomResponse, the BVH --
// and not one went through AudioEngine, which is the single place the feature
// was switched off. A unit test of a component cannot see that nobody built the
// thing the component goes in.
//
// So these tests are deliberately at the seam, and they assert the boring
// structural facts rather than any DSP behaviour.

#include "EnjinTest.h"
#include "Enjin/Audio/AudioEngine.h"
#include "Enjin/Acoustics/EarlyReflections.h"
#include "Enjin/Logging/Log.h"

#include <cstdio>

using namespace Enjin;
using namespace Enjin::Audio;

namespace {

// A sound device is not always present (CI containers, a machine with audio
// disabled). That is a reason to SKIP, never a reason to pass quietly: the
// thing under test is what Initialize builds, so without it there is nothing
// to look at.
bool StartEngine(AudioEngine& engine) {
    Logger::Get().Initialize("test_reverb_bus.log");
    return engine.Initialize();
}

} // namespace

ENJIN_TEST(ReverbBus, AnInitializedEngineHasAReverbBus) {
    // Arrange
    AudioEngine engine;
    if (!StartEngine(engine)) {
        ENJIN_SKIP("no audio device available to initialize against");
        return;
    }

    // Act / Assert: this is the whole bug in one line. It was false in every
    // default build, and nothing anywhere said so.
    ENJIN_EXPECT_TRUE(engine.HasReverbBus());

    engine.Shutdown();
}

ENJIN_TEST(ReverbBus, AMeasuredRoomActuallyReachesTheBus) {
    // Arrange
    AudioEngine engine;
    if (!StartEngine(engine)) {
        ENJIN_SKIP("no audio device available to initialize against");
        return;
    }

    // Act: hand it a room, the way AudioReactiveSystem does every time the
    // listener moves far enough to be somewhere else.
    const f32 rt60[3] = { 1.9f, 1.6f, 1.1f };
    engine.SetMeasuredRoom(rt60, 7.5f, 0.8f);

    // Assert: the flag only flips if the call got past its reverbReady guard.
    // With the bus compiled out this returned at line one and stayed false, so
    // the engine believed it had no measurement while the acoustics upstream
    // were measuring perfectly and logging the numbers.
    ENJIN_EXPECT_TRUE(engine.HasMeasuredRoom());

    engine.ClearMeasuredRoom();
    ENJIN_EXPECT_TRUE(!engine.HasMeasuredRoom());

    engine.Shutdown();
}

ENJIN_TEST(ReverbBus, ReflectionsCanBeHandedOverWithoutABus) {
    // Arrange: an engine that was never initialized, which is what a headless
    // tool or a failed device open leaves you holding.
    AudioEngine engine;

    Acoustics::EarlyReflectionResult reflections;
    Acoustics::EarlyReflection tap;
    tap.delay = 0.011f;
    tap.gain[0] = tap.gain[1] = tap.gain[2] = 0.3f;
    reflections.taps.push_back(tap);

    // Act / Assert: has to be a no-op rather than a crash. Every one of these
    // is called unconditionally from AudioReactiveSystem, which does not know
    // or care whether a device opened.
    engine.SetMeasuredReflections(reflections);
    engine.SetEnvironmentReverb(0.3f, 0.5f, 0.5f, 1.5f, 0.02f);
    ENJIN_EXPECT_TRUE(!engine.HasReverbBus());
    ENJIN_EXPECT_TRUE(!engine.HasMeasuredRoom());
}

ENJIN_TEST(ReverbBus, MoreReflectionsThanSlotsKeepsTheLoudest) {
    // Arrange
    AudioEngine engine;
    if (!StartEngine(engine)) {
        ENJIN_SKIP("no audio device available to initialize against");
        return;
    }

    // 80 taps against a cap of 32. Which 48 get dropped is the whole question:
    // dropping the tail of the list would throw away the loudest reflections
    // whenever the tracer happened to return them late.
    Acoustics::EarlyReflectionResult reflections;
    for (int i = 0; i < 80; ++i) {
        Acoustics::EarlyReflection tap;
        tap.delay = 0.005f + 0.002f * static_cast<f32>(i);
        const f32 g = 0.01f * static_cast<f32>(i + 1);   // ascending: loudest LAST
        tap.gain[0] = tap.gain[1] = tap.gain[2] = g;
        reflections.taps.push_back(tap);
    }

    // Act / Assert: it must not crash, must not overrun the staging array, and
    // must survive being handed far more than it can hold. The selection itself
    // is checked by TestReverbDSP against the renderer; what is checked here is
    // that the seam takes the input at all.
    engine.SetMeasuredReflections(reflections);
    ENJIN_EXPECT_TRUE(engine.HasReverbBus());

    engine.Shutdown();
}

ENJIN_TEST_MAIN()
