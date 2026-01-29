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

#else
// Non-Windows stub implementations
std::string FileDialog::OpenFile(const std::string&, const std::vector<FileFilter>&, const std::string&) {
    return "";
}

std::string FileDialog::SaveFile(const std::string&, const std::vector<FileFilter>&, const std::string&, const std::string&) {
    return "";
}

std::string FileDialog::OpenFolder(const std::string&, const std::string&) {
    return "";
}
#endif

} // namespace Enjin
