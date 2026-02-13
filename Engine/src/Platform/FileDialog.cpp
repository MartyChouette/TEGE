#include "Enjin/Platform/FileDialog.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <algorithm>
#endif

namespace Enjin {

void* FileDialog::s_OwnerWindow = nullptr;

void FileDialog::SetOwnerWindow(void* handle) {
    s_OwnerWindow = handle;
}

#ifdef _WIN32

std::string FileDialog::OpenFile(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& defaultPath
) {
    // Build filter string for Windows (format: "Name\0*.ext\0Name2\0*.ext2\0\0")
    std::string filterStr;
    if (filters.empty()) {
        filterStr = "All Files\0*.*\0";
    } else {
        for (const auto& filter : filters) {
            filterStr += filter.name;
            filterStr.push_back('\0');
            filterStr += filter.extensions;
            filterStr.push_back('\0');
        }
    }
    filterStr.push_back('\0');

    char filename[MAX_PATH] = "";

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(s_OwnerWindow);
    ofn.lpstrFilter = filterStr.c_str();
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!defaultPath.empty()) {
        ofn.lpstrInitialDir = defaultPath.c_str();
    }

    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }

    return "";
}

std::string FileDialog::SaveFile(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& defaultPath,
    const std::string& defaultName
) {
    // Build filter string
    std::string filterStr;
    std::string defaultExt;

    if (filters.empty()) {
        filterStr = "All Files\0*.*\0";
    } else {
        for (const auto& filter : filters) {
            filterStr += filter.name;
            filterStr.push_back('\0');
            filterStr += filter.extensions;
            filterStr.push_back('\0');

            // Extract default extension from first filter
            if (defaultExt.empty() && !filter.extensions.empty()) {
                size_t dotPos = filter.extensions.find('.');
                size_t endPos = filter.extensions.find_first_of(";*", dotPos + 1);
                if (dotPos != std::string::npos) {
                    defaultExt = filter.extensions.substr(dotPos + 1,
                        endPos != std::string::npos ? endPos - dotPos - 1 : std::string::npos);
                }
            }
        }
    }
    filterStr.push_back('\0');

    char filename[MAX_PATH] = "";
    if (!defaultName.empty()) {
        strncpy(filename, defaultName.c_str(), MAX_PATH - 1);
    }

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(s_OwnerWindow);
    ofn.lpstrFilter = filterStr.c_str();
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrDefExt = defaultExt.empty() ? nullptr : defaultExt.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (!defaultPath.empty()) {
        ofn.lpstrInitialDir = defaultPath.c_str();
    }

    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }

    return "";
}

std::string FileDialog::OpenFolder(
    const std::string& title,
    const std::string& defaultPath
) {
    // Use IFileOpenDialog with FOS_PICKFOLDERS for a proper native folder picker.
    // Pass NULL as owner window to avoid modal freeze with GLFW+Vulkan.
    // COM is already initialized by GLFW (OleInitialize) — do NOT call
    // CoInitializeEx/CoUninitialize here.
    std::string result;

    IFileOpenDialog* pDialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&pDialog));
    if (SUCCEEDED(hr)) {
        DWORD options = 0;
        pDialog->GetOptions(&options);
        pDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

        std::wstring wTitle(title.begin(), title.end());
        pDialog->SetTitle(wTitle.c_str());

        if (!defaultPath.empty()) {
            std::wstring wPath(defaultPath.begin(), defaultPath.end());
            IShellItem* pFolder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(wPath.c_str(), nullptr,
                          IID_IShellItem, reinterpret_cast<void**>(&pFolder)))) {
                pDialog->SetFolder(pFolder);
                pFolder->Release();
            }
        }

        // NULL owner — avoids modal deadlock with GLFW/Vulkan window
        hr = pDialog->Show(NULL);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pDialog->GetResult(&pItem))) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        result.resize(len - 1);
                        WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, result.data(), len, nullptr, nullptr);
                    }
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        pDialog->Release();
    }

    return result;
}

#elif defined(__APPLE__)
// macOS implementation using osascript

#include <cstdio>
#include <array>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

// Shell-escape a string for safe interpolation into single-quoted shell arguments.
// Wraps the string in single quotes and escapes any embedded single quotes.
static std::string ShellEscape(const std::string& s) {
    std::string result = "'";
    for (char c : s) {
        if (c == '\'') result += "'\\''";
        else result += c;
    }
    result += "'";
    return result;
}

