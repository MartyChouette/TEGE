#include "Enjin/Platform/Desktop.h"

#include <filesystem>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shellapi.h>
#  include <cstdlib>
#elif defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#else
#  include <spawn.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
extern char** environ;
#endif

#ifdef __EMSCRIPTEN__

namespace Enjin::Platform {

// In a browser the only one of these that means anything is opening a URL,
// which the page can do in a new tab. There is no local filesystem to reveal
// and no process to launch, and saying so honestly lets callers report it
// rather than appear to have done something.
bool OpenInDesktop(const std::string& pathOrUrl) {
    if (pathOrUrl.empty()) return false;
    return EM_ASM_INT({
        try { return window.open(UTF8ToString($0), "_blank") ? 1 : 0; } catch (e) { return 0; }
    }, pathOrUrl.c_str()) != 0;
}
bool RevealInFileManager(const std::string&) { return false; }
bool OpenUrlPreferChromium(const std::string& url) { return OpenInDesktop(url); }
bool LaunchDetached(const std::string&, const std::string&) { return false; }

} // namespace Enjin::Platform

#else // native desktop

namespace Enjin::Platform {

namespace {

#ifndef _WIN32
// Start a program detached. The child is reaped by init once this process
// stops waiting on it, which is what we want for a fire-and-forget open.
// No shell is involved anywhere here, so a path containing spaces, quotes or
// semicolons is passed through as one argument and cannot become a command.
bool SpawnDetached(const char* program, const std::vector<std::string>& args,
                   const std::string& workingDir) {
    std::vector<const char*> argv;
    argv.push_back(program);
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_t* actionsPtr = nullptr;
    if (!workingDir.empty()) {
#if defined(__linux__) || defined(__APPLE__)
        if (posix_spawn_file_actions_init(&actions) == 0) {
            // posix_spawn_file_actions_addchdir_np is glibc 2.29+ and macOS 10.15+.
            // Where it is missing the child simply inherits our directory, which
            // is a worse default but not a failure.
#  if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 29))
            posix_spawn_file_actions_addchdir_np(&actions, workingDir.c_str());
            actionsPtr = &actions;
#  elif defined(__APPLE__)
            posix_spawn_file_actions_addchdir_np(&actions, workingDir.c_str());
            actionsPtr = &actions;
#  else
            posix_spawn_file_actions_destroy(&actions);
#  endif
        }
#endif
    }

    pid_t pid = 0;
    const int rc = posix_spawnp(&pid, program, actionsPtr, nullptr,
                                const_cast<char* const*>(argv.data()), environ);
    if (actionsPtr) posix_spawn_file_actions_destroy(actionsPtr);
    return rc == 0 && pid > 0;
}
#endif

#ifdef _WIN32
bool ShellOpen(const char* verb, const std::string& target, const std::string& params,
               const std::string& workingDir) {
    const auto rc = reinterpret_cast<INT_PTR>(ShellExecuteA(
        nullptr, verb, target.c_str(),
        params.empty() ? nullptr : params.c_str(),
        workingDir.empty() ? nullptr : workingDir.c_str(), SW_SHOWNORMAL));
    return rc > 32;   // ShellExecute's documented success threshold
}
#endif

} // namespace

bool OpenInDesktop(const std::string& pathOrUrl) {
    if (pathOrUrl.empty()) return false;
#ifdef _WIN32
    return ShellOpen("open", pathOrUrl, {}, {});
#elif defined(__APPLE__)
    return SpawnDetached("open", {pathOrUrl}, {});
#else
    return SpawnDetached("xdg-open", {pathOrUrl}, {});
#endif
}

bool RevealInFileManager(const std::string& path) {
    if (path.empty()) return false;
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);

#ifdef _WIN32
    // /select, highlights the file inside its folder. Explorer wants backslashes.
    if (exists && !std::filesystem::is_directory(path, ec)) {
        std::string win = std::filesystem::absolute(path, ec).string();
        for (char& c : win) { if (c == '/') c = '\\'; }
        return ShellOpen("open", "explorer.exe", "/select,\"" + win + "\"", {});
    }
    return ShellOpen("open", path, {}, {});
#elif defined(__APPLE__)
    if (exists && !std::filesystem::is_directory(path, ec))
        return SpawnDetached("open", {"-R", path}, {});
    return SpawnDetached("open", {path}, {});
#else
    // No portable "select this file" on Linux: some file managers accept it,
    // most do not, and xdg-open on a file opens it in its editor rather than
    // showing it. Opening the containing directory is the behaviour that works
    // on every desktop.
    if (exists && !std::filesystem::is_directory(path, ec)) {
        const std::string parent = std::filesystem::path(path).parent_path().string();
        return SpawnDetached("xdg-open", {parent.empty() ? std::string(".") : parent}, {});
    }
    return SpawnDetached("xdg-open", {path}, {});
#endif
}

bool OpenUrlPreferChromium(const std::string& url) {
    if (url.empty()) return false;
#ifdef _WIN32
    std::vector<std::string> candidates;
    if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        candidates.push_back(std::string(localAppData) + "\\Google\\Chrome\\Application\\chrome.exe");
    candidates.push_back("C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe");
    candidates.push_back("C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe");
    candidates.push_back("C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe");
    candidates.push_back("C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe");
    std::error_code ec;
    for (const auto& exe : candidates) {
        if (std::filesystem::exists(exe, ec) && ShellOpen("open", exe, url, {}))
            return true;
    }
#elif !defined(__APPLE__)
    // Names as the browsers install themselves on Linux. posix_spawnp fails
    // cleanly when the binary is not on PATH, so trying each in turn costs
    // nothing and needs no separate "is it installed" probe.
    for (const char* browser : {"google-chrome", "google-chrome-stable", "chromium",
                                "chromium-browser", "microsoft-edge", "brave-browser"}) {
        if (SpawnDetached(browser, {url}, {})) return true;
    }
#endif
    return OpenInDesktop(url);
}

bool LaunchDetached(const std::string& exePath, const std::string& workingDir) {
    if (exePath.empty()) return false;
    std::error_code ec;
    if (!std::filesystem::exists(exePath, ec)) return false;

#ifdef _WIN32
    return ShellOpen("open", exePath, {}, workingDir);
#else
    // A built game is not necessarily marked executable when it arrives from a
    // zip or a copy off a Windows volume, and the spawn would fail with a
    // permission error the caller cannot explain. Set the bit first.
    auto perms = std::filesystem::status(exePath, ec).permissions();
    if (!ec && (perms & std::filesystem::perms::owner_exec) == std::filesystem::perms::none) {
        std::filesystem::permissions(exePath,
            perms | std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec,
            std::filesystem::perm_options::replace, ec);
    }
    // posix_spawnp only consults PATH for a bare name; an absolute path is used
    // as given, which is what we want here.
    const std::string abs = std::filesystem::absolute(exePath, ec).string();
    return SpawnDetached(ec ? exePath.c_str() : abs.c_str(), {}, workingDir);
#endif
}

} // namespace Enjin::Platform

#endif // __EMSCRIPTEN__
