#pragma once

// EnjinTest — Header-only unit test framework for Enjin Engine
// Zero external dependencies. Auto-registration, colored output, CLI filtering.

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>
#include <type_traits>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#endif

namespace EnjinTest {

// ---------------------------------------------------------------------------
// Console colors
// ---------------------------------------------------------------------------
enum class Color { Reset, Red, Green, Yellow, Cyan };

inline void SetColor(Color c) {
#ifdef _WIN32
    static bool initialized = false;
    static bool useAnsi = false;
    if (!initialized) {
        initialized = true;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            if (SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                useAnsi = true;
        }
    }
    if (useAnsi) {
        switch (c) {
            case Color::Reset:  printf("\033[0m");  break;
            case Color::Red:    printf("\033[31m"); break;
            case Color::Green:  printf("\033[32m"); break;
            case Color::Yellow: printf("\033[33m"); break;
            case Color::Cyan:   printf("\033[36m"); break;
        }
    } else {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        switch (c) {
            case Color::Reset:  SetConsoleTextAttribute(h, 7);  break;
            case Color::Red:    SetConsoleTextAttribute(h, 12); break;
            case Color::Green:  SetConsoleTextAttribute(h, 10); break;
            case Color::Yellow: SetConsoleTextAttribute(h, 14); break;
            case Color::Cyan:   SetConsoleTextAttribute(h, 11); break;
        }
    }
#else
    switch (c) {
        case Color::Reset:  printf("\033[0m");  break;
        case Color::Red:    printf("\033[31m"); break;
        case Color::Green:  printf("\033[32m"); break;
        case Color::Yellow: printf("\033[33m"); break;
        case Color::Cyan:   printf("\033[36m"); break;
    }
#endif
}

// ---------------------------------------------------------------------------
// Test case storage
// ---------------------------------------------------------------------------
struct TestCase {
    const char* suite;
    const char* name;
    std::function<void()> func;
};

inline std::vector<TestCase>& GetRegistry() {
    static std::vector<TestCase> registry;
    return registry;
}

struct TestRegistrar {
    TestRegistrar(const char* suite, const char* name, std::function<void()> func) {
        GetRegistry().push_back({suite, name, std::move(func)});
    }
};

// ---------------------------------------------------------------------------
// Per-test failure tracking (thread-local for safety)
// ---------------------------------------------------------------------------
struct TestContext {
    int failures = 0;
    bool aborted = false;
    // Assertions ATTEMPTED, not failed. A test that never asserts anything used
    // to report PASS, because nothing incremented `failures`. That is how 14
    // empty tests and a wall of EXPECT_TRUE(true) stayed green -- including
    // Bindings.RegisterAllDoesNotCrash, which covered exactly the surface that
    // shipped a web player with ZERO script bindings for months
    // (asNOT_SUPPORTED is a return code, not a crash).
    int assertions = 0;
    // An explicit, reported skip. Distinct from a filtered-out test and from a
    // pass: five tests used to `return` before their first assertion when a
    // fixture or an env var was missing, and reported PASS, so the FBX import
    // and ray-tracing probe paths were green in CI without ever running.
    bool skipped = false;
    const char* skipReason = nullptr;
};

inline TestContext& CurrentContext() {
    static thread_local TestContext ctx;
    return ctx;
}

// ---------------------------------------------------------------------------
// Pattern matching (trailing wildcard only)
// ---------------------------------------------------------------------------
inline bool MatchFilter(const char* text, const char* pattern) {
    size_t pLen = strlen(pattern);
    if (pLen == 0) return true;
    if (pattern[pLen - 1] == '*') {
        return strncmp(text, pattern, pLen - 1) == 0;
    }
    return strcmp(text, pattern) == 0;
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------
inline int Run(int argc, char** argv) {
    const char* filterPattern = nullptr;
    const char* suiteFilter = nullptr;
    bool listOnly = false;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc)
            filterPattern = argv[++i];
        else if (strcmp(argv[i], "--suite") == 0 && i + 1 < argc)
            suiteFilter = argv[++i];
        else if (strcmp(argv[i], "--list") == 0)
            listOnly = true;
        else if (strcmp(argv[i], "-v") == 0)
            verbose = true;
    }

    auto& registry = GetRegistry();

