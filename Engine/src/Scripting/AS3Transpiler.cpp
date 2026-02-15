#include "Enjin/Scripting/AS3Transpiler.h"
#include "Enjin/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <filesystem>

namespace Enjin {
namespace Scripting {

// ============================================================================
// Constructor — initialize default mappings
// ============================================================================

AS3Transpiler::AS3Transpiler() {
    InitDefaultMappings();
}

void AS3Transpiler::InitDefaultMappings() {
    m_Mappings.clear();

    // --- MovieClip / display object mappings ---
    m_Mappings.push_back({
        "(\\w+)\\.gotoAndPlay\\((\\d+)\\)",
        "Animator_GotoAndPlay($1_entity, $2)",
        "MovieClip.gotoAndPlay(frame) -> Animator_GotoAndPlay"
    });
    m_Mappings.push_back({
        "(\\w+)\\.gotoAndStop\\((\\d+)\\)",
        "Animator_GotoAndStop($1_entity, $2)",
        "MovieClip.gotoAndStop(frame) -> Animator_GotoAndStop"
    });
    m_Mappings.push_back({
        "(\\w+)\\.gotoAndPlay\\(\"(\\w+)\"\\)",
        "Animator_Play($1_entity, \"$2\")",
        "MovieClip.gotoAndPlay(label) -> Animator_Play"
    });
    m_Mappings.push_back({
        "(\\w+)\\.gotoAndStop\\(\"(\\w+)\"\\)",
        "Animator_PlayAndStop($1_entity, \"$2\")",
        "MovieClip.gotoAndStop(label) -> Animator_PlayAndStop"
    });
    m_Mappings.push_back({
        "(\\w+)\\.play\\(\\)",
        "Animator_Resume($1_entity)",
        "MovieClip.play() -> Animator_Resume"
    });
    m_Mappings.push_back({
        "(\\w+)\\.stop\\(\\)",
        "Animator_Pause($1_entity)",
        "MovieClip.stop() -> Animator_Pause"
    });

    // --- Position / transform ---
    m_Mappings.push_back({
        "(\\w+)\\._x\\b",
        "Transform_GetPosition($1_entity).x",
        "mc._x -> Transform_GetPosition().x", true, false
    });
    m_Mappings.push_back({
        "(\\w+)\\._y\\b",
        "Transform_GetPosition($1_entity).y",
        "mc._y -> Transform_GetPosition().y", true, false
    });
    m_Mappings.push_back({
        "(\\w+)\\.x\\b",
        "Transform_GetPosition($1_entity).x",
        "mc.x -> Transform_GetPosition().x", false, true
    });
    m_Mappings.push_back({
        "(\\w+)\\.y\\b",
        "Transform_GetPosition($1_entity).y",
        "mc.y -> Transform_GetPosition().y", false, true
    });
    m_Mappings.push_back({
        "(\\w+)\\._xscale\\b",
        "(Transform_GetScale($1_entity).x * 100.0f)",
        "mc._xscale -> Transform_GetScale().x * 100", true, false
    });
    m_Mappings.push_back({
        "(\\w+)\\._yscale\\b",
        "(Transform_GetScale($1_entity).y * 100.0f)",
        "mc._yscale -> Transform_GetScale().y * 100", true, false
    });
    m_Mappings.push_back({
        "(\\w+)\\._rotation\\b",
        "Transform_GetRotationZ($1_entity)",
        "mc._rotation -> Transform_GetRotationZ", true, false
    });
    m_Mappings.push_back({
        "(\\w+)\\._alpha\\b",
        "(Sprite_GetAlpha($1_entity) * 100.0f)",
        "mc._alpha -> Sprite_GetAlpha * 100", true, false
    });
    m_Mappings.push_back({
        "(\\w+)\\._visible\\b",
        "Entity_IsVisible($1_entity)",
        "mc._visible -> Entity_IsVisible", true, false
    });

    // --- Input ---
    m_Mappings.push_back({
        "Key\\.isDown\\(Key\\.LEFT\\)",
        "Input_IsKeyDown(KEY_LEFT)",
        "Key.isDown(Key.LEFT) -> Input_IsKeyDown", true, false
    });
    m_Mappings.push_back({
        "Key\\.isDown\\(Key\\.RIGHT\\)",
        "Input_IsKeyDown(KEY_RIGHT)",
        "Key.isDown(Key.RIGHT) -> Input_IsKeyDown", true, false
    });
    m_Mappings.push_back({
        "Key\\.isDown\\(Key\\.UP\\)",
        "Input_IsKeyDown(KEY_UP)",
        "Key.isDown(Key.UP) -> Input_IsKeyDown", true, false
    });
    m_Mappings.push_back({
        "Key\\.isDown\\(Key\\.DOWN\\)",
        "Input_IsKeyDown(KEY_DOWN)",
        "Key.isDown(Key.DOWN) -> Input_IsKeyDown", true, false
    });
    m_Mappings.push_back({
        "Key\\.isDown\\(Key\\.SPACE\\)",
        "Input_IsKeyDown(KEY_SPACE)",
        "Key.isDown(Key.SPACE) -> Input_IsKeyDown", true, false
    });

    // --- Mouse ---
    m_Mappings.push_back({
        "_xmouse\\b",
        "Input_GetMouseX()",
        "_xmouse -> Input_GetMouseX()", true, false
    });
    m_Mappings.push_back({
        "_ymouse\\b",
        "Input_GetMouseY()",
        "_ymouse -> Input_GetMouseY()", true, false
    });
    m_Mappings.push_back({
        "Mouse\\.hide\\(\\)",
        "Input_HideCursor()",
        "Mouse.hide() -> Input_HideCursor()"
    });
    m_Mappings.push_back({
        "Mouse\\.show\\(\\)",
        "Input_ShowCursor()",
        "Mouse.show() -> Input_ShowCursor()"
    });

    // --- Events (AS3) ---
    m_Mappings.push_back({
        "addEventListener\\(\"(\\w+)\",\\s*(\\w+)\\)",
        "Events_Listen(\"$1\", \"$2\")",
        "addEventListener -> Events_Listen", false, true
    });
    m_Mappings.push_back({
        "removeEventListener\\(\"(\\w+)\",\\s*(\\w+)\\)",
        "Events_Unlisten(\"$1\", \"$2\")",
        "removeEventListener -> Events_Unlisten", false, true
    });
    m_Mappings.push_back({
        "dispatchEvent\\(new\\s+Event\\(\"(\\w+)\"\\)\\)",
        "Events_Fire(\"$1\")",
        "dispatchEvent -> Events_Fire", false, true
    });

    // --- Display list (AS3) ---
    m_Mappings.push_back({
        "addChild\\((\\w+)\\)",
        "Entity_SetParent($1_entity, this_entity)",
        "addChild -> Entity_SetParent", false, true
    });
    m_Mappings.push_back({
        "removeChild\\((\\w+)\\)",
        "Entity_RemoveParent($1_entity)",
        "removeChild -> Entity_RemoveParent", false, true
    });
    m_Mappings.push_back({
        "stage\\.stageWidth\\b",
        "Stage_GetWidth()",
        "stage.stageWidth -> Stage_GetWidth()", false, true
    });
    m_Mappings.push_back({
        "stage\\.stageHeight\\b",
        "Stage_GetHeight()",
        "stage.stageHeight -> Stage_GetHeight()", false, true
    });

    // --- Audio ---
    m_Mappings.push_back({
        "new Sound\\(\\)",
        "Audio_CreateSound()",
        "new Sound() -> Audio_CreateSound()"
    });
    m_Mappings.push_back({
        "(\\w+)\\.attachSound\\(\"(\\w+)\"\\)",
        "Audio_LoadSound($1, \"$2\")",
        "attachSound -> Audio_LoadSound", true, false
    });

    // --- SharedObject (mapped to Flash_SO_* functions backed by TieredSaveSystem) ---
    m_Mappings.push_back({
        "(\\w+)\\.data\\.(\\w+)\\s*=\\s*\"([^\"]+)\"",
        "Flash_SO_Set(\"$1\", \"$2\", \"$3\")",
        "SharedObject.data.key = value -> Flash_SO_Set"
    });
    m_Mappings.push_back({
        "(\\w+)\\.data\\.(\\w+)",
        "Flash_SO_Get(\"$1\", \"$2\")",
        "SharedObject.data.key -> Flash_SO_Get"
    });
    m_Mappings.push_back({
        "(\\w+)\\.flush\\(\\)",
        "Flash_SO_Flush(\"$1\")",
        "SharedObject.flush -> Flash_SO_Flush"
    });

    // --- Math ---
    m_Mappings.push_back({
        "Math\\.random\\(\\)",
        "Math_Random()",
        "Math.random() -> Math_Random()"
    });
    m_Mappings.push_back({
        "Math\\.floor\\(",
        "Math_Floor(",
        "Math.floor -> Math_Floor"
    });
    m_Mappings.push_back({
        "Math\\.ceil\\(",
        "Math_Ceil(",
        "Math.ceil -> Math_Ceil"
    });
    m_Mappings.push_back({
        "Math\\.abs\\(",
        "Math_Abs(",
        "Math.abs -> Math_Abs"
    });
    m_Mappings.push_back({
        "Math\\.sqrt\\(",
        "Math_Sqrt(",
        "Math.sqrt -> Math_Sqrt"
    });
    m_Mappings.push_back({
        "Math\\.atan2\\(",
        "Math_Atan2(",
        "Math.atan2 -> Math_Atan2"
    });
    m_Mappings.push_back({
        "Math\\.round\\(",
        "Math_Round(",
        "Math.round -> Math_Round"
    });
    m_Mappings.push_back({
        "Math\\.sin\\(",
        "Math_Sin(",
        "Math.sin -> Math_Sin"
    });
    m_Mappings.push_back({
        "Math\\.cos\\(",
        "Math_Cos(",
        "Math.cos -> Math_Cos"
    });
    m_Mappings.push_back({
        "Math\\.min\\(",
        "Math_Min(",
        "Math.min -> Math_Min"
    });
    m_Mappings.push_back({
        "Math\\.max\\(",
        "Math_Max(",
        "Math.max -> Math_Max"
    });
    m_Mappings.push_back({
        "Math\\.pow\\(",
        "Math_Pow(",
        "Math.pow -> Math_Pow"
    });
    m_Mappings.push_back({
        "Math\\.PI\\b",
        "3.14159265f",
        "Math.PI -> float literal"
    });

    // --- Flash API shim functions (MovieClip) ---
    m_Mappings.push_back({
        "(\\w+)\\.alpha\\s*=\\s*(.+?);",
        "Flash_SetAlpha($1_entity, $2);",
        "mc.alpha = val -> Flash_SetAlpha", false, true
    });
    m_Mappings.push_back({
        "(\\w+)\\.visible\\s*=\\s*(.+?);",
        "Flash_SetVisible($1_entity, $2);",
        "mc.visible = val -> Flash_SetVisible", false, true
    });
    m_Mappings.push_back({
        "(\\w+)\\.scaleX\\s*=\\s*(.+?);",
        "Flash_SetScaleX($1_entity, $2);",
        "mc.scaleX = val -> Flash_SetScaleX", false, true
    });
    m_Mappings.push_back({
        "(\\w+)\\.scaleY\\s*=\\s*(.+?);",
        "Flash_SetScaleY($1_entity, $2);",
        "mc.scaleY = val -> Flash_SetScaleY", false, true
    });

    // --- Timer ---
    m_Mappings.push_back({
        "setTimeout\\(",
        "Flash_SetTimeout(",
        "setTimeout -> Flash_SetTimeout"
    });
    m_Mappings.push_back({
        "setInterval\\(",
        "Flash_SetInterval(",
        "setInterval -> Flash_SetInterval"
    });
    m_Mappings.push_back({
        "clearInterval\\(",
        "Flash_ClearInterval(",
        "clearInterval -> Flash_ClearInterval"
    });
    m_Mappings.push_back({
        "clearTimeout\\(",
        "Flash_ClearInterval(",
        "clearTimeout -> Flash_ClearInterval"
    });

    // --- Utility ---
    m_Mappings.push_back({
        "trace\\(",
        "Debug_Print(",
        "trace() -> Debug_Print()"
    });
    m_Mappings.push_back({
        "getTimer\\(\\)",
        "Time_GetElapsedMs()",
        "getTimer() -> Time_GetElapsedMs()"
    });
    m_Mappings.push_back({
        "_root\\b",
        "root_entity",
        "_root -> root_entity", true, false
    });
    m_Mappings.push_back({
        "this\\._parent\\b",
        "Entity_GetParent(this_entity)",
        "this._parent -> Entity_GetParent", true, false
    });
}

void AS3Transpiler::AddMapping(const APIMapping& mapping) {
    m_Mappings.push_back(mapping);
}

// ============================================================================
// Version detection
// ============================================================================

ASVersion AS3Transpiler::DetectVersion(const std::string& source) {
    if (source.find("package ") != std::string::npos ||
        source.find("import flash.") != std::string::npos ||
        source.find("addEventListener") != std::string::npos ||
        source.find("public class ") != std::string::npos) {
        return ASVersion::AS3;
    }
    if (source.find("onClipEvent") != std::string::npos ||
        source.find("_root.") != std::string::npos ||
        source.find("_parent.") != std::string::npos ||
        source.find("onEnterFrame") != std::string::npos) {
        return ASVersion::AS2;
    }
    return ASVersion::AS3;
}

// ============================================================================
// Comment handling
// ============================================================================

std::string AS3Transpiler::StripComments(const std::string& source,
                                          std::vector<std::pair<u32, std::string>>& comments) {
    std::string result;
    result.reserve(source.size());
    u32 line = 1;
    usize i = 0;
    while (i < source.size()) {
        if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '/') {
            std::string comment;
            while (i < source.size() && source[i] != '\n') {
                comment += source[i++];
            }
            comments.push_back({line, comment});
        }
        else if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '*') {
            std::string comment;
            while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/')) {
                if (source[i] == '\n') { line++; result += '\n'; }
                comment += source[i++];
            }
            if (i + 1 < source.size()) { comment += "*/"; i += 2; }
            comments.push_back({line, comment});
        }
        else {
            if (source[i] == '\n') line++;
            result += source[i++];
        }
    }
    return result;
}

