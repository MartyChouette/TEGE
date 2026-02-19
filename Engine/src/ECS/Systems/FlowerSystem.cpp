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
                    // Broken petal released: zero velocity so it falls cleanly.
                    // UpdateBrokenParts applies gravity, CheckGroundImpact handles landing.
                    auto* tether = m_World->GetComponent<TetherComponent>(m_GrabbedEntity);
                    if (tether) {
                        tether->velocity = Math::Vector3(0, 0, 0);
                    }
                }
                grab->isGrabbed = false;
            }
            m_GrabbedEntity = INVALID_ENTITY;
            m_DripAccumulator = 0.0f;  // Stop pending tension drips on release
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
// Phase 2: SetupJointsIfNeeded — one-time: capture rest distances, init state
// ---------------------------------------------------------------------------
void FlowerSystem::SetupJointsIfNeeded() {
    if (!m_World || m_JointsInitialized) return;
    m_JointsInitialized = true;

    // For each tethered entity, capture rest distance and init runtime state
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

        // Capture rest distance from initial positions
        Math::Vector3 attachWorld = connectedTransform->position + tether->attachLocalPos;
        tether->restDistance = (transform->position - attachWorld).Length();
        if (tether->restDistance < 0.01f) tether->restDistance = 0.1f;

        // Cache initial positions
        tether->junctionWorldPos = transform->position + (attachWorld - transform->position) * 0.3f;
        tether->lastAnchorWorldA = transform->position;
        tether->lastAnchorWorldB = attachWorld;
        tether->prevAnchorWorldA = transform->position;
        tether->prevRelVec = transform->position - attachWorld;
        tether->absoluteTravel = 0.0f;
        tether->relativeTravel = 0.0f;
        tether->aliveTime = 0.0f;

        tether->hadJoint = true;
    }
}

