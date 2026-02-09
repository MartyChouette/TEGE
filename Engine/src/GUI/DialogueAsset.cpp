#include "Enjin/GUI/DialogueAsset.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>

namespace Enjin {
namespace GUI {

std::string DialogueAsset::s_LastError;

bool DialogueAsset::Save(const std::string& filepath, const DialogueTreeData& tree) {
    s_LastError.clear();

    nlohmann::json wrapper;
    wrapper["version"] = 1;
    wrapper["type"] = "dialogue";
    wrapper["data"] = tree.ToJson();

    std::ofstream file(filepath);
    if (!file.is_open()) {
        s_LastError = "Failed to open file for writing: " + filepath;
        ENJIN_LOG_ERROR(Editor, "DialogueAsset: %s", s_LastError.c_str());
        return false;
    }

    file << wrapper.dump(2);
    if (!file.good()) {
        s_LastError = "Write error while saving: " + filepath;
        ENJIN_LOG_ERROR(Editor, "DialogueAsset: %s", s_LastError.c_str());
        return false;
    }

    ENJIN_LOG_INFO(Editor, "DialogueAsset: saved '%s' (%zu nodes)",
                   filepath.c_str(), tree.nodes.size());
    return true;
}

bool DialogueAsset::Load(const std::string& filepath, DialogueTreeData& outTree) {
    s_LastError.clear();

    std::ifstream file(filepath);
    if (!file.is_open()) {
        s_LastError = "Failed to open file for reading: " + filepath;
        ENJIN_LOG_ERROR(Editor, "DialogueAsset: %s", s_LastError.c_str());
        return false;
    }

    nlohmann::json wrapper;
    try {
        file >> wrapper;
    } catch (const nlohmann::json::parse_error& e) {
        s_LastError = "JSON parse error: " + std::string(e.what());
        ENJIN_LOG_ERROR(Editor, "DialogueAsset: %s", s_LastError.c_str());
        return false;
    }

    // Validate version
    if (!wrapper.contains("version") || !wrapper["version"].is_number_integer()) {
        s_LastError = "Missing or invalid 'version' field in: " + filepath;
        ENJIN_LOG_ERROR(Editor, "DialogueAsset: %s", s_LastError.c_str());
        return false;
    }

    i32 version = wrapper["version"].get<i32>();
    if (version != 1) {
        s_LastError = "Unsupported dialogue asset version: " + std::to_string(version);
        ENJIN_LOG_ERROR(Editor, "DialogueAsset: %s", s_LastError.c_str());
        return false;
    }

    // Validate type
    if (!wrapper.contains("type") || !wrapper["type"].is_string()) {
        s_LastError = "Missing or invalid 'type' field in: " + filepath;
        ENJIN_LOG_ERROR(Editor, "DialogueAsset: %s", s_LastError.c_str());
        return false;
    }

    std::string type = wrapper["type"].get<std::string>();
    if (type != "dialogue") {
        s_LastError = "Expected type 'dialogue', got '" + type + "' in: " + filepath;
        ENJIN_LOG_ERROR(Editor, "DialogueAsset: %s", s_LastError.c_str());
        return false;
    }

    // Validate data
    if (!wrapper.contains("data") || !wrapper["data"].is_object()) {
        s_LastError = "Missing or invalid 'data' field in: " + filepath;
        ENJIN_LOG_ERROR(Editor, "DialogueAsset: %s", s_LastError.c_str());
        return false;
    }

    try {
        outTree = DialogueTreeData::FromJson(wrapper["data"]);
    } catch (const std::exception& e) {
        s_LastError = "Failed to deserialize dialogue data: " + std::string(e.what());
        ENJIN_LOG_ERROR(Editor, "DialogueAsset: %s", s_LastError.c_str());
        return false;
    }

    ENJIN_LOG_INFO(Editor, "DialogueAsset: loaded '%s' (%zu nodes)",
                   filepath.c_str(), outTree.nodes.size());
    return true;
}

const std::string& DialogueAsset::GetLastError() {
    return s_LastError;
}

} // namespace GUI
} // namespace Enjin
