#ifdef ENJIN_PHYSICS_JOLT

// Jolt requires this before any Jolt headers
#include <Jolt/Jolt.h>

// Jolt headers
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Math/Math.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include "Enjin/Physics/JoltBackend.h"
#include "Enjin/Physics/JoltContactListener.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"

#include <algorithm>
#include <climits>
#include <thread>
#include <cmath>

// Jolt callback stubs
static void JoltTraceImpl(const char* fmt, ...) {
    // Route to engine log if desired; silent for now
}

#ifdef JPH_ENABLE_ASSERTS
static bool JoltAssertFailed(const char* expression, const char* message, const char* file, unsigned int line) {
    ENJIN_LOG_ERROR(Physics, "Jolt assert failed: %s (%s) at %s:%u", expression, message ? message : "", file, line);
    return true; // break into debugger
}
#endif

namespace Enjin {
namespace Physics {

// ============================================================================
// Broad Phase Layers
// ============================================================================

namespace BPLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr unsigned int NUM_LAYERS = 2;
}

// Maps ObjectLayer to BroadPhaseLayer.
// ObjectLayer 0 = static (NonMoving), everything else = Moving.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return BPLayers::NUM_LAYERS; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return layer == 0 ? BPLayers::NON_MOVING : BPLayers::MOVING;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        switch ((JPH::BroadPhaseLayer::Type)layer) {
            case (JPH::BroadPhaseLayer::Type)0: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)1: return "MOVING";
            default: return "UNKNOWN";
        }
    }
#endif
};

// Always allow object vs broad phase — fine-grained filtering in ObjectLayerPairFilter
class ObjectVsBPLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override {
        return true;
    }
};

// Simple ObjectLayerPairFilter — allows all layer combinations.
// Fine-grained bilateral collision filtering is done in JoltContactListener::OnContactValidate
// using per-body categoryBits/collisionMask from m_BodyFilterData.
class EnjinObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override {
        return true;
    }
};

// We need a custom body filter for raycasts that checks categoryBits against layerMask
class EnjinBodyFilter final : public JPH::BodyFilter {
public:
    const std::unordered_map<uint32_t, CollisionFilterData>* filterData = nullptr;
    u32 layerMask = 0xFFFFFFFF;
    JPH::BodyID ignoreBodyID;  // Body to exclude from results (e.g. self)

    bool ShouldCollide(const JPH::BodyID& bodyID) const override {
        // Skip the ignored body (prevents self-hit in ground checks)
        if (!ignoreBodyID.IsInvalid() && bodyID == ignoreBodyID) return false;

        if (!filterData || layerMask == 0xFFFFFFFF) return true;

        auto it = filterData->find(bodyID.GetIndex());
        if (it == filterData->end()) return true;

        return (it->second.categoryBits & layerMask) != 0;
    }

    bool ShouldCollideLocked(const JPH::Body&) const override { return true; }
};

// Persistent instances (must outlive PhysicsSystem)
static BPLayerInterfaceImpl s_BPLayerInterface;
static ObjectVsBPLayerFilterImpl s_ObjectVsBPFilter;
static EnjinObjectLayerPairFilter s_EnjinObjectFilter;

// ============================================================================
// Conversion Helpers
// ============================================================================

static inline JPH::Vec3 ToJolt(const Math::Vector3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

static inline JPH::RVec3 ToJoltR(const Math::Vector3& v) {
    return JPH::RVec3(v.x, v.y, v.z);
}

static inline JPH::Quat ToJolt(const Math::Quaternion& q) {
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

static inline Math::Vector3 FromJolt(const JPH::Vec3& v) {
    return Math::Vector3(v.GetX(), v.GetY(), v.GetZ());
}

static inline Math::Vector3 FromJoltR(const JPH::RVec3& v) {
    return Math::Vector3(static_cast<f32>(v.GetX()), static_cast<f32>(v.GetY()), static_cast<f32>(v.GetZ()));
}

static inline Math::Quaternion FromJolt(const JPH::Quat& q) {
    return Math::Quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
}

static constexpr f32 DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

// ============================================================================
// JoltBackend Implementation
// ============================================================================

JoltBackend::JoltBackend() {
    InitializeJolt();
}

JoltBackend::~JoltBackend() {
    ShutdownJolt();
}

void JoltBackend::InitializeJolt() {
    // Register Jolt allocation hook and types
    JPH::RegisterDefaultAllocator();

    JPH::Trace = JoltTraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = JoltAssertFailed;
#endif

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // 16 MB temp allocator for Jolt step
    m_TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);

    // Thread pool: use hardware threads minus 1 (min 1)
    // PH-C1: hardware_concurrency() can return 0; guard against unsigned wraparound
    unsigned int hwThreads = std::thread::hardware_concurrency();
    unsigned int numThreads = (hwThreads > 1) ? hwThreads - 1 : 1;
    m_JobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, static_cast<int>(numThreads));

    // Create the physics system
    const unsigned int maxBodies = 8192;
    const unsigned int numBodyMutexes = 0;  // auto
    const unsigned int maxBodyPairs = 8192;
    const unsigned int maxContactConstraints = 4096;

    m_PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
    m_PhysicsSystem->Init(
        maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
        s_BPLayerInterface, s_ObjectVsBPFilter, s_EnjinObjectFilter);

    // Contact listener (also handles bilateral collision filtering)
    m_ContactListener = std::make_unique<JoltContactListener>();
    m_ContactListener->filterData = &m_BodyFilterData;
    m_PhysicsSystem->SetContactListener(m_ContactListener.get());

    // Set gravity
    m_PhysicsSystem->SetGravity(ToJolt(m_Gravity));

    m_Initialized = true;
    ENJIN_LOG_INFO(Physics, "Jolt Physics initialized (threads: %u, max bodies: %u)", numThreads, maxBodies);
}

void JoltBackend::ShutdownJolt() {
    if (!m_Initialized) return;

    // Remove all bodies
    if (m_PhysicsSystem) {
        auto& bodyInterface = m_PhysicsSystem->GetBodyInterface();
        for (auto& [entity, bodyID] : m_EntityToBody) {
            bodyInterface.RemoveBody(bodyID);
            bodyInterface.DestroyBody(bodyID);
        }
    }

    m_EntityToBody.clear();
    m_BodyIndexToEntity.clear();
    m_BodyFilterData.clear();

    // Destroy all character controllers
    for (auto& [entity, character] : m_CharacterControllers) {
        delete character;
    }
    m_CharacterControllers.clear();
    m_EntityToConstraint.clear();
    m_EntityToConeConstraint.clear();

    m_ContactListener.reset();
    m_PhysicsSystem.reset();
    m_JobSystem.reset();
    m_TempAllocator.reset();

    // Cleanup Jolt singletons
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    m_Initialized = false;
}

void JoltBackend::SetWorld(ECS::World* world) {
    m_World = world;
}

void JoltBackend::SetGravity(const Math::Vector3& gravity) {
    m_Gravity = gravity;
    if (m_PhysicsSystem) {
        m_PhysicsSystem->SetGravity(ToJolt(gravity));
    }
}

Math::Vector3 JoltBackend::GetGravity() const {
    return m_Gravity;
}

// ============================================================================
// Update Loop
// ============================================================================

void JoltBackend::Update(f32 deltaTime) {
    if (!m_World || !m_Initialized || deltaTime <= 0.0f) return;

    // PH-H9 fix: cache deltaTime for MoveKinematic calls in SyncECSToJolt
    m_LastDeltaTime = deltaTime;

    // 1. Sync ECS state to Jolt bodies
    SyncECSToJolt();

    static int s_PhysLog = 0;
    if (s_PhysLog++ < 5) {
        printf("[PHYSICS] bodies=%d entities_with_colliders=%d\n",
            static_cast<int>(m_EntityToBody.size()), static_cast<int>(m_CurrentEntitiesCache.size()));
    }

    // 2. Sync joint components to Jolt constraints
    SyncJointsToJolt();

    // 3. Apply gravity zone overrides
    ApplyGravityZones();

    // 4. Step Jolt simulation (1 collision step)
    m_PhysicsSystem->Update(deltaTime, 1, m_TempAllocator.get(), m_JobSystem.get());

    // 5. Write Jolt state back to ECS
    SyncJoltToECS();

    // 6. Process contact events into CollisionEvent format
    ProcessContactEvents();
}

// ============================================================================
// ECS → Jolt Sync
// ============================================================================

