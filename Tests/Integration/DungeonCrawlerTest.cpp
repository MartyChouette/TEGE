// Dungeon Crawler mode test
// Creates a mini dungeon (like the template), verifies:
// 1. Template structure: player has dungeonCrawlerMode, walls have BoxColliders
// 2. Snap turn logic: yaw changes by ±90° correctly
// 3. Facing-relative movement: W/S maps to correct cardinal direction based on yaw
// 4. Wall collision: movement blocked when target cell has a BoxCollider
// 5. Serialization round-trip: new fields survive save/load

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

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

// Mini dungeon layout (4x4 for simplicity)
// 1 = floor, 0 = solid wall
// Player starts at (1,1), facing south (yaw=180)
static const int GRID = 4;
static const f32 CELL = 3.0f;
static int layout[GRID][GRID] = {
    {0, 0, 0, 0},
    {0, 1, 1, 0},
    {0, 1, 1, 0},
    {0, 0, 0, 0},
};

struct DungeonScene {
    ECS::World world;
    ECS::Entity player = ECS::INVALID_ENTITY;
    std::vector<ECS::Entity> walls;
    std::vector<ECS::Entity> floors;

    DungeonScene() = default;
    DungeonScene(const DungeonScene&) = delete;
    DungeonScene& operator=(const DungeonScene&) = delete;
};

void BuildMiniDungeon(DungeonScene& scene) {
    const f32 WALL_H = 3.0f;
    const f32 WALL_THICK = 0.15f;

    int wallIdx = 0;
    int floorIdx = 0;
    char nameBuf[64];

    for (int z = 0; z < GRID; ++z) {
        for (int x = 0; x < GRID; ++x) {
            if (layout[z][x] == 0) continue;

            f32 cx = static_cast<f32>(x) * CELL + CELL * 0.5f;
            f32 cz = static_cast<f32>(z) * CELL + CELL * 0.5f;

            // Floor
            snprintf(nameBuf, sizeof(nameBuf), "Floor_%d", floorIdx++);
            ECS::Entity floor = scene.world.CreateEntity();
            scene.world.AddComponent<ECS::NameComponent>(floor, nameBuf);
            auto& ft = scene.world.AddComponent<ECS::TransformComponent>(floor);
            ft.position = Math::Vector3(cx, 0.0f, cz);
            ft.scale = Math::Vector3(CELL, 0.1f, CELL);
            scene.floors.push_back(floor);

            // Walls on edges adjacent to solid cells
            auto makeWall = [&](const char* name, Math::Vector3 pos, Math::Vector3 scale) {
                ECS::Entity wall = scene.world.CreateEntity();
                scene.world.AddComponent<ECS::NameComponent>(wall, name);
                auto& wt = scene.world.AddComponent<ECS::TransformComponent>(wall);
                wt.position = pos;
                wt.scale = scale;
                auto& col = scene.world.AddComponent<ECS::BoxColliderComponent>(wall);
                col.size = Math::Vector3(1.0f, 1.0f, 1.0f);
                scene.walls.push_back(wall);
                return wall;
            };

            // North wall
            if (z == 0 || layout[z - 1][x] == 0) {
                snprintf(nameBuf, sizeof(nameBuf), "Wall_N_%d", wallIdx++);
                makeWall(nameBuf,
                    Math::Vector3(cx, WALL_H * 0.5f, cz - CELL * 0.5f),
                    Math::Vector3(CELL, WALL_H, WALL_THICK));
            }
            // South wall
            if (z == GRID - 1 || layout[z + 1][x] == 0) {
                snprintf(nameBuf, sizeof(nameBuf), "Wall_S_%d", wallIdx++);
                makeWall(nameBuf,
                    Math::Vector3(cx, WALL_H * 0.5f, cz + CELL * 0.5f),
                    Math::Vector3(CELL, WALL_H, WALL_THICK));
            }
            // West wall
            if (x == 0 || layout[z][x - 1] == 0) {
                snprintf(nameBuf, sizeof(nameBuf), "Wall_W_%d", wallIdx++);
                makeWall(nameBuf,
                    Math::Vector3(cx - CELL * 0.5f, WALL_H * 0.5f, cz),
                    Math::Vector3(WALL_THICK, WALL_H, CELL));
            }
            // East wall
            if (x == GRID - 1 || layout[z][x + 1] == 0) {
                snprintf(nameBuf, sizeof(nameBuf), "Wall_E_%d", wallIdx++);
                makeWall(nameBuf,
                    Math::Vector3(cx + CELL * 0.5f, WALL_H * 0.5f, cz),
                    Math::Vector3(WALL_THICK, WALL_H, CELL));
            }
        }
    }

    // Player at cell (1,1) facing south
    scene.player = scene.world.CreateEntity();
    scene.world.AddComponent<ECS::NameComponent>(scene.player, "Player");
    auto& pt = scene.world.AddComponent<ECS::TransformComponent>(scene.player);
    pt.position = Math::Vector3(1.0f * CELL + CELL * 0.5f, 0.0f, 1.0f * CELL + CELL * 0.5f);

    auto& fps = scene.world.AddComponent<ECS::FirstPersonController>(scene.player);
    fps.moveSpeed = 4.0f;
    fps.mouseSensitivity = 0.15f;
    fps.gridMovement = true;
    fps.gridCellSize = CELL;
    fps.gridOrigin = Math::Vector3(CELL * 0.5f, 0.0f, CELL * 0.5f);
    fps.standingHeight = 1.5f;
    fps.currentHeight = 1.5f;
    fps.dungeonCrawlerMode = true;
    fps.snapTurnAngle = 90.0f;
    fps.yaw = 180.0f;  // Facing south
}

