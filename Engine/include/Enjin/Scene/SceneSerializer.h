#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Accessibility/ContentWarning.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace Enjin {
namespace Scene {

// Scene serialization result
struct SerializationResult {
    bool success = false;
    std::string error;
    std::string filepath;
};

// Scene deserialization result
struct DeserializationResult {
    bool success = false;
    std::string error;
    std::string filepath;
    std::vector<ECS::Entity> entities;
    ECS::Entity rootEntity = ECS::INVALID_ENTITY;
};

// Scene serialization options
struct SerializationOptions {
    bool prettyPrint = true;        // Format JSON for readability
    bool includeVertexData = true;  // Include full mesh vertex/index data
    u32 indentSize = 2;             // JSON indent size (if prettyPrint)
    bool deterministic = true;      // Sort entities by ID, round floats for stable diffs
};

// Scene Serializer - Saves and loads scenes to/from JSON files
class ENJIN_API SceneSerializer {
public:
    // Scene format version — increment when component serialization formats change
    static constexpr u32 SCENE_FORMAT_VERSION = 1;

    SceneSerializer(ECS::World* world);
    ~SceneSerializer() = default;

    // Save all entities to a JSON file
    SerializationResult Save(const std::string& filepath, const SerializationOptions& options = SerializationOptions{});

    // Save specific entities to a JSON file
    SerializationResult SaveEntities(const std::string& filepath, const std::vector<ECS::Entity>& entities, const SerializationOptions& options = SerializationOptions{});

    // Load scene from a JSON file (clears existing entities first)
    DeserializationResult Load(const std::string& filepath, bool clearExisting = true);

    // Load and add entities from a JSON file (keeps existing entities)
    DeserializationResult LoadAdditive(const std::string& filepath);

    // Save to string (for in-memory operations like play mode)
    std::string SaveToString(const SerializationOptions& options = SerializationOptions{});

    // Load from string (for in-memory operations like play mode)
    DeserializationResult LoadFromString(const std::string& jsonString, bool clearExisting = true);

    // Get/Set world
    void SetWorld(ECS::World* world) { m_World = world; }
    ECS::World* GetWorld() const { return m_World; }

    // Per-entity serialization (for undo/redo, clipboard, duplicate)
    // Serializes all components of a single entity to a JSON string
    static std::string SerializeEntityToString(ECS::World* world, ECS::Entity entity);
    // Recreates an entity from a JSON string, returns the new entity ID
    static ECS::Entity DeserializeEntityFromString(ECS::World* world, const std::string& json);

    // Per-component serialization (for component remove undo)
    // Serializes a single component identified by its JSON key
    static std::string SerializeOneComponent(ECS::World* world, ECS::Entity entity, const std::string& key);
    // Deserializes and adds a single component from JSON
    static bool DeserializeOneComponent(ECS::World* world, ECS::Entity entity,
                                         const std::string& key, const std::string& json);

    // Scene-level accessibility content flags
    void SetContentFlags(const Accessibility::SceneContentFlags& flags) { m_ContentFlags = flags; }
    const Accessibility::SceneContentFlags& GetContentFlags() const { return m_ContentFlags; }
    Accessibility::SceneContentFlags& GetContentFlags() { return m_ContentFlags; }

    // Scene-level skybox configuration
    void SetSkyboxConfig(const Renderer::SkyboxConfig& config) { m_SkyboxConfig = config; }
    const Renderer::SkyboxConfig& GetSkyboxConfig() const { return m_SkyboxConfig; }

    // Scene-level render settings
    void SetRenderSettings(const Renderer::SceneRenderSettings& s) { m_RenderSettings = s; }
    const Renderer::SceneRenderSettings& GetRenderSettings() const { return m_RenderSettings; }

private:
    // Apply incremental migrations from fromVersion up to SCENE_FORMAT_VERSION
    static void MigrateScene(nlohmann::json& root, u32 fromVersion);

    ECS::World* m_World = nullptr;
    Accessibility::SceneContentFlags m_ContentFlags;
    Renderer::SkyboxConfig m_SkyboxConfig;
    Renderer::SceneRenderSettings m_RenderSettings;
};

} // namespace Scene
} // namespace Enjin
