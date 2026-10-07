// The Entity menu entries added by GR-17.
//
// Each of these was something a person could only make by knowing which
// component to add to an empty entity: an audio source, a particle emitter, a
// trigger zone, a player character, a door. The bar in the golden rule is that
// every capability is reachable from the editor without that knowledge, and
// Entity is where people look first.
//
// Every entry makes what the Add Component list would, on a named entity in
// front of the editor camera, plus the one or two things that make it usable
// on the spot (a mesh to see, a collider to stand on, a follow camera).
//
// The entries are one table. The menu draws it and the command palette
// registers it ("Create Door"), so the two cannot disagree (GR-16).

#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/ECS/Components/EffekseerEffect.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Door.h"
#include "Enjin/ECS/Components/Ladder.h"
#include "Enjin/ECS/Components/Rope.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/ECS/Components/ReflectionProbe.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/Renderer/MeshFactory.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <type_traits>

namespace Enjin {
namespace Editor {

ECS::Entity EditorLayer::MakeMenuEntity(const char* name, const Math::Vector3& position) {
    ECS::Entity e = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(e, name);
    auto& tf = m_World->AddComponent<ECS::TransformComponent>(e);
    tf.position = position;
    return e;
}

void EditorLayer::FinishMenuEntity(ECS::Entity e) {
    MarkDirty();
    SelectEntity(e);
    RecordLayerCreate(e);
}

Math::Vector3 EditorLayer::EntitySpawnPosition() const {
    // In front of the editor camera at its orbit-pivot depth, so a new object
    // is in view rather than at a world origin the camera has left
    if (!m_Camera) return Math::Vector3(0.0f, 0.0f, 0.0f);
    f32 dist = m_CameraController ? m_CameraController->GetOrbitDistance() : 6.0f;
    if (!(dist > 0.5f)) dist = 6.0f;
    return m_Camera->GetPosition() + m_Camera->GetForward() * dist;
}

std::vector<EditorLayer::EntityMenuEntry> EditorLayer::BuildEntityMenuTable() {
    std::vector<EntityMenuEntry> t;
    // group "" = directly in the Entity menu
    auto add = [&](const char* group, const char* label, const char* tip,
                   std::function<void(const Math::Vector3&)> fn) {
        t.push_back({ group, label, tip, std::move(fn) });
    };
    auto simple = [&](const char* group, const char* label, const char* tip, auto componentTag) {
        add(group, label, tip, [this, label, componentTag](const Math::Vector3& at) {
            using C = typename decltype(componentTag)::type;
            ECS::Entity e = MakeMenuEntity(label, at);
            m_World->AddComponent<C>(e);
            FinishMenuEntity(e);
        });
    };

    // --- Directly in the menu ---
    simple("", "Audio Source", "A 3D sound in the world. Pick its clip in the inspector.",
           std::type_identity<ECS::AudioSourceComponent>{});
    simple("", "Particle Emitter", nullptr, std::type_identity<ECS::ParticleEmitterComponent>{});
    simple("", "Effekseer Effect", "An effect made in the Effekseer editor. Pick its file in the inspector.",
           std::type_identity<ECS::EffekseerEffectComponent>{});
    add("", "Text", "Text in the world. For screen text, use a UI Canvas.",
        [this](const Math::Vector3& at) {
            ECS::Entity e = MakeMenuEntity("Text", at);
            m_World->AddComponent<ECS::TextComponent>(e).text = "Text";
            FinishMenuEntity(e);
        });
    add("", "UI Canvas", "Screen UI: menus, HUD, buttons. Opens in the UI editor.",
        [this](const Math::Vector3&) {
            ECS::Entity e = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(e, "UI Canvas");
            m_World->AddComponent<GUI::UICanvasComponent>(e, GUI::UICanvasComponent{});
            MarkDirty();
            OpenUIEditor(e);
        });

    // --- Player Character ---
    // A capsule you can see, the controller, a collider fitted to that capsule,
    // and the follow camera that controller type wants: the same three things
    // the inspector's "Basic Movement" card adds.
    struct Kind { const char* label; const char* controllerType; };
    static const Kind kKinds[] = {
        {"First Person",  "FirstPerson"},
        {"Third Person",  "ThirdPerson"},
        {"Top-Down 3D",   "TopDown3D"},
        {"Platformer 2D", "Platformer2D"},
        {"Top-Down 2D",   "TopDown2D"},
    };
    for (const Kind& k : kKinds) {
        add("Player Character", k.label, nullptr, [this, k](const Math::Vector3& at) {
            const std::string type = k.controllerType;
            const bool is2D = type == "Platformer2D" || type == "TopDown2D";
            ECS::Entity e = MakeMenuEntity("Player", at);
            m_World->AddComponent<ECS::MaterialComponent>(e).baseColor = Math::Vector3(0.35f, 0.55f, 0.85f);
            if (is2D) {
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.8f, 1.6f));
                m_World->AddComponent<ECS::BoxColliderComponent>(e).size = Math::Vector3(0.8f, 1.6f, 0.2f);
                if (type == "Platformer2D") m_World->AddComponent<ECS::Platformer2DController>(e);
                else                        m_World->AddComponent<ECS::TopDown2DController>(e);
            } else {
                // Collider from the same numbers as the mesh: radius 0.3, and
                // CreateCapsule's 1.0 cylinder plus its two caps
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
                auto& col = m_World->AddComponent<ECS::CapsuleColliderComponent>(e);
                col.radius = 0.3f;
                col.SetTotalHeight(1.0f + 2.0f * 0.3f);
                if (type == "FirstPerson")      m_World->AddComponent<ECS::FirstPersonController>(e);
                else if (type == "ThirdPerson") m_World->AddComponent<ECS::ThirdPersonController>(e);
                else                            m_World->AddComponent<ECS::TopDown3DController>(e);
            }
            SetupCameraForController(e, k.controllerType);
            FinishMenuEntity(e);
        });
    }

