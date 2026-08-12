#ifdef ENJIN_PHYSICS_BOX2D

#include "Enjin/Physics/Box2DBackend.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Assets/MeshAssetCache.h"   // reload freed CPU verts for collider gen (task #3)
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Physics {

// ============================================================================
// Conversion Helpers
// ============================================================================

static inline b2Vec2 ToBox2D(const Math::Vector2& v) {
    return { v.x, v.y };
}

static inline Math::Vector2 FromBox2D(b2Vec2 v) {
    return Math::Vector2(v.x, v.y);
}

// Extract Z-axis Euler angle (radians) from a quaternion
// Uses direct extraction instead of full Euler decomposition for performance
static f32 GetRotationZ(const ECS::TransformComponent& t) {
    return t.rotation.GetRotationZ();
}

static Math::Vector2 GetPosition2D(const ECS::TransformComponent& t) {
    return Math::Vector2(t.position.x, t.position.y);
}

// Encode entity ID into a void* for Box2D userData
static void* EntityToUserData(ECS::Entity entity) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(entity));
}

static ECS::Entity UserDataToEntity(void* userData) {
    return static_cast<ECS::Entity>(reinterpret_cast<uintptr_t>(userData));
}

// ============================================================================
// Box2DBackend Implementation
// ============================================================================

Box2DBackend::Box2DBackend() = default;

Box2DBackend::~Box2DBackend() {
    Shutdown();
}

void Box2DBackend::Initialize(ECS::World* world) {
    m_World = world;
    if (!m_World) return;

    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = ToBox2D(m_Gravity);

    m_WorldId = b2CreateWorld(&worldDef);
    m_Initialized = true;

    ENJIN_LOG_INFO(Physics, "Box2D Physics initialized (sub-steps: %u)", m_SubStepCount);
}

void Box2DBackend::Shutdown() {
    if (!m_Initialized) return;

    // Clear all tracking maps before destroying the world
    m_EntityToBody.clear();
    m_BodyUserDataToEntity.clear();
    m_EntityToJoint.clear();
    m_ActiveContacts.clear();
    m_ActiveSensorContacts.clear();

    b2DestroyWorld(m_WorldId);
    m_WorldId = b2_nullWorldId;
    m_World = nullptr;
    m_Initialized = false;
}

void Box2DBackend::SetGravity(const Math::Vector2& gravity) {
    m_Gravity = gravity;
    if (m_Initialized) {
        b2World_SetGravity(m_WorldId, ToBox2D(gravity));
    }
}

Math::Vector2 Box2DBackend::GetGravity() const {
    return m_Gravity;
}

void Box2DBackend::SetVelocityIterations(u32 iterations) {
    // Box2D v3 uses subStepCount instead of velocity/position iterations
    m_SubStepCount = std::max(1u, iterations / 2);
}

void Box2DBackend::SetPositionIterations(u32 /*iterations*/) {
    // Box2D v3 handles this internally via sub-stepping
}

void Box2DBackend::SetOnCollisionEnter(CollisionCallback cb) { m_OnCollisionEnter = cb; }
void Box2DBackend::SetOnCollisionExit(CollisionCallback cb) { m_OnCollisionExit = cb; }
void Box2DBackend::SetOnSensorEnter(CollisionCallback cb) { m_OnSensorEnter = cb; }
void Box2DBackend::SetOnSensorExit(CollisionCallback cb) { m_OnSensorExit = cb; }

void Box2DBackend::SetCCDEnabled(bool enabled) {
    m_CCDEnabled = enabled;
}

// ============================================================================
// Update Loop
// ============================================================================

void Box2DBackend::Update(f32 deltaTime) {
    if (!m_World || !m_Initialized || deltaTime <= 0.0f) return;

    m_LastDeltaTime = deltaTime;

    // 1. Sync ECS state to Box2D bodies
    SyncECSToBox2D();

    // 2. Sync joint components to Box2D joints
    SyncJointsToBox2D();

    // 3. Apply gravity zone overrides
    ApplyGravityZones();

    // 4. Step the Box2D world
    b2World_Step(m_WorldId, deltaTime, static_cast<int>(m_SubStepCount));

    // 5. Write Box2D state back to ECS
    SyncBox2DToECS();

    // 6. Process contact and sensor events
    ProcessEvents();
}

void Box2DBackend::SyncAndProcessEvents() {
    if (!m_World || !m_Initialized) return;

    // Re-sync kinematic body positions (controllers may have moved them)
    SyncECSToBox2D();

    // Micro-step to trigger overlap detection without meaningful simulation advance
    b2World_Step(m_WorldId, 0.0001f, 1);

    // Fire any new sensor/collision events from the updated positions
    ProcessEvents();
}

// ============================================================================
// ECS → Box2D Sync
// ============================================================================

