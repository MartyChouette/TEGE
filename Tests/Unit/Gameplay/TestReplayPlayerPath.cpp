// Replay was editor-only, which is the one place a repro is least useful.
//
// The .tegereplay format, its serializer, and Input::InjectFrameState all
// existed. Both player runtimes simply had no way in: no --replay flag, no
// loader, no playback loop. A tester running a shipped build could record
// nothing and send nothing, so "it happened on my machine" had no artefact
// behind it and the feature's whole purpose went unserved.
//
// The player path itself is a main() and not unit-testable, so these cover the
// two things it depends on being true: that a replay round-trips through the
// file format, and that the fields the player reads are the ones the editor
// writes.
#include "EnjinTest.h"
#include "Enjin/Gameplay/Replay.h"
#include "Enjin/Editor/FeedbackSystem.h"

using namespace Enjin;
using namespace Enjin::Gameplay;

namespace {

ReplayData MakeSession() {
    ReplayData r;
    r.engineVersion = "test";
    r.rngSeed = 12345;
    r.simFixedTimestep = true;
    r.simTicksPerSecond = 120;
    r.sceneJson = "{\"version\":\"1.0\",\"entities\":[]}";

    for (int i = 0; i < 30; ++i) {
        ReplayFrame f;
        f.dt = 1.0f / 120.0f;
        f.mouseX = static_cast<f32>(i);
        f.mouseY = static_cast<f32>(i) * 2.0f;
        if (i % 3 == 0) f.keysDown.push_back(87);   // W
        if (i % 5 == 0) f.mouseMask = 0x1;
        r.frames.push_back(f);
    }

    ReplayEndEntity end;
    end.name = "Player";
    end.position = Math::Vector3(3.0f, 0.0f, -7.0f);
    r.endState.push_back(end);

    ReplayBookmark bm;
    bm.frame = 12;
    bm.time = 0.1f;
    bm.label = "script exception";
    r.bookmarks.push_back(bm);
    return r;
}

} // namespace

ENJIN_TEST(ReplayPlayerPath, AReplayRoundTripsThroughItsFileFormat) {
    // Arrange
    const ReplayData original = MakeSession();

    // Act
    const std::string text = SerializeReplay(original);
    ReplayData parsed;
    ENJIN_ASSERT_TRUE(ParseReplay(text, parsed));

    // Assert: everything the player reads survives the trip.
    ENJIN_EXPECT_EQ(parsed.frames.size(), original.frames.size());
    ENJIN_EXPECT_EQ(parsed.endState.size(), original.endState.size());
    ENJIN_EXPECT_EQ(parsed.bookmarks.size(), original.bookmarks.size());
    ENJIN_EXPECT_TRUE(parsed.sceneJson == original.sceneJson);
}

ENJIN_TEST(ReplayPlayerPath, TheSimulationClockConfigSurvives) {
    // The player FORCES these from the file rather than taking the project's,
    // because a clock that stepped differently during recording lands the same
    // inputs on different simulation frames. That is divergence with no symptom
    // until something falls through a floor.
    const ReplayData original = MakeSession();
    ReplayData parsed;
    ENJIN_ASSERT_TRUE(ParseReplay(SerializeReplay(original), parsed));

    ENJIN_EXPECT_TRUE(parsed.simFixedTimestep == original.simFixedTimestep);
    ENJIN_EXPECT_EQ(parsed.simTicksPerSecond, original.simTicksPerSecond);
    ENJIN_EXPECT_EQ(parsed.rngSeed, original.rngSeed);
}

ENJIN_TEST(ReplayPlayerPath, TheScenesnapshotIsWhatMakesAReplayAReproduction) {
    // The player loads this before stepping anything. Replaying an input stream
    // against whichever scene the project starts with diverges on frame one, and
    // then every later frame is noise -- a repro that does not reproduce is worse
    // than none, because it looks like evidence.
    const ReplayData original = MakeSession();
    ReplayData parsed;
    ENJIN_ASSERT_TRUE(ParseReplay(SerializeReplay(original), parsed));
    ENJIN_EXPECT_FALSE(parsed.sceneJson.empty());
}

