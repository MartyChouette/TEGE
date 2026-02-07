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
            for (Entity entity : m_World->GetEntitiesWithComponent<FlowerStemComponent>()) {
                auto* stem = m_World->GetComponent<FlowerStemComponent>(entity);
                if (!stem) continue;
                auto* transform = m_World->GetComponent<TransformComponent>(entity);
                if (!transform) continue;

                f32 phase = transform->position.x * 0.7f + transform->position.z * 0.4f + windVec.w * 1.5f;
                f32 sway = std::sin(phase) * stem->stemSwayAmplitude * windMag;
                transform->position.x += windDir.x * sway * deltaTime;
                transform->position.z += windDir.z * sway * deltaTime;
            }
        }
    }

    SetupJointsIfNeeded();
    ProcessGrabForces(deltaTime);
    UpdateJellyMeshes(deltaTime);
    UpdateJointTracking();
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

            for (Entity entity : m_World->GetEntitiesWithComponent<GrabbableComponent>()) {
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
                    // Broken petal released: spawn splash and hide offscreen
                    auto* transform = m_World->GetComponent<TransformComponent>(m_GrabbedEntity);
                    if (transform) {
                        auto* mat = m_World->GetComponent<MaterialComponent>(m_GrabbedEntity);
                        Math::Vector3 color = mat ? mat->baseColor : Math::Vector3(1, 1, 1);
                        SpawnGroundSplash(transform->position, color);
                        // Hide entity (keeps GPU buffers valid, avoids freeze)
                        transform->visible = false;
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
// Phase 2: SetupJointsIfNeeded — one-time: create RBs + SpringJoints
// ---------------------------------------------------------------------------
void FlowerSystem::SetupJointsIfNeeded() {
    if (!m_World || m_JointsInitialized) return;
    m_JointsInitialized = true;

    // Add Kinematic RigidbodyComponent to stem entities
    for (Entity entity : m_World->GetEntitiesWithComponent<FlowerStemComponent>()) {
        auto* stem = m_World->GetComponent<FlowerStemComponent>(entity);
        if (!stem) continue;
        if (!m_World->HasComponent<RigidbodyComponent>(entity)) {
            auto& rb = m_World->AddComponent<RigidbodyComponent>(entity);
            rb.bodyType = RigidbodyComponent::BodyType::Kinematic;
            rb.mass = 5.0f;
            rb.useGravity = false;
        }
    }

    // For each tethered entity, add Dynamic RB + SpringJointComponent
    for (Entity entity : m_World->GetEntitiesWithComponent<TetherComponent>()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || tether->isBroken) continue;

        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (!transform) continue;

        // Determine connected entity (fall back to stemEntity for backward compat)
        Entity connected = tether->connectedEntity;
        if (connected == INVALID_ENTITY) connected = tether->stemEntity;

        auto* connectedTransform = m_World->GetComponent<TransformComponent>(connected);
        if (!connectedTransform) continue;

        // Read physics tuning from TetherComponent auto-physics fields
        f32 mass = tether->autoMass;
        f32 springK = tether->autoSpringK;
        f32 dampingC = tether->autoDamping;
        f32 breakForce = tether->autoBreakForce;
        f32 rbDrag = tether->autoDrag;

        // Add Dynamic RigidbodyComponent if not present
        if (!m_World->HasComponent<RigidbodyComponent>(entity)) {
            auto& rb = m_World->AddComponent<RigidbodyComponent>(entity);
            rb.bodyType = RigidbodyComponent::BodyType::Dynamic;
            rb.mass = mass;
            rb.useGravity = false;
            rb.drag = rbDrag;
        }

        // Compute rest length from initial positions
        Math::Vector3 attachWorld = connectedTransform->position + tether->attachLocalPos;
        f32 restLen = (transform->position - attachWorld).Length();
        if (restLen < 0.01f) restLen = 0.1f;

        // Cache initial junction position
        tether->junctionWorldPos = transform->position + (attachWorld - transform->position) * 0.3f;
        tether->lastAnchorWorldA = transform->position;
        tether->lastAnchorWorldB = attachWorld;

        // Add SpringJointComponent
        auto& joint = m_World->AddComponent<SpringJointComponent>(entity);
        joint.entityA = entity;
        joint.entityB = connected;
        joint.anchorA = Math::Vector3(0, 0, 0);
        joint.anchorB = tether->attachLocalPos;
        joint.restLength = restLen;
        joint.springConstant = springK;
        joint.dampingCoefficient = dampingC;
        joint.breakable = true;
        joint.breakForce = breakForce;

        tether->hadJoint = true;
    }
}

// ---------------------------------------------------------------------------
// Phase 2b: ProcessGrabForces — apply grab pull + wind to rigidbody velocity
// ---------------------------------------------------------------------------
void FlowerSystem::ProcessGrabForces(f32 dt) {
    if (!m_World) return;

    for (Entity entity : m_World->GetEntitiesWithComponent<TetherComponent>()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || tether->isBroken) continue;

        auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
        auto* rb = m_World->GetComponent<RigidbodyComponent>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (!rb || !transform) continue;

        Math::Vector3 force(0, 0, 0);

        // Read per-entity grab parameters (with defaults for backward compat)
        f32 maxPullDist = grab ? grab->maxPullDistance : 2.0f;
        f32 maxVel = grab ? grab->maxVelocity : 50.0f;
        f32 windSway = grab ? grab->windSwayScale : 0.15f;

        // Apply pull force toward cursor when grabbed
        if (grab && grab->isGrabbed) {
            Math::Vector3 pullDir = grab->cursorWorldPoint - transform->position;
            f32 pullLen = pullDir.Length();
            if (pullLen > maxPullDist) {
                pullDir = pullDir * (maxPullDist / pullLen);
            }
            force = force + pullDir * grab->pullForce;
        }

        // Wind sway
        if (m_WindSystem) {
            Math::Vector4 windVec = m_WindSystem->GetWindVector();
            Math::Vector3 windForce(windVec.x, windVec.y, windVec.z);
            f32 heightFactor = Math::Clamp(transform->position.y / 2.0f, 0.1f, 1.0f);
            f32 phase = transform->position.x * 0.5f + transform->position.z * 0.3f + windVec.w * 2.0f;
            f32 sway = std::sin(phase) * heightFactor;
            force = force + windForce * sway * windSway;
        }

        // Apply force as velocity change (F/m * dt)
        if (force.Length() > 1e-6f && rb->mass > 0.0f) {
            Math::Vector3 dv = force * (dt / rb->mass);
            rb->velocity = rb->velocity + dv;

            // Clamp velocity
            f32 speed = rb->velocity.Length();
            if (speed > maxVel) {
                rb->velocity = rb->velocity * (maxVel / speed);
            }
            // NaN guard
            if (std::isnan(rb->velocity.x) || std::isnan(rb->velocity.y) || std::isnan(rb->velocity.z)) {
                rb->velocity = Math::Vector3(0, 0, 0);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Phase 3: UpdateJellyMeshes
// ---------------------------------------------------------------------------
void FlowerSystem::UpdateJellyMeshes(f32 dt) {
    if (!m_World) return;

    for (Entity entity : m_World->GetEntitiesWithComponent<JellyMeshComponent>()) {
        auto* jelly = m_World->GetComponent<JellyMeshComponent>(entity);
        if (!jelly) continue;

        // Skip broken entities — jelly deformation on a teleporting petal
        // causes meshDirty every frame, forcing GPU buffer recreation that
        // can race with the in-flight render and crash the driver.
        auto* tetherCheck = m_World->GetComponent<TetherComponent>(entity);
        if (tetherCheck && tetherCheck->isBroken) continue;

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
            Entity connected = tether->connectedEntity != INVALID_ENTITY ? tether->connectedEntity : tether->stemEntity;
            auto* stemTransform = m_World->GetComponent<TransformComponent>(connected);
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

            // Clamp jelly vertex velocity to prevent runaway from rapid shaking
            f32 jellySpeed = vel.Length();
            if (jellySpeed > 30.0f) {
                vel = vel * (30.0f / jellySpeed);
            }

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
// Phase 4: UpdateJointTracking — detect joint breaks, cache anchors, particles
// ---------------------------------------------------------------------------
void FlowerSystem::UpdateJointTracking() {
    if (!m_World) return;

    // Collect break events first — spawning particles during iteration would
    // create entities and invalidate the entity list iterator, causing a freeze.
    struct BreakEvent {
        Entity entity;
        Math::Vector3 junctionPos;   // Last cached junction point
        Math::Vector3 color;
        Entity stemEntity;
        bool hasWitheredTag;
        bool hasHealthyTag;
    };
    std::vector<BreakEvent> breaks;

    for (Entity entity : m_World->GetEntitiesWithComponent<TetherComponent>()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || tether->isBroken) continue;

        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (!transform) continue;

        // Determine connected entity
        Entity connected = tether->connectedEntity;
        if (connected == INVALID_ENTITY) connected = tether->stemEntity;
        if (connected == INVALID_ENTITY || !m_World->IsValid(connected)) {
            tether->isBroken = true;
            continue;
        }

        auto* connectedTransform = m_World->GetComponent<TransformComponent>(connected);

        // Check if SpringJointComponent still exists
        bool hasJoint = m_World->HasComponent<SpringJointComponent>(entity);

        if (hasJoint) {
            auto* joint = m_World->GetComponent<SpringJointComponent>(entity);
            if (joint && connectedTransform) {
                // Cache anchor world positions
                tether->lastAnchorWorldA = transform->position;
                tether->lastAnchorWorldB = connectedTransform->position + tether->attachLocalPos;

                // Update junction pos (30% from entity toward connected attachment)
                tether->junctionWorldPos = transform->position +
                    (tether->lastAnchorWorldB - transform->position) * 0.3f;

                // Read tension from joint stress
                if (joint->breakForce > 0.0f) {
                    tether->currentTension = Math::Clamp(joint->currentStress / joint->breakForce, 0.0f, 1.0f);
                }
            }
            tether->hadJoint = true;

            // Spawn sap drip particles when grabbed and under tension
            auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
            auto* stemComp = m_World->GetComponent<FlowerStemComponent>(tether->stemEntity);
            auto* particleConfig = stemComp ? m_World->GetComponent<FlowerParticleConfigComponent>(tether->stemEntity) : nullptr;
            f32 dripThreshold = particleConfig ? particleConfig->tensionDripThreshold : 0.15f;
            if (grab && grab->isGrabbed && tether->currentTension > dripThreshold) {
                f32 liquidIntensity = stemComp ? stemComp->liquidIntensity : 1.0f;
                if (liquidIntensity > 0.0f) {
                    Math::Vector3 sapClr = stemComp ? stemComp->sapColor : Math::Vector3(0.15f, 0.45f, 0.1f);
                    Math::Vector3 squirtDir = transform->position - tether->junctionWorldPos;
                    f32 squirtLen = squirtDir.Length();
                    if (squirtLen > 0.01f) squirtDir = squirtDir * (1.0f / squirtLen);
                    SpawnTensionDrip(tether->junctionWorldPos, sapClr, tether->currentTension,
                                     squirtDir, liquidIntensity, particleConfig);
                }
            }
        }
        else if (tether->hadJoint) {
            // Joint was destroyed by physics solver — this is a break!
            tether->isBroken = true;
            tether->justBroke = true;

            auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
            if (grab) {
                grab->isBroken = true;
            }

            BreakEvent evt;
            evt.entity = entity;
            evt.junctionPos = tether->junctionWorldPos;  // Last cached value
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

    // Spawn particles and update counters outside the entity iteration
    for (const auto& evt : breaks) {
        auto* pConfig = m_World->GetComponent<FlowerParticleConfigComponent>(evt.stemEntity);
        SpawnBreakParticles(evt.junctionPos, evt.color, pConfig);

        auto* stemComp = m_World->GetComponent<FlowerStemComponent>(evt.stemEntity);
        if (stemComp) {
            stemComp->partsRemoved++;
            if (evt.hasWitheredTag) stemComp->witheredRemoved++;
            else if (evt.hasHealthyTag) stemComp->healthyRemoved++;
        }

        // Critical: clear meshDirty and remove JellyMeshComponent to stop
        // the RenderSystem from erasing/recreating GPU buffers on this entity.
        auto* jelly = m_World->GetComponent<JellyMeshComponent>(evt.entity);
        if (jelly) {
            jelly->meshDirty = false;
            jelly->initialized = false;
            jelly->velocities.clear();
            jelly->restPositions.clear();
        }

        ENJIN_LOG_INFO(Editor, "Joint broken for entity %llu", (unsigned long long)evt.entity);
    }
}

// ---------------------------------------------------------------------------
// Phase 5: UpdateBrokenParts — gravity for released broken parts
// ---------------------------------------------------------------------------
void FlowerSystem::UpdateBrokenParts(f32 dt) {
    if (!m_World) return;

    for (Entity entity : m_World->GetEntitiesWithComponent<TetherComponent>()) {
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
    // Find particle gravity from first stem's FlowerParticleConfigComponent (if any)
    f32 gravity = 9.81f;
    if (m_World) {
        for (Entity entity : m_World->GetEntitiesWithComponent<FlowerParticleConfigComponent>()) {
            auto* cfg = m_World->GetComponent<FlowerParticleConfigComponent>(entity);
            if (cfg) { gravity = cfg->particleGravity; break; }
        }
    }

    for (auto it = m_Particles.begin(); it != m_Particles.end(); ) {
        it->lifetime += dt;
        if (it->lifetime >= it->maxLifetime) {
            it = m_Particles.erase(it);
            continue;
        }

        // Apply gravity and move
        it->velocity = it->velocity + Math::Vector3(0.0f, -gravity, 0.0f) * dt;
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

    for (Entity entity : m_World->GetEntitiesWithComponent<TetherComponent>()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || !tether->isBroken) continue;

        auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
        if (grab && grab->isGrabbed) continue; // Still held

        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        auto* rb = m_World->GetComponent<RigidbodyComponent>(entity);
        if (!transform || !rb) continue;

        // Read ground level from stem's FlowerStemComponent (default 0)
        f32 groundY = 0.0f;
        if (tether->stemEntity != INVALID_ENTITY) {
            auto* stemComp = m_World->GetComponent<FlowerStemComponent>(tether->stemEntity);
            if (stemComp) groundY = stemComp->groundLevel;
        }

        // Check if fallen to ground level
        if (transform->position.y <= groundY) {
            transform->position.y = groundY;
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
        // Find particle config from this entity's tether -> stem
        const FlowerParticleConfigComponent* splashConfig = nullptr;
        auto* tetherForSplash = m_World->GetComponent<TetherComponent>(evt.entity);
        if (tetherForSplash && tetherForSplash->stemEntity != INVALID_ENTITY) {
            splashConfig = m_World->GetComponent<FlowerParticleConfigComponent>(tetherForSplash->stemEntity);
        }
        SpawnGroundSplash(evt.position, evt.color, splashConfig);
        if (m_World->HasComponent<RigidbodyComponent>(evt.entity)) {
            m_World->RemoveComponent<RigidbodyComponent>(evt.entity);
        }
    }
}

// ---------------------------------------------------------------------------
// SpawnBreakParticles — violent sap burst when a petal/leaf tears off.
// Green liquid jets outward + drips down. Big, dramatic, satisfying.
// ---------------------------------------------------------------------------
void FlowerSystem::SpawnBreakParticles(const Math::Vector3& position, const Math::Vector3& color,
                                       const FlowerParticleConfigComponent* config) {
    // Green sap color mixed with petal color
    Math::Vector3 sapColor(0.1f, 0.5f, 0.08f);
    Math::Vector3 mixColor = sapColor * 0.7f + color * 0.3f;

    // Read config values with fallback defaults
    const int burstCount = config ? config->breakBurstCount : 20;
    const f32 burstSpeed = config ? config->breakBurstSpeed : 2.5f;
    const f32 burstUpKick = config ? config->breakBurstUpKick : 2.0f;
    const f32 burstLifetime = config ? config->breakBurstLifetime : 0.9f;
    const f32 burstScale = config ? config->breakBurstScale : 0.08f;
    const int dripCount = config ? config->breakDripCount : 8;
    const f32 dripSpeed = config ? config->breakDripSpeed : 0.5f;
    const f32 dripLifetime = config ? config->breakDripLifetime : 1.2f;

    // Main burst — violent jets radiating outward
    for (int i = 0; i < burstCount; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;

        f32 angle = static_cast<f32>(i) / static_cast<f32>(burstCount) * 6.28318f;
        f32 speed = burstSpeed + static_cast<f32>(i % 5) * 0.8f;
        f32 upKick = burstUpKick + static_cast<f32>(i % 4) * 0.8f;

        FlowerParticle fp;
        fp.position = position;
        fp.color = mixColor;
        fp.velocity = Math::Vector3(
            std::cos(angle) * speed,
            upKick,
            std::sin(angle) * speed
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = burstLifetime + static_cast<f32>(i % 3) * 0.3f;
        fp.scale = burstScale + static_cast<f32>(i % 3) * 0.03f;
        fp.isLiquid = true;
        m_Particles.push_back(fp);
    }

    // Secondary drips — slower, fall straight down like dripping sap
    for (int i = 0; i < dripCount; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;

        f32 angle = static_cast<f32>(i) / static_cast<f32>(dripCount) * 6.28318f;

        FlowerParticle fp;
        fp.position = position + Math::Vector3(
            std::cos(angle) * 0.05f, -0.02f, std::sin(angle) * 0.05f);
        fp.color = sapColor;
        fp.velocity = Math::Vector3(
            std::cos(angle) * 0.3f,
            -dripSpeed - static_cast<f32>(i % 3) * 0.5f,
            std::sin(angle) * 0.3f
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = dripLifetime + static_cast<f32>(i % 3) * 0.3f;
        fp.scale = 0.05f;
        fp.isLiquid = true;
        m_Particles.push_back(fp);
    }
}

// ---------------------------------------------------------------------------
// SpawnGroundSplash — sap splat when broken petal is released
// ---------------------------------------------------------------------------
void FlowerSystem::SpawnGroundSplash(const Math::Vector3& position, const Math::Vector3& color,
                                     const FlowerParticleConfigComponent* config) {
    Math::Vector3 sapColor(0.1f, 0.5f, 0.08f);
    Math::Vector3 mixColor = sapColor * 0.6f + color * 0.4f;

    const int count = config ? config->splashCount : 12;
    const f32 splashSpeed = config ? config->splashSpeed : 1.5f;
    const f32 splashUpKick = config ? config->splashUpKick : 2.5f;
    const f32 splashLifetime = config ? config->splashLifetime : 0.7f;

    for (int i = 0; i < count; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;

        f32 angle = static_cast<f32>(i) / static_cast<f32>(count) * 6.28318f;
        f32 speed = splashSpeed + static_cast<f32>(i % 3) * 0.5f;

        FlowerParticle fp;
        fp.position = position + Math::Vector3(0.0f, 0.05f, 0.0f);
        fp.color = mixColor;
        fp.velocity = Math::Vector3(
            std::cos(angle) * speed * 0.6f,
            splashUpKick + static_cast<f32>(i % 3) * 0.8f,
            std::sin(angle) * speed * 0.6f
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = splashLifetime + static_cast<f32>(i % 3) * 0.2f;
        fp.scale = 0.06f;
        fp.isLiquid = true;
        m_Particles.push_back(fp);
    }
}

// ---------------------------------------------------------------------------
// SpawnTensionDrip — squirting green sap from attachment point while pulling.
// Liquid shoots in the pull direction, then drips down under gravity.
// Rate and violence scale with tension and liquidIntensity.
// ---------------------------------------------------------------------------
void FlowerSystem::SpawnTensionDrip(const Math::Vector3& position, const Math::Vector3& color,
                                     f32 tension, const Math::Vector3& squirtDir, f32 intensity,
                                     const FlowerParticleConfigComponent* config) {
    f32 dripRate = config ? config->tensionDripRate : 3.0f;
    f32 baseSquirtSpeed = config ? config->tensionSquirtSpeed : 2.0f;

    // Accumulate — higher tension + intensity = more particles per frame
    m_DripAccumulator += tension * intensity * dripRate;
    i32 spawnCount = static_cast<i32>(m_DripAccumulator);
    m_DripAccumulator -= static_cast<f32>(spawnCount);
    if (spawnCount < 1) return;

    for (i32 i = 0; i < spawnCount; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;

        // Pseudo-random spread using particle count
        f32 seed = static_cast<f32>((m_Particles.size() * 73 + i * 37) % 100) / 100.0f;
        f32 angle = seed * 6.28318f;

        // Squirt primarily in pull direction with random cone spread
        f32 squirtSpeed = (baseSquirtSpeed + tension * 5.0f) * intensity;
        f32 spread = 0.3f + seed * 0.5f;

        FlowerParticle fp;
        fp.position = position + Math::Vector3(
            std::cos(angle) * 0.03f, 0.02f, std::sin(angle) * 0.03f);
        fp.color = color;
        fp.velocity = squirtDir * squirtSpeed + Math::Vector3(
            std::cos(angle) * spread,
            0.5f + seed * 1.5f,  // Slight upward squirt
            std::sin(angle) * spread
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = 0.4f + tension * 0.4f;
        fp.scale = 0.04f + tension * 0.04f;
        fp.isLiquid = true;
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
    bool anyEvaluated = false;
    f32 totalScore = 0.0f;
    for (Entity entity : m_World->GetEntitiesWithComponent<FlowerStemComponent>()) {
        auto* stem = m_World->GetComponent<FlowerStemComponent>(entity);
        if (!stem) continue;
        totalParts += stem->partsRemoved;
        totalHealthy += stem->healthyRemoved;
        totalWithered += stem->witheredRemoved;
        if (stem->evaluated) {
            anyEvaluated = true;
            totalScore += stem->score;
        }
    }

    // Count total pluckable parts (entities with TetherComponent)
    i32 totalPluckable = static_cast<i32>(m_World->GetEntitiesWithComponent<TetherComponent>().size());

    // Update score_display text entity
    for (Entity entity : m_World->GetEntitiesWithComponent<TagComponent>()) {
        auto* tags = m_World->GetComponent<TagComponent>(entity);
        if (!tags || !tags->HasTag("score_display")) continue;
        auto* text = m_World->GetComponent<TextComponent>(entity);
        if (!text) continue;

        char buf[256];
        if (anyEvaluated) {
            // Show final evaluated score
            snprintf(buf, sizeof(buf), "Score: %.0f | Healthy: %d | Withered: %d | Plucked: %d",
                     totalScore, totalHealthy, totalWithered, totalParts);
        } else {
            // Show live plucking progress
            snprintf(buf, sizeof(buf), "Plucked: %d/%d | Score: %d",
                     totalParts, totalPluckable + totalParts,
                     totalHealthy * 10 - totalWithered * 5);
        }
        text->text = buf;
        text->dirty = true;
    }
}

// ---------------------------------------------------------------------------
// Evaluate
// ---------------------------------------------------------------------------
void FlowerSystem::Evaluate() {
    if (!m_World) return;

    for (Entity entity : m_World->GetEntitiesWithComponent<FlowerStemComponent>()) {
        auto* stem = m_World->GetComponent<FlowerStemComponent>(entity);
        if (!stem) continue;

        stem->score = static_cast<f32>(stem->healthyRemoved) * stem->healthyBonus
                    - static_cast<f32>(stem->witheredRemoved) * stem->witheredPenalty;
        stem->evaluated = true;

        ENJIN_LOG_INFO(Editor, "Flower evaluated: healthy=%d, withered=%d, score=%.1f",
                      stem->healthyRemoved, stem->witheredRemoved, stem->score);
    }

    // Find score display text entity (TagComponent with "score_display")
    for (Entity entity : m_World->GetEntitiesWithComponent<TagComponent>()) {
        auto* tags = m_World->GetComponent<TagComponent>(entity);
        if (!tags || !tags->HasTag("score_display")) continue;

        auto* text = m_World->GetComponent<TextComponent>(entity);
        if (!text) continue;

        // Build score summary from all FlowerStemComponents
        f32 totalScore = 0.0f;
        i32 totalHealthy = 0, totalWithered = 0, totalParts = 0;
        for (Entity stemEntity : m_World->GetEntitiesWithComponent<FlowerStemComponent>()) {
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
