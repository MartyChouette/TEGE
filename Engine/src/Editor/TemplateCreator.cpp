#include "Enjin/Editor/TemplateCreator.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

namespace Enjin {
namespace Editor {

// ---------------------------------------------------------------------------
// SanitizeId
// ---------------------------------------------------------------------------
std::string TemplateCreator::SanitizeId(const std::string& name) {
    std::string id = name;
    // Convert to lowercase and replace unsafe chars
    for (char& c : id) {
        if (c == ' ') { c = '_'; continue; }
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
            continue;
        }
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }
    // Remove leading/trailing underscores
    while (!id.empty() && id.front() == '_') id.erase(id.begin());
    while (!id.empty() && id.back() == '_') id.pop_back();
    if (id.empty()) id = "template";
    return id;
}

// ---------------------------------------------------------------------------
// SaveTemplate
// ---------------------------------------------------------------------------
bool TemplateCreator::SaveTemplate(const std::string& outputDir, const TemplateMetadata& meta,
                                   ECS::World* world, Scene::SceneSerializer& serializer) {
    if (!world) {
        ENJIN_LOG_ERROR(Editor, "TemplateCreator::SaveTemplate — null world");
        return false;
    }

    // Determine template id
    std::string id = meta.id.empty() ? SanitizeId(meta.name) : meta.id;

    // Create template directory
    std::filesystem::path templateDir = std::filesystem::path(outputDir) / id;
    try {
        std::filesystem::create_directories(templateDir);
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "TemplateCreator: failed to create directory %s — %s",
                        templateDir.string().c_str(), e.what());
        return false;
    }

    // Save the scene
    std::string scenePath = (templateDir / "scene.enjin").string();
    Scene::SerializationOptions opts;
    opts.includeVertexData = true;
    opts.prettyPrint = true;
    auto result = serializer.Save(scenePath, opts);
    if (!result.success) {
        ENJIN_LOG_ERROR(Editor, "TemplateCreator: failed to save scene — %s", result.error.c_str());
        return false;
    }

    // Write meta.json
    json j;
    j["id"] = id;
    j["name"] = meta.name;
    j["description"] = meta.description;
    j["category"] = meta.category;
    j["author"] = meta.author;
    j["accentColor"] = { meta.accentColor.x, meta.accentColor.y, meta.accentColor.z, meta.accentColor.w };
    j["thumbnailPath"] = meta.thumbnailPath;

    std::string metaPath = (templateDir / "meta.json").string();
    try {
        std::ofstream ofs(metaPath);
        if (!ofs.is_open()) {
            ENJIN_LOG_ERROR(Editor, "TemplateCreator: failed to open %s for writing", metaPath.c_str());
            return false;
        }
        ofs << j.dump(2);
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "TemplateCreator: failed to write meta.json — %s", e.what());
        return false;
    }

    // Copy thumbnail if provided
    if (!meta.thumbnailPath.empty()) {
        try {
            std::filesystem::path srcThumb(meta.thumbnailPath);
            if (std::filesystem::exists(srcThumb)) {
                std::filesystem::path dstThumb = templateDir / "thumbnail.png";
                std::filesystem::copy_file(srcThumb, dstThumb,
                    std::filesystem::copy_options::overwrite_existing);
            }
        } catch (const std::exception& e) {
            ENJIN_LOG_WARN(Editor, "TemplateCreator: failed to copy thumbnail — %s", e.what());
            // Non-fatal
        }
    }

    ENJIN_LOG_INFO(Editor, "Template saved: %s (%s)", meta.name.c_str(), templateDir.string().c_str());
    return true;
}