    if (listOnly) {
        for (auto& tc : registry) {
            if (suiteFilter && strcmp(tc.suite, suiteFilter) != 0) continue;
            if (filterPattern && !MatchFilter(tc.name, filterPattern)) continue;
            printf("%s.%s\n", tc.suite, tc.name);
        }
        return 0;
    }

    int totalRun = 0, totalPassed = 0, totalFailed = 0, totalSkipped = 0;
    int totalDeclaredSkip = 0;   // ENJIN_SKIP, as opposed to filtered out
    const char* lastSuite = nullptr;

    for (auto& tc : registry) {
        if (suiteFilter && strcmp(tc.suite, suiteFilter) != 0) { totalSkipped++; continue; }
        if (filterPattern && !MatchFilter(tc.name, filterPattern)) { totalSkipped++; continue; }

        if (!lastSuite || strcmp(lastSuite, tc.suite) != 0) {
            lastSuite = tc.suite;
            SetColor(Color::Cyan);
            printf("\n[%s]\n", tc.suite);
            SetColor(Color::Reset);
        }

        auto& ctx = CurrentContext();
        ctx.failures = 0;
        ctx.aborted = false;
        ctx.assertions = 0;
        ctx.skipped = false;
        ctx.skipReason = nullptr;

        tc.func();
        totalRun++;

        if (ctx.skipped) {
            // Deliberately not run. Reported, and never counted as a pass --
            // the whole point is that a missing fixture stops being invisible.
            totalDeclaredSkip++;
            SetColor(Color::Yellow);
            printf("  SKIP  %s (%s)\n", tc.name, ctx.skipReason ? ctx.skipReason : "no reason given");
            SetColor(Color::Reset);
        } else if (ctx.assertions == 0) {
            // A test that checked nothing is not a passing test. If a test
            // genuinely has nothing to assert, it says so with ENJIN_SKIP.
            totalFailed++;
            SetColor(Color::Red);
            printf("  FAIL  %s (no assertions ran)\n", tc.name);
            SetColor(Color::Reset);
        } else if (ctx.failures == 0) {
            totalPassed++;
            if (verbose) {
                SetColor(Color::Green);
                printf("  PASS  %s (%d assertion(s))\n", tc.name, ctx.assertions);
                SetColor(Color::Reset);
            }
        } else {
            totalFailed++;
            SetColor(Color::Red);
            printf("  FAIL  %s (%d of %d assertion(s) failed)\n",
                   tc.name, ctx.failures, ctx.assertions);
            SetColor(Color::Reset);
        }
    }

    printf("\n");
    printf("========================================\n");
    printf("  Ran: %d  ", totalRun);
    SetColor(Color::Green);
    printf("Passed: %d  ", totalPassed);
    if (totalFailed > 0) {
        SetColor(Color::Red);
        printf("Failed: %d  ", totalFailed);
    } else {
        printf("Failed: %d  ", totalFailed);
    }
    SetColor(Color::Reset);
    if (totalDeclaredSkip > 0) {
        SetColor(Color::Yellow);
        printf("Skipped: %d  ", totalDeclaredSkip);
        SetColor(Color::Reset);
    }
    if (totalSkipped > 0)
        printf("Filtered: %d", totalSkipped);
    printf("\n");
    printf("========================================\n");

    return totalFailed > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Assertion helpers (implementation)
// ---------------------------------------------------------------------------
// Called by every assertion macro before it evaluates anything, so the count is
// of assertions REACHED. An assertion inside a branch that never runs does not
// count, which is the point: it did not check anything.
inline void CountAssertion() { CurrentContext().assertions++; }

inline void SkipTest(const char* reason) {
    auto& ctx = CurrentContext();
    ctx.skipped = true;
    ctx.skipReason = reason;
}

inline void ReportFailure(const char* file, int line, const char* expr) {
    SetColor(Color::Red);
    printf("    FAIL: %s:%d: %s\n", file, line, expr);
    SetColor(Color::Reset);
    CurrentContext().failures++;
}

inline void ReportFailureMsg(const char* file, int line, const char* msg) {
    SetColor(Color::Red);
    printf("    FAIL: %s:%d: %s\n", file, line, msg);
    SetColor(Color::Reset);
    CurrentContext().failures++;
}

} // namespace EnjinTest

// ---------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------

