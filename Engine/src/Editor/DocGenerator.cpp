#include "Enjin/Editor/DocGenerator.h"
#include "Enjin/VisualScript/NodeRegistry.h"
#include "Enjin/VisualScript/NodeDefinition.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Enjin {
namespace Editor {

namespace fs = std::filesystem;

void DocGenerator::SetOutputDirectory(const std::string& path) {
    m_OutputDirectory = path;
}

bool DocGenerator::GenerateAll(asIScriptEngine* scriptEngine) {
    m_GeneratedFiles.clear();
    m_LastError.clear();

    // Ensure output directory exists
    try {
        fs::create_directories(m_OutputDirectory);
    } catch (const std::exception& e) {
        m_LastError = std::string("Failed to create output directory: ") + e.what();
        return false;
    }

    bool ok = true;

    ok = GenerateComponentDocs() && ok;
    if (scriptEngine) {
        ok = GenerateScriptAPIDocs(scriptEngine) && ok;
    }
    ok = GenerateVisualScriptNodeDocs() && ok;
    ok = GenerateDataAssetDocs() && ok;
    ok = GenerateIndex() && ok;

    return ok;
}

// ============================================================================
// COMPONENT DOCS
// ============================================================================

std::vector<StructDoc> DocGenerator::ParseComponentHeader(const std::string& filepath) {
    std::vector<StructDoc> structs;
    std::ifstream file(filepath);
    if (!file.is_open()) return structs;

    std::string line;
    std::string pendingComment;
    StructDoc* currentStruct = nullptr;

    while (std::getline(file, line)) {
        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            pendingComment.clear();
            continue;
        }
        std::string trimmed = line.substr(start);

        // Collect /// comments
        if (trimmed.substr(0, 3) == "///") {
            std::string comment = trimmed.substr(3);
            if (!comment.empty() && comment[0] == ' ') comment = comment.substr(1);
            if (!pendingComment.empty()) pendingComment += " ";
            pendingComment += comment;
            continue;
        }

        // Detect struct opening
        if (trimmed.find("struct ") == 0 && trimmed.find('{') != std::string::npos) {
            // Extract struct name: "struct FooBar {" or "struct ENJIN_API FooBar {"
            std::string rest = trimmed.substr(7); // skip "struct "
            // Remove ENJIN_API if present
            if (rest.find("ENJIN_API ") == 0) rest = rest.substr(10);
            size_t nameEnd = rest.find_first_of(" :{");
            std::string name = (nameEnd != std::string::npos) ? rest.substr(0, nameEnd) : rest;

            // Skip empty or internal names
            if (!name.empty() && name[0] != '_') {
                structs.push_back({});
                currentStruct = &structs.back();
                currentStruct->name = name;
                currentStruct->comment = pendingComment;
            }
            pendingComment.clear();
            continue;
        }

        // Detect struct closing
        if (currentStruct && trimmed[0] == '}') {
            currentStruct = nullptr;
            pendingComment.clear();
            continue;
        }

        // Inside a struct: parse fields
        if (currentStruct) {
            // Skip access specifiers, methods, nested structs, etc.
            if (trimmed.find("public:") == 0 || trimmed.find("private:") == 0 ||
                trimmed.find("protected:") == 0 || trimmed.find("//") == 0 ||
                trimmed.find("static ") == 0 || trimmed.find("virtual ") == 0 ||
                trimmed.find("void ") == 0 || trimmed.find("bool ") == 0 ||
                trimmed.find("enum ") == 0 || trimmed.find("struct ") == 0 ||
                trimmed.find("using ") == 0 || trimmed.find("friend ") == 0 ||
                trimmed.find("template") == 0 || trimmed.find("#") == 0 ||
                trimmed.find("(") != std::string::npos) {
                // Check if this is a function (has parentheses) — skip unless it's a field with parens in default
                if (trimmed.find("(") != std::string::npos && trimmed.find("=") == std::string::npos) {
                    pendingComment.clear();
                    continue;
                }
                // "bool " could be a field
                if (trimmed.find("bool ") != 0 || trimmed.find("(") != std::string::npos) {
                    pendingComment.clear();
                    continue;
                }
            }

            // Try to parse a field: "type name = default;" or "type name;"
            // Find semicolon
            size_t semi = trimmed.find(';');
            if (semi == std::string::npos) {
                pendingComment.clear();
                continue;
            }

            std::string fieldLine = trimmed.substr(0, semi);

            // Check for default value
            std::string defaultVal;
            size_t eqPos = fieldLine.find('=');
            if (eqPos != std::string::npos) {
                defaultVal = fieldLine.substr(eqPos + 1);
                // Trim whitespace
                size_t dvStart = defaultVal.find_first_not_of(" \t");
                if (dvStart != std::string::npos) defaultVal = defaultVal.substr(dvStart);
                size_t dvEnd = defaultVal.find_last_not_of(" \t");
                if (dvEnd != std::string::npos) defaultVal = defaultVal.substr(0, dvEnd + 1);
                fieldLine = fieldLine.substr(0, eqPos);
            }

            // Trim fieldLine
            size_t flEnd = fieldLine.find_last_not_of(" \t");
            if (flEnd != std::string::npos) fieldLine = fieldLine.substr(0, flEnd + 1);

            // Split into type and name (last word is name)
            size_t lastSpace = fieldLine.find_last_of(" \t");
            if (lastSpace != std::string::npos) {
                std::string type = fieldLine.substr(0, lastSpace);
                std::string name = fieldLine.substr(lastSpace + 1);

                // Trim type
                size_t typeStart = type.find_first_not_of(" \t");
                if (typeStart != std::string::npos) type = type.substr(typeStart);

                if (!name.empty() && !type.empty()) {
                    FieldDoc field;
                    field.name = name;
                    field.type = type;
                    field.comment = pendingComment;
                    field.defaultValue = defaultVal;
                    currentStruct->fields.push_back(field);
                }
            }
        }
        pendingComment.clear();
    }

