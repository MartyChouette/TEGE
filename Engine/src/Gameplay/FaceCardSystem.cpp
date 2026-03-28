#include "Enjin/Gameplay/FaceCardSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Gameplay {

void FaceCardSystem::Update(f32 deltaTime) {
    if (!m_World) return;

    auto entities = m_World->GetEntitiesWithComponent<ECS::FaceCardComponent>();
    for (auto entity : entities) {
        auto* fc = m_World->GetComponent<ECS::FaceCardComponent>(entity);
        if (!fc || !fc->enabled) continue;
        if (fc->currentExpression.empty()) continue;

        // Detect expression change
        if (fc->currentExpression != fc->previousExpression) {
            // Look up the texture path for the new expression
            auto it = fc->expressions.find(fc->currentExpression);
            if (it == fc->expressions.end()) {
                ENJIN_LOG_WARN(Game, "FaceCard: expression '%s' not found on entity %llu",
                    fc->currentExpression.c_str(), entity);
                continue;
            }

            const std::string& texturePath = it->second;

            // Update MaterialComponent texture (the primary rendering path)
            auto* mat = m_World->GetComponent<ECS::MaterialComponent>(entity);
            if (mat) {
                mat->baseColorTexturePath = texturePath;
                mat->InvalidateTextureCache();
            }

            // Also update Sprite2DComponent texture if present
            auto* sprite = m_World->GetComponent<ECS::Sprite2DComponent>(entity);
            if (sprite) {
                sprite->texturePath = texturePath;
                sprite->spriteDirty = true;
            }

            // Start crossfade transition if duration > 0
            if (fc->transitionDuration > 0.0f) {
                fc->transitionTimer = fc->transitionDuration;
                // Start at low opacity for crossfade-in
                if (mat) mat->opacity = 0.0f;
            }

            fc->previousExpression = fc->currentExpression;
        }

        // Update crossfade transition
        if (fc->transitionTimer > 0.0f && fc->transitionDuration > 0.0f) {
            fc->transitionTimer -= deltaTime;
            if (fc->transitionTimer <= 0.0f) {
                fc->transitionTimer = 0.0f;
                // Transition complete — full opacity
                auto* mat = m_World->GetComponent<ECS::MaterialComponent>(entity);
                if (mat) mat->opacity = 1.0f;
            } else {
                // Lerp opacity from 0 to 1 over transitionDuration
                f32 progress = 1.0f - (fc->transitionTimer / fc->transitionDuration);
                auto* mat = m_World->GetComponent<ECS::MaterialComponent>(entity);
                if (mat) mat->opacity = progress;
            }
        }

        // Apply flipX to Sprite2DComponent if present
        auto* sprite = m_World->GetComponent<ECS::Sprite2DComponent>(entity);
        if (sprite && sprite->flipX != fc->flipX) {
            sprite->flipX = fc->flipX;
            sprite->spriteDirty = true;
        }
    }
}

} // namespace Gameplay
} // namespace Enjin
