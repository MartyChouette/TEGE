#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Scene/SceneSerializer.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Editor {

// Metadata for a custom template
struct TemplateMetadata {
    std::string id;             // Filesystem-safe identifier (directory name)
    std::string name;           // Display name
    std::string description;    // Brief description
    std::string category;       // "2D", "3D", "Multiplayer", "Tools"
    std::string author;
    Math::Vector4 accentColor = Math::Vector4(0.3f, 0.6f, 1.0f, 1.0f);
    std::string thumbnailPath;  // Optional PNG path (relative to template dir)
    bool isBuiltin = false;     // True for engine-shipped templates (read-only)
};

// Template export/management system
class ENJIN_API TemplateCreator {
public:
    // Save current scene as a template.
    // Creates {outputDir}/{meta.id}/ containing scene.enjin + meta.json + optional thumbnail.
    static bool SaveTemplate(const std::string& outputDir, const TemplateMetadata& meta,
                             ECS::World* world, Scene::SceneSerializer& serializer);

    // Load a custom template into the world.
    // templatePath is the directory containing scene.enjin + meta.json.
    static bool LoadTemplate(const std::string& templatePath,
                             ECS::World* world, Scene::SceneSerializer& serializer,
                             TemplateMetadata& outMeta);

    // Scan a directory for custom templates (each subdirectory with meta.json).
    static std::vector<TemplateMetadata> ScanTemplates(const std::string& templatesDir);

    // Delete a custom template by removing its directory.
    static bool DeleteTemplate(const std::string& templatesDir, const std::string& templateId);

    // Save a thumbnail PNG to an existing template directory.
    // pixels is RGBA8 data, width x height. Downscales to 280x180 if larger.
    static bool SaveThumbnail(const std::string& templatesDir, const std::string& templateId,
                              const u8* pixels, u32 width, u32 height);

private:
    // Sanitize a string for safe use as a directory name.
    static std::string SanitizeId(const std::string& name);
};

} // namespace Editor
} // namespace Enjin