void JoltBackend::SyncECSToJolt() {
    // Build set of entities that currently have colliders (reuse member to avoid per-frame alloc)
    m_CurrentEntitiesCache.clear();

    auto addEntities = [&](const auto& entities) {
        for (ECS::Entity e : entities) {
            m_CurrentEntitiesCache.insert(e);
        }
    };

    if (m_UseExternalColliders && !m_ExternalColliderEntities.empty()) {
        // WASM workaround: use collider entities provided by caller
        for (ECS::Entity e : m_ExternalColliderEntities) {
            m_CurrentEntitiesCache.insert(e);
        }
        static int s_ExtLog = 0;
        if (s_ExtLog++ < 3) printf("[PHYSICS-EXT] external=%zu cached=%zu\n", m_ExternalColliderEntities.size(), m_CurrentEntitiesCache.size());
    } else {
        addEntities(m_World->GetEntitiesWithComponent<ECS::BoxColliderComponent>());
        addEntities(m_World->GetEntitiesWithComponent<ECS::SphereColliderComponent>());
        addEntities(m_World->GetEntitiesWithComponent<ECS::CapsuleColliderComponent>());
        addEntities(m_World->GetEntitiesWithComponent<ECS::MeshColliderComponent>());
    }

    auto& bodyInterface = m_PhysicsSystem->GetBodyInterface();

    // Create bodies for new entities
    for (ECS::Entity entity : m_CurrentEntitiesCache) {
        if (m_EntityToBody.find(entity) == m_EntityToBody.end()) {
            CreateBodyForEntity(entity);
        }
    }

    // Destroy bodies for removed entities (reuse member to avoid per-frame alloc)
    m_ToRemoveCache.clear();
    for (auto& [entity, bodyID] : m_EntityToBody) {
        if (m_CurrentEntitiesCache.find(entity) == m_CurrentEntitiesCache.end()) {
            m_ToRemoveCache.push_back(entity);
        }
    }
    for (ECS::Entity entity : m_ToRemoveCache) {
        DestroyBodyForEntity(entity);
    }

    // Detect body type changes (e.g. rigidbody added/changed after collider was created)
    // and recreate the body with the correct motion type.
    m_ToRemoveCache.clear();
    for (auto& [entity, bodyID] : m_EntityToBody) {
        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);
        JPH::EMotionType currentMotion = bodyInterface.GetMotionType(bodyID);
        JPH::EMotionType desiredMotion = JPH::EMotionType::Static;
        if (rb) {
            if (rb->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                desiredMotion = JPH::EMotionType::Dynamic;
            else if (rb->bodyType == ECS::RigidbodyComponent::BodyType::Kinematic)
                desiredMotion = JPH::EMotionType::Kinematic;
        }
        if (currentMotion != desiredMotion) {
            m_ToRemoveCache.push_back(entity);
        }
    }
    for (ECS::Entity entity : m_ToRemoveCache) {
        DestroyBodyForEntity(entity);
        CreateBodyForEntity(entity);
    }

    // Update kinematic bodies and property changes
    for (auto& [entity, bodyID] : m_EntityToBody) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);
        if (!transform) continue;

        if (rb && rb->bodyType == ECS::RigidbodyComponent::BodyType::Kinematic) {
            // Kinematic bodies: drive position from ECS
            bodyInterface.MoveKinematic(bodyID, ToJoltR(transform->position), ToJolt(transform->rotation), m_LastDeltaTime);
        } else if (rb && rb->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) {
            // Dynamic bodies: sync gravity and velocity from ECS when externally modified
            // (e.g. FlowerSystem applies pull force via rb->velocity)
            bodyInterface.SetGravityFactor(bodyID, rb->useGravity ? std::clamp(rb->gravityScale, -10.0f, 10.0f) : 0.0f);
            // Push ECS velocity to Jolt so external force application works
            JPH::Vec3 joltVel = bodyInterface.GetLinearVelocity(bodyID);
            JPH::Vec3 ecsVel = ToJolt(rb->velocity);
            // Only override if ECS velocity meaningfully differs (external force was applied)
            f32 diff = (ecsVel - joltVel).Length();
            if (diff > 0.01f) {
                bodyInterface.SetLinearVelocity(bodyID, ecsVel);
            }
        } else if (!rb || rb->bodyType == ECS::RigidbodyComponent::BodyType::Static) {
            // Static bodies: just set position if it changed
            bodyInterface.SetPosition(bodyID, ToJoltR(transform->position), JPH::EActivation::DontActivate);
            bodyInterface.SetRotation(bodyID, ToJolt(transform->rotation), JPH::EActivation::DontActivate);
        }

        // Update collision filter data
        ColliderInfo info = GetColliderInfo(entity);
        m_BodyFilterData[bodyID.GetIndex()] = {info.categoryBits, info.collisionMask};
    }

    m_CurrentEntitiesCache.clear();  // Release references after use
}