std::string AS3Transpiler::NormalizeWhitespace(const std::string& source) {
    std::string result;
    result.reserve(source.size());
    std::istringstream stream(source);
    std::string line;
    bool first = true;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (!first) result += '\n';
        result += line;
        first = false;
    }
    return result;
}

// ============================================================================
// AS2-specific transforms
// ============================================================================

std::string AS3Transpiler::TransformAS2Events(const std::string& source,
                                               std::vector<TranspilerWarning>& warnings) {
    std::string result = source;
    {
        std::regex re("onClipEvent\\s*\\(\\s*enterFrame\\s*\\)\\s*\\{");
        result = std::regex_replace(result, re, "void OnUpdate(float dt) {");
    }
    {
        std::regex re("onClipEvent\\s*\\(\\s*load\\s*\\)\\s*\\{");
        result = std::regex_replace(result, re, "void OnStart() {");
    }
    {
        std::regex re("onClipEvent\\s*\\(\\s*mouseDown\\s*\\)\\s*\\{");
        result = std::regex_replace(result, re, "void OnMouseDown() {");
    }
    {
        std::regex re("onClipEvent\\s*\\(\\s*mouseUp\\s*\\)\\s*\\{");
        result = std::regex_replace(result, re, "void OnMouseUp() {");
    }
    {
        std::regex re("onClipEvent\\s*\\(\\s*keyDown\\s*\\)\\s*\\{");
        result = std::regex_replace(result, re, "void OnKeyDown() {");
    }
    {
        std::regex re("(?:\\w+\\.)?onEnterFrame\\s*=\\s*function\\s*\\(\\s*\\)\\s*\\{");
        result = std::regex_replace(result, re, "void OnUpdate(float dt) {");
    }
    // Warn about unsupported events
    {
        std::regex re("onClipEvent\\s*\\(\\s*(\\w+)\\s*\\)");
        std::smatch match;
        std::string searchStr = result;
        while (std::regex_search(searchStr, match, re)) {
            std::string eventName = match[1].str();
            if (eventName != "enterFrame" && eventName != "load" &&
                eventName != "mouseDown" && eventName != "mouseUp" &&
                eventName != "keyDown") {
                TranspilerWarning w;
                w.message = "Unsupported onClipEvent type: " + eventName;
                w.originalCode = match[0].str();
                warnings.push_back(w);
            }
            searchStr = match.suffix().str();
        }
    }
    return result;
}