void Box2DBackend::SyncECSToBox2D() {
    // Build set of entities that currently have Body2DComponent (reuse member to avoid per-frame alloc)
    m_CurrentEntitiesCache.clear();
    for (ECS::Entity e : m_World->GetEntitiesWithComponent<Body2DComponent>()) {
        m_CurrentEntitiesCache.insert(e);
    }
    const auto& currentEntities = m_CurrentEntitiesCache;

    // Create bodies for new entities
    for (ECS::Entity entity : currentEntities) {
        if (m_EntityToBody.find(entity) == m_EntityToBody.end()) {
            CreateBodyForEntity(entity);
        }
    }

    // Destroy bodies for removed entities (reuse member to avoid per-frame alloc)
    m_ToRemoveCache.clear();
    for (auto& [entity, bodyId] : m_EntityToBody) {
        if (currentEntities.find(entity) == currentEntities.end()) {
            m_ToRemoveCache.push_back(entity);
        }
    }
    for (ECS::Entity entity : m_ToRemoveCache) {
        DestroyBodyForEntity(entity);
    }

    // Update kinematic/static bodies and property changes.
    // Also sync sensor bodies from ECS → Box2D so that entities driven by
    // controllers, AI, or tweens (player, pickups, enemies) keep their
    // Box2D body in sync without Box2D overwriting their position.
    for (auto& [entity, bodyId] : m_EntityToBody) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        auto* body2d = m_World->GetComponent<Body2DComponent>(entity);
        if (!transform || !body2d) continue;

        if (body2d->isKinematic) {
            // Kinematic bodies: compute velocity to reach ECS position.
            // Using SetLinearVelocity instead of SetTransform ensures Box2D
            // properly detects sensor overlaps during the step.
            b2Vec2 currentPos = b2Body_GetPosition(bodyId);
            Math::Vector2 ecsPos = GetPosition2D(*transform);
            f32 ecsAngle = GetRotationZ(*transform);
            f32 dt = m_LastDeltaTime > 0.0f ? m_LastDeltaTime : (1.0f / 60.0f);

            b2Vec2 vel = {(ecsPos.x - currentPos.x) / dt, (ecsPos.y - currentPos.y) / dt};
            b2Body_SetLinearVelocity(bodyId, vel);

            // Also set angle directly (rotation doesn't affect sensors much)
            b2Rot currentRot = b2Body_GetRotation(bodyId);
            f32 currentAngle = b2Rot_GetAngle(currentRot);
            if (std::abs(currentAngle - ecsAngle) > 0.01f) {
                b2Body_SetTransform(bodyId, currentPos, b2MakeRot(ecsAngle));
            }
        } else if (body2d->isStatic || body2d->isSensor) {
            // Static/sensor bodies: teleport is fine (they don't need smooth motion)
            b2Vec2 currentPos = b2Body_GetPosition(bodyId);
            Math::Vector2 ecsPos = GetPosition2D(*transform);
            f32 ecsAngle = GetRotationZ(*transform);

            if (std::abs(currentPos.x - ecsPos.x) > 0.001f ||
                std::abs(currentPos.y - ecsPos.y) > 0.001f) {
                b2Body_SetTransform(bodyId, ToBox2D(ecsPos), b2MakeRot(ecsAngle));
            }
        }
    }
}