ENJIN_TEST(ReplayPlayerPath, TheDtStreamIsReplayedExactly) {
    // A replay carries its own dt, recorded with the time scale already folded
    // in. Scaling it again would replay a bullet-time session in bullet time
    // squared; re-deriving it from a frame counter would drop the variable-step
    // sessions the format exists to reproduce.
    ReplayData original = MakeSession();
    original.frames[7].dt = 0.25f;      // a hitch, of the kind that matters
    original.frames[8].dt = 0.0011f;

    ReplayData parsed;
    ENJIN_ASSERT_TRUE(ParseReplay(SerializeReplay(original), parsed));
    ENJIN_ASSERT_TRUE(parsed.frames.size() > 8);
    ENJIN_EXPECT_FLOAT_NEAR(parsed.frames[7].dt, 0.25f, 0.00001f);
    ENJIN_EXPECT_FLOAT_NEAR(parsed.frames[8].dt, 0.0011f, 0.00001f);
}

ENJIN_TEST(ReplayPlayerPath, InputFramesUnpackIntoTheBuffersTheRuntimesInject) {
    // ReplayFrameToBuffers is the one call both the editor and the player make
    // to turn a stored frame into something Input::InjectFrameState takes.
    ReplayData original = MakeSession();
    ReplayData parsed;
    ENJIN_ASSERT_TRUE(ParseReplay(SerializeReplay(original), parsed));
    ENJIN_ASSERT_TRUE(parsed.frames.size() >= 6);

    bool keys[512] = {};
    bool mouse[8] = {};
    Math::Vector2 mpos;

    // Frame 0: W down, mouse button 0 down.
    ReplayFrameToBuffers(parsed.frames[0], keys, mouse, mpos);
    ENJIN_EXPECT_TRUE(keys[87]);
    ENJIN_EXPECT_TRUE(mouse[0]);

    // Frame 1: neither. The unpack must CLEAR, not accumulate -- a held key that
    // never releases is the classic replay divergence.
    ReplayFrameToBuffers(parsed.frames[1], keys, mouse, mpos);
    ENJIN_EXPECT_FALSE(keys[87]);
    ENJIN_EXPECT_FALSE(mouse[0]);

    // And the mouse position tracks the frame it came from.
    ReplayFrameToBuffers(parsed.frames[4], keys, mouse, mpos);
    ENJIN_EXPECT_FLOAT_NEAR(mpos.x, 4.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mpos.y, 8.0f, 0.001f);
}

ENJIN_TEST(ReplayPlayerPath, GarbageIsRejectedRatherThanPartlyLoaded) {
    // The player refuses to replay a file it could not read, instead of running
    // an empty stream and reporting a successful session.
    ReplayData out;
    ENJIN_EXPECT_FALSE(ParseReplay("", out));
    ENJIN_EXPECT_FALSE(ParseReplay("not json at all", out));
    ENJIN_EXPECT_FALSE(ParseReplay("{\"unrelated\":true}", out) && !out.frames.empty());
}

// ---------------------------------------------------------------------------
// Bug reports carry a reproduction now
// ---------------------------------------------------------------------------

ENJIN_TEST(ReplayPlayerPath, ADiagnosticSnapshotStartsWithNoReproAtAll) {
    // A bug report used to be a machine spec, a frame rate and fifty lines of
    // console: everything about the CONDITIONS and nothing about what happened.
    // Meanwhile the editor was recording the whole session two different ways and
    // neither reached the report, so "steps to reproduce" was a free-text box the
    // reporter filled in from memory.
    //
    // The defaults are an ABSENCE, not a plausible value: an empty path and zero
    // frames, so a report filed before anything was played says so instead of
    // pointing at a replay nobody wrote.
    Editor::DiagnosticSnapshot d;
    ENJIN_EXPECT_TRUE(d.replayPath.empty());
    ENJIN_EXPECT_EQ(d.replayFrames, 0u);
    ENJIN_EXPECT_EQ(d.debugRecorderFrames, 0u);
    ENJIN_EXPECT_FLOAT_NEAR(d.debugRecorderSeconds, 0.0f, 0.0001f);
    ENJIN_EXPECT_TRUE(d.sessionMarks.empty());
}

ENJIN_TEST_MAIN()
