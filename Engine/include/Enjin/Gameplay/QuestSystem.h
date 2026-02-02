#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Gameplay {

class ENJIN_API QuestSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }

    void Update(ECS::World* world, f32 deltaTime);

    // Quest management
    void StartQuest(ECS::World* world, const std::string& questId);
    void CompleteObjective(ECS::World* world, const std::string& questId, i32 objectiveIndex);
    void FailQuest(ECS::World* world, const std::string& questId);

    // Query
    bool IsQuestActive(ECS::World* world, const std::string& questId) const;
    bool IsQuestComplete(ECS::World* world, const std::string& questId) const;

    // HUD overlay (draws active quests in corner during play)
    void DrawQuestLog(f32 viewportX, f32 viewportY, f32 viewportW, f32 viewportH);

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

private:
    ECS::World* m_World = nullptr;
    bool m_Enabled = false;
    bool m_ShowQuestLog = true;
};

} // namespace Gameplay
} // namespace Enjin
