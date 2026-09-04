// A crash report has to name the code that crashed.
//
// The Windows handler walks the frames and writes a minidump. The POSIX one
// wrote the context, the last log lines and the signal number and then
// re-raised, so every Linux crash report said something went wrong and nothing
// about where.
//
// Crashing on purpose only works in a child process, so the POSIX tests fork
// and let the child take the signal. On Windows the handler is a structured
// exception filter with no equivalent fork, so the test there checks the parts
// that are reachable without dying.
#include "EnjinTest.h"
#include "Enjin/Debug/CrashHandler.h"

#include <cstdio>
#include <cstring>
#include <string>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace Enjin;

namespace {

std::string ReadWholeFile(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return {};
    std::string out;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}

bool Contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

ENJIN_TEST(CrashHandler, InstallAndUninstallAreSafe) {
    // Arrange / Act: the pair must be callable without a crash to handle.
    Debug::InstallCrashHandler();
    Debug::UninstallCrashHandler();

    // Assert: reaching here is the assertion. A handler that broke the process
    // on install would take the whole suite with it.
    ENJIN_EXPECT_TRUE(true);
}

#ifndef _WIN32

ENJIN_TEST(CrashHandler, ReportNamesTheSignalAndTheCallStack) {
    // Arrange: a report path this test owns, cleared first so a stale file
    // cannot pass for a fresh one.
    // The handler writes to a fixed name next to the working directory.
    const char* reportPath = "enjin_crash.txt";
    std::remove(reportPath);

    // Act: fork, install the handler in the child, and dereference null. The
    // child dies; the parent waits and reads what it left behind.
    pid_t pid = fork();
    ENJIN_ASSERT_TRUE(pid >= 0);
    if (pid == 0) {
        Debug::InstallCrashHandler();
        volatile int* boom = nullptr;
        *boom = 1;          // SIGSEGV
        _exit(0);           // not reached
    }
    int status = 0;
    waitpid(pid, &status, 0);

    // Assert: the child died of a signal, and the report it left names the
    // signal and carries a call stack, not just a context dump.
    ENJIN_EXPECT_TRUE(WIFSIGNALED(status));
    const std::string report = ReadWholeFile(reportPath);
    ENJIN_ASSERT_TRUE(!report.empty());
    ENJIN_EXPECT_TRUE(Contains(report, "ENJIN CRASH REPORT"));
    ENJIN_EXPECT_TRUE(Contains(report, "SIGSEGV"));
    ENJIN_EXPECT_TRUE(Contains(report, "Call Stack:"));

    // The stack has to hold frames. backtrace_symbols_fd writes one line per
    // frame, so the section between the heading and the next one is not empty.
    const size_t stackAt = report.find("Call Stack:");
    const size_t logAt = report.find("Last Log Lines:");
    ENJIN_ASSERT_TRUE(stackAt != std::string::npos && logAt != std::string::npos);
    ENJIN_ASSERT_TRUE(logAt > stackAt);
    const std::string stack = report.substr(stackAt, logAt - stackAt);
    // Heading, then at least a couple of frame lines.
    size_t lines = 0;
    for (char c : stack) { if (c == '\n') ++lines; }
    ENJIN_EXPECT_TRUE(lines >= 3);

    std::remove(reportPath);
}

ENJIN_TEST(CrashHandler, AbortIsReportedToo) {
    // Arrange
    const char* reportPath = "enjin_crash.txt";
    std::remove(reportPath);

    // Act
    pid_t pid = fork();
    ENJIN_ASSERT_TRUE(pid >= 0);
    if (pid == 0) {
        Debug::InstallCrashHandler();
        std::abort();       // SIGABRT
        _exit(0);           // not reached
    }
    int status = 0;
    waitpid(pid, &status, 0);

    // Assert
    ENJIN_EXPECT_TRUE(WIFSIGNALED(status));
    const std::string report = ReadWholeFile(reportPath);
    ENJIN_ASSERT_TRUE(!report.empty());
    ENJIN_EXPECT_TRUE(Contains(report, "SIGABRT"));
    ENJIN_EXPECT_TRUE(Contains(report, "Call Stack:"));

    std::remove(reportPath);
}

#endif // !_WIN32

ENJIN_TEST_MAIN()