// S-C1: Use posix_spawn with pipe to capture output (avoids popen shell injection)
static std::string ExecuteCommand(const std::string& cmd) {
    std::string result;
    int pipefd[2];
    if (pipe(pipefd) != 0) return "";

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipefd[0]);

    const char* argv[] = { "/bin/sh", "-c", cmd.c_str(), nullptr };
    pid_t pid = 0;
    extern char** environ;
    int spawnResult = posix_spawnp(&pid, "/bin/sh", &actions, nullptr,
                                   const_cast<char**>(argv), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);

    if (spawnResult == 0) {
        char buffer[4096];
        ssize_t n;
        while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';
            result += buffer;
        }
        int status = 0;
        waitpid(pid, &status, 0);
    }
    close(pipefd[0]);

    // Remove trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

std::string FileDialog::OpenFile(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& defaultPath
) {
    // Build osascript AppleScript command with shell-escaped user strings
    std::string script = "POSIX path of (choose file";
    if (!title.empty()) {
        // Escape double quotes inside the AppleScript string
        std::string safeTitle = title;
        for (size_t p = safeTitle.find('"'); p != std::string::npos; p = safeTitle.find('"', p + 2))
            safeTitle.replace(p, 1, "\\\"");
        script += " with prompt \"" + safeTitle + "\"";
    }
    if (!filters.empty()) {
        script += " of type {";
        bool first = true;
        for (const auto& filter : filters) {
            // Extract extensions (e.g., "*.gltf;*.glb" -> "gltf", "glb")
            std::string ext = filter.extensions;
            size_t pos = 0;
            while (pos < ext.size()) {
                size_t star = ext.find('*', pos);
                if (star == std::string::npos) break;
                size_t dot = ext.find('.', star);
                if (dot == std::string::npos) break;
                size_t end = ext.find_first_of(";* ", dot + 1);
                std::string e = ext.substr(dot + 1, end != std::string::npos ? end - dot - 1 : std::string::npos);
                if (!e.empty()) {
                    if (!first) script += ", ";
                    // Only allow alphanumeric extension chars to prevent injection
                    bool safe = true;
                    for (char c : e) { if (!std::isalnum(static_cast<unsigned char>(c))) { safe = false; break; } }
                    if (safe) script += "\"" + e + "\"";
                    first = false;
                }
                pos = end != std::string::npos ? end + 1 : ext.size();
            }
        }
        script += "}";
    }
    if (!defaultPath.empty()) {
        std::string safePath = defaultPath;
        for (size_t p = safePath.find('"'); p != std::string::npos; p = safePath.find('"', p + 2))
            safePath.replace(p, 1, "\\\"");
        script += " default location POSIX file \"" + safePath + "\"";
    }
    script += ")";
    std::string cmd = "osascript -e " + ShellEscape(script) + " 2>/dev/null";
    return ExecuteCommand(cmd);
}

std::string FileDialog::SaveFile(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& defaultPath,
    const std::string& defaultName
) {
    // Build osascript AppleScript command with shell-escaped user strings
    std::string script = "POSIX path of (choose file name";
    auto escapeAS = [](const std::string& s) {
        std::string out = s;
        for (size_t p = out.find('"'); p != std::string::npos; p = out.find('"', p + 2))
            out.replace(p, 1, "\\\"");
        return out;
    };
    if (!title.empty()) {
        script += " with prompt \"" + escapeAS(title) + "\"";
    }
    if (!defaultName.empty()) {
        script += " default name \"" + escapeAS(defaultName) + "\"";
    }
    if (!defaultPath.empty()) {
        script += " default location POSIX file \"" + escapeAS(defaultPath) + "\"";
    }
    script += ")";
    std::string cmd = "osascript -e " + ShellEscape(script) + " 2>/dev/null";
    (void)filters;
    return ExecuteCommand(cmd);
}

std::string FileDialog::OpenFolder(
    const std::string& title,
    const std::string& defaultPath
) {
    // Build osascript AppleScript command with shell-escaped user strings
    std::string script = "POSIX path of (choose folder";
    auto escapeAS = [](const std::string& s) {
        std::string out = s;
        for (size_t p = out.find('"'); p != std::string::npos; p = out.find('"', p + 2))
            out.replace(p, 1, "\\\"");
        return out;
    };
    if (!title.empty()) {
        script += " with prompt \"" + escapeAS(title) + "\"";
    }
    if (!defaultPath.empty()) {
        script += " default location POSIX file \"" + escapeAS(defaultPath) + "\"";
    }
    script += ")";
    std::string cmd = "osascript -e " + ShellEscape(script) + " 2>/dev/null";
    return ExecuteCommand(cmd);
}

#else
// Linux implementation using zenity (GTK dialog) or kdialog (KDE)

#include <cstdio>
#include <cstdlib>
#include <array>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

// Shell-escape a string for safe interpolation into single-quoted shell arguments.
// Wraps the string in single quotes and escapes any embedded single quotes.
static std::string ShellEscape(const std::string& s) {
    std::string result = "'";
    for (char c : s) {
        if (c == '\'') result += "'\\''";
        else result += c;
    }
    result += "'";
    return result;
}

