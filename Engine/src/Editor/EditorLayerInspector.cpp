#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Editor/PlayModeDiff.h"
#include "Enjin/Physics/PhysicsBackendType.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/Flower.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Renderer/MeshSimplifier.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTReflections.h"
#include "Enjin/Renderer/RayTracing/RTAmbientOcclusion.h"
#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"
#include "Enjin/Renderer/RayTracing/OIDNDenoiser.h"
#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#include "Enjin/Renderer/RayTracing/AccelerationStructureManager.h"
#include "Enjin/Renderer/SHLightProbe.h"
#include "Enjin/Renderer/SDFScene.h"
#include "Enjin/Renderer/OITManager.h"
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Assets/FontLibrary.h"
#include "Enjin/Assets/AssetLibrary.h"
#include "Enjin/Assets/AssetMetadata.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/FileDialog.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Plugin/PluginRepository.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Renderer/NormalMapGenerator.h"
#include "Enjin/Editor/SpriteContourTracer.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/GUI/DialogueImportExport.h"
#include "Enjin/Assets/SWFLoader.h"
#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Effects/VoronoiMeshFracture.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/Math/Math.h"
#include <stb_image.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
// Undefine Windows macros that collide with engine methods
#undef LoadImage
#undef CreateWindow
#undef min
#undef max
#else
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <climits>
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Editor {

// --- Undo-aware component removal helper ---

template<typename T>
void EditorLayer::RemoveComponentWithUndo(ECS::Entity entity, const std::string& componentKey,
                                           const std::string& componentName) {
    auto cmd = std::make_unique<RemoveComponentCommand>(
        m_World, entity, componentKey, componentName,
        [this, entity]() {
            if (m_World->HasComponent<T>(entity))
                m_World->RemoveComponent<T>(entity);
        }
    );
    m_UndoRedo.Execute(std::move(cmd));
    ShowNotification("Removed " + componentName + " (Ctrl+Z to undo)", NotificationType::Info);
}

enum class DimensionTag : u8 { Any = 0, Only2D = 1, Only3D = 2 };

struct ComponentEntry {
    const char* displayName;
    const char* category;
    const char* controllerType; // non-null for controllers needing camera setup
    std::function<bool(ECS::World*, ECS::Entity)> hasComponent;
    std::function<void(ECS::World*, ECS::Entity)> addComponent;
    std::function<void(ECS::World*, ECS::Entity)> removeComponent; // for undo of add
    const char* componentKey; // JSON key for serialization (for undo of remove)
    DimensionTag dimension = DimensionTag::Any; // 2D/3D filtering
};

static const std::vector<ComponentEntry>& GetComponentEntries() {
    static const std::vector<ComponentEntry> entries = {
        // -- Rendering --
        {"Mesh", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::MeshComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::MeshComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::MeshComponent>(e); },
            "mesh", DimensionTag::Only3D},
        {"LOD (Auto-Generate)", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::LODComponent>(e) || !w->HasComponent<ECS::MeshComponent>(e); },
            [](ECS::World* w, ECS::Entity e) {
                auto* mesh = w->GetComponent<ECS::MeshComponent>(e);
                if (mesh && mesh->IsValid()) {
                    auto& lod = w->AddComponent<ECS::LODComponent>(e);
                    Renderer::MeshSimplifier::GenerateLODs(*mesh, lod);
                }
            },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::LODComponent>(e); },
            "lod", DimensionTag::Only3D},
        {"Material", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::MaterialComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::MaterialComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::MaterialComponent>(e); },
            "material"},
        {"Light", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::LightComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::LightComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::LightComponent>(e); },
            "light"},
        {"Camera", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::CameraComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::CameraComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::CameraComponent>(e); },
            "camera"},
        {"Text", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TextComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TextComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TextComponent>(e); },
            "text"},

        // -- Character Controller --
        {"2D Platformer", "Character Controller", "Platformer2D",
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::Platformer2DController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::Platformer2DController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::Platformer2DController>(e); },
            "platformer2D", DimensionTag::Only2D},
        {"2D Top-Down", "Character Controller", "TopDown2D",
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TopDown2DController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TopDown2DController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TopDown2DController>(e); },
            "topDown2D", DimensionTag::Only2D},
        {"3D Top-Down (Isometric)", "Character Controller", "TopDown3D",
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TopDown3DController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TopDown3DController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TopDown3DController>(e); },
            "topDown3D", DimensionTag::Only3D},
        {"3D Third Person", "Character Controller", "ThirdPerson",
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ThirdPersonController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ThirdPersonController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ThirdPersonController>(e); },
            "thirdPerson", DimensionTag::Only3D},
        {"3D First Person", "Character Controller", "FirstPerson",
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::FirstPersonController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::FirstPersonController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::FirstPersonController>(e); },
            "firstPerson", DimensionTag::Only3D},
        {"Vehicle", "Character Controller", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::VehicleController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::VehicleController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::VehicleController>(e); },
            "vehicle", DimensionTag::Only3D},
        {"Surface Aligned (Planet)", "Character Controller", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SurfaceAlignedController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SurfaceAlignedController>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SurfaceAlignedController>(e); },
            "surfaceAligned", DimensionTag::Only3D},

        // -- Physics --
        {"Rigidbody", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::RigidbodyComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::RigidbodyComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::RigidbodyComponent>(e); },
            "rigidbody"},
        {"Box Collider", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::BoxColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::BoxColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::BoxColliderComponent>(e); },
            "boxCollider"},
        {"Sphere Collider", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SphereColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SphereColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SphereColliderComponent>(e); },
            "sphereCollider"},
        {"Capsule Collider", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::CapsuleColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::CapsuleColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::CapsuleColliderComponent>(e); },
            "capsuleCollider"},
        {"Polygon Collider 2D", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::PolygonCollider2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::PolygonCollider2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::PolygonCollider2DComponent>(e); },
            "polygonCollider2D", DimensionTag::Only2D},
        {"Per-Frame Collider", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::PerFrameColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::PerFrameColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::PerFrameColliderComponent>(e); },
            "perFrameCollider", DimensionTag::Only2D},
        {"Body 2D", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<Physics::Body2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<Physics::Body2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<Physics::Body2DComponent>(e); },
            "body2D", DimensionTag::Only2D},
        {"Trigger Zone", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TriggerZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TriggerZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TriggerZoneComponent>(e); },
            "triggerZone"},

        // -- Gameplay --
        {"Health", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::HealthComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::HealthComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::HealthComponent>(e); },
            "health"},
        {"Damage", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DamageComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DamageComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DamageComponent>(e); },
            "damage"},
        {"Interactable", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::InteractableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::InteractableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::InteractableComponent>(e); },
            "interactable"},
        {"Pickup", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::PickupComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::PickupComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::PickupComponent>(e); },
            "pickup"},
        {"Inventory", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::InventoryComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::InventoryComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::InventoryComponent>(e); },
            "inventory"},
        {"Timer", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TimerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TimerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TimerComponent>(e); },
            "timer"},
        {"Possessable", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::PossessableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::PossessableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::PossessableComponent>(e); },
            "possessable"},
        {"Damage Resistance", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DamageResistanceComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DamageResistanceComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DamageResistanceComponent>(e); },
            "damageResistance"},
        {"Resource/Stamina", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ResourceComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ResourceComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ResourceComponent>(e); },
            "resource"},
        {"Footstep", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::FootstepComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::FootstepComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::FootstepComponent>(e); },
            "footstep"},
        {"Poolable", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::PoolableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::PoolableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::PoolableComponent>(e); },
            "poolable"},
        {"Quest State", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::QuestStateComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::QuestStateComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::QuestStateComponent>(e); },
            "questState"},
        {"Quest Flow", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::QuestFlowComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::QuestFlowComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::QuestFlowComponent>(e); },
            "questFlow"},
        {"HUD Widget", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::HUDWidgetComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::HUDWidgetComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::HUDWidgetComponent>(e); },
            "hudWidget"},
        {"UI Canvas", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<GUI::UICanvasComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<GUI::UICanvasComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<GUI::UICanvasComponent>(e); },
            "uiCanvas"},
        {"Cinematic Camera", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::CinematicCameraComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::CinematicCameraComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::CinematicCameraComponent>(e); },
            "cinematicCamera"},
        {"Tween", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TweenComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TweenComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TweenComponent>(e); },
            "tween"},
        {"State Machine", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::StateMachineComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::StateMachineComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::StateMachineComponent>(e); },
            "stateMachine"},
        {"Dialogue", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DialogueComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DialogueComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DialogueComponent>(e); },
            "dialogue"},
        {"Dialogue Box", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DialogueBoxComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DialogueBoxComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DialogueBoxComponent>(e); },
            "dialogueBox"},
        {"Save Data", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SaveDataComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SaveDataComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SaveDataComponent>(e); },
            "saveData"},
        {"Save/Load Menu", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SaveLoadMenuComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SaveLoadMenuComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SaveLoadMenuComponent>(e); },
            "saveLoadMenu"},
        {"Visual Script", "Scripting", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::VisualScriptComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::VisualScriptComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::VisualScriptComponent>(e); },
            "visualScript"},

        // -- Joints --
        {"Distance Joint", "Joints", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DistanceJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DistanceJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DistanceJointComponent>(e); },
            "distanceJoint"},
        {"Hinge Joint", "Joints", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::HingeJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::HingeJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::HingeJointComponent>(e); },
            "hingeJoint"},
        {"Ball-Socket Joint", "Joints", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::BallSocketJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::BallSocketJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::BallSocketJointComponent>(e); },
            "ballSocketJoint"},
        {"Spring Joint", "Joints", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SpringJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SpringJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SpringJointComponent>(e); },
            "springJoint"},
        {"Fixed Joint", "Joints", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::FixedJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::FixedJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::FixedJointComponent>(e); },
            "fixedJoint"},
        {"Slider Joint", "Joints", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SliderJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SliderJointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SliderJointComponent>(e); },
            "sliderJoint"},
        {"Ragdoll", "Joints", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::RagdollComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::RagdollComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::RagdollComponent>(e); },
            "ragdoll", DimensionTag::Only3D},

        // -- AI --
        {"AI Controller", "AI", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AIControllerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AIControllerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AIControllerComponent>(e); },
            "aiController"},
        {"Behavior Tree", "AI", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::BehaviorTreeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::BehaviorTreeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::BehaviorTreeComponent>(e); },
            "behaviorTree"},
        {"Follow Target", "AI", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::FollowTargetComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::FollowTargetComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::FollowTargetComponent>(e); },
            "followTarget"},
        {"Look At Target", "AI", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::LookAtTargetComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::LookAtTargetComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::LookAtTargetComponent>(e); },
            "lookAtTarget"},
        {"Waypoint", "AI", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::WaypointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::WaypointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::WaypointComponent>(e); },
            "waypoint"},

        // -- Audio --
        {"Audio Source", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AudioSourceComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AudioSourceComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AudioSourceComponent>(e); },
            "audioSource"},
        {"Audio Listener", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AudioListenerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AudioListenerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AudioListenerComponent>(e); },
            "audioListener"},

        // -- Visual --
        {"Billboard", "Visual", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::BillboardComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::BillboardComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::BillboardComponent>(e); },
            "billboard"},
        {"Particle Emitter", "Visual", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ParticleEmitterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ParticleEmitterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ParticleEmitterComponent>(e); },
            "particleEmitter"},

        // -- Effects --
        {"Weather Zone", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::WeatherZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::WeatherZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::WeatherZoneComponent>(e); },
            "weatherZone", DimensionTag::Only3D},
        {"Water Volume", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::WaterVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::WaterVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::WaterVolumeComponent>(e); },
            "waterVolume", DimensionTag::Only3D},
        {"Water 3D", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::Water3DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::Water3DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::Water3DComponent>(e); },
            "water3D", DimensionTag::Only3D},
        {"Grass Volume", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::GrassVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::GrassVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::GrassVolumeComponent>(e); },
            "grassVolume", DimensionTag::Only3D},
        {"Shrub Volume", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ShrubVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ShrubVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ShrubVolumeComponent>(e); },
            "shrubVolume", DimensionTag::Only3D},
        {"Tree Volume", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TreeVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TreeVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TreeVolumeComponent>(e); },
            "treeVolume", DimensionTag::Only3D},
        {"Vegetation", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::VegetationComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::VegetationComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::VegetationComponent>(e); },
            "vegetation", DimensionTag::Only3D},
        {"Camera Trigger", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::CameraTriggerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::CameraTriggerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::CameraTriggerComponent>(e); },
            "cameraTrigger", DimensionTag::Only3D},
        {"Temperature Zone", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TemperatureZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TemperatureZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TemperatureZoneComponent>(e); },
            "temperatureZone"},
        {"Gravity Zone", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::GravityZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::GravityZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::GravityZoneComponent>(e); },
            "gravityZone"},
        {"Fluid Volume", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::FluidVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::FluidVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::FluidVolumeComponent>(e); },
            "fluidVolume"},
        {"Post-Process Volume", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::PostProcessVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::PostProcessVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::PostProcessVolumeComponent>(e); },
            "postProcessVolume"},

        // -- 2D Graphics --
        {"Sprite", "2D Graphics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::Sprite2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::Sprite2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::Sprite2DComponent>(e); },
            "sprite2D", DimensionTag::Only2D},
        {"Animated Sprite", "2D Graphics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AnimatedSprite2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AnimatedSprite2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AnimatedSprite2DComponent>(e); },
            "animatedSprite2D", DimensionTag::Only2D},
        {"Tilemap", "2D Graphics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TilemapComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TilemapComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TilemapComponent>(e); },
            "tilemap", DimensionTag::Only2D},
        {"2D Camera Bounds", "2D Graphics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::Camera2DBoundsComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::Camera2DBoundsComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::Camera2DBoundsComponent>(e); },
            "camera2DBounds", DimensionTag::Only2D},

        // -- Scripting --
        {"Script", "Scripting", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ScriptComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ScriptComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ScriptComponent>(e); },
            "scriptComponent"},
        {"Notes", "Scripting", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::NotesComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::NotesComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::NotesComponent>(e); },
            "notes"},

        // -- Networking --
        {"Network Identity", "Networking", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::NetworkIdentityComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::NetworkIdentityComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::NetworkIdentityComponent>(e); },
            "networkIdentity"},
        {"Network Transform", "Networking", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::NetworkTransformComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::NetworkTransformComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::NetworkTransformComponent>(e); },
            "networkTransform"},

        // -- Puzzle --
        {"Lock", "Puzzle", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::LockComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::LockComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::LockComponent>(e); },
            "lock"},
        {"Pushable", "Puzzle", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::PushableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::PushableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::PushableComponent>(e); },
            "pushable"},
        {"Switch", "Puzzle", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SwitchComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SwitchComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SwitchComponent>(e); },
            "switch"},
        {"Goal Zone", "Puzzle", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::GoalZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::GoalZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::GoalZoneComponent>(e); },
            "goalZone"},
        {"Conveyor", "Puzzle", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ConveyorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ConveyorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ConveyorComponent>(e); },
            "conveyor"},
        {"Teleporter", "Puzzle", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TeleporterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TeleporterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TeleporterComponent>(e); },
            "teleporter"},
        {"Destructible", "Puzzle", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DestructibleComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DestructibleComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DestructibleComponent>(e); },
            "destructible"},
        {"Curl Noise Field", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::CurlNoiseFieldComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::CurlNoiseFieldComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::CurlNoiseFieldComponent>(e); },
            "curlNoiseField"},
        {"Fracture Config", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::FractureConfigComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::FractureConfigComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::FractureConfigComponent>(e); },
            "fractureConfig"},
        {"Moving Platform", "Puzzle", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::MovingPlatformComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::MovingPlatformComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::MovingPlatformComponent>(e); },
            "movingPlatform"},

        // -- 3D / Animation --
        {"Skeleton", "3D / Animation", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SkeletonComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SkeletonComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SkeletonComponent>(e); },
            "skeleton", DimensionTag::Only3D},
        {"Animator", "3D / Animation", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AnimatorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AnimatorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AnimatorComponent>(e); },
            "animator", DimensionTag::Only3D},
        {"Terrain", "3D / Animation", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TerrainComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TerrainComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TerrainComponent>(e); },
            "terrain", DimensionTag::Only3D},
        {"Terrain 2D", "3D / Animation", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::Terrain2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::Terrain2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::Terrain2DComponent>(e); },
            "terrain2D", DimensionTag::Only2D},
        {"Look-At IK", "3D / Animation", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::LookAtIKComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::LookAtIKComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::LookAtIKComponent>(e); },
            "lookAtIK", DimensionTag::Only3D},
        {"Interaction IK", "3D / Animation", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::InteractionIKComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::InteractionIKComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::InteractionIKComponent>(e); },
            "interactionIK", DimensionTag::Only3D},
        {"Flower Stem", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::FlowerStemComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::FlowerStemComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::FlowerStemComponent>(e); },
            "flowerStem"},
        {"Flower Particle Config", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::FlowerParticleConfigComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::FlowerParticleConfigComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::FlowerParticleConfigComponent>(e); },
            "flowerParticleConfig"},
        {"Jelly Mesh", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::JellyMeshComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::JellyMeshComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::JellyMeshComponent>(e); },
            "jellyMesh"},
        {"Tether", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TetherComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TetherComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TetherComponent>(e); },
            "tether"},
        {"Grabbable", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::GrabbableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::GrabbableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::GrabbableComponent>(e); },
            "grabbable"},

        // -- Scene --
        {"Streaming Volume", "Scene", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<Scene::StreamingVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<Scene::StreamingVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<Scene::StreamingVolumeComponent>(e); },
            "streamingVolume", DimensionTag::Only3D},
        {"Streaming Portal", "Scene", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<Scene::StreamingPortalComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<Scene::StreamingPortalComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<Scene::StreamingPortalComponent>(e); },
            "streamingPortal", DimensionTag::Only3D},

        // -- Effects --
        {"Interactive Water", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<Effects::InteractiveWaterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<Effects::InteractiveWaterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<Effects::InteractiveWaterComponent>(e); },
            "interactiveWater", DimensionTag::Only3D},
        {"Water Interactor", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<Effects::WaterInteractorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<Effects::WaterInteractorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<Effects::WaterInteractorComponent>(e); },
            "waterInteractor", DimensionTag::Only3D},

        // -- Other --
        {"Tags", "Other", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TagComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TagComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TagComponent>(e); },
            "tag"},
        {"Spawn Point", "Other", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SpawnPointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SpawnPointComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SpawnPointComponent>(e); },
            "spawnPoint"},
        {"Layer", "Other", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::LayerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::LayerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::LayerComponent>(e); },
            "layer"},
    };
    return entries;
}