#define ENJIN_TEST(Suite, Name)                                                     \
    static void EnjinTest_##Suite##_##Name();                                       \
    static EnjinTest::TestRegistrar s_reg_##Suite##_##Name(                         \
        #Suite, #Name, EnjinTest_##Suite##_##Name);                                 \
    static void EnjinTest_##Suite##_##Name()

// Declare a test deliberately not run, with a reason. Reported as SKIP and
// never counted as a pass. Use this instead of a bare `return` when a fixture,
// an asset or an environment variable is missing -- a silent return reports
// PASS, which is how the FBX import and ray-tracing probe paths stayed green in
// CI without ever executing.
// The property is that we got here at all: the code under test survived
// something hostile (a fuzzed buffer, a malformed scene, a teardown ordering)
// without crashing, hanging or corrupting itself.
//
// This exists because that property was being written as ENJIN_EXPECT_TRUE(true),
// which reads as a placeholder and is indistinguishable from a test somebody
// forgot to finish. Surviving IS a real property for a fuzz case -- it just has
// to say so, and say WHAT it survived, so the next reader can tell a deliberate
// no-crash assertion from an unfinished one.
//
// Not for a test whose NAME promises something checkable. "StringLengthCapped"
// must check the cap.
#define ENJIN_SURVIVED(what)                                                        \
    do { EnjinTest::CountAssertion(); (void)sizeof(what); } while(0)

#define ENJIN_SKIP(reason)                                                          \
    do { EnjinTest::SkipTest(reason); return; } while(0)

#define ENJIN_TEST_MAIN()                                                           \
    int main(int argc, char** argv) {                                               \
        return EnjinTest::Run(argc, argv);                                          \
    }

// --- EXPECT (non-fatal) ---

// --- values in a failure line -----------------------------------------------
//
// A comparison written as ENJIN_EXPECT_TRUE(a == b) prints "EXPECT_TRUE(a == b)"
// and nothing else, so a failure names the line and not the numbers -- which is
// a rebuild under a debugger to learn something the test already knew.
// ENJIN_EXPECT_FLOAT_NEAR has always printed both sides, and those are the
// readable failures in the log. This is the same idea for every other
// comparison.
//
// It has to accept whatever a test compares, so an unprintable type yields "?"
// rather than refusing to compile. The ORDER below matters: a const char* is a
// pointer and a bool is an integer, so the specific cases come first.
namespace EnjinTest {

template <typename T>
inline std::string Describe(const T& value) {
    using U = std::decay_t<T>;
    if constexpr (std::is_same_v<U, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_same_v<U, char*> || std::is_same_v<U, const char*>) {
        return value ? std::string("\"") + value + "\"" : std::string("(null)");
    } else if constexpr (std::is_same_v<U, std::string>) {
        return std::string("\"") + value + "\"";
    } else if constexpr (std::is_enum_v<U>) {
        return std::to_string(static_cast<long long>(value));
    } else if constexpr (std::is_floating_point_v<U>) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(value));
        return buffer;
    } else if constexpr (std::is_integral_v<U>) {
        return std::to_string(value);
    } else if constexpr (std::is_pointer_v<U>) {
        if (!value) return "nullptr";
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%p", static_cast<const void*>(value));
        return buffer;
    } else {
        return "?";
    }
}

} // namespace EnjinTest