    return structs;
}

bool DocGenerator::GenerateComponentDocs() {
    std::string componentsDir = "Engine/include/Enjin/ECS/Components";
    if (!fs::exists(componentsDir)) {
        // Try relative to current working directory
        componentsDir = "Engine/include/Enjin/ECS/Components";
        if (!fs::exists(componentsDir)) {
            m_LastError = "Components directory not found";
            return false;
        }
    }

    std::ostringstream md;
    md << "# ECS Components Reference\n\n";
    md << "Auto-generated documentation of all ECS component types.\n\n";
    md << "---\n\n";

    int structCount = 0;

    // Iterate all header files in Components/
    std::vector<std::string> headers;
    for (const auto& entry : fs::recursive_directory_iterator(componentsDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".h" || ext == ".hpp") {
                headers.push_back(entry.path().string());
            }
        }
    }
    std::sort(headers.begin(), headers.end());

    for (const auto& header : headers) {
        auto structs = ParseComponentHeader(header);
        for (const auto& s : structs) {
            md << "## " << s.name << "\n\n";
            if (!s.comment.empty()) {
                md << s.comment << "\n\n";
            }

            // Show source file
            std::string relPath = fs::relative(header, ".").string();
            md << "**Source:** `" << relPath << "`\n\n";

            if (!s.fields.empty()) {
                md << "| Field | Type | Default | Description |\n";
                md << "|-------|------|---------|-------------|\n";
                for (const auto& f : s.fields) {
                    md << "| `" << f.name << "` | `" << f.type << "` | ";
                    md << (f.defaultValue.empty() ? "-" : "`" + f.defaultValue + "`");
                    md << " | " << (f.comment.empty() ? "-" : f.comment) << " |\n";
                }
                md << "\n";
            }
            md << "---\n\n";
            structCount++;
        }
    }

    md << "\n*Generated " << structCount << " component types.*\n";

    return WriteMarkdown("COMPONENTS.md", md.str());
}

// ============================================================================
// SCRIPT API DOCS
// ============================================================================

