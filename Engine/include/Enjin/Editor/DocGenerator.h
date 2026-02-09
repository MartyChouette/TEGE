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
    std::string m_LastError;
    std::vector<std::string> m_GeneratedFiles;
};

} // namespace Editor
} // namespace Enjin