// Both operands are bound ONCE. A macro that evaluated (a) in the comparison and
// again in the message would call a test's helper twice, which is how an
// assertion starts changing what it measures.
#define ENJIN_IMPL_CMP(label, a, b, cond, onFailure)                                \
    do { EnjinTest::CountAssertion();                                               \
         auto&& _enjinA = (a); auto&& _enjinB = (b);                                \
         if (!(cond)) {                                                             \
             const std::string _enjinMsg = std::string(label "(" #a "=") +          \
                 EnjinTest::Describe(_enjinA) + ", " #b "=" +                       \
                 EnjinTest::Describe(_enjinB) + ")";                                \
             EnjinTest::ReportFailureMsg(__FILE__, __LINE__, _enjinMsg.c_str());    \
             onFailure;                                                             \
         }} while(0)

#define ENJIN_EXPECT_TRUE(expr)                                                     \
    do { EnjinTest::CountAssertion(); if (!(expr)) {                                                             \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_TRUE(" #expr ")");     \
    }} while(0)

#define ENJIN_EXPECT_FALSE(expr)                                                    \
    do { EnjinTest::CountAssertion(); if ((expr)) {                                                              \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_FALSE(" #expr ")");    \
    }} while(0)

#define ENJIN_EXPECT_EQ(actual, expected)                                           \
    ENJIN_IMPL_CMP("EXPECT_EQ", actual, expected, _enjinA == _enjinB, (void)0)

#define ENJIN_EXPECT_NE(actual, expected)                                           \
    ENJIN_IMPL_CMP("EXPECT_NE", actual, expected, !(_enjinA == _enjinB), (void)0)

#define ENJIN_EXPECT_LT(a, b)                                                       \
    ENJIN_IMPL_CMP("EXPECT_LT", a, b, _enjinA < _enjinB, (void)0)

#define ENJIN_EXPECT_LE(a, b)                                                       \
    ENJIN_IMPL_CMP("EXPECT_LE", a, b, _enjinA <= _enjinB, (void)0)

#define ENJIN_EXPECT_GT(a, b)                                                       \
    ENJIN_IMPL_CMP("EXPECT_GT", a, b, _enjinA > _enjinB, (void)0)

#define ENJIN_EXPECT_GE(a, b)                                                       \
    ENJIN_IMPL_CMP("EXPECT_GE", a, b, _enjinA >= _enjinB, (void)0)

#define ENJIN_EXPECT_FLOAT_NEAR(a, b, tol)                                         \
    do { EnjinTest::CountAssertion(); if (std::fabs((a) - (b)) > (tol)) {                                       \
        char _buf[256];                                                             \
        snprintf(_buf, sizeof(_buf),                                                \
            "EXPECT_FLOAT_NEAR(" #a "=%g, " #b "=%g, tol=%g) delta=%g",            \
            (double)(a), (double)(b), (double)(tol),                                \
            (double)std::fabs((a) - (b)));                                          \
        EnjinTest::ReportFailureMsg(__FILE__, __LINE__, _buf);                      \
    }} while(0)

#define ENJIN_EXPECT_FLOAT_EQ(a, b)                                                \
    ENJIN_EXPECT_FLOAT_NEAR(a, b, 0.001f)

#define ENJIN_EXPECT_STR_EQ(a, b)                                                  \
    do { EnjinTest::CountAssertion(); if (std::string(a) != std::string(b)) {                                   \
        char _buf[512];                                                             \
        snprintf(_buf, sizeof(_buf),                                                \
            "EXPECT_STR_EQ(\"%s\", \"%s\")",                                        \
            std::string(a).c_str(), std::string(b).c_str());                        \
        EnjinTest::ReportFailureMsg(__FILE__, __LINE__, _buf);                      \
    }} while(0)

#define ENJIN_EXPECT_NULL(ptr)                                                      \
    do { EnjinTest::CountAssertion(); if ((ptr) != nullptr) {                                                    \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_NULL(" #ptr ")");      \
    }} while(0)

#define ENJIN_EXPECT_NOT_NULL(ptr)                                                  \
    do { EnjinTest::CountAssertion(); if ((ptr) == nullptr) {                                                    \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_NOT_NULL(" #ptr ")");  \
    }} while(0)

// The ASSERT family had TRUE, FALSE, EQ, NE, NULL and NOT_NULL and nothing else,
// so every ordering comparison that had to be fatal was written as
// ENJIN_ASSERT_TRUE(x > y) -- 489 of them -- and threw both values away.
// CLAUDE.md documented that workaround as the convention. It existed only
// because these did not.
#define ENJIN_ASSERT_LT(a, b)                                                       \
    ENJIN_IMPL_CMP("ASSERT_LT", a, b, _enjinA < _enjinB, return)

#define ENJIN_ASSERT_LE(a, b)                                                       \
    ENJIN_IMPL_CMP("ASSERT_LE", a, b, _enjinA <= _enjinB, return)

#define ENJIN_ASSERT_GT(a, b)                                                       \
    ENJIN_IMPL_CMP("ASSERT_GT", a, b, _enjinA > _enjinB, return)

#define ENJIN_ASSERT_GE(a, b)                                                       \
    ENJIN_IMPL_CMP("ASSERT_GE", a, b, _enjinA >= _enjinB, return)

#define ENJIN_ASSERT_FLOAT_NEAR(a, b, tol)                                          \
    do { EnjinTest::CountAssertion();                                               \
         const double _enjinA = (double)(a), _enjinB = (double)(b);                 \
         if (std::fabs(_enjinA - _enjinB) > (double)(tol)) {                        \
             char _buf[256];                                                        \
             snprintf(_buf, sizeof(_buf),                                           \
                 "ASSERT_FLOAT_NEAR(" #a "=%g, " #b "=%g, tol=%g) delta=%g",        \
                 _enjinA, _enjinB, (double)(tol), std::fabs(_enjinA - _enjinB));    \
             EnjinTest::ReportFailureMsg(__FILE__, __LINE__, _buf);                 \
             return;                                                                \
         }} while(0)

#define ENJIN_ASSERT_FLOAT_EQ(a, b)                                                 \
    ENJIN_ASSERT_FLOAT_NEAR(a, b, 0.001f)

#define ENJIN_ASSERT_STR_EQ(a, b)                                                   \
    ENJIN_IMPL_CMP("ASSERT_STR_EQ", a, b,                                           \
                   std::string(_enjinA) == std::string(_enjinB), return)

#define ENJIN_EXPECT_VEC2_EQ(v, ex, ey)                                            \
    do {                                                                            \
        ENJIN_EXPECT_FLOAT_EQ((v).x, (ex));                                        \
        ENJIN_EXPECT_FLOAT_EQ((v).y, (ey));                                        \
    } while(0)

#define ENJIN_EXPECT_VEC3_EQ(v, ex, ey, ez)                                        \
    do {                                                                            \
        ENJIN_EXPECT_FLOAT_EQ((v).x, (ex));                                        \
        ENJIN_EXPECT_FLOAT_EQ((v).y, (ey));                                        \
        ENJIN_EXPECT_FLOAT_EQ((v).z, (ez));                                        \
    } while(0)

#define ENJIN_EXPECT_VEC4_EQ(v, ex, ey, ez, ew)                                    \
    do {                                                                            \
        ENJIN_EXPECT_FLOAT_EQ((v).x, (ex));                                        \
        ENJIN_EXPECT_FLOAT_EQ((v).y, (ey));                                        \
        ENJIN_EXPECT_FLOAT_EQ((v).z, (ez));                                        \
        ENJIN_EXPECT_FLOAT_EQ((v).w, (ew));                                        \
    } while(0)

#define ENJIN_EXPECT_HAS_COMPONENT(world, entity, Type)                            \
    ENJIN_EXPECT_TRUE((world).HasComponent<Type>(entity))

// --- ASSERT (fatal — returns from test on failure) ---

#define ENJIN_ASSERT_TRUE(expr)                                                     \
    do { EnjinTest::CountAssertion(); if (!(expr)) {                                                             \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "ASSERT_TRUE(" #expr ")");     \
        return;                                                                     \
    }} while(0)

#define ENJIN_ASSERT_FALSE(expr)                                                    \
    do { EnjinTest::CountAssertion(); if ((expr)) {                                                              \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "ASSERT_FALSE(" #expr ")");    \
        return;                                                                     \
    }} while(0)

#define ENJIN_ASSERT_EQ(actual, expected)                                           \
    do { EnjinTest::CountAssertion(); if (!((actual) == (expected))) {                                            \
        EnjinTest::ReportFailure(__FILE__, __LINE__,                                \
            "ASSERT_EQ(" #actual ", " #expected ")");                               \
        return;                                                                     \
    }} while(0)

#define ENJIN_ASSERT_NE(actual, expected)                                           \
    do { EnjinTest::CountAssertion(); if ((actual) == (expected)) {                                              \
        EnjinTest::ReportFailure(__FILE__, __LINE__,                                \
            "ASSERT_NE(" #actual ", " #expected ")");                               \
        return;                                                                     \
    }} while(0)

#define ENJIN_ASSERT_NOT_NULL(ptr)                                                  \
    do { EnjinTest::CountAssertion(); if ((ptr) == nullptr) {                                                    \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "ASSERT_NOT_NULL(" #ptr ")");  \
        return;                                                                     \
    }} while(0)

#define ENJIN_ASSERT_NULL(ptr)                                                      \
    do { EnjinTest::CountAssertion(); if ((ptr) != nullptr) {                                                    \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "ASSERT_NULL(" #ptr ")");      \
        return;                                                                     \
    }} while(0)
