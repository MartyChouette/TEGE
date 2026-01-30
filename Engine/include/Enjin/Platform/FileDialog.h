#pragma once

#include "Enjin/Platform/Platform.h"
#include <string>
#include <vector>

namespace Enjin {

// File filter for dialog (e.g., "Scene Files", "*.scene;*.json")
struct FileFilter {
    std::string name;        // Display name (e.g., "Scene Files")
    std::string extensions;  // Extensions (e.g., "*.scene;*.json")
};

// Native file dialog utilities
class ENJIN_API FileDialog {
public:
    // Open a file dialog and return the selected path (empty if cancelled)
    static std::string OpenFile(
        const std::string& title = "Open File",
        const std::vector<FileFilter>& filters = {},
        const std::string& defaultPath = ""
    );

    // Save file dialog and return the selected path (empty if cancelled)
    static std::string SaveFile(
        const std::string& title = "Save File",
        const std::vector<FileFilter>& filters = {},
        const std::string& defaultPath = "",
        const std::string& defaultName = ""
    );

    // Open folder dialog and return the selected path (empty if cancelled)
    static std::string OpenFolder(
        const std::string& title = "Select Folder",
        const std::string& defaultPath = ""
    );
};

} // namespace Enjin
