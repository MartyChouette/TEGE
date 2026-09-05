#include "Enjin/Platform/Desktop.h"
#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/EditorWidgets.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <functional>
#include <cmath>
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/MeshRenderer.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/VirtualCamera.h"
#include "Enjin/ECS/Components/Lens.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/HoverHighlight.h"
#include "Enjin/ECS/Components/Swarm.h"
#include "Enjin/ECS/Components/GeneratedGeometry.h"
#include "Enjin/ECS/Components/GeneratedTexture.h"
#include "Enjin/ECS/Components/ProceduralTexture.h"
#include "Enjin/ECS/Components/ProceduralMesh.h"
#include "Enjin/Effects/Metaballs.h"
#include "Enjin/ECS/Components/DungeonGenerator.h"
#include "Enjin/ECS/Systems/DungeonGeneratorSystem.h"
#include "Enjin/ECS/Components/RandomBag.h"
#include "Enjin/ECS/Systems/RandomBagSystem.h"
#include "Enjin/ECS/Components/Scatter.h"
#include "Enjin/ECS/Systems/ScatterSystem.h"
#include "Enjin/ECS/Components/TerrainGenerator.h"
#include "Enjin/ECS/Systems/TerrainGeneratorSystem.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/WFC.h"
#include "Enjin/ECS/Systems/WFCSystem.h"
#include "Enjin/Editor/ComponentHelp.h"
#include "Enjin/ECS/Components/GPUParticleEmitter.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Cloth.h"
#include "Enjin/ECS/Components/DynamicDifficulty.h"
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
#include "Enjin/ECS/Components/GaussianSplat.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/Viewmodel.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/ECS/Components/ReflectionProbe.h"
#include "Enjin/ECS/Components/ReflectivePlane.h"
#include "Enjin/ECS/Components/ActionTrigger.h"
#include "Enjin/ECS/Components/Ladder.h"
#include "Enjin/ECS/Components/Rope.h"
#include "Enjin/ECS/Components/Door.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/ECS/Components/ArtStyle.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/CineComponent.h"
#include "Enjin/ECS/Components/Elemental.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/DisplayGraphic.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/BoneAttachment.h"
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
#include "Enjin/ECS/Components/CustomShader.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/GUI/DialogueImportExport.h"
#include "Enjin/Assets/SWFLoader.h"
#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Effects/VoronoiMeshFracture.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/ECS/Components/ParallaxMachine.h"
#include "Enjin/ECS/Components/ParallaxLayer.h"
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

// Compute an entity's local mesh AABB (min/max) and transform scale, for auto-fitting a freshly
// added collider to the visible mesh. Collider sizes are WORLD-space (Jolt/Box2D do not apply the
// transform scale), so a new collider must be sized to mesh-extent * scale rather than the 1x1x1
// default — otherwise a scaled mesh (e.g. a 50x0.1x50 ground) gets a 1-unit collider and objects
// pass straight through. Returns false if the entity has no usable mesh.
static bool ComputeMeshLocalBounds(ECS::World* w, ECS::Entity e,
                                   Math::Vector3& outMin, Math::Vector3& outMax, Math::Vector3& outScale) {
    outScale = Math::Vector3(1.0f, 1.0f, 1.0f);
    if (auto* t = w->GetComponent<ECS::TransformComponent>(e)) outScale = t->scale;
    auto* mesh = w->GetComponent<ECS::MeshComponent>(e);
    if (!mesh || mesh->vertices.empty()) return false;
    Math::Vector3 mn = mesh->vertices[0].position;
    Math::Vector3 mx = mesh->vertices[0].position;
    for (const auto& v : mesh->vertices) {
        mn.x = std::min(mn.x, v.position.x); mn.y = std::min(mn.y, v.position.y); mn.z = std::min(mn.z, v.position.z);
        mx.x = std::max(mx.x, v.position.x); mx.y = std::max(mx.y, v.position.y); mx.z = std::max(mx.z, v.position.z);
    }
    outMin = mn;
    outMax = mx;
    return true;
}

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
        {"Material Slots", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::MaterialSlotsComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::MaterialSlotsComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::MaterialSlotsComponent>(e); },
            "materialSlots"},
        {"Mesh Renderer", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::MeshRendererComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::MeshRendererComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::MeshRendererComponent>(e); },
            "meshRenderer"},
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
        {"Virtual Cinematography (CINE)", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::CineComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::CineComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::CineComponent>(e); },
            "cineComponent", DimensionTag::Only3D},
        {"Text", "UI", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TextComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TextComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TextComponent>(e); },
            "text"},
        {"Vector Graphic (SVG)", "UI", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DisplayGraphicComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DisplayGraphicComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DisplayGraphicComponent>(e); },
            "displayGraphic"},

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
            [](ECS::World* w, ECS::Entity e) {
                auto& col = w->AddComponent<ECS::BoxColliderComponent>(e);
                Math::Vector3 mn, mx, s;
                if (ComputeMeshLocalBounds(w, e, mn, mx, s)) {
                    Math::Vector3 ext = mx - mn, ctr = (mx + mn) * 0.5f;
                    col.size   = Math::Vector3(ext.x * s.x, ext.y * s.y, ext.z * s.z);
                    col.center = Math::Vector3(ctr.x * s.x, ctr.y * s.y, ctr.z * s.z);
                }
            },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::BoxColliderComponent>(e); },
            "boxCollider"},
        {"Sphere Collider", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SphereColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) {
                auto& col = w->AddComponent<ECS::SphereColliderComponent>(e);
                Math::Vector3 mn, mx, s;
                if (ComputeMeshLocalBounds(w, e, mn, mx, s)) {
                    Math::Vector3 ext = mx - mn, ctr = (mx + mn) * 0.5f;
                    col.radius = 0.5f * std::max(ext.x * s.x, std::max(ext.y * s.y, ext.z * s.z));
                    col.center = Math::Vector3(ctr.x * s.x, ctr.y * s.y, ctr.z * s.z);
                }
            },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SphereColliderComponent>(e); },
            "sphereCollider"},
        {"Capsule Collider", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::CapsuleColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) {
                auto& col = w->AddComponent<ECS::CapsuleColliderComponent>(e);
                Math::Vector3 mn, mx, s;
                if (ComputeMeshLocalBounds(w, e, mn, mx, s)) {
                    Math::Vector3 ext = mx - mn, ctr = (mx + mn) * 0.5f;
                    // Y-axis capsule (default): radius from XZ extent, height = cylinder portion only
                    // (total = height + 2*radius per the capsule convention).
                    col.radius = 0.5f * std::max(ext.x * s.x, ext.z * s.z);
                    col.height = std::max(0.0f, ext.y * s.y - 2.0f * col.radius);
                    col.center = Math::Vector3(ctr.x * s.x, ctr.y * s.y, ctr.z * s.z);
                }
            },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::CapsuleColliderComponent>(e); },
            "capsuleCollider"},
        {"Mesh Collider", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::MeshColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::MeshColliderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::MeshColliderComponent>(e); },
            "meshCollider"},
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
        {"Joint 2D", "Physics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<Physics::Joint2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<Physics::Joint2DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<Physics::Joint2DComponent>(e); },
            "joint2D", DimensionTag::Only2D},
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
        {"Reaction-Diffusion", "Generated Geometry", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ReactionDiffusionComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ReactionDiffusionComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ReactionDiffusionComponent>(e); },
            "reaction diffusion gray scott turing pattern texture"},
        {"Physarum (slime mould)", "Generated Geometry", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::PhysarumComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::PhysarumComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::PhysarumComponent>(e); },
            "physarum slime mould agents trail network texture"},
        {"Metaball Blob", "Generated Geometry", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<Effects::MetaballComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<Effects::MetaballComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<Effects::MetaballComponent>(e); },
            "metaball blob isosurface marching cubes goo"},
        {"Metaball Surface", "Generated Geometry", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::MetaballSurfaceComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::MetaballSurfaceComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::MetaballSurfaceComponent>(e); },
            "metaball surface isosurface mesh receiver"},
        {"Cellular Automata", "Generated Geometry", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::CellularAutomataComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::CellularAutomataComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::CellularAutomataComponent>(e); },
            "cellular automata life conway voxel geometry"},
        {"4D Projection", "Generated Geometry", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::Projection4DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::Projection4DComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::Projection4DComponent>(e); },
            "4d tesseract polytope stereographic hypercube"},
        {"Fourier Mesh", "Generated Geometry", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::FourierMeshComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::FourierMeshComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::FourierMeshComponent>(e); },
            "fourier dft epicycle contour decomposition"},
        {"Swarm (crowd demo)", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SwarmComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SwarmComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SwarmComponent>(e); },
            "swarm crowd agents boids instanced"},
        {"Dungeon Generator", "Procedural", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DungeonGeneratorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DungeonGeneratorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DungeonGeneratorComponent>(e); },
            "procgen dungeon cave rooms tilemap random walk cellular bsp"},
        {"Random Bag", "Procedural", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::RandomBagComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::RandomBagComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::RandomBagComponent>(e); },
            "procgen random bag weighted loot table tetris 7-bag deck shuffle spawn draw pull"},
        {"Scatter", "Procedural", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ScatterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ScatterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ScatterComponent>(e); },
            "procgen scatter prefab instance foliage rocks trees grass poisson blue-noise jitter grid distribute place spawn"},
        {"Terrain Generator", "Procedural", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TerrainGeneratorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TerrainGeneratorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TerrainGeneratorComponent>(e); },
            "procgen terrain generator heightmap fbm noise fractal ridged mountains hills erosion hydraulic thermal landscape"},
        {"Wave Function Collapse", "Procedural", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::WFCComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::WFCComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::WFCComponent>(e); },
            "procgen wfc wave function collapse constraint tile adjacency socket autotile carcassonne pattern"},
        {"Cloth", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ClothComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ClothComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ClothComponent>(e); },
            "cloth fabric flag cape curtain banner tear rip softbody"},
        {"GPU Particle Emitter", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::GPUParticleEmitterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::GPUParticleEmitterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::GPUParticleEmitterComponent>(e); },
            "particle emitter gpu compute fire smoke sparks blood mist fx"},
        {"Reverb Zone", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ReverbZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ReverbZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ReverbZoneComponent>(e); },
            "reverbZone"},
        {"Ambient Sound Layer", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AmbientSoundLayerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AmbientSoundLayerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AmbientSoundLayerComponent>(e); },
            "ambientSoundLayer"},
        {"Music Zone", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::MusicZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::MusicZoneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::MusicZoneComponent>(e); },
            "musicZone"},
        {"Audio Snapshot Trigger", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AudioSnapshotTriggerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AudioSnapshotTriggerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AudioSnapshotTriggerComponent>(e); },
            "audioSnapshotTrigger"},
        {"Audio Occlusion", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AudioOcclusionComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AudioOcclusionComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AudioOcclusionComponent>(e); },
            "audioOcclusion"},
        {"Lip Sync", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::LipSyncComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::LipSyncComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::LipSyncComponent>(e); },
            "lipSync"},
        {"Audio Reactive", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AudioReactiveComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AudioReactiveComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AudioReactiveComponent>(e); },
            "audioReactive"},
        {"Audio Threshold Trigger", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AudioThresholdTriggerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AudioThresholdTriggerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AudioThresholdTriggerComponent>(e); },
            "audioThresholdTrigger"},
        {"RTPC", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::RTPCComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::RTPCComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::RTPCComponent>(e); },
            "rtpc"},
        {"Beat Clock", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::BeatClockComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::BeatClockComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::BeatClockComponent>(e); },
            "beatClock"},
        {"Beat Sync", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::BeatSyncComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::BeatSyncComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::BeatSyncComponent>(e); },
            "beatSync"},
        {"Audio Fidelity", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AudioFidelityComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AudioFidelityComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AudioFidelityComponent>(e); },
            "audioFidelity"},
        {"MIDI Binding", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::MIDIBindingComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::MIDIBindingComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::MIDIBindingComponent>(e); },
            "midiBinding"},
        {"Conductor", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ConductorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ConductorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ConductorComponent>(e); },
            "conductor"},
        {"Audio Collision", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AudioCollisionComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AudioCollisionComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AudioCollisionComponent>(e); },
            "audioCollision"},
        {"Material Interaction Table", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::MaterialInteractionTableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::MaterialInteractionTableComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::MaterialInteractionTableComponent>(e); },
            "materialInteractionTable"},
        {"Sidechain", "Audio", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SidechainComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SidechainComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SidechainComponent>(e); },
            "sidechain"},
        {"Pose Library", "3D / Animation", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::PoseLibraryComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::PoseLibraryComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::PoseLibraryComponent>(e); },
            "poseLibrary"},
        {"Record Rewind", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::RecordRewindComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::RecordRewindComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::RecordRewindComponent>(e); },
            "recordRewind"},
        {"Scene Rewind", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SceneRewindComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SceneRewindComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SceneRewindComponent>(e); },
            "sceneRewind"},
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
        {"Ladder", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::LadderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::LadderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::LadderComponent>(e); },
            "ladder", DimensionTag::Only3D},
        {"Rope", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::RopeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::RopeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::RopeComponent>(e); },
            "rope", DimensionTag::Only3D},
        {"Door", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DoorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DoorComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DoorComponent>(e); },
            "door", DimensionTag::Only3D},
        {"Chain", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::RopeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) {
                ECS::RopeComponent chain;
                chain.style = ECS::RopeStyle::Chain;
                chain.segments = 10;
                chain.thickness = 0.09f;
                chain.iterations = 12;   // chains hang stiff, not stretchy
                w->AddComponent<ECS::RopeComponent>(e, chain);
            },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::RopeComponent>(e); },
            "rope", DimensionTag::Only3D},
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
        {"Game Over", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::GameOverComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::GameOverComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::GameOverComponent>(e); },
            "gameOver"},
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
        // "HUD Widget" removed from the Add Component menu: HUDSystem is
        // retired — HUD elements are authored as UICanvas (UI unification).
        // Legacy hudWidget data still loads and auto-migrates to canvases.
        {"UI Canvas", "UI", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<GUI::UICanvasComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<GUI::UICanvasComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<GUI::UICanvasComponent>(e); },
            "uiCanvas"},
        {"Cinematic Camera", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::CinematicCameraComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::CinematicCameraComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::CinematicCameraComponent>(e); },
            "cinematicCamera"},
        {"Tween", "3D / Animation", nullptr,
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
        {"Dialogue Box", "UI", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DialogueBoxComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DialogueBoxComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DialogueBoxComponent>(e); },
            "dialogueBox"},
        {"Save Data", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SaveDataComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SaveDataComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SaveDataComponent>(e); },
            "saveData"},
        {"Save/Load Menu", "UI", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::SaveLoadMenuComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::SaveLoadMenuComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::SaveLoadMenuComponent>(e); },
            "saveLoadMenu"},
        {"Dynamic Difficulty", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::DynamicDifficultyComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::DynamicDifficultyComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::DynamicDifficultyComponent>(e); },
            "dynamicDifficulty"},
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
        {"Virtual Camera", "Camera", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::VirtualCameraComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::VirtualCameraComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::VirtualCameraComponent>(e); },
            "virtualCamera"},
        {"Lens", "Camera", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::LensComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::LensComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::LensComponent>(e); },
            "lens"},

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
        {"Billboard", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::BillboardComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::BillboardComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::BillboardComponent>(e); },
            "billboard"},
        {"Particle Emitter", "Effects", nullptr,
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
        {"Gaussian Splat", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::GaussianSplatComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::GaussianSplatComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::GaussianSplatComponent>(e); },
            "gaussianSplat", DimensionTag::Only3D},
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
        {"Viewmodel (First Person)", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ViewmodelComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ViewmodelComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ViewmodelComponent>(e); },
            "viewmodel", DimensionTag::Only3D},
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
        {"Gravity Zone", "Physics", nullptr,
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
        {"Art Style", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ArtStyleComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ArtStyleComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ArtStyleComponent>(e); },
            "artStyle"},
        {"Reflection Probe", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ReflectionProbeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ReflectionProbeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ReflectionProbeComponent>(e); },
            "reflectionProbe", DimensionTag::Only3D},
        {"Action Trigger", "Gameplay", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ActionTriggerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ActionTriggerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ActionTriggerComponent>(e); },
            "actionTrigger"},
        {"Reflective Floor", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ReflectivePlaneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ReflectivePlaneComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ReflectivePlaneComponent>(e); },
            "reflectivePlane", DimensionTag::Only3D},
        {"Elemental Surface", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ElementalSurfaceComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ElementalSurfaceComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ElementalSurfaceComponent>(e); },
            "elementalSurface"},
        {"Elemental Emitter", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ElementalEmitterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ElementalEmitterComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ElementalEmitterComponent>(e); },
            "elementalEmitter"},
        {"Elemental Volume", "Effects", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ElementalVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ElementalVolumeComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ElementalVolumeComponent>(e); },
            "elementalVolume"},

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
        {"Parallax Machine", "2D Graphics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ParallaxMachineComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ParallaxMachineComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ParallaxMachineComponent>(e); },
            "parallaxMachine", DimensionTag::Only2D},
        {"Parallax Layer", "2D Graphics", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ParallaxLayerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ParallaxLayerComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ParallaxLayerComponent>(e); },
            "parallaxLayer", DimensionTag::Only2D},

        // -- Scripting --
        {"Script", "Scripting", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::ScriptComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::ScriptComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::ScriptComponent>(e); },
            "scriptComponent"},
        {"Notes", "Other", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::NotesComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::NotesComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::NotesComponent>(e); },
            "notes"},
        {"Hover Highlight", "Other", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::HoverHighlightComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::HoverHighlightComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::HoverHighlightComponent>(e); },
            "hoverHighlight"},

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
        {"Terrain", "Rendering", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TerrainComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TerrainComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TerrainComponent>(e); },
            "terrain", DimensionTag::Only3D},
        {"Terrain 2D", "Rendering", nullptr,
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
        {"Two-Bone IK", "3D / Animation", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::TwoBoneIKComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::TwoBoneIKComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::TwoBoneIKComponent>(e); },
            "twoBoneIK", DimensionTag::Only3D},
        {"Bone Attachment", "3D / Animation", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::BoneAttachmentComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::BoneAttachmentComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::BoneAttachmentComponent>(e); },
            "boneAttachment", DimensionTag::Only3D},
        {"Animation Recorder", "3D / Animation", nullptr,
            [](ECS::World* w, ECS::Entity e) { return w->HasComponent<ECS::AnimationRecorderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->AddComponent<ECS::AnimationRecorderComponent>(e); },
            [](ECS::World* w, ECS::Entity e) { w->RemoveComponent<ECS::AnimationRecorderComponent>(e); },
            "animationRecorder", DimensionTag::Only3D},
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
        {"Grabbable", "Gameplay", nullptr,
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


// Case-insensitive string equality, for matching a relation's partner label to
// a present module's display name on the wiring board.
static bool WiringLabelEq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

// Scale the alpha channel of a packed color (for fading the board in on flip).
static ImU32 WiringFade(ImU32 col, float alpha) {
    ImU32 a = (col >> IM_COL32_A_SHIFT) & 0xFF;
    a = (ImU32)(a * (alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha)));
    return (col & ~(0xFFu << IM_COL32_A_SHIFT)) | (a << IM_COL32_A_SHIFT);
}

static ImU32 WiringCableColor(Editor::RelationKind k) {
    using RK = Editor::RelationKind;
    switch (k) {
        case RK::Requires:       return IM_COL32(230,165, 75,255);
        case RK::PairsWith:      return IM_COL32( 60,210,190,255);
        case RK::Paints:         return IM_COL32( 90,160,240,255);
        case RK::PulledByScript:
        case RK::DrivenByScript: return IM_COL32(180,140,235,255);
        case RK::FeedsRenderer:  return IM_COL32(120,200,140,255);
        case RK::FeedsPhysics:   return IM_COL32(210,120,120,255);
    }
    return IM_COL32(150,150,150,255);
}

// Truncate a label with an ellipsis so it fits inside maxW pixels.
static std::string WiringClip(const std::string& s, float maxW) {
    if (ImGui::CalcTextSize(s.c_str()).x <= maxW) return s;
    std::string out = s;
    while (out.size() > 1 && ImGui::CalcTextSize((out + "...").c_str()).x > maxW) out.pop_back();
    return out + "...";
}

// ---------------------------------------------------------------------------
// Per-entity wiring board — the "back of the rack" (Reason-style flip target).
// Draws the selected entity's components as modules and their ComponentHelp
// relations as patch cables to partner modules or subsystem sinks (Renderer,
// Physics, Input, ...). Read-only; cheap draw-list boxes + beziers, 120fps-safe.
// Return codes: 0 = nothing, 1 = clicked a module (caller flips back to its
// panel), 2 = open Visual Script editor, 3 = open Shader Graph editor.
// ---------------------------------------------------------------------------
enum class WiringAction : int { None = 0, FlipBack = 1, OpenVisualScript = 2, OpenShaderGraph = 3, OpenScript = 4 };

