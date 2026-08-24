// Replay round-trip + input-injection tests: the machinery behind shareable
// .tegereplay files (scene snapshot + per-frame input/dt stream).

#include "EnjinTest.h"
#include "Enjin/Gameplay/Replay.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Math.h"

using namespace Enjin;

ENJIN_TEST(Replay, SerializeParseRoundTripsFramesAndDt) {
    // Arrange: two frames - W+D held with a mouse press, then empty.
    Gameplay::ReplayData r;
    r.engineVersion = "0.9.7";
    r.sceneJson = "{\"entities\":[]}";
    Gameplay::ReplayFrame f1;
    f1.keysDown = {87, 68};
    f1.mouseMask = 0x1;
    f1.mouseX = 100.5f; f1.mouseY = 200.25f;
    f1.dt = 0.0166f;
    r.frames.push_back(f1);
    r.frames.push_back(Gameplay::ReplayFrame{});

    // Act
    std::string text = Gameplay::SerializeReplay(r);
    Gameplay::ReplayData back;
    bool ok = Gameplay::ParseReplay(text, back);

    // Assert
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_ASSERT_EQ((int)back.frames.size(), 2);
    ENJIN_ASSERT_EQ((int)back.frames[0].keysDown.size(), 2);
    ENJIN_EXPECT_EQ((int)back.frames[0].keysDown[0], 87);
    ENJIN_EXPECT_EQ((int)back.frames[0].mouseMask, 1);
    ENJIN_EXPECT_TRUE(back.frames[0].dt > 0.016f && back.frames[0].dt < 0.017f);
    ENJIN_EXPECT_TRUE(back.frames[0].mouseX > 100.4f && back.frames[0].mouseX < 100.6f);
    ENJIN_EXPECT_TRUE(back.sceneJson == r.sceneJson);
    ENJIN_EXPECT_TRUE(back.frames[1].keysDown.empty());
}

ENJIN_TEST(Replay, ParseRejectsGarbage) {
    Gameplay::ReplayData out;
    ENJIN_EXPECT_FALSE(Gameplay::ParseReplay("not json", out));
    ENJIN_EXPECT_FALSE(Gameplay::ParseReplay("{\"foo\":1}", out));
}

ENJIN_TEST(Replay, RngSeedRoundTripsAndDefaultsToZero) {
    // Arrange: a recorded session carries the script-RNG seed.
    Gameplay::ReplayData r;
    r.rngSeed = 0xDEADBEEF;
    r.frames.push_back(Gameplay::ReplayFrame{});

    // Act
    Gameplay::ReplayData back;
    bool ok = Gameplay::ParseReplay(Gameplay::SerializeReplay(r), back);

    // Assert: seed survives, and a legacy replay without the field parses as 0.
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_EQ(back.rngSeed, 0xDEADBEEFu);
    Gameplay::ReplayData legacy;
    ENJIN_ASSERT_TRUE(Gameplay::ParseReplay("{\"tege_replay\":1,\"frames\":[]}", legacy));
    ENJIN_EXPECT_EQ(legacy.rngSeed, 0u);
}

ENJIN_TEST(Replay, EndStateRoundTrips) {
    // Arrange: a recording that ended with the player at a known spot.
    Gameplay::ReplayData r;
    r.frames.push_back(Gameplay::ReplayFrame{});
    Gameplay::ReplayEndEntity e;
    e.name = "Capsule";
    e.position = Math::Vector3(3.0f, 1.5f, -2.0f);
    e.rotY = 0.7071f; e.rotW = 0.7071f;
    r.endState.push_back(e);

    // Act
    Gameplay::ReplayData back;
    bool ok = Gameplay::ParseReplay(Gameplay::SerializeReplay(r), back);

    // Assert: the end point survives; legacy replays parse with none.
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_ASSERT_EQ((int)back.endState.size(), 1);
    ENJIN_EXPECT_TRUE(back.endState[0].name == "Capsule");
    ENJIN_EXPECT_FLOAT_NEAR(back.endState[0].position.y, 1.5f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(back.endState[0].rotY, 0.7071f, 0.001f);
    Gameplay::ReplayData legacy;
    ENJIN_ASSERT_TRUE(Gameplay::ParseReplay("{\"tege_replay\":1,\"frames\":[]}", legacy));
    ENJIN_EXPECT_TRUE(legacy.endState.empty());
}

ENJIN_TEST(Replay, SetRandomSeedReproducesTheScriptRandomStream) {
    // Arrange/Act: seed, drain five values, reseed with the same seed.
    Math::SetRandomSeed(1234u);
    f32 first[5];
    for (f32& v : first) v = Math::Random01();
    Math::SetRandomSeed(1234u);

    // Assert: identical sequence - the property replay playback relies on.
    for (f32 v : first) {
        ENJIN_EXPECT_FLOAT_NEAR(Math::Random01(), v, 0.0f);
    }
    Math::SetRandomSeed(0u);   // restore default stream state for other tests
}

ENJIN_TEST(Replay, InjectionDrivesEdgeDetection) {
    // Arrange: headless input (no window), injection on.
    Input::SetReplayInjection(true);
    bool keys[512] = {}; bool mouse[8] = {};
    keys[87] = true;   // W held

    // Act: frame 1 injects W down; frame 2 releases it.
    Input::InjectFrameState(keys, mouse, Math::Vector2(10.0f, 20.0f));
    Input::Update();
    bool downOnFrame1 = Input::IsKeyDown(static_cast<KeyCode>(87));
    bool pressedOnFrame1 = Input::IsKeyPressed(static_cast<KeyCode>(87));

    keys[87] = false;
    Input::InjectFrameState(keys, mouse, Math::Vector2(10.0f, 20.0f));
    Input::Update();
    bool downOnFrame2 = Input::IsKeyDown(static_cast<KeyCode>(87));
    bool releasedOnFrame2 = Input::IsKeyReleased(static_cast<KeyCode>(87));

    Input::SetReplayInjection(false);

    // Assert: held + pressed-edge on frame 1, released-edge on frame 2.
    ENJIN_EXPECT_TRUE(downOnFrame1);
    ENJIN_EXPECT_TRUE(pressedOnFrame1);
    ENJIN_EXPECT_FALSE(downOnFrame2);
    ENJIN_EXPECT_TRUE(releasedOnFrame2);
}

ENJIN_TEST_MAIN()