void JoltBackend::CreateBodyForEntity(ECS::Entity entity) {
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
    if (!transform) return;

    // Guard against duplicate body creation — destroy existing body first
    if (m_EntityToBody.find(entity) != m_EntityToBody.end()) {
        ENJIN_LOG_WARN(Physics, "CreateBodyForEntity called for entity %llu that already has a body — replacing",
                       static_cast<unsigned long long>(entity));
        DestroyBodyForEntity(entity);
    }

    auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);

    // Determine shape
    JPH::ShapeRefC shape;
    ColliderInfo colliderInfo;
    Math::Vector3 colliderCenter(0, 0, 0);
    f32 friction = 0.5f;
    f32 bounciness = 0.0f;

    if (auto* box = m_World->GetComponent<ECS::BoxColliderComponent>(entity)) {
        // Collider size is in world space — not multiplied by transform scale.
        // Templates set size to match the desired world dimensions directly.
        JPH::Vec3 halfExtents(
            box->size.x * 0.5f,
            box->size.y * 0.5f,
            box->size.z * 0.5f
        );
        // Clamp to minimum valid size
        halfExtents = JPH::Vec3(
            std::max(halfExtents.GetX(), 0.01f),
            std::max(halfExtents.GetY(), 0.01f),
            std::max(halfExtents.GetZ(), 0.01f)
        );
        shape = new JPH::BoxShape(halfExtents);
        colliderCenter = box->center;
        friction = box->friction;
        bounciness = box->bounciness;
        colliderInfo = {box->categoryBits, box->collisionMask, box->isTrigger, true};
    } else if (auto* sphere = m_World->GetComponent<ECS::SphereColliderComponent>(entity)) {
        // Sphere radius is in world space — not scaled by transform
        f32 worldRadius = std::max(sphere->radius, 0.01f);
        shape = new JPH::SphereShape(worldRadius);
        colliderCenter = sphere->center;
        friction = sphere->friction;
        bounciness = sphere->bounciness;
        colliderInfo = {sphere->categoryBits, sphere->collisionMask, sphere->isTrigger, true};
    } else if (auto* capsule = m_World->GetComponent<ECS::CapsuleColliderComponent>(entity)) {
        // Capsule dimensions are in world space — not scaled by transform
        f32 worldRadius = std::max(capsule->radius, 0.01f);
        f32 halfHeight = std::max(capsule->height * 0.5f - capsule->radius, 0.01f);

        // Jolt CapsuleShape is aligned along Y by default
        JPH::ShapeRefC capsuleShape = new JPH::CapsuleShape(halfHeight, worldRadius);

        // Rotate for X or Z direction
        if (capsule->direction == ECS::CapsuleColliderComponent::Direction::X) {
            shape = new JPH::RotatedTranslatedShape(
                JPH::Vec3::sZero(), JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), JPH::JPH_PI * 0.5f), capsuleShape);
        } else if (capsule->direction == ECS::CapsuleColliderComponent::Direction::Z) {
            shape = new JPH::RotatedTranslatedShape(
                JPH::Vec3::sZero(), JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::JPH_PI * 0.5f), capsuleShape);
        } else {
            shape = capsuleShape;
        }

        colliderCenter = capsule->center;
        friction = capsule->friction;
        bounciness = capsule->bounciness;
        colliderInfo = {capsule->categoryBits, capsule->collisionMask, capsule->isTrigger, true};
    } else if (auto* meshCol = m_World->GetComponent<ECS::MeshColliderComponent>(entity)) {
        // Auto-generate collision geometry from MeshComponent if needed
        if (meshCol->autoGenerate && !meshCol->generated) {
            auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
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
            if (meshCol->convex) {
                // Build convex hull — Jolt computes the hull from the point cloud
                JPH::Array<JPH::Vec3> points;
                points.reserve(meshCol->vertices.size());
                for (const auto& v : meshCol->vertices) {
                    points.push_back(JPH::Vec3(v.x, v.y, v.z));
                }
                JPH::ConvexHullShapeSettings hullSettings(points.data(), static_cast<int>(points.size()));
                hullSettings.mMaxConvexRadius = 0.05f;
                auto result = hullSettings.Create();
                if (result.IsValid()) {
                    shape = result.Get();
                } else {
                    ENJIN_LOG_WARN(Physics, "MeshCollider convex hull creation failed for entity %llu",
                                   static_cast<unsigned long long>(entity));
                }
            } else {
                // Triangle mesh — static bodies only
                if (!rb || rb->bodyType == ECS::RigidbodyComponent::BodyType::Static) {
                    JPH::TriangleList triangles;
                    if (!meshCol->indices.empty() && meshCol->indices.size() % 3 == 0) {
                        triangles.reserve(meshCol->indices.size() / 3);
                        for (size_t i = 0; i + 2 < meshCol->indices.size(); i += 3) {
                            const auto& v0 = meshCol->vertices[meshCol->indices[i]];
                            const auto& v1 = meshCol->vertices[meshCol->indices[i + 1]];
                            const auto& v2 = meshCol->vertices[meshCol->indices[i + 2]];
                            triangles.push_back(JPH::Triangle(
                                JPH::Float3(v0.x, v0.y, v0.z),
                                JPH::Float3(v1.x, v1.y, v1.z),
                                JPH::Float3(v2.x, v2.y, v2.z)));
                        }
                    }
                    if (!triangles.empty()) {
                        JPH::MeshShapeSettings meshSettings(triangles);
                        auto result = meshSettings.Create();
                        if (result.IsValid()) {
                            shape = result.Get();
                        } else {
                            ENJIN_LOG_WARN(Physics, "MeshCollider triangle mesh creation failed for entity %llu",
                                           static_cast<unsigned long long>(entity));
                        }
                    }
                } else {
                    ENJIN_LOG_WARN(Physics, "MeshCollider triangle mesh mode requires static body (entity %llu)",
                                   static_cast<unsigned long long>(entity));
                }
            }

            friction = meshCol->friction;
            bounciness = meshCol->bounciness;
            colliderInfo = {meshCol->categoryBits, meshCol->collisionMask, meshCol->isTrigger, true};
        }
    }

    if (!shape) return;

    // Apply center offset if non-zero
    if (colliderCenter.x != 0.0f || colliderCenter.y != 0.0f || colliderCenter.z != 0.0f) {
        shape = new JPH::RotatedTranslatedShape(ToJolt(colliderCenter), JPH::Quat::sIdentity(), shape);
    }

    // Determine motion type
    JPH::EMotionType motionType = JPH::EMotionType::Static;
    JPH::ObjectLayer objectLayer = 0;  // static layer

    if (rb) {
        switch (rb->bodyType) {
            case ECS::RigidbodyComponent::BodyType::Dynamic:
                motionType = JPH::EMotionType::Dynamic;
                objectLayer = 1;
                break;
            case ECS::RigidbodyComponent::BodyType::Kinematic:
                motionType = JPH::EMotionType::Kinematic;
                objectLayer = 1;
                break;
            case ECS::RigidbodyComponent::BodyType::Static:
                motionType = JPH::EMotionType::Static;
                objectLayer = 0;
                break;
        }
    }

    // Build body creation settings
    JPH::BodyCreationSettings bodySettings(shape, ToJoltR(transform->position),
        ToJolt(transform->rotation), motionType, objectLayer);

    // P5 fix: Clamp material properties to valid ranges
    bodySettings.mFriction = std::clamp(friction, 0.0f, 1.0f);
    bodySettings.mRestitution = std::clamp(bounciness, 0.0f, 1.0f);
    bodySettings.mIsSensor = colliderInfo.isTrigger;
    bodySettings.mUserData = static_cast<uint64_t>(entity);

    if (rb) {
        bodySettings.mLinearDamping = rb->drag;
        bodySettings.mAngularDamping = rb->angularDrag;
        // P9 fix: Clamp gravity scale to prevent physics instability
        f32 clampedGravityScale = std::clamp(rb->gravityScale, -10.0f, 10.0f);
        bodySettings.mGravityFactor = rb->useGravity ? clampedGravityScale : 0.0f;

        // Mass override
        if (rb->mass > 0.0f && motionType == JPH::EMotionType::Dynamic) {
            bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            bodySettings.mMassPropertiesOverride.mMass = rb->mass;
        }

        // Allowed DOFs
        JPH::EAllowedDOFs dofs = JPH::EAllowedDOFs::All;
        if (rb->freezePositionX) dofs &= ~JPH::EAllowedDOFs::TranslationX;
        if (rb->freezePositionY) dofs &= ~JPH::EAllowedDOFs::TranslationY;
        if (rb->freezePositionZ) dofs &= ~JPH::EAllowedDOFs::TranslationZ;
        if (rb->freezeRotationX) dofs &= ~JPH::EAllowedDOFs::RotationX;
        if (rb->freezeRotationY) dofs &= ~JPH::EAllowedDOFs::RotationY;
        if (rb->freezeRotationZ) dofs &= ~JPH::EAllowedDOFs::RotationZ;
        bodySettings.mAllowedDOFs = dofs;

        // CCD
        if (rb->collisionMode == ECS::RigidbodyComponent::CollisionMode::Continuous ||
            rb->collisionMode == ECS::RigidbodyComponent::CollisionMode::ContinuousSpeculative) {
            bodySettings.mMotionQuality = JPH::EMotionQuality::LinearCast;
        }

        // Set initial velocity
        bodySettings.mLinearVelocity = ToJolt(rb->velocity);
        bodySettings.mAngularVelocity = ToJolt(rb->angularVelocity);
    }

    // Create and add the body
    auto& bodyInterface = m_PhysicsSystem->GetBodyInterface();
    JPH::BodyID bodyID = bodyInterface.CreateAndAddBody(bodySettings, JPH::EActivation::Activate);

    if (bodyID.IsInvalid()) {
        ENJIN_LOG_WARN(Physics, "Failed to create Jolt body for entity %llu", static_cast<unsigned long long>(entity));
        return;
    }

    // Use BodyID index as the key for ObjectLayer-based collision filtering
    uint32_t bodyIndex = bodyID.GetIndex();

    // Store the ObjectLayer on the body to match our filter lookup
    // We set objectLayer based on body index so the filter can look up filter data
    // Actually, Jolt's ObjectLayer is set at creation time and we use it for broad phase only.
    // The fine-grained filter uses BodyID index to look up our filter data map.

    m_EntityToBody[entity] = bodyID;
    m_BodyIndexToEntity[bodyIndex] = entity;
    m_BodyFilterData[bodyIndex] = {colliderInfo.categoryBits, colliderInfo.collisionMask};
}

void JoltBackend::DestroyBodyForEntity(ECS::Entity entity) {
    auto it = m_EntityToBody.find(entity);
    if (it == m_EntityToBody.end()) return;

    JPH::BodyID bodyID = it->second;
    uint32_t bodyIndex = bodyID.GetIndex();

    // Remove any joint constraints first
    DestroyJointForEntity(entity);

    auto& bodyInterface = m_PhysicsSystem->GetBodyInterface();
    bodyInterface.RemoveBody(bodyID);
    bodyInterface.DestroyBody(bodyID);

    m_BodyIndexToEntity.erase(bodyIndex);
    m_BodyFilterData.erase(bodyIndex);
    m_EntityToBody.erase(it);
}

ColliderInfo JoltBackend::GetColliderInfo(ECS::Entity entity) {
    ColliderInfo info;
    if (auto* box = m_World->GetComponent<ECS::BoxColliderComponent>(entity)) {
        info.categoryBits = box->categoryBits;
        info.collisionMask = box->collisionMask;
        info.isTrigger = box->isTrigger;
        info.hasCollider = true;
    } else if (auto* sphere = m_World->GetComponent<ECS::SphereColliderComponent>(entity)) {
        info.categoryBits = sphere->categoryBits;
        info.collisionMask = sphere->collisionMask;
        info.isTrigger = sphere->isTrigger;
        info.hasCollider = true;
    } else if (auto* capsule = m_World->GetComponent<ECS::CapsuleColliderComponent>(entity)) {
        info.categoryBits = capsule->categoryBits;
        info.collisionMask = capsule->collisionMask;
        info.isTrigger = capsule->isTrigger;
        info.hasCollider = true;
    } else if (auto* meshCol = m_World->GetComponent<ECS::MeshColliderComponent>(entity)) {
        info.categoryBits = meshCol->categoryBits;
        info.collisionMask = meshCol->collisionMask;
        info.isTrigger = meshCol->isTrigger;
        info.hasCollider = true;
    }
    return info;
}

// ============================================================================
// Jolt → ECS Sync
// ============================================================================

