// Play mode is supposed to be non-destructive: Stop returns the scene to what
// was authored, so the next save writes the author's data and not the
// playtest's. It did not. Stop restored an entity's transform, its visibility
// and its rigidbody velocities, and that was the entire restore -- every other
// component a system had written during play was still holding the playtest's
// state when the editor saved.
//
// The damage that motivated this (REQ 7, "Engine Requests from Tune In"):
//
//   MeshComponent            generated glyph geometry over authored vertices
//   MaterialComponent        baseColor and five more, forced by SDF text/fades
//   HealthComponent          authored start HP becomes the playtest's end HP
//   DoorComponent            a door left open is saved open
//   Camera2DBoundsComponent  authored zoom baseline replaced by runtime zoom
//   TextComponent            authored string replaced by the last displayed one
//
// Tune_In shipped with a 28-vertex mesh in its scene file that reads as
// hand-authored geometry and is in fact the glyph mesh for "107.4 FM".
//
// The first test is the assertion that matters and the only one that scales:
// serialize, mutate, restore, serialize again, and require the two strings to be
// identical. It covers every registered component at once without enumerating
// any of them, so a component added next year is covered the day it is
// registered. The per-row tests below it exist to name the failures.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Door.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/Scene/SceneSerializer.h"
#include <cmath>
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

namespace {

bool Near(f32 a, f32 b, f32 eps = 0.0001f) { return std::fabs(a - b) < eps; }

// A cube, so the mesh row has real geometry to lose.
MeshComponent AuthoredCube() {
    MeshComponent m;
    for (int i = 0; i < 8; ++i) {
        MeshComponent::Vertex v;
        v.position = Vector3(static_cast<f32>(i & 1), static_cast<f32>((i >> 1) & 1),
                             static_cast<f32>((i >> 2) & 1));
        v.normal = Vector3(0.0f, 1.0f, 0.0f);
        m.vertices.push_back(v);
    }
    for (u32 i = 0; i < 12; ++i) { m.indices.push_back(i % 8); }
    return m;
}

// An entity carrying one of every component in the damage table, authored the
// way someone would author it.
Entity AuthoredEntity(World& w) {
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"Radio"});
    w.AddComponent<TransformComponent>(e, TransformComponent{});
    w.AddComponent<MeshComponent>(e, AuthoredCube());

    MaterialComponent mat;
    mat.baseColor = Vector3(0.85f, 0.12f, 0.06f);   // a deliberate red
    mat.opacity = 0.4f;                              // mid-fade, authored
    mat.emissiveColor = Vector3(0.9f, 0.7f, 0.2f);
    mat.gouraudOnly = true;
    w.AddComponent<MaterialComponent>(e, mat);

    HealthComponent hp;
    hp.maxHealth = 250.0f;
    hp.currentHealth = 250.0f;
    w.AddComponent<HealthComponent>(e, hp);

    DoorComponent door;
    door.openAngle = 95.0f;
    door.autoCloseDelay = 4.0f;
    w.AddComponent<DoorComponent>(e, door);

    Camera2DBoundsComponent bounds;
    bounds.useBounds = true;
    bounds.baseOrthoSize = 12.0f;
    bounds.currentZoom = 1.0f;
    w.AddComponent<Camera2DBoundsComponent>(e, bounds);

    TextComponent text;
    text.text = "107.4 FM";
    text.fontSize = 48.0f;
    w.AddComponent<TextComponent>(e, text);

    return e;
}

// Everything a play session's systems would scribble on this entity.
void PlayMutates(World& w, Entity e) {
    auto* mesh = w.GetComponent<MeshComponent>(e);
    mesh->vertices.clear();
    mesh->indices.clear();
    for (int i = 0; i < 28; ++i) {           // the glyph mesh for "107.4 FM"
        MeshComponent::Vertex v;
        v.position = Vector3(static_cast<f32>(i) * 0.1f, 0.0f, 0.0f);
        mesh->vertices.push_back(v);
    }
    for (u32 i = 0; i < 42; ++i) mesh->indices.push_back(i % 28);

    auto* mat = w.GetComponent<MaterialComponent>(e);
    mat->baseColor = Vector3(1.0f, 1.0f, 1.0f);      // SDF text forces white
    mat->opacity = 1.0f;                              // the fade-in finished
    mat->emissiveColor = Vector3(0.0f, 0.0f, 0.0f);   // the fade-out finished
    mat->gouraudOnly = false;

    w.GetComponent<HealthComponent>(e)->currentHealth = 7.5f;

    auto* door = w.GetComponent<DoorComponent>(e);
    door->open = true;
    door->currentAngle = 95.0f;
    door->initialized = true;

    auto* bounds = w.GetComponent<Camera2DBoundsComponent>(e);
    bounds->baseOrthoSize = 3.0f;
    bounds->currentZoom = 2.4f;

    w.GetComponent<TextComponent>(e)->text = "SIGNAL LOST";
}

