// Double-clicking a console error opens the script at the line the error names.
// That gesture is only as good as the parse behind it, and the parse has two
// message shapes to get right plus one trap: the same console line carries the
// C++ site of the log call, so an anchor on the first thing shaped like
// file:line sends every jump into the engine's own source.

#include "EnjinTest.h"
#include "Enjin/Editor/ScriptErrorLocation.h"

using Enjin::Editor::ParseScriptErrorLocation;

namespace {

struct Loc {
    bool ok = false;
    std::string path;
    int line = 0;
};

Loc Parse(const std::string& msg) {
    Loc l;
    l.ok = ParseScriptErrorLocation(msg, l.path, l.line);
    return l;
}

} // namespace

ENJIN_TEST(ScriptErrorLocation, CompilerMessage) {
    // What MessageCallback formats: "<section> (row, col): message"
    Loc l = Parse("scripts/Player.as (31, 9): Expected ';'");
    ENJIN_ASSERT_TRUE(l.ok);
    ENJIN_EXPECT_EQ(l.path, std::string("scripts/Player.as"));
    ENJIN_EXPECT_EQ(l.line, 31);
}

ENJIN_TEST(ScriptErrorLocation, RuntimeException) {
    // What DescribeException builds: "<what> in <decl> at <section>:<line>:<col>".
    // The declaration's argument list puts a '(' in front of the location, which
    // is what the inspector's old find('(')+atoi read instead of a line number.
    Loc l = Parse("Null pointer access in void Player::OnUpdate(float) at scripts/Player.as:31:9");
    ENJIN_ASSERT_TRUE(l.ok);
    ENJIN_EXPECT_EQ(l.path, std::string("scripts/Player.as"));
    ENJIN_EXPECT_EQ(l.line, 31);
}

ENJIN_TEST(ScriptErrorLocation, IgnoresTheCppSiteOfTheLogCall) {
    // The console shows the whole formatted log line. The engine's own source
    // file sits in it, before the script location and shaped exactly like one.
    Loc l = Parse("[2026-09-10 04:45:12] [ERROR] [SCRIPT] ScriptSystem.cpp:93 "
                  "(Enjin::Scripting::ScriptSystem::HandleScriptError) "
                  "Null pointer access in void Player::OnUpdate(float) at scripts/Player.as:31:9");
    ENJIN_ASSERT_TRUE(l.ok);
    ENJIN_EXPECT_EQ(l.path, std::string("scripts/Player.as"));
    ENJIN_EXPECT_EQ(l.line, 31);
}

ENJIN_TEST(ScriptErrorLocation, WindowsAbsolutePathKeepsItsDrive) {
    Loc l = Parse("D:\\GitHub\\game\\scripts\\Player.as (7, 1): Expected identifier");
    ENJIN_ASSERT_TRUE(l.ok);
    ENJIN_EXPECT_EQ(l.path, std::string("D:\\GitHub\\game\\scripts\\Player.as"));
    ENJIN_EXPECT_EQ(l.line, 7);
}

ENJIN_TEST(ScriptErrorLocation, EmbeddedApiSectionIsABareName) {
    // Includes are added as sections under their own name, with no directory.
    Loc l = Parse("TegeBehavior.as (12, 3): Expected expression value");
    ENJIN_ASSERT_TRUE(l.ok);
    ENJIN_EXPECT_EQ(l.path, std::string("TegeBehavior.as"));
    ENJIN_EXPECT_EQ(l.line, 12);
}

ENJIN_TEST(ScriptErrorLocation, AngelscriptExtension) {
    Loc l = Parse("scripts/Enemy.angelscript (4, 2): Unexpected token");
    ENJIN_ASSERT_TRUE(l.ok);
    ENJIN_EXPECT_EQ(l.path, std::string("scripts/Enemy.angelscript"));
    ENJIN_EXPECT_EQ(l.line, 4);
}

ENJIN_TEST(ScriptErrorLocation, LastLocationWins) {
    // A message can quote a path and then report one. The reported one is last.
    Loc l = Parse("While compiling scripts/Base.as: scripts/Derived.as (18, 4): "
                  "No matching signatures");
    ENJIN_ASSERT_TRUE(l.ok);
    ENJIN_EXPECT_EQ(l.path, std::string("scripts/Derived.as"));
    ENJIN_EXPECT_EQ(l.line, 18);
}

ENJIN_TEST(ScriptErrorLocation, RejectsMessagesWithNoScriptInThem) {
    // Most console lines. Reporting a location for these would put a
    // double-click affordance on every row and open the wrong thing.
    ENJIN_EXPECT_FALSE(Parse("Loaded texture: assets/hero.png (16x16)").ok);
    ENJIN_EXPECT_FALSE(Parse("RenderSystem.cpp:4294 sprite batch flushed").ok);
    ENJIN_EXPECT_FALSE(Parse("Entered Play Mode").ok);
}

ENJIN_TEST(ScriptErrorLocation, ExtensionMustEndTheName) {
    // "sprites.assets" contains ".as". An asset path is not a script location.
    ENJIN_EXPECT_FALSE(Parse("Packed sprites.assets (12 entries)").ok);
}

ENJIN_TEST(ScriptErrorLocation, NoLineNumberIsNotALocation) {
    // A script named with nowhere to jump to: the window would open at the top
    // of the file and claim to have found something.
    ENJIN_EXPECT_FALSE(Parse("Attached scripts/Player.as to entity").ok);
}

ENJIN_TEST(ScriptErrorLocation, LeavesOutputsAloneOnFailure) {
    std::string path = "untouched";
    int line = -7;
    ENJIN_EXPECT_FALSE(ParseScriptErrorLocation("Entered Play Mode", path, line));
    ENJIN_EXPECT_EQ(path, std::string("untouched"));
    ENJIN_EXPECT_EQ(line, -7);
}

ENJIN_TEST_MAIN()