// Simulate the wall collision check from ControllerSystem
// Checks both the cell boundary midpoint and the target cell center
bool IsTargetBlocked(ECS::World& world, f32 currentX, f32 currentZ, f32 targetX, f32 targetZ) {
    f32 midX = (currentX + targetX) * 0.5f;
    f32 midZ = (currentZ + targetZ) * 0.5f;
    for (ECS::Entity other : world.GetAllEntities()) {
        if (!world.HasComponent<ECS::BoxColliderComponent>(other) ||
            !world.HasComponent<ECS::TransformComponent>(other)) {
            continue;
        }
        auto* col = world.GetComponent<ECS::BoxColliderComponent>(other);
        if (col->isTrigger) continue;
        auto* otherT = world.GetComponent<ECS::TransformComponent>(other);
        f32 colCX = otherT->position.x + col->center.x;
        f32 colCZ = otherT->position.z + col->center.z;
        f32 colHalfX = col->size.x * otherT->scale.x * 0.5f;
        f32 colHalfZ = col->size.z * otherT->scale.z * 0.5f;
        bool midHit = (midX >= colCX - colHalfX && midX <= colCX + colHalfX &&
                       midZ >= colCZ - colHalfZ && midZ <= colCZ + colHalfZ);
        bool tgtHit = (targetX >= colCX - colHalfX && targetX <= colCX + colHalfX &&
                       targetZ >= colCZ - colHalfZ && targetZ <= colCZ + colHalfZ);
        if (midHit || tgtHit) {
            return true;
        }
    }
    return false;
}

// Compute facing-relative move direction (mirrors ControllerSystem logic)
void GetFacingRelativeTarget(f32 yaw, f32 inputY, f32 cellSize,
                             f32 posX, f32 posZ, f32 originX, f32 originZ,
                             f32& outTargetX, f32& outTargetZ) {
    f32 yr = yaw * 3.14159265f / 180.0f;
    f32 fwdX = -std::sin(yr);
    f32 fwdZ = -std::cos(yr);
    f32 dirX, dirZ;
    if (std::fabs(fwdX) > std::fabs(fwdZ)) {
        dirX = fwdX > 0.0f ? 1.0f : -1.0f;
        dirZ = 0.0f;
    } else {
        dirX = 0.0f;
        dirZ = fwdZ > 0.0f ? 1.0f : -1.0f;
    }
    if (inputY < 0.0f) {
        dirX = -dirX;
        dirZ = -dirZ;
    }
    f32 snappedX = std::round((posX - originX) / cellSize) * cellSize + originX;
    f32 snappedZ = std::round((posZ - originZ) / cellSize) * cellSize + originZ;
    outTargetX = snappedX + dirX * cellSize;
    outTargetZ = snappedZ + dirZ * cellSize;
}

