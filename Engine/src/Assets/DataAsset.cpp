#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Assets {

namespace fs = std::filesystem;
using json = nlohmann::json;

// ============================================================================
// SINGLETON
// ============================================================================

DataAssetRegistry& DataAssetRegistry::Get() {
    static DataAssetRegistry instance;
    return instance;
}

// ============================================================================
// JSON SERIALIZATION HELPERS
// ============================================================================

static json SerializeValue(const DataAssetValue& value, DataFieldType type) {
    json j;
    j["type"] = DataFieldTypeToString(type);
    switch (type) {
        case DataFieldType::String:
            j["value"] = std::get<std::string>(value);
            break;
        case DataFieldType::Float:
            j["value"] = std::get<f32>(value);
            break;
        case DataFieldType::Int:
            j["value"] = std::get<i32>(value);
            break;
        case DataFieldType::Bool:
            j["value"] = std::get<bool>(value);
            break;
        case DataFieldType::Vector3: {
            auto& v = std::get<Math::Vector3>(value);
            j["value"] = {v.x, v.y, v.z};
            break;
        }
        case DataFieldType::Vector4: {
            auto& v = std::get<Math::Vector4>(value);
            j["value"] = {v.x, v.y, v.z, v.w};
            break;
        }
        case DataFieldType::StringArray: {
            auto& arr = std::get<std::vector<std::string>>(value);
            j["value"] = arr;
            break;
        }
        case DataFieldType::FloatArray: {
            auto& arr = std::get<std::vector<f32>>(value);
            j["value"] = arr;
            break;
        }
    }
    return j;
}

static DataAssetValue DeserializeValue(const json& j) {
    std::string typeStr = j.value("type", "String");
    DataFieldType type = DataFieldTypeFromString(typeStr);

    switch (type) {
        case DataFieldType::String:
            return j.value("value", std::string(""));
        case DataFieldType::Float:
            return j.value("value", 0.0f);
        case DataFieldType::Int:
            return j.value("value", 0);
        case DataFieldType::Bool:
            return j.value("value", false);
        case DataFieldType::Vector3: {
            if (j.contains("value") && j["value"].is_array() && j["value"].size() >= 3) {
                return Math::Vector3(j["value"][0].get<f32>(),
                                     j["value"][1].get<f32>(),
                                     j["value"][2].get<f32>());
            }
            return Math::Vector3(0, 0, 0);
        }
        case DataFieldType::Vector4: {
            if (j.contains("value") && j["value"].is_array() && j["value"].size() >= 4) {
                return Math::Vector4(j["value"][0].get<f32>(),
                                     j["value"][1].get<f32>(),
                                     j["value"][2].get<f32>(),
                                     j["value"][3].get<f32>());
            }
            return Math::Vector4(0, 0, 0, 0);
        }
        case DataFieldType::StringArray: {
            std::vector<std::string> arr;
            if (j.contains("value") && j["value"].is_array()) {
                for (const auto& elem : j["value"]) {
                    arr.push_back(elem.get<std::string>());
                }
            }
            return arr;
        }
        case DataFieldType::FloatArray: {
            std::vector<f32> arr;
            if (j.contains("value") && j["value"].is_array()) {
                for (const auto& elem : j["value"]) {
                    arr.push_back(elem.get<f32>());
                }
            }
            return arr;
        }
    }
    return std::string("");
}

static DataFieldType DetectFieldType(const DataAssetValue& value) {
    if (std::holds_alternative<std::string>(value))              return DataFieldType::String;
    if (std::holds_alternative<f32>(value))                      return DataFieldType::Float;
    if (std::holds_alternative<i32>(value))                      return DataFieldType::Int;
    if (std::holds_alternative<bool>(value))                     return DataFieldType::Bool;
    if (std::holds_alternative<Math::Vector3>(value))            return DataFieldType::Vector3;
    if (std::holds_alternative<Math::Vector4>(value))            return DataFieldType::Vector4;
    if (std::holds_alternative<std::vector<std::string>>(value)) return DataFieldType::StringArray;
    if (std::holds_alternative<std::vector<f32>>(value))         return DataFieldType::FloatArray;
    return DataFieldType::String;
}

