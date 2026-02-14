#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace Scripting {

// ============================================================================
// ActionScript 2/3 → AngelScript Transpiler
// ============================================================================
// Converts common ActionScript patterns to AngelScript equivalents.
// This is a source-to-source transpiler that handles:
//
// AS2 patterns:
//   onClipEvent(enterFrame) → void OnUpdate(float dt)
//   _root.gotoAndPlay(5)    → Animator_GotoAndPlay(entity, 5)
//   this._x / this._y       → Transform_GetPosition(entity).x / .y
//   Key.isDown(Key.LEFT)    → Input_IsKeyDown(KEY_LEFT)
//   Mouse._xmouse           → Input_GetMouseX()
//
// AS3 patterns:
//   addEventListener("enterFrame", onTick) → Events_Listen("enterFrame", "onTick")
//   stage.addChild(mc)      → Entity_SetParent(mc, stageEntity)
//   mc.gotoAndPlay("walk")  → Animator_Play(mc, "walk")
//   new Sound()             → Audio_CreateSound()
//   SharedObject.getLocal() → Save_GetSlot()
//
// Limitations:
//   - No bytecode decompilation (source-level only)
//   - Class inheritance flattened to composition
//   - Dynamic typing converted to explicit casts
//   - eval() and complex reflection not supported

// --- Transpiler configuration ---

enum class ASVersion : u8 {
    AS2,    // ActionScript 2.0 (onClipEvent style)
    AS3,    // ActionScript 3.0 (addEventListener, package/class style)
    Auto    // Auto-detect from syntax
};

struct TranspilerConfig {
    ASVersion sourceVersion = ASVersion::Auto;
    bool preserveComments = true;
    bool addTypeAnnotations = true;
    bool generateEventHooks = true;     // Auto-create OnUpdate/OnStart wrappers
    bool wrapInClass = false;           // Wrap output in a TegeBehavior class
    std::string className = "FlashBehavior";
    bool verbose = false;               // Add conversion notes as comments
};

// --- Transpiler result ---

struct TranspilerWarning {
    u32 line = 0;
    std::string message;
    std::string originalCode;
};

struct TranspileResult {
    bool success = false;
    std::string errorMessage;
    std::string angelScript;          // The transpiled AngelScript code
    u32 linesProcessed = 0;
    u32 patternsMatched = 0;
    u32 unsupportedConstructs = 0;
    std::vector<TranspilerWarning> warnings;
    std::vector<std::string> requiredImports;  // AngelScript #includes needed
};

// --- API mapping entry ---

struct APIMapping {
    std::string asPattern;       // Regex pattern to match in AS code
    std::string angelReplacement; // AngelScript replacement (with capture group refs)
    std::string description;
    bool isAS2Only = false;
    bool isAS3Only = false;
};

// ============================================================================
// AS3Transpiler class
// ============================================================================

class ENJIN_API AS3Transpiler {
public:
    AS3Transpiler();

    // Transpile ActionScript source to AngelScript
    TranspileResult Transpile(const std::string& asSource, const TranspilerConfig& config = {});

    // Transpile a single file
    TranspileResult TranspileFile(const std::string& inputPath,
                                   const TranspilerConfig& config = {});

    // Transpile and write output to file
    bool TranspileToFile(const std::string& inputPath, const std::string& outputPath,
                         const TranspilerConfig& config = {});

    // Get the default API mapping table
    const std::vector<APIMapping>& GetAPIMappings() const { return m_Mappings; }

    // Add a custom API mapping
    void AddMapping(const APIMapping& mapping);

    // Get a preview of what would be transpiled (first N lines)
    std::string Preview(const std::string& asSource, u32 maxLines = 20);

private:
    std::vector<APIMapping> m_Mappings;

    // Auto-detect AS2 vs AS3
    ASVersion DetectVersion(const std::string& source);

    // Pre-processing
    std::string StripComments(const std::string& source,
                               std::vector<std::pair<u32, std::string>>& comments);
    std::string NormalizeWhitespace(const std::string& source);

    // AS2-specific transforms
    std::string TransformAS2Events(const std::string& source,
                                    std::vector<TranspilerWarning>& warnings);
    std::string TransformAS2Properties(const std::string& source,
                                        std::vector<TranspilerWarning>& warnings);

    // AS3-specific transforms
    std::string TransformAS3Classes(const std::string& source,
                                     std::vector<TranspilerWarning>& warnings);
    std::string TransformAS3Events(const std::string& source,
                                    std::vector<TranspilerWarning>& warnings);
    std::string TransformAS3Imports(const std::string& source,
                                     std::vector<std::string>& requiredImports);

    // Common transforms
    std::string ApplyAPIMappings(const std::string& source, u32& matchCount);
    std::string TransformTypes(const std::string& source);
    std::string TransformLoops(const std::string& source);
    std::string TransformStringOps(const std::string& source);
    std::string WrapInBehavior(const std::string& source, const std::string& className);

    // Array literal transforms: [1,2,3] → array<int> = {1,2,3}
    std::string TransformArrayLiterals(const std::string& source);

    // Helpers
    void InitDefaultMappings();
    u32 CountLines(const std::string& s);
};

// Type alias for task-level naming compatibility
using ActionScriptTranspiler = AS3Transpiler;

} // namespace Scripting
} // namespace Enjin
