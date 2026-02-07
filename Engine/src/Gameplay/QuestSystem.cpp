#include "Enjin/Gameplay/QuestSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include <imgui.h>

namespace Enjin {
namespace Gameplay {

void QuestSystem::Update(ECS::World* world, f32 deltaTime) {
    if (!m_Enabled || !world) return;

    auto entities = world->GetEntitiesWithComponent<ECS::QuestStateComponent>();
    for (auto entity : entities) {
        auto* quest = world->GetComponent<ECS::QuestStateComponent>(entity);
        if (!quest) continue;

        if (quest->status == ECS::QuestStateComponent::Status::Active) {
            quest->timeElapsed += deltaTime;

            // Check if all objectives are complete
            bool allDone = true;
            for (const auto& [name, complete] : quest->objectiveFlags) {
                if (!complete) { allDone = false; break; }
            }
            if (allDone && !quest->objectiveFlags.empty()) {
                quest->status = ECS::QuestStateComponent::Status::Completed;
            }
        }
    }
}

void QuestSystem::StartQuest(ECS::World* world, const std::string& questId) {
    if (!world) return;
    auto entities = world->GetEntitiesWithComponent<ECS::QuestStateComponent>();
    for (auto entity : entities) {
        auto* quest = world->GetComponent<ECS::QuestStateComponent>(entity);
        if (quest && quest->questId == questId && quest->status == ECS::QuestStateComponent::Status::NotStarted) {
            quest->status = ECS::QuestStateComponent::Status::Active;
            quest->timeElapsed = 0.0f;
        }
    }
}

void QuestSystem::CompleteObjective(ECS::World* world, const std::string& questId, i32 objectiveIndex) {
    if (!world) return;
    auto entities = world->GetEntitiesWithComponent<ECS::QuestStateComponent>();
    for (auto entity : entities) {
        auto* quest = world->GetComponent<ECS::QuestStateComponent>(entity);
        if (quest && quest->questId == questId && quest->status == ECS::QuestStateComponent::Status::Active) {
            if (objectiveIndex >= 0 && objectiveIndex < static_cast<i32>(quest->objectiveFlags.size())) {
                quest->objectiveFlags[objectiveIndex].second = true;
                quest->currentObjective = std::min(objectiveIndex + 1, static_cast<i32>(quest->objectiveFlags.size()));
            }
        }
    }
}

void QuestSystem::FailQuest(ECS::World* world, const std::string& questId) {
    if (!world) return;
    auto entities = world->GetEntitiesWithComponent<ECS::QuestStateComponent>();
    for (auto entity : entities) {
        auto* quest = world->GetComponent<ECS::QuestStateComponent>(entity);
        if (quest && quest->questId == questId) {
            quest->status = ECS::QuestStateComponent::Status::Failed;
        }
    }
}

bool QuestSystem::IsQuestActive(ECS::World* world, const std::string& questId) const {
    if (!world) return false;
    auto entities = world->GetEntitiesWithComponent<ECS::QuestStateComponent>();
    for (auto entity : entities) {
        auto* quest = world->GetComponent<ECS::QuestStateComponent>(entity);
        if (quest && quest->questId == questId) {
            return quest->status == ECS::QuestStateComponent::Status::Active;
        }
    }
    return false;
}

bool QuestSystem::IsQuestComplete(ECS::World* world, const std::string& questId) const {
    if (!world) return false;
    auto entities = world->GetEntitiesWithComponent<ECS::QuestStateComponent>();
    for (auto entity : entities) {
        auto* quest = world->GetComponent<ECS::QuestStateComponent>(entity);
        if (quest && quest->questId == questId) {
            return quest->status == ECS::QuestStateComponent::Status::Completed;
        }
    }
    return false;
}

void QuestSystem::DrawQuestLog(f32 viewportX, f32 viewportY, f32 viewportW, f32 viewportH) {
    if (!m_World || !m_ShowQuestLog) return;

    auto entities = m_World->GetEntitiesWithComponent<ECS::QuestStateComponent>();

    // Collect active quests
    std::vector<ECS::QuestStateComponent*> activeQuests;
    for (auto entity : entities) {
        auto* quest = m_World->GetComponent<ECS::QuestStateComponent>(entity);
        if (quest && quest->status == ECS::QuestStateComponent::Status::Active) {
            activeQuests.push_back(quest);
        }
    }

    if (activeQuests.empty()) return;

    // Draw quest log overlay in top-right corner of the viewport
    f32 logWidth = 250.0f;
    f32 logPadding = 10.0f;
    f32 logX = viewportX + viewportW - logWidth - logPadding;
    f32 logY = viewportY + logPadding;

    ImGui::SetNextWindowPos(ImVec2(logX, logY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(logWidth, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing;

    if (ImGui::Begin("##QuestLog", nullptr, flags)) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "Quest Log");
        ImGui::Separator();

        for (auto* quest : activeQuests) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", quest->questId.c_str());

            for (usize i = 0; i < quest->objectiveFlags.size(); i++) {
                const auto& [name, complete] = quest->objectiveFlags[i];
                if (complete) {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "  [x] %s", name.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  [ ] %s", name.c_str());
                }
            }

            ImGui::Spacing();
        }
    }
    ImGui::End();
}

} // namespace Gameplay
} // namespace Enjin