static int DrawEntityWiringBoardImpl(ECS::World* world, ECS::Entity entity, float alpha, std::string* outPayload) {
    if (!world) { ImGui::TextDisabled("No entity selected."); return 0; }

    // Manual node positions (drag-to-rearrange), kept per session, keyed per entity.
    static std::unordered_map<std::string, ImVec2> s_manual;
    static bool s_dragMoved = false;
    const std::string keyPre = std::to_string((unsigned long long)entity) + "|";

    // Header: color legend (readability) + a reset for the drag layout.
    struct LegendItem { Editor::RelationKind k; const char* name; };
    static const LegendItem legend[] = {
        { Editor::RelationKind::PairsWith,     "pairs" },
        { Editor::RelationKind::Requires,      "needs" },
        { Editor::RelationKind::Paints,        "paints" },
        { Editor::RelationKind::PulledByScript,"script" },
        { Editor::RelationKind::FeedsRenderer, "renderer" },
        { Editor::RelationKind::FeedsPhysics,  "physics" },
    };
    for (int i = 0; i < (int)(sizeof(legend)/sizeof(legend[0])); ++i) {
        if (i) ImGui::SameLine();
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(WiringCableColor(legend[i].k));
        ImGui::TextColored(c, "--");
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextDisabled("%s", legend[i].name);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset layout")) {
        for (auto it = s_manual.begin(); it != s_manual.end(); )
            it = (it->first.rfind(keyPre, 0) == 0) ? s_manual.erase(it) : std::next(it);
    }
    ImGui::Spacing();

    struct Node {
        std::string label;
        const Editor::ComponentHelp* help = nullptr;
        bool isSink = false;
        int  action = 0;       // WiringAction to fire when a sink is clicked (0 = none)
        std::string payload;   // e.g. the .as path for a script sink
        ImVec2 pos{}, size{};
    };
    std::vector<Node> nodes;
    auto findByLabel = [&](const std::string& lbl) -> int {
        for (int i = 0; i < (int)nodes.size(); ++i)
            if (WiringLabelEq(nodes[i].label, lbl)) return i;
        return -1;
    };
    auto addModule = [&](const char* label, const char* key) {
        if (findByLabel(label) >= 0) return;
        Node n; n.label = label; n.help = Editor::GetComponentHelp(key);
        nodes.push_back(std::move(n));
    };

    // Present component modules. Transform isn't in the Add menu (it's implicit).
    if (world->HasComponent<ECS::TransformComponent>(entity)) addModule("Transform", "transform");
    for (const auto& ce : GetComponentEntries())
        if (ce.hasComponent && ce.hasComponent(world, entity))
            addModule(ce.displayName, ce.componentKey);
    const int moduleCount = (int)nodes.size();

    // Resolve relations into edges; unmatched targets become right-side sinks.
    struct Edge { int from, to; Editor::RelationKind kind; };
    std::vector<Edge> edges;
    for (int i = 0; i < moduleCount; ++i) {
        if (!nodes[i].help) continue;
        for (const auto& rel : nodes[i].help->relations) {
            if (rel.present && !rel.present(world, entity)) continue; // absent partner: don't clutter
            int t = findByLabel(rel.label);
            if (t < 0) { Node s; s.label = rel.label; s.isSink = true; nodes.push_back(std::move(s)); t = (int)nodes.size() - 1; }
            edges.push_back({ i, t, rel.kind });
        }
    }

    // Live node-map / asset cross-references: wire modules to the REAL graphs and
    // script files this entity uses (reads actual component data, not authored).
    auto addSinkEdge = [&](const char* fromLabel, const std::string& sinkLabel,
                           Editor::RelationKind kind, int action = 0, const std::string& payload = "") {
        int from = findByLabel(fromLabel);
        if (from < 0) return;
        Node s; s.label = sinkLabel; s.isSink = true; s.action = action; s.payload = payload;
        nodes.push_back(std::move(s));
        edges.push_back({ from, (int)nodes.size() - 1, kind });
    };
    if (auto* sc = world->GetComponent<ECS::ScriptComponent>(entity)) {
        for (const auto& att : sc->scripts) {
            std::string name = !att.className.empty() ? att.className
                : std::filesystem::path(att.scriptPath).filename().string();
            if (name.empty()) name = "script";
            addSinkEdge("Script", name, Editor::RelationKind::DrivenByScript,
                        (int)WiringAction::OpenScript, att.scriptPath);
        }
    }
    if (auto* vs = world->GetComponent<ECS::VisualScriptComponent>(entity)) {
        addSinkEdge("Visual Script",
                    "Graph (" + std::to_string(vs->graph.GetNodes().size()) + " nodes)",
                    Editor::RelationKind::DrivenByScript, (int)WiringAction::OpenVisualScript);
    }
    if (auto* cs = world->GetComponent<ECS::CustomShaderComponent>(entity)) {
        std::string gl = cs->graphLabel.empty() ? std::string("Shader Graph")
                                                 : ("Graph: " + cs->graphLabel);
        addSinkEdge("Material", gl, Editor::RelationKind::FeedsRenderer, (int)WiringAction::OpenShaderGraph);
    }

    // Layout: modules stacked left, sinks stacked right; manual drags override.
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float boxW = 156.0f, boxH = 30.0f, gapY = 12.0f;
    const float availW = ImGui::GetContentRegionAvail().x;
    const float leftX  = origin.x + 12.0f;
    const float rightX = origin.x + availW - boxW - 12.0f;
    float ly = origin.y + 6.0f, ry = origin.y + 6.0f;
    for (auto& n : nodes) {
        n.size = ImVec2(boxW, boxH);
        if (n.isSink) { n.pos = ImVec2(rightX, ry); ry += boxH + gapY; }
        else          { n.pos = ImVec2(leftX,  ly); ly += boxH + gapY; }
        auto it = s_manual.find(keyPre + n.label);
        if (it != s_manual.end()) n.pos = it->second;
    }
    float neededH = std::max(ly, ry) - origin.y + 6.0f;
    if (neededH < 120.0f) neededH = 120.0f;
    ImGui::Dummy(ImVec2(availW, neededH)); // reserve scroll height

    // Interaction: an invisible button per node for hover, drag and click.
    int hovered = -1, result = 0;
    for (int i = 0; i < (int)nodes.size(); ++i) {
        ImGui::SetCursorScreenPos(nodes[i].pos);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##node", nodes[i].size);
        if (ImGui::IsItemActivated()) s_dragMoved = false;
        if (ImGui::IsItemActive()) {
            ImVec2 md = ImGui::GetIO().MouseDelta;
            if (md.x != 0.0f || md.y != 0.0f) {
                s_dragMoved = true;
                nodes[i].pos = ImVec2(nodes[i].pos.x + md.x, nodes[i].pos.y + md.y);
                s_manual[keyPre + nodes[i].label] = nodes[i].pos; // persist (session)
            }
        }
        if (ImGui::IsItemHovered()) {
            hovered = i;
            if (!nodes[i].isSink && nodes[i].help && nodes[i].help->whatItDoes)
                ImGui::SetTooltip("%s\n(click to open  |  drag to move)", nodes[i].help->whatItDoes);
            else if (nodes[i].isSink && nodes[i].action == (int)WiringAction::OpenScript)
                ImGui::SetTooltip("%s\n(click to open the .as  |  drag to move)", nodes[i].payload.c_str());
            else if (nodes[i].isSink && nodes[i].action != 0)
                ImGui::SetTooltip("%s\n(click to open the editor  |  drag to move)", nodes[i].label.c_str());
            else if (nodes[i].isSink)
                ImGui::SetTooltip("%s (engine subsystem)", nodes[i].label.c_str());
        }
        if (ImGui::IsItemDeactivated() && !s_dragMoved) { // a click, not a drag
            if (!nodes[i].isSink)          result = (int)WiringAction::FlipBack;
            else if (nodes[i].action != 0) {
                result = nodes[i].action;
                if (outPayload) *outPayload = nodes[i].payload;
            }
        }
        ImGui::PopID();
    }

    // Cables: fan the endpoints out per node so parallel cables don't overlap.
    std::vector<int> outTot(nodes.size(), 0), inTot(nodes.size(), 0), outSeen(nodes.size(), 0), inSeen(nodes.size(), 0);
    for (const auto& e : edges) { outTot[e.from]++; inTot[e.to]++; }
    const float lane = 8.0f, maxOff = boxH * 0.5f - 5.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (const auto& e : edges) {
        bool lit = (hovered == e.from || hovered == e.to);
        int oi = outSeen[e.from]++, ii = inSeen[e.to]++;
        float aOff = (oi - (outTot[e.from] - 1) * 0.5f) * lane;
        float bOff = (ii - (inTot[e.to]   - 1) * 0.5f) * lane;
        aOff = std::max(-maxOff, std::min(maxOff, aOff));
        bOff = std::max(-maxOff, std::min(maxOff, bOff));
        ImVec2 a = ImVec2(nodes[e.from].pos.x + boxW, nodes[e.from].pos.y + boxH * 0.5f + aOff);
        ImVec2 b = ImVec2(nodes[e.to].pos.x,          nodes[e.to].pos.y   + boxH * 0.5f + bOff);
        float dx = (b.x - a.x) * 0.5f; if (dx < 40.0f) dx = 40.0f;
        ImU32 c = WiringFade(WiringCableColor(e.kind), lit ? alpha : alpha * 0.8f);
        dl->AddBezierCubic(a, ImVec2(a.x + dx, a.y), ImVec2(b.x - dx, b.y), b, c, lit ? 3.0f : 2.0f);
        dl->AddCircleFilled(a, 3.0f, c);
        dl->AddCircleFilled(b, 3.0f, c);
    }
    for (int i = 0; i < (int)nodes.size(); ++i) {
        const Node& n = nodes[i];
        bool hot = (hovered == i);
        ImVec2 p0 = n.pos, p1 = ImVec2(n.pos.x + boxW, n.pos.y + boxH);
        ImU32 fill   = WiringFade(n.isSink ? IM_COL32(40,44,54,255)
                                           : (hot ? IM_COL32(70,76,92,255) : IM_COL32(54,58,70,255)), alpha);
        ImU32 border = WiringFade(hot ? IM_COL32(150,195,255,255)
                                      : (n.isSink ? IM_COL32(90,100,120,255) : IM_COL32(120,150,190,255)),
                                  alpha);
        dl->AddRectFilled(p0, p1, fill, 5.0f);
        dl->AddRect(p0, p1, border, 5.0f, 0, hot ? 2.5f : 1.5f);
        std::string label = WiringClip(n.label, boxW - 14.0f);
        ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        ImVec2 tp = ImVec2(p0.x + (boxW - ts.x) * 0.5f, p0.y + (boxH - ts.y) * 0.5f);
        dl->AddText(tp, WiringFade(IM_COL32(222,226,234,255), alpha), label.c_str());
    }
    if (moduleCount == 0) ImGui::TextDisabled("This entity has no components.");
    return result;
}