void Box2DBackend::CreateBodyForEntity(ECS::Entity entity) {
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
    auto* body2d = m_World->GetComponent<Body2DComponent>(entity);
    if (!transform || !body2d) return;

    // Guard against duplicate body creation — destroy existing body first
    if (m_EntityToBody.find(entity) != m_EntityToBody.end()) {
        ENJIN_LOG_WARN(Physics, "CreateBodyForEntity called for entity %llu that already has a body — replacing",
                       static_cast<unsigned long long>(entity));
        DestroyBodyForEntity(entity);
    }

    Math::Vector2 pos = GetPosition2D(*transform);
    f32 angle = GetRotationZ(*transform);

    // Body definition
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.position = ToBox2D(pos);
    bodyDef.rotation = b2MakeRot(angle);
    bodyDef.linearDamping = body2d->linearDamping;
    bodyDef.angularDamping = body2d->angularDamping;
    bodyDef.gravityScale = body2d->gravityScale;
    bodyDef.userData = EntityToUserData(entity);
    bodyDef.fixedRotation = body2d->fixedRotation;

    if (body2d->isStatic) {
        bodyDef.type = b2_staticBody;
    } else if (body2d->isKinematic) {
        bodyDef.type = b2_kinematicBody;
    } else {
        bodyDef.type = b2_dynamicBody;
    }

    // CCD for fast-moving bodies
    if (m_CCDEnabled && !body2d->isStatic) {
        bodyDef.isBullet = true;
    }

    // Initial velocity
    bodyDef.linearVelocity = ToBox2D(body2d->velocity);
    bodyDef.angularVelocity = body2d->angularVelocity;

    b2BodyId bodyId = b2CreateBody(m_WorldId, &bodyDef);

    // Shape definition
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    // P6 fix: Clamp material properties to valid ranges
    shapeDef.density = std::max(body2d->material.density, 0.0f);
    shapeDef.friction = std::clamp(body2d->material.friction, 0.0f, 1.0f);
    shapeDef.restitution = std::clamp(body2d->material.restitution, 0.0f, 1.0f);
    shapeDef.isSensor = body2d->isSensor;
    // Box2D v3 sensor event flags:
    // - Sensor shapes MUST have enableSensorEvents=true to generate begin/end events
    // - Visitor shapes (kinematic/dynamic) also need enableSensorEvents=true to participate
    // - Static non-sensor shapes don't need sensor events (they're walls/platforms)
    // BUG FIX: Previously set enableSensorEvents=!isStatic, which disabled events on
    // static sensors (spikes, hazards). Static sensors need events to detect player overlap.
    shapeDef.enableSensorEvents = body2d->isSensor || !body2d->isStatic;
    shapeDef.enableContactEvents = !body2d->isSensor;

    // Collision filtering (v3.0.0 uses uint32_t)
    shapeDef.filter.categoryBits = body2d->categoryBits;
    shapeDef.filter.maskBits = body2d->collisionMask;

    // Create shape based on type
    switch (body2d->shapeType) {
        case Shape2DType::Circle: {
            b2Circle circle;
            circle.center = ToBox2D(body2d->circle.offset);
            circle.radius = std::max(body2d->circle.radius, 0.01f);
            b2CreateCircleShape(bodyId, &shapeDef, &circle);
            break;
        }
        case Shape2DType::Box: {
            f32 hx = std::max(body2d->box.halfExtents.x, 0.01f);
            f32 hy = std::max(body2d->box.halfExtents.y, 0.01f);
            b2Polygon box = b2MakeOffsetBox(hx, hy,
                ToBox2D(body2d->box.offset), body2d->box.rotation);
            b2CreatePolygonShape(bodyId, &shapeDef, &box);
            break;
        }
        case Shape2DType::Polygon: {
            const auto& verts = body2d->polygon.vertices;
            if (verts.size() >= 3 && verts.size() <= 8) {
                b2Vec2 points[8];
                int count = static_cast<int>(std::min(verts.size(), static_cast<size_t>(8)));
                for (int i = 0; i < count; ++i) {
                    points[i] = ToBox2D(verts[i] + body2d->polygon.offset);
                }
                b2Hull hull = b2ComputeHull(points, count);
                if (hull.count > 0) {
                    b2Polygon poly = b2MakePolygon(&hull, 0.0f);
                    b2CreatePolygonShape(bodyId, &shapeDef, &poly);
                }
            } else {
                // Fallback to a unit box if polygon is invalid
                ENJIN_LOG_WARN(Physics, "Polygon has %zu vertices (must be 3-8) for entity %llu — using unit box fallback",
                               verts.size(), static_cast<unsigned long long>(entity));
                b2Polygon box = b2MakeBox(0.5f, 0.5f);
                b2CreatePolygonShape(bodyId, &shapeDef, &box);
            }
            break;
        }
        case Shape2DType::Capsule: {
            f32 r = std::max(body2d->capsule.radius, 0.01f);
            f32 h = std::max(body2d->capsule.height, r * 2.0f + 0.01f);
            // b2Capsule: two semicircle centers + radius
            // Height is total including caps, so center-to-center = height - 2*radius
            f32 halfSegment = (h - 2.0f * r) * 0.5f;
            b2Capsule capsule;
            capsule.center1 = { body2d->capsule.offset.x, body2d->capsule.offset.y + halfSegment };
            capsule.center2 = { body2d->capsule.offset.x, body2d->capsule.offset.y - halfSegment };
            capsule.radius = r;
            b2CreateCapsuleShape(bodyId, &shapeDef, &capsule);
            break;
        }
    }

    // MeshCollider override: project mesh vertices to XY and create convex polygon
    if (auto* meshCol = m_World->GetComponent<ECS::MeshColliderComponent>(entity)) {
        if (meshCol->autoGenerate && !meshCol->generated) {
            auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
            if (mesh) Assets::MeshAssetCache::Get().EnsureCpuData(*mesh);  // reload if CPU verts were freed after GPU upload
            if (mesh && mesh->IsValid()) {
                meshCol->vertices.clear();
                meshCol->vertices.reserve(mesh->vertices.size());
                for (const auto& v : mesh->vertices) {
                    meshCol->vertices.push_back(v.position);
                }
                meshCol->indices = mesh->indices;
                meshCol->generated = true;
            }
        }

        if (meshCol->generated && !meshCol->vertices.empty()) {
            // Project 3D vertices to 2D (XY plane) and build convex hull
            // Box2D supports max 8 vertices per polygon
            b2Vec2 points[8];
            int count = static_cast<int>(std::min(meshCol->vertices.size(), static_cast<size_t>(8)));

            if (meshCol->vertices.size() <= 8) {
                for (int i = 0; i < count; ++i) {
                    points[i] = { meshCol->vertices[i].x, meshCol->vertices[i].y };
                }
            } else {
                // Sample vertices evenly when there are more than 8
                for (int i = 0; i < 8; ++i) {
                    size_t idx = (i * meshCol->vertices.size()) / 8;
                    points[i] = { meshCol->vertices[idx].x, meshCol->vertices[idx].y };
                }
            }

            b2Hull hull = b2ComputeHull(points, count);
            if (hull.count > 0) {
                // Override collision filter from mesh collider
                shapeDef.filter.categoryBits = meshCol->categoryBits;
                shapeDef.filter.maskBits = meshCol->collisionMask;
                shapeDef.isSensor = meshCol->isTrigger;
                shapeDef.friction = std::clamp(meshCol->friction, 0.0f, 1.0f);
                shapeDef.restitution = std::clamp(meshCol->bounciness, 0.0f, 1.0f);
                b2Polygon poly = b2MakePolygon(&hull, 0.0f);
                b2CreatePolygonShape(bodyId, &shapeDef, &poly);
            } else {
                ENJIN_LOG_WARN(Physics, "MeshCollider 2D hull failed for entity %llu — mesh has %zu vertices",
                               static_cast<unsigned long long>(entity), meshCol->vertices.size());
            }
        }
    }

    m_EntityToBody[entity] = bodyId;
    m_BodyUserDataToEntity[static_cast<uint64_t>(entity)] = entity;
}

