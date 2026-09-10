#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
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

// What a directory scan actually did. Returned rather than logged-and-forgotten
// so a view can SAY what it searched: a scan of the wrong tree and a project with
// no records are otherwise the same silence, and telling them apart by hand cost
// a session (2026-09-10, Ink Ribbon).
struct DataAssetScanResult {
    std::string directory;        // absolute path actually searched
    bool directoryExists = false;
    usize filesFound = 0;         // files with the right extension
    usize loaded = 0;             // of those, the ones that parsed
};

class ENJIN_API DataAssetRegistry {
public:
    static DataAssetRegistry& Get();

    // Bumped by every mutation. A view caches against this instead of growing its
    // own dirty flag, which is the difference between one panel being correct and
    // every panel being correct: a dirty flag on the VIEWER only works while that
    // viewer is the sole mutator, and a second view of the same data is stale from
    // the moment it opens.
    u64 Version() const { return m_Version; }

    // Schema management
    void RegisterSchema(const DataAssetSchema& schema);
    void RemoveSchema(const std::string& name);
    const DataAssetSchema* FindSchema(const std::string& name) const;
    std::vector<const DataAssetSchema*> GetAllSchemas() const;

    // Schema I/O
    bool SaveSchema(const DataAssetSchema& schema, const std::string& path);
    bool LoadSchema(const std::string& path);
    // Parse from text rather than from a path, so a caller reading out of an
    // .enjpak and a caller reading off disk share ONE parser. `sourcePath` is
    // recorded and used in messages; it is never opened.
    bool LoadSchemaFromString(const std::string& text, const std::string& sourcePath);
    DataAssetScanResult ScanSchemaDirectory(const std::string& directory);

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
    bool LoadAssetFromString(const std::string& text, const std::string& sourcePath);
    DataAssetScanResult ScanAssetDirectory(const std::string& directory);

    // Typed getters (with fallback defaults)
    f32 GetFloat(const std::string& assetName, const std::string& field, f32 fallback = 0.0f) const;
    i32 GetInt(const std::string& assetName, const std::string& field, i32 fallback = 0) const;
    bool GetBool(const std::string& assetName, const std::string& field, bool fallback = false) const;
    std::string GetString(const std::string& assetName, const std::string& field, const std::string& fallback = "") const;
    Math::Vector3 GetVector3(const std::string& assetName, const std::string& field, Math::Vector3 fallback = {0,0,0}) const;
    Math::Vector4 GetVector4(const std::string& assetName, const std::string& field, Math::Vector4 fallback = {0,0,0,0}) const;

    // --- Arrays -------------------------------------------------------------
    //
    // DataAssetValue carries eight types and the serializer round-trips all
    // eight, but only five had a getter -- so a format that can hold a list had
    // no way to give one back, and Vector4 was unreachable too. The workarounds
    // were indexed keys (cue0_t, cue1_t, read until empty) or one delimited
    // string parsed by hand, both of which work around a capability the format
    // already has. This is not caption-specific: waypoints, loot tables,
    // dialogue, spawn sets and schedules hit the same wall.
    //
    // Scalar accessors rather than an array<T> return, deliberately. Nothing in
    // the engine returns array<T> to script today; CScriptArray is included and
    // read from script class properties, but nothing constructs one and hands it
    // back. Doing so brings ownership and refcount questions and sets an
    // engine-wide pattern, which is worth deciding on its own rather than as a
    // side effect of a caption loader. These need none of it and can be
    // superseded if array returns are ever adopted broadly.

    // Element count, or 0 when the asset is missing, the field is missing, or
    // the field is not an array. All three mean "nothing to iterate", and a
    // caller writing `for (i = 0; i < len; ++i)` is correct in every one of them
    // -- which is why this is 0 and not a -1 that every caller must remember to
    // special-case.
    usize GetArrayLength(const std::string& assetName, const std::string& field) const;

    // Element reads. An out-of-range or wrong-type read returns the fallback AND
    // warns once per asset+field, because a silently empty string is
    // indistinguishable from authored empty text -- the same reason Audio_GetTime
    // answers -1 rather than 0.0 when it cannot say where a sound is.
    std::string GetStringAt(const std::string& assetName, const std::string& field,
                            usize index, const std::string& fallback = "") const;
    f32 GetFloatAt(const std::string& assetName, const std::string& field,
                   usize index, f32 fallback = 0.0f) const;

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
    u64 m_Version = 0;

    // Which asset+field pairs have already reported a bad element read, so a
    // per-frame loop over a mis-authored field warns once instead of every
    // frame. Mutable because the read accessors are const and diagnosing a bad
    // read is not a change to the data.
    mutable std::unordered_set<std::string> m_WarnedArrayReads;
    void WarnOnceAboutArray(const std::string& assetName, const std::string& field,
                            const std::string& what) const;
};

} // namespace Assets
} // namespace Enjin