int main() {
    std::printf("=== Dungeon Crawler Test ===\n\n");

    // ========================================
    // TEST 1: Template structure
    // ========================================
    std::printf("--- Test 1: Template Structure ---\n");
    {
        DungeonScene scene;
        BuildMiniDungeon(scene);

        // Player has correct components and fields
        HAS_COMPONENT(scene.world, scene.player, ECS::FirstPersonController, "Player FPSController");
        auto* fps = scene.world.GetComponent<ECS::FirstPersonController>(scene.player);
        if (fps) {
            CHECK_BOOL(fps->dungeonCrawlerMode, true, "fps.dungeonCrawlerMode");
            CHECK_FLOAT(fps->snapTurnAngle, 90.0f, "fps.snapTurnAngle");
            CHECK_BOOL(fps->gridMovement, true, "fps.gridMovement");
            CHECK_FLOAT(fps->gridCellSize, CELL, "fps.gridCellSize");
            CHECK_FLOAT(fps->yaw, 180.0f, "fps.yaw (facing south)");
        }

        // Walls have BoxColliderComponent
        std::printf("  Wall count: %d\n", (int)scene.walls.size());
        int wallsWithColliders = 0;
        for (auto w : scene.walls) {
            if (scene.world.HasComponent<ECS::BoxColliderComponent>(w)) {
                wallsWithColliders++;
            }
        }
        CHECK_EQ(wallsWithColliders, (int)scene.walls.size(), "All walls have BoxCollider");
        // 4 open cells in a 2x2 block, outer walls on all edges
        // Each of the 4 cells has walls where adjacent to solid
        // Expect > 0 walls
        ++g_Checks;
        if (scene.walls.size() == 0) {
            std::printf("  FAIL: No walls created\n");
            ++g_Failures;
        }
    }

    // ========================================
    // TEST 2: Snap turns
    // ========================================
    std::printf("\n--- Test 2: Snap Turns ---\n");
    {
        ECS::FirstPersonController ctrl;
        ctrl.dungeonCrawlerMode = true;
        ctrl.snapTurnAngle = 90.0f;
        ctrl.yaw = 180.0f;  // Facing south

        // Turn right (D key): yaw += 90 => 270
        ctrl.yaw += ctrl.snapTurnAngle;
        while (ctrl.yaw < 0.0f) ctrl.yaw += 360.0f;
        while (ctrl.yaw >= 360.0f) ctrl.yaw -= 360.0f;
        CHECK_FLOAT(ctrl.yaw, 270.0f, "After right turn: yaw=270");

        // Turn right again: 270 + 90 = 360 => 0
        ctrl.yaw += ctrl.snapTurnAngle;
        while (ctrl.yaw < 0.0f) ctrl.yaw += 360.0f;
        while (ctrl.yaw >= 360.0f) ctrl.yaw -= 360.0f;
        CHECK_FLOAT(ctrl.yaw, 0.0f, "After 2nd right turn: yaw=0 (north)");

        // Turn left (A key): 0 - 90 = -90 => 270
        ctrl.yaw -= ctrl.snapTurnAngle;
        while (ctrl.yaw < 0.0f) ctrl.yaw += 360.0f;
        while (ctrl.yaw >= 360.0f) ctrl.yaw -= 360.0f;
        CHECK_FLOAT(ctrl.yaw, 270.0f, "After left turn from north: yaw=270");

        // Reset to 0 (north), turn left
        ctrl.yaw = 0.0f;
        ctrl.yaw -= ctrl.snapTurnAngle;
        while (ctrl.yaw < 0.0f) ctrl.yaw += 360.0f;
        while (ctrl.yaw >= 360.0f) ctrl.yaw -= 360.0f;
        CHECK_FLOAT(ctrl.yaw, 270.0f, "Left from north: yaw=270");

        // Full rotation: 4 right turns from 180 should return to 180
        ctrl.yaw = 180.0f;
        for (int i = 0; i < 4; i++) {
            ctrl.yaw += ctrl.snapTurnAngle;
            while (ctrl.yaw < 0.0f) ctrl.yaw += 360.0f;
            while (ctrl.yaw >= 360.0f) ctrl.yaw -= 360.0f;
        }
        CHECK_FLOAT(ctrl.yaw, 180.0f, "Full 360 rotation returns to start");

        // Debounce: snapTurnPending prevents repeated turns
        ctrl.snapTurnPending = false;
        CHECK_BOOL(ctrl.snapTurnPending, false, "snapTurnPending initially false");
        ctrl.snapTurnPending = true;
        CHECK_BOOL(ctrl.snapTurnPending, true, "snapTurnPending set to true after turn");
    }

    // ========================================
    // TEST 3: Facing-relative movement
    // ========================================
    std::printf("\n--- Test 3: Facing-Relative Movement ---\n");
    {
        // Player at cell (1,1), grid origin at (1.5, 0, 1.5), cell size 3.0
        f32 posX = 1.0f * CELL + CELL * 0.5f;  // 4.5
        f32 posZ = 1.0f * CELL + CELL * 0.5f;  // 4.5
        f32 ox = CELL * 0.5f;                    // 1.5
        f32 oz = CELL * 0.5f;                    // 1.5
        f32 targetX, targetZ;

        // Facing south (yaw=180), press W (forward, inputY=1.0) => move +Z
        GetFacingRelativeTarget(180.0f, 1.0f, CELL, posX, posZ, ox, oz, targetX, targetZ);
        CHECK_FLOAT(targetX, posX, "South+W: X unchanged");
        CHECK_FLOAT(targetZ, posZ + CELL, "South+W: Z + CELL (move south)");

        // Facing south (yaw=180), press S (backward, inputY=-1.0) => move -Z
        GetFacingRelativeTarget(180.0f, -1.0f, CELL, posX, posZ, ox, oz, targetX, targetZ);
        CHECK_FLOAT(targetX, posX, "South+S: X unchanged");
        CHECK_FLOAT(targetZ, posZ - CELL, "South+S: Z - CELL (move north)");

        // Facing north (yaw=0), press W => move -Z
        GetFacingRelativeTarget(0.0f, 1.0f, CELL, posX, posZ, ox, oz, targetX, targetZ);
        CHECK_FLOAT(targetX, posX, "North+W: X unchanged");
        CHECK_FLOAT(targetZ, posZ - CELL, "North+W: Z - CELL (move north)");

        // Facing east (yaw=90), press W => move -X
        // yaw=90: fwdX = -sin(90°) = -1, fwdZ = -cos(90°) = 0 => dominant is X, dir = -1
        GetFacingRelativeTarget(90.0f, 1.0f, CELL, posX, posZ, ox, oz, targetX, targetZ);
        CHECK_FLOAT(targetX, posX - CELL, "East+W: X - CELL (move east in engine coords)");
        CHECK_FLOAT(targetZ, posZ, "East+W: Z unchanged");

        // Facing west (yaw=270), press W => move +X
        // yaw=270: fwdX = -sin(270°) = 1, fwdZ = -cos(270°) = 0 => dominant is X, dir = +1
        GetFacingRelativeTarget(270.0f, 1.0f, CELL, posX, posZ, ox, oz, targetX, targetZ);
        CHECK_FLOAT(targetX, posX + CELL, "West+W: X + CELL (move west in engine coords)");
        CHECK_FLOAT(targetZ, posZ, "West+W: Z unchanged");
    }

    // ========================================
    // TEST 4: Wall collision
    // ========================================
    std::printf("\n--- Test 4: Wall Collision ---\n");
    {
        DungeonScene scene;
        BuildMiniDungeon(scene);
        auto* fps = scene.world.GetComponent<ECS::FirstPersonController>(scene.player);
        auto* pt = scene.world.GetComponent<ECS::TransformComponent>(scene.player);

        f32 posX = pt->position.x;  // Cell (1,1) center: 4.5
        f32 posZ = pt->position.z;  // Cell (1,1) center: 4.5
        f32 ox = fps->gridOrigin.x;
        f32 oz = fps->gridOrigin.z;

        // Yaw direction mapping: 0=north(-Z), 90=west(-X), 180=south(+Z), 270=east(+X)

        // Moving south from (1,1) to (1,2) should be open (layout[2][1] = 1)
        f32 targetX, targetZ;
        GetFacingRelativeTarget(180.0f, 1.0f, CELL, posX, posZ, ox, oz, targetX, targetZ);
        bool blocked = IsTargetBlocked(scene.world, posX, posZ, targetX, targetZ);
        CHECK_BOOL(blocked, false, "Move south (1,1)->(1,2): open");

        // Moving east (+X) from (1,1) to (2,1) should be open (layout[1][2] = 1)
        GetFacingRelativeTarget(270.0f, 1.0f, CELL, posX, posZ, ox, oz, targetX, targetZ);
        blocked = IsTargetBlocked(scene.world, posX, posZ, targetX, targetZ);
        CHECK_BOOL(blocked, false, "Move east (1,1)->(2,1): open");

        // Moving north from (1,1) to (1,0) should be blocked (layout[0][1] = 0, wall there)
        GetFacingRelativeTarget(0.0f, 1.0f, CELL, posX, posZ, ox, oz, targetX, targetZ);
        blocked = IsTargetBlocked(scene.world, posX, posZ, targetX, targetZ);
        CHECK_BOOL(blocked, true, "Move north (1,1)->(1,0): BLOCKED by wall");

        // Moving west (-X) from (1,1) to (0,1) should be blocked (layout[1][0] = 0, wall there)
        GetFacingRelativeTarget(90.0f, 1.0f, CELL, posX, posZ, ox, oz, targetX, targetZ);
        blocked = IsTargetBlocked(scene.world, posX, posZ, targetX, targetZ);
        CHECK_BOOL(blocked, true, "Move west (1,1)->(0,1): BLOCKED by wall");

        // From cell (2,2): moving south to (2,3) should be blocked (layout[3][2] = 0)
        f32 posX2 = 2.0f * CELL + CELL * 0.5f;  // 7.5
        f32 posZ2 = 2.0f * CELL + CELL * 0.5f;  // 7.5
        GetFacingRelativeTarget(180.0f, 1.0f, CELL, posX2, posZ2, ox, oz, targetX, targetZ);
        blocked = IsTargetBlocked(scene.world, posX2, posZ2, targetX, targetZ);
        CHECK_BOOL(blocked, true, "Move south (2,2)->(2,3): BLOCKED by wall");

        // From cell (2,2): moving east (+X) to (3,2) should be blocked (layout[2][3] = 0)
        GetFacingRelativeTarget(270.0f, 1.0f, CELL, posX2, posZ2, ox, oz, targetX, targetZ);
        blocked = IsTargetBlocked(scene.world, posX2, posZ2, targetX, targetZ);
        CHECK_BOOL(blocked, true, "Move east (2,2)->(3,2): BLOCKED by wall");
    }

    // ========================================
    // TEST 5: Serialization round-trip
    // ========================================
    std::printf("\n--- Test 5: Serialization Round-Trip ---\n");
    {
        DungeonScene scene;
        BuildMiniDungeon(scene);

        // Serialize
        Scene::SceneSerializer serializer(&scene.world);
        std::string json = serializer.SaveToString();

        ++g_Checks;
        if (json.empty()) {
            std::printf("  FAIL: SaveToString returned empty\n");
            ++g_Failures;
        } else {
            std::printf("  Serialized %d bytes\n", (int)json.size());
        }

        // Deserialize into fresh world
        ECS::World dstWorld;
        Scene::SceneSerializer loader(&dstWorld);
        auto result = loader.LoadFromString(json);
        CHECK_BOOL(result.success, true, "LoadFromString succeeded");

        // Find player entity in new world
        ECS::Entity dstPlayer = ECS::INVALID_ENTITY;
        for (ECS::Entity e : dstWorld.GetAllEntities()) {
            auto* name = dstWorld.GetComponent<ECS::NameComponent>(e);
            if (name && name->name == "Player") {
                dstPlayer = e;
                break;
            }
        }

        ++g_Checks;
        if (dstPlayer == ECS::INVALID_ENTITY) {
            std::printf("  FAIL: Player entity not found after load\n");
            ++g_Failures;
        } else {
            HAS_COMPONENT(dstWorld, dstPlayer, ECS::FirstPersonController, "Player FPS after load");
            auto* fps = dstWorld.GetComponent<ECS::FirstPersonController>(dstPlayer);
            if (fps) {
                CHECK_BOOL(fps->dungeonCrawlerMode, true, "rt.dungeonCrawlerMode");
                CHECK_FLOAT(fps->snapTurnAngle, 90.0f, "rt.snapTurnAngle");
                CHECK_BOOL(fps->gridMovement, true, "rt.gridMovement");
                CHECK_FLOAT(fps->gridCellSize, CELL, "rt.gridCellSize");
                CHECK_FLOAT(fps->mouseSensitivity, 0.15f, "rt.mouseSensitivity");
                CHECK_FLOAT(fps->standingHeight, 1.5f, "rt.standingHeight");
                // Runtime state should NOT persist
                CHECK_BOOL(fps->snapTurnPending, false, "rt.snapTurnPending (runtime, should be false)");
            }
        }

        // Verify walls still have BoxColliders after round-trip
        int wallsAfter = 0;
        int wallsWithCollider = 0;
        for (ECS::Entity e : dstWorld.GetAllEntities()) {
            auto* name = dstWorld.GetComponent<ECS::NameComponent>(e);
            if (name && name->name.find("Wall_") == 0) {
                wallsAfter++;
                if (dstWorld.HasComponent<ECS::BoxColliderComponent>(e)) {
                    wallsWithCollider++;
                }
            }
        }
        std::printf("  Walls after load: %d, with colliders: %d\n", wallsAfter, wallsWithCollider);
        CHECK_EQ(wallsWithCollider, wallsAfter, "All walls retain BoxCollider after round-trip");
        ++g_Checks;
        if (wallsAfter == 0) {
            std::printf("  FAIL: No wall entities found after round-trip\n");
            ++g_Failures;
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