// S-C1: Use posix_spawn with pipe to capture output (avoids popen shell injection)
static std::string ExecuteCommand(const std::string& cmd) {
    std::string result;
    int pipefd[2];
    if (pipe(pipefd) != 0) return "";

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipefd[0]);

    const char* argv[] = { "/bin/sh", "-c", cmd.c_str(), nullptr };
    pid_t pid = 0;
    extern char** environ;
    int spawnResult = posix_spawnp(&pid, "/bin/sh", &actions, nullptr,
                                   const_cast<char**>(argv), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);

    if (spawnResult == 0) {
        char buffer[4096];
        ssize_t n;
        while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';
            result += buffer;
        }
        int status = 0;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            close(pipefd[0]);
            return ""; // User cancelled or error
        }
    }
    close(pipefd[0]);

    // Remove trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

static bool HasCommand(const char* cmd) {
    // Validate command name contains only safe characters (alphanumeric, dash, underscore)
    for (const char* p = cmd; *p; ++p) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            return false;
        }
    }
    // S-C1: Use posix_spawn instead of std::system
    std::string which = "which " + std::string(cmd) + " >/dev/null 2>&1";
    const char* argv[] = { "/bin/sh", "-c", which.c_str(), nullptr };
    pid_t pid = 0;
    extern char** environ;
    if (posix_spawnp(&pid, "/bin/sh", nullptr, nullptr, const_cast<char**>(argv), environ) != 0) return false;
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::string FileDialog::OpenFile(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& defaultPath
) {
    if (HasCommand("zenity")) {
        std::string cmd = "zenity --file-selection";
        if (!title.empty()) cmd += " --title=" + ShellEscape(title);
        if (!defaultPath.empty()) cmd += " --filename=" + ShellEscape(defaultPath + "/");
        for (const auto& filter : filters) {
            cmd += " --file-filter=" + ShellEscape(filter.name + " | " + filter.extensions);
        }
        cmd += " 2>/dev/null";
        return ExecuteCommand(cmd);
    }
    if (HasCommand("kdialog")) {
        std::string cmd = "kdialog --getopenfilename";
        cmd += " " + ShellEscape(defaultPath.empty() ? "." : defaultPath);
        if (!filters.empty()) {
            std::string filterStr;
            for (const auto& filter : filters) {
                filterStr += filter.extensions + " ";
            }
            cmd += " " + ShellEscape(filterStr);
        }
        if (!title.empty()) cmd += " --title " + ShellEscape(title);
        cmd += " 2>/dev/null";
        return ExecuteCommand(cmd);
    }
    return "";
}

std::string FileDialog::SaveFile(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& defaultPath,
    const std::string& defaultName
) {
    if (HasCommand("zenity")) {
        std::string cmd = "zenity --file-selection --save --confirm-overwrite";
        if (!title.empty()) cmd += " --title=" + ShellEscape(title);
        std::string initPath = defaultPath.empty() ? "." : defaultPath;
        if (!defaultName.empty()) initPath += "/" + defaultName;
        cmd += " --filename=" + ShellEscape(initPath);
        for (const auto& filter : filters) {
            cmd += " --file-filter=" + ShellEscape(filter.name + " | " + filter.extensions);
        }
        cmd += " 2>/dev/null";
        return ExecuteCommand(cmd);
    }
    if (HasCommand("kdialog")) {
        std::string cmd = "kdialog --getsavefilename";
        std::string initPath = defaultPath.empty() ? "." : defaultPath;
        if (!defaultName.empty()) initPath += "/" + defaultName;
        cmd += " " + ShellEscape(initPath);
        if (!title.empty()) cmd += " --title " + ShellEscape(title);
        cmd += " 2>/dev/null";
        return ExecuteCommand(cmd);
    }
    return "";
}

std::string FileDialog::OpenFolder(
    const std::string& title,
    const std::string& defaultPath
) {
    if (HasCommand("zenity")) {
        std::string cmd = "zenity --file-selection --directory";
        if (!title.empty()) cmd += " --title=" + ShellEscape(title);
        if (!defaultPath.empty()) cmd += " --filename=" + ShellEscape(defaultPath + "/");
        cmd += " 2>/dev/null";
        return ExecuteCommand(cmd);
    }
    if (HasCommand("kdialog")) {
        std::string cmd = "kdialog --getexistingdirectory";
        cmd += " " + ShellEscape(defaultPath.empty() ? "." : defaultPath);
        if (!title.empty()) cmd += " --title " + ShellEscape(title);
        cmd += " 2>/dev/null";
        return ExecuteCommand(cmd);
    }
    return "";
}

#endif

} // namespace Enjin
