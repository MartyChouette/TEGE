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

#include "Enjin/Editor/EditorLayer.h"
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

namespace Enjin {
namespace Editor {

namespace {

// A capsule collider around the capsule mesh this file puts on a character:
// radius 0.3, total height 1.6 (CreateCapsule's radius and cylinder height,
// plus the two caps). Measured from the same numbers, not typed separately.
void CapsuleForCharacter(ECS::CapsuleColliderComponent& col) {
    col.radius = 0.3f;
    col.SetTotalHeight(1.0f + 2.0f * 0.3f);
}

}  // namespace

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

void EditorLayer::DrawEntityMenuCommon(const Math::Vector3& spawn) {
    if (!m_World) return;
    if (ImGui::MenuItem("Audio Source")) {
        ECS::Entity e = MakeMenuEntity("Audio Source", spawn);
        m_World->AddComponent<ECS::AudioSourceComponent>(e);
        FinishMenuEntity(e);
    }
    ImGui::SetItemTooltip("A 3D sound in the world. Pick its clip in the inspector.");
    if (ImGui::MenuItem("Particle Emitter")) {
        ECS::Entity e = MakeMenuEntity("Particle Emitter", spawn);
        m_World->AddComponent<ECS::ParticleEmitterComponent>(e);
        FinishMenuEntity(e);
    }
    if (ImGui::MenuItem("Text")) {
        ECS::Entity e = MakeMenuEntity("Text", spawn);
        auto& text = m_World->AddComponent<ECS::TextComponent>(e);
        text.text = "Text";
        FinishMenuEntity(e);
    }
    ImGui::SetItemTooltip("Text in the world. For screen text, use a UI Canvas.");
    if (ImGui::MenuItem("UI Canvas")) {
        ECS::Entity e = m_World->CreateEntity();
        m_World->AddComponent<ECS::NameComponent>(e, "UI Canvas");
        m_World->AddComponent<GUI::UICanvasComponent>(e, GUI::UICanvasComponent{});
        MarkDirty();
        OpenUIEditor(e);
    }
    ImGui::SetItemTooltip("Screen UI: menus, HUD, buttons. Opens in the UI editor.");
}

void EditorLayer::DrawEntityMenuGameplay(const Math::Vector3& spawn) {
    if (!m_World) return;

    if (ImGui::BeginMenu("Player Character")) {
        // A capsule you can see, the controller, a collider fitted to that
        // capsule, and the follow camera that controller type wants: the same
        // three things the inspector's "Basic Movement" card adds.
        struct Kind { const char* label; const char* controllerType; bool is2D; };
        static const Kind kKinds[] = {
            {"First Person",  "FirstPerson",  false},
            {"Third Person",  "ThirdPerson",  false},
            {"Top-Down 3D",   "TopDown3D",    false},
            {"Platformer 2D", "Platformer2D", true},
            {"Top-Down 2D",   "TopDown2D",    true},
        };
        for (const Kind& k : kKinds) {
            if (!ImGui::MenuItem(k.label)) continue;
            ECS::Entity e = MakeMenuEntity("Player", spawn);
            auto& mat = m_World->AddComponent<ECS::MaterialComponent>(e);
            mat.baseColor = Math::Vector3(0.35f, 0.55f, 0.85f);
            if (k.is2D) {
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule2D(0.8f, 1.6f));
                auto& box = m_World->AddComponent<ECS::BoxColliderComponent>(e);
                box.size = Math::Vector3(0.8f, 1.6f, 0.2f);
                if (std::string(k.controllerType) == "Platformer2D")
                    m_World->AddComponent<ECS::Platformer2DController>(e);
                else
                    m_World->AddComponent<ECS::TopDown2DController>(e);
            } else {
                m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCapsule(0.3f, 1.0f));
                CapsuleForCharacter(m_World->AddComponent<ECS::CapsuleColliderComponent>(e));
                const std::string type = k.controllerType;
                if (type == "FirstPerson")      m_World->AddComponent<ECS::FirstPersonController>(e);
                else if (type == "ThirdPerson") m_World->AddComponent<ECS::ThirdPersonController>(e);
                else                            m_World->AddComponent<ECS::TopDown3DController>(e);
            }
            SetupCameraForController(e, k.controllerType);
            FinishMenuEntity(e);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Gameplay")) {
        if (ImGui::MenuItem("Trigger Zone")) {
            ECS::Entity e = MakeMenuEntity("Trigger Zone", spawn);
            m_World->AddComponent<ECS::TriggerZoneComponent>(e);
            FinishMenuEntity(e);
        }
        ImGui::SetItemTooltip("Tells scripts and triggers when something enters or leaves it.");
        if (ImGui::MenuItem("Spawn Point")) {
            ECS::Entity e = MakeMenuEntity("Spawn Point", spawn);
            m_World->AddComponent<ECS::SpawnPointComponent>(e);
            FinishMenuEntity(e);
        }
        if (ImGui::MenuItem("Save Point")) {
            ECS::Entity e = MakeMenuEntity("Save Point", spawn);
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCylinder(0.4f, 0.2f));
            m_World->AddComponent<ECS::MaterialComponent>(e).baseColor = Math::Vector3(0.9f, 0.8f, 0.3f);
            m_World->AddComponent<ECS::SavePointComponent>(e);
            FinishMenuEntity(e);
        }
        if (ImGui::MenuItem("Door")) {
            // The Door component swings its OWN entity about that entity's
            // origin, so the entity is the hinge and the panel is a child
            // offset half a door-width from it -- the shape Playground's door
            // uses. A panel on the hinge entity itself would spin about its
            // middle. The panel is kinematic, so it pushes the player rather
            // than being pushed.
            ECS::Entity hinge = MakeMenuEntity("Door", spawn);
            m_World->AddComponent<ECS::DoorComponent>(hinge);
            ECS::Entity panel = MakeMenuEntity("Door Panel", Math::Vector3(0.5f, 1.0f, 0.0f));
            auto& ptf = *m_World->GetComponent<ECS::TransformComponent>(panel);
            ptf.scale = Math::Vector3(1.0f, 2.0f, 0.1f);
            m_World->AddComponent<ECS::MeshComponent>(panel, Renderer::MeshFactory::CreateCube(1.0f));
            m_World->AddComponent<ECS::MaterialComponent>(panel).baseColor = Math::Vector3(0.55f, 0.4f, 0.25f);
            m_World->AddComponent<ECS::BoxColliderComponent>(panel).size = Math::Vector3(1.0f, 2.0f, 0.1f);
            auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(panel);
            rb.bodyType = ECS::RigidbodyComponent::BodyType::Kinematic;
            rb.useGravity = false;
            ECS::SetParent(m_World, panel, hinge);
            RecordLayerCreate(panel);
            FinishMenuEntity(hinge);
        }
        ImGui::SetItemTooltip("Opens with Interact. The entity is the hinge; the panel is its child.");
        if (ImGui::MenuItem("Ladder")) {
            ECS::Entity e = MakeMenuEntity("Ladder", spawn);
            auto& ladder = m_World->AddComponent<ECS::LadderComponent>(e);
            // A mesh the size of the climbable volume, so what you see is what
            // you can climb
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCube(1.0f));
            m_World->GetComponent<ECS::TransformComponent>(e)->scale =
                Math::Vector3(ladder.halfExtents.x * 2.0f, ladder.halfExtents.y * 2.0f, 0.1f);
            m_World->AddComponent<ECS::MaterialComponent>(e).baseColor = Math::Vector3(0.5f, 0.35f, 0.2f);
            FinishMenuEntity(e);
        }
        if (ImGui::MenuItem("Rope")) {
            ECS::Entity e = MakeMenuEntity("Rope", spawn);
            m_World->AddComponent<ECS::RopeComponent>(e);
            FinishMenuEntity(e);
        }
        ImGui::SetItemTooltip("Hangs down from the entity and swings.");
        if (ImGui::MenuItem("Boat")) {
            ECS::Entity e = MakeMenuEntity("Boat", spawn);
            m_World->AddComponent<ECS::MeshComponent>(e, Renderer::MeshFactory::CreateCube(1.0f));
            m_World->GetComponent<ECS::TransformComponent>(e)->scale = Math::Vector3(1.2f, 0.4f, 2.5f);
            m_World->AddComponent<ECS::MaterialComponent>(e).baseColor = Math::Vector3(0.6f, 0.45f, 0.3f);
            m_World->AddComponent<ECS::WaterVehicleController>(e);
            FinishMenuEntity(e);
        }
        ImGui::SetItemTooltip("Drives on water. Put it on a Water Volume or Water 3D.");
        if (ImGui::MenuItem("Gravity Zone")) {
            ECS::Entity e = MakeMenuEntity("Gravity Zone", spawn);
            m_World->AddComponent<ECS::GravityZoneComponent>(e);
            FinishMenuEntity(e);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Rendering")) {
        if (ImGui::MenuItem("Reflection Probe")) {
            ECS::Entity e = MakeMenuEntity("Reflection Probe", spawn);
            m_World->AddComponent<ECS::ReflectionProbeComponent>(e);
            FinishMenuEntity(e);
        }
        if (ImGui::MenuItem("Post-Process Volume")) {
            ECS::Entity e = MakeMenuEntity("Post-Process Volume", spawn);
            m_World->AddComponent<ECS::PostProcessVolumeComponent>(e);
            FinishMenuEntity(e);
        }
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Tilemap")) {
        ECS::Entity e = MakeMenuEntity("Tilemap", spawn);
        auto& map = m_World->AddComponent<ECS::TilemapComponent>(e);
        // A grid to paint on. At 0 x 0 there was nothing to click.
        map.width = 16;
        map.height = 10;
        map.tiles.assign(static_cast<usize>(map.width) * map.height, -1);
        m_World->AddComponent<ECS::MaterialComponent>(e);
        FinishMenuEntity(e);
    }
    ImGui::SetItemTooltip("A 16 x 10 grid. Pick a tileset in the inspector and paint.");

    if (ImGui::BeginMenu("Prefab")) {
        // Every prefab file in the project, newest layout first by path
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