static std::string ToLowerStr(const char* s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

static int LevenshteinDistance(const std::string& a, const std::string& b) {
    const int m = static_cast<int>(a.size());
    const int n = static_cast<int>(b.size());
    if (m == 0) return n;
    if (n == 0) return m;

    // Single-row DP
    std::vector<int> row(n + 1);
    for (int j = 0; j <= n; j++) row[j] = j;

    for (int i = 1; i <= m; i++) {
        int prev = row[0];
        row[0] = i;
        for (int j = 1; j <= n; j++) {
            int temp = row[j];
            if (a[i - 1] == b[j - 1]) {
                row[j] = prev;
            } else {
                row[j] = 1 + std::min({prev, row[j], row[j - 1]});
            }
            prev = temp;
        }
    }
    return row[n];
}

static bool MatchesWordBoundaries(const std::string& lowerText, const std::string& lowerQuery) {
    // Extract word-initial characters (start of string + chars after spaces)
    std::string initials;
    if (!lowerText.empty()) initials += lowerText[0];
    for (size_t i = 1; i < lowerText.size(); i++) {
        if (lowerText[i - 1] == ' ' && i < lowerText.size()) {
            initials += lowerText[i];
        }
    }
    // Check if query is a subsequence of initials
    if (lowerQuery.size() > initials.size()) return false;
    size_t qi = 0;
    for (size_t ii = 0; ii < initials.size() && qi < lowerQuery.size(); ii++) {
        if (initials[ii] == lowerQuery[qi]) qi++;
    }
    return qi == lowerQuery.size();
}

// Returns 0 for no match, higher scores for better matches
static int ScoreComponentMatch(const ComponentEntry& entry, const char* filter) {
    if (!filter || filter[0] == '\0') return 100;

    std::string lowerFilter = ToLowerStr(filter);
    std::string lowerName = ToLowerStr(entry.displayName);
    std::string lowerCat = ToLowerStr(entry.category);

    // Exact match
    if (lowerName == lowerFilter) return 100;

    // Prefix match
    if (lowerName.find(lowerFilter) == 0) return 90;

    // Name substring
    if (lowerName.find(lowerFilter) != std::string::npos) return 70;

    // Category substring
    if (lowerCat.find(lowerFilter) != std::string::npos) return 60;

    // Word boundary / initials match (e.g. "bc" -> "box collider")
    if (MatchesWordBoundaries(lowerName, lowerFilter)) return 50;

    // Fuzzy matching via Levenshtein distance
    int maxDist = (static_cast<int>(lowerFilter.size()) <= 4) ? 2 : static_cast<int>(lowerFilter.size()) / 2;

    // Check against full name
    int dist = LevenshteinDistance(lowerFilter, lowerName);
    if (dist <= maxDist) {
        return 35 - (dist * 10); // dist=1 -> 25, dist=2 -> 15
    }

    // Check against individual words in the name (e.g. "colider" matching "Collider" in "Box Collider")
    std::istringstream iss(lowerName);
    std::string word;
    int bestWordDist = maxDist + 1;
    while (iss >> word) {
        int wDist = LevenshteinDistance(lowerFilter, word);
        if (wDist < bestWordDist) bestWordDist = wDist;
    }
    if (bestWordDist <= maxDist) {
        return 35 - (bestWordDist * 10);
    }

    return 0; // No match
}


void EditorLayer::DrawInspectorPanel() {
    ImGuiWindowFlags flags = 0;
    if (m_FocusMode || !m_PlayMode.IsStopped()) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    ImGui::Begin("Inspector", nullptr, flags);

    // Focus ring for keyboard navigation
    if (m_ShowFocusRing && m_FocusedPanel == FocusedPanel::Inspector) {
        ImVec2 wMin = ImGui::GetWindowPos();
        ImVec2 wMax = ImVec2(wMin.x + ImGui::GetWindowWidth(), wMin.y + ImGui::GetWindowHeight());
        ImGui::GetWindowDrawList()->AddRect(wMin, wMax, IM_COL32(100, 200, 255, 200), 0.0f, 0, 2.0f);
        ImGui::SetWindowFocus();
    }

    // Show lock warning if entity is locked by another user
    if (m_PrimarySelected != ECS::INVALID_ENTITY && m_SceneLockManager.IsEntityLockedByOther(m_PrimarySelected)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        const auto* lockInfo = m_SceneLockManager.GetEntityLockInfo(m_PrimarySelected);
        if (lockInfo) {
            ImGui::Text("[X] Locked by %s@%s", lockInfo->user.c_str(), lockInfo->machine.c_str());
        } else {
            ImGui::Text("[X] Locked by another user");
        }
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    if (m_SelectedEntities.size() > 1 && m_World) {
        // Multi-select inspector
        DrawMultiSelectInspector();
    } else if (m_PrimarySelected != ECS::INVALID_ENTITY && m_World) {
        // Entity name (editable)
        ECS::NameComponent* nameComp = m_World->GetComponent<ECS::NameComponent>(m_PrimarySelected);
        if (nameComp) {
            char nameBuffer[256];
            strncpy(nameBuffer, nameComp->name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##EntityName", nameBuffer, sizeof(nameBuffer))) {
                std::string oldName = nameComp->name;
                nameComp->name = nameBuffer;
                if (m_CollabSystem.IsActive()) {
                    m_CollabSystem.OnEntityRenamed(m_PrimarySelected, oldName, nameComp->name);
                }
            }
        } else {
            // No name component - show entity ID and add button
            ImGui::Text("Entity %llu", (unsigned long long)m_PrimarySelected);
            ImGui::SameLine();
            if (ImGui::SmallButton("Add Name")) {
                m_World->AddComponent<ECS::NameComponent>(m_PrimarySelected, "Unnamed");
            }
        }
        // Prefab instance indicator
        if (Assets::PrefabUtils::IsPrefabInstance(m_World, m_PrimarySelected)) {
            auto* prefabInst = m_World->GetComponent<Assets::PrefabInstanceComponent>(m_PrimarySelected);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
            ImGui::Text("Prefab Instance");
            ImGui::PopStyleColor();
            if (prefabInst && !prefabInst->prefabPath.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", std::filesystem::path(prefabInst->prefabPath).filename().string().c_str());
            }
        }
        ImGui::Separator();

        // Transform component
        if (m_World->HasComponent<ECS::TransformComponent>(m_PrimarySelected)) {
            DrawTransformComponent(m_PrimarySelected);
        }

        // Mesh component
        if (m_World->HasComponent<ECS::MeshComponent>(m_PrimarySelected)) {
            DrawMeshComponent(m_PrimarySelected);
        }

        // LOD component
        if (m_World->HasComponent<ECS::LODComponent>(m_PrimarySelected)) {
            DrawLODComponent(m_PrimarySelected);
        }

        // Material component
        if (m_World->HasComponent<ECS::MaterialComponent>(m_PrimarySelected)) {
            DrawMaterialComponent(m_PrimarySelected);
        }

        // Light component
        if (m_World->HasComponent<ECS::LightComponent>(m_PrimarySelected)) {
            DrawLightComponent(m_PrimarySelected);
        }

        // Camera component
        if (m_World->HasComponent<ECS::CameraComponent>(m_PrimarySelected)) {
            DrawCameraComponent(m_PrimarySelected);
        }

        // Weather Zone component
        if (m_World->HasComponent<ECS::WeatherZoneComponent>(m_PrimarySelected)) {
            DrawWeatherZoneComponent(m_PrimarySelected);
        }

        // Water Volume component
        if (m_World->HasComponent<ECS::WaterVolumeComponent>(m_PrimarySelected)) {
            DrawWaterVolumeComponent(m_PrimarySelected);
        }

        // Water 3D component
        if (m_World->HasComponent<ECS::Water3DComponent>(m_PrimarySelected)) {
            DrawWater3DComponent(m_PrimarySelected);
        }

        // Grass Volume component
        if (m_World->HasComponent<ECS::GrassVolumeComponent>(m_PrimarySelected)) {
            DrawGrassVolumeComponent(m_PrimarySelected);
        }

        // Shrub Volume component
        if (m_World->HasComponent<ECS::ShrubVolumeComponent>(m_PrimarySelected)) {
            DrawShrubVolumeComponent(m_PrimarySelected);
        }

        // Tree Volume component
        if (m_World->HasComponent<ECS::TreeVolumeComponent>(m_PrimarySelected)) {
            DrawTreeVolumeComponent(m_PrimarySelected);
        }

        // 3D Terrain component
        if (m_World->HasComponent<ECS::TerrainComponent>(m_PrimarySelected)) {
            DrawTerrainComponent(m_PrimarySelected);
        }

        // 2D Terrain component
        if (m_World->HasComponent<ECS::Terrain2DComponent>(m_PrimarySelected)) {
            DrawTerrain2DComponent(m_PrimarySelected);
        }

        // Vegetation component
        if (m_World->HasComponent<ECS::VegetationComponent>(m_PrimarySelected)) {
            DrawVegetationComponent(m_PrimarySelected);
        }

        // Camera Trigger component
        if (m_World->HasComponent<ECS::CameraTriggerComponent>(m_PrimarySelected)) {
            DrawCameraTriggerComponent(m_PrimarySelected);
        }

        // Temperature Zone component
        if (m_World->HasComponent<ECS::TemperatureZoneComponent>(m_PrimarySelected)) {
            DrawTemperatureZoneComponent(m_PrimarySelected);
        }

        // Gravity Zone component
        if (m_World->HasComponent<ECS::GravityZoneComponent>(m_PrimarySelected)) {
            DrawGravityZoneComponent(m_PrimarySelected);
        }

        // Post-Process Volume component
        if (m_World->HasComponent<ECS::PostProcessVolumeComponent>(m_PrimarySelected)) {
            DrawPostProcessVolumeComponent(m_PrimarySelected);
        }

        // Fluid Volume component
        if (m_World->HasComponent<ECS::FluidVolumeComponent>(m_PrimarySelected)) {
            DrawFluidVolumeComponent(m_PrimarySelected);

            // Show coupling UI when entity also has a terrain
            if (m_World->HasComponent<ECS::TerrainComponent>(m_PrimarySelected)) {
                DrawFluidTerrainCoupling(m_PrimarySelected);
            }
        }

        // Notes component
        if (m_World->HasComponent<ECS::NotesComponent>(m_PrimarySelected)) {
            DrawNotesComponent(m_PrimarySelected);
        }

        // Text component
        if (m_World->HasComponent<ECS::TextComponent>(m_PrimarySelected)) {
            DrawTextComponent(m_PrimarySelected);
        }

        // Character Controller components
        if (m_World->HasComponent<ECS::Platformer2DController>(m_PrimarySelected)) {
            DrawPlatformer2DController(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::TopDown2DController>(m_PrimarySelected)) {
            DrawTopDown2DController(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::TopDown3DController>(m_PrimarySelected)) {
            DrawTopDown3DController(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::ThirdPersonController>(m_PrimarySelected)) {
            DrawThirdPersonController(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::FirstPersonController>(m_PrimarySelected)) {
            DrawFirstPersonController(m_PrimarySelected);
        }

        // Gameplay components
        if (m_World->HasComponent<ECS::HealthComponent>(m_PrimarySelected)) {
            DrawHealthComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::RigidbodyComponent>(m_PrimarySelected)) {
            DrawRigidbodyComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::BoxColliderComponent>(m_PrimarySelected)) {
            DrawBoxColliderComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SphereColliderComponent>(m_PrimarySelected)) {
            DrawSphereColliderComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::CapsuleColliderComponent>(m_PrimarySelected)) {
            DrawCapsuleColliderComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::PolygonCollider2DComponent>(m_PrimarySelected)) {
            DrawPolygonCollider2DComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::PerFrameColliderComponent>(m_PrimarySelected)) {
            DrawPerFrameColliderComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<Physics::Body2DComponent>(m_PrimarySelected)) {
            DrawBody2DComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::TriggerZoneComponent>(m_PrimarySelected)) {
            DrawTriggerZoneComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::DamageComponent>(m_PrimarySelected)) {
            DrawDamageComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::InteractableComponent>(m_PrimarySelected)) {
            DrawInteractableComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::PickupComponent>(m_PrimarySelected)) {
            DrawPickupComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::InventoryComponent>(m_PrimarySelected)) {
            DrawInventoryComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::TimerComponent>(m_PrimarySelected)) {
            DrawTimerComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AudioSourceComponent>(m_PrimarySelected)) {
            DrawAudioSourceComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AudioListenerComponent>(m_PrimarySelected)) {
            DrawAudioListenerComponent(m_PrimarySelected);
        }

        // AI components
        if (m_World->HasComponent<ECS::AIControllerComponent>(m_PrimarySelected)) {
            DrawAIControllerComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::BehaviorTreeComponent>(m_PrimarySelected)) {
            DrawBehaviorTreeComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::FollowTargetComponent>(m_PrimarySelected)) {
            DrawFollowTargetComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::LookAtTargetComponent>(m_PrimarySelected)) {
            DrawLookAtTargetComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::WaypointComponent>(m_PrimarySelected)) {
            DrawWaypointComponent(m_PrimarySelected);
        }

        // IK Components
        if (m_World->HasComponent<ECS::LookAtIKComponent>(m_PrimarySelected)) {
            if (ImGui::CollapsingHeader("Look-At IK")) {
                auto* ik = m_World->GetComponent<ECS::LookAtIKComponent>(m_PrimarySelected);
                char headBone[128];
                strncpy(headBone, ik->headBoneName.c_str(), sizeof(headBone) - 1);
                headBone[sizeof(headBone) - 1] = '\0';
                if (ImGui::InputText("Head Bone", headBone, sizeof(headBone))) {
                    ik->headBoneName = headBone;
                }
                char neckBone[128];
                strncpy(neckBone, ik->neckBoneName.c_str(), sizeof(neckBone) - 1);
                neckBone[sizeof(neckBone) - 1] = '\0';
                if (ImGui::InputText("Neck Bone", neckBone, sizeof(neckBone))) {
                    ik->neckBoneName = neckBone;
                }
                ImGui::SliderFloat("Max Rotation", &ik->maxRotation, 0.0f, 90.0f);
                ImGui::SliderFloat("Smooth Speed##LookAtIK", &ik->smoothSpeed, 0.1f, 20.0f);
                ImGui::SliderFloat("Look Weight", &ik->lookWeight, 0.0f, 1.0f);
                ImGui::DragFloat3("Target Pos", &ik->targetWorldPos.x, 0.1f);
            }
        }

        if (m_World->HasComponent<ECS::InteractionIKComponent>(m_PrimarySelected)) {
            if (ImGui::CollapsingHeader("Interaction IK")) {
                auto* ik = m_World->GetComponent<ECS::InteractionIKComponent>(m_PrimarySelected);
                char handBone[128];
                strncpy(handBone, ik->handBoneName.c_str(), sizeof(handBone) - 1);
                handBone[sizeof(handBone) - 1] = '\0';
                if (ImGui::InputText("Hand Bone", handBone, sizeof(handBone))) {
                    ik->handBoneName = handBone;
                }
                char elbowBone[128];
                strncpy(elbowBone, ik->elbowBoneName.c_str(), sizeof(elbowBone) - 1);
                elbowBone[sizeof(elbowBone) - 1] = '\0';
                if (ImGui::InputText("Elbow Bone", elbowBone, sizeof(elbowBone))) {
                    ik->elbowBoneName = elbowBone;
                }
                char shoulderBone[128];
                strncpy(shoulderBone, ik->shoulderBoneName.c_str(), sizeof(shoulderBone) - 1);
                shoulderBone[sizeof(shoulderBone) - 1] = '\0';
                if (ImGui::InputText("Shoulder Bone", shoulderBone, sizeof(shoulderBone))) {
                    ik->shoulderBoneName = shoulderBone;
                }
                ImGui::SliderFloat("Radius", &ik->interactionRadius, 0.1f, 10.0f);
                ImGui::SliderFloat("IK Weight", &ik->ikWeight, 0.0f, 1.0f);
                ImGui::SliderFloat("Smooth Speed##InteractionIK", &ik->smoothSpeed, 0.1f, 20.0f);
                char ikTag[128];
                strncpy(ikTag, ik->interactionTag.c_str(), sizeof(ikTag) - 1);
                ikTag[sizeof(ikTag) - 1] = '\0';
                if (ImGui::InputText("Interaction Tag", ikTag, sizeof(ikTag))) {
                    ik->interactionTag = ikTag;
                }
            }
        }

        // Visual components
        if (m_World->HasComponent<ECS::BillboardComponent>(m_PrimarySelected)) {
            DrawBillboardComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::ParticleEmitterComponent>(m_PrimarySelected)) {
            DrawParticleEmitterComponent(m_PrimarySelected);
        }

        // 2D components
        if (m_World->HasComponent<ECS::Sprite2DComponent>(m_PrimarySelected)) {
            DrawSprite2DComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AnimatedSprite2DComponent>(m_PrimarySelected)) {
            DrawAnimatedSprite2DComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::TilemapComponent>(m_PrimarySelected)) {
            DrawTilemapComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::StateMachineComponent>(m_PrimarySelected)) {
            DrawStateMachineComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::Camera2DBoundsComponent>(m_PrimarySelected)) {
            DrawCamera2DBoundsComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::DialogueComponent>(m_PrimarySelected)) {
            DrawDialogueComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::DialogueBoxComponent>(m_PrimarySelected)) {
            DrawDialogueBoxComponent(m_PrimarySelected);
        }

        // Other components
        if (m_World->HasComponent<ECS::TagComponent>(m_PrimarySelected)) {
            DrawTagComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SpawnPointComponent>(m_PrimarySelected)) {
            DrawSpawnPointComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::LayerComponent>(m_PrimarySelected)) {
            DrawLayerComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SaveDataComponent>(m_PrimarySelected)) {
            DrawSaveDataComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SaveLoadMenuComponent>(m_PrimarySelected)) {
            DrawSaveLoadMenuComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SkeletonComponent>(m_PrimarySelected)) {
            DrawSkeletonComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<Scene::StreamingVolumeComponent>(m_PrimarySelected)) {
            DrawStreamingVolumeComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<Scene::StreamingPortalComponent>(m_PrimarySelected)) {
            DrawStreamingPortalComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<Effects::InteractiveWaterComponent>(m_PrimarySelected)) {
            bool waterOpen = ImGui::CollapsingHeader("[~] Interactive Water", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem("InteractiveWaterCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<Effects::InteractiveWaterComponent>(m_PrimarySelected, "interactiveWater", "Interactive Water");
                    ImGui::EndPopup();
                } else {
                    ImGui::EndPopup();
                }
            }
            if (waterOpen) {
                auto* iw = m_World->GetComponent<Effects::InteractiveWaterComponent>(m_PrimarySelected);
                if (iw) {
                    ImGui::Text("Grid");
                    ImGui::DragInt("Resolution", &iw->gridResolution, 1, 16, 256);
                    ImGui::DragFloat("Grid Size", &iw->gridSize, 0.1f, 1.0f, 200.0f);
                    ImGui::DragFloat("Base Height", &iw->baseHeight, 0.1f, -100.0f, 100.0f);
                    ImGui::Separator();
                    ImGui::Text("Wave Simulation");
                    ImGui::DragFloat("Wave Speed", &iw->waveSpeed, 0.05f, 0.0f, 20.0f);
                    ImGui::DragFloat("Damping", &iw->damping, 0.001f, 0.9f, 0.999f);
                    ImGui::DragFloat("Tension", &iw->tension, 0.01f, 0.0f, 1.0f);
                    ImGui::Separator();
                    ImGui::Text("Appearance");
                    ImGui::ColorEdit3("Shallow Color", &iw->shallowColor.x);
                    ImGui::ColorEdit3("Deep Color", &iw->deepColor.x);
                    ImGui::ColorEdit3("Foam Color", &iw->foamColor.x);
                    ImGui::DragFloat("Opacity", &iw->opacity, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Depth Threshold", &iw->depthColorThreshold, 0.1f, 0.0f, 20.0f);
                    ImGui::DragFloat("Foam Threshold", &iw->foamThreshold, 0.01f, 0.0f, 2.0f);
                    ImGui::DragFloat("UV Tiling", &iw->uvTiling, 0.1f, 0.1f, 20.0f);
                    ImGui::DragFloat("UV Scroll Speed", &iw->uvScrollSpeed, 0.001f, 0.0f, 1.0f);
                    ImGui::Separator();
                    ImGui::Text("Interaction");
                    ImGui::DragFloat("Interaction Radius", &iw->interactionRadius, 0.1f, 0.1f, 10.0f);
                    ImGui::DragFloat("Interaction Strength", &iw->interactionStrength, 0.01f, 0.0f, 5.0f);
                    ImGui::Checkbox("Enable Buoyancy", &iw->enableBuoyancy);
                    if (iw->enableBuoyancy) {
                        ImGui::DragFloat("Buoyancy Force", &iw->buoyancyForce, 0.1f, 0.0f, 50.0f);
                        ImGui::DragFloat("Water Drag", &iw->waterDrag, 0.01f, 0.0f, 5.0f);
                    }
                    ImGui::Separator();
                    const char* boundaryModes[] = { "Absorbing", "Reflecting" };
                    int bm = static_cast<int>(iw->boundaryMode);
                    if (ImGui::Combo("Boundary Mode", &bm, boundaryModes, 2)) {
                        iw->boundaryMode = static_cast<Effects::InteractiveWaterComponent::BoundaryMode>(bm);
                    }
                }
            }
        }
        if (m_World->HasComponent<Effects::WaterInteractorComponent>(m_PrimarySelected)) {
            bool interactorOpen = ImGui::CollapsingHeader("[~] Water Interactor", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem("WaterInteractorCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<Effects::WaterInteractorComponent>(m_PrimarySelected, "waterInteractor", "Water Interactor");
                    ImGui::EndPopup();
                } else {
                    ImGui::EndPopup();
                }
            }
            if (interactorOpen) {
                auto* wi = m_World->GetComponent<Effects::WaterInteractorComponent>(m_PrimarySelected);
                if (wi) {
                    ImGui::DragFloat("Splash Multiplier", &wi->splashMultiplier, 0.1f, 0.0f, 10.0f);
                    ImGui::DragFloat("Wake Width", &wi->wakeWidth, 0.1f, 0.0f, 5.0f);
                    ImGui::Checkbox("Generate Wake", &wi->generateWake);
                    ImGui::Checkbox("Apply Buoyancy", &wi->applyBuoyancy);
                }
            }
        }
        if (m_World->HasComponent<ECS::AnimatorComponent>(m_PrimarySelected)) {
            // Animator component inspector (inline)
            bool animOpen = ImGui::CollapsingHeader("[>] Animator", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem("AnimatorCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::AnimatorComponent>(m_PrimarySelected, "animator", "Animator");
                    ImGui::EndPopup();
                } else {
                    ImGui::EndPopup();
                }
            }
            if (animOpen) {
                auto* animComp = m_World->GetComponent<ECS::AnimatorComponent>(m_PrimarySelected);
                if (animComp) {
                    auto& animator = animComp->animator;
                    const auto& animations = animator.GetAnimations();

                    // Current animation name (read-only)
                    const auto& currentName = animator.GetCurrentAnimationName();
                    ImGui::Text("Current: %s", currentName.empty() ? "(none)" : currentName.c_str());

                    // Playing state indicator
                    bool isPlaying = animator.IsPlaying();
                    ImGui::Text("State: %s", isPlaying ? "Playing" : "Stopped");
                    if (animator.IsBlending()) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("(blending)");
                    }

                    // Normalized time progress bar
                    f32 normalizedTime = animator.GetNormalizedTime();
                    ImGui::ProgressBar(normalizedTime, ImVec2(-1, 0));

                    // Speed
                    f32 speed = animator.GetSpeed();
                    if (ImGui::DragFloat("Speed##Animator", &speed, 0.01f, 0.0f, 10.0f, "%.2f")) {
                        animator.SetSpeed(speed);
                    }

                    // Play / Stop buttons
                    if (isPlaying) {
                        if (ImGui::Button("Stop##Animator")) {
                            animator.Stop();
                        }
                    } else {
                        if (!currentName.empty()) {
                            if (ImGui::Button("Play##Animator")) {
                                animator.Play(currentName, 0.0f);
                            }
                        }
                    }

                    // Available animations list
                    if (!animations.empty() && ImGui::TreeNode("Animations")) {
                        for (const auto& [name, anim] : animations) {
                            bool isCurrent = (name == currentName);
                            if (isCurrent) {
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
                            }
                            ImGui::Text("%s (%.2fs, %zu tracks)", name.c_str(),
                                        anim.duration, anim.tracks.size());
                            if (isCurrent) {
                                ImGui::PopStyleColor();
                            }
                            ImGui::SameLine();
                            std::string playLabel = "Play##" + name;
                            if (ImGui::SmallButton(playLabel.c_str())) {
                                animator.Play(name, 0.2f);
                            }
                        }
                        ImGui::TreePop();
                    } else if (animations.empty()) {
                        ImGui::TextDisabled("No animations loaded");
                    }

                    // State machine info
                    const auto& stateMachine = animComp->stateMachine;
                    const auto& currentState = stateMachine.GetCurrentState();
                    if (!stateMachine.GetStates().empty() && ImGui::TreeNode("State Machine")) {
                        ImGui::Text("Current State: %s", currentState.empty() ? "(none)" : currentState.c_str());
                        ImGui::Text("Default State: %s", stateMachine.GetDefaultState().empty() ? "(none)" : stateMachine.GetDefaultState().c_str());
                        ImGui::Text("States: %zu", stateMachine.GetStates().size());
                        ImGui::Text("Transitions: %zu", stateMachine.GetTransitions().size());
                        ImGui::TreePop();
                    }
                }
            }
        }

        // Flower components
        if (m_World->HasComponent<ECS::JellyMeshComponent>(m_PrimarySelected)) {
            DrawJellyMeshComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::TetherComponent>(m_PrimarySelected)) {
            DrawTetherComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::GrabbableComponent>(m_PrimarySelected)) {
            DrawGrabbableComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::FlowerStemComponent>(m_PrimarySelected)) {
            DrawFlowerStemComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::FlowerParticleConfigComponent>(m_PrimarySelected)) {
            DrawFlowerParticleConfigComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::ScriptComponent>(m_PrimarySelected)) {
            DrawScriptComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::VehicleController>(m_PrimarySelected)) {
            DrawVehicleController(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SurfaceAlignedController>(m_PrimarySelected)) {
            DrawSurfaceAlignedController(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::PossessableComponent>(m_PrimarySelected)) {
            DrawPossessableComponent(m_PrimarySelected);
        }

        // Puzzle components
        if (m_World->HasComponent<ECS::LockComponent>(m_PrimarySelected)) {
            DrawLockComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::PushableComponent>(m_PrimarySelected)) {
            DrawPushableComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SwitchComponent>(m_PrimarySelected)) {
            DrawSwitchComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::GoalZoneComponent>(m_PrimarySelected)) {
            DrawGoalZoneComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::ConveyorComponent>(m_PrimarySelected)) {
            DrawConveyorComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::TeleporterComponent>(m_PrimarySelected)) {
            DrawTeleporterComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::DestructibleComponent>(m_PrimarySelected)) {
            DrawDestructibleComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::CurlNoiseFieldComponent>(m_PrimarySelected)) {
            DrawCurlNoiseFieldComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::FractureConfigComponent>(m_PrimarySelected)) {
            DrawFractureConfigComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::MovingPlatformComponent>(m_PrimarySelected)) {
            DrawMovingPlatformComponent(m_PrimarySelected);
        }

        // New gameplay components
        if (m_World->HasComponent<ECS::DamageResistanceComponent>(m_PrimarySelected)) {
            DrawDamageResistanceComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::ResourceComponent>(m_PrimarySelected)) {
            DrawResourceComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::FootstepComponent>(m_PrimarySelected)) {
            DrawFootstepComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::PoolableComponent>(m_PrimarySelected)) {
            DrawPoolableComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::QuestStateComponent>(m_PrimarySelected)) {
            DrawQuestStateComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::QuestFlowComponent>(m_PrimarySelected)) {
            DrawQuestFlowComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::HUDWidgetComponent>(m_PrimarySelected)) {
            DrawHUDWidgetComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<GUI::UICanvasComponent>(m_PrimarySelected)) {
            DrawUICanvasComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::CinematicCameraComponent>(m_PrimarySelected)) {
            DrawCinematicCameraComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::TweenComponent>(m_PrimarySelected)) {
            DrawTweenComponent(m_PrimarySelected);
        }

        // Networking components
        if (m_World->HasComponent<ECS::NetworkIdentityComponent>(m_PrimarySelected)) {
            DrawNetworkIdentityComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::NetworkTransformComponent>(m_PrimarySelected)) {
            DrawNetworkTransformComponent(m_PrimarySelected);
        }

        // Joint & Ragdoll components
        if (m_World->HasComponent<ECS::DistanceJointComponent>(m_PrimarySelected)) {
            DrawDistanceJointComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::HingeJointComponent>(m_PrimarySelected)) {
            DrawHingeJointComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::BallSocketJointComponent>(m_PrimarySelected)) {
            DrawBallSocketJointComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SpringJointComponent>(m_PrimarySelected)) {
            DrawSpringJointComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::FixedJointComponent>(m_PrimarySelected)) {
            DrawFixedJointComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SliderJointComponent>(m_PrimarySelected)) {
            DrawSliderJointComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::RagdollComponent>(m_PrimarySelected)) {
            DrawRagdollComponent(m_PrimarySelected);
        }

        DrawSmartSuggestions(m_PrimarySelected);
        ImGui::Separator();
        DrawQuickSetup(m_PrimarySelected);
        ImGui::Separator();

        // Animate button — opens Flash Timeline and sets up recording
        if (ImGui::Button("Animate")) {
            // Open FlashTimeline panel if not visible
            SetPanelVisibility(EditorPanel::FlashTimeline, true);

            // Initialize timeline data if needed
            if (m_FlashTimelineEditor.GetTimeline() == nullptr) {
                m_FlashTimelineEditor.SetTimeline(&m_FlashTimelineData);
            }

            // Find or create a layer for the selected entity
            bool hasLayer = false;
            for (auto& layer : m_FlashTimelineData.layers) {
                if (layer.entity == m_PrimarySelected) {
                    hasLayer = true;
                    break;
                }
            }
            if (!hasLayer) {
                auto* nameComp = m_World->GetComponent<ECS::NameComponent>(m_PrimarySelected);
                std::string layerName = nameComp ? nameComp->name : "Entity " + std::to_string(m_PrimarySelected);
                m_FlashTimelineData.AddLayer(layerName, m_PrimarySelected);

                // Create initial keyframe at frame 0 from current transform
                auto& newLayer = m_FlashTimelineData.layers.back();
                auto* tf = m_World->GetComponent<ECS::TransformComponent>(m_PrimarySelected);
                if (tf) {
                    Editor::FlashKeyframe& kf = newLayer.GetOrCreateKeyframe(0);
                    kf.position = tf->position;
                    kf.rotation = tf->rotation.ToEuler();
                    kf.scale = tf->scale;
                }
            }

            // Enable record mode
            m_FlashTimelineEditor.SetRecordMode(true);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("?");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Open animation timeline for this entity.\n"
                              "Pose the entity with gizmos and keyframes\n"
                              "are captured automatically in record mode.");
        }
        ImGui::Separator();

        // Add component button
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        ImGui::SetNextWindowSizeConstraints(ImVec2(300, 0), ImVec2(400, 500));
        if (ImGui::BeginPopup("AddComponentPopup")) {
            const auto& allEntries = GetComponentEntries();
            bool hasFilter = m_ComponentSearchBuf[0] != '\0';

            // Auto-focus search field on first frame
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }

            // Track previous filter to detect changes
            char prevFilter[256];
            std::memcpy(prevFilter, m_ComponentSearchBuf, sizeof(prevFilter));

            auto projectMode = m_SceneManager.GetProjectMode();
            bool showDimToggle = projectMode != Scene::ProjectMode::Mixed;

            ImGui::SetNextItemWidth(showDimToggle ? -50.0f : -1.0f);
            ImGui::InputTextWithHint("##ComponentSearch", "Search components...", m_ComponentSearchBuf, sizeof(m_ComponentSearchBuf));

            // "All" toggle to bypass dimension filtering
            if (showDimToggle) {
                ImGui::SameLine();
                ImGui::Checkbox("All", &m_ShowAllComponents);
                if (ImGui::IsItemHovered()) {
                    const char* modeName = projectMode == Scene::ProjectMode::Mode2D ? "2D" : "3D";
                    ImGui::SetTooltip("Show components from all dimensions\n(project is set to %s mode)", modeName);
                }
            }

            // Reset selection index when filter changes
            if (std::strcmp(prevFilter, m_ComponentSearchBuf) != 0) {
                m_ComponentSearchSelectedIndex = 0;
            }

            ImGui::Separator();

            // Build visible entries list (not already on entity, matches filter)
            struct VisibleEntry {
                int originalIndex;
                const ComponentEntry* entry;
                int score;
            };
            std::vector<VisibleEntry> visible;
            for (int i = 0; i < static_cast<int>(allEntries.size()); i++) {
                const auto& e = allEntries[i];
                if (e.hasComponent(m_World, m_PrimarySelected)) continue;
                // Dimension filtering
                if (!m_ShowAllComponents && projectMode != Scene::ProjectMode::Mixed) {
                    if (projectMode == Scene::ProjectMode::Mode2D && e.dimension == DimensionTag::Only3D) continue;
                    if (projectMode == Scene::ProjectMode::Mode3D && e.dimension == DimensionTag::Only2D) continue;
                }
                int score = ScoreComponentMatch(e, m_ComponentSearchBuf);
                if (score <= 0) continue;
                visible.push_back({i, &e, score});
            }

            // Sort by score descending when filter is active (preserves category order for ties)
            if (hasFilter) {
                std::stable_sort(visible.begin(), visible.end(),
                    [](const VisibleEntry& a, const VisibleEntry& b) { return a.score > b.score; });
            }

            // Clamp selection index
            if (m_ComponentSearchSelectedIndex >= static_cast<int>(visible.size())) {
                m_ComponentSearchSelectedIndex = static_cast<int>(visible.size()) - 1;
            }
            if (m_ComponentSearchSelectedIndex < 0 && !visible.empty()) {
                m_ComponentSearchSelectedIndex = 0;
            }

            // Keyboard navigation
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && m_ComponentSearchSelectedIndex < static_cast<int>(visible.size()) - 1) {
                m_ComponentSearchSelectedIndex++;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && m_ComponentSearchSelectedIndex > 0) {
                m_ComponentSearchSelectedIndex--;
            }
            bool enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);

            // Trigger selected entry on Enter
            int activatedIndex = -1;
            if (enterPressed && m_ComponentSearchSelectedIndex >= 0 && m_ComponentSearchSelectedIndex < static_cast<int>(visible.size())) {
                activatedIndex = m_ComponentSearchSelectedIndex;
            }

            if (ImGui::BeginChild("ComponentList", ImVec2(0, 350), false)) {
                // Recently-used section (only when no filter)
                if (!hasFilter && !m_EditorSettings.recentComponents.empty()) {
                    bool anyRecent = false;
                    for (const auto& rcName : m_EditorSettings.recentComponents) {
                        // Find matching entry that isn't already on the entity
                        for (const auto& e : allEntries) {
                            if (std::strcmp(e.displayName, rcName.c_str()) == 0 && !e.hasComponent(m_World, m_PrimarySelected)) {
                                // Dimension filtering for recently-used
                                if (!m_ShowAllComponents && projectMode != Scene::ProjectMode::Mixed) {
                                    if (projectMode == Scene::ProjectMode::Mode2D && e.dimension == DimensionTag::Only3D) break;
                                    if (projectMode == Scene::ProjectMode::Mode3D && e.dimension == DimensionTag::Only2D) break;
                                }
                                if (!anyRecent) {
                                    ImGui::TextDisabled("Recently Used");
                                    anyRecent = true;
                                }
                                if (ImGui::Selectable(e.displayName)) {
                                    ECS::Entity target = m_PrimarySelected;
                                    auto addFn = e.addComponent;
                                    auto removeFn = e.removeComponent;
                                    addFn(m_World, target);
                                    auto cmd = std::make_unique<AddComponentCommand>(
                                        e.displayName,
                                        [this, addFn, target]() { addFn(m_World, target); },
                                        [this, removeFn, target]() { removeFn(m_World, target); }
                                    );
                                    m_UndoRedo.Execute(std::move(cmd));
                                    if (e.controllerType) {
                                        SetupCameraForController(target, e.controllerType);
                                    }
                                    m_EditorSettings.AddRecentComponent(e.displayName);
                                    m_EditorSettings.Save();
                                    ImGui::CloseCurrentPopup();
                                }
                                break;
                            }
                        }
                    }
                    if (anyRecent) {
                        ImGui::Separator();
                    }
                }

                if (hasFilter) {
                    // Filtered flat list with category hints
                    for (int vi = 0; vi < static_cast<int>(visible.size()); vi++) {
                        const auto& ve = visible[vi];
                        bool isSelected = (vi == m_ComponentSearchSelectedIndex);

                        if (ImGui::Selectable(ve.entry->displayName, isSelected)) {
                            activatedIndex = vi;
                        }
                        if (isSelected && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) {
                            ImGui::SetScrollHereY();
                        }
                        // Show category hint on the right
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ve.entry->category).x);
                        ImGui::TextDisabled("%s", ve.entry->category);
                    }
                } else {
                    // Categorized view with collapsible headers
                    const char* currentCategory = nullptr;
                    int viIndex = 0;
                    for (int vi = 0; vi < static_cast<int>(visible.size()); vi++) {
                        const auto& ve = visible[vi];
                        // New category header
                        if (!currentCategory || std::strcmp(currentCategory, ve.entry->category) != 0) {
                            currentCategory = ve.entry->category;
                            if (!ImGui::CollapsingHeader(currentCategory, ImGuiTreeNodeFlags_DefaultOpen)) {
                                // Skip all entries in this collapsed category
                                while (vi + 1 < static_cast<int>(visible.size()) &&
                                       std::strcmp(visible[vi + 1].entry->category, currentCategory) == 0) {
                                    vi++;
                                }
                                continue;
                            }
                        }

                        bool isSelected = (vi == m_ComponentSearchSelectedIndex);
                        ImGui::Indent(8.0f);
                        if (ImGui::Selectable(ve.entry->displayName, isSelected)) {
                            activatedIndex = vi;
                        }
                        if (isSelected && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) {
                            ImGui::SetScrollHereY();
                        }
                        ImGui::Unindent(8.0f);
                    }
                }

                if (visible.empty()) {
                    ImGui::TextDisabled("No matching components");
                }
            }
            ImGui::EndChild();

            // Handle activation (click or Enter)
            if (activatedIndex >= 0 && activatedIndex < static_cast<int>(visible.size())) {
                const auto& ve = visible[activatedIndex];
                ECS::Entity target = m_PrimarySelected;
                auto addFn = ve.entry->addComponent;
                auto removeFn = ve.entry->removeComponent;
                addFn(m_World, target);
                auto cmd = std::make_unique<AddComponentCommand>(
                    ve.entry->displayName,
                    [this, addFn, target]() { addFn(m_World, target); },
                    [this, removeFn, target]() { removeFn(m_World, target); }
                );
                m_UndoRedo.Execute(std::move(cmd));
                if (ve.entry->controllerType) {
                    SetupCameraForController(target, ve.entry->controllerType);
                }
                m_EditorSettings.AddRecentComponent(ve.entry->displayName);
                m_EditorSettings.Save();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        } else {
            // Popup closed — reset search state
            m_ComponentSearchBuf[0] = '\0';
            m_ComponentSearchSelectedIndex = -1;
        }
    } else {
        DrawEmptyState("< >", "No Entity Selected", "Select an entity in the Hierarchy to inspect it");
    }

    ImGui::End();
}


void EditorLayer::DrawMultiSelectInspector() {
    ImGui::Text("%zu entities selected", m_SelectedEntities.size());
    ImGui::Separator();

    // List selected entity names
    if (ImGui::CollapsingHeader("Selected Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (ECS::Entity e : m_SelectedEntities) {
            std::string name = "Entity " + std::to_string(e);
            if (m_World->HasComponent<ECS::NameComponent>(e)) {
                name = m_World->GetComponent<ECS::NameComponent>(e)->name;
            }
            bool isPrimary = (e == m_PrimarySelected);
            if (isPrimary) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::BulletText("%s [%llu]%s", name.c_str(), (unsigned long long)e,
                              isPrimary ? " (primary)" : "");
            if (isPrimary) ImGui::PopStyleColor();
        }
    }

    // Batch transform editing
    if (ImGui::CollapsingHeader("Batch Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Changes apply as delta to all selected");
        ImGui::Spacing();

        static Math::Vector3 batchOffset(0.0f, 0.0f, 0.0f);
        static Math::Vector3 batchRotation(0.0f, 0.0f, 0.0f);
        static Math::Vector3 batchScale(1.0f, 1.0f, 1.0f);

        // Position offset
        ImGui::Text("Position Offset");
        f32 pos[3] = { batchOffset.x, batchOffset.y, batchOffset.z };
        if (ImGui::DragFloat3("##BatchPos", pos, 0.1f)) {
            batchOffset = Math::Vector3(pos[0], pos[1], pos[2]);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Apply##Pos")) {
            for (ECS::Entity e : m_SelectedEntities) {
                auto* t = m_World->GetComponent<ECS::TransformComponent>(e);
                if (t) t->position = t->position + batchOffset;
            }
            batchOffset = Math::Vector3(0.0f, 0.0f, 0.0f);
        }

        // Rotation offset (euler degrees)
        ImGui::Text("Rotation Offset (degrees)");
        f32 rot[3] = { batchRotation.x, batchRotation.y, batchRotation.z };
        if (ImGui::DragFloat3("##BatchRot", rot, 1.0f)) {
            batchRotation = Math::Vector3(rot[0], rot[1], rot[2]);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Apply##Rot")) {
            Math::Quaternion dq = Math::Quaternion(Math::Vector3(0,1,0), Math::Radians(batchRotation.y))
                                * Math::Quaternion(Math::Vector3(1,0,0), Math::Radians(batchRotation.x))
                                * Math::Quaternion(Math::Vector3(0,0,1), Math::Radians(batchRotation.z));
            for (ECS::Entity e : m_SelectedEntities) {
                auto* t = m_World->GetComponent<ECS::TransformComponent>(e);
                if (t) t->rotation = dq * t->rotation;
            }
            batchRotation = Math::Vector3(0.0f, 0.0f, 0.0f);
        }

        // Scale multiplier
        ImGui::Text("Scale Multiplier");
        f32 scl[3] = { batchScale.x, batchScale.y, batchScale.z };
        if (ImGui::DragFloat3("##BatchScale", scl, 0.01f, 0.01f, 100.0f)) {
            batchScale = Math::Vector3(scl[0], scl[1], scl[2]);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Apply##Scale")) {
            for (ECS::Entity e : m_SelectedEntities) {
                auto* t = m_World->GetComponent<ECS::TransformComponent>(e);
                if (t) {
                    t->scale = Math::Vector3(t->scale.x * batchScale.x,
                                             t->scale.y * batchScale.y,
                                             t->scale.z * batchScale.z);
                }
            }
            batchScale = Math::Vector3(1.0f, 1.0f, 1.0f);
        }
    }
}


bool EditorLayer::EntityHasAnyController(ECS::Entity entity) const {
    if (!m_World) return false;
    return m_World->HasComponent<ECS::Platformer2DController>(entity) ||
           m_World->HasComponent<ECS::TopDown2DController>(entity) ||
           m_World->HasComponent<ECS::TopDown3DController>(entity) ||
           m_World->HasComponent<ECS::ThirdPersonController>(entity) ||
           m_World->HasComponent<ECS::FirstPersonController>(entity) ||
           m_World->HasComponent<ECS::VehicleController>(entity) ||
           m_World->HasComponent<ECS::SurfaceAlignedController>(entity);
}

ECS::Entity EditorLayer::FindPlayerEntity() const {
    if (!m_World) return ECS::INVALID_ENTITY;
    ECS::Entity player = m_World->FindEntityByName("Player");
    if (player != ECS::INVALID_ENTITY) return player;
    // Fallback: first entity with any controller
    auto tryFirst = [](const auto& entities) -> ECS::Entity {
        for (ECS::Entity e : entities) return e;
        return ECS::INVALID_ENTITY;
    };
    ECS::Entity found = tryFirst(m_World->GetEntitiesWithComponent<ECS::Platformer2DController>());
    if (found != ECS::INVALID_ENTITY) return found;
    found = tryFirst(m_World->GetEntitiesWithComponent<ECS::TopDown2DController>());
    if (found != ECS::INVALID_ENTITY) return found;
    found = tryFirst(m_World->GetEntitiesWithComponent<ECS::TopDown3DController>());
    if (found != ECS::INVALID_ENTITY) return found;
    found = tryFirst(m_World->GetEntitiesWithComponent<ECS::ThirdPersonController>());
    if (found != ECS::INVALID_ENTITY) return found;
    found = tryFirst(m_World->GetEntitiesWithComponent<ECS::FirstPersonController>());
    return found;
}

static bool EntityHasAnyCollider(ECS::World* world, ECS::Entity entity) {
    return world->HasComponent<ECS::BoxColliderComponent>(entity) ||
           world->HasComponent<ECS::SphereColliderComponent>(entity) ||
           world->HasComponent<ECS::CapsuleColliderComponent>(entity) ||
           world->HasComponent<ECS::PolygonCollider2DComponent>(entity) ||
           world->HasComponent<ECS::TriggerZoneComponent>(entity);
}

// ============================================================================
// Creative Intelligence — Smart Suggestions (Cross-System Integration)
// ============================================================================


void EditorLayer::DrawSmartSuggestions(ECS::Entity entity) {
    if (!m_World) return;

    // Collect applicable suggestions
    struct Suggestion {
        const char* text;
        const char* buttonId;
        int ruleId;
    };
    std::vector<Suggestion> suggestions;

    bool hasDialogue = m_World->HasComponent<ECS::DialogueComponent>(entity);
    bool hasDialogueBox = m_World->HasComponent<ECS::DialogueBoxComponent>(entity);
    bool hasAI = m_World->HasComponent<ECS::AIControllerComponent>(entity);
    bool hasHealth = m_World->HasComponent<ECS::HealthComponent>(entity);
    bool hasCollider = EntityHasAnyCollider(m_World, entity);
    bool hasController = EntityHasAnyController(entity);
    bool hasMesh = m_World->HasComponent<ECS::MeshComponent>(entity);
    bool hasMaterial = m_World->HasComponent<ECS::MaterialComponent>(entity);
    bool hasBT = m_World->HasComponent<ECS::BehaviorTreeComponent>(entity);
    bool hasSprite = m_World->HasComponent<ECS::Sprite2DComponent>(entity);
    bool hasDamage = m_World->HasComponent<ECS::DamageComponent>(entity);
    bool hasCam2D = m_World->HasComponent<ECS::Camera2DBoundsComponent>(entity);

    // Rule 1: Dialogue but no DialogueBox
    if (hasDialogue && !hasDialogueBox)
        suggestions.push_back({"Add Dialogue Box for UI display", "##SugDlgBox", 1});

    // Rule 2: AIController but no Health
    if (hasAI && !hasHealth)
        suggestions.push_back({"Add Health for combat", "##SugHealth", 2});

    // Rule 3: Health but no Collider
    if (hasHealth && !hasCollider)
        suggestions.push_back({"Add collider for damage detection", "##SugCollider", 3});

    // Rule 4: Controller but no Camera in scene
    if (hasController) {
        ECS::Entity cam = ECS::CameraManager::GetActiveCamera(m_World);
        if (cam == ECS::INVALID_ENTITY)
            suggestions.push_back({"Add camera for this controller", "##SugCamera", 4});
    }

    // Rule 5: Mesh but no Material
    if (hasMesh && !hasMaterial)
        suggestions.push_back({"Add material for rendering", "##SugMat", 5});

    // Rule 6: BehaviorTree but no AIController
    if (hasBT && !hasAI)
        suggestions.push_back({"Add AI Controller for movement", "##SugAI", 6});

    // Rule 7: Sprite + Controller but no Camera2DBounds
    if (hasSprite && hasController && !hasCam2D)
        suggestions.push_back({"Add 2D Camera for follow", "##SugCam2D", 7});

    // Rule 8: Damage but no Collider
    if (hasDamage && !hasCollider)
        suggestions.push_back({"Add collider for damage area", "##SugDmgCol", 8});

    if (suggestions.empty()) return;

    ImGui::Spacing();
    ImVec4 bgColor(0.15f, 0.25f, 0.4f, 0.3f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
    ImGui::BeginChild("SmartSuggestions", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Suggestions");
    ImGui::Separator();

    for (auto& s : suggestions) {
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 0.9f), "!");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", s.text);
        ImGui::SameLine();

        std::string btnLabel = std::string("+ Add") + s.buttonId;
        if (ImGui::SmallButton(btnLabel.c_str())) {
            switch (s.ruleId) {
            case 1: m_World->AddComponent<ECS::DialogueBoxComponent>(entity); break;
            case 2: {
                auto& h = m_World->AddComponent<ECS::HealthComponent>(entity);
                h.maxHealth = 100.0f;
                h.currentHealth = 100.0f;
                break;
            }
            case 3: m_World->AddComponent<ECS::BoxColliderComponent>(entity); break;
            case 4: {
                // Determine controller type string for camera setup
                std::string ctrlType = "ThirdPerson";
                if (m_World->HasComponent<ECS::Platformer2DController>(entity)) ctrlType = "Platformer2D";
                else if (m_World->HasComponent<ECS::TopDown2DController>(entity)) ctrlType = "TopDown2D";
                else if (m_World->HasComponent<ECS::TopDown3DController>(entity)) ctrlType = "TopDown3D";
                else if (m_World->HasComponent<ECS::FirstPersonController>(entity)) ctrlType = "FirstPerson";
                SetupCameraForController(entity, ctrlType);
                break;
            }
            case 5: m_World->AddComponent<ECS::MaterialComponent>(entity); break;
            case 6: m_World->AddComponent<ECS::AIControllerComponent>(entity); break;
            case 7: {
                auto& cam2d = m_World->AddComponent<ECS::Camera2DBoundsComponent>(entity);
                cam2d.followTarget = entity;
                break;
            }
            case 8: m_World->AddComponent<ECS::BoxColliderComponent>(entity); break;
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Creative Intelligence — Quick Setup (One-Click Patterns)
// ============================================================================


void EditorLayer::DrawQuickSetup(ECS::Entity entity) {
    if (!m_World) return;

    bool hasController = EntityHasAnyController(entity);
    bool hasAI = m_World->HasComponent<ECS::AIControllerComponent>(entity);
    bool hasBT = m_World->HasComponent<ECS::BehaviorTreeComponent>(entity);
    bool hasDialogue = m_World->HasComponent<ECS::DialogueComponent>(entity);
    bool hasPickup = m_World->HasComponent<ECS::PickupComponent>(entity);
    bool hasPlatformer = m_World->HasComponent<ECS::Platformer2DController>(entity);

    auto projectMode = m_SceneManager.GetProjectMode();
    bool is2D = (projectMode == Scene::ProjectMode::Mode2D);

    // Also treat as 2D if the entity has 2D-specific components (sprite, tilemap, Camera2DBounds)
    // even if the project mode isn't explicitly set to 2D
    if (!is2D && (m_World->HasComponent<ECS::Sprite2DComponent>(entity) ||
                  m_World->HasComponent<ECS::AnimatedSprite2DComponent>(entity) ||
                  m_World->HasComponent<ECS::TilemapComponent>(entity) ||
                  m_World->HasComponent<ECS::Camera2DBoundsComponent>(entity))) {
        is2D = true;
    }

    // Count how many buttons we'd show
    int buttonCount = 0;
    if (!hasController) buttonCount++;
    if (is2D && !hasPlatformer && !hasController) buttonCount++;
    if ((hasAI || hasBT) && !m_World->HasComponent<ECS::HealthComponent>(entity)) buttonCount++;
    if (!hasDialogue && !hasController) buttonCount++;
    if (!hasPickup && !hasController) buttonCount++;

    if (buttonCount == 0) return;

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Quick Setup")) {
        f32 buttonWidth = ImGui::GetContentRegionAvail().x;

        // Pattern 1: Add Basic Movement
        if (!hasController) {
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Adds controller + collider + camera");
            if (ImGui::Button("Add Basic Movement", ImVec2(buttonWidth, 0))) {
                if (is2D) {
                    m_World->AddComponent<ECS::Platformer2DController>(entity);
                    m_World->AddComponent<ECS::BoxColliderComponent>(entity);
                    SetupCameraForController(entity, "Platformer2D");
                } else {
                    m_World->AddComponent<ECS::ThirdPersonController>(entity);
                    auto& cap = m_World->AddComponent<ECS::CapsuleColliderComponent>(entity);
                    cap.radius = 0.3f;
                    cap.height = 1.8f;
                    SetupCameraForController(entity, "ThirdPerson");
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(is2D ?
                "Adds Platformer2D controller + BoxCollider + camera" :
                "Adds ThirdPerson controller + CapsuleCollider + camera");
        }

        // Pattern 2: Setup 2D Platformer (only in 2D mode)
        if (is2D && !hasPlatformer && !hasController) {
            if (ImGui::Button("Setup 2D Platformer", ImVec2(buttonWidth, 0))) {
                m_World->AddComponent<ECS::Platformer2DController>(entity);
                m_World->AddComponent<ECS::BoxColliderComponent>(entity);
                if (!m_World->HasComponent<ECS::Sprite2DComponent>(entity)) {
                    m_World->AddComponent<ECS::Sprite2DComponent>(entity);
                }
                auto& cam2d = m_World->AddComponent<ECS::Camera2DBoundsComponent>(entity);
                cam2d.followTarget = entity;
                cam2d.deadZoneSize = Math::Vector2(1.0f, 1.0f);
                cam2d.lookAheadDistance = 2.0f;
                cam2d.lookAheadSmoothing = 3.0f;
                SetupCameraForController(entity, "Platformer2D");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Adds Platformer2D + BoxCollider + Sprite2D + Camera2D with follow/dead zone/look-ahead");
        }

        // Pattern 3: Make Patroller (has AI/BT but no Health)
        if ((hasAI || hasBT) && !m_World->HasComponent<ECS::HealthComponent>(entity)) {
            if (ImGui::Button("Make Patroller", ImVec2(buttonWidth, 0))) {
                if (!hasAI) {
                    auto& ai = m_World->AddComponent<ECS::AIControllerComponent>(entity);
                    ai.currentState = ECS::AIControllerComponent::AIState::Patrol;
                    ECS::Entity player = FindPlayerEntity();
                    if (player != ECS::INVALID_ENTITY)
                        ai.targetEntity = player;
                }
                auto& hp = m_World->AddComponent<ECS::HealthComponent>(entity);
                hp.maxHealth = 50.0f;
                hp.currentHealth = 50.0f;
                if (!hasBT)
                    m_World->AddComponent<ECS::BehaviorTreeComponent>(entity);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Adds AI Controller (Patrol) + Health (50) + Behavior Tree, auto-targets Player");
        }

        // Pattern 4: Setup NPC
        if (!hasDialogue && !hasController) {
            if (ImGui::Button("Setup NPC", ImVec2(buttonWidth, 0))) {
                auto& dlg = m_World->AddComponent<ECS::DialogueComponent>(entity);
                auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
                if (nameComp && !nameComp->name.empty())
                    dlg.speakerName = nameComp->name;
                m_World->AddComponent<ECS::DialogueBoxComponent>(entity);
                auto& interact = m_World->AddComponent<ECS::InteractableComponent>(entity);
                interact.promptText = "Talk";
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Adds Dialogue (speaker=entity name) + DialogueBox + Interactable (\"Talk\")");
        }

        // Pattern 5: Make Collectible
        if (!hasPickup && !hasController) {
            if (ImGui::Button("Make Collectible", ImVec2(buttonWidth, 0))) {
                auto& pickup = m_World->AddComponent<ECS::PickupComponent>(entity);
                pickup.type = ECS::PickupComponent::PickupType::Coin;
                pickup.value = 1.0f;
                pickup.destroyOnPickup = true;
                pickup.bobSpeed = 2.0f;
                pickup.bobHeight = 0.2f;
                auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(entity);
                trigger.shape = ECS::TriggerZoneComponent::Shape::Sphere;
                trigger.sphereRadius = 1.0f;
                if (!m_World->HasComponent<ECS::TweenComponent>(entity)) {
                    auto& tween = m_World->AddComponent<ECS::TweenComponent>(entity);
                    tween.autoPlay = true;
                    ECS::TweenEntry bob;
                    bob.property = ECS::TweenProperty::Position;
                    bob.easing = ECS::EasingType::EaseInOutSine;
                    bob.mode = ECS::TweenMode::PingPong;
                    bob.startValue = Math::Vector3(0.0f, 0.0f, 0.0f);
                    bob.endValue = Math::Vector3(0.0f, 0.3f, 0.0f);
                    bob.duration = 1.0f;
                    bob.isPlaying = true;
                    tween.tweens.push_back(bob);
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Adds Pickup (Coin) + TriggerZone (sphere) + Tween (bob up/down)");
        }
    }
}


} // namespace Editor
} // namespace Enjin
