#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/GUI/DialogueTree.h"
#include <string>

namespace Enjin {
namespace GUI {

// Dialogue asset file (.enjdlg) - standalone dialogue tree files
class ENJIN_API DialogueAsset {
public:
    // Save dialogue tree to .enjdlg file
    static bool Save(const std::string& filepath, const DialogueTreeData& tree);

    // Load dialogue tree from .enjdlg file
    static bool Load(const std::string& filepath, DialogueTreeData& outTree);

    // Get last error message
    static const std::string& GetLastError();

private:
    static std::string s_LastError;
};

} // namespace GUI
} // namespace Enjin
