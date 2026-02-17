// Round-trip serialization test for all 34 newly-added components.
// Creates entities with specific field values, serializes to JSON string,
// deserializes into a fresh world, and verifies all data survived.

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>

using namespace Enjin;

static int g_Failures = 0;
static int g_Checks = 0;

#define CHECK_EQ(actual, expected, label) do { \
    ++g_Checks; \
    if ((actual) != (expected)) { \
        std::printf("  FAIL: %s  expected %s got different value\n", label, #expected); \
        ++g_Failures; \
    } \
} while(0)

#define CHECK_FLOAT(actual, expected, label) do { \
    ++g_Checks; \
    if (std::fabs((actual) - (expected)) > 0.001f) { \
        std::printf("  FAIL: %s  expected %.4f got %.4f\n", label, (double)(expected), (double)(actual)); \
        ++g_Failures; \
    } \
} while(0)

#define CHECK_VEC3(actual, ex, ey, ez, label) do { \
    CHECK_FLOAT((actual).x, ex, label " .x"); \
    CHECK_FLOAT((actual).y, ey, label " .y"); \
    CHECK_FLOAT((actual).z, ez, label " .z"); \
} while(0)

#define CHECK_VEC2(actual, ex, ey, label) do { \
    CHECK_FLOAT((actual).x, ex, label " .x"); \
    CHECK_FLOAT((actual).y, ey, label " .y"); \
} while(0)

#define CHECK_STR(actual, expected, label) do { \
    ++g_Checks; \
    if ((actual) != (expected)) { \
        std::printf("  FAIL: %s  expected \"%s\" got \"%s\"\n", label, (expected).c_str(), (actual).c_str()); \
        ++g_Failures; \
    } \
} while(0)

#define CHECK_BOOL(actual, expected, label) do { \
    ++g_Checks; \
    if ((actual) != (expected)) { \
        std::printf("  FAIL: %s  expected %s got %s\n", label, (expected) ? "true" : "false", (actual) ? "true" : "false"); \
        ++g_Failures; \
    } \
} while(0)

#define HAS_COMPONENT(world, entity, Type, label) do { \
    ++g_Checks; \
    if (!(world).HasComponent<Type>(entity)) { \
        std::printf("  FAIL: %s  component missing after round-trip\n", label); \
        ++g_Failures; \
    } \
} while(0)

