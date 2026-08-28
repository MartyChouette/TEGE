#include "Enjin/Editor/EditorLayer.h"
#ifndef _WIN32
// POSIX environment for posix_spawn. Declared at GLOBAL scope: a block-scope
// extern inside namespace Enjin mangles as a namespaced symbol under GCC.
extern char** environ;
#endif
#include "Enjin/Editor/EditorWidgets.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/VirtualCamera.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
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
#include "Enjin/Renderer/ReflectionProbeSystem.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/ECS/Components/ArtStyle.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/CineComponent.h"
#include "Enjin/ECS/Components/Elemental.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/BoneAttachment.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/Editor/SpriteSheetImporter.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/ECS/Components/ParallaxLayer.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Animation/RagdollSystem.h"
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
#include "Enjin/ECS/Components/GaussianSplat.h"
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
#include <stb_image_write.h>   // atlas packer output (impl in the shared stb TU)
#include <imgui.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include "Enjin/Editor/ComponentHelp.h"
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

// S19/S20/S23: Shell-escape a string for safe interpolation into shell commands (Unix only).
// Wraps the string in single quotes and escapes any embedded single quotes.
#ifndef _WIN32
static std::string ShellEscape(const std::string& s) {
    std::string result = "'";
    for (char c : s) {
        if (c == '\'') result += "'\\''";
        else result += c;
    }
    result += "'";
    return result;
}
#endif

// --- Undo-aware component removal helper ---
static const char* GetComponentIcon(const char* componentName) {
    // Map component names to bracket icons
    struct IconMap { const char* name; const char* icon; };
    static const IconMap icons[] = {
        {"Transform", "[+] "},
        {"Mesh", "[M] "},
        {"Material", "[*] "},
        {"Light", "[L] "},
        {"Camera", "[C] "},
        {"Sprite 2D", "[S] "},
        {"Tilemap", "[T] "},
        {"Particle Emitter", "[P] "},
        {"Audio Source", "[A] "},
        {"Rigidbody", "[R] "},
        {"Box Collider", "[B] "},
        {"Sphere Collider", "[O] "},
        {"Capsule Collider", "[I] "},
        {"Dialogue", "[D] "},
        {"Dialogue Box", "[D] "},
        {"Visual Script", "[V] "},
        {"UI Canvas", "[U] "},
        {"Terrain", "[~] "},
        {"Animator", "[>] "},
        {"Health", "[H] "},
        {"AI Controller", "[AI]"},
        {"Behavior Tree", "[BT]"},
        {"LOD", "[#] "},
        {"Skybox", "[=] "},
    };
    for (const auto& m : icons) {
        if (std::strcmp(componentName, m.name) == 0) return m.icon;
    }
    return "";
}


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

void EditorLayer::DrawTransformComponent(ECS::Entity entity) {
    std::string hdr = std::string(GetComponentIcon("Transform")) + "Transform";
    if (UI::SectionHeader(hdr.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ECS::TransformComponent* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) return;
        DrawComponentHelp("transform", m_World, entity);

        // Runtime-driven note: several systems overwrite the transform every
        // frame in Play, so the values here are a STARTING pose, not the live
        // truth. Name the drivers so "I set it but it ignores me" has an answer.
        {
            std::vector<const char*> drivers;
            if (auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity)) {
                if (rb->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic)
                    drivers.push_back("Physics (dynamic rigidbody)");
            }
            if (m_World->HasComponent<ECS::Platformer2DController>(entity) ||
                m_World->HasComponent<ECS::TopDown2DController>(entity) ||
                m_World->HasComponent<ECS::TopDown3DController>(entity) ||
                m_World->HasComponent<ECS::ThirdPersonController>(entity) ||
                m_World->HasComponent<ECS::FirstPersonController>(entity)) {
                drivers.push_back("Character Controller");
            }
            if (auto* tw = m_World->GetComponent<ECS::TweenComponent>(entity)) {
                if (!tw->tweens.empty()) drivers.push_back("Tween");
            }
            if (m_World->HasComponent<ECS::ParallaxLayerComponent>(entity))
                drivers.push_back("Parallax Layer");
            if (m_World->HasComponent<ECS::VirtualCameraComponent>(entity))
                drivers.push_back("Camera Director (when this vcam is live)");

            if (!drivers.empty()) {
                std::string list;
                for (usize i = 0; i < drivers.size(); ++i) {
                    if (i) list += ", ";
                    list += drivers[i];
                }
                bool playing = !m_PlayMode.IsStopped();
                ImGui::PushStyleColor(ImGuiCol_Text,
                    playing ? ImVec4(1.0f, 0.70f, 0.20f, 1.0f) : ImVec4(0.55f, 0.65f, 0.85f, 1.0f));
                ImGui::TextWrapped(playing ? "(!) Driven now by: %s" : "(i) Driven in Play by: %s",
                                   list.c_str());
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(playing
                        ? "These systems are overwriting the transform every frame right now.\n"
                          "Edits here will be stomped immediately."
                        : "The values below are the STARTING pose. In Play mode these systems\n"
                          "take over and overwrite the transform every frame.");
                }
            }
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Visible", &transform->visible);
        ImGui::SetItemTooltip("Toggle entity visibility in the scene");

        // Position
        f32 pos[3] = { transform->position.x, transform->position.y, transform->position.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Position", pos,
                [transform](f32 x, f32 y, f32 z) { transform->position = Math::Vector3(x, y, z); },
                0.1f)) {
            transform->position = Math::Vector3(pos[0], pos[1], pos[2]);
        }
        ImGui::SetItemTooltip("World position (X, Y, Z)");

        // Rotation (euler degrees). Re-extract from the quaternion ONLY when the
        // selection changed or something else moved the rotation (gizmo, script,
        // controller) — detected by comparing the live quaternion to the one the
        // cached euler would produce. While the user drags, we keep editing the
        // cached euler so crossing +/-90 doesn't gimbal-lock.
        {
            bool resync = (m_RotEulerEntity != m_PrimarySelected);
            if (!resync) {
                Math::Quaternion fromCache = Math::Quaternion::FromEuler(Math::Vector3(
                    Math::Radians(m_RotEulerCacheDeg.x),
                    Math::Radians(m_RotEulerCacheDeg.y),
                    Math::Radians(m_RotEulerCacheDeg.z)));
                // Quaternion double-cover: q and -q are the same rotation.
                f32 dot = fromCache.x * transform->rotation.x + fromCache.y * transform->rotation.y +
                          fromCache.z * transform->rotation.z + fromCache.w * transform->rotation.w;
                if (Math::Abs(dot) < 0.99995f) resync = true;   // changed elsewhere
            }
            if (resync) {
                m_RotEulerEntity = m_PrimarySelected;
                m_RotEulerCacheDeg = transform->rotation.ToEulerDegrees();
            }
        }
        f32 rot[3] = { m_RotEulerCacheDeg.x, m_RotEulerCacheDeg.y, m_RotEulerCacheDeg.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Rotation", rot,
                [this, transform](f32 x, f32 y, f32 z) {
                    m_RotEulerCacheDeg = Math::Vector3(x, y, z);
                    transform->rotation = Math::Quaternion::FromEuler(
                        Math::Vector3(Math::Radians(x), Math::Radians(y), Math::Radians(z)));
                }, 1.0f)) {
            m_RotEulerCacheDeg = Math::Vector3(rot[0], rot[1], rot[2]);
            transform->rotation = Math::Quaternion::FromEuler(
                Math::Vector3(Math::Radians(rot[0]), Math::Radians(rot[1]), Math::Radians(rot[2])));
        }
        ImGui::SetItemTooltip("Euler rotation in degrees (X, Y, Z)");

        // Scale
        f32 scale[3] = { transform->scale.x, transform->scale.y, transform->scale.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Scale", scale,
                [transform](f32 x, f32 y, f32 z) { transform->scale = Math::Vector3(x, y, z); },
                0.1f, 0.001f, 1000.0f)) {
            transform->scale = Math::Vector3(scale[0], scale[1], scale[2]);
        }
        ImGui::SetItemTooltip("Scale multiplier per axis");
    }
}

void EditorLayer::DrawMeshComponent(ECS::Entity entity) {
    bool meshOpen = UI::SectionHeader("[M] Mesh", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("MeshCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::MeshComponent>(entity, "mesh", "Mesh");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (meshOpen) {
        ECS::MeshComponent* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
        if (!mesh) return;
        DrawComponentHelp("mesh", m_World, entity);

        ImGui::Text("Vertices: %zu", mesh->vertices.size());
        ImGui::Text("Indices: %zu", mesh->indices.size());

        // Mesh Info read-only panel
        if (mesh->IsValid() && ImGui::TreeNode("Mesh Info")) {
            usize vertexCount = mesh->vertices.size();
            usize indexCount = mesh->indices.size();
            usize triangleCount = indexCount / 3;

            ImGui::Text("Vertex Count: %zu", vertexCount);
            ImGui::Text("Triangle Count: %zu", triangleCount);
            ImGui::Text("Index Count: %zu", indexCount);

            // Compute bounding box from vertices (use cached AABB if valid)
            Math::Vector3 bboxMin, bboxMax;
            if (mesh->aabbDirty) {
                bboxMin = Math::Vector3( 1e30f,  1e30f,  1e30f);
                bboxMax = Math::Vector3(-1e30f, -1e30f, -1e30f);
                for (const auto& v : mesh->vertices) {
                    if (v.position.x < bboxMin.x) bboxMin.x = v.position.x;
                    if (v.position.y < bboxMin.y) bboxMin.y = v.position.y;
                    if (v.position.z < bboxMin.z) bboxMin.z = v.position.z;
                    if (v.position.x > bboxMax.x) bboxMax.x = v.position.x;
                    if (v.position.y > bboxMax.y) bboxMax.y = v.position.y;
                    if (v.position.z > bboxMax.z) bboxMax.z = v.position.z;
                }
                mesh->cachedAABBMin = bboxMin;
                mesh->cachedAABBMax = bboxMax;
                mesh->aabbDirty = false;
            } else {
                bboxMin = mesh->cachedAABBMin;
                bboxMax = mesh->cachedAABBMax;
            }
            Math::Vector3 dims = bboxMax - bboxMin;
            ImGui::Text("Bounds: %.2f x %.2f x %.2f", dims.x, dims.y, dims.z);
            ImGui::Text("  Min: (%.3f, %.3f, %.3f)", bboxMin.x, bboxMin.y, bboxMin.z);
            ImGui::Text("  Max: (%.3f, %.3f, %.3f)", bboxMax.x, bboxMax.y, bboxMax.z);

            // Check for bone weights
            bool hasBones = false;
            u32 maxBoneIndex = 0;
            for (const auto& v : mesh->vertices) {
                f32 totalWeight = v.boneWeights.x + v.boneWeights.y + v.boneWeights.z + v.boneWeights.w;
                if (totalWeight > 0.001f) {
                    hasBones = true;
                    for (int bi = 0; bi < 4; ++bi) {
                        if (v.boneIndices[bi] > maxBoneIndex) {
                            maxBoneIndex = v.boneIndices[bi];
                        }
                    }
                }
            }
            if (hasBones) {
                ImGui::Text("Bone Weights: Yes (%u bones referenced)", maxBoneIndex + 1);
            } else {
                ImGui::TextDisabled("Bone Weights: No");
            }

            // UV channels — check if any vertex has non-zero UVs
            bool hasUV = false;
            for (const auto& v : mesh->vertices) {
                if (v.uv.x != 0.0f || v.uv.y != 0.0f) {
                    hasUV = true;
                    break;
                }
            }
            ImGui::Text("UV Channel: %s", hasUV ? "Yes" : "No");

            // Memory estimate
            usize vertexMemory = vertexCount * sizeof(ECS::MeshComponent::Vertex);
            usize indexMemory = indexCount * sizeof(u32);
            usize totalMemory = vertexMemory + indexMemory;
            if (totalMemory >= 1024 * 1024) {
                ImGui::Text("Memory: %.2f MB (verts: %.2f MB, idx: %.2f MB)",
                    static_cast<f32>(totalMemory) / (1024.0f * 1024.0f),
                    static_cast<f32>(vertexMemory) / (1024.0f * 1024.0f),
                    static_cast<f32>(indexMemory) / (1024.0f * 1024.0f));
            } else {
                ImGui::Text("Memory: %.1f KB (verts: %.1f KB, idx: %.1f KB)",
                    static_cast<f32>(totalMemory) / 1024.0f,
                    static_cast<f32>(vertexMemory) / 1024.0f,
                    static_cast<f32>(indexMemory) / 1024.0f);
            }

            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawLODComponent(ECS::Entity entity) {
    bool lodOpen = UI::SectionHeader("LOD", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("LODCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::LODComponent>(entity, "lod", "LOD");
            ImGui::EndPopup();
            return;
        }
        if (ImGui::MenuItem("Regenerate LODs")) {
            auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
            auto* lod = m_World->GetComponent<ECS::LODComponent>(entity);
            if (mesh && lod && mesh->IsValid()) {
                Renderer::MeshSimplifier::GenerateLODs(*mesh, *lod);
            }
        }
        ImGui::EndPopup();
    }
    if (lodOpen) {
        ECS::LODComponent* lod = m_World->GetComponent<ECS::LODComponent>(entity);
        if (!lod) return;
        DrawComponentHelp("lod", m_World, entity);

        if (!m_World->HasComponent<ECS::MeshComponent>(entity)) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "No MeshComponent — LOD requires a mesh to simplify");
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &lod->enabled);
        ImGui::Text("Levels: %d | Active: LOD %d", lod->levelCount, lod->activeLOD);

        if (InspectorUndo::SliderFloat(m_UndoRedo, "Base Distance", &lod->baseDistance, 2.0f, 100.0f, "%.1f")) {
            // Recompute distance thresholds
            for (int i = 0; i < lod->levelCount; ++i) {
                lod->levels[i].maxDistance = lod->baseDistance * std::pow(lod->distanceMultiplier, static_cast<f32>(i));
            }
        }
        if (InspectorUndo::SliderFloat(m_UndoRedo, "Distance Mult", &lod->distanceMultiplier, 1.2f, 5.0f, "%.1f")) {
            for (int i = 0; i < lod->levelCount; ++i) {
                lod->levels[i].maxDistance = lod->baseDistance * std::pow(lod->distanceMultiplier, static_cast<f32>(i));
            }
        }

        ImGui::Separator();
        for (int i = 0; i < lod->levelCount; ++i) {
            auto& level = lod->levels[i];
            bool isActive = (i == lod->activeLOD);

            if (isActive) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.4f, 1.0f));
            }

            ImGui::Text("LOD %d: %u verts, %u tris (%.0f%%)  dist < %.1f%s",
                i, level.vertexCount, level.triangleCount,
                level.reductionRatio * 100.0f, level.maxDistance,
                isActive ? "  [ACTIVE]" : "");

            if (isActive) {
                ImGui::PopStyleColor();
            }

            // Reduction ratio slider for regeneration
            char label[32];
            snprintf(label, sizeof(label), "Ratio##lod%d", i);
            if (i > 0) {
                ImGui::SameLine();
                ImGui::PushItemWidth(80);
                InspectorUndo::SliderFloat(m_UndoRedo, label, &lod->reductionRatios[i], 0.01f, 0.99f, "%.2f");
                ImGui::PopItemWidth();
            }
        }
    }
}

void EditorLayer::DrawMaterialComponent(ECS::Entity entity) {
    bool matOpen = UI::SectionHeader("[*] Material", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("MaterialCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::MaterialComponent>(entity, "material", "Material");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (matOpen) {
        ECS::MaterialComponent* material = m_World->GetComponent<ECS::MaterialComponent>(entity);
        if (!material) return;
        DrawComponentHelp("material", m_World, entity);

        // Base color
        f32 baseColor[3] = { material->baseColor.x, material->baseColor.y, material->baseColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Base Color", baseColor,
                [material](f32 r, f32 g, f32 b) { material->baseColor = Math::Vector3(r, g, b); })) {
            material->baseColor = Math::Vector3(baseColor[0], baseColor[1], baseColor[2]);
        }
        ImGui::SetItemTooltip("Surface color when no texture is applied");

        // Opacity
        InspectorUndo::DragFloat(m_UndoRedo, "Opacity", &material->opacity, 0.01f, 0.0f, 1.0f);
        ImGui::SetItemTooltip("Overall transparency (0 = invisible, 1 = fully opaque)");

        // PBR properties
        InspectorUndo::DragFloat(m_UndoRedo, "Metallic", &material->metallic, 0.01f, 0.0f, 1.0f);
        ImGui::SetItemTooltip("Metallic factor (0 = dielectric/plastic, 1 = metal)");
        InspectorUndo::DragFloat(m_UndoRedo, "Roughness", &material->roughness, 0.01f, 0.0f, 1.0f);
        ImGui::SetItemTooltip("Surface roughness (0 = mirror smooth, 1 = fully rough)");

        // Emission
        f32 emissive[3] = { material->emissiveColor.x, material->emissiveColor.y, material->emissiveColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Emissive Color", emissive,
                [material](f32 r, f32 g, f32 b) { material->emissiveColor = Math::Vector3(r, g, b); })) {
            material->emissiveColor = Math::Vector3(emissive[0], emissive[1], emissive[2]);
        }
        ImGui::SetItemTooltip("Self-illumination color (adds glow independent of lighting)");
        InspectorUndo::DragFloat(m_UndoRedo, "Emissive Strength", &material->emissiveStrength, 0.1f, 0.0f, 100.0f);
        ImGui::SetItemTooltip("Emissive intensity multiplier");

        // Rendering options
        InspectorUndo::Checkbox(m_UndoRedo, "Double Sided", &material->doubleSided);
        ImGui::SetItemTooltip("Render both front and back faces");
        InspectorUndo::Checkbox(m_UndoRedo, "Cast Shadows", &material->castShadows);
        ImGui::SetItemTooltip("Whether this object casts shadows");
        InspectorUndo::Checkbox(m_UndoRedo, "Receive Shadows", &material->receiveShadows);
        ImGui::SetItemTooltip("Whether shadows are drawn on this surface");

        // Shadow dither mode
        const char* shadowDitherModes[] = { "None", "By Darkness", "By Distance", "By Angle" };
        int currentDither = static_cast<int>(material->shadowDitherMode);
        if (InspectorUndo::Combo(m_UndoRedo, "Shadow Dither", &currentDither, shadowDitherModes, 4)) {
            material->shadowDitherMode = static_cast<u8>(currentDither);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply dither pattern to shadows instead of smooth darkening");

        // Shadow dither pattern (only shown when dither mode is active)
        if (material->shadowDitherMode > 0) {
            const char* shadowDitherPatterns[] = { "Bayer 4x4", "Bayer 8x8", "Blue Noise", "Halftone", "Crosshatch", "Overlook" };
            int currentPattern = static_cast<int>(material->shadowDitherPattern);
            if (currentPattern > 5) currentPattern = 0;
            if (InspectorUndo::Combo(m_UndoRedo, "Dither Pattern", &currentPattern, shadowDitherPatterns, 6)) {
                material->shadowDitherPattern = static_cast<u8>(currentPattern);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bayer: ordered grid | Blue Noise: organic scatter | Halftone: comic dots | Crosshatch: pen strokes | Overlook: geometric hex");
        }

        // Artistic surface controls
        if (ImGui::TreeNode("Artistic Surface")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Reflectivity", &material->reflectivity, 0.005f, 0.0f, 1.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fake environment reflection strength (0 = off, 0.3 = chrome, 0.8 = mirror)");
            InspectorUndo::DragFloat(m_UndoRedo, "Fresnel Power", &material->fresnelPower, 0.05f, 0.5f, 10.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Edge vs center falloff (low = uniform chrome, high = edge-only glass)");
            InspectorUndo::DragFloat(m_UndoRedo, "Rim Light", &material->rimLightStrength, 0.01f, 0.0f, 3.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Additive edge glow for silhouettes and backlit effects");

            // Preset buttons
            if (ImGui::Button("Metal")) { material->reflectivity = 0.6f; material->fresnelPower = 2.5f; material->rimLightStrength = 0.0f; }
            ImGui::SameLine();
            if (ImGui::Button("Glass")) { material->reflectivity = 0.3f; material->fresnelPower = 5.0f; material->rimLightStrength = 0.3f; }
            ImGui::SameLine();
            if (ImGui::Button("Rim Glow")) { material->reflectivity = 0.0f; material->fresnelPower = 3.0f; material->rimLightStrength = 1.5f; }
            ImGui::SameLine();
            if (ImGui::Button("Clear##artistic")) { material->reflectivity = 0.0f; material->fresnelPower = 5.0f; material->rimLightStrength = 0.0f; }

            ImGui::Separator();
            ImGui::Text("Procedural Surface Noise");
            InspectorUndo::DragFloat(m_UndoRedo, "Noise Scale", &material->surfaceNoiseScale, 0.1f, 0.0f, 50.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Noise frequency in world units (0 = off, 2-5 = fine grain, 10-20 = broad patches)");
            if (material->surfaceNoiseScale > 0.0f) {
                InspectorUndo::DragFloat(m_UndoRedo, "Noise Strength", &material->surfaceNoiseStrength, 0.005f, 0.0f, 1.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much noise modulates the diffuse color (0 = none, 0.3 = subtle, 1.0 = strong)");
            }
            // Noise presets
            if (ImGui::Button("Stone##noise")) { material->surfaceNoiseScale = 5.0f; material->surfaceNoiseStrength = 0.25f; }
            ImGui::SameLine();
            if (ImGui::Button("Wood##noise")) { material->surfaceNoiseScale = 8.0f; material->surfaceNoiseStrength = 0.15f; }
            ImGui::SameLine();
            if (ImGui::Button("Dirt##noise")) { material->surfaceNoiseScale = 3.0f; material->surfaceNoiseStrength = 0.35f; }
            ImGui::SameLine();
            if (ImGui::Button("Clear##noise")) { material->surfaceNoiseScale = 0.0f; material->surfaceNoiseStrength = 0.0f; }

            ImGui::TreePop();
        }

        // Transmission / Subsurface Scattering
        if (ImGui::TreeNode("Transmission / SSS")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Transmission", &material->transmission, 0.005f, 0.0f, 1.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Light transmission through surface (0 = opaque, 1 = fully transmissive)");
            InspectorUndo::DragFloat(m_UndoRedo, "IOR", &material->ior, 0.01f, 1.0f, 2.5f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Index of refraction (1.0 = vacuum, 1.33 = water, 1.5 = glass, 2.42 = diamond)");
            InspectorUndo::DragFloat(m_UndoRedo, "Thickness", &material->thickness, 0.01f, 0.0f, 10.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Thin-surface thickness for translucency light falloff (0 = solid volume)");

            ImGui::Separator();
            InspectorUndo::DragFloat(m_UndoRedo, "SSS Intensity", &material->sssIntensity, 0.005f, 0.0f, 1.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Subsurface scattering strength (skin, wax, marble, leaves)");
            if (material->sssIntensity > 0.0f) {
                InspectorUndo::DragFloat(m_UndoRedo, "SSS Radius", &material->sssRadius, 0.01f, 0.01f, 10.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scatter distance in world units");
                f32 sssCol[3] = { material->sssColor.x, material->sssColor.y, material->sssColor.z };
                if (InspectorUndo::ColorEdit3(m_UndoRedo, "SSS Color", sssCol,
                        [material](f32 r, f32 g, f32 b) { material->sssColor = Math::Vector3(r, g, b); })) {
                    material->sssColor = Math::Vector3(sssCol[0], sssCol[1], sssCol[2]);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scatter tint color (reddish for skin, greenish for leaves)");
            }

            // Presets
            if (ImGui::Button("Glass##sss")) { material->transmission = 0.9f; material->ior = 1.5f; material->thickness = 0.0f; material->sssIntensity = 0.0f; }
            ImGui::SameLine();
            if (ImGui::Button("Water##sss")) { material->transmission = 0.8f; material->ior = 1.33f; material->thickness = 0.0f; material->sssIntensity = 0.0f; }
            ImGui::SameLine();
            if (ImGui::Button("Skin##sss")) { material->transmission = 0.0f; material->ior = 1.4f; material->thickness = 0.0f; material->sssIntensity = 0.6f; material->sssRadius = 1.5f; material->sssColor = Math::Vector3(1.0f, 0.2f, 0.1f); }
            ImGui::SameLine();
            if (ImGui::Button("Leaf##sss")) { material->transmission = 0.3f; material->ior = 1.45f; material->thickness = 0.1f; material->sssIntensity = 0.4f; material->sssRadius = 0.5f; material->sssColor = Math::Vector3(0.3f, 0.8f, 0.1f); }

            ImGui::TreePop();
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Exclude From Cel Shading##Mat", &material->excludeFromCelShading);
        ImGui::SetItemTooltip("Exempt this material from the global cel shading effect");

        // Per-material geometry outline override
        ImGui::DragFloat("Outline Width##Mat", &material->outlineWidth, 0.001f, 0.0f, 0.2f, "%.3f");
        ImGui::SetItemTooltip("Per-material outline width (0 = use global setting).\nInverted-hull backface extrusion in world units.");
        if (material->outlineWidth > 0.0f) {
            ImGui::ColorEdit3("Outline Color##Mat", &material->outlineColor.x);
        }

        // Dithered gradient rendering
        InspectorUndo::Checkbox(m_UndoRedo, "Dithered Gradient##Mat", &material->ditherGradient);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Low-poly flat shading with banded lighting and dither transitions");
        if (material->ditherGradient) {
            int bands = static_cast<int>(material->ditherGradientBands);
            if (ImGui::SliderInt("Gradient Bands##DG", &bands, 2, 8)) {
                material->ditherGradientBands = static_cast<u8>(bands);
            }
            const char* patterns[] = { "Bayer 4x4", "Bayer 8x8", "Blue Noise", "Halftone", "Crosshatch", "Overlook" };
            int pat = static_cast<int>(material->ditherGradientPattern);
            if (ImGui::Combo("Dither Pattern##DG", &pat, patterns, 6)) {
                material->ditherGradientPattern = static_cast<u8>(pat);
            }
        }

        // Dithered transparency (CRT-style alternating pixel transparency)
        InspectorUndo::Checkbox(m_UndoRedo, "Dithered Transparency##Mat", &material->ditherTransparency);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("CRT-style transparency: alternating pixels with blend color, naturally blurred by phosphor bloom");
        if (material->ditherTransparency) {
            const char* dtPatterns[] = { "Checkerboard", "H-Stripe", "V-Stripe", "Bayer 2x2" };
            int dtPat = static_cast<int>(material->ditherTransPattern);
            if (ImGui::Combo("Trans Pattern##DT", &dtPat, dtPatterns, 4)) {
                material->ditherTransPattern = static_cast<u8>(dtPat);
            }
            float blendCol[3] = { material->ditherTransBlendColor.x, material->ditherTransBlendColor.y, material->ditherTransBlendColor.z };
            if (ImGui::ColorEdit3("Blend Color##DT", blendCol)) {
                material->ditherTransBlendColor = Math::Vector3(blendCol[0], blendCol[1], blendCol[2]);
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Trans Opacity##DT", &material->ditherTransOpacity, 0.01f, 0.0f, 1.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("0=all blend color, 1=all original, 0.5=even mix");
        }

        // Alpha mode
        const char* alphaModes[] = { "Opaque", "Mask", "Blend" };
        int currentMode = static_cast<int>(material->alphaMode);
        if (InspectorUndo::Combo(m_UndoRedo, "Alpha Mode", &currentMode, alphaModes, 3)) {
            material->alphaMode = static_cast<ECS::MaterialComponent::AlphaMode>(currentMode);
        }
        ImGui::SetItemTooltip("Opaque: solid | Mask: cutout by alpha | Blend: transparent");

        if (material->alphaMode == ECS::MaterialComponent::AlphaMode::Mask) {
            InspectorUndo::DragFloat(m_UndoRedo, "Alpha Cutoff", &material->alphaCutoff, 0.01f, 0.0f, 1.0f);
            ImGui::SetItemTooltip("Alpha threshold below which pixels are discarded");
        }

        // Texture paths
        if (ImGui::TreeNode("Textures")) {
            // Per-material texture filter override. "Global" follows the scene's
            // Rendering > Texture Filtering setting; the others force this material's
            // textures to a specific filter (e.g. Point for crisp pixel-art).
            const char* filterOverrides[] = { "Global", "Point (Nearest)", "Bilinear", "Trilinear" };
            int filterOv = static_cast<int>(material->textureFilterOverride);
            if (filterOv < 0 || filterOv > 3) filterOv = 0;
            if (InspectorUndo::Combo(m_UndoRedo, "Filter", &filterOv, filterOverrides, 4)) {
                material->textureFilterOverride = static_cast<u8>(filterOv);
            }
            ImGui::Spacing();

            // ── Atlas Region (trim sheets): map this material into a sub-rect
            //    of an atlas texture; mesh UVs tile within the region.
            if (ImGui::TreeNode("Atlas Region")) {
                char atlasBuf[512];
                snprintf(atlasBuf, sizeof(atlasBuf), "%s", m_LoadedAtlasPath.c_str());
                if (ImGui::InputText("Atlas JSON", atlasBuf, sizeof(atlasBuf))) m_LoadedAtlasPath = atlasBuf;
                ImGui::SameLine();
                if (ImGui::Button("Load")) {
                    m_LoadedAtlasRegions.clear();
                    m_LoadedAtlasImage.clear();
                    std::ifstream f(m_LoadedAtlasPath);
                    if (f.is_open()) {
                        try {
                            nlohmann::json aj = nlohmann::json::parse(f);
                            m_LoadedAtlasImage = aj.value("image", "");
                            if (aj.contains("regions")) {
                                for (auto& [name, r] : aj["regions"].items()) {
                                    if (r.is_array() && r.size() >= 4)
                                        m_LoadedAtlasRegions.emplace_back(name, Math::Vector4(
                                            r[0].get<f32>(), r[1].get<f32>(), r[2].get<f32>(), r[3].get<f32>()));
                                }
                            }
                        } catch (...) {}
                    }
                }
                if (!m_LoadedAtlasRegions.empty()) {
                    for (const auto& [name, r] : m_LoadedAtlasRegions) {
                        if (ImGui::SmallButton(name.c_str())) {
                            material->uvRegionOffset = Math::Vector2(r.x, r.y);
                            material->uvRegionScale = Math::Vector2(r.z, r.w);
                            if (!m_LoadedAtlasImage.empty()) {
                                std::filesystem::path base = std::filesystem::path(m_LoadedAtlasPath).parent_path();
                                material->baseColorTexturePath = (base / m_LoadedAtlasImage).string();
                                material->InvalidateTextureCache();
                            }
                        }
                        ImGui::SameLine();
                    }
                    ImGui::NewLine();
                }
                ImGui::DragFloat2("Region Offset", &material->uvRegionOffset.x, 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat2("Region Scale", &material->uvRegionScale.x, 0.001f, 0.001f, 1.0f);
                if (ImGui::Button("Clear Region")) {
                    material->uvRegionOffset = Math::Vector2(0.0f, 0.0f);
                    material->uvRegionScale = Math::Vector2(1.0f, 1.0f);
                }
                ImGui::TreePop();
            }

            // Helper: accept image drag-drop on last widget
            auto textureDrop = [&](std::string& pathField, i32& cacheField) {
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                        std::string p(static_cast<const char*>(payload->Data));
                        std::string e = std::filesystem::path(p).extension().string();
                        std::transform(e.begin(), e.end(), e.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" || e == ".bmp" || e == ".svg") {
                            pathField = p;
                            cacheField = -1;
                            material->InvalidateTextureCache();
                            if (m_RenderSystem) m_RenderSystem->ClearFailedTexture(p);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            };

            // Base color texture path
            char basePath[256];
            strncpy(basePath, material->baseColorTexturePath.c_str(), sizeof(basePath) - 1);
            basePath[sizeof(basePath) - 1] = '\0';
            if (InspectorUndo::InputText(m_UndoRedo, "Base Color", basePath, sizeof(basePath),
                    [material](const std::string& val) {
                        material->baseColorTexturePath = val;
                        if (val.empty()) material->baseColorTexture = -1;
                    })) {
                material->baseColorTexturePath = basePath;
                if (material->baseColorTexturePath.empty()) material->baseColorTexture = -1;
            }
            textureDrop(material->baseColorTexturePath, material->baseColorTexture);

            // Normal map texture path
            char normalPath[256];
            strncpy(normalPath, material->normalTexturePath.c_str(), sizeof(normalPath) - 1);
            normalPath[sizeof(normalPath) - 1] = '\0';
            if (InspectorUndo::InputText(m_UndoRedo, "Normal Map", normalPath, sizeof(normalPath),
                    [material](const std::string& val) {
                        material->normalTexturePath = val;
                        if (val.empty()) material->normalTexture = -1;
                    })) {
                material->normalTexturePath = normalPath;
                if (material->normalTexturePath.empty()) material->normalTexture = -1;
            }
            textureDrop(material->normalTexturePath, material->normalTexture);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tangent-space normal map (RGB encoded)");

            // Metallic-roughness texture path
            char mrPath[256];
            strncpy(mrPath, material->metallicRoughnessTexturePath.c_str(), sizeof(mrPath) - 1);
            mrPath[sizeof(mrPath) - 1] = '\0';
            if (InspectorUndo::InputText(m_UndoRedo, "Metallic/Roughness", mrPath, sizeof(mrPath),
                    [material](const std::string& val) {
                        material->metallicRoughnessTexturePath = val;
                        if (val.empty()) material->metallicRoughnessTexture = -1;
                    })) {
                material->metallicRoughnessTexturePath = mrPath;
                if (material->metallicRoughnessTexturePath.empty()) material->metallicRoughnessTexture = -1;
            }
            textureDrop(material->metallicRoughnessTexturePath, material->metallicRoughnessTexture);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("glTF metallic-roughness (G=roughness, B=metallic)");

            // Emissive texture path
            char emissivePath[256];
            strncpy(emissivePath, material->emissiveTexturePath.c_str(), sizeof(emissivePath) - 1);
            emissivePath[sizeof(emissivePath) - 1] = '\0';
            if (InspectorUndo::InputText(m_UndoRedo, "Emissive Map", emissivePath, sizeof(emissivePath),
                    [material](const std::string& val) {
                        material->emissiveTexturePath = val;
                        if (val.empty()) material->emissiveTexture = -1;
                    })) {
                material->emissiveTexturePath = emissivePath;
                if (material->emissiveTexturePath.empty()) material->emissiveTexture = -1;
            }
            textureDrop(material->emissiveTexturePath, material->emissiveTexture);

            // Matcap (Material Capture) — a hand-painted reflection style. The sphere
            // image is indexed by the view-space normal, so whatever is painted into
            // it becomes the object's reflection and sheen. Tailored and deterministic:
            // it renders the same every frame on every machine, no probe or SSR needed.
            ImGui::Separator();
            ImGui::TextUnformatted("Matcap Reflection (hand-painted)");
            char matcapPath[256];
            strncpy(matcapPath, material->matcapTexturePath.c_str(), sizeof(matcapPath) - 1);
            matcapPath[sizeof(matcapPath) - 1] = '\0';
            if (InspectorUndo::InputText(m_UndoRedo, "Matcap", matcapPath, sizeof(matcapPath),
                    [material](const std::string& val) {
                        material->matcapTexturePath = val;
                        if (val.empty()) material->matcapTexture = -1;
                    })) {
                material->matcapTexturePath = matcapPath;
                if (material->matcapTexturePath.empty()) material->matcapTexture = -1;
            }
            textureDrop(material->matcapTexturePath, material->matcapTexture);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Matcap: a sphere image indexed by the view-space normal.\nPaint the reflection and sheen you want and it renders it back,\nidentically every frame. A hand-crafted reflection style — no\nreflection probe, no screen-space tricks.");
            if (!material->matcapTexturePath.empty())
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Matcap reflection active");

            // Scrolling reflection — a hand-crafted fake reflection (N64 chrome/water):
            // the texture scrolls over time, masked by fresnel. Deterministic and painted.
            ImGui::Separator();
            ImGui::TextUnformatted("Scrolling Reflection (N64 chrome/water)");
            char scrollPath[256];
            strncpy(scrollPath, material->scrollReflectionTexturePath.c_str(), sizeof(scrollPath) - 1);
            scrollPath[sizeof(scrollPath) - 1] = '\0';
            if (InspectorUndo::InputText(m_UndoRedo, "Scroll Reflection", scrollPath, sizeof(scrollPath),
                    [material](const std::string& val) {
                        material->scrollReflectionTexturePath = val;
                        if (val.empty()) material->scrollReflectionTexture = -1;
                    })) {
                material->scrollReflectionTexturePath = scrollPath;
                if (material->scrollReflectionTexturePath.empty()) material->scrollReflectionTexture = -1;
            }
            textureDrop(material->scrollReflectionTexturePath, material->scrollReflectionTexture);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("A texture scrolled over time and blended in by fresnel — a\ncheap, painted reflection. No probe, no screen-space tracing.");
            if (!material->scrollReflectionTexturePath.empty()) {
                f32 spd[2] = { material->scrollReflectionSpeed.x, material->scrollReflectionSpeed.y };
                if (ImGui::DragFloat2("Scroll Speed##ScrollRefl", spd, 0.005f, -2.0f, 2.0f)) {
                    material->scrollReflectionSpeed = Math::Vector2(spd[0], spd[1]);
                }
                ImGui::DragFloat("Strength##ScrollRefl", &material->scrollReflectionStrength, 0.01f, 0.0f, 2.0f);
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Scrolling reflection active");
            }
            ImGui::TreePop();
        }

        // Parallax / Height mapping
        if (ImGui::TreeNode("Parallax Mapping")) {
            char heightPath[256];
            strncpy(heightPath, material->heightTexturePath.c_str(), sizeof(heightPath) - 1);
            heightPath[sizeof(heightPath) - 1] = '\0';
            if (InspectorUndo::InputText(m_UndoRedo, "Height Map Path", heightPath, sizeof(heightPath),
                    [material](const std::string& val) {
                        material->heightTexturePath = val;
                        if (val.empty()) material->heightTexture = -1;
                    })) {
                material->heightTexturePath = heightPath;
                if (material->heightTexturePath.empty()) {
                    material->heightTexture = -1;
                }
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    std::string p(static_cast<const char*>(payload->Data));
                    std::string e = std::filesystem::path(p).extension().string();
                    std::transform(e.begin(), e.end(), e.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" || e == ".bmp" || e == ".svg") {
                        material->heightTexturePath = p;
                        material->heightTexture = -1;
                        material->InvalidateTextureCache();
                        if (m_RenderSystem) m_RenderSystem->ClearFailedTexture(p);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Path to grayscale height map texture");

            InspectorUndo::DragFloat(m_UndoRedo, "Parallax Scale", &material->parallaxScale, 0.001f, 0.0f, 0.2f, "%.3f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Height displacement intensity (0.03-0.05 typical)");

            const char* parallaxModes[] = { "Basic", "Steep", "Occlusion Mapping", "Relief Mapping" };
            int pMode = static_cast<int>(material->parallaxMode);
            if (ImGui::Combo("Parallax Mode", &pMode, parallaxModes, 4)) {
                material->parallaxMode = static_cast<u32>(pMode);
            }
            if (material->parallaxMode >= 1) {
                int steps = static_cast<int>(material->pomMaxSteps);
                if (ImGui::DragInt("POM Max Steps", &steps, 1, 8, 128)) {
                    material->pomMaxSteps = static_cast<u32>(steps);
                }
                ImGui::DragFloat("POM Height Scale", &material->pomHeightScale, 0.001f, 0.0f, 0.3f, "%.3f");
            }

            ImGui::TreePop();
        }

        // Surface response: sound + particle this material makes when walked on or hit.
        if (ImGui::TreeNode("Surface Response")) {
            char footBuf[256];
            strncpy(footBuf, material->footstepSound.c_str(), sizeof(footBuf) - 1);
            footBuf[sizeof(footBuf) - 1] = '\0';
            if (ImGui::InputText("Footstep Sound", footBuf, sizeof(footBuf)))
                material->footstepSound = footBuf;
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PATH"))
                    material->footstepSound = std::string(static_cast<const char*>(pl->Data));
                ImGui::EndDragDropTarget();
            }

            char hitBuf[256];
            strncpy(hitBuf, material->impactSound.c_str(), sizeof(hitBuf) - 1);
            hitBuf[sizeof(hitBuf) - 1] = '\0';
            if (ImGui::InputText("Impact Sound", hitBuf, sizeof(hitBuf)))
                material->impactSound = hitBuf;
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PATH"))
                    material->impactSound = std::string(static_cast<const char*>(pl->Data));
                ImGui::EndDragDropTarget();
            }

            const char* particles[] = { "None", "Dust", "Grass", "Spark", "Splash", "Smoke", "Snow" };
            int sp = static_cast<int>(material->surfaceParticle);
            if (sp < 0 || sp > 6) sp = 0;
            if (ImGui::Combo("Surface Particle", &sp, particles, IM_ARRAYSIZE(particles)))
                material->surfaceParticle = static_cast<u8>(sp);
            ImGui::SetItemTooltip("Bursts at the foot/contact when this surface is walked on or struck");

            ImGui::SliderFloat("Footstep Volume", &material->footstepVolume, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Impact Threshold", &material->impactThreshold, 0.1f, 0.0f, 50.0f);
            ImGui::SetItemTooltip("Minimum collision speed (units/s) that fires the impact sound");

            ImGui::TreePop();
        }

        // Retro rendering effects (per-material)
        if (ImGui::TreeNode("Retro Effects")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Flat Shading", &material->flatShading);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use face normals for faceted look");

            InspectorUndo::Checkbox(m_UndoRedo, "Affine Texturing", &material->affineTexturing);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("PS1-style texture warping (no perspective correction)");

            InspectorUndo::Checkbox(m_UndoRedo, "Vertex Snapping", &material->vertexSnapping);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("PS1-style vertex wobble from low-precision coordinates");

            if (material->vertexSnapping) {
                int snapRes = static_cast<int>(material->vertexSnapResolution);
                if (InspectorUndo::SliderInt(m_UndoRedo, "Snap Resolution", &snapRes, 80, 320)) {
                    material->vertexSnapResolution = static_cast<u8>(snapRes);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lower = more wobble (PS1 ~160)");
            }

            InspectorUndo::Checkbox(m_UndoRedo, "Stipple Transparency", &material->stippleTransparency);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Screen-door transparency using dither pattern");

            InspectorUndo::Checkbox(m_UndoRedo, "UV Quantize (PS1)", &material->uvQuantize);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap UVs to 128-step grid for PS1-style texture swimming");

            InspectorUndo::Checkbox(m_UndoRedo, "Gouraud Only", &material->gouraudOnly);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use vertex lighting only (no per-pixel), faceted PS1/N64 look");

            ImGui::TreePop();
        }

        // Mark material SSBO dirty so the GPU buffer is rebuilt with any changes
        if (m_RenderSystem) m_RenderSystem->MarkMaterialsDirty();
    }
}

void EditorLayer::DrawMaterialSlotsComponent(ECS::Entity entity) {
    bool slotsOpen = UI::SectionHeader("[M+] Material Slots", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("MaterialSlotsCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::MaterialSlotsComponent>(entity, "materialSlots", "Material Slots");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (slotsOpen) {
        ECS::MaterialSlotsComponent* matSlots = m_World->GetComponent<ECS::MaterialSlotsComponent>(entity);
        if (!matSlots || matSlots->slots.empty()) {
            ImGui::TextDisabled("No material slots");
            return;
        }
        DrawComponentHelp("materialSlots", m_World, entity);

        // Also get mesh to show sub-mesh names
        ECS::MeshComponent* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);

        ImGui::Text("%zu material slot(s)", matSlots->slots.size());
        ImGui::Separator();

        for (usize i = 0; i < matSlots->slots.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ECS::MaterialComponent& slot = matSlots->slots[i];

            // Build header label: "Slot N (SubMeshName)" if we can find a sub-mesh with this slot
            std::string headerLabel = "Slot " + std::to_string(i);
            if (mesh) {
                for (const auto& sm : mesh->subMeshes) {
                    if (sm.materialSlot == static_cast<i32>(i) && !sm.name.empty()) {
                        headerLabel += " (" + sm.name + ")";
                        break;
                    }
                }
            }
            headerLabel += "###matSlot" + std::to_string(i);

            if (ImGui::TreeNode(headerLabel.c_str())) {
                // Base color
                f32 baseColor[3] = { slot.baseColor.x, slot.baseColor.y, slot.baseColor.z };
                std::string bcLabel = "Base Color##slot" + std::to_string(i);
                if (ImGui::ColorEdit3(bcLabel.c_str(), baseColor)) {
                    slot.baseColor = Math::Vector3(baseColor[0], baseColor[1], baseColor[2]);
                }

                // Opacity
                std::string opLabel = "Opacity##slot" + std::to_string(i);
                ImGui::DragFloat(opLabel.c_str(), &slot.opacity, 0.01f, 0.0f, 1.0f);

                // PBR
                std::string metLabel = "Metallic##slot" + std::to_string(i);
                ImGui::DragFloat(metLabel.c_str(), &slot.metallic, 0.01f, 0.0f, 1.0f);
                std::string roughLabel = "Roughness##slot" + std::to_string(i);
                ImGui::DragFloat(roughLabel.c_str(), &slot.roughness, 0.01f, 0.0f, 1.0f);

                // Emission
                f32 emissive[3] = { slot.emissiveColor.x, slot.emissiveColor.y, slot.emissiveColor.z };
                std::string emLabel = "Emissive##slot" + std::to_string(i);
                if (ImGui::ColorEdit3(emLabel.c_str(), emissive)) {
                    slot.emissiveColor = Math::Vector3(emissive[0], emissive[1], emissive[2]);
                }
                std::string esLabel = "Emissive Strength##slot" + std::to_string(i);
                ImGui::DragFloat(esLabel.c_str(), &slot.emissiveStrength, 0.1f, 0.0f, 100.0f);

                // Rendering flags
                std::string dsLabel = "Double Sided##slot" + std::to_string(i);
                ImGui::Checkbox(dsLabel.c_str(), &slot.doubleSided);
                std::string csLabel = "Cast Shadows##slot" + std::to_string(i);
                ImGui::Checkbox(csLabel.c_str(), &slot.castShadows);
                std::string rsLabel = "Receive Shadows##slot" + std::to_string(i);
                ImGui::Checkbox(rsLabel.c_str(), &slot.receiveShadows);

                // Alpha mode
                const char* alphaModes[] = { "Opaque", "Mask", "Blend" };
                int currentMode = static_cast<int>(slot.alphaMode);
                std::string amLabel = "Alpha Mode##slot" + std::to_string(i);
                if (ImGui::Combo(amLabel.c_str(), &currentMode, alphaModes, 3)) {
                    slot.alphaMode = static_cast<ECS::MaterialComponent::AlphaMode>(currentMode);
                }

                // Texture paths
                std::string texLabel = "Textures##slot" + std::to_string(i);
                if (ImGui::TreeNode(texLabel.c_str())) {
                    // Helper: accept image drag-drop on last widget
                    auto textureDrop = [&](std::string& pathField, i32& cacheField) {
                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                                std::string p(static_cast<const char*>(payload->Data));
                                std::string e = std::filesystem::path(p).extension().string();
                                std::transform(e.begin(), e.end(), e.begin(),
                                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                                if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" || e == ".bmp" || e == ".svg") {
                                    pathField = p;
                                    cacheField = -1;
                                    slot.InvalidateTextureCache();
                                    if (m_RenderSystem) m_RenderSystem->ClearFailedTexture(p);
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    };

                    char basePath[256];
                    strncpy(basePath, slot.baseColorTexturePath.c_str(), sizeof(basePath) - 1);
                    basePath[sizeof(basePath) - 1] = '\0';
                    std::string bcTexLabel = "Base Color##slotTex" + std::to_string(i);
                    if (ImGui::InputText(bcTexLabel.c_str(), basePath, sizeof(basePath))) {
                        slot.baseColorTexturePath = basePath;
                        if (slot.baseColorTexturePath.empty()) slot.baseColorTexture = -1;
                        slot.InvalidateTextureCache();
                    }
                    textureDrop(slot.baseColorTexturePath, slot.baseColorTexture);

                    char normalPath[256];
                    strncpy(normalPath, slot.normalTexturePath.c_str(), sizeof(normalPath) - 1);
                    normalPath[sizeof(normalPath) - 1] = '\0';
                    std::string nmTexLabel = "Normal Map##slotTex" + std::to_string(i);
                    if (ImGui::InputText(nmTexLabel.c_str(), normalPath, sizeof(normalPath))) {
                        slot.normalTexturePath = normalPath;
                        if (slot.normalTexturePath.empty()) slot.normalTexture = -1;
                        slot.InvalidateTextureCache();
                    }
                    textureDrop(slot.normalTexturePath, slot.normalTexture);

                    char mrPath[256];
                    strncpy(mrPath, slot.metallicRoughnessTexturePath.c_str(), sizeof(mrPath) - 1);
                    mrPath[sizeof(mrPath) - 1] = '\0';
                    std::string mrTexLabel = "Metallic/Roughness##slotTex" + std::to_string(i);
                    if (ImGui::InputText(mrTexLabel.c_str(), mrPath, sizeof(mrPath))) {
                        slot.metallicRoughnessTexturePath = mrPath;
                        if (slot.metallicRoughnessTexturePath.empty()) slot.metallicRoughnessTexture = -1;
                        slot.InvalidateTextureCache();
                    }
                    textureDrop(slot.metallicRoughnessTexturePath, slot.metallicRoughnessTexture);

                    char emissivePath[256];
                    strncpy(emissivePath, slot.emissiveTexturePath.c_str(), sizeof(emissivePath) - 1);
                    emissivePath[sizeof(emissivePath) - 1] = '\0';
                    std::string emTexLabel = "Emissive Map##slotTex" + std::to_string(i);
                    if (ImGui::InputText(emTexLabel.c_str(), emissivePath, sizeof(emissivePath))) {
                        slot.emissiveTexturePath = emissivePath;
                        if (slot.emissiveTexturePath.empty()) slot.emissiveTexture = -1;
                        slot.InvalidateTextureCache();
                    }
                    textureDrop(slot.emissiveTexturePath, slot.emissiveTexture);

                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        // Mark material SSBO dirty so the GPU buffer is rebuilt with any changes
        if (m_RenderSystem) m_RenderSystem->MarkMaterialsDirty();
    }
}

void EditorLayer::DrawLightComponent(ECS::Entity entity) {
    bool lightOpen = UI::SectionHeader("[L] Light", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("LightCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::LightComponent>(entity, "light", "Light");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (lightOpen) {
        ECS::LightComponent* light = m_World->GetComponent<ECS::LightComponent>(entity);
        if (!light) return;
        DrawComponentHelp("light", m_World, entity);

        // Light type
        const char* lightTypes[] = { "Directional", "Point", "Spot" };
        int currentType = static_cast<int>(light->type);
        if (InspectorUndo::Combo(m_UndoRedo, "Type", &currentType, lightTypes, 3)) {
            light->type = static_cast<ECS::LightType>(currentType);
        }
        ImGui::SetItemTooltip("Directional: sun-like | Point: omnidirectional | Spot: cone-shaped");

        // Color
        f32 color[3] = { light->color.x, light->color.y, light->color.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Color", color,
                [light](f32 r, f32 g, f32 b) { light->color = Math::Vector3(r, g, b); })) {
            light->color = Math::Vector3(color[0], color[1], color[2]);
        }
        ImGui::SetItemTooltip("Light color");

        // Intensity
        InspectorUndo::DragFloat(m_UndoRedo, "Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
        ImGui::SetItemTooltip("Brightness multiplier");

        // Point/Spot specific
        if (light->type == ECS::LightType::Point || light->type == ECS::LightType::Spot) {
            InspectorUndo::DragFloat(m_UndoRedo, "Range", &light->range, 0.5f, 0.1f, 1000.0f);
            ImGui::SetItemTooltip("Maximum distance the light reaches");

            if (ImGui::TreeNode("Attenuation")) {
                InspectorUndo::DragFloat(m_UndoRedo, "Constant", &light->constantAttenuation, 0.01f, 0.0f, 10.0f);
                ImGui::SetItemTooltip("Base attenuation (distance-independent)");
                InspectorUndo::DragFloat(m_UndoRedo, "Linear", &light->linearAttenuation, 0.001f, 0.0f, 1.0f);
                ImGui::SetItemTooltip("Linear falloff factor");
                InspectorUndo::DragFloat(m_UndoRedo, "Quadratic", &light->quadraticAttenuation, 0.001f, 0.0f, 1.0f);
                ImGui::SetItemTooltip("Quadratic falloff factor (physically realistic)");
                ImGui::TreePop();
            }
        }

        // Spot specific
        if (light->type == ECS::LightType::Spot) {
            InspectorUndo::DragFloat(m_UndoRedo, "Inner Cone", &light->innerConeAngle, 0.5f, 0.0f, light->outerConeAngle);
            ImGui::SetItemTooltip("Angle of full-intensity spotlight cone");
            InspectorUndo::DragFloat(m_UndoRedo, "Outer Cone", &light->outerConeAngle, 0.5f, light->innerConeAngle, 90.0f);
            ImGui::SetItemTooltip("Angle where light fades to zero");
        }

        // Shadows
        InspectorUndo::Checkbox(m_UndoRedo, "Cast Shadows", &light->castShadows);
        ImGui::SetItemTooltip("Enable shadow casting from this light");
        if (light->castShadows && m_RenderSystem &&
            (light->type == ECS::LightType::Point || light->type == ECS::LightType::Spot)) {
            if (m_RenderSystem->LightHasShadowSlot(entity)) {
                ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "Shadow slot: active");
                ImGui::SetItemTooltip("This light won one of the per-frame shadow map slots\n(4 point + 4 spot, strongest by intensity over distance)");
            } else {
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "Shadow slot: outbid - light passes through walls");
                ImGui::SetItemTooltip("Only the 4 strongest point and 4 strongest spot lights get shadow\nmaps each frame (intensity over distance squared). This light lost the\ncontest, so it cannot be occluded. Raise its intensity, move it closer,\nor reduce competing shadow casters.");
            }
        }
    }
}

void EditorLayer::DrawCameraComponent(ECS::Entity entity) {
    bool camOpen = UI::SectionHeader("[C] Camera", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("CameraCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::CameraComponent>(entity, "camera", "Camera");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (camOpen) {
        ECS::CameraComponent* camera = m_World->GetComponent<ECS::CameraComponent>(entity);
        if (!camera) return;
        DrawComponentHelp("camera", m_World, entity);

        // Camera preset dropdown
        {
            const char* presetNames[] = {
                "(Custom)", "Isometric 45", "Isometric 30", "Top-Down", "Side Scroller",
                "First Person", "Third Person", "Cinematic Wide", "Security Cam", "Bird's Eye"
            };
            int presetIdx = 0; // Default to Custom
            if (ImGui::Combo("Preset", &presetIdx, presetNames, 10)) {
                if (presetIdx > 0) {
                    auto result = ECS::ApplyCameraPreset(static_cast<ECS::CameraPreset>(presetIdx));
                    *camera = result.camera;
                    // Apply rotation to transform
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (transform) {
                        transform->rotation = Math::Quaternion::FromEuler(result.rotation);
                    }
                }
            }
            ImGui::SetItemTooltip("Apply a camera preset configuration");
        }

        ImGui::Separator();

        // Projection type
        const char* projTypes[] = { "Perspective", "Orthographic" };
        int currentType = static_cast<int>(camera->projectionType);
        if (InspectorUndo::Combo(m_UndoRedo, "Projection", &currentType, projTypes, 2)) {
            if (currentType >= 0 && currentType <= 1) {
                camera->projectionType = static_cast<ECS::ProjectionType>(currentType);
            }
        }
        ImGui::SetItemTooltip("Perspective: 3D depth | Orthographic: flat/2D");

        // Perspective settings
        if (camera->projectionType == ECS::ProjectionType::Perspective) {
            InspectorUndo::DragFloat(m_UndoRedo, "Field of View", &camera->fieldOfView, 0.5f, 1.0f, 179.0f);
            ImGui::SetItemTooltip("Vertical field of view in degrees");
        }

        // Orthographic settings
        if (camera->projectionType == ECS::ProjectionType::Orthographic) {
            InspectorUndo::DragFloat(m_UndoRedo, "Ortho Size", &camera->orthoSize, 0.5f, 0.1f, 100.0f);
            ImGui::SetItemTooltip("Half-height of the orthographic view volume");
        }

        // Common settings
        InspectorUndo::DragFloat(m_UndoRedo, "Near Plane", &camera->nearPlane, 0.01f, 0.001f, camera->farPlane - 0.01f);
        ImGui::SetItemTooltip("Minimum render distance (objects closer are clipped)");
        InspectorUndo::DragFloat(m_UndoRedo, "Far Plane", &camera->farPlane, 1.0f, camera->nearPlane + 0.01f, 10000.0f);
        ImGui::SetItemTooltip("Maximum render distance (objects farther are clipped)");

        ImGui::Separator();

        // Rendering
        InspectorUndo::Checkbox(m_UndoRedo, "Post-Processing", &camera->enablePostProcessing);
        ImGui::SetItemTooltip("Enable bloom, FXAA, tone mapping, color grading on this camera");

        ImGui::Separator();

        // Virtual camera settings
        ImGui::Text("Virtual Camera");
        InspectorUndo::Checkbox(m_UndoRedo, "Active", &camera->isActive);
        InspectorUndo::DragInt(m_UndoRedo, "Priority", &camera->priority, 1, -100, 100);
        ImGui::TextDisabled("(Higher priority cameras take precedence)");

        ImGui::Separator();

        // Clear settings
        if (ImGui::TreeNode("Clear Settings")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Clear Color", &camera->clearColor);
            InspectorUndo::Checkbox(m_UndoRedo, "Clear Depth", &camera->clearDepth);
            if (camera->clearColor) {
                f32 bgColor[3] = { camera->backgroundColor.x, camera->backgroundColor.y, camera->backgroundColor.z };
                if (InspectorUndo::ColorEdit3(m_UndoRedo, "Background Color", bgColor,
                        [camera](f32 r, f32 g, f32 b) { camera->backgroundColor = Math::Vector3(r, g, b); })) {
                    camera->backgroundColor = Math::Vector3(bgColor[0], bgColor[1], bgColor[2]);
                }
            }
            ImGui::TreePop();
        }

        // Viewport settings
        if (ImGui::TreeNode("Viewport")) {
            InspectorUndo::DragFloat(m_UndoRedo, "X", &camera->viewportX, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Y", &camera->viewportY, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Width", &camera->viewportWidth, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Height", &camera->viewportHeight, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

    }
}

void EditorLayer::DrawNotesComponent(ECS::Entity entity) {
    bool notesOpen = UI::SectionHeader("Notes", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("NotesCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::NotesComponent>(entity, "notes", "Notes");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (notesOpen) {
        ECS::NotesComponent* notes = m_World->GetComponent<ECS::NotesComponent>(entity);
        if (!notes) return;
        DrawComponentHelp("notes", m_World, entity);

        // Multi-line text input for notes
        static char notesBuffer[4096];
        strncpy(notesBuffer, notes->notes.c_str(), sizeof(notesBuffer) - 1);
        notesBuffer[sizeof(notesBuffer) - 1] = '\0';

        ImGui::TextDisabled("Developer notes (not exported to builds)");
        if (InspectorUndo::InputTextMultiline(m_UndoRedo, "##Notes", notesBuffer, sizeof(notesBuffer),
                [notes](const std::string& val) { notes->notes = val; },
                ImVec2(-1, 100), ImGuiInputTextFlags_AllowTabInput)) {
            notes->notes = notesBuffer;
        }
    }
}

void EditorLayer::DrawTextComponent(ECS::Entity entity) {
    bool textOpen = UI::SectionHeader("Text", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TextCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::TextComponent>(entity, "text", "Text");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (textOpen) {
        ECS::TextComponent* text = m_World->GetComponent<ECS::TextComponent>(entity);
        if (!text) return;
        DrawComponentHelp("text", m_World, entity);

        // --- Silent-failure warnings ---
        // Text can rasterize to nothing or render invisibly with no error in the
        // log. Surface the common causes right at the top so "my text isn't
        // showing" has an answer instead of being a mystery.
        {
            std::vector<std::pair<int, std::string>> warns;  // first: 1=warning, 0=info
            auto colorClose = [](const Math::Vector3& a, const Math::Vector3& b) {
                return std::fabs(a.x - b.x) < 0.04f && std::fabs(a.y - b.y) < 0.04f &&
                       std::fabs(a.z - b.z) < 0.04f;
            };

            if (text->text.empty()) {
                warns.push_back({0, "No text yet - nothing renders until you type content below."});
            }
            if (!text->fontPath.empty()) {
                std::error_code ec;
                bool found = std::filesystem::exists(text->fontPath, ec);
                if (!found && !m_SceneManager.GetProjectPath().empty()) {
                    auto root = std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path();
                    found = std::filesystem::exists(root / text->fontPath, ec);
                }
                if (!found)
                    warns.push_back({1, "Font not found: '" + text->fontPath +
                                        "'. Falling back to the built-in font."});
            }
            if (!text->text.empty() && text->bgOpacity > 0.5f &&
                colorClose(text->textColor, text->bgColor)) {
                warns.push_back({1, "Text color matches the opaque background - the text will be invisible."});
            }
            f32 usableW = static_cast<f32>(text->textureWidth) - text->paddingX * 2.0f;
            f32 usableH = static_cast<f32>(text->textureHeight) - text->paddingY * 2.0f;
            if (usableW <= 0.0f || usableH <= 0.0f) {
                warns.push_back({1, "Padding leaves no room in the texture - lower Padding or raise Texture Width/Height."});
            } else if (!text->text.empty() && text->fontSize > usableH) {
                warns.push_back({1, "Font Size is taller than the texture - text may be clipped. Raise Texture Height or lower Font Size."});
            }
            if (text->wrapWidth <= text->paddingX * 2.0f) {
                warns.push_back({1, "Wrap Width sits inside the padding - no room for a line. Raise Wrap Width."});
            }
            if (auto* tr = m_World->GetComponent<ECS::TransformComponent>(entity)) {
                if (std::fabs(tr->scale.x) < 1e-4f || std::fabs(tr->scale.y) < 1e-4f) {
                    warns.push_back({1, "Entity scale is ~0 - the text surface has no size to draw on."});
                }
            }

            for (const auto& w : warns) {
                ImVec4 c = (w.first == 1) ? ImVec4(1.0f, 0.70f, 0.20f, 1.0f)
                                          : ImVec4(0.60f, 0.70f, 0.90f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, c);
                ImGui::TextWrapped("%s %s", (w.first == 1) ? "(!)" : "(i)", w.second.c_str());
                ImGui::PopStyleColor();
            }
            if (!warns.empty()) ImGui::Separator();
        }

        // Multi-line text input
        static char textBuffer[8192];
        strncpy(textBuffer, text->text.c_str(), sizeof(textBuffer) - 1);
        textBuffer[sizeof(textBuffer) - 1] = '\0';

        ImGui::TextDisabled("Text content (multi-line)");
        if (InspectorUndo::InputTextMultiline(m_UndoRedo, "##TextContent", textBuffer, sizeof(textBuffer),
                [text](const std::string& val) { text->text = val; text->dirty = true; },
                ImVec2(-1, 120), ImGuiInputTextFlags_AllowTabInput)) {
            text->text = textBuffer;
            text->dirty = true;
        }

        // Font path with browse button
        static char fontPathBuffer[512];
        strncpy(fontPathBuffer, text->fontPath.c_str(), sizeof(fontPathBuffer) - 1);
        fontPathBuffer[sizeof(fontPathBuffer) - 1] = '\0';

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
        if (InspectorUndo::InputText(m_UndoRedo, "##FontPath", fontPathBuffer, sizeof(fontPathBuffer),
                [text](const std::string& val) { text->fontPath = val; text->dirty = true; })) {
            text->fontPath = fontPathBuffer;
            text->dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse##Font")) {
            std::string path = FileDialog::OpenFile("Select Font", {{ "Font Files", "*.ttf;*.otf" }});
            if (!path.empty()) {
                text->fontPath = path;
                text->dirty = true;
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Font");

        // Font size
        if (InspectorUndo::DragFloat(m_UndoRedo, "Font Size", &text->fontSize, 0.5f, 4.0f, 256.0f)) {
            text->dirty = true;
        }

        // Texture dimensions
        int texW = static_cast<int>(text->textureWidth);
        int texH = static_cast<int>(text->textureHeight);
        if (InspectorUndo::DragInt(m_UndoRedo, "Texture Width", &texW, 16, 64, 4096)) {
            text->textureWidth = static_cast<u32>(texW);
            text->dirty = true;
        }
        if (InspectorUndo::DragInt(m_UndoRedo, "Texture Height", &texH, 16, 64, 4096)) {
            text->textureHeight = static_cast<u32>(texH);
            text->dirty = true;
        }

        // Wrap width
        if (InspectorUndo::DragFloat(m_UndoRedo, "Wrap Width", &text->wrapWidth, 1.0f, 32.0f, 4096.0f)) {
            text->dirty = true;
        }

        // Padding
        if (InspectorUndo::DragFloat(m_UndoRedo, "Padding X", &text->paddingX, 0.5f, 0.0f, 256.0f)) {
            text->dirty = true;
        }
        if (InspectorUndo::DragFloat(m_UndoRedo, "Padding Y", &text->paddingY, 0.5f, 0.0f, 256.0f)) {
            text->dirty = true;
        }

        // Alignment
        const char* alignItems[] = { "Left", "Center", "Right" };
        int currentAlign = static_cast<int>(text->horizontalAlign);
        if (InspectorUndo::Combo(m_UndoRedo, "Alignment", &currentAlign, alignItems, 3)) {
            text->horizontalAlign = static_cast<ECS::TextAlign>(currentAlign);
            text->dirty = true;
        }

        // Text color
        f32 textCol[3] = { text->textColor.x, text->textColor.y, text->textColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Text Color", textCol,
                [text](f32 r, f32 g, f32 b) { text->textColor = Math::Vector3(r, g, b); text->dirty = true; })) {
            text->textColor = Math::Vector3(textCol[0], textCol[1], textCol[2]);
            text->dirty = true;
        }

        // Background color and opacity
        f32 bgCol[3] = { text->bgColor.x, text->bgColor.y, text->bgColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Background", bgCol,
                [text](f32 r, f32 g, f32 b) { text->bgColor = Math::Vector3(r, g, b); text->dirty = true; })) {
            text->bgColor = Math::Vector3(bgCol[0], bgCol[1], bgCol[2]);
            text->dirty = true;
        }
        if (InspectorUndo::DragFloat(m_UndoRedo, "BG Opacity", &text->bgOpacity, 0.01f, 0.0f, 1.0f)) {
            text->dirty = true;
        }
    }
}

// ============================================================================
// Atlas Packer (Tools menu): shelf-packs a folder of images into one atlas +
// a .atlas.json region map. Regions are aligned to a 1/256 UV grid so the
// material path's packed-u8 region quantization is lossless.
// ============================================================================
void EditorLayer::DrawAtlasPackerWindow() {
    if (!m_ShowAtlasPacker) return;
    ImGui::SetNextWindowSize(ImVec2(460, 240), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Atlas Packer", &m_ShowAtlasPacker)) { ImGui::End(); return; }

    ImGui::TextWrapped("Packs every image in a folder into one atlas texture + a region map. "
                       "Apply regions to materials in the material inspector's Atlas Region section.");
    ImGui::InputText("Image Folder", m_AtlasInputDir, sizeof(m_AtlasInputDir));
    ImGui::InputText("Output Name", m_AtlasOutputName, sizeof(m_AtlasOutputName));
    const char* sizes[] = { "1024", "2048", "4096" };
    int sizeIdx = (m_AtlasSize == 1024) ? 0 : (m_AtlasSize == 4096 ? 2 : 1);
    if (ImGui::Combo("Atlas Size", &sizeIdx, sizes, 3)) m_AtlasSize = (sizeIdx == 0) ? 1024 : (sizeIdx == 2 ? 4096 : 2048);

    if (ImGui::Button("Pack")) {
        namespace fs = std::filesystem;
        m_AtlasStatus.clear();
        struct Src { std::string name; int w, h; u8* pixels; };
        std::vector<Src> images;
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(m_AtlasInputDir, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".tga" && ext != ".bmp") continue;
            int w = 0, h = 0, ch = 0;
            u8* px = stbi_load(entry.path().string().c_str(), &w, &h, &ch, 4);
            if (px) images.push_back({entry.path().stem().string(), w, h, px});
        }
        if (ec) m_AtlasStatus = "Folder not readable: " + std::string(ec.message());
        else if (images.empty()) m_AtlasStatus = "No images found in that folder.";
        else {
            // Tallest-first shelf packing on a grid (grid = atlas/256 so the
            // normalized regions land exactly on 1/256 steps).
            std::sort(images.begin(), images.end(), [](const Src& a, const Src& b) { return a.h > b.h; });
            const int A = m_AtlasSize;
            const int grid = A / 256;
            auto align = [grid](int v) { return ((v + grid - 1) / grid) * grid; };
            std::vector<u8> atlas(static_cast<usize>(A) * A * 4, 0);
            struct Placed { std::string name; int x, y, w, h; };
            std::vector<Placed> placed;
            int cx = 0, cy = 0, shelfH = 0;
            bool overflow = false;
            for (auto& img : images) {
                int w = align(img.w), h = align(img.h);
                if (cx + w > A) { cy += shelfH; cx = 0; shelfH = 0; }
                if (cy + h > A || w > A) { overflow = true; continue; }
                for (int row = 0; row < img.h; ++row)
                    memcpy(&atlas[((static_cast<usize>(cy) + row) * A + cx) * 4],
                           &img.pixels[static_cast<usize>(row) * img.w * 4],
                           static_cast<usize>(img.w) * 4);
                placed.push_back({img.name, cx, cy, w, h});
                cx += w;
                if (h > shelfH) shelfH = h;
            }
            fs::path outDir = fs::path(m_AtlasInputDir);
            const std::string& projPath = m_SceneManager.GetProjectPath();
            if (!projPath.empty()) outDir = fs::path(projPath).parent_path() / "assets" / "textures";
            fs::create_directories(outDir, ec);
            std::string imgPath = (outDir / (std::string(m_AtlasOutputName) + ".png")).string();
            std::string jsonPath = (outDir / (std::string(m_AtlasOutputName) + ".atlas.json")).string();
            stbi_write_png(imgPath.c_str(), A, A, 4, atlas.data(), A * 4);
            std::string j = "{\n  \"image\": \"" + fs::path(imgPath).filename().string() + "\",\n  \"regions\": {\n";
            for (usize i = 0; i < placed.size(); ++i) {
                const auto& p = placed[i];
                char buf[256];
                snprintf(buf, sizeof(buf), "    \"%s\": [%.6f, %.6f, %.6f, %.6f]%s\n",
                         p.name.c_str(), static_cast<f32>(p.x) / A, static_cast<f32>(p.y) / A,
                         static_cast<f32>(p.w) / A, static_cast<f32>(p.h) / A,
                         (i + 1 < placed.size()) ? "," : "");
                j += buf;
            }
            j += "  }\n}\n";
            std::ofstream jf(jsonPath, std::ios::binary);
            jf << j;
            m_AtlasStatus = "Packed " + std::to_string(placed.size()) + " images -> " + imgPath +
                            (overflow ? "  (some images did not fit!)" : "");
            ENJIN_LOG_INFO(Editor, "Atlas packer: %s", m_AtlasStatus.c_str());
        }
        for (auto& img : images) stbi_image_free(img.pixels);
    }
    if (!m_AtlasStatus.empty()) ImGui::TextWrapped("%s", m_AtlasStatus.c_str());
    ImGui::End();
}

void EditorLayer::DrawWeatherZoneComponent(ECS::Entity entity) {
    bool wzOpen = UI::SectionHeader("Weather Zone", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("WeatherZoneCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::WeatherZoneComponent>(entity, "weatherZone", "Weather Zone");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (wzOpen) {
        ECS::WeatherZoneComponent* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
        if (!zone) return;
        DrawComponentHelp("weatherZone", m_World, entity);

        // Bounding box
        f32 extents[3] = { zone->halfExtents.x, zone->halfExtents.y, zone->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents", extents,
                [zone](f32 x, f32 y, f32 z) { zone->halfExtents = Math::Vector3(x, y, z); },
                0.5f, 0.1f, 500.0f)) {
            zone->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }
        InspectorUndo::DragInt(m_UndoRedo, "Priority", &zone->priority, 1, -100, 100);

        ImGui::Separator();

        // Weather type
        const char* weatherTypes[] = { "Clear", "Cloudy", "Rain", "Heavy Rain", "Snow", "Fog", "Storm" };
        int currentWeather = static_cast<int>(zone->weatherType);
        if (InspectorUndo::Combo(m_UndoRedo, "Weather Type", &currentWeather, weatherTypes, 7)) {
            zone->weatherType = static_cast<u32>(currentWeather);
        }

        // Show relevant controls based on weather type
        if (zone->weatherType == 2 || zone->weatherType == 3 || zone->weatherType == 6) {
            InspectorUndo::SliderFloat(m_UndoRedo, "Rain Intensity", &zone->rainIntensity, 0.0f, 1.0f);
        }
        if (zone->weatherType == 4) {
            InspectorUndo::SliderFloat(m_UndoRedo, "Snow Intensity", &zone->snowIntensity, 0.0f, 1.0f);
        }

        // Custom precipitation sprites (either can show: temperature zones can
        // flip rain to snow and back inside the same zone)
        if (zone->weatherType == 2 || zone->weatherType == 3 || zone->weatherType == 4 || zone->weatherType == 6) {
            auto drawSpritePicker = [&](const char* label, const char* id,
                                        std::string& path, i32& cachedIndex) {
                if (!path.empty()) {
                    size_t lastSlash = path.find_last_of("/\\");
                    std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
                    ImGui::Text("%s: %s", label, filename.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton((std::string("Clear##") + id).c_str())) {
                        path.clear();
                        cachedIndex = -2;
                    }
                } else {
                    if (ImGui::Button((std::string(label) + " Sprite...##" + id).c_str())) {
                        std::string picked = FileDialog::OpenFile("Texture",
                            {{ "Images", "*.png;*.jpg;*.jpeg;*.bmp;*.tga" }});
                        if (!picked.empty()) {
                            path = picked;
                            cachedIndex = -2;
                        }
                    }
                }
            };
            if (zone->weatherType != 4) {
                drawSpritePicker("Rain", "WZRain", zone->rainTexturePath, zone->cachedRainTexIndex);
            }
            drawSpritePicker("Snow", "WZSnow", zone->snowTexturePath, zone->cachedSnowTexIndex);
            ImGui::TextDisabled("Empty = built-in streak/flake look");
        }
        if (zone->weatherType == 6) {
            InspectorUndo::Checkbox(m_UndoRedo, "Lightning Enabled", &zone->lightningEnabled);
            if (zone->lightningEnabled) {
                InspectorUndo::DragFloat(m_UndoRedo, "Lightning Min Interval", &zone->lightningMinInterval, 0.1f, 0.1f, zone->lightningMaxInterval);
                InspectorUndo::DragFloat(m_UndoRedo, "Lightning Max Interval", &zone->lightningMaxInterval, 0.1f, zone->lightningMinInterval, 60.0f);
            }
        }

        // Wind settings (for rain/snow/storm)
        if (zone->weatherType >= 2 && zone->weatherType != 5) {
            ImGui::Spacing();
            ImGui::Text("Wind");
            f32 windDir[3] = { zone->windDirection.x, zone->windDirection.y, zone->windDirection.z };
            if (InspectorUndo::DragFloat3(m_UndoRedo, "Wind Direction", windDir,
                    [zone](f32 x, f32 y, f32 z) { zone->windDirection = Math::Vector3(x, y, z); },
                    0.05f, -1.0f, 1.0f)) {
                zone->windDirection = Math::Vector3(windDir[0], windDir[1], windDir[2]);
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Wind Strength", &zone->windStrength, 0.1f, 0.0f, 20.0f);
        }

        // Fog settings
        if (zone->weatherType >= 1) {
            ImGui::Spacing();
            ImGui::Text("Fog Settings");
            InspectorUndo::SliderFloat(m_UndoRedo, "Fog Density", &zone->fogDensity, 0.0f, 1.0f);
            f32 fogCol[3] = { zone->fogColor.x, zone->fogColor.y, zone->fogColor.z };
            if (InspectorUndo::ColorEdit3(m_UndoRedo, "Fog Color", fogCol,
                    [zone](f32 r, f32 g, f32 b) { zone->fogColor = Math::Vector3(r, g, b); })) {
                zone->fogColor = Math::Vector3(fogCol[0], fogCol[1], fogCol[2]);
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Fog Start", &zone->fogStart, 1.0f, 0.0f, zone->fogEnd);
            InspectorUndo::DragFloat(m_UndoRedo, "Fog End", &zone->fogEnd, 1.0f, zone->fogStart, 500.0f);
        }
    }
}

void EditorLayer::DrawWaterVolumeComponent(ECS::Entity entity) {
    bool wvOpen = UI::SectionHeader("Water Volume", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("WaterVolumeCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::WaterVolumeComponent>(entity, "waterVolume", "Water Volume");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (wvOpen) {
        ECS::WaterVolumeComponent* volume = m_World->GetComponent<ECS::WaterVolumeComponent>(entity);
        if (!volume) return;
        DrawComponentHelp("waterVolume", m_World, entity);

        // Track changes that require mesh regeneration
        auto oldExtents = volume->halfExtents;
        auto oldWaterType = volume->waterType;

        // Bounding box
        f32 extents[3] = { volume->halfExtents.x, volume->halfExtents.y, volume->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents", extents,
                [volume](f32 x, f32 y, f32 z) { volume->halfExtents = Math::Vector3(x, y, z); },
                0.5f, 0.1f, 500.0f)) {
            volume->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }
        InspectorUndo::DragInt(m_UndoRedo, "Priority", &volume->priority, 1, -100, 100);

        ImGui::Separator();

        // Water type
        const char* waterTypeNames[] = { "Lake", "Ocean", "River", "Pond" };
        int currentType = static_cast<int>(volume->waterType);
        if (InspectorUndo::Combo(m_UndoRedo, "Water Type", &currentType, waterTypeNames, 4)) {
            volume->waterType = static_cast<ECS::WaterType>(currentType);
        }

        // Water settings
        f32 waterCol[3] = { volume->waterColor.x, volume->waterColor.y, volume->waterColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Water Color", waterCol,
                [volume](f32 r, f32 g, f32 b) { volume->waterColor = Math::Vector3(r, g, b); })) {
            volume->waterColor = Math::Vector3(waterCol[0], waterCol[1], waterCol[2]);
        }
        InspectorUndo::SliderFloat(m_UndoRedo, "Opacity", &volume->opacity, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Wave Speed", &volume->waveSpeed, 0.1f, 0.0f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Wave Height", &volume->waveHeight, 0.01f, 0.0f, 2.0f);

        ImGui::Separator();

        // Shore & Foam
        InspectorUndo::Checkbox(m_UndoRedo, "Enable Shore Foam", &volume->enableShore);
        if (volume->enableShore) {
            InspectorUndo::SliderFloat(m_UndoRedo, "Shore Width", &volume->shoreWidth, 0.0f, 0.5f, "%.2f");
            InspectorUndo::SliderFloat(m_UndoRedo, "Foam Intensity", &volume->foamIntensity, 0.0f, 1.0f, "%.2f");
            InspectorUndo::DragFloat(m_UndoRedo, "Foam Scale", &volume->foamScale, 0.5f, 1.0f, 50.0f, "%.1f");
            f32 shoreCol[3] = { volume->shoreColor.x, volume->shoreColor.y, volume->shoreColor.z };
            if (InspectorUndo::ColorEdit3(m_UndoRedo, "Shore Color", shoreCol,
                    [volume](f32 r, f32 g, f32 b) { volume->shoreColor = Math::Vector3(r, g, b); })) {
                volume->shoreColor = Math::Vector3(shoreCol[0], shoreCol[1], shoreCol[2]);
            }
        }

        ImGui::Separator();

        // Buoyancy — floats dynamic rigidbodies that fall in
        InspectorUndo::Checkbox(m_UndoRedo, "Enable Buoyancy", &volume->enableBuoyancy);
        if (volume->enableBuoyancy) {
            InspectorUndo::DragFloat(m_UndoRedo, "Buoyancy Strength", &volume->buoyancyStrength, 0.05f, 0.0f, 6.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("How hard the water pushes bodies up.\n1 = neutral, >1 floats (higher = rides higher).");
            InspectorUndo::DragFloat(m_UndoRedo, "Buoyancy Drag", &volume->buoyancyDrag, 0.05f, 0.0f, 10.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Water resistance — damps bobbing and drift so things settle.");
        }

        ImGui::Separator();

        // Freeze Settings
        if (ImGui::TreeNodeEx("Freeze Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            f32 iceCol[3] = { volume->iceColor.x, volume->iceColor.y, volume->iceColor.z };
            if (InspectorUndo::ColorEdit3(m_UndoRedo, "Ice Color", iceCol,
                    [volume](f32 r, f32 g, f32 b) { volume->iceColor = Math::Vector3(r, g, b); })) {
                volume->iceColor = Math::Vector3(iceCol[0], iceCol[1], iceCol[2]);
            }
            InspectorUndo::SliderFloat(m_UndoRedo, "Ice Opacity", &volume->iceOpacity, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Freeze Rate", &volume->freezeRate, 0.01f, 0.01f, 5.0f, "%.2f");
            InspectorUndo::DragFloat(m_UndoRedo, "Thaw Rate", &volume->thawRate, 0.01f, 0.01f, 5.0f, "%.2f");

            // Read-only freeze progress indicator
            ImGui::Spacing();
            ImGui::ProgressBar(volume->freezeProgress, ImVec2(-1, 0),
                volume->isFrozen ? "Frozen" : (volume->freezeProgress > 0.01f ? "Freezing..." : "Liquid"));
            ImGui::TreePop();
        }

        // Regenerate mesh if extents or water type changed
        bool needsRegen = (oldExtents.x != volume->halfExtents.x ||
                          oldExtents.y != volume->halfExtents.y ||
                          oldExtents.z != volume->halfExtents.z ||
                          oldWaterType != volume->waterType);
        if (needsRegen) {
            volume->meshCreated = false;
            RemoveComponentWithUndo<ECS::MeshComponent>(entity, "mesh", "Mesh");
            RemoveComponentWithUndo<ECS::MaterialComponent>(entity, "material", "Material");
            if (m_RenderSystem) {
                m_RenderSystem->OnEntityRemoved(entity);
            }
        }

        // Info
        ImGui::Spacing();
        ImGui::TextDisabled("Water surface is at entity's Y position");
        ImGui::TextDisabled("Half Extents define the area and depth");
    }
}

void EditorLayer::DrawWater3DComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("[W] Water 3D", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("Water3DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::Water3DComponent>(entity, "water3D", "Water 3D");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* w = m_World->GetComponent<ECS::Water3DComponent>(entity);
        if (!w) return;
        DrawComponentHelp("water3D", m_World, entity);

        auto oldW = w->settings.width;
        auto oldD = w->settings.depth;
        auto oldT = w->settings.tileSize;

        InspectorUndo::DragFloat(m_UndoRedo, "Width", &w->settings.width, 1.0f, 1.0f, 1000.0f);
        if (w->settings.width < 0.01f) w->settings.width = 0.01f;
        InspectorUndo::DragFloat(m_UndoRedo, "Depth", &w->settings.depth, 1.0f, 1.0f, 1000.0f);
        if (w->settings.depth < 0.01f) w->settings.depth = 0.01f;
        InspectorUndo::DragFloat(m_UndoRedo, "Tile Size", &w->settings.tileSize, 0.1f, 0.5f, 20.0f);
        if (w->settings.tileSize < 0.01f) w->settings.tileSize = 0.01f;

        const char* styleNames[] = {"Flat", "Animated", "VertexWave", "Reflective", "Refractive"};
        int style = static_cast<int>(w->settings.style);
        if (InspectorUndo::Combo(m_UndoRedo, "Style", &style, styleNames, 5))
            w->settings.style = static_cast<Effects::WaterStyle>(style);

        // Reflection controls: Reflective mirrors the scene; Refractive adds a
        // fresnel-split refraction shimmer on top (grazing = reflect, top-down = refract).
        if (w->settings.style == Effects::WaterStyle::Reflective ||
            w->settings.style == Effects::WaterStyle::Refractive) {
            InspectorUndo::SliderFloat(m_UndoRedo, "Reflection Strength", &w->settings.reflectionStrength, 0.0f, 1.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much of the mirrored scene shows in the water.\nHand-crafted mirror geometry, no screen-space tricks.");
            if (w->settings.style == Effects::WaterStyle::Refractive) {
                InspectorUndo::DragFloat(m_UndoRedo, "Fresnel Power", &w->settings.fresnelPower, 0.05f, 1.0f, 10.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reflection/refraction split.\nHigher = reflection only at grazing angles,\nrefracted deep colour when looking straight down.");
            }
        }

        ImGui::Separator();
        f32 sc[3] = {w->settings.shallowColor.x, w->settings.shallowColor.y, w->settings.shallowColor.z};
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Shallow Color", sc,
                [w](f32 r, f32 g, f32 b) { w->settings.shallowColor = Math::Vector3(r, g, b); }))
            w->settings.shallowColor = Math::Vector3(sc[0], sc[1], sc[2]);
        f32 dc[3] = {w->settings.deepColor.x, w->settings.deepColor.y, w->settings.deepColor.z};
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Deep Color", dc,
                [w](f32 r, f32 g, f32 b) { w->settings.deepColor = Math::Vector3(r, g, b); }))
            w->settings.deepColor = Math::Vector3(dc[0], dc[1], dc[2]);
        InspectorUndo::SliderFloat(m_UndoRedo, "Opacity", &w->settings.opacity, 0.0f, 1.0f);

        ImGui::Separator();
        InspectorUndo::DragFloat(m_UndoRedo, "Wave Speed", &w->settings.waveSpeed, 0.1f, 0.0f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Wave Height", &w->settings.waveHeight, 0.01f, 0.0f, 5.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Wave Frequency", &w->settings.waveFrequency, 0.01f, 0.01f, 5.0f);
        if (w->settings.style == Effects::WaterStyle::VertexWave ||
            w->settings.style == Effects::WaterStyle::Reflective ||
            w->settings.style == Effects::WaterStyle::Refractive) {
            InspectorUndo::Checkbox(m_UndoRedo, "Gerstner Waves", &w->settings.gerstnerWaves);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Trochoidal waves: vertices also move horizontally toward crests,\n"
                "giving sharp peaks and flat troughs (open-ocean look) instead of\n"
                "rounded sine swells. Uses the same speed/height/frequency.");
            if (w->settings.gerstnerWaves) {
                InspectorUndo::SliderFloat(m_UndoRedo, "Wave Steepness", &w->settings.waveSteepness, 0.0f, 1.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                    "0 = plain sine look, 1 = sharpest crests the math allows\nwithout the surface folding over itself.");
            }
        }

        ImGui::Separator();
        InspectorUndo::Checkbox(m_UndoRedo, "Enable Foam", &w->settings.enableFoam);
        if (w->settings.enableFoam) {
            InspectorUndo::SliderFloat(m_UndoRedo, "Foam Threshold", &w->settings.foamThreshold, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Foam Scale", &w->settings.foamScale, 0.1f, 0.1f, 20.0f);
        }

        ImGui::Separator();
        InspectorUndo::Checkbox(m_UndoRedo, "Enable Buoyancy", &w->settings.enableBuoyancy);
        if (w->settings.enableBuoyancy) {
            InspectorUndo::DragFloat(m_UndoRedo, "Buoyancy Strength", &w->settings.buoyancyStrength, 0.05f, 0.0f, 6.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("How hard the water pushes bodies up. 1 = neutral, >1 floats.");
            InspectorUndo::DragFloat(m_UndoRedo, "Buoyancy Drag", &w->settings.buoyancyDrag, 0.05f, 0.0f, 10.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Water resistance — damps bobbing and drift.");
        }

        // Force mesh rebuild if size/tessellation changed
        if (w->settings.width != oldW || w->settings.depth != oldD || w->settings.tileSize != oldT) {
            w->meshCreated = false;
            if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
                m_World->RemoveComponent<ECS::MeshComponent>(entity);
            }
            if (m_World->HasComponent<ECS::MaterialComponent>(entity)) {
                m_World->RemoveComponent<ECS::MaterialComponent>(entity);
            }
            if (m_RenderSystem) {
                m_RenderSystem->OnEntityRemoved(entity);
            }
        }
    }
}

void EditorLayer::DrawGrassVolumeComponent(ECS::Entity entity) {
    bool gvOpen = UI::SectionHeader("Grass Volume", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("GrassVolumeCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::GrassVolumeComponent>(entity, "grassVolume", "Grass Volume");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (gvOpen) {
        ECS::GrassVolumeComponent* grass = m_World->GetComponent<ECS::GrassVolumeComponent>(entity);
        if (!grass) return;
        DrawComponentHelp("grassVolume", m_World, entity);

        f32 extents[3] = { grass->halfExtents.x, grass->halfExtents.y, grass->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents", extents,
                [grass](f32 x, f32 y, f32 z) { grass->halfExtents = Math::Vector3(x, y, z); },
                0.5f, 0.1f, 500.0f)) {
            grass->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }

        int density = static_cast<int>(grass->density);
        if (InspectorUndo::DragInt(m_UndoRedo, "Density", &density, 100, 100, 50000)) {
            grass->density = static_cast<u32>(density);
        }

        ImGui::Separator();

        // Blade geometry
        InspectorUndo::DragFloat(m_UndoRedo, "Blade Height", &grass->bladeHeight, 0.01f, 0.01f, 2.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Height Variance", &grass->bladeHeightVariance, 0.01f, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Blade Width", &grass->bladeWidth, 0.005f, 0.005f, 0.5f);

        ImGui::Separator();

        // Colors
        f32 baseCol[3] = { grass->baseColor.x, grass->baseColor.y, grass->baseColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Base Color", baseCol,
                [grass](f32 r, f32 g, f32 b) { grass->baseColor = Math::Vector3(r, g, b); })) {
            grass->baseColor = Math::Vector3(baseCol[0], baseCol[1], baseCol[2]);
        }
        f32 tipCol[3] = { grass->tipColor.x, grass->tipColor.y, grass->tipColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Tip Color", tipCol,
                [grass](f32 r, f32 g, f32 b) { grass->tipColor = Math::Vector3(r, g, b); })) {
            grass->tipColor = Math::Vector3(tipCol[0], tipCol[1], tipCol[2]);
        }

        ImGui::Separator();
        InspectorUndo::DragFloat(m_UndoRedo, "Wind Sway", &grass->windSwayStrength, 0.05f, 0.0f, 5.0f);

        ImGui::Separator();
        ImGui::Text("Custom Texture");
        if (!grass->customAssetPath.empty()) {
            size_t lastSlash = grass->customAssetPath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ? grass->customAssetPath.substr(lastSlash + 1) : grass->customAssetPath;
            ImGui::Text("Texture: %s", filename.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X##GrassAsset")) {
                grass->customAssetPath.clear();
                grass->cachedTexIndex = -2;
            }
        } else {
            if (ImGui::Button("Browse Custom Texture##Grass")) {
                std::string path = FileDialog::OpenFile("Custom Grass Texture", {{ "Images", "*.png;*.jpg;*.jpeg;*.bmp;*.tga" }});
                if (!path.empty()) {
                    grass->customAssetPath = path;
                    grass->cachedTexIndex = -2;
                }
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Grass sits on the XZ plane at entity's Y position");
    }
}

void EditorLayer::DrawShrubVolumeComponent(ECS::Entity entity) {
    bool svOpen = UI::SectionHeader("Shrub Volume", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ShrubVolumeCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ShrubVolumeComponent>(entity, "shrubVolume", "Shrub Volume");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (svOpen) {
        ECS::ShrubVolumeComponent* shrub = m_World->GetComponent<ECS::ShrubVolumeComponent>(entity);
        if (!shrub) return;
        DrawComponentHelp("shrubVolume", m_World, entity);

        f32 extents[3] = { shrub->halfExtents.x, shrub->halfExtents.y, shrub->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents", extents,
                [shrub](f32 x, f32 y, f32 z) { shrub->halfExtents = Math::Vector3(x, y, z); },
                0.5f, 0.1f, 500.0f)) {
            shrub->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }

        int density = static_cast<int>(shrub->density);
        if (InspectorUndo::DragInt(m_UndoRedo, "Density", &density, 50, 10, 10000)) {
            shrub->density = static_cast<u32>(density);
        }

        ImGui::Separator();

        InspectorUndo::DragFloat(m_UndoRedo, "Shrub Height", &shrub->shrubHeight, 0.01f, 0.05f, 3.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Height Variance", &shrub->heightVariance, 0.01f, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Width", &shrub->width, 0.01f, 0.05f, 2.0f);

        ImGui::Separator();

        f32 baseCol[3] = { shrub->baseColor.x, shrub->baseColor.y, shrub->baseColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Base Color", baseCol,
                [shrub](f32 r, f32 g, f32 b) { shrub->baseColor = Math::Vector3(r, g, b); })) {
            shrub->baseColor = Math::Vector3(baseCol[0], baseCol[1], baseCol[2]);
        }
        f32 tipCol[3] = { shrub->tipColor.x, shrub->tipColor.y, shrub->tipColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Tip Color", tipCol,
                [shrub](f32 r, f32 g, f32 b) { shrub->tipColor = Math::Vector3(r, g, b); })) {
            shrub->tipColor = Math::Vector3(tipCol[0], tipCol[1], tipCol[2]);
        }

        ImGui::Separator();
        InspectorUndo::DragFloat(m_UndoRedo, "Wind Sway", &shrub->windSwayStrength, 0.05f, 0.0f, 5.0f);
        i32 shrubQuads = static_cast<i32>(shrub->quadsPerShrub);
        if (ImGui::DragInt("Quads Per Shrub", &shrubQuads, 1, 1, 3)) {
            shrub->quadsPerShrub = static_cast<u32>(shrubQuads < 1 ? 1 : (shrubQuads > 3 ? 3 : shrubQuads));
        }
        ImGui::SetItemTooltip("Crossed quads per shrub: 1 = flat card, 3 = full star");

        ImGui::Separator();
        ImGui::Text("Custom Texture");
        if (!shrub->customAssetPath.empty()) {
            size_t lastSlash = shrub->customAssetPath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ? shrub->customAssetPath.substr(lastSlash + 1) : shrub->customAssetPath;
            ImGui::Text("Texture: %s", filename.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X##ShrubAsset")) {
                shrub->customAssetPath.clear();
                shrub->cachedTexIndex = -2;
            }
        } else {
            if (ImGui::Button("Browse Custom Texture##Shrub")) {
                std::string path = FileDialog::OpenFile("Custom Shrub Texture", {{ "Images", "*.png;*.jpg;*.jpeg;*.bmp;*.tga" }});
                if (!path.empty()) {
                    shrub->customAssetPath = path;
                    shrub->cachedTexIndex = -2;
                }
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Shrubs sit on the XZ plane at entity's Y position");
    }
}

void EditorLayer::DrawTreeVolumeComponent(ECS::Entity entity) {
    bool tvOpen = UI::SectionHeader("Tree Volume", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TreeVolumeCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::TreeVolumeComponent>(entity, "treeVolume", "Tree Volume");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (tvOpen) {
        ECS::TreeVolumeComponent* tree = m_World->GetComponent<ECS::TreeVolumeComponent>(entity);
        if (!tree) return;
        DrawComponentHelp("treeVolume", m_World, entity);

        f32 extents[3] = { tree->halfExtents.x, tree->halfExtents.y, tree->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents", extents,
                [tree](f32 x, f32 y, f32 z) { tree->halfExtents = Math::Vector3(x, y, z); },
                0.5f, 0.1f, 500.0f)) {
            tree->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }

        int density = static_cast<int>(tree->density);
        if (InspectorUndo::DragInt(m_UndoRedo, "Density", &density, 10, 1, 5000)) {
            tree->density = static_cast<u32>(density);
        }

        const char* treeTypes[] = { "Deciduous", "Evergreen" };
        int treeTypeIdx = static_cast<int>(tree->treeType);
        if (InspectorUndo::Combo(m_UndoRedo, "Tree Type", &treeTypeIdx, treeTypes, 2)) {
            tree->treeType = static_cast<ECS::TreeType>(treeTypeIdx);
        }

        ImGui::Separator();
        ImGui::Text("Trunk");
        InspectorUndo::DragFloat(m_UndoRedo, "Trunk Height", &tree->trunkHeight, 0.05f, 0.1f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Trunk Width", &tree->trunkWidth, 0.01f, 0.02f, 1.0f);
        f32 trunkCol[3] = { tree->trunkColor.x, tree->trunkColor.y, tree->trunkColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Trunk Color", trunkCol,
                [tree](f32 r, f32 g, f32 b) { tree->trunkColor = Math::Vector3(r, g, b); })) {
            tree->trunkColor = Math::Vector3(trunkCol[0], trunkCol[1], trunkCol[2]);
        }

        ImGui::Separator();
        ImGui::Text("Canopy");
        InspectorUndo::DragFloat(m_UndoRedo, "Canopy Radius", &tree->canopyRadius, 0.05f, 0.1f, 5.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Canopy Offset", &tree->canopyOffset, 0.05f, 0.0f, 10.0f);
        f32 canopyBaseCol[3] = { tree->canopyBaseColor.x, tree->canopyBaseColor.y, tree->canopyBaseColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Canopy Base", canopyBaseCol,
                [tree](f32 r, f32 g, f32 b) { tree->canopyBaseColor = Math::Vector3(r, g, b); })) {
            tree->canopyBaseColor = Math::Vector3(canopyBaseCol[0], canopyBaseCol[1], canopyBaseCol[2]);
        }
        f32 canopyTipCol[3] = { tree->canopyTipColor.x, tree->canopyTipColor.y, tree->canopyTipColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Canopy Tip", canopyTipCol,
                [tree](f32 r, f32 g, f32 b) { tree->canopyTipColor = Math::Vector3(r, g, b); })) {
            tree->canopyTipColor = Math::Vector3(canopyTipCol[0], canopyTipCol[1], canopyTipCol[2]);
        }

        if (tree->treeType == ECS::TreeType::Deciduous) {
            ImGui::Separator();
            ImGui::Text("Seasonal Colors");
            f32 springCol[3] = { tree->springCanopyColor.x, tree->springCanopyColor.y, tree->springCanopyColor.z };
            if (InspectorUndo::ColorEdit3(m_UndoRedo, "Spring", springCol,
                    [tree](f32 r, f32 g, f32 b) { tree->springCanopyColor = Math::Vector3(r, g, b); })) {
                tree->springCanopyColor = Math::Vector3(springCol[0], springCol[1], springCol[2]);
            }
            f32 summerCol[3] = { tree->summerCanopyColor.x, tree->summerCanopyColor.y, tree->summerCanopyColor.z };
            if (InspectorUndo::ColorEdit3(m_UndoRedo, "Summer", summerCol,
                    [tree](f32 r, f32 g, f32 b) { tree->summerCanopyColor = Math::Vector3(r, g, b); })) {
                tree->summerCanopyColor = Math::Vector3(summerCol[0], summerCol[1], summerCol[2]);
            }
            f32 fallCol[3] = { tree->fallCanopyColor.x, tree->fallCanopyColor.y, tree->fallCanopyColor.z };
            if (InspectorUndo::ColorEdit3(m_UndoRedo, "Fall", fallCol,
                    [tree](f32 r, f32 g, f32 b) { tree->fallCanopyColor = Math::Vector3(r, g, b); })) {
                tree->fallCanopyColor = Math::Vector3(fallCol[0], fallCol[1], fallCol[2]);
            }
            ImGui::TextDisabled("Winter: bare branches (no canopy)");
        }

        ImGui::Separator();
        ImGui::Text("Height Variance");
        ImGui::DragFloat("Min Scale", &tree->minHeightScale, 0.05f, 0.1f, 2.0f);
        ImGui::DragFloat("Max Scale", &tree->maxHeightScale, 0.05f, 0.1f, 3.0f);
        if (tree->minHeightScale > tree->maxHeightScale) tree->maxHeightScale = tree->minHeightScale;
        i32 treeCanopyQuads = static_cast<i32>(tree->canopyQuads);
        if (ImGui::DragInt("Canopy Quads", &treeCanopyQuads, 1, 0, 3)) {
            tree->canopyQuads = static_cast<u32>(treeCanopyQuads < 0 ? 0 : (treeCanopyQuads > 3 ? 3 : treeCanopyQuads));
        }
        ImGui::SetItemTooltip("Crossed canopy quads: 0 = bare trunk, 3 = full crown");

        ImGui::Separator();
        ImGui::DragFloat("Wind Sway", &tree->windSwayStrength, 0.05f, 0.0f, 5.0f);

        ImGui::Separator();
        ImGui::Text("Textures");
        // Bark texture
        if (!tree->barkTexturePath.empty()) {
            size_t lastSlash = tree->barkTexturePath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ? tree->barkTexturePath.substr(lastSlash + 1) : tree->barkTexturePath;
            ImGui::Text("Bark: %s", filename.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X##BarkTex")) {
                tree->barkTexturePath.clear();
                tree->cachedBarkTexIndex = -2;
            }
        } else {
            if (ImGui::Button("Load Bark Texture")) {
                std::string path = FileDialog::OpenFile("Bark Texture", {{ "Images", "*.png;*.jpg;*.jpeg;*.bmp;*.tga" }});
                if (!path.empty()) {
                    tree->barkTexturePath = path;
                    tree->cachedBarkTexIndex = -2;
                }
            }
        }
        // Canopy texture
        if (!tree->canopyTexturePath.empty()) {
            size_t lastSlash = tree->canopyTexturePath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ? tree->canopyTexturePath.substr(lastSlash + 1) : tree->canopyTexturePath;
            ImGui::Text("Canopy: %s", filename.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X##CanopyTex")) {
                tree->canopyTexturePath.clear();
                tree->cachedCanopyTexIndex = -2;
            }
        } else {
            if (ImGui::Button("Load Canopy Texture")) {
                std::string path = FileDialog::OpenFile("Canopy Texture", {{ "Images", "*.png;*.jpg;*.jpeg;*.bmp;*.tga" }});
                if (!path.empty()) {
                    tree->canopyTexturePath = path;
                    tree->cachedCanopyTexIndex = -2;
                }
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Trees sit on the XZ plane at entity's Y position");
    }
}

void EditorLayer::DrawTerrainComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("3D Terrain", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TerrainCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::TerrainComponent>(entity, "terrain", "Terrain");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        ECS::TerrainComponent* terrain = m_World->GetComponent<ECS::TerrainComponent>(entity);
        if (!terrain) return;
        DrawComponentHelp("terrain", m_World, entity);

        int w = static_cast<int>(terrain->gridWidth);
        int h = static_cast<int>(terrain->gridHeight);
        bool resized = false;
        resized |= ImGui::DragInt("Grid Width", &w, 1, 4, 512);
        resized |= ImGui::DragInt("Grid Height", &h, 1, 4, 512);
        if (resized) {
            terrain->gridWidth = static_cast<u32>(w);
            terrain->gridHeight = static_cast<u32>(h);
            terrain->InitializeFlat(0.0f);
        }
        ImGui::DragFloat("Cell Size", &terrain->cellSize, 0.1f, 0.1f, 10.0f);
        ImGui::DragFloat("Max Height", &terrain->maxHeight, 1.0f, 1.0f, 200.0f);

        if (terrain->heightmap.empty()) {
            if (ImGui::Button("Initialize Flat")) {
                terrain->InitializeFlat(0.0f);
            }
        }

        ImGui::Separator();
        ImGui::Text("Terrain Brush");
        ImGui::Checkbox("Edit Mode", &m_TerrainEditMode);

        if (m_TerrainEditMode) {
            const char* brushModes[] = { "Raise", "Lower", "Flatten", "Smooth", "Paint" };
            int brushIdx = static_cast<int>(m_TerrainBrush.mode);
            if (ImGui::Combo("Brush Mode", &brushIdx, brushModes, 5)) {
                m_TerrainBrush.mode = static_cast<TerrainBrushMode>(brushIdx);
            }
            ImGui::DragFloat("Radius", &m_TerrainBrush.radius, 0.1f, 0.5f, 50.0f);
            ImGui::DragFloat("Strength", &m_TerrainBrush.strength, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Falloff", &m_TerrainBrush.falloff, 0.01f, 0.0f, 1.0f);

            if (m_TerrainBrush.mode == TerrainBrushMode::Flatten) {
                ImGui::DragFloat("Flatten Height", &m_TerrainBrush.flattenHeight, 0.1f, 0.0f, terrain->maxHeight);
            }
            if (m_TerrainBrush.mode == TerrainBrushMode::Paint) {
                int layer = static_cast<int>(m_TerrainBrush.paintLayer);
                if (ImGui::DragInt("Paint Layer", &layer, 1, 0, 3)) {
                    m_TerrainBrush.paintLayer = static_cast<u32>(layer);
                }
            }

            // Brush cursor feedback
            if (m_BrushHitValid) {
                ImGui::Separator();
                ImGui::Text("Brush: (%.1f, %.1f, %.1f)",
                            m_BrushHitPoint.x, m_BrushHitPoint.y, m_BrushHitPoint.z);
            }
        }

        ImGui::Separator();
        ImGui::Text("Texture Layers");
        for (int i = 0; i < 4; ++i) {
            ImGui::PushID(i);
            char label[32];
            std::snprintf(label, sizeof(label), "Layer %d", i);
            if (ImGui::TreeNode(label)) {
                if (!terrain->layers[i].texturePath.empty()) {
                    ImGui::Text("Texture: %s", terrain->layers[i].texturePath.c_str());
                    if (ImGui::SmallButton("Clear")) {
                        terrain->layers[i].texturePath.clear();
                    }
                } else {
                    if (ImGui::Button("Load Texture")) {
                        std::string path = FileDialog::OpenFile("Texture", {{ "Images", "*.png;*.jpg;*.jpeg;*.bmp;*.tga" }});
                        if (!path.empty()) terrain->layers[i].texturePath = path;
                    }
                }
                ImGui::DragFloat("Tile Scale", &terrain->layers[i].tileScale, 0.1f, 0.1f, 100.0f);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}

void EditorLayer::DrawTerrain2DComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("2D Terrain", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("Terrain2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::Terrain2DComponent>(entity, "terrain2d", "Terrain 2D");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        ECS::Terrain2DComponent* terrain = m_World->GetComponent<ECS::Terrain2DComponent>(entity);
        if (!terrain) return;
        DrawComponentHelp("terrain2d", m_World, entity);

        ImGui::DragFloat("Depth", &terrain->depth, 0.1f, 0.1f, 50.0f);
        ImGui::DragFloat("UV Scale", &terrain->uvScale, 0.01f, 0.01f, 10.0f);

        // Surface colliders: one thin static box per control-point segment,
        // spawned as child-less collider entities (same pattern as tree trunks)
        if (ImGui::Button("Generate Surface Colliders")) {
            auto* terrainTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
            Math::Vector3 base = terrainTransform ? terrainTransform->position : Math::Vector3(0.0f);
            u32 made = 0;
            for (usize i = 0; i + 1 < terrain->controlPoints.size(); ++i) {
                Math::Vector2 a = terrain->controlPoints[i];
                Math::Vector2 b = terrain->controlPoints[i + 1];
                Math::Vector2 mid = (a + b) * 0.5f;
                f32 dx = b.x - a.x, dy = b.y - a.y;
                f32 len = std::sqrt(dx * dx + dy * dy);
                if (len < 0.001f) continue;
                f32 angleDeg = std::atan2(dy, dx) * 57.29578f;

                ECS::Entity col = m_World->CreateEntity();
                ECS::NameComponent nameComp;
                nameComp.name = "TerrainCollider_" + std::to_string(i);
                m_World->AddComponent<ECS::NameComponent>(col, nameComp);

                ECS::TransformComponent xform;
                xform.position = base + Math::Vector3(mid.x, mid.y, 0.0f);
                xform.rotation = Math::Quaternion::FromEuler(Math::Vector3(0.0f, 0.0f, angleDeg));
                m_World->AddComponent<ECS::TransformComponent>(col, xform);

                ECS::BoxColliderComponent box;
                box.size = Math::Vector3(len, 0.2f, 1.0f);
                box.isTrigger = false;
                m_World->AddComponent<ECS::BoxColliderComponent>(col, box);
                ++made;
            }
            ENJIN_LOG_INFO(Editor, "Generated %u terrain surface colliders", made);
        }
        ImGui::SetItemTooltip("Creates a thin static box collider along each surface segment");

        ImGui::Separator();
        ImGui::Checkbox("Edit Mode (Drag Points)", &m_TerrainEditMode);
        if (m_TerrainEditMode && m_BrushHitValid) {
            ImGui::Text("Cursor: (%.1f, %.1f)", m_BrushHitPoint.x, m_BrushHitPoint.y);
            if (m_Dragging2DPoint >= 0)
                ImGui::Text("Dragging point %d", m_Dragging2DPoint);
        }

        ImGui::Separator();
        ImGui::Text("Control Points (%zu)", terrain->controlPoints.size());

        if (ImGui::Button("Add Point")) {
            f32 x = terrain->controlPoints.empty() ? 0.0f : terrain->controlPoints.back().x + 2.0f;
            terrain->AddPoint(Math::Vector2(x, 0.0f));
        }

        for (usize i = 0; i < terrain->controlPoints.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            f32 pt[2] = { terrain->controlPoints[i].x, terrain->controlPoints[i].y };
            char label[32];
            std::snprintf(label, sizeof(label), "Point %zu", i);
            if (ImGui::DragFloat2(label, pt, 0.1f)) {
                terrain->controlPoints[i] = Math::Vector2(pt[0], pt[1]);
                terrain->meshDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X") && terrain->controlPoints.size() > 2) {
                terrain->controlPoints.erase(terrain->controlPoints.begin() + static_cast<std::ptrdiff_t>(i));
                terrain->meshDirty = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
    }
}

void EditorLayer::DrawViewmodelComponent(ECS::Entity entity) {
    bool vmOpen = UI::SectionHeader("Viewmodel (First Person)", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ViewmodelCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ViewmodelComponent>(entity, "viewmodel", "Viewmodel");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (vmOpen) {
        ECS::ViewmodelComponent* vm = m_World->GetComponent<ECS::ViewmodelComponent>(entity);
        if (!vm) return;
        DrawComponentHelp("viewmodel", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &vm->enabled);

        ImGui::Spacing();
        ImGui::TextDisabled("Renders in front of world geometry, never clips walls,");
        ImGui::TextDisabled("casts no shadows. Parent this entity to the camera.");
    }
}

void EditorLayer::DrawVegetationComponent(ECS::Entity entity) {
    bool vegOpen = UI::SectionHeader("Vegetation", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("VegetationCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::VegetationComponent>(entity, "vegetation", "Vegetation");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (vegOpen) {
        ECS::VegetationComponent* veg = m_World->GetComponent<ECS::VegetationComponent>(entity);
        if (!veg) return;
        DrawComponentHelp("vegetation", m_World, entity);

        InspectorUndo::DragFloat(m_UndoRedo, "Sway Strength", &veg->swayStrength, 0.05f, 0.0f, 5.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Sway Frequency", &veg->swayFrequency, 0.05f, 0.0f, 5.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Use Vertex Color Weight", &veg->useVertexColorWeight);

        ImGui::Spacing();
        ImGui::TextDisabled("Red vertex color channel = sway weight");
        ImGui::TextDisabled("Trunk (red=0) stays still, leaves (red=1) sway");
    }
}

void EditorLayer::DrawCameraTriggerComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Camera Trigger", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* trigger = m_World->GetComponent<ECS::CameraTriggerComponent>(entity);
        if (!trigger) return;
        DrawComponentHelp("cameraTrigger", m_World, entity);

        // Half-extents
        f32 halfExt[3] = { trigger->halfExtents.x, trigger->halfExtents.y, trigger->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents", halfExt,
                [trigger](f32 x, f32 y, f32 z) { trigger->halfExtents = Math::Vector3(x, y, z); },
                0.5f, 0.1f, 500.0f)) {
            trigger->halfExtents = Math::Vector3(halfExt[0], halfExt[1], halfExt[2]);
        }

        // Priority
        InspectorUndo::DragInt(m_UndoRedo, "Priority", &trigger->priority, 1, -100, 100);

        // Blend time
        InspectorUndo::DragFloat(m_UndoRedo, "Blend Time", &trigger->blendTime, 0.05f, 0.0f, 5.0f, "%.2f s");

        // Target camera dropdown
        std::vector<ECS::Entity> cameraEntities;
        if (m_World) {
            const auto& camEnts = m_World->GetEntitiesWithComponent<ECS::CameraComponent>();
            cameraEntities.assign(camEnts.begin(), camEnts.end());
        }

        std::string currentName = "(None)";
        if (trigger->targetCamera != ECS::INVALID_ENTITY) {
            if (m_World->HasComponent<ECS::NameComponent>(trigger->targetCamera)) {
                currentName = m_World->GetComponent<ECS::NameComponent>(trigger->targetCamera)->name;
            } else {
                currentName = "Camera (Entity " + std::to_string(trigger->targetCamera) + ")";
            }
        }

        ImGui::SetNextItemWidth(200 * ImGui::GetIO().FontGlobalScale);
        if (ImGui::BeginCombo("Target Camera", currentName.c_str())) {
            // None option
            if (ImGui::Selectable("(None)", trigger->targetCamera == ECS::INVALID_ENTITY)) {
                trigger->targetCamera = ECS::INVALID_ENTITY;
            }
            for (ECS::Entity camEntity : cameraEntities) {
                std::string name;
                if (m_World->HasComponent<ECS::NameComponent>(camEntity)) {
                    name = m_World->GetComponent<ECS::NameComponent>(camEntity)->name;
                } else {
                    name = "Camera (Entity " + std::to_string(camEntity) + ")";
                }
                bool isSelected = (camEntity == trigger->targetCamera);
                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    trigger->targetCamera = camEntity;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Zone activates target camera when player enters");

        // Remove component button
        if (ImGui::Button("Remove##CameraTrigger")) {
            RemoveComponentWithUndo<ECS::CameraTriggerComponent>(entity, "cameraTrigger", "Camera Trigger");
        }
    }
}

void EditorLayer::DrawTemperatureZoneComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Temperature Zone", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* tempZone = m_World->GetComponent<ECS::TemperatureZoneComponent>(entity);
        if (!tempZone) return;
        DrawComponentHelp("temperatureZone", m_World, entity);

        // Half-extents
        f32 halfExt[3] = { tempZone->halfExtents.x, tempZone->halfExtents.y, tempZone->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents##TempZone", halfExt,
                [tempZone](f32 x, f32 y, f32 z) { tempZone->halfExtents = Math::Vector3(x, y, z); },
                0.5f, 0.1f, 500.0f)) {
            tempZone->halfExtents = Math::Vector3(halfExt[0], halfExt[1], halfExt[2]);
        }

        // Temperature slider with color-coded display
        InspectorUndo::DragFloat(m_UndoRedo, "Temperature (C)", &tempZone->temperature, 0.5f, -40.0f, 50.0f, "%.1f");

        // Visual temperature indicator
        if (tempZone->IsFreezing()) {
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Freezing (Snow/Ice)");
        } else if (tempZone->IsNearFreezing()) {
            ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "Near Freezing (Sleet/Mix)");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Warm (Rain)");
        }

        // Priority
        InspectorUndo::DragInt(m_UndoRedo, "Priority##TempZone", &tempZone->priority, 1, -100, 100);

        ImGui::Spacing();
        ImGui::TextDisabled("Affects precipitation type in overlapping weather zones");

        // Remove component
        if (ImGui::Button("Remove##TemperatureZone")) {
            RemoveComponentWithUndo<ECS::TemperatureZoneComponent>(entity, "temperatureZone", "Temperature Zone");
        }
    }
}

void EditorLayer::DrawGravityZoneComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Gravity Zone", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* zone = m_World->GetComponent<ECS::GravityZoneComponent>(entity);
        if (!zone) return;
        DrawComponentHelp("gravityZone", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Active##GravZone", &zone->isActive);

        // Shape selector
        const char* shapes[] = { "Box", "Sphere" };
        int shapeIdx = static_cast<int>(zone->shape);
        if (InspectorUndo::Combo(m_UndoRedo, "Shape##GravZone", &shapeIdx, shapes, 2)) {
            zone->shape = static_cast<ECS::GravityZoneShape>(shapeIdx);
        }

        if (zone->shape == ECS::GravityZoneShape::Sphere) {
            // Sphere: single radius
            f32 radius = zone->halfExtents.x;
            if (InspectorUndo::DragFloat(m_UndoRedo, "Radius##GravZone", &radius, 0.5f, 0.1f, 500.0f)) {
                zone->halfExtents = Math::Vector3(radius, radius, radius);
            }
        } else {
            // Box: half-extents
            f32 halfExt[3] = { zone->halfExtents.x, zone->halfExtents.y, zone->halfExtents.z };
            if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents##GravZone", halfExt,
                    [zone](f32 x, f32 y, f32 z) { zone->halfExtents = Math::Vector3(x, y, z); },
                    0.5f, 0.1f, 500.0f)) {
                zone->halfExtents = Math::Vector3(halfExt[0], halfExt[1], halfExt[2]);
            }
        }

        // Mode selector
        const char* modes[] = { "Directional", "Point (Planetary)" };
        int modeIdx = static_cast<int>(zone->mode);
        if (InspectorUndo::Combo(m_UndoRedo, "Mode##GravZone", &modeIdx, modes, 2)) {
            zone->mode = static_cast<ECS::GravityZoneMode>(modeIdx);
        }

        if (zone->mode == ECS::GravityZoneMode::Directional) {
            // Gravity direction
            f32 dir[3] = { zone->gravityDirection.x, zone->gravityDirection.y, zone->gravityDirection.z };
            if (InspectorUndo::DragFloat3(m_UndoRedo, "Direction##GravZone", dir,
                    [zone](f32 x, f32 y, f32 z) { zone->gravityDirection = Math::Vector3(x, y, z); },
                    0.01f, -1.0f, 1.0f)) {
                zone->gravityDirection = Math::Vector3(dir[0], dir[1], dir[2]);
                f32 len = zone->gravityDirection.Length();
                if (len > 0.001f) {
                    zone->gravityDirection = zone->gravityDirection * (1.0f / len);
                }
            }

            // Quick direction presets
            ImGui::Text("Presets:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Down")) { zone->gravityDirection = Math::Vector3(0, -1, 0); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Up")) { zone->gravityDirection = Math::Vector3(0, 1, 0); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Left")) { zone->gravityDirection = Math::Vector3(-1, 0, 0); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Right")) { zone->gravityDirection = Math::Vector3(1, 0, 0); }
        } else {
            ImGui::TextDisabled("Gravity pulls toward this entity's position");
            ImGui::TextDisabled("(Mario Galaxy-style planetary gravity)");

            // For sphere shape + point mode, this is a classic planetary body
            if (zone->shape == ECS::GravityZoneShape::Sphere) {
                ImGui::TextDisabled("Tip: Use Sphere shape for natural planet gravity");
            }
        }

        // Gravity strength
        InspectorUndo::DragFloat(m_UndoRedo, "Strength##GravZone", &zone->gravityStrength, 0.1f, 0.0f, 100.0f, "%.2f m/s^2");
        ImGui::SameLine();
        if (ImGui::SmallButton("Zero-G")) { zone->gravityStrength = 0.0f; }

        // Planet presets (for point mode)
        if (zone->mode == ECS::GravityZoneMode::Point) {
            ImGui::Text("Planet Presets:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Earth##PP")) { zone->gravityStrength = 9.81f; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Moon##PP")) { zone->gravityStrength = 1.62f; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Jupiter##PP")) { zone->gravityStrength = 24.79f; }
        }

        // Priority
        ImGui::DragInt("Priority##GravZone", &zone->priority, 1, -100, 100);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Higher priority zones override lower ones when overlapping");

        // Remove component
        if (ImGui::Button("Remove##GravityZone")) {
            RemoveComponentWithUndo<ECS::GravityZoneComponent>(entity, "gravityZone", "Gravity Zone");
        }
    }
}

void EditorLayer::DrawReflectionProbeComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Reflection Probe", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* probe = m_World->GetComponent<ECS::ReflectionProbeComponent>(entity);
        if (!probe) return;
        DrawComponentHelp("reflectionProbe", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Active##ReflProbe", &probe->isActive);

        // Box extents (min/max offsets from probe center)
        f32 boxMin[3] = { probe->boxMin.x, probe->boxMin.y, probe->boxMin.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Box Min##ReflProbe", boxMin,
                [probe](f32 x, f32 y, f32 z) { probe->boxMin = Math::Vector3(x, y, z); },
                0.5f, -500.0f, 0.0f)) {
            probe->boxMin = Math::Vector3(boxMin[0], boxMin[1], boxMin[2]);
        }

        f32 boxMax[3] = { probe->boxMax.x, probe->boxMax.y, probe->boxMax.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Box Max##ReflProbe", boxMax,
                [probe](f32 x, f32 y, f32 z) { probe->boxMax = Math::Vector3(x, y, z); },
                0.5f, 0.0f, 500.0f)) {
            probe->boxMax = Math::Vector3(boxMax[0], boxMax[1], boxMax[2]);
        }

        // Quick size presets
        ImGui::Text("Presets:");
        ImGui::SameLine();
        if (ImGui::SmallButton("Small Room")) {
            probe->boxMin = Math::Vector3(-3.0f, -1.5f, -3.0f);
            probe->boxMax = Math::Vector3(3.0f, 1.5f, 3.0f);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Large Room")) {
            probe->boxMin = Math::Vector3(-10.0f, -4.0f, -10.0f);
            probe->boxMax = Math::Vector3(10.0f, 4.0f, 10.0f);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Outdoor")) {
            probe->boxMin = Math::Vector3(-50.0f, -10.0f, -50.0f);
            probe->boxMax = Math::Vector3(50.0f, 30.0f, 50.0f);
        }

        ImGui::Separator();

        // Intensity
        InspectorUndo::DragFloat(m_UndoRedo, "Intensity##ReflProbe", &probe->intensity, 0.01f, 0.0f, 2.0f);

        // Blend distance
        InspectorUndo::DragFloat(m_UndoRedo, "Blend Distance##ReflProbe", &probe->blendDistance, 0.1f, 0.0f, 20.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("How far inside the box before reaching full influence (smooth edge falloff)");

        // Priority
        int priority = static_cast<int>(probe->priority);
        if (ImGui::DragInt("Priority##ReflProbe", &priority, 1, 0, 100)) {
            probe->priority = static_cast<u32>(priority);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Higher priority probes override lower ones when overlapping");

        // Resolution for cubemap baking
        ImGui::Separator();
        ImGui::Text("Cubemap Baking");
        const char* resolutions[] = { "128", "256", "512" };
        int resIdx = (probe->resolution == 128) ? 0 : (probe->resolution == 512) ? 2 : 1;
        if (ImGui::Combo("Resolution##ReflProbe", &resIdx, resolutions, 3)) {
            probe->resolution = (resIdx == 0) ? 128 : (resIdx == 2) ? 512 : 256;
        }

        // Bake button — queues a deferred bake (runs between frames)
        if (ImGui::Button("Bake##ReflProbe")) {
            if (m_RenderSystem) {
                auto* probeSystem = m_RenderSystem->GetReflectionProbes();
                if (probeSystem) {
                    probeSystem->RequestBake(static_cast<u64>(entity));
                }
            }
        }
        ImGui::SameLine();
        if (probe->baked && probe->cubemapTextureId >= 0) {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Baked");
        } else if (m_RenderSystem && m_RenderSystem->GetReflectionProbes() &&
                   m_RenderSystem->GetReflectionProbes()->HasPendingBake()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Baking...");
        } else {
            ImGui::TextDisabled("Not baked");
        }

        ImGui::Separator();

        // Remove component
        if (ImGui::Button("Remove##ReflectionProbe")) {
            RemoveComponentWithUndo<ECS::ReflectionProbeComponent>(entity, "reflectionProbe", "Reflection Probe");
        }
    }
}

void EditorLayer::DrawReflectivePlaneComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Reflective Floor", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* plane = m_World->GetComponent<ECS::ReflectivePlaneComponent>(entity);
        if (!plane) return;
        DrawComponentHelp("reflectivePlane", m_World, entity);

        ImGui::TextWrapped("A glassy floor that mirrors the scene above it. The floor's "
                           "MATERIAL must be semi-transparent (alpha mode Blend) for the "
                           "reflection to show through.");
        ImGui::Separator();

        InspectorUndo::Checkbox(m_UndoRedo, "Active##ReflPlane", &plane->active);

        InspectorUndo::DragFloat(m_UndoRedo, "Reflection Strength##ReflPlane",
                                 &plane->reflectionStrength, 0.01f, 0.0f, 1.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("How strongly the mirror shows (0 = off, 1 = full).");

        f32 tint[3] = { plane->tint.x, plane->tint.y, plane->tint.z };
        if (ImGui::ColorEdit3("Tint##ReflPlane", tint)) {
            plane->tint = Math::Vector3(tint[0], tint[1], tint[2]);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Colours the reflection — wet-asphalt blue, gold sheen, murky water.");

        InspectorUndo::DragFloat(m_UndoRedo, "Clip Bias##ReflPlane",
                                 &plane->clipBias, 0.005f, 0.0f, 1.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lifts the mirror plane a hair above the floor so the floor isn't caught in its own reflection.");

        ImGui::Separator();
        if (ImGui::Button("Remove##ReflectivePlane")) {
            RemoveComponentWithUndo<ECS::ReflectivePlaneComponent>(entity, "reflectivePlane", "Reflective Floor");
        }
    }
}

void EditorLayer::DrawFluidVolumeComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Fluid Volume", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* vol = m_World->GetComponent<ECS::FluidVolumeComponent>(entity);
        if (!vol) return;
        DrawComponentHelp("fluidVolume", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Active##FluidVol", &vol->isActive);

        // Half Extents
        f32 halfExt[3] = { vol->halfExtents.x, vol->halfExtents.y, vol->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents##FluidVol", halfExt,
                [vol](f32 x, f32 y, f32 z) { vol->halfExtents = Math::Vector3(x, y, z); },
                0.5f, 0.1f, 500.0f)) {
            vol->halfExtents = Math::Vector3(halfExt[0], halfExt[1], halfExt[2]);
        }

        // Fluid Type
        const char* types[] = { "Water", "Lava", "Gas", "Smoke", "Steam" };
        int typeIdx = static_cast<int>(vol->fluidType);
        if (InspectorUndo::Combo(m_UndoRedo, "Fluid Type##FluidVol", &typeIdx, types, 5)) {
            vol->fluidType = static_cast<ECS::FluidType>(typeIdx);
            vol->ApplyPreset();
        }

        // Dimension
        const char* dims[] = { "2D", "3D" };
        int dimIdx = static_cast<int>(vol->dimension);
        if (InspectorUndo::Combo(m_UndoRedo, "Dimension##FluidVol", &dimIdx, dims, 2)) {
            vol->dimension = static_cast<ECS::FluidDimension>(dimIdx);
            vol->simulationInitialized = false;
        }

        // Grid Size
        i32 gridSize = static_cast<i32>(vol->gridSize);
        i32 maxGrid = vol->dimension == ECS::FluidDimension::Mode3D ? 48 : 128;
        if (InspectorUndo::DragInt(m_UndoRedo, "Grid Size##FluidVol", &gridSize, 1, 8, maxGrid)) {
            vol->gridSize = static_cast<u32>(gridSize);
            vol->simulationInitialized = false;
        }

        ImGui::Separator();
        ImGui::Text("Simulation");
        InspectorUndo::DragFloat(m_UndoRedo, "Viscosity##FluidVol", &vol->viscosity, 0.00001f, 0.0f, 1.0f, "%.6f");
        InspectorUndo::DragFloat(m_UndoRedo, "Diffusion##FluidVol", &vol->diffusion, 0.00001f, 0.0f, 0.1f, "%.6f");
        InspectorUndo::DragFloat(m_UndoRedo, "Dissipation##FluidVol", &vol->dissipation, 0.001f, 0.9f, 1.0f, "%.4f");
        InspectorUndo::DragFloat(m_UndoRedo, "Vel Dissipation##FluidVol", &vol->velocityDissipation, 0.001f, 0.9f, 1.0f, "%.4f");
        i32 iters = vol->solverIterations;
        if (InspectorUndo::DragInt(m_UndoRedo, "Solver Iterations##FluidVol", &iters, 1, 1, 40)) {
            vol->solverIterations = iters;
        }
        InspectorUndo::DragFloat(m_UndoRedo, "Buoyancy##FluidVol", &vol->buoyancy, 0.1f, 0.0f, 10.0f);

        ImGui::Separator();
        ImGui::Text("Rendering");
        f32 col[3] = { vol->fluidColor.x, vol->fluidColor.y, vol->fluidColor.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Color##FluidVol", col,
                [vol](f32 r, f32 g, f32 b) { vol->fluidColor = Math::Vector3(r, g, b); })) {
            vol->fluidColor = Math::Vector3(col[0], col[1], col[2]);
        }
        InspectorUndo::DragFloat(m_UndoRedo, "Opacity##FluidVol", &vol->opacity, 0.01f, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Density Threshold##FluidVol", &vol->densityThreshold, 0.001f, 0.0f, 1.0f, "%.4f");
        InspectorUndo::Checkbox(m_UndoRedo, "Render Enabled##FluidVol", &vol->renderEnabled);

        ImGui::Separator();
        ImGui::Text("Source");
        InspectorUndo::DragFloat(m_UndoRedo, "Source Radius##FluidVol", &vol->sourceRadius, 0.1f, 0.1f, 20.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Source Density##FluidVol", &vol->sourceDensity, 1.0f, 0.0f, 1000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Source Vel Scale##FluidVol", &vol->sourceVelocityScale, 1.0f, 0.0f, 200.0f);

        ImGui::DragInt("Priority##FluidVol", &vol->priority, 1, -100, 100);

        if (ImGui::Button("Reset Simulation##FluidVol")) {
            m_FluidSimulation.Reset(entity);
        }

        if (ImGui::Button("Remove##FluidVolume")) {
            RemoveComponentWithUndo<ECS::FluidVolumeComponent>(entity, "fluidVolume", "Fluid Volume");
        }
    }
}

void EditorLayer::DrawFluidTerrainCoupling(ECS::Entity entity) {
    if (UI::SectionHeader("Fluid Terrain Coupling")) {
        auto& config = m_FluidTerrainCoupling.GetConfig();

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled##FluidTerrain", &config.enabled);

        // Mode toggle
        bool accumulate = config.accumulateMode;
        const char* modes[] = { "Erosion", "Accumulate (Lava)" };
        int modeIdx = accumulate ? 1 : 0;
        if (InspectorUndo::Combo(m_UndoRedo, "Mode##FluidTerrain", &modeIdx, modes, 2)) {
            config.accumulateMode = (modeIdx == 1);
        }

        ImGui::Separator();
        ImGui::Text("Erosion / Deposition");
        InspectorUndo::DragFloat(m_UndoRedo, "Erosion Rate##FluidTerrain", &config.erosionRate, 0.001f, 0.0f, 1.0f, "%.4f");
        InspectorUndo::DragFloat(m_UndoRedo, "Deposition Rate##FluidTerrain", &config.depositionRate, 0.001f, 0.0f, 1.0f, "%.4f");
        InspectorUndo::DragFloat(m_UndoRedo, "Hardness##FluidTerrain", &config.hardness, 0.01f, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Height Scale##FluidTerrain", &config.heightScale, 0.1f, 0.01f, 100.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Max Erosion/Frame##FluidTerrain", &config.maxErosionPerFrame, 0.01f, 0.01f, 10.0f);

        ImGui::Separator();
        ImGui::Text("Bidirectional Feedback");
        InspectorUndo::Checkbox(m_UndoRedo, "Bidirectional##FluidTerrain", &config.bidirectional);
        if (config.bidirectional) {
            InspectorUndo::DragFloat(m_UndoRedo, "Slope Vel Scale##FluidTerrain", &config.slopeVelocityScale, 0.1f, 0.0f, 50.0f);
        }

        // Info text
        ImGui::Separator();
        if (config.accumulateMode) {
            ImGui::TextWrapped("Fluid density adds height to terrain (lava/mud building)");
        } else {
            ImGui::TextWrapped("Fast-moving fluid erodes terrain; slow-moving fluid deposits sediment");
        }
    }
}


void EditorLayer::DrawPlatformer2DController(ECS::Entity entity) {
    bool ctrlOpen = UI::SectionHeader("2D Platformer Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("Platformer2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::Platformer2DController>(entity, "platformer2D", "2D Platformer");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity);
        if (!ctrl) return;
        DrawComponentHelp("platformer2D", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##Platformer2D")) {
            InspectorUndo::Checkbox(m_UndoRedo, "WASD", &ctrl->useWASD);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Arrow Keys", &ctrl->useArrowKeys);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                InspectorUndo::DragInt(m_UndoRedo, "Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                InspectorUndo::DragFloat(m_UndoRedo, "Stick Sensitivity", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
                bool connected = Input::IsGamepadConnected(ctrl->gamepadIndex);
                ImGui::TextColored(connected ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                    connected ? "Connected: %s" : "Not Connected", connected ? Input::GetGamepadName(ctrl->gamepadIndex) : "");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement")) {
            // Movement mode toggle
            const char* moveMode = ctrl->gridMovement ? "Grid (Tile-based)" : "Free";
            if (ImGui::BeginCombo("Movement Mode##plat2d", moveMode)) {
                if (ImGui::Selectable("Free", !ctrl->gridMovement)) ctrl->gridMovement = false;
                if (ImGui::Selectable("Grid (Tile-based)", ctrl->gridMovement)) ctrl->gridMovement = true;
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free: smooth continuous movement\nGrid: snap to tile cells");

            if (ctrl->gridMovement) {
                InspectorUndo::DragFloat(m_UndoRedo, "Cell Size##plat2d", &ctrl->gridCellSize, 0.1f, 0.25f, 10.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Grid Move Speed##plat2d", &ctrl->gridMoveSpeed, 0.5f, 1.0f, 30.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("How fast the entity moves between grid cells");
            } else {
                InspectorUndo::DragFloat(m_UndoRedo, "Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Sprint Multiplier", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Acceleration", &ctrl->acceleration, 1.0f, 1.0f, 200.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Deceleration", &ctrl->deceleration, 1.0f, 1.0f, 200.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Air Control", &ctrl->airControl, 0.05f, 0.0f, 1.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Jumping")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Jump Force", &ctrl->jumpForce, 0.1f, 0.1f, 30.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Gravity", &ctrl->gravity, 0.5f, 1.0f, 100.0f);
            InspectorUndo::DragInt(m_UndoRedo, "Max Jumps", &ctrl->maxJumps, 1, 1, 5);
            InspectorUndo::DragFloat(m_UndoRedo, "Coyote Time", &ctrl->coyoteTime, 0.01f, 0.0f, 0.5f);
            InspectorUndo::DragFloat(m_UndoRedo, "Jump Buffer", &ctrl->jumpBufferTime, 0.01f, 0.0f, 0.5f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Collision")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Radius", &ctrl->collisionRadius, 0.05f, 0.05f, 5.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Height", &ctrl->collisionHeight, 0.1f, 0.1f, 10.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Wall Mechanics")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Wall Jump", &ctrl->enableWallJump);
            InspectorUndo::Checkbox(m_UndoRedo, "Wall Slide", &ctrl->enableWallSlide);
            if (ctrl->enableWallSlide) {
                InspectorUndo::DragFloat(m_UndoRedo, "Slide Speed", &ctrl->wallSlideSpeed, 0.1f, 0.1f, 10.0f);
            }
            if (ctrl->enableWallJump) {
                InspectorUndo::DragFloat(m_UndoRedo, "Wall Jump Force", &ctrl->wallJumpForce, 0.1f, 0.1f, 20.0f);
            }
            ImGui::TreePop();
        }

        // State display
        ImGui::Separator();
        ImGui::TextDisabled("State:");
        ImGui::Text("Grounded: %s", ctrl->isGrounded ? "Yes" : "No");
        ImGui::Text("Jumping: %s | Falling: %s", ctrl->isJumping ? "Yes" : "No", ctrl->isFalling ? "Yes" : "No");
        ImGui::Text("Velocity: %.1f, %.1f", ctrl->velocity.x, ctrl->velocity.y);
    }
}

void EditorLayer::DrawTopDown2DController(ECS::Entity entity) {
    bool ctrlOpen = UI::SectionHeader("2D Top-Down Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TopDown2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::TopDown2DController>(entity, "topDown2D", "2D Top-Down");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::TopDown2DController>(entity);
        if (!ctrl) return;
        DrawComponentHelp("topDown2D", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##TopDown2D")) {
            InspectorUndo::Checkbox(m_UndoRedo, "WASD", &ctrl->useWASD);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Arrow Keys", &ctrl->useArrowKeys);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                InspectorUndo::DragInt(m_UndoRedo, "Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                InspectorUndo::DragFloat(m_UndoRedo, "Stick Sensitivity", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
                bool connected = Input::IsGamepadConnected(ctrl->gamepadIndex);
                ImGui::TextColored(connected ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                    connected ? "Connected: %s" : "Not Connected", connected ? Input::GetGamepadName(ctrl->gamepadIndex) : "");
            }
            ImGui::TreePop();
        }

        // Movement mode toggle
        {
            const char* moveMode = ctrl->gridMovement ? "Grid (Tile-based)" : "Free";
            if (ImGui::BeginCombo("Movement Mode##td2d", moveMode)) {
                if (ImGui::Selectable("Free", !ctrl->gridMovement)) ctrl->gridMovement = false;
                if (ImGui::Selectable("Grid (Tile-based)", ctrl->gridMovement)) ctrl->gridMovement = true;
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free: smooth continuous movement\nGrid: snap to tile cells");
        }

        if (ctrl->gridMovement) {
            InspectorUndo::DragFloat(m_UndoRedo, "Cell Size##td2d", &ctrl->gridCellSize, 0.1f, 0.25f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Grid Move Speed##td2d", &ctrl->gridMoveSpeed, 0.5f, 1.0f, 30.0f);
        } else {
            InspectorUndo::DragFloat(m_UndoRedo, "Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Sprint Multiplier", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Acceleration", &ctrl->acceleration, 1.0f, 1.0f, 200.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Deceleration", &ctrl->deceleration, 1.0f, 1.0f, 200.0f);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Rotate To Face Movement", &ctrl->rotateToFaceMovement);
        if (ctrl->rotateToFaceMovement) {
            InspectorUndo::DragFloat(m_UndoRedo, "Rotation Speed", &ctrl->rotationSpeed, 10.0f, 0.0f, 1440.0f);
        }

        if (ImGui::TreeNode("Dash")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Enable Dash", &ctrl->enableDash);
            if (ctrl->enableDash) {
                InspectorUndo::DragFloat(m_UndoRedo, "Dash Speed", &ctrl->dashSpeed, 0.5f, 5.0f, 50.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Dash Duration", &ctrl->dashDuration, 0.01f, 0.05f, 1.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Dash Cooldown", &ctrl->dashCooldown, 0.1f, 0.0f, 5.0f);
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawTopDown3DController(ECS::Entity entity) {
    bool ctrlOpen = UI::SectionHeader("3D Top-Down Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TopDown3DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::TopDown3DController>(entity, "topDown3D", "3D Top-Down");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::TopDown3DController>(entity);
        if (!ctrl) return;
        DrawComponentHelp("topDown3D", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##TopDown3D")) {
            InspectorUndo::Checkbox(m_UndoRedo, "WASD", &ctrl->useWASD);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Arrow Keys", &ctrl->useArrowKeys);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                InspectorUndo::DragInt(m_UndoRedo, "Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                InspectorUndo::DragFloat(m_UndoRedo, "Stick Sensitivity", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
                bool connected = Input::IsGamepadConnected(ctrl->gamepadIndex);
                ImGui::TextColored(connected ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                    connected ? "Connected: %s" : "Not Connected", connected ? Input::GetGamepadName(ctrl->gamepadIndex) : "");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement")) {
            const char* moveMode = ctrl->gridMovement ? "Grid (Tile-based)" : "Free";
            if (ImGui::BeginCombo("Movement Mode##td3d", moveMode)) {
                if (ImGui::Selectable("Free", !ctrl->gridMovement)) ctrl->gridMovement = false;
                if (ImGui::Selectable("Grid (Tile-based)", ctrl->gridMovement)) ctrl->gridMovement = true;
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free: smooth continuous movement\nGrid: snap to tile cells");

            if (ctrl->gridMovement) {
                InspectorUndo::DragFloat(m_UndoRedo, "Cell Size##td3d", &ctrl->gridCellSize, 0.1f, 0.25f, 10.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Grid Move Speed##td3d", &ctrl->gridMoveSpeed, 0.5f, 1.0f, 30.0f);
                InspectorUndo::DragFloat3(m_UndoRedo, "Grid Origin##td3d", &ctrl->gridOrigin.x, [ctrl](f32 x, f32 y, f32 z) { ctrl->gridOrigin = Math::Vector3(x, y, z); }, 0.1f);
            } else {
                InspectorUndo::DragFloat(m_UndoRedo, "Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Sprint Multiplier", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Acceleration##td3d", &ctrl->acceleration, 0.5f, 0.0f, 200.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Deceleration##td3d", &ctrl->deceleration, 0.5f, 0.0f, 200.0f);
            }
            InspectorUndo::Checkbox(m_UndoRedo, "Rotate To Face Movement", &ctrl->rotateToFaceMovement);
            InspectorUndo::DragFloat(m_UndoRedo, "Rotation Speed", &ctrl->rotationSpeed, 10.0f, 0.0f, 1440.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Camera")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Camera Angle", &ctrl->cameraAngle, 1.0f, 0.0f, 90.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Camera Distance", &ctrl->cameraDistance, 0.5f, 5.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Camera Height", &ctrl->cameraHeight, 0.5f, 1.0f, 30.0f);
            InspectorUndo::Checkbox(m_UndoRedo, "Lock Camera To Player", &ctrl->lockCameraToPlayer);
            ImGui::TreePop();
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Enable Click-To-Move", &ctrl->enableClickToMove);
        if (ctrl->enableClickToMove) {
            InspectorUndo::DragFloat(m_UndoRedo, "Arrival Threshold##td3d", &ctrl->arrivalThreshold, 0.05f, 0.1f, 3.0f);
        }
        InspectorUndo::Checkbox(m_UndoRedo, "Enable Dash", &ctrl->enableDash);
        if (ctrl->enableDash) {
            InspectorUndo::DragFloat(m_UndoRedo, "Dash Speed##td3d", &ctrl->dashSpeed, 0.5f, 1.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Dash Duration##td3d", &ctrl->dashDuration, 0.05f, 0.05f, 2.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Dash Cooldown##td3d", &ctrl->dashCooldown, 0.1f, 0.0f, 10.0f);
        }
    }
}

void EditorLayer::DrawThirdPersonController(ECS::Entity entity) {
    bool ctrlOpen = UI::SectionHeader("3D Third Person Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ThirdPersonCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ThirdPersonController>(entity, "thirdPerson", "3D Third Person");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::ThirdPersonController>(entity);
        if (!ctrl) return;
        DrawComponentHelp("thirdPerson", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##ThirdPerson")) {
            InspectorUndo::Checkbox(m_UndoRedo, "WASD", &ctrl->useWASD);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Arrow Keys", &ctrl->useArrowKeys);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                InspectorUndo::DragInt(m_UndoRedo, "Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                InspectorUndo::DragFloat(m_UndoRedo, "Stick Sensitivity", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
                bool connected = Input::IsGamepadConnected(ctrl->gamepadIndex);
                ImGui::TextColored(connected ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                    connected ? "Connected: %s" : "Not Connected", connected ? Input::GetGamepadName(ctrl->gamepadIndex) : "");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement")) {
            const char* moveMode = ctrl->gridMovement ? "Grid (Tile-based)" : "Free";
            if (ImGui::BeginCombo("Movement Mode##tps", moveMode)) {
                if (ImGui::Selectable("Free", !ctrl->gridMovement)) ctrl->gridMovement = false;
                if (ImGui::Selectable("Grid (Tile-based)", ctrl->gridMovement)) ctrl->gridMovement = true;
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free: smooth continuous movement\nGrid: snap to tile cells");

            if (ctrl->gridMovement) {
                InspectorUndo::DragFloat(m_UndoRedo, "Cell Size##tps", &ctrl->gridCellSize, 0.1f, 0.25f, 10.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Grid Move Speed##tps", &ctrl->gridMoveSpeed, 0.5f, 1.0f, 30.0f);
                InspectorUndo::DragFloat3(m_UndoRedo, "Grid Origin##tps", &ctrl->gridOrigin.x, [ctrl](f32 x, f32 y, f32 z) { ctrl->gridOrigin = Math::Vector3(x, y, z); }, 0.1f);
            } else {
                InspectorUndo::DragFloat(m_UndoRedo, "Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Sprint Multiplier", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Acceleration##tps", &ctrl->acceleration, 0.5f, 0.0f, 200.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Deceleration##tps", &ctrl->deceleration, 0.5f, 0.0f, 200.0f);
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Jump Force", &ctrl->jumpForce, 0.1f, 1.0f, 30.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Gravity", &ctrl->gravity, 0.5f, 1.0f, 100.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Rotation")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Rotate To Face Movement", &ctrl->rotateToFaceMovement);
            InspectorUndo::Checkbox(m_UndoRedo, "Rotate To Face Camera", &ctrl->rotateToFaceCamera);
            InspectorUndo::DragFloat(m_UndoRedo, "Rotation Speed", &ctrl->rotationSpeed, 10.0f, 0.0f, 1440.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Camera")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Disable Mouse Look##tps", &ctrl->disableMouseLook);
            InspectorUndo::Checkbox(m_UndoRedo, "Capture Mouse On Click##tps", &ctrl->captureMouseOnClick);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("On: clicking the game captures the cursor for free look.\n"
                                  "Off: cursor stays visible, camera orbits only while holding\n"
                                  "right mouse, and on-screen UI stays clickable (Web Demo mode).");
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Distance", &ctrl->cameraDistance, 0.1f, ctrl->cameraMinDistance, ctrl->cameraMaxDistance);
            InspectorUndo::DragFloat(m_UndoRedo, "Min Distance##tps", &ctrl->cameraMinDistance, 0.1f, 0.5f, ctrl->cameraMaxDistance);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Distance##tps", &ctrl->cameraMaxDistance, 0.1f, ctrl->cameraMinDistance, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Height", &ctrl->cameraHeight, 0.1f, 0.0f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Sensitivity", &ctrl->cameraSensitivity, 0.1f, 0.1f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Lerp Speed", &ctrl->cameraLerpSpeed, 0.5f, 1.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Pitch", &ctrl->cameraPitch, 1.0f, ctrl->cameraMinPitch, ctrl->cameraMaxPitch);
            InspectorUndo::DragFloat(m_UndoRedo, "Min Pitch##tps", &ctrl->cameraMinPitch, 1.0f, -89.0f, 0.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Pitch##tps", &ctrl->cameraMaxPitch, 1.0f, 0.0f, 89.0f);
            InspectorUndo::Checkbox(m_UndoRedo, "Enable Collision", &ctrl->enableCameraCollision);
            if (ctrl->enableCameraCollision) {
                InspectorUndo::DragFloat(m_UndoRedo, "Collision Radius##tps", &ctrl->cameraCollisionRadius, 0.05f, 0.05f, 2.0f);
            }
            ImGui::TreePop();
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Enable Lock-On", &ctrl->enableLockOn);
        if (ctrl->enableLockOn) {
            InspectorUndo::DragFloat(m_UndoRedo, "Lock-On Range", &ctrl->lockOnRange, 0.5f, 1.0f, 100.0f);
        }

        ImGui::Separator();
        ImGui::TextDisabled("State:");
        ImGui::Text("Grounded: %s | Sprinting: %s", ctrl->isGrounded ? "Yes" : "No", ctrl->isSprinting ? "Yes" : "No");
    }
}

void EditorLayer::DrawFirstPersonController(ECS::Entity entity) {
    bool ctrlOpen = UI::SectionHeader("3D First Person Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("FirstPersonCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::FirstPersonController>(entity, "firstPerson", "3D First Person");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::FirstPersonController>(entity);
        if (!ctrl) return;
        DrawComponentHelp("firstPerson", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##FirstPerson")) {
            InspectorUndo::Checkbox(m_UndoRedo, "WASD", &ctrl->useWASD);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Arrow Keys", &ctrl->useArrowKeys);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                InspectorUndo::DragInt(m_UndoRedo, "Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                InspectorUndo::DragFloat(m_UndoRedo, "Stick Sensitivity", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
                bool connected = Input::IsGamepadConnected(ctrl->gamepadIndex);
                ImGui::TextColored(connected ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                    connected ? "Connected: %s" : "Not Connected", connected ? Input::GetGamepadName(ctrl->gamepadIndex) : "");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement")) {
            const char* moveMode = ctrl->gridMovement ? "Grid (Tile-based)" : "Free";
            if (ImGui::BeginCombo("Movement Mode##fps", moveMode)) {
                if (ImGui::Selectable("Free", !ctrl->gridMovement)) ctrl->gridMovement = false;
                if (ImGui::Selectable("Grid (Tile-based)", ctrl->gridMovement)) ctrl->gridMovement = true;
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Free: smooth continuous movement\nGrid: snap to tile cells (dungeon crawler style)");

            if (ctrl->gridMovement) {
                InspectorUndo::DragFloat(m_UndoRedo, "Cell Size##fps", &ctrl->gridCellSize, 0.1f, 0.25f, 10.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Grid Move Speed##fps", &ctrl->gridMoveSpeed, 0.5f, 1.0f, 30.0f);
                InspectorUndo::DragFloat3(m_UndoRedo, "Grid Origin##fps", &ctrl->gridOrigin.x, [ctrl](f32 x, f32 y, f32 z) { ctrl->gridOrigin = Math::Vector3(x, y, z); }, 0.1f);
            } else {
                InspectorUndo::DragFloat(m_UndoRedo, "Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Sprint Multiplier", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Acceleration##fps", &ctrl->acceleration, 0.5f, 0.0f, 200.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Deceleration##fps", &ctrl->deceleration, 0.5f, 0.0f, 200.0f);
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Jump Force", &ctrl->jumpForce, 0.1f, 1.0f, 30.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Gravity", &ctrl->gravity, 0.5f, 1.0f, 100.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Mouse Look")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Disable Mouse Look##fps", &ctrl->disableMouseLook);
            InspectorUndo::DragFloat(m_UndoRedo, "Sensitivity", &ctrl->mouseSensitivity, 0.1f, 0.1f, 10.0f);
            InspectorUndo::Checkbox(m_UndoRedo, "Invert Y", &ctrl->invertY);
            InspectorUndo::DragFloat(m_UndoRedo, "Pitch", &ctrl->pitch, 1.0f, ctrl->minPitch, ctrl->maxPitch);
            InspectorUndo::DragFloat(m_UndoRedo, "Min Pitch##fps", &ctrl->minPitch, 1.0f, -89.0f, 0.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Pitch##fps", &ctrl->maxPitch, 1.0f, 0.0f, 89.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Yaw", &ctrl->yaw, 1.0f, -180.0f, 180.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Crouching")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Enable Crouch", &ctrl->enableCrouch);
            InspectorUndo::DragFloat(m_UndoRedo, "Standing Height", &ctrl->standingHeight, 0.1f, 0.5f, 3.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Crouching Height", &ctrl->crouchingHeight, 0.1f, 0.3f, 2.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Crouch Speed Mult", &ctrl->crouchSpeed, 0.1f, 0.1f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Head Bob")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Enable Head Bob", &ctrl->enableHeadBob);
            InspectorUndo::DragFloat(m_UndoRedo, "Frequency", &ctrl->headBobFrequency, 0.5f, 1.0f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Amplitude", &ctrl->headBobAmplitude, 0.01f, 0.0f, 0.2f);
            ImGui::TreePop();
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Sprint FOV Increase", &ctrl->sprintFOVIncrease, 0.5f, 0.0f, 30.0f);

        if (ImGui::TreeNode("Dash")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Enable Dash", &ctrl->enableDash);
            if (ctrl->enableDash) {
                InspectorUndo::DragFloat(m_UndoRedo, "Dash Speed##fps", &ctrl->dashSpeed, 0.5f, 1.0f, 50.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Dash Duration##fps", &ctrl->dashDuration, 0.05f, 0.05f, 2.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Dash Cooldown##fps", &ctrl->dashCooldown, 0.1f, 0.0f, 10.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Dungeon Crawler")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Dungeon Crawler Mode", &ctrl->dungeonCrawlerMode);
            if (ctrl->dungeonCrawlerMode) {
                InspectorUndo::DragFloat(m_UndoRedo, "Snap Turn Angle", &ctrl->snapTurnAngle, 5.0f, 15.0f, 180.0f);
            }
            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::TextDisabled("State:");
        ImGui::Text("Grounded: %s | Crouching: %s | Sprinting: %s | Dashing: %s",
            ctrl->isGrounded ? "Yes" : "No",
            ctrl->isCrouching ? "Yes" : "No",
            ctrl->isSprinting ? "Yes" : "No",
            ctrl->isDashing ? "Yes" : "No");
    }
}

// ============================================================================
// Gameplay Component Inspector Drawing
// ============================================================================

void EditorLayer::DrawHealthComponent(ECS::Entity entity) {
    bool healthOpen = UI::SectionHeader("[H] Health", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("HealthCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::HealthComponent>(entity, "health", "Health");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (healthOpen) {
        auto* health = m_World->GetComponent<ECS::HealthComponent>(entity);
        if (!health) return;
        DrawComponentHelp("health", m_World, entity);

        // Health bar
        f32 healthPercent = health->GetHealthPercent();
        ImVec4 healthColor = healthPercent > 0.5f ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) :
                             healthPercent > 0.25f ? ImVec4(0.8f, 0.8f, 0.2f, 1.0f) :
                                                     ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, healthColor);
        ImGui::ProgressBar(healthPercent, ImVec2(-1, 0),
            (std::to_string((int)health->currentHealth) + " / " + std::to_string((int)health->maxHealth)).c_str());
        ImGui::PopStyleColor();

        InspectorUndo::DragFloat(m_UndoRedo, "Max Health", &health->maxHealth, 1.0f, 1.0f, 10000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Current Health", &health->currentHealth, 1.0f, 0.0f, health->maxHealth);

        if (ImGui::TreeNode("Regeneration")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Regen Rate (HP/s)", &health->regenRate, 0.5f, 0.0f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Regen Delay", &health->regenDelay, 0.1f, 0.0f, 10.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Shield")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Max Shield", &health->maxShield, 1.0f, 0.0f, 1000.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Current Shield", &health->currentShield, 1.0f, 0.0f, health->maxShield);
            InspectorUndo::DragFloat(m_UndoRedo, "Shield Regen", &health->shieldRegenRate, 0.5f, 0.0f, 100.0f);
            ImGui::TreePop();
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Invulnerable", &health->isInvulnerable);
        InspectorUndo::DragFloat(m_UndoRedo, "Invuln Time After Hit", &health->invulnerabilityTime, 0.1f, 0.0f, 5.0f);

        ImGui::Separator();
        ImGui::Text("Status: %s", health->isDead ? "DEAD" : "Alive");
    }
}

void EditorLayer::DrawRecordRewindComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("[RW] Record Rewind", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("RecordRewindCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::RecordRewindComponent>(entity, "recordRewind", "Record Rewind");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* rr = m_World->GetComponent<ECS::RecordRewindComponent>(entity);
        if (!rr) return;
        DrawComponentHelp("recordRewind", m_World, entity);

        InspectorUndo::DragFloat(m_UndoRedo, "Max Duration (s)", &rr->maxDuration, 0.5f, 1.0f, 60.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Record Interval", &rr->recordInterval, 0.001f, 0.01f, 0.5f);
        InspectorUndo::DragFloat(m_UndoRedo, "Rewind Speed", &rr->rewindSpeed, 0.1f, 0.1f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Cooldown (s)", &rr->cooldown, 0.1f, 0.0f, 30.0f);

        int key = rr->rewindKey;
        if (ImGui::InputInt("Rewind Key", &key)) rr->rewindKey = key;

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &rr->enabled);

        if (ImGui::TreeNode("Channels")) {
            bool chTransform = (rr->channels & 0x01) != 0;
            bool chVelocity  = (rr->channels & 0x02) != 0;
            bool chHealth    = (rr->channels & 0x04) != 0;
            bool chAnimation = (rr->channels & 0x08) != 0;
            bool chPhysics   = (rr->channels & 0x10) != 0;
            bool chMaterial  = (rr->channels & 0x20) != 0;
            if (ImGui::Checkbox("Transform", &chTransform)) rr->channels = (rr->channels & ~0x01u) | (chTransform ? 0x01u : 0);
            if (ImGui::Checkbox("Velocity",  &chVelocity))  rr->channels = (rr->channels & ~0x02u) | (chVelocity  ? 0x02u : 0);
            if (ImGui::Checkbox("Health",    &chHealth))    rr->channels = (rr->channels & ~0x04u) | (chHealth    ? 0x04u : 0);
            if (ImGui::Checkbox("Animation", &chAnimation)) rr->channels = (rr->channels & ~0x08u) | (chAnimation ? 0x08u : 0);
            if (ImGui::Checkbox("Physics",   &chPhysics))   rr->channels = (rr->channels & ~0x10u) | (chPhysics   ? 0x10u : 0);
            if (ImGui::Checkbox("Material",  &chMaterial))  rr->channels = (rr->channels & ~0x20u) | (chMaterial  ? 0x20u : 0);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Visual Feedback")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Vignette Strength", &rr->rewindVignetteStrength, 0.05f, 0.0f, 1.0f);
            f32 tint[3] = { rr->rewindTint.x, rr->rewindTint.y, rr->rewindTint.z };
            if (ImGui::ColorEdit3("Rewind Tint", tint)) {
                rr->rewindTint = Math::Vector3(tint[0], tint[1], tint[2]);
            }
            ImGui::TreePop();
        }

        // Runtime stats
        ImGui::Separator();
        ImGui::Text("Frames: %u / %u", rr->history.Count(), rr->history.Capacity());
        ImGui::Text("Recorded: %.1f s", rr->currentRecordedTime);
        ImGui::Text("Status: %s", rr->rewinding ? "REWINDING" : (rr->cooldownTimer > 0.0f ? "COOLDOWN" : "Recording"));
    }
}

void EditorLayer::DrawSceneRewindComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("[SW] Scene Rewind", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("SceneRewindCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::SceneRewindComponent>(entity, "sceneRewind", "Scene Rewind");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(entity);
        if (!sr) return;
        DrawComponentHelp("sceneRewind", m_World, entity);

        InspectorUndo::DragFloat(m_UndoRedo, "Max Duration (s)", &sr->maxDuration, 0.5f, 1.0f, 120.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Record Interval", &sr->recordInterval, 0.001f, 0.01f, 0.5f);
        InspectorUndo::DragFloat(m_UndoRedo, "Rewind Speed", &sr->rewindSpeed, 0.1f, 0.1f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Cooldown (s)", &sr->cooldown, 0.1f, 0.0f, 30.0f);

        int charges = sr->charges;
        if (ImGui::InputInt("Charges (0=unlimited)", &charges)) sr->charges = charges;

        int key = sr->rewindKey;
        if (ImGui::InputInt("Rewind Key", &key)) sr->rewindKey = key;

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &sr->enabled);
        InspectorUndo::Checkbox(m_UndoRedo, "Delta Compression", &sr->deltaCompression);

        int kfInterval = static_cast<int>(sr->keyframeInterval);
        if (ImGui::InputInt("Keyframe Interval", &kfInterval)) {
            sr->keyframeInterval = static_cast<u32>(Math::Max(1, kfInterval));
        }

        if (ImGui::TreeNode("Channels")) {
            bool chTransform = (sr->channels & 0x01) != 0;
            bool chVelocity  = (sr->channels & 0x02) != 0;
            bool chHealth    = (sr->channels & 0x04) != 0;
            bool chAnimation = (sr->channels & 0x08) != 0;
            bool chPhysics   = (sr->channels & 0x10) != 0;
            bool chMaterial  = (sr->channels & 0x20) != 0;
            if (ImGui::Checkbox("Transform", &chTransform)) sr->channels = (sr->channels & ~0x01u) | (chTransform ? 0x01u : 0);
            if (ImGui::Checkbox("Velocity",  &chVelocity))  sr->channels = (sr->channels & ~0x02u) | (chVelocity  ? 0x02u : 0);
            if (ImGui::Checkbox("Health",    &chHealth))    sr->channels = (sr->channels & ~0x04u) | (chHealth    ? 0x04u : 0);
            if (ImGui::Checkbox("Animation", &chAnimation)) sr->channels = (sr->channels & ~0x08u) | (chAnimation ? 0x08u : 0);
            if (ImGui::Checkbox("Physics",   &chPhysics))   sr->channels = (sr->channels & ~0x10u) | (chPhysics   ? 0x10u : 0);
            if (ImGui::Checkbox("Material",  &chMaterial))  sr->channels = (sr->channels & ~0x20u) | (chMaterial  ? 0x20u : 0);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Visual Feedback")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Vignette Strength", &sr->rewindVignetteStrength, 0.05f, 0.0f, 1.0f);
            f32 tint[3] = { sr->rewindTint.x, sr->rewindTint.y, sr->rewindTint.z };
            if (ImGui::ColorEdit3("Rewind Tint", tint)) {
                sr->rewindTint = Math::Vector3(tint[0], tint[1], tint[2]);
            }
            ImGui::TreePop();
        }

        // Runtime stats
        ImGui::Separator();
        ImGui::Text("Frames: %u / %u", sr->history.Count(), sr->history.Capacity());
        ImGui::Text("Recorded: %.1f s", sr->currentRecordedTime);
        ImGui::Text("Charges: %d / %d", sr->chargesUsed, sr->charges);
        ImGui::Text("Status: %s", sr->rewinding ? "REWINDING" : (sr->cooldownTimer > 0.0f ? "COOLDOWN" : "Recording"));
        if (sr->deltaCompression) {
            ImGui::Text("Keyframe in: %u frames", sr->keyframeInterval - sr->framesSinceKeyframe);
        }
    }
}

void EditorLayer::DrawSprite2DComponent(ECS::Entity entity) {
    bool spriteOpen = UI::SectionHeader("[S] Sprite 2D", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("Sprite2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::Sprite2DComponent>(entity, "sprite2D", "Sprite");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (spriteOpen) {
        auto* sprite = m_World->GetComponent<ECS::Sprite2DComponent>(entity);
        if (!sprite) return;
        DrawComponentHelp("sprite2D", m_World, entity);

        // Texture path
        char pathBuffer[256];
        strncpy(pathBuffer, sprite->texturePath.c_str(), sizeof(pathBuffer) - 1);
        pathBuffer[sizeof(pathBuffer) - 1] = '\0';
        if (InspectorUndo::InputText(m_UndoRedo, "Texture Path", pathBuffer, sizeof(pathBuffer),
                [sprite](const std::string& val) { sprite->texturePath = val; })) {
            sprite->texturePath = pathBuffer;
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string p(static_cast<const char*>(payload->Data));
                std::string e = std::filesystem::path(p).extension().string();
                std::transform(e.begin(), e.end(), e.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" || e == ".bmp" || e == ".svg") {
                    sprite->texturePath = p;
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Normal map path (for 2.5D lit sprite mode)
        char normalMapBuffer[256];
        strncpy(normalMapBuffer, sprite->normalMapPath.c_str(), sizeof(normalMapBuffer) - 1);
        normalMapBuffer[sizeof(normalMapBuffer) - 1] = '\0';
        if (InspectorUndo::InputText(m_UndoRedo, "Normal Map", normalMapBuffer, sizeof(normalMapBuffer),
                [sprite](const std::string& val) { sprite->normalMapPath = val; })) {
            sprite->normalMapPath = normalMapBuffer;
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string p(static_cast<const char*>(payload->Data));
                std::string e = std::filesystem::path(p).extension().string();
                std::transform(e.begin(), e.end(), e.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" || e == ".bmp" || e == ".svg") {
                    sprite->normalMapPath = p;
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Optional normal map for per-pixel lighting in 2.5D mode");
        }

        // --- Feature 1: Texture preview with source rect overlay ---
        if (!sprite->texturePath.empty() && m_RenderSystem) {
            VkDescriptorSet texId = GetImGuiTexture(sprite->texturePath);
            if (texId) {
                auto tex = m_RenderSystem->LoadTexture(sprite->texturePath);
                if (tex && tex->GetWidth() > 0 && tex->GetHeight() > 0) {
                    f32 texW = static_cast<f32>(tex->GetWidth());
                    f32 texH = static_cast<f32>(tex->GetHeight());

                    // Compute preview size (max 128px, preserve aspect ratio)
                    f32 maxDim = 128.0f;
                    f32 scale = std::min(maxDim / texW, maxDim / texH);
                    ImVec2 previewSize(texW * scale, texH * scale);

                    ImVec2 imgPos = ImGui::GetCursorScreenPos();
                    ImGui::Image(texId, previewSize);

                    // Draw source rect overlay if set
                    if (sprite->srcWidth > 0 && sprite->srcHeight > 0) {
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        f32 rx = imgPos.x + (sprite->srcX / texW) * previewSize.x;
                        f32 ry = imgPos.y + (sprite->srcY / texH) * previewSize.y;
                        f32 rw = (sprite->srcWidth / texW) * previewSize.x;
                        f32 rh = (sprite->srcHeight / texH) * previewSize.y;
                        drawList->AddRect(ImVec2(rx, ry), ImVec2(rx + rw, ry + rh),
                                          IM_COL32(255, 50, 50, 255), 0.0f, 0, 2.0f);
                    }

                    ImGui::Text("Texture: %ux%u", tex->GetWidth(), tex->GetHeight());
                }
            }
        }

        // Source rectangle (for sprite sheets)
        ImGui::Text("Source Rectangle:");
        InspectorUndo::DragFloat(m_UndoRedo, "Src X", &sprite->srcX, 1.0f, 0.0f, 4096.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Src Y", &sprite->srcY, 1.0f, 0.0f, 4096.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Src Width", &sprite->srcWidth, 1.0f, 0.0f, 4096.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Src Height", &sprite->srcHeight, 1.0f, 0.0f, 4096.0f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Set to 0 to use full texture");
        }

        // --- Feature 2: Sprite sheet frame picker ---
        if (!sprite->texturePath.empty() && m_RenderSystem) {
            VkDescriptorSet texId = GetImGuiTexture(sprite->texturePath);
            auto tex = m_RenderSystem->LoadTexture(sprite->texturePath);
            if (texId && tex && tex->GetWidth() > 0 && tex->GetHeight() > 0 &&
                ImGui::TreeNode("Frame Picker")) {
                f32 texW = static_cast<f32>(tex->GetWidth());
                f32 texH = static_cast<f32>(tex->GetHeight());

                ImGui::DragFloat("Frame Width", &m_SpriteFramePickerW, 1.0f, 1.0f, texW);
                ImGui::DragFloat("Frame Height", &m_SpriteFramePickerH, 1.0f, 1.0f, texH);

                // Scale sheet to fit panel width
                f32 panelWidth = ImGui::GetContentRegionAvail().x;
                f32 sheetScale = std::min(panelWidth / texW, 1.0f);
                ImVec2 sheetSize(texW * sheetScale, texH * sheetScale);

                ImVec2 sheetPos = ImGui::GetCursorScreenPos();
                ImGui::Image(texId, sheetSize);

                // Draw grid overlay and handle clicks
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                u32 gridCols = static_cast<u32>(texW / m_SpriteFramePickerW);
                u32 gridRows = static_cast<u32>(texH / m_SpriteFramePickerH);
                f32 cellW = m_SpriteFramePickerW * sheetScale;
                f32 cellH = m_SpriteFramePickerH * sheetScale;

                // Grid lines
                for (u32 c = 1; c < gridCols; ++c) {
                    f32 x = sheetPos.x + c * cellW;
                    drawList->AddLine(ImVec2(x, sheetPos.y), ImVec2(x, sheetPos.y + sheetSize.y),
                                      IM_COL32(255, 255, 255, 80));
                }
                for (u32 r = 1; r < gridRows; ++r) {
                    f32 y = sheetPos.y + r * cellH;
                    drawList->AddLine(ImVec2(sheetPos.x, y), ImVec2(sheetPos.x + sheetSize.x, y),
                                      IM_COL32(255, 255, 255, 80));
                }

                // Highlight current selection
                if (sprite->srcWidth > 0 && sprite->srcHeight > 0) {
                    f32 selX = sheetPos.x + (sprite->srcX / texW) * sheetSize.x;
                    f32 selY = sheetPos.y + (sprite->srcY / texH) * sheetSize.y;
                    f32 selW = (sprite->srcWidth / texW) * sheetSize.x;
                    f32 selH = (sprite->srcHeight / texH) * sheetSize.y;
                    drawList->AddRectFilled(ImVec2(selX, selY), ImVec2(selX + selW, selY + selH),
                                            IM_COL32(50, 150, 255, 60));
                    drawList->AddRect(ImVec2(selX, selY), ImVec2(selX + selW, selY + selH),
                                      IM_COL32(50, 150, 255, 255), 0.0f, 0, 2.0f);
                }

                // Click to select frame
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    f32 localX = mousePos.x - sheetPos.x;
                    f32 localY = mousePos.y - sheetPos.y;
                    if (localX >= 0 && localY >= 0 && localX < sheetSize.x && localY < sheetSize.y) {
                        u32 col = static_cast<u32>(localX / cellW);
                        u32 row = static_cast<u32>(localY / cellH);
                        sprite->srcX = col * m_SpriteFramePickerW;
                        sprite->srcY = row * m_SpriteFramePickerH;
                        sprite->srcWidth = m_SpriteFramePickerW;
                        sprite->srcHeight = m_SpriteFramePickerH;
                        sprite->spriteDirty = true;
                    }
                }

                ImGui::TreePop();
            }
        }

        // Size (DragFloat2 — use DragFloat3 wrapper with z=0 trick isn't clean, so use raw with manual undo)
        f32 size[2] = { sprite->size.x, sprite->size.y };
        if (ImGui::DragFloat2("Size", size, 0.1f, 0.1f, 100.0f)) {
            sprite->size = Math::Vector2(size[0], size[1]);
        }

        // Pivot
        f32 pivot[2] = { sprite->pivot.x, sprite->pivot.y };
        if (ImGui::DragFloat2("Pivot", pivot, 0.01f, 0.0f, 1.0f)) {
            sprite->pivot = Math::Vector2(pivot[0], pivot[1]);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0,0 = top-left, 0.5,0.5 = center, 1,1 = bottom-right");
        }

        // Tint
        f32 tint[3] = { sprite->tint.x, sprite->tint.y, sprite->tint.z };
        if (InspectorUndo::ColorEdit3(m_UndoRedo, "Tint", tint,
                [sprite](f32 r, f32 g, f32 b) { sprite->tint = Math::Vector3(r, g, b); })) {
            sprite->tint = Math::Vector3(tint[0], tint[1], tint[2]);
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Alpha", &sprite->alpha, 0.01f, 0.0f, 1.0f);

        // Sorting
        InspectorUndo::DragInt(m_UndoRedo, "Sorting Layer", &sprite->sortingLayer, 1, -100, 100);
        InspectorUndo::DragInt(m_UndoRedo, "Order in Layer", &sprite->orderInLayer, 1, -1000, 1000);

        // Flip
        InspectorUndo::Checkbox(m_UndoRedo, "Flip X", &sprite->flipX);
        ImGui::SameLine();
        InspectorUndo::Checkbox(m_UndoRedo, "Flip Y", &sprite->flipY);

        InspectorUndo::Checkbox(m_UndoRedo, "Visible", &sprite->visible);

        // Generate Collider from sprite alpha
        if (!sprite->texturePath.empty() && m_RenderSystem) {
            ImGui::Separator();
            ImGui::Text("Generate Collider:");

            auto generateCollider = [&](const char* label, int type) {
                if (ImGui::Button(label)) {
                    int w, h, ch;
                    u8* pixels = stbi_load(sprite->texturePath.c_str(), &w, &h, &ch, 4);
                    if (pixels) {
                        Math::Vector2 sprSize(sprite->size.x > 0 ? sprite->size.x : 1.0f,
                                              sprite->size.y > 0 ? sprite->size.y : 1.0f);
                        if (type == 0) {
                            auto box = SpriteColliderGenerator::FitBoxCollider(
                                pixels, (u32)w, (u32)h, sprSize, sprite->pivot);
                            if (!m_World->HasComponent<ECS::BoxColliderComponent>(entity))
                                m_World->AddComponent<ECS::BoxColliderComponent>(entity);
                            *m_World->GetComponent<ECS::BoxColliderComponent>(entity) = box;
                        } else if (type == 1) {
                            auto capsule = SpriteColliderGenerator::FitCapsuleCollider(
                                pixels, (u32)w, (u32)h, sprSize, sprite->pivot);
                            if (!m_World->HasComponent<ECS::CapsuleColliderComponent>(entity))
                                m_World->AddComponent<ECS::CapsuleColliderComponent>(entity);
                            *m_World->GetComponent<ECS::CapsuleColliderComponent>(entity) = capsule;
                        } else {
                            auto sphere = SpriteColliderGenerator::FitSphereCollider(
                                pixels, (u32)w, (u32)h, sprSize, sprite->pivot);
                            if (!m_World->HasComponent<ECS::SphereColliderComponent>(entity))
                                m_World->AddComponent<ECS::SphereColliderComponent>(entity);
                            *m_World->GetComponent<ECS::SphereColliderComponent>(entity) = sphere;
                        }
                        stbi_image_free(pixels);
                    }
                }
            };

            generateCollider("Fit Box", 0);
            ImGui::SameLine();
            generateCollider("Fit Capsule", 1);
            ImGui::SameLine();
            generateCollider("Fit Circle", 2);
            ImGui::SameLine();
            if (ImGui::Button("Fit Polygon")) {
                int w, h, channels;
                u8* pixels = stbi_load(sprite->texturePath.c_str(), &w, &h, &channels, 4);
                if (pixels) {
                    Math::Vector2 sprSize(sprite->size.x > 0 ? sprite->size.x : 1.0f,
                                          sprite->size.y > 0 ? sprite->size.y : 1.0f);
                    auto poly = SpriteColliderGenerator::FitPolygonCollider(
                        pixels, (u32)w, (u32)h, sprSize, sprite->pivot);
                    if (!m_World->HasComponent<ECS::PolygonCollider2DComponent>(entity))
                        m_World->AddComponent<ECS::PolygonCollider2DComponent>(entity);
                    *m_World->GetComponent<ECS::PolygonCollider2DComponent>(entity) = poly;
                    stbi_image_free(pixels);
                }
            }
        }

        // Drop Shadow settings
        ImGui::Separator();
        ImGui::Text("Drop Shadow:");
        InspectorUndo::Checkbox(m_UndoRedo, "Enable Shadow", &sprite->dropShadow);
        if (sprite->dropShadow) {
            f32 offset[2] = { sprite->shadowOffset.x, sprite->shadowOffset.y };
            if (ImGui::DragFloat2("Shadow Offset", offset, 0.1f, -50.0f, 50.0f)) {
                sprite->shadowOffset = Math::Vector2(offset[0], offset[1]);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Offset in world units (X, Y) from sprite position");
            }
            f32 col[4] = { sprite->shadowColor.x, sprite->shadowColor.y, sprite->shadowColor.z, sprite->shadowColor.w };
            if (ImGui::ColorEdit4("Shadow Color", col, ImGuiColorEditFlags_AlphaBar)) {
                sprite->shadowColor = Math::Vector4(col[0], col[1], col[2], col[3]);
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Shadow Scale", &sprite->shadowScale, 0.01f, 0.1f, 3.0f);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Scale of the shadow relative to sprite (1.0 = same size)");
            }
        }

        // Generate Normal Map from height/grayscale texture
        if (!sprite->texturePath.empty()) {
            ImGui::Separator();
            if (ImGui::Button("Generate Normal Map")) {
                std::string texPath = sprite->texturePath;
                // Build output path: foo.png -> foo_normal.png
                auto dotPos = texPath.rfind('.');
                std::string normalPath;
                if (dotPos != std::string::npos) {
                    normalPath = texPath.substr(0, dotPos) + "_normal" + texPath.substr(dotPos);
                } else {
                    normalPath = texPath + "_normal.png";
                }
                Renderer::NormalMapGenerator::Options opts;
                opts.strength = 1.0f;
                if (Renderer::NormalMapGenerator::GenerateAndSave(texPath, normalPath, opts)) {
                    sprite->normalMapPath = normalPath;
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Generate a normal map from this texture (treats as height map)");
            }
        }
    }
}

void EditorLayer::DrawAnimatedSprite2DComponent(ECS::Entity entity) {
    bool animOpen = UI::SectionHeader("Animated Sprite 2D", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("AnimSprite2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::AnimatedSprite2DComponent>(entity, "animatedSprite2D", "Animated Sprite");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (animOpen) {
        auto* anim = m_World->GetComponent<ECS::AnimatedSprite2DComponent>(entity);
        if (!anim) return;
        DrawComponentHelp("animatedSprite2D", m_World, entity);

        // --- Feature 3: Animation preview widget ---
        auto* sprite = m_World->GetComponent<ECS::Sprite2DComponent>(entity);
        if (sprite && !sprite->texturePath.empty() && !anim->frames.empty() && m_RenderSystem) {
            VkDescriptorSet texId = GetImGuiTexture(sprite->texturePath);
            if (texId && sprite->texPixelWidth > 0 && sprite->texPixelHeight > 0) {
                u32 frameIdx = anim->currentFrame < static_cast<u32>(anim->frames.size())
                    ? anim->currentFrame : 0;
                const auto& frame = anim->frames[frameIdx];
                f32 tw = sprite->texPixelWidth;
                f32 th = sprite->texPixelHeight;
                f32 fw = sprite->srcWidth > 0 ? sprite->srcWidth : tw;
                f32 fh = sprite->srcHeight > 0 ? sprite->srcHeight : th;

                ImVec2 uv0(frame.srcX / tw, frame.srcY / th);
                ImVec2 uv1((frame.srcX + fw) / tw, (frame.srcY + fh) / th);

                ImGui::Image(texId, ImVec2(64, 64), uv0, uv1);
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::Text("Frame %u/%zu", frameIdx + 1, anim->frames.size());
                if (anim->frames.size() > 0) {
                    f32 progress = static_cast<f32>(frameIdx) / static_cast<f32>(anim->frames.size());
                    ImGui::ProgressBar(progress, ImVec2(100, 0));
                }
                ImGui::EndGroup();
            }
        }

        ImGui::Text("Frames: %zu", anim->frames.size());
        ImGui::Text("Current Frame: %u", anim->currentFrame);
        ImGui::Text("Frame Timer: %.2f", anim->frameTimer);

        InspectorUndo::Checkbox(m_UndoRedo, "Playing", &anim->playing);
        InspectorUndo::Checkbox(m_UndoRedo, "Loop", &anim->loop);
        InspectorUndo::DragFloat(m_UndoRedo, "Playback Speed", &anim->playbackSpeed, 0.1f, 0.1f, 10.0f);

        if (anim->animationComplete) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Animation Complete");
        }

        // --- Feature 4: Sprite atlas auto-slicer ---
        if (sprite && !sprite->texturePath.empty() && m_RenderSystem) {
            auto tex = m_RenderSystem->LoadTexture(sprite->texturePath);
            if (tex && tex->GetWidth() > 0 && tex->GetHeight() > 0) {
                ImGui::Separator();
                ImGui::Text("Auto-Slice");
                ImGui::DragFloat("Slice Width", &m_AutoSliceWidth, 1.0f, 1.0f, 2048.0f);
                ImGui::DragFloat("Slice Height", &m_AutoSliceHeight, 1.0f, 1.0f, 2048.0f);
                ImGui::DragFloat("Frame Duration", &m_AutoSliceDuration, 0.01f, 0.01f, 2.0f);
                ImGui::DragInt("Frame Count (0=all)", &m_AutoSliceCount, 1, 0, 1024);
                if (ImGui::Button("Slice")) {
                    f32 texW = static_cast<f32>(tex->GetWidth());
                    f32 texH = static_cast<f32>(tex->GetHeight());
                    u32 cols = static_cast<u32>(texW / m_AutoSliceWidth);
                    u32 rows = static_cast<u32>(texH / m_AutoSliceHeight);
                    u32 total = (m_AutoSliceCount > 0) ? static_cast<u32>(m_AutoSliceCount) : cols * rows;

                    anim->frames.clear();
                    for (u32 i = 0; i < total; ++i) {
                        ECS::AnimatedSprite2DComponent::Frame f;
                        f.srcX = static_cast<f32>(i % cols) * m_AutoSliceWidth;
                        f.srcY = static_cast<f32>(i / cols) * m_AutoSliceHeight;
                        f.duration = m_AutoSliceDuration;
                        anim->frames.push_back(f);
                    }
                    anim->currentFrame = 0;
                    anim->frameTimer = 0.0f;
                    sprite->srcWidth = m_AutoSliceWidth;
                    sprite->srcHeight = m_AutoSliceHeight;
                    sprite->spriteDirty = true;
                }
            }

            // --- Auto-detect sprite regions by transparency ---
            if (sprite && !sprite->texturePath.empty()) {
                ImGui::Separator();
                ImGui::Text("Auto-Detect Regions");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Automatically detect sprite regions by transparency");

                static u8 autoDetectThreshold = 10;
                ImGui::SliderScalar("Alpha Threshold", ImGuiDataType_U8, &autoDetectThreshold,
                    nullptr, nullptr, "%u");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Minimum alpha value to consider a pixel non-transparent (0-255)");

                static f32 autoDetectFPS = 12.0f;
                ImGui::DragFloat("Frame Rate##AutoDetect", &autoDetectFPS, 0.5f, 1.0f, 60.0f, "%.1f fps");

                if (ImGui::Button("Detect Regions##AutoDetect")) {
                    Editor::SpriteSheetImporter importer;
                    if (importer.LoadImage(sprite->texturePath)) {
                        auto result = importer.SliceAutoDetect(autoDetectThreshold);
                        if (!result.slices.empty()) {
                            anim->frames.clear();
                            f32 frameDuration = (autoDetectFPS > 0.0f) ? (1.0f / autoDetectFPS) : 0.1f;
                            for (const auto& slice : result.slices) {
                                ECS::AnimatedSprite2DComponent::Frame f;
                                f.srcX = slice.x;
                                f.srcY = slice.y;
                                f.duration = frameDuration;
                                anim->frames.push_back(f);
                            }
                            // Use first slice dimensions for sprite source size
                            sprite->srcWidth = result.slices[0].width;
                            sprite->srcHeight = result.slices[0].height;
                            sprite->spriteDirty = true;
                            anim->currentFrame = 0;
                            anim->frameTimer = 0.0f;
                            ENJIN_LOG_INFO(Editor, "Auto-detected %zu sprite regions (threshold=%u)",
                                result.slices.size(), autoDetectThreshold);
                        } else {
                            ENJIN_LOG_WARN(Editor, "No sprite regions detected (threshold=%u)", autoDetectThreshold);
                        }
                    } else {
                        ENJIN_LOG_WARN(Editor, "Failed to load image for auto-detect: %s", sprite->texturePath.c_str());
                    }
                }
            }
        }
        ImGui::Separator();

        // Frame editor
        if (ImGui::TreeNode("Frames")) {
            for (usize i = 0; i < anim->frames.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode("Frame", "Frame %zu", i)) {
                    ImGui::DragFloat("Src X", &anim->frames[i].srcX, 1.0f);
                    ImGui::DragFloat("Src Y", &anim->frames[i].srcY, 1.0f);
                    ImGui::DragFloat("Duration", &anim->frames[i].duration, 0.01f, 0.01f, 2.0f);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Add Frame")) {
                ECS::AnimatedSprite2DComponent::Frame frame;
                frame.srcX = 0;
                frame.srcY = 0;
                frame.duration = 0.1f;
                anim->frames.push_back(frame);
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawTilemapComponent(ECS::Entity entity) {
    bool tilemapOpen = UI::SectionHeader("Tilemap", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TilemapCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::TilemapComponent>(entity, "tilemap", "Tilemap");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (tilemapOpen) {
        auto* tilemap = m_World->GetComponent<ECS::TilemapComponent>(entity);
        if (!tilemap) return;
        DrawComponentHelp("tilemap", m_World, entity);

        // Tileset path
        char pathBuffer[256];
        strncpy(pathBuffer, tilemap->tilesetPath.c_str(), sizeof(pathBuffer) - 1);
        pathBuffer[sizeof(pathBuffer) - 1] = '\0';
        if (InspectorUndo::InputText(m_UndoRedo, "Tileset Path", pathBuffer, sizeof(pathBuffer), [tilemap](const std::string& val) { tilemap->tilesetPath = val; })) {
            tilemap->tilesetPath = pathBuffer;
        }

        // Tile size
        InspectorUndo::DragFloat(m_UndoRedo, "Tile Width (px)", &tilemap->tileWidth, 1.0f, 1.0f, 256.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Tile Height (px)", &tilemap->tileHeight, 1.0f, 1.0f, 256.0f);

        int cols = static_cast<int>(tilemap->tilesetColumns);
        if (ImGui::DragInt("Tileset Columns", &cols, 1, 1, 64)) {
            tilemap->tilesetColumns = static_cast<u32>(cols);
        }

        // World scale
        InspectorUndo::DragFloat(m_UndoRedo, "World Tile Width", &tilemap->worldTileWidth, 0.1f, 0.1f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "World Tile Height", &tilemap->worldTileHeight, 0.1f, 0.1f, 10.0f);

        // Map size
        int w = static_cast<int>(tilemap->width);
        int h = static_cast<int>(tilemap->height);
        bool sizeChanged = false;
        if (ImGui::DragInt("Map Width", &w, 1, 1, 256)) {
            tilemap->width = static_cast<u32>(w);
            sizeChanged = true;
        }
        if (ImGui::DragInt("Map Height", &h, 1, 1, 256)) {
            tilemap->height = static_cast<u32>(h);
            sizeChanged = true;
        }
        if (sizeChanged) {
            tilemap->tiles.resize(tilemap->width * tilemap->height, -1);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Has Collision", &tilemap->hasCollision);

        ImGui::Text("Total Tiles: %zu", tilemap->tiles.size());

        // --- Feature 6: Viewport brush edit mode toggle ---
        ImGui::Separator();
        if (ImGui::Checkbox("Edit Mode (Viewport Brush)", &m_TilemapEditMode)) {
            if (m_TilemapEditMode) {
                m_TerrainEditMode = false;  // Disable terrain editing if active
            }
        }
        if (m_TilemapEditMode) {
            ImGui::DragInt("Brush Tile", &m_TileBrushIndex, 1, -1, 999);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f),
                "LMB: Paint tile %d | RMB: Erase", m_TileBrushIndex);
        }

        // --- Feature 5: Tilemap visual grid editor in inspector ---
        if (tilemap->width > 0 && tilemap->height > 0 && ImGui::TreeNode("Tile Editor")) {

            // Tileset palette (if texture loaded)
            if (!tilemap->tilesetPath.empty() && m_RenderSystem) {
                VkDescriptorSet texId = GetImGuiTexture(tilemap->tilesetPath);
                auto tex = m_RenderSystem->LoadTexture(tilemap->tilesetPath);
                if (texId && tex && tex->GetWidth() > 0 && tex->GetHeight() > 0) {
                    ImGui::Text("Tileset Palette:");
                    f32 texW = static_cast<f32>(tex->GetWidth());
                    f32 texH = static_cast<f32>(tex->GetHeight());
                    f32 panelW = ImGui::GetContentRegionAvail().x;
                    f32 paletteScale = std::min(panelW / texW, 1.0f);
                    ImVec2 paletteSize(texW * paletteScale, texH * paletteScale);

                    ImVec2 palPos = ImGui::GetCursorScreenPos();
                    ImGui::Image(texId, paletteSize);

                    // Grid overlay on palette
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    f32 cellW = tilemap->tileWidth * paletteScale;
                    f32 cellH = tilemap->tileHeight * paletteScale;
                    u32 palCols = static_cast<u32>(texW / tilemap->tileWidth);
                    u32 palRows = static_cast<u32>(texH / tilemap->tileHeight);

                    for (u32 c = 1; c < palCols; ++c) {
                        f32 x = palPos.x + c * cellW;
                        drawList->AddLine(ImVec2(x, palPos.y), ImVec2(x, palPos.y + paletteSize.y),
                                          IM_COL32(255, 255, 255, 60));
                    }
                    for (u32 r = 1; r < palRows; ++r) {
                        f32 y = palPos.y + r * cellH;
                        drawList->AddLine(ImVec2(palPos.x, y), ImVec2(palPos.x + paletteSize.x, y),
                                          IM_COL32(255, 255, 255, 60));
                    }

                    // Highlight selected tile in palette
                    if (m_TileBrushIndex >= 0) {
                        u32 selCol = static_cast<u32>(m_TileBrushIndex) % palCols;
                        u32 selRow = static_cast<u32>(m_TileBrushIndex) / palCols;
                        f32 sx = palPos.x + selCol * cellW;
                        f32 sy = palPos.y + selRow * cellH;
                        drawList->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + cellW, sy + cellH),
                                                IM_COL32(50, 150, 255, 60));
                        drawList->AddRect(ImVec2(sx, sy), ImVec2(sx + cellW, sy + cellH),
                                          IM_COL32(50, 150, 255, 255), 0.0f, 0, 2.0f);
                    }

                    // Click palette to select brush tile
                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        ImVec2 mousePos = ImGui::GetMousePos();
                        f32 lx = mousePos.x - palPos.x;
                        f32 ly = mousePos.y - palPos.y;
                        if (lx >= 0 && ly >= 0 && lx < paletteSize.x && ly < paletteSize.y) {
                            u32 col = static_cast<u32>(lx / cellW);
                            u32 row = static_cast<u32>(ly / cellH);
                            m_TileBrushIndex = static_cast<i32>(row * palCols + col);
                        }
                    }
                }
            }

            ImGui::DragInt("Brush Tile##TileEditor", &m_TileBrushIndex, 1, -1, 999);

            // Draw the tile grid
            ImGui::Text("Tile Grid:");
            f32 cellSize = 20.0f;
            f32 gridW = tilemap->width * cellSize;
            f32 gridH = tilemap->height * cellSize;
            ImVec2 gridOrigin = ImGui::GetCursorScreenPos();

            // Reserve space for the grid
            ImGui::Dummy(ImVec2(gridW, gridH));

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            for (u32 row = 0; row < tilemap->height; ++row) {
                for (u32 col = 0; col < tilemap->width; ++col) {
                    f32 x0 = gridOrigin.x + col * cellSize;
                    f32 y0 = gridOrigin.y + row * cellSize;
                    f32 x1 = x0 + cellSize;
                    f32 y1 = y0 + cellSize;

                    i32 tileIdx = tilemap->GetTile(col, row);

                    if (tileIdx >= 0) {
                        // Color-code tiles by index
                        u8 r = static_cast<u8>((tileIdx * 47 + 80) % 200 + 55);
                        u8 g = static_cast<u8>((tileIdx * 73 + 120) % 200 + 55);
                        u8 b = static_cast<u8>((tileIdx * 31 + 160) % 200 + 55);
                        drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(r, g, b, 180));

                        // Show tile ID text
                        char idBuf[8];
                        snprintf(idBuf, sizeof(idBuf), "%d", tileIdx);
                        ImVec2 textSize = ImGui::CalcTextSize(idBuf);
                        if (textSize.x < cellSize && textSize.y < cellSize) {
                            drawList->AddText(ImVec2(x0 + 1, y0 + 1), IM_COL32(255, 255, 255, 200), idBuf);
                        }
                    } else {
                        drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(40, 40, 40, 180));
                    }

                    // Cell border
                    drawList->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(100, 100, 100, 150));
                }
            }

            // Handle click/drag on grid
            ImVec2 mousePos = ImGui::GetMousePos();
            bool hovering = mousePos.x >= gridOrigin.x && mousePos.y >= gridOrigin.y &&
                            mousePos.x < gridOrigin.x + gridW && mousePos.y < gridOrigin.y + gridH;
            if (hovering && ImGui::IsWindowHovered()) {
                u32 col = static_cast<u32>((mousePos.x - gridOrigin.x) / cellSize);
                u32 row = static_cast<u32>((mousePos.y - gridOrigin.y) / cellSize);

                // Highlight hovered cell
                f32 hx = gridOrigin.x + col * cellSize;
                f32 hy = gridOrigin.y + row * cellSize;
                drawList->AddRect(ImVec2(hx, hy), ImVec2(hx + cellSize, hy + cellSize),
                                  IM_COL32(255, 255, 0, 200), 0.0f, 0, 2.0f);

                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    tilemap->SetTile(col, row, m_TileBrushIndex);
                } else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    tilemap->SetTile(col, row, -1);
                }
            }

            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawStateMachineComponent(ECS::Entity entity) {
    bool smOpen = UI::SectionHeader("State Machine", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("StateMachineCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::StateMachineComponent>(entity, "stateMachine", "State Machine");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (smOpen) {
        auto* sm = m_World->GetComponent<ECS::StateMachineComponent>(entity);
        if (!sm) return;
        DrawComponentHelp("stateMachine", m_World, entity);

        // Current state display
        if (!sm->states.empty()) {
            // Combo to select current state from defined states
            if (ImGui::BeginCombo("Current State", sm->currentState.c_str())) {
                for (const auto& s : sm->states) {
                    bool selected = (sm->currentState == s.name);
                    if (ImGui::Selectable(s.name.c_str(), selected)) {
                        sm->SetState(s.name);
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::TextDisabled("No states defined");
        }

        ImGui::Text("Previous: %s", sm->previousState.empty() ? "(none)" : sm->previousState.c_str());
        ImGui::Text("State Time: %.2f s", sm->stateTime);
        ImGui::Separator();

        // --- Parameters ---
        if (ImGui::TreeNode("Parameters")) {
            // Bool params
            if (ImGui::TreeNode("Bool")) {
                std::string toRemove;
                for (auto& [name, val] : sm->boolParams) {
                    ImGui::PushID(name.c_str());
                    ImGui::Checkbox(name.c_str(), &val);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) toRemove = name;
                    ImGui::PopID();
                }
                if (!toRemove.empty()) sm->boolParams.erase(toRemove);

                static char newBoolName[64] = "";
                ImGui::InputText("##newBool", newBoolName, sizeof(newBoolName));
                ImGui::SameLine();
                if (ImGui::SmallButton("+ Bool") && newBoolName[0] != '\0') {
                    sm->boolParams[newBoolName] = false;
                    newBoolName[0] = '\0';
                }
                ImGui::TreePop();
            }

            // Float params
            if (ImGui::TreeNode("Float")) {
                std::string toRemove;
                for (auto& [name, val] : sm->floatParams) {
                    ImGui::PushID(name.c_str());
                    ImGui::DragFloat(name.c_str(), &val, 0.1f);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) toRemove = name;
                    ImGui::PopID();
                }
                if (!toRemove.empty()) sm->floatParams.erase(toRemove);

                static char newFloatName[64] = "";
                ImGui::InputText("##newFloat", newFloatName, sizeof(newFloatName));
                ImGui::SameLine();
                if (ImGui::SmallButton("+ Float") && newFloatName[0] != '\0') {
                    sm->floatParams[newFloatName] = 0.0f;
                    newFloatName[0] = '\0';
                }
                ImGui::TreePop();
            }

            // Int params
            if (ImGui::TreeNode("Int")) {
                std::string toRemove;
                for (auto& [name, val] : sm->intParams) {
                    ImGui::PushID(name.c_str());
                    ImGui::DragInt(name.c_str(), &val);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) toRemove = name;
                    ImGui::PopID();
                }
                if (!toRemove.empty()) sm->intParams.erase(toRemove);

                static char newIntName[64] = "";
                ImGui::InputText("##newInt", newIntName, sizeof(newIntName));
                ImGui::SameLine();
                if (ImGui::SmallButton("+ Int") && newIntName[0] != '\0') {
                    sm->intParams[newIntName] = 0;
                    newIntName[0] = '\0';
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }

        // --- States ---
        if (ImGui::TreeNode("States")) {
            i32 stateToRemove = -1;
            for (i32 si = 0; si < static_cast<i32>(sm->states.size()); si++) {
                auto& state = sm->states[si];
                ImGui::PushID(si);

                bool isCurrent = (state.name == sm->currentState);
                if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));

                bool stateOpen = ImGui::TreeNode("##state", "%s%s", state.name.c_str(), isCurrent ? " (active)" : "");

                if (isCurrent) ImGui::PopStyleColor();

                ImGui::SameLine();
                if (ImGui::SmallButton("X##removeState")) stateToRemove = si;

                if (stateOpen) {
                    // State name
                    char nameBuf[64];
                    strncpy(nameBuf, state.name.c_str(), sizeof(nameBuf) - 1);
                    nameBuf[sizeof(nameBuf) - 1] = '\0';
                    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                        // Update currentState reference if renaming the active state
                        if (sm->currentState == state.name) sm->currentState = nameBuf;
                        if (sm->previousState == state.name) sm->previousState = nameBuf;
                        state.name = nameBuf;
                    }

                    // Script callbacks
                    char enterBuf[128], updateBuf[128], exitBuf[128];
                    strncpy(enterBuf, state.onEnter.c_str(), sizeof(enterBuf) - 1);
                    enterBuf[sizeof(enterBuf) - 1] = '\0';
                    strncpy(updateBuf, state.onUpdate.c_str(), sizeof(updateBuf) - 1);
                    updateBuf[sizeof(updateBuf) - 1] = '\0';
                    strncpy(exitBuf, state.onExit.c_str(), sizeof(exitBuf) - 1);
                    exitBuf[sizeof(exitBuf) - 1] = '\0';
                    if (ImGui::InputText("On Enter", enterBuf, sizeof(enterBuf)))
                        state.onEnter = enterBuf;
                    if (ImGui::InputText("On Update", updateBuf, sizeof(updateBuf)))
                        state.onUpdate = updateBuf;
                    if (ImGui::InputText("On Exit", exitBuf, sizeof(exitBuf)))
                        state.onExit = exitBuf;

                    // Transitions
                    i32 transToRemove = -1;
                    for (i32 ti = 0; ti < static_cast<i32>(state.transitions.size()); ti++) {
                        auto& trans = state.transitions[ti];
                        ImGui::PushID(ti);

                        bool transOpen = ImGui::TreeNode("##trans", "-> %s", trans.toState.c_str());
                        ImGui::SameLine();
                        if (ImGui::SmallButton("X##removeTrans")) transToRemove = ti;

                        if (transOpen) {
                            // Target state combo
                            if (ImGui::BeginCombo("Target", trans.toState.c_str())) {
                                for (const auto& s : sm->states) {
                                    if (s.name == state.name) continue; // skip self
                                    bool sel = (trans.toState == s.name);
                                    if (ImGui::Selectable(s.name.c_str(), sel)) {
                                        trans.toState = s.name;
                                    }
                                }
                                ImGui::EndCombo();
                            }

                            // Conditions
                            i32 condToRemove = -1;
                            for (i32 ci = 0; ci < static_cast<i32>(trans.conditions.size()); ci++) {
                                auto& cond = trans.conditions[ci];
                                ImGui::PushID(ci);
                                ImGui::Separator();

                                // Condition type
                                const char* condTypes[] = {"Bool True", "Bool False", "Float >", "Float <",
                                                           "Int ==", "Int !=", "Trigger"};
                                i32 condType = static_cast<i32>(cond.type);
                                if (ImGui::Combo("Type", &condType, condTypes, 7)) {
                                    cond.type = static_cast<ECS::SMConditionType>(condType);
                                }

                                // Param name
                                char paramBuf[64];
                                strncpy(paramBuf, cond.paramName.c_str(), sizeof(paramBuf) - 1);
                                paramBuf[sizeof(paramBuf) - 1] = '\0';
                                if (ImGui::InputText("Param", paramBuf, sizeof(paramBuf))) {
                                    cond.paramName = paramBuf;
                                }

                                // Threshold / intValue based on type
                                if (cond.type == ECS::SMConditionType::FloatGreater ||
                                    cond.type == ECS::SMConditionType::FloatLess) {
                                    ImGui::DragFloat("Threshold", &cond.threshold, 0.1f);
                                }
                                if (cond.type == ECS::SMConditionType::IntEquals ||
                                    cond.type == ECS::SMConditionType::IntNotEquals) {
                                    ImGui::DragInt("Value", &cond.intValue);
                                }

                                if (ImGui::SmallButton("Remove Condition")) condToRemove = ci;

                                ImGui::PopID();
                            }
                            if (condToRemove >= 0) {
                                trans.conditions.erase(trans.conditions.begin() + condToRemove);
                            }

                            if (ImGui::Button("+ Condition")) {
                                trans.conditions.push_back({});
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    if (transToRemove >= 0) {
                        state.transitions.erase(state.transitions.begin() + transToRemove);
                    }

                    if (ImGui::Button("+ Transition")) {
                        ECS::SMTransition t;
                        // Default to first other state
                        for (const auto& s : sm->states) {
                            if (s.name != state.name) { t.toState = s.name; break; }
                        }
                        state.transitions.push_back(t);
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (stateToRemove >= 0) {
                sm->states.erase(sm->states.begin() + stateToRemove);
            }

            if (ImGui::Button("+ State")) {
                ECS::SMState s;
                s.name = "State " + std::to_string(sm->states.size());
                sm->states.push_back(s);
                // Auto-set current state if this is the first one
                if (sm->states.size() == 1) {
                    sm->currentState = s.name;
                }
            }
            ImGui::TreePop();
        }

        // Open in Graph Editor button
        ImGui::Separator();
        if (ImGui::Button("Open in Graph Editor")) {
            SetPanelVisibility(EditorPanel::AnimGraph, true);
            m_AnimGraphEditor.SetTarget(m_World, entity);
        }

        // Play mode: send trigger
        if (m_PlayMode.IsPlaying() || m_PlayMode.IsPaused()) {
            ImGui::Separator();
            static char triggerBuf[64] = "";
            ImGui::InputText("##trigger", triggerBuf, sizeof(triggerBuf));
            ImGui::SameLine();
            if (ImGui::Button("Send Trigger") && triggerBuf[0] != '\0') {
                sm->SendTrigger(triggerBuf);
                triggerBuf[0] = '\0';
            }
        }
    }
}

void EditorLayer::DrawDialogueComponent(ECS::Entity entity) {
    bool dlgOpen = UI::SectionHeader("Dialogue", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("DialogueCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::DialogueComponent>(entity, "dialogue", "Dialogue");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (dlgOpen) {
        auto* dialogue = m_World->GetComponent<ECS::DialogueComponent>(entity);
        if (!dialogue) return;
        DrawComponentHelp("dialogue", m_World, entity);

        // Speaker name
        char nameBuffer[64];
        strncpy(nameBuffer, dialogue->speakerName.c_str(), sizeof(nameBuffer) - 1);
        nameBuffer[sizeof(nameBuffer) - 1] = '\0';
        if (ImGui::InputText("Speaker Name", nameBuffer, sizeof(nameBuffer))) {
            dialogue->speakerName = nameBuffer;
        }
        if (dialogue->speakerName.empty()) {
            auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
            if (nameComp && !nameComp->name.empty()) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Auto-fill")) {
                    dialogue->speakerName = nameComp->name;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set to entity name: %s", nameComp->name.c_str());
            }
        }

        // Portrait path
        char portraitBuffer[256];
        strncpy(portraitBuffer, dialogue->portraitPath.c_str(), sizeof(portraitBuffer) - 1);
        portraitBuffer[sizeof(portraitBuffer) - 1] = '\0';
        if (ImGui::InputText("Portrait Path", portraitBuffer, sizeof(portraitBuffer))) {
            dialogue->portraitPath = portraitBuffer;
        }

        // Typewriter settings
        ImGui::DragFloat("Char Delay", &dialogue->charDelay, 0.01f, 0.01f, 0.5f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Seconds between characters (typewriter effect)");
        }
        ImGui::Checkbox("Play Type Sound", &dialogue->playTypeSound);

        // Status
        ImGui::Separator();
        ImGui::Text("Lines: %zu", dialogue->dialogueLines.size());
        ImGui::Text("Current Line: %u", dialogue->currentLine);
        ImGui::Text("Typing: %s", dialogue->isTyping ? "Yes" : "No");
        ImGui::Text("Waiting for Input: %s", dialogue->waitingForInput ? "Yes" : "No");

        if (dialogue->IsComplete()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Dialogue Complete");
        }

        // Preview current text
        if (!dialogue->dialogueLines.empty()) {
            ImGui::Separator();
            ImGui::Text("Preview:");
            ImGui::TextWrapped("%s", dialogue->GetVisibleText().c_str());
        }

        // Dialogue lines editor
        if (ImGui::TreeNode("Dialogue Lines")) {
            for (usize i = 0; i < dialogue->dialogueLines.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                char lineBuffer[512];
                strncpy(lineBuffer, dialogue->dialogueLines[i].c_str(), sizeof(lineBuffer) - 1);
                lineBuffer[sizeof(lineBuffer) - 1] = '\0';
                if (ImGui::InputTextMultiline("##line", lineBuffer, sizeof(lineBuffer), ImVec2(-1, 60))) {
                    dialogue->dialogueLines[i] = lineBuffer;
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Add Line")) {
                dialogue->dialogueLines.push_back("");
            }
            ImGui::TreePop();
        }

        // Choices editor
        if (ImGui::TreeNode("Choices")) {
            for (usize i = 0; i < dialogue->choices.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode("Choice", "Choice %zu", i)) {
                    char choiceBuffer[256];
                    strncpy(choiceBuffer, dialogue->choices[i].text.c_str(), sizeof(choiceBuffer) - 1);
                    choiceBuffer[sizeof(choiceBuffer) - 1] = '\0';
                    if (ImGui::InputText("Text", choiceBuffer, sizeof(choiceBuffer))) {
                        dialogue->choices[i].text = choiceBuffer;
                    }

                    char nextBuffer[64];
                    strncpy(nextBuffer, dialogue->choices[i].nextDialogueId.c_str(), sizeof(nextBuffer) - 1);
                    nextBuffer[sizeof(nextBuffer) - 1] = '\0';
                    if (ImGui::InputText("Next Dialogue ID", nextBuffer, sizeof(nextBuffer))) {
                        dialogue->choices[i].nextDialogueId = nextBuffer;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (ImGui::Button("Add Choice")) {
                ECS::DialogueComponent::Choice choice;
                choice.text = "Choice";
                dialogue->choices.push_back(choice);
            }
            ImGui::TreePop();
        }

        // --- Dialogue Tree section ---
        ImGui::Separator();
        if (ImGui::TreeNode("Dialogue Tree")) {
            if (dialogue->dialogueTree.nodes.empty()) {
                ImGui::TextWrapped("No dialogue tree. Add a root node to enable tree-based dialogue.");
                if (ImGui::Button("Add Root Node")) {
                    u32 rootId = dialogue->dialogueTree.AddNode(GUI::DialogueNodeType::Root);
                    dialogue->dialogueTree.rootNodeId = rootId;
                    auto* rootNode = dialogue->dialogueTree.GetNode(rootId);
                    if (rootNode) rootNode->editorPosition = Math::Vector2(50, 100);
                }
            } else {
                ImGui::Text("Nodes: %zu", dialogue->dialogueTree.nodes.size());
                ImGui::Text("Tree Name: %s", dialogue->dialogueTree.treeName.empty() ? "(unnamed)" : dialogue->dialogueTree.treeName.c_str());
                if (dlgOpen && ImGui::Button("Open Dialogue Editor")) {
                    m_DialogueTreeEditor.SetTree(&dialogue->dialogueTree);
                    m_DialogueTreeEditor.SetOpen(true);
                }
            }
            ImGui::TreePop();
        }

        // Variables section
        if (ImGui::TreeNode("Dialogue Variables")) {
            i32 removeIdx = -1;
            i32 idx = 0;
            for (auto& [key, val] : dialogue->variables) {
                ImGui::PushID(idx);
                ImGui::Text("%s", key.c_str());
                ImGui::SameLine();
                char valBuf[256];
                strncpy(valBuf, val.c_str(), sizeof(valBuf) - 1);
                valBuf[sizeof(valBuf) - 1] = '\0';
                if (ImGui::InputText("##val", valBuf, sizeof(valBuf))) {
                    val = valBuf;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    removeIdx = idx;
                }
                ImGui::PopID();
                idx++;
            }
            if (removeIdx >= 0) {
                auto it = dialogue->variables.begin();
                std::advance(it, removeIdx);
                dialogue->variables.erase(it);
            }

            static char newKeyBuf[128] = "";
            ImGui::InputText("New Variable", newKeyBuf, sizeof(newKeyBuf));
            ImGui::SameLine();
            if (ImGui::Button("Add") && newKeyBuf[0] != '\0') {
                dialogue->variables[newKeyBuf] = "";
                newKeyBuf[0] = '\0';
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawDialogueBoxComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Dialogue Box", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* box = m_World->GetComponent<ECS::DialogueBoxComponent>(entity);
        if (!box) return;

        if (ImGui::TreeNode("Box Layout")) {
            ImGui::DragFloat("Height", &box->boxHeight, 1.0f, 50.0f, 600.0f);
            ImGui::DragFloat("Margin", &box->boxMargin, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("Padding", &box->boxPadding, 0.5f, 0.0f, 50.0f);
            f32 col[3] = { box->boxColor.x, box->boxColor.y, box->boxColor.z };
            if (ImGui::ColorEdit3("Box Color", col)) {
                box->boxColor = Math::Vector3(col[0], col[1], col[2]);
            }
            ImGui::SliderFloat("Box Alpha", &box->boxAlpha, 0.0f, 1.0f);
            ImGui::DragFloat("Border Radius", &box->boxBorderRadius, 0.5f, 0.0f, 32.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Text Style")) {
            ImGui::DragFloat("Speaker Font Size", &box->speakerFontSize, 0.5f, 8.0f, 48.0f);
            f32 sc[3] = { box->defaultSpeakerColor.x, box->defaultSpeakerColor.y, box->defaultSpeakerColor.z };
            if (ImGui::ColorEdit3("Default Speaker Color", sc)) {
                box->defaultSpeakerColor = Math::Vector3(sc[0], sc[1], sc[2]);
            }
            ImGui::DragFloat("Text Font Size", &box->textFontSize, 0.5f, 8.0f, 48.0f);
            f32 tc[3] = { box->textColor.x, box->textColor.y, box->textColor.z };
            if (ImGui::ColorEdit3("Text Color", tc)) {
                box->textColor = Math::Vector3(tc[0], tc[1], tc[2]);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Portrait")) {
            ImGui::Checkbox("Show Portrait", &box->showPortrait);
            if (box->showPortrait) {
                ImGui::DragFloat("Portrait Size", &box->portraitSize, 1.0f, 32.0f, 256.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Choices")) {
            ImGui::DragFloat("Choice Spacing", &box->choiceSpacing, 0.5f, 0.0f, 24.0f);
            f32 cc[3] = { box->choiceColor.x, box->choiceColor.y, box->choiceColor.z };
            if (ImGui::ColorEdit3("Choice BG Color", cc)) {
                box->choiceColor = Math::Vector3(cc[0], cc[1], cc[2]);
            }
            f32 ctc[3] = { box->choiceTextColor.x, box->choiceTextColor.y, box->choiceTextColor.z };
            if (ImGui::ColorEdit3("Choice Text Color", ctc)) {
                box->choiceTextColor = Math::Vector3(ctc[0], ctc[1], ctc[2]);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Continue Indicator")) {
            char buf[64];
            strncpy(buf, box->continueText.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Text", buf, sizeof(buf))) {
                box->continueText = buf;
            }
            ImGui::DragFloat("Blink Speed", &box->continueBlinkSpeed, 0.1f, 0.5f, 10.0f);
            ImGui::TreePop();
        }

        if (box->initialized) {
            ImGui::TextDisabled("UI elements built (%u panel)", box->panelElementId);
            if (ImGui::Button("Rebuild UI")) {
                box->initialized = false;
            }
        }
    }
}

void EditorLayer::DrawDamageComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Damage", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* dmg = m_World->GetComponent<ECS::DamageComponent>(entity);
        if (!dmg) return;
        DrawComponentHelp("damage", m_World, entity);

        InspectorUndo::DragFloat(m_UndoRedo, "Damage", &dmg->damage, 0.5f, 0.0f, 10000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Knockback Force", &dmg->knockbackForce, 0.5f, 0.0f, 1000.0f);

        const char* types[] = { "Physical", "Fire", "Ice", "Electric", "Poison", "Magic" };
        int type = static_cast<int>(dmg->type);
        if (InspectorUndo::Combo(m_UndoRedo, "Damage Type", &type, types, 6)) {
            dmg->type = static_cast<ECS::DamageComponent::DamageType>(type);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Destroy On Hit", &dmg->destroyOnHit);
        InspectorUndo::Checkbox(m_UndoRedo, "Damage Once Per Entity", &dmg->damageOnce);

        if (!dmg->damageOnce) {
            InspectorUndo::DragFloat(m_UndoRedo, "Damage Interval", &dmg->damageInterval, 0.1f, 0.0f, 10.0f);
        }

        if (ImGui::BeginPopupContextItem("DamageContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::DamageComponent>(entity, "damage", "Damage");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawInteractableComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Interactable", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* inter = m_World->GetComponent<ECS::InteractableComponent>(entity);
        if (!inter) return;
        DrawComponentHelp("interactable", m_World, entity);

        char promptBuffer[256];
        strncpy(promptBuffer, inter->promptText.c_str(), sizeof(promptBuffer) - 1);
        promptBuffer[sizeof(promptBuffer) - 1] = '\0';
        if (InspectorUndo::InputText(m_UndoRedo, "Prompt Text", promptBuffer, sizeof(promptBuffer), [inter](const std::string& val) { inter->promptText = val; })) {
            inter->promptText = promptBuffer;
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Interaction Range", &inter->interactionRange, 0.1f, 0.1f, 50.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Requires Look At", &inter->requiresLookAt);
        if (inter->requiresLookAt) {
            InspectorUndo::DragFloat(m_UndoRedo, "Look At Angle", &inter->lookAtAngle, 1.0f, 1.0f, 180.0f);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &inter->isEnabled);
        InspectorUndo::Checkbox(m_UndoRedo, "Single Use", &inter->singleUse);
        if (inter->singleUse) {
            ImGui::SameLine();
            ImGui::Text("(%s)", inter->hasBeenUsed ? "Used" : "Available");
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Highlight On Hover", &inter->highlightOnHover);
        if (inter->highlightOnHover) {
            f32 col[3] = { inter->highlightColor.x, inter->highlightColor.y, inter->highlightColor.z };
            if (InspectorUndo::ColorEdit3(m_UndoRedo, "Highlight Color", col, [inter](f32 r, f32 g, f32 b) { inter->highlightColor = Math::Vector3(r, g, b); })) {
                inter->highlightColor = Math::Vector3(col[0], col[1], col[2]);
            }
        }

        if (ImGui::BeginPopupContextItem("InteractableContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::InteractableComponent>(entity, "interactable", "Interactable");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawPickupComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Pickup", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* pickup = m_World->GetComponent<ECS::PickupComponent>(entity);
        if (!pickup) return;
        DrawComponentHelp("pickup", m_World, entity);

        const char* types[] = { "Health", "Ammo", "Coin", "Key", "Powerup", "Custom" };
        int type = static_cast<int>(pickup->type);
        if (InspectorUndo::Combo(m_UndoRedo, "Pickup Type", &type, types, 6)) {
            pickup->type = static_cast<ECS::PickupComponent::PickupType>(type);
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Value", &pickup->value, 0.5f, 0.0f, 10000.0f);

        if (pickup->type == ECS::PickupComponent::PickupType::Custom) {
            char idBuffer[128];
            strncpy(idBuffer, pickup->customId.c_str(), sizeof(idBuffer) - 1);
            idBuffer[sizeof(idBuffer) - 1] = '\0';
            if (InspectorUndo::InputText(m_UndoRedo, "Custom ID", idBuffer, sizeof(idBuffer), [pickup](const std::string& val) { pickup->customId = val; })) {
                pickup->customId = idBuffer;
            }
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Pickup Range", &pickup->pickupRange, 0.1f, 0.1f, 50.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Destroy On Pickup", &pickup->destroyOnPickup);

        if (ImGui::TreeNode("Magnet")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Magnet To Player", &pickup->magnetToPlayer);
            if (pickup->magnetToPlayer) {
                InspectorUndo::DragFloat(m_UndoRedo, "Magnet Range", &pickup->magnetRange, 0.5f, 0.1f, 50.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Magnet Speed", &pickup->magnetSpeed, 0.5f, 0.1f, 100.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Respawn")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Can Respawn", &pickup->canRespawn);
            if (pickup->canRespawn) {
                InspectorUndo::DragFloat(m_UndoRedo, "Respawn Time", &pickup->respawnTime, 0.5f, 0.0f, 300.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Visual")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Bob Speed", &pickup->bobSpeed, 0.1f, 0.0f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Bob Height", &pickup->bobHeight, 0.01f, 0.0f, 2.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Rotation Speed", &pickup->rotationSpeed, 5.0f, 0.0f, 720.0f);
            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::Text("Status: %s", pickup->isCollected ? "Collected" : "Available");

        if (ImGui::BeginPopupContextItem("PickupContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::PickupComponent>(entity, "pickup", "Pickup");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawInventoryComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Inventory", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* inv = m_World->GetComponent<ECS::InventoryComponent>(entity);
        if (!inv) return;
        DrawComponentHelp("inventory", m_World, entity);

        int maxSlots = static_cast<int>(inv->maxSlots);
        if (ImGui::InputInt("Max Slots", &maxSlots)) {
            inv->maxSlots = static_cast<usize>(maxSlots > 0 ? maxSlots : 1);
        }

        InspectorUndo::DragInt(m_UndoRedo, "Coins", &inv->coins, 1, 0, 999999);
        InspectorUndo::DragInt(m_UndoRedo, "Gems", &inv->gems, 1, 0, 999999);

        if (ImGui::TreeNode("Keys")) {
            for (usize i = 0; i < inv->keys.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                char keyBuffer[128];
                strncpy(keyBuffer, inv->keys[i].c_str(), sizeof(keyBuffer) - 1);
                keyBuffer[sizeof(keyBuffer) - 1] = '\0';
                if (ImGui::InputText("##key", keyBuffer, sizeof(keyBuffer))) {
                    inv->keys[i] = keyBuffer;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    inv->keys.erase(inv->keys.begin() + i);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (ImGui::SmallButton("Add Key")) {
                inv->keys.push_back("new_key");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Slots")) {
            ImGui::Text("Used: %zu / %zu", inv->slots.size(), inv->maxSlots);
            for (usize i = 0; i < inv->slots.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto& slot = inv->slots[i];
                ImGui::Text("[%zu] %s x%d (max %d)", i, slot.itemId.c_str(), slot.quantity, slot.maxStack);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("InventoryContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::InventoryComponent>(entity, "inventory", "Inventory");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawTimerComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Timer", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* timer = m_World->GetComponent<ECS::TimerComponent>(entity);
        if (!timer) return;
        DrawComponentHelp("timer", m_World, entity);

        InspectorUndo::DragFloat(m_UndoRedo, "Duration", &timer->duration, 0.1f, 0.01f, 3600.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Loop", &timer->loop);
        InspectorUndo::Checkbox(m_UndoRedo, "Auto Start", &timer->autoStart);

        // Progress bar
        f32 progress = timer->GetProgress();
        ImGui::ProgressBar(progress, ImVec2(-1, 0),
            (std::to_string((int)(timer->GetRemaining() * 10) / 10.0f) + "s remaining").c_str());

        ImGui::Separator();
        ImGui::Text("Running: %s", timer->isRunning ? "Yes" : "No");
        ImGui::Text("Loop Count: %d", timer->loopCount);
        ImGui::Text("Complete: %s", timer->IsComplete() ? "Yes" : "No");

        if (ImGui::BeginPopupContextItem("TimerContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::TimerComponent>(entity, "timer", "Timer");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawGameOverComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Game Over", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* go = m_World->GetComponent<ECS::GameOverComponent>(entity);
        if (!go) return;
        DrawComponentHelp("gameOver", m_World, entity);

        // Messages
        char victoryBuf[256];
        strncpy(victoryBuf, go->victoryMessage.c_str(), sizeof(victoryBuf) - 1);
        victoryBuf[sizeof(victoryBuf) - 1] = '\0';
        if (InspectorUndo::InputText(m_UndoRedo, "Victory Message", victoryBuf, sizeof(victoryBuf),
                [go](const std::string& val) { go->victoryMessage = val; })) {
            go->victoryMessage = victoryBuf;
        }

        char defeatBuf[256];
        strncpy(defeatBuf, go->defeatMessage.c_str(), sizeof(defeatBuf) - 1);
        defeatBuf[sizeof(defeatBuf) - 1] = '\0';
        if (InspectorUndo::InputText(m_UndoRedo, "Defeat Message", defeatBuf, sizeof(defeatBuf),
                [go](const std::string& val) { go->defeatMessage = val; })) {
            go->defeatMessage = defeatBuf;
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Delay", &go->delay, 0.1f, 0.0f, 10.0f);
        ImGui::SetItemTooltip("Seconds before the game over screen appears");

        InspectorUndo::Checkbox(m_UndoRedo, "Allow Restart", &go->allowRestart);
        InspectorUndo::Checkbox(m_UndoRedo, "Return to Menu", &go->returnToMenu);

        ImGui::Separator();
        ImGui::Text("Victory Conditions");

        InspectorUndo::Checkbox(m_UndoRedo, "All Enemies Defeated", &go->victoryOnAllEnemiesDefeated);
        ImGui::SetItemTooltip("Win when all entities with Damage+Health (non-player) are dead");

        // Victory trigger entity
        u64 triggerID = static_cast<u64>(go->victoryTriggerEntity);
        if (ImGui::InputScalar("Victory Trigger Entity", ImGuiDataType_U64, &triggerID)) {
            go->victoryTriggerEntity = static_cast<ECS::Entity>(triggerID);
        }
        ImGui::SetItemTooltip("Entity ID of a TriggerZone that triggers victory when a player enters (0 = disabled)");

        // Runtime status
        ImGui::Separator();
        ImGui::Text("Status: %s", go->triggered ? (go->won ? "VICTORY" : "DEFEAT") : "Waiting");

        if (ImGui::BeginPopupContextItem("GameOverContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::GameOverComponent>(entity, "gameOver", "Game Over");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawAIControllerComponent(ECS::Entity entity) {
    if (UI::SectionHeader("AI Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* ai = m_World->GetComponent<ECS::AIControllerComponent>(entity);
        if (!ai) return;
        DrawComponentHelp("aiController", m_World, entity);

        const char* states[] = { "Idle", "Patrol", "Chase", "Attack", "Flee", "Dead" };
        int state = static_cast<int>(ai->currentState);
        if (InspectorUndo::Combo(m_UndoRedo, "Current State", &state, states, 6)) {
            ai->currentState = static_cast<ECS::AIControllerComponent::AIState>(state);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "2D Mode", &ai->is2D);

        if (ImGui::TreeNode("Detection")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Detection Range", &ai->detectionRange, 0.5f, 0.0f, 200.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Attack Range", &ai->attackRange, 0.5f, 0.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Lose Target Range", &ai->loseTargetRange, 0.5f, 0.0f, 300.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Field of View", &ai->fieldOfView, 1.0f, 0.0f, 360.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Move Speed", &ai->moveSpeed, 0.1f, 0.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Chase Speed", &ai->chaseSpeed, 0.1f, 0.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Flee Speed", &ai->fleeSpeed, 0.1f, 0.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Turn Speed", &ai->turnSpeed, 5.0f, 0.0f, 720.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Stopping Distance", &ai->stoppingDistance, 0.1f, 0.0f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Arrival Radius", &ai->arrivalRadius, 0.1f, 0.1f, 5.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Attack")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Attack Cooldown", &ai->attackCooldown, 0.1f, 0.0f, 30.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Attack Damage", &ai->attackDamage, 0.5f, 0.0f, 1000.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Patrol")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Wait Time", &ai->patrolWaitTime, 0.1f, 0.0f, 30.0f);
            InspectorUndo::Checkbox(m_UndoRedo, "Loop Patrol", &ai->patrolLoop);
            ImGui::Text("Patrol Points: %zu", ai->patrolPoints.size());
            ImGui::Text("Current Index: %zu", ai->currentPatrolIndex);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Navigation (A* Pathfinding)")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Use Navmesh", &ai->useNavmesh);
            if (ai->useNavmesh) {
                InspectorUndo::DragFloat(m_UndoRedo, "Repath Interval", &ai->repathInterval, 0.1f, 0.0f, 5.0f,
                                 "%.1f s");
                InspectorUndo::DragFloat(m_UndoRedo, "Flee Distance", &ai->fleeDistance, 0.5f, 1.0f, 100.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Target")) {
            u64 tgtId = static_cast<u64>(ai->targetEntity);
            std::string currentLabel = "None";
            if (ai->targetEntity != 0 && ai->targetEntity != ECS::INVALID_ENTITY) {
                auto* tgtName = m_World->GetComponent<ECS::NameComponent>(ai->targetEntity);
                currentLabel = tgtName ? tgtName->name : ("Entity " + std::to_string(ai->targetEntity));
            }
            if (ImGui::BeginCombo("Target Entity", currentLabel.c_str())) {
                if (ImGui::Selectable("None", ai->targetEntity == 0)) {
                    ai->targetEntity = 0;
                }
                for (ECS::Entity e : m_World->GetAllEntities()) {
                    if (e == entity) continue;
                    auto* n = m_World->GetComponent<ECS::NameComponent>(e);
                    std::string label = n ? n->name : ("Entity " + std::to_string(e));
                    if (ImGui::Selectable(label.c_str(), ai->targetEntity == e)) {
                        ai->targetEntity = e;
                    }
                }
                ImGui::EndCombo();
            }
            if (ai->targetEntity == 0 || ai->targetEntity == ECS::INVALID_ENTITY) {
                ECS::Entity player = FindPlayerEntity();
                if (player != ECS::INVALID_ENTITY && player != entity) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Target \"Player\"")) {
                        ai->targetEntity = player;
                    }
                }
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Debug")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Draw Path", &ai->debugDrawPath);
            InspectorUndo::Checkbox(m_UndoRedo, "Draw Detection", &ai->debugDrawDetection);
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("AIControllerContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::AIControllerComponent>(entity, "aiController", "AI Controller");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawFollowTargetComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Follow Target", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* follow = m_World->GetComponent<ECS::FollowTargetComponent>(entity);
        if (!follow) return;
        DrawComponentHelp("followTarget", m_World, entity);

        ImGui::Text("Target Entity: %llu", (unsigned long long)follow->target);
        InspectorUndo::DragFloat(m_UndoRedo, "Follow Distance", &follow->followDistance, 0.1f, 0.0f, 100.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Min Distance", &follow->minDistance, 0.1f, 0.0f, follow->followDistance);
        InspectorUndo::DragFloat(m_UndoRedo, "Max Distance", &follow->maxDistance, 0.5f, follow->followDistance, 1000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Move Speed", &follow->moveSpeed, 0.1f, 0.0f, 50.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Smooth Time", &follow->smoothTime, 0.01f, 0.0f, 5.0f);

        InspectorUndo::Checkbox(m_UndoRedo, "Match Target Rotation", &follow->matchTargetRotation);
        if (follow->matchTargetRotation) {
            InspectorUndo::DragFloat(m_UndoRedo, "Rotation Speed", &follow->rotationSpeed, 5.0f, 0.0f, 720.0f);
        }

        f32 offset[3] = { follow->offset.x, follow->offset.y, follow->offset.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Offset", offset, [follow](f32 x, f32 y, f32 z) { follow->offset = Math::Vector3(x, y, z); }, 0.1f)) {
            follow->offset = Math::Vector3(offset[0], offset[1], offset[2]);
        }
        InspectorUndo::Checkbox(m_UndoRedo, "Use Local Offset", &follow->useLocalOffset);

        if (ImGui::BeginPopupContextItem("FollowTargetContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::FollowTargetComponent>(entity, "followTarget", "Follow Target");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawLookAtTargetComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Look At Target", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* lookAt = m_World->GetComponent<ECS::LookAtTargetComponent>(entity);
        if (!lookAt) return;
        DrawComponentHelp("lookAtTarget", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Use World Position", &lookAt->useWorldTarget);
        if (lookAt->useWorldTarget) {
            f32 target[3] = { lookAt->worldTarget.x, lookAt->worldTarget.y, lookAt->worldTarget.z };
            if (InspectorUndo::DragFloat3(m_UndoRedo, "World Target", target, [lookAt](f32 x, f32 y, f32 z) { lookAt->worldTarget = Math::Vector3(x, y, z); }, 0.1f)) {
                lookAt->worldTarget = Math::Vector3(target[0], target[1], target[2]);
            }
        } else {
            ImGui::Text("Target Entity: %llu", (unsigned long long)lookAt->target);
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Rotation Speed", &lookAt->rotationSpeed, 5.0f, 0.0f, 720.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Instant", &lookAt->instant);

        if (ImGui::TreeNode("Constraints")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Constrain X", &lookAt->constrainX);
            InspectorUndo::Checkbox(m_UndoRedo, "Constrain Y", &lookAt->constrainY);
            InspectorUndo::Checkbox(m_UndoRedo, "Constrain Z", &lookAt->constrainZ);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Limits")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Min Yaw", &lookAt->minYaw, 1.0f, -180.0f, lookAt->maxYaw);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Yaw", &lookAt->maxYaw, 1.0f, lookAt->minYaw, 180.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Min Pitch", &lookAt->minPitch, 1.0f, -89.0f, lookAt->maxPitch);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Pitch", &lookAt->maxPitch, 1.0f, lookAt->minPitch, 89.0f);
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("LookAtTargetContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::LookAtTargetComponent>(entity, "lookAtTarget", "Look At Target");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawVirtualCameraComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Virtual Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* vc = m_World->GetComponent<ECS::VirtualCameraComponent>(entity);
        if (!vc) return;
        DrawComponentHelp("virtualCamera", m_World, entity);

        // Live indicator — the Director publishes isLive on the winning vcam.
        if (vc->isLive) {
            ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.4f, 1.0f), "@ LIVE");
        } else {
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.6f, 1.0f), "  standby");
        }

        // Shot preset — the tutorialless front door. Pick one and the framing
        // fields fill in with a good shot; everything stays editable below.
        {
            ImGui::TextUnformatted("Shot Preset");
            ImGui::SameLine(120.0f);
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::BeginCombo("##vcamShot", ECS::VCamShotName(vc->shot))) {
                for (u8 s = 0; s < static_cast<u8>(ECS::VCamShot::Count); ++s) {
                    ECS::VCamShot shot = static_cast<ECS::VCamShot>(s);
                    bool sel = (vc->shot == shot);
                    if (ImGui::Selectable(ECS::VCamShotName(shot), sel)) {
                        ECS::ApplyVCamPreset(*vc, shot);  // seed framing, keep targets/priority
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &vc->enabled);
        InspectorUndo::DragInt(m_UndoRedo, "Priority", &vc->priority, 1, -1000, 1000);

        // Entity-reference fields: drag an entity from the Hierarchy onto the slot.
        auto entityField = [&](const char* label, ECS::Entity& ref) {
            std::string slot = "(this entity)";
            if (ref != 0 && m_World->IsValid(ref)) {
                auto* nc = m_World->GetComponent<ECS::NameComponent>(ref);
                slot = nc ? nc->name : ("Entity " + std::to_string((u64)ref));
            } else if (ref != 0) {
                slot = "(missing)";
            }
            ImGui::Text("%s", label);
            ImGui::SameLine(120.0f);
            ImGui::Button((slot + "##vcam_" + label).c_str(), ImVec2(150.0f, 0.0f));
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ENTITY_REPARENT")) {
                    if (pl->Data && pl->DataSize >= static_cast<int>(sizeof(ECS::Entity))) {
                        ref = *static_cast<const ECS::Entity*>(pl->Data);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if (ref != 0) {
                ImGui::SameLine();
                if (ImGui::SmallButton((std::string("x##clr_") + label).c_str())) ref = 0;
            }
        };
        entityField("Follow", vc->follow);
        entityField("Look At", vc->lookAt);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f),
                           "(drag an entity from the Hierarchy onto a slot)");

        // Editing any framing field means it's no longer a stock preset.
        bool edited = false;
        f32 off[3] = { vc->offset.x, vc->offset.y, vc->offset.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Offset", off,
                [vc](f32 x, f32 y, f32 z) { vc->offset = Math::Vector3(x, y, z); }, 0.1f)) {
            vc->offset = Math::Vector3(off[0], off[1], off[2]); edited = true;
        }
        if (InspectorUndo::Checkbox(m_UndoRedo, "Offset follows facing", &vc->offsetInFollowSpace)) edited = true;
        ImGui::SameLine(); ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("On: offset rotates with the follow target's facing (over-shoulder, follow).\n"
                              "Off: offset is fixed in world axes (isometric, top-down).");
        }
        f32 lo[3] = { vc->lookOffset.x, vc->lookOffset.y, vc->lookOffset.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Look Offset", lo,
                [vc](f32 x, f32 y, f32 z) { vc->lookOffset = Math::Vector3(x, y, z); }, 0.1f)) {
            vc->lookOffset = Math::Vector3(lo[0], lo[1], lo[2]); edited = true;
        }
        if (InspectorUndo::DragFloat(m_UndoRedo, "FOV", &vc->fov, 0.5f, 10.0f, 120.0f)) edited = true;
        if (InspectorUndo::DragFloat(m_UndoRedo, "Damping (s)", &vc->damping, 0.02f, 0.0f, 3.0f)) edited = true;
        if (InspectorUndo::DragFloat(m_UndoRedo, "Blend Time (s)", &vc->blendTime, 0.02f, 0.0f, 4.0f)) edited = true;
        if (edited) vc->shot = ECS::VCamShot::Custom;

        if (ImGui::BeginPopupContextItem("VirtualCameraContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::VirtualCameraComponent>(entity, "virtualCamera", "Virtual Camera");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawWaypointComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Waypoint", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* wp = m_World->GetComponent<ECS::WaypointComponent>(entity);
        if (!wp) return;
        DrawComponentHelp("waypoint", m_World, entity);

        char idBuffer[128];
        strncpy(idBuffer, wp->waypointId.c_str(), sizeof(idBuffer) - 1);
        idBuffer[sizeof(idBuffer) - 1] = '\0';
        if (InspectorUndo::InputText(m_UndoRedo, "Waypoint ID", idBuffer, sizeof(idBuffer), [wp](const std::string& val) { wp->waypointId = val; })) {
            wp->waypointId = idBuffer;
        }

        ImGui::InputInt("Index", &wp->index);
        ImGui::Text("Next Waypoint: %llu", (unsigned long long)wp->nextWaypoint);
        InspectorUndo::DragFloat(m_UndoRedo, "Wait Time", &wp->waitTime, 0.1f, 0.0f, 60.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Radius", &wp->radius, 0.05f, 0.01f, 10.0f);

        if (ImGui::BeginPopupContextItem("WaypointContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::WaypointComponent>(entity, "waypoint", "Waypoint");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawBillboardComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Billboard", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* bb = m_World->GetComponent<ECS::BillboardComponent>(entity);
        if (!bb) return;
        DrawComponentHelp("billboard", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Face Camera", &bb->faceCamera);
        InspectorUndo::Checkbox(m_UndoRedo, "Lock Y Axis", &bb->lockY);
        InspectorUndo::DragFloat(m_UndoRedo, "Rotation Offset", &bb->rotationOffset, 1.0f, -180.0f, 180.0f);

        if (ImGui::BeginPopupContextItem("BillboardContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::BillboardComponent>(entity, "billboard", "Billboard");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawParticleEmitterComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* emitter = m_World->GetComponent<ECS::ParticleEmitterComponent>(entity);
        if (!emitter) return;
        DrawComponentHelp("particleEmitter", m_World, entity);

        // Preset dropdown
        {
            const char* presetNames[] = { "(None)", "Fire", "Smoke", "Sparks", "Snow", "Rain",
                "Magic", "Explosion", "Water Splash", "Blood/Sap", "Lava", "Fountain", "Drip" };
            static int currentPreset = 0;
            if (ImGui::Combo("Preset", &currentPreset, presetNames, IM_ARRAYSIZE(presetNames))) {
                if (currentPreset > 0) {
                    ECS::ApplyParticlePreset(*emitter, presetNames[currentPreset]);
                    currentPreset = 0;
                }
            }
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Playing", &emitter->isPlaying);
        InspectorUndo::Checkbox(m_UndoRedo, "Play On Awake", &emitter->playOnAwake);
        InspectorUndo::Checkbox(m_UndoRedo, "Loop", &emitter->loop);

        if (ImGui::TreeNode("Emission")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Rate (per sec)", &emitter->emissionRate, 0.5f, 0.0f, 1000.0f);
            InspectorUndo::DragInt(m_UndoRedo, "Burst Count", &emitter->burstCount, 1, 0, 100);
            if (emitter->burstCount > 0) {
                InspectorUndo::DragFloat(m_UndoRedo, "Burst Interval", &emitter->burstInterval, 0.1f, 0.0f, 30.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Particle Properties")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Lifetime", &emitter->lifetime, 0.1f, 0.01f, 60.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Lifetime Variance", &emitter->lifetimeVariance, 0.1f, 0.0f, emitter->lifetime);
            InspectorUndo::DragFloat(m_UndoRedo, "Start Speed", &emitter->startSpeed, 0.1f, 0.0f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Speed Variance", &emitter->speedVariance, 0.1f, 0.0f, emitter->startSpeed);
            InspectorUndo::DragFloat(m_UndoRedo, "Start Size", &emitter->startSize, 0.01f, 0.001f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "End Size", &emitter->endSize, 0.01f, 0.0f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Start Alpha", &emitter->startAlpha, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "End Alpha", &emitter->endAlpha, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Colors")) {
            f32 startCol[3] = { emitter->startColor.x, emitter->startColor.y, emitter->startColor.z };
            if (InspectorUndo::ColorEdit3(m_UndoRedo, "Start Color", startCol, [emitter](f32 r, f32 g, f32 b) { emitter->startColor = Math::Vector3(r, g, b); })) {
                emitter->startColor = Math::Vector3(startCol[0], startCol[1], startCol[2]);
            }
            f32 endCol[3] = { emitter->endColor.x, emitter->endColor.y, emitter->endColor.z };
            if (InspectorUndo::ColorEdit3(m_UndoRedo, "End Color", endCol, [emitter](f32 r, f32 g, f32 b) { emitter->endColor = Math::Vector3(r, g, b); })) {
                emitter->endColor = Math::Vector3(endCol[0], endCol[1], endCol[2]);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Shape")) {
            const char* shapes[] = { "Point", "Sphere", "Hemisphere", "Cone", "Box" };
            int shape = static_cast<int>(emitter->shape);
            if (InspectorUndo::Combo(m_UndoRedo, "Shape", &shape, shapes, 5)) {
                emitter->shape = static_cast<ECS::ParticleEmitterComponent::EmitterShape>(shape);
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Shape Radius", &emitter->shapeRadius, 0.05f, 0.0f, 50.0f);
            if (emitter->shape == ECS::ParticleEmitterComponent::EmitterShape::Cone) {
                InspectorUndo::DragFloat(m_UndoRedo, "Cone Angle", &emitter->coneAngle, 1.0f, 0.0f, 90.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Forces")) {
            f32 gravity[3] = { emitter->gravity.x, emitter->gravity.y, emitter->gravity.z };
            if (InspectorUndo::DragFloat3(m_UndoRedo, "Gravity", gravity, [emitter](f32 x, f32 y, f32 z) { emitter->gravity = Math::Vector3(x, y, z); }, 0.1f)) {
                emitter->gravity = Math::Vector3(gravity[0], gravity[1], gravity[2]);
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Drag", &emitter->drag, 0.01f, 0.0f, 10.0f);
            InspectorUndo::Checkbox(m_UndoRedo, "Use Scene Wind", &emitter->useSceneWind);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Push these particles with the world's wind (WindSystem / Weather),\nso they drift and gust like leaves, dust, snow, or embers.");
            if (emitter->useSceneWind) {
                InspectorUndo::DragFloat(m_UndoRedo, "Wind Influence", &emitter->windInfluence, 0.05f, 0.0f, 30.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("How strongly the scene wind pushes these particles.");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Texture")) {
            char pathBuffer[256];
            strncpy(pathBuffer, emitter->texturePath.c_str(), sizeof(pathBuffer) - 1);
            pathBuffer[sizeof(pathBuffer) - 1] = '\0';
            if (InspectorUndo::InputText(m_UndoRedo, "Texture Path", pathBuffer, sizeof(pathBuffer), [emitter](const std::string& val) { emitter->texturePath = val; })) {
                emitter->texturePath = pathBuffer;
            }
            InspectorUndo::DragInt(m_UndoRedo, "Sheet X", &emitter->textureSheetX, 1, 1, 16);
            InspectorUndo::DragInt(m_UndoRedo, "Sheet Y", &emitter->textureSheetY, 1, 1, 16);
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("ParticleEmitterContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::ParticleEmitterComponent>(entity, "particleEmitter", "Particle Emitter");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawCamera2DBoundsComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Camera 2D Bounds", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* bounds = m_World->GetComponent<ECS::Camera2DBoundsComponent>(entity);
        if (!bounds) return;
        DrawComponentHelp("camera2DBounds", m_World, entity);

        // Bounds section
        if (ImGui::TreeNode("Bounds")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Use Bounds", &bounds->useBounds);
            if (bounds->useBounds) {
                f32 minB[2] = { bounds->minBounds.x, bounds->minBounds.y };
                if (ImGui::DragFloat2("Min Bounds", minB, 0.5f)) {
                    bounds->minBounds = Math::Vector2(minB[0], minB[1]);
                }
                f32 maxB[2] = { bounds->maxBounds.x, bounds->maxBounds.y };
                if (ImGui::DragFloat2("Max Bounds", maxB, 0.5f)) {
                    bounds->maxBounds = Math::Vector2(maxB[0], maxB[1]);
                }
                InspectorUndo::DragFloat(m_UndoRedo, "Padding", &bounds->boundsPadding, 0.1f, 0.0f, 100.0f);
            }
            ImGui::TreePop();
        }

        // Follow section
        if (ImGui::TreeNode("Follow")) {
            ImGui::Text("Target: %llu", (unsigned long long)bounds->followTarget);
            // Entity picker for follow target
            if (ImGui::BeginCombo("##FollowTarget", bounds->followTarget ? "Selected" : "None")) {
                if (ImGui::Selectable("None", bounds->followTarget == 0)) {
                    bounds->followTarget = 0;
                }
                for (ECS::Entity e : m_World->GetAllEntities()) {
                    if (e == entity) continue;
                    auto* name = m_World->GetComponent<ECS::NameComponent>(e);
                    std::string label = name ? name->name : ("Entity " + std::to_string(e));
                    if (ImGui::Selectable(label.c_str(), bounds->followTarget == e)) {
                        bounds->followTarget = e;
                    }
                }
                ImGui::EndCombo();
            }
            if (bounds->followTarget == 0) {
                ECS::Entity player = FindPlayerEntity();
                if (player != ECS::INVALID_ENTITY && player != entity) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Follow \"Player\"")) {
                        bounds->followTarget = player;
                    }
                }
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Smoothing", &bounds->followSmoothing, 0.1f, 0.1f, 50.0f);
            f32 offset[2] = { bounds->followOffset.x, bounds->followOffset.y };
            if (ImGui::DragFloat2("Offset", offset, 0.1f)) {
                bounds->followOffset = Math::Vector2(offset[0], offset[1]);
            }
            ImGui::TreePop();
        }

        // Dead Zone section
        if (ImGui::TreeNode("Dead Zone")) {
            f32 dz[2] = { bounds->deadZoneSize.x, bounds->deadZoneSize.y };
            if (ImGui::DragFloat2("Size", dz, 0.1f, 0.0f, 20.0f)) {
                bounds->deadZoneSize = Math::Vector2(dz[0], dz[1]);
            }
            ImGui::TextDisabled("Camera won't move until target exits this region");
            ImGui::TreePop();
        }

        // Look Ahead section
        if (ImGui::TreeNode("Look Ahead")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Distance", &bounds->lookAheadDistance, 0.1f, 0.0f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Smoothing", &bounds->lookAheadSmoothing, 0.1f, 0.1f, 20.0f);
            ImGui::TextDisabled("Camera leads in movement direction");
            ImGui::TreePop();
        }

        // Screen Shake section
        if (ImGui::TreeNode("Screen Shake")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Frequency", &bounds->shakeFrequency, 0.5f, 1.0f, 50.0f);
            if (ImGui::Button("Test Shake")) {
                bounds->TriggerShake(0.5f, 0.3f);
            }
            ImGui::TextDisabled("Call Camera2D_Shake() from script");
            ImGui::TreePop();
        }

        // Zoom section
        if (ImGui::TreeNode("Zoom")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Current", &bounds->currentZoom, 0.05f, bounds->minZoom, bounds->maxZoom);
            InspectorUndo::DragFloat(m_UndoRedo, "Target", &bounds->targetZoom, 0.05f, bounds->minZoom, bounds->maxZoom);
            InspectorUndo::DragFloat(m_UndoRedo, "Min", &bounds->minZoom, 0.05f, 0.1f, bounds->maxZoom);
            InspectorUndo::DragFloat(m_UndoRedo, "Max", &bounds->maxZoom, 0.05f, bounds->minZoom, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Smoothing", &bounds->zoomSmoothing, 0.1f, 0.0f, 20.0f);
            ImGui::TreePop();
        }

        // Multi-Target section
        if (ImGui::TreeNode("Multi-Target")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Auto Zoom to Fit", &bounds->autoZoomToFitTargets);
            InspectorUndo::DragFloat(m_UndoRedo, "Padding", &bounds->multiTargetPadding, 0.1f, 0.0f, 20.0f);

            ImGui::Text("Additional Targets (%zu):", bounds->additionalTargets.size());
            for (usize i = 0; i < bounds->additionalTargets.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                ECS::Entity t = bounds->additionalTargets[i];
                auto* name = m_World->GetComponent<ECS::NameComponent>(t);
                std::string label = name ? name->name : ("Entity " + std::to_string(t));
                ImGui::Text("  - %s", label.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    bounds->additionalTargets.erase(bounds->additionalTargets.begin() + i);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            if (ImGui::BeginCombo("Add Target", "Select...")) {
                for (ECS::Entity e : m_World->GetAllEntities()) {
                    if (e == entity || e == bounds->followTarget) continue;
                    if (std::find(bounds->additionalTargets.begin(), bounds->additionalTargets.end(), e) != bounds->additionalTargets.end()) continue;
                    auto* name = m_World->GetComponent<ECS::NameComponent>(e);
                    std::string label = name ? name->name : ("Entity " + std::to_string(e));
                    if (ImGui::Selectable(label.c_str())) {
                        bounds->additionalTargets.push_back(e);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("Camera2DBoundsContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::Camera2DBoundsComponent>(entity, "camera2DBounds", "2D Camera Bounds");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawTagComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* tags = m_World->GetComponent<ECS::TagComponent>(entity);
        if (!tags) return;
        DrawComponentHelp("tag", m_World, entity);

        for (usize i = 0; i < tags->tags.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            char tagBuffer[128];
            strncpy(tagBuffer, tags->tags[i].c_str(), sizeof(tagBuffer) - 1);
            tagBuffer[sizeof(tagBuffer) - 1] = '\0';
            if (ImGui::InputText("##tag", tagBuffer, sizeof(tagBuffer))) {
                tags->tags[i] = tagBuffer;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                tags->tags.erase(tags->tags.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        if (ImGui::Button("Add Tag")) {
            tags->tags.push_back("new_tag");
        }

        if (ImGui::BeginPopupContextItem("TagContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::TagComponent>(entity, "tag", "Tags");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawSpawnPointComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Spawn Point", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* spawn = m_World->GetComponent<ECS::SpawnPointComponent>(entity);
        if (!spawn) return;
        DrawComponentHelp("spawnPoint", m_World, entity);

        char idBuffer[128];
        strncpy(idBuffer, spawn->spawnId.c_str(), sizeof(idBuffer) - 1);
        idBuffer[sizeof(idBuffer) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Spawn ID", idBuffer, sizeof(idBuffer), [spawn](const std::string& val) { spawn->spawnId = val; });

        char prefabBuffer[256];
        strncpy(prefabBuffer, spawn->prefabToSpawn.c_str(), sizeof(prefabBuffer) - 1);
        prefabBuffer[sizeof(prefabBuffer) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Prefab To Spawn", prefabBuffer, sizeof(prefabBuffer), [spawn](const std::string& val) { spawn->prefabToSpawn = val; });

        InspectorUndo::Checkbox(m_UndoRedo, "Spawn On Start", &spawn->spawnOnStart);
        InspectorUndo::DragFloat(m_UndoRedo, "Spawn Delay", &spawn->spawnDelay, 0.1f, 0.0f, 60.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Respawn Time", &spawn->respawnTime, 0.5f, 0.0f, 300.0f);

        int maxSpawns = spawn->maxSpawns;
        if (ImGui::InputInt("Max Spawns (-1 = unlimited)", &maxSpawns)) {
            spawn->maxSpawns = maxSpawns;
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Spawn Radius", &spawn->spawnRadius, 0.1f, 0.0f, 50.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Random Rotation", &spawn->randomRotation);

        ImGui::Separator();
        ImGui::Text("Current Spawns: %d", spawn->currentSpawns);
        ImGui::Text("Active Entities: %zu", spawn->spawnedEntities.size());

        if (ImGui::BeginPopupContextItem("SpawnPointContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::SpawnPointComponent>(entity, "spawnPoint", "Spawn Point");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawLayerComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Layer", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* layer = m_World->GetComponent<ECS::LayerComponent>(entity);
        if (!layer) return;
        DrawComponentHelp("layer", m_World, entity);

        int layerVal = static_cast<int>(layer->layer);
        if (ImGui::InputInt("Layer", &layerVal)) {
            if (layerVal >= 0 && layerVal <= 31) {
                layer->layer = static_cast<u32>(layerVal);
            }
        }

        char nameBuffer[128];
        strncpy(nameBuffer, layer->layerName.c_str(), sizeof(nameBuffer) - 1);
        nameBuffer[sizeof(nameBuffer) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Layer Name", nameBuffer, sizeof(nameBuffer), [layer](const std::string& val) { layer->layerName = val; });

        if (ImGui::BeginPopupContextItem("LayerContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::LayerComponent>(entity, "layer", "Layer");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawStreamingVolumeComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Streaming Volume", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* vol = m_World->GetComponent<Scene::StreamingVolumeComponent>(entity);
        if (!vol) return;
        DrawComponentHelp("streamingVolume", m_World, entity);

        char chunkBuf[256];
        strncpy(chunkBuf, vol->chunkId.c_str(), sizeof(chunkBuf) - 1);
        chunkBuf[sizeof(chunkBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Chunk ID", chunkBuf, sizeof(chunkBuf), [vol](const std::string& val) { vol->chunkId = val; });

        f32 halfExt[3] = { vol->halfExtents.x, vol->halfExtents.y, vol->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents", halfExt,
                [vol](f32 x, f32 y, f32 z) { vol->halfExtents = Math::Vector3(x, y, z); },
                0.5f, 0.0f, 10000.0f)) {
            vol->halfExtents = Math::Vector3(halfExt[0], halfExt[1], halfExt[2]);
        }
        InspectorUndo::DragFloat(m_UndoRedo, "Load Distance", &vol->loadDistance, 1.0f, 0.0f, 10000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Unload Distance", &vol->unloadDistance, 1.0f, 0.0f, 10000.0f);

        const char* priorityNames[] = { "Critical", "High", "Normal", "Low" };
        int priIdx = static_cast<int>(vol->priority);
        if (ImGui::Combo("Priority", &priIdx, priorityNames, 4)) {
            vol->priority = static_cast<Scene::StreamPriority>(priIdx);
        }

        if (ImGui::BeginPopupContextItem("StreamingVolumeCtx")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<Scene::StreamingVolumeComponent>(entity, "streamingVolume", "Streaming Volume");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawStreamingPortalComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Streaming Portal", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* portal = m_World->GetComponent<Scene::StreamingPortalComponent>(entity);
        if (!portal) return;
        DrawComponentHelp("streamingPortal", m_World, entity);

        char chunkABuf[256], chunkBBuf[256];
        strncpy(chunkABuf, portal->chunkA.c_str(), sizeof(chunkABuf) - 1);
        chunkABuf[sizeof(chunkABuf) - 1] = '\0';
        strncpy(chunkBBuf, portal->chunkB.c_str(), sizeof(chunkBBuf) - 1);
        chunkBBuf[sizeof(chunkBBuf) - 1] = '\0';

        InspectorUndo::InputText(m_UndoRedo, "Chunk A", chunkABuf, sizeof(chunkABuf), [portal](const std::string& val) { portal->chunkA = val; });
        InspectorUndo::InputText(m_UndoRedo, "Chunk B", chunkBBuf, sizeof(chunkBBuf), [portal](const std::string& val) { portal->chunkB = val; });
        f32 portalExt[3] = { portal->halfExtents.x, portal->halfExtents.y, portal->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents", portalExt,
                [portal](f32 x, f32 y, f32 z) { portal->halfExtents = Math::Vector3(x, y, z); },
                0.1f, 0.0f, 100.0f)) {
            portal->halfExtents = Math::Vector3(portalExt[0], portalExt[1], portalExt[2]);
        }
        InspectorUndo::Checkbox(m_UndoRedo, "Bidirectional", &portal->bidirectional);

        if (ImGui::BeginPopupContextItem("StreamingPortalCtx")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<Scene::StreamingPortalComponent>(entity, "streamingPortal", "Streaming Portal");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawCineComponent(ECS::Entity entity) {
    std::string hdr = std::string(GetComponentIcon("Camera")) + "Virtual Cinematography (CINE)";
    if (UI::SectionHeader(hdr.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* cine = m_World->GetComponent<ECS::CineComponent>(entity);
        if (!cine) return;
        DrawComponentHelp("cineComponent", m_World, entity);

        ImGui::Checkbox("Enabled##CineComp", &cine->enabled);

        ImGui::SeparatorText("Director Preset");
        const char* directorNames[] = {
            "Kubrick (1-Point Dolly, Deep Focus, No Ease)",
            "Hitchcock (Triads, Vertigo Zoom, Proximity)",
            "Polanski (Eye-Level Lock, Claustrophobic Track)",
            "Welles (Extreme Low Angle, Crane Oner, Contrast)",
            "Spielberg (Retargeted Oner, Emotional Reveals)",
            "Lucas (Graphic Tableaux, Dual-Vcam Wipes)",
            "Ford (Locked Frame Holds, Absolute Look Aim)",
            "Kurosawa (Telephoto Compression, A/B/C Multicam)"
        };
        int currentDirector = static_cast<int>(cine->directorStyle);
        if (ImGui::Combo("Director Style##CineComp", &currentDirector, directorNames, 8)) {
            cine->directorStyle = static_cast<ECS::CineDirectorStyle>(currentDirector);
        }

        ImGui::SeparatorText("Grip Department & Rig");
        const char* rigNames[] = {
            "Fixed (Lock-off)",
            "HumanCarried (Steadicam / Handheld)",
            "Tracked (Dolly Track)",
            "Arm (Crane / Boom Arm)",
            "Suspended (Cable Cam)",
            "FreeFlying (Drone / Flycam)"
        };
        int currentRig = static_cast<int>(cine->rigArchetype);
        if (ImGui::Combo("Rig Archetype##CineComp", &currentRig, rigNames, 6)) {
            cine->rigArchetype = static_cast<ECS::CineRigArchetype>(currentRig);
        }

        ImGui::SeparatorText("Second-Order Dynamics");
        ImGui::SliderFloat("Frequency (f)##CineComp", &cine->frequency, 0.1f, 10.0f, "%.1f Hz");
        ImGui::SetItemTooltip("Spring responsiveness (higher = faster, lower = heavier mass)");
        ImGui::SliderFloat("Damping Ratio (zeta)##CineComp", &cine->dampingRatio, 0.1f, 2.0f, "%.2f");
        ImGui::SetItemTooltip("1.0 = Critically Damped, <1.0 = Underdamped / Overshoot, >1.0 = Overdamped");
        ImGui::SliderFloat("Initial Response (r)##CineComp", &cine->initialResponse, -2.0f, 2.0f, "%.2f");
        ImGui::SetItemTooltip("0 = Smooth Start, >0 = Immediate, <0 = Anticipation");

        ImGui::SeparatorText("Camera & Optics");
        ImGui::SliderFloat("Focal Length (mm)##CineComp", &cine->focalLengthMm, 12.0f, 300.0f, "%.0f mm");
        ImGui::SliderFloat("Aperture (T-Stop)##CineComp", &cine->apertureTStop, 1.0f, 22.0f, "T%.1f");
        ImGui::DragFloat("Focus Distance (m)##CineComp", &cine->focusDistanceMeters, 0.1f, 0.1f, 100.0f, "%.2f m");
        ImGui::SliderFloat("Anamorphic Squeeze##CineComp", &cine->squeezeRatio, 1.0f, 2.0f, "%.2fx");

        ImGui::SeparatorText("Electric & Lighting Ratios");
        ImGui::DragFloat("Key Intensity (EV)##CineComp", &cine->keyIntensityEv, 0.1f, 0.0f, 20.0f, "%.1f EV");
        ImGui::SliderFloat("Key : Fill Ratio##CineComp", &cine->keyToFillRatio, 1.0f, 16.0f, "%.1f : 1");
        ImGui::SliderFloat("Key : Rim Ratio##CineComp", &cine->keyToRimRatio, 1.0f, 16.0f, "%.1f : 1");

        ImGui::SeparatorText("Staging & Target Intent");
        i32 targetId = static_cast<i32>(cine->targetSubjectEntityId);
        if (ImGui::InputInt("Target Subject Entity ID##CineComp", &targetId)) {
            cine->targetSubjectEntityId = static_cast<u64>(targetId);
        }
        ImGui::DragFloat3("Framing Offset##CineComp", &cine->framingOffset.x, 0.05f);

        if (ImGui::BeginPopupContextItem("CineCompCtx")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::CineComponent>(entity, "cineComponent", "Virtual Cinematography (CINE)");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawSaveDataComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Save Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* save = m_World->GetComponent<ECS::SaveDataComponent>(entity);
        if (!save) return;
        DrawComponentHelp("saveData", m_World, entity);

        // Persistence tier dropdown
        const char* tierNames[] = { "Scene State", "Run State", "Meta Progression" };
        int tierIdx = static_cast<int>(save->tier);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("Persistence Tier", &tierIdx, tierNames, 3)) {
            save->tier = static_cast<ECS::PersistenceTier>(tierIdx);
        }
        if (ImGui::IsItemHovered()) {
            const char* tierTooltips[] = {
                "Per-scene within a run. Resets on new game.",
                "Per-run (inventory, quest progress). Resets on new game.",
                "Permanent (unlocks, achievements). Survives across runs."
            };
            ImGui::SetTooltip("%s", tierTooltips[tierIdx]);
        }

        ImGui::Separator();
        InspectorUndo::Checkbox(m_UndoRedo, "Save Position", &save->savePosition);
        InspectorUndo::Checkbox(m_UndoRedo, "Save Rotation", &save->saveRotation);
        InspectorUndo::Checkbox(m_UndoRedo, "Save Scale", &save->saveScale);
        InspectorUndo::Checkbox(m_UndoRedo, "Save Enabled", &save->saveEnabled);

        // Tags
        ImGui::Separator();
        ImGui::Text("Tags (%zu)", save->tags.size());
        for (usize i = 0; i < save->tags.size(); ++i) {
            ImGui::PushID(static_cast<int>(i) + 10000);
            char tagBuf[128];
            strncpy(tagBuf, save->tags[i].c_str(), sizeof(tagBuf) - 1);
            tagBuf[sizeof(tagBuf) - 1] = '\0';
            ImGui::SetNextItemWidth(-50.0f);
            if (ImGui::InputText("##tag", tagBuf, sizeof(tagBuf))) {
                save->tags[i] = tagBuf;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                save->tags.erase(save->tags.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        if (ImGui::SmallButton("Add Tag")) {
            save->tags.push_back("tag");
        }

        // Custom data
        ImGui::Separator();
        ImGui::Text("Custom Data (%zu entries)", save->customData.size());

        for (usize i = 0; i < save->customData.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            char keyBuf[128], valBuf[256];
            strncpy(keyBuf, save->customData[i].first.c_str(), sizeof(keyBuf) - 1);
            keyBuf[sizeof(keyBuf) - 1] = '\0';
            strncpy(valBuf, save->customData[i].second.c_str(), sizeof(valBuf) - 1);
            valBuf[sizeof(valBuf) - 1] = '\0';

            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputText("##key", keyBuf, sizeof(keyBuf))) {
                save->customData[i].first = keyBuf;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-50.0f);
            if (ImGui::InputText("##val", valBuf, sizeof(valBuf))) {
                save->customData[i].second = valBuf;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                save->customData.erase(save->customData.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        if (ImGui::Button("Add Custom Data")) {
            save->customData.push_back({"key", "value"});
        }

        if (ImGui::BeginPopupContextItem("SaveDataContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::SaveDataComponent>(entity, "saveData", "Save Data");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawSaveLoadMenuComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Save/Load Menu", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* menu = m_World->GetComponent<ECS::SaveLoadMenuComponent>(entity);
        if (!menu) return;
        DrawComponentHelp("saveLoadMenu", m_World, entity);

        ImGui::Checkbox("Show on Pause", &menu->showOnPause);
        ImGui::Checkbox("Allow Manual Save", &menu->allowManualSave);
        ImGui::Checkbox("Allow Manual Load", &menu->allowManualLoad);
        ImGui::Checkbox("Allow Delete", &menu->allowDelete);
        ImGui::Checkbox("Show Auto-Saves", &menu->showAutoSaves);
        ImGui::SliderInt("Columns Per Row", &menu->columnsPerRow, 1, 6);

        char headerBuf[128];
        strncpy(headerBuf, menu->headerText.c_str(), sizeof(headerBuf) - 1);
        headerBuf[sizeof(headerBuf) - 1] = '\0';
        if (ImGui::InputText("Header Text", headerBuf, sizeof(headerBuf))) {
            menu->headerText = headerBuf;
        }

        if (ImGui::BeginPopupContextItem("SaveLoadMenuContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::SaveLoadMenuComponent>(entity, "saveLoadMenu", "Save/Load Menu");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawSkeletonComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Skeleton", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* skel = m_World->GetComponent<ECS::SkeletonComponent>(entity);
        if (!skel) return;
        DrawComponentHelp("skeleton", m_World, entity);

        if (skel->skeleton) {
            ImGui::Text("Name: %s", skel->skeleton->name.empty() ? "(unnamed)" : skel->skeleton->name.c_str());
            ImGui::Text("Bones: %zu", skel->skeleton->bones.size());

            if (!skel->skeleton->bones.empty() && ImGui::TreeNode("Bone List")) {
                for (usize i = 0; i < skel->skeleton->bones.size(); ++i) {
                    ImGui::Text("[%zu] %s (parent: %d)", i, skel->skeleton->bones[i].name.c_str(), skel->skeleton->bones[i].parentIndex);
                }
                ImGui::TreePop();
            }
        } else {
            ImGui::TextDisabled("No skeleton loaded");
        }

        if (!skel->sourceAssetPath.empty()) {
            ImGui::Text("Source: %s", skel->sourceAssetPath.c_str());
        }

        if (ImGui::BeginPopupContextItem("SkeletonContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::SkeletonComponent>(entity, "skeleton", "Skeleton");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawJellyMeshComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Jelly Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* jelly = m_World->GetComponent<ECS::JellyMeshComponent>(entity);
        if (!jelly) return;
        DrawComponentHelp("jellyMesh", m_World, entity);

        InspectorUndo::SliderFloat(m_UndoRedo, "Spring Stiffness##Jelly", &jelly->springStiffness, 1.0f, 200.0f);
        InspectorUndo::SliderFloat(m_UndoRedo, "Damping##Jelly", &jelly->damping, 0.1f, 20.0f);
        InspectorUndo::SliderFloat(m_UndoRedo, "Max Stretch##Jelly", &jelly->maxStretch, 0.01f, 2.0f);

        if (jelly->initialized) {
            ImGui::TextDisabled("Vertices: %zu", jelly->restPositions.size());
        } else {
            ImGui::TextDisabled("Not initialized (waiting for play mode)");
        }

        if (ImGui::BeginPopupContextItem("JellyMeshContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::JellyMeshComponent>(entity, "jellyMesh", "Jelly Mesh");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawTetherComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Tether", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* tether = m_World->GetComponent<ECS::TetherComponent>(entity);
        if (!tether) return;
        DrawComponentHelp("tether", m_World, entity);

        // Stem entity (for scoring)
        char stemLabel[128] = "None";
        if (tether->stemEntity != ECS::INVALID_ENTITY && m_World->HasComponent<ECS::NameComponent>(tether->stemEntity)) {
            auto* name = m_World->GetComponent<ECS::NameComponent>(tether->stemEntity);
            snprintf(stemLabel, sizeof(stemLabel), "%s (%llu)", name->name.c_str(), (unsigned long long)tether->stemEntity);
        } else if (tether->stemEntity != ECS::INVALID_ENTITY) {
            snprintf(stemLabel, sizeof(stemLabel), "Entity %llu", (unsigned long long)tether->stemEntity);
        }
        ImGui::Text("Stem: %s", stemLabel);

        // Connected entity (physics joint target)
        char connLabel[128] = "None";
        if (tether->connectedEntity != ECS::INVALID_ENTITY && m_World->HasComponent<ECS::NameComponent>(tether->connectedEntity)) {
            auto* name = m_World->GetComponent<ECS::NameComponent>(tether->connectedEntity);
            snprintf(connLabel, sizeof(connLabel), "%s (%llu)", name->name.c_str(), (unsigned long long)tether->connectedEntity);
        } else if (tether->connectedEntity != ECS::INVALID_ENTITY) {
            snprintf(connLabel, sizeof(connLabel), "Entity %llu", (unsigned long long)tether->connectedEntity);
        }
        ImGui::Text("Connected: %s", connLabel);

        f32 attachPos[3] = { tether->attachLocalPos.x, tether->attachLocalPos.y, tether->attachLocalPos.z };
        InspectorUndo::DragFloat3(m_UndoRedo, "Attach Local Pos", attachPos, [tether](f32 x, f32 y, f32 z) { tether->attachLocalPos = Math::Vector3(x, y, z); }, 0.01f);

        if (ImGui::TreeNode("Break Criteria")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Max Distance##tether", &tether->maxDistance, 0.05f, 0.1f, 5.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Relative Speed##tether", &tether->relativeSpeedThreshold, 0.5f, 1.0f, 30.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Own Speed##tether", &tether->ownSpeedThreshold, 0.5f, 1.0f, 30.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Abs Travel##tether", &tether->absoluteTravelThreshold, 0.5f, 1.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Rel Travel##tether", &tether->relativeTravelThreshold, 0.5f, 1.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Arm Delay##tether", &tether->armDelay, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Spring/Damper")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Mass##tether", &tether->autoMass, 0.05f, 0.01f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Spring K##tether", &tether->autoSpringK, 10.0f, 1.0f, 5000.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Damping##tether", &tether->autoDamping, 1.0f, 0.0f, 200.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Drag##tether", &tether->autoDrag, 0.1f, 0.0f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Force##tether", &tether->driveMaxForce, 10.0f, 1.0f, 2000.0f);
            ImGui::TreePop();
        }

        // Read-only tension bar
        ImGui::ProgressBar(tether->currentTension, ImVec2(-1, 0), tether->isBroken ? "BROKEN" : nullptr);
        if (tether->isBroken) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Tether is broken");
        }

        if (ImGui::BeginPopupContextItem("TetherContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::TetherComponent>(entity, "tether", "Tether");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawGrabbableComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Grabbable", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* grab = m_World->GetComponent<ECS::GrabbableComponent>(entity);
        if (!grab) return;
        DrawComponentHelp("grabbable", m_World, entity);

        InspectorUndo::DragFloat(m_UndoRedo, "Grab Spring", &grab->grabSpring, 1.0f, 1.0f, 500.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Grab Damper", &grab->grabDamper, 0.5f, 0.0f, 100.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Max Accel", &grab->maxAccel, 1.0f, 1.0f, 200.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Max Speed", &grab->maxSpeed, 0.5f, 1.0f, 50.0f);
        InspectorUndo::SliderFloat(m_UndoRedo, "Grab Radius", &grab->grabRadius, 0.1f, 5.0f);
        InspectorUndo::SliderFloat(m_UndoRedo, "Wind Sway Scale", &grab->windSwayScale, 0.0f, 1.0f);

        if (grab->isGrabbed) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Currently grabbed");
        }

        if (ImGui::BeginPopupContextItem("GrabbableContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::GrabbableComponent>(entity, "grabbable", "Grabbable");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawFlowerStemComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Flower Stem", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* stem = m_World->GetComponent<ECS::FlowerStemComponent>(entity);
        if (!stem) return;
        DrawComponentHelp("flowerStem", m_World, entity);

        InspectorUndo::SliderFloat(m_UndoRedo, "Healthy Bonus", &stem->healthyBonus, 0.0f, 50.0f);
        InspectorUndo::SliderFloat(m_UndoRedo, "Withered Penalty", &stem->witheredPenalty, 0.0f, 50.0f);
        InspectorUndo::SliderFloat(m_UndoRedo, "Liquid Intensity", &stem->liquidIntensity, 0.0f, 2.0f, "%.2f");
        ImGui::SameLine();
        if (ImGui::SmallButton("Off##liq")) stem->liquidIntensity = 0.0f;
        InspectorUndo::DragFloat(m_UndoRedo, "Ground Level", &stem->groundLevel, 0.1f, -100.0f, 100.0f);
        f32 sapCol[3] = { stem->sapColor.x, stem->sapColor.y, stem->sapColor.z };
        InspectorUndo::ColorEdit3(m_UndoRedo, "Sap Color", sapCol, [stem](f32 r, f32 g, f32 b) { stem->sapColor = Math::Vector3(r, g, b); });
        InspectorUndo::SliderFloat(m_UndoRedo, "Stem Sway Amplitude", &stem->stemSwayAmplitude, 0.0f, 0.5f);

        ImGui::Separator();
        ImGui::Text("Parts Removed: %d", stem->partsRemoved);
        ImGui::Text("Healthy: %d  Withered: %d", stem->healthyRemoved, stem->witheredRemoved);
        if (stem->evaluated) {
            ImGui::Text("Score: %.1f", stem->score);
        } else {
            ImGui::TextDisabled("Not evaluated yet");
        }

        if (ImGui::BeginPopupContextItem("FlowerStemContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::FlowerStemComponent>(entity, "flowerStem", "Flower Stem");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawFlowerParticleConfigComponent(ECS::Entity entity) {
    if (UI::SectionHeader("Flower Particle Config", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* cfg = m_World->GetComponent<ECS::FlowerParticleConfigComponent>(entity);
        if (!cfg) return;
        DrawComponentHelp("flowerParticleConfig", m_World, entity);

        if (ImGui::TreeNode("Break Burst")) {
            InspectorUndo::DragInt(m_UndoRedo, "Count##breakBurst", &cfg->breakBurstCount, 1, 1, 100);
            InspectorUndo::DragFloat(m_UndoRedo, "Speed##breakBurst", &cfg->breakBurstSpeed, 0.1f, 0.1f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Up Kick##breakBurst", &cfg->breakBurstUpKick, 0.1f, 0.0f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Lifetime##breakBurst", &cfg->breakBurstLifetime, 0.05f, 0.1f, 5.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Scale##breakBurst", &cfg->breakBurstScale, 0.01f, 0.01f, 0.5f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Break Drip")) {
            InspectorUndo::DragInt(m_UndoRedo, "Count##breakDrip", &cfg->breakDripCount, 1, 0, 50);
            InspectorUndo::DragFloat(m_UndoRedo, "Speed##breakDrip", &cfg->breakDripSpeed, 0.1f, 0.0f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Lifetime##breakDrip", &cfg->breakDripLifetime, 0.05f, 0.1f, 5.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Ground Splash")) {
            InspectorUndo::DragInt(m_UndoRedo, "Count##splash", &cfg->splashCount, 1, 1, 50);
            InspectorUndo::DragFloat(m_UndoRedo, "Speed##splash", &cfg->splashSpeed, 0.1f, 0.1f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Up Kick##splash", &cfg->splashUpKick, 0.1f, 0.0f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Lifetime##splash", &cfg->splashLifetime, 0.05f, 0.1f, 5.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Tension Drip")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Drip Rate", &cfg->tensionDripRate, 0.1f, 0.1f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Threshold", &cfg->tensionDripThreshold, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Squirt Speed", &cfg->tensionSquirtSpeed, 0.1f, 0.1f, 20.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Particle Physics")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Gravity##flowerParticle", &cfg->particleGravity, 0.1f, 0.0f, 50.0f);
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextItem("FlowerParticleConfigContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::FlowerParticleConfigComponent>(entity, "flowerParticleConfig", "Flower Particle Config");
            }
            ImGui::EndPopup();
        }
    }
}

// ============================================================================
// External IDE Launcher
// ============================================================================

void EditorLayer::OpenInExternalIDE(const std::string& filePath) {
    u32 ide = m_EditorSettings.externalIDE;
    std::string cmd;

#ifdef ENJIN_PLATFORM_WINDOWS
    // Launch via ShellExecuteA, not CreateProcessA("start ..."): "start" is a
    // cmd.exe builtin, not an executable, so the old CreateProcessA path could
    // never find it and silently opened nothing. ShellExecute launches the real
    // program. For VS Code we resolve the actual Code.exe so it works even when
    // `code` isn't on PATH; if the preferred IDE can't be launched we fall back
    // to the OS default association, then the "Open With..." picker.
    namespace fs = std::filesystem;
    std::string quoted = "\"" + filePath + "\"";
    auto shell = [](const std::string& exe, const std::string& args) -> bool {
        return reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", exe.c_str(),
            args.empty() ? nullptr : args.c_str(), nullptr, SW_SHOWNORMAL)) > 32;
    };
    bool launched = false;
    switch (ide) {
    case 2: // Visual Studio
        launched = shell("devenv.exe", "/edit " + quoted);
        break;
    case 3: // Rider
        launched = shell("rider64.exe", quoted);
        break;
    case 4: // Custom
        if (!m_EditorSettings.customIDEPath.empty()) {
            launched = shell(m_EditorSettings.customIDEPath, quoted);
        }
        break;
    case 1: // VS Code
    default: { // Auto - prefer VS Code
        std::vector<std::string> vscode;
        if (const char* lad = std::getenv("LOCALAPPDATA")) {
            vscode.push_back(std::string(lad) + "\\Programs\\Microsoft VS Code\\Code.exe");
            vscode.push_back(std::string(lad) + "\\Programs\\Microsoft VS Code Insiders\\Code - Insiders.exe");
        }
        vscode.push_back("C:\\Program Files\\Microsoft VS Code\\Code.exe");
        vscode.push_back("C:\\Program Files (x86)\\Microsoft VS Code\\Code.exe");
        for (const auto& exe : vscode) {
            std::error_code ec;
            if (fs::exists(exe, ec) && shell(exe, quoted)) { launched = true; break; }
        }
        if (!launched) launched = shell("code.cmd", quoted); // `code` on PATH
        break;
    }
    }
    if (!launched) {
        // No IDE reachable: OS default app, then the explicit Open With dialog
        // (source files like .as often have no default association).
        if (reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", filePath.c_str(),
                nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
            ShellExecuteA(nullptr, "openas", filePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
    ENJIN_LOG_INFO(Editor, "Opening in IDE: %s", filePath.c_str());
    return;
#elif defined(ENJIN_PLATFORM_MACOS)
    // S19: Shell-escape paths to prevent command injection
    {
        std::string escapedFile = ShellEscape(filePath);
        switch (ide) {
        case 1: // VS Code
            cmd = "code " + escapedFile + " &";
            break;
        case 2: // Visual Studio
            cmd = "open -a 'Visual Studio' " + escapedFile + " &";
            break;
        case 3: // Rider
            cmd = "open -a 'Rider' " + escapedFile + " &";
            break;
        case 4: // Custom
            if (!m_EditorSettings.customIDEPath.empty()) {
                cmd = ShellEscape(m_EditorSettings.customIDEPath) + " " + escapedFile + " &";
            }
            break;
        default: // Auto - try VS Code
            cmd = "code " + escapedFile + " &";
            break;
        }
    }
#else
    // S19: Shell-escape paths to prevent command injection
    {
        std::string escapedFile = ShellEscape(filePath);
        switch (ide) {
        case 1: // VS Code
            cmd = "code " + escapedFile + " &";
            break;
        case 2: // Visual Studio
            cmd = "code " + escapedFile + " &"; // No VS on Linux, fall back to code
            break;
        case 3: // Rider
            cmd = "rider " + escapedFile + " &";
            break;
        case 4: // Custom
            if (!m_EditorSettings.customIDEPath.empty()) {
                cmd = ShellEscape(m_EditorSettings.customIDEPath) + " " + escapedFile + " &";
            }
            break;
        default: // Auto - try VS Code
            cmd = "code " + escapedFile + " &";
            break;
        }
    }
#endif

    if (!cmd.empty()) {
#ifdef _WIN32
        STARTUPINFOA si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi{};
        std::string cmdCopy = cmd;
        if (CreateProcessA(nullptr, cmdCopy.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
#else
        // S-C1: Use posix_spawn instead of std::system to avoid shell injection
        {
            const char* argv[] = { "/bin/sh", "-c", cmd.c_str(), nullptr };
            pid_t pid = 0;
            posix_spawnp(&pid, "/bin/sh", nullptr, nullptr, const_cast<char**>(argv), environ);
            // Fire and forget — IDE runs in background
        }
#endif
        ENJIN_LOG_INFO(Editor, "Opening in IDE: %s", filePath.c_str());
    } else {
        ENJIN_LOG_WARN(Editor, "No IDE configured to open: %s", filePath.c_str());
    }
}

void EditorLayer::OpenScriptAtLine(const std::string& filePath, int line) {
    if (line <= 0) { OpenInExternalIDE(filePath); return; }
    u32 ide = m_EditorSettings.externalIDE;
    bool vscode = (ide == 0 /*Auto*/ || ide == 1 /*VS Code*/);
#ifdef ENJIN_PLATFORM_WINDOWS
    if (vscode) {
        // VS Code jumps to a line with:  code -g "file:line"
        namespace fs = std::filesystem;
        std::string arg = "-g \"" + filePath + ":" + std::to_string(line) + "\"";
        std::vector<std::string> exes;
        if (const char* lad = std::getenv("LOCALAPPDATA"))
            exes.push_back(std::string(lad) + "\\Programs\\Microsoft VS Code\\Code.exe");
        exes.push_back("C:\\Program Files\\Microsoft VS Code\\Code.exe");
        exes.push_back("C:\\Program Files (x86)\\Microsoft VS Code\\Code.exe");
        for (const auto& exe : exes) {
            std::error_code ec;
            if (fs::exists(exe, ec) &&
                reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", exe.c_str(),
                    arg.c_str(), nullptr, SW_SHOWNORMAL)) > 32) {
                return;
            }
        }
        if (reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", "code.cmd",
                arg.c_str(), nullptr, SW_SHOWNORMAL)) > 32) {
            return;
        }
    }
#else
    if (vscode) {
        std::string cmd = "code -g " + ShellEscape(filePath + ":" + std::to_string(line)) + " &";
        const char* argv[] = { "/bin/sh", "-c", cmd.c_str(), nullptr };
        pid_t pid = 0;
        posix_spawnp(&pid, "/bin/sh", nullptr, nullptr, const_cast<char**>(argv), environ);
        return;
    }
#endif
    // Non-VS-Code IDE (or launch failed): just open the file; the error text
    // still shows the line number.
    OpenInExternalIDE(filePath);
}

// ============================================================================
// Script Component Inspector
// ============================================================================

void EditorLayer::AttachScriptFromAsset(ECS::Entity target, const std::string& assetPath) {
    if (target == ECS::INVALID_ENTITY || !m_World) return;
    std::filesystem::path fp(assetPath);
    std::string ext = fp.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext != ".as" && ext != ".angelscript") return;

    // Store relative to the project root (e.g. scripts/Foo.as) so the runtime resolves it.
    std::string rel = assetPath;
    std::string proj = m_SceneManager.GetProjectPath();
    if (!proj.empty()) {
        std::error_code ec;
        auto r = std::filesystem::relative(fp, std::filesystem::path(proj).parent_path(), ec);
        if (!ec && !r.empty() && r.generic_string().rfind("..", 0) != 0) rel = r.generic_string();
    }
    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(target);
    if (!sc) sc = &m_World->AddComponent<ECS::ScriptComponent>(target);
    ECS::ScriptAttachment att;
    att.scriptPath = rel;
    att.className = fp.stem().string();  // best guess; editable in the inspector
    att.enabled = true;
    sc->scripts.push_back(att);
    SelectEntity(target);
    ShowNotification("Attached " + fp.filename().string() + " to entity", NotificationType::Info);
}

void EditorLayer::DrawScriptComponent(ECS::Entity entity) {
    bool scriptOpen = UI::SectionHeader("Scripts", ImGuiTreeNodeFlags_DefaultOpen);
    // Drop an .as onto the Scripts header to append it (works even when the
    // component has no scripts yet, so there is no per-slot node to target).
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            if (pl->Data) AttachScriptFromAsset(entity, std::string(static_cast<const char*>(pl->Data)));
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopupContextItem("ScriptComponentCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ScriptComponent>(entity, "scriptComponent", "Script");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (scriptOpen) {
        auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
        if (!sc) return;
        DrawComponentHelp("scriptComponent", m_World, entity);

        // Resolve a project-relative script path (scripts/Foo.as) to absolute
        // for opening — the process CWD is the exe dir, not the project.
        auto resolveAbs = [&](const std::string& rel) -> std::string {
            std::filesystem::path p(rel);
            if (p.is_absolute()) return rel;
            if (!m_SceneManager.GetProjectPath().empty())
                return (std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path() / p).string();
            return rel;
        };
        // Convert an absolute asset path to a project-relative one for storage.
        auto toProjectRel = [&](const std::string& abs) -> std::string {
            std::filesystem::path fp(abs);
            std::string rel = abs;
            const std::string& proj = m_SceneManager.GetProjectPath();
            if (!proj.empty()) {
                std::error_code ec;
                auto r = std::filesystem::relative(fp, std::filesystem::path(proj).parent_path(), ec);
                if (!ec && !r.empty() && r.generic_string().rfind("..", 0) != 0) rel = r.generic_string();
            }
            return rel;
        };

        // Draw each script attachment
        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(sc->scripts.size()); i++) {
            auto& script = sc->scripts[i];
            ImGui::PushID(i);

            // Header shows the script's NAME (its .as file), not an abstract
            // path. Class name is a secondary field below.
            std::string scriptName = std::filesystem::path(script.scriptPath).stem().string();
            if (scriptName.empty()) scriptName = script.className;
            if (scriptName.empty()) scriptName = "(no script assigned)";
            bool nodeOpen = ImGui::TreeNodeEx(scriptName.c_str(),
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

            // Drop an .as from the Asset Browser directly onto this slot in the
            // chain to set/replace the script here.
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    if (pl->Data) {
                        std::string dropped(static_cast<const char*>(pl->Data));
                        std::string dext = std::filesystem::path(dropped).extension().string();
                        std::transform(dext.begin(), dext.end(), dext.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        if (dext == ".as" || dext == ".angelscript") {
                            script.scriptPath = toProjectRel(dropped);
                            script.className = std::filesystem::path(dropped).stem().string();
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Enabled checkbox on same line
            ImGui::SameLine(ImGui::GetWindowWidth() - 50);
            ImGui::Checkbox("##Enabled", &script.enabled);

            if (ImGui::BeginPopupContextItem("ScriptAttachCtx")) {
                if (ImGui::MenuItem("Remove Script")) {
                    removeIdx = i;
                }
                if (!script.scriptPath.empty() && ImGui::MenuItem("Open in IDE")) {
                    OpenInExternalIDE(resolveAbs(script.scriptPath));
                }
                ImGui::EndPopup();
            }

            if (nodeOpen) {
                // Show the file by name with an Open button — no path typing.
                ImGui::TextDisabled("File:");
                ImGui::SameLine();
                if (script.scriptPath.empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                                       "none - drop a .as here or use Add Script");
                } else {
                    std::string fname = std::filesystem::path(script.scriptPath).filename().string();
                    ImGui::TextUnformatted(fname.c_str());
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", script.scriptPath.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Open")) {
                        OpenInExternalIDE(resolveAbs(script.scriptPath));
                    }
                }

                // Class name
                char classBuf[128];
                strncpy(classBuf, script.className.c_str(), sizeof(classBuf) - 1);
                classBuf[sizeof(classBuf) - 1] = '\0';
                if (ImGui::InputText("Class Name", classBuf, sizeof(classBuf))) {
                    script.className = classBuf;
                }

                // Error state
                if (script.hasError) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    ImGui::TextWrapped("Error: %s", script.lastError.c_str());
                    ImGui::PopStyleColor();
                    if (!script.scriptPath.empty() && ImGui::SmallButton("Open at error")) {
                        // lastError is "file (row, col): message" — pull the row.
                        int line = 0;
                        auto lp = script.lastError.find('(');
                        if (lp != std::string::npos)
                            line = std::atoi(script.lastError.c_str() + lp + 1);
                        OpenScriptAtLine(resolveAbs(script.scriptPath), line);
                    }
                }

                // Status
                if (script.initialized) {
                    ImGui::TextDisabled("Status: Initialized%s", script.started ? " + Started" : "");
                } else {
                    ImGui::TextDisabled("Status: Not initialized (enter play mode)");
                }

                // Properties
                if (!script.properties.empty()) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Properties:");
                    for (auto& prop : script.properties) {
                        ImGui::PushID(prop.name.c_str());

                        // Show section header if present
                        if (!prop.header.empty()) {
                            ImGui::Separator();
                            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", prop.header.c_str());
                        }

                        // Show tooltip if available
                        if (!prop.tooltip.empty()) {
                            ImGui::TextDisabled("(?)");
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("%s", prop.tooltip.c_str());
                            }
                            ImGui::SameLine();
                        }

                        auto& val = prop.isOverridden ? prop.instanceValue : prop.defaultValue;
                        bool changed = false;

                        switch (prop.type) {
                        case ECS::ScriptPropertyType::Int: {
                            int v = val.intVal;
                            if (prop.hasRange) {
                                changed = ImGui::SliderInt(prop.name.c_str(), &v,
                                    static_cast<int>(prop.rangeMin), static_cast<int>(prop.rangeMax));
                            } else {
                                changed = ImGui::DragInt(prop.name.c_str(), &v);
                            }
                            if (changed) {
                                prop.instanceValue.intVal = v;
                                prop.isOverridden = true;
                            }
                            break;
                        }
                        case ECS::ScriptPropertyType::Float: {
                            f32 v = val.floatVal;
                            if (prop.hasRange) {
                                changed = ImGui::SliderFloat(prop.name.c_str(), &v, prop.rangeMin, prop.rangeMax);
                            } else {
                                changed = ImGui::DragFloat(prop.name.c_str(), &v, 0.1f);
                            }
                            if (changed) {
                                prop.instanceValue.floatVal = v;
                                prop.isOverridden = true;
                            }
                            break;
                        }
                        case ECS::ScriptPropertyType::Bool: {
                            bool v = val.boolVal;
                            if (ImGui::Checkbox(prop.name.c_str(), &v)) {
                                prop.instanceValue.boolVal = v;
                                prop.isOverridden = true;
                            }
                            break;
                        }
                        case ECS::ScriptPropertyType::String: {
                            char strBuf[512];
                            strncpy(strBuf, val.stringVal.c_str(), sizeof(strBuf) - 1);
                            strBuf[sizeof(strBuf) - 1] = '\0';
                            if (ImGui::InputText(prop.name.c_str(), strBuf, sizeof(strBuf))) {
                                prop.instanceValue.stringVal = strBuf;
                                prop.isOverridden = true;
                            }
                            break;
                        }
                        case ECS::ScriptPropertyType::Vector2: {
                            f32 v[2] = { val.vec2Val.x, val.vec2Val.y };
                            if (ImGui::DragFloat2(prop.name.c_str(), v, 0.1f)) {
                                prop.instanceValue.vec2Val = Math::Vector2(v[0], v[1]);
                                prop.isOverridden = true;
                            }
                            break;
                        }
                        case ECS::ScriptPropertyType::Vector3: {
                            f32 v[3] = { val.vec3Val.x, val.vec3Val.y, val.vec3Val.z };
                            if (ImGui::DragFloat3(prop.name.c_str(), v, 0.1f)) {
                                prop.instanceValue.vec3Val = Math::Vector3(v[0], v[1], v[2]);
                                prop.isOverridden = true;
                            }
                            break;
                        }
                        case ECS::ScriptPropertyType::Vector4: {
                            f32 v[4] = { val.vec4Val.x, val.vec4Val.y, val.vec4Val.z, val.vec4Val.w };
                            if (ImGui::DragFloat4(prop.name.c_str(), v, 0.1f)) {
                                prop.instanceValue.vec4Val = Math::Vector4(v[0], v[1], v[2], v[3]);
                                prop.isOverridden = true;
                            }
                            break;
                        }
                        case ECS::ScriptPropertyType::Entity: {
                            // Entity-reference slot: drag an entity from the Hierarchy onto
                            // it to statically assign a reference (like Unity's object slot),
                            // instead of typing a raw numeric ID. Accepts the same payload
                            // the Hierarchy uses for reparenting.
                            u64 eid = val.entityVal;
                            ECS::Entity ent = static_cast<ECS::Entity>(eid);
                            std::string slot = "None";
                            if (eid != 0) {
                                if (m_World && m_World->IsValid(ent)) {
                                    auto* nc = m_World->GetComponent<ECS::NameComponent>(ent);
                                    slot = (nc && !nc->name.empty())
                                        ? nc->name
                                        : ("Entity " + std::to_string(ECS::EntityIndex(ent)));
                                } else {
                                    slot = "(missing)";
                                }
                            }
                            ImGui::Button((slot + "##ent_" + prop.name).c_str(), ImVec2(160.0f, 0.0f));
                            if (ImGui::BeginDragDropTarget()) {
                                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ENTITY_REPARENT")) {
                                    if (pl->Data && pl->DataSize >= static_cast<int>(sizeof(ECS::Entity))) {
                                        ECS::Entity dropped = *static_cast<const ECS::Entity*>(pl->Data);
                                        prop.instanceValue.entityVal = static_cast<u64>(dropped);
                                        prop.isOverridden = true;
                                    }
                                }
                                ImGui::EndDragDropTarget();
                            }
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("Drag an entity from the Hierarchy to assign");
                            ImGui::SameLine();
                            if (ImGui::SmallButton(("X##entclr_" + prop.name).c_str())) {
                                prop.instanceValue.entityVal = 0;
                                prop.isOverridden = true;
                            }
                            ImGui::SameLine();
                            ImGui::Text("%s", prop.name.c_str());
                            break;
                        }
                        case ECS::ScriptPropertyType::EntityArray: {
                            // Resizable list of entity-reference slots. Drag an entity
                            // from the Hierarchy onto a slot to assign it; +/X add/remove.
                            auto& arr = prop.instanceValue.entityArrayVal;
                            ImGui::Text("%s", prop.name.c_str());
                            ImGui::SameLine();
                            ImGui::TextDisabled("(%d)", static_cast<int>(arr.size()));
                            ImGui::SameLine();
                            if (ImGui::SmallButton(("+##add_" + prop.name).c_str())) {
                                arr.push_back(0);
                                prop.isOverridden = true;
                            }
                            ImGui::Indent();
                            int removeIdx = -1;
                            for (size_t k = 0; k < arr.size(); ++k) {
                                ImGui::PushID(static_cast<int>(k));
                                u64 eid = arr[k];
                                ECS::Entity ent = static_cast<ECS::Entity>(eid);
                                std::string slot = "None";
                                if (eid != 0) {
                                    if (m_World && m_World->IsValid(ent)) {
                                        auto* nc = m_World->GetComponent<ECS::NameComponent>(ent);
                                        slot = (nc && !nc->name.empty())
                                            ? nc->name
                                            : ("Entity " + std::to_string(ECS::EntityIndex(ent)));
                                    } else {
                                        slot = "(missing)";
                                    }
                                }
                                ImGui::Text("%d:", static_cast<int>(k));
                                ImGui::SameLine();
                                ImGui::Button((slot + "##el").c_str(), ImVec2(140.0f, 0.0f));
                                if (ImGui::BeginDragDropTarget()) {
                                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ENTITY_REPARENT")) {
                                        if (pl->Data && pl->DataSize >= static_cast<int>(sizeof(ECS::Entity))) {
                                            arr[k] = static_cast<u64>(*static_cast<const ECS::Entity*>(pl->Data));
                                            prop.isOverridden = true;
                                        }
                                    }
                                    ImGui::EndDragDropTarget();
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("X")) removeIdx = static_cast<int>(k);
                                ImGui::PopID();
                            }
                            if (removeIdx >= 0) {
                                arr.erase(arr.begin() + removeIdx);
                                prop.isOverridden = true;
                            }
                            ImGui::Unindent();
                            break;
                        }
                        case ECS::ScriptPropertyType::Enum: {
                            int v = val.intVal;
                            if (!val.enumNames.empty()) {
                                const char* preview = (v >= 0 && v < static_cast<int>(val.enumNames.size()))
                                    ? val.enumNames[v].c_str() : "Unknown";
                                if (ImGui::BeginCombo(prop.name.c_str(), preview)) {
                                    for (int e = 0; e < static_cast<int>(val.enumNames.size()); e++) {
                                        bool selected = (v == e);
                                        if (ImGui::Selectable(val.enumNames[e].c_str(), selected)) {
                                            prop.instanceValue.intVal = e;
                                            prop.isOverridden = true;
                                        }
                                    }
                                    ImGui::EndCombo();
                                }
                            } else {
                                if (ImGui::DragInt(prop.name.c_str(), &v)) {
                                    prop.instanceValue.intVal = v;
                                    prop.isOverridden = true;
                                }
                            }
                            break;
                        }
                        }

                        // Reset to default button
                        if (prop.isOverridden) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Reset")) {
                                prop.isOverridden = false;
                            }
                        }

                        ImGui::PopID();
                    }
                }

                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        // Remove script if requested
        if (removeIdx >= 0 && removeIdx < static_cast<int>(sc->scripts.size())) {
            sc->scripts.erase(sc->scripts.begin() + removeIdx);
        }

        // Add script: attach an existing one from scripts/, or create a new one.
        ImGui::Separator();
        if (ImGui::Button("Add Script")) {
            ImGui::OpenPopup("Add Script");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(or drag a .as from the Asset Browser)");

        if (ImGui::BeginPopup("Add Script")) {
            ImGui::TextDisabled("Attach an existing script:");
            ImGui::Separator();
            // List scripts/*.as in the project, skipping the enjin_api headers.
            std::vector<std::pair<std::string, std::string>> found; // (relative, absolute)
            {
                std::filesystem::path root;
                if (!m_SceneManager.GetProjectPath().empty())
                    root = std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path() / "scripts";
                std::error_code ec;
                if (!root.empty() && std::filesystem::exists(root, ec)) {
                    for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
                         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
                        if (ec) break;
                        if (!it->is_regular_file(ec)) continue;
                        std::filesystem::path p = it->path();
                        std::string e = p.extension().string();
                        std::transform(e.begin(), e.end(), e.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        if (e != ".as") continue;
                        std::string rel = std::filesystem::relative(p, root, ec).generic_string();
                        if (rel.rfind("enjin_api/", 0) == 0) continue;
                        found.push_back({rel, p.string()});
                    }
                }
            }
            if (found.empty()) {
                ImGui::TextDisabled("(no scripts in scripts/ yet)");
            } else {
                for (auto& s : found) {
                    if (ImGui::Selectable(s.first.c_str())) {
                        AttachScriptFromAsset(entity, s.second);
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::Selectable("+ New Script...")) {
                m_NewScriptNameBuf[0] = '\0';
                m_NewScriptNameError.clear();
                m_OpenCreateScriptPopup = true; // deferred: opened after this popup closes
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Deferred open of the Create Script modal — OpenPopup must run at this
        // stack level, not from inside the Add Script popup above.
        if (m_OpenCreateScriptPopup) {
            m_OpenCreateScriptPopup = false;
            m_ShowCreateScriptPopup = true;
            ImGui::OpenPopup("Create Script");
        }

        // Create Script modal popup
        if (ImGui::BeginPopupModal("Create Script", &m_ShowCreateScriptPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter a class name for the new script:");
            ImGui::Separator();

            ImGui::InputText("Class Name", m_NewScriptNameBuf, sizeof(m_NewScriptNameBuf));

            // Starter template — shows a beginner what scripts look like
            static const char* s_ScriptTemplates[] = { "Empty", "Rotator", "Interactable", "Spawner" };
            ImGui::Combo("Template", &m_NewScriptTemplate, s_ScriptTemplates, 4);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Empty: lifecycle stubs only\n"
                                  "Rotator: spins the entity (simplest visible behavior)\n"
                                  "Interactable: reacts when the player is near and presses a key\n"
                                  "Spawner: uses a repeating Timer");
            }

            if (!m_NewScriptNameError.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::TextWrapped("%s", m_NewScriptNameError.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Separator();
            if (ImGui::Button("Create", ImVec2(120, 0))) {
                std::string name = m_NewScriptNameBuf;
                // Validation
                bool valid = true;
                if (name.empty()) {
                    m_NewScriptNameError = "Class name cannot be empty.";
                    valid = false;
                } else if (name[0] < 'A' || name[0] > 'Z') {
                    m_NewScriptNameError = "Class name must start with an uppercase letter.";
                    valid = false;
                } else {
                    for (char c : name) {
                        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                            m_NewScriptNameError = "Class name must only contain letters, digits, or underscores.";
                            valid = false;
                            break;
                        }
                    }
                }
                // Resolve against the project root — the process CWD is the
                // exe directory, so bare relative paths would create the
                // script inside the engine install instead of the project.
                std::filesystem::path projRoot;
                if (!m_SceneManager.GetProjectPath().empty()) {
                    projRoot = std::filesystem::path(m_SceneManager.GetProjectPath()).parent_path();
                }
                std::filesystem::path scriptsDir = projRoot / "scripts";
                std::filesystem::path absPath = scriptsDir / (name + ".as");

                if (valid) {
                    if (std::filesystem::exists(absPath)) {
                        m_NewScriptNameError = "File already exists: scripts/" + name + ".as";
                        valid = false;
                    }
                }

                if (valid) {
                    // The attachment stores the project-relative path; the
                    // script root (set at play) resolves it back to absolute.
                    std::string scriptPath = "scripts/" + name + ".as";
                    std::error_code ec;
                    std::filesystem::create_directories(scriptsDir, ec);

                    // Self-heal: make sure the project carries the enjin_api
                    // script headers (TegeBehavior.as etc.) for #include
                    // resolution — older projects were created without them.
                    if (!std::filesystem::exists(scriptsDir / "enjin_api" / "TegeBehavior.as", ec)) {
                        std::filesystem::path engineApi = Scripting::ScriptEngine::FindApiDirectory("");
                        if (!engineApi.empty()) {
                            std::filesystem::copy(engineApi, scriptsDir / "enjin_api",
                                std::filesystem::copy_options::recursive |
                                std::filesystem::copy_options::skip_existing, ec);
                        }
                    }

                    // Write the chosen starter template
                    std::ofstream file(absPath);
                    if (file.is_open()) {
                        if (m_NewScriptTemplate == 1) {           // Rotator
                            file << "// Spins this entity. The simplest visible behavior -\n";
                            file << "// attach, press Play, watch it turn.\n";
                            file << "class " << name << " : TegeBehavior {\n";
                            file << "    [Property] float degreesPerSecond = 90.0f;\n";
                            file << "\n";
                            file << "    void OnUpdate(float dt) {\n";
                            file << "        Vector3 rot = GetRotation();\n";
                            file << "        rot.y += degreesPerSecond * dt;\n";
                            file << "        SetRotation(rot);\n";
                            file << "    }\n";
                            file << "}\n";
                        } else if (m_NewScriptTemplate == 2) {    // Interactable
                            file << "// Reacts when the player is close and presses E.\n";
                            file << "class " << name << " : TegeBehavior {\n";
                            file << "    [Property] float interactRange = 2.0f;\n";
                            file << "\n";
                            file << "    void OnUpdate(float dt) {\n";
                            file << "        uint64 player = Scene_FindEntityByTag(\"player\");\n";
                            file << "        if (player == 0) return;\n";
                            file << "        Vector3 toPlayer = Entity_GetPosition(player) - GetPosition();\n";
                            file << "        if (toPlayer.Length() <= interactRange && Input_GetKeyDown(Key::E)) {\n";
                            file << "            Debug_Log(GetName() + \" was interacted with!\");\n";
                            file << "            // Your reaction here\n";
                            file << "        }\n";
                            file << "    }\n";
                            file << "}\n";
                        } else if (m_NewScriptTemplate == 3) {    // Spawner
                            file << "#include \"Timer.as\"\n";
                            file << "// Does something on a repeating timer.\n";
                            file << "class " << name << " : TegeBehavior {\n";
                            file << "    [Property] float intervalSeconds = 2.0f;\n";
                            file << "    Timer timer;\n";
                            file << "\n";
                            file << "    void OnStart() {\n";
                            file << "        timer.Start(intervalSeconds, true); // repeating\n";
                            file << "    }\n";
                            file << "\n";
                            file << "    void OnUpdate(float dt) {\n";
                            file << "        timer.Update(dt);\n";
                            file << "        if (timer.JustFinished()) {\n";
                            file << "            Debug_Log(\"tick from \" + GetName());\n";
                            file << "            // Spawn / trigger something here\n";
                            file << "        }\n";
                            file << "    }\n";
                            file << "}\n";
                        } else {                                   // Empty
                            file << "class " << name << " : TegeBehavior {\n";
                            file << "    void OnCreate() {\n";
                            file << "        // Called when the entity is created\n";
                            file << "    }\n";
                            file << "\n";
                            file << "    void OnUpdate(float dt) {\n";
                            file << "        // Called every frame\n";
                            file << "    }\n";
                            file << "\n";
                            file << "    void OnDestroy() {\n";
                            file << "        // Called when the entity is destroyed\n";
                            file << "    }\n";
                            file << "}\n";
                        }
                        file.close();

                        // Add the script attachment
                        ECS::ScriptAttachment attachment;
                        attachment.scriptPath = scriptPath;
                        attachment.className = name;
                        sc->scripts.push_back(std::move(attachment));

                        ENJIN_LOG_INFO(Editor, "Created script: %s", absPath.string().c_str());

                        // Open in IDE
                        OpenInExternalIDE(absPath.string());

                        m_ShowCreateScriptPopup = false;
                        ImGui::CloseCurrentPopup();
                    } else {
                        m_NewScriptNameError = "Failed to create file: " + scriptPath;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_ShowCreateScriptPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

// ============================================================================
// Vehicle Controller Inspector
// ============================================================================

void EditorLayer::DrawVehicleController(ECS::Entity entity) {
    bool ctrlOpen = UI::SectionHeader("Vehicle Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("VehicleControllerCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::VehicleController>(entity, "vehicle", "Vehicle");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::VehicleController>(entity);
        if (!ctrl) return;
        DrawComponentHelp("vehicle", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Engine")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Max Speed", &ctrl->maxSpeed, 0.5f, 0.0f, 200.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Reverse Max Speed", &ctrl->reverseMaxSpeed, 0.5f, 0.0f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Acceleration", &ctrl->acceleration, 0.5f, 0.0f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Brake Force", &ctrl->brakeForce, 0.5f, 0.0f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Engine Brake", &ctrl->engineBrake, 0.5f, 0.0f, 50.0f);
            InspectorUndo::Checkbox(m_UndoRedo, "Handbrake", &ctrl->handbrake);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Steering")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Max Steer Angle", &ctrl->maxSteerAngle, 0.5f, 0.0f, 90.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Steer Speed", &ctrl->steerSpeed, 1.0f, 0.0f, 500.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Steer Return Speed", &ctrl->steerReturnSpeed, 1.0f, 0.0f, 500.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Wheel Base", &ctrl->wheelBase, 0.1f, 0.1f, 10.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Physics")) {
            InspectorUndo::SliderFloat(m_UndoRedo, "Grip", &ctrl->grip, 0.0f, 2.0f);
            InspectorUndo::SliderFloat(m_UndoRedo, "Drift Factor", &ctrl->driftFactor, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Downforce Multiplier", &ctrl->downforceMultiplier, 0.1f, 0.0f, 5.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Mass (kg)", &ctrl->mass, 10.0f, 1.0f, 10000.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Camera")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Camera Distance", &ctrl->cameraDistance, 0.1f, 1.0f, 30.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Camera Height", &ctrl->cameraHeight, 0.1f, 0.0f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Camera Pitch##veh", &ctrl->cameraPitch, 1.0f, -89.0f, 89.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Camera Lerp Speed", &ctrl->cameraLerpSpeed, 0.1f, 0.1f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Camera Look Ahead", &ctrl->cameraLookAhead, 0.1f, 0.0f, 10.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Visuals")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Body Roll Amount", &ctrl->bodyRollAmount, 0.5f, 0.0f, 30.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Body Pitch Amount", &ctrl->bodyPitchAmount, 0.5f, 0.0f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Model Forward Yaw", &ctrl->modelForwardYaw, 1.0f, -180.0f, 180.0f);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Spin the visual body so its nose faces the drive direction (-Z).\n0 = model faces -Z, 180 = faces +Z, -90/+90 = faces +X/-X.");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Base Controller")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Move Speed", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
            InspectorUndo::Checkbox(m_UndoRedo, "WASD", &ctrl->useWASD);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Arrow Keys##veh", &ctrl->useArrowKeys);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Gamepad", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                InspectorUndo::DragInt(m_UndoRedo, "Gamepad Index", &ctrl->gamepadIndex, 1, 0, 3);
                InspectorUndo::DragFloat(m_UndoRedo, "Stick Sensitivity##veh", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
            }
            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Speed: %.1f  Steer: %.1f deg  Drifting: %s",
            ctrl->currentSpeed, ctrl->currentSteerAngle, ctrl->isDrifting ? "Yes" : "No");
    }
}

// ============================================================================
// Surface Aligned (Planet) Controller Inspector
// ============================================================================

void EditorLayer::DrawSurfaceAlignedController(ECS::Entity entity) {
    bool ctrlOpen = UI::SectionHeader("Surface Aligned (Planet) Controller", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("SurfaceAlignedCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::SurfaceAlignedController>(entity, "surfaceAligned", "Surface Aligned");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (ctrlOpen) {
        auto* ctrl = m_World->GetComponent<ECS::SurfaceAlignedController>(entity);
        if (!ctrl) return;
        DrawComponentHelp("surfaceAligned", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled", &ctrl->isEnabled);

        if (ImGui::TreeNode("Input##SurfAligned")) {
            InspectorUndo::Checkbox(m_UndoRedo, "WASD##sa", &ctrl->useWASD);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Arrow Keys##sa", &ctrl->useArrowKeys);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Gamepad##sa", &ctrl->useGamepad);
            if (ctrl->useGamepad) {
                InspectorUndo::DragInt(m_UndoRedo, "Gamepad Index##sa", &ctrl->gamepadIndex, 1, 0, 3);
                InspectorUndo::DragFloat(m_UndoRedo, "Stick Sensitivity##sa", &ctrl->gamepadLookSensitivity, 0.1f, 0.1f, 10.0f);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Movement##SurfAligned")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Move Speed##sa", &ctrl->moveSpeed, 0.1f, 0.1f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Sprint Multiplier##sa", &ctrl->sprintMultiplier, 0.1f, 1.0f, 5.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Acceleration##sa", &ctrl->acceleration, 0.5f, 0.0f, 200.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Deceleration##sa", &ctrl->deceleration, 0.5f, 0.0f, 200.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Jump Force##sa", &ctrl->jumpForce, 0.1f, 1.0f, 30.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Surface Alignment")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Align Speed", &ctrl->alignSpeed, 0.5f, 0.1f, 30.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("How fast the character aligns to surface normal");
            InspectorUndo::DragFloat(m_UndoRedo, "Ground Check Distance", &ctrl->groundCheckDistance, 0.1f, 0.1f, 10.0f);
            ImGui::Text("Local Up: (%.2f, %.2f, %.2f)", ctrl->localUp.x, ctrl->localUp.y, ctrl->localUp.z);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Camera##SurfAligned")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Disable Mouse Look##sa", &ctrl->disableMouseLook);
            InspectorUndo::DragFloat(m_UndoRedo, "Distance##sa", &ctrl->cameraDistance, 0.1f, 1.0f, 30.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Height##sa", &ctrl->cameraHeight, 0.1f, 0.0f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Sensitivity##sa", &ctrl->cameraSensitivity, 0.1f, 0.1f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Lerp Speed##sa", &ctrl->cameraLerpSpeed, 0.5f, 1.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Pitch##sa", &ctrl->cameraPitch, 1.0f, ctrl->cameraMinPitch, ctrl->cameraMaxPitch);
            InspectorUndo::DragFloat(m_UndoRedo, "Min Pitch##sa", &ctrl->cameraMinPitch, 1.0f, -89.0f, 0.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Pitch##sa", &ctrl->cameraMaxPitch, 1.0f, 0.0f, 89.0f);
            ImGui::TreePop();
        }

        // State display
        ImGui::TextDisabled("State: %s%s%s",
            ctrl->isGrounded ? "Grounded " : "",
            ctrl->isJumping ? "Jumping " : "",
            ctrl->isFalling ? "Falling " : "");
    }
}

// ============================================================================
// Possessable Component Inspector
// ============================================================================

void EditorLayer::DrawPossessableComponent(ECS::Entity entity) {
    bool possOpen = UI::SectionHeader("Possessable", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("PossessableCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::PossessableComponent>(entity, "possessable", "Possessable");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (possOpen) {
        auto* poss = m_World->GetComponent<ECS::PossessableComponent>(entity);
        if (!poss) return;
        DrawComponentHelp("possessable", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Is Possessed", &poss->isPossessed);
        InspectorUndo::Checkbox(m_UndoRedo, "Auto Detect Controller", &poss->autoDetect);
        InspectorUndo::Checkbox(m_UndoRedo, "Disable On Unpossess", &poss->disableOnUnpossess);

        InspectorUndo::DragInt(m_UndoRedo, "Player Index", &poss->playerIndex, 1, 0, 3);
        InspectorUndo::DragFloat(m_UndoRedo, "Possess Range", &poss->possessRange, 0.5f, 0.0f, 100.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = unlimited range");
        InspectorUndo::DragFloat(m_UndoRedo, "Transition Duration", &poss->transitionDuration, 0.05f, 0.0f, 3.0f);

        char promptBuf[256];
        strncpy(promptBuf, poss->promptText.c_str(), sizeof(promptBuf) - 1);
        promptBuf[sizeof(promptBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Prompt Text", promptBuf, sizeof(promptBuf), [poss](const std::string& val) { poss->promptText = val; });

        ImGui::Separator();
        ImGui::TextDisabled("Previous Possessor: %llu",
            static_cast<unsigned long long>(poss->previousPossessor));
    }
}

// ============================================================================
// Puzzle Component Inspectors
// ============================================================================

void EditorLayer::DrawLockComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Lock", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("LockCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::LockComponent>(entity, "lock", "Lock");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* lock = m_World->GetComponent<ECS::LockComponent>(entity);
        if (!lock) return;
        DrawComponentHelp("lock", m_World, entity);

        char keyBuf[128];
        strncpy(keyBuf, lock->requiredKey.c_str(), sizeof(keyBuf) - 1);
        keyBuf[sizeof(keyBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Required Key##Lock", keyBuf, sizeof(keyBuf), [lock](const std::string& val) { lock->requiredKey = val; });

        InspectorUndo::Checkbox(m_UndoRedo, "Is Locked##Lock", &lock->isLocked);
        InspectorUndo::Checkbox(m_UndoRedo, "Consume Key##Lock", &lock->consumeKey);
        InspectorUndo::Checkbox(m_UndoRedo, "Auto Open##Lock", &lock->autoOpen);
        InspectorUndo::DragFloat(m_UndoRedo, "Interact Range##Lock", &lock->interactRange, 0.1f, 0.0f, 100.0f);

        const char* openModes[] = { "Toggle", "Open Only", "Timed" };
        int modeIdx = static_cast<int>(lock->openMode);
        if (InspectorUndo::Combo(m_UndoRedo, "Open Mode##Lock", &modeIdx, openModes, 3)) {
            lock->openMode = static_cast<ECS::LockComponent::OpenMode>(modeIdx);
        }

        if (lock->openMode == ECS::LockComponent::OpenMode::Timed) {
            InspectorUndo::DragFloat(m_UndoRedo, "Open Duration##Lock", &lock->openDuration, 0.1f, 0.0f, 60.0f);
        }

        f32 closedPos[3] = { lock->closedPosition.x, lock->closedPosition.y, lock->closedPosition.z };
        InspectorUndo::DragFloat3(m_UndoRedo, "Closed Position##Lock", closedPos, [lock](f32 x, f32 y, f32 z) { lock->closedPosition = Math::Vector3(x, y, z); }, 0.1f);
        f32 openPos[3] = { lock->openPosition.x, lock->openPosition.y, lock->openPosition.z };
        InspectorUndo::DragFloat3(m_UndoRedo, "Open Position##Lock", openPos, [lock](f32 x, f32 y, f32 z) { lock->openPosition = Math::Vector3(x, y, z); }, 0.1f);
        f32 closedRot[3] = { lock->closedRotation.x, lock->closedRotation.y, lock->closedRotation.z };
        InspectorUndo::DragFloat3(m_UndoRedo, "Closed Rotation##Lock", closedRot, [lock](f32 x, f32 y, f32 z) { lock->closedRotation = Math::Vector3(x, y, z); }, 0.1f);
        f32 openRot[3] = { lock->openRotation.x, lock->openRotation.y, lock->openRotation.z };
        InspectorUndo::DragFloat3(m_UndoRedo, "Open Rotation##Lock", openRot, [lock](f32 x, f32 y, f32 z) { lock->openRotation = Math::Vector3(x, y, z); }, 0.1f);

        InspectorUndo::DragFloat(m_UndoRedo, "Open Speed##Lock", &lock->openSpeed, 0.1f, 0.0f, 50.0f);

        char lockedBuf[256];
        strncpy(lockedBuf, lock->lockedPrompt.c_str(), sizeof(lockedBuf) - 1);
        lockedBuf[sizeof(lockedBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Locked Prompt##Lock", lockedBuf, sizeof(lockedBuf), [lock](const std::string& val) { lock->lockedPrompt = val; });

        char unlockedBuf[256];
        strncpy(unlockedBuf, lock->unlockedPrompt.c_str(), sizeof(unlockedBuf) - 1);
        unlockedBuf[sizeof(unlockedBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Unlocked Prompt##Lock", unlockedBuf, sizeof(unlockedBuf), [lock](const std::string& val) { lock->unlockedPrompt = val; });
    }
}

void EditorLayer::DrawPushableComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Pushable", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("PushableCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::PushableComponent>(entity, "pushable", "Pushable");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* push = m_World->GetComponent<ECS::PushableComponent>(entity);
        if (!push) return;
        DrawComponentHelp("pushable", m_World, entity);

        InspectorUndo::DragFloat(m_UndoRedo, "Mass##Push", &push->mass, 0.1f, 0.01f, 1000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Push Speed##Push", &push->pushSpeed, 0.1f, 0.0f, 50.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Friction##Push", &push->friction, 0.01f, 0.0f, 1.0f);

        InspectorUndo::Checkbox(m_UndoRedo, "Grid Snap##Push", &push->gridSnap);
        if (push->gridSnap) {
            InspectorUndo::DragFloat(m_UndoRedo, "Grid Cell Size##Push", &push->gridCellSize, 0.1f, 0.1f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Grid Move Speed##Push", &push->gridMoveSpeed, 0.1f, 0.1f, 50.0f);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Pushable X##Push", &push->pushableX);
        ImGui::SameLine();
        InspectorUndo::Checkbox(m_UndoRedo, "Pushable Y##Push", &push->pushableY);
        ImGui::SameLine();
        InspectorUndo::Checkbox(m_UndoRedo, "Pushable Z##Push", &push->pushableZ);
        InspectorUndo::Checkbox(m_UndoRedo, "Can Be Pushed Off##Push", &push->canBePushedOff);
    }
}

void EditorLayer::DrawSwitchComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Switch", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("SwitchCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::SwitchComponent>(entity, "switch", "Switch");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* sw = m_World->GetComponent<ECS::SwitchComponent>(entity);
        if (!sw) return;
        DrawComponentHelp("switch", m_World, entity);

        const char* types[] = { "Pressure Plate", "Toggle", "One Shot", "Timed", "Sequence" };
        int typeIdx = static_cast<int>(sw->type);
        if (InspectorUndo::Combo(m_UndoRedo, "Type##Switch", &typeIdx, types, 5)) {
            sw->type = static_cast<ECS::SwitchComponent::SwitchType>(typeIdx);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Require Specific Tag##Switch", &sw->requireSpecificTag);
        if (sw->requireSpecificTag) {
            char tagBuf[128];
            strncpy(tagBuf, sw->requiredTag.c_str(), sizeof(tagBuf) - 1);
            tagBuf[sizeof(tagBuf) - 1] = '\0';
            InspectorUndo::InputText(m_UndoRedo, "Required Tag##Switch", tagBuf, sizeof(tagBuf), [sw](const std::string& val) { sw->requiredTag = val; });
        }

        if (sw->type == ECS::SwitchComponent::SwitchType::PressurePlate) {
            InspectorUndo::DragFloat(m_UndoRedo, "Activation Weight##Switch", &sw->activationWeight, 0.1f, 0.0f, 1000.0f);
        }
        if (sw->type == ECS::SwitchComponent::SwitchType::Timed) {
            InspectorUndo::DragFloat(m_UndoRedo, "Active Duration##Switch", &sw->activeDuration, 0.1f, 0.0f, 60.0f);
        }
        if (sw->type == ECS::SwitchComponent::SwitchType::Sequence) {
            InspectorUndo::DragInt(m_UndoRedo, "Sequence Index##Switch", &sw->sequenceIndex, 1, 0, 100);
            InspectorUndo::DragInt(m_UndoRedo, "Sequence Group##Switch", &sw->sequenceGroup, 1, 0, 100);
        }

        f32 offPos[3] = { sw->offPosition.x, sw->offPosition.y, sw->offPosition.z };
        InspectorUndo::DragFloat3(m_UndoRedo, "Off Position##Switch", offPos, [sw](f32 x, f32 y, f32 z) { sw->offPosition = Math::Vector3(x, y, z); }, 0.1f);
        f32 onPos[3] = { sw->onPosition.x, sw->onPosition.y, sw->onPosition.z };
        InspectorUndo::DragFloat3(m_UndoRedo, "On Position##Switch", onPos, [sw](f32 x, f32 y, f32 z) { sw->onPosition = Math::Vector3(x, y, z); }, 0.1f);

        InspectorUndo::DragFloat(m_UndoRedo, "Transition Speed##Switch", &sw->transitionSpeed, 0.1f, 0.0f, 50.0f);

        char promptBuf[256];
        strncpy(promptBuf, sw->promptText.c_str(), sizeof(promptBuf) - 1);
        promptBuf[sizeof(promptBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Prompt Text##Switch", promptBuf, sizeof(promptBuf), [sw](const std::string& val) { sw->promptText = val; });
        InspectorUndo::Checkbox(m_UndoRedo, "Show Prompt##Switch", &sw->showPrompt);
    }
}

void EditorLayer::DrawGoalZoneComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Goal Zone", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("GoalZoneCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::GoalZoneComponent>(entity, "goalZone", "Goal Zone");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* goal = m_World->GetComponent<ECS::GoalZoneComponent>(entity);
        if (!goal) return;
        DrawComponentHelp("goalZone", m_World, entity);

        const char* types[] = { "Push Target", "Stand On", "Item Deposit", "Checkpoint", "Level Exit" };
        int typeIdx = static_cast<int>(goal->type);
        if (InspectorUndo::Combo(m_UndoRedo, "Type##Goal", &typeIdx, types, 5)) {
            goal->type = static_cast<ECS::GoalZoneComponent::GoalType>(typeIdx);
        }

        char tagBuf[128];
        strncpy(tagBuf, goal->requiredTag.c_str(), sizeof(tagBuf) - 1);
        tagBuf[sizeof(tagBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Required Tag##Goal", tagBuf, sizeof(tagBuf), [goal](const std::string& val) { goal->requiredTag = val; });

        char itemBuf[128];
        strncpy(itemBuf, goal->requiredItem.c_str(), sizeof(itemBuf) - 1);
        itemBuf[sizeof(itemBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Required Item##Goal", itemBuf, sizeof(itemBuf), [goal](const std::string& val) { goal->requiredItem = val; });

        InspectorUndo::DragInt(m_UndoRedo, "Goal Group##Goal", &goal->goalGroup, 1, 0, 100);

        f32 inactiveCol[3] = { goal->inactiveColor.x, goal->inactiveColor.y, goal->inactiveColor.z };
        InspectorUndo::ColorEdit3(m_UndoRedo, "Inactive Color##Goal", inactiveCol, [goal](f32 r, f32 g, f32 b) { goal->inactiveColor = Math::Vector3(r, g, b); });
        f32 activeCol[3] = { goal->activeColor.x, goal->activeColor.y, goal->activeColor.z };
        InspectorUndo::ColorEdit3(m_UndoRedo, "Active Color##Goal", activeCol, [goal](f32 r, f32 g, f32 b) { goal->activeColor = Math::Vector3(r, g, b); });

        if (goal->type == ECS::GoalZoneComponent::GoalType::LevelExit) {
            char sceneBuf[256];
            strncpy(sceneBuf, goal->nextScene.c_str(), sizeof(sceneBuf) - 1);
            sceneBuf[sizeof(sceneBuf) - 1] = '\0';
            InspectorUndo::InputText(m_UndoRedo, "Next Scene##Goal", sceneBuf, sizeof(sceneBuf), [goal](const std::string& val) { goal->nextScene = val; });
        }

        ImGui::TextDisabled("Satisfied: %s", goal->isSatisfied ? "Yes" : "No");
    }
}

void EditorLayer::DrawConveyorComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Conveyor", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ConveyorCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ConveyorComponent>(entity, "conveyor", "Conveyor");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* conv = m_World->GetComponent<ECS::ConveyorComponent>(entity);
        if (!conv) return;
        DrawComponentHelp("conveyor", m_World, entity);

        f32 dir[3] = { conv->direction.x, conv->direction.y, conv->direction.z };
        InspectorUndo::DragFloat3(m_UndoRedo, "Direction##Conv", dir, [conv](f32 x, f32 y, f32 z) { conv->direction = Math::Vector3(x, y, z); }, 0.01f, -1.0f, 1.0f);

        InspectorUndo::DragFloat(m_UndoRedo, "Speed##Conv", &conv->speed, 0.1f, 0.0f, 100.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Affects Player##Conv", &conv->affectsPlayer);
        InspectorUndo::Checkbox(m_UndoRedo, "Affects Pushables##Conv", &conv->affectsPushables);
        InspectorUndo::Checkbox(m_UndoRedo, "Is Active##Conv", &conv->isActive);
    }
}

void EditorLayer::DrawTeleporterComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Teleporter", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TeleporterCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::TeleporterComponent>(entity, "teleporter", "Teleporter");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* tp = m_World->GetComponent<ECS::TeleporterComponent>(entity);
        if (!tp) return;
        DrawComponentHelp("teleporter", m_World, entity);

        f32 targetPos[3] = { tp->targetPosition.x, tp->targetPosition.y, tp->targetPosition.z };
        InspectorUndo::DragFloat3(m_UndoRedo, "Target Position##Tele", targetPos, [tp](f32 x, f32 y, f32 z) { tp->targetPosition = Math::Vector3(x, y, z); }, 0.1f);
        f32 targetRot[3] = { tp->targetRotation.x, tp->targetRotation.y, tp->targetRotation.z };
        InspectorUndo::DragFloat3(m_UndoRedo, "Target Rotation##Tele", targetRot, [tp](f32 x, f32 y, f32 z) { tp->targetRotation = Math::Vector3(x, y, z); }, 0.1f);

        InspectorUndo::DragFloat(m_UndoRedo, "Cooldown##Tele", &tp->cooldown, 0.1f, 0.0f, 30.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Preserve Velocity##Tele", &tp->preserveVelocity);

        char tagBuf[128];
        strncpy(tagBuf, tp->requiredTag.c_str(), sizeof(tagBuf) - 1);
        tagBuf[sizeof(tagBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Required Tag##Tele", tagBuf, sizeof(tagBuf), [tp](const std::string& val) { tp->requiredTag = val; });
    }
}

void EditorLayer::DrawDestructibleComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Destructible", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("DestructibleCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::DestructibleComponent>(entity, "destructible", "Destructible");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* dest = m_World->GetComponent<ECS::DestructibleComponent>(entity);
        if (!dest) return;
        DrawComponentHelp("destructible", m_World, entity);

        InspectorUndo::DragFloat(m_UndoRedo, "Health##Dest", &dest->health, 0.1f, 0.0f, 10000.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Destroy On Hit##Dest", &dest->destroyOnHit);

        InspectorUndo::Checkbox(m_UndoRedo, "Spawn Pickup##Dest", &dest->spawnPickup);
        if (dest->spawnPickup) {
            char pickupBuf[128];
            strncpy(pickupBuf, dest->pickupId.c_str(), sizeof(pickupBuf) - 1);
            pickupBuf[sizeof(pickupBuf) - 1] = '\0';
            InspectorUndo::InputText(m_UndoRedo, "Pickup ID##Dest", pickupBuf, sizeof(pickupBuf), [dest](const std::string& val) { dest->pickupId = val; });
            InspectorUndo::DragInt(m_UndoRedo, "Pickup Count##Dest", &dest->pickupCount, 1, 0, 100);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Can Respawn##Dest", &dest->canRespawn);
        if (dest->canRespawn) {
            InspectorUndo::DragFloat(m_UndoRedo, "Respawn Time##Dest", &dest->respawnTime, 0.1f, 0.0f, 300.0f);
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Shake On Hit##Dest", &dest->shakeOnHit, 0.01f, 0.0f, 2.0f);
    }
}

void EditorLayer::DrawCurlNoiseFieldComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Curl Noise Field", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("CurlNoiseFieldCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::CurlNoiseFieldComponent>(entity, "curlNoiseField", "Curl Noise Field");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* cn = m_World->GetComponent<ECS::CurlNoiseFieldComponent>(entity);
        if (!cn) return;
        DrawComponentHelp("curlNoiseField", m_World, entity);

        InspectorUndo::DragInt(m_UndoRedo, "Octaves##CNF", &cn->octaves, 1, 1, 8);
        InspectorUndo::DragFloat(m_UndoRedo, "Frequency##CNF", &cn->frequency, 0.01f, 0.01f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Amplitude##CNF", &cn->amplitude, 0.1f, 0.0f, 100.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Lacunarity##CNF", &cn->lacunarity, 0.01f, 1.0f, 4.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Persistence##CNF", &cn->persistence, 0.01f, 0.0f, 1.0f);

        i32 seedI = static_cast<i32>(cn->seed);
        if (InspectorUndo::DragInt(m_UndoRedo, "Seed##CNF", &seedI, 1, 0, 100000)) {
            cn->seed = static_cast<u32>(seedI);
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Time Scale##CNF", &cn->timeScale, 0.01f, 0.0f, 10.0f);
        f32 he[3] = { cn->halfExtents.x, cn->halfExtents.y, cn->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents##CNF", he,
                [cn](f32 x, f32 y, f32 z) { cn->halfExtents = Math::Vector3(x, y, z); },
                0.1f, 0.1f, 100.0f)) {
            cn->halfExtents = Math::Vector3(he[0], he[1], he[2]);
        }

        int falloffI = static_cast<int>(cn->falloff);
        const char* falloffNames[] = { "None", "Linear", "Smooth" };
        if (ImGui::Combo("Falloff##CNF", &falloffI, falloffNames, 3)) {
            cn->falloff = static_cast<ECS::CurlNoiseFieldComponent::Falloff>(falloffI);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Affect Particles##CNF", &cn->affectParticles);
        InspectorUndo::Checkbox(m_UndoRedo, "Affect Mesh Vertices##CNF", &cn->affectMeshVertices);
        InspectorUndo::Checkbox(m_UndoRedo, "Show Debug Arrows##CNF", &cn->showDebugArrows);

        if (cn->showDebugArrows) {
            i32 res = static_cast<i32>(cn->debugArrowResolution);
            if (InspectorUndo::DragInt(m_UndoRedo, "Arrow Resolution##CNF", &res, 1, 2, 8)) {
                cn->debugArrowResolution = static_cast<u32>(res);
            }
        }
    }
}

void EditorLayer::DrawFractureConfigComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Fracture Config", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("FractureConfigCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::FractureConfigComponent>(entity, "fractureConfig", "Fracture Config");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* fc = m_World->GetComponent<ECS::FractureConfigComponent>(entity);
        if (!fc) return;
        DrawComponentHelp("fractureConfig", m_World, entity);

        i32 fragCount = static_cast<i32>(fc->fragmentCount);
        if (InspectorUndo::DragInt(m_UndoRedo, "Fragment Count##FC", &fragCount, 1, 2, 64)) {
            fc->fragmentCount = static_cast<u32>(fragCount);
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Explosion Force##FC", &fc->explosionForce, 0.1f, 0.0f, 100.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Persistent Fragments##FC", &fc->persistentFragments);
        InspectorUndo::Checkbox(m_UndoRedo, "Allow Re-fracture##FC", &fc->allowRefracture);

        if (fc->allowRefracture) {
            i32 maxDepth = static_cast<i32>(fc->maxRefractureDepth);
            if (InspectorUndo::DragInt(m_UndoRedo, "Max Refracture Depth##FC", &maxDepth, 1, 1, 5)) {
                fc->maxRefractureDepth = static_cast<u32>(maxDepth);
            }
        }

        i32 maxFrag = static_cast<i32>(fc->maxFragmentEntities);
        if (InspectorUndo::DragInt(m_UndoRedo, "Max Fragment Entities##FC", &maxFrag, 1, 16, 1024)) {
            fc->maxFragmentEntities = static_cast<u32>(maxFrag);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Auto Cleanup##FC", &fc->autoCleanup);
        if (fc->autoCleanup) {
            InspectorUndo::DragFloat(m_UndoRedo, "Cleanup Delay##FC", &fc->cleanupDelay, 0.5f, 1.0f, 120.0f);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Pre-fracture##FC", &fc->preFracture);
        if (fc->preFracture) {
            InspectorUndo::DragFloat(m_UndoRedo, "Joint Break Force##FC", &fc->jointBreakForce, 1.0f, 1.0f, 10000.0f);
        }

        if (ImGui::TreeNode("Fragment Physics##FC")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Density##FC", &fc->fragmentDensity, 0.1f, 0.01f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Friction##FC", &fc->fragmentFriction, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Bounciness##FC", &fc->fragmentBounciness, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Impact Bias##FC", &fc->impactBias, 0.01f, 0.0f, 1.0f);
    }
}

void EditorLayer::DrawMovingPlatformComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Moving Platform", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("MovingPlatformCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::MovingPlatformComponent>(entity, "movingPlatform", "Moving Platform");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* plat = m_World->GetComponent<ECS::MovingPlatformComponent>(entity);
        if (!plat) return;
        DrawComponentHelp("movingPlatform", m_World, entity);

        // Waypoints list
        ImGui::Text("Waypoints: %zu", plat->waypoints.size());
        for (usize i = 0; i < plat->waypoints.size(); i++) {
            ImGui::PushID(static_cast<int>(i));
            f32 wp[3] = { plat->waypoints[i].x, plat->waypoints[i].y, plat->waypoints[i].z };
            char label[32];
            snprintf(label, sizeof(label), "WP %zu##Plat", i);
            if (ImGui::DragFloat3(label, wp, 0.1f)) {
                plat->waypoints[i] = Math::Vector3(wp[0], wp[1], wp[2]);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                plat->waypoints.erase(plat->waypoints.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Add Waypoint##Plat")) {
            plat->waypoints.push_back(Math::Vector3(0, 0, 0));
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Speed##Plat", &plat->speed, 0.1f, 0.0f, 50.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Wait Time##Plat", &plat->waitTime, 0.1f, 0.0f, 30.0f);

        const char* modes[] = { "Loop", "Ping Pong", "One Way", "Triggered" };
        int modeIdx = static_cast<int>(plat->mode);
        if (InspectorUndo::Combo(m_UndoRedo, "Mode##Plat", &modeIdx, modes, 4)) {
            plat->mode = static_cast<ECS::MovingPlatformComponent::PlatformMode>(modeIdx);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Carry Entities##Plat", &plat->carryEntities);
    }
}

void EditorLayer::DrawDamageResistanceComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Damage Resistance", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("DamResCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::DamageResistanceComponent>(entity, "damageResistance", "Damage Resistance");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* r = m_World->GetComponent<ECS::DamageResistanceComponent>(entity);
        if (!r) return;
        DrawComponentHelp("damageResistance", m_World, entity);
        InspectorUndo::SliderFloat(m_UndoRedo, "Physical", &r->physicalMult, 0.0f, 3.0f, "%.2f");
        InspectorUndo::SliderFloat(m_UndoRedo, "Fire", &r->fireMult, 0.0f, 3.0f, "%.2f");
        InspectorUndo::SliderFloat(m_UndoRedo, "Ice", &r->iceMult, 0.0f, 3.0f, "%.2f");
        InspectorUndo::SliderFloat(m_UndoRedo, "Electric", &r->electricMult, 0.0f, 3.0f, "%.2f");
        InspectorUndo::SliderFloat(m_UndoRedo, "Poison", &r->poisonMult, 0.0f, 3.0f, "%.2f");
        InspectorUndo::SliderFloat(m_UndoRedo, "Magic", &r->magicMult, 0.0f, 3.0f, "%.2f");
        ImGui::TextWrapped("0.0 = Immune, 1.0 = Normal, 2.0+ = Weakness");
    }
}

void EditorLayer::DrawResourceComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Resource", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ResCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ResourceComponent>(entity, "resource", "Resource");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* r = m_World->GetComponent<ECS::ResourceComponent>(entity);
        if (!r) return;
        DrawComponentHelp("resource", m_World, entity);

        char buf[64];
        strncpy(buf, r->resourceName.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Name", buf, sizeof(buf), [r](const std::string& val) { r->resourceName = val; });

        // Progress bar
        f32 pct = r->GetPercent();
        ImGui::ProgressBar(pct, ImVec2(-1, 0),
            (std::to_string((int)r->currentValue) + " / " + std::to_string((int)r->maxValue)).c_str());

        InspectorUndo::DragFloat(m_UndoRedo, "Max Value", &r->maxValue, 1.0f, 1.0f, 10000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Current Value", &r->currentValue, 1.0f, 0.0f, r->maxValue);

        if (ImGui::TreeNode("Regeneration##Res")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Regen Rate (/s)", &r->regenRate, 0.5f, 0.0f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Regen Delay (s)", &r->regenDelay, 0.1f, 0.0f, 10.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Depleted Threshold", &r->depletedThreshold, 1.0f, 0.0f, r->maxValue);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Action Costs")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Sprint Cost/s", &r->sprintCostPerSec, 0.5f, 0.0f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Jump Cost", &r->jumpCost, 0.5f, 0.0f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Dash Cost", &r->dashCost, 0.5f, 0.0f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Attack Cost", &r->attackCost, 0.5f, 0.0f, 100.0f);
            ImGui::TreePop();
        }

        ImGui::Text("Depleted: %s", r->depleted ? "Yes" : "No");
    }
}

void EditorLayer::DrawFootstepComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Footsteps", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("FootCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::FootstepComponent>(entity, "footstep", "Footstep");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* f = m_World->GetComponent<ECS::FootstepComponent>(entity);
        if (!f) return;
        DrawComponentHelp("footstep", m_World, entity);

        char walkBuf[256], runBuf[256];
        strncpy(walkBuf, f->defaultWalkSound.c_str(), sizeof(walkBuf) - 1); walkBuf[sizeof(walkBuf) - 1] = '\0';
        strncpy(runBuf, f->defaultRunSound.c_str(), sizeof(runBuf) - 1); runBuf[sizeof(runBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Default Walk Sound", walkBuf, sizeof(walkBuf), [f](const std::string& val) { f->defaultWalkSound = val; });
        InspectorUndo::InputText(m_UndoRedo, "Default Run Sound", runBuf, sizeof(runBuf), [f](const std::string& val) { f->defaultRunSound = val; });

        InspectorUndo::DragFloat(m_UndoRedo, "Walk Interval (s)", &f->walkStepInterval, 0.01f, 0.1f, 2.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Run Interval (s)", &f->runStepInterval, 0.01f, 0.05f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Volume", &f->volume, 0.01f, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Pitch Variance", &f->pitchVariance, 0.01f, 0.0f, 0.5f);

        if (ImGui::TreeNode("Surface Sounds")) {
            for (usize i = 0; i < f->surfaceSounds.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto& ss = f->surfaceSounds[i];
                char tag[64], walk[256], run[256];
                strncpy(tag, ss.surfaceTag.c_str(), sizeof(tag) - 1); tag[sizeof(tag) - 1] = '\0';
                strncpy(walk, ss.walkSound.c_str(), sizeof(walk) - 1); walk[sizeof(walk) - 1] = '\0';
                strncpy(run, ss.runSound.c_str(), sizeof(run) - 1); run[sizeof(run) - 1] = '\0';
                if (ImGui::InputText("Tag", tag, sizeof(tag))) ss.surfaceTag = tag;
                if (ImGui::InputText("Walk", walk, sizeof(walk))) ss.walkSound = walk;
                if (ImGui::InputText("Run", run, sizeof(run))) ss.runSound = run;
                ImGui::DragFloat("Vol Scale", &ss.volumeScale, 0.01f, 0.0f, 2.0f);
                if (ImGui::Button("Remove")) {
                    f->surfaceSounds.erase(f->surfaceSounds.begin() + i);
                    ImGui::PopID();
                    break;
                }
                ImGui::Separator();
                ImGui::PopID();
            }
            if (ImGui::Button("Add Surface")) {
                f->surfaceSounds.push_back({});
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawPoolableComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Poolable", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("PoolCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::PoolableComponent>(entity, "poolable", "Poolable");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* p = m_World->GetComponent<ECS::PoolableComponent>(entity);
        if (!p) return;
        DrawComponentHelp("poolable", m_World, entity);

        char buf[64];
        strncpy(buf, p->poolId.c_str(), sizeof(buf) - 1); buf[sizeof(buf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Pool ID", buf, sizeof(buf), [p](const std::string& val) { p->poolId = val; });
        InspectorUndo::Checkbox(m_UndoRedo, "Active", &p->isActive);
        InspectorUndo::DragFloat(m_UndoRedo, "Lifetime (0=inf)", &p->lifetime, 0.1f, 0.0f, 60.0f);
        ImGui::Text("Active Time: %.1f", p->activeTime);
    }
}

void EditorLayer::DrawQuestStateComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Quest State", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("QuestCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::QuestStateComponent>(entity, "questState", "Quest State");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* q = m_World->GetComponent<ECS::QuestStateComponent>(entity);
        if (!q) return;
        DrawComponentHelp("questState", m_World, entity);

        char buf[128];
        strncpy(buf, q->questId.c_str(), sizeof(buf) - 1); buf[sizeof(buf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Quest ID", buf, sizeof(buf), [q](const std::string& val) { q->questId = val; });

        const char* statuses[] = { "Not Started", "Active", "Completed", "Failed" };
        int status = static_cast<int>(q->status);
        if (InspectorUndo::Combo(m_UndoRedo, "Status", &status, statuses, 4)) {
            q->status = static_cast<ECS::QuestStateComponent::Status>(status);
        }

        InspectorUndo::DragInt(m_UndoRedo, "Current Objective", &q->currentObjective, 1, 0, 100);
        ImGui::Text("Time Elapsed: %.1f s", q->timeElapsed);

        if (ImGui::TreeNode("Objectives")) {
            for (usize i = 0; i < q->objectiveFlags.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto& [name, complete] = q->objectiveFlags[i];
                char objBuf[128];
                strncpy(objBuf, name.c_str(), sizeof(objBuf) - 1); objBuf[sizeof(objBuf) - 1] = '\0';
                ImGui::Checkbox("##done", &complete);
                ImGui::SameLine();
                if (ImGui::InputText("##name", objBuf, sizeof(objBuf))) name = objBuf;
                ImGui::SameLine();
                if (ImGui::Button("X")) {
                    q->objectiveFlags.erase(q->objectiveFlags.begin() + i);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add Objective")) {
                q->objectiveFlags.push_back({"New Objective", false});
            }
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawHUDWidgetComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("HUD Widget", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("HUDCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::HUDWidgetComponent>(entity, "hudWidget", "HUD Widget");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* h = m_World->GetComponent<ECS::HUDWidgetComponent>(entity);
        if (!h) return;
        DrawComponentHelp("hudWidget", m_World, entity);

        const char* types[] = { "Health Bar", "Resource Bar", "Label", "Objective Marker", "Crosshair", "Minimap" };
        int type = static_cast<int>(h->type);
        if (InspectorUndo::Combo(m_UndoRedo, "Widget Type", &type, types, 6)) {
            h->type = static_cast<ECS::HUDWidgetComponent::WidgetType>(type);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Visible", &h->visible);
        InspectorUndo::Checkbox(m_UndoRedo, "Screen Space", &h->screenSpace);

        if (ImGui::TreeNode("Position & Size")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Anchor X", &h->anchorX, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Anchor Y", &h->anchorY, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Width", &h->width, 0.01f, 0.01f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Height", &h->height, 0.01f, 0.01f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Colors")) {
            f32 fillCol[3] = { h->fillColor.x, h->fillColor.y, h->fillColor.z };
            InspectorUndo::ColorEdit3(m_UndoRedo, "Fill Color", fillCol, [h](f32 r, f32 g, f32 b) { h->fillColor = Math::Vector3(r, g, b); });
            f32 bgCol[3] = { h->bgColor.x, h->bgColor.y, h->bgColor.z };
            InspectorUndo::ColorEdit3(m_UndoRedo, "Bg Color", bgCol, [h](f32 r, f32 g, f32 b) { h->bgColor = Math::Vector3(r, g, b); });
            f32 textCol[3] = { h->textColor.x, h->textColor.y, h->textColor.z };
            InspectorUndo::ColorEdit3(m_UndoRedo, "Text Color", textCol, [h](f32 r, f32 g, f32 b) { h->textColor = Math::Vector3(r, g, b); });
            ImGui::TreePop();
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Font Size", &h->fontSize, 1.0f, 8.0f, 64.0f);

        char textBuf[256];
        strncpy(textBuf, h->text.c_str(), sizeof(textBuf) - 1); textBuf[sizeof(textBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Text", textBuf, sizeof(textBuf), [h](const std::string& val) { h->text = val; });

        char bindBuf[64];
        strncpy(bindBuf, h->bindField.c_str(), sizeof(bindBuf) - 1); bindBuf[sizeof(bindBuf) - 1] = '\0';
        InspectorUndo::InputText(m_UndoRedo, "Bind Field", bindBuf, sizeof(bindBuf), [h](const std::string& val) { h->bindField = val; });

        InspectorUndo::DragFloat(m_UndoRedo, "Current Value", &h->currentValue, 0.1f, 0.0f, 10000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Max Value", &h->maxValue, 0.1f, 0.1f, 10000.0f);

        if (!h->screenSpace) {
            f32 worldOff[3] = { h->worldOffset.x, h->worldOffset.y, h->worldOffset.z };
            InspectorUndo::DragFloat3(m_UndoRedo, "World Offset", worldOff, [h](f32 x, f32 y, f32 z) { h->worldOffset = Math::Vector3(x, y, z); }, 0.1f);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Render Distance", &h->maxRenderDistance, 1.0f, 1.0f, 500.0f);
        }
    }
}

void EditorLayer::DrawCinematicCameraComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Cinematic Camera", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("CineCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::CinematicCameraComponent>(entity, "cinematicCamera", "Cinematic Camera");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* c = m_World->GetComponent<ECS::CinematicCameraComponent>(entity);
        if (!c) return;
        DrawComponentHelp("cinematicCamera", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Loop", &c->loop);
        InspectorUndo::Checkbox(m_UndoRedo, "Auto Play", &c->autoPlay);
        InspectorUndo::Checkbox(m_UndoRedo, "Hide HUD", &c->hideHUD);
        InspectorUndo::Checkbox(m_UndoRedo, "Disable Input", &c->disableInput);

        ImGui::Text("Status: %s", c->isPlaying ? "Playing" : (c->isComplete ? "Complete" : "Stopped"));
        ImGui::Text("Time: %.1f s | Segment: %d / %d", c->currentTime, c->currentSegment, (int)c->waypoints.size() - 1);

        if (ImGui::TreeNode("Waypoints")) {
            for (usize i = 0; i < c->waypoints.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto& wp = c->waypoints[i];
                if (ImGui::TreeNode("Waypoint", "Waypoint %d", (int)i)) {
                    ImGui::DragFloat3("Position", &wp.position.x, 0.1f);
                    ImGui::DragFloat3("Look At", &wp.lookAt.x, 0.1f);
                    ImGui::DragFloat("FOV", &wp.fov, 1.0f, 10.0f, 120.0f);
                    ImGui::DragFloat("Duration (s)", &wp.duration, 0.1f, 0.0f, 30.0f);
                    ImGui::DragFloat("Hold Time (s)", &wp.holdTime, 0.1f, 0.0f, 10.0f);

                    const char* easings[] = { "Linear", "Ease In", "Ease Out", "Ease In-Out", "Smash Cut" };
                    int easing = static_cast<int>(wp.easing);
                    if (ImGui::Combo("Easing", &easing, easings, 5)) {
                        wp.easing = static_cast<ECS::CinematicCameraComponent::Waypoint::Easing>(easing);
                    }

                    // Use current camera position for this waypoint
                    if (m_Camera && ImGui::Button("Set From Camera")) {
                        wp.position = m_Camera->GetPosition();
                        wp.lookAt = m_Camera->GetPosition() + m_Camera->GetForward() * 10.0f;
                        wp.fov = m_Camera->GetFOV();
                    }

                    if (ImGui::Button("Remove")) {
                        c->waypoints.erase(c->waypoints.begin() + i);
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add Waypoint")) {
                ECS::CinematicCameraComponent::Waypoint wp;
                if (m_Camera) {
                    wp.position = m_Camera->GetPosition();
                    wp.lookAt = m_Camera->GetPosition() + m_Camera->GetForward() * 10.0f;
                    wp.fov = m_Camera->GetFOV();
                }
                c->waypoints.push_back(wp);
            }
            ImGui::TreePop();
        }
    }
}

// ============================================================================
// Tween Component
// ============================================================================

void EditorLayer::DrawTweenComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Tween", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("TweenCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::TweenComponent>(entity, "tween", "Tween");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* tc = m_World->GetComponent<ECS::TweenComponent>(entity);
        if (!tc) return;
        DrawComponentHelp("tween", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Auto-Play", &tc->autoPlay);

        static const char* propertyNames[] = { "Position", "Rotation", "Scale", "Base Color", "Emissive Color", "Opacity", "Float" };
        static const char* easingNames[] = {
            "Linear",
            "Ease In Quad", "Ease Out Quad", "Ease In-Out Quad",
            "Ease In Cubic", "Ease Out Cubic", "Ease In-Out Cubic",
            "Ease In Quart", "Ease Out Quart", "Ease In-Out Quart",
            "Ease In Sine", "Ease Out Sine", "Ease In-Out Sine",
            "Ease In Expo", "Ease Out Expo", "Ease In-Out Expo",
            "Ease In Back", "Ease Out Back", "Ease In-Out Back",
            "Ease In Elastic", "Ease Out Elastic", "Ease In-Out Elastic",
            "Ease In Bounce", "Ease Out Bounce", "Ease In-Out Bounce"
        };
        static const char* modeNames[] = { "Once", "Loop", "Ping-Pong" };

        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(tc->tweens.size()); ++i) {
            ImGui::PushID(i);
            auto& tw = tc->tweens[i];

            char label[32];
            snprintf(label, sizeof(label), "Tween %d", i);
            if (ImGui::TreeNode(label)) {
                int prop = static_cast<int>(tw.property);
                if (ImGui::Combo("Property", &prop, propertyNames, static_cast<int>(ECS::TweenProperty::COUNT))) {
                    tw.property = static_cast<ECS::TweenProperty>(prop);
                }

                int easing = static_cast<int>(tw.easing);
                if (ImGui::Combo("Easing", &easing, easingNames, static_cast<int>(ECS::EasingType::COUNT))) {
                    tw.easing = static_cast<ECS::EasingType>(easing);
                }

                int mode = static_cast<int>(tw.mode);
                if (ImGui::Combo("Mode", &mode, modeNames, static_cast<int>(ECS::TweenMode::COUNT))) {
                    tw.mode = static_cast<ECS::TweenMode>(mode);
                }

                ImGui::Checkbox("Use Current as Start", &tw.useCurrentAsStart);

                if (tw.useCurrentAsStart) {
                    ImGui::BeginDisabled();
                }
                if (tw.property == ECS::TweenProperty::Opacity) {
                    ImGui::DragFloat("Start Value", &tw.startValue.x, 0.01f, 0.0f, 1.0f);
                } else if (tw.property == ECS::TweenProperty::Float) {
                    ImGui::DragFloat("Start Value", &tw.startValue.x, 0.1f);
                } else {
                    ImGui::DragFloat3("Start Value", &tw.startValue.x, 0.1f);
                }
                if (tw.useCurrentAsStart) {
                    ImGui::EndDisabled();
                }

                if (tw.property == ECS::TweenProperty::Opacity) {
                    ImGui::DragFloat("End Value", &tw.endValue.x, 0.01f, 0.0f, 1.0f);
                } else if (tw.property == ECS::TweenProperty::Float) {
                    ImGui::DragFloat("End Value", &tw.endValue.x, 0.1f);
                } else {
                    ImGui::DragFloat3("End Value", &tw.endValue.x, 0.1f);
                }

                ImGui::DragFloat("Duration (s)", &tw.duration, 0.05f, 0.01f, 60.0f);
                ImGui::DragFloat("Delay (s)", &tw.delay, 0.05f, 0.0f, 30.0f);

                // On Complete callback (most useful for Once mode)
                if (tw.mode == ECS::TweenMode::Once) {
                    char callbackBuf[128] = {};
                    std::strncpy(callbackBuf, tw.onCompleteCallback.c_str(), sizeof(callbackBuf) - 1);
                    if (ImGui::InputText("On Complete", callbackBuf, sizeof(callbackBuf))) {
                        tw.onCompleteCallback = callbackBuf;
                    }
                }

                // Progress bar during play mode
                if (tw.isPlaying && tw.duration > 0.0f) {
                    f32 progress = std::clamp((tw.elapsed - tw.delay) / tw.duration, 0.0f, 1.0f);
                    ImGui::ProgressBar(progress);
                    // Show current value for Float property
                    if (tw.property == ECS::TweenProperty::Float) {
                        ImGui::Text("Current Value: %.3f", tw.currentValue.x);
                    }
                } else if (tw.isComplete) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Complete");
                }

                if (ImGui::Button("Remove")) {
                    removeIdx = i;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (removeIdx >= 0 && removeIdx < static_cast<int>(tc->tweens.size())) {
            tc->tweens.erase(tc->tweens.begin() + removeIdx);
        }

        if (ImGui::Button("+ Add Tween")) {
            tc->tweens.push_back(ECS::TweenEntry{});
        }
    }
}

// ============================================================================
// Joint & Ragdoll Components
// ============================================================================

void EditorLayer::DrawAnimationRecorderComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Animation Recorder", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("AnimRecorderCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::AnimationRecorderComponent>(entity, "animationRecorder", "Animation Recorder");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* rec = m_World->GetComponent<ECS::AnimationRecorderComponent>(entity);
        if (!rec) return;
        DrawComponentHelp("animationRecorder", m_World, entity);

        // Animation name input
        char nameBuf[128];
        strncpy(nameBuf, rec->recordedAnimName.c_str(), sizeof(nameBuf) - 1);
        nameBuf[sizeof(nameBuf) - 1] = '\0';
        if (ImGui::InputText("Animation Name", nameBuf, sizeof(nameBuf))) {
            rec->recordedAnimName = nameBuf;
        }

        // Sample rate
        f32 fps = (rec->recordInterval > 0.0f) ? (1.0f / rec->recordInterval) : 30.0f;
        if (ImGui::DragFloat("Sample Rate (FPS)", &fps, 1.0f, 1.0f, 120.0f, "%.0f")) {
            rec->recordInterval = (fps > 0.0f) ? (1.0f / fps) : (1.0f / 30.0f);
        }
        ImGui::SetItemTooltip("Keyframes captured per second");

        // Record / Stop buttons
        ImGui::Separator();
        if (!rec->recording) {
            bool hasAnimator = m_World->HasComponent<ECS::AnimatorComponent>(entity);
            bool hasSkeleton = m_World->HasComponent<ECS::SkeletonComponent>(entity);
            bool canRecord = hasAnimator && hasSkeleton;

            if (!canRecord) ImGui::BeginDisabled();
            if (ImGui::Button("Record")) {
                Animation::AnimationRecorderSystem::StartRecording(m_World, entity);
            }
            if (!canRecord) {
                ImGui::EndDisabled();
                ImGui::SetItemTooltip("Requires both AnimatorComponent and SkeletonComponent");
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Stop Recording")) {
                Animation::AnimationRecorderSystem::StopRecording(m_World, entity);
            }
            ImGui::PopStyleColor();
        }

        // Status display
        if (rec->recording) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "RECORDING");
            ImGui::Text("Time: %.2f s", rec->totalRecordedTime);
            usize totalKeyframes = 0;
            for (const auto& track : rec->tracks) {
                totalKeyframes += track.keyframes.size();
            }
            ImGui::Text("Tracks: %u | Keyframes: %u",
                         static_cast<u32>(rec->tracks.size()),
                         static_cast<u32>(totalKeyframes));
        }

        ImGui::Text("Recordings completed: %d", rec->recordCount);
    }
}

// ============================================================================
// Runtime Dialogue System
// ============================================================================


void EditorLayer::DrawCameraFrustum(ECS::Entity cameraEntity) {
    if (!m_Camera || !m_Renderer || !m_World) {
        return;
    }

    auto* camComp = m_World->GetComponent<ECS::CameraComponent>(cameraEntity);
    auto* transform = m_World->GetComponent<ECS::TransformComponent>(cameraEntity);
    if (!camComp || !transform) {
        return;
    }

    f32 screenWidth = m_EditorViewportImageMaxX - m_EditorViewportImageMinX;
    f32 screenHeight = m_EditorViewportImageMaxY - m_EditorViewportImageMinY;
    if (screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    // Get editor camera matrices for projection
    Math::Matrix4 viewMat = m_Camera->GetViewMatrix();
    Math::Matrix4 projMat = m_Camera->GetProjectionMatrix();
    Math::Matrix4 viewProj = projMat * viewMat;

    // Project world position to editor viewport screen position
    auto worldToScreen = [&](const Math::Vector3& worldPos, ImVec2& screenPos) -> bool {
        Math::Vector4 clipPos = viewProj * Math::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
        if (clipPos.w <= 0.001f) return false;
        f32 ndcX = clipPos.x / clipPos.w;
        f32 ndcY = clipPos.y / clipPos.w;
        f32 ndcZ = clipPos.z / clipPos.w;
        if (ndcZ < 0.0f || ndcZ > 1.0f) return false;
        screenPos.x = (ndcX + 1.0f) * 0.5f * screenWidth + m_EditorViewportImageMinX;
        screenPos.y = (ndcY + 1.0f) * 0.5f * screenHeight + m_EditorViewportImageMinY;
        return true;
    };

    ImDrawList* drawList = GetViewportOverlayDrawList();

    // Calculate camera orientation
    Math::Vector3 camPos = transform->position;
    Math::Vector3 forward = transform->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
    Math::Vector3 up = transform->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
    Math::Vector3 right = transform->rotation.Rotate(Math::Vector3(1.0f, 0.0f, 0.0f));

    // Frustum parameters
    f32 fov = camComp->fieldOfView * (3.14159f / 180.0f);
    f32 aspect = screenWidth / screenHeight;
    f32 nearDist = camComp->nearPlane;
    f32 farDist = Math::Min(camComp->farPlane, 50.0f); // Clamp far plane for visualization

    // Calculate frustum dimensions at near and far planes
    f32 nearHeight = 2.0f * Math::Tan(fov * 0.5f) * nearDist;
    f32 nearWidth = nearHeight * aspect;
    f32 farHeight = 2.0f * Math::Tan(fov * 0.5f) * farDist;
    f32 farWidth = farHeight * aspect;

    // Calculate frustum corner points
    Math::Vector3 nearCenter = camPos + forward * nearDist;
    Math::Vector3 farCenter = camPos + forward * farDist;

    Math::Vector3 nearTopLeft = nearCenter + up * (nearHeight * 0.5f) - right * (nearWidth * 0.5f);
    Math::Vector3 nearTopRight = nearCenter + up * (nearHeight * 0.5f) + right * (nearWidth * 0.5f);
    Math::Vector3 nearBottomLeft = nearCenter - up * (nearHeight * 0.5f) - right * (nearWidth * 0.5f);
    Math::Vector3 nearBottomRight = nearCenter - up * (nearHeight * 0.5f) + right * (nearWidth * 0.5f);

    Math::Vector3 farTopLeft = farCenter + up * (farHeight * 0.5f) - right * (farWidth * 0.5f);
    Math::Vector3 farTopRight = farCenter + up * (farHeight * 0.5f) + right * (farWidth * 0.5f);
    Math::Vector3 farBottomLeft = farCenter - up * (farHeight * 0.5f) - right * (farWidth * 0.5f);
    Math::Vector3 farBottomRight = farCenter - up * (farHeight * 0.5f) + right * (farWidth * 0.5f);

    // Colors - yellow/orange for selected camera, gray for others
    bool isSelected = IsSelected(cameraEntity);
    ImU32 frustumColor = isSelected ? IM_COL32(255, 200, 50, 180) : IM_COL32(150, 150, 150, 100);
    ImU32 directionColor = isSelected ? IM_COL32(255, 100, 50, 255) : IM_COL32(200, 100, 50, 180);
    f32 lineThickness = isSelected ? 2.0f : 1.0f;

    // Draw frustum lines from camera position to far corners
    auto drawLine3D = [&](const Math::Vector3& from, const Math::Vector3& to, ImU32 color, f32 thickness) {
        ImVec2 screenFrom, screenTo;
        if (worldToScreen(from, screenFrom) && worldToScreen(to, screenTo)) {
            drawList->AddLine(screenFrom, screenTo, color, thickness);
        }
    };

    // Draw edges from camera to near plane
    drawLine3D(camPos, nearTopLeft, frustumColor, lineThickness);
    drawLine3D(camPos, nearTopRight, frustumColor, lineThickness);
    drawLine3D(camPos, nearBottomLeft, frustumColor, lineThickness);
    drawLine3D(camPos, nearBottomRight, frustumColor, lineThickness);

    // Draw near plane rectangle
    drawLine3D(nearTopLeft, nearTopRight, frustumColor, lineThickness);
    drawLine3D(nearTopRight, nearBottomRight, frustumColor, lineThickness);
    drawLine3D(nearBottomRight, nearBottomLeft, frustumColor, lineThickness);
    drawLine3D(nearBottomLeft, nearTopLeft, frustumColor, lineThickness);

    // Draw far plane rectangle
    drawLine3D(farTopLeft, farTopRight, frustumColor, lineThickness);
    drawLine3D(farTopRight, farBottomRight, frustumColor, lineThickness);
    drawLine3D(farBottomRight, farBottomLeft, frustumColor, lineThickness);
    drawLine3D(farBottomLeft, farTopLeft, frustumColor, lineThickness);

    // Draw edges from near to far plane
    drawLine3D(nearTopLeft, farTopLeft, frustumColor, lineThickness);
    drawLine3D(nearTopRight, farTopRight, frustumColor, lineThickness);
    drawLine3D(nearBottomLeft, farBottomLeft, frustumColor, lineThickness);
    drawLine3D(nearBottomRight, farBottomRight, frustumColor, lineThickness);

    // Draw direction arrow (forward direction)
    f32 arrowLength = 1.5f;
    Math::Vector3 arrowEnd = camPos + forward * arrowLength;
    drawLine3D(camPos, arrowEnd, directionColor, lineThickness + 1.0f);

    // Draw up direction (small)
    Math::Vector3 upEnd = camPos + up * 0.5f;
    drawLine3D(camPos, upEnd, IM_COL32(50, 255, 50, 200), lineThickness);

    // Draw camera icon (small box at camera position)
    ImVec2 screenCamPos;
    if (worldToScreen(camPos, screenCamPos)) {
        f32 iconSize = isSelected ? 8.0f : 5.0f;
        drawList->AddRectFilled(
            ImVec2(screenCamPos.x - iconSize, screenCamPos.y - iconSize),
            ImVec2(screenCamPos.x + iconSize, screenCamPos.y + iconSize),
            isSelected ? IM_COL32(255, 200, 50, 255) : IM_COL32(150, 150, 150, 200)
        );
        drawList->AddRect(
            ImVec2(screenCamPos.x - iconSize, screenCamPos.y - iconSize),
            ImVec2(screenCamPos.x + iconSize, screenCamPos.y + iconSize),
            IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.0f
        );
    }
}

void EditorLayer::SetupCameraForController(ECS::Entity controllerEntity, const std::string& controllerType) {
    if (!m_World) return;

    // Check if a game camera already exists
    ECS::Entity existingCamera = ECS::CameraManager::GetActiveCamera(m_World);

    // Get the controller entity's transform for positioning the camera relative to it
    auto* playerTransform = m_World->GetComponent<ECS::TransformComponent>(controllerEntity);
    Math::Vector3 playerPos = playerTransform ? playerTransform->position : Math::Vector3(0.0f, 0.0f, 0.0f);

    // Create a camera entity if none exists
    ECS::Entity cameraEntity;
    if (existingCamera == ECS::INVALID_ENTITY) {
        cameraEntity = m_World->CreateEntity();
        auto& name = m_World->AddComponent<ECS::NameComponent>(cameraEntity);
        name.name = "Game Camera";
        m_World->AddComponent<ECS::TransformComponent>(cameraEntity);
        m_World->AddComponent<ECS::CameraComponent>(cameraEntity);
        ENJIN_LOG_INFO(Editor, "Auto-created Game Camera for %s controller", controllerType.c_str());
    } else {
        cameraEntity = existingCamera;
    }

    auto* camTransform = m_World->GetComponent<ECS::TransformComponent>(cameraEntity);
    auto* camComp = m_World->GetComponent<ECS::CameraComponent>(cameraEntity);
    if (!camTransform || !camComp) return;

    // Configure camera based on controller type
    if (controllerType == "Platformer2D") {
        // Side-scroller: orthographic, looking along -Z, offset behind player
        camComp->projectionType = ECS::ProjectionType::Orthographic;
        camComp->orthoSize = 8.0f;
        camComp->nearPlane = 0.1f;
        camComp->farPlane = 100.0f;
        camTransform->position = playerPos + Math::Vector3(0.0f, 0.0f, 15.0f);
        camTransform->rotation = Math::Quaternion::Identity();
    } else if (controllerType == "TopDown2D") {
        // Top-down 2D: orthographic, looking along -Z at XY plane
        camComp->projectionType = ECS::ProjectionType::Orthographic;
        camComp->orthoSize = 12.0f;
        camComp->nearPlane = 0.1f;
        camComp->farPlane = 100.0f;
        camTransform->position = playerPos + Math::Vector3(0.0f, 0.0f, 15.0f);
        camTransform->rotation = Math::Quaternion::Identity();
    } else if (controllerType == "TopDown3D") {
        // Isometric/CRPG: perspective, ~45° angle looking down and slightly behind
        camComp->projectionType = ECS::ProjectionType::Perspective;
        camComp->fieldOfView = 50.0f;
        camComp->nearPlane = 0.1f;
        camComp->farPlane = 500.0f;
        f32 distance = 15.0f;
        f32 angle = Math::Radians(45.0f);
        camTransform->position = playerPos + Math::Vector3(
            0.0f,
            distance * Math::Sin(angle),
            distance * Math::Cos(angle));
        // Look down at 45 degrees
        camTransform->rotation = Math::Quaternion(
            Math::Vector3(1.0f, 0.0f, 0.0f), -angle);
    } else if (controllerType == "ThirdPerson") {
        // Over-the-shoulder: perspective, behind and above player
        // Match ThirdPersonController defaults: cameraDistance=5, cameraHeight=2, cameraPitch=20
        camComp->projectionType = ECS::ProjectionType::Perspective;
        camComp->fieldOfView = 60.0f;
        camComp->nearPlane = 0.1f;
        camComp->farPlane = 1000.0f;
        f32 pitchRad = Math::Radians(20.0f);
        f32 dist = 5.0f;
        f32 height = 2.0f;
        camTransform->position = playerPos + Math::Vector3(
            0.0f,
            Math::Sin(pitchRad) * dist + height,
            Math::Cos(pitchRad) * dist);
        camTransform->rotation = Math::Quaternion(
            Math::Vector3(1.0f, 0.0f, 0.0f), -pitchRad);
    } else if (controllerType == "FirstPerson") {
        // First-person: perspective, at player eye height
        camComp->projectionType = ECS::ProjectionType::Perspective;
        camComp->fieldOfView = 75.0f;
        camComp->nearPlane = 0.05f;
        camComp->farPlane = 1000.0f;
        camTransform->position = playerPos + Math::Vector3(0.0f, 1.7f, 0.0f);
        camTransform->rotation = Math::Quaternion::Identity();
    }

    // Make sure the camera is active
    camComp->isActive = true;
    camComp->priority = 10;

    // Select the camera so the user can adjust it
    m_SelectedGameCamera = cameraEntity;
}

// ============================================================================
// Creative Intelligence — Helpers
// ============================================================================


// (X/Y/W/H <-> anchor conversion now lives on UICanvasComponent as
// GetDesignRect/SetDesignRect — anchor-preserving, parent-chain aware.)

void EditorLayer::OpenUIEditor(ECS::Entity canvasEntity) {
    if (!m_World || !m_World->HasComponent<GUI::UICanvasComponent>(canvasEntity)) return;
    SelectEntity(canvasEntity);
    m_UIEditMode = true;
    m_UIEditCanvasEntity = canvasEntity;
    m_UIEditSelectedElementId = 0;
    // Make sure the Game View is visible -- that's where the WYSIWYG overlay lives
    SetPanelVisibility(EditorPanel::GameView, true);
}

void EditorLayer::DrawUICanvasComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("UI Canvas", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("UICanvasCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<GUI::UICanvasComponent>(entity, "uiCanvas", "UI Canvas");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (!open) return;

    auto* canvas = m_World->GetComponent<GUI::UICanvasComponent>(entity);
    if (!canvas) return;
    DrawComponentHelp("uiCanvas", m_World, entity);

    // --- Canvas Settings ---
    char nameBuf[128];
    strncpy(nameBuf, canvas->canvasName.c_str(), sizeof(nameBuf) - 1); nameBuf[sizeof(nameBuf) - 1] = '\0';
    if (ImGui::InputText("Canvas Name", nameBuf, sizeof(nameBuf))) canvas->canvasName = nameBuf;

    ImGui::Checkbox("Visible", &canvas->visible);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::DragInt("Sort Order", &canvas->sortOrder, 1, -100, 1000);

    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat("Design W", &canvas->designWidth, 1.0f, 320.0f, 7680.0f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat("Design H", &canvas->designHeight, 1.0f, 240.0f, 4320.0f);

    const char* scaleModes[] = { "Scale With Screen", "Constant Pixel", "Constant Physical" };
    int scaleMode = static_cast<int>(canvas->scaleMode);
    if (ImGui::Combo("Scale Mode", &scaleMode, scaleModes, 3)) {
        canvas->scaleMode = static_cast<GUI::UIScaleMode>(scaleMode);
    }

    // UI Edit Mode toggle
    if (ImGui::Checkbox("Edit in Viewport", &m_UIEditMode)) {
        if (m_UIEditMode) {
            m_UIEditCanvasEntity = entity;
            m_TerrainEditMode = false;
            m_TilemapEditMode = false;
        } else {
            m_UIEditCanvasEntity = ECS::INVALID_ENTITY;
        }
    }
    if (m_UIEditMode && m_UIEditCanvasEntity == entity) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "ACTIVE");
    }

    // Theme (collapsible — rarely changed)
    if (ImGui::TreeNode("Theme")) {
        auto& theme = canvas->theme;

        const char* presets[] = { "Dark", "Light", "RetroGreen", "Fantasy" };
        int presetIdx = static_cast<int>(
            theme.name == "Light" ? 1 :
            theme.name == "RetroGreen" ? 2 :
            theme.name == "Fantasy" ? 3 : 0);
        if (ImGui::Combo("Preset", &presetIdx, presets, 4)) {
            if (presetIdx >= 0 && presetIdx < static_cast<int>(GUI::UIThemePreset::Count)) {
                theme = GUI::UITheme::FromPreset(static_cast<GUI::UIThemePreset>(presetIdx));
            }
        }

        ImGui::ColorEdit3("Primary", &theme.primary.x);
        ImGui::ColorEdit3("Secondary", &theme.secondary.x);
        ImGui::ColorEdit3("Background", &theme.background.x);
        ImGui::ColorEdit3("Surface", &theme.surface.x);
        ImGui::ColorEdit3("Error", &theme.error.x);
        ImGui::ColorEdit3("Text Primary", &theme.textPrimary.x);
        ImGui::ColorEdit3("Text Secondary", &theme.textSecondary.x);
        ImGui::ColorEdit3("Button Default", &theme.buttonDefault.x);
        ImGui::ColorEdit3("Button Hovered", &theme.buttonHovered.x);
        ImGui::ColorEdit3("Button Pressed", &theme.buttonPressed.x);
        ImGui::ColorEdit3("Slider Fill", &theme.sliderFill.x);
        ImGui::ColorEdit3("Slider Track", &theme.sliderTrack.x);
        ImGui::DragFloat("Border Radius", &theme.borderRadius, 0.5f, 0.0f, 20.0f);
        ImGui::DragFloat("Border Width", &theme.borderWidth, 0.25f, 0.0f, 5.0f);
        ImGui::DragFloat("Font Size Body", &theme.fontSizeBody, 0.5f, 8.0f, 48.0f);
        ImGui::DragFloat("Font Size Heading", &theme.fontSizeHeading, 0.5f, 12.0f, 72.0f);
        ImGui::DragFloat("BG Alpha", &theme.bgAlpha, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Focus Border Width", &theme.focusBorderWidth, 0.25f, 0.0f, 6.0f);
        ImGui::ColorEdit3("Focus Color", &theme.inputFocused.x);

        ImGui::TreePop();
    }

    ImGui::Separator();

    // --- Elements List (flat with layer reordering) ---
    ImGui::TextDisabled("Elements");

    for (usize i = 0; i < canvas->elements.size(); ++i) {
        auto& elem = canvas->elements[i];
        if (elem.parentId != 0) continue; // Show root-level only in flat list

        ImGui::PushID(static_cast<int>(elem.id));

        bool isSelected = (m_UIEditSelectedElementId == elem.id);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.42f, 0.30f, 1.0f));
        }

        // Element button (click to select)
        std::string label = elem.name;
        if (!elem.visible) label += " (hidden)";
        if (ImGui::Button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 60.0f, 0))) {
            m_UIEditSelectedElementId = elem.id;
        }

        if (isSelected) {
            ImGui::PopStyleColor();
        }

        // Up/down layer buttons
        ImGui::SameLine();
        if (ImGui::SmallButton("^")) {
            canvas->MoveElementUp(elem.id);
        }
        ImGui::SetItemTooltip("Move up (draws earlier)");
        ImGui::SameLine();
        if (ImGui::SmallButton("v")) {
            canvas->MoveElementDown(elem.id);
        }
        ImGui::SetItemTooltip("Move down (draws later)");

        ImGui::PopID();
    }

    ImGui::Spacing();

    // Add element buttons (compact row)
    if (ImGui::SmallButton("+Panel")) canvas->AddElement(GUI::UIWidgetType::Panel, "Panel");
    ImGui::SameLine();
    if (ImGui::SmallButton("+Button")) canvas->AddElement(GUI::UIWidgetType::Button, "Button");
    ImGui::SameLine();
    if (ImGui::SmallButton("+Label")) canvas->AddElement(GUI::UIWidgetType::Label, "Label");
    ImGui::SameLine();
    if (ImGui::SmallButton("+Image")) canvas->AddElement(GUI::UIWidgetType::Image, "Image");

    if (ImGui::SmallButton("+Progress")) canvas->AddElement(GUI::UIWidgetType::ProgressBar, "ProgressBar");
    ImGui::SameLine();
    if (ImGui::SmallButton("+Slider")) canvas->AddElement(GUI::UIWidgetType::Slider, "Slider");
    ImGui::SameLine();
    if (ImGui::SmallButton("+Checkbox")) canvas->AddElement(GUI::UIWidgetType::Checkbox, "Checkbox");
    ImGui::SameLine();
    if (ImGui::SmallButton("+Toggle")) canvas->AddElement(GUI::UIWidgetType::Toggle, "Toggle");

    if (ImGui::SmallButton("+Dropdown")) {
        u32 ddId = canvas->AddElement(GUI::UIWidgetType::Dropdown, "Dropdown");
        auto* dd = canvas->GetElement(ddId);
        if (dd) { dd->data.options = {"Option 1", "Option 2", "Option 3"}; dd->data.placeholder = "Select..."; }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+TextInput")) {
        u32 tiId = canvas->AddElement(GUI::UIWidgetType::TextInput, "TextInput");
        auto* ti = canvas->GetElement(tiId);
        if (ti) ti->data.placeholder = "Type here...";
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+Radio")) {
        u32 rgId = canvas->AddElement(GUI::UIWidgetType::RadioGroup, "RadioGroup");
        auto* rg = canvas->GetElement(rgId);
        if (rg) rg->data.options = {"Option A", "Option B", "Option C"};
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+ListView")) canvas->AddElement(GUI::UIWidgetType::ListView, "ListView");

    if (ImGui::SmallButton("+ScrollArea")) canvas->AddElement(GUI::UIWidgetType::ScrollArea, "ScrollArea");
    ImGui::SameLine();
    if (ImGui::SmallButton("+Grid")) {
        u32 gId = canvas->AddElement(GUI::UIWidgetType::Grid, "Grid");
        auto* g = canvas->GetElement(gId);
        if (g) g->data.gridColumns = 3;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+TabGroup")) canvas->AddElement(GUI::UIWidgetType::TabGroup, "TabGroup");
    ImGui::SameLine();
    if (ImGui::SmallButton("+Modal")) {
        u32 mId = canvas->AddElement(GUI::UIWidgetType::Modal, "Modal");
        auto* m = canvas->GetElement(mId);
        if (m) { m->data.text = "Modal Title"; m->visible = false; }
    }

    // Templates
    if (ImGui::SmallButton("Main Menu Template")) {
        auto tmpl = GUI::UITemplates::CreateMainMenu("My Game");
        *canvas = tmpl;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Pause Menu Template")) {
        auto tmpl = GUI::UITemplates::CreatePauseMenu();
        *canvas = tmpl;
    }

    ImGui::Separator();

    // --- Selected Element Properties (flat, no TreeNodes for common fields) ---
    auto* sel = canvas->GetElement(m_UIEditSelectedElementId);
    if (!sel) return;

    ImGui::TextDisabled("Selected: %s (#%u)", sel->name.c_str(), sel->id);

    // Row 1: Name + Type + Visible/Enabled
    char elemNameBuf[128];
    strncpy(elemNameBuf, sel->name.c_str(), sizeof(elemNameBuf) - 1); elemNameBuf[sizeof(elemNameBuf) - 1] = '\0';
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    if (ImGui::InputText("##ElemName", elemNameBuf, sizeof(elemNameBuf))) sel->name = elemNameBuf;
    ImGui::SameLine();

    const char* widgetTypes[] = {
        "Panel", "Button", "Label", "Image", "ProgressBar", "Slider", "Checkbox", "Toggle",
        "Dropdown", "TextInput", "RadioGroup", "ScrollArea", "Grid", "TabGroup", "Tooltip", "Modal", "ListView"
    };
    int widgetType = static_cast<int>(sel->type);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::Combo("##WidgetType", &widgetType, widgetTypes, 17)) {
        sel->type = static_cast<GUI::UIWidgetType>(widgetType);
    }

    ImGui::Checkbox("Visible", &sel->visible);
    ImGui::SameLine();
    ImGui::Checkbox("Enabled", &sel->enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Focusable", &sel->focusable);

    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragInt("Tab Order", &sel->tabOrder, 1, 0, 100, sel->tabOrder == 0 ? "Auto" : "%d");

    ImGui::Spacing();

    // --- Position & Size (X/Y/W/H, design-space, anchor-preserving) ---
    // GetDesignRect/SetDesignRect resolve through the CURRENT anchors, so
    // editing here no longer silently re-anchors everything to the top-left
    // corner (the old XYWHToAnchor did — which is why authored UI never
    // scaled with the play screen).
    ImGui::TextDisabled("Position & Size");
    GUI::UIRect dr = canvas->GetDesignRect(sel->id);
    f32 x = dr.x, y = dr.y, w = dr.w, h = dr.h;

    bool changed = false;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.24f);
    if (ImGui::DragFloat("##X", &x, 1.0f, -10000.0f, 10000.0f, "X: %.0f")) changed = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.32f);
    if (ImGui::DragFloat("##Y", &y, 1.0f, -10000.0f, 10000.0f, "Y: %.0f")) changed = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.48f);
    if (ImGui::DragFloat("##W", &w, 1.0f, 1.0f, 10000.0f, "W: %.0f")) changed = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat("##H", &h, 1.0f, 1.0f, 10000.0f, "H: %.0f")) changed = true;

    if (changed) {
        canvas->SetDesignRect(sel->id, x, y, w, h);
    }

    // --- Alignment Buttons ---
    {
        f32 dw = canvas->designWidth;
        f32 dh = canvas->designHeight;

        if (ImGui::SmallButton("AlignL")) { canvas->SetDesignRect(sel->id, 0, y, w, h); }
        ImGui::SameLine();
        if (ImGui::SmallButton("CenterH")) { canvas->SetDesignRect(sel->id, (dw - w) * 0.5f, y, w, h); }
        ImGui::SameLine();
        if (ImGui::SmallButton("AlignR")) { canvas->SetDesignRect(sel->id, dw - w, y, w, h); }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (ImGui::SmallButton("AlignT")) { canvas->SetDesignRect(sel->id, x, 0, w, h); }
        ImGui::SameLine();
        if (ImGui::SmallButton("CenterV")) { canvas->SetDesignRect(sel->id, x, (dh - h) * 0.5f, w, h); }
        ImGui::SameLine();
        if (ImGui::SmallButton("AlignB")) { canvas->SetDesignRect(sel->id, x, dh - h, w, h); }
    }

    // --- Anchor presets: which part of the screen the element follows ---
    // Re-anchors WITHOUT moving the element at design resolution; the anchor
    // decides how it tracks other screen sizes. 3x3 point grid + stretch.
    {
        ImGui::TextDisabled("Anchor");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Which part of the screen this element follows when the\n"
                              "window size changes. Picking a preset keeps the element\n"
                              "where it is now. Auto picks from its current position.");
        }

        const auto& an = sel->anchor;
        auto axisPreset = [](f32 mn, f32 mx) -> i32 {
            if (mn == 0.0f && mx == 1.0f) return 3;              // stretch
            if (mn == mx) {
                if (mn == 0.0f) return 0;
                if (mn == 0.5f) return 1;
                if (mn == 1.0f) return 2;
            }
            return -1;                                            // custom
        };
        i32 curX = axisPreset(an.anchorMin.x, an.anchorMax.x);
        i32 curY = axisPreset(an.anchorMin.y, an.anchorMax.y);

        static const char* rowLabels[3] = { "T", "M", "B" };
        static const char* colLabels[3] = { "L", "C", "R" };
        for (i32 py = 0; py < 3; ++py) {
            for (i32 px = 0; px < 3; ++px) {
                if (px > 0) ImGui::SameLine();
                char label[8];
                snprintf(label, sizeof(label), "%s%s##anch%d%d", rowLabels[py], colLabels[px], px, py);
                bool active = (curX == px && curY == py);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.55f, 0.96f, 1.0f));
                if (ImGui::SmallButton(label)) canvas->ApplyAnchorPreset(sel->id, px, py);
                if (active) ImGui::PopStyleColor();
            }
            ImGui::SameLine();
            if (py == 0) {
                bool active = (curX == 3 && curY != 3);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.55f, 0.96f, 1.0f));
                if (ImGui::SmallButton("Stretch X")) canvas->ApplyAnchorPreset(sel->id, 3, curY < 0 ? 1 : curY);
                if (active) ImGui::PopStyleColor();
            } else if (py == 1) {
                bool active = (curY == 3 && curX != 3);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.55f, 0.96f, 1.0f));
                if (ImGui::SmallButton("Stretch Y")) canvas->ApplyAnchorPreset(sel->id, curX < 0 ? 1 : curX, 3);
                if (active) ImGui::PopStyleColor();
            } else {
                bool active = (curX == 3 && curY == 3);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.55f, 0.96f, 1.0f));
                if (ImGui::SmallButton("Stretch All")) canvas->ApplyAnchorPreset(sel->id, 3, 3);
                if (active) ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::SmallButton("Auto")) {
                    // Pick the nearest ninth from the element's center position
                    GUI::UIRect r = canvas->GetDesignRect(sel->id);
                    f32 fx = (r.x + r.w * 0.5f) / (canvas->designWidth  > 0 ? canvas->designWidth  : 1.0f);
                    f32 fy = (r.y + r.h * 0.5f) / (canvas->designHeight > 0 ? canvas->designHeight : 1.0f);
                    i32 ax = fx < (1.0f / 3.0f) ? 0 : (fx < (2.0f / 3.0f) ? 1 : 2);
                    i32 ay = fy < (1.0f / 3.0f) ? 0 : (fy < (2.0f / 3.0f) ? 1 : 2);
                    canvas->ApplyAnchorPreset(sel->id, ax, ay);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Anchor to the screen region the element currently sits in");
                }
            }
        }
    }

    ImGui::Spacing();

    // --- Widget Data (shown directly, no TreeNode) ---
    if (sel->type == GUI::UIWidgetType::Button || sel->type == GUI::UIWidgetType::Label ||
        sel->type == GUI::UIWidgetType::Checkbox || sel->type == GUI::UIWidgetType::Toggle) {
        ImGui::TextDisabled("Text");
        char textBuf[256];
        strncpy(textBuf, sel->data.text.c_str(), sizeof(textBuf) - 1); textBuf[sizeof(textBuf) - 1] = '\0';
        if (ImGui::InputText("##Text", textBuf, sizeof(textBuf))) sel->data.text = textBuf;

        const char* hAligns[] = { "Left", "Center", "Right" };
        int hAlign = sel->data.textAlignH;
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::Combo("H Align", &hAlign, hAligns, 3)) sel->data.textAlignH = static_cast<u8>(hAlign);
        ImGui::SameLine();
        const char* vAligns[] = { "Top", "Center", "Bottom" };
        int vAlign = sel->data.textAlignV;
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::Combo("V Align", &vAlign, vAligns, 3)) sel->data.textAlignV = static_cast<u8>(vAlign);

        // Data binding: live gameplay value pushed into this element every frame
        // (same behavior on desktop, editor play, and web). "None" = static.
        ImGui::TextDisabled("Bind To");
        const char* bindOptions[] = { "None", "Player Health", "Coins" };
        const char* bindKeys[]    = { "",     "health",        "coins" };
        int bindIdx = 0;
        for (int bi = 1; bi < 3; ++bi) {
            if (sel->data.bindField == bindKeys[bi]) bindIdx = bi;
        }
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::Combo("##BindField", &bindIdx, bindOptions, 3)) {
            sel->data.bindField = bindKeys[bindIdx];
            sel->data.boundText.clear();
            MarkDirty();
        }
        if (sel->data.bindField == "coins") {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            f32 total = sel->data.bindMaxValue;
            if (ImGui::DragFloat("Total##BindMax", &total, 1.0f, 0.0f, 9999.0f, "%.0f")) {
                sel->data.bindMaxValue = total;
                MarkDirty();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Total coins (0 = auto-detect from scene)");
        }
    }

    if (sel->type == GUI::UIWidgetType::Image) {
        ImGui::TextDisabled("Image");
        char imgBuf[256];
        strncpy(imgBuf, sel->data.imagePath.c_str(), sizeof(imgBuf) - 1); imgBuf[sizeof(imgBuf) - 1] = '\0';
        if (ImGui::InputText("Image Path", imgBuf, sizeof(imgBuf))) sel->data.imagePath = imgBuf;
        ImGui::ColorEdit3("Image Tint", &sel->data.imageTint.x);
        ImGui::DragFloat("Image Alpha", &sel->data.imageAlpha, 0.01f, 0.0f, 1.0f);
    }

    if (sel->type == GUI::UIWidgetType::ProgressBar) {
        ImGui::TextDisabled("Progress Bar");
        ImGui::DragFloat("Value", &sel->data.progressValue, 0.01f, 0.0f, 1.0f);
        ImGui::ColorEdit3("Fill Color", &sel->data.progressFillColor.x);

        // Data binding (same combo as text widgets): a bound bar auto-fills from
        // live gameplay each frame on every platform.
        const char* pbBindOptions[] = { "None", "Player Health", "Coins" };
        const char* pbBindKeys[]    = { "",     "health",        "coins" };
        int pbBindIdx = 0;
        for (int bi = 1; bi < 3; ++bi) {
            if (sel->data.bindField == pbBindKeys[bi]) pbBindIdx = bi;
        }
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::Combo("Bind To##PB", &pbBindIdx, pbBindOptions, 3)) {
            sel->data.bindField = pbBindKeys[pbBindIdx];
            sel->data.boundText.clear();
            MarkDirty();
        }
    }

    if (sel->type == GUI::UIWidgetType::Slider) {
        ImGui::TextDisabled("Slider");
        ImGui::DragFloat("Value", &sel->data.sliderValue, 0.01f, sel->data.sliderMin, sel->data.sliderMax);
        ImGui::DragFloat("Min", &sel->data.sliderMin, 0.01f);
        ImGui::SameLine();
        ImGui::DragFloat("Max", &sel->data.sliderMax, 0.01f);
    }

    if (sel->type == GUI::UIWidgetType::Checkbox || sel->type == GUI::UIWidgetType::Toggle) {
        ImGui::Checkbox("Checked", &sel->data.checked);
    }

    // Dropdown
    if (sel->type == GUI::UIWidgetType::Dropdown) {
        ImGui::TextDisabled("Dropdown");
        ImGui::DragInt("Selected##DD", &sel->data.selectedOption, 1, 0, std::max(0, static_cast<int>(sel->data.options.size()) - 1));
        char placeBuf[128];
        strncpy(placeBuf, sel->data.placeholder.c_str(), sizeof(placeBuf) - 1); placeBuf[sizeof(placeBuf) - 1] = '\0';
        if (ImGui::InputText("Placeholder##DD", placeBuf, sizeof(placeBuf))) sel->data.placeholder = placeBuf;

        ImGui::Text("Options (%d):", static_cast<int>(sel->data.options.size()));
        for (int i = 0; i < static_cast<int>(sel->data.options.size()); ++i) {
            char optBuf[128];
            strncpy(optBuf, sel->data.options[i].c_str(), sizeof(optBuf) - 1); optBuf[sizeof(optBuf) - 1] = '\0';
            char label[32]; snprintf(label, sizeof(label), "##opt%d", i);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30.0f);
            if (ImGui::InputText(label, optBuf, sizeof(optBuf))) sel->data.options[i] = optBuf;
            ImGui::SameLine();
            char delLabel[32]; snprintf(delLabel, sizeof(delLabel), "X##opt%d", i);
            if (ImGui::SmallButton(delLabel)) { sel->data.options.erase(sel->data.options.begin() + i); --i; }
        }
        if (ImGui::SmallButton("+ Add Option")) sel->data.options.push_back("New Option");
    }

    // TextInput
    if (sel->type == GUI::UIWidgetType::TextInput) {
        ImGui::TextDisabled("Text Input");
        char inputBuf[256];
        strncpy(inputBuf, sel->data.inputText.c_str(), sizeof(inputBuf) - 1); inputBuf[sizeof(inputBuf) - 1] = '\0';
        if (ImGui::InputText("Input Text##TI", inputBuf, sizeof(inputBuf))) sel->data.inputText = inputBuf;
        char placeBuf2[128];
        strncpy(placeBuf2, sel->data.placeholder.c_str(), sizeof(placeBuf2) - 1); placeBuf2[sizeof(placeBuf2) - 1] = '\0';
        if (ImGui::InputText("Placeholder##TI", placeBuf2, sizeof(placeBuf2))) sel->data.placeholder = placeBuf2;
    }

    // RadioGroup
    if (sel->type == GUI::UIWidgetType::RadioGroup) {
        ImGui::TextDisabled("Radio Group");
        ImGui::DragInt("Selected##RG", &sel->data.selectedOption, 1, 0, std::max(0, static_cast<int>(sel->data.options.size()) - 1));
        ImGui::Text("Options (%d):", static_cast<int>(sel->data.options.size()));
        for (int i = 0; i < static_cast<int>(sel->data.options.size()); ++i) {
            char optBuf[128];
            strncpy(optBuf, sel->data.options[i].c_str(), sizeof(optBuf) - 1); optBuf[sizeof(optBuf) - 1] = '\0';
            char label[32]; snprintf(label, sizeof(label), "##ropt%d", i);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30.0f);
            if (ImGui::InputText(label, optBuf, sizeof(optBuf))) sel->data.options[i] = optBuf;
            ImGui::SameLine();
            char delLabel[32]; snprintf(delLabel, sizeof(delLabel), "X##ropt%d", i);
            if (ImGui::SmallButton(delLabel)) { sel->data.options.erase(sel->data.options.begin() + i); --i; }
        }
        if (ImGui::SmallButton("+ Add Radio Option")) sel->data.options.push_back("New Option");
    }

    // Grid
    if (sel->type == GUI::UIWidgetType::Grid) {
        ImGui::TextDisabled("Grid Layout");
        ImGui::DragInt("Columns", &sel->data.gridColumns, 1, 1, 12);
    }

    // TabGroup
    if (sel->type == GUI::UIWidgetType::TabGroup) {
        ImGui::TextDisabled("Tab Group");
        ImGui::DragInt("Active Tab", &sel->data.activeTabIndex, 1, 0,
            std::max(0, static_cast<int>(sel->childIds.size()) - 1));
        ImGui::TextDisabled("Add child elements as tab pages");
    }

    // Tooltip
    if (sel->type == GUI::UIWidgetType::Tooltip || !sel->data.tooltipText.empty()) {
        ImGui::TextDisabled("Tooltip");
        char tipBuf[256];
        strncpy(tipBuf, sel->data.tooltipText.c_str(), sizeof(tipBuf) - 1); tipBuf[sizeof(tipBuf) - 1] = '\0';
        if (ImGui::InputText("Tooltip Text", tipBuf, sizeof(tipBuf))) sel->data.tooltipText = tipBuf;
        ImGui::DragFloat("Tooltip Delay", &sel->data.tooltipDelay, 0.05f, 0.0f, 5.0f, "%.2f s");
    }

    // Modal
    if (sel->type == GUI::UIWidgetType::Modal) {
        ImGui::TextDisabled("Modal Dialog");
        char modalTextBuf[256];
        strncpy(modalTextBuf, sel->data.text.c_str(), sizeof(modalTextBuf) - 1); modalTextBuf[sizeof(modalTextBuf) - 1] = '\0';
        if (ImGui::InputText("Title##Modal", modalTextBuf, sizeof(modalTextBuf))) sel->data.text = modalTextBuf;
        ImGui::TextDisabled("Toggle visible to show/hide");
    }

    // ListView
    if (sel->type == GUI::UIWidgetType::ListView) {
        ImGui::TextDisabled("List View");
        ImGui::DragInt("Selected Item", &sel->data.listSelectedIndex, 1, -1,
            std::max(0, static_cast<int>(sel->childIds.size()) - 1));
        ImGui::TextDisabled("Add child elements as list items");
    }

    ImGui::Spacing();

    // --- Style (shown directly) ---
    ImGui::TextDisabled("Style");
    ImGui::ColorEdit3("BG Color", &sel->style.bgColor.x);
    ImGui::ColorEdit3("Text Color", &sel->style.textColor.x);
    ImGui::ColorEdit3("Border Color", &sel->style.borderColor.x);
    ImGui::DragFloat("BG Alpha", &sel->style.bgAlpha, 0.01f, -1.0f, 1.0f);

    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat("Border Radius", &sel->style.borderRadius, 0.5f, -1.0f, 20.0f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat("Border Width", &sel->style.borderWidth, 0.25f, -1.0f, 5.0f);

    ImGui::DragFloat("Font Size", &sel->style.fontSize, 0.5f, -1.0f, 72.0f);
    ImGui::ColorEdit3("Focus Color", &sel->style.focusColor.x);
    ImGui::TextDisabled("(-1 = use theme default)");

    // Nine-slice (collapsible — rarely used)
    if (sel->type == GUI::UIWidgetType::Panel || sel->type == GUI::UIWidgetType::Button) {
        if (ImGui::TreeNode("Nine-Slice")) {
            char pathBuf[256];
            strncpy(pathBuf, sel->style.nineSlice.texturePath.c_str(), sizeof(pathBuf) - 1);
            pathBuf[sizeof(pathBuf) - 1] = '\0';
            if (ImGui::InputText("Texture Path", pathBuf, sizeof(pathBuf)))
                sel->style.nineSlice.texturePath = pathBuf;

            ImGui::DragFloat("Border Left", &sel->style.nineSlice.borderLeft, 0.5f, 0.0f, 128.0f);
            ImGui::DragFloat("Border Right", &sel->style.nineSlice.borderRight, 0.5f, 0.0f, 128.0f);
            ImGui::DragFloat("Border Top", &sel->style.nineSlice.borderTop, 0.5f, 0.0f, 128.0f);
            ImGui::DragFloat("Border Bottom", &sel->style.nineSlice.borderBottom, 0.5f, 0.0f, 128.0f);

            // Texture preview with border guide lines
            if (!sel->style.nineSlice.texturePath.empty()) {
                VkDescriptorSet texDS = GetImGuiTexture(sel->style.nineSlice.texturePath);
                if (texDS != VK_NULL_HANDLE) {
                    f32 previewSize = 128.0f;
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texDS)), ImVec2(previewSize, previewSize));

                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImU32 guideColor = IM_COL32(255, 255, 0, 200);

                    auto tex = m_RenderSystem->LoadTexture(sel->style.nineSlice.texturePath);
                    if (tex && tex->IsValid()) {
                        f32 tw = static_cast<f32>(tex->GetWidth());
                        f32 th = static_cast<f32>(tex->GetHeight());
                        f32 scaleX = previewSize / tw;
                        f32 scaleY = previewSize / th;

                        f32 left = pos.x + sel->style.nineSlice.borderLeft * scaleX;
                        f32 right = pos.x + previewSize - sel->style.nineSlice.borderRight * scaleX;
                        f32 top = pos.y + sel->style.nineSlice.borderTop * scaleY;
                        f32 bottom = pos.y + previewSize - sel->style.nineSlice.borderBottom * scaleY;

                        dl->AddLine(ImVec2(left, pos.y), ImVec2(left, pos.y + previewSize), guideColor, 1.0f);
                        dl->AddLine(ImVec2(right, pos.y), ImVec2(right, pos.y + previewSize), guideColor, 1.0f);
                        dl->AddLine(ImVec2(pos.x, top), ImVec2(pos.x + previewSize, top), guideColor, 1.0f);
                        dl->AddLine(ImVec2(pos.x, bottom), ImVec2(pos.x + previewSize, bottom), guideColor, 1.0f);
                    }
                }
            }

            ImGui::TextDisabled("Leave texture empty for flat color");
            ImGui::TreePop();
        }
    }

    ImGui::Spacing();

    // --- Events (shown directly) ---
    ImGui::TextDisabled("Events");
    char clickBuf[128];
    strncpy(clickBuf, sel->onClickEvent.c_str(), sizeof(clickBuf) - 1); clickBuf[sizeof(clickBuf) - 1] = '\0';
    if (ImGui::InputText("On Click", clickBuf, sizeof(clickBuf))) sel->onClickEvent = clickBuf;

    char valBuf[128];
    strncpy(valBuf, sel->onValueChangedEvent.c_str(), sizeof(valBuf) - 1); valBuf[sizeof(valBuf) - 1] = '\0';
    if (ImGui::InputText("On Value Changed", valBuf, sizeof(valBuf))) sel->onValueChangedEvent = valBuf;

    char subBuf[128];
    strncpy(subBuf, sel->onSubmitEvent.c_str(), sizeof(subBuf) - 1); subBuf[sizeof(subBuf) - 1] = '\0';
    if (ImGui::InputText("On Submit", subBuf, sizeof(subBuf))) sel->onSubmitEvent = subBuf;

    ImGui::Spacing();

    // --- Accessibility ---
    ImGui::TextDisabled("Accessibility");
    char accLabelBuf[256];
    strncpy(accLabelBuf, sel->accessibleLabel.c_str(), sizeof(accLabelBuf) - 1); accLabelBuf[sizeof(accLabelBuf) - 1] = '\0';
    if (ImGui::InputText("Accessible Label", accLabelBuf, sizeof(accLabelBuf))) sel->accessibleLabel = accLabelBuf;
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Screen reader label. Falls back to element name if empty.");

    // Advanced Anchor (collapsible for power users)
    if (ImGui::TreeNode("Advanced Anchor")) {
        ImGui::DragFloat2("Anchor Min", &sel->anchor.anchorMin.x, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat2("Anchor Max", &sel->anchor.anchorMax.x, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat2("Pivot", &sel->anchor.pivot.x, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Offset Left", &sel->anchor.offsetLeft, 1.0f);
        ImGui::DragFloat("Offset Right", &sel->anchor.offsetRight, 1.0f);
        ImGui::DragFloat("Offset Top", &sel->anchor.offsetTop, 1.0f);
        ImGui::DragFloat("Offset Bottom", &sel->anchor.offsetBottom, 1.0f);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    // --- Duplicate + Delete ---
    if (ImGui::Button("Duplicate")) {
        u32 newId = canvas->DuplicateElement(m_UIEditSelectedElementId);
        if (newId) m_UIEditSelectedElementId = newId;
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Delete Element")) {
        canvas->RemoveElement(m_UIEditSelectedElementId);
        m_UIEditSelectedElementId = 0;
    }
    ImGui::PopStyleColor();
}

// ============================================================================
// UI EDITOR — Viewport WYSIWYG Editing
// ============================================================================

void EditorLayer::UIEditorScreenToDesign(f32 screenX, f32 screenY, f32& designX, f32& designY) {
    f32 gvW = m_GameViewImageMaxX - m_GameViewImageMinX;
    f32 gvH = m_GameViewImageMaxY - m_GameViewImageMinY;
    f32 localX = screenX - m_GameViewImageMinX;
    f32 localY = screenY - m_GameViewImageMinY;

    auto* canvas = m_World->GetComponent<GUI::UICanvasComponent>(m_UIEditCanvasEntity);
    if (!canvas || gvW <= 0 || gvH <= 0) {
        designX = localX;
        designY = localY;
        return;
    }

    f32 scaleFactor = std::min(gvW / canvas->designWidth, gvH / canvas->designHeight);
    if (canvas->scaleMode == GUI::UIScaleMode::ConstantPixelSize) scaleFactor = 1.0f;

    designX = localX / scaleFactor;
    designY = localY / scaleFactor;
}

void EditorLayer::UIEditorDesignToScreen(f32 designX, f32 designY, f32& screenX, f32& screenY) {
    f32 gvW = m_GameViewImageMaxX - m_GameViewImageMinX;
    f32 gvH = m_GameViewImageMaxY - m_GameViewImageMinY;

    auto* canvas = m_World->GetComponent<GUI::UICanvasComponent>(m_UIEditCanvasEntity);
    if (!canvas || gvW <= 0 || gvH <= 0) {
        screenX = designX + m_GameViewImageMinX;
        screenY = designY + m_GameViewImageMinY;
        return;
    }

    f32 scaleFactor = std::min(gvW / canvas->designWidth, gvH / canvas->designHeight);
    if (canvas->scaleMode == GUI::UIScaleMode::ConstantPixelSize) scaleFactor = 1.0f;

    screenX = designX * scaleFactor + m_GameViewImageMinX;
    screenY = designY * scaleFactor + m_GameViewImageMinY;
}

UIEditDragMode EditorLayer::UIEditorHitTestHandles(f32 localX, f32 localY, const GUI::UIRect& rect) {
    constexpr f32 handleSize = 5.0f;

    f32 left   = rect.x;
    f32 right  = rect.x + rect.w;
    f32 top    = rect.y;
    f32 bottom = rect.y + rect.h;
    f32 midX   = rect.x + rect.w * 0.5f;
    f32 midY   = rect.y + rect.h * 0.5f;

    auto hitHandle = [&](f32 hx, f32 hy) {
        return (localX >= hx - handleSize && localX <= hx + handleSize &&
                localY >= hy - handleSize && localY <= hy + handleSize);
    };

    // Corners first (higher priority)
    if (hitHandle(left, top))     return UIEditDragMode::ResizeTL;
    if (hitHandle(right, top))    return UIEditDragMode::ResizeTR;
    if (hitHandle(left, bottom))  return UIEditDragMode::ResizeBL;
    if (hitHandle(right, bottom)) return UIEditDragMode::ResizeBR;

    // Edges
    if (hitHandle(midX, top))     return UIEditDragMode::ResizeTop;
    if (hitHandle(midX, bottom))  return UIEditDragMode::ResizeBottom;
    if (hitHandle(left, midY))    return UIEditDragMode::ResizeLeft;
    if (hitHandle(right, midY))   return UIEditDragMode::ResizeRight;

    return UIEditDragMode::None;
}


void EditorLayer::DrawBehaviorTreeComponent(ECS::Entity entity) {
    auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(entity);
    if (!bt) return;

    if (!UI::SectionHeader("Behavior Tree", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Checkbox("Enabled##bt", &bt->enabled);

    ImGui::DragFloat("Tick Interval##bt", &bt->tickInterval, 0.01f, 0.0f, 10.0f, "%.2f s");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("0 = every frame");

    ImGui::Checkbox("Debug##bt", &bt->debugEnabled);

    // Blackboard summary
    if (!bt->blackboardDefaults.empty()) {
        ImGui::Text("Blackboard: %zu variables", bt->blackboardDefaults.size());
    }

    // Node count
    ImGui::Text("Nodes: %zu", bt->graph.GetNodes().size());

    // Open editor button
    if (ImGui::Button("Open Editor##bt")) {
        SetPanelVisibility(EditorPanel::BehaviorTree, true);
        m_BehaviorTreeEditor.SetTarget(m_World, entity);
    }
}

// ============================================================================
// Quest Flow Panel
// ============================================================================


void EditorLayer::DrawQuestFlowComponent(ECS::Entity entity) {
    auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(entity);
    if (!qf) return;

    if (!UI::SectionHeader("Quest Flow", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Checkbox("Enabled##qf", &qf->enabled);

    char questIdBuf[128];
    strncpy(questIdBuf, qf->questId.c_str(), sizeof(questIdBuf) - 1);
    questIdBuf[sizeof(questIdBuf) - 1] = '\0';
    if (ImGui::InputText("Quest ID##qfinsp", questIdBuf, sizeof(questIdBuf))) {
        qf->questId = questIdBuf;
    }

    char titleBuf[128];
    strncpy(titleBuf, qf->questTitle.c_str(), sizeof(titleBuf) - 1);
    titleBuf[sizeof(titleBuf) - 1] = '\0';
    if (ImGui::InputText("Title##qfinsp", titleBuf, sizeof(titleBuf))) {
        qf->questTitle = titleBuf;
    }

    ImGui::Text("Status: %s", Gameplay::QuestFlowStatusToString(qf->status));
    ImGui::Text("Nodes: %zu", qf->graph.GetNodes().size());

    // Open editor button
    if (ImGui::Button("Open Editor##qf")) {
        SetPanelVisibility(EditorPanel::QuestFlow, true);
        m_QuestFlowEditor.SetTarget(m_World, entity);
    }
}


void EditorLayer::DrawNetworkIdentityComponent(ECS::Entity entity) {
    auto* net = m_World->GetComponent<ECS::NetworkIdentityComponent>(entity);
    if (!net) return;
    DrawComponentHelp("networkIdentity", m_World, entity);

    bool open = UI::SectionHeader("[N] Network Identity", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("NetworkIdentityContext");
    }
    if (ImGui::BeginPopup("NetworkIdentityContext")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::NetworkIdentityComponent>(entity, "networkIdentity", "Network Identity");
        }
        ImGui::EndPopup();
    }
    if (!open) return;

    ImGui::Text("Network ID: %u", net->networkId);
    ImGui::Text("Owner ID: %u", net->ownerId);
    ImGui::Text("Locally Owned: %s", net->isLocallyOwned ? "Yes" : "No");

    ImGui::Checkbox("Sync Transform", &net->syncTransform);
    ImGui::DragFloat("Sync Interval", &net->syncInterval, 0.01f, 0.01f, 1.0f, "%.3f s");
}

void EditorLayer::DrawNetworkTransformComponent(ECS::Entity entity) {
    auto* nt = m_World->GetComponent<ECS::NetworkTransformComponent>(entity);
    if (!nt) return;
    DrawComponentHelp("networkTransform", m_World, entity);

    bool open = UI::SectionHeader("Network Transform", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("NetworkTransformContext");
    }
    if (ImGui::BeginPopup("NetworkTransformContext")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::NetworkTransformComponent>(entity, "networkTransform", "Network Transform");
        }
        ImGui::EndPopup();
    }
    if (!open) return;

    ImGui::Text("Last Synced Pos: %.2f, %.2f, %.2f",
                nt->lastSyncedPosition.x, nt->lastSyncedPosition.y, nt->lastSyncedPosition.z);
    ImGui::Text("Network Velocity: %.2f, %.2f, %.2f",
                nt->networkVelocity.x, nt->networkVelocity.y, nt->networkVelocity.z);
    ImGui::DragFloat("Interp Duration", &nt->interpDuration, 0.01f, 0.01f, 1.0f, "%.3f s");
}

// ============================================================================
// ELEMENTAL COMPONENTS
// ============================================================================

void EditorLayer::DrawElementalSurfaceComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Elemental Surface", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ElementalSurfaceCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ElementalSurfaceComponent>(entity, "elementalSurface", "Elemental Surface");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* surface = m_World->GetComponent<ECS::ElementalSurfaceComponent>(entity);
        if (!surface) return;
        DrawComponentHelp("elementalSurface", m_World, entity);

        InspectorUndo::SliderFloat(m_UndoRedo, "Flammability", &surface->flammability, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Accumulation Rate", &surface->accumulationRate, 0.05f, 0.0f, 5.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Decay Rate", &surface->decayRate, 0.01f, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Max Accumulation", &surface->maxAccumulation, 0.1f, 0.1f, 5.0f);

        ImGui::Separator();
        ImGui::Text("Current State (read-only):");
        ImGui::Text("  Char: %.2f  Wet: %.2f", surface->charAmount, surface->wetness);
        ImGui::Text("  Snow: %.2f  Frost: %.2f", surface->snowCoverage, surface->frostAmount);

        // Accumulation preview bar
        f32 accum[4] = { surface->accumulation.x, surface->accumulation.y,
                         surface->accumulation.z, surface->accumulation.w };
        ImGui::Text("Accumulation [F/W/E/A]:");
        ImGui::ProgressBar(accum[0] / surface->maxAccumulation, ImVec2(-1, 0), "Fire");
        ImGui::ProgressBar(accum[1] / surface->maxAccumulation, ImVec2(-1, 0), "Water");
        ImGui::ProgressBar(accum[2] / surface->maxAccumulation, ImVec2(-1, 0), "Earth");
        ImGui::ProgressBar(accum[3] / surface->maxAccumulation, ImVec2(-1, 0), "Air");

        if (ImGui::Button("Reset Accumulation##ElemSurf")) {
            surface->accumulation = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            surface->charAmount = 0.0f;
            surface->wetness = 0.0f;
            surface->snowCoverage = 0.0f;
            surface->frostAmount = 0.0f;
        }
    }
}

void EditorLayer::DrawElementalEmitterComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Elemental Emitter", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ElementalEmitterCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ElementalEmitterComponent>(entity, "elementalEmitter", "Elemental Emitter");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* emitter = m_World->GetComponent<ECS::ElementalEmitterComponent>(entity);
        if (!emitter) return;
        DrawComponentHelp("elementalEmitter", m_World, entity);

        InspectorUndo::Checkbox(m_UndoRedo, "Active##ElemEmit", &emitter->active);

        // Element type preset dropdown
        const char* presets[] = { "Fire", "Water", "Earth", "Air", "Steam", "Snow", "Custom" };
        static int currentPreset = 6; // default to Custom
        if (ImGui::Combo("Element Preset##ElemEmit", &currentPreset, presets, IM_ARRAYSIZE(presets))) {
            switch (currentPreset) {
                case 0: emitter->element = Math::Vector4(1.0f, 0.0f, 0.0f, 0.0f); break; // Fire
                case 1: emitter->element = Math::Vector4(0.0f, 1.0f, 0.0f, 0.0f); break; // Water
                case 2: emitter->element = Math::Vector4(0.0f, 0.0f, 1.0f, 0.0f); break; // Earth
                case 3: emitter->element = Math::Vector4(0.0f, 0.0f, 0.0f, 1.0f); break; // Air
                case 4: emitter->element = Math::Vector4(0.5f, 0.5f, 0.0f, 0.0f); break; // Steam
                case 5: emitter->element = Math::Vector4(0.0f, 0.4f, 0.0f, 0.6f); break; // Snow
                default: break; // Custom: don't change
            }
        }

        // Element signature (advanced)
        if (ImGui::TreeNode("Element Signature##ElemEmit")) {
            f32 elem[4] = { emitter->element.x, emitter->element.y, emitter->element.z, emitter->element.w };
            ImGui::DragFloat4("FWEA##ElemSig", elem, 0.05f, -1.0f, 1.0f);
            emitter->element = Math::Vector4(elem[0], elem[1], elem[2], elem[3]);
            ImGui::TreePop();
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Emission Rate##ElemEmit", &emitter->emissionRate, 0.5f, 0.0f, 100.0f);
        InspectorUndo::SliderFloat(m_UndoRedo, "Intensity##ElemEmit", &emitter->intensity, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Lifetime##ElemEmit", &emitter->lifetime, 0.1f, 0.1f, 30.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Spread##ElemEmit", &emitter->spread, 0.05f, 0.0f, 3.14f);
        InspectorUndo::DragFloat(m_UndoRedo, "Speed##ElemEmit", &emitter->speed, 0.1f, 0.0f, 50.0f);

        f32 dir[3] = { emitter->direction.x, emitter->direction.y, emitter->direction.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Direction##ElemEmit", dir,
                [emitter](f32 x, f32 y, f32 z) { emitter->direction = Math::Vector3(x, y, z); },
                0.05f, -1.0f, 1.0f)) {
            emitter->direction = Math::Vector3(dir[0], dir[1], dir[2]);
        }
    }
}

void EditorLayer::DrawElementalVolumeComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Elemental Volume", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ElementalVolumeCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ElementalVolumeComponent>(entity, "elementalVolume", "Elemental Volume");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* vol = m_World->GetComponent<ECS::ElementalVolumeComponent>(entity);
        if (!vol) return;
        DrawComponentHelp("elementalVolume", m_World, entity);

        f32 extents[3] = { vol->halfExtents.x, vol->halfExtents.y, vol->halfExtents.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Half Extents##ElemVol", extents,
                [vol](f32 x, f32 y, f32 z) { vol->halfExtents = Math::Vector3(x, y, z); },
                0.5f, 0.1f, 500.0f)) {
            vol->halfExtents = Math::Vector3(extents[0], extents[1], extents[2]);
        }

        if (ImGui::TreeNode("Element Bias##ElemVol")) {
            f32 bias[4] = { vol->elementBias.x, vol->elementBias.y, vol->elementBias.z, vol->elementBias.w };
            ImGui::DragFloat4("FWEA##ElemBias", bias, 0.05f, -1.0f, 1.0f);
            vol->elementBias = Math::Vector4(bias[0], bias[1], bias[2], bias[3]);
            ImGui::TreePop();
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Temperature Bias##ElemVol", &vol->temperatureBias, 0.5f, -50.0f, 50.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Wind Multiplier##ElemVol", &vol->windMultiplier, 0.1f, 0.0f, 5.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Kill On Contact##ElemVol", &vol->killOnContact);
    }
}

// ============================================================================
// DYNAMIC DIFFICULTY
// ============================================================================

void EditorLayer::DrawDynamicDifficultyComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Dynamic Difficulty", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("DynDiffCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::DynamicDifficultyComponent>(entity, "dynamicDifficulty", "Dynamic Difficulty");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* dd = m_World->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (!dd) return;
        DrawComponentHelp("dynamicDifficulty", m_World, entity);

        // --- Mode ---
        if (ImGui::TreeNodeEx("Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
            InspectorUndo::Checkbox(m_UndoRedo, "Enabled##DD", &dd->enabled);
            InspectorUndo::Checkbox(m_UndoRedo, "Visible to Player##DD", &dd->visibleToPlayer);

            const char* difficulties[] = { "Easy", "Normal", "Hard", "Nightmare" };
            int baseDiff = static_cast<int>(dd->baseDifficulty);
            if (InspectorUndo::Combo(m_UndoRedo, "Base Difficulty", &baseDiff, difficulties, 4)) {
                dd->baseDifficulty = static_cast<u32>(baseDiff);
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Adjustment Range##DD", &dd->adjustmentRange, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Smoothing Rate##DD", &dd->smoothingRate, 0.005f, 0.001f, 1.0f);
            ImGui::TreePop();
        }

        // --- Input Metrics ---
        if (ImGui::TreeNodeEx("Input Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Deaths
            InspectorUndo::Checkbox(m_UndoRedo, "Track Deaths##DD", &dd->trackDeaths);
            if (dd->trackDeaths) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Death Weight##DD", &dd->deathWeight, 0.05f, 0.0f, 5.0f);
                i32 deathWindow = static_cast<i32>(dd->deathWindow);
                if (InspectorUndo::DragInt(m_UndoRedo, "Death Window (sec)##DD", &deathWindow, 1, 10, 3600)) {
                    dd->deathWindow = static_cast<u32>(deathWindow);
                }
                ImGui::Text("Recent Deaths: %u", dd->recentDeaths);
                ImGui::Unindent();
            }

            // Health
            InspectorUndo::Checkbox(m_UndoRedo, "Track Health##DD", &dd->trackHealth);
            if (dd->trackHealth) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Health Weight##DD", &dd->healthWeight, 0.05f, 0.0f, 5.0f);
                ImGui::Text("Player Entity: %llu", static_cast<unsigned long long>(dd->playerEntity));
                ImGui::Unindent();
            }

            // Accuracy
            InspectorUndo::Checkbox(m_UndoRedo, "Track Accuracy##DD", &dd->trackAccuracy);
            if (dd->trackAccuracy) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Accuracy Weight##DD", &dd->accuracyWeight, 0.05f, 0.0f, 5.0f);
                ImGui::Text("Shots: %u / %u", dd->shotsHit, dd->shotsFired);
                ImGui::Unindent();
            }

            // Time
            InspectorUndo::Checkbox(m_UndoRedo, "Track Time##DD", &dd->trackTime);
            if (dd->trackTime) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Time Weight##DD", &dd->timeWeight, 0.05f, 0.0f, 5.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Expected Time (sec)##DD", &dd->expectedCompletionTime, 1.0f, 1.0f, 7200.0f);
                ImGui::Text("Elapsed: %.1f s", dd->elapsedTime);
                ImGui::Unindent();
            }

            // Resources
            InspectorUndo::Checkbox(m_UndoRedo, "Track Resources##DD", &dd->trackResources);
            if (dd->trackResources) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Resource Weight##DD", &dd->resourceWeight, 0.05f, 0.0f, 5.0f);
                ImGui::Text("Resource Ratio: %.2f", dd->resourceRatio);
                ImGui::Unindent();
            }

            // Checkpoint Health
            InspectorUndo::Checkbox(m_UndoRedo, "Track Checkpoint Health##DD", &dd->trackCheckpointHealth);
            if (dd->trackCheckpointHealth) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Checkpoint Weight##DD", &dd->checkpointHealthWeight, 0.05f, 0.0f, 5.0f);
                ImGui::Text("Last Checkpoint HP: %.0f%%", dd->lastCheckpointHealthPercent * 100.0f);
                ImGui::Unindent();
            }

            ImGui::TreePop();
        }

        // --- Output Adjustments ---
        if (ImGui::TreeNodeEx("Output Adjustments", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Enemy Damage
            InspectorUndo::Checkbox(m_UndoRedo, "Adjust Enemy Damage##DD", &dd->adjustEnemyDamage);
            if (dd->adjustEnemyDamage) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Damage Range##DD", &dd->enemyDamageRange, 0.01f, 0.0f, 1.0f);
                ImGui::Text("Multiplier: %.3f", dd->enemyDamageMultiplier);
                ImGui::Unindent();
            }

            // Enemy Health
            InspectorUndo::Checkbox(m_UndoRedo, "Adjust Enemy Health##DD", &dd->adjustEnemyHealth);
            if (dd->adjustEnemyHealth) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Health Range##DD", &dd->enemyHealthRange, 0.01f, 0.0f, 1.0f);
                ImGui::Text("Multiplier: %.3f", dd->enemyHealthMultiplier);
                ImGui::Unindent();
            }

            // AI Aggression
            InspectorUndo::Checkbox(m_UndoRedo, "Adjust AI Aggression##DD", &dd->adjustAIAggression);
            if (dd->adjustAIAggression) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Aggression Range##DD", &dd->aiAggressionRange, 0.01f, 0.0f, 1.0f);
                ImGui::Text("Multiplier: %.3f", dd->aiAggressionMultiplier);
                ImGui::Unindent();
            }

            // Resource Drops
            InspectorUndo::Checkbox(m_UndoRedo, "Adjust Resource Drops##DD", &dd->adjustResourceDrops);
            if (dd->adjustResourceDrops) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Drop Range##DD", &dd->resourceDropRange, 0.01f, 0.0f, 2.0f);
                ImGui::Text("Multiplier: %.3f", dd->resourceDropMultiplier);
                ImGui::Unindent();
            }

            // Hint Frequency
            InspectorUndo::Checkbox(m_UndoRedo, "Adjust Hint Frequency##DD", &dd->adjustHintFrequency);
            if (dd->adjustHintFrequency) {
                ImGui::Indent();
                i32 deathsBeforeHint = static_cast<i32>(dd->deathsBeforeHint);
                if (InspectorUndo::DragInt(m_UndoRedo, "Deaths Before Hint##DD", &deathsBeforeHint, 1, 1, 20)) {
                    dd->deathsBeforeHint = static_cast<u32>(deathsBeforeHint);
                }
                ImGui::Text("Hint Cooldown: %.1f s", dd->hintCooldown);
                ImGui::Text("Hints Shown: %u", dd->hintsBeforeSection);
                ImGui::Unindent();
            }

            // Checkpoint Frequency
            InspectorUndo::Checkbox(m_UndoRedo, "Adjust Checkpoint Frequency##DD", &dd->adjustCheckpointFrequency);
            if (dd->adjustCheckpointFrequency) {
                ImGui::Indent();
                InspectorUndo::DragFloat(m_UndoRedo, "Checkpoint Range##DD", &dd->checkpointRange, 0.01f, 0.0f, 1.0f);
                ImGui::Text("Multiplier: %.3f", dd->checkpointMultiplier);
                ImGui::Unindent();
            }

            ImGui::TreePop();
        }

        // --- Live Readout ---
        if (ImGui::TreeNodeEx("Live Readout##DD", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Raw Score: %.3f", dd->difficultyScore);
            ImGui::Text("Smoothed Score: %.3f", dd->smoothedScore);

            // Visual difficulty bar
            ImVec4 barColor;
            if (dd->smoothedScore < 0.3f) barColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);       // Green = easy
            else if (dd->smoothedScore < 0.6f) barColor = ImVec4(0.9f, 0.8f, 0.1f, 1.0f);   // Yellow = moderate
            else barColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);                                   // Red = hard

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
            ImGui::ProgressBar(dd->smoothedScore, ImVec2(-1, 0), "");
            ImGui::PopStyleColor();

            ImGui::TreePop();
        }
    }
}

// ============================================================================
// ART STYLE
// ============================================================================

void EditorLayer::DrawArtStyleComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Art Style", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ArtStyleCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ArtStyleComponent>(entity, "artStyle", "Art Style");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* as = m_World->GetComponent<ECS::ArtStyleComponent>(entity);
        if (!as) return;
        DrawComponentHelp("artStyle", m_World, entity);

        // Style selector
        const char* styleNames[] = {
            "Inherit (Scene Default)", "Pre-PBR", "Hand-Painted", "Cel / Toon",
            "NPR Illustrative", "Retro 3D", "Pixel Art", "Material Expression",
            "Analog / Degraded"
        };
        int styleIdx = static_cast<int>(as->style);
        if (InspectorUndo::Combo(m_UndoRedo, "Style", &styleIdx, styleNames, 9)) {
            as->style = static_cast<ECS::ArtStyleType>(styleIdx);
        }
        ImGui::SetItemTooltip(
            "Override the scene-level art style for this entity.\n"
            "'Inherit' uses whatever the scene is set to.");

        InspectorUndo::Checkbox(m_UndoRedo, "Propagate to Children##AS", &as->propagateToChildren);
        ImGui::SetItemTooltip("Apply this style to all child entities in the hierarchy.");

        ImGui::Separator();

        // Show only the parameters for the active style
        switch (as->style) {
        case ECS::ArtStyleType::PrePBR:
            if (ImGui::TreeNodeEx("Pre-PBR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                InspectorUndo::Checkbox(m_UndoRedo, "Half-Lambert##AS", &as->prePBR_halfLambert);
                ImGui::SetItemTooltip("Soft light wrapping (NdotL * 0.5 + 0.5).\nGives a less harsh terminator line.");
                InspectorUndo::Checkbox(m_UndoRedo, "Flat Shading##AS", &as->prePBR_flatShading);
                InspectorUndo::Checkbox(m_UndoRedo, "Gouraud Only##AS", &as->prePBR_gouraudOnly);
                ImGui::SetItemTooltip("Per-vertex lighting only (no per-pixel).\nClassic N64/PS2 look.");
                InspectorUndo::DragFloat(m_UndoRedo, "Specular Strength##AS", &as->prePBR_specularStrength, 0.01f, 0.0f, 2.0f);
                ImGui::TreePop();
            }
            break;

        case ECS::ArtStyleType::HandPainted:
            if (ImGui::TreeNodeEx("Hand-Painted Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                InspectorUndo::DragFloat(m_UndoRedo, "Light Wrap##AS", &as->handPainted_lightWrapAmount, 0.01f, 0.0f, 1.0f);
                ImGui::SetItemTooltip("How much light wraps around the surface.\n0 = standard Lambert, 1 = full wrap.");
                const char* rampModes[] = { "Off", "Smooth", "Warm", "Cool", "Anime" };
                int ramp = static_cast<int>(as->handPainted_lightRampMode);
                if (InspectorUndo::Combo(m_UndoRedo, "Light Ramp##AS", &ramp, rampModes, 5)) {
                    as->handPainted_lightRampMode = static_cast<u8>(ramp);
                }
                InspectorUndo::DragFloat(m_UndoRedo, "Saturation Boost##AS", &as->handPainted_saturationBoost, 0.01f, -0.5f, 0.5f);
                ImGui::TreePop();
            }
            break;

        case ECS::ArtStyleType::CelToon:
            if (ImGui::TreeNodeEx("Cel/Toon Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                InspectorUndo::DragFloat(m_UndoRedo, "Diffuse Bands##AS", &as->cel_diffuseBands, 0.1f, 2.0f, 8.0f);
                ImGui::SetItemTooltip("Number of discrete shading bands (2=stark, 8=smooth banding).");
                InspectorUndo::DragFloat(m_UndoRedo, "Specular Cutoff##AS", &as->cel_specularCutoff, 0.01f, 0.0f, 1.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Outline Width##AS", &as->cel_outlineWidth, 0.001f, 0.0f, 0.1f);
                {
                    f32 oc[3] = { as->cel_outlineColor.x, as->cel_outlineColor.y, as->cel_outlineColor.z };
                    if (InspectorUndo::ColorEdit3(m_UndoRedo, "Outline Color##AS", oc,
                            [as](f32 r, f32 g, f32 b) { as->cel_outlineColor = Math::Vector3(r, g, b); })) {
                        as->cel_outlineColor = Math::Vector3(oc[0], oc[1], oc[2]);
                    }
                }
                InspectorUndo::DragFloat(m_UndoRedo, "Rim Strength##AS", &as->cel_rimStrength, 0.01f, 0.0f, 3.0f);
                const char* shadowModes[] = { "Dark", "Purple", "Blue", "Warm", "Neutral Cool" };
                int shadow = static_cast<int>(as->cel_shadowMode);
                if (InspectorUndo::Combo(m_UndoRedo, "Shadow Mode##AS", &shadow, shadowModes, 5)) {
                    as->cel_shadowMode = static_cast<u8>(shadow);
                }
                const char* rampModes[] = { "Off", "Smooth", "Warm", "Cool", "Anime" };
                int ramp = static_cast<int>(as->cel_lightRampMode);
                if (InspectorUndo::Combo(m_UndoRedo, "Light Ramp##ASCel", &ramp, rampModes, 5)) {
                    as->cel_lightRampMode = static_cast<u8>(ramp);
                }
                ImGui::TreePop();
            }
            break;

        case ECS::ArtStyleType::NPR:
            if (ImGui::TreeNodeEx("NPR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                InspectorUndo::Checkbox(m_UndoRedo, "Cel Outline##AS", &as->npr_celOutline);
                if (as->npr_celOutline) {
                    ImGui::Indent();
                    InspectorUndo::DragFloat(m_UndoRedo, "Outline Thickness##AS", &as->npr_outlineThickness, 0.1f, 0.5f, 5.0f);
                    InspectorUndo::DragFloat(m_UndoRedo, "Curvature Weight##AS", &as->npr_curvatureWeight, 0.01f, 0.0f, 2.0f);
                    ImGui::SetItemTooltip("Curvature-driven outline thickness variation.\n0 = uniform, higher = ink pen feel.");
                    ImGui::Unindent();
                }
                i32 stipple = static_cast<i32>(as->npr_stipplePatternMask);
                if (InspectorUndo::DragInt(m_UndoRedo, "Stipple Patterns##AS", &stipple, 1, 0, 255)) {
                    as->npr_stipplePatternMask = static_cast<u32>(stipple);
                }
                InspectorUndo::DragFloat(m_UndoRedo, "Stipple Density##AS", &as->npr_stippleDensity, 0.01f, 0.0f, 1.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Stipple Strength##AS", &as->npr_stippleStrength, 0.01f, 0.0f, 1.0f);
                i32 bands = static_cast<i32>(as->npr_diffuseBands);
                if (InspectorUndo::DragInt(m_UndoRedo, "Diffuse Bands##ASNPR", &bands, 1, 2, 8)) {
                    as->npr_diffuseBands = static_cast<u8>(bands);
                }
                ImGui::TreePop();
            }
            break;

        case ECS::ArtStyleType::Retro:
            if (ImGui::TreeNodeEx("Retro 3D Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                InspectorUndo::Checkbox(m_UndoRedo, "Vertex Snapping##AS", &as->retro_vertexSnapping);
                if (as->retro_vertexSnapping) {
                    ImGui::Indent();
                    i32 res = static_cast<i32>(as->retro_snapResolution);
                    if (InspectorUndo::DragInt(m_UndoRedo, "Snap Resolution##AS", &res, 8, 80, 320)) {
                        as->retro_snapResolution = static_cast<u8>(res);
                    }
                    ImGui::SetItemTooltip("Virtual grid resolution for vertex snapping.\nLower = more jitter (PS1 look).");
                    ImGui::Unindent();
                }
                InspectorUndo::Checkbox(m_UndoRedo, "Affine Texturing##AS", &as->retro_affineTexturing);
                ImGui::SetItemTooltip("Non-perspective-correct texture mapping (PS1).");
                InspectorUndo::Checkbox(m_UndoRedo, "UV Quantize##AS", &as->retro_uvQuantize);
                InspectorUndo::Checkbox(m_UndoRedo, "Flat Shading##ASRetro", &as->retro_flatShading);
                InspectorUndo::DragFloat(m_UndoRedo, "Texture Page Size##AS", &as->retro_texturePageSize, 1.0f, 0.0f, 256.0f);
                ImGui::SetItemTooltip("PS1 VRAM page size in texels.\n0 = off, 64/128 typical.");
                InspectorUndo::DragFloat(m_UndoRedo, "Posterize Levels##AS", &as->retro_posterizeLevels, 1.0f, 4.0f, 256.0f);
                ImGui::TreePop();
            }
            break;

        case ECS::ArtStyleType::PixelArt:
            if (ImGui::TreeNodeEx("Pixel Art Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                i32 colors = static_cast<i32>(as->pixel_paletteColors);
                if (InspectorUndo::DragInt(m_UndoRedo, "Palette Colors##AS", &colors, 1, 2, 256)) {
                    as->pixel_paletteColors = static_cast<u32>(colors);
                }
                const char* paletteModes[] = { "Per-Channel", "PICO-8", "Game Boy", "NES", "CGA", "C64" };
                int pm = static_cast<int>(as->pixel_paletteMode);
                if (InspectorUndo::Combo(m_UndoRedo, "Palette##AS", &pm, paletteModes, 6)) {
                    as->pixel_paletteMode = static_cast<u32>(pm);
                }
                InspectorUndo::Checkbox(m_UndoRedo, "Point Filtering##AS", &as->pixel_pointFiltering);
                ImGui::SetItemTooltip("Nearest-neighbor texture sampling for crispy pixels.");
                i32 nq = static_cast<i32>(as->pixel_normalQuantizeSteps);
                if (InspectorUndo::DragInt(m_UndoRedo, "Normal Quantize##AS", &nq, 1, 0, 16)) {
                    as->pixel_normalQuantizeSteps = static_cast<u32>(nq);
                }
                ImGui::SetItemTooltip("0 = off, 4-16 = snap normals to N cardinal directions.\nGives a 2D-in-3D look.");
                ImGui::TreePop();
            }
            break;

        case ECS::ArtStyleType::MaterialExpression:
            if (ImGui::TreeNodeEx("Material Expression Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                InspectorUndo::DragFloat(m_UndoRedo, "Surface Noise Scale##AS", &as->matExpr_surfaceNoiseScale, 0.1f, 0.0f, 50.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Surface Noise Strength##AS", &as->matExpr_surfaceNoiseStrength, 0.01f, 0.0f, 1.0f);
                ImGui::SetItemTooltip("Procedural world-space noise on the diffuse color.");
                InspectorUndo::DragFloat(m_UndoRedo, "SSS Intensity##AS", &as->matExpr_sssIntensity, 0.01f, 0.0f, 3.0f);
                if (as->matExpr_sssIntensity > 0.0f) {
                    ImGui::Indent();
                    InspectorUndo::DragFloat(m_UndoRedo, "SSS Radius##AS", &as->matExpr_sssRadius, 0.1f, 0.1f, 10.0f);
                    {
                        f32 sc[3] = { as->matExpr_sssColor.x, as->matExpr_sssColor.y, as->matExpr_sssColor.z };
                        if (InspectorUndo::ColorEdit3(m_UndoRedo, "SSS Color##AS", sc,
                                [as](f32 r, f32 g, f32 b) { as->matExpr_sssColor = Math::Vector3(r, g, b); })) {
                            as->matExpr_sssColor = Math::Vector3(sc[0], sc[1], sc[2]);
                        }
                    }
                    ImGui::Unindent();
                }
                ImGui::TreePop();
            }
            break;

        case ECS::ArtStyleType::Analog:
            if (ImGui::TreeNodeEx("Analog Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                InspectorUndo::Checkbox(m_UndoRedo, "Film Grain##AS", &as->analog_filmGrain);
                if (as->analog_filmGrain) {
                    ImGui::Indent();
                    InspectorUndo::DragFloat(m_UndoRedo, "Grain Intensity##AS", &as->analog_filmGrainIntensity, 0.005f, 0.0f, 0.5f);
                    ImGui::Unindent();
                }
                InspectorUndo::Checkbox(m_UndoRedo, "Chromatic Aberration##AS", &as->analog_chromaticAberration);
                if (as->analog_chromaticAberration) {
                    ImGui::Indent();
                    InspectorUndo::DragFloat(m_UndoRedo, "Aberration Intensity##AS", &as->analog_chromaticIntensity, 0.001f, 0.0f, 0.05f);
                    ImGui::Unindent();
                }
                InspectorUndo::Checkbox(m_UndoRedo, "VHS Filter##AS", &as->analog_vhsEnabled);
                if (as->analog_vhsEnabled) {
                    ImGui::Indent();
                    InspectorUndo::DragFloat(m_UndoRedo, "Tracking##AS", &as->analog_vhsTrackingIntensity, 0.01f, 0.0f, 1.0f);
                    ImGui::Unindent();
                }
                InspectorUndo::Checkbox(m_UndoRedo, "CRT Scanlines##AS", &as->analog_crtEnabled);
                if (as->analog_crtEnabled) {
                    ImGui::Indent();
                    InspectorUndo::DragFloat(m_UndoRedo, "Scanline Intensity##AS", &as->analog_scanlineIntensity, 0.01f, 0.0f, 1.0f);
                    ImGui::Unindent();
                }
                InspectorUndo::Checkbox(m_UndoRedo, "Film Gate Weave##AS", &as->analog_filmGateWeave);
                if (as->analog_filmGateWeave) {
                    ImGui::Indent();
                    InspectorUndo::DragFloat(m_UndoRedo, "Weave Intensity##AS", &as->analog_gateWeaveIntensity, 0.001f, 0.0f, 0.02f);
                    ImGui::Unindent();
                }
                InspectorUndo::Checkbox(m_UndoRedo, "Light Leaks##AS", &as->analog_lightLeaks);
                if (as->analog_lightLeaks) {
                    ImGui::Indent();
                    InspectorUndo::DragFloat(m_UndoRedo, "Leak Intensity##AS", &as->analog_lightLeakIntensity, 0.01f, 0.0f, 1.0f);
                    ImGui::Unindent();
                }
                ImGui::TreePop();
            }
            break;

        case ECS::ArtStyleType::Inherit:
        default:
            ImGui::TextDisabled("This entity uses the scene-level art style.");
            ImGui::TextDisabled("Choose a style above to override per-entity.");
            break;
        }
    }
}

// ============================================================================
// BONE ATTACHMENT
// ============================================================================

void EditorLayer::DrawBoneAttachmentComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("Bone Attachment", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("BoneAttachmentCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::BoneAttachmentComponent>(entity, "boneAttachment", "Bone Attachment");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (!open) return;

    auto* ba = m_World->GetComponent<ECS::BoneAttachmentComponent>(entity);
    if (!ba) return;
    DrawComponentHelp("boneAttachment", m_World, entity);

    // Target entity picker
    u64 targetId = static_cast<u64>(ba->targetEntity);
    if (ImGui::InputScalar("Target Entity##BA", ImGuiDataType_U64, &targetId)) {
        ba->targetEntity = static_cast<ECS::Entity>(targetId);
    }
    // Show target entity name if valid
    if (ba->targetEntity != ECS::INVALID_ENTITY && m_World->IsValid(ba->targetEntity)) {
        auto* targetName = m_World->GetComponent<ECS::NameComponent>(ba->targetEntity);
        if (targetName) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", targetName->name.c_str());
        }
    }

    // Bone name dropdown: populate from target entity's skeleton
    const Animation::Skeleton* skeleton = nullptr;
    if (ba->targetEntity != ECS::INVALID_ENTITY && m_World->IsValid(ba->targetEntity)) {
        auto* animComp = m_World->GetComponent<ECS::AnimatorComponent>(ba->targetEntity);
        if (animComp) {
            skeleton = animComp->animator.GetSkeleton();
        }
    }

    if (skeleton && !skeleton->bones.empty()) {
        // Combo box listing all bone names
        if (ImGui::BeginCombo("Bone##BA", ba->targetBoneName.empty() ? "(none)" : ba->targetBoneName.c_str())) {
            for (usize i = 0; i < skeleton->bones.size(); ++i) {
                const auto& boneName = skeleton->bones[i].name;
                bool selected = (ba->targetBoneName == boneName);
                if (ImGui::Selectable(boneName.c_str(), selected)) {
                    ba->targetBoneName = boneName;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    } else {
        // Fallback text input if no skeleton available
        static char boneNameBuf[256];
        strncpy(boneNameBuf, ba->targetBoneName.c_str(), sizeof(boneNameBuf) - 1);
        boneNameBuf[sizeof(boneNameBuf) - 1] = '\0';
        if (ImGui::InputText("Bone##BA", boneNameBuf, sizeof(boneNameBuf))) {
            ba->targetBoneName = boneNameBuf;
        }
        if (!skeleton) {
            ImGui::TextDisabled("(target has no skeleton)");
        }
    }

    // Position offset
    ImGui::DragFloat3("Position Offset##BA", &ba->positionOffset.x, 0.01f);

    // Rotation offset (as Euler angles for readability)
    Math::Vector3 euler = ba->rotationOffset.ToEulerDegrees();
    if (ImGui::DragFloat3("Rotation Offset##BA", &euler.x, 0.5f)) {
        ba->rotationOffset = Math::Quaternion::FromEulerDegrees(euler);
    }
}

// ============================================================================
// NETWORK PANEL
// ============================================================================


// Explicit template instantiations for RemoveComponentWithUndo
// (needed by split files: EditorLayerComponents_Audio.cpp, EditorLayerComponents_Physics.cpp)
template void EditorLayer::RemoveComponentWithUndo<ECS::AudioSourceComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::AudioListenerComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::ReverbZoneComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::AmbientSoundLayerComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::MusicZoneComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::AudioSnapshotTriggerComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::AudioOcclusionComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::LipSyncComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::AudioReactiveComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::AudioThresholdTriggerComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::RTPCComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::BeatClockComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::BeatSyncComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::ConductorComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::AudioCollisionComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::SidechainComponent>(ECS::Entity, const std::string&, const std::string&);
// Physics split
template void EditorLayer::RemoveComponentWithUndo<ECS::RigidbodyComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::BoxColliderComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::SphereColliderComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::CapsuleColliderComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::MeshColliderComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::TriggerZoneComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::PerFrameColliderComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::PolygonCollider2DComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::RagdollComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::DistanceJointComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::HingeJointComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::BallSocketJointComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::SpringJointComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::FixedJointComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::SliderJointComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<Physics::Body2DComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<Physics::Joint2DComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::MIDIBindingComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::AudioFidelityComponent>(ECS::Entity, const std::string&, const std::string&);
template void EditorLayer::RemoveComponentWithUndo<ECS::MaterialInteractionTableComponent>(ECS::Entity, const std::string&, const std::string&);

} // namespace Editor
} // namespace Enjin

void Enjin::Editor::EditorLayer::DrawGaussianSplatComponent(ECS::Entity entity) {
    bool open = UI::SectionHeader("[G] Gaussian Splat", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("GaussianSplatCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::GaussianSplatComponent>(entity, "gaussianSplat", "Gaussian Splat");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (!open) return;
    auto* splat = m_World->GetComponent<ECS::GaussianSplatComponent>(entity);
    if (!splat) return;
    DrawComponentHelp("gaussianSplat", m_World, entity);

    if (!splat->sourcePath.empty()) {
        size_t sl = splat->sourcePath.find_last_of("/\\");
        std::string fn = (sl != std::string::npos) ? splat->sourcePath.substr(sl + 1) : splat->sourcePath;
        ImGui::Text("File: %s", fn.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X##SplatFile")) { splat->sourcePath.clear(); splat->dirty = true; }
    }
    if (ImGui::Button(splat->sourcePath.empty() ? "Choose Splat File..." : "Change...")) {
        std::string p = FileDialog::OpenFile("Gaussian Splat",
                                             {{ "Gaussian splats", "*.ply;*.spz" }});
        if (!p.empty()) { splat->sourcePath = p; splat->dirty = true; }
    }
    ImGui::SetItemTooltip("A 3D Gaussian splat capture: .ply (INRIA layout) or .spz (Niantic).\n"
                          "Free phone apps can capture these of real places and objects.");

    if (ImGui::SliderFloat("Opacity", &splat->opacityScale, 0.0f, 2.0f)) {}
    ImGui::SetItemTooltip("Fades the whole cloud");
    if (ImGui::SliderFloat("Splat Scale", &splat->splatScale, 0.1f, 4.0f)) {}
    ImGui::SetItemTooltip("Multiplies every gaussian's size - raise to close gaps, lower to sharpen");
    if (ImGui::Checkbox("Flip Y/Z (COLMAP -> engine up)", &splat->flipYZ)) splat->dirty = true;
    ImGui::SetItemTooltip("Most captures come out of COLMAP-style tools upside down without this");
    ImGui::Checkbox("Visible", &splat->visible);

    if (!splat->loadError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "Load failed: %s", splat->loadError.c_str());
    } else if (splat->loadedCount > 0) {
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "%u splats resident", splat->loadedCount);
    } else if (!splat->sourcePath.empty()) {
        ImGui::TextDisabled("Loading...");
    }
}