void Box2DBackend::DestroyBodyForEntity(ECS::Entity entity) {
    auto it = m_EntityToBody.find(entity);
    if (it == m_EntityToBody.end()) return;

    // Remove any joints first
    DestroyJointForEntity(entity);

    b2DestroyBody(it->second);
    m_BodyUserDataToEntity.erase(static_cast<uint64_t>(entity));
    m_EntityToBody.erase(it);

    // Clean up stale collision/sensor pairs involving this entity
    auto purgeEntity = [entity](std::unordered_set<u64>& pairs) {
        for (auto pit = pairs.begin(); pit != pairs.end(); ) {
            u64 key = *pit;
            ECS::Entity a = static_cast<ECS::Entity>(key >> 32);
            ECS::Entity b = static_cast<ECS::Entity>(key & 0xFFFFFFFF);
            if (a == entity || b == entity)
                pit = pairs.erase(pit);
            else
                ++pit;
        }
    };
    purgeEntity(m_ActiveContacts);
    purgeEntity(m_ActiveSensorContacts);
}

// ============================================================================
// Box2D → ECS Sync
// ============================================================================

void Box2DBackend::SyncBox2DToECS() {
    for (auto& [entity, bodyId] : m_EntityToBody) {
        // P8 fix: Verify body is still valid before accessing properties
        if (!b2Body_IsValid(bodyId)) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        auto* body2d = m_World->GetComponent<Body2DComponent>(entity);
        if (!transform || !body2d) continue;

        // Only sync dynamic non-sensor non-kinematic bodies back to ECS.
        // Sensor/kinematic bodies are driven by ECS (controllers, AI, tweens) — don't overwrite.
        if (body2d->isStatic || body2d->isSensor || body2d->isKinematic) continue;

        // Position
        b2Vec2 pos = b2Body_GetPosition(bodyId);
        transform->position.x = pos.x;
        transform->position.y = pos.y;

        // Rotation (Z axis only for 2D) — construct quaternion directly from Z angle
        b2Rot rot = b2Body_GetRotation(bodyId);
        f32 newAngle = b2Rot_GetAngle(rot);
        transform->rotation = Math::Quaternion(Math::Vector3(0.0f, 0.0f, 1.0f), newAngle);

        // Velocity
        b2Vec2 linearVel = b2Body_GetLinearVelocity(bodyId);
        body2d->velocity.x = linearVel.x;
        body2d->velocity.y = linearVel.y;

        body2d->angularVelocity = b2Body_GetAngularVelocity(bodyId);

        // Update mass from Box2D
        b2MassData massData = b2Body_GetMassData(bodyId);
        body2d->mass = massData.mass;
        body2d->inverseMass = (massData.mass > 0.0001f) ? 1.0f / massData.mass : 0.0f;
        body2d->inertia = massData.rotationalInertia;
        body2d->inverseInertia = (massData.rotationalInertia > 0.0001f) ? 1.0f / massData.rotationalInertia : 0.0f;
    }
}

// ============================================================================
// Contact and Sensor Event Processing
// ============================================================================

ECS::Entity Box2DBackend::ResolveEntity(b2ShapeId shapeId) const {
    if (!b2Shape_IsValid(shapeId)) return 0;
    b2BodyId bodyId = b2Shape_GetBody(shapeId);
    if (!b2Body_IsValid(bodyId)) return 0;
    void* userData = b2Body_GetUserData(bodyId);
    if (!userData) return 0;
    return UserDataToEntity(userData);
}

