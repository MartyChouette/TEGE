#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/GUI/DialogueTree.h"
#include <unordered_map>
#include <functional>

namespace Enjin {
namespace ECS {

class ENJIN_API DialogueSystem {
public:
    DialogueSystem() = default;
    ~DialogueSystem() = default;

    // Main update — call every frame during play mode
    void Update(World* world, f32 deltaTime);

    // Start a tree-based dialogue on an entity
    void StartDialogue(World* world, Entity entity);

    // Advance past current text node (or skip typewriter)
    void Advance(World* world, Entity entity);

    // Select a choice by index
    void SelectChoice(World* world, Entity entity, u32 choiceIndex);

    // Variable access
    void SetVariable(World* world, Entity entity, const std::string& name, const std::string& value);
    std::string GetVariable(World* world, Entity entity, const std::string& name) const;

    // Query state
    bool IsActive(World* world, Entity entity) const;
    Entity GetActiveDialogueEntity() const { return m_ActiveEntity; }

    // Clear all players (call on play mode stop)
    void Clear();

    // Event callback for Event nodes
    using EventCallback = std::function<void(Entity, const std::string&)>;
    void SetEventCallback(EventCallback cb) { m_EventCallback = std::move(cb); }

private:
    std::unordered_map<Entity, GUI::DialoguePlayer> m_Players;
    Entity m_ActiveEntity = INVALID_ENTITY;
    EventCallback m_EventCallback;

    void SyncNodeToComponent(DialogueComponent& dlg, GUI::DialoguePlayer& player);
    void ProcessLegacy(World* world, Entity entity, DialogueComponent& dlg, f32 deltaTime);
};

} // namespace ECS
} // namespace Enjin
