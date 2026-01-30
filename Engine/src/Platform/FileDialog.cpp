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
    ofn.hwndOwner = nullptr;
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
    ofn.hwndOwner = nullptr;
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
    char path[MAX_PATH] = "";

    BROWSEINFOA bi = {};
    bi.hwndOwner = nullptr;
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != nullptr) {
        if (SHGetPathFromIDListA(pidl, path)) {
            CoTaskMemFree(pidl);
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }

    return "";
}

#elif defined(__APPLE__)
// macOS implementation using osascript

#include <cstdio>
#include <array>

static std::string ExecuteCommand(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
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
    std::string cmd = "osascript -e 'POSIX path of (choose file";
    if (!title.empty()) {
        cmd += " with prompt \"" + title + "\"";
    }
    if (!filters.empty()) {
        cmd += " of type {";
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
                    if (!first) cmd += ", ";
                    cmd += "\"" + e + "\"";
                    first = false;
                }
                pos = end != std::string::npos ? end + 1 : ext.size();
            }
        }
        cmd += "}";
    }
    if (!defaultPath.empty()) {
        cmd += " default location POSIX file \"" + defaultPath + "\"";
    }
    cmd += ")' 2>/dev/null";
    return ExecuteCommand(cmd);
}

std::string FileDialog::SaveFile(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& defaultPath,
    const std::string& defaultName
) {
    std::string cmd = "osascript -e 'POSIX path of (choose file name";
    if (!title.empty()) {
        cmd += " with prompt \"" + title + "\"";
    }
    if (!defaultName.empty()) {
        cmd += " default name \"" + defaultName + "\"";
    }
    if (!defaultPath.empty()) {
        cmd += " default location POSIX file \"" + defaultPath + "\"";
    }
    cmd += ")' 2>/dev/null";
    (void)filters;
    return ExecuteCommand(cmd);
}

std::string FileDialog::OpenFolder(
    const std::string& title,
    const std::string& defaultPath
) {
    std::string cmd = "osascript -e 'POSIX path of (choose folder";
    if (!title.empty()) {
        cmd += " with prompt \"" + title + "\"";
    }
    if (!defaultPath.empty()) {
        cmd += " default location POSIX file \"" + defaultPath + "\"";
    }
    cmd += ")' 2>/dev/null";
    return ExecuteCommand(cmd);
}

#else
// Linux implementation using zenity (GTK dialog) or kdialog (KDE)

#include <cstdio>
#include <cstdlib>
#include <array>

static std::string ExecuteCommand(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        result += buffer.data();
    }
    int status = pclose(pipe);
    if (status != 0) return ""; // User cancelled
    // Remove trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

static bool HasCommand(const char* cmd) {
    std::string check = std::string("which ") + cmd + " >/dev/null 2>&1";
    return std::system(check.c_str()) == 0;
}

std::string FileDialog::OpenFile(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& defaultPath
) {
    if (HasCommand("zenity")) {
        std::string cmd = "zenity --file-selection";
        if (!title.empty()) cmd += " --title=\"" + title + "\"";
        if (!defaultPath.empty()) cmd += " --filename=\"" + defaultPath + "/\"";
        for (const auto& filter : filters) {
            cmd += " --file-filter=\"" + filter.name + " | " + filter.extensions + "\"";
        }
        cmd += " 2>/dev/null";
        return ExecuteCommand(cmd);
    }
    if (HasCommand("kdialog")) {
        std::string cmd = "kdialog --getopenfilename";
        cmd += " \"" + (defaultPath.empty() ? "." : defaultPath) + "\"";
        if (!filters.empty()) {
            cmd += " \"";
            for (const auto& filter : filters) {
                cmd += filter.extensions + " ";
            }
            cmd += "\"";
        }
        if (!title.empty()) cmd += " --title \"" + title + "\"";
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
        if (!title.empty()) cmd += " --title=\"" + title + "\"";
        std::string initPath = defaultPath.empty() ? "." : defaultPath;
        if (!defaultName.empty()) initPath += "/" + defaultName;
        cmd += " --filename=\"" + initPath + "\"";
        for (const auto& filter : filters) {
            cmd += " --file-filter=\"" + filter.name + " | " + filter.extensions + "\"";
        }
        cmd += " 2>/dev/null";
        return ExecuteCommand(cmd);
    }
    if (HasCommand("kdialog")) {
        std::string cmd = "kdialog --getsavefilename";
        std::string initPath = defaultPath.empty() ? "." : defaultPath;
        if (!defaultName.empty()) initPath += "/" + defaultName;
        cmd += " \"" + initPath + "\"";
        if (!title.empty()) cmd += " --title \"" + title + "\"";
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
        if (!title.empty()) cmd += " --title=\"" + title + "\"";
        if (!defaultPath.empty()) cmd += " --filename=\"" + defaultPath + "/\"";
        cmd += " 2>/dev/null";
        return ExecuteCommand(cmd);
    }
    if (HasCommand("kdialog")) {
        std::string cmd = "kdialog --getexistingdirectory";
        cmd += " \"" + (defaultPath.empty() ? "." : defaultPath) + "\"";
        if (!title.empty()) cmd += " --title \"" + title + "\"";
        cmd += " 2>/dev/null";
        return ExecuteCommand(cmd);
    }
    return "";
}

#endif

} // namespace Enjin