std::string AS3Transpiler::TransformAS2Properties(const std::string& source,
                                                    std::vector<TranspilerWarning>& /*warnings*/) {
    std::string result = source;
    {
        std::regex re("(\\w+)\\._x\\s*=\\s*(.+?);");
        result = std::regex_replace(result, re, "Transform_SetPositionX($1_entity, $2);");
    }
    {
        std::regex re("(\\w+)\\._y\\s*=\\s*(.+?);");
        result = std::regex_replace(result, re, "Transform_SetPositionY($1_entity, $2);");
    }
    {
        std::regex re("(\\w+)\\._rotation\\s*=\\s*(.+?);");
        result = std::regex_replace(result, re, "Transform_SetRotationZ($1_entity, $2);");
    }
    {
        std::regex re("(\\w+)\\._alpha\\s*=\\s*(.+?);");
        result = std::regex_replace(result, re, "Sprite_SetAlpha($1_entity, ($2) / 100.0f);");
    }
    {
        std::regex re("(\\w+)\\._visible\\s*=\\s*(.+?);");
        result = std::regex_replace(result, re, "Entity_SetVisible($1_entity, $2);");
    }
    return result;
}

// ============================================================================
// AS3-specific transforms
// ============================================================================

std::string AS3Transpiler::TransformAS3Classes(const std::string& source,
                                                std::vector<TranspilerWarning>& warnings) {
    std::string result = source;
    {
        std::regex re("package\\s+[\\w.]*\\s*\\{");
        result = std::regex_replace(result, re, "// [package removed]");
    }
    {
        std::regex re("import\\s+[\\w.*]+\\s*;");
        result = std::regex_replace(result, re, "");
    }
    {
        std::regex re("public\\s+class\\s+(\\w+)\\s+extends\\s+(\\w+)\\s*\\{");
        std::smatch match;
        if (std::regex_search(result, match, re)) {
            std::string className = match[1].str();
            std::string baseClass = match[2].str();
            result = std::regex_replace(result, re,
                "// class " + className + " (was extends " + baseClass + ")");
            if (baseClass != "Sprite" && baseClass != "MovieClip" && baseClass != "Object") {
                TranspilerWarning w;
                w.message = "Custom inheritance from '" + baseClass + "' - needs manual adaptation";
                w.originalCode = match[0].str();
                warnings.push_back(w);
            }
        }
    }
    {
        std::regex re("public\\s+function\\s+(\\w+)\\s*\\(\\s*\\)\\s*\\{");
        result = std::regex_replace(result, re, "void OnStart() { // was $1()");
    }
    {
        std::regex re("\\b(public|private|protected|internal)\\s+(?=function|var|const)");
        result = std::regex_replace(result, re, "");
    }
    {
        std::regex re("function\\s+(\\w+)\\s*\\(([^)]*)\\)\\s*:\\s*(\\w+)");
        result = std::regex_replace(result, re, "$3 $1($2)");
    }
    {
        std::regex re("function\\s+(\\w+)\\s*\\(([^)]*)\\)\\s*\\{");
        result = std::regex_replace(result, re, "void $1($2) {");
    }
    return result;
}