void JoltBackend::SyncJoltToECS() {
    auto& bodyInterface = m_PhysicsSystem->GetBodyInterface();

    for (auto& [entity, bodyID] : m_EntityToBody) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);
        if (!transform) continue;

        // Only sync dynamic bodies back to ECS
        if (rb && rb->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) {
            // Position and rotation
            transform->position = FromJoltR(bodyInterface.GetPosition(bodyID));
            transform->rotation = FromJolt(bodyInterface.GetRotation(bodyID));

            // Velocity
            rb->velocity = FromJolt(bodyInterface.GetLinearVelocity(bodyID));
            rb->angularVelocity = FromJolt(bodyInterface.GetAngularVelocity(bodyID));

            // Sleep state
            rb->isSleeping = !bodyInterface.IsActive(bodyID);

            // Ground check via downward raycast
            rb->isGrounded = false;
            JPH::RRayCast groundRay(
                ToJoltR(transform->position),
                JPH::Vec3(0, -1, 0)  // direction not normalized needed, but will set max distance via collector
            );

            // Use a short raycast downward for ground check
            f32 checkDist = 0.15f;
            JPH::RayCastResult groundResult;
            EnjinBodyFilter bodyFilter;
            bodyFilter.filterData = &m_BodyFilterData;
            bodyFilter.layerMask = 0xFFFFFFFF;

            // Cast a short ray downward from entity bottom
            // Approximate entity half-height
            f32 halfHeight = 0.5f;
            if (auto* box = m_World->GetComponent<ECS::BoxColliderComponent>(entity)) {
                halfHeight = box->size.y * transform->scale.y * 0.5f;
            } else if (auto* capsule = m_World->GetComponent<ECS::CapsuleColliderComponent>(entity)) {
                halfHeight = capsule->height * transform->scale.y * 0.5f;
            } else if (auto* meshCol = m_World->GetComponent<ECS::MeshColliderComponent>(entity)) {
                // Approximate half-height from cached mesh vertices AABB
                if (meshCol->generated && !meshCol->vertices.empty()) {
                    f32 minY = meshCol->vertices[0].y, maxY = meshCol->vertices[0].y;
                    for (const auto& v : meshCol->vertices) {
                        if (v.y < minY) minY = v.y;
                        if (v.y > maxY) maxY = v.y;
                    }
                    halfHeight = (maxY - minY) * transform->scale.y * 0.5f;
                }
            }

            JPH::RRayCast shortRay(
                ToJoltR(transform->position + Math::Vector3(0, -halfHeight + 0.02f, 0)),
                JPH::Vec3(0, -(checkDist + 0.02f), 0)
            );

            JPH::BroadPhaseLayerFilter groundBPFilter;
            JPH::ObjectLayerFilter groundObjFilter;
            if (m_PhysicsSystem->GetNarrowPhaseQuery().CastRay(shortRay, groundResult, groundBPFilter, groundObjFilter, bodyFilter)) {
                // Make sure we didn't hit ourselves
                JPH::BodyID hitBody = groundResult.mBodyID;
                if (hitBody != bodyID) {
                    rb->isGrounded = true;
                }
            }
        }
    }
}

// ============================================================================
// Gravity Zones
// ============================================================================

void JoltBackend::ApplyGravityZones() {
    auto gravityZoneEntities = m_World->GetEntitiesWithComponent<ECS::GravityZoneComponent>();
    if (gravityZoneEntities.empty()) return;

    auto& bodyInterface = m_PhysicsSystem->GetBodyInterface();

    for (auto& [entity, bodyID] : m_EntityToBody) {
        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);
        if (!rb || rb->bodyType != ECS::RigidbodyComponent::BodyType::Dynamic) continue;
        if (!rb->useGravity) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // Find highest-priority gravity zone containing this entity
        i32 bestPriority = INT_MIN;
        Math::Vector3 customGravity = m_Gravity;
        bool inZone = false;

        for (ECS::Entity zoneEntity : gravityZoneEntities) {
            auto* zone = m_World->GetComponent<ECS::GravityZoneComponent>(zoneEntity);
            auto* zoneTransform = m_World->GetComponent<ECS::TransformComponent>(zoneEntity);
            if (!zone || !zoneTransform || !zone->isActive) continue;
            if (zone->priority <= bestPriority) continue;

            if (zone->ContainsPoint(zoneTransform->position, transform->position)) {
                customGravity = zone->GetGravityAt(zoneTransform->position, transform->position);
                bestPriority = zone->priority;
                inZone = true;
            }
        }

        if (inZone) {
            // Disable Jolt's built-in gravity for this body
            bodyInterface.SetGravityFactor(bodyID, 0.0f);
            // Apply custom gravity as a force (guard against zero/negative mass)
            if (rb->mass > 0.0001f) {
                Math::Vector3 force = customGravity * rb->gravityScale * rb->mass;
                bodyInterface.AddForce(bodyID, ToJolt(force));
            }
        } else {
            // Restore normal gravity factor (P9 fix: clamp)
            bodyInterface.SetGravityFactor(bodyID, std::clamp(rb->gravityScale, -10.0f, 10.0f));
        }
    }
}

// ============================================================================
// Joint Synchronization
// ============================================================================

void JoltBackend::SyncJointsToJolt() {
    // Track which joint entities currently exist (reuse member to avoid per-frame alloc)
    m_CurrentJointEntitiesCache.clear();

    auto processJoints = [&](const auto& entities, u8 jointType) {
        for (ECS::Entity e : entities) {
            m_CurrentJointEntitiesCache.insert(e);
            if (m_EntityToConstraint.find(e) == m_EntityToConstraint.end()) {
                CreateJointForEntity(e, jointType);
            }
        }
    };

    processJoints(m_World->GetEntitiesWithComponent<ECS::DistanceJointComponent>(), 0);
    processJoints(m_World->GetEntitiesWithComponent<ECS::HingeJointComponent>(), 1);
    processJoints(m_World->GetEntitiesWithComponent<ECS::BallSocketJointComponent>(), 2);
    processJoints(m_World->GetEntitiesWithComponent<ECS::SpringJointComponent>(), 3);
    processJoints(m_World->GetEntitiesWithComponent<ECS::FixedJointComponent>(), 4);
    processJoints(m_World->GetEntitiesWithComponent<ECS::SliderJointComponent>(), 5);

    // Remove constraints for deleted joint entities (reuse member to avoid per-frame alloc)
    m_JointToRemoveCache.clear();
    for (auto& [entity, constraint] : m_EntityToConstraint) {
        if (m_CurrentJointEntitiesCache.find(entity) == m_CurrentJointEntitiesCache.end()) {
            m_JointToRemoveCache.push_back(entity);
        }
    }
    for (ECS::Entity e : m_JointToRemoveCache) {
        DestroyJointForEntity(e);
    }

    // Compute currentStress for spring joints from entity positions (Hooke's law)
    // Jolt manages constraint forces internally but doesn't expose them back to ECS,
    // so we derive stress from displacement * springConstant + damping.
    for (auto& [entity, constraint] : m_EntityToConstraint) {
        if (auto* sj = m_World->GetComponent<ECS::SpringJointComponent>(entity)) {
            if (!sj->breakable) continue;
            auto* tA = m_World->GetComponent<ECS::TransformComponent>(sj->entityA);
            auto* tB = m_World->GetComponent<ECS::TransformComponent>(sj->entityB);
            if (tA && tB) {
                Math::Vector3 wA = tA->position + sj->anchorA;
                Math::Vector3 wB = tB->position + sj->anchorB;
                f32 dist = (wB - wA).Length();
                f32 displacement = dist - sj->restLength;
                f32 springForce = sj->springConstant * displacement;
                // Add damping contribution from relative velocity
                auto* rbA = m_World->GetComponent<ECS::RigidbodyComponent>(sj->entityA);
                auto* rbB = m_World->GetComponent<ECS::RigidbodyComponent>(sj->entityB);
                if (dist > 1e-6f) {
                    Math::Vector3 dir = (wB - wA) * (1.0f / dist);
                    Math::Vector3 velA = rbA ? rbA->velocity : Math::Vector3(0, 0, 0);
                    Math::Vector3 velB = rbB ? rbB->velocity : Math::Vector3(0, 0, 0);
                    f32 relVel = (velB - velA).Dot(dir);
                    springForce += sj->dampingCoefficient * relVel;
                }
                sj->currentStress = std::abs(springForce);
            }
        }
    }

    // Check breakable joints
    for (auto it = m_EntityToConstraint.begin(); it != m_EntityToConstraint.end(); ) {
        ECS::Entity entity = it->first;
        // Check each joint type for break force
        bool shouldBreak = false;

        if (auto* dj = m_World->GetComponent<ECS::DistanceJointComponent>(entity)) {
            if (dj->breakable && dj->currentStress > dj->breakForce) shouldBreak = true;
        } else if (auto* hj = m_World->GetComponent<ECS::HingeJointComponent>(entity)) {
            if (hj->breakable && hj->currentStress > hj->breakForce) shouldBreak = true;
        } else if (auto* bsj = m_World->GetComponent<ECS::BallSocketJointComponent>(entity)) {
            if (bsj->breakable && bsj->currentStress > bsj->breakForce) shouldBreak = true;
        } else if (auto* sj = m_World->GetComponent<ECS::SpringJointComponent>(entity)) {
            if (sj->breakable && sj->currentStress > sj->breakForce) shouldBreak = true;
        } else if (auto* fj = m_World->GetComponent<ECS::FixedJointComponent>(entity)) {
            if (fj->breakable && fj->currentStress > fj->breakForce) shouldBreak = true;
        } else if (auto* slj = m_World->GetComponent<ECS::SliderJointComponent>(entity)) {
            if (slj->breakable && slj->currentStress > slj->breakForce) shouldBreak = true;
        }

        if (shouldBreak) {
            ECS::Entity e = entity;
            ++it;
            DestroyJointForEntity(e);
        } else {
            ++it;
        }
    }
}