std::string Serialize(World& w, Entity e) {
    return Scene::SceneSerializer::SerializeEntityToString(&w, e, /*includeVertexData=*/true);
}

} // namespace

// ---------------------------------------------------------------------------
// The whole assertion
// ---------------------------------------------------------------------------

ENJIN_TEST(ComponentSnapshot, PlayIsNonDestructive) {
    // Arrange: an authored entity, and what it serializes to before anyone plays.
    World w;
    Entity e = AuthoredEntity(w);
    const std::string authored = Serialize(w, e);

    Scene::ComponentSnapshot snap;
    snap.CaptureEntity(&w, e, static_cast<u64>(e));

    // Act: a play session writes over it, then Stop restores.
    PlayMutates(w, e);
    ENJIN_ASSERT_TRUE(Serialize(w, e) != authored);   // the mutation is real

    Scene::ComponentSnapshot::RestoreStats stats;
    snap.RestoreEntity(&w, e, static_cast<u64>(e), stats);

    // Assert: byte-identical. Anything a system touched and the restore missed
    // shows up here as a diff, whatever component it lives in.
    ENJIN_EXPECT_EQ(Serialize(w, e), authored);
    ENJIN_EXPECT_TRUE(stats.componentsRestored >= 7);
    ENJIN_EXPECT_EQ(stats.componentsReadded, static_cast<usize>(0));
}

// ---------------------------------------------------------------------------
// One per row of the damage table, so a regression says which one broke
// ---------------------------------------------------------------------------

ENJIN_TEST(ComponentSnapshot, GeneratedTextMeshDoesNotEatAuthoredGeometry) {
    World w;
    Entity e = AuthoredEntity(w);
    Scene::ComponentSnapshot snap;
    snap.CaptureEntity(&w, e, static_cast<u64>(e));

    PlayMutates(w, e);
    ENJIN_ASSERT_EQ(w.GetComponent<MeshComponent>(e)->vertices.size(), static_cast<usize>(28));

    Scene::ComponentSnapshot::RestoreStats stats;
    snap.RestoreEntity(&w, e, static_cast<u64>(e), stats);

    const auto* mesh = w.GetComponent<MeshComponent>(e);
    ENJIN_EXPECT_EQ(mesh->vertices.size(), static_cast<usize>(8));
    ENJIN_EXPECT_EQ(mesh->indices.size(), static_cast<usize>(12));
    ENJIN_EXPECT_TRUE(Near(mesh->vertices[7].position.x, 1.0f));
}

ENJIN_TEST(ComponentSnapshot, MaterialFieldsThatEqualTheirDefaultComeBack) {
    // The sharp end of REQ 7. MaterialComponent is the only component that
    // serializes conditionally -- a field equal to the struct default is omitted
    // -- so a runtime write that lands ON a default takes the authored value with
    // it. White baseColor, 1.0 opacity, black emissive and gouraudOnly=false are
    // all defaults, and all four are where an ordinary fade or a line of SDF text
    // leaves the material.
    World w;
    Entity e = AuthoredEntity(w);
    Scene::ComponentSnapshot snap;
    snap.CaptureEntity(&w, e, static_cast<u64>(e));

    PlayMutates(w, e);
    Scene::ComponentSnapshot::RestoreStats stats;
    snap.RestoreEntity(&w, e, static_cast<u64>(e), stats);

    const auto* mat = w.GetComponent<MaterialComponent>(e);
    ENJIN_EXPECT_TRUE(Near(mat->baseColor.x, 0.85f));
    ENJIN_EXPECT_TRUE(Near(mat->baseColor.y, 0.12f));
    ENJIN_EXPECT_TRUE(Near(mat->opacity, 0.4f));
    ENJIN_EXPECT_TRUE(Near(mat->emissiveColor.x, 0.9f));
    ENJIN_EXPECT_TRUE(mat->gouraudOnly);
}

ENJIN_TEST(ComponentSnapshot, HealthReturnsToItsAuthoredStart) {
    World w;
    Entity e = AuthoredEntity(w);
    Scene::ComponentSnapshot snap;
    snap.CaptureEntity(&w, e, static_cast<u64>(e));
    PlayMutates(w, e);
    Scene::ComponentSnapshot::RestoreStats stats;
    snap.RestoreEntity(&w, e, static_cast<u64>(e), stats);
    ENJIN_EXPECT_TRUE(Near(w.GetComponent<HealthComponent>(e)->currentHealth, 250.0f));
}

