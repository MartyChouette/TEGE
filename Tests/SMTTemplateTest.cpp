// SMT Dungeon Template round-trip test
// Replicates the full dungeon template (walls, doors, encounters, stairs,
// treasure, torches, lights, player controller, HUD), serializes to JSON,
// reloads into a fresh world, and verifies all components survived.

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>

using namespace Enjin;

static int g_Failures = 0;
static int g_Checks = 0;

#define CHECK_EQ(actual, expected, label) do { \
    ++g_Checks; \
    if ((actual) != (expected)) { \
        std::printf("  FAIL: %s  expected %lld got %lld\n", label, (long long)(expected), (long long)(actual)); \
        ++g_Failures; \
    } \
} while(0)

#define CHECK_FLOAT(actual, expected, label) do { \
    ++g_Checks; \
    if (std::fabs((actual) - (expected)) > 0.01f) { \
        std::printf("  FAIL: %s  expected %.4f got %.4f\n", label, (double)(expected), (double)(actual)); \
        ++g_Failures; \
    } \
} while(0)

#define CHECK_VEC3(actual, ex, ey, ez, label) do { \
    CHECK_FLOAT((actual).x, ex, label " .x"); \
    CHECK_FLOAT((actual).y, ey, label " .y"); \
    CHECK_FLOAT((actual).z, ez, label " .z"); \
} while(0)

#define CHECK_STR(actual, expected, label) do { \
    ++g_Checks; \
    if ((actual) != std::string(expected)) { \
        std::printf("  FAIL: %s  expected \"%s\" got \"%s\"\n", label, expected, (actual).c_str()); \
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
        std::printf("  FAIL: %s  component missing\n", label); \
        ++g_Failures; \
    } \
} while(0)

// Helper to find entity by name in a world
ECS::Entity FindEntity(ECS::World& world, const std::string& name) {
    for (ECS::Entity e : world.GetAllEntities()) {
        auto* nc = world.GetComponent<ECS::NameComponent>(e);
        if (nc && nc->name == name) return e;
    }
    return ECS::INVALID_ENTITY;
}

// Count entities whose name starts with a prefix
int CountEntitiesWithPrefix(ECS::World& world, const std::string& prefix) {
    int count = 0;
    for (ECS::Entity e : world.GetAllEntities()) {
        auto* nc = world.GetComponent<ECS::NameComponent>(e);
        if (nc && nc->name.substr(0, prefix.size()) == prefix) count++;
    }
    return count;
}