std::string AS3Transpiler::TransformAS3Events(const std::string& source,
                                               std::vector<TranspilerWarning>& /*warnings*/) {
    std::string result = source;
    {
        std::regex re("addEventListener\\s*\\(\\s*Event\\.ENTER_FRAME\\s*,\\s*(\\w+)\\s*\\)");
        result = std::regex_replace(result, re, "Events_Listen(\"enterFrame\", \"$1\")");
    }
    {
        std::regex re("addEventListener\\s*\\(\\s*MouseEvent\\.CLICK\\s*,\\s*(\\w+)\\s*\\)");
        result = std::regex_replace(result, re, "Events_Listen(\"mouseClick\", \"$1\")");
    }
    {
        std::regex re("addEventListener\\s*\\(\\s*KeyboardEvent\\.KEY_DOWN\\s*,\\s*(\\w+)\\s*\\)");
        result = std::regex_replace(result, re, "Events_Listen(\"keyDown\", \"$1\")");
    }
    {
        std::regex re("addEventListener\\s*\\(\\s*KeyboardEvent\\.KEY_UP\\s*,\\s*(\\w+)\\s*\\)");
        result = std::regex_replace(result, re, "Events_Listen(\"keyUp\", \"$1\")");
    }
    {
        std::regex re("removeEventListener\\s*\\(\\s*Event\\.ENTER_FRAME\\s*,\\s*(\\w+)\\s*\\)");
        result = std::regex_replace(result, re, "Events_Unlisten(\"enterFrame\", \"$1\")");
    }
    return result;
}

