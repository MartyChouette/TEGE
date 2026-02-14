#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/GUI/DialogueTree.h"
#include "Enjin/GUI/UICanvas.h"
#include <unordered_map>
#include <functional>

namespace Enjin::Accessibility {
    class SubtitleSystem;
}

namespace Enjin {
namespace ECS {

class ENJIN_API DialogueSystem {
public:
    DialogueSystem() = default;
    ~DialogueSystem() = default;

    // Enable/disable dialogue updates
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

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

    // EntityEventBus integration (broadcasts Dialogue_<eventName> events)
    void SetEventBus(EntityEventBus* bus) { m_EventBus = bus; }

    // SubtitleSystem integration (routes text to accessibility subtitles)
    void SetSubtitleSystem(Accessibility::SubtitleSystem* subs) { m_SubtitleSystem = subs; }

private:
    bool m_Enabled = true;
    std::unordered_map<Entity, GUI::DialoguePlayer> m_Players;
    Entity m_ActiveEntity = INVALID_ENTITY;
    EventCallback m_EventCallback;
    EntityEventBus* m_EventBus = nullptr;
    Accessibility::SubtitleSystem* m_SubtitleSystem = nullptr;

    void SyncNodeToComponent(DialogueComponent& dlg, GUI::DialoguePlayer& player);
    void ProcessLegacy(World* world, Entity entity, DialogueComponent& dlg, f32 deltaTime);
    void BroadcastEvent(Entity entity, const std::string& eventName);
    void ShowSubtitle(const DialogueComponent& dlg);
    void BuildDialogueBoxUI(GUI::UICanvasComponent& canvas, DialogueBoxComponent& box);
    void SyncDialogueBoxUI(GUI::UICanvasComponent& canvas, DialogueBoxComponent& box,
                           const DialogueComponent& dlg, f32 deltaTime);
};

} // namespace ECS
} // namespace Enjin