int main() {
    std::printf("=== Scene Serializer Round-Trip Test ===\n\n");

    // ---------- Phase 1: Create world & entities with all 34 components ----------
    ECS::World srcWorld;

    // Entity 1: Physics & Collision
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"PhysicsEntity"});
        auto& t = srcWorld.AddComponent<ECS::TransformComponent>(e);
        t.position = Math::Vector3(1, 2, 3);

        auto& rb = srcWorld.AddComponent<ECS::RigidbodyComponent>(e);
        rb.mass = 5.5f;
        rb.drag = 0.3f;
        rb.angularDrag = 0.1f;
        rb.useGravity = false;
        rb.gravityScale = 2.0f;
        rb.freezePositionX = true;
        rb.freezeRotationZ = true;
        rb.bodyType = ECS::RigidbodyComponent::BodyType::Kinematic;
        rb.collisionMode = ECS::RigidbodyComponent::CollisionMode::Continuous;

        auto& box = srcWorld.AddComponent<ECS::BoxColliderComponent>(e);
        box.center = Math::Vector3(0.1f, 0.2f, 0.3f);
        box.size = Math::Vector3(2, 3, 4);
        box.isTrigger = true;
        box.friction = 0.8f;
        box.bounciness = 0.4f;
        box.categoryBits = (1u << 3);
        box.collisionMask = 0xFF00FF00;

        auto& sphere = srcWorld.AddComponent<ECS::SphereColliderComponent>(e);
        sphere.center = Math::Vector3(1, 1, 1);
        sphere.radius = 2.5f;
        sphere.friction = 0.9f;

        auto& capsule = srcWorld.AddComponent<ECS::CapsuleColliderComponent>(e);
        capsule.radius = 0.75f;
        capsule.height = 3.0f;
        capsule.direction = ECS::CapsuleColliderComponent::Direction::Z;
        capsule.bounciness = 0.6f;
    }

    // Entity 2: Health & Damage
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"HealthEntity"});

        auto& h = srcWorld.AddComponent<ECS::HealthComponent>(e);
        h.maxHealth = 200.0f;
        h.currentHealth = 150.0f;  // Partially damaged
        h.regenRate = 5.0f;
        h.regenDelay = 2.0f;
        h.isInvulnerable = true;
        h.invulnerabilityTime = 1.5f;
        h.maxShield = 50.0f;
        h.currentShield = 30.0f;  // Partially depleted
        h.shieldRegenRate = 3.0f;
        h.shieldRegenDelay = 4.0f;

        auto& d = srcWorld.AddComponent<ECS::DamageComponent>(e);
        d.damage = 25.0f;
        d.knockbackForce = 10.0f;
        d.destroyOnHit = false;
        d.damageOnce = false;
        d.damageInterval = 0.5f;
        d.type = ECS::DamageComponent::DamageType::Fire;
    }

    // Entity 3: Triggers & Interaction
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"TriggerEntity"});

        auto& tz = srcWorld.AddComponent<ECS::TriggerZoneComponent>(e);
        tz.shape = ECS::TriggerZoneComponent::Shape::Sphere;
        tz.boxSize = Math::Vector3(5, 5, 5);
        tz.sphereRadius = 3.0f;
        tz.triggerMask = 0x0F;
        tz.triggerOnce = true;

        auto& ic = srcWorld.AddComponent<ECS::InteractableComponent>(e);
        ic.promptText = "Open chest";
        ic.interactionRange = 3.5f;
        ic.requiresLookAt = false;
        ic.lookAtAngle = 60.0f;
        ic.singleUse = true;
        ic.highlightColor = Math::Vector3(0, 1, 0);

        auto& pk = srcWorld.AddComponent<ECS::PickupComponent>(e);
        pk.type = ECS::PickupComponent::PickupType::Key;
        pk.value = 5.0f;
        pk.customId = "golden_key";
        pk.magnetToPlayer = true;
        pk.magnetRange = 5.0f;
        pk.canRespawn = true;
        pk.respawnTime = 15.0f;
        pk.bobHeight = 0.5f;
    }

    // Entity 4: Tags, Layers, Billboard
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"TagEntity"});

        auto& tag = srcWorld.AddComponent<ECS::TagComponent>(e);
        tag.tags = {"enemy", "boss", "damageable"};

        auto& layer = srcWorld.AddComponent<ECS::LayerComponent>(e);
        layer.layer = 7;
        layer.layerName = "Enemies";

        auto& bb = srcWorld.AddComponent<ECS::BillboardComponent>(e);
        bb.faceCamera = true;
        bb.lockY = false;
        bb.rotationOffset = 45.0f;
    }

    // Entity 5: Particles
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"ParticleEntity"});

        auto& pe = srcWorld.AddComponent<ECS::ParticleEmitterComponent>(e);
        pe.playOnAwake = false;
        pe.loop = false;
        pe.emissionRate = 50.0f;
        pe.burstCount = 20;
        pe.lifetime = 3.0f;
        pe.startSpeed = 10.0f;
        pe.startColor = Math::Vector3(1, 0, 0);
        pe.endColor = Math::Vector3(0, 0, 1);
        pe.shape = ECS::ParticleEmitterComponent::EmitterShape::Sphere;
        pe.shapeRadius = 2.0f;
        pe.texturePath = "particles/fire.png";
        pe.textureSheetX = 4;
        pe.textureSheetY = 4;
    }

    // Entity 6: 2D Rendering
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"Sprite2DEntity"});

        auto& sp = srcWorld.AddComponent<ECS::Sprite2DComponent>(e);
        sp.texturePath = "sprites/hero.png";
        sp.srcX = 32; sp.srcY = 64;
        sp.srcWidth = 16; sp.srcHeight = 16;
        sp.size = Math::Vector2(2.0f, 2.0f);
        sp.pivot = Math::Vector2(0.5f, 1.0f);
        sp.tint = Math::Vector3(0.8f, 0.9f, 1.0f);
        sp.alpha = 0.75f;
        sp.sortingLayer = 5;
        sp.orderInLayer = 10;
        sp.flipX = true;

        auto& anim = srcWorld.AddComponent<ECS::AnimatedSprite2DComponent>(e);
        anim.frames.push_back({0, 0, 0.1f});
        anim.frames.push_back({16, 0, 0.15f});
        anim.frames.push_back({32, 0, 0.1f});
        anim.playing = true;
        anim.loop = false;
        anim.playbackSpeed = 1.5f;

        auto& tm = srcWorld.AddComponent<ECS::TilemapComponent>(e);
        tm.tiles = {0, 1, 2, 3, -1, 5, 6, 7, 8};
        tm.width = 3; tm.height = 3;
        tm.tilesetPath = "tiles/dungeon.png";
        tm.tileWidth = 16; tm.tileHeight = 16;
        tm.tilesetColumns = 8;
        tm.worldTileWidth = 1.0f;
        tm.worldTileHeight = 1.0f;
        tm.hasCollision = true;
        tm.collisionMask = {true, false, true, false, false, true, false, false, true};

        auto& cb = srcWorld.AddComponent<ECS::Camera2DBoundsComponent>(e);
        cb.useBounds = true;
        cb.minBounds = Math::Vector2(-10, -10);
        cb.maxBounds = Math::Vector2(100, 100);
        cb.boundsPadding = 2.0f;
        cb.followSmoothing = 8.0f;
        cb.followOffset = Math::Vector2(0, 3);
        cb.minZoom = 0.25f;
        cb.maxZoom = 4.0f;
        cb.currentZoom = 1.5f;
    }

    // Entity 7: Logic
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"LogicEntity"});

        auto& sm = srcWorld.AddComponent<ECS::StateMachineComponent>(e);
        sm.currentState = "patrol";
        sm.floatParams = {{"speed", 3.0f}, {"timer", 0.0f}};
        sm.intParams = {{"count", 5}};
        sm.boolParams = {{"alert", true}};
        // Add a state for the upgraded component
        ECS::SMState patrolState;
        patrolState.name = "patrol";
        sm.states.push_back(patrolState);

        auto& dl = srcWorld.AddComponent<ECS::DialogueComponent>(e);
        dl.dialogueLines = {"Hello traveler!", "Beware the dungeon ahead.", "Good luck!"};
        dl.charDelay = 0.03f;
        dl.speakerName = "Old Man";
        dl.portraitPath = "portraits/oldman.png";
        dl.typeSound = "sfx/type.wav";
        dl.playTypeSound = true;
        dl.choices.push_back({"Thank you", "npc_goodbye"});
        dl.choices.push_back({"Tell me more", "npc_lore"});
    }

    // Entity 8: AI & Navigation
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"AIEntity"});

        auto& ai = srcWorld.AddComponent<ECS::AIControllerComponent>(e);
        ai.detectionRange = 15.0f;
        ai.attackRange = 3.0f;
        ai.loseTargetRange = 20.0f;
        ai.fieldOfView = 90.0f;
        ai.moveSpeed = 4.0f;
        ai.turnSpeed = 270.0f;
        ai.attackCooldown = 2.0f;
        ai.attackDamage = 15.0f;
        ai.patrolPoints = {Math::Vector3(0,0,0), Math::Vector3(10,0,0), Math::Vector3(10,0,10)};
        ai.patrolWaitTime = 3.0f;

        auto& ft = srcWorld.AddComponent<ECS::FollowTargetComponent>(e);
        ft.followDistance = 5.0f;
        ft.minDistance = 2.0f;
        ft.maxDistance = 30.0f;
        ft.moveSpeed = 6.0f;
        ft.smoothTime = 0.5f;
        ft.matchTargetRotation = true;
        ft.offset = Math::Vector3(0, 1, -2);
        ft.useLocalOffset = true;

        auto& la = srcWorld.AddComponent<ECS::LookAtTargetComponent>(e);
        la.worldTarget = Math::Vector3(5, 5, 5);
        la.useWorldTarget = true;
        la.rotationSpeed = 90.0f;
        la.constrainX = true;
        la.constrainZ = false;
        la.minYaw = -90.0f;
        la.maxYaw = 90.0f;

        auto& wp = srcWorld.AddComponent<ECS::WaypointComponent>(e);
        wp.waypointId = "patrol_A";
        wp.index = 2;
        wp.waitTime = 1.5f;
        wp.radius = 1.0f;
    }

    // Entity 9: Spawning & Timers
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"SpawnEntity"});

        auto& sp = srcWorld.AddComponent<ECS::SpawnPointComponent>(e);
        sp.spawnId = "enemy_spawn_1";
        sp.prefabToSpawn = "prefabs/goblin.prefab";
        sp.spawnOnStart = true;
        sp.spawnDelay = 2.0f;
        sp.respawnTime = 10.0f;
        sp.maxSpawns = 5;
        sp.spawnRadius = 3.0f;
        sp.randomRotation = true;

        auto& timer = srcWorld.AddComponent<ECS::TimerComponent>(e);
        timer.duration = 5.0f;
        timer.loop = true;
        timer.autoStart = true;
    }

    // Entity 10: Inventory & Save Data
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"InventoryEntity"});

        auto& inv = srcWorld.AddComponent<ECS::InventoryComponent>(e);
        inv.slots.push_back({"sword", 1, 1});
        inv.slots.push_back({"potion", 5, 20});
        inv.maxSlots = 30;
        inv.coins = 150;
        inv.gems = 10;
        inv.keys = {"dungeon_key", "chest_key"};

        auto& sd = srcWorld.AddComponent<ECS::SaveDataComponent>(e);
        sd.savePosition = true;
        sd.saveRotation = false;
        sd.saveScale = true;
        sd.saveEnabled = true;
        sd.customData = {{"quest_progress", "3"}, {"faction", "rebels"}};
    }

    // Entity 11: Flower Components
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"FlowerEntity"});

        auto& jm = srcWorld.AddComponent<ECS::JellyMeshComponent>(e);
        jm.springStiffness = 120.0f;
        jm.damping = 8.0f;
        jm.maxStretch = 0.5f;

        auto& te = srcWorld.AddComponent<ECS::TetherComponent>(e);
        te.attachLocalPos = Math::Vector3(0, 1, 0);
        te.connectedEntity = e;  // Self-ref for test purposes
        te.breakDistance = 3.0f;
        te.tensionRamp = 3.0f;
        te.autoMass = 0.5f;
        te.autoSpringK = 200.0f;
        te.autoDamping = 12.0f;
        te.autoBreakForce = 60.0f;
        te.autoDrag = 2.0f;

        auto& gr = srcWorld.AddComponent<ECS::GrabbableComponent>(e);
        gr.pullForce = 20.0f;
        gr.grabRadius = 1.0f;
        gr.maxPullDistance = 3.0f;
        gr.maxVelocity = 75.0f;
        gr.windSwayScale = 0.25f;

        auto& fs = srcWorld.AddComponent<ECS::FlowerStemComponent>(e);
        fs.healthyBonus = 15.0f;
        fs.witheredPenalty = 8.0f;
        fs.groundLevel = -0.5f;
        fs.sapColor = Math::Vector3(0.2f, 0.6f, 0.1f);
        fs.stemSwayAmplitude = 0.1f;

        auto& fpc = srcWorld.AddComponent<ECS::FlowerParticleConfigComponent>(e);
        fpc.breakBurstCount = 25;
        fpc.breakBurstSpeed = 3.0f;
        fpc.breakBurstUpKick = 2.5f;
        fpc.breakBurstLifetime = 1.0f;
        fpc.breakBurstScale = 0.1f;
        fpc.breakDripCount = 10;
        fpc.breakDripSpeed = 0.7f;
        fpc.breakDripLifetime = 1.5f;
        fpc.splashCount = 15;
        fpc.splashSpeed = 2.0f;
        fpc.splashUpKick = 3.0f;
        fpc.splashLifetime = 0.8f;
        fpc.tensionDripRate = 4.0f;
        fpc.tensionDripThreshold = 0.2f;
        fpc.tensionSquirtSpeed = 3.0f;
        fpc.particleGravity = 12.0f;
    }

    // Entity 12: LOD, Grass, Vegetation
    {
        auto e = srcWorld.CreateEntity();
        srcWorld.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"LODEntity"});

        auto& lod = srcWorld.AddComponent<ECS::LODComponent>(e);
        lod.levelCount = 2;
        lod.baseDistance = 15.0f;
        lod.distanceMultiplier = 3.0f;
        lod.enabled = true;
        lod.autoGenerated = true;
        lod.reductionRatios = {1.0f, 0.4f, 0.2f, 0.1f, 0.05f};
        // Add a simple mesh to LOD level 0
        ECS::MeshComponent::Vertex v;
        v.position = Math::Vector3(0, 0, 0);
        v.normal = Math::Vector3(0, 1, 0);
        v.uv = Math::Vector2(0, 0);
        lod.levels[0].mesh.vertices.push_back(v);
        v.position = Math::Vector3(1, 0, 0);
        lod.levels[0].mesh.vertices.push_back(v);
        v.position = Math::Vector3(0, 1, 0);
        lod.levels[0].mesh.vertices.push_back(v);
        lod.levels[0].mesh.indices = {0, 1, 2};
        lod.levels[0].maxDistance = 15.0f;
        lod.levels[0].reductionRatio = 1.0f;
        lod.levels[1].mesh.vertices.push_back(lod.levels[0].mesh.vertices[0]);
        lod.levels[1].mesh.vertices.push_back(lod.levels[0].mesh.vertices[1]);
        lod.levels[1].mesh.indices = {0, 1, 0};
        lod.levels[1].maxDistance = 45.0f;
        lod.levels[1].reductionRatio = 0.4f;

        auto& gv = srcWorld.AddComponent<ECS::GrassVolumeComponent>(e);
        gv.halfExtents = Math::Vector3(20, 0, 20);
        gv.density = 10000;
        gv.bladeHeight = 0.5f;
        gv.bladeHeightVariance = 0.2f;
        gv.bladeWidth = 0.05f;
        gv.baseColor = Math::Vector3(0.1f, 0.4f, 0.05f);
        gv.tipColor = Math::Vector3(0.3f, 0.6f, 0.15f);
        gv.windSwayStrength = 2.0f;

        auto& veg = srcWorld.AddComponent<ECS::VegetationComponent>(e);
        veg.swayStrength = 1.5f;
        veg.swayFrequency = 0.8f;
        veg.useVertexColorWeight = false;
    }

    // ---------- Phase 2: Serialize ----------
    std::printf("Serializing %zu entities...\n", srcWorld.GetAllEntities().size());
    Scene::SceneSerializer serializer(&srcWorld);
    Scene::SerializationOptions opts;
    opts.includeVertexData = true;
    opts.prettyPrint = false;
    std::string jsonStr = serializer.SaveToString(opts);

    if (jsonStr.empty()) {
        std::printf("FATAL: SaveToString returned empty string!\n");
        return 1;
    }
    std::printf("Serialized to %zu bytes of JSON.\n\n", jsonStr.size());

    // ---------- Phase 3: Deserialize into fresh world ----------
    ECS::World dstWorld;
    Scene::SceneSerializer loader(&dstWorld);
    auto result = loader.LoadFromString(jsonStr, true);

    if (!result.success) {
        std::printf("FATAL: LoadFromString failed: %s\n", result.error.c_str());
        return 1;
    }
    std::printf("Deserialized %zu entities.\n\n", result.entities.size());

    CHECK_EQ(result.entities.size(), srcWorld.GetAllEntities().size(), "entity count");

    // ---------- Phase 4: Verify all component data ----------
    auto entities = dstWorld.GetAllEntities();

    // Helper: find entity by name
    auto findEntity = [&](const std::string& name) -> ECS::Entity {
        for (auto e : entities) {
            auto* nc = dstWorld.GetComponent<ECS::NameComponent>(e);
            if (nc && nc->name == name) return e;
        }
        return ECS::INVALID_ENTITY;
    };

    // --- Physics ---
    std::printf("Checking Physics & Collision...\n");
    {
        auto e = findEntity("PhysicsEntity");
        CHECK_EQ(e != ECS::INVALID_ENTITY, true, "PhysicsEntity exists");
        HAS_COMPONENT(dstWorld, e, ECS::RigidbodyComponent, "RigidbodyComponent");
        auto* rb = dstWorld.GetComponent<ECS::RigidbodyComponent>(e);
        if (rb) {
            CHECK_FLOAT(rb->mass, 5.5f, "rb.mass");
            CHECK_FLOAT(rb->drag, 0.3f, "rb.drag");
            CHECK_BOOL(rb->useGravity, false, "rb.useGravity");
            CHECK_FLOAT(rb->gravityScale, 2.0f, "rb.gravityScale");
            CHECK_BOOL(rb->freezePositionX, true, "rb.freezePositionX");
            CHECK_BOOL(rb->freezeRotationZ, true, "rb.freezeRotationZ");
            CHECK_EQ(static_cast<int>(rb->bodyType), static_cast<int>(ECS::RigidbodyComponent::BodyType::Kinematic), "rb.bodyType");
            CHECK_EQ(static_cast<int>(rb->collisionMode), static_cast<int>(ECS::RigidbodyComponent::CollisionMode::Continuous), "rb.collisionMode");
            // Runtime state should be default
            CHECK_FLOAT(rb->velocity.x, 0.0f, "rb.velocity (runtime reset)");
            CHECK_BOOL(rb->isGrounded, false, "rb.isGrounded (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::BoxColliderComponent, "BoxColliderComponent");
        auto* box = dstWorld.GetComponent<ECS::BoxColliderComponent>(e);
        if (box) {
            CHECK_VEC3(box->center, 0.1f, 0.2f, 0.3f, "box.center");
            CHECK_VEC3(box->size, 2, 3, 4, "box.size");
            CHECK_BOOL(box->isTrigger, true, "box.isTrigger");
            CHECK_FLOAT(box->friction, 0.8f, "box.friction");
            CHECK_EQ(box->categoryBits, (1u << 3), "box.categoryBits");
            CHECK_EQ(box->collisionMask, 0xFF00FF00u, "box.collisionMask");
        }
        HAS_COMPONENT(dstWorld, e, ECS::SphereColliderComponent, "SphereColliderComponent");
        auto* sc = dstWorld.GetComponent<ECS::SphereColliderComponent>(e);
        if (sc) {
            CHECK_FLOAT(sc->radius, 2.5f, "sphere.radius");
        }
        HAS_COMPONENT(dstWorld, e, ECS::CapsuleColliderComponent, "CapsuleColliderComponent");
        auto* cc = dstWorld.GetComponent<ECS::CapsuleColliderComponent>(e);
        if (cc) {
            CHECK_FLOAT(cc->height, 3.0f, "capsule.height");
            CHECK_EQ(static_cast<int>(cc->direction), static_cast<int>(ECS::CapsuleColliderComponent::Direction::Z), "capsule.direction");
        }
    }

    // --- Health & Damage ---
    std::printf("Checking Health & Damage...\n");
    {
        auto e = findEntity("HealthEntity");
        HAS_COMPONENT(dstWorld, e, ECS::HealthComponent, "HealthComponent");
        auto* h = dstWorld.GetComponent<ECS::HealthComponent>(e);
        if (h) {
            CHECK_FLOAT(h->maxHealth, 200.0f, "h.maxHealth");
            CHECK_FLOAT(h->currentHealth, 150.0f, "h.currentHealth (round-trip)");
            CHECK_FLOAT(h->regenRate, 5.0f, "h.regenRate");
            CHECK_BOOL(h->isInvulnerable, true, "h.isInvulnerable");
            CHECK_FLOAT(h->maxShield, 50.0f, "h.maxShield");
            CHECK_FLOAT(h->currentShield, 30.0f, "h.currentShield (round-trip)");
            CHECK_BOOL(h->isDead, false, "h.isDead");
        }
        HAS_COMPONENT(dstWorld, e, ECS::DamageComponent, "DamageComponent");
        auto* d = dstWorld.GetComponent<ECS::DamageComponent>(e);
        if (d) {
            CHECK_FLOAT(d->damage, 25.0f, "d.damage");
            CHECK_FLOAT(d->knockbackForce, 10.0f, "d.knockbackForce");
            CHECK_BOOL(d->destroyOnHit, false, "d.destroyOnHit");
            CHECK_EQ(static_cast<int>(d->type), static_cast<int>(ECS::DamageComponent::DamageType::Fire), "d.type");
            CHECK_EQ(d->damagedEntities.size(), (size_t)0, "d.damagedEntities (runtime reset)");
        }
    }

    // --- Triggers ---
    std::printf("Checking Triggers & Interaction...\n");
    {
        auto e = findEntity("TriggerEntity");
        HAS_COMPONENT(dstWorld, e, ECS::TriggerZoneComponent, "TriggerZoneComponent");
        auto* tz = dstWorld.GetComponent<ECS::TriggerZoneComponent>(e);
        if (tz) {
            CHECK_EQ(static_cast<int>(tz->shape), static_cast<int>(ECS::TriggerZoneComponent::Shape::Sphere), "tz.shape");
            CHECK_FLOAT(tz->sphereRadius, 3.0f, "tz.sphereRadius");
            CHECK_BOOL(tz->triggerOnce, true, "tz.triggerOnce");
            CHECK_BOOL(tz->hasTriggered, false, "tz.hasTriggered (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::InteractableComponent, "InteractableComponent");
        auto* ic = dstWorld.GetComponent<ECS::InteractableComponent>(e);
        if (ic) {
            CHECK_STR(ic->promptText, std::string("Open chest"), "ic.promptText");
            CHECK_FLOAT(ic->interactionRange, 3.5f, "ic.interactionRange");
            CHECK_BOOL(ic->singleUse, true, "ic.singleUse");
            CHECK_BOOL(ic->hasBeenUsed, false, "ic.hasBeenUsed (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::PickupComponent, "PickupComponent");
        auto* pk = dstWorld.GetComponent<ECS::PickupComponent>(e);
        if (pk) {
            CHECK_EQ(static_cast<int>(pk->type), static_cast<int>(ECS::PickupComponent::PickupType::Key), "pk.type");
            CHECK_STR(pk->customId, std::string("golden_key"), "pk.customId");
            CHECK_BOOL(pk->magnetToPlayer, true, "pk.magnetToPlayer");
            CHECK_BOOL(pk->isCollected, false, "pk.isCollected (runtime reset)");
        }
    }

    // --- Tags, Layers, Billboard ---
    std::printf("Checking Tags, Layers, Billboard...\n");
    {
        auto e = findEntity("TagEntity");
        HAS_COMPONENT(dstWorld, e, ECS::TagComponent, "TagComponent");
        auto* tag = dstWorld.GetComponent<ECS::TagComponent>(e);
        if (tag) {
            CHECK_EQ(tag->tags.size(), (size_t)3, "tag count");
            CHECK_STR(tag->tags[0], std::string("enemy"), "tag[0]");
            CHECK_STR(tag->tags[2], std::string("damageable"), "tag[2]");
        }
        HAS_COMPONENT(dstWorld, e, ECS::LayerComponent, "LayerComponent");
        auto* layer = dstWorld.GetComponent<ECS::LayerComponent>(e);
        if (layer) {
            CHECK_EQ(layer->layer, 7u, "layer.layer");
            CHECK_STR(layer->layerName, std::string("Enemies"), "layer.layerName");
        }
        HAS_COMPONENT(dstWorld, e, ECS::BillboardComponent, "BillboardComponent");
        auto* bb = dstWorld.GetComponent<ECS::BillboardComponent>(e);
        if (bb) {
            CHECK_BOOL(bb->lockY, false, "bb.lockY");
            CHECK_FLOAT(bb->rotationOffset, 45.0f, "bb.rotationOffset");
        }
    }

    // --- Particles ---
    std::printf("Checking Particles...\n");
    {
        auto e = findEntity("ParticleEntity");
        HAS_COMPONENT(dstWorld, e, ECS::ParticleEmitterComponent, "ParticleEmitterComponent");
        auto* pe = dstWorld.GetComponent<ECS::ParticleEmitterComponent>(e);
        if (pe) {
            CHECK_BOOL(pe->playOnAwake, false, "pe.playOnAwake");
            CHECK_FLOAT(pe->emissionRate, 50.0f, "pe.emissionRate");
            CHECK_EQ(pe->burstCount, 20, "pe.burstCount");
            CHECK_VEC3(pe->startColor, 1, 0, 0, "pe.startColor");
            CHECK_VEC3(pe->endColor, 0, 0, 1, "pe.endColor");
            CHECK_STR(pe->texturePath, std::string("particles/fire.png"), "pe.texturePath");
            CHECK_EQ(pe->textureSheetX, 4, "pe.textureSheetX");
        }
    }

    // --- 2D Rendering ---
    std::printf("Checking 2D Rendering...\n");
    {
        auto e = findEntity("Sprite2DEntity");
        HAS_COMPONENT(dstWorld, e, ECS::Sprite2DComponent, "Sprite2DComponent");
        auto* sp = dstWorld.GetComponent<ECS::Sprite2DComponent>(e);
        if (sp) {
            CHECK_STR(sp->texturePath, std::string("sprites/hero.png"), "sp.texturePath");
            CHECK_FLOAT(sp->srcX, 32.0f, "sp.srcX");
            CHECK_VEC2(sp->size, 2.0f, 2.0f, "sp.size");
            CHECK_FLOAT(sp->alpha, 0.75f, "sp.alpha");
            CHECK_EQ(sp->sortingLayer, 5, "sp.sortingLayer");
            CHECK_BOOL(sp->flipX, true, "sp.flipX");
        }
        HAS_COMPONENT(dstWorld, e, ECS::AnimatedSprite2DComponent, "AnimatedSprite2DComponent");
        auto* anim = dstWorld.GetComponent<ECS::AnimatedSprite2DComponent>(e);
        if (anim) {
            CHECK_EQ(anim->frames.size(), (size_t)3, "anim frame count");
            CHECK_FLOAT(anim->frames[1].srcX, 16.0f, "anim.frames[1].srcX");
            CHECK_FLOAT(anim->playbackSpeed, 1.5f, "anim.playbackSpeed");
            CHECK_BOOL(anim->loop, false, "anim.loop");
            CHECK_EQ(anim->currentFrame, 0u, "anim.currentFrame (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::TilemapComponent, "TilemapComponent");
        auto* tm = dstWorld.GetComponent<ECS::TilemapComponent>(e);
        if (tm) {
            CHECK_EQ(tm->width, 3u, "tm.width");
            CHECK_EQ(tm->tiles.size(), (size_t)9, "tm.tiles count");
            CHECK_EQ(tm->tiles[4], -1, "tm.tiles[4] (empty)");
            CHECK_BOOL(tm->hasCollision, true, "tm.hasCollision");
            CHECK_EQ(tm->collisionMask.size(), (size_t)9, "tm.collisionMask count");
        }
        HAS_COMPONENT(dstWorld, e, ECS::Camera2DBoundsComponent, "Camera2DBoundsComponent");
        auto* cb = dstWorld.GetComponent<ECS::Camera2DBoundsComponent>(e);
        if (cb) {
            CHECK_BOOL(cb->useBounds, true, "cb.useBounds");
            CHECK_VEC2(cb->minBounds, -10, -10, "cb.minBounds");
            CHECK_VEC2(cb->maxBounds, 100, 100, "cb.maxBounds");
            CHECK_FLOAT(cb->currentZoom, 1.5f, "cb.currentZoom");
        }
    }

    // --- Logic ---
    std::printf("Checking Logic...\n");
    {
        auto e = findEntity("LogicEntity");
        HAS_COMPONENT(dstWorld, e, ECS::StateMachineComponent, "StateMachineComponent");
        auto* sm = dstWorld.GetComponent<ECS::StateMachineComponent>(e);
        if (sm) {
            CHECK_STR(sm->currentState, std::string("patrol"), "sm.currentState");
            CHECK_EQ(sm->floatParams.size(), (size_t)2, "sm.floatParams count");
            CHECK_FLOAT(sm->GetFloat("speed"), 3.0f, "sm.floatParams speed");
            CHECK_EQ(sm->boolParams.size(), (size_t)1, "sm.boolParams count");
            CHECK_BOOL(sm->GetBool("alert"), true, "sm.boolParams alert");
            CHECK_FLOAT(sm->stateTime, 0.0f, "sm.stateTime (runtime reset)");
            CHECK_EQ(sm->states.size(), (size_t)1, "sm.states count");
            CHECK_STR(sm->states[0].name, std::string("patrol"), "sm.states[0].name");
        }
        HAS_COMPONENT(dstWorld, e, ECS::DialogueComponent, "DialogueComponent");
        auto* dl = dstWorld.GetComponent<ECS::DialogueComponent>(e);
        if (dl) {
            CHECK_EQ(dl->dialogueLines.size(), (size_t)3, "dl.dialogueLines count");
            CHECK_STR(dl->speakerName, std::string("Old Man"), "dl.speakerName");
            CHECK_FLOAT(dl->charDelay, 0.03f, "dl.charDelay");
            CHECK_EQ(dl->choices.size(), (size_t)2, "dl.choices count");
            CHECK_STR(dl->choices[0].text, std::string("Thank you"), "dl.choices[0].text");
            CHECK_STR(dl->choices[1].nextDialogueId, std::string("npc_lore"), "dl.choices[1].nextDialogueId");
            CHECK_BOOL(dl->isTyping, false, "dl.isTyping (runtime reset)");
        }
    }

    // --- AI ---
    std::printf("Checking AI & Navigation...\n");
    {
        auto e = findEntity("AIEntity");
        HAS_COMPONENT(dstWorld, e, ECS::AIControllerComponent, "AIControllerComponent");
        auto* ai = dstWorld.GetComponent<ECS::AIControllerComponent>(e);
        if (ai) {
            CHECK_FLOAT(ai->detectionRange, 15.0f, "ai.detectionRange");
            CHECK_FLOAT(ai->attackDamage, 15.0f, "ai.attackDamage");
            CHECK_EQ(ai->patrolPoints.size(), (size_t)3, "ai.patrolPoints count");
            CHECK_VEC3(ai->patrolPoints[2], 10, 0, 10, "ai.patrolPoints[2]");
            CHECK_FLOAT(ai->attackTimer, 0.0f, "ai.attackTimer (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::FollowTargetComponent, "FollowTargetComponent");
        auto* ft = dstWorld.GetComponent<ECS::FollowTargetComponent>(e);
        if (ft) {
            CHECK_FLOAT(ft->followDistance, 5.0f, "ft.followDistance");
            CHECK_BOOL(ft->matchTargetRotation, true, "ft.matchTargetRotation");
            CHECK_VEC3(ft->offset, 0, 1, -2, "ft.offset");
            CHECK_BOOL(ft->useLocalOffset, true, "ft.useLocalOffset");
        }
        HAS_COMPONENT(dstWorld, e, ECS::LookAtTargetComponent, "LookAtTargetComponent");
        auto* la = dstWorld.GetComponent<ECS::LookAtTargetComponent>(e);
        if (la) {
            CHECK_VEC3(la->worldTarget, 5, 5, 5, "la.worldTarget");
            CHECK_BOOL(la->useWorldTarget, true, "la.useWorldTarget");
            CHECK_FLOAT(la->minYaw, -90.0f, "la.minYaw");
        }
        HAS_COMPONENT(dstWorld, e, ECS::WaypointComponent, "WaypointComponent");
        auto* wp = dstWorld.GetComponent<ECS::WaypointComponent>(e);
        if (wp) {
            CHECK_STR(wp->waypointId, std::string("patrol_A"), "wp.waypointId");
            CHECK_EQ(wp->index, 2, "wp.index");
        }
    }

    // --- Spawning & Timers ---
    std::printf("Checking Spawning & Timers...\n");
    {
        auto e = findEntity("SpawnEntity");
        HAS_COMPONENT(dstWorld, e, ECS::SpawnPointComponent, "SpawnPointComponent");
        auto* sp = dstWorld.GetComponent<ECS::SpawnPointComponent>(e);
        if (sp) {
            CHECK_STR(sp->spawnId, std::string("enemy_spawn_1"), "sp.spawnId");
            CHECK_STR(sp->prefabToSpawn, std::string("prefabs/goblin.prefab"), "sp.prefabToSpawn");
            CHECK_BOOL(sp->spawnOnStart, true, "sp.spawnOnStart");
            CHECK_EQ(sp->maxSpawns, 5, "sp.maxSpawns");
            CHECK_EQ(sp->currentSpawns, 0, "sp.currentSpawns (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::TimerComponent, "TimerComponent");
        auto* t = dstWorld.GetComponent<ECS::TimerComponent>(e);
        if (t) {
            CHECK_FLOAT(t->duration, 5.0f, "timer.duration");
            CHECK_BOOL(t->loop, true, "timer.loop");
            CHECK_BOOL(t->autoStart, true, "timer.autoStart");
            CHECK_FLOAT(t->elapsed, 0.0f, "timer.elapsed (runtime reset)");
            CHECK_BOOL(t->isRunning, false, "timer.isRunning (runtime reset)");
        }
    }

    // --- Inventory & Save Data ---
    std::printf("Checking Inventory & Save Data...\n");
    {
        auto e = findEntity("InventoryEntity");
        HAS_COMPONENT(dstWorld, e, ECS::InventoryComponent, "InventoryComponent");
        auto* inv = dstWorld.GetComponent<ECS::InventoryComponent>(e);
        if (inv) {
            CHECK_EQ(inv->slots.size(), (size_t)2, "inv.slots count");
            CHECK_STR(inv->slots[0].itemId, std::string("sword"), "inv.slots[0].itemId");
            CHECK_EQ(inv->slots[1].quantity, 5, "inv.slots[1].quantity");
            CHECK_EQ(inv->maxSlots, (usize)30, "inv.maxSlots");
            CHECK_EQ(inv->coins, 150, "inv.coins");
            CHECK_EQ(inv->gems, 10, "inv.gems");
            CHECK_EQ(inv->keys.size(), (size_t)2, "inv.keys count");
            CHECK_STR(inv->keys[1], std::string("chest_key"), "inv.keys[1]");
        }
        HAS_COMPONENT(dstWorld, e, ECS::SaveDataComponent, "SaveDataComponent");
        auto* sd = dstWorld.GetComponent<ECS::SaveDataComponent>(e);
        if (sd) {
            CHECK_BOOL(sd->savePosition, true, "sd.savePosition");
            CHECK_BOOL(sd->saveRotation, false, "sd.saveRotation");
            CHECK_BOOL(sd->saveScale, true, "sd.saveScale");
            CHECK_EQ(sd->customData.size(), (size_t)2, "sd.customData count");
            CHECK_STR(sd->customData[0].second, std::string("3"), "sd.customData[0] quest_progress");
        }
    }

    // --- Flower ---
    std::printf("Checking Flower Components...\n");
    {
        auto e = findEntity("FlowerEntity");
        HAS_COMPONENT(dstWorld, e, ECS::JellyMeshComponent, "JellyMeshComponent");
        auto* jm = dstWorld.GetComponent<ECS::JellyMeshComponent>(e);
        if (jm) {
            CHECK_FLOAT(jm->springStiffness, 120.0f, "jm.springStiffness");
            CHECK_FLOAT(jm->damping, 8.0f, "jm.damping");
            CHECK_FLOAT(jm->maxStretch, 0.5f, "jm.maxStretch");
            CHECK_BOOL(jm->initialized, false, "jm.initialized (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::TetherComponent, "TetherComponent");
        auto* te = dstWorld.GetComponent<ECS::TetherComponent>(e);
        if (te) {
            CHECK_VEC3(te->attachLocalPos, 0, 1, 0, "te.attachLocalPos");
            CHECK_FLOAT(te->breakDistance, 3.0f, "te.breakDistance");
            CHECK_FLOAT(te->tensionRamp, 3.0f, "te.tensionRamp");
            CHECK_FLOAT(te->autoMass, 0.5f, "te.autoMass");
            CHECK_FLOAT(te->autoSpringK, 200.0f, "te.autoSpringK");
            CHECK_FLOAT(te->autoDamping, 12.0f, "te.autoDamping");
            CHECK_FLOAT(te->autoBreakForce, 60.0f, "te.autoBreakForce");
            CHECK_FLOAT(te->autoDrag, 2.0f, "te.autoDrag");
            CHECK_BOOL(te->isBroken, false, "te.isBroken (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::GrabbableComponent, "GrabbableComponent");
        auto* gr = dstWorld.GetComponent<ECS::GrabbableComponent>(e);
        if (gr) {
            CHECK_FLOAT(gr->pullForce, 20.0f, "gr.pullForce");
            CHECK_FLOAT(gr->grabRadius, 1.0f, "gr.grabRadius");
            CHECK_FLOAT(gr->maxPullDistance, 3.0f, "gr.maxPullDistance");
            CHECK_FLOAT(gr->maxVelocity, 75.0f, "gr.maxVelocity");
            CHECK_FLOAT(gr->windSwayScale, 0.25f, "gr.windSwayScale");
            CHECK_BOOL(gr->isGrabbed, false, "gr.isGrabbed (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::FlowerStemComponent, "FlowerStemComponent");
        auto* fs = dstWorld.GetComponent<ECS::FlowerStemComponent>(e);
        if (fs) {
            CHECK_FLOAT(fs->healthyBonus, 15.0f, "fs.healthyBonus");
            CHECK_FLOAT(fs->witheredPenalty, 8.0f, "fs.witheredPenalty");
            CHECK_FLOAT(fs->groundLevel, -0.5f, "fs.groundLevel");
            CHECK_VEC3(fs->sapColor, 0.2f, 0.6f, 0.1f, "fs.sapColor");
            CHECK_FLOAT(fs->stemSwayAmplitude, 0.1f, "fs.stemSwayAmplitude");
            CHECK_BOOL(fs->evaluated, false, "fs.evaluated (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::FlowerParticleConfigComponent, "FlowerParticleConfigComponent");
        auto* fpc = dstWorld.GetComponent<ECS::FlowerParticleConfigComponent>(e);
        if (fpc) {
            CHECK_EQ(fpc->breakBurstCount, 25, "fpc.breakBurstCount");
            CHECK_FLOAT(fpc->breakBurstSpeed, 3.0f, "fpc.breakBurstSpeed");
            CHECK_FLOAT(fpc->breakBurstUpKick, 2.5f, "fpc.breakBurstUpKick");
            CHECK_FLOAT(fpc->breakBurstLifetime, 1.0f, "fpc.breakBurstLifetime");
            CHECK_FLOAT(fpc->breakBurstScale, 0.1f, "fpc.breakBurstScale");
            CHECK_EQ(fpc->breakDripCount, 10, "fpc.breakDripCount");
            CHECK_FLOAT(fpc->breakDripSpeed, 0.7f, "fpc.breakDripSpeed");
            CHECK_FLOAT(fpc->breakDripLifetime, 1.5f, "fpc.breakDripLifetime");
            CHECK_EQ(fpc->splashCount, 15, "fpc.splashCount");
            CHECK_FLOAT(fpc->splashSpeed, 2.0f, "fpc.splashSpeed");
            CHECK_FLOAT(fpc->splashUpKick, 3.0f, "fpc.splashUpKick");
            CHECK_FLOAT(fpc->splashLifetime, 0.8f, "fpc.splashLifetime");
            CHECK_FLOAT(fpc->tensionDripRate, 4.0f, "fpc.tensionDripRate");
            CHECK_FLOAT(fpc->tensionDripThreshold, 0.2f, "fpc.tensionDripThreshold");
            CHECK_FLOAT(fpc->tensionSquirtSpeed, 3.0f, "fpc.tensionSquirtSpeed");
            CHECK_FLOAT(fpc->particleGravity, 12.0f, "fpc.particleGravity");
        }
    }

    // --- LOD, Grass, Vegetation ---
    std::printf("Checking LOD, Grass, Vegetation...\n");
    {
        auto e = findEntity("LODEntity");
        HAS_COMPONENT(dstWorld, e, ECS::LODComponent, "LODComponent");
        auto* lod = dstWorld.GetComponent<ECS::LODComponent>(e);
        if (lod) {
            CHECK_EQ(lod->levelCount, 2, "lod.levelCount");
            CHECK_FLOAT(lod->baseDistance, 15.0f, "lod.baseDistance");
            CHECK_FLOAT(lod->distanceMultiplier, 3.0f, "lod.distanceMultiplier");
            CHECK_BOOL(lod->autoGenerated, true, "lod.autoGenerated");
            CHECK_FLOAT(lod->reductionRatios[1], 0.4f, "lod.reductionRatios[1]");
            CHECK_EQ(lod->levels[0].mesh.vertices.size(), (size_t)3, "lod.levels[0] vertex count");
            CHECK_EQ(lod->levels[0].mesh.indices.size(), (size_t)3, "lod.levels[0] index count");
            CHECK_EQ(lod->levels[1].mesh.vertices.size(), (size_t)2, "lod.levels[1] vertex count");
            CHECK_FLOAT(lod->levels[1].maxDistance, 45.0f, "lod.levels[1].maxDistance");
            CHECK_EQ(lod->activeLOD, 0, "lod.activeLOD (runtime reset)");
        }
        HAS_COMPONENT(dstWorld, e, ECS::GrassVolumeComponent, "GrassVolumeComponent");
        auto* gv = dstWorld.GetComponent<ECS::GrassVolumeComponent>(e);
        if (gv) {
            CHECK_VEC3(gv->halfExtents, 20, 0, 20, "gv.halfExtents");
            CHECK_EQ(gv->density, 10000u, "gv.density");
            CHECK_FLOAT(gv->bladeHeight, 0.5f, "gv.bladeHeight");
            CHECK_FLOAT(gv->windSwayStrength, 2.0f, "gv.windSwayStrength");
        }
        HAS_COMPONENT(dstWorld, e, ECS::VegetationComponent, "VegetationComponent");
        auto* veg = dstWorld.GetComponent<ECS::VegetationComponent>(e);
        if (veg) {
            CHECK_FLOAT(veg->swayStrength, 1.5f, "veg.swayStrength");
            CHECK_FLOAT(veg->swayFrequency, 0.8f, "veg.swayFrequency");
            CHECK_BOOL(veg->useVertexColorWeight, false, "veg.useVertexColorWeight");
        }
    }

    // ---------- Summary ----------
    std::printf("\n=== Results: %d checks, %d failures ===\n", g_Checks, g_Failures);
    if (g_Failures == 0) {
        std::printf("ALL PASSED\n");
    } else {
        std::printf("SOME TESTS FAILED\n");
    }
    return g_Failures > 0 ? 1 : 0;
}
