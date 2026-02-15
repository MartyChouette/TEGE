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

        tc.func();
        totalRun++;

        if (ctx.failures == 0) {
            totalPassed++;
            if (verbose) {
                SetColor(Color::Green);
                printf("  PASS  %s\n", tc.name);
                SetColor(Color::Reset);
            }
        } else {
            totalFailed++;
            SetColor(Color::Red);
            printf("  FAIL  %s (%d assertion(s) failed)\n", tc.name, ctx.failures);
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
    if (totalSkipped > 0)
        printf("Skipped: %d", totalSkipped);
    printf("\n");
    printf("========================================\n");

    return totalFailed > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Assertion helpers (implementation)
// ---------------------------------------------------------------------------
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

#define ENJIN_TEST_MAIN()                                                           \
    int main(int argc, char** argv) {                                               \
        return EnjinTest::Run(argc, argv);                                          \
    }

// --- EXPECT (non-fatal) ---

#define ENJIN_EXPECT_TRUE(expr)                                                     \
    do { if (!(expr)) {                                                             \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_TRUE(" #expr ")");     \
    }} while(0)

#define ENJIN_EXPECT_FALSE(expr)                                                    \
    do { if ((expr)) {                                                              \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_FALSE(" #expr ")");    \
    }} while(0)

#define ENJIN_EXPECT_EQ(actual, expected)                                           \
    do { if (!((actual) == (expected))) {                                            \
        EnjinTest::ReportFailure(__FILE__, __LINE__,                                \
            "EXPECT_EQ(" #actual ", " #expected ")");                               \
    }} while(0)

#define ENJIN_EXPECT_NE(actual, expected)                                           \
    do { if ((actual) == (expected)) {                                              \
        EnjinTest::ReportFailure(__FILE__, __LINE__,                                \
            "EXPECT_NE(" #actual ", " #expected ")");                               \
    }} while(0)

#define ENJIN_EXPECT_LT(a, b)                                                      \
    do { if (!((a) < (b))) {                                                        \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_LT(" #a ", " #b ")");  \
    }} while(0)

#define ENJIN_EXPECT_LE(a, b)                                                      \
    do { if (!((a) <= (b))) {                                                       \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_LE(" #a ", " #b ")");  \
    }} while(0)

#define ENJIN_EXPECT_GT(a, b)                                                      \
    do { if (!((a) > (b))) {                                                        \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_GT(" #a ", " #b ")");  \
    }} while(0)

#define ENJIN_EXPECT_GE(a, b)                                                      \
    do { if (!((a) >= (b))) {                                                       \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_GE(" #a ", " #b ")");  \
    }} while(0)

#define ENJIN_EXPECT_FLOAT_NEAR(a, b, tol)                                         \
    do { if (std::fabs((a) - (b)) > (tol)) {                                       \
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
    do { if (std::string(a) != std::string(b)) {                                   \
        char _buf[512];                                                             \
        snprintf(_buf, sizeof(_buf),                                                \
            "EXPECT_STR_EQ(\"%s\", \"%s\")",                                        \
            std::string(a).c_str(), std::string(b).c_str());                        \
        EnjinTest::ReportFailureMsg(__FILE__, __LINE__, _buf);                      \
    }} while(0)

#define ENJIN_EXPECT_NULL(ptr)                                                      \
    do { if ((ptr) != nullptr) {                                                    \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_NULL(" #ptr ")");      \
    }} while(0)

#define ENJIN_EXPECT_NOT_NULL(ptr)                                                  \
    do { if ((ptr) == nullptr) {                                                    \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "EXPECT_NOT_NULL(" #ptr ")");  \
    }} while(0)

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
    do { if (!(expr)) {                                                             \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "ASSERT_TRUE(" #expr ")");     \
        return;                                                                     \
    }} while(0)

#define ENJIN_ASSERT_FALSE(expr)                                                    \
    do { if ((expr)) {                                                              \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "ASSERT_FALSE(" #expr ")");    \
        return;                                                                     \
    }} while(0)

#define ENJIN_ASSERT_EQ(actual, expected)                                           \
    do { if (!((actual) == (expected))) {                                            \
        EnjinTest::ReportFailure(__FILE__, __LINE__,                                \
            "ASSERT_EQ(" #actual ", " #expected ")");                               \
        return;                                                                     \
    }} while(0)

#define ENJIN_ASSERT_NE(actual, expected)                                           \
    do { if ((actual) == (expected)) {                                              \
        EnjinTest::ReportFailure(__FILE__, __LINE__,                                \
            "ASSERT_NE(" #actual ", " #expected ")");                               \
        return;                                                                     \
    }} while(0)

#define ENJIN_ASSERT_NOT_NULL(ptr)                                                  \
    do { if ((ptr) == nullptr) {                                                    \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "ASSERT_NOT_NULL(" #ptr ")");  \
        return;                                                                     \
    }} while(0)

#define ENJIN_ASSERT_NULL(ptr)                                                      \
    do { if ((ptr) != nullptr) {                                                    \
        EnjinTest::ReportFailure(__FILE__, __LINE__, "ASSERT_NULL(" #ptr ")");      \
        return;                                                                     \
    }} while(0)