// ---------------------------------------------------------------------------
// Phase 2b: ProcessGrabForces — spring-damped grab pull + tether spring + wind
// Ref: GrabPull.cs (grab), XYTetherJoint (auto spring/damper)
// ---------------------------------------------------------------------------
void FlowerSystem::ProcessGrabForces(f32 dt) {
    if (!m_World || dt <= 0.0f) return;

    for (Entity entity : m_World->GetEntitiesWithComponent<TetherComponent>()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || tether->isBroken) continue;
        if (!tether->hadJoint) continue;  // Not yet initialized

        auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (!transform) continue;

        Math::Vector3 velocity = tether->velocity;

        // --- Tether spring: ALWAYS pull back toward anchor (ref: XYTetherJoint auto-drive) ---
        // This is the resistance the user fights against when dragging.
        // Uses adaptive drive: spring/damper scale with tension for "feel".
        Entity connected = tether->connectedEntity;
        if (connected == INVALID_ENTITY) connected = tether->stemEntity;
        auto* connTransform = m_World->GetComponent<TransformComponent>(connected);
        if (connTransform) {
            Math::Vector3 anchor = connTransform->position + tether->attachLocalPos;
            Math::Vector3 toAnchor = anchor - transform->position;
            f32 dist = toAnchor.Length();

            if (dist > 0.001f) {
                Math::Vector3 dir = toAnchor * (1.0f / dist);
                f32 stretch = dist - tether->restDistance;
                if (stretch > 0.0f) {
                    // Adaptive drive: scale spring/damper based on tension (ref: XYTetherJoint ritual feel)
                    // Low tension = soft (easy initial pull), high tension = stiff (strong resistance near break)
                    f32 tensionNorm = Math::Clamp(stretch / tether->maxDistance, 0.0f, 1.0f);
                    f32 springMult = tether->adaptiveMinSpringMult + tensionNorm * (tether->adaptiveMaxSpringMult - tether->adaptiveMinSpringMult);
                    f32 damperMult = tether->adaptiveMinDamperMult + tensionNorm * (tether->adaptiveMaxDamperMult - tether->adaptiveMinDamperMult);

                    f32 springForce = tether->autoSpringK * springMult * stretch;
                    f32 dampForce = tether->autoDamping * damperMult * velocity.Dot(dir * -1.0f);
                    f32 totalForce = springForce - dampForce;
                    if (totalForce > tether->driveMaxForce) totalForce = tether->driveMaxForce;
                    if (totalForce > 0.0f) {
                        Math::Vector3 accel = dir * (totalForce / tether->autoMass);
                        velocity = velocity + accel * dt;
                    }
                }
            }

            // Drag (always active, damps idle oscillation)
            velocity = velocity * std::max(0.0f, 1.0f - tether->autoDrag * dt);
        }

        // --- Grab: direct smooth follow toward cursor ---
        // NOT a spring (springs oscillate = "tug tug" feel). Instead, set velocity
        // directly toward cursor. The tether spring above provides all the resistance
        // and tension. This gives: smooth drag → tension builds → snap.
        if (grab && grab->isGrabbed) {
            Math::Vector3 toTarget = grab->cursorWorldPoint - transform->position;
            f32 dist = toTarget.Length();

            if (dist > 0.001f) {
                Math::Vector3 dir = toTarget * (1.0f / dist);
                // Desired speed scales with distance but caps out
                f32 desiredSpeed = std::min(dist * grab->grabSpring, grab->maxSpeed);
                Math::Vector3 desiredVel = dir * desiredSpeed;

                // Smooth blend toward desired velocity (critically damped, no overshoot)
                f32 blend = 1.0f - std::exp(-grab->grabDamper * dt);
                velocity = velocity * (1.0f - blend) + desiredVel * blend;
            }
        }

        // --- Wind sway (subtle, always active) ---
        f32 windSway = grab ? grab->windSwayScale : 0.15f;
        if (m_WindSystem && windSway > 0.0f) {
            Math::Vector4 windVec = m_WindSystem->GetWindVector();
            Math::Vector3 windForce(windVec.x, windVec.y, windVec.z);
            f32 heightFactor = Math::Clamp(transform->position.y / 2.0f, 0.1f, 1.0f);
            f32 phase = transform->position.x * 0.5f + transform->position.z * 0.3f + windVec.w * 2.0f;
            f32 sway = std::sin(phase) * heightFactor;
            velocity = velocity + windForce * (sway * windSway * dt);
        }

        // NaN guard
        if (std::isnan(velocity.x) || std::isnan(velocity.y) || std::isnan(velocity.z)) {
            velocity = Math::Vector3(0, 0, 0);
        }

        tether->velocity = velocity;

        // Move entity by velocity (FlowerSystem owns all petal movement — no physics backend)
        transform->position = transform->position + velocity * dt;
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

        // Skip broken entities — jelly deformation on a broken/flying petal
        // causes meshDirty, forcing GPU buffer recreation that can race with
        // the in-flight render and crash the driver (VK_ERROR_DEVICE_LOST).
        auto* tetherCheck = m_World->GetComponent<TetherComponent>(entity);
        if (tetherCheck && tetherCheck->isBroken) {
            continue;
        }
        // (Break cleanup in UpdateJointTracking sets isBroken — checked above)

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
// Phase 4: UpdateJointTracking — distance/speed/travel break detection
// Ref: XYTetherJoint break criteria from Iris v3.0
// ---------------------------------------------------------------------------
void FlowerSystem::UpdateJointTracking() {
    if (!m_World) return;

    // Collect break events first — spawning particles during iteration would
    // create entities and invalidate the entity list iterator, causing a freeze.
    m_BreakEvents.clear();

    // Use a fixed dt for travel accumulators (assumes ~60fps, safe approximation)
    f32 dt = 1.0f / 60.0f;

    for (Entity entity : m_World->GetEntitiesWithComponent<TetherComponent>()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || tether->isBroken) continue;
        if (!tether->hadJoint) continue;

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
        if (!connectedTransform) continue;

        // Current anchor positions
        Math::Vector3 anchorA = transform->position;
        Math::Vector3 anchorB = connectedTransform->position + tether->attachLocalPos;
        Math::Vector3 relVec = anchorA - anchorB;
        f32 distance = relVec.Length();

        // Update junction position (30% from entity toward anchor — for particle spawn)
        tether->junctionWorldPos = anchorA + (anchorB - anchorA) * 0.3f;

        // Compute tension (normalized distance beyond rest / maxDistance)
        f32 stretch = distance - tether->restDistance;
        if (stretch < 0.0f) stretch = 0.0f;
        tether->currentTension = Math::Clamp(stretch / tether->maxDistance, 0.0f, 1.0f);

        // --- Travel accumulators (ref: XYTetherJoint.FixedUpdate) ---
        // Absolute travel: total distance anchor A has moved
        f32 anchorADelta = (anchorA - tether->prevAnchorWorldA).Length();
        tether->absoluteTravel += anchorADelta;

        // Relative travel: total change in the A-B vector (stretch accumulator)
        f32 relDelta = (relVec - tether->prevRelVec).Length();
        tether->relativeTravel += relDelta;

        // Velocity estimates
        f32 ownSpeed = anchorADelta / dt;  // How fast the petal itself is moving
        Math::Vector3 anchorBVelocity = (anchorB - tether->lastAnchorWorldB);
        f32 relativeSpeed = ((anchorA - tether->prevAnchorWorldA) - anchorBVelocity).Length() / dt;

        // Cache for next frame
        tether->prevAnchorWorldA = anchorA;
        tether->prevRelVec = relVec;
        tether->lastAnchorWorldA = anchorA;
        tether->lastAnchorWorldB = anchorB;

        // Advance alive timer
        tether->aliveTime += dt;

        // Grab pointer needed for break detection below
        auto* grab = m_World->GetComponent<GrabbableComponent>(entity);

        // --- Break detection (ref: XYTetherJoint) ---
        // Don't check breaks during arm delay grace period
        if (tether->aliveTime < tether->armDelay) continue;

        bool shouldBreak = false;

        // Distance break: stretched beyond max
        if (distance > tether->restDistance + tether->maxDistance) {
            shouldBreak = true;
        }
        // Relative speed break: petal separating too fast from anchor
        if (relativeSpeed > tether->relativeSpeedThreshold) {
            shouldBreak = true;
        }
        // Own speed break: petal moving too fast (absolute)
        if (ownSpeed > tether->ownSpeedThreshold) {
            shouldBreak = true;
        }
        // Absolute travel break: petal has traveled too far total
        if (tether->absoluteTravel > tether->absoluteTravelThreshold) {
            shouldBreak = true;
        }
        // Relative travel break: cumulative stretch exceeded
        if (tether->relativeTravel > tether->relativeTravelThreshold) {
            shouldBreak = true;
        }

        // --- Pluck dwell: hold at high tension for N seconds → auto-break (ref: XYTetherJoint) ---
        if (tether->currentTension >= tether->pluckDwellThreshold) {
            tether->pluckDwellTimer += dt;
            if (tether->pluckDwellTimer >= tether->pluckDwellSeconds) {
                shouldBreak = true;
            }
        } else {
            tether->pluckDwellTimer = 0.0f;  // Reset if tension drops below threshold
        }

        // --- Release pop: pull above high threshold, release below low → auto-break ---
        // Track if we've reached high tension during this grab
        if (tether->currentTension >= tether->releasePopHighThreshold) {
            tether->reachedHighTension = true;
        }
        // If we reached high and now dropped below low → pop!
        if (tether->reachedHighTension && tether->currentTension < tether->releasePopLowThreshold) {
            shouldBreak = true;
        }
        // Reset flag when grab releases (tension returns near zero)
        if (tether->currentTension < 0.05f) {
            tether->reachedHighTension = false;
        }

        if (!shouldBreak) continue;

        // Cap to 1 break per frame to avoid GPU vertex buffer race from
        // simultaneous jelly mesh mutations + particle spawns.
        if (m_BreakEvents.size() >= 1) continue;

        tether->isBroken = true;
        tether->justBroke = true;
        m_DripAccumulator = 0.0f;  // Stop any pending tension drip spawns

        // Pop impulse: kick the petal outward from the anchor point
        // This gives a satisfying snap when the tether breaks
        Math::Vector3 popDir = anchorA - anchorB;
        f32 popLen = popDir.Length();
        if (popLen > 1e-6f) {
            popDir = popDir * (1.0f / popLen);
        } else {
            popDir = Math::Vector3(0.0f, 1.0f, 0.0f);
        }
        // Scale pop by tension — harder pull = bigger pop
        f32 popStrength = 3.0f + tether->currentTension * 5.0f;
        tether->velocity = popDir * popStrength + Math::Vector3(0.0f, 2.0f, 0.0f);

        if (grab) {
            grab->isBroken = true;
        }

        BreakEvent evt;
        evt.entity = entity;
        evt.junctionPos = tether->junctionWorldPos;
        evt.petalPos = anchorA;  // Current petal/leaf position
        evt.color = Math::Vector3(1.0f, 1.0f, 1.0f);
        auto* mat = m_World->GetComponent<MaterialComponent>(entity);
        if (mat) evt.color = mat->baseColor;
        evt.stemEntity = tether->stemEntity;
        auto* tags = m_World->GetComponent<TagComponent>(entity);
        evt.hasWitheredTag = tags && tags->HasTag("withered");
        evt.hasHealthyTag = tags && tags->HasTag("healthy");
        m_BreakEvents.push_back(evt);
    }

    // Spawn particles and update counters outside the entity iteration
    for (const auto& evt : m_BreakEvents) {
        auto* pConfig = m_World->GetComponent<FlowerParticleConfigComponent>(evt.stemEntity);
        SpawnBreakParticles(evt.junctionPos, evt.petalPos, evt.color, pConfig);

        auto* stemComp = m_World->GetComponent<FlowerStemComponent>(evt.stemEntity);
        if (stemComp) {
            stemComp->partsRemoved++;
            if (evt.hasWitheredTag) stemComp->witheredRemoved++;
            else if (evt.hasHealthyTag) stemComp->healthyRemoved++;
        }

        // DO NOT remove JellyMeshComponent or toggle visibility here.
        // The jelly update already skips broken entities (isBroken check).
        // Removing components or hiding entities during break can cause
        // VK_ERROR_DEVICE_LOST because the render path uses persistently
        // mapped GPU buffers with no per-frame synchronization — any
        // structural change (component removal, render data invalidation,
        // pool re-eligibility) while in-flight frames reference the buffer
        // is unsafe. The entity just keeps its last deformed mesh and falls.

        ENJIN_LOG_INFO(Editor, "Tether broken for entity %llu", (unsigned long long)evt.entity);
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

        // If released, apply gravity (FlowerSystem owns all petal movement)
        // Skip already-landed entities (visible=false set by CheckGroundImpact)
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (transform && transform->visible) {
            tether->velocity = tether->velocity + Math::Vector3(0.0f, -9.81f, 0.0f) * dt;
            transform->position = transform->position + tether->velocity * dt;
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

    // Find ground level from first stem (default 0)
    f32 groundY = -10.0f;
    if (m_World) {
        for (Entity entity : m_World->GetEntitiesWithComponent<FlowerStemComponent>()) {
            auto* stem = m_World->GetComponent<FlowerStemComponent>(entity);
            if (stem) { groundY = stem->groundLevel - 0.5f; break; }
        }
    }

    for (auto it = m_Particles.begin(); it != m_Particles.end(); ) {
        it->lifetime += dt;
        // Kill if expired OR fallen below ground
        if (it->lifetime >= it->maxLifetime || it->position.y < groundY) {
            it = m_Particles.erase(it);
            continue;
        }

        // Apply gravity and move
        it->velocity = it->velocity + Math::Vector3(0.0f, -gravity, 0.0f) * dt;
        it->position = it->position + it->velocity * dt;

        // Fade out: shrink based on initial scale (not hardcoded)
        f32 t = it->lifetime / it->maxLifetime;
        it->scale *= (1.0f - t * 0.3f * dt * 60.0f);  // Gradual shrink per frame
        if (it->scale < 0.005f) it->scale = 0.005f;

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
    // P-19: Reuse member vector to avoid per-frame allocation
    m_ImpactEvents.clear();

    for (Entity entity : m_World->GetEntitiesWithComponent<TetherComponent>()) {
        auto* tether = m_World->GetComponent<TetherComponent>(entity);
        if (!tether || !tether->isBroken) continue;

        auto* grab = m_World->GetComponent<GrabbableComponent>(entity);
        if (grab && grab->isGrabbed) continue; // Still held

        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (!transform) continue;

        // Skip already-landed entities (visible=false means already processed)
        if (!transform->visible) continue;

        // Read ground level from stem's FlowerStemComponent (default 0)
        f32 groundY = 0.0f;
        if (tether->stemEntity != INVALID_ENTITY) {
            auto* stemComp = m_World->GetComponent<FlowerStemComponent>(tether->stemEntity);
            if (stemComp) groundY = stemComp->groundLevel;
        }

        // Check if fallen to ground level
        if (transform->position.y <= groundY) {
            transform->position.y = groundY;
            tether->velocity = Math::Vector3(0, 0, 0);

            ImpactEvent evt;
            evt.entity = entity;
            evt.position = transform->position;
            evt.color = Math::Vector3(1.0f, 1.0f, 1.0f);
            auto* mat = m_World->GetComponent<MaterialComponent>(entity);
            if (mat) evt.color = mat->baseColor;
            m_ImpactEvents.push_back(evt);
        }
    }

    // Spawn splash particles, hide petal, and sprout a new flower at impact
    for (const auto& evt : m_ImpactEvents) {
        // Find particle config from this entity's tether -> stem
        const FlowerParticleConfigComponent* splashConfig = nullptr;
        auto* tetherForSplash = m_World->GetComponent<TetherComponent>(evt.entity);
        if (tetherForSplash && tetherForSplash->stemEntity != INVALID_ENTITY) {
            splashConfig = m_World->GetComponent<FlowerParticleConfigComponent>(tetherForSplash->stemEntity);
        }
        SpawnGroundSplash(evt.position, evt.color, splashConfig);

        // Hide the fallen petal/leaf
        auto* impactTransform = m_World->GetComponent<TransformComponent>(evt.entity);
        if (impactTransform) {
            impactTransform->visible = false;
        }

        // Sprout a tiny new flower at the impact point
        SpawnSproutFlower(evt.position, evt.color);
    }
}

// ---------------------------------------------------------------------------
// SpawnBreakParticles — violent sap burst when a petal/leaf tears off.
// Green liquid jets outward + drips down. Big, dramatic, satisfying.
// ---------------------------------------------------------------------------
void FlowerSystem::SpawnBreakParticles(const Math::Vector3& junctionPos, const Math::Vector3& petalPos,
                                       const Math::Vector3& color, const FlowerParticleConfigComponent* config) {
    // Sap color (dark green) and mixed color
    Math::Vector3 sapColor(0.08f, 0.35f, 0.05f);
    Math::Vector3 mixColor = sapColor * 0.6f + color * 0.4f;

    // Direction from junction toward petal (spray direction)
    Math::Vector3 breakDir = petalPos - junctionPos;
    f32 breakLen = breakDir.Length();
    if (breakLen > 1e-6f) breakDir = breakDir * (1.0f / breakLen);
    else breakDir = Math::Vector3(0.0f, 1.0f, 0.0f);

    const int sprayCount = config ? config->breakBurstCount : 10;
    const f32 spraySpeed = config ? config->breakBurstSpeed : 2.5f;
    const f32 sprayLifetime = config ? config->breakBurstLifetime : 0.3f;
    const int dripCount = config ? config->breakDripCount : 6;
    const f32 dripSpeed = config ? config->breakDripSpeed : 3.0f;
    const f32 dripLifetime = config ? config->breakDripLifetime : 0.5f;

    m_Particles.reserve(m_Particles.size() + sprayCount + dripCount + 6);

    // TYPE 1: Long thin sprays — fast streaks from JUNCTION (stem side)
    // These shoot outward from where the tether was attached
    for (int i = 0; i < sprayCount; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;

        f32 angle = static_cast<f32>(i) / static_cast<f32>(sprayCount) * 6.28318f;
        f32 speed = spraySpeed * (0.7f + static_cast<f32>(i % 3) * 0.2f);
        // Spray mostly in break direction with some radial spread
        f32 spread = 0.4f;

        FlowerParticle fp;
        fp.position = junctionPos;
        fp.color = sapColor;
        fp.velocity = breakDir * speed * 0.6f + Math::Vector3(
            std::cos(angle) * spread * speed,
            -1.0f - static_cast<f32>(i % 3) * 0.5f,
            std::sin(angle) * spread * speed
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = sprayLifetime;
        fp.scale = 0.035f;  // Thin streaks
        fp.isLiquid = true;
        m_Particles.push_back(fp);
    }

    // TYPE 2: Thick heavy drops — from PETAL (the part that tore off)
    // These are fat sap globules that drip from the torn edge
    for (int i = 0; i < dripCount; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;

        f32 angle = static_cast<f32>(i) / static_cast<f32>(dripCount) * 6.28318f;

        FlowerParticle fp;
        fp.position = petalPos;
        fp.color = mixColor;
        fp.velocity = Math::Vector3(
            std::cos(angle) * 0.4f,
            -dripSpeed - static_cast<f32>(i % 3) * 1.0f,
            std::sin(angle) * 0.4f
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = dripLifetime;
        fp.scale = 0.08f;  // Fat drops
        fp.isLiquid = true;
        m_Particles.push_back(fp);
    }

    // A few extra drips from junction (stem wound)
    for (int i = 0; i < 3; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;
        f32 angle = static_cast<f32>(i) * 2.09f;  // 120 degree spacing
        FlowerParticle fp;
        fp.position = junctionPos;
        fp.color = sapColor;
        fp.velocity = Math::Vector3(std::cos(angle) * 0.2f, -dripSpeed * 0.7f, std::sin(angle) * 0.2f);
        fp.lifetime = 0.0f;
        fp.maxLifetime = dripLifetime * 0.8f;
        fp.scale = 0.06f;
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

    m_Particles.reserve(m_Particles.size() + count);

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
    if (m_DripAccumulator > 3.0f) m_DripAccumulator = 3.0f;  // Cap backlog
    i32 spawnCount = static_cast<i32>(m_DripAccumulator);
    m_DripAccumulator -= static_cast<f32>(spawnCount);
    if (spawnCount < 1) return;

    m_Particles.reserve(m_Particles.size() + spawnCount);

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
            -0.5f - seed * 1.5f,  // Drip downward
            std::sin(angle) * spread
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = 0.25f + tension * 0.15f;
        fp.scale = 0.04f + tension * 0.03f;
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

// ---------------------------------------------------------------------------
// SpawnSproutFlower — particle burst that looks like a tiny flower sprouting
// from the ground where a petal/leaf landed. Uses particles only (no entity
// creation) to avoid GPU buffer allocation races during the render frame.
// ---------------------------------------------------------------------------
void FlowerSystem::SpawnSproutFlower(const Math::Vector3& position, const Math::Vector3& petalColor) {
    // Sprout particles: small green stems shooting up + colored petal bits
    const int stemCount = 4;
    const int petalBits = 3;

    m_Particles.reserve(m_Particles.size() + stemCount + petalBits);

    // Green stem particles — shoot upward from ground
    for (int i = 0; i < stemCount; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;

        f32 seed = static_cast<f32>((m_Particles.size() * 53 + i * 29) % 100) / 100.0f;
        f32 angle = seed * 6.28318f;

        FlowerParticle fp;
        fp.position = position + Math::Vector3(seed * 0.05f, 0.02f, (1.0f - seed) * 0.05f);
        fp.color = Math::Vector3(0.15f + seed * 0.1f, 0.55f + seed * 0.2f, 0.1f); // Green
        fp.velocity = Math::Vector3(
            std::cos(angle) * 0.15f,
            1.5f + seed * 1.0f,  // Upward sprout
            std::sin(angle) * 0.15f
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = 0.5f + seed * 0.3f;
        fp.scale = 0.03f + seed * 0.02f;
        fp.isLiquid = false;  // Round burst, not streak
        m_Particles.push_back(fp);
    }

    // Colored petal-like particles — small burst with the petal's color
    for (int i = 0; i < petalBits; ++i) {
        if (m_Particles.size() >= MAX_PARTICLES) break;

        f32 seed = static_cast<f32>((m_Particles.size() * 41 + i * 67) % 100) / 100.0f;
        f32 angle = static_cast<f32>(i) / static_cast<f32>(petalBits) * 6.28318f + seed;

        FlowerParticle fp;
        fp.position = position + Math::Vector3(0.0f, 0.1f + seed * 0.05f, 0.0f);
        fp.color = petalColor;
        fp.velocity = Math::Vector3(
            std::cos(angle) * 0.3f,
            2.0f + seed * 0.8f,
            std::sin(angle) * 0.3f
        );
        fp.lifetime = 0.0f;
        fp.maxLifetime = 0.4f + seed * 0.2f;
        fp.scale = 0.04f + seed * 0.02f;
        fp.isLiquid = false;
        m_Particles.push_back(fp);
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