static json SerializeSchema(const DataAssetSchema& schema) {
    json j;
    j["name"] = schema.name;
    j["description"] = schema.description;
    j["fields"] = json::array();
    for (const auto& field : schema.fields) {
        json fj;
        fj["name"] = field.name;
        fj["type"] = DataFieldTypeToString(field.type);
        fj["default"] = SerializeValue(field.defaultValue, field.type);
        j["fields"].push_back(fj);
    }
    return j;
}

static DataAssetSchema DeserializeSchema(const json& j) {
    DataAssetSchema schema;
    schema.name = j.value("name", "");
    schema.description = j.value("description", "");
    if (j.contains("fields") && j["fields"].is_array()) {
        for (const auto& fj : j["fields"]) {
            DataAssetField field;
            field.name = fj.value("name", "");
            field.type = DataFieldTypeFromString(fj.value("type", "String"));
            if (fj.contains("default")) {
                field.defaultValue = DeserializeValue(fj["default"]);
            } else {
                // Provide a default value based on type
                switch (field.type) {
                    case DataFieldType::String:      field.defaultValue = std::string(""); break;
                    case DataFieldType::Float:        field.defaultValue = 0.0f; break;
                    case DataFieldType::Int:          field.defaultValue = 0; break;
                    case DataFieldType::Bool:         field.defaultValue = false; break;
                    case DataFieldType::Vector3:      field.defaultValue = Math::Vector3(0,0,0); break;
                    case DataFieldType::Vector4:      field.defaultValue = Math::Vector4(0,0,0,0); break;
                    case DataFieldType::StringArray:  field.defaultValue = std::vector<std::string>{}; break;
                    case DataFieldType::FloatArray:   field.defaultValue = std::vector<f32>{}; break;
                }
            }
            schema.fields.push_back(field);
        }
    }
    return schema;
}

static json SerializeAsset(const DataAsset& asset) {
    json j;
    j["name"] = asset.name;
    j["schema"] = asset.schemaName;
    j["values"] = json::object();
    for (const auto& [key, value] : asset.values) {
        j["values"][key] = SerializeValue(value, DetectFieldType(value));
    }
    return j;
}

static DataAsset DeserializeAsset(const json& j) {
    DataAsset asset;
    asset.name = j.value("name", "");
    asset.schemaName = j.value("schema", "");
    if (j.contains("values") && j["values"].is_object()) {
        for (auto& [key, vj] : j["values"].items()) {
            asset.values[key] = DeserializeValue(vj);
        }
    }
    return asset;
}

// ============================================================================
// SCHEMA MANAGEMENT
// ============================================================================

void DataAssetRegistry::RegisterSchema(const DataAssetSchema& schema) {
    m_Schemas[schema.name] = schema;
    ENJIN_LOG_INFO(Script, "Registered DataAsset schema: %s (%zu fields)",
                   schema.name.c_str(), schema.fields.size());
}

void DataAssetRegistry::RemoveSchema(const std::string& name) {
    m_Schemas.erase(name);
}

const DataAssetSchema* DataAssetRegistry::FindSchema(const std::string& name) const {
    auto it = m_Schemas.find(name);
    return it != m_Schemas.end() ? &it->second : nullptr;
}

std::vector<const DataAssetSchema*> DataAssetRegistry::GetAllSchemas() const {
    std::vector<const DataAssetSchema*> result;
    result.reserve(m_Schemas.size());
    for (const auto& [name, schema] : m_Schemas) {
        result.push_back(&schema);
    }
    return result;
}

// ============================================================================
// SCHEMA I/O
// ============================================================================