void JoltBackend::CreateJointForEntity(ECS::Entity entity, u8 jointType) {
    auto& bodyInterface = m_PhysicsSystem->GetBodyInterface();

    auto getBodyID = [&](ECS::Entity e) -> JPH::BodyID {
        auto it = m_EntityToBody.find(e);
        if (it != m_EntityToBody.end()) return it->second;
        return JPH::BodyID();
    };

    // Helper: lock two bodies, create a constraint, add it
    auto createConstraint = [&](JPH::BodyID idA, JPH::BodyID idB,
                                auto& settings, auto postCreate) -> JPH::Constraint* {
        JPH::BodyLockWrite lockA(m_PhysicsSystem->GetBodyLockInterface(), idA);
        JPH::BodyLockWrite lockB(m_PhysicsSystem->GetBodyLockInterface(), idB);
        if (!lockA.Succeeded() || !lockB.Succeeded()) return nullptr;

        auto* c = settings.Create(lockA.GetBody(), lockB.GetBody());
        if (!c) return nullptr;
        postCreate(c, lockA.GetBody(), lockB.GetBody());
        m_PhysicsSystem->AddConstraint(c);
        return c;
    };

    auto noop = [](auto*, auto&, auto&) {};

    JPH::Constraint* constraint = nullptr;

    switch (jointType) {
    case 0: { // Distance
        auto* joint = m_World->GetComponent<ECS::DistanceJointComponent>(entity);
        if (!joint || joint->entityA == 0 || joint->entityB == 0) return;
        JPH::BodyID bodyA = getBodyID(joint->entityA);
        JPH::BodyID bodyB = getBodyID(joint->entityB);
        if (bodyA.IsInvalid() || bodyB.IsInvalid()) return;

        JPH::DistanceConstraintSettings settings;
        settings.mPoint1 = ToJoltR(FromJoltR(bodyInterface.GetPosition(bodyA)) + joint->anchorA);
        settings.mPoint2 = ToJoltR(FromJoltR(bodyInterface.GetPosition(bodyB)) + joint->anchorB);
        settings.mMinDistance = joint->restDistance - joint->tolerance;
        settings.mMaxDistance = joint->restDistance + joint->tolerance;
        if (joint->stiffness < 1.0f && joint->stiffness > 0.0f) {
            settings.mLimitsSpringSettings.mFrequency = joint->stiffness * 10.0f;
            settings.mLimitsSpringSettings.mDamping = 0.5f;
        }

        constraint = createConstraint(bodyA, bodyB, settings, noop);
        break;
    }
    case 1: { // Hinge
        auto* joint = m_World->GetComponent<ECS::HingeJointComponent>(entity);
        if (!joint || joint->entityA == 0 || joint->entityB == 0) return;
        JPH::BodyID bodyA = getBodyID(joint->entityA);
        JPH::BodyID bodyB = getBodyID(joint->entityB);
        if (bodyA.IsInvalid() || bodyB.IsInvalid()) return;

        JPH::HingeConstraintSettings settings;
        settings.mPoint1 = ToJoltR(FromJoltR(bodyInterface.GetPosition(bodyA)) + joint->anchorA);
        settings.mPoint2 = ToJoltR(FromJoltR(bodyInterface.GetPosition(bodyB)) + joint->anchorB);
        JPH::Vec3 rawHingeAxis = ToJolt(joint->axis);
        if (rawHingeAxis.LengthSq() < 1e-12f) rawHingeAxis = JPH::Vec3::sAxisY();
        settings.mHingeAxis1 = rawHingeAxis.Normalized();
        settings.mHingeAxis2 = settings.mHingeAxis1;
        // Compute a perpendicular normal axis
        JPH::Vec3 up = JPH::Vec3::sAxisY();
        if (std::abs(settings.mHingeAxis1.Dot(up)) > 0.9f) up = JPH::Vec3::sAxisX();
        settings.mNormalAxis1 = settings.mHingeAxis1.Cross(up).Normalized();
        settings.mNormalAxis2 = settings.mNormalAxis1;

        if (joint->useLimits) {
            f32 lo = std::min(joint->lowerLimit, joint->upperLimit) * DEG_TO_RAD;
            f32 hi = std::max(joint->lowerLimit, joint->upperLimit) * DEG_TO_RAD;
            settings.mLimitsMin = lo;
            settings.mLimitsMax = hi;
        }

        bool useMotor = joint->useMotor;
        f32 motorSpeed = joint->motorSpeed;
        constraint = createConstraint(bodyA, bodyB, settings,
            [useMotor, motorSpeed](JPH::Constraint* c, auto&, auto&) {
                if (useMotor) {
                    auto* hc = static_cast<JPH::HingeConstraint*>(c);
                    hc->SetMotorState(JPH::EMotorState::Velocity);
                    hc->SetTargetAngularVelocity(motorSpeed * (3.14159265358979323846f / 180.0f));
                }
            });
        break;
    }
    case 2: { // BallSocket
        auto* joint = m_World->GetComponent<ECS::BallSocketJointComponent>(entity);
        if (!joint || joint->entityA == 0 || joint->entityB == 0) return;
        JPH::BodyID bodyA = getBodyID(joint->entityA);
        JPH::BodyID bodyB = getBodyID(joint->entityB);
        if (bodyA.IsInvalid() || bodyB.IsInvalid()) return;

        JPH::PointConstraintSettings settings;
        settings.mPoint1 = ToJoltR(FromJoltR(bodyInterface.GetPosition(bodyA)) + joint->anchorA);
        settings.mPoint2 = ToJoltR(FromJoltR(bodyInterface.GetPosition(bodyB)) + joint->anchorB);

        bool useCone = joint->useConeLimit;
        f32 coneAngle = joint->coneAngleLimit;
        ECS::Entity jointEntity = entity;
        constraint = createConstraint(bodyA, bodyB, settings,
            [this, useCone, coneAngle, jointEntity](JPH::Constraint*, auto& bA, auto& bB) {
                if (useCone) {
                    JPH::ConeConstraintSettings coneSettings;
                    coneSettings.mHalfConeAngle = coneAngle * (3.14159265358979323846f / 180.0f);
                    auto* cc = coneSettings.Create(bA, bB);
                    if (cc) {
                        m_PhysicsSystem->AddConstraint(cc);
                        m_EntityToConeConstraint[jointEntity] = cc;
                    }
                }
            });
        break;
    }
    case 3: { // Spring (implemented as DistanceConstraint with spring settings)
        auto* joint = m_World->GetComponent<ECS::SpringJointComponent>(entity);
        if (!joint || joint->entityA == 0 || joint->entityB == 0) return;
        JPH::BodyID bodyA = getBodyID(joint->entityA);
        JPH::BodyID bodyB = getBodyID(joint->entityB);
        if (bodyA.IsInvalid() || bodyB.IsInvalid()) return;

        JPH::DistanceConstraintSettings settings;
        settings.mPoint1 = ToJoltR(FromJoltR(bodyInterface.GetPosition(bodyA)) + joint->anchorA);
        settings.mPoint2 = ToJoltR(FromJoltR(bodyInterface.GetPosition(bodyB)) + joint->anchorB);
        settings.mMinDistance = joint->minDistance > 0 ? joint->minDistance : 0.0f;
        settings.mMaxDistance = joint->maxDistance > 0 ? joint->maxDistance : 1000.0f;
        settings.mLimitsSpringSettings.mStiffness = std::max(joint->springConstant, 0.0f);
        settings.mLimitsSpringSettings.mDamping = std::max(joint->dampingCoefficient, 0.0f);

        constraint = createConstraint(bodyA, bodyB, settings, noop);
        break;
    }
    case 4: { // Fixed
        auto* joint = m_World->GetComponent<ECS::FixedJointComponent>(entity);
        if (!joint || joint->entityA == 0 || joint->entityB == 0) return;
        JPH::BodyID bodyA = getBodyID(joint->entityA);
        JPH::BodyID bodyB = getBodyID(joint->entityB);
        if (bodyA.IsInvalid() || bodyB.IsInvalid()) return;

        JPH::FixedConstraintSettings settings;
        settings.mAutoDetectPoint = true;

        constraint = createConstraint(bodyA, bodyB, settings, noop);
        break;
    }
    case 5: { // Slider
        auto* joint = m_World->GetComponent<ECS::SliderJointComponent>(entity);
        if (!joint || joint->entityA == 0 || joint->entityB == 0) return;
        JPH::BodyID bodyA = getBodyID(joint->entityA);
        JPH::BodyID bodyB = getBodyID(joint->entityB);
        if (bodyA.IsInvalid() || bodyB.IsInvalid()) return;

        JPH::SliderConstraintSettings settings;
        settings.mAutoDetectPoint = true;
        JPH::Vec3 rawSliderAxis = ToJolt(joint->slideAxis);
        if (rawSliderAxis.LengthSq() < 1e-12f) rawSliderAxis = JPH::Vec3::sAxisX();
        settings.mSliderAxis1 = rawSliderAxis.Normalized();
        settings.mSliderAxis2 = settings.mSliderAxis1;

        if (joint->useLimits) {
            settings.mLimitsMin = joint->lowerLimit;
            settings.mLimitsMax = joint->upperLimit;
        }

        bool useMotor = joint->useMotor;
        f32 motorSpeed = joint->motorSpeed;
        constraint = createConstraint(bodyA, bodyB, settings,
            [useMotor, motorSpeed](JPH::Constraint* c, auto&, auto&) {
                if (useMotor) {
                    auto* sc = static_cast<JPH::SliderConstraint*>(c);
                    sc->SetMotorState(JPH::EMotorState::Velocity);
                    sc->SetTargetVelocity(motorSpeed);
                }
            });
        break;
    }
    }

    if (constraint) {
        m_EntityToConstraint[entity] = constraint;
    }
}