// ---------------------------------------------------------------------------
// LoadTemplate
// ---------------------------------------------------------------------------
bool TemplateCreator::LoadTemplate(const std::string& templatePath,
                                   ECS::World* world, Scene::SceneSerializer& serializer,
                                   TemplateMetadata& outMeta) {
    if (!world) {
        ENJIN_LOG_ERROR(Editor, "TemplateCreator::LoadTemplate — null world");
        return false;
    }

    std::filesystem::path dir(templatePath);

    // Read meta.json
    std::string metaPath = (dir / "meta.json").string();
    try {
        std::ifstream ifs(metaPath);
        if (!ifs.is_open()) {
            ENJIN_LOG_ERROR(Editor, "TemplateCreator: meta.json not found at %s", metaPath.c_str());
            return false;
        }
        json j = json::parse(ifs);

        outMeta.id = j.value("id", "");
        outMeta.name = j.value("name", "Unnamed");
        outMeta.description = j.value("description", "");
        outMeta.category = j.value("category", "");
        outMeta.author = j.value("author", "");

        if (j.contains("accentColor") && j["accentColor"].is_array() && j["accentColor"].size() >= 4) {
            outMeta.accentColor.x = j["accentColor"][0].get<f32>();
            outMeta.accentColor.y = j["accentColor"][1].get<f32>();
            outMeta.accentColor.z = j["accentColor"][2].get<f32>();
            outMeta.accentColor.w = j["accentColor"][3].get<f32>();
        }

        outMeta.thumbnailPath = j.value("thumbnailPath", "");
        outMeta.isBuiltin = false;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "TemplateCreator: failed to parse meta.json — %s", e.what());
        return false;
    }

    // Load the scene
    std::string scenePath = (dir / "scene.enjin").string();
    if (!std::filesystem::exists(scenePath)) {
        ENJIN_LOG_ERROR(Editor, "TemplateCreator: scene.enjin not found at %s", scenePath.c_str());
        return false;
    }

    auto result = serializer.Load(scenePath, true);
    if (!result.success) {
        ENJIN_LOG_ERROR(Editor, "TemplateCreator: failed to load scene — %s", result.error.c_str());
        return false;
    }

    ENJIN_LOG_INFO(Editor, "Template loaded: %s (%zu entities)", outMeta.name.c_str(), result.entities.size());
    return true;
}

// ---------------------------------------------------------------------------
// ScanTemplates
// ---------------------------------------------------------------------------
std::vector<TemplateMetadata> TemplateCreator::ScanTemplates(const std::string& templatesDir) {
    std::vector<TemplateMetadata> templates;

    std::filesystem::path dir(templatesDir);
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        return templates;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_directory()) continue;

            std::filesystem::path metaFile = entry.path() / "meta.json";
            if (!std::filesystem::exists(metaFile)) continue;

            try {
                std::ifstream ifs(metaFile.string());
                if (!ifs.is_open()) continue;

                json j = json::parse(ifs);

                TemplateMetadata meta;
                meta.id = j.value("id", entry.path().filename().string());
                meta.name = j.value("name", meta.id);
                meta.description = j.value("description", "");
                meta.category = j.value("category", "");
                meta.author = j.value("author", "");

                if (j.contains("accentColor") && j["accentColor"].is_array() && j["accentColor"].size() >= 4) {
                    meta.accentColor.x = j["accentColor"][0].get<f32>();
                    meta.accentColor.y = j["accentColor"][1].get<f32>();
                    meta.accentColor.z = j["accentColor"][2].get<f32>();
                    meta.accentColor.w = j["accentColor"][3].get<f32>();
                }

                meta.thumbnailPath = j.value("thumbnailPath", "");
                meta.isBuiltin = false;

                templates.push_back(std::move(meta));
            } catch (const std::exception&) {
                // Skip malformed templates
                continue;
            }
        }
    } catch (const std::exception& e) {
        ENJIN_LOG_WARN(Editor, "TemplateCreator: error scanning templates dir — %s", e.what());
    }

    // Sort alphabetically by name
    std::sort(templates.begin(), templates.end(),
        [](const TemplateMetadata& a, const TemplateMetadata& b) { return a.name < b.name; });

    return templates;
}

// ---------------------------------------------------------------------------
// DeleteTemplate
// ---------------------------------------------------------------------------
bool TemplateCreator::DeleteTemplate(const std::string& templatesDir, const std::string& templateId) {
    if (templateId.empty()) return false;

    std::filesystem::path dir = std::filesystem::path(templatesDir) / templateId;
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        ENJIN_LOG_WARN(Editor, "TemplateCreator: template directory not found — %s", dir.string().c_str());
        return false;
    }

    // Safety: only delete if it contains meta.json (avoid accidentally deleting wrong dirs)
    if (!std::filesystem::exists(dir / "meta.json")) {
        ENJIN_LOG_ERROR(Editor, "TemplateCreator: refusing to delete — no meta.json in %s", dir.string().c_str());
        return false;
    }

    try {
        std::filesystem::remove_all(dir);
        ENJIN_LOG_INFO(Editor, "Template deleted: %s", templateId.c_str());
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "TemplateCreator: failed to delete template — %s", e.what());
        return false;
    }
}

} // namespace Editor
} // namespace Enjin
