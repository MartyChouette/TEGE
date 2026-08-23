// Replay round-trip + input-injection tests: the machinery behind shareable
// .tegereplay files (scene snapshot + per-frame input/dt stream).

#include "EnjinTest.h"
#include "Enjin/Gameplay/Replay.h"
#include "Enjin/Platform/Input.h"

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