void JoltBackend::DestroyJointForEntity(ECS::Entity entity) {
    auto it = m_EntityToConstraint.find(entity);
    if (it == m_EntityToConstraint.end()) return;

    // PH-C3 fix: validate constraint pointer before removal to prevent
    // dangling dereference if constraint was already removed externally.
    if (it->second != nullptr) {
        m_PhysicsSystem->RemoveConstraint(it->second);
    }
    m_EntityToConstraint.erase(it);

    // PH-H8 fix: also remove any secondary cone constraint for ball-socket joints
    auto coneIt = m_EntityToConeConstraint.find(entity);
    if (coneIt != m_EntityToConeConstraint.end()) {
        if (coneIt->second != nullptr) {
            m_PhysicsSystem->RemoveConstraint(coneIt->second);
        }
        m_EntityToConeConstraint.erase(coneIt);
    }
}

// ============================================================================
// Contact Event Processing
// ============================================================================

void JoltBackend::ProcessContactEvents() {
    // Drain buffered events from the listener
    std::vector<JoltContactEvent> rawEvents;
    m_ContactListener->DrainEvents(rawEvents);

    m_CurrentCollisionPairs.clear();

    // Process raw Jolt events into current collision pairs
    for (const auto& evt : rawEvents) {
        // Resolve entities from body IDs
        auto itA = m_BodyIndexToEntity.find(evt.bodyA.GetIndex());
        auto itB = m_BodyIndexToEntity.find(evt.bodyB.GetIndex());
        if (itA == m_BodyIndexToEntity.end() || itB == m_BodyIndexToEntity.end()) continue;

        ECS::Entity entityA = itA->second;
        ECS::Entity entityB = itB->second;

        if (evt.type == JoltContactEvent::Type::Added || evt.type == JoltContactEvent::Type::Persisted) {
            u64 pairKey = MakeCollisionPairKey(entityA, entityB);
            m_CurrentCollisionPairs.insert(pairKey);

            // New collision (enter)
            if (evt.type == JoltContactEvent::Type::Added) {
                if (m_PreviousCollisionPairs.find(pairKey) == m_PreviousCollisionPairs.end()) {
                    CollisionEvent collisionEvt;
                    collisionEvt.entityA = entityA;
                    collisionEvt.entityB = entityB;
                    collisionEvt.contactPoint = FromJoltR(evt.contactPoint);
                    collisionEvt.normal = FromJolt(evt.contactNormal);
                    collisionEvt.type = CollisionEvent::Type::Enter;
                    collisionEvt.isTrigger = evt.isSensor;
                    m_PendingCollisionEvents.push_back(collisionEvt);
                }
            }
        }
    }

    // Detect exits: pairs that were in previous frame but not current
    // Note: MakeCollisionPairKey stores min(a,b) in upper 32 bits, max(a,b) in lower.
    // Exit events therefore report entities in canonical (min/max) order, which may
    // differ from the original enter event ordering. This is by design — consumers
    // should not rely on entityA/entityB ordering for identity.
    for (u64 prevPair : m_PreviousCollisionPairs) {
        if (m_CurrentCollisionPairs.find(prevPair) == m_CurrentCollisionPairs.end()) {
            ECS::Entity entityA = static_cast<ECS::Entity>(prevPair >> 32);
            ECS::Entity entityB = static_cast<ECS::Entity>(prevPair & 0xFFFFFFFF);

            CollisionEvent exitEvt;
            exitEvt.entityA = entityA;
            exitEvt.entityB = entityB;
            exitEvt.type = CollisionEvent::Type::Exit;

            // Check trigger state if entities still exist
            if (m_World->IsValid(entityA)) {
                exitEvt.isTrigger = GetColliderInfo(entityA).isTrigger;
            }
            if (m_World->IsValid(entityB)) {
                exitEvt.isTrigger = exitEvt.isTrigger || GetColliderInfo(entityB).isTrigger;
            }

            m_PendingCollisionEvents.push_back(exitEvt);
        }
    }

    std::swap(m_PreviousCollisionPairs, m_CurrentCollisionPairs);
}

const std::vector<CollisionEvent>& JoltBackend::GetPendingCollisionEvents() const {
    return m_PendingCollisionEvents;
}

void JoltBackend::ClearPendingCollisionEvents() {
    m_PendingCollisionEvents.clear();
}

// ============================================================================
// Raycasting
// ============================================================================

RaycastHit JoltBackend::Raycast(const Ray& ray, f32 maxDistance, u32 layerMask) {
    RaycastHit result;
    if (!m_Initialized) return result;

    // Jolt ray: origin + direction * maxDistance
    JPH::RRayCast joltRay(ToJoltR(ray.origin), ToJolt(ray.direction) * maxDistance);

    JPH::RayCastResult castResult;
    EnjinBodyFilter bodyFilter;
    bodyFilter.filterData = &m_BodyFilterData;
    bodyFilter.layerMask = layerMask;

    JPH::BroadPhaseLayerFilter bpFilter;
    JPH::ObjectLayerFilter objFilter;
    if (m_PhysicsSystem->GetNarrowPhaseQuery().CastRay(joltRay, castResult, bpFilter, objFilter, bodyFilter)) {
        result.hit = true;
        result.distance = castResult.mFraction * maxDistance;
        result.point = FromJoltR(joltRay.GetPointOnRay(castResult.mFraction));
        result.entity = 0;

        // Resolve entity from body
        auto it = m_BodyIndexToEntity.find(castResult.mBodyID.GetIndex());
        if (it != m_BodyIndexToEntity.end()) {
            result.entity = it->second;
        }

        // Get surface normal at hit point
        JPH::BodyLockRead lock(m_PhysicsSystem->GetBodyLockInterface(), castResult.mBodyID);
        if (lock.Succeeded()) {
            result.normal = FromJolt(lock.GetBody().GetWorldSpaceSurfaceNormal(
                castResult.mSubShapeID2, joltRay.GetPointOnRay(castResult.mFraction)));
        }
    }

    return result;
}

