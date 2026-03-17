#include "EnjinTest.h"
#include "Enjin/GUI/ScreenTransition.h"

using namespace Enjin;
using namespace Enjin::GUI;

// ===========================================================================
// TransitionConfig Defaults
// ===========================================================================

ENJIN_TEST(Config, Defaults) {
    TransitionConfig cfg;
    ENJIN_EXPECT_EQ((int)cfg.type, (int)TransitionType::FadeBlack);
    ENJIN_EXPECT_FLOAT_NEAR(cfg.outDuration, 0.5f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(cfg.holdDuration, 0.1f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(cfg.inDuration, 0.5f, 0.01f);
    ENJIN_EXPECT_EQ(cfg.pixelSize, 8u);
}

// ===========================================================================
// TransitionType Enum
// ===========================================================================

ENJIN_TEST(TransitionType, AllNineTypes) {
    ENJIN_EXPECT_EQ((int)TransitionType::FadeBlack, 0);
    ENJIN_EXPECT_EQ((int)TransitionType::FadeWhite, 1);
    ENJIN_EXPECT_EQ((int)TransitionType::CircleIris, 2);
    ENJIN_EXPECT_EQ((int)TransitionType::DiamondWipe, 3);
    ENJIN_EXPECT_EQ((int)TransitionType::PixelDissolve, 4);
    ENJIN_EXPECT_EQ((int)TransitionType::SlideLeft, 5);
    ENJIN_EXPECT_EQ((int)TransitionType::SlideRight, 6);
    ENJIN_EXPECT_EQ((int)TransitionType::SlideUp, 7);
    ENJIN_EXPECT_EQ((int)TransitionType::SlideDown, 8);
}

// ===========================================================================
// Initial State
// ===========================================================================

ENJIN_TEST(State, InitiallyIdle) {
    ScreenTransition transition;
    ENJIN_EXPECT_FALSE(transition.IsActive());
    ENJIN_EXPECT_FALSE(transition.IsAtMidpoint());
    ENJIN_EXPECT_FALSE(transition.IsComplete());
    ENJIN_EXPECT_EQ((int)transition.GetPhase(), (int)TransitionPhase::Idle);
}

// ===========================================================================
// Start & Phase Progression
// ===========================================================================

ENJIN_TEST(Phases, StartBecomesActive) {
    ScreenTransition transition;
    TransitionConfig cfg;
    cfg.outDuration = 1.0f;
    cfg.holdDuration = 0.5f;
    cfg.inDuration = 1.0f;
    transition.Start(cfg);
    ENJIN_EXPECT_TRUE(transition.IsActive());
    ENJIN_EXPECT_EQ((int)transition.GetPhase(), (int)TransitionPhase::TransitionOut);
}

ENJIN_TEST(Phases, OutToHold) {
    ScreenTransition transition;
    TransitionConfig cfg;
    cfg.outDuration = 0.5f;
    cfg.holdDuration = 0.5f;
    cfg.inDuration = 0.5f;
    transition.Start(cfg);

    // Advance past out duration
    transition.Update(0.6f);
    ENJIN_EXPECT_EQ((int)transition.GetPhase(), (int)TransitionPhase::Hold);
}

ENJIN_TEST(Phases, HoldToIn) {
    ScreenTransition transition;
    TransitionConfig cfg;
    cfg.outDuration = 0.5f;
    cfg.holdDuration = 0.5f;
    cfg.inDuration = 0.5f;
    transition.Start(cfg);

    // Past out + hold
    transition.Update(0.6f); // out → hold
    transition.Update(0.6f); // hold → in
    ENJIN_EXPECT_EQ((int)transition.GetPhase(), (int)TransitionPhase::TransitionIn);
}

ENJIN_TEST(Phases, CompletesAfterAllPhases) {
    ScreenTransition transition;
    TransitionConfig cfg;
    cfg.outDuration = 0.2f;
    cfg.holdDuration = 0.1f;
    cfg.inDuration = 0.2f;
    transition.Start(cfg);

    // Advance past total duration
    transition.Update(0.25f); // out → hold
    transition.Update(0.15f); // hold → in
    transition.Update(0.25f); // in → complete

    ENJIN_EXPECT_TRUE(transition.IsComplete());
    ENJIN_EXPECT_FALSE(transition.IsActive());
    ENJIN_EXPECT_EQ((int)transition.GetPhase(), (int)TransitionPhase::Idle);
}

// ===========================================================================
// Midpoint Detection
// ===========================================================================

ENJIN_TEST(Midpoint, DetectedDuringHold) {
    ScreenTransition transition;
    TransitionConfig cfg;
    cfg.outDuration = 0.5f;
    cfg.holdDuration = 0.5f;
    cfg.inDuration = 0.5f;
    transition.Start(cfg);

    transition.Update(0.6f); // into Hold phase
    ENJIN_EXPECT_TRUE(transition.IsAtMidpoint());
}

ENJIN_TEST(Midpoint, NotDuringOut) {
    ScreenTransition transition;
    TransitionConfig cfg;
    cfg.outDuration = 1.0f;
    cfg.holdDuration = 0.5f;
    cfg.inDuration = 1.0f;
    transition.Start(cfg);

    transition.Update(0.3f); // still in Out
    ENJIN_EXPECT_FALSE(transition.IsAtMidpoint());
}

// ===========================================================================
// All 9 Transition Types
// ===========================================================================

ENJIN_TEST(Types, AllTypesComplete) {
    TransitionType types[] = {
        TransitionType::FadeBlack, TransitionType::FadeWhite,
        TransitionType::CircleIris, TransitionType::DiamondWipe,
        TransitionType::PixelDissolve, TransitionType::SlideLeft,
        TransitionType::SlideRight, TransitionType::SlideUp,
        TransitionType::SlideDown
    };
    for (auto type : types) {
        ScreenTransition transition;
        TransitionConfig cfg;
        cfg.type = type;
        cfg.outDuration = 0.1f;
        cfg.holdDuration = 0.05f;
        cfg.inDuration = 0.1f;
        transition.Start(cfg);
        transition.Update(0.12f);
        transition.Update(0.08f);
        transition.Update(0.12f);
        ENJIN_EXPECT_TRUE(transition.IsComplete());
    }
}

ENJIN_TEST_MAIN()