ENJIN_TEST(ComponentSnapshot, ADoorLeftOpenIsNotSavedOpen) {
    World w;
    Entity e = AuthoredEntity(w);
    Scene::ComponentSnapshot snap;
    snap.CaptureEntity(&w, e, static_cast<u64>(e));
    PlayMutates(w, e);
    Scene::ComponentSnapshot::RestoreStats stats;
    snap.RestoreEntity(&w, e, static_cast<u64>(e), stats);
    const auto* door = w.GetComponent<DoorComponent>(e);
    ENJIN_EXPECT_TRUE(!door->open);
    ENJIN_EXPECT_TRUE(!door->initialized);
    ENJIN_EXPECT_TRUE(Near(door->currentAngle, 0.0f));
}

ENJIN_TEST(ComponentSnapshot, CameraZoomBaselineSurvivesAPlaytest) {
    World w;
    Entity e = AuthoredEntity(w);
    Scene::ComponentSnapshot snap;
    snap.CaptureEntity(&w, e, static_cast<u64>(e));
    PlayMutates(w, e);
    Scene::ComponentSnapshot::RestoreStats stats;
    snap.RestoreEntity(&w, e, static_cast<u64>(e), stats);
    const auto* b = w.GetComponent<Camera2DBoundsComponent>(e);
    ENJIN_EXPECT_TRUE(Near(b->baseOrthoSize, 12.0f));
    ENJIN_EXPECT_TRUE(Near(b->currentZoom, 1.0f));
}

ENJIN_TEST(ComponentSnapshot, AuthoredStringOutlivesTheDisplayedOne) {
    World w;
    Entity e = AuthoredEntity(w);
    Scene::ComponentSnapshot snap;
    snap.CaptureEntity(&w, e, static_cast<u64>(e));
    PlayMutates(w, e);
    Scene::ComponentSnapshot::RestoreStats stats;
    snap.RestoreEntity(&w, e, static_cast<u64>(e), stats);
    ENJIN_EXPECT_EQ(w.GetComponent<TextComponent>(e)->text, std::string("107.4 FM"));
}

// ---------------------------------------------------------------------------
// Structural cases
// ---------------------------------------------------------------------------

ENJIN_TEST(ComponentSnapshot, AComponentRemovedDuringPlayComesBack) {
    World w;
    Entity e = AuthoredEntity(w);
    Scene::ComponentSnapshot snap;
    snap.CaptureEntity(&w, e, static_cast<u64>(e));

    w.RemoveComponent<DoorComponent>(e);
    ENJIN_ASSERT_TRUE(!w.HasComponent<DoorComponent>(e));

    Scene::ComponentSnapshot::RestoreStats stats;
    snap.RestoreEntity(&w, e, static_cast<u64>(e), stats);

    ENJIN_ASSERT_TRUE(w.HasComponent<DoorComponent>(e));
    ENJIN_EXPECT_TRUE(Near(w.GetComponent<DoorComponent>(e)->openAngle, 95.0f));
    ENJIN_EXPECT_EQ(stats.componentsReadded, static_cast<usize>(1));
}

ENJIN_TEST(ComponentSnapshot, AComponentAddedDuringPlayIsKept) {
    // "Scene changes persist on Stop" is the documented PlayMode contract, and
    // removing a component tears down that entity's GPU buffers with nothing
    // queued to rebuild them. The restore counts these and leaves them alone.
    World w;
    Entity e = AuthoredEntity(w);
    Scene::ComponentSnapshot snap;
    snap.CaptureEntity(&w, e, static_cast<u64>(e));

    w.AddComponent<NotesComponent>(e, NotesComponent{});

    Scene::ComponentSnapshot::RestoreStats stats;
    snap.RestoreEntity(&w, e, static_cast<u64>(e), stats);

    ENJIN_EXPECT_TRUE(w.HasComponent<NotesComponent>(e));
    ENJIN_EXPECT_EQ(stats.componentsAppeared, static_cast<usize>(1));
}

ENJIN_TEST(ComponentSnapshot, RestoresOntoTheHandleOfARecreatedEntity) {
    // An entity destroyed during play is rebuilt at Stop from the JSON captured
    // at its DEATH, so it comes back holding the state it died in -- on a NEW
    // handle. Its record is still filed under the old one.
    World w;
    Entity original = AuthoredEntity(w);
    const u64 key = static_cast<u64>(original);
    const std::string authored = Serialize(w, original);

    Scene::ComponentSnapshot snap;
    snap.CaptureEntity(&w, original, key);

    PlayMutates(w, original);
    const std::string died = Serialize(w, original);
    w.DestroyEntity(original);
    w.FlushPendingDestructions();

    Entity recreated = Scene::SceneSerializer::DeserializeEntityFromString(&w, died);
    ENJIN_ASSERT_TRUE(w.IsValid(recreated));

    Scene::ComponentSnapshot::RestoreStats stats;
    snap.RestoreEntity(&w, recreated, key, stats);

    ENJIN_EXPECT_EQ(Serialize(w, recreated), authored);
}