std::vector<RaycastHit> JoltBackend::RaycastAll(const Ray& ray, f32 maxDistance, u32 layerMask) {
    std::vector<RaycastHit> results;
    if (!m_Initialized) return results;

    JPH::RRayCast joltRay(ToJoltR(ray.origin), ToJolt(ray.direction) * maxDistance);

    EnjinBodyFilter bodyFilter;
    bodyFilter.filterData = &m_BodyFilterData;
    bodyFilter.layerMask = layerMask;

    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    JPH::RayCastSettings raySettings;
    JPH::BroadPhaseLayerFilter bpFilter;
    JPH::ObjectLayerFilter objFilter;
    m_PhysicsSystem->GetNarrowPhaseQuery().CastRay(joltRay, raySettings, collector, bpFilter, objFilter, bodyFilter);

    collector.Sort();

    for (auto& hit : collector.mHits) {
        RaycastHit result;
        result.hit = true;
        result.distance = hit.mFraction * maxDistance;
        result.point = FromJoltR(joltRay.GetPointOnRay(hit.mFraction));

        auto it = m_BodyIndexToEntity.find(hit.mBodyID.GetIndex());
        if (it != m_BodyIndexToEntity.end()) {
            result.entity = it->second;
        }

        results.push_back(result);
    }

    return results;
}

// ============================================================================
// Ground Check
// ============================================================================

bool JoltBackend::CheckGround(const Math::Vector3& position, f32 checkDistance, RaycastHit& hit, u32 layerMask, ECS::Entity ignoreEntity) {
    if (!m_Initialized) return false;

    Ray ray;
    ray.origin = position;
    ray.direction = Math::Vector3(0, -1, 0);

    // Use custom raycast with self-exclusion to prevent hitting own collider
    JPH::RRayCast joltRay(ToJoltR(ray.origin), JPH::Vec3(0, -checkDistance, 0));

    JPH::RayCastResult castResult;
    EnjinBodyFilter bodyFilter;
    bodyFilter.filterData = &m_BodyFilterData;
    bodyFilter.layerMask = layerMask;

    // Exclude the requesting entity's own body
    if (ignoreEntity != 0) {
        auto it = m_EntityToBody.find(ignoreEntity);
        if (it != m_EntityToBody.end()) {
            bodyFilter.ignoreBodyID = it->second;
        }
    }

    JPH::BroadPhaseLayerFilter bpFilter;
    JPH::ObjectLayerFilter objFilter;
    if (m_PhysicsSystem->GetNarrowPhaseQuery().CastRay(joltRay, castResult, bpFilter, objFilter, bodyFilter)) {
        hit.hit = true;
        hit.distance = castResult.mFraction * checkDistance;
        hit.point = FromJoltR(joltRay.GetPointOnRay(castResult.mFraction));
        hit.entity = 0;
        auto it = m_BodyIndexToEntity.find(castResult.mBodyID.GetIndex());
        if (it != m_BodyIndexToEntity.end()) {
            hit.entity = it->second;
        }
        return true;
    }
    return false;
}

// ============================================================================
// CharacterVirtual — proper capsule character controller
// ============================================================================

void JoltBackend::CreateCharacterController(ECS::Entity entity, f32 capsuleRadius, f32 capsuleHalfHeight,
                                             const Math::Vector3& position) {
    if (!m_Initialized || !m_PhysicsSystem) return;

    // Destroy existing if re-creating
    DestroyCharacterController(entity);

    JPH::CharacterVirtualSettings settings;
    f32 stemHalfHeight = std::max(capsuleHalfHeight - capsuleRadius, 0.01f);
    settings.mShape = new JPH::CapsuleShape(stemHalfHeight, capsuleRadius);
    settings.mMass = 70.0f;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(50.0f);
    settings.mMaxStrength = 100.0f;
    settings.mCharacterPadding = 0.02f;
    settings.mPenetrationRecoverySpeed = 1.0f;
    settings.mPredictiveContactDistance = 0.1f;

    auto* character = new JPH::CharacterVirtual(
        &settings,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(),
        m_PhysicsSystem.get());

    m_CharacterControllers[entity] = character;
    ENJIN_LOG_INFO(Physics, "Created CharacterVirtual for entity %llu (r=%.2f h=%.2f)",
                   (unsigned long long)entity, capsuleRadius, capsuleHalfHeight);
}

void JoltBackend::DestroyCharacterController(ECS::Entity entity) {
    auto it = m_CharacterControllers.find(entity);
    if (it != m_CharacterControllers.end()) {
        delete it->second;
        m_CharacterControllers.erase(it);
    }
}

bool JoltBackend::HasCharacterController(ECS::Entity entity) const {
    return m_CharacterControllers.find(entity) != m_CharacterControllers.end();
}

void JoltBackend::DestroyAllCharacterControllers() {
    for (auto& [entity, character] : m_CharacterControllers) {
        delete character;
    }
    m_CharacterControllers.clear();
}

IPhysicsBackend::CharacterState JoltBackend::UpdateCharacterController(
    ECS::Entity entity, const Math::Vector3& velocity, f32 deltaTime) {
    CharacterState result;

    auto it = m_CharacterControllers.find(entity);
    if (it == m_CharacterControllers.end() || !m_Initialized) {
        result.position = velocity; // Fallback: return velocity as-is (caller handles)
        return result;
    }

    auto* character = it->second;

    // Set desired velocity
    character->SetLinearVelocity(JPH::Vec3(velocity.x, velocity.y, velocity.z));

    // Exclude the character's own collider body from collision (if it has one)
    EnjinBodyFilter bodyFilter;
    bodyFilter.filterData = &m_BodyFilterData;
    bodyFilter.layerMask = 0xFFFFFFFF;
    auto bodyIt = m_EntityToBody.find(entity);
    if (bodyIt != m_EntityToBody.end()) {
        bodyFilter.ignoreBodyID = bodyIt->second;
    }

    // Extended update: handles stair stepping + stick to floor
    JPH::CharacterVirtual::ExtendedUpdateSettings extSettings;
    extSettings.mStickToFloorStepDown = JPH::Vec3(0, -0.5f, 0);
    extSettings.mWalkStairsStepUp = JPH::Vec3(0, 0.4f, 0);
    extSettings.mWalkStairsMinStepForward = 0.02f;
    extSettings.mWalkStairsStepForwardTest = 0.15f;

    JPH::BroadPhaseLayerFilter bpFilter;
    JPH::ObjectLayerFilter objFilter;
    JPH::ShapeFilter shapeFilter;

    character->ExtendedUpdate(
        deltaTime,
        JPH::Vec3(0, -20.0f, 0),  // Gravity for contact force (not movement gravity)
        extSettings,
        bpFilter, objFilter, bodyFilter, shapeFilter,
        *m_TempAllocator);

    // Read back state
    JPH::RVec3 pos = character->GetPosition();
    result.position = Math::Vector3(static_cast<f32>(pos.GetX()),
                                     static_cast<f32>(pos.GetY()),
                                     static_cast<f32>(pos.GetZ()));

    JPH::Vec3 gn = character->GetGroundNormal();
    result.groundNormal = Math::Vector3(gn.GetX(), gn.GetY(), gn.GetZ());

    JPH::Vec3 gv = character->GetGroundVelocity();
    result.groundVelocity = Math::Vector3(gv.GetX(), gv.GetY(), gv.GetZ());

    switch (character->GetGroundState()) {
        case JPH::CharacterBase::EGroundState::OnGround:
            result.groundState = CharacterGroundState::OnGround;
            break;
        case JPH::CharacterBase::EGroundState::OnSteepGround:
            result.groundState = CharacterGroundState::OnSteepGround;
            break;
        default:
            result.groundState = CharacterGroundState::InAir;
            break;
    }

    return result;
}

// ============================================================================
// MoveAndSlide (shape cast for character controllers)
// ============================================================================

