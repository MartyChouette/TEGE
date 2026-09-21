// The GPU-safe-point capability, pinned at the type level.
//
// Destroying or recreating a GPU object outside RenderSystem::FlushPendingChanges
// invalidates a command buffer that is still recording, and the driver
// access-violates at SUBMIT -- so the stack you get names the submit and not the
// code that broke it. That rule used to be a comment on each function carrying
// it, and a comment does not survive being called from a new place.
//
// GpuLifetimeToken makes it a compile error instead. These tests assert the
// PROPERTIES that make that true, because the thing they guard -- "this does not
// compile from an editor panel" -- cannot itself be written as a passing test.
// If any assertion here fails, the token has stopped being a capability and has
// become a parameter anyone can supply.
#include "EnjinTest.h"
#include "Enjin/Renderer/GpuLifetime.h"

#include <type_traits>

using Enjin::Renderer::GpuLifetimeToken;

ENJIN_TEST(GpuLifetimeToken, test_a_token_cannot_be_constructed_by_a_caller) {
    // Arrange / Act — compile-time properties, so the assertions are the test.
    constexpr bool defaultConstructible = std::is_default_constructible_v<GpuLifetimeToken>;

    // Assert — the constructor is private and RenderSystem is its only friend.
    // A public constructor would let an editor panel mint its own and the whole
    // mechanism would be decoration.
    ENJIN_EXPECT_TRUE(!defaultConstructible);
}

ENJIN_TEST(GpuLifetimeToken, test_a_token_cannot_be_copied_or_moved) {
    // Arrange / Act
    constexpr bool copyCtor = std::is_copy_constructible_v<GpuLifetimeToken>;
    constexpr bool copyAssign = std::is_copy_assignable_v<GpuLifetimeToken>;
    constexpr bool moveCtor = std::is_move_constructible_v<GpuLifetimeToken>;
    constexpr bool moveAssign = std::is_move_assignable_v<GpuLifetimeToken>;

    // Assert — a copyable token could be stashed in a member during the safe
    // point and produced later from anywhere, which is exactly the call the type
    // exists to prevent. It may only be passed down the call chain by reference.
    ENJIN_EXPECT_TRUE(!copyCtor);
    ENJIN_EXPECT_TRUE(!copyAssign);
    ENJIN_EXPECT_TRUE(!moveCtor);
    ENJIN_EXPECT_TRUE(!moveAssign);
}

ENJIN_TEST(GpuLifetimeToken, test_the_token_costs_nothing_to_pass) {
    // Arrange / Act — it carries no state; it is a proof, not a payload.
    constexpr bool empty = std::is_empty_v<GpuLifetimeToken>;

    // Assert — if this ever fails, someone has put data on it, and the next
    // question is whether that data is really a capability or a disguised
    // parameter that belongs in its own argument.
    ENJIN_EXPECT_TRUE(empty);
}

ENJIN_TEST_MAIN()