void Box2DBackend::ProcessEvents() {
    // --- Contact events ---
    b2ContactEvents contacts = b2World_GetContactEvents(m_WorldId);

    m_NewContactsCache.clear();
    auto& newContacts = m_NewContactsCache;

    // Begin touch events
    for (int i = 0; i < contacts.beginCount; ++i) {
        const b2ContactBeginTouchEvent& evt = contacts.beginEvents[i];
        ECS::Entity entityA = ResolveEntity(evt.shapeIdA);
        ECS::Entity entityB = ResolveEntity(evt.shapeIdB);
        if (entityA == 0 || entityB == 0) continue;

        u64 pairKey = MakeCollisionPairKey(entityA, entityB);
        newContacts.insert(pairKey);

        if (m_ActiveContacts.find(pairKey) == m_ActiveContacts.end()) {
            Contact2D contact;
            contact.entityA = entityA;
            contact.entityB = entityB;
            // Use body positions as contact point approximation
            auto* tA = m_World->GetComponent<ECS::TransformComponent>(entityA);
            auto* tB = m_World->GetComponent<ECS::TransformComponent>(entityB);
            if (tA && tB) {
                contact.point = Math::Vector2(
                    (tA->position.x + tB->position.x) * 0.5f,
                    (tA->position.y + tB->position.y) * 0.5f);
                Math::Vector2 diff(tB->position.x - tA->position.x, tB->position.y - tA->position.y);
                f32 len = diff.Length();
                contact.normal = (len > 0.001f) ? diff * (1.0f / len) : Math::Vector2(0.0f, 1.0f);
            }

            if (m_OnCollisionEnter) m_OnCollisionEnter(contact);
        }
    }

    // Merge: carry forward all previous active contacts that haven't ended this frame.
    // This prevents losing contacts that are still active but didn't fire a new beginEvent.
    for (u64 prevKey : m_ActiveContacts) {
        newContacts.insert(prevKey);
    }

    // End touch events — remove ended contacts and fire callbacks
    for (int i = 0; i < contacts.endCount; ++i) {
        const b2ContactEndTouchEvent& evt = contacts.endEvents[i];
        // Shapes may have been destroyed, check validity
        if (!b2Shape_IsValid(evt.shapeIdA) || !b2Shape_IsValid(evt.shapeIdB)) continue;

        ECS::Entity entityA = ResolveEntity(evt.shapeIdA);
        ECS::Entity entityB = ResolveEntity(evt.shapeIdB);
        if (entityA == 0 || entityB == 0) continue;

        u64 pairKey = MakeCollisionPairKey(entityA, entityB);

        if (newContacts.find(pairKey) != newContacts.end()) {
            newContacts.erase(pairKey);

            Contact2D contact;
            contact.entityA = entityA;
            contact.entityB = entityB;

            if (m_OnCollisionExit) m_OnCollisionExit(contact);
        }
    }

    m_ActiveContacts = std::move(newContacts);

    // --- Sensor events ---
    b2SensorEvents sensors = b2World_GetSensorEvents(m_WorldId);

    m_NewSensorContactsCache.clear();
    auto& newSensorContacts = m_NewSensorContactsCache;

    for (int i = 0; i < sensors.beginCount; ++i) {
        const b2SensorBeginTouchEvent& evt = sensors.beginEvents[i];
        ECS::Entity sensorEntity = ResolveEntity(evt.sensorShapeId);
        ECS::Entity visitorEntity = ResolveEntity(evt.visitorShapeId);
        if (sensorEntity == 0 || visitorEntity == 0) continue;

        u64 pairKey = MakeCollisionPairKey(sensorEntity, visitorEntity);
        newSensorContacts.insert(pairKey);

        if (m_ActiveSensorContacts.find(pairKey) == m_ActiveSensorContacts.end()) {
            Contact2D contact;
            contact.entityA = sensorEntity;
            contact.entityB = visitorEntity;

            if (m_OnSensorEnter) m_OnSensorEnter(contact);
        }
    }

    // Carry forward active sensor contacts that haven't ended this frame
    for (u64 prevKey : m_ActiveSensorContacts) {
        newSensorContacts.insert(prevKey);
    }

    for (int i = 0; i < sensors.endCount; ++i) {
        const b2SensorEndTouchEvent& evt = sensors.endEvents[i];
        if (!b2Shape_IsValid(evt.sensorShapeId) || !b2Shape_IsValid(evt.visitorShapeId)) continue;

        ECS::Entity sensorEntity = ResolveEntity(evt.sensorShapeId);
        ECS::Entity visitorEntity = ResolveEntity(evt.visitorShapeId);
        if (sensorEntity == 0 || visitorEntity == 0) continue;

        u64 pairKey = MakeCollisionPairKey(sensorEntity, visitorEntity);

        if (m_ActiveSensorContacts.find(pairKey) != m_ActiveSensorContacts.end()) {
            Contact2D contact;
            contact.entityA = sensorEntity;
            contact.entityB = visitorEntity;

            if (m_OnSensorExit) m_OnSensorExit(contact);
        }

        newSensorContacts.erase(pairKey);
    }

    m_ActiveSensorContacts = std::move(newSensorContacts);
}

// ============================================================================
// Raycasting
// ============================================================================