bool DataAssetRegistry::SaveSchema(const DataAssetSchema& schema, const std::string& path) {
    try {
        json j = Assets::SerializeSchema(schema);
        std::ofstream file(path);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Script, "Failed to save schema to: %s", path.c_str());
            return false;
        }
        file << j.dump(2);
        ENJIN_LOG_INFO(Script, "Saved schema '%s' to %s", schema.name.c_str(), path.c_str());
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Script, "Failed to serialize schema: %s", e.what());
        return false;
    }
}

bool DataAssetRegistry::LoadSchema(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Script, "Failed to open schema file: %s", path.c_str());
            return false;
        }
        json j;
        file >> j;
        DataAssetSchema schema = Assets::DeserializeSchema(j);
        if (schema.name.empty()) {
            ENJIN_LOG_ERROR(Script, "Schema file missing 'name' field: %s", path.c_str());
            return false;
        }
        RegisterSchema(schema);
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Script, "Failed to parse schema file '%s': %s", path.c_str(), e.what());
        return false;
    }
}

void DataAssetRegistry::ScanSchemaDirectory(const std::string& directory) {
    if (!fs::exists(directory) || !fs::is_directory(directory)) return;
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".enjschema") {
            LoadSchema(entry.path().string());
        }
    }
}

// ============================================================================
// ASSET MANAGEMENT
// ============================================================================

void DataAssetRegistry::CreateAsset(const DataAsset& asset) {
    m_Assets[asset.name] = asset;
    ENJIN_LOG_INFO(Script, "Created DataAsset: %s (schema: %s)",
                   asset.name.c_str(), asset.schemaName.c_str());
}

void DataAssetRegistry::RemoveAsset(const std::string& name) {
    m_Assets.erase(name);
}

const DataAsset* DataAssetRegistry::FindAsset(const std::string& name) const {
    auto it = m_Assets.find(name);
    return it != m_Assets.end() ? &it->second : nullptr;
}

DataAsset* DataAssetRegistry::FindAssetMut(const std::string& name) {
    auto it = m_Assets.find(name);
    return it != m_Assets.end() ? &it->second : nullptr;
}

std::vector<const DataAsset*> DataAssetRegistry::GetAssetsBySchema(const std::string& schemaName) const {
    std::vector<const DataAsset*> result;
    for (const auto& [name, asset] : m_Assets) {
        if (asset.schemaName == schemaName) {
            result.push_back(&asset);
        }
    }
    return result;
}

std::vector<const DataAsset*> DataAssetRegistry::GetAllAssets() const {
    std::vector<const DataAsset*> result;
    result.reserve(m_Assets.size());
    for (const auto& [name, asset] : m_Assets) {
        result.push_back(&asset);
    }
    return result;
}

// ============================================================================
// ASSET I/O
// ============================================================================

bool DataAssetRegistry::SaveAsset(const DataAsset& asset, const std::string& path) {
    try {
        json j = Assets::SerializeAsset(asset);
        std::ofstream file(path);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Script, "Failed to save data asset to: %s", path.c_str());
            return false;
        }
        file << j.dump(2);
        ENJIN_LOG_INFO(Script, "Saved data asset '%s' to %s", asset.name.c_str(), path.c_str());
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Script, "Failed to serialize data asset: %s", e.what());
        return false;
    }
}

bool DataAssetRegistry::LoadAsset(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Script, "Failed to open data asset file: %s", path.c_str());
            return false;
        }
        json j;
        file >> j;
        DataAsset asset = Assets::DeserializeAsset(j);
        if (asset.name.empty()) {
            ENJIN_LOG_ERROR(Script, "Data asset file missing 'name' field: %s", path.c_str());
            return false;
        }
        asset.filePath = path;
        CreateAsset(asset);
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Script, "Failed to parse data asset '%s': %s", path.c_str(), e.what());
        return false;
    }
}

