#include "EnjinTest.h"
#include "Enjin/Input/InputAction.h"
#include <string>

using namespace Enjin;
using namespace Enjin::InputSystem;

// ============================================================================
// PROMPT TEXT RESOLVES TO THE LIVE BINDING
//
// Four shipped component defaults said "Press E" while the engine knew the real
// binding all along, so every prompt started lying the moment a player rebound
// Interact. Authored text now carries a token and this substitutes it.
//
// The rule these tests pin down: a prompt may be stale, but it must never be
// EMPTY or confidently wrong. An unresolved token stays visible so the author
// can see it; a blanked one is an invisible mistake.
// ============================================================================

ENJIN_TEST(PromptBindings, TokenResolvesToTheBoundKey) {
    InputActionMap map;
    map.LoadDefaults();

    const std::string out = map.ResolvePromptText("Press {Interact} to open");

    // Whatever Interact is bound to, the braces are gone and something is there.
    ENJIN_EXPECT_TRUE(out.find("{Interact}") == std::string::npos);
    ENJIN_EXPECT_TRUE(out.rfind("Press ", 0) == 0);
    ENJIN_EXPECT_TRUE(out.find(" to open") != std::string::npos);
    ENJIN_EXPECT_TRUE(out.size() > std::string("Press  to open").size());
}

ENJIN_TEST(PromptBindings, RebindingChangesThePrompt) {
    InputActionMap map;
    map.LoadDefaults();
    const std::string before = map.ResolvePromptText("Press {Interact}");

    // Find Interact and move it to a different key.
    i32 interact = -1;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* n = map.GetActionName(i);
        if (n && std::string(n) == "Interact") { interact = i; break; }
    }
    ENJIN_ASSERT_TRUE(interact >= 0);
    InputBinding rebound;
    rebound.type = BindingType::Key;
    rebound.code = static_cast<i32>(KeyCode::J);
    map.SetBinding(static_cast<GameAction>(interact), 0, rebound);

    const std::string after = map.ResolvePromptText("Press {Interact}");

    // This is the whole point: the prompt followed the rebind rather than
    // continuing to name a key nobody is pressing.
    ENJIN_EXPECT_TRUE(after != before);
    ENJIN_EXPECT_TRUE(after.find("{") == std::string::npos);
}

ENJIN_TEST(PromptBindings, TextWithNoTokenIsUntouched) {
    InputActionMap map;
    map.LoadDefaults();
    // Old scenes authored before the token existed keep working. They stay
    // stale, which is what they already were, rather than breaking.
    const std::string legacy = "Press E to interact";
    ENJIN_EXPECT_TRUE(map.ResolvePromptText(legacy) == legacy);
    ENJIN_EXPECT_TRUE(map.ResolvePromptText("") == "");
}

ENJIN_TEST(PromptBindings, UnknownTokenStaysVisible) {
    InputActionMap map;
    map.LoadDefaults();
    // A typo'd action name is an authoring mistake, and it should be one the
    // author can SEE. Blanking it produces "Press  to win" and no clue why.
    const std::string out = map.ResolvePromptText("Press {Frobnicate} to win");
    ENJIN_EXPECT_TRUE(out.find("{Frobnicate}") != std::string::npos);
}

ENJIN_TEST(PromptBindings, MalformedBracesDoNotEatTheText) {
    InputActionMap map;
    map.LoadDefaults();
    // An unclosed brace must not swallow the rest of the string.
    const std::string out = map.ResolvePromptText("Press {Interact to open");
    ENJIN_EXPECT_TRUE(out.find("to open") != std::string::npos);
}

ENJIN_TEST(PromptBindings, SeveralTokensInOneString) {
    InputActionMap map;
    map.LoadDefaults();
    const std::string out = map.ResolvePromptText("{Interact} to open, {Jump} to climb");
    ENJIN_EXPECT_TRUE(out.find("to open") != std::string::npos);
    ENJIN_EXPECT_TRUE(out.find("to climb") != std::string::npos);
    ENJIN_EXPECT_TRUE(out.find("{Interact}") == std::string::npos);
}

ENJIN_TEST_MAIN()