std::string AS3Transpiler::TransformAS3Imports(const std::string& source,
                                                std::vector<std::string>& requiredImports) {
    std::string result = source;
    if (result.find("Events_Listen") != std::string::npos) requiredImports.push_back("events");
    if (result.find("Audio_") != std::string::npos) requiredImports.push_back("audio");
    if (result.find("Save_") != std::string::npos) requiredImports.push_back("save");
    if (result.find("Input_") != std::string::npos) requiredImports.push_back("input");
    if (result.find("Animator_") != std::string::npos) requiredImports.push_back("animation");
    if (result.find("Transform_") != std::string::npos) requiredImports.push_back("transform");
    return result;
}

// ============================================================================
// Common transforms
// ============================================================================

std::string AS3Transpiler::ApplyAPIMappings(const std::string& source, u32& matchCount) {
    std::string result = source;
    matchCount = 0;
    for (auto& mapping : m_Mappings) {
        try {
            std::regex re(mapping.asPattern);
            std::string before = result;
            result = std::regex_replace(result, re, mapping.angelReplacement);
            if (result != before) matchCount++;
        } catch (const std::regex_error&) {
            // Skip invalid regex patterns
        }
    }
    return result;
}

std::string AS3Transpiler::TransformTypes(const std::string& source) {
    std::string result = source;
    { std::regex re(":\\s*Number\\b"); result = std::regex_replace(result, re, ": float"); }
    { std::regex re(":\\s*int\\b"); result = std::regex_replace(result, re, ": int"); }
    { std::regex re(":\\s*uint\\b"); result = std::regex_replace(result, re, ": uint"); }
    { std::regex re(":\\s*String\\b"); result = std::regex_replace(result, re, ": string"); }
    { std::regex re(":\\s*Boolean\\b"); result = std::regex_replace(result, re, ": bool"); }
    { std::regex re(":\\s*void\\b"); result = std::regex_replace(result, re, ": void"); }
    { std::regex re(":\\s*Array\\b"); result = std::regex_replace(result, re, ""); }
    {
        std::regex re("\\bvar\\s+(\\w+)\\s*:\\s*(\\w+)\\s*=");
        result = std::regex_replace(result, re, "$2 $1 =");
    }
    {
        std::regex re("\\bvar\\s+(\\w+)\\s*=");
        result = std::regex_replace(result, re, "auto $1 =");
    }
    {
        std::regex re("\\bconst\\s+(\\w+)\\s*:\\s*(\\w+)\\s*=");
        result = std::regex_replace(result, re, "const $2 $1 =");
    }
    return result;
}