// Build the full SMT dungeon template into a world
void BuildSMTDungeon(ECS::World& world) {
    const f32 CELL = 3.0f;
    const f32 WALL_H = 3.0f;
    const f32 WALL_THICK = 0.15f;

    const int GRID = 8;
    int layout[GRID][GRID] = {
        {0,0,0,0,0,0,0,0},
        {0,1,1,1,0,1,1,0},
        {0,1,0,1,0,1,3,0},
        {0,1,0,2,1,1,1,0},
        {0,1,0,1,0,0,1,0},
        {0,1,1,1,1,3,1,0},
        {0,5,0,0,1,1,4,0},
        {0,0,0,0,0,0,0,0},
    };

    Math::Vector3 floorColor(0.15f, 0.15f, 0.18f);
    Math::Vector3 ceilingColor(0.12f, 0.12f, 0.14f);
    Math::Vector3 wallColor(0.22f, 0.20f, 0.25f);
    Math::Vector3 wallAccent(0.28f, 0.18f, 0.18f);
    Math::Vector3 doorColor(0.45f, 0.30f, 0.15f);
    Math::Vector3 encounterColor(0.6f, 0.1f, 0.1f);
    Math::Vector3 stairsColor(0.5f, 0.5f, 0.55f);
    Math::Vector3 treasureColor(0.8f, 0.7f, 0.2f);

    int wallIdx = 0;
    int floorIdx = 0;
    char nameBuf[64];

    auto makeWall = [&](const char* name, Math::Vector3 pos, Math::Vector3 scale, Math::Vector3 color, bool addCollider = false) {
        ECS::Entity wall = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(wall, name);
        auto& wt = world.AddComponent<ECS::TransformComponent>(wall);
        wt.position = pos;
        wt.scale = scale;
        auto& wmat = world.AddComponent<ECS::MaterialComponent>(wall);
        wmat.baseColor = color;
        wmat.roughness = 0.85f;
        // Skip MeshComponent (requires renderer) - serialization doesn't need mesh data
        if (addCollider) {
            auto& col = world.AddComponent<ECS::BoxColliderComponent>(wall);
            col.size = Math::Vector3(1.0f, 1.0f, 1.0f);
        }
        return wall;
    };

    for (int z = 0; z < GRID; ++z) {
        for (int x = 0; x < GRID; ++x) {
            if (layout[z][x] == 0) continue;

            f32 cx = static_cast<f32>(x) * CELL + CELL * 0.5f;
            f32 cz = static_cast<f32>(z) * CELL + CELL * 0.5f;

            // Floor
            snprintf(nameBuf, sizeof(nameBuf), "Floor_%d", floorIdx++);
            makeWall(nameBuf, Math::Vector3(cx, 0.0f, cz), Math::Vector3(CELL, 0.1f, CELL), floorColor);

            // Ceiling
            snprintf(nameBuf, sizeof(nameBuf), "Ceiling_%d", floorIdx);
            makeWall(nameBuf, Math::Vector3(cx, WALL_H, cz), Math::Vector3(CELL, 0.1f, CELL), ceilingColor);

            // Special cells
            if (layout[z][x] == 2) {
                snprintf(nameBuf, sizeof(nameBuf), "Door_%d_%d", x, z);
                ECS::Entity door = makeWall(nameBuf,
                    Math::Vector3(cx, WALL_H * 0.4f, cz),
                    Math::Vector3(CELL * 0.7f, WALL_H * 0.8f, WALL_THICK * 2.0f),
                    doorColor);
                auto& dtag = world.AddComponent<ECS::TagComponent>(door);
                dtag.tags.push_back("door");
                auto& interact = world.AddComponent<ECS::InteractableComponent>(door);
                interact.promptText = "Open Door";
                interact.interactionRange = CELL;
            }
            else if (layout[z][x] == 3) {
                snprintf(nameBuf, sizeof(nameBuf), "Encounter_%d_%d", x, z);
                ECS::Entity enc = makeWall(nameBuf,
                    Math::Vector3(cx, 0.06f, cz),
                    Math::Vector3(CELL * 0.6f, 0.02f, CELL * 0.6f),
                    encounterColor);
                auto& etag = world.AddComponent<ECS::TagComponent>(enc);
                etag.tags.push_back("encounter_zone");
                auto& trigger = world.AddComponent<ECS::TriggerZoneComponent>(enc);
                trigger.shape = ECS::TriggerZoneComponent::Shape::Sphere;
                trigger.sphereRadius = CELL * 0.5f;
                trigger.triggerOnce = false;
            }
            else if (layout[z][x] == 4) {
                snprintf(nameBuf, sizeof(nameBuf), "Stairs_%d_%d", x, z);
                ECS::Entity stairs = world.CreateEntity();
                world.AddComponent<ECS::NameComponent>(stairs, nameBuf);
                auto& st = world.AddComponent<ECS::TransformComponent>(stairs);
                st.position = Math::Vector3(cx, 0.3f, cz);
                st.scale = Math::Vector3(CELL * 0.5f, 0.6f, CELL * 0.5f);
                st.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-20.0f));
                auto& smat = world.AddComponent<ECS::MaterialComponent>(stairs);
                smat.baseColor = stairsColor;
                smat.roughness = 0.7f;
                auto& stag = world.AddComponent<ECS::TagComponent>(stairs);
                stag.tags.push_back("stairs_down");
                auto& sinteract = world.AddComponent<ECS::InteractableComponent>(stairs);
                sinteract.promptText = "Descend";
                sinteract.interactionRange = CELL;
            }
            else if (layout[z][x] == 5) {
                snprintf(nameBuf, sizeof(nameBuf), "Treasure_%d_%d", x, z);
                ECS::Entity chest = world.CreateEntity();
                world.AddComponent<ECS::NameComponent>(chest, nameBuf);
                auto& ct = world.AddComponent<ECS::TransformComponent>(chest);
                ct.position = Math::Vector3(cx, 0.3f, cz);
                ct.scale = Math::Vector3(0.8f, 0.6f, 0.5f);
                auto& cmat = world.AddComponent<ECS::MaterialComponent>(chest);
                cmat.baseColor = treasureColor;
                cmat.roughness = 0.4f;
                cmat.metallic = 0.6f;
                auto& ctag = world.AddComponent<ECS::TagComponent>(chest);
                ctag.tags.push_back("treasure");
                auto& cinteract = world.AddComponent<ECS::InteractableComponent>(chest);
                cinteract.promptText = "Open Chest";
                cinteract.interactionRange = CELL * 0.8f;
                cinteract.singleUse = true;
            }

            // Walls on edges
            if (z == 0 || layout[z - 1][x] == 0) {
                snprintf(nameBuf, sizeof(nameBuf), "Wall_N_%d", wallIdx++);
                bool accent = (wallIdx % 5 == 0);
                makeWall(nameBuf,
                    Math::Vector3(cx, WALL_H * 0.5f, cz - CELL * 0.5f),
                    Math::Vector3(CELL, WALL_H, WALL_THICK),
                    accent ? wallAccent : wallColor, true);
            }
            if (z == GRID - 1 || layout[z + 1][x] == 0) {
                snprintf(nameBuf, sizeof(nameBuf), "Wall_S_%d", wallIdx++);
                bool accent = (wallIdx % 5 == 0);
                makeWall(nameBuf,
                    Math::Vector3(cx, WALL_H * 0.5f, cz + CELL * 0.5f),
                    Math::Vector3(CELL, WALL_H, WALL_THICK),
                    accent ? wallAccent : wallColor, true);
            }
            if (x == 0 || layout[z][x - 1] == 0) {
                snprintf(nameBuf, sizeof(nameBuf), "Wall_W_%d", wallIdx++);
                bool accent = (wallIdx % 5 == 0);
                makeWall(nameBuf,
                    Math::Vector3(cx - CELL * 0.5f, WALL_H * 0.5f, cz),
                    Math::Vector3(WALL_THICK, WALL_H, CELL),
                    accent ? wallAccent : wallColor, true);
            }
            if (x == GRID - 1 || layout[z][x + 1] == 0) {
                snprintf(nameBuf, sizeof(nameBuf), "Wall_E_%d", wallIdx++);
                bool accent = (wallIdx % 5 == 0);
                makeWall(nameBuf,
                    Math::Vector3(cx + CELL * 0.5f, WALL_H * 0.5f, cz),
                    Math::Vector3(WALL_THICK, WALL_H, CELL),
                    accent ? wallAccent : wallColor, true);
            }
        }
    }

    // Torches (point lights)
    int torchIdx = 0;
    int torchCells[][2] = {{1,1}, {3,3}, {1,5}, {5,5}, {3,1}, {6,5}};
    for (auto& tc : torchCells) {
        int tx = tc[0], tz = tc[1];
        if (layout[tz][tx] == 0) continue;
        f32 cx = static_cast<f32>(tx) * CELL + CELL * 0.5f;
        f32 cz = static_cast<f32>(tz) * CELL + CELL * 0.5f;

        snprintf(nameBuf, sizeof(nameBuf), "Torch_%d", torchIdx++);
        ECS::Entity torch = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(torch, nameBuf);
        auto& tt = world.AddComponent<ECS::TransformComponent>(torch);
        tt.position = Math::Vector3(cx + 1.2f, WALL_H * 0.7f, cz);
        tt.scale = Math::Vector3(0.1f, 0.3f, 0.1f);
        auto& tmat = world.AddComponent<ECS::MaterialComponent>(torch);
        tmat.baseColor = Math::Vector3(0.8f, 0.5f, 0.1f);
        tmat.emissiveColor = Math::Vector3(1.0f, 0.6f, 0.2f);
        tmat.emissiveStrength = 2.0f;
        auto& lc = world.AddComponent<ECS::LightComponent>(torch);
        lc.type = ECS::LightType::Point;
        lc.color = Math::Vector3(1.0f, 0.65f, 0.3f);
        lc.intensity = 2.5f;
        lc.range = CELL * 2.5f;
        lc.castShadows = true;
    }

    // Ambient directional light
    {
        ECS::Entity ambLight = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(ambLight, "Ambient Light");
        auto& lt = world.AddComponent<ECS::TransformComponent>(ambLight);
        lt.rotation = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(-70.0f));
        auto& lc = world.AddComponent<ECS::LightComponent>(ambLight);
        lc.type = ECS::LightType::Directional;
        lc.color = Math::Vector3(0.15f, 0.15f, 0.25f);
        lc.intensity = 0.3f;
        lc.castShadows = false;
    }

    // Player with FPS controller
    {
        Math::Vector3 playerStart(1.0f * CELL + CELL * 0.5f, 0.0f, 1.0f * CELL + CELL * 0.5f);
        ECS::Entity player = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(player, "Player");
        auto& pt = world.AddComponent<ECS::TransformComponent>(player);
        pt.position = playerStart;

        auto& fps = world.AddComponent<ECS::FirstPersonController>(player);
        fps.moveSpeed = 4.0f;
        fps.mouseSensitivity = 0.15f;
        fps.gridMovement = true;
        fps.gridCellSize = CELL;
        fps.gridOrigin = Math::Vector3(CELL * 0.5f, 0.0f, CELL * 0.5f);
        fps.standingHeight = 1.5f;
        fps.currentHeight = 1.5f;
        fps.dungeonCrawlerMode = true;
        fps.snapTurnAngle = 90.0f;
        fps.yaw = 180.0f;
        fps.disableMouseLook = false;
    }

    // Game camera
    {
        ECS::Entity cam = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(cam, "Game Camera");
        auto& ct = world.AddComponent<ECS::TransformComponent>(cam);
        ct.position = Math::Vector3(4.5f, 1.5f, 4.5f);
        auto& cc = world.AddComponent<ECS::CameraComponent>(cam);
        cc.fieldOfView = 70.0f;
        cc.nearPlane = 0.1f;
        cc.farPlane = 50.0f;
        cc.isActive = true;
    }

    // HUD: Party stats
    {
        ECS::Entity partyHud = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(partyHud, "Party HUD");
        auto& ht = world.AddComponent<ECS::TransformComponent>(partyHud);
        ht.position = Math::Vector3(-3.0f, 4.5f, 0.0f);
        auto& htext = world.AddComponent<ECS::TextComponent>(partyHud);
        htext.text = "=== PARTY ===\nHero Lv12 HP 142/142";
        htext.fontSize = 16.0f;
        htext.textColor = Math::Vector3(0.0f, 1.0f, 0.4f);
        auto& htag = world.AddComponent<ECS::TagComponent>(partyHud);
        htag.tags.push_back("party_hud");
    }

    // HUD: Compass
    {
        ECS::Entity compass = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(compass, "Compass");
        auto& cpt = world.AddComponent<ECS::TransformComponent>(compass);
        cpt.position = Math::Vector3(3.5f, 4.5f, 0.0f);
        auto& ctext = world.AddComponent<ECS::TextComponent>(compass);
        ctext.text = "B1F  N\nW + E\n    S";
        ctext.fontSize = 18.0f;
        ctext.textColor = Math::Vector3(0.7f, 0.7f, 1.0f);
        auto& ctag = world.AddComponent<ECS::TagComponent>(compass);
        ctag.tags.push_back("compass_display");
    }

    // HUD: Message box
    {
        ECS::Entity msgBox = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(msgBox, "Message Box");
        auto& mt = world.AddComponent<ECS::TransformComponent>(msgBox);
        mt.position = Math::Vector3(0.0f, 0.5f, 0.0f);
        auto& mtext = world.AddComponent<ECS::TextComponent>(msgBox);
        mtext.text = "You enter the dungeon...\nThe air is thick with dread.";
        mtext.fontSize = 18.0f;
        mtext.textColor = Math::Vector3(0.9f, 0.9f, 0.9f);
        auto& mtag = world.AddComponent<ECS::TagComponent>(msgBox);
        mtag.tags.push_back("message_box");
    }
}

