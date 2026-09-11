#pragma once

#include "Enjin/Platform/Platform.h"
#include <string>
#include <vector>

// Forward declarations
class asIScriptEngine;

namespace Enjin {
namespace Editor {

// Parsed struct field from a component header
struct FieldDoc {
    std::string name;
    std::string type;
    std::string comment;
    std::string defaultValue;
};

// Parsed struct from a component header
struct StructDoc {
    std::string name;
    std::string comment;
    std::vector<FieldDoc> fields;
};

class ENJIN_API DocGenerator {
public:
    void SetOutputDirectory(const std::string& path);

    // Where to find Engine/include/Enjin/ECS/Components for the component
    // reference. Defaults to a search from the working directory and the
    // executable's own directory upwards, because the editor's CWD is its exe
    // directory and never the repo root -- the component generator looked for a
    // bare relative "Engine/include/..." and its "fall back to the current
    // working directory" retry looked at the SAME path, so it failed every time
    // outside a shell sitting in the repo root, and took GenerateAll's return
    // value down with it.
    void SetSourceRoot(const std::string& path);

    // Empty when the component reference could be generated. Otherwise says why
    // it could not, so a caller can report a partial result instead of a bare
    // failure -- an installed editor has no engine headers to parse and that is
    // not the same thing as something being broken.
    const std::string& GetSkippedReason() const { return m_SkippedReason; }

    // Generate all documentation
    bool GenerateAll(asIScriptEngine* scriptEngine = nullptr);

    // Individual generators
    bool GenerateComponentDocs();
    bool GenerateScriptAPIDocs(asIScriptEngine* scriptEngine);
    bool GenerateVisualScriptNodeDocs();
    bool GenerateDataAssetDocs();

    const std::string& GetLastError() const { return m_LastError; }
    const std::vector<std::string>& GetGeneratedFiles() const { return m_GeneratedFiles; }

private:
    std::vector<StructDoc> ParseComponentHeader(const std::string& filepath);
    bool WriteMarkdown(const std::string& filename, const std::string& content);
    bool GenerateIndex();

    std::string m_OutputDirectory = "docs/generated";
    std::string m_SourceRoot;      // empty = search
    std::string m_SkippedReason;
    std::string m_LastError;

    // Locate a directory containing Engine/include/Enjin/ECS/Components, looking
    // at m_SourceRoot first, then walking up from the working directory and from
    // the executable's directory. Empty if there is none.
    std::string ResolveSourceRoot() const;
    std::vector<std::string> m_GeneratedFiles;
};

} // namespace Editor
} // namespace Enjin