    // --- Gameplay ---
    const char* kPlay = "Gameplay";
    simple(kPlay, "Trigger Zone", "Tells scripts and triggers when something enters or leaves it.",
           std::type_identity<ECS::TriggerZoneComponent>{});
    simple(kPlay, "Spawn Point", nullptr, std::type_identity<ECS::SpawnPointComponent>{});
    add(kPlay, "Save Point", nullptr, [this](const Math::Vector3& at) {
        ECS::Entity e = MakeMenuEntity("Save Point", at);
        m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCylinder(0.4f, 0.2f));
        m_World->AddComponent<ECS::MaterialComponent>(e).baseColor = Math::Vector3(0.9f, 0.8f, 0.3f);
        m_World->AddComponent<ECS::SavePointComponent>(e);
        FinishMenuEntity(e);
    });
    add(kPlay, "Door", "Opens with Interact. The entity is the hinge; the panel is its child.",
        [this](const Math::Vector3& at) {
            // The Door component swings its OWN entity about that entity's
            // origin, so the entity is the hinge and the panel is a child offset
            // half a door-width from it -- the shape Playground's door uses. A
            // panel on the hinge entity itself would spin about its middle. The
            // panel is kinematic, so it pushes the player rather than being
            // pushed.
            ECS::Entity hinge = MakeMenuEntity("Door", at);
            m_World->AddComponent<ECS::DoorComponent>(hinge);
            ECS::Entity panel = MakeMenuEntity("Door Panel", Math::Vector3(0.5f, 1.0f, 0.0f));
            m_World->GetComponent<ECS::TransformComponent>(panel)->scale = Math::Vector3(1.0f, 2.0f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(panel, Renderer::MeshFactory::CreateCube(1.0f));
            m_World->AddComponent<ECS::MaterialComponent>(panel).baseColor = Math::Vector3(0.55f, 0.4f, 0.25f);
            m_World->AddComponent<ECS::BoxColliderComponent>(panel).size = Math::Vector3(1.0f, 2.0f, 0.1f);
            auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(panel);
            rb.bodyType = ECS::RigidbodyComponent::BodyType::Kinematic;
            rb.useGravity = false;
            ECS::SetParent(m_World, panel, hinge);
            RecordLayerCreate(panel);
            FinishMenuEntity(hinge);
        });
    add(kPlay, "Ladder", nullptr, [this](const Math::Vector3& at) {
        ECS::Entity e = MakeMenuEntity("Ladder", at);
        auto& ladder = m_World->AddComponent<ECS::LadderComponent>(e);
        // A mesh the size of the climbable volume, so what you see is what you
        // can climb
        m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCube(1.0f));
        m_World->GetComponent<ECS::TransformComponent>(e)->scale =
            Math::Vector3(ladder.halfExtents.x * 2.0f, ladder.halfExtents.y * 2.0f, 0.1f);
        m_World->AddComponent<ECS::MaterialComponent>(e).baseColor = Math::Vector3(0.5f, 0.35f, 0.2f);
        FinishMenuEntity(e);
    });
    simple(kPlay, "Rope", "Hangs down from the entity and swings.", std::type_identity<ECS::RopeComponent>{});
    add(kPlay, "Boat", "Drives on water. Put it on a Water Volume or Water 3D.",
        [this](const Math::Vector3& at) {
            ECS::Entity e = MakeMenuEntity("Boat", at);
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCube(1.0f));
            m_World->GetComponent<ECS::TransformComponent>(e)->scale = Math::Vector3(1.2f, 0.4f, 2.5f);
            m_World->AddComponent<ECS::MaterialComponent>(e).baseColor = Math::Vector3(0.6f, 0.45f, 0.3f);
            m_World->AddComponent<ECS::WaterVehicleController>(e);
            FinishMenuEntity(e);
        });
    simple(kPlay, "Gravity Zone", nullptr, std::type_identity<ECS::GravityZoneComponent>{});

    // --- Rendering ---
    simple("Rendering", "Reflection Probe", nullptr, std::type_identity<ECS::ReflectionProbeComponent>{});
    simple("Rendering", "Post-Process Volume", nullptr, std::type_identity<ECS::PostProcessVolumeComponent>{});

    // --- Tilemap ---
    add("", "Tilemap", "A 16 x 10 grid. Pick a tileset in the inspector and paint.",
        [this](const Math::Vector3& at) {
            ECS::Entity e = MakeMenuEntity("Tilemap", at);
            auto& map = m_World->AddComponent<ECS::TilemapComponent>(e);
            // A grid to paint on. At 0 x 0 there was nothing to click.
            map.width = 16;
            map.height = 10;
            map.tiles.assign(static_cast<usize>(map.width) * map.height, -1);
            m_World->AddComponent<ECS::MaterialComponent>(e);
            FinishMenuEntity(e);
        });
    return t;
}