int main() {
    std::printf("=== SMT Dungeon Template Round-Trip Test ===\n\n");

    // ========================================
    // Phase 1: Build the template
    // ========================================
    std::printf("--- Phase 1: Build Template ---\n");
    ECS::World srcWorld;
    BuildSMTDungeon(srcWorld);

    int srcEntityCount = 0;
    for ([[maybe_unused]] ECS::Entity e : srcWorld.GetAllEntities()) srcEntityCount++;
    std::printf("  Source entities: %d\n", srcEntityCount);

    int srcFloors = CountEntitiesWithPrefix(srcWorld, "Floor_");
    int srcWalls = CountEntitiesWithPrefix(srcWorld, "Wall_");
    int srcTorches = CountEntitiesWithPrefix(srcWorld, "Torch_");
    std::printf("  Floors: %d, Walls: %d, Torches: %d\n", srcFloors, srcWalls, srcTorches);

    // ========================================
    // Phase 2: Serialize
    // ========================================
    std::printf("\n--- Phase 2: Serialize ---\n");
    Scene::SceneSerializer serializer(&srcWorld);
    std::string json = serializer.SaveToString();

    ++g_Checks;
    if (json.empty()) {
        std::printf("  FAIL: SaveToString returned empty\n");
        ++g_Failures;
    } else {
        std::printf("  JSON size: %d bytes\n", (int)json.size());
    }

    // ========================================
    // Phase 3: Deserialize into fresh world
    // ========================================
    std::printf("\n--- Phase 3: Deserialize ---\n");
    ECS::World dstWorld;
    Scene::SceneSerializer loader(&dstWorld);
    auto result = loader.LoadFromString(json);
    CHECK_BOOL(result.success, true, "LoadFromString");

    int dstEntityCount = 0;
    for ([[maybe_unused]] ECS::Entity e : dstWorld.GetAllEntities()) dstEntityCount++;
    std::printf("  Loaded entities: %d\n", dstEntityCount);
    CHECK_EQ(dstEntityCount, srcEntityCount, "Entity count matches");

    // ========================================
    // Phase 4: Verify entity counts by category
    // ========================================
    std::printf("\n--- Phase 4: Entity Counts ---\n");
    int dstFloors = CountEntitiesWithPrefix(dstWorld, "Floor_");
    int dstWalls = CountEntitiesWithPrefix(dstWorld, "Wall_");
    int dstTorches = CountEntitiesWithPrefix(dstWorld, "Torch_");

    CHECK_EQ(dstFloors, srcFloors, "Floor count");
    CHECK_EQ(dstWalls, srcWalls, "Wall count");
    CHECK_EQ(dstTorches, srcTorches, "Torch count");
    std::printf("  Floors: %d, Walls: %d, Torches: %d\n", dstFloors, dstWalls, dstTorches);

    // ========================================
    // Phase 5: Verify Player controller
    // ========================================
    std::printf("\n--- Phase 5: Player Controller ---\n");
    {
        ECS::Entity player = FindEntity(dstWorld, "Player");
        ++g_Checks;
        if (player == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Player entity not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, player, ECS::FirstPersonController, "Player.FPSController");
            auto* fps = dstWorld.GetComponent<ECS::FirstPersonController>(player);
            if (fps) {
                CHECK_BOOL(fps->dungeonCrawlerMode, true, "fps.dungeonCrawlerMode");
                CHECK_FLOAT(fps->snapTurnAngle, 90.0f, "fps.snapTurnAngle");
                CHECK_BOOL(fps->gridMovement, true, "fps.gridMovement");
                CHECK_FLOAT(fps->gridCellSize, 3.0f, "fps.gridCellSize");
                CHECK_FLOAT(fps->mouseSensitivity, 0.15f, "fps.mouseSensitivity");
                CHECK_FLOAT(fps->standingHeight, 1.5f, "fps.standingHeight");
                CHECK_FLOAT(fps->moveSpeed, 4.0f, "fps.moveSpeed");
                CHECK_BOOL(fps->disableMouseLook, false, "fps.disableMouseLook");
                // Runtime state should reset
                CHECK_BOOL(fps->snapTurnPending, false, "fps.snapTurnPending (runtime reset)");
                CHECK_BOOL(fps->gridMoving, false, "fps.gridMoving (runtime reset)");
            }

            HAS_COMPONENT(dstWorld, player, ECS::TransformComponent, "Player.Transform");
            auto* pt = dstWorld.GetComponent<ECS::TransformComponent>(player);
            if (pt) {
                CHECK_FLOAT(pt->position.x, 4.5f, "player pos.x");
                CHECK_FLOAT(pt->position.z, 4.5f, "player pos.z");
            }
        }
    }

    // ========================================
    // Phase 6: Verify Game Camera
    // ========================================
    std::printf("\n--- Phase 6: Game Camera ---\n");
    {
        ECS::Entity cam = FindEntity(dstWorld, "Game Camera");
        ++g_Checks;
        if (cam == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Game Camera entity not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, cam, ECS::CameraComponent, "Camera.CameraComponent");
            auto* cc = dstWorld.GetComponent<ECS::CameraComponent>(cam);
            if (cc) {
                CHECK_FLOAT(cc->fieldOfView, 70.0f, "camera.fov");
                CHECK_FLOAT(cc->nearPlane, 0.1f, "camera.near");
                CHECK_FLOAT(cc->farPlane, 50.0f, "camera.far");
                CHECK_BOOL(cc->isActive, true, "camera.isActive");
            }
        }
    }

    // ========================================
    // Phase 7: Verify Door (InteractableComponent + TagComponent)
    // ========================================
    std::printf("\n--- Phase 7: Door ---\n");
    {
        ECS::Entity door = FindEntity(dstWorld, "Door_3_3");
        ++g_Checks;
        if (door == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Door_3_3 entity not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, door, ECS::InteractableComponent, "Door.Interactable");
            auto* inter = dstWorld.GetComponent<ECS::InteractableComponent>(door);
            if (inter) {
                CHECK_STR(inter->promptText, "Open Door", "door.promptText");
                CHECK_FLOAT(inter->interactionRange, 3.0f, "door.interactionRange");
            }
            HAS_COMPONENT(dstWorld, door, ECS::TagComponent, "Door.Tag");
            auto* tag = dstWorld.GetComponent<ECS::TagComponent>(door);
            if (tag) {
                ++g_Checks;
                if (tag->tags.empty() || tag->tags[0] != "door") {
                    std::printf("  FAIL: door tag expected 'door' got '%s'\n",
                        tag->tags.empty() ? "(empty)" : tag->tags[0].c_str());
                    ++g_Failures;
                }
            }
            HAS_COMPONENT(dstWorld, door, ECS::MaterialComponent, "Door.Material");
            auto* mat = dstWorld.GetComponent<ECS::MaterialComponent>(door);
            if (mat) {
                CHECK_VEC3(mat->baseColor, 0.45f, 0.30f, 0.15f, "door.color");
            }
        }
    }

    // ========================================
    // Phase 8: Verify Encounter Zone (TriggerZoneComponent)
    // ========================================
    std::printf("\n--- Phase 8: Encounter Zone ---\n");
    {
        ECS::Entity enc = FindEntity(dstWorld, "Encounter_6_2");
        ++g_Checks;
        if (enc == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Encounter_6_2 entity not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, enc, ECS::TriggerZoneComponent, "Encounter.TriggerZone");
            auto* tz = dstWorld.GetComponent<ECS::TriggerZoneComponent>(enc);
            if (tz) {
                CHECK_EQ((int)tz->shape, (int)ECS::TriggerZoneComponent::Shape::Sphere, "enc.shape");
                CHECK_FLOAT(tz->sphereRadius, 1.5f, "enc.sphereRadius");
                CHECK_BOOL(tz->triggerOnce, false, "enc.triggerOnce");
            }
            HAS_COMPONENT(dstWorld, enc, ECS::TagComponent, "Encounter.Tag");
            auto* tag = dstWorld.GetComponent<ECS::TagComponent>(enc);
            if (tag) {
                ++g_Checks;
                if (tag->tags.empty() || tag->tags[0] != "encounter_zone") {
                    std::printf("  FAIL: encounter tag expected 'encounter_zone'\n");
                    ++g_Failures;
                }
            }
        }
    }

    // ========================================
    // Phase 9: Verify Stairs (rotation + interactable)
    // ========================================
    std::printf("\n--- Phase 9: Stairs ---\n");
    {
        ECS::Entity stairs = FindEntity(dstWorld, "Stairs_6_6");
        ++g_Checks;
        if (stairs == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Stairs_6_6 entity not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, stairs, ECS::InteractableComponent, "Stairs.Interactable");
            auto* inter = dstWorld.GetComponent<ECS::InteractableComponent>(stairs);
            if (inter) {
                CHECK_STR(inter->promptText, "Descend", "stairs.promptText");
            }
            HAS_COMPONENT(dstWorld, stairs, ECS::TagComponent, "Stairs.Tag");
            auto* tag = dstWorld.GetComponent<ECS::TagComponent>(stairs);
            if (tag) {
                ++g_Checks;
                if (tag->tags.empty() || tag->tags[0] != "stairs_down") {
                    std::printf("  FAIL: stairs tag expected 'stairs_down'\n");
                    ++g_Failures;
                }
            }
            HAS_COMPONENT(dstWorld, stairs, ECS::TransformComponent, "Stairs.Transform");
            auto* st = dstWorld.GetComponent<ECS::TransformComponent>(stairs);
            if (st) {
                CHECK_FLOAT(st->scale.x, 1.5f, "stairs.scale.x");
                CHECK_FLOAT(st->scale.y, 0.6f, "stairs.scale.y");
            }
        }
    }

    // ========================================
    // Phase 10: Verify Treasure (singleUse + metallic)
    // ========================================
    std::printf("\n--- Phase 10: Treasure ---\n");
    {
        ECS::Entity chest = FindEntity(dstWorld, "Treasure_1_6");
        ++g_Checks;
        if (chest == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Treasure_1_6 entity not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, chest, ECS::InteractableComponent, "Treasure.Interactable");
            auto* inter = dstWorld.GetComponent<ECS::InteractableComponent>(chest);
            if (inter) {
                CHECK_STR(inter->promptText, "Open Chest", "chest.promptText");
                CHECK_BOOL(inter->singleUse, true, "chest.singleUse");
            }
            HAS_COMPONENT(dstWorld, chest, ECS::MaterialComponent, "Treasure.Material");
            auto* mat = dstWorld.GetComponent<ECS::MaterialComponent>(chest);
            if (mat) {
                CHECK_FLOAT(mat->metallic, 0.6f, "chest.metallic");
                CHECK_VEC3(mat->baseColor, 0.8f, 0.7f, 0.2f, "chest.color");
            }
        }
    }

    // ========================================
    // Phase 11: Verify Torches (LightComponent)
    // ========================================
    std::printf("\n--- Phase 11: Torches ---\n");
    {
        ECS::Entity torch = FindEntity(dstWorld, "Torch_0");
        ++g_Checks;
        if (torch == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Torch_0 entity not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, torch, ECS::LightComponent, "Torch.Light");
            auto* lc = dstWorld.GetComponent<ECS::LightComponent>(torch);
            if (lc) {
                CHECK_EQ((int)lc->type, (int)ECS::LightType::Point, "torch.lightType");
                CHECK_FLOAT(lc->intensity, 2.5f, "torch.intensity");
                CHECK_FLOAT(lc->range, 7.5f, "torch.range");
                CHECK_BOOL(lc->castShadows, true, "torch.castShadows");
                CHECK_VEC3(lc->color, 1.0f, 0.65f, 0.3f, "torch.color");
            }
            HAS_COMPONENT(dstWorld, torch, ECS::MaterialComponent, "Torch.Material");
            auto* mat = dstWorld.GetComponent<ECS::MaterialComponent>(torch);
            if (mat) {
                CHECK_FLOAT(mat->emissiveStrength, 2.0f, "torch.emissiveStrength");
            }
        }
    }

    // ========================================
    // Phase 12: Verify Ambient Light
    // ========================================
    std::printf("\n--- Phase 12: Ambient Light ---\n");
    {
        ECS::Entity amb = FindEntity(dstWorld, "Ambient Light");
        ++g_Checks;
        if (amb == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Ambient Light entity not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, amb, ECS::LightComponent, "Ambient.Light");
            auto* lc = dstWorld.GetComponent<ECS::LightComponent>(amb);
            if (lc) {
                CHECK_EQ((int)lc->type, (int)ECS::LightType::Directional, "ambient.lightType");
                CHECK_FLOAT(lc->intensity, 0.3f, "ambient.intensity");
                CHECK_BOOL(lc->castShadows, false, "ambient.castShadows");
            }
        }
    }

    // ========================================
    // Phase 13: Verify walls have BoxColliders
    // ========================================
    std::printf("\n--- Phase 13: Wall Colliders ---\n");
    {
        int wallsTotal = 0;
        int wallsWithCollider = 0;
        for (ECS::Entity e : dstWorld.GetAllEntities()) {
            auto* nc = dstWorld.GetComponent<ECS::NameComponent>(e);
            if (nc && nc->name.find("Wall_") == 0) {
                wallsTotal++;
                if (dstWorld.HasComponent<ECS::BoxColliderComponent>(e)) {
                    wallsWithCollider++;
                }
            }
        }
        std::printf("  Walls: %d, With colliders: %d\n", wallsTotal, wallsWithCollider);
        CHECK_EQ(wallsWithCollider, wallsTotal, "All walls have BoxCollider");
    }

    // ========================================
    // Phase 14: Verify HUD Text entities
    // ========================================
    std::printf("\n--- Phase 14: HUD Text ---\n");
    {
        ECS::Entity partyHud = FindEntity(dstWorld, "Party HUD");
        ++g_Checks;
        if (partyHud == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Party HUD not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, partyHud, ECS::TextComponent, "PartyHUD.Text");
            auto* txt = dstWorld.GetComponent<ECS::TextComponent>(partyHud);
            if (txt) {
                CHECK_FLOAT(txt->fontSize, 16.0f, "partyHud.fontSize");
                CHECK_VEC3(txt->textColor, 0.0f, 1.0f, 0.4f, "partyHud.textColor");
                ++g_Checks;
                if (txt->text.find("PARTY") == std::string::npos) {
                    std::printf("  FAIL: partyHud text doesn't contain 'PARTY'\n");
                    ++g_Failures;
                }
            }
            HAS_COMPONENT(dstWorld, partyHud, ECS::TagComponent, "PartyHUD.Tag");
            auto* tag = dstWorld.GetComponent<ECS::TagComponent>(partyHud);
            if (tag) {
                ++g_Checks;
                if (tag->tags.empty() || tag->tags[0] != "party_hud") {
                    std::printf("  FAIL: partyHud tag expected 'party_hud'\n");
                    ++g_Failures;
                }
            }
        }

        ECS::Entity compass = FindEntity(dstWorld, "Compass");
        ++g_Checks;
        if (compass == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Compass not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, compass, ECS::TextComponent, "Compass.Text");
            HAS_COMPONENT(dstWorld, compass, ECS::TagComponent, "Compass.Tag");
        }

        ECS::Entity msgBox = FindEntity(dstWorld, "Message Box");
        ++g_Checks;
        if (msgBox == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Message Box not found\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, msgBox, ECS::TextComponent, "MsgBox.Text");
            auto* txt = dstWorld.GetComponent<ECS::TextComponent>(msgBox);
            if (txt) {
                ++g_Checks;
                if (txt->text.find("dungeon") == std::string::npos) {
                    std::printf("  FAIL: msgBox text doesn't contain 'dungeon'\n");
                    ++g_Failures;
                }
            }
        }
    }

    // ========================================
    // Phase 15: Second round-trip (save loaded world, reload again)
    // ========================================
    std::printf("\n--- Phase 15: Double Round-Trip ---\n");
    {
        Scene::SceneSerializer ser2(&dstWorld);
        std::string json2 = ser2.SaveToString();
        ++g_Checks;
        if (json2.empty()) {
            std::printf("  FAIL: Second SaveToString returned empty\n");
            ++g_Failures;
        }

        ECS::World dstWorld2;
        Scene::SceneSerializer loader2(&dstWorld2);
        auto result2 = loader2.LoadFromString(json2);
        CHECK_BOOL(result2.success, true, "Second LoadFromString");

        int dst2EntityCount = 0;
        for ([[maybe_unused]] ECS::Entity e : dstWorld2.GetAllEntities()) dst2EntityCount++;
        CHECK_EQ(dst2EntityCount, srcEntityCount, "Double round-trip entity count");
        std::printf("  Entities after double round-trip: %d\n", dst2EntityCount);

        // Spot-check player still intact
        ECS::Entity player2 = FindEntity(dstWorld2, "Player");
        ++g_Checks;
        if (player2 == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Player not found after double round-trip\n");
            ++g_Failures;
        } else {
            auto* fps2 = dstWorld2.GetComponent<ECS::FirstPersonController>(player2);
            if (fps2) {
                CHECK_BOOL(fps2->dungeonCrawlerMode, true, "double-rt.dungeonCrawlerMode");
                CHECK_FLOAT(fps2->gridCellSize, 3.0f, "double-rt.gridCellSize");
            }
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