void DataAssetRegistry::ScanAssetDirectory(const std::string& directory) {
    if (!fs::exists(directory) || !fs::is_directory(directory)) return;
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".enjdata") {
            LoadAsset(entry.path().string());
        }
    }
}

// ============================================================================
// TYPED GETTERS
// ============================================================================

f32 DataAssetRegistry::GetFloat(const std::string& assetName, const std::string& field, f32 fallback) const {
    const DataAsset* asset = FindAsset(assetName);
    if (!asset) return fallback;
    auto it = asset->values.find(field);
    if (it == asset->values.end()) return fallback;
    if (std::holds_alternative<f32>(it->second)) return std::get<f32>(it->second);
    if (std::holds_alternative<i32>(it->second)) return static_cast<f32>(std::get<i32>(it->second));
    return fallback;
}

i32 DataAssetRegistry::GetInt(const std::string& assetName, const std::string& field, i32 fallback) const {
    const DataAsset* asset = FindAsset(assetName);
    if (!asset) return fallback;
    auto it = asset->values.find(field);
    if (it == asset->values.end()) return fallback;
    if (std::holds_alternative<i32>(it->second)) return std::get<i32>(it->second);
    if (std::holds_alternative<f32>(it->second)) return static_cast<i32>(std::get<f32>(it->second));
    return fallback;
}

bool DataAssetRegistry::GetBool(const std::string& assetName, const std::string& field, bool fallback) const {
    const DataAsset* asset = FindAsset(assetName);
    if (!asset) return fallback;
    auto it = asset->values.find(field);
    if (it == asset->values.end()) return fallback;
    if (std::holds_alternative<bool>(it->second)) return std::get<bool>(it->second);
    return fallback;
}

std::string DataAssetRegistry::GetString(const std::string& assetName, const std::string& field, const std::string& fallback) const {
    const DataAsset* asset = FindAsset(assetName);
    if (!asset) return fallback;
    auto it = asset->values.find(field);
    if (it == asset->values.end()) return fallback;
    if (std::holds_alternative<std::string>(it->second)) return std::get<std::string>(it->second);
    return fallback;
}

Math::Vector3 DataAssetRegistry::GetVector3(const std::string& assetName, const std::string& field, Math::Vector3 fallback) const {
    const DataAsset* asset = FindAsset(assetName);
    if (!asset) return fallback;
    auto it = asset->values.find(field);
    if (it == asset->values.end()) return fallback;
    if (std::holds_alternative<Math::Vector3>(it->second)) return std::get<Math::Vector3>(it->second);
    return fallback;
}

// ============================================================================
// TYPED SETTERS
// ============================================================================

void DataAssetRegistry::SetFloat(const std::string& assetName, const std::string& field, f32 value) {
    DataAsset* asset = FindAssetMut(assetName);
    if (asset) asset->values[field] = value;
}

void DataAssetRegistry::SetInt(const std::string& assetName, const std::string& field, i32 value) {
    DataAsset* asset = FindAssetMut(assetName);
    if (asset) asset->values[field] = value;
}

void DataAssetRegistry::SetBool(const std::string& assetName, const std::string& field, bool value) {
    DataAsset* asset = FindAssetMut(assetName);
    if (asset) asset->values[field] = value;
}

void DataAssetRegistry::SetString(const std::string& assetName, const std::string& field, const std::string& value) {
    DataAsset* asset = FindAssetMut(assetName);
    if (asset) asset->values[field] = value;
}

void DataAssetRegistry::SetVector3(const std::string& assetName, const std::string& field, Math::Vector3 value) {
    DataAsset* asset = FindAssetMut(assetName);
    if (asset) asset->values[field] = value;
}

// ============================================================================
// CLEAR
// ============================================================================

void DataAssetRegistry::Clear() {
    m_Schemas.clear();
    m_Assets.clear();
}

} // namespace Assets
} // namespace Enjin
