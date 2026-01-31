#include "Enjin/ECS/Systems/FlowerSystem.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include <cmath>
#include <cstdio>

namespace Enjin {
namespace ECS {

void FlowerSystem::SetGameViewBounds(f32 minX, f32 minY, f32 maxX, f32 maxY) {
    m_ViewMinX = minX;
    m_ViewMinY = minY;
    m_ViewMaxX = maxX;
    m_ViewMaxY = maxY;
}

void FlowerSystem::Update(f32 deltaTime) {
    if (!m_Enabled || !m_World) return;

    ProcessInput();
    UpdateTethers(deltaTime);
    UpdateJellyMeshes(deltaTime);
    CheckBreaks();
}

// ---------------------------------------------------------------------------
// Phase 1: ProcessInput
// ---------------------------------------------------------------------------
void FlowerSystem::ProcessInput() {
    if (!m_World) return;

    f32 viewW = m_ViewMaxX - m_ViewMinX;
    f32 viewH = m_ViewMaxY - m_ViewMinY;
    if (viewW <= 0.0f || viewH <= 0.0f) return;

    // Build game camera for picking
    if (m_GameCameraEntity == INVALID_ENTITY) return;
    auto* cameraComp = m_World->GetComponent<CameraComponent>(m_GameCameraEntity);
    auto* cameraTransform = m_World->GetComponent<TransformComponent>(m_GameCameraEntity);
    if (!cameraComp || !cameraTransform) return;

    Renderer::Camera pickCamera;
    f32 aspect = cameraComp->GetAspectRatio(m_RTWidth, m_RTHeight);
    if (cameraComp->projectionType == ProjectionType::Perspective) {
        pickCamera.SetPerspective(cameraComp->fieldOfView, aspect,
                                  cameraComp->nearPlane, cameraComp->farPlane);
    } else {
        f32 halfH = cameraComp->orthoSize;
        f32 halfW = halfH * aspect;
        pickCamera.SetOrthographic(-halfW, halfW, -halfH, halfH,
                                   cameraComp->nearPlane, cameraComp->farPlane);
    }
    pickCamera.SetPosition(cameraTransform->position);
    Math::Vector3 forward = cameraTransform->rotation.Rotate(Math::Vector3(0, 0, -1));
    Math::Vector3 up = cameraTransform->rotation.Rotate(Math::Vector3(0, 1, 0));
    pickCamera.SetLookAt(cameraTransform->position, cameraTransform->position + forward, up);

    Math::Vector2 mousePos = Input::GetMousePosition();

    // LMB pressed - try to pick a grabbable entity
    if (Input::IsMouseButtonPressed(MouseButton::Left)) {
        // Check mouse is inside game view bounds
        if (mousePos.x >= m_ViewMinX && mousePos.x <= m_ViewMaxX &&
            mousePos.y >= m_ViewMinY && mousePos.y <= m_ViewMaxY) {

            // Convert to game-view-local coordinates then to render target space
            f32 localX = (mousePos.x - m_ViewMinX) / viewW * static_cast<f32>(m_RTWidth);
            f32 localY = (mousePos.y - m_ViewMinY) / viewH * static_cast<f32>(m_RTHeight);

            Entity hit = Editor::ScenePicker::PickEntity(m_World, &pickCamera,
                                                         localX, localY,
                                                         static_cast<f32>(m_RTWidth),
                                                         static_cast<f32>(m_RTHeight));

            if (hit != INVALID_ENTITY && m_World->HasComponent<GrabbableComponent>(hit)) {
                auto* grab = m_World->GetComponent<GrabbableComponent>(hit);
                auto* hitTransform = m_World->GetComponent<TransformComponent>(hit);
                if (grab && hitTransform) {
                    grab->isGrabbed = true;
                    grab->grabWorldPoint = hitTransform->position;
                    grab->cursorWorldPoint = hitTransform->position;
                    m_GrabbedEntity = hit;

                    // Compute grab depth (distance from camera to entity along camera forward)
                    Math::Vector3 toEntity = hitTransform->position - cameraTransform->position;
                    m_GrabDepth = toEntity.Dot(forward);
                    if (m_GrabDepth < 0.1f) m_GrabDepth = 0.1f;
                }
            }
        }
    }

    // LMB released - clear grab
    if (Input::IsMouseButtonReleased(MouseButton::Left)) {
        if (m_GrabbedEntity != INVALID_ENTITY) {
            auto* grab = m_World->GetComponent<GrabbableComponent>(m_GrabbedEntity);
            if (grab) {
                grab->isGrabbed = false;
            }
            m_GrabbedEntity = INVALID_ENTITY;
        }
    }

    // LMB held - update cursor world position
    if (m_GrabbedEntity != INVALID_ENTITY && Input::IsMouseButtonDown(MouseButton::Left)) {
        auto* grab = m_World->GetComponent<GrabbableComponent>(m_GrabbedEntity);
        if (grab) {
            // Project cursor to world plane at grab depth
            f32 localX = (mousePos.x - m_ViewMinX) / viewW * static_cast<f32>(m_RTWidth);
            f32 localY = (mousePos.y - m_ViewMinY) / viewH * static_cast<f32>(m_RTHeight);

            Editor::Ray ray = Editor::ScenePicker::ScreenToRay(
                &pickCamera, localX, localY,
                static_cast<f32>(m_RTWidth), static_cast<f32>(m_RTHeight));

            // Intersect ray with plane perpendicular to camera forward at grab depth
            f32 denom = ray.direction.Dot(forward);
            if (std::abs(denom) > 1e-6f) {
                Math::Vector3 planePoint = cameraTransform->position + forward * m_GrabDepth;
                f32 t = (planePoint - ray.origin).Dot(forward) / denom;
                if (t > 0.0f) {
                    grab->cursorWorldPoint = ray.origin + ray.direction * t;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Phase 2: UpdateTethers
// ---------------------------------------------------------------------------
void FlowerSystem::UpdateTethers(f32 dt) {
    if (!m_World) return;

    for (Entity entity : m_World->GetAllEntities()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || tether->isBroken) continue;

        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (!transform) continue;

        // Get stem attachment world position
        auto* stemTransform = m_World->GetComponent<TransformComponent>(tether->stemEntity);
        if (!stemTransform) continue;

        Math::Vector3 attachWorld = stemTransform->position + tether->attachLocalPos;

        // Auto-compute rest length on first frame
        if (!tether->restLengthInitialized) {
            f32 dist = (transform->position - attachWorld).Length();
            tether->computedRestLength = (tether->restLength > 0.0f) ? tether->restLength : dist;
            tether->restLengthInitialized = true;
        }

        f32 restLen = tether->computedRestLength;
        Math::Vector3 delta = transform->position - attachWorld;
        f32 distance = delta.Length();
        f32 stretch = distance - restLen;
        if (stretch < 0.0f) stretch = 0.0f;

        Math::Vector3 direction = (distance > 1e-6f) ? delta * (1.0f / distance) : Math::Vector3(0, 1, 0);

        // Spring force: pull back toward stem with non-linear ramp
        f32 rampFactor = 1.0f + std::pow(stretch / tether->breakDistance, tether->tensionRamp);
        Math::Vector3 springForce = direction * (-tether->tetherStiffness * stretch * rampFactor);

        // If grabbed, add pull force toward cursor
        auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
        Math::Vector3 velocity(0, 0, 0);

        // Use rigidbody velocity if present, otherwise track implicitly via position
        auto* rb = m_World->GetComponent<RigidbodyComponent>(entity);
        if (rb) {
            velocity = rb->velocity;
        }

        Math::Vector3 totalForce = springForce;

        if (grab && grab->isGrabbed) {
            Math::Vector3 pullDir = grab->cursorWorldPoint - transform->position;
            totalForce = totalForce + pullDir * grab->pullForce;
        }

        // Damping
        totalForce = totalForce - velocity * tether->tetherDamping;

        // Semi-implicit Euler
        velocity = velocity + totalForce * dt;
        transform->position = transform->position + velocity * dt;

        if (rb) {
            rb->velocity = velocity;
        }

        // Update tension feedback
        f32 currentDist = (transform->position - attachWorld).Length();
        f32 currentStretch = currentDist - restLen;
        if (currentStretch < 0.0f) currentStretch = 0.0f;
        tether->currentTension = Math::Clamp(currentStretch / tether->breakDistance, 0.0f, 1.0f);
    }
}

// ---------------------------------------------------------------------------
// Phase 3: UpdateJellyMeshes
// ---------------------------------------------------------------------------
void FlowerSystem::UpdateJellyMeshes(f32 dt) {
    if (!m_World) return;

    for (Entity entity : m_World->GetAllEntities()) {
        auto* jelly = m_World->GetComponent<JellyMeshComponent>(entity);
        if (!jelly) continue;

        auto* mesh = m_World->GetComponent<MeshComponent>(entity);
        if (!mesh || mesh->vertices.empty()) continue;

        // Lazy init: snapshot rest positions
        if (!jelly->initialized) {
            jelly->restPositions.resize(mesh->vertices.size());
            jelly->velocities.resize(mesh->vertices.size(), Math::Vector3(0, 0, 0));
            for (usize i = 0; i < mesh->vertices.size(); ++i) {
                jelly->restPositions[i] = mesh->vertices[i].position;
            }
            jelly->initialized = true;
        }

        // Safety check: arrays must match vertex count
        if (jelly->restPositions.size() != mesh->vertices.size()) continue;

        auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        bool isGrabbed = grab && grab->isGrabbed && transform;

        // Compute center of mesh in local space (for weight-by-distance)
        Math::Vector3 localCenter(0, 0, 0);
        for (const auto& rest : jelly->restPositions) {
            localCenter = localCenter + rest;
        }
        if (!jelly->restPositions.empty()) {
            localCenter = localCenter * (1.0f / static_cast<f32>(jelly->restPositions.size()));
        }

        // Compute pull direction in local space if grabbed
        Math::Vector3 localPullDir(0, 0, 0);
        f32 pullMagnitude = 0.0f;
        if (isGrabbed) {
            Math::Vector3 worldPull = grab->cursorWorldPoint - transform->position;
            pullMagnitude = worldPull.Length();
            if (pullMagnitude > 1e-6f) {
                // Approximate: treat pull in world as local direction (ignoring rotation for simplicity)
                localPullDir = worldPull * (1.0f / pullMagnitude);
            }
        }

        bool anyMoved = false;
        for (usize i = 0; i < mesh->vertices.size(); ++i) {
            Math::Vector3& pos = mesh->vertices[i].position;
            Math::Vector3& vel = jelly->velocities[i];
            const Math::Vector3& rest = jelly->restPositions[i];

            // Target position: rest position with optional pull deformation
            Math::Vector3 target = rest;
            if (isGrabbed && pullMagnitude > 0.01f) {
                // Weight by distance from center: vertices farther from center deform more
                f32 distFromCenter = (rest - localCenter).Length();
                f32 maxDist = 1.0f; // Normalize to roughly unit scale
                f32 weight = Math::Clamp(distFromCenter / maxDist, 0.0f, 1.0f);
                target = rest + localPullDir * (pullMagnitude * weight * 0.3f);
            }

            // Spring force toward target
            Math::Vector3 force = (target - pos) * jelly->springStiffness - vel * jelly->damping;

            // Semi-implicit Euler
            vel = vel + force * dt;
            pos = pos + vel * dt;

            // Clamp to max stretch from rest
            Math::Vector3 offset = pos - rest;
            f32 offsetLen = offset.Length();
            if (offsetLen > jelly->maxStretch) {
                pos = rest + offset * (jelly->maxStretch / offsetLen);
                // Kill velocity component along stretch direction
                Math::Vector3 dir = offset * (1.0f / offsetLen);
                f32 velAlongDir = vel.Dot(dir);
                if (velAlongDir > 0.0f) {
                    vel = vel - dir * velAlongDir;
                }
            }

            // Check if anything moved (to decide meshDirty)
            if (offsetLen > 1e-5f || vel.Length() > 1e-5f) {
                anyMoved = true;
            }
        }

        if (anyMoved) {
            jelly->meshDirty = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Phase 4: CheckBreaks
// ---------------------------------------------------------------------------
void FlowerSystem::CheckBreaks() {
    if (!m_World) return;

    for (Entity entity : m_World->GetAllEntities()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || tether->isBroken) continue;

        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (!transform) continue;

        auto* stemTransform = m_World->GetComponent<TransformComponent>(tether->stemEntity);
        if (!stemTransform) continue;

        Math::Vector3 attachWorld = stemTransform->position + tether->attachLocalPos;
        f32 distance = (transform->position - attachWorld).Length();
        f32 stretch = distance - tether->computedRestLength;
        if (stretch < 0.0f) stretch = 0.0f;

        if (stretch >= tether->breakDistance) {
            tether->isBroken = true;

            // Clear grab state
            auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
            if (grab) {
                grab->isGrabbed = false;
            }
            if (m_GrabbedEntity == entity) {
                m_GrabbedEntity = INVALID_ENTITY;
            }

            // Give the entity a fling velocity via RigidbodyComponent
            if (!m_World->HasComponent<RigidbodyComponent>(entity)) {
                m_World->AddComponent<RigidbodyComponent>(entity);
            }
            auto* rb = m_World->GetComponent<RigidbodyComponent>(entity);
            if (rb) {
                Math::Vector3 flingDir = (transform->position - attachWorld);
                f32 flingLen = flingDir.Length();
                if (flingLen > 1e-6f) {
                    flingDir = flingDir * (1.0f / flingLen);
                }
                rb->velocity = flingDir * 3.0f + Math::Vector3(0, 2.0f, 0);
                rb->useGravity = true;
                rb->mass = 0.1f;
            }

            // Update FlowerStemComponent counters
            auto* stemComp = m_World->GetComponent<FlowerStemComponent>(tether->stemEntity);
            if (stemComp) {
                stemComp->partsRemoved++;

                // Check tags to determine healthy vs withered
                auto* tags = m_World->GetComponent<TagComponent>(entity);
                if (tags) {
                    if (tags->HasTag("withered")) {
                        stemComp->witheredRemoved++;
                    } else if (tags->HasTag("healthy")) {
                        stemComp->healthyRemoved++;
                    }
                }
            }

            ENJIN_LOG_INFO(Editor, "Tether broken for entity %llu", (unsigned long long)entity);
        }
    }
}

// ---------------------------------------------------------------------------
// Evaluate
// ---------------------------------------------------------------------------
void FlowerSystem::Evaluate() {
    if (!m_World) return;

    for (Entity entity : m_World->GetAllEntities()) {
        auto* stem = m_World->GetComponent<FlowerStemComponent>(entity);
        if (!stem) continue;

        stem->score = static_cast<f32>(stem->healthyRemoved) * stem->healthyBonus
                    - static_cast<f32>(stem->witheredRemoved) * stem->witheredPenalty;
        stem->evaluated = true;

        ENJIN_LOG_INFO(Editor, "Flower evaluated: healthy=%d, withered=%d, score=%.1f",
                      stem->healthyRemoved, stem->witheredRemoved, stem->score);
    }

    // Find score display text entity (TagComponent with "score_display")
    for (Entity entity : m_World->GetAllEntities()) {
        auto* tags = m_World->GetComponent<TagComponent>(entity);
        if (!tags || !tags->HasTag("score_display")) continue;

        auto* text = m_World->GetComponent<TextComponent>(entity);
        if (!text) continue;

        // Build score summary from all FlowerStemComponents
        f32 totalScore = 0.0f;
        i32 totalHealthy = 0, totalWithered = 0, totalParts = 0;
        for (Entity stemEntity : m_World->GetAllEntities()) {
            auto* stem = m_World->GetComponent<FlowerStemComponent>(stemEntity);
            if (!stem) continue;
            totalScore += stem->score;
            totalHealthy += stem->healthyRemoved;
            totalWithered += stem->witheredRemoved;
            totalParts += stem->partsRemoved;
        }

        char buf[256];
        snprintf(buf, sizeof(buf), "Score: %.0f | Healthy: %d | Withered: %d | Total removed: %d",
                 totalScore, totalHealthy, totalWithered, totalParts);
        text->text = buf;
        text->dirty = true;
    }
}

Math::Vector3 FlowerSystem::ScreenToWorldOnPlane(f32 screenX, f32 screenY, f32 planeDepth) {
    if (!m_Camera) return Math::Vector3(0, 0, 0);

    Editor::Ray ray = Editor::ScenePicker::ScreenToRay(
        m_Camera, screenX, screenY,
        static_cast<f32>(m_RTWidth), static_cast<f32>(m_RTHeight));

    // Intersect with plane at planeDepth along camera forward
    // Simplified: use the ray parameter directly
    f32 t = planeDepth / ray.direction.Length();
    return ray.origin + ray.direction * t;
}

} // namespace ECS
} // namespace Enjin