Math::Vector3 JoltBackend::MoveAndSlide(const Math::Vector3& position, const Math::Vector3& velocity,
                                         const AABB& collider, f32 deltaTime, u32 layerMask) {
    if (!m_Initialized) return position + velocity * deltaTime;

    // Fallback to simple AABB-based slide resolution
    // A full Jolt character controller would use JPH::CharacterVirtual,
    // but for API compatibility we do iterative shape cast + slide.
    Math::Vector3 newPos = position + velocity * deltaTime;
    Math::Vector3 halfExtents = collider.GetHalfSize();
    if (halfExtents.x <= 0) halfExtents.x = 0.01f;
    if (halfExtents.y <= 0) halfExtents.y = 0.01f;
    if (halfExtents.z <= 0) halfExtents.z = 0.01f;

    // Simple iterative collision resolution (up to 3 iterations)
    for (int iter = 0; iter < 3; ++iter) {
        AABB movedCollider = AABB::FromCenterSize(newPos, halfExtents * 2.0f);
        bool resolved = true;

        for (auto& [entity, bodyID] : m_EntityToBody) {
            // Skip entities not matching layer mask
            auto filterIt = m_BodyFilterData.find(bodyID.GetIndex());
            if (filterIt != m_BodyFilterData.end()) {
                if (!(filterIt->second.categoryBits & layerMask)) continue;
            }

            // Get entity AABB from Jolt body bounds
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (!transform) continue;

            // Check if entity has a collider that isn't a trigger
            ColliderInfo info = GetColliderInfo(entity);
            if (info.isTrigger) continue;

            // Build world AABB for this entity
            AABB entityAABB;
            if (auto* box = m_World->GetComponent<ECS::BoxColliderComponent>(entity)) {
                Math::Vector3 worldCenter = transform->position + box->center;
                Math::Vector3 worldSize(
                    box->size.x * transform->scale.x,
                    box->size.y * transform->scale.y,
                    box->size.z * transform->scale.z
                );
                entityAABB = AABB::FromCenterSize(worldCenter, worldSize);
            } else if (auto* sphere = m_World->GetComponent<ECS::SphereColliderComponent>(entity)) {
                Math::Vector3 worldCenter = transform->position + sphere->center;
                f32 r = sphere->radius * Math::Max(transform->scale.x, Math::Max(transform->scale.y, transform->scale.z));
                entityAABB = AABB::FromCenterSize(worldCenter, Math::Vector3(r * 2, r * 2, r * 2));
            } else if (auto* capsule = m_World->GetComponent<ECS::CapsuleColliderComponent>(entity)) {
                Math::Vector3 worldCenter = transform->position + capsule->center;
                f32 r = capsule->radius * Math::Max(transform->scale.x, transform->scale.z);
                f32 h = capsule->height * transform->scale.y;
                entityAABB = AABB::FromCenterSize(worldCenter, Math::Vector3(r * 2, h, r * 2));
            } else if (auto* meshCol = m_World->GetComponent<ECS::MeshColliderComponent>(entity)) {
                // Compute AABB from cached mesh vertices
                if (meshCol->generated && !meshCol->vertices.empty()) {
                    Math::Vector3 mn = meshCol->vertices[0], mx = meshCol->vertices[0];
                    for (const auto& v : meshCol->vertices) {
                        mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
                        mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
                    }
                    Math::Vector3 worldCenter = transform->position + (mn + mx) * 0.5f;
                    Math::Vector3 worldSize(
                        (mx.x - mn.x) * transform->scale.x,
                        (mx.y - mn.y) * transform->scale.y,
                        (mx.z - mn.z) * transform->scale.z
                    );
                    entityAABB = AABB::FromCenterSize(worldCenter, worldSize);
                } else {
                    continue;
                }
            } else {
                continue;
            }

            CollisionResult result;
            if (CheckAABBCollision(movedCollider, entityAABB, result)) {
                newPos = newPos + result.normal * result.penetration;
                resolved = false;
            }
        }

        if (resolved) break;
    }

    return newPos;
}

// ============================================================================
// Spatial Queries
// ============================================================================

std::vector<ECS::Entity> JoltBackend::GetCollidersInRadius(const Math::Vector3& center, f32 radius, u32 layerMask) {
    std::vector<ECS::Entity> result;
    if (!m_World) return result;

    f32 radiusSq = radius * radius;

    for (auto& [entity, bodyID] : m_EntityToBody) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // Layer mask filter
        auto filterIt = m_BodyFilterData.find(bodyID.GetIndex());
        if (filterIt != m_BodyFilterData.end()) {
            if (!(filterIt->second.categoryBits & layerMask)) continue;
        }

        Math::Vector3 diff = transform->position - center;
        f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (distSq <= radiusSq) {
            result.push_back(entity);
        }
    }

    return result;
}

std::vector<ECS::Entity> JoltBackend::OverlapBox(const Math::Vector3& center, const Math::Vector3& halfExtents, u32 layerMask) {
    std::vector<ECS::Entity> result;
    if (!m_World) return result;

    AABB queryBox(center - halfExtents, center + halfExtents);

    for (auto& [entity, bodyID] : m_EntityToBody) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        auto filterIt = m_BodyFilterData.find(bodyID.GetIndex());
        if (filterIt != m_BodyFilterData.end()) {
            if (!(filterIt->second.categoryBits & layerMask)) continue;
        }

        // Simple point-in-box check (could use Jolt broad phase query for more accuracy)
        if (queryBox.Contains(transform->position)) {
            result.push_back(entity);
        }
    }

    return result;
}

// ============================================================================
// Stateless Collision Checks
// ============================================================================

bool JoltBackend::CheckAABBCollision(const AABB& a, const AABB& b, CollisionResult& result) {
    result.hit = false;

    if (!a.Intersects(b)) return false;

    result.hit = true;

    f32 overlapX = Math::Min(a.max.x, b.max.x) - Math::Max(a.min.x, b.min.x);
    f32 overlapY = Math::Min(a.max.y, b.max.y) - Math::Max(a.min.y, b.min.y);
    f32 overlapZ = Math::Min(a.max.z, b.max.z) - Math::Max(a.min.z, b.min.z);

    if (overlapX <= overlapY && overlapX <= overlapZ) {
        result.penetration = overlapX;
        result.normal = (a.GetCenter().x < b.GetCenter().x) ?
                        Math::Vector3(-1, 0, 0) : Math::Vector3(1, 0, 0);
    } else if (overlapY <= overlapX && overlapY <= overlapZ) {
        result.penetration = overlapY;
        result.normal = (a.GetCenter().y < b.GetCenter().y) ?
                        Math::Vector3(0, -1, 0) : Math::Vector3(0, 1, 0);
    } else {
        result.penetration = overlapZ;
        result.normal = (a.GetCenter().z < b.GetCenter().z) ?
                        Math::Vector3(0, 0, -1) : Math::Vector3(0, 0, 1);
    }

    result.point = Math::Vector3(
        (Math::Max(a.min.x, b.min.x) + Math::Min(a.max.x, b.max.x)) * 0.5f,
        (Math::Max(a.min.y, b.min.y) + Math::Min(a.max.y, b.max.y)) * 0.5f,
        (Math::Max(a.min.z, b.min.z) + Math::Min(a.max.z, b.max.z)) * 0.5f
    );

    return true;
}

bool JoltBackend::CheckSphereCollision(const Math::Vector3& centerA, f32 radiusA,
                                        const Math::Vector3& centerB, f32 radiusB,
                                        CollisionResult& result) {
    result.hit = false;

    Math::Vector3 diff = centerB - centerA;
    f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    f32 radiusSum = radiusA + radiusB;

    if (distSq >= radiusSum * radiusSum) return false;

    result.hit = true;
    f32 dist = Math::Sqrt(distSq);

    if (dist > 0.0001f) {
        result.normal = diff * (1.0f / dist);
    } else {
        result.normal = Math::Vector3(0, 1, 0);
    }

    result.penetration = radiusSum - dist;
    result.point = centerA + result.normal * radiusA;

    return true;
}

// ============================================================================
// Body velocity query (for rewind system)
// ============================================================================

bool JoltBackend::GetBodyVelocity(ECS::Entity entity, Math::Vector3& outLinear, Math::Vector3& outAngular) const {
    auto it = m_EntityToBody.find(entity);
    if (it == m_EntityToBody.end()) return false;

    const JPH::BodyInterface& bi = m_PhysicsSystem->GetBodyInterface();
    JPH::BodyID bodyID = it->second;
    if (!bi.IsAdded(bodyID)) return false;

    JPH::Vec3 lv = bi.GetLinearVelocity(bodyID);
    JPH::Vec3 av = bi.GetAngularVelocity(bodyID);
    outLinear = Math::Vector3(lv.GetX(), lv.GetY(), lv.GetZ());
    outAngular = Math::Vector3(av.GetX(), av.GetY(), av.GetZ());
    return true;
}

// ============================================================================
// Force-set body state (for rewind system)
// ============================================================================

void JoltBackend::ForceSetBodyState(ECS::Entity entity, const Math::Vector3& position,
                                     const Math::Quaternion& rotation,
                                     const Math::Vector3& linearVel,
                                     const Math::Vector3& angularVel) {
    auto it = m_EntityToBody.find(entity);
    if (it == m_EntityToBody.end()) return;

    JPH::BodyInterface& bi = m_PhysicsSystem->GetBodyInterface();
    JPH::BodyID bodyID = it->second;
    if (!bi.IsAdded(bodyID)) return;

    JPH::RVec3 jPos(position.x, position.y, position.z);
    JPH::Quat jRot(rotation.x, rotation.y, rotation.z, rotation.w);
    bi.SetPositionAndRotation(bodyID, jPos, jRot, JPH::EActivation::Activate);
    bi.SetLinearVelocity(bodyID, JPH::Vec3(linearVel.x, linearVel.y, linearVel.z));
    bi.SetAngularVelocity(bodyID, JPH::Vec3(angularVel.x, angularVel.y, angularVel.z));
}

} // namespace Physics
} // namespace Enjin

#endif // ENJIN_PHYSICS_JOLT
