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
    // Set the owner window handle for all subsequent dialogs.
    // On Windows this is an HWND — ensures the dialog appears on top of the
    // application window and prevents the OS from marking it "Not Responding".
    static void SetOwnerWindow(void* handle);

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

    // Whether a native file dialog can actually be shown.
    //
    // All three calls above return an empty string when the user cancels. On a
    // Linux desktop with no dialog helper installed they return an empty string
    // too, and the caller cannot tell the difference: the button appears to do
    // nothing. Check this before offering a dialog, and say so plainly when it
    // is false rather than opening one that will never appear.
    //
    // Always true on Windows and macOS, whose dialogs are part of the OS.
    static bool IsAvailable();

private:
    static void* s_OwnerWindow;
};

} // namespace Enjin
