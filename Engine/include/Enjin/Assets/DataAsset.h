#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>

namespace Enjin {
namespace Assets {

// ============================================================================
// DATA FIELD TYPES
// ============================================================================

enum class DataFieldType : u8 {
    String,
    Float,
    Int,
    Bool,
    Vector3,
    Vector4,
    StringArray,
    FloatArray
};

inline const char* DataFieldTypeToString(DataFieldType type) {
    switch (type) {
        case DataFieldType::String:      return "String";
        case DataFieldType::Float:       return "Float";
        case DataFieldType::Int:         return "Int";
        case DataFieldType::Bool:        return "Bool";
        case DataFieldType::Vector3:     return "Vector3";
        case DataFieldType::Vector4:     return "Vector4";
        case DataFieldType::StringArray: return "StringArray";
        case DataFieldType::FloatArray:  return "FloatArray";
        default:                         return "Unknown";
    }
}

inline DataFieldType DataFieldTypeFromString(const std::string& s) {
    if (s == "String")      return DataFieldType::String;
    if (s == "Float")       return DataFieldType::Float;
    if (s == "Int")         return DataFieldType::Int;
    if (s == "Bool")        return DataFieldType::Bool;
    if (s == "Vector3")     return DataFieldType::Vector3;
    if (s == "Vector4")     return DataFieldType::Vector4;
    if (s == "StringArray") return DataFieldType::StringArray;
    if (s == "FloatArray")  return DataFieldType::FloatArray;
    return DataFieldType::String;
}

// ============================================================================
// DATA ASSET VALUE
// ============================================================================

using DataAssetValue = std::variant<
    std::string,
    f32,
    i32,
    bool,
    Math::Vector3,
    Math::Vector4,
    std::vector<std::string>,
    std::vector<f32>
>;

// ============================================================================
// SCHEMA DEFINITION
// ============================================================================

struct DataAssetField {
    std::string name;
    DataFieldType type = DataFieldType::String;
    DataAssetValue defaultValue;
};

struct DataAssetSchema {
    std::string name;
    std::string description;
    std::vector<DataAssetField> fields;
};

// ============================================================================
// DATA ASSET INSTANCE
// ============================================================================

struct DataAsset {
    std::string name;
    std::string schemaName;
    std::string filePath;
    std::unordered_map<std::string, DataAssetValue> values;
};

// ============================================================================
// DATA ASSET REGISTRY (Singleton)
// ============================================================================

class ENJIN_API DataAssetRegistry {
public:
    static DataAssetRegistry& Get();

    // Schema management
    void RegisterSchema(const DataAssetSchema& schema);
    void RemoveSchema(const std::string& name);
    const DataAssetSchema* FindSchema(const std::string& name) const;
    std::vector<const DataAssetSchema*> GetAllSchemas() const;

    // Schema I/O
    bool SaveSchema(const DataAssetSchema& schema, const std::string& path);
    bool LoadSchema(const std::string& path);
    void ScanSchemaDirectory(const std::string& directory);

    // Asset management
    void CreateAsset(const DataAsset& asset);
    void RemoveAsset(const std::string& name);
    const DataAsset* FindAsset(const std::string& name) const;
    DataAsset* FindAssetMut(const std::string& name);
    std::vector<const DataAsset*> GetAssetsBySchema(const std::string& schemaName) const;
    std::vector<const DataAsset*> GetAllAssets() const;

    // Asset I/O
    bool SaveAsset(const DataAsset& asset, const std::string& path);
    bool LoadAsset(const std::string& path);
    void ScanAssetDirectory(const std::string& directory);

    // Typed getters (with fallback defaults)
    f32 GetFloat(const std::string& assetName, const std::string& field, f32 fallback = 0.0f) const;
    i32 GetInt(const std::string& assetName, const std::string& field, i32 fallback = 0) const;
    bool GetBool(const std::string& assetName, const std::string& field, bool fallback = false) const;
    std::string GetString(const std::string& assetName, const std::string& field, const std::string& fallback = "") const;
    Math::Vector3 GetVector3(const std::string& assetName, const std::string& field, Math::Vector3 fallback = {0,0,0}) const;

    // Typed setters
    void SetFloat(const std::string& assetName, const std::string& field, f32 value);
    void SetInt(const std::string& assetName, const std::string& field, i32 value);
    void SetBool(const std::string& assetName, const std::string& field, bool value);
    void SetString(const std::string& assetName, const std::string& field, const std::string& value);
    void SetVector3(const std::string& assetName, const std::string& field, Math::Vector3 value);

    // Clear all schemas and assets
    void Clear();

private:
    DataAssetRegistry() = default;
    ~DataAssetRegistry() = default;
    DataAssetRegistry(const DataAssetRegistry&) = delete;
    DataAssetRegistry& operator=(const DataAssetRegistry&) = delete;

    std::unordered_map<std::string, DataAssetSchema> m_Schemas;
    std::unordered_map<std::string, DataAsset> m_Assets;
};

} // namespace Assets
} // namespace Enjin
