#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/GUI/DialogueTree.h"
#include <string>
#include <vector>

namespace Enjin {
namespace GUI {

// Result of an import/export operation
struct DialogueIOResult {
    bool success = false;
    std::string errorMessage;
    u32 nodeCount = 0;
    u32 linkCount = 0;
    std::vector<std::string> warnings;
};

// ============================================================================
// YARN SPINNER (.yarn) IMPORT/EXPORT
// ============================================================================
// Yarn Spinner format: plain text with header-based node separation
// Supports: titles, body text, choices (->), conditionals (<<if>>), variables (<<set>>),
//           commands (<<command>>), jumps (<<jump>>)

class ENJIN_API YarnSpinnerIO {
public:
    // Import a .yarn file into a DialogueTreeData
    static DialogueIOResult Import(const std::string& filepath, DialogueTreeData& outTree);

    // Import from a string (for clipboard/embedded use)
    static DialogueIOResult ImportFromString(const std::string& yarnText, DialogueTreeData& outTree);

    // Export a DialogueTreeData to .yarn format
    static DialogueIOResult Export(const std::string& filepath, const DialogueTreeData& tree);

    // Export to string
    static std::string ExportToString(const DialogueTreeData& tree);

private:
    // Parse a single Yarn node block
    struct YarnNodeBlock {
        std::string title;
        std::string tags;
        std::string body;
        u32 lineNumber = 0;
    };

    static std::vector<YarnNodeBlock> ParseBlocks(const std::string& text);
    static void ConvertBlockToNodes(const YarnNodeBlock& block, DialogueTreeData& tree,
                                    std::unordered_map<std::string, u32>& titleToNodeId,
                                    DialogueIOResult& result);
    static std::string StripTags(const std::string& line);
};

// ============================================================================
// TWINE (.html / .twee) IMPORT/EXPORT
// ============================================================================
// Twine format: passage-based with wiki-style links
// Supports Twine 2 HTML (.html) and Twee 3 (.twee/.tw) plain-text format
// Links: [[text->target]], [[text|target]], [[target]]
// Variables: (set: $var to value), (if: $var is value)

class ENJIN_API TwineIO {
public:
    // Import a Twine HTML or Twee file
    static DialogueIOResult Import(const std::string& filepath, DialogueTreeData& outTree);

    // Import from string (detects HTML vs Twee format)
    static DialogueIOResult ImportFromString(const std::string& text, DialogueTreeData& outTree);

    // Export to Twee 3 format (.twee)
    static DialogueIOResult Export(const std::string& filepath, const DialogueTreeData& tree);

    // Export to string (Twee format)
    static std::string ExportToString(const DialogueTreeData& tree);

private:
    // Twine passage representation
    struct TwinePassage {
        std::string name;
        std::string tags;
        std::string body;
        std::vector<std::pair<std::string, std::string>> links; // display text, target passage
    };

    // HTML parser (Twine 2 export)
    static std::vector<TwinePassage> ParseHTML(const std::string& html);

    // Twee 3 parser
    static std::vector<TwinePassage> ParseTwee(const std::string& twee);

    // Detect format
    static bool IsHTML(const std::string& text);

    // Convert passages to dialogue nodes
    static void ConvertPassagesToTree(const std::vector<TwinePassage>& passages,
                                      DialogueTreeData& tree, DialogueIOResult& result);

    // Parse Twine links from text: [[text->target]], [[text|target]], [[target]]
    static std::vector<std::pair<std::string, std::string>> ExtractLinks(const std::string& text);

    // Remove link syntax from body text
    static std::string StripLinks(const std::string& text);

    // Parse Harlowe/SugarCube macros
    static bool ParseSetMacro(const std::string& line, std::string& varName, std::string& varValue);
    static bool ParseIfMacro(const std::string& line, std::string& varName,
                             DialogueCondition::Op& op, std::string& value);
};

} // namespace GUI
} // namespace Enjin
