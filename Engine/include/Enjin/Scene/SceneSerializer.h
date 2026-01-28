#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
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
};

// Scene Serializer - Saves and loads scenes to/from JSON files
class ENJIN_API SceneSerializer {
public:
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

    // Get/Set world
    void SetWorld(ECS::World* world) { m_World = world; }
    ECS::World* GetWorld() const { return m_World; }

private:
    ECS::World* m_World = nullptr;
};

} // namespace Scene
} // namespace Enjin