// ---------------------------------------------------------------------------
// A material at all defaults
// ---------------------------------------------------------------------------

ENJIN_TEST(ComponentSnapshot, AnAllDefaultMaterialStillLoads) {
    // Found by the recreate test above, and much sharper than the restore it was
    // testing. MaterialComponent is the one component that serializes
    // conditionally -- it writes only what differs from a fresh material -- so a
    // material sitting on every default writes no keys at all, and a
    // default-constructed nlohmann::json is null, not {}. The scene then carried
    // "material": null, and reading it threw "cannot use value() with null" out
    // of the entity loop, which is not caught per entity: the whole scene load
    // failed and every entity after that one was lost.
    //
    // A fade-in that ends at opacity 1.0 is enough to get there.
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"Plain"});
    w.AddComponent<TransformComponent>(e, TransformComponent{});
    w.AddComponent<MaterialComponent>(e, MaterialComponent{});

    const std::string json = Serialize(w, e);
    ENJIN_EXPECT_TRUE(json.find("\"material\":null") == std::string::npos);

    World dst;
    Entity loaded = Scene::SceneSerializer::DeserializeEntityFromString(&dst, json);
    ENJIN_ASSERT_TRUE(dst.IsValid(loaded));
    ENJIN_EXPECT_TRUE(dst.HasComponent<MaterialComponent>(loaded));
    ENJIN_EXPECT_EQ(dst.GetComponent<NameComponent>(loaded)->name, std::string("Plain"));
}

ENJIN_TEST(ComponentSnapshot, AMaterialWrittenAsNullOnDiskStillLoads) {
    // The healing half: scenes saved before the fix carry "material": null, and
    // they have to load rather than fail. Read as the empty object it meant.
    World w;
    const std::string onDisk =
        "{\"id\":1,\"name\":{\"name\":\"Legacy\"},\"material\":null,"
        "\"transform\":{\"position\":[0.0,0.0,0.0],\"rotation\":[0.0,0.0,0.0,1.0],"
        "\"scale\":[1.0,1.0,1.0]}}";

    Entity loaded = Scene::SceneSerializer::DeserializeEntityFromString(&w, onDisk);
    ENJIN_ASSERT_TRUE(w.IsValid(loaded));
    ENJIN_EXPECT_TRUE(w.HasComponent<MaterialComponent>(loaded));
    ENJIN_EXPECT_EQ(w.GetComponent<NameComponent>(loaded)->name, std::string("Legacy"));
}

ENJIN_TEST(ComponentSnapshot, FlipbookFpsSurvivesAnEmptyGrid) {
    // The one place MaterialComponent's conditional serialization really did lose
    // data. Dropping a field for equalling its default is lossless -- an omitted
    // key reads back as that default. Gating a field on a DIFFERENT field is not:
    // flipbookFps was written only when cols and rows were both non-zero, so an
    // fps typed in before the grid was filled in vanished, and filling the grid
    // in later brought back 10 rather than what was typed.
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"Sprite"});
    w.AddComponent<TransformComponent>(e, TransformComponent{});
    MaterialComponent mat;
    mat.flipbookFps = 24.0f;   // grid still 0x0
    w.AddComponent<MaterialComponent>(e, mat);

    World dst;
    Entity loaded = Scene::SceneSerializer::DeserializeEntityFromString(
        &dst, Serialize(w, e));
    ENJIN_ASSERT_TRUE(dst.IsValid(loaded));
    ENJIN_EXPECT_TRUE(Near(dst.GetComponent<MaterialComponent>(loaded)->flipbookFps, 24.0f));
}

// ---------------------------------------------------------------------------
// Coverage
// ---------------------------------------------------------------------------

ENJIN_TEST(ComponentSnapshot, EveryRegisteredComponentCanBeSnapshotted) {
    // A registry entry with no copy/assign pair is a component that saves,
    // mutates during play and never restores -- silently, and only for that one
    // component. The uniform entries get the pair from the SERDES macro; the
    // irregular ones spell it out, which is the half that can be forgotten.
    std::string missing;
    for (const auto& key : Scene::SceneSerializer::ComponentKeysMissingSnapshotOps()) {
        if (!missing.empty()) missing += ", ";
        missing += key;
    }
    // Compared as a string so a failure names the components rather than a count.
    ENJIN_EXPECT_EQ(missing, std::string(""));
    ENJIN_EXPECT_TRUE(Scene::SceneSerializer::RegisteredComponentKeys().size() > 100);
}

ENJIN_TEST_MAIN()
