#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <functional>

namespace Enjin {
namespace Editor {

// A registered command in the palette
struct PaletteCommand {
    std::string name;           // Display name (e.g., "Create Empty Entity")
    std::string category;       // Category for grouping (e.g., "Entity", "Scene", "View")
    std::string shortcut;       // Display shortcut hint (e.g., "Ctrl+Shift+N")
    std::string description;    // Brief description for screen readers
    std::function<void()> action; // Callback when executed
};

// Command palette for keyboard-driven and screen-reader-accessible editing
// Invoked via Ctrl+P, provides fuzzy-search over all registered commands
class ENJIN_API CommandPalette {
public:
    CommandPalette() = default;
    ~CommandPalette() = default;

    // Register a command
    void RegisterCommand(const PaletteCommand& cmd);

    // Clear all registered commands (call before re-registering)
    void ClearCommands();

    // Open/close the palette
    void Open();
    void Close();
    bool IsOpen() const { return m_Open; }
    void Toggle() { if (m_Open) Close(); else Open(); }

    // Render the palette popup (call each frame from editor)
    // Returns true if a command was executed
    bool Render();

    // Get the last executed command name (for announcer/screen reader)
    const std::string& GetLastExecutedCommand() const { return m_LastExecuted; }

    // Get current filtered results count
    usize GetFilteredCount() const { return m_Filtered.size(); }

private:
    std::vector<PaletteCommand> m_Commands;
    std::vector<usize> m_Filtered;       // Indices into m_Commands
    char m_SearchBuf[256] = {};
    i32 m_SelectedIndex = 0;
    bool m_Open = false;
    bool m_JustOpened = false;
    std::string m_LastExecuted;

    void UpdateFilter();
    bool FuzzyMatch(const std::string& text, const std::string& query) const;
    i32 FuzzyScore(const std::string& text, const std::string& query) const;
};

} // namespace Editor
} // namespace Enjin