std::string AS3Transpiler::TransformLoops(const std::string& source) {
    std::string result = source;
    {
        std::regex re("for\\s+each\\s*\\(\\s*var\\s+(\\w+)\\s*:\\s*(\\w+)\\s+in\\s+(\\w+)\\s*\\)");
        result = std::regex_replace(result, re,
            "for (uint _i = 0; _i < $3.length(); _i++) { $2 $1 = $3[_i];");
    }
    return result;
}

std::string AS3Transpiler::TransformStringOps(const std::string& source) {
    std::string result = source;
    {
        std::regex re("(\\w+)\\.length\\b(?!\\s*\\()");
        result = std::regex_replace(result, re, "$1.length()");
    }
    {
        std::regex re("\\bString\\((\\w+)\\)");
        result = std::regex_replace(result, re, "(\"\" + $1)");
    }
    {
        std::regex re("\\bNumber\\((\\w+)\\)");
        result = std::regex_replace(result, re, "parseFloat($1)");
    }
    return result;
}

std::string AS3Transpiler::TransformArrayLiterals(const std::string& source) {
    std::string result = source;
    // Transform: var x:Array = [1,2,3] -> array<int> x = {1,2,3}
    // Transform: new Array(a,b,c) -> array<int> = {a,b,c}
    {
        std::regex re("new\\s+Array\\(([^)]*)\\)");
        result = std::regex_replace(result, re, "{$1}");
    }
    // Transform standalone array literal assignments: = [items] -> = {items}
    // Only match right side of assignment to avoid breaking array access like arr[0]
    {
        std::regex re("=\\s*\\[([^\\]]*?)\\]\\s*;");
        result = std::regex_replace(result, re, "= {$1};");
    }
    return result;
}

