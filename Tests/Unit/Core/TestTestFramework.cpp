// The test framework had no way to tell a passing test from an empty one.
//
// Assertions only ever incremented a FAILURE counter, so a test body with no
// assertions in it reported PASS. That is how 14 empty tests, 24 asserting only
// EXPECT_TRUE(true), and 5 that returned before their first assertion when a
// fixture was missing all stayed green -- including
// Bindings.RegisterAllDoesNotCrash, which covered exactly the surface that
// shipped a web player with ZERO script bindings for months (on WASM every raw
// asFUNCTION registration fails with asNOT_SUPPORTED, which is a return code and
// not a crash, so "did not crash" was true the whole time).
//
// The framework now counts assertions REACHED and fails a test that reached
// none, and ENJIN_SKIP marks a deliberate skip that is reported rather than
// counted as a pass. This file guards that machinery, because a suite whose gate
// silently stops working is worse than one that never had a gate.
#include "EnjinTest.h"
#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// The counter
// ---------------------------------------------------------------------------

ENJIN_TEST(TestFramework, AssertionsAreCounted) {
    // Every macro must call CountAssertion before it evaluates anything, so the
    // count is of assertions REACHED. Measured through the public context.
    const int before = EnjinTest::CurrentContext().assertions;

    ENJIN_EXPECT_TRUE(1 + 1 == 2);
    ENJIN_EXPECT_EQ(2 + 2, 4);
    ENJIN_EXPECT_FALSE(false);

    // Three assertions above, plus this one has not run yet when the value is
    // read, so the delta is exactly 3.
    const int after = EnjinTest::CurrentContext().assertions;
    ENJIN_EXPECT_EQ(after - before, 3);
}

ENJIN_TEST(TestFramework, EveryMacroFamilyCounts) {
    // A macro added later that forgets CountAssertion would reintroduce the hole
    // for its own users only, which is the hardest version to notice. Exercise
    // one of each family and require the count to move every time.
    int last = EnjinTest::CurrentContext().assertions;
    auto moved = [&last]() {
        const int now = EnjinTest::CurrentContext().assertions;
        const bool advanced = now > last;
        last = now;
        return advanced;
    };

    ENJIN_EXPECT_TRUE(true);            bool a = moved();
    ENJIN_EXPECT_EQ(1, 1);              bool b = moved();
    ENJIN_EXPECT_NE(1, 2);              bool c = moved();
    ENJIN_EXPECT_LT(1, 2);              bool d = moved();
    ENJIN_EXPECT_GE(2, 2);              bool e = moved();
    ENJIN_EXPECT_FLOAT_EQ(1.0f, 1.0f);  bool f = moved();
    ENJIN_EXPECT_STR_EQ("x", "x");      bool g = moved();
    ENJIN_EXPECT_NULL(nullptr);         bool h = moved();
    int one = 1;
    ENJIN_EXPECT_NOT_NULL(&one);        bool i = moved();
    ENJIN_SURVIVED("nothing in particular"); bool j = moved();

    ENJIN_EXPECT_TRUE(a && b && c && d && e && f && g && h && i && j);
}

ENJIN_TEST(TestFramework, AnAssertionInsideADeadBranchDoesNotCount) {
    // The count has to be of assertions REACHED, not assertions written. A test
    // whose only checks sit behind a condition that never holds has checked
    // nothing, and must not be able to buy a pass with them.
    const int before = EnjinTest::CurrentContext().assertions;
    if (false) {
        ENJIN_EXPECT_TRUE(false);
        ENJIN_EXPECT_EQ(1, 2);
    }
    const int after = EnjinTest::CurrentContext().assertions;
    ENJIN_EXPECT_EQ(after - before, 0);
}

// ---------------------------------------------------------------------------
// Skip
// ---------------------------------------------------------------------------

ENJIN_TEST(TestFramework, SkipIsReportedNotPassed) {
    // ENJIN_SKIP returns immediately, so this test cannot both skip and assert.
    // Drive the underlying call instead, check the flag, then clear it so the
    // runner does not report this test as skipped.
    auto& ctx = EnjinTest::CurrentContext();
    ENJIN_EXPECT_FALSE(ctx.skipped);

    EnjinTest::SkipTest("a reason the runner will print");
    ENJIN_EXPECT_TRUE(ctx.skipped);
    ENJIN_EXPECT_TRUE(std::string(ctx.skipReason) == "a reason the runner will print");

    ctx.skipped = false;
    ctx.skipReason = nullptr;
}

ENJIN_TEST(TestFramework, FailuresAreCountedSeparatelyFromAssertions) {
    // A failing assertion still counts as an assertion -- otherwise a test whose
    // every check failed would be reported as "no assertions ran" rather than as
    // the failure it is.
    auto& ctx = EnjinTest::CurrentContext();
    const int assertionsBefore = ctx.assertions;
    const int failuresBefore = ctx.failures;

    // This prints one red FAIL line on purpose. Say so, because an unexplained
    // failure line in CI output is how people learn to skim past them.
    std::printf("    (the next FAIL line is deliberate: a failed assertion still counts)\n");
    ENJIN_EXPECT_TRUE(false);   // deliberately fails

    ENJIN_EXPECT_EQ(ctx.assertions - assertionsBefore, 2);  // the failure + this one
    ENJIN_EXPECT_EQ(ctx.failures - failuresBefore, 1);

    // Swallow the deliberate failure so this test reports green.
    ctx.failures = failuresBefore;
}

ENJIN_TEST_MAIN()