bool DocGenerator::GenerateScriptAPIDocs(asIScriptEngine* scriptEngine) {
    if (!scriptEngine) {
        m_LastError = "No script engine available";
        return false;
    }

    std::ostringstream md;
    md << "# AngelScript API Reference\n\n";
    md << "Auto-generated documentation of all registered AngelScript functions.\n\n";
    md << "---\n\n";

    // Group functions by category prefix
    std::unordered_map<std::string, std::vector<std::string>> categories;

    asUINT funcCount = scriptEngine->GetGlobalFunctionCount();
    for (asUINT i = 0; i < funcCount; i++) {
        asIScriptFunction* func = scriptEngine->GetGlobalFunctionByIndex(i);
        if (!func) continue;

        std::string decl = func->GetDeclaration(true, false, true);
        std::string name = func->GetName();

        // Extract category from prefix (e.g. "Entity_Spawn" -> "Entity")
        std::string category = "General";
        size_t underscore = name.find('_');
        if (underscore != std::string::npos) {
            category = name.substr(0, underscore);
        }

        categories[category].push_back(decl);
    }

    // Sort categories
    std::vector<std::string> catNames;
    catNames.reserve(categories.size());
    for (const auto& [cat, _] : categories) {
        catNames.push_back(cat);
    }
    std::sort(catNames.begin(), catNames.end());

    int totalFunctions = 0;
    for (const auto& cat : catNames) {
        md << "## " << cat << "\n\n";
        auto& funcs = categories[cat];
        std::sort(funcs.begin(), funcs.end());
        for (const auto& decl : funcs) {
            md << "- `" << decl << "`\n";
            totalFunctions++;
        }
        md << "\n";
    }

    md << "---\n\n*Generated " << totalFunctions << " functions across "
       << catNames.size() << " categories.*\n";

    return WriteMarkdown("SCRIPTING_API.md", md.str());
}

// ============================================================================
// VISUAL SCRIPT NODE DOCS
// ============================================================================

bool DocGenerator::GenerateVisualScriptNodeDocs() {
    auto& registry = VisualScript::NodeRegistry::Instance();
    auto allNodes = registry.GetAllNodes();

    std::ostringstream md;
    md << "# Visual Script Node Reference\n\n";
    md << "Auto-generated documentation of all visual script nodes.\n\n";
    md << "---\n\n";

    // Group by category
    std::unordered_map<std::string, std::vector<const VisualScript::NodeDefinition*>> categories;
    for (const auto* node : allNodes) {
        std::string catName = VisualScript::NodeCategoryToString(node->category);
        categories[catName].push_back(node);
    }

    std::vector<std::string> catNames;
    catNames.reserve(categories.size());
    for (const auto& [cat, _] : categories) {
        catNames.push_back(cat);
    }
    std::sort(catNames.begin(), catNames.end());

    for (const auto& cat : catNames) {
        md << "## " << cat << "\n\n";
        auto& nodes = categories[cat];

        for (const auto* node : nodes) {
            md << "### " << node->displayName << "\n\n";
            if (!node->description.empty()) {
                md << node->description << "\n\n";
            }

            md << "**Type ID:** `" << node->typeId << "`";
            if (node->IsPure()) md << " | **Pure**";
            if (node->IsEvent()) md << " | **Event**";
            if (node->IsLatent()) md << " | **Latent**";
            md << "\n\n";

            // Inputs
            bool hasDataInputs = false;
            for (const auto& pin : node->inputs) {
                if (pin.type != Enjin::Editor::PinType::Flow) {
                    hasDataInputs = true;
                    break;
                }
            }
            if (hasDataInputs) {
                md << "**Inputs:**\n";
                for (const auto& pin : node->inputs) {
                    if (pin.type == Enjin::Editor::PinType::Flow) continue;
                    md << "- `" << pin.name << "` (" << static_cast<int>(pin.type) << ")\n";
                }
                md << "\n";
            }

            // Outputs
            bool hasDataOutputs = false;
            for (const auto& pin : node->outputs) {
                if (pin.type != Enjin::Editor::PinType::Flow) {
                    hasDataOutputs = true;
                    break;
                }
            }
            if (hasDataOutputs) {
                md << "**Outputs:**\n";
                for (const auto& pin : node->outputs) {
                    if (pin.type == Enjin::Editor::PinType::Flow) continue;
                    md << "- `" << pin.name << "` (" << static_cast<int>(pin.type) << ")\n";
                }
                md << "\n";
            }
        }
    }

    md << "---\n\n*Generated " << allNodes.size() << " nodes across "
       << catNames.size() << " categories.*\n";

    return WriteMarkdown("VISUAL_SCRIPT_NODES.md", md.str());
}