std::string AS3Transpiler::WrapInBehavior(const std::string& source, const std::string& className) {
    std::ostringstream wrapped;
    wrapped << "// Auto-transpiled from ActionScript\n";
    wrapped << "// Generated by Enjin AS3 Transpiler\n\n";
    wrapped << "class " << className << " : TegeBehavior {\n";
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) wrapped << "    " << line << "\n";
        else wrapped << "\n";
    }
    wrapped << "}\n";
    return wrapped.str();
}

u32 AS3Transpiler::CountLines(const std::string& s) {
    return static_cast<u32>(std::count(s.begin(), s.end(), '\n')) + 1;
}

// ============================================================================
// Main transpile function
// ============================================================================

TranspileResult AS3Transpiler::Transpile(const std::string& asSource,
                                          const TranspilerConfig& config) {
    TranspileResult result;
    if (asSource.empty()) {
        result.errorMessage = "Empty source code";
        return result;
    }

    ASVersion version = config.sourceVersion;
    if (version == ASVersion::Auto) version = DetectVersion(asSource);

    std::string code = NormalizeWhitespace(asSource);

    std::vector<std::pair<u32, std::string>> comments;
    if (!config.preserveComments) code = StripComments(code, comments);

    result.linesProcessed = CountLines(code);

    if (version == ASVersion::AS2) {
        code = TransformAS2Events(code, result.warnings);
        code = TransformAS2Properties(code, result.warnings);
    } else {
        code = TransformAS3Classes(code, result.warnings);
        code = TransformAS3Events(code, result.warnings);
        code = TransformAS3Imports(code, result.requiredImports);
    }

    code = TransformTypes(code);
    code = TransformLoops(code);
    code = TransformStringOps(code);
    code = TransformArrayLiterals(code);

    u32 matchCount = 0;
    code = ApplyAPIMappings(code, matchCount);
    result.patternsMatched = matchCount;

    if (config.wrapInClass) code = WrapInBehavior(code, config.className);

    if (config.verbose) {
        std::ostringstream header;
        header << "// === Transpiled from ActionScript "
               << (version == ASVersion::AS2 ? "2.0" : "3.0") << " ===\n";
        header << "// Lines: " << result.linesProcessed << "\n";
        header << "// API mappings matched: " << result.patternsMatched << "\n";
        if (!result.warnings.empty())
            header << "// Warnings: " << result.warnings.size() << "\n";
        header << "// =====================================\n\n";
        code = header.str() + code;
    }

    result.angelScript = code;
    result.success = true;

    ENJIN_LOG_INFO(Script, "Transpiled AS%s: %u lines, %u API matches, %u warnings",
                   (version == ASVersion::AS2 ? "2" : "3"),
                   result.linesProcessed, result.patternsMatched,
                   static_cast<u32>(result.warnings.size()));
    return result;
}

TranspileResult AS3Transpiler::TranspileFile(const std::string& inputPath,
                                              const TranspilerConfig& config) {
    std::ifstream file(inputPath);
    if (!file.is_open()) {
        TranspileResult result;
        result.errorMessage = "Cannot open file: " + inputPath;
        return result;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return Transpile(ss.str(), config);
}

bool AS3Transpiler::TranspileToFile(const std::string& inputPath, const std::string& outputPath,
                                     const TranspilerConfig& config) {
    auto result = TranspileFile(inputPath, config);
    if (!result.success) return false;
    std::ofstream out(outputPath);
    if (!out.is_open()) return false;
    out << result.angelScript;
    return true;
}

std::string AS3Transpiler::Preview(const std::string& asSource, u32 maxLines) {
    auto result = Transpile(asSource);
    if (!result.success) return "// Error: " + result.errorMessage;
    std::string output;
    std::istringstream stream(result.angelScript);
    std::string line;
    u32 count = 0;
    while (std::getline(stream, line) && count < maxLines) {
        output += line + "\n";
        count++;
    }
    if (count >= maxLines)
        output += "// ... (" + std::to_string(result.linesProcessed - maxLines) + " more lines)\n";
    return output;
}

} // namespace Scripting
} // namespace Enjin