bool Box2DBackend::Raycast(const Math::Vector2& origin, const Math::Vector2& direction,
                           f32 maxDistance, RayHit2D& outHit, u32 layerMask) const {
    if (!m_Initialized) return false;

    f32 dirLen = direction.Length();
    if (dirLen < 0.0001f) return false;  // Zero-length direction
    Math::Vector2 dir = direction * (1.0f / dirLen);
    b2Vec2 translation = { dir.x * maxDistance, dir.y * maxDistance };

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = layerMask;
    filter.maskBits = layerMask;

    // Use CastRay (callback) instead of CastRayClosest so we can skip sensors.
    // Sensors (enemies, coins, pickups) should not block wall/ground raycasts.
    struct RayCtx {
        const Box2DBackend* self;
        ECS::World* world;
        RayHit2D bestHit;
        f32 bestFraction;
        bool found;
    };
    RayCtx ctx{ this, m_World, {}, 1.0f, false };

    auto callback = [](b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context) -> float {
        auto* c = static_cast<RayCtx*>(context);
        ECS::Entity entity = c->self->ResolveEntity(shapeId);
        // Skip sensor bodies (enemies, coins, pickups) — they shouldn't block movement
        if (entity != 0 && c->world) {
            auto* body2d = c->world->GetComponent<Physics::Body2DComponent>(entity);
            if (body2d && body2d->isSensor) return 1.0f;  // Continue searching
        }
        if (fraction < c->bestFraction) {
            c->bestFraction = fraction;
            c->bestHit.point = FromBox2D(point);
            c->bestHit.normal = FromBox2D(normal);
            c->bestHit.distance = fraction;
            c->bestHit.entity = entity;
            c->found = true;
        }
        return fraction;  // Clip to this distance
    };

    b2World_CastRay(m_WorldId, ToBox2D(origin), translation, filter, callback, &ctx);

    if (ctx.found) {
        outHit = ctx.bestHit;
        outHit.distance *= maxDistance;  // Convert fraction to actual distance
        return true;
    }

    return false;
}

// Raycast callback context for collecting all hits
struct RaycastAllContext {
    const Box2DBackend* backend;
    std::vector<RayHit2D>* hits;
    f32 maxDistance;
};

static float RaycastAllCallback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context) {
    auto* ctx = static_cast<RaycastAllContext*>(context);

    RayHit2D hit;
    hit.point = FromBox2D(point);
    hit.normal = FromBox2D(normal);
    hit.distance = fraction * ctx->maxDistance;
    hit.entity = ctx->backend->ResolveEntity(shapeId);

    ctx->hits->push_back(hit);
    return 1.0f; // Continue collecting all hits
}

std::vector<RayHit2D> Box2DBackend::RaycastAll(const Math::Vector2& origin, const Math::Vector2& direction,
                                                f32 maxDistance, u32 layerMask) const {
    std::vector<RayHit2D> hits;
    if (!m_Initialized) return hits;

    f32 dirLen = direction.Length();
    if (dirLen < 0.0001f) return hits;  // Zero-length direction
    Math::Vector2 dir = direction * (1.0f / dirLen);
    b2Vec2 translation = { dir.x * maxDistance, dir.y * maxDistance };

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = layerMask;
    filter.maskBits = layerMask;

    RaycastAllContext ctx;
    ctx.backend = this;
    ctx.hits = &hits;
    ctx.maxDistance = maxDistance;

    b2World_CastRay(m_WorldId, ToBox2D(origin), translation, filter, RaycastAllCallback, &ctx);

    // Sort by distance
    std::sort(hits.begin(), hits.end(), [](const RayHit2D& a, const RayHit2D& b) {
        return a.distance < b.distance;
    });

    return hits;
}

// ============================================================================
// Overlap Queries
// ============================================================================

struct OverlapContext {
    const Box2DBackend* backend;
    std::vector<ECS::Entity>* entities;
};

static bool OverlapCallback(b2ShapeId shapeId, void* context) {
    auto* ctx = static_cast<OverlapContext*>(context);
    ECS::Entity entity = ctx->backend->ResolveEntity(shapeId);
    if (entity != 0) {
        ctx->entities->push_back(entity);
    }
    return true; // Continue collecting
}

bool Box2DBackend::OverlapCircle(const Math::Vector2& center, f32 radius,
                                  std::vector<ECS::Entity>& outEntities, u32 layerMask) const {
    outEntities.clear();
    if (!m_Initialized) return false;

    // Use AABB overlap as broad phase, then filter by circle distance
    b2AABB aabb;
    aabb.lowerBound = { center.x - radius, center.y - radius };
    aabb.upperBound = { center.x + radius, center.y + radius };

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = layerMask;
    filter.maskBits = layerMask;

    // Collect all entities in AABB
    std::vector<ECS::Entity> candidates;
    OverlapContext ctx;
    ctx.backend = this;
    ctx.entities = &candidates;

    b2World_OverlapAABB(m_WorldId, aabb, filter, OverlapCallback, &ctx);

    // Filter by actual circle distance
    f32 radiusSq = radius * radius;
    for (ECS::Entity entity : candidates) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        Math::Vector2 pos = GetPosition2D(*transform);
        Math::Vector2 diff = pos - center;
        if (diff.LengthSquared() <= radiusSq) {
            outEntities.push_back(entity);
        }
    }

    return !outEntities.empty();
}

bool Box2DBackend::OverlapBox(const Math::Vector2& center, const Math::Vector2& halfExtents,
                               std::vector<ECS::Entity>& outEntities, u32 layerMask) const {
    outEntities.clear();
    if (!m_Initialized) return false;

    b2AABB aabb;
    aabb.lowerBound = { center.x - halfExtents.x, center.y - halfExtents.y };
    aabb.upperBound = { center.x + halfExtents.x, center.y + halfExtents.y };

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = layerMask;
    filter.maskBits = layerMask;

    OverlapContext ctx;
    ctx.backend = this;
    ctx.entities = &outEntities;

    b2World_OverlapAABB(m_WorldId, aabb, filter, OverlapCallback, &ctx);

    return !outEntities.empty();
}

