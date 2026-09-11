// Three lists of keyboard shortcuts, and they disagreed.
//
//   * The Keyboard Shortcuts window listed "Ctrl+P  Play / Stop" and, four rows
//     above it, "Ctrl+P  Command Palette". Ctrl+P is the palette. It also listed
//     "Ctrl+Shift+P  Pause / Resume" and "F10  Toggle input action map" --
//     neither has a handler anywhere in the editor.
//   * The menu bar labelled seven accelerators nobody implemented: Ctrl+N,
//     Ctrl+O, Ctrl+Shift+S, Ctrl+I, Ctrl+X, Ctrl+C, Ctrl+V. Every one of those
//     menu items works when clicked; pressing the key did nothing.
//   * The menu bar gave Ctrl+B to BOTH "Build Game..." and "Creative Mode". The
//     handler toggles creative mode, so the one key it named next to Build Game
//     opened something else.
//
// Five real shortcuts were in none of the lists: F5, F11, Ctrl+B, Ctrl+Shift+B
// and Ctrl+1..5.
//
// A label naming a key that does nothing is worse than no label: it is the
// software describing itself, wrongly, in the place you go when you do not know.
// There is one table now, and the window, the menu and the handler all read it.
#include "EnjinTest.h"
#include "Enjin/Editor/EditorShortcuts.h"
#include <cstring>
#include <string>
#include <set>
#include <map>

using namespace Enjin;
using namespace Enjin::Editor;

ENJIN_TEST(EditorShortcuts, EveryChordCanActuallyBePressed) {
    // The regression that matters. A chord the table displays but the parser
    // cannot evaluate is a shortcut the editor advertises and cannot honour --
    // which is the entire bug, in a form a test can catch.
    for (usize i = 0; i < ShortcutCount(); ++i) {
        const auto& e = ShortcutTable()[i];
        ENJIN_ASSERT_TRUE(e.chord != nullptr);
        if (!ShortcutChordParses(e.chord)) {
            ENJIN_EXPECT_TRUE(false);
            std::printf("    unparseable chord: '%s' for '%s'\n", e.chord, e.description);
        }
    }
}

ENJIN_TEST(EditorShortcuts, NoTwoActionsClaimTheSameChord) {
    // Ctrl+P was listed twice with two different meanings, and Ctrl+B was
    // printed beside two different menu items. Only one of each pair could ever
    // have worked, and the table could not tell you which.
    std::map<std::string, std::string> seen;
    usize withChords = 0;
    for (usize i = 0; i < ShortcutCount(); ++i) {
        const auto& e = ShortcutTable()[i];
        if (e.chord[0] == '\0') continue;   // "menu only" is not a claim on a key
        ++withChords;

        const std::string chord = e.chord;
        auto it = seen.find(chord);
        if (it != seen.end()) {
            std::printf("    '%s' claimed by both '%s' and '%s'\n",
                        chord.c_str(), it->second.c_str(), e.description);
        } else {
            seen[chord] = e.description;
        }
    }
    // Asserted on the COUNT, so an assertion runs whether or not a duplicate
    // exists. Asserting only inside the failure branch means a clean table
    // reaches no assertion at all -- and this framework counts assertions
    // REACHED, so "no duplicates" and "the test did nothing" looked identical.
    ENJIN_EXPECT_EQ(seen.size(), withChords);
}

ENJIN_TEST(EditorShortcuts, EveryActionHasExactlyOneRow) {
    // ShortcutFor indexes the table by action, so a missing or duplicated row
    // would silently hand back the wrong shortcut rather than fail.
    std::set<int> seen;
    for (usize i = 0; i < ShortcutCount(); ++i) {
        const int a = static_cast<int>(ShortcutTable()[i].action);
        ENJIN_EXPECT_TRUE(seen.insert(a).second);
    }
    ENJIN_EXPECT_EQ(seen.size(), static_cast<usize>(ShortcutAction::Count));
}

ENJIN_TEST(EditorShortcuts, EveryRowIsDescribedAndCategorised) {
    // The window groups by category and prints the description. An empty one
    // renders as a blank row, which reads as a shortcut whose purpose the
    // software will not tell you.
    for (usize i = 0; i < ShortcutCount(); ++i) {
        const auto& e = ShortcutTable()[i];
        ENJIN_ASSERT_TRUE(e.category != nullptr && e.category[0] != '\0');
        ENJIN_ASSERT_TRUE(e.description != nullptr && e.description[0] != '\0');
    }
}

ENJIN_TEST(EditorShortcuts, TheChordsThatWereWrongAreRight) {
    // Named individually, because these are the specific false claims. If any
    // comes back, it comes back with a test attached.
    ENJIN_EXPECT_STR_EQ(ShortcutChord(ShortcutAction::CommandPalette), "Ctrl+P");
    ENJIN_EXPECT_STR_EQ(ShortcutChord(ShortcutAction::CreativeMode), "Ctrl+B");
    // Build Game has no key. Printing Ctrl+B beside it pointed at creative mode.
    ENJIN_EXPECT_STR_EQ(ShortcutChord(ShortcutAction::BuildGame), "");
}

ENJIN_TEST(EditorShortcuts, TheShortcutsThatWereMissingArePresent) {
    // These all worked and appeared in no list, so the only way to find them was
    // to read the source.
    const ShortcutAction wereMissing[] = {
        ShortcutAction::QuickBugReport,  // F5
        ShortcutAction::FocusMode,       // F11
        ShortcutAction::CreativeMode,    // Ctrl+B
        ShortcutAction::ReportBug,       // Ctrl+Shift+B
        ShortcutAction::FocusHierarchy,  // Ctrl+1
    };
    for (ShortcutAction a : wereMissing) {
        ENJIN_EXPECT_TRUE(ShortcutChord(a)[0] != '\0');
    }
}

ENJIN_TEST(EditorShortcuts, AParenthesisedNoteIsNotPartOfTheChord) {
    // "` (Backtick)" shows the character and names it, because a backtick is
    // hard to read in a list. The parser has to stop at the space.
    ENJIN_EXPECT_TRUE(ShortcutChordParses("` (Backtick)"));
    ENJIN_EXPECT_STR_EQ(ShortcutChord(ShortcutAction::Console), "` (Backtick)");
}

ENJIN_TEST(EditorShortcuts, GarbageDoesNotParse) {
    // The parser has to be able to say no, or the "every chord parses" test
    // above passes for any string at all.
    ENJIN_EXPECT_FALSE(ShortcutChordParses("Ctrl+Nonsense"));
    ENJIN_EXPECT_FALSE(ShortcutChordParses("Meta+Q"));
    ENJIN_EXPECT_FALSE(ShortcutChordParses("Ctrl+"));
    // And an empty chord is a true statement, not a failure to parse.
    ENJIN_EXPECT_TRUE(ShortcutChordParses(""));
}

ENJIN_TEST_MAIN()