// ============================================================================
// DATA ASSET DOCS
// ============================================================================

bool DocGenerator::GenerateDataAssetDocs() {
    auto& registry = Assets::DataAssetRegistry::Get();
    auto schemas = registry.GetAllSchemas();

    std::ostringstream md;
    md << "# Data Asset Reference\n\n";
    md << "Auto-generated documentation of all registered data asset schemas.\n\n";
    md << "---\n\n";

    if (schemas.empty()) {
        md << "*No schemas registered.*\n";
    }

    for (const auto* schema : schemas) {
        md << "## " << schema->name << "\n\n";
        if (!schema->description.empty()) {
            md << schema->description << "\n\n";
        }

        if (!schema->fields.empty()) {
            md << "| Field | Type | Default |\n";
            md << "|-------|------|---------|\n";
            for (const auto& field : schema->fields) {
                md << "| `" << field.name << "` | "
                   << Assets::DataFieldTypeToString(field.type) << " | ";

                // Format default value
                switch (field.type) {
                    case Assets::DataFieldType::String:
                        md << "`\"" << std::get<std::string>(field.defaultValue) << "\"`";
                        break;
                    case Assets::DataFieldType::Float:
                        md << std::get<f32>(field.defaultValue);
                        break;
                    case Assets::DataFieldType::Int:
                        md << std::get<i32>(field.defaultValue);
                        break;
                    case Assets::DataFieldType::Bool:
                        md << (std::get<bool>(field.defaultValue) ? "true" : "false");
                        break;
                    default:
                        md << "-";
                        break;
                }
                md << " |\n";
            }
            md << "\n";
        }

        // List assets using this schema
        auto assets = registry.GetAssetsBySchema(schema->name);
        if (!assets.empty()) {
            md << "**Instances:** ";
            for (size_t i = 0; i < assets.size(); i++) {
                if (i > 0) md << ", ";
                md << "`" << assets[i]->name << "`";
            }
            md << "\n\n";
        }

        md << "---\n\n";
    }

    md << "*Generated " << schemas.size() << " schemas.*\n";

    return WriteMarkdown("DATA_ASSETS.md", md.str());
}

// ============================================================================
// INDEX
// ============================================================================

bool DocGenerator::GenerateIndex() {
    std::ostringstream md;
    md << "# Generated Documentation Index\n\n";
    md << "Auto-generated API and reference documentation for the Enjin Engine.\n\n";

    md << "## Contents\n\n";
    md << "- [ECS Components](COMPONENTS.md) - All component struct definitions and fields\n";
    md << "- [Scripting API](SCRIPTING_API.md) - AngelScript function reference\n";
    md << "- [Visual Script Nodes](VISUAL_SCRIPT_NODES.md) - Blueprint-style node reference\n";
    md << "- [Data Assets](DATA_ASSETS.md) - Registered schemas and data asset instances\n";
    md << "\n---\n\n";
    md << "*Generated " << m_GeneratedFiles.size() << " documentation files.*\n";

    return WriteMarkdown("INDEX.md", md.str());
}

// ============================================================================
// HELPERS
// ============================================================================

bool DocGenerator::WriteMarkdown(const std::string& filename, const std::string& content) {
    std::string fullPath = m_OutputDirectory + "/" + filename;
    try {
        std::ofstream file(fullPath);
        if (!file.is_open()) {
            m_LastError = "Failed to write: " + fullPath;
            return false;
        }
        file << content;
        m_GeneratedFiles.push_back(fullPath);
        return true;
    } catch (const std::exception& e) {
        m_LastError = std::string("Write error: ") + e.what();
        return false;
    }
}

} // namespace Editor
} // namespace Enjin