// ============================================================================
// Gravity Zones
// ============================================================================

void Box2DBackend::ApplyGravityZones() {
    auto gravityZoneEntities = m_World->GetEntitiesWithComponent<ECS::GravityZoneComponent>();
    if (gravityZoneEntities.empty()) return;

    for (auto& [entity, bodyId] : m_EntityToBody) {
        auto* body2d = m_World->GetComponent<Body2DComponent>(entity);
        if (!body2d || body2d->isStatic || body2d->isSensor) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // Build a Vector3 with z=0 for ContainsPoint compatibility
        Math::Vector3 bodyPos(transform->position.x, transform->position.y, 0.0f);

        // Find highest-priority active gravity zone containing this body
        i32 bestPriority = INT_MIN;
        Math::Vector3 customGravity;
        bool inZone = false;

        for (ECS::Entity zoneEntity : gravityZoneEntities) {
            auto* zone = m_World->GetComponent<ECS::GravityZoneComponent>(zoneEntity);
            auto* zoneTransform = m_World->GetComponent<ECS::TransformComponent>(zoneEntity);
            if (!zone || !zoneTransform || !zone->isActive) continue;
            if (zone->priority <= bestPriority) continue;

            Math::Vector3 zoneCenter(zoneTransform->position.x, zoneTransform->position.y, 0.0f);
            if (zone->ContainsPoint(zoneCenter, bodyPos)) {
                customGravity = zone->GetGravityAt(zoneCenter, bodyPos);
                bestPriority = zone->priority;
                inZone = true;
            }
        }

        if (inZone) {
            // Disable world gravity for this body
            b2Body_SetGravityScale(bodyId, 0.0f);
            // Apply custom gravity as a force: F = gravity * gravityScale * mass
            f32 mass = b2Body_GetMass(bodyId);
            if (mass > 0.0001f) {
                b2Vec2 force = { customGravity.x * body2d->gravityScale * mass,
                                 customGravity.y * body2d->gravityScale * mass };
                b2Body_ApplyForceToCenter(bodyId, force, true);
            }
        } else {
            // Restore the component's gravity scale
            b2Body_SetGravityScale(bodyId, body2d->gravityScale);
        }
    }
}

// ============================================================================
// Joint Synchronization
// ============================================================================

void Box2DBackend::SyncJointsToBox2D() {
    std::unordered_set<ECS::Entity> currentJointEntities;

    for (ECS::Entity e : m_World->GetEntitiesWithComponent<Joint2DComponent>()) {
        currentJointEntities.insert(e);
        if (m_EntityToJoint.find(e) == m_EntityToJoint.end()) {
            CreateJointForEntity(e);
        }
    }

    // Remove joints for deleted entities (reuse member to avoid per-frame alloc)
    m_JointToRemoveCache.clear();
    for (auto& [entity, jointId] : m_EntityToJoint) {
        if (currentJointEntities.find(entity) == currentJointEntities.end()) {
            m_JointToRemoveCache.push_back(entity);
        }
    }
    for (ECS::Entity e : m_JointToRemoveCache) {
        DestroyJointForEntity(e);
    }
}