void EditorLayer::DrawInspectorPanel() {
    ImGuiWindowFlags flags = 0;
    if (m_FocusMode) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    bool panelOpen = true;
    ImGui::Begin("Inspector", &panelOpen, flags);
    if (!panelOpen) {
        SetPanelVisibility(EditorPanel::Inspector, false);
    }

    // Set proportional widget width so labels don't consume all space
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);

    // --- Inspector lock: pin the panel to one entity so it stops following the
    // selection. Toggle captures the current selection; while locked, the whole
    // panel below runs against the pinned entity. ---
    if (m_InspectorLocked && (!m_World || !m_World->IsValid(m_InspectorLockedEntity))) {
        m_InspectorLocked = false; // the pinned entity went away
        m_InspectorLockedEntity = ECS::INVALID_ENTITY;
    }
    if (ImGui::SmallButton(m_InspectorLocked ? "[L] Locked" : "[ ] Lock")) {
        if (m_InspectorLocked) {
            m_InspectorLocked = false; m_InspectorLockedEntity = ECS::INVALID_ENTITY;
        } else if (m_PrimarySelected != ECS::INVALID_ENTITY) {
            m_InspectorLocked = true; m_InspectorLockedEntity = m_PrimarySelected;
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Lock this Inspector to the current entity so it stops following what you select");
    if (m_InspectorLocked) {
        ImGui::SameLine();
        std::string lnm = "entity";
        if (auto* lnc = m_World ? m_World->GetComponent<ECS::NameComponent>(m_InspectorLockedEntity) : nullptr) lnm = lnc->name;
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.3f, 1.0f), "pinned to %s", lnm.c_str());
    }
    ImGui::Separator();
    const bool inspLocked = m_InspectorLocked && m_World && m_World->IsValid(m_InspectorLockedEntity);
    const ECS::Entity inspSavedPrimary = m_PrimarySelected;
    if (inspLocked) m_PrimarySelected = m_InspectorLockedEntity;

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

    if (!inspLocked && m_SelectedEntities.size() > 1 && m_World) {
        // Multi-select inspector
        DrawMultiSelectInspector();
    } else if (m_PrimarySelected != ECS::INVALID_ENTITY && m_World) {
        Scene::Layer* vwsActive = m_LayerSystem.ActiveLayer();
        const bool vwsCapturing = vwsActive && !vwsActive->locked;

        // Generic property-edit undo: on quiet frames (no active widget, no
        // edit session) refresh the baseline snapshot. While a widget is
        // active the baseline stays frozen at the pre-edit state; the session
        // resolves into one EntityEditCommand at the bottom of this block.
        // Skipped during ACTIVE play (gameplay mutates components every frame
        // — paused/stopped editing still captures).
        const bool propUndoEligible = !m_PlayMode.IsPlaying();
        if (propUndoEligible && !m_PropUndoEditing) {
            // Refresh the baseline ONLY when something could have changed the
            // entity: selection switch, an undo/redo, or a half-second
            // heartbeat. Serializing the whole entity every quiet frame
            // tanked editor FPS whenever a heavy entity was selected — an
            // animated FBX carries megabytes of clip keyframes per snapshot
            // (Marty, 2026-08-07: "selecting an FBX slows its anim fps").
            const bool needRefresh =
                m_PropUndoBaselineEntity != m_PrimarySelected ||
                m_PropUndoStackAtSessionStart != m_UndoRedo.GetUndoCount() ||
                (++m_PropUndoRefreshTick >= 30);
            if (needRefresh) {
                m_PropUndoRefreshTick = 0;
                m_PropUndoBaseline = Scene::SceneSerializer::SerializeEntityToString(
                    m_World, m_PrimarySelected, /*includeVertexData=*/false);
                m_PropUndoBaselineEntity = m_PrimarySelected;
                m_PropUndoStackAtSessionStart = m_UndoRedo.GetUndoCount();
            }
        }

        // VWS: the "before" snapshot for layer capture is the SAME pre-edit state the undo
        // baseline holds, and that baseline is frozen during an edit + only re-serialized on a
        // selection/undo/heartbeat. So reuse it instead of re-serializing the entity every frame
        // (#5: this was the ~2-4ms/frame cost with a heavy animated FBX selected + a layer active).
        // includeVertexData=false in both keeps them byte-identical. During play the baseline isn't
        // maintained, so fall back to a fresh serialize there to preserve capture behavior.
        std::string vwsBeforeFallback;
        const std::string* vwsBeforeSnapshot = nullptr;
        if (vwsCapturing) {
            if (propUndoEligible && m_PropUndoBaselineEntity == m_PrimarySelected) {
                vwsBeforeSnapshot = &m_PropUndoBaseline;
            } else {
                vwsBeforeFallback = Scene::SceneSerializer::SerializeEntityToString(
                    m_World, m_PrimarySelected, /*includeVertexData=*/false);
                vwsBeforeSnapshot = &vwsBeforeFallback;
            }
        }

        // Entity name (editable)
        ECS::NameComponent* nameComp = m_World->GetComponent<ECS::NameComponent>(m_PrimarySelected);
        if (nameComp) {
            char nameBuffer[256];
            strncpy(nameBuffer, nameComp->name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##EntityName", nameBuffer, sizeof(nameBuffer))) {
                std::string oldName = nameComp->name;
                m_World->SetEntityName(m_PrimarySelected, nameBuffer);
                if (m_CollabSystem.IsActive()) {
                    m_CollabSystem.OnEntityRenamed(m_PrimarySelected, oldName, nameComp->name);
                }
            }
            // Drop a .as script from the Asset Browser onto the name to attach it here.
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* aspl = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    AttachScriptFromAsset(m_PrimarySelected, static_cast<const char*>(aspl->Data));
                }
                ImGui::EndDragDropTarget();
            }
        } else {
            // No name component - show entity ID and add button
            ImGui::Text("Entity %llu", (unsigned long long)m_PrimarySelected);
            ImGui::SameLine();
            if (ImGui::SmallButton("Add Name")) {
                m_World->AddComponent<ECS::NameComponent>(m_PrimarySelected, "Unnamed");
            }
        }

        // Copy the entity as scene-file JSON — the exact format .enjin scenes
        // use, so authors can learn the schema from a working example (or paste
        // it into a bug report). Vertex data is skipped to keep it readable.
        if (ImGui::SmallButton("Copy as JSON")) {
            std::string js = Scene::SceneSerializer::SerializeEntityToString(
                m_World, m_PrimarySelected, /*includeVertexData=*/false);
            ImGui::SetClipboardText(js.c_str());
            ShowNotification("Entity JSON copied to clipboard", NotificationType::Info);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Copy this entity in the .enjin scene-file JSON format\n"
                              "(without vertex data). Great for learning the schema,\n"
                              "diffing, or bug reports.");
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

        // Reason-style flip: front = component panels, back = the wiring board
        // ("the back of the rack"). Tab (when the Inspector is focused) or the
        // button toggles; a quick alpha "turn" sells the flip. reducedMotion =
        // instant swap. State is panel-local (one Inspector).
        static bool  s_ShowWiring = false;
        static float s_FlipAnim   = 0.0f;   // 0 = front panels, 1 = wiring board
        if (ImGui::SmallButton(s_ShowWiring ? "< Back to Panels" : "Flip to Wiring >"))
            s_ShowWiring = !s_ShowWiring;
        ImGui::SameLine();
        ImGui::TextDisabled(s_ShowWiring ? "the wiring behind this entity" : "Tab: see how this entity is wired");
        if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput &&
            ImGui::IsKeyPressed(ImGuiKey_Tab, false))
            s_ShowWiring = !s_ShowWiring;
        {
            float target = s_ShowWiring ? 1.0f : 0.0f;
            if (m_EditorSettings.reducedMotion) {
                s_FlipAnim = target;
            } else {
                float step = ImGui::GetIO().DeltaTime * 6.0f; // ~0.17s flip
                if (s_FlipAnim < target) s_FlipAnim = std::min(target, s_FlipAnim + step);
                else                     s_FlipAnim = std::max(target, s_FlipAnim - step);
            }
        }
        ImGui::Separator();
        const bool showWiring = s_FlipAnim >= 0.5f;
        float contentAlpha = std::fabs(s_FlipAnim - 0.5f) * 2.0f; // 1 at ends, 0 mid-flip
        if (contentAlpha < 0.05f) contentAlpha = 0.05f;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * contentAlpha);
        if (showWiring) {
            std::string wiringPayload;
            int act = DrawEntityWiringBoardImpl(m_World, m_PrimarySelected, contentAlpha, &wiringPayload);
            if (act == (int)WiringAction::FlipBack) {
                s_ShowWiring = false; // clicked a module: flip back to its panel
            } else if (act == (int)WiringAction::OpenVisualScript) {
                SetPanelVisibility(EditorPanel::VisualScript, true);
            } else if (act == (int)WiringAction::OpenShaderGraph) {
                m_ShaderGraphEditor.SetOpen(true);
            } else if (act == (int)WiringAction::OpenScript && !wiringPayload.empty()) {
                // Resolve the .as against the project root and open it in the OS editor.
                std::string proj = m_SceneManager.GetProjectPath();
                std::filesystem::path root = proj.empty()
                    ? std::filesystem::current_path()
                    : std::filesystem::path(proj).parent_path();
                std::filesystem::path full = root / wiringPayload;
                Platform::OpenInDesktop(std::filesystem::absolute(full).string());
            }
        } else {

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

        // Material Slots component (multi-material)
        if (m_World->HasComponent<ECS::MaterialSlotsComponent>(m_PrimarySelected)) {
            DrawMaterialSlotsComponent(m_PrimarySelected);
        }

        // Mesh Renderer component
        if (m_World->HasComponent<ECS::MeshRendererComponent>(m_PrimarySelected)) {
            bool mrOpen = UI::SectionHeader("[R] Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem("MeshRendererCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::MeshRendererComponent>(m_PrimarySelected, "meshRenderer", "Mesh Renderer");
                }
                ImGui::EndPopup();
            }
            if (mrOpen) {
                auto* mr = m_World->GetComponent<ECS::MeshRendererComponent>(m_PrimarySelected);
                if (mr) {
                    DrawComponentHelp("meshRenderer", m_World, m_PrimarySelected);
                    ImGui::Checkbox("Enabled##MR", &mr->enabled);
                    ImGui::Checkbox("Frustum Cull##MR", &mr->frustumCull);
                    ImGui::Checkbox("Occlusion Cull##MR", &mr->occlusionCull);
                    ImGui::DragFloat("Max Draw Distance##MR", &mr->maxDrawDistance, 1.0f, 0.0f, 10000.0f, "%.0f");
                    if (mr->maxDrawDistance < 0.0f) mr->maxDrawDistance = 0.0f;
                    ImGui::DragInt("Render Queue##MR", &mr->renderQueue, 1.0f, -5000, 5000);
                    ImGui::DragFloat("LOD Bias##MR", &mr->lodBias, 0.1f, -2.0f, 2.0f, "%.1f");

                    const char* shadowModes[] = { "From Material", "Off", "On", "Two-Sided" };
                    int sm = static_cast<int>(mr->shadowMode);
                    if (ImGui::Combo("Shadow Mode##MR", &sm, shadowModes, 4)) {
                        mr->shadowMode = static_cast<ECS::MeshRendererComponent::ShadowMode>(sm);
                    }

                    ImGui::Checkbox("Motion Vectors##MR", &mr->contributeMotionVectors);
                    ImGui::Checkbox("Allow Instancing##MR", &mr->allowInstancing);
                    ImGui::Checkbox("Wireframe##MR", &mr->wireframe);
                    if (mr->wireframe) {
                        f32 wfColor[3] = { mr->wireframeColor.x, mr->wireframeColor.y, mr->wireframeColor.z };
                        if (ImGui::ColorEdit3("Wire Color##MR", wfColor)) {
                            mr->wireframeColor = Math::Vector3(wfColor[0], wfColor[1], wfColor[2]);
                        }
                        ImGui::DragFloat("Wire Opacity##MR", &mr->wireframeOpacity, 0.05f, 0.0f, 1.0f, "%.2f");
                    }

                    if (!mr->customShaderName.empty()) {
                        ImGui::TextDisabled("Shader: %s", mr->customShaderName.c_str());
                    }
                }
            }
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

        // Gaussian splat component
        if (m_World->HasComponent<ECS::GaussianSplatComponent>(m_PrimarySelected)) {
            DrawGaussianSplatComponent(m_PrimarySelected);
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

        // Viewmodel component
        if (m_World->HasComponent<ECS::ViewmodelComponent>(m_PrimarySelected)) {
            DrawViewmodelComponent(m_PrimarySelected);
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

        // Art Style component
        if (m_World->HasComponent<ECS::ArtStyleComponent>(m_PrimarySelected)) {
            DrawArtStyleComponent(m_PrimarySelected);
        }

        // Reflection Probe component
        if (m_World->HasComponent<ECS::ReflectionProbeComponent>(m_PrimarySelected)) {
            DrawReflectionProbeComponent(m_PrimarySelected);
        }

        // Reflective Floor component (planar reflection)
        if (m_World->HasComponent<ECS::ReflectivePlaneComponent>(m_PrimarySelected)) {
            DrawReflectivePlaneComponent(m_PrimarySelected);
        }

        // Action Trigger (input action -> scene effect, no script)
        if (m_World->HasComponent<ECS::ActionTriggerComponent>(m_PrimarySelected)) {
            DrawActionTriggerComponent(m_PrimarySelected);
        }

        // Ladder component (G1 climbable volume)
        if (m_World->HasComponent<ECS::LadderComponent>(m_PrimarySelected)) {
            DrawLadderComponent(m_PrimarySelected);
        }

        // Rope component (G3 verlet rope)
        if (m_World->HasComponent<ECS::RopeComponent>(m_PrimarySelected)) {
            DrawRopeComponent(m_PrimarySelected);
        }

        // Door component (G7 hinged door)
        if (m_World->HasComponent<ECS::DoorComponent>(m_PrimarySelected)) {
            DrawDoorComponent(m_PrimarySelected);
        }

        // Fluid Volume component
        if (m_World->HasComponent<ECS::FluidVolumeComponent>(m_PrimarySelected)) {
            DrawFluidVolumeComponent(m_PrimarySelected);

            // Show coupling UI when entity also has a terrain
            if (m_World->HasComponent<ECS::TerrainComponent>(m_PrimarySelected)) {
                DrawFluidTerrainCoupling(m_PrimarySelected);
            }
        }

        // Elemental Surface component
        if (m_World->HasComponent<ECS::ElementalSurfaceComponent>(m_PrimarySelected)) {
            DrawElementalSurfaceComponent(m_PrimarySelected);
        }

        // Elemental Emitter component
        if (m_World->HasComponent<ECS::ElementalEmitterComponent>(m_PrimarySelected)) {
            DrawElementalEmitterComponent(m_PrimarySelected);
        }

        // Elemental Volume component
        if (m_World->HasComponent<ECS::ElementalVolumeComponent>(m_PrimarySelected)) {
            DrawElementalVolumeComponent(m_PrimarySelected);
        }

        // Notes component
        if (m_World->HasComponent<ECS::NotesComponent>(m_PrimarySelected)) {
            DrawNotesComponent(m_PrimarySelected);
            DrawHoverHighlightComponent(m_PrimarySelected);
        }

        // Text component
        if (m_World->HasComponent<ECS::TextComponent>(m_PrimarySelected)) {
            DrawTextComponent(m_PrimarySelected);
        }

        // Vector graphic component (unified display P2)
        if (m_World->HasComponent<ECS::DisplayGraphicComponent>(m_PrimarySelected)) {
            DrawDisplayGraphicComponent(m_PrimarySelected);
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
        if (auto* em = m_World->GetComponent<ECS::GPUParticleEmitterComponent>(m_PrimarySelected)) {
            bool emOpen = ImGui::CollapsingHeader("GPU Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen);
            bool emRemoved = false;
            if (ImGui::BeginPopupContextItem("GPUEmitterCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::GPUParticleEmitterComponent>(m_PrimarySelected, "gpuParticleEmitter", "GPU Particle Emitter");
                    emRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!emRemoved && emOpen) {
                DrawComponentHelp("gpuParticleEmitter", m_World, m_PrimarySelected);
                // Load Preset just FILLS the fields below (they stay editable), so
                // gravity/size/lifetime are always visible. Index 0 is "-" (no-op).
                int preset = 0;
                const char* names[] = {"Load Preset...","Smoke","Fire","Sparks","Blood","Mist","Spray","Dust","Magic","Snow","Liquid","Impact","Pickup"};
                if (ImGui::Combo("Load Preset", &preset, names, IM_ARRAYSIZE(names)) && preset > 0) {
                    // names[1..9] map to GPUParticlePreset Smoke(1)..Snow(9).
                    em->LoadPreset(static_cast<Effects::GPUParticlePreset>(preset));
                }

                ImGui::Checkbox("Emitting", &em->emitting);
                ImGui::DragFloat("Rate (/sec)", &em->spawnRate, 5.0f, 0.0f, 5000.0f);

                // 2D mode aims with a single Angle and keeps particles in the sprite
                // plane; 3D mode uses the full direction vector.
                ImGui::Checkbox("2D Mode", &em->emit2D);
                if (ImGui::IsItemHovered())
                    ImGui::SetItemTooltip("For 2D scenes: keep particles in this plane and aim with one Angle");
                if (em->emit2D) {
                    ImGui::SliderFloat("Angle", &em->angle2D, 0.0f, 360.0f, "%.0f deg");
                    ImGui::SameLine();
                    ImGui::TextDisabled("(0=right, 90=up)");
                    ImGui::DragInt("Sorting Layer", &em->sortingLayer, 0.1f, -64, 64);
                    ImGui::DragInt("Order In Layer", &em->orderInLayer, 0.1f, -512, 512);
                    if (ImGui::IsItemHovered())
                        ImGui::SetItemTooltip("Higher draws in front. Match your sprites' layers to sit behind/in front of them.");
                } else {
                    ImGui::DragFloat3("Direction", &em->direction.x, 0.05f);
                }

                const char* shapes[] = {"Point","Sphere","Hemisphere","Cone","Box","Disc (2D)","Line (2D)"};
                int shape = static_cast<int>(em->shape);
                if (ImGui::Combo("Shape", &shape, shapes, IM_ARRAYSIZE(shapes)))
                    em->shape = static_cast<ECS::EmitShape>(shape);
                if (em->shape != ECS::EmitShape::Point)
                    ImGui::DragFloat("Shape Size", &em->shapeSize, 0.02f, 0.0f, 50.0f);

                // Always-visible spawn params (honored per-particle by the GPU sim).
                ImGui::SeparatorText("Particle");
                const char* sprites[] = {"Soft Glow","Hard Circle","Square","Streak","Star","Texture"};
                int sprite = static_cast<int>(em->sprite);
                if (sprite < 0 || sprite > 5) sprite = 0;
                if (ImGui::Combo("Sprite", &sprite, sprites, IM_ARRAYSIZE(sprites)))
                    em->sprite = static_cast<Enjin::u8>(sprite);
                if (em->sprite == 5) {
                    char texBuf[256];
                    strncpy(texBuf, em->spriteTexturePath.c_str(), sizeof(texBuf) - 1);
                    texBuf[sizeof(texBuf) - 1] = '\0';
                    if (ImGui::InputText("Sprite Texture", texBuf, sizeof(texBuf))) {
                        em->spriteTexturePath = texBuf;
                        em->cachedSpriteTexIndex = -2;   // re-resolve
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            em->spriteTexturePath = std::string(static_cast<const char*>(pl->Data));
                            em->cachedSpriteTexIndex = -2;
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::TextDisabled("Image sprite (desktop; web falls back to Soft Glow)");
                } else {
                    ImGui::SliderFloat("Softness", &em->softness, 0.0f, 1.0f, "%.2f");
                    ImGui::SetItemTooltip("0 = hard edge, 1 = fully soft falloff");
                }
                ImGui::ColorEdit4("Color", &em->customColor.x);
                ImGui::SliderFloat("Opacity", &em->customColor.w, 0.0f, 1.0f, "%.2f");
                ImGui::SetItemTooltip("Per-particle transparency (the color's alpha channel)");
                ImGui::DragFloat("Size", &em->customSize, 0.01f, 0.0f, 20.0f);
                ImGui::DragFloat("Lifetime (s)", &em->customLifetime, 0.05f, 0.0f, 60.0f);
                ImGui::DragFloat("Speed", &em->customSpeed, 0.05f, 0.0f, 100.0f);
                ImGui::DragFloat("Spread", &em->customSpread, 0.01f, 0.0f, 3.14159f);
                ImGui::DragFloat("Gravity Scale", &em->customGravityScale, 0.05f, -10.0f, 10.0f);
                ImGui::SetItemTooltip("1 = normal fall, 0 = float, negative = rise (fire/smoke)");
                ImGui::DragFloat("Drag", &em->customDrag, 0.01f, 0.0f, 10.0f);
                ImGui::Checkbox("Collide", &em->collide);
                ImGui::SetItemTooltip("Particles bounce off world Box/Sphere/Capsule colliders (non-trigger)");
                if (em->collide) {
                    ImGui::SliderFloat("Bounciness", &em->bounciness, 0.0f, 1.0f, "%.2f");
                    ImGui::SetItemTooltip("0 = splat like dust, 1 = superball sparks");
                    ImGui::SliderFloat("Friction", &em->friction, 0.0f, 1.0f, "%.2f");
                    ImGui::SetItemTooltip("0 = skid along surfaces, 1 = stop dead on contact");
                    ImGui::DragFloat("Radius", &em->collisionRadius, 0.01f, 0.0f, 2.0f);
                    ImGui::SetItemTooltip("Collision size (0 = point). Match roughly half the particle Size\nso big particles don't sink in before reacting");
                    char impactBuf[260];
                    snprintf(impactBuf, sizeof(impactBuf), "%s", em->impactSound.c_str());
                    if (ImGui::InputText("Impact Sound", impactBuf, sizeof(impactBuf))) em->impactSound = impactBuf;
                    ImGui::SetItemTooltip("Audio file played where particles strike colliders (project-relative).\nEmpty = silent. Desktop builds for now.");
                    if (!em->impactSound.empty()) {
                        ImGui::SliderFloat("Impact Volume", &em->impactVolume, 0.0f, 2.0f, "%.2f");
                        ImGui::DragFloat("Impact Min Speed", &em->impactMinSpeed, 0.05f, 0.5f, 30.0f);
                        ImGui::SetItemTooltip("Strikes softer than this stay silent (m/s)");
                        ImGui::DragFloat("Impact Cooldown", &em->impactCooldown, 0.01f, 0.0f, 2.0f);
                        ImGui::SetItemTooltip("Minimum seconds between impact sounds from this emitter");
                    }
                    ImGui::Checkbox("Leave Stains", &em->leaveStains);
                    ImGui::SetItemTooltip("Particles leave a lasting mark where they strike (liquid, blood, paint).\nDesktop builds for now.");
                    if (em->leaveStains) {
                        ImGui::ColorEdit4("Stain Color", &em->stainColor.x);
                        ImGui::DragFloat("Stain Size", &em->stainSize, 0.02f, 0.05f, 4.0f);
                        ImGui::DragFloat("Stain Lifetime", &em->stainLifetime, 1.0f, 2.0f, 3600.0f);
                        ImGui::SetItemTooltip("Seconds a stain lingers before fading");
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Burst 500")) { em->burstCount = 500; em->burstNow = true; }
                ImGui::SameLine();
                if (ImGui::Button("Burst 5000")) { em->burstCount = 5000; em->burstNow = true; }
                ImGui::TextDisabled("(sim + draw run entirely on the GPU)");
                ImGui::TextDisabled("Note: particles are untextured soft sprites (color only).");
            }
        }
        if (auto* cl = m_World->GetComponent<ECS::ClothComponent>(m_PrimarySelected)) {
            bool clOpen = ImGui::CollapsingHeader("Cloth", ImGuiTreeNodeFlags_DefaultOpen);
            bool clRemoved = false;
            if (ImGui::BeginPopupContextItem("ClothCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::ClothComponent>(m_PrimarySelected, "cloth", "Cloth");
                    clRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!clRemoved && clOpen) {
                DrawComponentHelp("cloth", m_World, m_PrimarySelected);
                bool rebuild = false;
                rebuild |= ImGui::DragFloat("Width", &cl->width, 0.05f, 0.1f, 50.0f);
                rebuild |= ImGui::DragFloat("Height", &cl->height, 0.05f, 0.1f, 50.0f);
                rebuild |= ImGui::DragInt("Res X", &cl->resX, 1, 2, 64);
                rebuild |= ImGui::DragInt("Res Y", &cl->resY, 1, 2, 64);
                const char* pins[] = {"Top Edge","Top Corners","Left Edge","All Corners","None","Bottom Edge"};
                int pin = static_cast<int>(cl->pin);
                if (ImGui::Combo("Pin", &pin, pins, IM_ARRAYSIZE(pins))) {
                    cl->pin = static_cast<ECS::ClothPin>(pin);
                    rebuild = true;
                }
                ImGui::DragInt("Iterations", &cl->iterations, 1, 1, 20);
                ImGui::SetItemTooltip("Constraint solver passes: more = stiffer fabric");
                ImGui::SliderFloat("Damping", &cl->damping, 0.0f, 0.2f, "%.3f");
                ImGui::DragFloat("Gravity Scale", &cl->gravityScale, 0.05f, -2.0f, 5.0f);
                ImGui::DragFloat3("Wind", &cl->wind.x, 0.1f);
                ImGui::SetItemTooltip("Constant wind on this cloth only, added on top of weather wind");
                ImGui::Checkbox("Weather Wind", &cl->useWeatherWind);
                ImGui::SetItemTooltip("Cloth catches the scene's live wind (gusts + turbulence).\nA Weather Zone covering the cloth overrides it - wind strength 0 keeps indoor cloth calm.");
                if (cl->useWeatherWind) {
                    ImGui::DragFloat("Wind Catch", &cl->weatherWindScale, 0.05f, 0.0f, 5.0f);
                    ImGui::SetItemTooltip("How strongly this fabric catches the weather wind (heavy canvas < light silk)");
                }
                ImGui::Checkbox("Tearable", &cl->tearable);
                ImGui::SetItemTooltip("On = links stretched past the threshold snap and the fabric rips");
                if (cl->tearable) {
                    ImGui::DragFloat("Tear Threshold", &cl->tearThreshold, 0.02f, 1.1f, 5.0f);
                    ImGui::SetItemTooltip("Stretch factor (x rest length) that snaps a link");
                    rebuild |= ImGui::DragFloat("Pin Strength", &cl->pinStrength, 0.02f, 0.2f, 4.0f);
                    ImGui::SetItemTooltip(">1 = reinforced stitching at the pins, <1 = fabric rips off its pins first");
                    const char* seamModes[] = {"None","Vertical","Horizontal","Grid"};
                    int sm = static_cast<int>(cl->seams);
                    if (ImGui::Combo("Seams", &sm, seamModes, IM_ARRAYSIZE(seamModes))) {
                        cl->seams = static_cast<ECS::ClothSeams>(sm);
                        rebuild = true;
                    }
                    ImGui::SetItemTooltip("Sewn panels: tears run along the seam lines instead of spreading evenly");
                    if (cl->seams != ECS::ClothSeams::None) {
                        rebuild |= ImGui::DragInt("Seam Spacing", &cl->seamSpacing, 1, 2, 32);
                        rebuild |= ImGui::DragFloat("Seam Strength", &cl->seamStrength, 0.05f, 1.0f, 5.0f);
                        ImGui::SetItemTooltip("How much tougher the stitched links are than the fabric");
                    }
                }
                ImGui::Checkbox("Self Collide", &cl->selfCollide);
                ImGui::SetItemTooltip("Folded fabric stacks with thickness instead of passing through itself");
                if (cl->selfCollide) {
                    ImGui::DragFloat("Thickness", &cl->thickness, 0.005f, 0.0f, 0.5f, cl->thickness <= 0.0f ? "auto" : "%.3f");
                    ImGui::SetItemTooltip("0 = automatic (half the grid spacing)");
                }
                ImGui::Checkbox("Collide", &cl->collide);
                ImGui::SetItemTooltip("Push cloth points out of Box/Sphere/Capsule colliders");
                if (cl->collide) {
                    ImGui::SliderFloat("Friction", &cl->friction, 0.0f, 1.0f, "%.2f");
                    ImGui::DragFloat("Collision Skin", &cl->collisionSkin, 0.005f, 0.0f, 0.3f);
                }
                if (ImGui::Button("Reset Cloth") || rebuild) cl->initialized = false;
                ImGui::TextDisabled("(simulates in Play mode; pinned points follow the entity)");
            }
        }
        // --- Generated textures -------------------------------------------
        // Both simulations bake RGBA8 into ProceduralTextureComponent, which the
        // renderer uploads and binds as this entity's base colour. Give the
        // entity a mesh (a quad or a cube) to see the result on.
        if (auto* rd = m_World->GetComponent<ECS::ReactionDiffusionComponent>(m_PrimarySelected)) {
            bool rdOpen = ImGui::CollapsingHeader("Reaction-Diffusion", ImGuiTreeNodeFlags_DefaultOpen);
            bool rdRemoved = false;
            if (ImGui::BeginPopupContextItem("ReactionDiffusionCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::ReactionDiffusionComponent>(m_PrimarySelected, "reactionDiffusion", "Reaction-Diffusion");
                    rdRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!rdRemoved && rdOpen) {
                DrawComponentHelp("reactionDiffusion", m_World, m_PrimarySelected);
                const char* presets[] = { "Mitosis Spots", "Coral Growth", "Fingerprints",
                                          "Leopard", "Labyrinth", "Worm Holes",
                                          "Bubble Packing", "Spirals", "Custom" };
                int pi = static_cast<int>(rd->preset);
                if (ImGui::Combo("Preset", &pi, presets, IM_ARRAYSIZE(presets)))
                    rd->preset = static_cast<Effects::RDPreset>(pi);
                i32 w = static_cast<i32>(rd->width), h = static_cast<i32>(rd->height);
                if (ImGui::DragInt("Width", &w, 8, 16, 1024)) rd->width = static_cast<u32>(w);
                if (ImGui::DragInt("Height", &h, 8, 16, 1024)) rd->height = static_cast<u32>(h);
                ImGui::SetItemTooltip("The grid is stepped on the CPU and the whole image re-uploaded.\nCost is quadratic in resolution.");

                const bool custom = (rd->preset == Effects::RDPreset::Custom);
                if (!custom) ImGui::BeginDisabled();
                ImGui::DragFloat("Feed Rate", &rd->feedRate, 0.001f, 0.0f, 0.12f, "%.4f");
                ImGui::DragFloat("Kill Rate", &rd->killRate, 0.001f, 0.0f, 0.10f, "%.4f");
                ImGui::DragFloat("Diffusion U", &rd->diffusionU, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Diffusion V", &rd->diffusionV, 0.01f, 0.0f, 2.0f);
                if (!custom) {
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("Feed/kill come from the preset. Choose Custom to edit them.");
                }

                i32 steps = static_cast<i32>(rd->stepsPerFrame);
                if (ImGui::DragInt("Steps Per Update", &steps, 1, 1, 64)) rd->stepsPerFrame = static_cast<u32>(steps);
                ImGui::SetItemTooltip("How fast the pattern grows. This is simulation sub-steps, not frames.");
                ImGui::Checkbox("Wrap Edges", &rd->wrapEdges);
                ImGui::SeparatorText("Seeding");
                ImGui::Checkbox("Centre Circle", &rd->seedCentreCircle);
                ImGui::SetItemTooltip("A uniform Gray-Scott field never develops anything.\nWithout a seed the texture stays flat forever.");
                ImGui::SliderFloat("Seed Radius", &rd->seedRadius, 0.0f, 0.5f, "%.3f");
                i32 spots = static_cast<i32>(rd->seedRandomSpots);
                if (ImGui::DragInt("Random Spots", &spots, 1, 0, 512)) rd->seedRandomSpots = static_cast<u32>(spots);
                if (!rd->seedCentreCircle && rd->seedRandomSpots == 0)
                    ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                        "No seeding: this will stay a flat colour.");
                ImGui::SeparatorText("Look");
                f32 lo[3] = { rd->colorLow.x, rd->colorLow.y, rd->colorLow.z };
                if (ImGui::ColorEdit3("Color Low", lo)) rd->colorLow = Math::Vector3(lo[0], lo[1], lo[2]);
                f32 hi[3] = { rd->colorHigh.x, rd->colorHigh.y, rd->colorHigh.z };
                if (ImGui::ColorEdit3("Color High", hi)) rd->colorHigh = Math::Vector3(hi[0], hi[1], hi[2]);
                i32 settle = static_cast<i32>(rd->settleSteps);
                if (ImGui::DragInt("Settle Steps", &settle, 25, 0, 20000))
                    rd->settleSteps = static_cast<u32>(settle < 0 ? 0 : settle);
                ImGui::SetItemTooltip("Simulation steps run before the single bake.\nThese all happen in one frame, so a large value stalls the load.");
                if (ImGui::Button("Re-bake")) rd->resetRequested = true;
                ImGui::SameLine();
                ImGui::TextDisabled("bakes once, does not animate");
                ImGui::TextDisabled("%u step(s) simulated", rd->stepCount);
                if (!m_World->HasComponent<ECS::MeshComponent>(m_PrimarySelected))
                    ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                        "No mesh on this entity - the texture has nothing to draw on.");
            }
        }
        if (auto* ph = m_World->GetComponent<ECS::PhysarumComponent>(m_PrimarySelected)) {
            bool phOpen = ImGui::CollapsingHeader("Physarum", ImGuiTreeNodeFlags_DefaultOpen);
            bool phRemoved = false;
            if (ImGui::BeginPopupContextItem("PhysarumCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::PhysarumComponent>(m_PrimarySelected, "physarum", "Physarum");
                    phRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!phRemoved && phOpen) {
                DrawComponentHelp("physarum", m_World, m_PrimarySelected);
                const char* presets[] = { "Classic Slime", "Branching Network", "Dense Web",
                                          "Tendrils", "Pulsating", "Custom" };
                int pi = static_cast<int>(ph->preset);
                if (ImGui::Combo("Preset", &pi, presets, IM_ARRAYSIZE(presets)))
                    ph->preset = static_cast<Effects::PhysarumPreset>(pi);
                i32 w = static_cast<i32>(ph->width), h = static_cast<i32>(ph->height);
                if (ImGui::DragInt("Width", &w, 16, 32, 2048)) ph->width = static_cast<u32>(w);
                if (ImGui::DragInt("Height", &h, 16, 32, 2048)) ph->height = static_cast<u32>(h);
                i32 agents = static_cast<i32>(ph->agentCount);
                if (ImGui::DragInt("Agents", &agents, 500, 1, 500000)) ph->agentCount = static_cast<u32>(agents < 1 ? 1 : agents);
                ImGui::SetItemTooltip("Every agent is sensed and moved on the main thread each step");

                const bool custom = (ph->preset == Effects::PhysarumPreset::Custom);
                if (!custom) ImGui::BeginDisabled();
                ImGui::DragFloat("Sensor Angle", &ph->sensorAngle, 0.5f, 0.0f, 180.0f, "%.1f deg");
                ImGui::DragFloat("Sensor Distance", &ph->sensorDistance, 0.5f, 0.0f, 128.0f, "%.1f px");
                ImGui::DragFloat("Turn Speed", &ph->turnSpeed, 1.0f, 0.0f, 360.0f, "%.1f deg/step");
                ImGui::DragFloat("Move Speed", &ph->moveSpeed, 0.05f, 0.0f, 32.0f, "%.2f px/step");
                ImGui::DragFloat("Trail Decay", &ph->trailDecay, 0.002f, 0.0f, 1.0f, "%.3f");
                ImGui::DragFloat("Trail Deposit", &ph->trailDeposit, 0.1f, 0.0f, 128.0f);
                ImGui::DragFloat("Diffuse Radius", &ph->diffuseRadius, 0.1f, 0.0f, 4.0f);
                if (!custom) {
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("Behaviour comes from the preset. Choose Custom to edit it.");
                }

                ImGui::Checkbox("Wrap Edges", &ph->wrapEdges);
                i32 steps = static_cast<i32>(ph->stepsPerFrame);
                if (ImGui::DragInt("Steps Per Update", &steps, 1, 1, 16)) ph->stepsPerFrame = static_cast<u32>(steps);
                ImGui::SeparatorText("Seeding");
                const char* seedings[] = { "Ring", "Circle", "Point" };
                int si = static_cast<int>(ph->seeding);
                if (ImGui::Combo("Start Shape", &si, seedings, IM_ARRAYSIZE(seedings)))
                    ph->seeding = static_cast<ECS::PhysarumComponent::Seeding>(si);
                ImGui::SetItemTooltip("Ring is the classic start: agents walk inward and the network forms where they collide");
                ImGui::SliderFloat("Inner Radius", &ph->seedInnerRadius, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Outer Radius", &ph->seedOuterRadius, 0.0f, 1.0f, "%.2f");
                ImGui::SeparatorText("Look");
                f32 tc[3] = { ph->trailColor.x, ph->trailColor.y, ph->trailColor.z };
                if (ImGui::ColorEdit3("Trail Color", tc)) ph->trailColor = Math::Vector3(tc[0], tc[1], tc[2]);
                f32 bc[3] = { ph->backgroundColor.x, ph->backgroundColor.y, ph->backgroundColor.z };
                if (ImGui::ColorEdit3("Background", bc)) ph->backgroundColor = Math::Vector3(bc[0], bc[1], bc[2]);
                i32 settle = static_cast<i32>(ph->settleSteps);
                if (ImGui::DragInt("Settle Steps", &settle, 25, 0, 20000))
                    ph->settleSteps = static_cast<u32>(settle < 0 ? 0 : settle);
                ImGui::SetItemTooltip("Steps run before the single bake. Every agent moves on\neach one, so cost is settleSteps x agentCount.");
                if (ImGui::Button("Re-bake")) ph->resetRequested = true;
                ImGui::SameLine();
                ImGui::TextDisabled("bakes once, does not animate");
                ImGui::TextDisabled("%u step(s) simulated", ph->stepCount);
                if (!m_World->HasComponent<ECS::MeshComponent>(m_PrimarySelected))
                    ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                        "No mesh on this entity - the texture has nothing to draw on.");
            }
        }
        if (auto* pt = m_World->GetComponent<ECS::ProceduralTextureComponent>(m_PrimarySelected)) {
            if (ImGui::CollapsingHeader("Procedural Texture")) {
                ImGui::TextDisabled("Generated by: %s", ECS::ProceduralTextureSourceName(pt->source));
                ImGui::TextDisabled("%u x %u, %zu KB", pt->width, pt->height, pt->pixels.size() / 1024);
                ImGui::Checkbox("Regenerate", &pt->regenerate);
                ImGui::SetItemTooltip("Uncheck to freeze the current image and stop the owning system rebuilding it");
                ImGui::TextDisabled("Added automatically. Pixels are runtime data and are not saved.");
            }
        }

        // --- Generated geometry -------------------------------------------
        // The four CPU generators. GeneratedGeometrySystem writes the result
        // into this entity's MeshComponent during play; the readouts below are
        // live values from the running generator, not authored fields.
        if (auto* mb = m_World->GetComponent<Effects::MetaballComponent>(m_PrimarySelected)) {
            bool mbOpen = ImGui::CollapsingHeader("Metaball Blob", ImGuiTreeNodeFlags_DefaultOpen);
            bool mbRemoved = false;
            if (ImGui::BeginPopupContextItem("MetaballCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<Effects::MetaballComponent>(m_PrimarySelected, "metaball", "Metaball Blob");
                    mbRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!mbRemoved && mbOpen) {
                DrawComponentHelp("metaball", m_World, m_PrimarySelected);
                ImGui::DragFloat("Radius", &mb->radius, 0.05f, 0.01f, 100.0f);
                ImGui::SetItemTooltip("Influence radius of this blob in the scalar field");
                ImGui::DragFloat("Strength", &mb->strength, 0.05f, -10.0f, 10.0f);
                ImGui::SetItemTooltip("Negative strength carves a hole out of the surface");
                ImGui::DragFloat("Threshold", &mb->threshold, 0.01f, 0.01f, 10.0f);
                ImGui::DragInt("Group", &mb->groupId, 1, 0, 64);
                ImGui::SetItemTooltip("Blobs merge only with others in the same group.\nA Metaball Surface with this group renders the result.");
                f32 mbCol[3] = { mb->color.x, mb->color.y, mb->color.z };
                if (ImGui::ColorEdit3("Color", mbCol)) mb->color = Math::Vector3(mbCol[0], mbCol[1], mbCol[2]);
                if (m_World->GetEntitiesWithComponent<ECS::MetaballSurfaceComponent>().empty())
                    ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                        "No Metaball Surface in the scene - nothing will render.");
            }
        }
        if (auto* ms = m_World->GetComponent<ECS::MetaballSurfaceComponent>(m_PrimarySelected)) {
            bool msOpen = ImGui::CollapsingHeader("Metaball Surface", ImGuiTreeNodeFlags_DefaultOpen);
            bool msRemoved = false;
            if (ImGui::BeginPopupContextItem("MetaballSurfCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::MetaballSurfaceComponent>(m_PrimarySelected, "metaballSurface", "Metaball Surface");
                    msRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!msRemoved && msOpen) {
                DrawComponentHelp("metaballSurface", m_World, m_PrimarySelected);
                ImGui::DragInt("Group", &ms->groupId, 1, 0, 64);
                ImGui::DragInt("Grid Resolution", &ms->gridResolution, 1, 16, 64);
                ImGui::SetItemTooltip("Marching cubes voxels per axis. Cost is cubic: 64 is 262144 cells per rebuild.");
                ImGui::DragFloat("Grid Size", &ms->gridSize, 0.25f, 0.1f, 500.0f);
                ImGui::SetItemTooltip("World extent of the evaluation grid. Blobs outside it are not captured.");
                ImGui::Checkbox("Smooth Normals", &ms->smoothNormals);
                ImGui::Checkbox("Auto Center", &ms->autoCenter);
                ImGui::SetItemTooltip("Keep the grid centred on the group centroid as blobs move");
                ImGui::DragFloat("Update Rate (Hz)", &ms->updateRate, 1.0f, 0.0f, 240.0f);
                ImGui::SetItemTooltip("0 = rebuild every frame");
                ImGui::Checkbox("Use Blob Colors", &ms->useBlobColors);
                i32 blobCount = 0;
                for (ECS::Entity be : m_World->GetEntitiesWithComponent<Effects::MetaballComponent>()) {
                    auto* b = m_World->GetComponent<Effects::MetaballComponent>(be);
                    if (b && b->groupId == ms->groupId) ++blobCount;
                }
                ImGui::TextDisabled("%d blob(s) in group %d", blobCount, ms->groupId);
                if (blobCount == 0)
                    ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                        "No Metaball Blob uses this group.");
            }
        }
        if (auto* ca = m_World->GetComponent<ECS::CellularAutomataComponent>(m_PrimarySelected)) {
            bool caOpen = ImGui::CollapsingHeader("Cellular Automata", ImGuiTreeNodeFlags_DefaultOpen);
            bool caRemoved = false;
            if (ImGui::BeginPopupContextItem("CellularAutomataCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::CellularAutomataComponent>(m_PrimarySelected, "cellularAutomata", "Cellular Automata");
                    caRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!caRemoved && caOpen) {
                DrawComponentHelp("cellularAutomata", m_World, m_PrimarySelected);
                const char* rules[] = { "Game of Life (B3/S23)", "HighLife (B36/S23)", "Day and Night",
                                        "Seeds (B2/S)", "Brian's Brain", "Rule 110", "Diamoeba", "Custom" };
                int r = static_cast<int>(ca->rule);
                if (ImGui::Combo("Rule", &r, rules, IM_ARRAYSIZE(rules)))
                    ca->rule = static_cast<Effects::CARule>(r);
                const char* modes[] = { "Voxels", "Marching Cubes", "Point Cloud" };
                int mm = static_cast<int>(ca->meshMode);
                if (ImGui::Combo("Mesh Mode", &mm, modes, IM_ARRAYSIZE(modes)))
                    ca->meshMode = static_cast<Effects::CAMeshMode>(mm);
                i32 w = static_cast<i32>(ca->width), h = static_cast<i32>(ca->height), d = static_cast<i32>(ca->depth);
                if (ImGui::DragInt("Width", &w, 1, 2, 256)) ca->width = static_cast<u32>(w);
                if (ImGui::DragInt("Height", &h, 1, 2, 256)) ca->height = static_cast<u32>(h);
                if (ImGui::DragInt("Depth", &d, 1, 1, 64)) ca->depth = static_cast<u32>(d);
                ImGui::SetItemTooltip("Depth > 1 layers the automaton into 3D");
                ImGui::DragFloat("Cell Size", &ca->cellSize, 0.01f, 0.001f, 10.0f);
                ImGui::DragFloat("Step Interval", &ca->updateInterval, 0.01f, 0.0f, 10.0f, "%.3f s");
                ImGui::DragFloat("Initial Fill %%", &ca->initialFillPercent, 1.0f, 0.0f, 100.0f);
                i32 seed = static_cast<i32>(ca->seed);
                if (ImGui::DragInt("Seed", &seed, 1, 0, 1000000)) ca->seed = static_cast<u32>(seed < 0 ? 0 : seed);
                ImGui::Checkbox("Wrap Edges", &ca->wrapEdges);
                if (ca->meshMode == Effects::CAMeshMode::MarchingCubes)
                    ImGui::SliderFloat("Iso Level", &ca->isoLevel, 0.0f, 1.0f, "%.2f");
                f32 lc[3] = { ca->liveColor.x, ca->liveColor.y, ca->liveColor.z };
                if (ImGui::ColorEdit3("Live Color", lc)) ca->liveColor = Math::Vector3(lc[0], lc[1], lc[2]);
                f32 dc[3] = { ca->dyingColor.x, ca->dyingColor.y, ca->dyingColor.z };
                if (ImGui::ColorEdit3("Dying Color", dc)) ca->dyingColor = Math::Vector3(dc[0], dc[1], dc[2]);
                const char* stamps[] = { "Random fill", "Glider", "Pulsar", "Gosper Glider Gun" };
                int si = 0;
                if (ca->stampPattern == "glider") si = 1;
                else if (ca->stampPattern == "pulsar") si = 2;
                else if (ca->stampPattern == "gospergun") si = 3;
                if (ImGui::Combo("Start Pattern", &si, stamps, IM_ARRAYSIZE(stamps))) {
                    ca->stampPattern = (si == 1) ? "glider" : (si == 2) ? "pulsar" : (si == 3) ? "gospergun" : "";
                    ca->resetRequested = true;
                }
                ImGui::Checkbox("Running", &ca->running);
                ImGui::SameLine();
                if (ImGui::Button("Reset")) ca->resetRequested = true;
                ImGui::TextDisabled("generation %u, %u live cell(s)", ca->generation, ca->liveCells);
            }
        }
        if (auto* p4 = m_World->GetComponent<ECS::Projection4DComponent>(m_PrimarySelected)) {
            bool p4Open = ImGui::CollapsingHeader("4D Projection", ImGuiTreeNodeFlags_DefaultOpen);
            bool p4Removed = false;
            if (ImGui::BeginPopupContextItem("Projection4DCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::Projection4DComponent>(m_PrimarySelected, "projection4D", "4D Projection");
                    p4Removed = true;
                }
                ImGui::EndPopup();
            }
            if (!p4Removed && p4Open) {
                DrawComponentHelp("projection4D", m_World, m_PrimarySelected);
                const char* polys[] = { "Tesseract (8-cell)", "5-cell (simplex)", "16-cell", "24-cell", "120-cell" };
                int pi = static_cast<int>(p4->polytope);
                if (ImGui::Combo("Polytope", &pi, polys, IM_ARRAYSIZE(polys)))
                    p4->polytope = static_cast<ECS::Projection4DComponent::Polytope>(pi);
                if (p4->polytope == ECS::Projection4DComponent::Polytope::Cell120)
                    ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                        "1200 edges. Lower Tube Segments or Update Rate if this stutters.");
                ImGui::SeparatorText("Rotation (radians/second)");
                ImGui::DragFloat("XY", &p4->rotation.speedXY, 0.01f, -10.0f, 10.0f);
                ImGui::DragFloat("XZ", &p4->rotation.speedXZ, 0.01f, -10.0f, 10.0f);
                ImGui::DragFloat("XW", &p4->rotation.speedXW, 0.01f, -10.0f, 10.0f);
                ImGui::SetItemTooltip("Rotations involving W are the ones that look impossible in 3D");
                ImGui::DragFloat("YZ", &p4->rotation.speedYZ, 0.01f, -10.0f, 10.0f);
                ImGui::DragFloat("YW", &p4->rotation.speedYW, 0.01f, -10.0f, 10.0f);
                ImGui::DragFloat("ZW", &p4->rotation.speedZW, 0.01f, -10.0f, 10.0f);
                ImGui::SeparatorText("Projection");
                ImGui::DragFloat("Line Width", &p4->lineWidth, 0.002f, 0.0005f, 1.0f, "%.4f");
                ImGui::DragFloat("Projection Distance", &p4->projectionDistance, 0.05f, 0.5f, 20.0f);
                i32 ts = static_cast<i32>(p4->tubeSegments);
                if (ImGui::DragInt("Tube Segments", &ts, 1, 3, 32)) p4->tubeSegments = static_cast<u32>(ts);
                ImGui::DragFloat("Scale", &p4->scale, 0.05f, 0.01f, 100.0f);
                ImGui::Checkbox("Animate", &p4->animate);
                ImGui::DragFloat("Update Rate (Hz)", &p4->updateRate, 1.0f, 0.0f, 240.0f);
            }
        }
        if (auto* fm = m_World->GetComponent<ECS::FourierMeshComponent>(m_PrimarySelected)) {
            bool fmOpen = ImGui::CollapsingHeader("Fourier Mesh", ImGuiTreeNodeFlags_DefaultOpen);
            bool fmRemoved = false;
            if (ImGui::BeginPopupContextItem("FourierMeshCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::FourierMeshComponent>(m_PrimarySelected, "fourierMesh", "Fourier Mesh");
                    fmRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!fmRemoved && fmOpen) {
                DrawComponentHelp("fourierMesh", m_World, m_PrimarySelected);
                const char* srcs[] = { "Circle", "Square", "Star", "Heart", "Custom" };
                int si = static_cast<int>(fm->source);
                if (ImGui::Combo("Contour", &si, srcs, IM_ARRAYSIZE(srcs)))
                    fm->source = static_cast<ECS::FourierMeshComponent::ContourSource>(si);
                if (fm->source == ECS::FourierMeshComponent::ContourSource::Custom && fm->customContour.size() < 3)
                    ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f),
                        "Custom contour needs at least 3 points (set from script).");
                ImGui::DragInt("Coefficients", &fm->coefficients, 1, 0, 1024);
                ImGui::SetItemTooltip("DFT terms computed. 0 = one per sample.");
                ImGui::DragInt("Terms", &fm->terms, 1, 1, 1024);
                ImGui::SetItemTooltip("Terms used in the reconstruction. Fewer terms = rounder approximation.");
                ImGui::DragInt("Samples", &fm->samples, 8, 16, 4096);
                ImGui::Checkbox("Animate Terms", &fm->animateTerms);
                if (fm->animateTerms)
                    ImGui::DragFloat("Animation Seconds", &fm->animationSeconds, 0.1f, 0.0f, 600.0f);
                ImGui::DragFloat("Extrude Depth", &fm->extrudeDepth, 0.01f, 0.0f, 100.0f);
                ImGui::SetItemTooltip("0 = flat triangulated contour, above 0 = extruded solid");
                ImGui::DragFloat("Scale", &fm->scale, 0.05f, 0.01f, 100.0f);
                ImGui::TextDisabled("%d of %d term(s) active", fm->activeTerms, fm->terms);
            }
        }
        if (auto* pm = m_World->GetComponent<ECS::ProceduralMeshComponent>(m_PrimarySelected)) {
            if (ImGui::CollapsingHeader("Procedural Mesh")) {
                ImGui::TextDisabled("Generated by: %s", ECS::ProceduralMeshSourceName(pm->source));
                ImGui::Checkbox("Regenerate", &pm->regenerate);
                ImGui::SetItemTooltip("Uncheck to freeze the current geometry and stop the owning system rebuilding it");
                if (auto* pmMesh = m_World->GetComponent<ECS::MeshComponent>(m_PrimarySelected))
                    ImGui::TextDisabled("%zu vertices, %zu triangles",
                                        pmMesh->vertices.size(), pmMesh->indices.size() / 3);
                ImGui::TextDisabled("Added automatically. Removing it stops GPU uploads.");
            }
        }
        if (auto* sw = m_World->GetComponent<ECS::SwarmComponent>(m_PrimarySelected)) {
            bool swOpen = ImGui::CollapsingHeader("Swarm", ImGuiTreeNodeFlags_DefaultOpen);
            bool swRemoved = false;
            if (ImGui::BeginPopupContextItem("SwarmCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::SwarmComponent>(m_PrimarySelected, "swarm", "Swarm");
                    swRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!swRemoved && swOpen) {
                DrawComponentHelp("swarm", m_World, m_PrimarySelected);
                i32 count = static_cast<i32>(sw->count);
                if (ImGui::DragInt("Count", &count, 10, 1, 200000)) sw->count = static_cast<u32>(count < 1 ? 1 : count);
                i32 renderCap = static_cast<i32>(sw->renderCap);
                if (ImGui::DragInt("Render Cap", &renderCap, 10, 1, 200000)) sw->renderCap = static_cast<u32>(renderCap < 1 ? 1 : renderCap);
                ImGui::SetItemTooltip("Max visible proxy cubes (sim count can exceed this)");
                ImGui::DragFloat("Spawn Radius", &sw->spawnRadius, 0.5f, 0.5f, 500.0f);
                ImGui::DragFloat("Max Speed", &sw->maxSpeed, 0.1f, 0.1f, 100.0f);
                ImGui::DragFloat("Pull", &sw->pull, 0.05f, 0.0f, 20.0f);
                ImGui::SetItemTooltip("Attraction toward this entity");
                ImGui::SliderFloat("Damping", &sw->damping, 0.0f, 1.0f, "%.2f");
                ImGui::DragFloat("Cube Size", &sw->cubeSize, 0.01f, 0.01f, 5.0f);
                f32 swCol[3] = { sw->color.x, sw->color.y, sw->color.z };
                if (ImGui::ColorEdit3("Color", swCol)) sw->color = Math::Vector3(swCol[0], swCol[1], swCol[2]);
                ImGui::TextDisabled("(simulates in Play mode as instanced proxy cubes)");
            }
        }
        if (auto* dg = m_World->GetComponent<ECS::DungeonGeneratorComponent>(m_PrimarySelected)) {
            bool dgOpen = ImGui::CollapsingHeader("Dungeon Generator", ImGuiTreeNodeFlags_DefaultOpen);
            bool dgRemoved = false;
            if (ImGui::BeginPopupContextItem("DungeonGenCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::DungeonGeneratorComponent>(m_PrimarySelected, "dungeonGenerator", "Dungeon Generator");
                    dgRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!dgRemoved && dgOpen) {
                DrawComponentHelp("dungeonGenerator", m_World, m_PrimarySelected);
                const char* algos[] = { "Random Walk (cave)", "Cellular (organic caves)", "BSP (rooms + corridors)" };
                int a = static_cast<int>(dg->algorithm);
                if (ImGui::Combo("Algorithm", &a, algos, IM_ARRAYSIZE(algos)))
                    dg->algorithm = static_cast<ECS::DungeonGeneratorComponent::Algorithm>(a);

                i32 w = static_cast<i32>(dg->width), h = static_cast<i32>(dg->height);
                if (ImGui::DragInt("Width", &w, 1, 4, 512))  dg->width = static_cast<u32>(w < 4 ? 4 : w);
                if (ImGui::DragInt("Height", &h, 1, 4, 512)) dg->height = static_cast<u32>(h < 4 ? 4 : h);
                i32 seed = static_cast<i32>(dg->seed);
                if (ImGui::DragInt("Seed (0 = random)", &seed, 1, 0, 2000000000)) dg->seed = static_cast<u32>(seed < 0 ? 0 : seed);

                if (dg->algorithm == ECS::DungeonGeneratorComponent::Algorithm::RandomWalk) {
                    i32 st = static_cast<i32>(dg->walkSteps);
                    if (ImGui::DragInt("Walk Steps", &st, 10, 10, 100000)) dg->walkSteps = static_cast<u32>(st < 10 ? 10 : st);
                    ImGui::SliderFloat("Turn Chance", &dg->walkTurnChance, 0.0f, 1.0f);
                } else if (dg->algorithm == ECS::DungeonGeneratorComponent::Algorithm::Cellular) {
                    i32 fp = static_cast<i32>(dg->fillPercent);
                    if (ImGui::SliderInt("Fill %", &fp, 0, 100)) dg->fillPercent = static_cast<u32>(fp);
                    i32 it = static_cast<i32>(dg->iterations);
                    if (ImGui::SliderInt("Smoothing Passes", &it, 0, 12)) dg->iterations = static_cast<u32>(it);
                } else {
                    i32 mn = static_cast<i32>(dg->minRoomSize), mx = static_cast<i32>(dg->maxRoomSize);
                    if (ImGui::DragInt("Min Room", &mn, 1, 2, 60)) dg->minRoomSize = static_cast<u32>(mn < 2 ? 2 : mn);
                    if (ImGui::DragInt("Max Room", &mx, 1, 2, 60)) dg->maxRoomSize = static_cast<u32>(mx < 2 ? 2 : mx);
                    i32 sd = static_cast<i32>(dg->splitDepth);
                    if (ImGui::SliderInt("Split Depth", &sd, 1, 8)) dg->splitDepth = static_cast<u32>(sd);
                    i32 cw = static_cast<i32>(dg->corridorWidth);
                    if (ImGui::SliderInt("Corridor Width", &cw, 1, 6)) dg->corridorWidth = static_cast<u32>(cw);
                }

                ImGui::Separator();
                ImGui::DragInt("Floor Tile", &dg->floorTile, 1, -1, 4096);
                ImGui::DragInt("Wall Tile (-1 = empty)", &dg->wallTile, 1, -1, 4096);
                ImGui::Checkbox("Fill Collision", &dg->fillCollision);
                ImGui::Checkbox("Generate On Play Start", &dg->generateOnStart);

                if (ImGui::Button("Generate Now")) {
                    auto* tm = m_World->GetComponent<ECS::TilemapComponent>(m_PrimarySelected);
                    if (!tm) { m_World->AddComponent<ECS::TilemapComponent>(m_PrimarySelected); tm = m_World->GetComponent<ECS::TilemapComponent>(m_PrimarySelected); }
                    if (tm) {
                        ECS::DungeonGeneratorSystem::Generate(*dg, *tm);
                        ENJIN_LOG_INFO(Editor, "Generated dungeon (seed %u) into tilemap", dg->lastSeed);
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("seed used: %u", dg->lastSeed);
                ImGui::TextDisabled("(paints this entity's Tilemap; add a Tilemap + tileset to see it)");
            }
        }
        if (auto* bag = m_World->GetComponent<ECS::RandomBagComponent>(m_PrimarySelected)) {
            bool bagOpen = ImGui::CollapsingHeader("Random Bag", ImGuiTreeNodeFlags_DefaultOpen);
            bool bagRemoved = false;
            if (ImGui::BeginPopupContextItem("RandomBagCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::RandomBagComponent>(m_PrimarySelected, "randomBag", "Random Bag");
                    bagRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!bagRemoved && bagOpen) {
                DrawComponentHelp("randomBag", m_World, m_PrimarySelected);

                const char* modes[] = {
                    "Uniform (equal chance, repeats)",
                    "Weighted (by weight, repeats)",
                    "No-Replace / 7-bag (each once, then reshuffle)",
                    "Deck (each item 'weight' times, then reshuffle)",
                    "Markov (next draw depends on current item)"
                };
                int m = static_cast<int>(bag->mode);
                if (ImGui::Combo("Mode", &m, modes, IM_ARRAYSIZE(modes)))
                    bag->mode = static_cast<ECS::RandomBagComponent::Mode>(m);

                const bool isMarkov = (bag->mode == ECS::RandomBagComponent::Mode::Markov);
                const bool usesWeight = (bag->mode == ECS::RandomBagComponent::Mode::Weighted ||
                                         bag->mode == ECS::RandomBagComponent::Mode::Deck ||
                                         isMarkov);   // Markov: weight = first-draw chance
                const bool usesRepeatGuard = (bag->mode == ECS::RandomBagComponent::Mode::Uniform ||
                                              bag->mode == ECS::RandomBagComponent::Mode::Weighted);

                ImGui::Separator();
                ImGui::TextDisabled(usesWeight ? "Items  (name + weight)" : "Items  (name)");
                int removeIdx = -1;
                for (int i = 0; i < static_cast<int>(bag->items.size()); ++i) {
                    ImGui::PushID(i);
                    char buf[128];
                    std::snprintf(buf, sizeof(buf), "%s", bag->items[i].name.c_str());
                    ImGui::SetNextItemWidth(usesWeight ? 150.0f : 220.0f);
                    if (ImGui::InputText("##name", buf, sizeof(buf)))
                        bag->items[i].name = buf;
                    if (usesWeight) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(70.0f);
                        ImGui::DragFloat("##w", &bag->items[i].weight, 0.1f, 0.0f, 100000.0f, "%.2f");
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) removeIdx = i;
                    ImGui::PopID();
                }
                if (removeIdx >= 0) bag->items.erase(bag->items.begin() + removeIdx);
                if (ImGui::Button("+ Add Item")) {
                    ECS::RandomBagComponent::Item it;
                    it.name = "item" + std::to_string(bag->items.size());
                    bag->items.push_back(it);
                }

                if (isMarkov && !bag->items.empty()) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Transition matrix  (row = current item, columns = chance of NEXT item)");
                    const int n = static_cast<int>(bag->items.size());
                    // Keep the flattened matrix sized N x N, preserving authored values
                    // when the item count changes (old size is always a perfect square).
                    if (static_cast<int>(bag->transitions.size()) != n * n) {
                        int oldN = static_cast<int>(std::round(std::sqrt(static_cast<double>(bag->transitions.size()))));
                        std::vector<f32> next(static_cast<usize>(n) * n, 1.0f);
                        for (int r = 0; r < std::min(oldN, n); ++r)
                            for (int c = 0; c < std::min(oldN, n); ++c)
                                next[static_cast<usize>(r) * n + c] = bag->transitions[static_cast<usize>(r) * oldN + c];
                        bag->transitions = std::move(next);
                    }
                    for (int r = 0; r < n; ++r) {
                        ImGui::PushID(1000 + r);
                        ImGui::Text("%.10s ->", bag->items[r].name.c_str());
                        for (int c = 0; c < n; ++c) {
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(46.0f);
                            ImGui::DragFloat(("##t" + std::to_string(c)).c_str(),
                                             &bag->transitions[static_cast<usize>(r) * n + c],
                                             0.05f, 0.0f, 1000.0f, "%.1f");
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("%s -> %s", bag->items[r].name.c_str(), bag->items[c].name.c_str());
                        }
                        ImGui::PopID();
                    }
                    if (ImGui::SmallButton("Reset Matrix (uniform)")) {
                        std::fill(bag->transitions.begin(), bag->transitions.end(), 1.0f);
                    }
                    ImGui::TextDisabled("(item weight above = FIRST draw; a row of zeros falls back to uniform)");
                }

                ImGui::Separator();
                i32 seed = static_cast<i32>(bag->seed);
                if (ImGui::DragInt("Seed (0 = random)", &seed, 1, 0, 2000000000))
                    bag->seed = static_cast<u32>(seed < 0 ? 0 : seed);
                if (usesRepeatGuard)
                    ImGui::Checkbox("Avoid immediate repeat", &bag->avoidImmediateRepeat);

                ImGui::Separator();
                if (ImGui::Button("Test Draw")) {
                    std::string r = ECS::RandomBagSystem::Draw(*bag);
                    bag->editorPreview = r.empty() ? "(empty bag)" : r;
                }
                ImGui::SameLine();
                if (ImGui::Button("Draw x10")) {
                    std::string line;
                    for (int k = 0; k < 10; ++k) {
                        std::string r = ECS::RandomBagSystem::Draw(*bag);
                        line += (r.empty() ? "-" : r);
                        if (k < 9) line += ", ";
                    }
                    bag->editorPreview = line;
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset")) {
                    ECS::RandomBagSystem::Reset(*bag);
                    bag->editorPreview.clear();
                }
                if (!bag->editorPreview.empty()) {
                    ImGui::TextWrapped("Drew: %s", bag->editorPreview.c_str());
                }
                ImGui::TextDisabled("remaining before reshuffle: %u", ECS::RandomBagSystem::Remaining(*bag));
            }
        }
        if (auto* sc = m_World->GetComponent<ECS::ScatterComponent>(m_PrimarySelected)) {
            bool scOpen = ImGui::CollapsingHeader("Scatter", ImGuiTreeNodeFlags_DefaultOpen);
            bool scRemoved = false;
            if (ImGui::BeginPopupContextItem("ScatterCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    ECS::ScatterSystem::Clear(m_World, m_PrimarySelected); // drop the batch with the component
                    RemoveComponentWithUndo<ECS::ScatterComponent>(m_PrimarySelected, "scatter", "Scatter");
                    scRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!scRemoved && scOpen) {
                DrawComponentHelp("scatter", m_World, m_PrimarySelected);

                const char* dists[] = {
                    "Uniform (random, may clump)",
                    "Poisson (blue-noise, even spacing)",
                    "Jittered Grid (regular but organic)",
                    "Voronoi (relaxed, organically even)"
                };
                int d = static_cast<int>(sc->distribution);
                if (ImGui::Combo("Distribution", &d, dists, IM_ARRAYSIZE(dists)))
                    sc->distribution = static_cast<ECS::ScatterComponent::Distribution>(d);

                const char* planes[] = { "XZ (3D ground)", "XY (2D plane)" };
                int pl = static_cast<int>(sc->plane);
                if (ImGui::Combo("Plane", &pl, planes, IM_ARRAYSIZE(planes)))
                    sc->plane = static_cast<ECS::ScatterComponent::Plane>(pl);

                // Prefab path: text field + drop target for an .enjprefab from the browser.
                char pbuf[512];
                std::snprintf(pbuf, sizeof(pbuf), "%s", sc->prefabPath.c_str());
                if (ImGui::InputText("Prefab", pbuf, sizeof(pbuf)))
                    sc->prefabPath = pbuf;
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* pl2 = ImGui::AcceptDragDropPayload("ASSET_PATH"))
                        sc->prefabPath = std::string(static_cast<const char*>(pl2->Data));
                    ImGui::EndDragDropTarget();
                }
                ImGui::TextDisabled("(drop an .enjprefab here, or type its path)");

                ImGui::Separator();
                ImGui::DragFloat("Region Width",  &sc->regionWidth,  0.5f, 0.0f, 100000.0f, "%.1f");
                ImGui::DragFloat("Region Height", &sc->regionHeight, 0.5f, 0.0f, 100000.0f, "%.1f");
                if (sc->distribution == ECS::ScatterComponent::Distribution::Poisson) {
                    ImGui::DragFloat("Min Spacing", &sc->minSpacing, 0.05f, 0.05f, 10000.0f, "%.2f");
                    ImGui::TextDisabled("(Poisson fits as many as spacing allows)");
                } else {
                    i32 tc = static_cast<i32>(sc->targetCount);
                    if (ImGui::DragInt("Count", &tc, 1, 0, 20000)) sc->targetCount = static_cast<u32>(tc < 0 ? 0 : tc);
                }
                if (sc->distribution == ECS::ScatterComponent::Distribution::Voronoi) {
                    i32 ri = static_cast<i32>(sc->relaxIterations);
                    if (ImGui::SliderInt("Relax Iterations", &ri, 0, 16)) sc->relaxIterations = static_cast<u32>(ri);
                    ImGui::TextDisabled("(0 = plain random; more passes spread points more evenly)");
                }

                ImGui::Separator();
                ImGui::DragFloatRange2("Scale Range", &sc->scaleMin, &sc->scaleMax, 0.01f, 0.01f, 100.0f, "%.2f");
                ImGui::Checkbox("Random Yaw", &sc->randomYaw);
                ImGui::DragFloat("Height Jitter", &sc->heightJitter, 0.05f, 0.0f, 10000.0f, "%.2f");
                if (sc->plane == ECS::ScatterComponent::Plane::XZ) {
                    ImGui::Checkbox("Conform To Terrain", &sc->conformToTerrain);
                    if (sc->conformToTerrain) {
                        ImGui::SliderFloat("Max Slope (deg)", &sc->maxSlopeDeg, 0.0f, 90.0f, "%.0f");
                        const bool hasTerrain = m_World->HasComponent<ECS::TerrainComponent>(m_PrimarySelected);
                        if (hasTerrain)
                            ImGui::TextDisabled("(instances drop onto this entity's terrain; steeper cells are culled)");
                        else
                            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                                               "no Terrain on this entity — add the Scatter to the terrain entity");
                    }
                }

                ImGui::Separator();
                i32 seed = static_cast<i32>(sc->seed);
                if (ImGui::DragInt("Seed (0 = random)", &seed, 1, 0, 2000000000))
                    sc->seed = static_cast<u32>(seed < 0 ? 0 : seed);
                ImGui::Checkbox("Generate On Play Start", &sc->generateOnStart);

                if (ImGui::Button("Generate Now")) {
                    ECS::ScatterSystem::Generate(m_World, m_PrimarySelected, *sc);
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear")) {
                    ECS::ScatterSystem::Clear(m_World, m_PrimarySelected);
                    sc->lastCount = 0;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("placed: %u  (seed %u)", sc->lastCount, sc->lastSeed);
                ImGui::TextDisabled("(instances are children of this entity; set a prefab to see them)");
            }
        }
        if (auto* tg = m_World->GetComponent<ECS::TerrainGeneratorComponent>(m_PrimarySelected)) {
            bool tgOpen = ImGui::CollapsingHeader("Terrain Generator", ImGuiTreeNodeFlags_DefaultOpen);
            bool tgRemoved = false;
            if (ImGui::BeginPopupContextItem("TerrainGenCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::TerrainGeneratorComponent>(m_PrimarySelected, "terrainGenerator", "Terrain Generator");
                    tgRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!tgRemoved && tgOpen) {
                DrawComponentHelp("terrainGenerator", m_World, m_PrimarySelected);

                i32 gw = static_cast<i32>(tg->gridWidth), gh = static_cast<i32>(tg->gridHeight);
                if (ImGui::DragInt("Grid Width", &gw, 1, 4, 1024))  tg->gridWidth = static_cast<u32>(gw < 4 ? 4 : gw);
                if (ImGui::DragInt("Grid Height", &gh, 1, 4, 1024)) tg->gridHeight = static_cast<u32>(gh < 4 ? 4 : gh);
                ImGui::DragFloat("Cell Size", &tg->cellSize, 0.05f, 0.05f, 1000.0f, "%.2f");
                ImGui::DragFloat("Max Height", &tg->maxHeight, 0.5f, 0.0f, 100000.0f, "%.1f");

                ImGui::Separator();
                ImGui::Checkbox("Ridged (mountains)", &tg->ridged);
                i32 oct = static_cast<i32>(tg->octaves);
                if (ImGui::SliderInt("Octaves", &oct, 1, 12)) tg->octaves = static_cast<u32>(oct);
                ImGui::DragFloat("Lacunarity", &tg->lacunarity, 0.01f, 1.5f, 3.0f, "%.2f");
                ImGui::DragFloat("Gain", &tg->gain, 0.01f, 0.2f, 0.8f, "%.2f");
                ImGui::DragFloat("Frequency", &tg->frequency, 0.01f, 0.05f, 20.0f, "%.2f");
                if (tg->ridged)
                    ImGui::DragFloat("Ridge Sharpness", &tg->ridgedPower, 0.05f, 0.5f, 6.0f, "%.2f");

                ImGui::Separator();
                ImGui::Checkbox("Hydraulic Erosion (rain)", &tg->hydraulic);
                if (tg->hydraulic) {
                    i32 dr = static_cast<i32>(tg->hydraulicDroplets);
                    if (ImGui::DragInt("Droplets", &dr, 1000, 1000, 500000)) tg->hydraulicDroplets = static_cast<u32>(dr < 1000 ? 1000 : dr);
                }
                ImGui::Checkbox("Thermal Erosion (slump)", &tg->thermal);
                if (tg->thermal) {
                    i32 it = static_cast<i32>(tg->thermalIterations);
                    if (ImGui::SliderInt("Passes", &it, 1, 200)) tg->thermalIterations = static_cast<u32>(it);
                    ImGui::DragFloat("Talus Angle", &tg->talusAngle, 0.005f, 0.001f, 1.0f, "%.3f");
                }

                ImGui::Separator();
                ImGui::Checkbox("Auto-Splat (paint by slope + height)", &tg->autoSplat);
                if (tg->autoSplat) {
                    ImGui::SliderFloat("Rock Slope (deg)", &tg->rockSlopeDeg, 5.0f, 80.0f, "%.0f");
                    ImGui::SliderFloat("Snow Above (%)", &tg->snowHeightFrac, 0.0f, 1.0f, "%.2f");
                    ImGui::SliderFloat("Shore Below (%)", &tg->shoreHeightFrac, 0.0f, 1.0f, "%.2f");
                    ImGui::SliderFloat("Blend Softness", &tg->splatBlend, 0.0f, 0.5f, "%.2f");
                    ImGui::TextDisabled("(layers: 0 base/grass, 1 rock, 2 snow, 3 shore — assign their textures on the Terrain)");
                }

                ImGui::Separator();
                i32 seed = static_cast<i32>(tg->seed);
                if (ImGui::DragInt("Seed (0 = random)", &seed, 1, 0, 2000000000)) tg->seed = static_cast<u32>(seed < 0 ? 0 : seed);
                ImGui::Checkbox("Generate On Play Start", &tg->generateOnStart);

                if (ImGui::Button("Generate Now")) {
                    auto* terrain = m_World->GetComponent<ECS::TerrainComponent>(m_PrimarySelected);
                    if (!terrain) { m_World->AddComponent<ECS::TerrainComponent>(m_PrimarySelected); terrain = m_World->GetComponent<ECS::TerrainComponent>(m_PrimarySelected); }
                    if (terrain) {
                        ECS::TerrainGeneratorSystem::Generate(*tg, *terrain);
                        ENJIN_LOG_INFO(Editor, "Generated terrain (seed %u)", tg->lastSeed);
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("seed used: %u", tg->lastSeed);
                if (tg->hydraulic)
                    ImGui::TextDisabled("(hydraulic erosion runs %u droplets — Generate may take a moment)", tg->hydraulicDroplets);
            }
        }
        if (auto* wfc = m_World->GetComponent<ECS::WFCComponent>(m_PrimarySelected)) {
            bool wfcOpen = ImGui::CollapsingHeader("Wave Function Collapse", ImGuiTreeNodeFlags_DefaultOpen);
            bool wfcRemoved = false;
            if (ImGui::BeginPopupContextItem("WFCCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::WFCComponent>(m_PrimarySelected, "wfc", "Wave Function Collapse");
                    wfcRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!wfcRemoved && wfcOpen) {
                DrawComponentHelp("wfc", m_World, m_PrimarySelected);

                const char* modes[] = { "2D Tiles (paints a Tilemap)", "3D Modules (places prefabs)" };
                int md = static_cast<int>(wfc->mode);
                if (ImGui::Combo("Mode", &md, modes, IM_ARRAYSIZE(modes)))
                    wfc->mode = static_cast<ECS::WFCComponent::Mode>(md);
                const bool is3D = (wfc->mode == ECS::WFCComponent::Mode::Modules3D);

                i32 ww = static_cast<i32>(wfc->width), wh = static_cast<i32>(wfc->height);
                if (ImGui::DragInt("Width",  &ww, 1, 1, is3D ? 64 : 1024)) wfc->width  = static_cast<u32>(ww < 1 ? 1 : ww);
                if (ImGui::DragInt("Height", &wh, 1, 1, is3D ? 64 : 1024)) wfc->height = static_cast<u32>(wh < 1 ? 1 : wh);
                if (is3D) {
                    i32 wd = static_cast<i32>(wfc->depth);
                    if (ImGui::DragInt("Depth (layers up)", &wd, 1, 1, 32)) wfc->depth = static_cast<u32>(wd < 1 ? 1 : wd);
                    ImGui::DragFloat("Cell Size", &wfc->cellSize, 0.05f, 0.05f, 1000.0f, "%.2f");
                }

                ImGui::Separator();
                if (is3D)
                    ImGui::TextDisabled("Tiles  (edge sockets N E S W Up Dn — modules touch where labels match; empty prefab = air)");
                else
                    ImGui::TextDisabled("Tiles  (edge sockets N E S W — tiles touch where labels match)");
                const int edgeCount = is3D ? 6 : 4;
                int removeIdx = -1;
                for (int i = 0; i < static_cast<int>(wfc->tiles.size()); ++i) {
                    ImGui::PushID(i);
                    auto& t = wfc->tiles[i];
                    if (is3D) {
                        char pb[512];
                        std::snprintf(pb, sizeof(pb), "%s", t.prefabPath.c_str());
                        ImGui::SetNextItemWidth(150.0f);
                        if (ImGui::InputText("prefab", pb, sizeof(pb))) t.prefabPath = pb;
                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PATH"))
                                t.prefabPath = std::string(static_cast<const char*>(pl->Data));
                            ImGui::EndDragDropTarget();
                        }
                    } else {
                        ImGui::SetNextItemWidth(70.0f);
                        ImGui::DragInt("tile", &t.tileIndex, 1, -1, 4096);
                    }
                    ImGui::SameLine();
                    const char* names[6] = { "N##n", "E##e", "S##s", "W##w", "Up##u", "Dn##d" };
                    for (int e = 0; e < edgeCount; ++e) {
                        char eb[64];
                        std::snprintf(eb, sizeof(eb), "%s", t.edges[e].c_str());
                        ImGui::SetNextItemWidth(46.0f);
                        if (ImGui::InputText(names[e], eb, sizeof(eb))) t.edges[e] = eb;
                        ImGui::SameLine();
                    }
                    if (!is3D) { ImGui::Checkbox("solid", &t.solid); ImGui::SameLine(); }
                    if (ImGui::SmallButton("X")) removeIdx = i;
                    ImGui::PopID();
                }
                if (removeIdx >= 0) wfc->tiles.erase(wfc->tiles.begin() + removeIdx);
                if (ImGui::Button("+ Add Tile")) {
                    ECS::WFCComponent::Tile t;
                    t.tileIndex = static_cast<i32>(wfc->tiles.size());
                    for (int e = 0; e < 6; ++e) t.edges[e] = "a";
                    wfc->tiles.push_back(t);
                }

                ImGui::Separator();
                i32 seed = static_cast<i32>(wfc->seed);
                if (ImGui::DragInt("Seed (0 = random)", &seed, 1, 0, 2000000000)) wfc->seed = static_cast<u32>(seed < 0 ? 0 : seed);
                if (!is3D) {
                    i32 bt = static_cast<i32>(wfc->maxBacktracks);
                    if (ImGui::DragInt("Max Backtracks", &bt, 1, 0, 100000)) wfc->maxBacktracks = static_cast<u32>(bt < 0 ? 0 : bt);
                }
                ImGui::Checkbox("Retry On Fail (reseed)", &wfc->retryOnFail);
                if (wfc->retryOnFail) {
                    i32 mr = static_cast<i32>(wfc->maxRetries);
                    if (ImGui::SliderInt("Max Retries", &mr, 1, 64)) wfc->maxRetries = static_cast<u32>(mr);
                }
                if (!is3D) ImGui::Checkbox("Fill Collision (from 'solid')", &wfc->fillCollision);
                ImGui::Checkbox("Generate On Play Start", &wfc->generateOnStart);

                if (ImGui::Button("Generate Now"))
                    ECS::WFCSystem::Generate(m_World, m_PrimarySelected, *wfc);
                ImGui::SameLine();
                if (wfc->lastSuccess)
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "collapsed OK (seed %u)", wfc->lastSeed);
                else if (wfc->lastSeed != 0)
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "contradiction — loosen rules / more retries");
                else if (wfc->tiles.empty())
                    ImGui::TextDisabled("add tiles, then Generate");
                else
                    ImGui::TextDisabled("press Generate");
                if (is3D)
                    ImGui::TextDisabled("(places %u prefab modules as children; set a prefab per tile to see them)", wfc->lastCount);
                else
                    ImGui::TextDisabled("(paints this entity's Tilemap; add a Tilemap + tileset to see it)");
            }
        }
        if (m_World->HasComponent<ECS::VisualScriptComponent>(m_PrimarySelected)) {
            bool vsOpen = ImGui::CollapsingHeader("Visual Script", ImGuiTreeNodeFlags_DefaultOpen);
            bool vsRemoved = false;
            if (ImGui::BeginPopupContextItem("VisualScriptCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::VisualScriptComponent>(m_PrimarySelected, "visualScript", "Visual Script");
                    vsRemoved = true;
                }
                ImGui::EndPopup();
            }
            if (!vsRemoved && vsOpen) {
                auto* vs = m_World->GetComponent<ECS::VisualScriptComponent>(m_PrimarySelected);
                if (vs) {
                    DrawComponentHelp("visualScript", m_World, m_PrimarySelected);
                    ImGui::Checkbox("Enabled", &vs->enabled);
                    ImGui::Text("Nodes: %zu  Variables: %zu  Functions: %zu",
                                vs->graph.GetNodes().size(), vs->variables.size(), vs->functions.size());
                    ImGui::TextDisabled("(edit the graph in the Visual Script panel)");
                }
            }
        }
        if (m_World->HasComponent<ECS::RecordRewindComponent>(m_PrimarySelected)) {
            DrawRecordRewindComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SceneRewindComponent>(m_PrimarySelected)) {
            DrawSceneRewindComponent(m_PrimarySelected);
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
        if (m_World->HasComponent<ECS::MeshColliderComponent>(m_PrimarySelected)) {
            DrawMeshColliderComponent(m_PrimarySelected);
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
        if (m_World->HasComponent<Physics::Joint2DComponent>(m_PrimarySelected)) {
            DrawJoint2DComponent(m_PrimarySelected);
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
        if (m_World->HasComponent<ECS::GameOverComponent>(m_PrimarySelected)) {
            DrawGameOverComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AudioSourceComponent>(m_PrimarySelected)) {
            DrawAudioSourceComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AudioListenerComponent>(m_PrimarySelected)) {
            DrawAudioListenerComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::ReverbZoneComponent>(m_PrimarySelected)) {
            DrawReverbZoneComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AmbientSoundLayerComponent>(m_PrimarySelected)) {
            DrawAmbientSoundLayerComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::MusicZoneComponent>(m_PrimarySelected)) {
            DrawMusicZoneComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AudioSnapshotTriggerComponent>(m_PrimarySelected)) {
            DrawAudioSnapshotTriggerComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AudioOcclusionComponent>(m_PrimarySelected)) {
            DrawAudioOcclusionComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::LipSyncComponent>(m_PrimarySelected)) {
            DrawLipSyncComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AudioReactiveComponent>(m_PrimarySelected)) {
            DrawAudioReactiveComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AudioThresholdTriggerComponent>(m_PrimarySelected)) {
            DrawAudioThresholdTriggerComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::RTPCComponent>(m_PrimarySelected)) {
            DrawRTPCComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::BeatClockComponent>(m_PrimarySelected)) {
            DrawBeatClockComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::BeatSyncComponent>(m_PrimarySelected)) {
            DrawBeatSyncComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::MIDIBindingComponent>(m_PrimarySelected)) {
            DrawMIDIBindingComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AudioFidelityComponent>(m_PrimarySelected)) {
            DrawAudioFidelityComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::ConductorComponent>(m_PrimarySelected)) {
            DrawConductorComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::AudioCollisionComponent>(m_PrimarySelected)) {
            DrawAudioCollisionComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::MaterialInteractionTableComponent>(m_PrimarySelected)) {
            DrawMaterialInteractionTableComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::SidechainComponent>(m_PrimarySelected)) {
            DrawSidechainComponent(m_PrimarySelected);
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
        if (m_World->HasComponent<ECS::VirtualCameraComponent>(m_PrimarySelected)) {
            DrawVirtualCameraComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::LensComponent>(m_PrimarySelected)) {
            DrawLensComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::LookAtTargetComponent>(m_PrimarySelected)) {
            DrawLookAtTargetComponent(m_PrimarySelected);
        }
        if (m_World->HasComponent<ECS::WaypointComponent>(m_PrimarySelected)) {
            DrawWaypointComponent(m_PrimarySelected);
        }

        // IK Components
        if (m_World->HasComponent<ECS::LookAtIKComponent>(m_PrimarySelected)) {
            bool ikOpen = UI::SectionHeader("[IK] Look-At IK", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem("LookAtIKCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::LookAtIKComponent>(m_PrimarySelected, "lookAtIK", "Look-At IK");
                    ImGui::EndPopup();
                } else { ImGui::EndPopup(); }
            } else if (ikOpen) {
                auto* ik = m_World->GetComponent<ECS::LookAtIKComponent>(m_PrimarySelected);
                if (ik) {
                    DrawComponentHelp("lookAtIK", m_World, m_PrimarySelected);
                    ImGui::SeparatorText("Bones");
                    char headBone[128];
                    strncpy(headBone, ik->headBoneName.c_str(), sizeof(headBone) - 1);
                    headBone[sizeof(headBone) - 1] = '\0';
                    if (ImGui::InputText("Head Bone##LookAtIK", headBone, sizeof(headBone))) {
                        ik->headBoneName = headBone;
                    }
                    ImGui::SetItemTooltip("Name of the head bone in the skeleton");
                    char neckBone[128];
                    strncpy(neckBone, ik->neckBoneName.c_str(), sizeof(neckBone) - 1);
                    neckBone[sizeof(neckBone) - 1] = '\0';
                    if (ImGui::InputText("Neck Bone##LookAtIK", neckBone, sizeof(neckBone))) {
                        ik->neckBoneName = neckBone;
                    }
                    ImGui::SetItemTooltip("Name of the neck bone (rotates with head)");

                    ImGui::SeparatorText("Settings");
                    ImGui::SliderFloat("Max Rotation##LookAtIK", &ik->maxRotation, 0.0f, 90.0f);
                    ImGui::SetItemTooltip("Maximum rotation angle in degrees");
                    ImGui::SliderFloat("Smooth Speed##LookAtIK", &ik->smoothSpeed, 0.1f, 20.0f);
                    ImGui::SetItemTooltip("How quickly the head tracks the target");
                    ImGui::SliderFloat("Look Weight##LookAtIK", &ik->lookWeight, 0.0f, 1.0f);
                    ImGui::SetItemTooltip("Blend weight (0 = no IK, 1 = full IK)");

                    ImGui::Spacing();
                    ImGui::DragFloat3("Target Position##LookAtIK", &ik->targetWorldPos.x, 0.1f);
                    ImGui::SetItemTooltip("World-space position the head looks toward");
                }
            }
        }

        if (m_World->HasComponent<ECS::InteractionIKComponent>(m_PrimarySelected)) {
            bool ikOpen = UI::SectionHeader("[IK] Interaction IK", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem("InteractionIKCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::InteractionIKComponent>(m_PrimarySelected, "interactionIK", "Interaction IK");
                    ImGui::EndPopup();
                } else { ImGui::EndPopup(); }
            } else if (ikOpen) {
                auto* ik = m_World->GetComponent<ECS::InteractionIKComponent>(m_PrimarySelected);
                if (ik) {
                    DrawComponentHelp("interactionIK", m_World, m_PrimarySelected);
                    ImGui::SeparatorText("Bones");
                    char handBone[128];
                    strncpy(handBone, ik->handBoneName.c_str(), sizeof(handBone) - 1);
                    handBone[sizeof(handBone) - 1] = '\0';
                    if (ImGui::InputText("Hand Bone##InteractionIK", handBone, sizeof(handBone))) {
                        ik->handBoneName = handBone;
                    }
                    ImGui::SetItemTooltip("Name of the hand/end-effector bone");
                    char elbowBone[128];
                    strncpy(elbowBone, ik->elbowBoneName.c_str(), sizeof(elbowBone) - 1);
                    elbowBone[sizeof(elbowBone) - 1] = '\0';
                    if (ImGui::InputText("Elbow Bone##InteractionIK", elbowBone, sizeof(elbowBone))) {
                        ik->elbowBoneName = elbowBone;
                    }
                    ImGui::SetItemTooltip("Name of the elbow/mid bone");
                    char shoulderBone[128];
                    strncpy(shoulderBone, ik->shoulderBoneName.c_str(), sizeof(shoulderBone) - 1);
                    shoulderBone[sizeof(shoulderBone) - 1] = '\0';
                    if (ImGui::InputText("Shoulder Bone##InteractionIK", shoulderBone, sizeof(shoulderBone))) {
                        ik->shoulderBoneName = shoulderBone;
                    }
                    ImGui::SetItemTooltip("Name of the shoulder/root bone");

                    ImGui::SeparatorText("Settings");
                    ImGui::SliderFloat("Interaction Radius##InteractionIK", &ik->interactionRadius, 0.1f, 10.0f);
                    ImGui::SetItemTooltip("Maximum reach distance for interaction targets");
                    ImGui::SliderFloat("IK Weight##InteractionIK", &ik->ikWeight, 0.0f, 1.0f);
                    ImGui::SetItemTooltip("Blend weight (0 = no IK, 1 = full IK)");
                    ImGui::SliderFloat("Smooth Speed##InteractionIK", &ik->smoothSpeed, 0.1f, 20.0f);
                    ImGui::SetItemTooltip("How quickly the hand reaches toward the target");

                    ImGui::Spacing();
                    char ikTag[128];
                    strncpy(ikTag, ik->interactionTag.c_str(), sizeof(ikTag) - 1);
                    ikTag[sizeof(ikTag) - 1] = '\0';
                    if (ImGui::InputText("Interaction Tag##InteractionIK", ikTag, sizeof(ikTag))) {
                        ik->interactionTag = ikTag;
                    }
                    ImGui::SetItemTooltip("Tag to match on interactable entities");
                }
            }
        }

        if (m_World->HasComponent<ECS::TwoBoneIKComponent>(m_PrimarySelected)) {
            bool ikOpen = UI::SectionHeader("[IK] Two-Bone IK", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem("TwoBoneIKCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::TwoBoneIKComponent>(m_PrimarySelected, "twoBoneIK", "Two-Bone IK");
                    ImGui::EndPopup();
                } else { ImGui::EndPopup(); }
            } else if (ikOpen) {
                auto* ik = m_World->GetComponent<ECS::TwoBoneIKComponent>(m_PrimarySelected);
                if (ik) {
                    DrawComponentHelp("twoBoneIK", m_World, m_PrimarySelected);
                    ImGui::SeparatorText("Bone Chain");
                    char rootBone[128];
                    strncpy(rootBone, ik->rootBoneName.c_str(), sizeof(rootBone) - 1);
                    rootBone[sizeof(rootBone) - 1] = '\0';
                    if (ImGui::InputText("Root Bone##TwoBoneIK", rootBone, sizeof(rootBone))) {
                        ik->rootBoneName = rootBone;
                    }
                    ImGui::SetItemTooltip("Upper bone (e.g. shoulder or thigh)");

                    char midBone[128];
                    strncpy(midBone, ik->midBoneName.c_str(), sizeof(midBone) - 1);
                    midBone[sizeof(midBone) - 1] = '\0';
                    if (ImGui::InputText("Mid Bone##TwoBoneIK", midBone, sizeof(midBone))) {
                        ik->midBoneName = midBone;
                    }
                    ImGui::SetItemTooltip("Middle joint (e.g. elbow or knee)");

                    char endBone[128];
                    strncpy(endBone, ik->endBoneName.c_str(), sizeof(endBone) - 1);
                    endBone[sizeof(endBone) - 1] = '\0';
                    if (ImGui::InputText("End Bone##TwoBoneIK", endBone, sizeof(endBone))) {
                        ik->endBoneName = endBone;
                    }
                    ImGui::SetItemTooltip("End effector (e.g. hand or foot)");

                    ImGui::SeparatorText("Target");
                    ImGui::Checkbox("Use Entity Target##TwoBoneIK", &ik->useEntityTarget);
                    ImGui::SetItemTooltip("Track an entity instead of a fixed position");
                    if (ik->useEntityTarget) {
                        i32 entityId = static_cast<i32>(ik->targetEntity);
                        if (ImGui::InputInt("Target Entity##TwoBoneIK", &entityId)) {
                            ik->targetEntity = static_cast<ECS::Entity>(entityId);
                        }
                        ImGui::SetItemTooltip("Entity ID to track");
                    } else {
                        ImGui::DragFloat3("Target Position##TwoBoneIK", &ik->targetPosition.x, 0.1f);
                        ImGui::SetItemTooltip("World-space position the end effector reaches toward");
                    }

                    ImGui::SeparatorText("Settings");
                    ImGui::SliderFloat("Weight##TwoBoneIK", &ik->weight, 0.0f, 1.0f);
                    ImGui::SetItemTooltip("Blend weight (0 = no IK, 1 = full IK)");
                    ImGui::DragFloat3("Pole Vector##TwoBoneIK", &ik->poleVector.x, 0.1f);
                    ImGui::SetItemTooltip("Direction hint for elbow/knee bend");
                }
            }
        }

        if (m_World->HasComponent<ECS::CineComponent>(m_PrimarySelected)) {
            DrawCineComponent(m_PrimarySelected);
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

        // Parallax Layer component (per-sprite parallax scroll)
        if (m_World->HasComponent<ECS::ParallaxLayerComponent>(m_PrimarySelected)) {
            bool plOpen = UI::SectionHeader("[//] Parallax Layer", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem("ParallaxLayerCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::ParallaxLayerComponent>(m_PrimarySelected, "parallaxLayer", "Parallax Layer");
                }
                ImGui::EndPopup();
            }
            if (plOpen) {
                auto* pl = m_World->GetComponent<ECS::ParallaxLayerComponent>(m_PrimarySelected);
                if (pl) {
                    DrawComponentHelp("parallaxLayer", m_World, m_PrimarySelected);
                    ImGui::DragFloat2("Parallax Factor##PL", &pl->factor.x, 0.01f, 0.0f, 2.0f, "%.2f");
                    ImGui::SetItemTooltip("Per-axis scroll fraction. 0 = locked to the camera (far backdrop), 1 = moves with the world (foreground). Distant layers use small values.");
                    ImGui::DragFloat2("Auto-Scroll##PL", &pl->autoScroll.x, 0.05f, -50.0f, 50.0f, "%.2f");
                    ImGui::SetItemTooltip("Constant drift in world units/second, on top of camera parallax. For endless skies / runners.");
                }
            }
        }

        // Parallax Machine component
        if (m_World->HasComponent<ECS::ParallaxMachineComponent>(m_PrimarySelected)) {
            bool pmOpen = UI::SectionHeader("[||] Parallax Machine", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem("ParallaxMachineCtx")) {
                if (ImGui::MenuItem("Remove Component")) {
                    RemoveComponentWithUndo<ECS::ParallaxMachineComponent>(m_PrimarySelected, "parallaxMachine", "Parallax Machine");
                    ImGui::EndPopup();
                } else {
                    ImGui::EndPopup();
                }
            }
            if (pmOpen) {
                auto* pm = m_World->GetComponent<ECS::ParallaxMachineComponent>(m_PrimarySelected);
                if (pm) {
                    DrawComponentHelp("parallaxMachine", m_World, m_PrimarySelected);
                    ImGui::Checkbox("Enabled##Parallax", &pm->enabled);
                    ImGui::DragFloat("Global Speed##Parallax", &pm->globalSpeed, 0.01f, 0.0f, 10.0f, "%.2f");
                    ImGui::DragFloat2("Origin##Parallax", &pm->origin.x, 0.1f);
                    ImGui::DragFloat2("Auto-Scroll Speed##Parallax", &pm->autoScrollSpeed.x, 0.01f);

                    ImGui::Separator();
                    ImGui::Text("Layers (%zu)", pm->layers.size());

                    int removeIdx = -1;
                    for (int i = 0; i < static_cast<int>(pm->layers.size()); ++i) {
                        auto& layer = pm->layers[i];
                        ImGui::PushID(i);

                        std::string headerLabel = layer.texturePath.empty()
                            ? "Layer " + std::to_string(i)
                            : "Layer " + std::to_string(i) + " (" + layer.texturePath + ")";

                        bool layerOpen = ImGui::TreeNode(headerLabel.c_str());

                        // Per-layer remove button on same line as tree node
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
                        if (ImGui::SmallButton("X##RemoveLayer")) {
                            removeIdx = i;
                        }

                        if (layerOpen) {
                            // Texture path
                            char texPath[256];
                            strncpy(texPath, layer.texturePath.c_str(), sizeof(texPath) - 1);
                            texPath[sizeof(texPath) - 1] = '\0';
                            if (ImGui::InputText("Texture", texPath, sizeof(texPath))) {
                                layer.texturePath = texPath;
                            }

                            ImGui::DragFloat("Distance", &layer.distance, 0.1f, 0.01f, 100.0f, "%.2f");
                            ImGui::DragFloat("Speed Multiplier", &layer.speedMultiplier, 0.01f, 0.0f, 10.0f, "%.2f");
                            ImGui::DragFloat2("Offset", &layer.offset.x, 0.1f);
                            ImGui::DragFloat2("Scale", &layer.scale.x, 0.1f, 0.01f, 1000.0f);
                            ImGui::ColorEdit3("Tint", &layer.tint.x);
                            ImGui::DragFloat("Alpha", &layer.alpha, 0.01f, 0.0f, 1.0f, "%.2f");
                            ImGui::Checkbox("Repeat X", &layer.repeatX);
                            ImGui::SameLine();
                            ImGui::Checkbox("Repeat Y", &layer.repeatY);
                            ImGui::Checkbox("Visible", &layer.visible);
                            ImGui::DragInt("Sort Order", &layer.sortOrder, 1, -100, 100);

                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }

                    // Remove layer if requested (deferred to avoid iterator invalidation)
                    if (removeIdx >= 0 && removeIdx < static_cast<int>(pm->layers.size())) {
                        pm->layers.erase(pm->layers.begin() + removeIdx);
                    }

                    // Add layer button
                    if (ImGui::Button("+ Add Layer##Parallax")) {
                        ECS::ParallaxLayer newLayer;
                        newLayer.sortOrder = static_cast<i32>(pm->layers.size());
                        pm->layers.push_back(newLayer);
                    }
                }
            }
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
            bool waterOpen = UI::SectionHeader("[~] Interactive Water", ImGuiTreeNodeFlags_DefaultOpen);
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
                    DrawComponentHelp("interactiveWater", m_World, m_PrimarySelected);
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
                    ImGui::Text("Current (lazy river)");
                    ImGui::DragFloat3("Flow Direction", &iw->currentDirection.x, 0.05f, -1.0f, 1.0f);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Horizontal flow direction (y ignored). Set Flow Speed > 0 to carry floating objects.");
                    ImGui::DragFloat("Flow Speed", &iw->currentSpeed, 0.05f, 0.0f, 20.0f);
                    ImGui::DragFloat("Flow Pull", &iw->currentPull, 0.05f, 0.0f, 20.0f);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("How fast objects are dragged up to flow speed (per second).");
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
            bool interactorOpen = UI::SectionHeader("[~] Water Interactor", ImGuiTreeNodeFlags_DefaultOpen);
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
                    DrawComponentHelp("waterInteractor", m_World, m_PrimarySelected);
                    ImGui::DragFloat("Splash Multiplier", &wi->splashMultiplier, 0.1f, 0.0f, 10.0f);
                    ImGui::DragFloat("Wake Width", &wi->wakeWidth, 0.1f, 0.0f, 5.0f);
                    ImGui::Checkbox("Generate Wake", &wi->generateWake);
                    ImGui::Checkbox("Apply Buoyancy", &wi->applyBuoyancy);
                    // Per-object density: < 1 floats, > 1 sinks (flamingo 0.05, human 0.95, grill 3.5)
                    ImGui::DragFloat("Density", &wi->density, 0.01f, 0.0f, 10.0f);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Relative to water. < 1 floats, 1 neutral, > 1 sinks.\nflamingo 0.05, foam 0.1, human 0.95, grill 3.5");
                    ImGui::DragFloat("Displaced Volume", &wi->volume, 0.1f, 0.0f, 50.0f);
                    ImGui::DragFloat("Waterlog Rate", &wi->waterlogRate, 0.05f, 0.0f, 5.0f);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Effective density gained per second while submerged.\n0 = never waterlogs (metal); ~0.3 sinks cardboard in a few seconds.");
                    ImGui::DragFloat("Waterlog Max Density", &wi->waterlogMaxDensity, 0.05f, 0.0f, 10.0f);
                }
            }
        }
        // Morph targets (blend shapes)
        if (m_World->HasComponent<ECS::MorphTargetComponent>(m_PrimarySelected)) {
            auto* morph = m_World->GetComponent<ECS::MorphTargetComponent>(m_PrimarySelected);
            if (morph && UI::SectionHeader("[M] Morph Targets", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextDisabled("%zu blend shapes", morph->targets.size());

                ImGui::Separator();
                for (usize i = 0; i < morph->targets.size(); ++i) {
                    f32 w = morph->weights[i];
                    if (ImGui::SliderFloat(morph->targets[i].name.c_str(), &w, 0.0f, 1.0f, "%.2f")) {
                        morph->weights[i] = w;
                        morph->weightsDirty = true;
                    }
                }

                if (morph->targets.size() > 1) {
                    if (ImGui::Button("Reset All##Morph")) {
                        for (auto& w : morph->weights) w = 0.0f;
                        morph->weightsDirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Max All##Morph")) {
                        for (auto& w : morph->weights) w = 1.0f;
                        morph->weightsDirty = true;
                    }
                }
            }
        }

        if (m_World->HasComponent<ECS::AnimatorComponent>(m_PrimarySelected)) {
            // Animator component inspector (inline)
            bool animOpen = UI::SectionHeader("[>] Animator", ImGuiTreeNodeFlags_DefaultOpen);
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
                    DrawComponentHelp("animator", m_World, m_PrimarySelected);
                    auto& animator = animComp->animator;
                    const auto& animations = animator.GetAnimations();

                    // Empty animator guidance: imported models keep their animator
                    // (with all the FBX clips) on the SKINNED MESH entity, which is
                    // a child of the import root — and often shares its name.
                    // Adding a fresh Animator to the root silently does nothing, so
                    // point at the entity that actually owns the animations.
                    if (animations.empty()) {
                        ImGui::PushTextWrapPos();
                        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1.0f),
                            "This animator has no animation clips.");
                        ImGui::PopTextWrapPos();

                        ECS::Entity clipOwner = ECS::INVALID_ENTITY;
                        std::vector<ECS::Entity> walk;
                        walk.push_back(m_PrimarySelected);
                        for (usize wi = 0; wi < walk.size() && clipOwner == ECS::INVALID_ENTITY; ++wi) {
                            for (ECS::Entity child : ECS::GetChildren(m_World, walk[wi])) {
                                auto* ca = m_World->GetComponent<ECS::AnimatorComponent>(child);
                                if (ca && !ca->animator.GetAnimations().empty()) {
                                    clipOwner = child;
                                    break;
                                }
                                walk.push_back(child);
                            }
                        }
                        if (clipOwner != ECS::INVALID_ENTITY) {
                            std::string ownerName = "child entity";
                            if (auto* nc = m_World->GetComponent<ECS::NameComponent>(clipOwner)) {
                                ownerName = nc->name;
                            }
                            ImGui::PushTextWrapPos();
                            ImGui::Text("This model's imported animations live on '%s'.", ownerName.c_str());
                            ImGui::PopTextWrapPos();
                            if (ImGui::SmallButton("Select It")) {
                                SelectEntity(clipOwner);
                            }
                        } else {
                            ImGui::PushTextWrapPos();
                            ImGui::TextDisabled("Animators need a skinned mesh with a skeleton. "
                                "Imported FBX models get one automatically on their mesh entity.");
                            ImGui::PopTextWrapPos();
                        }
                    }

                    // Current animation name (read-only)
                    const auto& currentName = animator.GetCurrentAnimationName();
                    ImGui::Text("Current: %s", currentName.empty() ? "(none)" : currentName.c_str());

                    // Playing state indicator
                    bool isPlaying = animator.IsPlaying();
                    bool isPaused = animator.IsPaused();
                    const char* stateLabel = isPlaying ? (isPaused ? "Paused" : "Playing") : "Stopped";
                    ImGui::Text("State: %s", stateLabel);
                    if (animator.IsBlending()) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("(blending)");
                    }

                    // Visual timeline with seconds, frame ticks, and event markers
                    f32 normalizedTime = animator.GetNormalizedTime();
                    f32 animDuration = 0.0f;
                    const Animation::SkeletalAnimation* curAnimPtr = nullptr;
                    if (!currentName.empty()) {
                        auto itCA = animations.find(currentName);
                        if (itCA != animations.end()) {
                            animDuration = itCA->second.duration;
                            curAnimPtr = &itCA->second;
                        }
                    }

                    // Draw timeline ruler + event markers above the slider
                    {
                        ImVec2 sliderPos = ImGui::GetCursorScreenPos();
                        f32 sliderWidth = ImGui::GetContentRegionAvail().x;
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        f32 rulerH = 18.0f;

                        // Background strip
                        drawList->AddRectFilled(sliderPos, ImVec2(sliderPos.x + sliderWidth, sliderPos.y + rulerH),
                            IM_COL32(30, 30, 35, 255));

                        // Second tick marks
                        if (animDuration > 0.0f) {
                            f32 secondStep = 1.0f;
                            if (animDuration < 2.0f) secondStep = 0.25f;
                            else if (animDuration < 5.0f) secondStep = 0.5f;
                            for (f32 t = 0.0f; t <= animDuration + 0.001f; t += secondStep) {
                                f32 norm = t / animDuration;
                                if (norm > 1.0f) norm = 1.0f;
                                f32 tx = sliderPos.x + norm * sliderWidth;
                                bool isMajor = (std::fmod(t, 1.0f) < 0.01f);
                                f32 tickH = isMajor ? rulerH * 0.6f : rulerH * 0.3f;
                                drawList->AddLine(ImVec2(tx, sliderPos.y + rulerH - tickH),
                                    ImVec2(tx, sliderPos.y + rulerH), IM_COL32(120, 120, 120, 200));
                                if (isMajor) {
                                    char secBuf[16];
                                    snprintf(secBuf, sizeof(secBuf), "%.0fs", t);
                                    drawList->AddText(ImVec2(tx + 2, sliderPos.y), IM_COL32(160, 160, 160, 200), secBuf);
                                }
                            }
                        }

                        // Event markers (clickable — click to seek)
                        if (curAnimPtr && animDuration > 0.0f) {
                            for (const auto& evt : curAnimPtr->events) {
                                f32 normEvtTime = evt.time / animDuration;
                                f32 markerX = sliderPos.x + normEvtTime * sliderWidth;
                                f32 triSize = 5.0f;
                                ImVec2 p1(markerX, sliderPos.y + rulerH);
                                ImVec2 p2(markerX - triSize, sliderPos.y + rulerH - triSize - 1.0f);
                                ImVec2 p3(markerX + triSize, sliderPos.y + rulerH - triSize - 1.0f);
                                drawList->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 200, 50, 255));

                                ImVec2 hitMin(markerX - triSize - 1, sliderPos.y + rulerH - triSize - 2);
                                ImVec2 hitMax(markerX + triSize + 1, sliderPos.y + rulerH + 1);
                                if (ImGui::IsMouseHoveringRect(hitMin, hitMax)) {
                                    ImGui::SetTooltip("Event: %s (%.3fs) — click to seek", evt.name.c_str(), evt.time);
                                    if (ImGui::IsMouseClicked(0)) {
                                        animator.SetNormalizedTime(normEvtTime);
                                        normalizedTime = normEvtTime;
                                    }
                                }
                            }
                        }

                        // Playhead indicator
                        f32 playheadX = sliderPos.x + normalizedTime * sliderWidth;
                        drawList->AddLine(ImVec2(playheadX, sliderPos.y), ImVec2(playheadX, sliderPos.y + rulerH),
                            IM_COL32(255, 80, 80, 255), 2.0f);

                        // Advance cursor past the ruler
                        ImGui::Dummy(ImVec2(sliderWidth, rulerH));
                    }

                    // Time display + slider
                    if (animDuration > 0.0f) {
                        f32 currentSec = normalizedTime * animDuration;
                        char timeBuf[64];
                        snprintf(timeBuf, sizeof(timeBuf), "%.2fs / %.2fs", currentSec, animDuration);
                        if (ImGui::SliderFloat("##Timeline", &normalizedTime, 0.0f, 1.0f, timeBuf)) {
                            animator.SetNormalizedTime(normalizedTime);
                        }
                    } else {
                        ImGui::SliderFloat("##Timeline", &normalizedTime, 0.0f, 1.0f, "%.2f");
                    }

                    // Speed
                    f32 speed = animator.GetSpeed();
                    if (ImGui::DragFloat("Speed##Animator", &speed, 0.01f, 0.0f, 10.0f, "%.2f")) {
                        animator.SetSpeed(speed);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Playback speed multiplier (1.0 = normal)");

                    // Play / Pause / Stop buttons
                    if (isPlaying && !isPaused) {
                        if (ImGui::Button("Pause##Animator")) {
                            animator.Pause();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Stop##Animator")) {
                            animator.StopAndReset();
                        }
                    } else if (isPlaying && isPaused) {
                        if (ImGui::Button("Resume##Animator")) {
                            animator.Resume();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Stop##Animator")) {
                            animator.StopAndReset();
                        }
                    } else {
                        if (!currentName.empty()) {
                            if (ImGui::Button("Play##Animator")) {
                                animator.Play(currentName, 0.0f);
                            }
                        }
                    }

                    // Available animations list with events sub-tree
                    if (!animations.empty() && ImGui::TreeNode("Animations")) {
                        auto& animsMut = animator.GetAnimationsMut();
                        for (auto& [name, anim] : animsMut) {
                            ImGui::PushID(name.c_str());
                            bool isCurrent = (name == currentName);
                            if (isCurrent) {
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
                            }

                            // Use a tree node for each animation so events can nest under it
                            char durBuf[32];
                            snprintf(durBuf, sizeof(durBuf), "%.2fs", anim.duration);
                            std::string animNodeLabel = name + " (" + durBuf + ", " +
                                std::to_string(anim.tracks.size()) + " tracks)##AnimNode_" + name;
                            bool animNodeOpen = ImGui::TreeNode(animNodeLabel.c_str());
                            if (isCurrent) {
                                ImGui::PopStyleColor();
                            }

                            // Play button on same line
                            ImGui::SameLine();
                            std::string playLabel = "Play##" + name;
                            if (ImGui::SmallButton(playLabel.c_str())) {
                                animator.Play(name, 0.2f);
                            }

                            if (animNodeOpen) {
                                // Events sub-tree
                                std::string eventsLabel = "Events (" + std::to_string(anim.events.size()) + ")##Events_" + name;
                                if (ImGui::TreeNode(eventsLabel.c_str())) {
                                    i32 removeIdx = -1;
                                    for (usize ei = 0; ei < anim.events.size(); ++ei) {
                                        auto& evt = anim.events[ei];
                                        ImGui::PushID(static_cast<int>(ei));

                                        // Time slider (0 to duration)
                                        f32 evtTime = evt.time;
                                        ImGui::SetNextItemWidth(100.0f);
                                        if (ImGui::DragFloat("##EvtTime", &evtTime, 0.001f, 0.0f, anim.duration, "%.3fs")) {
                                            evt.time = evtTime;
                                        }
                                        ImGui::SameLine();

                                        // Name text input
                                        char nameBuf[128];
                                        strncpy(nameBuf, evt.name.c_str(), sizeof(nameBuf) - 1);
                                        nameBuf[sizeof(nameBuf) - 1] = '\0';
                                        ImGui::SetNextItemWidth(120.0f);
                                        if (ImGui::InputText("##EvtName", nameBuf, sizeof(nameBuf))) {
                                            evt.name = nameBuf;
                                        }
                                        ImGui::SameLine();

                                        // Remove button
                                        if (ImGui::SmallButton("X##EvtRemove")) {
                                            removeIdx = static_cast<i32>(ei);
                                        }

                                        ImGui::PopID();
                                    }

                                    // Remove event if requested (deferred to avoid iterator invalidation)
                                    if (removeIdx >= 0 && removeIdx < static_cast<i32>(anim.events.size())) {
                                        anim.events.erase(anim.events.begin() + removeIdx);
                                    }

                                    // Add Event button
                                    std::string addEvtLabel = "Add Event##AddEvt_" + name;
                                    if (ImGui::Button(addEvtLabel.c_str())) {
                                        Animation::SkeletalAnimation::AnimEvent newEvt;
                                        newEvt.time = anim.duration * 0.5f;
                                        newEvt.name = "event_" + std::to_string(anim.events.size());
                                        anim.events.push_back(newEvt);
                                    }

                                    ImGui::TreePop();
                                }

                                ImGui::TreePop();
                            }
                            ImGui::PopID();
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

                    // ================================================================
                    // Blend Tree inspector
                    // ================================================================
                    if (ImGui::TreeNode("Blend Tree")) {
                        auto& bt = animComp->blendTree;

                        ImGui::Checkbox("Enabled##BlendTree", &bt.enabled);

                        // Parameter name
                        char paramBuf[128];
                        strncpy(paramBuf, bt.parameterName.c_str(), sizeof(paramBuf) - 1);
                        paramBuf[sizeof(paramBuf) - 1] = '\0';
                        if (ImGui::InputText("Parameter##BT", paramBuf, sizeof(paramBuf))) {
                            bt.parameterName = paramBuf;
                        }

                        // Current parameter value (editable for testing)
                        if (!bt.parameterName.empty()) {
                            f32 paramVal = animComp->GetBlendParameter(bt.parameterName);
                            if (ImGui::DragFloat("Value##BTParam", &paramVal, 0.01f, -100.0f, 100.0f, "%.2f")) {
                                animComp->SetBlendParameter(bt.parameterName, paramVal);
                            }
                        }

                        ImGui::Separator();
                        ImGui::Text("Nodes (%zu):", bt.nodes.size());

                        // Build list of animation names for combo selection
                        std::vector<std::string> btAnimNames;
                        btAnimNames.push_back("(none)");
                        for (const auto& [aname, aval] : animations) {
                            (void)aval;
                            btAnimNames.push_back(aname);
                        }

                        i32 btRemoveIdx = -1;
                        for (usize ni = 0; ni < bt.nodes.size(); ++ni) {
                            auto& node = bt.nodes[ni];
                            ImGui::PushID(static_cast<int>(ni));

                            // Threshold
                            ImGui::SetNextItemWidth(80.0f);
                            ImGui::DragFloat("##BTThreshold", &node.threshold, 0.01f, -100.0f, 100.0f, "%.2f");
                            ImGui::SameLine();

                            // Animation combo
                            ImGui::SetNextItemWidth(140.0f);
                            i32 btCurrentIdx = 0;
                            for (usize ai = 1; ai < btAnimNames.size(); ++ai) {
                                if (btAnimNames[ai] == node.animationName) {
                                    btCurrentIdx = static_cast<i32>(ai);
                                    break;
                                }
                            }
                            std::string btComboLabel = "##BTAnimCombo";
                            if (ImGui::BeginCombo(btComboLabel.c_str(), btAnimNames[btCurrentIdx].c_str())) {
                                for (usize ai = 0; ai < btAnimNames.size(); ++ai) {
                                    bool btSelected = (static_cast<i32>(ai) == btCurrentIdx);
                                    if (ImGui::Selectable(btAnimNames[ai].c_str(), btSelected)) {
                                        node.animationName = (ai == 0) ? "" : btAnimNames[ai];
                                    }
                                    if (btSelected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::SameLine();

                            // Remove node button
                            if (ImGui::SmallButton("X##BTRemove")) {
                                btRemoveIdx = static_cast<i32>(ni);
                            }

                            ImGui::PopID();
                        }

                        if (btRemoveIdx >= 0 && btRemoveIdx < static_cast<i32>(bt.nodes.size())) {
                            bt.nodes.erase(bt.nodes.begin() + btRemoveIdx);
                        }

                        // Add Node button
                        if (ImGui::Button("Add Node##BTAdd")) {
                            Animation::BlendNode newNode;
                            newNode.threshold = bt.nodes.empty() ? 0.0f : bt.nodes.back().threshold + 1.0f;
                            newNode.animationName = "";
                            bt.nodes.push_back(newNode);
                        }
                        ImGui::SameLine();

                        // Sort nodes by threshold
                        if (ImGui::Button("Sort##BTSort")) {
                            std::sort(bt.nodes.begin(), bt.nodes.end(),
                                [](const Animation::BlendNode& a, const Animation::BlendNode& b) {
                                    return a.threshold < b.threshold;
                                });
                        }

                        // Visual 1D blend axis + text preview
                        if (!bt.parameterName.empty() && bt.nodes.size() >= 2) {
                            ImGui::Separator();
                            f32 previewVal = animComp->GetBlendParameter(bt.parameterName);

                            // Draw 1D axis visualization
                            ImVec2 axisPos = ImGui::GetCursorScreenPos();
                            f32 axisWidth = ImGui::GetContentRegionAvail().x;
                            f32 axisH = 28.0f;
                            ImDrawList* btDL = ImGui::GetWindowDrawList();

                            // Compute axis range from node thresholds
                            f32 axisMin = bt.nodes.front().threshold;
                            f32 axisMax = bt.nodes.back().threshold;
                            f32 axisRange = axisMax - axisMin;
                            if (axisRange < 0.001f) axisRange = 1.0f;

                            // Background
                            btDL->AddRectFilled(axisPos, ImVec2(axisPos.x + axisWidth, axisPos.y + axisH),
                                IM_COL32(25, 25, 30, 255), 3.0f);

                            // Draw crossfade regions between adjacent nodes
                            for (usize ni = 0; ni + 1 < bt.nodes.size(); ++ni) {
                                f32 t0 = (bt.nodes[ni].threshold - axisMin) / axisRange;
                                f32 t1 = (bt.nodes[ni + 1].threshold - axisMin) / axisRange;
                                f32 x0 = axisPos.x + t0 * axisWidth;
                                f32 x1 = axisPos.x + t1 * axisWidth;
                                ImU32 regionColor = (ni % 2 == 0) ? IM_COL32(60, 80, 120, 100) : IM_COL32(80, 60, 120, 100);
                                btDL->AddRectFilled(ImVec2(x0, axisPos.y), ImVec2(x1, axisPos.y + axisH), regionColor);
                            }

                            // Draw node markers + labels
                            for (usize ni = 0; ni < bt.nodes.size(); ++ni) {
                                f32 t = (bt.nodes[ni].threshold - axisMin) / axisRange;
                                f32 nx = axisPos.x + t * axisWidth;
                                btDL->AddLine(ImVec2(nx, axisPos.y), ImVec2(nx, axisPos.y + axisH), IM_COL32(200, 200, 200, 200), 1.5f);
                                // Short animation name
                                std::string shortName = bt.nodes[ni].animationName;
                                if (shortName.size() > 10) shortName = shortName.substr(0, 9) + "~";
                                btDL->AddText(ImVec2(nx + 2, axisPos.y + 2), IM_COL32(180, 200, 255, 220), shortName.c_str());
                                // Threshold value at bottom
                                char threshBuf[16];
                                snprintf(threshBuf, sizeof(threshBuf), "%.1f", bt.nodes[ni].threshold);
                                btDL->AddText(ImVec2(nx + 2, axisPos.y + axisH - 13), IM_COL32(160, 160, 160, 180), threshBuf);
                            }

                            // Current value indicator (red playhead)
                            f32 valNorm = (previewVal - axisMin) / axisRange;
                            valNorm = Math::Clamp(valNorm, 0.0f, 1.0f);
                            f32 valX = axisPos.x + valNorm * axisWidth;
                            btDL->AddLine(ImVec2(valX, axisPos.y - 2), ImVec2(valX, axisPos.y + axisH + 2),
                                IM_COL32(255, 80, 80, 255), 2.0f);
                            btDL->AddTriangleFilled(
                                ImVec2(valX, axisPos.y - 2),
                                ImVec2(valX - 4, axisPos.y - 7),
                                ImVec2(valX + 4, axisPos.y - 7),
                                IM_COL32(255, 80, 80, 255));

                            ImGui::Dummy(ImVec2(axisWidth, axisH));

                            // Text preview
                            if (bt.enabled) {
                                std::string previewAnimA, previewAnimB;
                                f32 previewBlend = 0.0f;
                                bt.Evaluate(previewVal, previewAnimA, previewAnimB, previewBlend);
                                if (!previewAnimA.empty()) {
                                    if (previewAnimB.empty()) {
                                        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Output: %s (100%%)", previewAnimA.c_str());
                                    } else {
                                        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f),
                                            "Blend: %s (%.0f%%) + %s (%.0f%%)",
                                            previewAnimA.c_str(), (1.0f - previewBlend) * 100.0f,
                                            previewAnimB.c_str(), previewBlend * 100.0f);
                                    }
                                }
                            }
                        }

                        ImGui::TreePop();
                    }

                    // Skeleton bone hierarchy tree
                    const auto* skeleton = animator.GetSkeleton();
                    if (skeleton && !skeleton->bones.empty() && ImGui::TreeNode("Bones")) {
                        ImGui::Text("%zu bones", skeleton->bones.size());
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Expandable bone hierarchy. Click a bone to select it in the viewport.");

                        // Recursive lambda to draw bone hierarchy
                        std::function<void(i32)> drawBoneTree = [&](i32 parentIdx) {
                            for (usize i = 0; i < skeleton->bones.size(); ++i) {
                                const auto& bone = skeleton->bones[i];
                                if (bone.parentIndex != parentIdx) continue;

                                // Check if this bone has children
                                bool hasChildren = false;
                                for (usize j = 0; j < skeleton->bones.size(); ++j) {
                                    if (skeleton->bones[j].parentIndex == static_cast<i32>(i)) {
                                        hasChildren = true;
                                        break;
                                    }
                                }

                                bool isSelected = (animComp->selectedBoneIndex == static_cast<i32>(i));
                                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                                if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
                                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

                                if (isSelected) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.35f, 1.0f));
                                bool nodeOpen = ImGui::TreeNodeEx(bone.name.c_str(), flags);
                                if (isSelected) ImGui::PopStyleColor();

                                // Click to select bone
                                if (ImGui::IsItemClicked()) {
                                    animComp->selectedBoneIndex = static_cast<i32>(i);
                                    if (!animComp->showBones) animComp->showBones = true;
                                }

                                if (nodeOpen) {
                                    if (hasChildren) drawBoneTree(static_cast<i32>(i));
                                    ImGui::TreePop();
                                }
                            }
                        };

                        drawBoneTree(-1); // Start from root bones (parentIndex == -1)
                        ImGui::TreePop();
                    }

                    // Movement drive: the animator switches clips from the
                    // entity's world velocity — idle standing still, walk/run by
                    // speed, jump while airborne. Auto-filled by the importer.
                    ImGui::Separator();
                    if (ImGui::TreeNodeEx("Movement##Animator", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& mova = animComp->movement;
                        ImGui::Checkbox("Play By Movement", &mova.enabled);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Automatically play these clips as the entity moves:\n"
                                              "idle when still, walk/run by speed, jump in the air.\n"
                                              "Leave a clip empty to skip that state.");
                        }

                        // Clip pickers: combo over the animator's clips + "(none)"
                        auto clipCombo = [&](const char* label, std::string& target) {
                            std::string preview = target.empty() ? "(none)" : target;
                            if (ImGui::BeginCombo(label, preview.c_str())) {
                                if (ImGui::Selectable("(none)", target.empty())) target.clear();
                                for (const auto& [animName, animClip] : animations) {
                                    (void)animClip;
                                    if (ImGui::Selectable(animName.c_str(), target == animName)) {
                                        target = animName;
                                        mova.currentState = 255;  // re-evaluate state with new clip
                                    }
                                }
                                ImGui::EndCombo();
                            }
                        };
                        clipCombo("Idle##MovA", mova.idleClip);
                        clipCombo("Walk##MovA", mova.walkClip);
                        clipCombo("Run##MovA", mova.runClip);
                        clipCombo("Jump##MovA", mova.jumpClip);

                        ImGui::DragFloat("Walk Speed##MovA", &mova.walkThreshold, 0.01f, 0.01f, 2.0f, "%.2f u/s");
                        ImGui::DragFloat("Run Speed##MovA", &mova.runThreshold, 0.1f, 0.5f, 20.0f, "%.1f u/s");
                        ImGui::DragFloat("Air Speed##MovA", &mova.jumpThreshold, 0.1f, 0.2f, 20.0f, "%.1f u/s");
                        ImGui::DragFloat("Fade Time##MovA", &mova.fadeTime, 0.01f, 0.0f, 1.0f, "%.2f s");
                        ImGui::TreePop();
                    }

                    // Debug visualization toggle
                    ImGui::Separator();
                    ImGui::Checkbox("Show Bones##Animator", &animComp->showBones);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Draw wireframe skeleton lines in the viewport. Click bones to select.");
                    }

                    // When showBones is off, clear bone selection
                    if (!animComp->showBones) {
                        animComp->selectedBoneIndex = -1;
                    }

                    // Bone search: filter + click to select
                    if (animComp->showBones) {
                        const auto* searchSkel = animator.GetSkeleton();
                        if (searchSkel && !searchSkel->bones.empty()) {
                            static char boneSearchBuf[128] = {};
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
                            ImGui::InputTextWithHint("##BoneSearch", "Search bones...", boneSearchBuf, sizeof(boneSearchBuf));
                            if (boneSearchBuf[0] != '\0') {
                                std::string filter(boneSearchBuf);
                                for (auto& c : filter) c = static_cast<char>(std::tolower(c));
                                i32 matchCount = 0;
                                for (usize bi = 0; bi < searchSkel->bones.size() && matchCount < 8; ++bi) {
                                    std::string boneLower = searchSkel->bones[bi].name;
                                    for (auto& c : boneLower) c = static_cast<char>(std::tolower(c));
                                    if (boneLower.find(filter) != std::string::npos) {
                                        bool isSel = (animComp->selectedBoneIndex == static_cast<i32>(bi));
                                        if (ImGui::Selectable(searchSkel->bones[bi].name.c_str(), isSel)) {
                                            animComp->selectedBoneIndex = static_cast<i32>(bi);
                                        }
                                        matchCount++;
                                    }
                                }
                                if (matchCount == 0) ImGui::TextDisabled("No matching bones");
                            }
                        }
                    }

                    // Accessible bone region picker — large buttons grouped by body part
                    if (animComp->showBones && skeleton) {
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "Rig Regions");
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click a body region to filter the bone list.\nDesigned for easy navigation of face, hand, and full-body rigs.");

                        // Classify all bones into regions
                        std::unordered_map<ECS::BoneRegion, std::vector<i32>> regionBones;
                        for (usize i = 0; i < skeleton->bones.size(); ++i) {
                            ECS::BoneRegion region = ECS::ClassifyBoneRegion(skeleton->bones[i].name);
                            regionBones[region].push_back(static_cast<i32>(i));
                        }

                        // Draw region buttons in a grid (accessible: large, high-contrast)
                        static ECS::BoneRegion selectedRegion = ECS::BoneRegion::Unknown;
                        f32 btnW = ImGui::GetContentRegionAvail().x * 0.48f;
                        f32 btnH = 28.0f;

                        ECS::BoneRegion regionOrder[] = {
                            ECS::BoneRegion::Face, ECS::BoneRegion::Head,
                            ECS::BoneRegion::LeftHand, ECS::BoneRegion::RightHand,
                            ECS::BoneRegion::LeftArm, ECS::BoneRegion::RightArm,
                            ECS::BoneRegion::Spine, ECS::BoneRegion::Other,
                            ECS::BoneRegion::LeftLeg, ECS::BoneRegion::RightLeg,
                            ECS::BoneRegion::LeftFoot, ECS::BoneRegion::RightFoot,
                        };

                        for (i32 ri = 0; ri < 12; ri += 2) {
                            for (i32 col = 0; col < 2; ++col) {
                                ECS::BoneRegion region = regionOrder[ri + col];
                                auto it = regionBones.find(region);
                                u32 count = (it != regionBones.end()) ? static_cast<u32>(it->second.size()) : 0;
                                if (count == 0) {
                                    // Disabled button for empty regions
                                    if (col > 0) ImGui::SameLine();
                                    ImGui::BeginDisabled();
                                    ImGui::Button(ECS::BoneRegionName(region), ImVec2(btnW, btnH));
                                    ImGui::EndDisabled();
                                    continue;
                                }

                                bool isActive = (selectedRegion == region);
                                if (isActive) {
                                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
                                }

                                char label[64];
                                snprintf(label, sizeof(label), "%s (%u)", ECS::BoneRegionName(region), count);
                                if (col > 0) ImGui::SameLine();
                                if (ImGui::Button(label, ImVec2(btnW, btnH))) {
                                    selectedRegion = isActive ? ECS::BoneRegion::Unknown : region;
                                }

                                if (isActive) ImGui::PopStyleColor(2);
                            }
                        }

                        // Show filtered bone list for selected region
                        if (selectedRegion != ECS::BoneRegion::Unknown) {
                            auto it = regionBones.find(selectedRegion);
                            if (it != regionBones.end() && !it->second.empty()) {
                                ImGui::Indent(4.0f);
                                for (i32 bi : it->second) {
                                    bool isSel = (animComp->selectedBoneIndex == bi);
                                    if (isSel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.35f, 1.0f));
                                    if (ImGui::Selectable(skeleton->bones[bi].name.c_str(), isSel)) {
                                        animComp->selectedBoneIndex = bi;
                                    }
                                    if (isSel) ImGui::PopStyleColor();
                                }
                                ImGui::Unindent(4.0f);
                            }
                        }
                    }

                    // Pose Library — save and recall named bone poses
                    if (skeleton && m_World->HasComponent<ECS::PoseLibraryComponent>(m_PrimarySelected)) {
                        auto* poseLib = m_World->GetComponent<ECS::PoseLibraryComponent>(m_PrimarySelected);
                        if (poseLib) {
                            ImGui::Separator();
                            ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.5f, 1.0f), "Pose Library");
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save and recall bone poses for expressions and gestures.\nClick a pose to preview it. Use 'Save Current' to capture the current skeleton state.");

                            ImGui::DragFloat("Blend##PoseLib", &poseLib->blendWeight, 0.05f, 0.0f, 1.0f, "%.2f");
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Master blend weight for pose application (0 = no effect, 1 = full pose)");

                            // Pose buttons — large, accessible, grouped by category
                            std::string lastCategory;
                            for (usize pi = 0; pi < poseLib->poses.size(); ++pi) {
                                auto& pose = poseLib->poses[pi];

                                // Category header
                                if (pose.category != lastCategory) {
                                    if (!pose.category.empty()) ImGui::TextDisabled("%s", pose.category.c_str());
                                    lastCategory = pose.category;
                                }

                                bool isActive = (poseLib->activePose == pose.name);
                                if (isActive) {
                                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.5f, 0.3f, 1.0f));
                                }

                                f32 poseBtnW = ImGui::GetContentRegionAvail().x * 0.7f;
                                if (ImGui::Button(pose.name.c_str(), ImVec2(poseBtnW, 26.0f))) {
                                    poseLib->activePose = isActive ? "" : pose.name;
                                }
                                if (ImGui::IsItemHovered() && !pose.overrides.empty()) {
                                    ImGui::SetTooltip("%zu bone overrides", pose.overrides.size());
                                }

                                // Delete button
                                ImGui::SameLine();
                                ImGui::PushID(static_cast<int>(pi));
                                if (ImGui::SmallButton("X##PoseDel")) {
                                    if (poseLib->activePose == pose.name) poseLib->activePose.clear();
                                    poseLib->poses.erase(poseLib->poses.begin() + pi);
                                    ImGui::PopID();
                                    if (isActive) ImGui::PopStyleColor(2);
                                    break;
                                }
                                ImGui::PopID();

                                if (isActive) ImGui::PopStyleColor(2);
                            }

                            // Save current pose button
                            ImGui::Spacing();
                            static char newPoseNameBuf[128] = {};
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
                            ImGui::InputTextWithHint("##NewPoseName", "Pose name...", newPoseNameBuf, sizeof(newPoseNameBuf));
                            ImGui::SameLine();
                            if (ImGui::Button("Save Current") && newPoseNameBuf[0] != '\0') {
                                ECS::PoseLibraryComponent::NamedPose newPose;
                                newPose.name = newPoseNameBuf;
                                // Auto-categorize based on selected region or bone
                                if (animComp->selectedBoneIndex >= 0) {
                                    ECS::BoneRegion region = ECS::ClassifyBoneRegion(
                                        skeleton->bones[animComp->selectedBoneIndex].name);
                                    newPose.category = ECS::BoneRegionName(region);
                                }
                                // Capture current bone rotations from pose
                                const auto& pose = animator.GetCurrentPose();
                                for (usize bi = 0; bi < skeleton->bones.size(); ++bi) {
                                    if (bi < pose.localRotations.size()) {
                                        ECS::PoseLibraryComponent::BoneOverride bo;
                                        bo.boneName = skeleton->bones[bi].name;
                                        bo.rotation = pose.localRotations[bi];
                                        newPose.overrides.push_back(bo);
                                    }
                                }
                                poseLib->poses.push_back(std::move(newPose));
                                newPoseNameBuf[0] = '\0';
                            }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Capture all bone rotations as a named pose");
                        }
                    }

                    // Selected bone info panel
                    if (animComp->showBones && animComp->selectedBoneIndex >= 0) {
                        const auto* selSkel = animator.GetSkeleton();
                        if (selSkel && animComp->selectedBoneIndex < static_cast<i32>(selSkel->bones.size())) {
                            const auto& selBone = selSkel->bones[animComp->selectedBoneIndex];
                            const auto& selPose = animator.GetCurrentPose();

                            ImGui::Indent(8.0f);
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.35f, 1.0f));
                            ImGui::Text("Selected Bone: %s [%d]", selBone.name.c_str(), animComp->selectedBoneIndex);
                            ImGui::PopStyleColor();

                            // Parent name
                            if (selBone.parentIndex >= 0 && selBone.parentIndex < static_cast<i32>(selSkel->bones.size())) {
                                ImGui::Text("Parent: %s [%d]", selSkel->bones[selBone.parentIndex].name.c_str(), selBone.parentIndex);
                            } else {
                                ImGui::Text("Parent: (root)");
                            }

                            // Local pose data (from current animation pose)
                            if (animComp->selectedBoneIndex < static_cast<i32>(selPose.localPositions.size())) {
                                const auto& lp = selPose.localPositions[animComp->selectedBoneIndex];
                                ImGui::Text("Local Pos: %.3f, %.3f, %.3f", lp.x, lp.y, lp.z);
                            }
                            if (animComp->selectedBoneIndex < static_cast<i32>(selPose.localRotations.size())) {
                                const auto& lr = selPose.localRotations[animComp->selectedBoneIndex];
                                ImGui::Text("Local Rot: %.3f, %.3f, %.3f, %.3f", lr.x, lr.y, lr.z, lr.w);
                            }
                            if (animComp->selectedBoneIndex < static_cast<i32>(selPose.localScales.size())) {
                                const auto& ls = selPose.localScales[animComp->selectedBoneIndex];
                                ImGui::Text("Local Scale: %.3f, %.3f, %.3f", ls.x, ls.y, ls.z);
                            }

                            // World transform matrix diagonal (scale + shear indicator)
                            if (animComp->selectedBoneIndex < static_cast<i32>(selPose.worldTransforms.size())) {
                                const auto& wt = selPose.worldTransforms[animComp->selectedBoneIndex];
                                ImGui::Text("World Diag: %.3f, %.3f, %.3f, %.3f", wt.m[0], wt.m[5], wt.m[10], wt.m[15]);
                            }

                            // Deselect button
                            if (ImGui::Button("Deselect##BoneDeselect")) {
                                animComp->selectedBoneIndex = -1;
                            }
                            ImGui::SameLine();
                            ImGui::TextDisabled("(or press Escape)");

                            ImGui::Unindent(8.0f);
                        }
                    }

                    // Bone weight visualization (heat map)
                    {
                        bool prevShowWeights = animComp->showWeights;
                        ImGui::Checkbox("Show Weights##Animator", &animComp->showWeights);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Overlay bone weight heat map on mesh.\nBlue = 0 influence, Green = 0.5, Red = 1.0.\nSelect a bone below to see its influence on each vertex.");
                        }

                        if (animComp->showWeights) {
                            // Bone selector dropdown
                            const auto* skel = animator.GetSkeleton();
                            if (skel && !skel->bones.empty()) {
                                // Build preview label
                                const char* previewLabel = "(select bone)";
                                if (animComp->weightPreviewBoneIndex >= 0 &&
                                    animComp->weightPreviewBoneIndex < static_cast<i32>(skel->bones.size())) {
                                    previewLabel = skel->bones[animComp->weightPreviewBoneIndex].name.c_str();
                                }

                                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                                if (ImGui::BeginCombo("##BoneWeightSelect", previewLabel)) {
                                    for (usize bi = 0; bi < skel->bones.size(); ++bi) {
                                        bool isSelected = (static_cast<i32>(bi) == animComp->weightPreviewBoneIndex);
                                        if (ImGui::Selectable(skel->bones[bi].name.c_str(), isSelected)) {
                                            animComp->weightPreviewBoneIndex = static_cast<i32>(bi);
                                            // Apply bone weight colors to mesh
                                            ApplyBoneWeightColors(m_PrimarySelected, animComp->weightPreviewBoneIndex);
                                        }
                                        if (isSelected) ImGui::SetItemDefaultFocus();
                                    }
                                    ImGui::EndCombo();
                                }

                                // Apply weight colors when toggled on (first frame or bone changed)
                                if (!prevShowWeights && animComp->weightPreviewBoneIndex >= 0) {
                                    ApplyBoneWeightColors(m_PrimarySelected, animComp->weightPreviewBoneIndex);
                                }
                            } else {
                                ImGui::TextDisabled("No skeleton available");
                            }
                        } else if (prevShowWeights) {
                            // Toggled off: restore original vertex colors
                            RestoreBoneWeightColors(m_PrimarySelected);
                        }
                    }

                    // 3D Skeletal Onion Skinning
                    ImGui::Separator();
                    auto& onionSkin = animComp->onionSkin;
                    ImGui::Checkbox("Onion Skin##Animator3D", &onionSkin.enabled);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Show semi-transparent ghost copies at nearby animation frames");
                    }
                    if (onionSkin.enabled) {
                        ImGui::Indent(8.0f);
                        ImGui::SliderInt("Before##OnionBefore", &onionSkin.framesBefore, 0, 10);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Number of ghost frames before current time");
                        ImGui::SliderInt("After##OnionAfter", &onionSkin.framesAfter, 0, 10);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Number of ghost frames after current time");
                        ImGui::SliderFloat("Opacity##Onion3D", &onionSkin.opacity, 0.05f, 1.0f, "%.2f");
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Base opacity of the nearest ghost (closest frame)");
                        ImGui::SliderFloat("Falloff##Onion3D", &onionSkin.opacityFalloff, 0.1f, 1.0f, "%.2f");
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Each step multiplies opacity by this value (lower = faster fade)");
                        ImGui::ColorEdit3("Before Tint##Onion3D", &onionSkin.beforeTint.x);
                        ImGui::ColorEdit3("After Tint##Onion3D", &onionSkin.afterTint.x);

                        // Visual opacity preview strip
                        {
                            ImVec2 stripPos = ImGui::GetCursorScreenPos();
                            f32 stripW = ImGui::GetContentRegionAvail().x;
                            f32 stripH = 16.0f;
                            f32 cellW = stripW / static_cast<f32>(onionSkin.framesBefore + 1 + onionSkin.framesAfter);
                            ImDrawList* sDL = ImGui::GetWindowDrawList();

                            // Background
                            sDL->AddRectFilled(stripPos, ImVec2(stripPos.x + stripW, stripPos.y + stripH),
                                IM_COL32(20, 20, 20, 255), 2.0f);

                            // Before ghosts (left)
                            for (i32 i = onionSkin.framesBefore; i >= 1; --i) {
                                f32 alpha = onionSkin.opacity;
                                for (i32 j = 1; j < i; ++j) alpha *= onionSkin.opacityFalloff;
                                i32 idx = onionSkin.framesBefore - i;
                                f32 cx = stripPos.x + idx * cellW;
                                u8 a = static_cast<u8>(Math::Clamp(alpha * 255.0f, 0.0f, 255.0f));
                                sDL->AddRectFilled(ImVec2(cx + 1, stripPos.y + 1), ImVec2(cx + cellW - 1, stripPos.y + stripH - 1),
                                    IM_COL32(static_cast<u8>(onionSkin.beforeTint.x * 255), static_cast<u8>(onionSkin.beforeTint.y * 255),
                                             static_cast<u8>(onionSkin.beforeTint.z * 255), a));
                            }

                            // Current frame (center, white)
                            f32 curX = stripPos.x + onionSkin.framesBefore * cellW;
                            sDL->AddRectFilled(ImVec2(curX + 1, stripPos.y + 1), ImVec2(curX + cellW - 1, stripPos.y + stripH - 1),
                                IM_COL32(255, 255, 255, 255));

                            // After ghosts (right)
                            for (i32 i = 1; i <= onionSkin.framesAfter; ++i) {
                                f32 alpha = onionSkin.opacity;
                                for (i32 j = 1; j < i; ++j) alpha *= onionSkin.opacityFalloff;
                                i32 idx = onionSkin.framesBefore + i;
                                f32 cx = stripPos.x + idx * cellW;
                                u8 a = static_cast<u8>(Math::Clamp(alpha * 255.0f, 0.0f, 255.0f));
                                sDL->AddRectFilled(ImVec2(cx + 1, stripPos.y + 1), ImVec2(cx + cellW - 1, stripPos.y + stripH - 1),
                                    IM_COL32(static_cast<u8>(onionSkin.afterTint.x * 255), static_cast<u8>(onionSkin.afterTint.y * 255),
                                             static_cast<u8>(onionSkin.afterTint.z * 255), a));
                            }

                            ImGui::Dummy(ImVec2(stripW, stripH));
                        }

                        ImGui::Unindent(8.0f);
                    }

                    // ================================================================
                    // Animation Retargeting
                    // ================================================================
                    ImGui::Separator();
                    if (!animations.empty() && ImGui::TreeNode("Retargeting")) {
                        // Persistent retarget state (static per-inspector, fine for single selection)
                        static Animation::AnimationRetargetMap s_RetargetMap;
                        static bool s_RetargetAutoMap = true;
                        static f32 s_RetargetHeightScale = 1.0f;
                        static std::string s_RetargetSourceAnim;
                        static std::vector<std::pair<std::string, std::string>> s_RetargetPreview;
                        static ECS::Entity s_RetargetTarget = ECS::INVALID_ENTITY;

                        ImGui::TextDisabled("Transfer animations between different skeletons");

                        // Target entity selector — list entities with AnimatorComponent + skeleton
                        {
                            const char* targetLabel = "(self)";
                            if (s_RetargetTarget != ECS::INVALID_ENTITY && m_World->IsValid(s_RetargetTarget)) {
                                auto* targetName = m_World->GetComponent<ECS::NameComponent>(s_RetargetTarget);
                                if (targetName) targetLabel = targetName->name.c_str();
                            }
                            if (ImGui::BeginCombo("Target Entity##Retarget", targetLabel)) {
                                // "(self)" option — retarget onto own skeleton
                                if (ImGui::Selectable("(self)", s_RetargetTarget == ECS::INVALID_ENTITY)) {
                                    s_RetargetTarget = ECS::INVALID_ENTITY;
                                    s_RetargetPreview.clear();
                                }
                                // List other entities with AnimatorComponent
                                const auto& animEntities = m_World->GetEntitiesWithComponent<ECS::AnimatorComponent>();
                                for (auto e : animEntities) {
                                    if (e == m_PrimarySelected) continue;
                                    auto* otherAnim = m_World->GetComponent<ECS::AnimatorComponent>(e);
                                    if (!otherAnim) continue;
                                    const auto* otherSkel = otherAnim->animator.GetSkeleton();
                                    if (!otherSkel || otherSkel->bones.empty()) continue;

                                    auto* eName = m_World->GetComponent<ECS::NameComponent>(e);
                                    std::string label = eName ? eName->name : ("Entity " + std::to_string(e));
                                    label += " (" + std::to_string(otherSkel->bones.size()) + " bones)";
                                    if (ImGui::Selectable(label.c_str(), s_RetargetTarget == e)) {
                                        s_RetargetTarget = e;
                                        s_RetargetPreview.clear();
                                    }
                                }
                                ImGui::EndCombo();
                            }
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("Select the entity whose skeleton to retarget onto");
                        }

                        ImGui::Checkbox("Auto-Map By Name", &s_RetargetAutoMap);
                        s_RetargetMap.autoMapByName = s_RetargetAutoMap;
                        ImGui::DragFloat("Height Scale##Retarget", &s_RetargetHeightScale, 0.01f, 0.01f, 10.0f, "%.2f");
                        s_RetargetMap.heightScale = s_RetargetHeightScale;

                        // Source animation selector
                        if (ImGui::BeginCombo("Source Animation##Retarget",
                                s_RetargetSourceAnim.empty() ? "(select)" : s_RetargetSourceAnim.c_str())) {
                            for (const auto& [animName, animData] : animations) {
                                (void)animData;
                                if (ImGui::Selectable(animName.c_str(), animName == s_RetargetSourceAnim)) {
                                    s_RetargetSourceAnim = animName;
                                }
                            }
                            ImGui::EndCombo();
                        }

                        // Resolve target skeleton
                        const auto* srcSkel = animator.GetSkeleton();
                        const Animation::Skeleton* tgtSkel = srcSkel; // default: retarget onto self
                        if (s_RetargetTarget != ECS::INVALID_ENTITY && m_World->IsValid(s_RetargetTarget)) {
                            auto* targetAnimComp = m_World->GetComponent<ECS::AnimatorComponent>(s_RetargetTarget);
                            if (targetAnimComp) {
                                const auto* resolved = targetAnimComp->animator.GetSkeleton();
                                if (resolved && !resolved->bones.empty()) tgtSkel = resolved;
                            }
                        }

                        // Build preview button
                        if (srcSkel && tgtSkel && !s_RetargetSourceAnim.empty()) {
                            if (ImGui::Button("Preview Mapping##Retarget")) {
                                Animation::AnimationRetargetMap autoMap = Animation::BuildAutoRetargetMap(*srcSkel, *tgtSkel);
                                // Merge explicit overrides
                                for (const auto& [src, tgt] : s_RetargetMap.boneMapping) {
                                    autoMap.boneMapping[src] = tgt;
                                }
                                s_RetargetPreview.clear();
                                for (const auto& bone : srcSkel->bones) {
                                    auto it = autoMap.boneMapping.find(bone.name);
                                    std::string target = (it != autoMap.boneMapping.end()) ? it->second : "(unmapped)";
                                    s_RetargetPreview.push_back({bone.name, target});
                                }
                            }
                        }

                        // Show mapping preview
                        if (!s_RetargetPreview.empty()) {
                            ImGui::Text("Bone Mapping Preview:");
                            ImGui::Indent(8.0f);
                            for (const auto& [src, tgt] : s_RetargetPreview) {
                                if (tgt == "(unmapped)") {
                                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "%s -> %s", src.c_str(), tgt.c_str());
                                } else {
                                    ImGui::Text("%s -> %s", src.c_str(), tgt.c_str());
                                }
                            }
                            ImGui::Unindent(8.0f);
                        }

                        // Manual mapping overrides
                        if (srcSkel && ImGui::TreeNode("Manual Overrides##Retarget")) {
                            static char s_OverrideSrc[128] = "";
                            static char s_OverrideTgt[128] = "";
                            ImGui::SetNextItemWidth(120.0f);
                            ImGui::InputText("Source##RetargetOvr", s_OverrideSrc, sizeof(s_OverrideSrc));
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(120.0f);
                            ImGui::InputText("Target##RetargetOvr", s_OverrideTgt, sizeof(s_OverrideTgt));
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Add##RetargetOvr")) {
                                if (s_OverrideSrc[0] && s_OverrideTgt[0]) {
                                    s_RetargetMap.boneMapping[s_OverrideSrc] = s_OverrideTgt;
                                }
                            }
                            // Show current overrides
                            for (const auto& [src, tgt] : s_RetargetMap.boneMapping) {
                                ImGui::BulletText("%s -> %s", src.c_str(), tgt.c_str());
                            }
                            ImGui::TreePop();
                        }

                        // Retarget buttons
                        if (srcSkel && tgtSkel && !s_RetargetSourceAnim.empty()) {
                            if (ImGui::Button("Retarget Animation##Retarget")) {
                                auto itSrc = animations.find(s_RetargetSourceAnim);
                                if (itSrc != animations.end()) {
                                    Animation::SkeletalAnimation retargeted = Animation::RetargetAnimation(
                                        itSrc->second, *srcSkel, *tgtSkel, s_RetargetMap
                                    );
                                    // Add to target entity's animator if targeting another entity
                                    if (s_RetargetTarget != ECS::INVALID_ENTITY && m_World->IsValid(s_RetargetTarget)) {
                                        auto* targetAnimComp = m_World->GetComponent<ECS::AnimatorComponent>(s_RetargetTarget);
                                        if (targetAnimComp) {
                                            targetAnimComp->animator.AddAnimation(retargeted);
                                            ENJIN_LOG_INFO(Animation, "Retargeted '%s' -> '%s' onto target entity",
                                                s_RetargetSourceAnim.c_str(), retargeted.name.c_str());
                                        }
                                    } else {
                                        animComp->animator.AddAnimation(retargeted);
                                        ENJIN_LOG_INFO(Animation, "Retargeted animation '%s' -> '%s'",
                                            s_RetargetSourceAnim.c_str(), retargeted.name.c_str());
                                    }
                                }
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("Retarget All##Retarget")) {
                                u32 count = 0;
                                for (const auto& [animName, animData] : animations) {
                                    Animation::SkeletalAnimation retargeted = Animation::RetargetAnimation(
                                        animData, *srcSkel, *tgtSkel, s_RetargetMap
                                    );
                                    if (s_RetargetTarget != ECS::INVALID_ENTITY && m_World->IsValid(s_RetargetTarget)) {
                                        auto* targetAnimComp = m_World->GetComponent<ECS::AnimatorComponent>(s_RetargetTarget);
                                        if (targetAnimComp) targetAnimComp->animator.AddAnimation(retargeted);
                                    } else {
                                        animComp->animator.AddAnimation(retargeted);
                                    }
                                    ++count;
                                }
                                ENJIN_LOG_INFO(Animation, "Retargeted %u animations", count);
                            }
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("Retarget all animations from this entity to the target");
                        }

                        ImGui::TreePop();
                    }
                }
            }
        }

        // Bone Attachment component
        if (m_World->HasComponent<ECS::BoneAttachmentComponent>(m_PrimarySelected)) {
            DrawBoneAttachmentComponent(m_PrimarySelected);
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
        if (m_World->HasComponent<ECS::CustomShaderComponent>(m_PrimarySelected)) {
            if (ImGui::CollapsingHeader("Custom Shader (graph)", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto* cs = m_World->GetComponent<ECS::CustomShaderComponent>(m_PrimarySelected);
                if (cs) {
                    ImGui::TextDisabled("%s - %zu B vert / %zu B frag%s", cs->graphLabel.c_str(),
                                        cs->vertexSource.size(), cs->fragmentSource.size(),
                                        cs->failed ? "  [COMPILE FAILED - see log]" : "");
                    if (ImGui::Button("Remove Custom Shader")) {
                        m_RenderSystem->ClearEntityCustomShader(m_PrimarySelected);
                        m_World->RemoveComponent<ECS::CustomShaderComponent>(m_PrimarySelected);
                    }
                    ImGui::SetItemTooltip("Revert to the standard material pipeline and stop persisting the graph shader");
                }
            }
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
        if (m_World->HasComponent<ECS::DynamicDifficultyComponent>(m_PrimarySelected)) {
            DrawDynamicDifficultyComponent(m_PrimarySelected);
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
        if (m_World->HasComponent<ECS::AnimationRecorderComponent>(m_PrimarySelected)) {
            DrawAnimationRecorderComponent(m_PrimarySelected);
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
            } else {
                // No filter: group by category so each header appears exactly once, in a
                // canonical order — the registry list order no longer has to be grouped.
                static const char* kCatOrder[] = {
                    "Rendering", "UI", "3D / Animation", "Character Controller", "Physics",
                    "Joints", "AI", "Gameplay", "Effects", "Audio", "2D Graphics",
                    "Scripting", "Networking", "Puzzle", "Scene", "Other"
                };
                auto rank = [](const char* c) -> int {
                    for (int i = 0; i < static_cast<int>(sizeof(kCatOrder) / sizeof(kCatOrder[0])); ++i)
                        if (std::strcmp(c, kCatOrder[i]) == 0) return i;
                    return 999; // unknown categories sink to the bottom
                };
                std::stable_sort(visible.begin(), visible.end(),
                    [&](const VisibleEntry& a, const VisibleEntry& b) {
                        return rank(a.entry->category) < rank(b.entry->category);
                    });
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
                            if (!UI::SectionHeader(currentCategory, ImGuiTreeNodeFlags_DefaultOpen)) {
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

        } // end front-panel branch (else of the wiring flip)
        ImGui::PopStyleVar(); // flip content alpha

        // VWS: fold whatever the inspector changed this frame into the active layer.
        // RecordEntityChanges is a no-op when nothing differs from the snapshot. The "before"
        // is the frozen pre-edit baseline, so the diff captures the whole edit cumulatively.
        if (vwsCapturing && vwsBeforeSnapshot && m_World->IsValid(m_PrimarySelected)) {
            m_LayerSystem.RecordEntityChanges(m_PrimarySelected, *vwsBeforeSnapshot);
        }

        // Generic property-edit undo: resolve the edit session. A session
        // spans from the first frame a widget goes active to the first quiet
        // frame after; the whole drag/typing burst becomes ONE undo step.
        if (propUndoEligible && m_PropUndoBaselineEntity == m_PrimarySelected &&
            m_World->IsValid(m_PrimarySelected)) {
            const bool anyActive = ImGui::IsAnyItemActive();
            if (anyActive) {
                m_PropUndoEditing = true;
            } else if (m_PropUndoEditing) {
                m_PropUndoEditing = false;
                // If another command landed during the session (Add/Remove
                // Component, a bespoke transform command), that command owns
                // the change — don't double-record it.
                if (m_UndoRedo.GetUndoCount() == m_PropUndoStackAtSessionStart) {
                    std::string after = Scene::SceneSerializer::SerializeEntityToString(
                        m_World, m_PrimarySelected, /*includeVertexData=*/false);
                    if (after != m_PropUndoBaseline) {
                        m_UndoRedo.Execute(std::make_unique<EntityEditCommand>(
                            m_World, m_PrimarySelected, m_PropUndoBaseline, std::move(after)));
                    }
                }
            }
        } else {
            m_PropUndoEditing = false;  // selection changed or ineligible: drop the session
        }
    } else {
        DrawEmptyState("< >", "No Entity Selected", "Select an entity in the Hierarchy to inspect it");
    }

    if (inspLocked) m_PrimarySelected = inspSavedPrimary; // restore real selection

    ImGui::PopItemWidth();
    ImGui::End();
}


void EditorLayer::DrawMultiSelectInspector() {
    ImGui::Text("%zu entities selected", m_SelectedEntities.size());
    ImGui::Separator();

    // List selected entity names
    if (UI::SectionHeader("Selected Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (UI::SectionHeader("Batch Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
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
            RecordLayerEditForSelection("transform");
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
            RecordLayerEditForSelection("transform");
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
            RecordLayerEditForSelection("transform");
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
           world->HasComponent<ECS::MeshColliderComponent>(entity) ||
           world->HasComponent<ECS::PolygonCollider2DComponent>(entity) ||
           world->HasComponent<ECS::TriggerZoneComponent>(entity);
}

enum class RecommendedCollider { Box, Sphere, Capsule };

struct ColliderRecommendation {
    RecommendedCollider shape = RecommendedCollider::Box;
    Math::Vector3 boxSize{1.0f, 1.0f, 1.0f};
    f32 sphereRadius = 0.5f;
    f32 capsuleRadius = 0.3f;
    f32 capsuleHeight = 1.8f;
    Math::Vector3 center{0.0f, 0.0f, 0.0f};
    bool isTrigger = false;
    std::string reason;
};

static const char* ColliderShapeName(RecommendedCollider shape) {
    switch (shape) {
    case RecommendedCollider::Box:     return "Box";
    case RecommendedCollider::Sphere:  return "Sphere";
    case RecommendedCollider::Capsule: return "Capsule";
    }
    return "Box";
}

static ColliderRecommendation ChooseColliderForEntity(ECS::World* world, ECS::Entity entity) {
    ColliderRecommendation rec;

    // 1. Sprite2D → Box (thin)
    if (world->HasComponent<ECS::Sprite2DComponent>(entity) ||
        world->HasComponent<ECS::AnimatedSprite2DComponent>(entity)) {
        rec.shape = RecommendedCollider::Box;
        rec.boxSize = Math::Vector3(1.0f, 1.0f, 0.1f);
        rec.reason = "Box collider for 2D sprite";
        return rec;
    }

    // 2. Character controller or AI → Capsule
    bool hasCharCtrl = world->HasComponent<ECS::ThirdPersonController>(entity) ||
                       world->HasComponent<ECS::FirstPersonController>(entity) ||
                       world->HasComponent<ECS::TopDown3DController>(entity) ||
                       world->HasComponent<ECS::TopDown2DController>(entity) ||
                       world->HasComponent<ECS::Platformer2DController>(entity) ||
                       world->HasComponent<ECS::AIControllerComponent>(entity);
    if (hasCharCtrl) {
        rec.shape = RecommendedCollider::Capsule;
        auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
        if (mesh && !mesh->aabbDirty) {
            auto ext = mesh->cachedAABBMax - mesh->cachedAABBMin;
            rec.capsuleRadius = std::max(ext.x, ext.z) * 0.5f;
            rec.capsuleHeight = ext.y;
            rec.center = (mesh->cachedAABBMin + mesh->cachedAABBMax) * 0.5f;
        } else {
            rec.capsuleRadius = 0.3f;
            rec.capsuleHeight = 1.8f;
        }
        rec.reason = "Capsule collider for character";
        return rec;
    }

    // 3. Damage → Sphere trigger
    if (world->HasComponent<ECS::DamageComponent>(entity)) {
        rec.shape = RecommendedCollider::Sphere;
        rec.isTrigger = true;
        auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
        if (mesh && !mesh->aabbDirty) {
            auto ext = mesh->cachedAABBMax - mesh->cachedAABBMin;
            rec.sphereRadius = std::max({ext.x, ext.y, ext.z}) * 0.5f;
        } else {
            rec.sphereRadius = 1.0f;
        }
        rec.reason = "Sphere trigger for damage area";
        return rec;
    }

    // 4. Mesh with valid AABB → best-fit from aspect ratio
    auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
    if (mesh && !mesh->aabbDirty) {
        auto ext = mesh->cachedAABBMax - mesh->cachedAABBMin;
        rec.center = (mesh->cachedAABBMin + mesh->cachedAABBMax) * 0.5f;
        f32 maxDim = std::max({ext.x, ext.y, ext.z});
        f32 minDim = std::min({ext.x, ext.y, ext.z});

        if (ext.y > ext.x * 1.5f && ext.y > ext.z * 1.5f) {
            rec.shape = RecommendedCollider::Capsule;
            rec.capsuleRadius = std::max(ext.x, ext.z) * 0.5f;
            rec.capsuleHeight = ext.y;
            rec.reason = "Capsule collider (tall mesh)";
        } else if (maxDim > 0.001f && minDim / maxDim > 0.7f) {
            rec.shape = RecommendedCollider::Sphere;
            rec.sphereRadius = maxDim * 0.5f;
            rec.reason = "Sphere collider (uniform mesh)";
        } else {
            rec.shape = RecommendedCollider::Box;
            rec.boxSize = ext;
            rec.reason = "Box collider (sized to mesh)";
        }
        return rec;
    }

    // 5. Fallback → Box (1,1,1)
    rec.shape = RecommendedCollider::Box;
    rec.boxSize = Math::Vector3(1.0f, 1.0f, 1.0f);
    rec.reason = "Box collider (default)";
    return rec;
}

static void ApplyColliderRecommendation(ECS::World* world, ECS::Entity entity, const ColliderRecommendation& rec) {
    switch (rec.shape) {
    case RecommendedCollider::Box: {
        auto& col = world->AddComponent<ECS::BoxColliderComponent>(entity);
        col.size = rec.boxSize;
        col.center = rec.center;
        col.isTrigger = rec.isTrigger;
        break;
    }
    case RecommendedCollider::Sphere: {
        auto& col = world->AddComponent<ECS::SphereColliderComponent>(entity);
        col.radius = rec.sphereRadius;
        col.center = rec.center;
        col.isTrigger = rec.isTrigger;
        break;
    }
    case RecommendedCollider::Capsule: {
        auto& col = world->AddComponent<ECS::CapsuleColliderComponent>(entity);
        col.radius = rec.capsuleRadius;
        col.height = rec.capsuleHeight;
        col.center = rec.center;
        col.isTrigger = rec.isTrigger;
        break;
    }
    }
}

// ============================================================================
// Creative Intelligence — Smart Suggestions (Cross-System Integration)
// ============================================================================


void EditorLayer::DrawSmartSuggestions(ECS::Entity entity) {
    if (!m_World) return;

    // Collect applicable suggestions
    struct Suggestion {
        std::string text;
        const char* buttonId;
        int ruleId;
        std::string tooltip;
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
    bool hasRigidbody = m_World->HasComponent<ECS::RigidbodyComponent>(entity);
    bool hasAnimator = m_World->HasComponent<ECS::AnimatorComponent>(entity);
    bool hasSkeleton = m_World->HasComponent<ECS::SkeletonComponent>(entity);
    bool hasNetId = m_World->HasComponent<ECS::NetworkIdentityComponent>(entity);
    bool hasNetTransform = m_World->HasComponent<ECS::NetworkTransformComponent>(entity);
    bool hasDestructible = m_World->HasComponent<ECS::DestructibleComponent>(entity);

    // Detect 2D context
    auto projectMode = m_SceneManager.GetProjectMode();
    bool is2D = (projectMode == Scene::ProjectMode::Mode2D);
    if (!is2D && (hasSprite ||
                  m_World->HasComponent<ECS::AnimatedSprite2DComponent>(entity) ||
                  m_World->HasComponent<ECS::TilemapComponent>(entity) ||
                  m_World->HasComponent<ECS::Camera2DBoundsComponent>(entity))) {
        is2D = true;
    }

    // Rule 1: Dialogue but no DialogueBox
    if (hasDialogue && !hasDialogueBox)
        suggestions.push_back({"Add Dialogue Box for UI display", "##SugDlgBox", 1, ""});

    // Rule 2: AIController but no Health
    if (hasAI && !hasHealth)
        suggestions.push_back({"Add Health for combat", "##SugHealth", 2, ""});

    // Rule 3: Health but no Collider — context-aware shape
    if (hasHealth && !hasCollider) {
        auto rec = ChooseColliderForEntity(m_World, entity);
        std::string txt = std::string("Add ") + ColliderShapeName(rec.shape) + " Collider for damage detection";
        suggestions.push_back({txt, "##SugCollider", 3, rec.reason});
    }

    // Rule 4: Controller but no Camera in scene
    if (hasController) {
        ECS::Entity cam = ECS::CameraManager::GetActiveCamera(m_World);
        if (cam == ECS::INVALID_ENTITY)
            suggestions.push_back({"Add camera for this controller", "##SugCamera", 4, ""});
    }

    // Rule 5: Mesh but no Material
    if (hasMesh && !hasMaterial)
        suggestions.push_back({"Add material for rendering", "##SugMat", 5, ""});

    // Rule 6: BehaviorTree but no AIController
    if (hasBT && !hasAI)
        suggestions.push_back({"Add AI Controller for movement", "##SugAI", 6, ""});

    // Rule 7: Sprite + Controller but no Camera2DBounds
    if (hasSprite && hasController && !hasCam2D)
        suggestions.push_back({"Add 2D Camera for follow", "##SugCam2D", 7, ""});

    // Rule 8: Damage but no Collider — always sphere trigger
    if (hasDamage && !hasCollider)
        suggestions.push_back({"Add Sphere Collider (trigger) for damage area", "##SugDmgCol", 8,
                               "Sphere trigger detects entities in damage radius"});

    // Rule 9: Rigidbody but no Collider
    if (hasRigidbody && !hasCollider) {
        auto rec = ChooseColliderForEntity(m_World, entity);
        std::string txt = std::string("Add ") + ColliderShapeName(rec.shape) + " Collider for physics simulation";
        suggestions.push_back({txt, "##SugRBCol", 9, "Rigidbody needs a collider to interact with the physics world"});
    }

    // Rule 10: Animator but no Skeleton
    if (hasAnimator && !hasSkeleton)
        suggestions.push_back({"Add Skeleton for bone animation", "##SugSkel", 10,
                               "Animator requires skeleton with bone data"});

    // Rule 11: NetworkIdentity but no NetworkTransform
    if (hasNetId && !hasNetTransform)
        suggestions.push_back({"Add NetworkTransform for sync", "##SugNetTx", 11,
                               "Networked entity needs transform sync for multiplayer"});

    // Rule 12: Health + no controller/AI + no Destructible
    if (hasHealth && !hasController && !hasAI && !hasDestructible)
        suggestions.push_back({"Add Destructible for break behavior", "##SugDestr", 12,
                               "Non-character health entity needs break/respawn behavior"});

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
        ImGui::TextWrapped("%s", s.text.c_str());
        if (!s.tooltip.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", s.tooltip.c_str());
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
            case 3: {
                auto rec = ChooseColliderForEntity(m_World, entity);
                ApplyColliderRecommendation(m_World, entity, rec);
                break;
            }
            case 4: {
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
            case 8: {
                auto rec = ChooseColliderForEntity(m_World, entity);
                rec.shape = RecommendedCollider::Sphere;
                rec.isTrigger = true;
                if (rec.sphereRadius < 0.5f) rec.sphereRadius = 1.0f;
                ApplyColliderRecommendation(m_World, entity, rec);
                break;
            }
            case 9: {
                auto rec = ChooseColliderForEntity(m_World, entity);
                ApplyColliderRecommendation(m_World, entity, rec);
                break;
            }
            case 10: m_World->AddComponent<ECS::SkeletonComponent>(entity); break;
            case 11: m_World->AddComponent<ECS::NetworkTransformComponent>(entity); break;
            case 12: {
                auto& d = m_World->AddComponent<ECS::DestructibleComponent>(entity);
                d.health = 1.0f;
                d.destroyOnHit = true;
                break;
            }
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

    bool hasHealth = m_World->HasComponent<ECS::HealthComponent>(entity);
    bool hasCollider = EntityHasAnyCollider(m_World, entity);
    bool hasRigidbody = m_World->HasComponent<ECS::RigidbodyComponent>(entity);
    bool hasDestructible = m_World->HasComponent<ECS::DestructibleComponent>(entity);
    bool hasMesh = m_World->HasComponent<ECS::MeshComponent>(entity);

    // Count how many buttons we'd show
    int buttonCount = 0;
    if (!hasController) buttonCount++;
    if (is2D && !hasPlatformer && !hasController) buttonCount++;
    if ((hasAI || hasBT) && !hasHealth) buttonCount++;
    if (!hasDialogue && !hasController) buttonCount++;
    if (!hasPickup && !hasController) buttonCount++;
    if (!hasDestructible && (hasMesh || hasHealth)) buttonCount++;
    if (!hasRigidbody && hasMesh && !hasController) buttonCount++;

    if (buttonCount == 0) return;

    ImGui::Spacing();
    if (UI::SectionHeader("Quick Setup")) {
        f32 buttonWidth = ImGui::GetContentRegionAvail().x;
        (void)buttonWidth;

        // Recipe card: title + one-line description + chips listing exactly what it
        // adds + an Apply button, wrapped in a soft border. Returns true when Apply is
        // clicked. Recognition-over-recall + visible outcome beats a flat button list.
        auto quickCard = [&](const char* title, const char* desc,
                             std::initializer_list<const char*> chips) -> bool {
            ImGui::PushID(title);
            const f32 w = ImGui::GetContentRegionAvail().x;
            const ImVec2 startPos = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImGui::BeginGroup();
            ImGui::Dummy(ImVec2(2.0f, 1.0f));
            ImGui::TextUnformatted(title);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.66f, 0.74f, 1.0f));
            ImGui::TextWrapped("%s", desc);
            ImGui::PopStyleColor();
            bool first = true;
            for (const char* c : chips) {
                if (!first) ImGui::SameLine();
                first = false;
                const ImVec2 sz = ImGui::CalcTextSize(c);
                const ImVec2 p = ImGui::GetCursorScreenPos();
                dl->AddRectFilled(p, ImVec2(p.x + sz.x + 10.0f, p.y + sz.y + 4.0f),
                                  IM_COL32(70, 90, 130, 255), 4.0f);
                dl->AddText(ImVec2(p.x + 5.0f, p.y + 2.0f), IM_COL32(230, 235, 245, 255), c);
                ImGui::Dummy(ImVec2(sz.x + 10.0f, sz.y + 4.0f));
            }
            const bool clicked = ImGui::SmallButton("Apply");
            ImGui::Dummy(ImVec2(2.0f, 1.0f));
            ImGui::EndGroup();
            ImVec2 mn = ImGui::GetItemRectMin();
            ImVec2 mx = ImGui::GetItemRectMax();
            mx.x = startPos.x + w;
            const bool hov = ImGui::IsMouseHoveringRect(mn, mx, false);
            dl->AddRect(ImVec2(mn.x - 6.0f, mn.y - 4.0f), ImVec2(mx.x, mx.y + 4.0f),
                        hov ? IM_COL32(120, 160, 220, 220) : IM_COL32(74, 78, 90, 160),
                        6.0f, 0, hov ? 2.0f : 1.0f);
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImGui::PopID();
            return clicked;
        };

        // Pattern 1: Add Basic Movement
        if (!hasController) {
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Adds controller + collider + camera");
            if (quickCard("Basic Movement", "Walk, jump, and a follow camera.",
                          {"Controller", "Collider", "Camera"})) {
                if (is2D) {
                    m_World->AddComponent<ECS::Platformer2DController>(entity);
                    if (!hasCollider) m_World->AddComponent<ECS::BoxColliderComponent>(entity);
                    SetupCameraForController(entity, "Platformer2D");
                } else {
                    m_World->AddComponent<ECS::ThirdPersonController>(entity);
                    if (!hasCollider) {
                        auto& cap = m_World->AddComponent<ECS::CapsuleColliderComponent>(entity);
                        cap.radius = 0.3f;
                        cap.height = 1.8f;
                    }
                    SetupCameraForController(entity, "ThirdPerson");
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(is2D ?
                "Adds Platformer2D controller + BoxCollider + camera" :
                "Adds ThirdPerson controller + CapsuleCollider + camera");
        }

        // Pattern 2: Setup 2D Platformer (only in 2D mode)
        if (is2D && !hasPlatformer && !hasController) {
            if (quickCard("2D Platformer", "Side-scroller movement with a follow camera.",
                          {"Platformer2D", "Collider", "Sprite", "Camera2D"})) {
                m_World->AddComponent<ECS::Platformer2DController>(entity);
                if (!hasCollider) m_World->AddComponent<ECS::BoxColliderComponent>(entity);
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
        if ((hasAI || hasBT) && !hasHealth) {
            if (quickCard("Patroller Enemy", "Patrols an area and can take damage.",
                          {"AI", "Health", "Behavior Tree", "Collider"})) {
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
                if (!hasCollider) {
                    if (is2D) {
                        m_World->AddComponent<ECS::BoxColliderComponent>(entity);
                    } else {
                        auto rec = ChooseColliderForEntity(m_World, entity);
                        rec.shape = RecommendedCollider::Capsule;
                        if (rec.capsuleRadius < 0.1f) rec.capsuleRadius = 0.3f;
                        if (rec.capsuleHeight < 0.1f) rec.capsuleHeight = 1.8f;
                        ApplyColliderRecommendation(m_World, entity, rec);
                    }
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(is2D ?
                "Adds AI Controller (Patrol) + Health (50) + BT + BoxCollider, auto-targets Player" :
                "Adds AI Controller (Patrol) + Health (50) + BT + CapsuleCollider, auto-targets Player");
        }

        // Pattern 4: Setup NPC
        if (!hasDialogue && !hasController) {
            if (quickCard("Talkable NPC", "Walk up and interact to start a conversation.",
                          {"Dialogue", "Dialogue Box", "Interactable", "Trigger"})) {
                auto& dlg = m_World->AddComponent<ECS::DialogueComponent>(entity);
                auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
                if (nameComp && !nameComp->name.empty())
                    dlg.speakerName = nameComp->name;
                if (!m_World->HasComponent<ECS::DialogueBoxComponent>(entity))
                    m_World->AddComponent<ECS::DialogueBoxComponent>(entity);
                if (!m_World->HasComponent<ECS::InteractableComponent>(entity)) {
                    auto& interact = m_World->AddComponent<ECS::InteractableComponent>(entity);
                    interact.promptText = "Talk";
                }
                if (!hasCollider) {
                    if (is2D) {
                        auto& box = m_World->AddComponent<ECS::BoxColliderComponent>(entity);
                        box.size = Math::Vector3(2.0f, 2.0f, 0.1f);
                        box.isTrigger = true;
                    } else {
                        auto& sph = m_World->AddComponent<ECS::SphereColliderComponent>(entity);
                        sph.radius = 1.5f;
                        sph.isTrigger = true;
                    }
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(is2D ?
                "Adds Dialogue + DialogueBox + Interactable + BoxTrigger for interaction" :
                "Adds Dialogue + DialogueBox + Interactable + SphereTrigger (r=1.5) for interaction");
        }

        // Pattern 5: Make Collectible
        if (!hasPickup && !hasController) {
            if (quickCard("Collectible", "A bobbing pickup collected on touch.",
                          {"Pickup", "Trigger", "Tween"})) {
                auto& pickup = m_World->AddComponent<ECS::PickupComponent>(entity);
                pickup.type = ECS::PickupComponent::PickupType::Coin;
                pickup.value = 1.0f;
                pickup.destroyOnPickup = true;
                pickup.bobSpeed = 2.0f;
                pickup.bobHeight = 0.2f;
                if (!m_World->HasComponent<ECS::TriggerZoneComponent>(entity)) {
                    auto& trigger = m_World->AddComponent<ECS::TriggerZoneComponent>(entity);
                    trigger.shape = ECS::TriggerZoneComponent::Shape::Sphere;
                    trigger.sphereRadius = 1.0f;
                }
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

        // Pattern 6: Setup Destructible
        if (!hasDestructible && (hasMesh || hasHealth)) {
            if (quickCard("Destructible", "Takes damage and breaks apart.",
                          {"Health", "Destructible", "Collider"})) {
                if (!hasHealth) {
                    auto& hp = m_World->AddComponent<ECS::HealthComponent>(entity);
                    hp.maxHealth = 25.0f;
                    hp.currentHealth = 25.0f;
                }
                auto& d = m_World->AddComponent<ECS::DestructibleComponent>(entity);
                d.health = 25.0f;
                d.destroyOnHit = true;
                if (!hasCollider) {
                    auto rec = ChooseColliderForEntity(m_World, entity);
                    ApplyColliderRecommendation(m_World, entity, rec);
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Adds Health (25) + Destructible + best-fit collider from mesh");
        }

        // Pattern 7: Add Physics Object
        if (!hasRigidbody && hasMesh && !hasController) {
            if (quickCard("Physics Object", "Falls and collides under gravity.",
                          {"Rigidbody", "Collider"})) {
                auto& rb = m_World->AddComponent<ECS::RigidbodyComponent>(entity);
                rb.bodyType = ECS::RigidbodyComponent::BodyType::Dynamic;
                rb.useGravity = true;
                if (!hasCollider) {
                    auto rec = ChooseColliderForEntity(m_World, entity);
                    ApplyColliderRecommendation(m_World, entity, rec);
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Adds Rigidbody (dynamic, gravity) + best-fit collider from mesh bounds");
        }
    }
}


} // namespace Editor
} // namespace Enjin