void EditorLayer::DrawEntityMenuGroup(const char* group) {
    if (!m_World) return;
    const Math::Vector3 spawn = EntitySpawnPosition();
    for (const EntityMenuEntry& e : BuildEntityMenuTable()) {
        if (std::strcmp(e.group, group) != 0) continue;
        if (ImGui::MenuItem(e.label)) e.create(spawn);
        if (e.tooltip) ImGui::SetItemTooltip("%s", e.tooltip);
    }
}

void EditorLayer::DrawEntityMenuCommon(const Math::Vector3&) {
    // Audio Source, Particle Emitter, Text, UI Canvas
    if (!m_World) return;
    const Math::Vector3 spawn = EntitySpawnPosition();
    for (const EntityMenuEntry& e : BuildEntityMenuTable()) {
        if (e.group[0] != '\0' || std::strcmp(e.label, "Tilemap") == 0) continue;
        if (ImGui::MenuItem(e.label)) e.create(spawn);
        if (e.tooltip) ImGui::SetItemTooltip("%s", e.tooltip);
    }
}

void EditorLayer::DrawEntityMenuGameplay(const Math::Vector3& spawn) {
    if (!m_World) return;
    for (const char* group : { "Player Character", "Gameplay", "Rendering" }) {
        if (!ImGui::BeginMenu(group)) continue;
        DrawEntityMenuGroup(group);
        ImGui::EndMenu();
    }
    for (const EntityMenuEntry& e : BuildEntityMenuTable()) {
        if (std::strcmp(e.label, "Tilemap") != 0) continue;
        if (ImGui::MenuItem(e.label)) e.create(spawn);
        if (e.tooltip) ImGui::SetItemTooltip("%s", e.tooltip);
    }

    if (ImGui::BeginMenu("Prefab")) {
        // Every prefab file in the project, sorted by path
        std::vector<std::filesystem::path> prefabs;
        const std::string manifest = m_SceneManager.GetProjectPath();
        if (!manifest.empty()) {
            std::error_code ec;
            const std::filesystem::path root = std::filesystem::path(manifest).parent_path();
            for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
                 !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
                if (it->is_regular_file(ec) && it->path().extension() == ".enjprefab")
                    prefabs.push_back(it->path());
                if (prefabs.size() >= 100) break;
            }
            std::sort(prefabs.begin(), prefabs.end());
        }
        if (prefabs.empty()) {
            ImGui::TextDisabled("No .enjprefab files in this project");
            ImGui::TextDisabled("Right-click an entity > Save as Prefab... to make one");
        }
        for (const auto& p : prefabs) {
            if (!ImGui::MenuItem(p.stem().string().c_str())) continue;
            auto prefab = Assets::PrefabManager::Get().LoadPrefab(p.string());
            if (!prefab) {
                ShowNotification("Could not load " + p.filename().string(), NotificationType::Error);
                continue;
            }
            ECS::Entity root = Assets::PrefabManager::Get().Instantiate(m_World, *prefab, spawn);
            if (root != ECS::INVALID_ENTITY) FinishMenuEntity(root);
        }
        ImGui::EndMenu();
    }
}

} // namespace Editor
} // namespace Enjin
