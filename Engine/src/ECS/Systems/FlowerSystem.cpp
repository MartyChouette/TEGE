#include "Enjin/ECS/Systems/FlowerSystem.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <algorithm>

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

    // Apply subtle wind sway to stem entities so the whole flower leans
    if (m_WindSystem) {
        Math::Vector4 windVec = m_WindSystem->GetWindVector();
        Math::Vector3 windDir(windVec.x, 0.0f, windVec.z); // horizontal only
        f32 windMag = windDir.Length();
        if (windMag > 1e-6f) {
            windDir = windDir * (1.0f / windMag);
            for (Entity entity : m_World->GetAllEntities()) {
                auto* stem = m_World->GetComponent<FlowerStemComponent>(entity);
                if (!stem) continue;
                auto* transform = m_World->GetComponent<TransformComponent>(entity);
                if (!transform) continue;

                f32 phase = transform->position.x * 0.7f + transform->position.z * 0.4f + windVec.w * 1.5f;
                f32 sway = std::sin(phase) * 0.07f * windMag;
                transform->position.x += windDir.x * sway * deltaTime;
                transform->position.z += windDir.z * sway * deltaTime;
            }
        }
    }

    UpdateTethers(deltaTime);
    UpdateJellyMeshes(deltaTime);
    CheckBreaks();
    UpdateBrokenParts(deltaTime);
    UpdateParticles(deltaTime);
    CheckGroundImpact();
    UpdateScoreDisplay();
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

            Editor::Ray ray = Editor::ScenePicker::ScreenToRay(
                &pickCamera, localX, localY,
                static_cast<f32>(m_RTWidth), static_cast<f32>(m_RTHeight));

            // Pick only GrabbableComponent entities using sphere test (ignores ground)
            Entity bestEntity = INVALID_ENTITY;
            f32 bestDist = FLT_MAX;

            for (Entity entity : m_World->GetAllEntities()) {
                auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
                if (!grab) continue;
                auto* entTransform = m_World->GetComponent<TransformComponent>(entity);
                if (!entTransform) continue;

                // Ray-sphere test: use grabRadius as hit sphere size
                Math::Vector3 oc = ray.origin - entTransform->position;
                f32 r = grab->grabRadius;
                f32 a = ray.direction.Dot(ray.direction);
                f32 b = 2.0f * oc.Dot(ray.direction);
                f32 c = oc.Dot(oc) - r * r;
                f32 discriminant = b * b - 4.0f * a * c;

                if (discriminant >= 0.0f) {
                    f32 t = (-b - std::sqrt(discriminant)) / (2.0f * a);
                    if (t < 0.0f) t = (-b + std::sqrt(discriminant)) / (2.0f * a);
                    if (t >= 0.0f && t < bestDist) {
                        bestDist = t;
                        bestEntity = entity;
                    }
                }
            }

            if (bestEntity != INVALID_ENTITY) {
                auto* grab = m_World->GetComponent<GrabbableComponent>(bestEntity);
                auto* hitTransform = m_World->GetComponent<TransformComponent>(bestEntity);
                if (grab && hitTransform) {
                    grab->isGrabbed = true;
                    grab->grabWorldPoint = hitTransform->position;
                    grab->cursorWorldPoint = hitTransform->position;
                    m_GrabbedEntity = bestEntity;

                    // Compute grab depth (distance from camera to entity along camera forward)
                    Math::Vector3 toEntity = hitTransform->position - cameraTransform->position;
                    m_GrabDepth = toEntity.Dot(forward);
                    if (m_GrabDepth < 0.1f) m_GrabDepth = 0.1f;
                }
            }
        }
    }

    // LMB released - release or drop
    if (Input::IsMouseButtonReleased(MouseButton::Left)) {
        if (m_GrabbedEntity != INVALID_ENTITY) {
            auto* grab = m_World->GetComponent<GrabbableComponent>(m_GrabbedEntity);
            if (grab) {
                if (grab->isBroken) {
                    // Broken petal: drop with gravity
                    if (!m_World->HasComponent<RigidbodyComponent>(m_GrabbedEntity)) {
                        m_World->AddComponent<RigidbodyComponent>(m_GrabbedEntity);
                    }
                    auto* rb = m_World->GetComponent<RigidbodyComponent>(m_GrabbedEntity);
                    if (rb) {
                        rb->velocity = Math::Vector3(0.0f, -0.5f, 0.0f);
                        rb->useGravity = true;
                        rb->mass = 0.1f;
                    }
                }
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

        // If grabbed, add pull force toward cursor
        auto* grab = m_World->GetComponent<GrabbableComponent>(entity);

        // Spring force: pull back toward stem with non-linear ramp
        // Weaken spring when actively grabbed so the part stretches toward break
        f32 effectiveStiffness = tether->tetherStiffness;
        if (grab && grab->isGrabbed) {
            effectiveStiffness *= 0.2f;
        }
        f32 rampFactor = 1.0f + std::pow(stretch / tether->breakDistance, tether->tensionRamp);
        Math::Vector3 springForce = direction * (-effectiveStiffness * stretch * rampFactor);

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

        // Wind sway: higher parts sway more, sinusoidal variation for organic feel
        if (m_WindSystem) {
            Math::Vector4 windVec = m_WindSystem->GetWindVector();
            Math::Vector3 windForce(windVec.x, windVec.y, windVec.z);
            f32 heightFactor = Math::Clamp(transform->position.y / 2.0f, 0.1f, 1.0f);
            f32 phase = transform->position.x * 0.5f + transform->position.z * 0.3f + windVec.w * 2.0f;
            f32 sway = std::sin(phase) * heightFactor;
            totalForce = totalForce + windForce * sway * 0.15f;
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

        // Find stem direction in local space for anchor weighting
        // Vertices on the stem side resist deformation (anchored), far side stretches freely
        Math::Vector3 stemDirLocal(0, -1, 0); // Default: stem is below
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (tether && transform) {
            auto* stemTransform = m_World->GetComponent<TransformComponent>(tether->stemEntity);
            if (stemTransform) {
                Math::Vector3 toStem = stemTransform->position + tether->attachLocalPos - transform->position;
                f32 toStemLen = toStem.Length();
                if (toStemLen > 1e-6f) {
                    stemDirLocal = toStem * (1.0f / toStemLen);
                }
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
                // Directional weight: how much this vertex is on the "pull side" vs "stem side"
                // Project vertex offset from center onto pull direction
                Math::Vector3 fromCenter = rest - localCenter;
                f32 pullAlignment = fromCenter.Dot(localPullDir);  // positive = pull side
                f32 stemAlignment = fromCenter.Dot(stemDirLocal);  // positive = stem side

                // Vertices toward stem get near-zero weight (anchored)
                // Vertices away from stem get full weight (stretchy)
                f32 distFromCenter = fromCenter.Length();
                f32 maxDist = 1.0f;
                f32 radialWeight = Math::Clamp(distFromCenter / maxDist, 0.0f, 1.0f);

                // Anchor factor: 0 for stem-side vertices, 1 for pull-side vertices
                f32 anchorFactor = Math::Clamp((pullAlignment * 0.5f - stemAlignment * 0.3f + 0.5f), 0.05f, 1.0f);

                f32 weight = radialWeight * anchorFactor;
                target = rest + localPullDir * (pullMagnitude * weight * 1.0f);
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

    // Collect break events first — spawning particles during iteration would
    // create entities and invalidate the entity list iterator, causing a freeze.
    struct BreakEvent {
        Entity entity;
        Math::Vector3 position;
        Math::Vector3 color;
        Entity stemEntity;
        bool hasWitheredTag;
        bool hasHealthyTag;
    };
    std::vector<BreakEvent> breaks;

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
            tether->justBroke = true;

            // Mark grab as broken — petal stays under cursor
            auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
            if (grab) {
                grab->isBroken = true;
                // Keep isGrabbed = true so petal follows cursor
            }

            BreakEvent evt;
            evt.entity = entity;
            evt.position = transform->position;
            evt.color = Math::Vector3(1.0f, 1.0f, 1.0f);
            auto* mat = m_World->GetComponent<MaterialComponent>(entity);
            if (mat) evt.color = mat->baseColor;
            evt.stemEntity = tether->stemEntity;
            auto* tags = m_World->GetComponent<TagComponent>(entity);
            evt.hasWitheredTag = tags && tags->HasTag("withered");
            evt.hasHealthyTag = tags && tags->HasTag("healthy");
            breaks.push_back(evt);
        }
    }

    // Now spawn particles and update counters outside the entity iteration
    for (const auto& evt : breaks) {
        SpawnBreakParticles(evt.position, evt.color);

        auto* stemComp = m_World->GetComponent<FlowerStemComponent>(evt.stemEntity);
        if (stemComp) {
            stemComp->partsRemoved++;
            if (evt.hasWitheredTag) stemComp->witheredRemoved++;
            else if (evt.hasHealthyTag) stemComp->healthyRemoved++;
        }

        ENJIN_LOG_INFO(Editor, "Tether broken for entity %llu", (unsigned long long)evt.entity);
    }
}

// ---------------------------------------------------------------------------
// Phase 5: UpdateBrokenParts — gravity for released broken parts
// ---------------------------------------------------------------------------
void FlowerSystem::UpdateBrokenParts(f32 dt) {
    if (!m_World) return;

    for (Entity entity : m_World->GetAllEntities()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || !tether->isBroken) continue;

        // Clear one-frame justBroke flag
        if (tether->justBroke) {
            tether->justBroke = false;
        }

        auto* grab = m_World->GetComponent<GrabbableComponent>(entity);

        // If still grabbed (holding broken petal), move petal to cursor
        if (grab && grab->isGrabbed && grab->isBroken) {
            auto* transform = m_World->GetComponent<TransformComponent>(entity);
            if (transform) {
                transform->position = grab->cursorWorldPoint;
            }
            continue;
        }

        // If released (has rigidbody), apply gravity
        auto* rb = m_World->GetComponent<RigidbodyComponent>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (rb && transform && rb->useGravity) {
            rb->velocity = rb->velocity + Math::Vector3(0.0f, -9.81f * rb->mass, 0.0f) * dt;
            transform->position = transform->position + rb->velocity * dt;
        }
    }
}

// ---------------------------------------------------------------------------
// Phase 6: UpdateParticles — tick particle entity lifetimes and gravity
// ---------------------------------------------------------------------------
void FlowerSystem::UpdateParticles(f32 dt) {
    for (auto it = m_Particles.begin(); it != m_Particles.end(); ) {
        it->lifetime += dt;
        if (it->lifetime >= it->maxLifetime) {
            it = m_Particles.erase(it);
            continue;
        }

        // Apply gravity and move
        it->velocity = it->velocity + Math::Vector3(0.0f, -9.81f, 0.0f) * dt;
        it->position = it->position + it->velocity * dt;

        // Quadratic fade-out
        f32 t = it->lifetime / it->maxLifetime;
        it->scale = 0.06f * (1.0f - t * t);

        ++it;
    }
}

// ---------------------------------------------------------------------------
// Phase 7: CheckGroundImpact — detect broken+released parts hitting ground
// ---------------------------------------------------------------------------
void FlowerSystem::CheckGroundImpact() {
    if (!m_World) return;

    // Collect impact events first — spawning particles and removing components
    // during iteration would invalidate the entity list iterator, causing a freeze.
    struct ImpactEvent {
        Entity entity;
        Math::Vector3 position;
        Math::Vector3 color;
    };
    std::vector<ImpactEvent> impacts;

    for (Entity entity : m_World->GetAllEntities()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || !tether->isBroken) continue;

        auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
        if (grab && grab->isGrabbed) continue; // Still held

        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        auto* rb = m_World->GetComponent<RigidbodyComponent>(entity);
        if (!transform || !rb) continue;

        // Check if fallen to ground level
        if (transform->position.y <= 0.0f) {
            transform->position.y = 0.0f;
            rb->velocity = Math::Vector3(0, 0, 0);
            rb->useGravity = false;

            ImpactEvent evt;
            evt.entity = entity;
            evt.position = transform->position;
            evt.color = Math::Vector3(1.0f, 1.0f, 1.0f);
            auto* mat = m_World->GetComponent<MaterialComponent>(entity);
            if (mat) evt.color = mat->baseColor;
            impacts.push_back(evt);
        }
    }

    // Spawn splash particles and remove rigidbodies outside the entity iteration
    for (const auto& evt : impacts) {
        SpawnGroundSplash(evt.position, evt.color);
        m_World->RemoveComponent<RigidbodyComponent>(evt.entity);
    }
}

// ---------------------------------------------------------------------------
// SpawnBreakParticles — burst of 14 droplets radiating outward
// Particles are purely internal (no ECS entities) to avoid entity
// creation/destruction issues that cause freezes and crashes.
// ---------------------------------------------------------------------------
void FlowerSystem::SpawnBreakParticles(const Math::Vector3& position, const Math::Vector3& color) {
    const int count = 14;
    for (int i = 0; i < count; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;

        f32 angle = static_cast<f32>(i) / static_cast<f32>(count) * 6.28318f;
        f32 speed = 2.0f + static_cast<f32>(i % 3) * 0.5f;

        FlowerParticle fp;
        fp.position = position;
        fp.color = color;
        fp.velocity = Math::Vector3(
            std::cos(angle) * speed,
            1.5f + static_cast<f32>(i % 4) * 0.4f,
            std::sin(angle) * speed
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = 0.8f + static_cast<f32>(i % 3) * 0.2f;
        fp.scale = 0.06f;
        m_Particles.push_back(fp);
    }
}

// ---------------------------------------------------------------------------
// SpawnGroundSplash — 8 droplets spraying upward from impact point
// ---------------------------------------------------------------------------
void FlowerSystem::SpawnGroundSplash(const Math::Vector3& position, const Math::Vector3& color) {
    const int count = 8;
    for (int i = 0; i < count; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;

        f32 angle = static_cast<f32>(i) / static_cast<f32>(count) * 6.28318f;
        f32 speed = 1.0f + static_cast<f32>(i % 3) * 0.3f;

        FlowerParticle fp;
        fp.position = position + Math::Vector3(0.0f, 0.05f, 0.0f);
        fp.color = color;
        fp.velocity = Math::Vector3(
            std::cos(angle) * speed * 0.5f,
            2.0f + static_cast<f32>(i % 3) * 0.5f,
            std::sin(angle) * speed * 0.5f
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = 0.6f + static_cast<f32>(i % 3) * 0.15f;
        fp.scale = 0.04f;
        m_Particles.push_back(fp);
    }
}

// ---------------------------------------------------------------------------
// UpdateScoreDisplay — live score each frame
// ---------------------------------------------------------------------------
void FlowerSystem::UpdateScoreDisplay() {
    if (!m_World) return;

    // Gather totals from all stems
    i32 totalParts = 0, totalHealthy = 0, totalWithered = 0;
    for (Entity entity : m_World->GetAllEntities()) {
        auto* stem = m_World->GetComponent<FlowerStemComponent>(entity);
        if (!stem) continue;
        totalParts += stem->partsRemoved;
        totalHealthy += stem->healthyRemoved;
        totalWithered += stem->witheredRemoved;
    }

    // Count total pluckable parts (entities with TetherComponent)
    i32 totalPluckable = 0;
    for (Entity entity : m_World->GetAllEntities()) {
        if (m_World->HasComponent<TetherComponent>(entity))
            totalPluckable++;
    }

    // Update score_display text entity
    for (Entity entity : m_World->GetAllEntities()) {
        auto* tags = m_World->GetComponent<TagComponent>(entity);
        if (!tags || !tags->HasTag("score_display")) continue;
        auto* text = m_World->GetComponent<TextComponent>(entity);
        if (!text) continue;

        char buf[256];
        snprintf(buf, sizeof(buf), "Plucked: %d/%d | Score: %d",
                 totalParts, totalPluckable + totalParts,
                 totalHealthy * 10 - totalWithered * 5);
        text->text = buf;
        text->dirty = true;
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