void Box2DBackend::CreateJointForEntity(ECS::Entity entity) {
    auto* joint = m_World->GetComponent<Joint2DComponent>(entity);
    if (!joint || joint->connectedEntity == 0) return;

    // Find bodies for both entities
    auto itA = m_EntityToBody.find(entity);
    auto itB = m_EntityToBody.find(joint->connectedEntity);
    if (itA == m_EntityToBody.end() || itB == m_EntityToBody.end()) return;

    b2BodyId bodyA = itA->second;
    b2BodyId bodyB = itB->second;

    b2JointId jointId = b2_nullJointId;

    switch (joint->type) {
        case Joint2DType::Revolute: {
            b2RevoluteJointDef def = b2DefaultRevoluteJointDef();
            def.bodyIdA = bodyA;
            def.bodyIdB = bodyB;
            def.localAnchorA = ToBox2D(joint->anchorA);
            def.localAnchorB = ToBox2D(joint->anchorB);
            def.collideConnected = joint->collideConnected;
            def.enableLimit = joint->enableLimit;
            def.lowerAngle = joint->lowerAngle;
            def.upperAngle = joint->upperAngle;
            def.enableMotor = joint->enableMotor;
            def.motorSpeed = joint->motorSpeed;
            def.maxMotorTorque = joint->maxMotorTorque;

            jointId = b2CreateRevoluteJoint(m_WorldId, &def);
            break;
        }
        case Joint2DType::Prismatic: {
            b2PrismaticJointDef def = b2DefaultPrismaticJointDef();
            def.bodyIdA = bodyA;
            def.bodyIdB = bodyB;
            def.localAnchorA = ToBox2D(joint->anchorA);
            def.localAnchorB = ToBox2D(joint->anchorB);
            Math::Vector2 axis = joint->axis;
            f32 axisLen = axis.Length();
            if (axisLen < 0.0001f) axis = Math::Vector2(1.0f, 0.0f);  // Default to X axis
            else axis = axis * (1.0f / axisLen);
            def.localAxisA = ToBox2D(axis);
            def.collideConnected = joint->collideConnected;
            def.enableLimit = joint->enableLimit;
            def.lowerTranslation = joint->lowerTranslation;
            def.upperTranslation = joint->upperTranslation;
            def.enableMotor = joint->enableMotor;
            def.motorSpeed = joint->motorSpeed;
            def.maxMotorForce = joint->maxMotorTorque; // Reusing torque field as force

            jointId = b2CreatePrismaticJoint(m_WorldId, &def);
            break;
        }
        case Joint2DType::Distance: {
            b2DistanceJointDef def = b2DefaultDistanceJointDef();
            def.bodyIdA = bodyA;
            def.bodyIdB = bodyB;
            def.localAnchorA = ToBox2D(joint->anchorA);
            def.localAnchorB = ToBox2D(joint->anchorB);
            def.collideConnected = joint->collideConnected;
            def.length = joint->length;

            if (joint->stiffness > 0.0f) {
                def.enableSpring = true;
                def.hertz = joint->stiffness;
                def.dampingRatio = joint->damping;
            }

            if (joint->minLength > 0.0f || joint->maxLength > 0.0f) {
                def.enableLimit = true;
                def.minLength = joint->minLength;
                def.maxLength = (joint->maxLength > 0.0f) ? joint->maxLength : joint->length * 2.0f;
            }

            jointId = b2CreateDistanceJoint(m_WorldId, &def);
            break;
        }
        case Joint2DType::Rope: {
            // Rope is a distance joint with enableLimit + maxLength
            b2DistanceJointDef def = b2DefaultDistanceJointDef();
            def.bodyIdA = bodyA;
            def.bodyIdB = bodyB;
            def.localAnchorA = ToBox2D(joint->anchorA);
            def.localAnchorB = ToBox2D(joint->anchorB);
            def.collideConnected = joint->collideConnected;
            def.length = joint->length;
            def.enableLimit = true;
            def.minLength = 0.0f;
            def.maxLength = (joint->maxLength > 0.0f) ? joint->maxLength : joint->length;

            jointId = b2CreateDistanceJoint(m_WorldId, &def);
            break;
        }
        case Joint2DType::Weld: {
            b2WeldJointDef def = b2DefaultWeldJointDef();
            def.bodyIdA = bodyA;
            def.bodyIdB = bodyB;
            def.localAnchorA = ToBox2D(joint->anchorA);
            def.localAnchorB = ToBox2D(joint->anchorB);
            def.collideConnected = joint->collideConnected;

            if (joint->stiffness > 0.0f) {
                def.linearHertz = joint->stiffness;
                def.angularHertz = joint->stiffness;
                def.linearDampingRatio = joint->damping;
                def.angularDampingRatio = joint->damping;
            }

            jointId = b2CreateWeldJoint(m_WorldId, &def);
            break;
        }
    }

    if (b2Joint_IsValid(jointId)) {
        m_EntityToJoint[entity] = jointId;
    }
}

void Box2DBackend::DestroyJointForEntity(ECS::Entity entity) {
    auto it = m_EntityToJoint.find(entity);
    if (it == m_EntityToJoint.end()) return;

    if (b2Joint_IsValid(it->second)) {
        b2DestroyJoint(it->second);
    }
    m_EntityToJoint.erase(it);
}

// ============================================================================
// Body velocity query (for rewind system)
// ============================================================================

bool Box2DBackend::GetBodyVelocity(ECS::Entity entity, Math::Vector3& outLinear) const {
    auto it = m_EntityToBody.find(entity);
    if (it == m_EntityToBody.end()) return false;

    b2BodyId bodyId = it->second;
    if (!b2Body_IsValid(bodyId)) return false;

    b2Vec2 lv = b2Body_GetLinearVelocity(bodyId);
    outLinear = Math::Vector3(lv.x, lv.y, 0.0f);
    return true;
}

// ============================================================================
// Force-set body state (for rewind system)
// ============================================================================

void Box2DBackend::ForceSetBodyState(ECS::Entity entity, const Math::Vector3& position,
                                      const Math::Vector3& velocity) {
    auto it = m_EntityToBody.find(entity);
    if (it == m_EntityToBody.end()) return;

    b2BodyId bodyId = it->second;
    if (!b2Body_IsValid(bodyId)) return;

    // Use SetLinearVelocity for kinematic bodies (per CLAUDE.md convention)
    b2Body_SetLinearVelocity(bodyId, b2Vec2{velocity.x, velocity.y});

    // For dynamic bodies, teleport via SetTransform is acceptable since we're restoring
    // complete state. For kinematics, the next sync pass will pick up the ECS position.
    b2BodyType bodyType = b2Body_GetType(bodyId);
    if (bodyType == b2_dynamicBody) {
        b2Body_SetTransform(bodyId, b2Vec2{position.x, position.y}, b2MakeRot(0.0f));
    }
}

} // namespace Physics
} // namespace Enjin

#endif // ENJIN_PHYSICS_BOX2D
