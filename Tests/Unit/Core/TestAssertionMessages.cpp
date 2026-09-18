// What a failing assertion prints.
//
// A comparison failure used to print only its own source text -- "EXPECT_EQ(
// world.Count(), 3)" -- so reading the log told you which line broke and not
// what the numbers were, which is a rebuild under a debugger to learn something
// the test already knew. Describe() is what fixed it, and these tests pin its
// output, because a describer that silently degrades to "?" would put the
// failure messages back where they started without failing anything.
#include "EnjinTest.h"
#include <string>

namespace {
struct NotPrintable { int a, b; };
enum class Colour { Red = 0, Green = 1 };
}

ENJIN_TEST(AssertionMessages, test_numbers_describe_as_their_value) {
    // Arrange / Act / Assert
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(7), "7");
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(-3), "-3");
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(static_cast<unsigned long long>(42)), "42");
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(0.5f), "0.5");
}

ENJIN_TEST(AssertionMessages, test_a_bool_does_not_describe_as_one_or_zero) {
    // An integer 1 in a failure line for a bool comparison reads as a count.
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(true), "true");
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(false), "false");
}

ENJIN_TEST(AssertionMessages, test_strings_are_quoted_so_an_empty_one_is_visible) {
    // "" is the difference between a wrong string and no string, and unquoted
    // they look identical in a log.
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(std::string("hi")), "\"hi\"");
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(std::string("")), "\"\"");
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe("literal"), "\"literal\"");
}

ENJIN_TEST(AssertionMessages, test_a_null_pointer_says_so) {
    const int* p = nullptr;
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(p), "nullptr");
}

ENJIN_TEST(AssertionMessages, test_an_enum_describes_as_its_ordinal) {
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(Colour::Green), "1");
}

ENJIN_TEST(AssertionMessages, test_an_unprintable_type_yields_a_marker_not_a_build_error) {
    // The property that lets the comparison macros accept anything a test
    // compares. Without it, adding values to the failure line would have broken
    // every assertion over a type with no obvious text form.
    NotPrintable value{1, 2};
    ENJIN_EXPECT_STR_EQ(EnjinTest::Describe(value), "?");
}

ENJIN_TEST(AssertionMessages, test_both_operands_are_evaluated_exactly_once) {
    // The macro reads each operand twice as written -- once to compare, once to
    // describe. If they were not bound to locals first, an assertion over a
    // counter or a queue pop would change what it measures.
    int calls = 0;
    auto bump = [&]() { ++calls; return 1; };

    ENJIN_EXPECT_EQ(bump(), 1);
    ENJIN_EXPECT_EQ(calls, 1);

    calls = 0;
    ENJIN_EXPECT_GT(bump(), 0);
    ENJIN_EXPECT_EQ(calls, 1);
}

ENJIN_TEST(AssertionMessages, test_the_fatal_comparisons_exist_and_pass_when_true) {
    // ENJIN_ASSERT_LT/LE/GT/GE/FLOAT_NEAR/FLOAT_EQ/STR_EQ did not exist, so a
    // fatal ordering check had to be ENJIN_ASSERT_TRUE(x > y), which threw both
    // values away. This is the compile-time half of that fix: if any of these
    // were missing, this file would not build.
    ENJIN_ASSERT_LT(1, 2);
    ENJIN_ASSERT_LE(2, 2);
    ENJIN_ASSERT_GT(3, 2);
    ENJIN_ASSERT_GE(3, 3);
    ENJIN_ASSERT_FLOAT_NEAR(1.0f, 1.0001f, 0.001f);
    // Two values that DIFFER and still compare equal, so each assertion is a
    // real check rather than a tautology the vacuous-assertion audit would
    // rightly flag. FLOAT_EQ carries a 0.001 tolerance.
    ENJIN_ASSERT_FLOAT_EQ(2.0f, 2.0005f);
    const std::string built = std::string("a") + "b";
    ENJIN_ASSERT_STR_EQ(built, "ab");
}

ENJIN_TEST_MAIN()
