#include <unordered_set>
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Viewmodel.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/DisplayGraphic.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/VirtualCamera.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/GaussianSplat.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/ECS/Components/ReflectionProbe.h"
#include "Enjin/ECS/Components/ReflectivePlane.h"
#include "Enjin/ECS/Components/Ladder.h"
#include "Enjin/ECS/Components/Rope.h"
#include "Enjin/ECS/Components/Door.h"
#include "Enjin/ECS/Components/Elemental.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/BoneAttachment.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/GPUParticleEmitter.h"
#include "Enjin/ECS/Components/Cloth.h"
#include "Enjin/ECS/Components/Swarm.h"
#include "Enjin/ECS/Components/DungeonGenerator.h"
#include "Enjin/ECS/Components/RandomBag.h"
#include "Enjin/ECS/Components/Scatter.h"
#include "Enjin/ECS/Components/TerrainGenerator.h"
#include "Enjin/ECS/Components/WFC.h"
#include "Enjin/ECS/Components/CustomShader.h"
#include "Enjin/ECS/Components/Lens.h"
#include "Enjin/ECS/Components/CineComponent.h"
#include "Enjin/ECS/Components/MorphTarget.h"
#include "Enjin/ECS/Components/MeshRenderer.h"
#include "Enjin/ECS/Components/DynamicDifficulty.h"
#include "Enjin/ECS/Components/ArtStyle.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Assets/MeshAssetCache.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/Accessibility/ContentWarning.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/ECS/Components/ParallaxMachine.h"
#include "Enjin/ECS/Components/ParallaxLayer.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

using json = nlohmann::json;

namespace Enjin {
namespace Scene {

// Depth-limited parse for untrusted scene/layer JSON (see SceneSerializer.h).
// The parse callback fires per structural event with the current depth;
// throwing aborts the parse before the recursive parser can overflow the
// stack, and lands in the std::exception handler every call site already has.
json ParseSceneJson(const std::string& text) {
    return json::parse(text, [](int depth, json::parse_event_t, json&) -> bool {
        if (depth > static_cast<int>(kMaxSceneJsonDepth)) {
            throw std::runtime_error(
                "JSON nesting depth exceeds limit (" +
                std::to_string(kMaxSceneJsonDepth) + ") — rejecting document");
        }
        return true;
    });
}

// JSON serialization helpers for math types
namespace {

// Round float to 6 decimal places for deterministic serialization
static f32 RF(f32 val) {
    if (std::isnan(val) || std::isinf(val)) return val;
    if (std::fabs(val) < 1e-6f) return 0.0f;
    constexpr f32 mult = 1000000.0f;
    return std::round(val * mult) / mult;
}

// Tolerant bool deserialization -- handles numbers (from RF() bug) and booleans
static bool JB(const json& val) {
    if (val.is_boolean()) return val.get<bool>();
    if (val.is_number()) return val.get<double>() != 0.0;
    return false;
}

// String length caps for deserialized fields (SER-M6)
static constexpr usize MAX_STR_PATH = 4096;
static constexpr usize MAX_STR_NAME = 1024;
static constexpr usize MAX_STR_TEXT = 65536;
static constexpr usize MAX_STR_LARGE = 1048576;  // 1 MB for notes, descriptions

// Length-capped string deserialization to prevent OOM from malicious scene files
static std::string SafeStr(const json& val, usize maxLen = MAX_STR_TEXT) {
    auto s = val.get<std::string>();
    if (s.size() > maxLen) {
        s.resize(maxLen);
    }
    return s;
}

json SerializeVector2(const Math::Vector2& v) {
    return json::array({RF(v.x), RF(v.y)});
}

json SerializeVector3(const Math::Vector3& v) {
    return json::array({RF(v.x), RF(v.y), RF(v.z)});
}

json SerializeQuaternion(const Math::Quaternion& q) {
    return json::array({RF(q.x), RF(q.y), RF(q.z), RF(q.w)});
}

Math::Vector2 DeserializeVector2(const json& j) {
    if (!j.is_array() || j.size() < 2) return Math::Vector2(0.0f, 0.0f);
    return Math::Vector2(j[0].get<f32>(), j[1].get<f32>());
}

Math::Vector3 DeserializeVector3(const json& j) {
    if (!j.is_array() || j.size() < 3) return Math::Vector3(0.0f, 0.0f, 0.0f);
    return Math::Vector3(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>());
}

json SerializeVector4(const Math::Vector4& v) {
    return json::array({RF(v.x), RF(v.y), RF(v.z), RF(v.w)});
}

Math::Vector4 DeserializeVector4(const json& j) {
    if (!j.is_array() || j.size() < 4) return Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    return Math::Vector4(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>(), j[3].get<f32>());
}

Math::Quaternion DeserializeQuaternion(const json& j) {
    if (!j.is_array() || j.size() < 4) return Math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    return Math::Quaternion(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>(), j[3].get<f32>());
}

json SerializeMatrix4(const Math::Matrix4& mat) {
    json arr = json::array();
    for (int i = 0; i < 16; ++i) arr.push_back(RF(mat.m[i]));
    return arr;
}

Math::Matrix4 DeserializeMatrix4(const json& j) {
    Math::Matrix4 mat;
    if (!j.is_array() || j.size() < 16) return mat;
    for (int i = 0; i < 16; ++i) mat.m[i] = j[i].get<f32>();
    return mat;
}

// Serialize components
json SerializeNameComponent(const ECS::NameComponent& name) {
    json j;
    j["name"] = name.name;
    return j;
}

json SerializeTransformComponent(const ECS::TransformComponent& transform) {
    json j;
    j["position"] = SerializeVector3(transform.position);
    j["rotation"] = SerializeQuaternion(transform.rotation);
    j["scale"] = SerializeVector3(transform.scale);
    if (!transform.visible) j["visible"] = false;
    return j;
}

json SerializeMaterialComponent(const ECS::MaterialComponent& material) {
    json j;
    j["baseColor"] = SerializeVector3(material.baseColor);
    j["opacity"] = RF(material.opacity);
    j["metallic"] = RF(material.metallic);
    j["roughness"] = RF(material.roughness);
    j["emissiveColor"] = SerializeVector3(material.emissiveColor);
    j["emissiveStrength"] = RF(material.emissiveStrength);
    j["baseColorTexture"] = material.baseColorTexture;
    j["normalTexture"] = material.normalTexture;
    j["metallicRoughnessTexture"] = material.metallicRoughnessTexture;
    j["emissiveTexture"] = material.emissiveTexture;
    // Texture paths
    j["baseColorTexturePath"] = material.baseColorTexturePath;
    j["uvRegionOffset"] = { RF(material.uvRegionOffset.x), RF(material.uvRegionOffset.y) };
    j["uvRegionScale"] = { RF(material.uvRegionScale.x), RF(material.uvRegionScale.y) };
    if (material.uvScrollSpeed.x != 0.0f || material.uvScrollSpeed.y != 0.0f) {
        j["uvScrollSpeed"] = { RF(material.uvScrollSpeed.x), RF(material.uvScrollSpeed.y) };
    }
    if (material.flipbookCols > 0 && material.flipbookRows > 0) {
        j["flipbookCols"] = material.flipbookCols;
        j["flipbookRows"] = material.flipbookRows;
        j["flipbookFps"] = RF(material.flipbookFps);
    }
    j["normalTexturePath"] = material.normalTexturePath;
    j["metallicRoughnessTexturePath"] = material.metallicRoughnessTexturePath;
    j["emissiveTexturePath"] = material.emissiveTexturePath;
    j["specularTexturePath"] = material.specularTexturePath;
    j["doubleSided"] = material.doubleSided;
    j["castShadows"] = material.castShadows;
    j["receiveShadows"] = material.receiveShadows;
    j["alphaMode"] = static_cast<i32>(material.alphaMode);
    j["alphaCutoff"] = RF(material.alphaCutoff);
    // Height/parallax mapping
    j["heightTexturePath"] = material.heightTexturePath;
    j["parallaxScale"] = RF(material.parallaxScale);
    j["parallaxMode"] = material.parallaxMode;
    j["pomMaxSteps"] = material.pomMaxSteps;
    j["pomHeightScale"] = RF(material.pomHeightScale);
    // Retro rendering flags
    j["flatShading"] = material.flatShading;
    j["affineTexturing"] = material.affineTexturing;
    j["vertexSnapping"] = material.vertexSnapping;
    j["stippleTransparency"] = material.stippleTransparency;
    j["uvQuantize"] = material.uvQuantize;
    j["gouraudOnly"] = material.gouraudOnly;
    j["sdfText"] = material.sdfText;
    j["vertexSnapResolution"] = material.vertexSnapResolution;
    j["shadowDitherMode"] = material.shadowDitherMode;
    j["shadowDitherPattern"] = material.shadowDitherPattern;
    j["textureFilterOverride"] = material.textureFilterOverride;
    if (!material.footstepSound.empty()) j["footstepSound"] = material.footstepSound;
    if (!material.impactSound.empty())   j["impactSound"] = material.impactSound;
    if (material.surfaceParticle != 0)   j["surfaceParticle"] = material.surfaceParticle;
    j["footstepVolume"] = material.footstepVolume;
    j["impactThreshold"] = material.impactThreshold;
    j["reflectivity"] = material.reflectivity;
    j["fresnelPower"] = material.fresnelPower;
    j["rimLightStrength"] = material.rimLightStrength;
    j["excludeFromCelShading"] = material.excludeFromCelShading;
    j["outlineWidth"] = material.outlineWidth;
    j["outlineColor"] = SerializeVector3(material.outlineColor);
    j["ditherGradient"] = material.ditherGradient;
    j["ditherGradientBands"] = material.ditherGradientBands;
    j["ditherGradientPattern"] = material.ditherGradientPattern;
    j["ditherTransparency"] = material.ditherTransparency;
    j["ditherTransPattern"] = material.ditherTransPattern;
    j["ditherTransBlendColor"] = { material.ditherTransBlendColor.x, material.ditherTransBlendColor.y, material.ditherTransBlendColor.z };
    j["ditherTransOpacity"] = material.ditherTransOpacity;
    // Transmission / SSS
    j["transmission"] = RF(material.transmission);
    j["ior"] = RF(material.ior);
    j["thickness"] = RF(material.thickness);
    j["sssIntensity"] = RF(material.sssIntensity);
    j["sssRadius"] = RF(material.sssRadius);
    j["sssColor"] = SerializeVector3(material.sssColor);
    // Matcap texture
    j["matcapTexturePath"] = material.matcapTexturePath;
    j["scrollReflectionTexturePath"] = material.scrollReflectionTexturePath;
    j["scrollReflectionSpeed"] = SerializeVector2(material.scrollReflectionSpeed);
    j["scrollReflectionStrength"] = RF(material.scrollReflectionStrength);
    // Procedural surface noise
    j["surfaceNoiseScale"] = RF(material.surfaceNoiseScale);
    j["surfaceNoiseStrength"] = RF(material.surfaceNoiseStrength);
    return j;
}

json SerializeMaterialSlotsComponent(const ECS::MaterialSlotsComponent& slots) {
    json j;
    json slotsArray = json::array();
    for (const auto& slot : slots.slots) {
        slotsArray.push_back(SerializeMaterialComponent(slot));
    }
    j["slots"] = slotsArray;
    return j;
}

// Forward declaration (defined later, needed by DeserializeMaterialSlotsComponent)
ECS::MaterialComponent DeserializeMaterialComponent(const json& j);

ECS::MaterialSlotsComponent DeserializeMaterialSlotsComponent(const json& j) {
    ECS::MaterialSlotsComponent comp;
    if (j.contains("slots") && j["slots"].is_array()) {
        static constexpr usize kMaxSlots = 64;  // Reasonable cap
        usize count = std::min(j["slots"].size(), kMaxSlots);
        comp.slots.reserve(count);
        for (usize i = 0; i < count; ++i) {
            comp.slots.push_back(DeserializeMaterialComponent(j["slots"][i]));
        }
    }
    return comp;
}

json SerializeMeshComponent(const ECS::MeshComponent& mesh, bool includeVertexData,
                            bool preferReference = false) {
    json j;
    j["vertexCount"] = static_cast<u32>(mesh.vertices.size());
    j["indexCount"] = static_cast<u32>(mesh.indices.size());

    // Reference mode: an imported mesh with a valid source ref serializes as the
    // reference only (written below), skipping the heavy inline vertex/index arrays;
    // the loader reloads/shares the geometry from the source file via MeshAssetCache.
    // CRITICAL SAFETY: only drop the inline vertices if the cache can actually reload
    // this mesh RIGHT NOW (source present + hash matches). If it can't, we keep the
    // inline geometry, so geometry is never lost even if a source file is missing or
    // has drifted — worst case the file just doesn't shrink for that mesh.
    const bool writeAsReference = preferReference && mesh.source.Valid()
        && Assets::MeshAssetCache::Get().CanResolve(mesh.source);

    if (includeVertexData && !writeAsReference) {
        json vertices = json::array();
        for (const auto& v : mesh.vertices) {
            json vertex;
            vertex["position"] = SerializeVector3(v.position);
            vertex["normal"] = SerializeVector3(v.normal);
            vertex["uv"] = SerializeVector2(v.uv);
            // Only serialize color if non-default (white)
            if (v.color.x != 1.0f || v.color.y != 1.0f || v.color.z != 1.0f || v.color.w != 1.0f) {
                vertex["color"] = SerializeVector4(v.color);
            }
            // Tangent (for normal mapping)
            if (v.tangent.x != 0.0f || v.tangent.y != 0.0f || v.tangent.z != 0.0f) {
                vertex["tangent"] = SerializeVector4(v.tangent);
            }
            // Bone data (for skeletal animation)
            if (v.boneWeights.x != 0.0f || v.boneWeights.y != 0.0f || v.boneWeights.z != 0.0f || v.boneWeights.w != 0.0f) {
                vertex["boneWeights"] = SerializeVector4(v.boneWeights);
                vertex["boneIndices"] = json::array({v.boneIndices[0], v.boneIndices[1], v.boneIndices[2], v.boneIndices[3]});
                // Influences 5-8: dropping these was the ~10% spiky-vert import
                // corruption — dense rigs normalize across 8 weights at import,
                // so a save that keeps only 4 leaves those verts under-weighted
                // on every subsequent load (2026-08-07).
                if (v.boneWeights2.x != 0.0f || v.boneWeights2.y != 0.0f ||
                    v.boneWeights2.z != 0.0f || v.boneWeights2.w != 0.0f) {
                    vertex["boneWeights2"] = SerializeVector4(v.boneWeights2);
                    vertex["boneIndices2"] = json::array({v.boneIndices2[0], v.boneIndices2[1], v.boneIndices2[2], v.boneIndices2[3]});
                }
            }
            vertices.push_back(vertex);
        }
        j["vertices"] = vertices;
        j["indices"] = mesh.indices;
    }

    // Serialize sub-meshes (multi-material support)
    if (!mesh.subMeshes.empty()) {
        json subMeshes = json::array();
        for (const auto& sm : mesh.subMeshes) {
            json smJson;
            smJson["indexOffset"] = sm.indexOffset;
            smJson["indexCount"] = sm.indexCount;
            smJson["materialSlot"] = sm.materialSlot;
            if (!sm.name.empty()) smJson["name"] = sm.name;
            subMeshes.push_back(smJson);
        }
        j["subMeshes"] = subMeshes;
    }

    // Source asset reference for imported meshes. Written additively — Phase A still
    // serializes inline vertices above, so older loaders ignore this and newer ones
    // can prefer the reference (load-once/share) and skip the inline geometry.
    if (mesh.source.Valid()) {
        json src;
        src["path"] = mesh.source.sourcePath;
        src["meshIndex"] = mesh.source.meshIndex;
        src["axisZToY"] = mesh.source.axisZToY;
        src["axisLToR"] = mesh.source.axisLToR;
        src["contentHash"] = mesh.source.contentHash;
        j["source"] = src;
    }

    return j;
}

json SerializeLightComponent(const ECS::LightComponent& light) {
    json j;
    j["type"] = static_cast<i32>(light.type);
    j["color"] = SerializeVector3(light.color);
    j["intensity"] = RF(light.intensity);
    j["range"] = RF(light.range);
    j["constantAttenuation"] = RF(light.constantAttenuation);
    j["linearAttenuation"] = RF(light.linearAttenuation);
    j["quadraticAttenuation"] = RF(light.quadraticAttenuation);
    j["innerConeAngle"] = RF(light.innerConeAngle);
    j["outerConeAngle"] = RF(light.outerConeAngle);
    j["castShadows"] = light.castShadows;
    return j;
}

json SerializeNotesComponent(const ECS::NotesComponent& notes) {
    json j;
    j["notes"] = notes.notes;
    return j;
}

json SerializeTextComponent(const ECS::TextComponent& text) {
    json j;
    j["text"] = text.text;
    j["fontPath"] = text.fontPath;
    j["fontSize"] = RF(text.fontSize);
    j["wrapWidth"] = RF(text.wrapWidth);
    j["textureWidth"] = text.textureWidth;
    j["textureHeight"] = text.textureHeight;
    j["textColor"] = SerializeVector3(text.textColor);
    j["bgColor"] = SerializeVector3(text.bgColor);
    j["bgOpacity"] = RF(text.bgOpacity);
    j["horizontalAlign"] = static_cast<i32>(text.horizontalAlign);
    j["paddingX"] = RF(text.paddingX);
    j["paddingY"] = RF(text.paddingY);
    j["sdfText"] = text.sdfText;
    j["worldHeight"] = RF(text.worldHeight);
    j["lit"] = text.lit;
    return j;
}

json SerializeCameraComponent(const ECS::CameraComponent& camera) {
    json j;
    j["projectionType"] = static_cast<i32>(camera.projectionType);
    j["fieldOfView"] = RF(camera.fieldOfView);
    j["nearPlane"] = RF(camera.nearPlane);
    j["farPlane"] = RF(camera.farPlane);
    j["orthoSize"] = RF(camera.orthoSize);
    j["priority"] = camera.priority;
    j["isActive"] = camera.isActive;
    j["clearDepth"] = camera.clearDepth;
    j["clearColor"] = camera.clearColor;
    j["backgroundColor"] = SerializeVector3(camera.backgroundColor);
    j["viewportX"] = RF(camera.viewportX);
    j["viewportY"] = RF(camera.viewportY);
    j["viewportWidth"] = RF(camera.viewportWidth);
    j["viewportHeight"] = RF(camera.viewportHeight);
    j["cullingMask"] = camera.cullingMask;
    // Default is now OFF (opt-in), so persist the flag when it is ON. Older scenes that
    // omitted this field (when the default was ON) will load with PP off; re-enable it on
    // the camera (the editor prompts when you turn on a PP effect).
    if (camera.enablePostProcessing) j["enablePostProcessing"] = true;
    return j;
}

// Deserialize components
ECS::NameComponent DeserializeNameComponent(const json& j) {
    ECS::NameComponent name;
    if (j.contains("name")) name.name = j["name"].get<std::string>();
    return name;
}

ECS::TransformComponent DeserializeTransformComponent(const json& j) {
    ECS::TransformComponent transform;
    if (j.contains("position")) transform.position = DeserializeVector3(j["position"]);
    if (j.contains("rotation")) transform.rotation = DeserializeQuaternion(j["rotation"]);
    if (j.contains("scale")) transform.scale = DeserializeVector3(j["scale"]);
    if (j.contains("visible")) transform.visible = JB(j["visible"]);
    return transform;
}

ECS::MaterialComponent DeserializeMaterialComponent(const json& j) {
    ECS::MaterialComponent material;
    if (j.contains("baseColor")) material.baseColor = DeserializeVector3(j["baseColor"]);
    material.opacity = j.value("opacity", 1.0f);
    material.metallic = j.value("metallic", 0.0f);
    material.roughness = j.value("roughness", 0.5f);
    if (j.contains("emissiveColor")) material.emissiveColor = DeserializeVector3(j["emissiveColor"]);
    material.emissiveStrength = j.value("emissiveStrength", 0.0f);
    material.baseColorTexture = j.value("baseColorTexture", -1);
    material.normalTexture = j.value("normalTexture", -1);
    material.metallicRoughnessTexture = j.value("metallicRoughnessTexture", -1);
    material.emissiveTexture = j.value("emissiveTexture", -1);
    // Texture paths (optional, added in later versions)
    if (j.contains("uvRegionOffset") && j["uvRegionOffset"].is_array() && j["uvRegionOffset"].size() >= 2)
        material.uvRegionOffset = Math::Vector2(j["uvRegionOffset"][0].get<f32>(), j["uvRegionOffset"][1].get<f32>());
    if (j.contains("uvRegionScale") && j["uvRegionScale"].is_array() && j["uvRegionScale"].size() >= 2)
        material.uvRegionScale = Math::Vector2(j["uvRegionScale"][0].get<f32>(), j["uvRegionScale"][1].get<f32>());
    if (j.contains("uvScrollSpeed") && j["uvScrollSpeed"].is_array() && j["uvScrollSpeed"].size() >= 2)
        material.uvScrollSpeed = Math::Vector2(j["uvScrollSpeed"][0].get<f32>(), j["uvScrollSpeed"][1].get<f32>());
    if (j.contains("flipbookCols")) { u32 v = j["flipbookCols"].get<u32>(); if (v <= 256) material.flipbookCols = v; }
    if (j.contains("flipbookRows")) { u32 v = j["flipbookRows"].get<u32>(); if (v <= 256) material.flipbookRows = v; }
    if (j.contains("flipbookFps")) material.flipbookFps = j["flipbookFps"].get<f32>();
    if (j.contains("baseColorTexturePath")) {
        material.baseColorTexturePath = SafeStr(j["baseColorTexturePath"], MAX_STR_PATH);
    }
    if (j.contains("normalTexturePath")) {
        material.normalTexturePath = SafeStr(j["normalTexturePath"], MAX_STR_PATH);
    }
    if (j.contains("metallicRoughnessTexturePath")) {
        material.metallicRoughnessTexturePath = SafeStr(j["metallicRoughnessTexturePath"], MAX_STR_PATH);
    }
    if (j.contains("emissiveTexturePath")) {
        material.emissiveTexturePath = SafeStr(j["emissiveTexturePath"], MAX_STR_PATH);
    }
    if (j.contains("specularTexturePath")) {
        material.specularTexturePath = SafeStr(j["specularTexturePath"], MAX_STR_PATH);
    }
    material.doubleSided = j.contains("doubleSided") ? JB(j["doubleSided"]) : false;
    material.castShadows = j.contains("castShadows") ? JB(j["castShadows"]) : true;
    material.receiveShadows = j.contains("receiveShadows") ? JB(j["receiveShadows"]) : true;
    if (j.contains("alphaMode")) { i32 v = j["alphaMode"].get<i32>(); if (v >= 0 && v <= 2) material.alphaMode = static_cast<ECS::MaterialComponent::AlphaMode>(v); }
    material.alphaCutoff = j.value("alphaCutoff", 0.5f);
    // Height/parallax mapping (optional, added in later versions)
    if (j.contains("heightTexturePath")) material.heightTexturePath = SafeStr(j["heightTexturePath"], MAX_STR_PATH);
    if (j.contains("parallaxScale")) material.parallaxScale = j["parallaxScale"].get<f32>();
    if (j.contains("parallaxMode")) { u32 v = j["parallaxMode"].get<u32>(); if (v <= 3) material.parallaxMode = v; }
    if (j.contains("pomMaxSteps")) { u32 v = j["pomMaxSteps"].get<u32>(); if (v >= 1 && v <= 256) material.pomMaxSteps = v; }
    if (j.contains("pomHeightScale")) material.pomHeightScale = j["pomHeightScale"].get<f32>();
    // Retro rendering flags (optional, added in later versions)
    if (j.contains("flatShading")) material.flatShading = JB(j["flatShading"]);
    if (j.contains("affineTexturing")) material.affineTexturing = JB(j["affineTexturing"]);
    if (j.contains("vertexSnapping")) material.vertexSnapping = JB(j["vertexSnapping"]);
    if (j.contains("stippleTransparency")) material.stippleTransparency = JB(j["stippleTransparency"]);
    if (j.contains("uvQuantize")) material.uvQuantize = JB(j["uvQuantize"]);
    if (j.contains("gouraudOnly")) material.gouraudOnly = JB(j["gouraudOnly"]);
    if (j.contains("sdfText")) material.sdfText = JB(j["sdfText"]);
    // Full u8 range: the field is the RAW grid resolution (80-320-ish); the old
    // <=31 guard confused it with the 5-bit packed shader value and silently
    // reverted every authored value to the default on load
    if (j.contains("vertexSnapResolution")) material.vertexSnapResolution = j["vertexSnapResolution"].get<u8>();
    if (j.contains("shadowDitherMode")) { u8 v = j["shadowDitherMode"].get<u8>(); if (v <= 3) material.shadowDitherMode = v; }
    if (j.contains("shadowDitherPattern")) { u8 v = j["shadowDitherPattern"].get<u8>(); if (v <= 7) material.shadowDitherPattern = v; }
    if (j.contains("textureFilterOverride")) { u8 v = j["textureFilterOverride"].get<u8>(); if (v <= 3) material.textureFilterOverride = v; }
    if (j.contains("footstepSound")) material.footstepSound = j["footstepSound"].get<std::string>();
    if (j.contains("impactSound"))   material.impactSound = j["impactSound"].get<std::string>();
    if (j.contains("surfaceParticle")) { u8 v = j["surfaceParticle"].get<u8>(); if (v <= 6) material.surfaceParticle = v; }
    if (j.contains("footstepVolume")) material.footstepVolume = j["footstepVolume"].get<f32>();
    if (j.contains("impactThreshold")) material.impactThreshold = j["impactThreshold"].get<f32>();
    if (j.contains("reflectivity")) material.reflectivity = j["reflectivity"].get<f32>();
    if (j.contains("fresnelPower")) material.fresnelPower = j["fresnelPower"].get<f32>();
    if (j.contains("rimLightStrength")) material.rimLightStrength = j["rimLightStrength"].get<f32>();
    if (j.contains("excludeFromCelShading")) material.excludeFromCelShading = JB(j["excludeFromCelShading"]);
    if (j.contains("outlineWidth")) material.outlineWidth = j["outlineWidth"].get<f32>();
    if (j.contains("outlineColor")) material.outlineColor = DeserializeVector3(j["outlineColor"]);
    if (j.contains("ditherGradient")) material.ditherGradient = JB(j["ditherGradient"]);
    if (j.contains("ditherGradientBands")) { u8 v = j["ditherGradientBands"].get<u8>(); if (v >= 2 && v <= 8) material.ditherGradientBands = v; }
    if (j.contains("ditherGradientPattern")) { u8 v = j["ditherGradientPattern"].get<u8>(); if (v <= 5) material.ditherGradientPattern = v; }
    if (j.contains("ditherTransparency")) material.ditherTransparency = JB(j["ditherTransparency"]);
    if (j.contains("ditherTransPattern")) { u8 v = j["ditherTransPattern"].get<u8>(); if (v <= 3) material.ditherTransPattern = v; }
    if (j.contains("ditherTransBlendColor") && j["ditherTransBlendColor"].is_array() && j["ditherTransBlendColor"].size() >= 3) {
        material.ditherTransBlendColor = Math::Vector3(
            j["ditherTransBlendColor"][0].get<f32>(),
            j["ditherTransBlendColor"][1].get<f32>(),
            j["ditherTransBlendColor"][2].get<f32>()
        );
    }
    if (j.contains("ditherTransOpacity")) material.ditherTransOpacity = j["ditherTransOpacity"].get<f32>();
    // Transmission / SSS
    if (j.contains("transmission")) material.transmission = j["transmission"].get<f32>();
    if (j.contains("ior")) material.ior = j["ior"].get<f32>();
    if (j.contains("thickness")) material.thickness = j["thickness"].get<f32>();
    if (j.contains("sssIntensity")) material.sssIntensity = j["sssIntensity"].get<f32>();
    if (j.contains("sssRadius")) material.sssRadius = j["sssRadius"].get<f32>();
    if (j.contains("sssColor")) material.sssColor = DeserializeVector3(j["sssColor"]);
    // Matcap texture
    if (j.contains("matcapTexturePath")) material.matcapTexturePath = SafeStr(j["matcapTexturePath"], MAX_STR_PATH);
    if (j.contains("scrollReflectionTexturePath")) material.scrollReflectionTexturePath = SafeStr(j["scrollReflectionTexturePath"], MAX_STR_PATH);
    if (j.contains("scrollReflectionSpeed")) material.scrollReflectionSpeed = DeserializeVector2(j["scrollReflectionSpeed"]);
    if (j.contains("scrollReflectionStrength")) material.scrollReflectionStrength = j["scrollReflectionStrength"].get<f32>();
    // Procedural surface noise
    if (j.contains("surfaceNoiseScale")) material.surfaceNoiseScale = j["surfaceNoiseScale"].get<f32>();
    if (j.contains("surfaceNoiseStrength")) material.surfaceNoiseStrength = j["surfaceNoiseStrength"].get<f32>();
    return material;
}

ECS::MeshComponent DeserializeMeshComponent(const json& j) {
    ECS::MeshComponent mesh;

    static constexpr usize kMaxVertices = 10'000'000; // SN-H1: OOM cap
    if (j.contains("vertices") && j["vertices"].is_array()) {
        if (j["vertices"].size() > kMaxVertices) {
            ENJIN_LOG_WARN(Asset, "Mesh vertex count %zu exceeds cap", j["vertices"].size());
            return mesh;
        }
        mesh.vertices.reserve(j["vertices"].size());
        for (const auto& v : j["vertices"]) {
            ECS::MeshComponent::Vertex vertex;
            if (v.contains("position")) vertex.position = DeserializeVector3(v["position"]);
            if (v.contains("normal")) vertex.normal = DeserializeVector3(v["normal"]);
            if (v.contains("uv")) vertex.uv = DeserializeVector2(v["uv"]);
            if (v.contains("color")) {
                vertex.color = DeserializeVector4(v["color"]);
            }
            if (v.contains("tangent")) {
                vertex.tangent = DeserializeVector4(v["tangent"]);
            }
            if (v.contains("boneWeights")) {
                vertex.boneWeights = DeserializeVector4(v["boneWeights"]);
            }
            if (v.contains("boneIndices") && v["boneIndices"].is_array() && v["boneIndices"].size() >= 4) {
                vertex.boneIndices[0] = std::min(v["boneIndices"][0].get<u32>(), 255u);
                vertex.boneIndices[1] = std::min(v["boneIndices"][1].get<u32>(), 255u);
                vertex.boneIndices[2] = std::min(v["boneIndices"][2].get<u32>(), 255u);
                vertex.boneIndices[3] = std::min(v["boneIndices"][3].get<u32>(), 255u);
            }
            if (v.contains("boneWeights2")) {
                vertex.boneWeights2 = DeserializeVector4(v["boneWeights2"]);
            }
            if (v.contains("boneIndices2") && v["boneIndices2"].is_array() && v["boneIndices2"].size() >= 4) {
                vertex.boneIndices2[0] = std::min(v["boneIndices2"][0].get<u32>(), 255u);
                vertex.boneIndices2[1] = std::min(v["boneIndices2"][1].get<u32>(), 255u);
                vertex.boneIndices2[2] = std::min(v["boneIndices2"][2].get<u32>(), 255u);
                vertex.boneIndices2[3] = std::min(v["boneIndices2"][3].get<u32>(), 255u);
            }
            // Heal scenes saved before boneWeights2 serialization existed: their
            // dense-rig verts carry truncated weight sets that don't sum to 1
            // (spiky skinning). Renormalizing what survived is the closest
            // reconstruction available.
            {
                f32 wsum = vertex.boneWeights.x + vertex.boneWeights.y +
                           vertex.boneWeights.z + vertex.boneWeights.w +
                           vertex.boneWeights2.x + vertex.boneWeights2.y +
                           vertex.boneWeights2.z + vertex.boneWeights2.w;
                if (wsum > 0.0001f && std::abs(wsum - 1.0f) > 0.01f) {
                    f32 inv = 1.0f / wsum;
                    vertex.boneWeights.x *= inv;  vertex.boneWeights.y *= inv;
                    vertex.boneWeights.z *= inv;  vertex.boneWeights.w *= inv;
                    vertex.boneWeights2.x *= inv; vertex.boneWeights2.y *= inv;
                    vertex.boneWeights2.z *= inv; vertex.boneWeights2.w *= inv;
                }
            }
            mesh.vertices.push_back(vertex);
        }
    }

    if (j.contains("indices") && j["indices"].is_array()) {
        static constexpr usize kMaxIndices = 10'000'000;
        if (j["indices"].size() <= kMaxIndices) {
            mesh.indices = j["indices"].get<std::vector<u32>>();
        }
    }

    // Deserialize sub-meshes (multi-material support)
    if (j.contains("subMeshes") && j["subMeshes"].is_array()) {
        static constexpr usize kMaxSubMeshes = 256;
        usize count = std::min(j["subMeshes"].size(), kMaxSubMeshes);
        mesh.subMeshes.reserve(count);
        for (usize i = 0; i < count; ++i) {
            const auto& smJson = j["subMeshes"][i];
            ECS::MeshComponent::SubMesh sm;
            sm.indexOffset = smJson.value("indexOffset", 0u);
            sm.indexCount = smJson.value("indexCount", 0u);
            sm.materialSlot = smJson.value("materialSlot", 0);
            if (smJson.contains("name")) sm.name = smJson["name"].get<std::string>();
            mesh.subMeshes.push_back(sm);
        }
    }

    // Source asset reference (imported meshes). Additive/backward compatible:
    // scenes saved before this field simply won't have it.
    if (j.contains("source") && j["source"].is_object()) {
        const auto& src = j["source"];
        mesh.source.sourcePath = src.value("path", std::string{});
        mesh.source.meshIndex = src.value("meshIndex", -1);
        mesh.source.axisZToY = src.value("axisZToY", false);
        mesh.source.axisLToR = src.value("axisLToR", false);
        mesh.source.contentHash = src.value("contentHash", u64{0});
    }

    // Reference mode: the scene stored a source reference instead of inline vertices,
    // so pull the geometry from the source file (shared across every mesh that
    // references it). Only when the inline path produced nothing — an inline mesh
    // always wins, keeping old/authored scenes untouched. On failure the mesh stays
    // empty and the cache logs why (missing file / hash drift), rather than crashing.
    if (mesh.vertices.empty() && mesh.source.Valid()) {
        Assets::MeshAssetCache::Get().Resolve(mesh.source, mesh);
    }

    return mesh;
}

ECS::LightComponent DeserializeLightComponent(const json& j) {
    ECS::LightComponent light;
    if (j.contains("type")) { i32 v = j["type"].get<i32>(); if (v >= 0 && v <= 2) light.type = static_cast<ECS::LightType>(v); }
    if (j.contains("color")) light.color = DeserializeVector3(j["color"]);
    light.intensity = j.value("intensity", 1.0f);
    light.range = j.value("range", 10.0f);
    light.constantAttenuation = j.value("constantAttenuation", 1.0f);
    light.linearAttenuation = j.value("linearAttenuation", 0.09f);
    light.quadraticAttenuation = j.value("quadraticAttenuation", 0.032f);
    light.innerConeAngle = j.value("innerConeAngle", 12.5f);
    light.outerConeAngle = j.value("outerConeAngle", 17.5f);
    light.castShadows = j.contains("castShadows") ? JB(j["castShadows"]) : false;
    // Note: old scenes may contain "shadowMapResolution" â€" silently ignored
    return light;
}

ECS::NotesComponent DeserializeNotesComponent(const json& j) {
    ECS::NotesComponent notes;
    if (j.contains("notes")) notes.notes = SafeStr(j["notes"], MAX_STR_LARGE);
    return notes;
}

json SerializeDisplayGraphicComponent(const ECS::DisplayGraphicComponent& dg) {
    json j;
    j["sourcePath"] = dg.sourcePath;
    j["worldHeight"] = RF(dg.worldHeight);
    j["curveTolerance"] = RF(dg.curveTolerance);
    return j;
}

ECS::DisplayGraphicComponent DeserializeDisplayGraphicComponent(const json& j) {
    ECS::DisplayGraphicComponent dg;
    dg.sourcePath = j.value("sourcePath", std::string(""));
    dg.worldHeight = j.value("worldHeight", 1.0f);
    dg.curveTolerance = j.value("curveTolerance", 0.25f);
    dg.dirty = true;   // re-tessellate on load
    return dg;
}

ECS::TextComponent DeserializeTextComponent(const json& j) {
    ECS::TextComponent text;
    text.text = j.value("text", std::string(""));
    text.fontPath = j.value("fontPath", std::string(""));
    text.fontSize = j.value("fontSize", 32.0f);
    text.wrapWidth = j.value("wrapWidth", 512.0f);
    text.textureWidth = j.value("textureWidth", 512u);
    text.textureHeight = j.value("textureHeight", 512u);
    if (j.contains("textColor")) text.textColor = DeserializeVector3(j["textColor"]);
    if (j.contains("bgColor")) text.bgColor = DeserializeVector3(j["bgColor"]);
    text.bgOpacity = j.value("bgOpacity", 1.0f);
    if (j.contains("horizontalAlign")) {
        i32 v = j["horizontalAlign"].get<i32>(); if (v >= 0 && v <= 2) text.horizontalAlign = static_cast<ECS::TextAlign>(v);
    }
    text.paddingX = j.value("paddingX", 16.0f);
    text.paddingY = j.value("paddingY", 16.0f);
    text.sdfText = j.value("sdfText", true);
    text.worldHeight = j.value("worldHeight", 0.5f);
    text.lit = j.value("lit", false);
    text.dirty = true; // Re-rasterize on load
    return text;
}

ECS::CameraComponent DeserializeCameraComponent(const json& j) {
    ECS::CameraComponent camera;
    if (j.contains("projectionType")) { i32 v = j["projectionType"].get<i32>(); if (v >= 0 && v <= 1) camera.projectionType = static_cast<ECS::ProjectionType>(v); }
    camera.fieldOfView = j.value("fieldOfView", 60.0f);
    camera.nearPlane = j.value("nearPlane", 0.1f);
    camera.farPlane = j.value("farPlane", 1000.0f);
    camera.orthoSize = j.value("orthoSize", 10.0f);
    camera.priority = j.value("priority", 0);
    camera.isActive = j.contains("isActive") ? JB(j["isActive"]) : true;
    camera.clearDepth = j.contains("clearDepth") ? JB(j["clearDepth"]) : true;
    camera.clearColor = j.contains("clearColor") ? JB(j["clearColor"]) : true;
    if (j.contains("backgroundColor")) camera.backgroundColor = DeserializeVector3(j["backgroundColor"]);
    camera.viewportX = j.value("viewportX", 0.0f);
    camera.viewportY = j.value("viewportY", 0.0f);
    camera.viewportWidth = j.value("viewportWidth", 1.0f);
    camera.viewportHeight = j.value("viewportHeight", 1.0f);
    camera.cullingMask = j.value("cullingMask", 0xFFFFFFFFu);
    if (j.contains("enablePostProcessing")) camera.enablePostProcessing = j["enablePostProcessing"].get<bool>();
    return camera;
}

json SerializeWeatherZoneComponent(const ECS::WeatherZoneComponent& zone) {
    json j;
    j["halfExtents"] = SerializeVector3(zone.halfExtents);
    j["weatherType"] = zone.weatherType;
    j["rainIntensity"] = RF(zone.rainIntensity);
    j["snowIntensity"] = RF(zone.snowIntensity);
    j["fogDensity"] = RF(zone.fogDensity);
    j["fogColor"] = SerializeVector3(zone.fogColor);
    j["fogStart"] = RF(zone.fogStart);
    j["fogEnd"] = RF(zone.fogEnd);
    j["lightningEnabled"] = RF(zone.lightningEnabled);
    j["lightningMinInterval"] = RF(zone.lightningMinInterval);
    j["lightningMaxInterval"] = RF(zone.lightningMaxInterval);
    j["windDirection"] = SerializeVector3(zone.windDirection);
    j["windStrength"] = RF(zone.windStrength);
    j["priority"] = zone.priority;
    if (!zone.rainTexturePath.empty()) j["rainTexturePath"] = zone.rainTexturePath;
    if (!zone.snowTexturePath.empty()) j["snowTexturePath"] = zone.snowTexturePath;
    return j;
}

ECS::WeatherZoneComponent DeserializeWeatherZoneComponent(const json& j) {
    ECS::WeatherZoneComponent zone;
    if (j.contains("halfExtents")) zone.halfExtents = DeserializeVector3(j["halfExtents"]);
    // 6 = Storm is the highest WeatherType (the old <= 3 cap silently dropped Snow/Fog/Storm)
    if (j.contains("weatherType")) { u32 v = j["weatherType"].get<u32>(); if (v <= 6) zone.weatherType = v; }
    if (j.contains("rainIntensity")) zone.rainIntensity = j["rainIntensity"].get<f32>();
    if (j.contains("snowIntensity")) zone.snowIntensity = j["snowIntensity"].get<f32>();
    if (j.contains("fogDensity")) zone.fogDensity = j["fogDensity"].get<f32>();
    if (j.contains("fogColor")) zone.fogColor = DeserializeVector3(j["fogColor"]);
    if (j.contains("fogStart")) zone.fogStart = j["fogStart"].get<f32>();
    if (j.contains("fogEnd")) zone.fogEnd = j["fogEnd"].get<f32>();
    if (j.contains("lightningEnabled")) zone.lightningEnabled = JB(j["lightningEnabled"]);
    if (j.contains("lightningMinInterval")) zone.lightningMinInterval = j["lightningMinInterval"].get<f32>();
    if (j.contains("lightningMaxInterval")) zone.lightningMaxInterval = j["lightningMaxInterval"].get<f32>();
    if (j.contains("windDirection")) zone.windDirection = DeserializeVector3(j["windDirection"]);
    if (j.contains("windStrength")) zone.windStrength = j["windStrength"].get<f32>();
    if (j.contains("priority")) zone.priority = j["priority"].get<i32>();
    if (j.contains("rainTexturePath")) zone.rainTexturePath = SafeStr(j["rainTexturePath"], MAX_STR_PATH);
    if (j.contains("snowTexturePath")) zone.snowTexturePath = SafeStr(j["snowTexturePath"], MAX_STR_PATH);
    return zone;
}

json SerializeWaterVolumeComponent(const ECS::WaterVolumeComponent& volume) {
    json j;
    j["halfExtents"] = SerializeVector3(volume.halfExtents);
    j["waterType"] = static_cast<u32>(volume.waterType);
    j["waterColor"] = SerializeVector3(volume.waterColor);
    j["opacity"] = RF(volume.opacity);
    j["waveSpeed"] = RF(volume.waveSpeed);
    j["waveHeight"] = RF(volume.waveHeight);
    j["enableShore"] = RF(volume.enableShore);
    j["shoreWidth"] = RF(volume.shoreWidth);
    j["foamIntensity"] = RF(volume.foamIntensity);
    j["foamScale"] = RF(volume.foamScale);
    j["shoreColor"] = SerializeVector3(volume.shoreColor);
    j["enableBuoyancy"] = volume.enableBuoyancy;
    j["buoyancyStrength"] = RF(volume.buoyancyStrength);
    j["buoyancyDrag"] = RF(volume.buoyancyDrag);
    j["priority"] = volume.priority;
    j["iceColor"] = SerializeVector3(volume.iceColor);
    j["iceOpacity"] = RF(volume.iceOpacity);
    j["freezeRate"] = RF(volume.freezeRate);
    j["thawRate"] = RF(volume.thawRate);
    return j;
}

ECS::WaterVolumeComponent DeserializeWaterVolumeComponent(const json& j) {
    ECS::WaterVolumeComponent volume;
    if (j.contains("halfExtents")) volume.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("waterType")) { u32 v = j["waterType"].get<u32>(); if (v <= 3) volume.waterType = static_cast<ECS::WaterType>(v); }
    if (j.contains("waterColor")) volume.waterColor = DeserializeVector3(j["waterColor"]);
    if (j.contains("opacity")) volume.opacity = j["opacity"].get<f32>();
    if (j.contains("waveSpeed")) volume.waveSpeed = j["waveSpeed"].get<f32>();
    if (j.contains("waveHeight")) volume.waveHeight = j["waveHeight"].get<f32>();
    if (j.contains("enableShore")) volume.enableShore = JB(j["enableShore"]);
    if (j.contains("shoreWidth")) volume.shoreWidth = j["shoreWidth"].get<f32>();
    if (j.contains("foamIntensity")) volume.foamIntensity = j["foamIntensity"].get<f32>();
    if (j.contains("foamScale")) volume.foamScale = j["foamScale"].get<f32>();
    if (j.contains("shoreColor")) volume.shoreColor = DeserializeVector3(j["shoreColor"]);
    if (j.contains("enableBuoyancy")) volume.enableBuoyancy = JB(j["enableBuoyancy"]);
    if (j.contains("buoyancyStrength")) volume.buoyancyStrength = j["buoyancyStrength"].get<f32>();
    if (j.contains("buoyancyDrag")) volume.buoyancyDrag = j["buoyancyDrag"].get<f32>();
    if (j.contains("priority")) volume.priority = j["priority"].get<i32>();
    if (j.contains("iceColor")) volume.iceColor = DeserializeVector3(j["iceColor"]);
    if (j.contains("iceOpacity")) volume.iceOpacity = j["iceOpacity"].get<f32>();
    if (j.contains("freezeRate")) volume.freezeRate = j["freezeRate"].get<f32>();
    if (j.contains("thawRate")) volume.thawRate = j["thawRate"].get<f32>();
    return volume;
}

json SerializeWater3DComponent(const ECS::Water3DComponent& w) {
    json j;
    j["width"] = RF(w.settings.width);
    j["depth"] = RF(w.settings.depth);
    j["tileSize"] = RF(w.settings.tileSize);
    j["style"] = static_cast<u32>(w.settings.style);
    j["shallowColor"] = SerializeVector3(w.settings.shallowColor);
    j["deepColor"] = SerializeVector3(w.settings.deepColor);
    j["opacity"] = RF(w.settings.opacity);
    j["waveSpeed"] = RF(w.settings.waveSpeed);
    j["waveHeight"] = RF(w.settings.waveHeight);
    j["waveFrequency"] = RF(w.settings.waveFrequency);
    j["waveDirection"] = SerializeVector2(w.settings.waveDirection);
    j["gerstnerWaves"] = w.settings.gerstnerWaves;
    j["waveSteepness"] = RF(w.settings.waveSteepness);
    j["uvScrollSpeed"] = SerializeVector2(w.settings.uvScrollSpeed);
    j["reflectionStrength"] = RF(w.settings.reflectionStrength);
    j["fresnelPower"] = RF(w.settings.fresnelPower);
    j["enableFoam"] = w.settings.enableFoam;
    j["foamThreshold"] = RF(w.settings.foamThreshold);
    j["foamScale"] = RF(w.settings.foamScale);
    j["enableBuoyancy"] = w.settings.enableBuoyancy;
    j["buoyancyStrength"] = RF(w.settings.buoyancyStrength);
    j["buoyancyDrag"] = RF(w.settings.buoyancyDrag);
    return j;
}

ECS::Water3DComponent DeserializeWater3DComponent(const json& j) {
    ECS::Water3DComponent w;
    if (j.contains("width")) w.settings.width = j["width"].get<f32>();
    if (j.contains("depth")) w.settings.depth = j["depth"].get<f32>();
    if (j.contains("tileSize")) w.settings.tileSize = Math::Max(j["tileSize"].get<f32>(), 0.5f);
    if (j.contains("style")) { u32 v = j["style"].get<u32>(); if (v <= 4) w.settings.style = static_cast<Effects::WaterStyle>(v); }
    if (j.contains("shallowColor")) w.settings.shallowColor = DeserializeVector3(j["shallowColor"]);
    if (j.contains("deepColor")) w.settings.deepColor = DeserializeVector3(j["deepColor"]);
    if (j.contains("opacity")) w.settings.opacity = j["opacity"].get<f32>();
    if (j.contains("waveSpeed")) w.settings.waveSpeed = j["waveSpeed"].get<f32>();
    if (j.contains("waveHeight")) w.settings.waveHeight = j["waveHeight"].get<f32>();
    if (j.contains("waveFrequency")) w.settings.waveFrequency = j["waveFrequency"].get<f32>();
    if (j.contains("waveDirection")) w.settings.waveDirection = DeserializeVector2(j["waveDirection"]);
    if (j.contains("gerstnerWaves")) w.settings.gerstnerWaves = JB(j["gerstnerWaves"]);
    if (j.contains("waveSteepness")) w.settings.waveSteepness = Math::Clamp(j["waveSteepness"].get<f32>(), 0.0f, 1.0f);
    if (j.contains("uvScrollSpeed")) w.settings.uvScrollSpeed = DeserializeVector2(j["uvScrollSpeed"]);
    if (j.contains("reflectionStrength")) w.settings.reflectionStrength = j["reflectionStrength"].get<f32>();
    if (j.contains("fresnelPower")) w.settings.fresnelPower = j["fresnelPower"].get<f32>();
    if (j.contains("enableFoam")) w.settings.enableFoam = JB(j["enableFoam"]);
    if (j.contains("foamThreshold")) w.settings.foamThreshold = j["foamThreshold"].get<f32>();
    if (j.contains("foamScale")) w.settings.foamScale = j["foamScale"].get<f32>();
    if (j.contains("enableBuoyancy")) w.settings.enableBuoyancy = JB(j["enableBuoyancy"]);
    if (j.contains("buoyancyStrength")) w.settings.buoyancyStrength = j["buoyancyStrength"].get<f32>();
    if (j.contains("buoyancyDrag")) w.settings.buoyancyDrag = j["buoyancyDrag"].get<f32>();
    return w;
}

json SerializeShrubVolumeComponent(const ECS::ShrubVolumeComponent& shrub) {
    json j;
    j["halfExtents"] = SerializeVector3(shrub.halfExtents);
    j["density"] = shrub.density;
    j["shrubHeight"] = RF(shrub.shrubHeight);
    j["heightVariance"] = RF(shrub.heightVariance);
    j["width"] = shrub.width;
    j["baseColor"] = SerializeVector3(shrub.baseColor);
    j["tipColor"] = SerializeVector3(shrub.tipColor);
    j["windSwayStrength"] = RF(shrub.windSwayStrength);
    j["quadsPerShrub"] = shrub.quadsPerShrub;
    if (!shrub.customAssetPath.empty()) j["customAssetPath"] = shrub.customAssetPath;
    return j;
}

ECS::ShrubVolumeComponent DeserializeShrubVolumeComponent(const json& j) {
    ECS::ShrubVolumeComponent shrub;
    if (j.contains("halfExtents")) shrub.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("density")) shrub.density = j["density"].get<u32>();
    if (j.contains("shrubHeight")) shrub.shrubHeight = j["shrubHeight"].get<f32>();
    if (j.contains("heightVariance")) shrub.heightVariance = j["heightVariance"].get<f32>();
    if (j.contains("width")) shrub.width = j["width"].get<f32>();
    if (j.contains("baseColor")) shrub.baseColor = DeserializeVector3(j["baseColor"]);
    if (j.contains("tipColor")) shrub.tipColor = DeserializeVector3(j["tipColor"]);
    if (j.contains("windSwayStrength")) shrub.windSwayStrength = j["windSwayStrength"].get<f32>();
    if (j.contains("quadsPerShrub")) shrub.quadsPerShrub = j["quadsPerShrub"].get<u32>();
    if (j.contains("customAssetPath")) shrub.customAssetPath = SafeStr(j["customAssetPath"], MAX_STR_PATH);
    return shrub;
}

json SerializeTreeVolumeComponent(const ECS::TreeVolumeComponent& tree) {
    json j;
    j["halfExtents"] = SerializeVector3(tree.halfExtents);
    j["density"] = tree.density;
    j["trunkHeight"] = RF(tree.trunkHeight);
    j["trunkWidth"] = RF(tree.trunkWidth);
    j["canopyRadius"] = RF(tree.canopyRadius);
    j["canopyOffset"] = RF(tree.canopyOffset);
    j["generateColliders"] = tree.generateColliders;
    j["trunkColor"] = SerializeVector3(tree.trunkColor);
    j["canopyBaseColor"] = SerializeVector3(tree.canopyBaseColor);
    j["canopyTipColor"] = SerializeVector3(tree.canopyTipColor);
    j["windSwayStrength"] = RF(tree.windSwayStrength);
    j["canopyQuads"] = tree.canopyQuads;
    j["minHeightScale"] = RF(tree.minHeightScale);
    j["maxHeightScale"] = RF(tree.maxHeightScale);
    j["treeType"] = static_cast<u32>(tree.treeType);
    j["springCanopyColor"] = SerializeVector3(tree.springCanopyColor);
    j["summerCanopyColor"] = SerializeVector3(tree.summerCanopyColor);
    j["fallCanopyColor"] = SerializeVector3(tree.fallCanopyColor);
    if (!tree.barkTexturePath.empty()) j["barkTexturePath"] = tree.barkTexturePath;
    if (!tree.canopyTexturePath.empty()) j["canopyTexturePath"] = tree.canopyTexturePath;
    return j;
}

ECS::TreeVolumeComponent DeserializeTreeVolumeComponent(const json& j) {
    ECS::TreeVolumeComponent tree;
    if (j.contains("halfExtents")) tree.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("density")) tree.density = j["density"].get<u32>();
    if (j.contains("trunkHeight")) tree.trunkHeight = j["trunkHeight"].get<f32>();
    if (j.contains("trunkWidth")) tree.trunkWidth = j["trunkWidth"].get<f32>();
    if (j.contains("canopyRadius")) tree.canopyRadius = j["canopyRadius"].get<f32>();
    if (j.contains("canopyOffset")) tree.canopyOffset = j["canopyOffset"].get<f32>();
    if (j.contains("generateColliders")) tree.generateColliders = JB(j["generateColliders"]);
    if (j.contains("trunkColor")) tree.trunkColor = DeserializeVector3(j["trunkColor"]);
    if (j.contains("canopyBaseColor")) tree.canopyBaseColor = DeserializeVector3(j["canopyBaseColor"]);
    if (j.contains("canopyTipColor")) tree.canopyTipColor = DeserializeVector3(j["canopyTipColor"]);
    if (j.contains("windSwayStrength")) tree.windSwayStrength = j["windSwayStrength"].get<f32>();
    if (j.contains("canopyQuads")) tree.canopyQuads = j["canopyQuads"].get<u32>();
    if (j.contains("minHeightScale")) tree.minHeightScale = j["minHeightScale"].get<f32>();
    if (j.contains("maxHeightScale")) tree.maxHeightScale = j["maxHeightScale"].get<f32>();
    if (j.contains("treeType")) { u32 v = j["treeType"].get<u32>(); if (v <= 1) tree.treeType = static_cast<ECS::TreeType>(v); }
    if (j.contains("springCanopyColor")) tree.springCanopyColor = DeserializeVector3(j["springCanopyColor"]);
    if (j.contains("summerCanopyColor")) tree.summerCanopyColor = DeserializeVector3(j["summerCanopyColor"]);
    if (j.contains("fallCanopyColor")) tree.fallCanopyColor = DeserializeVector3(j["fallCanopyColor"]);
    if (j.contains("barkTexturePath")) tree.barkTexturePath = SafeStr(j["barkTexturePath"], MAX_STR_PATH);
    if (j.contains("canopyTexturePath")) tree.canopyTexturePath = SafeStr(j["canopyTexturePath"], MAX_STR_PATH);
    return tree;
}

// Terrain component serialization
json SerializeTerrainComponent(const ECS::TerrainComponent& terrain) {
    json j;
    j["gridWidth"] = RF(terrain.gridWidth);
    j["gridHeight"] = RF(terrain.gridHeight);
    j["cellSize"] = RF(terrain.cellSize);
    j["maxHeight"] = RF(terrain.maxHeight);
    j["heightmap"] = terrain.heightmap;
    j["splatmap"] = terrain.splatmap;
    json layersArr = json::array();
    for (int i = 0; i < 4; ++i) {
        json layer;
        layer["texturePath"] = terrain.layers[i].texturePath;
        layer["tileScale"] = RF(terrain.layers[i].tileScale);
        layersArr.push_back(layer);
    }
    j["layers"] = layersArr;
    return j;
}

ECS::TerrainComponent DeserializeTerrainComponent(const json& j) {
    ECS::TerrainComponent terrain;
    if (j.contains("gridWidth")) terrain.gridWidth = std::min(j["gridWidth"].get<u32>(), 4096u); // SN-H2: cap
    if (j.contains("gridHeight")) terrain.gridHeight = std::min(j["gridHeight"].get<u32>(), 4096u);
    if (j.contains("cellSize")) terrain.cellSize = j["cellSize"].get<f32>();
    if (j.contains("maxHeight")) terrain.maxHeight = j["maxHeight"].get<f32>();
    if (j.contains("heightmap") && j["heightmap"].is_array()) {
        static constexpr usize kMaxGridElements = 4096ull * 4096ull;
        usize expected = static_cast<usize>(terrain.gridWidth) * terrain.gridHeight;
        if (expected <= kMaxGridElements && j["heightmap"].size() <= expected) {
            auto hm = j["heightmap"].get<std::vector<f32>>();
            terrain.heightmap = std::move(hm);
        }
    }
    if (j.contains("splatmap") && j["splatmap"].is_array()) {
        static constexpr usize kMaxGridElements = 4096ull * 4096ull * 4;
        usize expected = static_cast<usize>(terrain.gridWidth) * terrain.gridHeight * 4;
        if (expected <= kMaxGridElements && j["splatmap"].size() <= expected) {
            auto sm = j["splatmap"].get<std::vector<f32>>();
            terrain.splatmap = std::move(sm);
        }
    }
    if (j.contains("layers")) {
        const auto& layersArr = j["layers"];
        for (int i = 0; i < 4 && i < static_cast<int>(layersArr.size()); ++i) {
            if (layersArr[i].contains("texturePath"))
                terrain.layers[i].texturePath = SafeStr(layersArr[i]["texturePath"], MAX_STR_PATH);
            if (layersArr[i].contains("tileScale"))
                terrain.layers[i].tileScale = layersArr[i]["tileScale"].get<f32>();
        }
    }
    terrain.meshDirty = true;
    return terrain;
}

// 2D Terrain component serialization
json SerializeTerrain2DComponent(const ECS::Terrain2DComponent& terrain) {
    json j;
    json points = json::array();
    for (const auto& p : terrain.controlPoints) {
        points.push_back(SerializeVector2(p));
    }
    j["controlPoints"] = points;
    j["depth"] = RF(terrain.depth);
    j["uvScale"] = RF(terrain.uvScale);
    j["texturePath"] = terrain.texturePath;
    j["autoColliders"] = RF(terrain.autoColliders);
    return j;
}

ECS::Terrain2DComponent DeserializeTerrain2DComponent(const json& j) {
    ECS::Terrain2DComponent terrain;
    if (j.contains("controlPoints")) {
        for (const auto& p : j["controlPoints"]) {
            terrain.controlPoints.push_back(DeserializeVector2(p));
        }
    }
    if (j.contains("depth")) terrain.depth = j["depth"].get<f32>();
    if (j.contains("uvScale")) terrain.uvScale = j["uvScale"].get<f32>();
    if (j.contains("texturePath")) terrain.texturePath = SafeStr(j["texturePath"], MAX_STR_PATH);
    if (j.contains("autoColliders")) terrain.autoColliders = JB(j["autoColliders"]);
    terrain.meshDirty = true;
    return terrain;
}

json SerializeCameraTriggerComponent(const ECS::CameraTriggerComponent& trigger) {
    json j;
    j["halfExtents"] = SerializeVector3(trigger.halfExtents);
    j["targetCamera"] = static_cast<u64>(trigger.targetCamera);
    j["priority"] = trigger.priority;
    j["blendTime"] = RF(trigger.blendTime);
    return j;
}

ECS::CameraTriggerComponent DeserializeCameraTriggerComponent(const json& j) {
    ECS::CameraTriggerComponent trigger;
    if (j.contains("halfExtents")) trigger.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("targetCamera")) trigger.targetCamera = j["targetCamera"].get<u64>();
    if (j.contains("priority")) trigger.priority = j["priority"].get<i32>();
    if (j.contains("blendTime")) trigger.blendTime = j["blendTime"].get<f32>();
    return trigger;
}

json SerializeTemperatureZoneComponent(const ECS::TemperatureZoneComponent& zone) {
    json j;
    j["halfExtents"] = SerializeVector3(zone.halfExtents);
    j["temperature"] = RF(zone.temperature);
    j["priority"] = zone.priority;
    return j;
}

ECS::TemperatureZoneComponent DeserializeTemperatureZoneComponent(const json& j) {
    ECS::TemperatureZoneComponent zone;
    if (j.contains("halfExtents")) zone.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("temperature")) zone.temperature = j["temperature"].get<f32>();
    if (j.contains("priority")) zone.priority = j["priority"].get<i32>();
    return zone;
}

json SerializeGravityZoneComponent(const ECS::GravityZoneComponent& zone) {
    json j;
    j["shape"] = static_cast<u32>(zone.shape);
    j["mode"] = static_cast<u32>(zone.mode);
    j["halfExtents"] = SerializeVector3(zone.halfExtents);
    j["gravityDirection"] = SerializeVector3(zone.gravityDirection);
    j["gravityStrength"] = RF(zone.gravityStrength);
    j["priority"] = zone.priority;
    j["isActive"] = zone.isActive;
    return j;
}

ECS::GravityZoneComponent DeserializeGravityZoneComponent(const json& j) {
    ECS::GravityZoneComponent zone;
    if (j.contains("shape")) { u32 v = j["shape"].get<u32>(); if (v <= 1) zone.shape = static_cast<ECS::GravityZoneShape>(v); }
    if (j.contains("mode")) { u32 v = j["mode"].get<u32>(); if (v <= 1) zone.mode = static_cast<ECS::GravityZoneMode>(v); }
    if (j.contains("halfExtents")) zone.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("gravityDirection")) zone.gravityDirection = DeserializeVector3(j["gravityDirection"]);
    if (j.contains("gravityStrength")) zone.gravityStrength = j["gravityStrength"].get<f32>();
    if (j.contains("priority")) zone.priority = j["priority"].get<i32>();
    if (j.contains("isActive")) zone.isActive = JB(j["isActive"]);
    return zone;
}

// Reflection Probe
json SerializeReflectionProbeComponent(const ECS::ReflectionProbeComponent& probe) {
    json j;
    j["boxMin"] = SerializeVector3(probe.boxMin);
    j["boxMax"] = SerializeVector3(probe.boxMax);
    j["resolution"] = probe.resolution;
    j["intensity"] = probe.intensity;
    j["priority"] = probe.priority;
    j["baked"] = probe.baked;
    j["isActive"] = probe.isActive;
    j["blendDistance"] = probe.blendDistance;
    return j;
}

ECS::ReflectionProbeComponent DeserializeReflectionProbeComponent(const json& j) {
    ECS::ReflectionProbeComponent probe;
    if (j.contains("boxMin")) probe.boxMin = DeserializeVector3(j["boxMin"]);
    if (j.contains("boxMax")) probe.boxMax = DeserializeVector3(j["boxMax"]);
    if (j.contains("resolution")) probe.resolution = j["resolution"].get<u32>();
    if (j.contains("intensity")) probe.intensity = j["intensity"].get<f32>();
    if (j.contains("priority")) probe.priority = j["priority"].get<u32>();
    if (j.contains("baked")) probe.baked = JB(j["baked"]);
    if (j.contains("isActive")) probe.isActive = JB(j["isActive"]);
    if (j.contains("blendDistance")) probe.blendDistance = j["blendDistance"].get<f32>();
    return probe;
}

json SerializeReflectivePlaneComponent(const ECS::ReflectivePlaneComponent& p) {
    json j;
    j["reflectionStrength"] = p.reflectionStrength;
    j["tint"] = SerializeVector3(p.tint);
    j["blur"] = p.blur;
    j["fresnelPower"] = p.fresnelPower;
    j["resolution"] = p.resolution;
    j["clipBias"] = p.clipBias;
    j["active"] = p.active;
    return j;
}

ECS::ReflectivePlaneComponent DeserializeReflectivePlaneComponent(const json& j) {
    ECS::ReflectivePlaneComponent p;
    if (j.contains("reflectionStrength")) p.reflectionStrength = j["reflectionStrength"].get<f32>();
    if (j.contains("tint")) p.tint = DeserializeVector3(j["tint"]);
    if (j.contains("blur")) p.blur = j["blur"].get<f32>();
    if (j.contains("fresnelPower")) p.fresnelPower = j["fresnelPower"].get<f32>();
    if (j.contains("resolution")) p.resolution = j["resolution"].get<u32>();
    if (j.contains("clipBias")) p.clipBias = j["clipBias"].get<f32>();
    if (j.contains("active")) p.active = JB(j["active"]);
    return p;
}

// Ladder (G1)
json SerializeLadderComponent(const ECS::LadderComponent& l) {
    json j;
    j["halfExtents"] = SerializeVector3(l.halfExtents);
    j["climbSpeed"] = l.climbSpeed;
    j["topBoost"] = l.topBoost;
    j["allowJumpOff"] = l.allowJumpOff;
    return j;
}

ECS::LadderComponent DeserializeLadderComponent(const json& j) {
    ECS::LadderComponent l;
    if (j.contains("halfExtents")) l.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("climbSpeed")) l.climbSpeed = j["climbSpeed"].get<f32>();
    if (j.contains("topBoost")) l.topBoost = j["topBoost"].get<f32>();
    if (j.contains("allowJumpOff")) l.allowJumpOff = JB(j["allowJumpOff"]);
    return l;
}

// Door (G7)
json SerializeDoorComponent(const ECS::DoorComponent& d) {
    json j;
    j["openAngle"] = d.openAngle;
    j["openSpeed"] = d.openSpeed;
    j["interactRadius"] = d.interactRadius;
    j["autoCloseDelay"] = d.autoCloseDelay;
    j["locked"] = d.locked;
    j["startOpen"] = d.startOpen;
    return j;
}

ECS::DoorComponent DeserializeDoorComponent(const json& j) {
    ECS::DoorComponent d;
    if (j.contains("openAngle")) d.openAngle = j["openAngle"].get<f32>();
    if (j.contains("openSpeed")) d.openSpeed = j["openSpeed"].get<f32>();
    if (j.contains("interactRadius")) d.interactRadius = j["interactRadius"].get<f32>();
    if (j.contains("autoCloseDelay")) d.autoCloseDelay = j["autoCloseDelay"].get<f32>();
    if (j.contains("locked")) d.locked = JB(j["locked"]);
    if (j.contains("startOpen")) d.startOpen = JB(j["startOpen"]);
    return d;
}

// Rope (G3)
json SerializeRopeComponent(const ECS::RopeComponent& r) {
    json j;
    j["style"] = static_cast<i32>(r.style);
    j["length"] = r.length;
    j["segments"] = r.segments;
    j["thickness"] = r.thickness;
    j["iterations"] = r.iterations;
    j["damping"] = r.damping;
    j["gravityScale"] = r.gravityScale;
    j["wind"] = SerializeVector3(r.wind);
    j["useWeatherWind"] = r.useWeatherWind;
    j["weatherWindScale"] = r.weatherWindScale;
    j["collide"] = r.collide;
    j["collisionSkin"] = r.collisionSkin;
    j["friction"] = r.friction;
    j["endMass"] = r.endMass;
    j["endAttachName"] = r.endAttachName;
    j["pinBottom"] = r.pinBottom;
    return j;
}

ECS::RopeComponent DeserializeRopeComponent(const json& j) {
    ECS::RopeComponent r;
    if (j.contains("style")) {
        i32 s = j["style"].get<i32>();
        if (s >= 0 && s <= static_cast<i32>(ECS::RopeStyle::Chain))
            r.style = static_cast<ECS::RopeStyle>(s);
    }
    if (j.contains("length")) r.length = j["length"].get<f32>();
    if (j.contains("segments")) r.segments = j["segments"].get<i32>();
    if (j.contains("thickness")) r.thickness = j["thickness"].get<f32>();
    if (j.contains("iterations")) r.iterations = j["iterations"].get<i32>();
    if (j.contains("damping")) r.damping = j["damping"].get<f32>();
    if (j.contains("gravityScale")) r.gravityScale = j["gravityScale"].get<f32>();
    if (j.contains("wind")) r.wind = DeserializeVector3(j["wind"]);
    if (j.contains("useWeatherWind")) r.useWeatherWind = JB(j["useWeatherWind"]);
    if (j.contains("weatherWindScale")) r.weatherWindScale = j["weatherWindScale"].get<f32>();
    if (j.contains("collide")) r.collide = JB(j["collide"]);
    if (j.contains("collisionSkin")) r.collisionSkin = j["collisionSkin"].get<f32>();
    if (j.contains("friction")) r.friction = j["friction"].get<f32>();
    if (j.contains("endMass")) r.endMass = j["endMass"].get<f32>();
    if (j.contains("endAttachName")) r.endAttachName = SafeStr(j["endAttachName"]);
    if (j.contains("pinBottom")) r.pinBottom = JB(j["pinBottom"]);
    return r;
}

// Elemental components
json SerializeElementalSurfaceComponent(const ECS::ElementalSurfaceComponent& s) {
    json j;
    j["accumulation"] = SerializeVector4(s.accumulation);
    j["flammability"] = RF(s.flammability);
    j["accumulationRate"] = RF(s.accumulationRate);
    j["decayRate"] = RF(s.decayRate);
    j["maxAccumulation"] = RF(s.maxAccumulation);
    j["snowDeformation"] = RF(s.snowDeformation);
    return j;
}

ECS::ElementalSurfaceComponent DeserializeElementalSurfaceComponent(const json& j) {
    ECS::ElementalSurfaceComponent s;
    if (j.contains("accumulation")) s.accumulation = DeserializeVector4(j["accumulation"]);
    if (j.contains("flammability")) s.flammability = j["flammability"].get<f32>();
    if (j.contains("accumulationRate")) s.accumulationRate = j["accumulationRate"].get<f32>();
    if (j.contains("decayRate")) s.decayRate = j["decayRate"].get<f32>();
    if (j.contains("maxAccumulation")) s.maxAccumulation = j["maxAccumulation"].get<f32>();
    if (j.contains("snowDeformation")) s.snowDeformation = j["snowDeformation"].get<f32>();
    return s;
}

json SerializeElementalEmitterComponent(const ECS::ElementalEmitterComponent& e) {
    json j;
    j["element"] = SerializeVector4(e.element);
    j["emissionRate"] = RF(e.emissionRate);
    j["intensity"] = RF(e.intensity);
    j["lifetime"] = RF(e.lifetime);
    j["spread"] = RF(e.spread);
    j["speed"] = RF(e.speed);
    j["direction"] = SerializeVector3(e.direction);
    j["active"] = e.active;
    return j;
}

ECS::ElementalEmitterComponent DeserializeElementalEmitterComponent(const json& j) {
    ECS::ElementalEmitterComponent e;
    if (j.contains("element")) e.element = DeserializeVector4(j["element"]);
    if (j.contains("emissionRate")) e.emissionRate = j["emissionRate"].get<f32>();
    if (j.contains("intensity")) e.intensity = j["intensity"].get<f32>();
    if (j.contains("lifetime")) e.lifetime = j["lifetime"].get<f32>();
    if (j.contains("spread")) e.spread = j["spread"].get<f32>();
    if (j.contains("speed")) e.speed = j["speed"].get<f32>();
    if (j.contains("direction")) e.direction = DeserializeVector3(j["direction"]);
    if (j.contains("active")) e.active = JB(j["active"]);
    return e;
}

json SerializeGPUParticleEmitterComponent(const ECS::GPUParticleEmitterComponent& e) {
    json j;
    j["preset"] = static_cast<u32>(e.preset);
    j["emitting"] = e.emitting;
    j["spawnRate"] = RF(e.spawnRate);
    j["direction"] = SerializeVector3(e.direction);
    j["shape"] = static_cast<u32>(e.shape);
    j["shapeSize"] = RF(e.shapeSize);
    j["emit2D"] = e.emit2D;
    j["angle2D"] = RF(e.angle2D);
    j["sortingLayer"] = e.sortingLayer;
    j["orderInLayer"] = e.orderInLayer;
    j["color"] = SerializeVector4(e.customColor);
    j["size"] = RF(e.customSize);
    j["lifetime"] = RF(e.customLifetime);
    j["speed"] = RF(e.customSpeed);
    j["spread"] = RF(e.customSpread);
    j["gravityScale"] = RF(e.customGravityScale);
    j["drag"] = RF(e.customDrag);
    j["sprite"] = static_cast<u32>(e.sprite);
    j["softness"] = RF(e.softness);
    j["collide"] = e.collide;
    j["bounciness"] = RF(e.bounciness);
    j["friction"] = RF(e.friction);
    j["collisionRadius"] = RF(e.collisionRadius);
    if (!e.impactSound.empty()) j["impactSound"] = e.impactSound;
    j["impactVolume"] = RF(e.impactVolume);
    j["impactMinSpeed"] = RF(e.impactMinSpeed);
    j["impactCooldown"] = RF(e.impactCooldown);
    j["leaveStains"] = e.leaveStains;
    j["stainColor"] = SerializeVector4(e.stainColor);
    j["stainSize"] = RF(e.stainSize);
    j["stainLifetime"] = RF(e.stainLifetime);
    if (!e.spriteTexturePath.empty()) j["spriteTexture"] = e.spriteTexturePath;
    return j;
}

ECS::GPUParticleEmitterComponent DeserializeGPUParticleEmitterComponent(const json& j) {
    ECS::GPUParticleEmitterComponent e;
    if (j.contains("preset")) { u32 v = j["preset"].get<u32>(); if (v < static_cast<u32>(Effects::GPUParticlePreset::Count)) e.preset = static_cast<Effects::GPUParticlePreset>(v); }
    if (j.contains("emitting")) e.emitting = JB(j["emitting"]);
    if (j.contains("spawnRate")) e.spawnRate = j["spawnRate"].get<f32>();
    if (j.contains("direction")) e.direction = DeserializeVector3(j["direction"]);
    if (j.contains("emit2D")) e.emit2D = JB(j["emit2D"]);
    if (j.contains("angle2D")) e.angle2D = j["angle2D"].get<f32>();
    if (j.contains("sortingLayer")) e.sortingLayer = j["sortingLayer"].get<i32>();
    if (j.contains("orderInLayer")) e.orderInLayer = j["orderInLayer"].get<i32>();
    if (j.contains("shape")) { u32 v = j["shape"].get<u32>(); if (v <= 6) e.shape = static_cast<ECS::EmitShape>(v); }
    if (j.contains("shapeSize")) e.shapeSize = j["shapeSize"].get<f32>();
    if (j.contains("color")) e.customColor = DeserializeVector4(j["color"]);
    if (j.contains("size")) e.customSize = j["size"].get<f32>();
    if (j.contains("lifetime")) e.customLifetime = j["lifetime"].get<f32>();
    if (j.contains("speed")) e.customSpeed = j["speed"].get<f32>();
    if (j.contains("spread")) e.customSpread = j["spread"].get<f32>();
    if (j.contains("gravityScale")) e.customGravityScale = j["gravityScale"].get<f32>();
    if (j.contains("drag")) e.customDrag = j["drag"].get<f32>();
    if (j.contains("sprite")) { u32 v = j["sprite"].get<u32>(); if (v <= 5) e.sprite = static_cast<u8>(v); }
    if (j.contains("softness")) e.softness = j["softness"].get<f32>();
    if (j.contains("spriteTexture")) e.spriteTexturePath = j["spriteTexture"].get<std::string>();
    if (j.contains("collide")) e.collide = JB(j["collide"]);
    if (j.contains("bounciness")) e.bounciness = j["bounciness"].get<f32>();
    if (j.contains("friction")) e.friction = j["friction"].get<f32>();
    if (j.contains("collisionRadius")) e.collisionRadius = j["collisionRadius"].get<f32>();
    if (j.contains("impactSound")) e.impactSound = SafeStr(j["impactSound"]);
    if (j.contains("impactVolume")) e.impactVolume = j["impactVolume"].get<f32>();
    if (j.contains("impactMinSpeed")) e.impactMinSpeed = j["impactMinSpeed"].get<f32>();
    if (j.contains("impactCooldown")) e.impactCooldown = j["impactCooldown"].get<f32>();
    if (j.contains("leaveStains")) e.leaveStains = JB(j["leaveStains"]);
    if (j.contains("stainColor")) e.stainColor = DeserializeVector4(j["stainColor"]);
    if (j.contains("stainSize")) e.stainSize = j["stainSize"].get<f32>();
    if (j.contains("stainLifetime")) e.stainLifetime = j["stainLifetime"].get<f32>();
    return e;
}

json SerializeCustomShaderComponent(const ECS::CustomShaderComponent& c) {
    json j;
    j["vs"] = c.vertexSource;
    j["fs"] = c.fragmentSource;
    j["label"] = c.graphLabel;
    if (!c.graphJson.empty()) j["graph"] = c.graphJson;
    return j;
}

ECS::CustomShaderComponent DeserializeCustomShaderComponent(const json& j) {
    ECS::CustomShaderComponent c;
    if (j.contains("vs")) c.vertexSource = SafeStr(j["vs"], 262144);
    if (j.contains("fs")) c.fragmentSource = SafeStr(j["fs"], 262144);
    if (j.contains("label")) c.graphLabel = SafeStr(j["label"]);
    if (j.contains("graph")) c.graphJson = SafeStr(j["graph"], 262144);
    return c;
}

json SerializeClothComponent(const ECS::ClothComponent& c) {
    json j;
    j["width"] = RF(c.width);
    j["height"] = RF(c.height);
    j["resX"] = c.resX;
    j["resY"] = c.resY;
    j["iterations"] = c.iterations;
    j["damping"] = RF(c.damping);
    j["gravityScale"] = RF(c.gravityScale);
    j["wind"] = SerializeVector3(c.wind);
    j["useWeatherWind"] = c.useWeatherWind;
    j["weatherWindScale"] = RF(c.weatherWindScale);
    j["pin"] = static_cast<u32>(c.pin);
    j["tearable"] = c.tearable;
    j["tearThreshold"] = RF(c.tearThreshold);
    j["collide"] = c.collide;
    j["collisionSkin"] = RF(c.collisionSkin);
    j["friction"] = RF(c.friction);
    j["selfCollide"] = c.selfCollide;
    j["thickness"] = RF(c.thickness);
    j["pinStrength"] = RF(c.pinStrength);
    j["seams"] = static_cast<u32>(c.seams);
    j["seamSpacing"] = c.seamSpacing;
    j["seamStrength"] = RF(c.seamStrength);
    return j;
}

ECS::ClothComponent DeserializeClothComponent(const json& j) {
    ECS::ClothComponent c;
    if (j.contains("width")) c.width = j["width"].get<f32>();
    if (j.contains("height")) c.height = j["height"].get<f32>();
    if (j.contains("resX")) c.resX = std::clamp(j["resX"].get<i32>(), 2, 128);
    if (j.contains("resY")) c.resY = std::clamp(j["resY"].get<i32>(), 2, 128);
    if (j.contains("iterations")) c.iterations = std::clamp(j["iterations"].get<i32>(), 1, 32);
    if (j.contains("damping")) c.damping = j["damping"].get<f32>();
    if (j.contains("gravityScale")) c.gravityScale = j["gravityScale"].get<f32>();
    if (j.contains("wind")) c.wind = DeserializeVector3(j["wind"]);
    if (j.contains("useWeatherWind")) c.useWeatherWind = JB(j["useWeatherWind"]);
    if (j.contains("weatherWindScale")) c.weatherWindScale = j["weatherWindScale"].get<f32>();
    if (j.contains("pin")) { u32 v = j["pin"].get<u32>(); if (v <= 5) c.pin = static_cast<ECS::ClothPin>(v); }  // 5 = BottomEdge
    if (j.contains("tearable")) c.tearable = JB(j["tearable"]);
    if (j.contains("tearThreshold")) c.tearThreshold = j["tearThreshold"].get<f32>();
    if (j.contains("collide")) c.collide = JB(j["collide"]);
    if (j.contains("collisionSkin")) c.collisionSkin = j["collisionSkin"].get<f32>();
    if (j.contains("friction")) c.friction = j["friction"].get<f32>();
    if (j.contains("selfCollide")) c.selfCollide = JB(j["selfCollide"]);
    if (j.contains("thickness")) c.thickness = j["thickness"].get<f32>();
    if (j.contains("pinStrength")) c.pinStrength = j["pinStrength"].get<f32>();
    if (j.contains("seams")) { u32 v = j["seams"].get<u32>(); if (v <= 3) c.seams = static_cast<ECS::ClothSeams>(v); }
    if (j.contains("seamSpacing")) c.seamSpacing = std::clamp(j["seamSpacing"].get<i32>(), 2, 64);
    if (j.contains("seamStrength")) c.seamStrength = j["seamStrength"].get<f32>();
    return c;
}

json SerializeSwarmComponent(const ECS::SwarmComponent& s) {
    json j;
    j["count"] = s.count;
    j["renderCap"] = s.renderCap;
    j["spawnRadius"] = RF(s.spawnRadius);
    j["maxSpeed"] = RF(s.maxSpeed);
    j["pull"] = RF(s.pull);
    j["damping"] = RF(s.damping);
    j["cubeSize"] = RF(s.cubeSize);
    j["color"] = SerializeVector3(s.color);
    return j;
}

ECS::SwarmComponent DeserializeSwarmComponent(const json& j) {
    ECS::SwarmComponent s;
    if (j.contains("count")) s.count = std::clamp(j["count"].get<u32>(), 1u, 200000u);
    if (j.contains("renderCap")) s.renderCap = std::clamp(j["renderCap"].get<u32>(), 1u, 200000u);
    if (j.contains("spawnRadius")) s.spawnRadius = j["spawnRadius"].get<f32>();
    if (j.contains("maxSpeed")) s.maxSpeed = j["maxSpeed"].get<f32>();
    if (j.contains("pull")) s.pull = j["pull"].get<f32>();
    if (j.contains("damping")) s.damping = j["damping"].get<f32>();
    if (j.contains("cubeSize")) s.cubeSize = j["cubeSize"].get<f32>();
    if (j.contains("color")) s.color = DeserializeVector3(j["color"]);
    return s;
}

json SerializeDungeonGeneratorComponent(const ECS::DungeonGeneratorComponent& d) {
    json j;
    j["algorithm"] = static_cast<u8>(d.algorithm);
    j["width"] = d.width; j["height"] = d.height; j["seed"] = d.seed;
    j["walkSteps"] = d.walkSteps; j["walkTurnChance"] = RF(d.walkTurnChance);
    j["fillPercent"] = d.fillPercent; j["iterations"] = d.iterations;
    j["minRoomSize"] = d.minRoomSize; j["maxRoomSize"] = d.maxRoomSize;
    j["splitDepth"] = d.splitDepth; j["corridorWidth"] = d.corridorWidth;
    j["floorTile"] = d.floorTile; j["wallTile"] = d.wallTile;
    j["fillCollision"] = d.fillCollision; j["generateOnStart"] = d.generateOnStart;
    return j;
}

ECS::DungeonGeneratorComponent DeserializeDungeonGeneratorComponent(const json& j) {
    ECS::DungeonGeneratorComponent d;
    if (j.contains("algorithm")) { u8 v = j["algorithm"].get<u8>(); if (v <= 2) d.algorithm = static_cast<ECS::DungeonGeneratorComponent::Algorithm>(v); }
    if (j.contains("width")) d.width = std::clamp(j["width"].get<u32>(), 4u, 512u);
    if (j.contains("height")) d.height = std::clamp(j["height"].get<u32>(), 4u, 512u);
    if (j.contains("seed")) d.seed = j["seed"].get<u32>();
    if (j.contains("walkSteps")) d.walkSteps = j["walkSteps"].get<u32>();
    if (j.contains("walkTurnChance")) d.walkTurnChance = j["walkTurnChance"].get<f32>();
    if (j.contains("fillPercent")) d.fillPercent = std::clamp(j["fillPercent"].get<u32>(), 0u, 100u);
    if (j.contains("iterations")) d.iterations = j["iterations"].get<u32>();
    if (j.contains("minRoomSize")) d.minRoomSize = j["minRoomSize"].get<u32>();
    if (j.contains("maxRoomSize")) d.maxRoomSize = j["maxRoomSize"].get<u32>();
    if (j.contains("splitDepth")) d.splitDepth = j["splitDepth"].get<u32>();
    if (j.contains("corridorWidth")) d.corridorWidth = j["corridorWidth"].get<u32>();
    if (j.contains("floorTile")) d.floorTile = j["floorTile"].get<i32>();
    if (j.contains("wallTile")) d.wallTile = j["wallTile"].get<i32>();
    if (j.contains("fillCollision")) d.fillCollision = JB(j["fillCollision"]);
    if (j.contains("generateOnStart")) d.generateOnStart = JB(j["generateOnStart"]);
    return d;
}

json SerializeRandomBagComponent(const ECS::RandomBagComponent& b) {
    json j;
    j["mode"] = static_cast<u8>(b.mode);
    j["seed"] = b.seed;
    j["avoidImmediateRepeat"] = b.avoidImmediateRepeat;
    if (!b.transitions.empty()) {
        json ta = json::array();
        for (f32 w : b.transitions) ta.push_back(RF(w));
        j["transitions"] = ta;
    }
    json items = json::array();
    for (const auto& it : b.items) {
        json ij;
        ij["name"] = it.name;
        ij["weight"] = RF(it.weight);
        items.push_back(ij);
    }
    j["items"] = items;
    return j;
}

ECS::RandomBagComponent DeserializeRandomBagComponent(const json& j) {
    ECS::RandomBagComponent b;
    if (j.contains("mode")) { u8 v = j["mode"].get<u8>(); if (v <= 4) b.mode = static_cast<ECS::RandomBagComponent::Mode>(v); }
    if (j.contains("transitions") && j["transitions"].is_array()) {
        const auto& ta = j["transitions"];
        usize n = ta.size() > 4096 ? 4096 : ta.size();   // 64x64 matrix cap
        b.transitions.reserve(n);
        for (usize i = 0; i < n; ++i) b.transitions.push_back(ta[i].is_number() ? ta[i].get<f32>() : 1.0f);
    }
    if (j.contains("seed")) b.seed = j["seed"].get<u32>();
    if (j.contains("avoidImmediateRepeat")) b.avoidImmediateRepeat = JB(j["avoidImmediateRepeat"]);
    if (j.contains("items") && j["items"].is_array()) {
        const auto& arr = j["items"];
        usize n = arr.size() > 100000 ? 100000 : arr.size(); // sanity cap
        for (usize i = 0; i < n; ++i) {
            const auto& ij = arr[i];
            ECS::RandomBagComponent::Item item;
            if (ij.contains("name") && ij["name"].is_string()) item.name = SafeStr(ij["name"]);
            if (ij.contains("weight")) item.weight = ij["weight"].get<f32>();
            b.items.push_back(std::move(item));
        }
    }
    return b;
}

json SerializeWFCComponent(const ECS::WFCComponent& g) {
    json j;
    j["mode"] = static_cast<u8>(g.mode);
    j["width"] = g.width; j["height"] = g.height; j["depth"] = g.depth;
    j["cellSize"] = RF(g.cellSize);
    j["seed"] = g.seed; j["maxBacktracks"] = g.maxBacktracks;
    j["retryOnFail"] = g.retryOnFail; j["maxRetries"] = g.maxRetries;
    j["fillCollision"] = g.fillCollision; j["generateOnStart"] = g.generateOnStart;
    json tiles = json::array();
    for (const auto& t : g.tiles) {
        json tj;
        tj["tileIndex"] = t.tileIndex;
        tj["prefabPath"] = t.prefabPath;
        tj["solid"] = t.solid;
        tj["edges"] = json::array({ t.edges[0], t.edges[1], t.edges[2],
                                    t.edges[3], t.edges[4], t.edges[5] });
        tiles.push_back(tj);
    }
    j["tiles"] = tiles;
    return j;
}

ECS::WFCComponent DeserializeWFCComponent(const json& j) {
    ECS::WFCComponent g;
    if (j.contains("mode")) { u8 v = j["mode"].get<u8>(); if (v <= 1) g.mode = static_cast<ECS::WFCComponent::Mode>(v); }
    if (j.contains("width")) g.width = std::clamp(j["width"].get<u32>(), 1u, 1024u);
    if (j.contains("height")) g.height = std::clamp(j["height"].get<u32>(), 1u, 1024u);
    if (j.contains("depth")) g.depth = std::clamp(j["depth"].get<u32>(), 1u, 64u);
    if (j.contains("cellSize")) g.cellSize = j["cellSize"].get<f32>();
    if (j.contains("seed")) g.seed = j["seed"].get<u32>();
    if (j.contains("maxBacktracks")) g.maxBacktracks = j["maxBacktracks"].get<u32>();
    if (j.contains("retryOnFail")) g.retryOnFail = JB(j["retryOnFail"]);
    if (j.contains("maxRetries")) g.maxRetries = std::clamp(j["maxRetries"].get<u32>(), 1u, 1000u);
    if (j.contains("fillCollision")) g.fillCollision = JB(j["fillCollision"]);
    if (j.contains("generateOnStart")) g.generateOnStart = JB(j["generateOnStart"]);
    if (j.contains("tiles") && j["tiles"].is_array()) {
        const auto& arr = j["tiles"];
        usize n = arr.size() > 4096 ? 4096 : arr.size(); // sanity cap
        for (usize i = 0; i < n; ++i) {
            const auto& tj = arr[i];
            ECS::WFCComponent::Tile t;
            if (tj.contains("tileIndex")) t.tileIndex = tj["tileIndex"].get<i32>();
            if (tj.contains("prefabPath") && tj["prefabPath"].is_string()) t.prefabPath = SafeStr(tj["prefabPath"]);
            if (tj.contains("solid")) t.solid = JB(tj["solid"]);
            if (tj.contains("edges") && tj["edges"].is_array()) {
                const auto& ea = tj["edges"];
                for (u32 e = 0; e < 6 && e < ea.size(); ++e)
                    if (ea[e].is_string()) t.edges[e] = SafeStr(ea[e]);
            }
            g.tiles.push_back(std::move(t));
        }
    }
    return g;
}

json SerializeGaussianSplatComponent(const ECS::GaussianSplatComponent& g) {
    json j;
    j["sourcePath"] = g.sourcePath;
    j["opacityScale"] = RF(g.opacityScale);
    j["splatScale"] = RF(g.splatScale);
    j["flipYZ"] = g.flipYZ;
    j["maxSplats"] = g.maxSplats;
    j["visible"] = g.visible;
    return j;
}

ECS::GaussianSplatComponent DeserializeGaussianSplatComponent(const json& j) {
    ECS::GaussianSplatComponent g;
    if (j.contains("sourcePath")) g.sourcePath = SafeStr(j["sourcePath"], MAX_STR_PATH);
    if (j.contains("opacityScale")) g.opacityScale = Math::Clamp(j["opacityScale"].get<f32>(), 0.0f, 4.0f);
    if (j.contains("splatScale")) g.splatScale = Math::Clamp(j["splatScale"].get<f32>(), 0.01f, 16.0f);
    if (j.contains("flipYZ")) g.flipYZ = JB(j["flipYZ"]);
    if (j.contains("maxSplats")) g.maxSplats = std::min(j["maxSplats"].get<u32>(), 8000000u);
    if (j.contains("visible")) g.visible = JB(j["visible"]);
    g.dirty = true;   // freshly loaded component always (re)loads its file
    return g;
}

json SerializeTerrainGeneratorComponent(const ECS::TerrainGeneratorComponent& t) {
    json j;
    j["gridWidth"] = t.gridWidth; j["gridHeight"] = t.gridHeight;
    j["cellSize"] = RF(t.cellSize); j["maxHeight"] = RF(t.maxHeight);
    j["ridged"] = t.ridged;
    j["octaves"] = t.octaves; j["lacunarity"] = RF(t.lacunarity);
    j["gain"] = RF(t.gain); j["frequency"] = RF(t.frequency);
    j["ridgedPower"] = RF(t.ridgedPower);
    j["hydraulic"] = t.hydraulic; j["hydraulicDroplets"] = t.hydraulicDroplets;
    j["thermal"] = t.thermal; j["thermalIterations"] = t.thermalIterations;
    j["talusAngle"] = RF(t.talusAngle);
    j["autoSplat"] = t.autoSplat; j["rockSlopeDeg"] = RF(t.rockSlopeDeg);
    j["snowHeightFrac"] = RF(t.snowHeightFrac); j["shoreHeightFrac"] = RF(t.shoreHeightFrac);
    j["splatBlend"] = RF(t.splatBlend);
    j["seed"] = t.seed; j["generateOnStart"] = t.generateOnStart;
    return j;
}

ECS::TerrainGeneratorComponent DeserializeTerrainGeneratorComponent(const json& j) {
    ECS::TerrainGeneratorComponent t;
    if (j.contains("gridWidth")) t.gridWidth = std::clamp(j["gridWidth"].get<u32>(), 4u, 1024u);
    if (j.contains("gridHeight")) t.gridHeight = std::clamp(j["gridHeight"].get<u32>(), 4u, 1024u);
    if (j.contains("cellSize")) t.cellSize = j["cellSize"].get<f32>();
    if (j.contains("maxHeight")) t.maxHeight = j["maxHeight"].get<f32>();
    if (j.contains("ridged")) t.ridged = JB(j["ridged"]);
    if (j.contains("octaves")) t.octaves = std::clamp(j["octaves"].get<u32>(), 1u, 12u);
    if (j.contains("lacunarity")) t.lacunarity = j["lacunarity"].get<f32>();
    if (j.contains("gain")) t.gain = j["gain"].get<f32>();
    if (j.contains("frequency")) t.frequency = j["frequency"].get<f32>();
    if (j.contains("ridgedPower")) t.ridgedPower = j["ridgedPower"].get<f32>();
    if (j.contains("hydraulic")) t.hydraulic = JB(j["hydraulic"]);
    if (j.contains("hydraulicDroplets")) t.hydraulicDroplets = std::clamp(j["hydraulicDroplets"].get<u32>(), 1000u, 500000u);
    if (j.contains("thermal")) t.thermal = JB(j["thermal"]);
    if (j.contains("thermalIterations")) t.thermalIterations = std::clamp(j["thermalIterations"].get<u32>(), 1u, 1000u);
    if (j.contains("talusAngle")) t.talusAngle = j["talusAngle"].get<f32>();
    if (j.contains("autoSplat")) t.autoSplat = JB(j["autoSplat"]);
    if (j.contains("rockSlopeDeg")) t.rockSlopeDeg = std::clamp(j["rockSlopeDeg"].get<f32>(), 0.0f, 89.0f);
    if (j.contains("snowHeightFrac")) t.snowHeightFrac = std::clamp(j["snowHeightFrac"].get<f32>(), 0.0f, 1.0f);
    if (j.contains("shoreHeightFrac")) t.shoreHeightFrac = std::clamp(j["shoreHeightFrac"].get<f32>(), 0.0f, 1.0f);
    if (j.contains("splatBlend")) t.splatBlend = std::clamp(j["splatBlend"].get<f32>(), 0.0f, 0.5f);
    if (j.contains("seed")) t.seed = j["seed"].get<u32>();
    if (j.contains("generateOnStart")) t.generateOnStart = JB(j["generateOnStart"]);
    return t;
}

json SerializeScatterComponent(const ECS::ScatterComponent& s) {
    json j;
    j["distribution"] = static_cast<u8>(s.distribution);
    j["plane"] = static_cast<u8>(s.plane);
    j["prefabPath"] = s.prefabPath;
    j["regionWidth"] = RF(s.regionWidth);
    j["regionHeight"] = RF(s.regionHeight);
    j["targetCount"] = s.targetCount;
    j["minSpacing"] = RF(s.minSpacing);
    j["relaxIterations"] = s.relaxIterations;
    j["scaleMin"] = RF(s.scaleMin);
    j["scaleMax"] = RF(s.scaleMax);
    j["randomYaw"] = s.randomYaw;
    j["heightJitter"] = RF(s.heightJitter);
    j["conformToTerrain"] = s.conformToTerrain;
    j["maxSlopeDeg"] = RF(s.maxSlopeDeg);
    j["seed"] = s.seed;
    j["generateOnStart"] = s.generateOnStart;
    return j;
}

ECS::ScatterComponent DeserializeScatterComponent(const json& j) {
    ECS::ScatterComponent s;
    if (j.contains("distribution")) { u8 v = j["distribution"].get<u8>(); if (v <= 3) s.distribution = static_cast<ECS::ScatterComponent::Distribution>(v); }
    if (j.contains("plane")) { u8 v = j["plane"].get<u8>(); if (v <= 1) s.plane = static_cast<ECS::ScatterComponent::Plane>(v); }
    if (j.contains("prefabPath") && j["prefabPath"].is_string()) s.prefabPath = SafeStr(j["prefabPath"]);
    if (j.contains("regionWidth")) s.regionWidth = std::clamp(j["regionWidth"].get<f32>(), 0.0f, 100000.0f);
    if (j.contains("regionHeight")) s.regionHeight = std::clamp(j["regionHeight"].get<f32>(), 0.0f, 100000.0f);
    if (j.contains("targetCount")) s.targetCount = std::clamp(j["targetCount"].get<u32>(), 0u, 20000u);
    if (j.contains("minSpacing")) s.minSpacing = std::clamp(j["minSpacing"].get<f32>(), 0.05f, 100000.0f);
    if (j.contains("relaxIterations")) s.relaxIterations = std::clamp(j["relaxIterations"].get<u32>(), 0u, 16u);
    if (j.contains("scaleMin")) s.scaleMin = j["scaleMin"].get<f32>();
    if (j.contains("scaleMax")) s.scaleMax = j["scaleMax"].get<f32>();
    if (j.contains("randomYaw")) s.randomYaw = JB(j["randomYaw"]);
    if (j.contains("heightJitter")) s.heightJitter = j["heightJitter"].get<f32>();
    if (j.contains("conformToTerrain")) s.conformToTerrain = JB(j["conformToTerrain"]);
    if (j.contains("maxSlopeDeg")) s.maxSlopeDeg = std::clamp(j["maxSlopeDeg"].get<f32>(), 0.0f, 90.0f);
    if (j.contains("seed")) s.seed = j["seed"].get<u32>();
    if (j.contains("generateOnStart")) s.generateOnStart = JB(j["generateOnStart"]);
    return s;
}

json SerializeElementalVolumeComponent(const ECS::ElementalVolumeComponent& v) {
    json j;
    j["halfExtents"] = SerializeVector3(v.halfExtents);
    j["elementBias"] = SerializeVector4(v.elementBias);
    j["temperatureBias"] = RF(v.temperatureBias);
    j["windMultiplier"] = RF(v.windMultiplier);
    j["killOnContact"] = v.killOnContact;
    return j;
}

ECS::ElementalVolumeComponent DeserializeElementalVolumeComponent(const json& j) {
    ECS::ElementalVolumeComponent v;
    if (j.contains("halfExtents")) v.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("elementBias")) v.elementBias = DeserializeVector4(j["elementBias"]);
    if (j.contains("temperatureBias")) v.temperatureBias = j["temperatureBias"].get<f32>();
    if (j.contains("windMultiplier")) v.windMultiplier = j["windMultiplier"].get<f32>();
    if (j.contains("killOnContact")) v.killOnContact = JB(j["killOnContact"]);
    return v;
}

json SerializeCineComponent(const ECS::CineComponent& c) {
    json j;
    j["enabled"] = c.enabled;
    j["directorStyle"] = static_cast<int>(c.directorStyle);
    j["rigArchetype"] = static_cast<int>(c.rigArchetype);
    j["focalLengthMm"] = RF(c.focalLengthMm);
    j["apertureTStop"] = RF(c.apertureTStop);
    j["focusDistanceMeters"] = RF(c.focusDistanceMeters);
    j["squeezeRatio"] = RF(c.squeezeRatio);
    j["keyIntensityEv"] = RF(c.keyIntensityEv);
    j["keyToFillRatio"] = RF(c.keyToFillRatio);
    j["keyToRimRatio"] = RF(c.keyToRimRatio);
    j["frequency"] = RF(c.frequency);
    j["dampingRatio"] = RF(c.dampingRatio);
    j["initialResponse"] = RF(c.initialResponse);
    j["targetSubjectEntityId"] = c.targetSubjectEntityId;
    j["framingOffset"] = SerializeVector3(c.framingOffset);
    return j;
}

ECS::CineComponent DeserializeCineComponent(const json& j) {
    ECS::CineComponent c;
    if (j.contains("enabled")) c.enabled = JB(j["enabled"]);
    if (j.contains("directorStyle")) c.directorStyle = static_cast<ECS::CineDirectorStyle>(j["directorStyle"].get<int>());
    if (j.contains("rigArchetype")) c.rigArchetype = static_cast<ECS::CineRigArchetype>(j["rigArchetype"].get<int>());
    if (j.contains("focalLengthMm")) c.focalLengthMm = j["focalLengthMm"].get<f32>();
    if (j.contains("apertureTStop")) c.apertureTStop = j["apertureTStop"].get<f32>();
    if (j.contains("focusDistanceMeters")) c.focusDistanceMeters = j["focusDistanceMeters"].get<f32>();
    if (j.contains("squeezeRatio")) c.squeezeRatio = j["squeezeRatio"].get<f32>();
    if (j.contains("keyIntensityEv")) c.keyIntensityEv = j["keyIntensityEv"].get<f32>();
    if (j.contains("keyToFillRatio")) c.keyToFillRatio = j["keyToFillRatio"].get<f32>();
    if (j.contains("keyToRimRatio")) c.keyToRimRatio = j["keyToRimRatio"].get<f32>();
    if (j.contains("frequency")) c.frequency = j["frequency"].get<f32>();
    if (j.contains("dampingRatio")) c.dampingRatio = j["dampingRatio"].get<f32>();
    if (j.contains("initialResponse")) c.initialResponse = j["initialResponse"].get<f32>();
    if (j.contains("targetSubjectEntityId")) c.targetSubjectEntityId = j["targetSubjectEntityId"].get<u64>();
    if (j.contains("framingOffset")) c.framingOffset = DeserializeVector3(j["framingOffset"]);
    return c;
}

// Serialize PostProcessSettings (GPU-aligned UBO struct) to JSON
json SerializePPSettings(const Renderer::PostProcessSettings& s) {
    json j;
    j["toneMappingMode"] = s.toneMappingMode;
    j["exposure"] = RF(s.exposure);
    j["gamma"] = RF(s.gamma);
    j["whitePoint"] = RF(s.whitePoint);
    j["bloomEnabled"] = s.bloomEnabled;
    j["bloomThreshold"] = RF(s.bloomThreshold);
    j["bloomIntensity"] = RF(s.bloomIntensity);
    j["bloomRadius"] = RF(s.bloomRadius);
    j["vignetteEnabled"] = s.vignetteEnabled;
    j["vignetteIntensity"] = RF(s.vignetteIntensity);
    j["vignetteSmoothness"] = RF(s.vignetteSmoothness);
    j["chromaticAberrationEnabled"] = s.chromaticAberrationEnabled;
    j["chromaticAberrationIntensity"] = RF(s.chromaticAberrationIntensity);
    j["colorFilter"] = SerializeVector3(s.colorFilter);
    j["saturation"] = RF(s.saturation);
    j["contrast"] = RF(s.contrast);
    j["brightness"] = RF(s.brightness);
    j["colorblindMode"] = s.colorblindMode;
    j["colorblindStrength"] = RF(s.colorblindStrength);
    j["filmGrainEnabled"] = s.filmGrainEnabled;
    j["filmGrainIntensity"] = RF(s.filmGrainIntensity);
    j["fxaaEnabled"] = s.fxaaEnabled;
    j["fxaaSpanMax"] = RF(s.fxaaSpanMax);
    j["fxaaReduceMin"] = RF(s.fxaaReduceMin);
    j["fxaaReduceMul"] = RF(s.fxaaReduceMul);
    j["ditherEnabled"] = s.ditherEnabled;
    j["ditherPattern"] = s.ditherPattern;
    j["ditherStrength"] = RF(s.ditherStrength);
    j["colorQuantEnabled"] = s.colorQuantEnabled;
    j["colorBitDepth"] = s.colorBitDepth;
    j["resDownscaleEnabled"] = s.resDownscaleEnabled;
    j["internalWidth"] = s.internalWidth;
    j["internalHeight"] = s.internalHeight;
    j["usePointFiltering"] = s.usePointFiltering;
    j["crtEnabled"] = s.crtEnabled;
    j["scanlineIntensity"] = RF(s.scanlineIntensity);
    j["scanlineWidth"] = RF(s.scanlineWidth);
    j["crtCurvature"] = RF(s.crtCurvature);
    j["lutEnabled"] = s.lutEnabled;
    j["lutStrength"] = RF(s.lutStrength);
    j["lutSize"] = s.lutSize;
    j["crtPhosphorEnabled"] = s.crtPhosphorEnabled;
    j["crtMaskType"] = s.crtMaskType;
    j["crtMaskPitch"] = RF(s.crtMaskPitch);
    j["crtBloomRadius"] = RF(s.crtBloomRadius);
    j["crtBloomStrength"] = RF(s.crtBloomStrength);
    j["vhsEnabled"] = s.vhsEnabled;
    j["vhsTrackingIntensity"] = RF(s.vhsTrackingIntensity);
    j["vhsTrackingSpeed"] = RF(s.vhsTrackingSpeed);
    j["vhsWobbleIntensity"] = RF(s.vhsWobbleIntensity);
    j["vhsWobbleSpeed"] = RF(s.vhsWobbleSpeed);
    j["vhsColorBleed"] = RF(s.vhsColorBleed);
    j["vhsNoiseIntensity"] = RF(s.vhsNoiseIntensity);
    j["vhsBlueShift"] = RF(s.vhsBlueShift);
    j["vhsScreenTear"] = s.vhsScreenTear;
    j["vhsInterlacing"] = s.vhsInterlacing;
    j["paletteEnabled"] = s.paletteEnabled;
    j["paletteColors"] = s.paletteColors;
    j["dofEnabled"] = s.dofEnabled;
    j["dofFocalDistance"] = RF(s.dofFocalDistance);
    j["dofFocalRange"] = RF(s.dofFocalRange);
    j["dofNearBlurStrength"] = RF(s.dofNearBlurStrength);
    j["dofFarBlurStrength"] = RF(s.dofFarBlurStrength);
    j["dofBokehSize"] = RF(s.dofBokehSize);
    j["dofApertureShape"] = s.dofApertureShape;
    j["dofDebugCoC"] = s.dofDebugCoC;
    j["tiltShiftEnabled"] = s.tiltShiftEnabled;
    j["tiltShiftFocusY"] = RF(s.tiltShiftFocusY);
    j["tiltShiftBandWidth"] = RF(s.tiltShiftBandWidth);
    j["tiltShiftBlurAmount"] = RF(s.tiltShiftBlurAmount);
    j["celOutlineEnabled"] = s.celOutlineEnabled;
    j["celOutlineThickness"] = RF(s.celOutlineThickness);
    j["celOutlineThreshold"] = RF(s.celOutlineThreshold);
    j["celOutlineCurvatureWeight"] = RF(s.celOutlineCurvatureWeight);
    j["celOutlineColor"] = SerializeVector3(s.celOutlineColor);
    j["stippleEnabled"] = s.stippleEnabled;
    j["stipplePatternMask"] = s.stipplePatternMask;
    j["stippleColorMode"] = s.stippleColorMode;
    j["stippleScale"] = RF(s.stippleScale);
    j["stippleDensity"] = RF(s.stippleDensity);
    j["stippleStrength"] = RF(s.stippleStrength);
    j["stippleFgColor"] = SerializeVector3(s.stippleFgColor);
    j["stippleBgColor"] = SerializeVector3(s.stippleBgColor);
    // Screen-Space Effects
    j["godRaysEnabled"] = s.godRaysEnabled;
    j["godRaysIntensity"] = RF(s.godRaysIntensity);
    j["godRaysDecay"] = RF(s.godRaysDecay);
    j["godRaysDensity"] = RF(s.godRaysDensity);
    j["godRaysSamples"] = s.godRaysSamples;
    j["godRaysWeight"] = RF(s.godRaysWeight);
    j["ssaoEnabled"] = s.ssaoEnabled;
    j["ssaoRadius"] = RF(s.ssaoRadius);
    j["ssaoIntensity"] = RF(s.ssaoIntensity);
    j["ssaoBias"] = RF(s.ssaoBias);
    j["ssaoSamples"] = s.ssaoSamples;
    j["ssrEnabled"] = s.ssrEnabled;
    j["ssrIntensity"] = RF(s.ssrIntensity);
    j["ssrMaxDistance"] = RF(s.ssrMaxDistance);
    j["ssrMaxSteps"] = s.ssrMaxSteps;
    j["ssrThickness"] = RF(s.ssrThickness);
    j["ssrEdgeFade"] = RF(s.ssrEdgeFade);
    j["contactShadowsEnabled"] = s.contactShadowsEnabled;
    j["contactShadowsLength"] = RF(s.contactShadowsLength);
    j["contactShadowsSteps"] = s.contactShadowsSteps;
    j["contactShadowsIntensity"] = RF(s.contactShadowsIntensity);
    j["causticsEnabled"] = s.causticsEnabled;
    j["causticsIntensity"] = RF(s.causticsIntensity);
    j["causticsScale"] = RF(s.causticsScale);
    j["causticsSpeed"] = RF(s.causticsSpeed);
    j["causticsWaterY"] = RF(s.causticsWaterY);
    j["fogShaftsEnabled"] = s.fogShaftsEnabled;
    j["fogShaftsIntensity"] = RF(s.fogShaftsIntensity);
    j["fogShaftsDensity"] = RF(s.fogShaftsDensity);
    j["fogShaftsDecay"] = RF(s.fogShaftsDecay);
    j["fogShaftsSamples"] = s.fogShaftsSamples;
    j["fogShaftsMaxDistance"] = RF(s.fogShaftsMaxDistance);
    return j;
}

Renderer::PostProcessSettings DeserializePPSettings(const json& j) {
    Renderer::PostProcessSettings s;
    auto GU = [&](const char* k) -> u32 { return j.contains(k) ? j[k].get<u32>() : 0; };
    auto GF = [&](const char* k, f32 def) -> f32 { return j.contains(k) ? j[k].get<f32>() : def; };

    // R4 fix: Clamp enum fields to valid ranges
    s.toneMappingMode = std::min(GU("toneMappingMode"), 5u); // 0-5 (None..AgX)
    // Migration: scenes saved with None (0) → ACES (3). None was an unintentional default.
    if (s.toneMappingMode == 0) s.toneMappingMode = 3;
    s.exposure = GF("exposure", 1.0f);
    s.gamma = GF("gamma", 1.0f);
    s.whitePoint = GF("whitePoint", 4.0f);
    s.bloomEnabled = GU("bloomEnabled");
    s.bloomThreshold = GF("bloomThreshold", 1.0f);
    s.bloomIntensity = GF("bloomIntensity", 0.5f);
    s.bloomRadius = GF("bloomRadius", 0.005f);
    s.vignetteEnabled = GU("vignetteEnabled");
    s.vignetteIntensity = GF("vignetteIntensity", 0.3f);
    s.vignetteSmoothness = GF("vignetteSmoothness", 0.5f);
    s.chromaticAberrationEnabled = GU("chromaticAberrationEnabled");
    s.chromaticAberrationIntensity = GF("chromaticAberrationIntensity", 0.005f);
    if (j.contains("colorFilter")) s.colorFilter = DeserializeVector3(j["colorFilter"]);
    s.saturation = GF("saturation", 1.0f);
    s.contrast = GF("contrast", 1.0f);
    s.brightness = GF("brightness", 0.0f);
    s.colorblindMode = std::min(GU("colorblindMode"), 7u); // 0-7 (off..achromatopsia)
    s.colorblindStrength = GF("colorblindStrength", 1.0f);
    s.filmGrainEnabled = GU("filmGrainEnabled");
    s.filmGrainIntensity = GF("filmGrainIntensity", 0.05f);
    s.fxaaEnabled = GU("fxaaEnabled");
    s.fxaaSpanMax = GF("fxaaSpanMax", 8.0f);
    s.fxaaReduceMin = GF("fxaaReduceMin", 1.0f / 128.0f);
    s.fxaaReduceMul = GF("fxaaReduceMul", 1.0f / 8.0f);
    s.ditherEnabled = GU("ditherEnabled");
    s.ditherPattern = std::min(GU("ditherPattern"), 2u);
    s.ditherStrength = GF("ditherStrength", 1.0f);
    s.colorQuantEnabled = GU("colorQuantEnabled");
    s.colorBitDepth = GU("colorBitDepth");
    if (s.colorBitDepth == 0) s.colorBitDepth = 8;
    s.resDownscaleEnabled = GU("resDownscaleEnabled");
    s.internalWidth = GU("internalWidth");
    if (s.internalWidth == 0) s.internalWidth = 320;
    s.internalHeight = GU("internalHeight");
    if (s.internalHeight == 0) s.internalHeight = 240;
    s.usePointFiltering = GU("usePointFiltering");
    s.crtEnabled = GU("crtEnabled");
    s.scanlineIntensity = GF("scanlineIntensity", 0.3f);
    s.scanlineWidth = GF("scanlineWidth", 1.0f);
    s.crtCurvature = GF("crtCurvature", 0.0f);
    s.lutEnabled = GU("lutEnabled");
    s.lutStrength = GF("lutStrength", 1.0f);
    s.lutSize = GU("lutSize");
    if (s.lutSize == 0) s.lutSize = 32;
    s.crtPhosphorEnabled = GU("crtPhosphorEnabled");
    s.crtMaskType = std::min(GU("crtMaskType"), 2u);
    s.crtMaskPitch = GF("crtMaskPitch", 1.0f);
    s.crtBloomRadius = GF("crtBloomRadius", 1.5f);
    s.crtBloomStrength = GF("crtBloomStrength", 0.3f);
    s.vhsEnabled = GU("vhsEnabled");
    s.vhsTrackingIntensity = GF("vhsTrackingIntensity", 0.3f);
    s.vhsTrackingSpeed = GF("vhsTrackingSpeed", 1.0f);
    s.vhsWobbleIntensity = GF("vhsWobbleIntensity", 0.002f);
    s.vhsWobbleSpeed = GF("vhsWobbleSpeed", 2.0f);
    s.vhsColorBleed = GF("vhsColorBleed", 0.003f);
    s.vhsNoiseIntensity = GF("vhsNoiseIntensity", 0.05f);
    s.vhsBlueShift = GF("vhsBlueShift", 0.05f);
    s.vhsScreenTear = GU("vhsScreenTear");
    s.vhsInterlacing = GU("vhsInterlacing");
    s.paletteEnabled = GU("paletteEnabled");
    s.paletteColors = GU("paletteColors");
    if (s.paletteColors == 0) s.paletteColors = 16;
    s.dofEnabled = GU("dofEnabled");
    s.dofFocalDistance = GF("dofFocalDistance", 10.0f);
    s.dofFocalRange = GF("dofFocalRange", 5.0f);
    s.dofNearBlurStrength = GF("dofNearBlurStrength", 1.0f);
    s.dofFarBlurStrength = GF("dofFarBlurStrength", 1.0f);
    s.dofBokehSize = GF("dofBokehSize", 4.0f);
    s.dofApertureShape = std::min(GU("dofApertureShape"), 2u);
    s.dofDebugCoC = GU("dofDebugCoC");
    s.tiltShiftEnabled = GU("tiltShiftEnabled");
    s.tiltShiftFocusY = GF("tiltShiftFocusY", 0.5f);
    s.tiltShiftBandWidth = GF("tiltShiftBandWidth", 0.3f);
    s.tiltShiftBlurAmount = GF("tiltShiftBlurAmount", 3.0f);
    s.celOutlineEnabled = GU("celOutlineEnabled");
    s.celOutlineThickness = GF("celOutlineThickness", 1.0f);
    s.celOutlineThreshold = GF("celOutlineThreshold", 0.1f);
    s.celOutlineCurvatureWeight = GF("celOutlineCurvatureWeight", 0.0f);
    if (j.contains("celOutlineColor")) s.celOutlineColor = DeserializeVector3(j["celOutlineColor"]);
    s.stippleEnabled = GU("stippleEnabled");
    s.stipplePatternMask = GU("stipplePatternMask");
    if (s.stipplePatternMask == 0) s.stipplePatternMask = 1;
    s.stippleColorMode = std::min(GU("stippleColorMode"), 2u);
    s.stippleScale = GF("stippleScale", 1.0f);
    s.stippleDensity = GF("stippleDensity", 0.5f);
    s.stippleStrength = GF("stippleStrength", 1.0f);
    if (j.contains("stippleFgColor")) s.stippleFgColor = DeserializeVector3(j["stippleFgColor"]);
    if (j.contains("stippleBgColor")) s.stippleBgColor = DeserializeVector3(j["stippleBgColor"]);
    // Screen-Space Effects
    s.godRaysEnabled = GU("godRaysEnabled");
    s.godRaysIntensity = GF("godRaysIntensity", 0.5f);
    s.godRaysDecay = GF("godRaysDecay", 0.97f);
    s.godRaysDensity = GF("godRaysDensity", 1.0f);
    s.godRaysSamples = GU("godRaysSamples");
    if (s.godRaysSamples == 0) s.godRaysSamples = 64;
    s.godRaysWeight = GF("godRaysWeight", 0.01f);
    s.ssaoEnabled = GU("ssaoEnabled");
    s.ssaoRadius = GF("ssaoRadius", 0.5f);
    s.ssaoIntensity = GF("ssaoIntensity", 1.5f);
    s.ssaoBias = GF("ssaoBias", 0.025f);
    s.ssaoSamples = GU("ssaoSamples");
    if (s.ssaoSamples == 0) s.ssaoSamples = 16;
    s.ssrEnabled = GU("ssrEnabled");
    s.ssrIntensity = GF("ssrIntensity", 0.6f);
    s.ssrMaxDistance = GF("ssrMaxDistance", 30.0f);
    s.ssrMaxSteps = GU("ssrMaxSteps");
    if (s.ssrMaxSteps == 0) s.ssrMaxSteps = 32;
    s.ssrThickness = GF("ssrThickness", 0.6f);
    s.ssrEdgeFade = GF("ssrEdgeFade", 0.1f);
    s.contactShadowsEnabled = GU("contactShadowsEnabled");
    s.contactShadowsLength = GF("contactShadowsLength", 0.1f);
    s.contactShadowsSteps = GU("contactShadowsSteps");
    if (s.contactShadowsSteps == 0) s.contactShadowsSteps = 16;
    s.contactShadowsIntensity = GF("contactShadowsIntensity", 1.0f);
    s.causticsEnabled = GU("causticsEnabled");
    s.causticsIntensity = GF("causticsIntensity", 0.3f);
    s.causticsScale = GF("causticsScale", 1.0f);
    s.causticsSpeed = GF("causticsSpeed", 1.0f);
    s.causticsWaterY = GF("causticsWaterY", 0.0f);
    s.fogShaftsEnabled = GU("fogShaftsEnabled");
    s.fogShaftsIntensity = GF("fogShaftsIntensity", 0.3f);
    s.fogShaftsDensity = GF("fogShaftsDensity", 0.05f);
    s.fogShaftsDecay = GF("fogShaftsDecay", 0.95f);
    s.fogShaftsSamples = GU("fogShaftsSamples");
    if (s.fogShaftsSamples == 0) s.fogShaftsSamples = 16;
    s.fogShaftsMaxDistance = GF("fogShaftsMaxDistance", 50.0f);
    return s;
}

json SerializePostProcessVolumeComponent(const ECS::PostProcessVolumeComponent& vol) {
    json j;
    j["shape"] = static_cast<u32>(vol.shape);
    j["halfExtents"] = SerializeVector3(vol.halfExtents);
    j["priority"] = vol.priority;
    j["isActive"] = vol.isActive;
    j["isGlobal"] = vol.isGlobal;
    j["blendRadius"] = RF(vol.blendRadius);
    j["weight"] = RF(vol.weight);
    j["overrideMask"] = vol.overrideMask;
    j["settings"] = SerializePPSettings(vol.settings);
    return j;
}

ECS::PostProcessVolumeComponent DeserializePostProcessVolumeComponent(const json& j) {
    ECS::PostProcessVolumeComponent vol;
    if (j.contains("shape")) { u32 v = j["shape"].get<u32>(); if (v <= 1) vol.shape = static_cast<ECS::PPVolumeShape>(v); }
    if (j.contains("halfExtents")) vol.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("priority")) vol.priority = j["priority"].get<i32>();
    if (j.contains("isActive")) vol.isActive = JB(j["isActive"]);
    if (j.contains("isGlobal")) vol.isGlobal = JB(j["isGlobal"]);
    if (j.contains("blendRadius")) vol.blendRadius = j["blendRadius"].get<f32>();
    if (j.contains("weight")) vol.weight = j["weight"].get<f32>();
    if (j.contains("overrideMask")) vol.overrideMask = j["overrideMask"].get<u32>();
    if (j.contains("settings")) vol.settings = DeserializePPSettings(j["settings"]);
    return vol;
}

json SerializeFluidVolumeComponent(const ECS::FluidVolumeComponent& vol) {
    json j;
    j["halfExtents"] = SerializeVector3(vol.halfExtents);
    j["fluidType"] = static_cast<u32>(vol.fluidType);
    j["dimension"] = static_cast<u32>(vol.dimension);
    j["gridSize"] = vol.gridSize;
    j["viscosity"] = RF(vol.viscosity);
    j["diffusion"] = RF(vol.diffusion);
    j["dissipation"] = RF(vol.dissipation);
    j["velocityDissipation"] = RF(vol.velocityDissipation);
    j["solverIterations"] = vol.solverIterations;
    j["buoyancy"] = RF(vol.buoyancy);
    j["fluidColor"] = SerializeVector3(vol.fluidColor);
    j["opacity"] = RF(vol.opacity);
    j["densityThreshold"] = RF(vol.densityThreshold);
    j["renderEnabled"] = RF(vol.renderEnabled);
    j["sourceRadius"] = RF(vol.sourceRadius);
    j["sourceDensity"] = RF(vol.sourceDensity);
    j["sourceVelocityScale"] = RF(vol.sourceVelocityScale);
    j["isActive"] = vol.isActive;
    j["priority"] = vol.priority;
    return j;
}

ECS::FluidVolumeComponent DeserializeFluidVolumeComponent(const json& j) {
    ECS::FluidVolumeComponent vol;
    if (j.contains("halfExtents")) vol.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("fluidType")) { u32 v = j["fluidType"].get<u32>(); if (v <= 4) vol.fluidType = static_cast<ECS::FluidType>(v); }
    if (j.contains("dimension")) { u32 v = j["dimension"].get<u32>(); if (v <= 1) vol.dimension = static_cast<ECS::FluidDimension>(v); }
    if (j.contains("gridSize")) { u32 v = j["gridSize"].get<u32>(); vol.gridSize = std::min(v, 512u); }
    if (j.contains("viscosity")) vol.viscosity = j["viscosity"].get<f32>();
    if (j.contains("diffusion")) vol.diffusion = j["diffusion"].get<f32>();
    if (j.contains("dissipation")) vol.dissipation = j["dissipation"].get<f32>();
    if (j.contains("velocityDissipation")) vol.velocityDissipation = j["velocityDissipation"].get<f32>();
    if (j.contains("solverIterations")) vol.solverIterations = j["solverIterations"].get<i32>();
    if (j.contains("buoyancy")) vol.buoyancy = j["buoyancy"].get<f32>();
    if (j.contains("fluidColor")) vol.fluidColor = DeserializeVector3(j["fluidColor"]);
    if (j.contains("opacity")) vol.opacity = j["opacity"].get<f32>();
    if (j.contains("densityThreshold")) vol.densityThreshold = j["densityThreshold"].get<f32>();
    if (j.contains("renderEnabled")) vol.renderEnabled = JB(j["renderEnabled"]);
    if (j.contains("sourceRadius")) vol.sourceRadius = j["sourceRadius"].get<f32>();
    if (j.contains("sourceDensity")) vol.sourceDensity = j["sourceDensity"].get<f32>();
    if (j.contains("sourceVelocityScale")) vol.sourceVelocityScale = j["sourceVelocityScale"].get<f32>();
    if (j.contains("isActive")) vol.isActive = JB(j["isActive"]);
    if (j.contains("priority")) vol.priority = j["priority"].get<i32>();
    return vol;
}

// Base controller fields helper
json SerializeControllerBase(const ECS::CharacterControllerBase& base) {
    json j;
    j["moveSpeed"] = RF(base.moveSpeed);
    j["sprintMultiplier"] = RF(base.sprintMultiplier);
    j["isEnabled"] = base.isEnabled;
    j["ignoreGlobalTimeScale"] = base.ignoreGlobalTimeScale;
    j["useWASD"] = base.useWASD;
    j["useArrowKeys"] = base.useArrowKeys;
    j["useGamepad"] = base.useGamepad;
    j["gamepadIndex"] = RF(base.gamepadIndex);
    j["gamepadLookSensitivity"] = RF(base.gamepadLookSensitivity);
    j["disableMouseLook"] = base.disableMouseLook;
    j["captureMouseOnClick"] = base.captureMouseOnClick;
    j["gridMovement"] = base.gridMovement;
    j["gridCellSize"] = RF(base.gridCellSize);
    j["gridMoveSpeed"] = RF(base.gridMoveSpeed);
    j["gridOrigin"] = SerializeVector3(base.gridOrigin);
    j["gridMoveStart"] = SerializeVector3(base.gridMoveStart);
    j["gridMoveTarget"] = SerializeVector3(base.gridMoveTarget);
    j["gridMoving"] = base.gridMoving;
    return j;
}

void DeserializeControllerBase(const json& j, ECS::CharacterControllerBase& base) {
    if (j.contains("moveSpeed")) base.moveSpeed = j["moveSpeed"].get<f32>();
    if (j.contains("sprintMultiplier")) base.sprintMultiplier = j["sprintMultiplier"].get<f32>();
    if (j.contains("isEnabled")) base.isEnabled = JB(j["isEnabled"]);
    if (j.contains("ignoreGlobalTimeScale")) base.ignoreGlobalTimeScale = JB(j["ignoreGlobalTimeScale"]);
    if (j.contains("useWASD")) base.useWASD = JB(j["useWASD"]);
    if (j.contains("useArrowKeys")) base.useArrowKeys = JB(j["useArrowKeys"]);
    if (j.contains("useGamepad")) base.useGamepad = JB(j["useGamepad"]);
    if (j.contains("gamepadIndex")) base.gamepadIndex = j["gamepadIndex"].get<i32>();
    if (j.contains("gamepadLookSensitivity")) base.gamepadLookSensitivity = j["gamepadLookSensitivity"].get<f32>();
    if (j.contains("disableMouseLook")) base.disableMouseLook = JB(j["disableMouseLook"]);
    if (j.contains("captureMouseOnClick")) base.captureMouseOnClick = JB(j["captureMouseOnClick"]);
    if (j.contains("gridMovement")) base.gridMovement = JB(j["gridMovement"]);
    if (j.contains("gridCellSize")) base.gridCellSize = j["gridCellSize"].get<f32>();
    if (j.contains("gridMoveSpeed")) base.gridMoveSpeed = j["gridMoveSpeed"].get<f32>();
    if (j.contains("gridOrigin")) base.gridOrigin = DeserializeVector3(j["gridOrigin"]);
    if (j.contains("gridMoveStart")) base.gridMoveStart = DeserializeVector3(j["gridMoveStart"]);
    if (j.contains("gridMoveTarget")) base.gridMoveTarget = DeserializeVector3(j["gridMoveTarget"]);
    if (j.contains("gridMoving")) base.gridMoving = JB(j["gridMoving"]);
}

json SerializePlatformer2D(const ECS::Platformer2DController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["jumpForce"] = RF(ctrl.jumpForce);
    j["gravity"] = RF(ctrl.gravity);
    j["maxJumps"] = ctrl.maxJumps;
    j["acceleration"] = RF(ctrl.acceleration);
    j["deceleration"] = RF(ctrl.deceleration);
    j["airControl"] = RF(ctrl.airControl);
    j["coyoteTime"] = RF(ctrl.coyoteTime);
    j["jumpBufferTime"] = RF(ctrl.jumpBufferTime);
    j["enableWallJump"] = ctrl.enableWallJump;
    j["enableWallSlide"] = ctrl.enableWallSlide;
    j["wallSlideSpeed"] = RF(ctrl.wallSlideSpeed);
    j["wallJumpForce"] = RF(ctrl.wallJumpForce);
    j["collisionRadius"] = RF(ctrl.collisionRadius);
    j["collisionHeight"] = RF(ctrl.collisionHeight);
    j["groundCheckDistance"] = RF(ctrl.groundCheckDistance);
    return j;
}

ECS::Platformer2DController DeserializePlatformer2D(const json& j) {
    ECS::Platformer2DController ctrl;
    DeserializeControllerBase(j, ctrl);
    if (j.contains("jumpForce")) ctrl.jumpForce = j["jumpForce"].get<f32>();
    if (j.contains("gravity")) ctrl.gravity = j["gravity"].get<f32>();
    if (j.contains("maxJumps")) ctrl.maxJumps = j["maxJumps"].get<i32>();
    if (j.contains("acceleration")) ctrl.acceleration = j["acceleration"].get<f32>();
    if (j.contains("deceleration")) ctrl.deceleration = j["deceleration"].get<f32>();
    if (j.contains("airControl")) ctrl.airControl = j["airControl"].get<f32>();
    if (j.contains("coyoteTime")) ctrl.coyoteTime = j["coyoteTime"].get<f32>();
    if (j.contains("jumpBufferTime")) ctrl.jumpBufferTime = j["jumpBufferTime"].get<f32>();
    if (j.contains("enableWallJump")) ctrl.enableWallJump = JB(j["enableWallJump"]);
    if (j.contains("enableWallSlide")) ctrl.enableWallSlide = JB(j["enableWallSlide"]);
    if (j.contains("wallSlideSpeed")) ctrl.wallSlideSpeed = j["wallSlideSpeed"].get<f32>();
    if (j.contains("wallJumpForce")) ctrl.wallJumpForce = j["wallJumpForce"].get<f32>();
    if (j.contains("collisionRadius")) ctrl.collisionRadius = j["collisionRadius"].get<f32>();
    if (j.contains("collisionHeight")) ctrl.collisionHeight = j["collisionHeight"].get<f32>();
    if (j.contains("groundCheckDistance")) ctrl.groundCheckDistance = j["groundCheckDistance"].get<f32>();
    return ctrl;
}

json SerializeTopDown2D(const ECS::TopDown2DController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["acceleration"] = RF(ctrl.acceleration);
    j["deceleration"] = RF(ctrl.deceleration);
    j["rotateToFaceMovement"] = ctrl.rotateToFaceMovement;
    j["rotationSpeed"] = RF(ctrl.rotationSpeed);
    j["enableDash"] = ctrl.enableDash;
    j["dashSpeed"] = RF(ctrl.dashSpeed);
    j["dashDuration"] = RF(ctrl.dashDuration);
    j["dashCooldown"] = RF(ctrl.dashCooldown);
    return j;
}

ECS::TopDown2DController DeserializeTopDown2D(const json& j) {
    ECS::TopDown2DController ctrl;
    DeserializeControllerBase(j, ctrl);
    if (j.contains("acceleration")) ctrl.acceleration = j["acceleration"].get<f32>();
    if (j.contains("deceleration")) ctrl.deceleration = j["deceleration"].get<f32>();
    if (j.contains("rotateToFaceMovement")) ctrl.rotateToFaceMovement = JB(j["rotateToFaceMovement"]);
    if (j.contains("rotationSpeed")) ctrl.rotationSpeed = j["rotationSpeed"].get<f32>();
    if (j.contains("enableDash")) ctrl.enableDash = JB(j["enableDash"]);
    if (j.contains("dashSpeed")) ctrl.dashSpeed = j["dashSpeed"].get<f32>();
    if (j.contains("dashDuration")) ctrl.dashDuration = j["dashDuration"].get<f32>();
    if (j.contains("dashCooldown")) ctrl.dashCooldown = j["dashCooldown"].get<f32>();
    return ctrl;
}

json SerializeTopDown3D(const ECS::TopDown3DController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["acceleration"] = RF(ctrl.acceleration);
    j["deceleration"] = RF(ctrl.deceleration);
    j["rotateToFaceMovement"] = ctrl.rotateToFaceMovement;
    j["rotationSpeed"] = RF(ctrl.rotationSpeed);
    j["cameraAngle"] = RF(ctrl.cameraAngle);
    j["cameraDistance"] = RF(ctrl.cameraDistance);
    j["cameraHeight"] = RF(ctrl.cameraHeight);
    j["lockCameraToPlayer"] = ctrl.lockCameraToPlayer;
    j["enableClickToMove"] = ctrl.enableClickToMove;
    j["arrivalThreshold"] = RF(ctrl.arrivalThreshold);
    j["enableDash"] = ctrl.enableDash;
    j["dashSpeed"] = RF(ctrl.dashSpeed);
    j["dashDuration"] = RF(ctrl.dashDuration);
    j["dashCooldown"] = RF(ctrl.dashCooldown);
    return j;
}

ECS::TopDown3DController DeserializeTopDown3D(const json& j) {
    ECS::TopDown3DController ctrl;
    DeserializeControllerBase(j, ctrl);
    if (j.contains("acceleration")) ctrl.acceleration = j["acceleration"].get<f32>();
    if (j.contains("deceleration")) ctrl.deceleration = j["deceleration"].get<f32>();
    if (j.contains("rotateToFaceMovement")) ctrl.rotateToFaceMovement = JB(j["rotateToFaceMovement"]);
    if (j.contains("rotationSpeed")) ctrl.rotationSpeed = j["rotationSpeed"].get<f32>();
    if (j.contains("cameraAngle")) ctrl.cameraAngle = j["cameraAngle"].get<f32>();
    if (j.contains("cameraDistance")) ctrl.cameraDistance = j["cameraDistance"].get<f32>();
    if (j.contains("cameraHeight")) ctrl.cameraHeight = j["cameraHeight"].get<f32>();
    if (j.contains("lockCameraToPlayer")) ctrl.lockCameraToPlayer = JB(j["lockCameraToPlayer"]);
    if (j.contains("enableClickToMove")) ctrl.enableClickToMove = JB(j["enableClickToMove"]);
    if (j.contains("arrivalThreshold")) ctrl.arrivalThreshold = j["arrivalThreshold"].get<f32>();
    if (j.contains("enableDash")) ctrl.enableDash = JB(j["enableDash"]);
    if (j.contains("dashSpeed")) ctrl.dashSpeed = j["dashSpeed"].get<f32>();
    if (j.contains("dashDuration")) ctrl.dashDuration = j["dashDuration"].get<f32>();
    if (j.contains("dashCooldown")) ctrl.dashCooldown = j["dashCooldown"].get<f32>();
    return ctrl;
}

json SerializeThirdPerson(const ECS::ThirdPersonController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["acceleration"] = RF(ctrl.acceleration);
    j["deceleration"] = RF(ctrl.deceleration);
    j["jumpForce"] = RF(ctrl.jumpForce);
    j["gravity"] = RF(ctrl.gravity);
    j["rotateToFaceMovement"] = ctrl.rotateToFaceMovement;
    j["rotateToFaceCamera"] = ctrl.rotateToFaceCamera;
    j["rotationSpeed"] = RF(ctrl.rotationSpeed);
    j["cameraDistance"] = RF(ctrl.cameraDistance);
    j["cameraHeight"] = RF(ctrl.cameraHeight);
    j["cameraMinDistance"] = RF(ctrl.cameraMinDistance);
    j["cameraMaxDistance"] = RF(ctrl.cameraMaxDistance);
    j["cameraPitch"] = RF(ctrl.cameraPitch);
    j["cameraYaw"] = RF(ctrl.cameraYaw);
    j["cameraMinPitch"] = RF(ctrl.cameraMinPitch);
    j["cameraMaxPitch"] = RF(ctrl.cameraMaxPitch);
    j["cameraSensitivity"] = RF(ctrl.cameraSensitivity);
    j["cameraLerpSpeed"] = RF(ctrl.cameraLerpSpeed);
    j["enableCameraCollision"] = ctrl.enableCameraCollision;
    j["enableLockOn"] = ctrl.enableLockOn;
    j["frameSide"] = static_cast<u8>(ctrl.frameSide);
    j["frameHorizontalBias"] = RF(ctrl.frameHorizontalBias);
    return j;
}

ECS::ThirdPersonController DeserializeThirdPerson(const json& j) {
    ECS::ThirdPersonController ctrl;
    DeserializeControllerBase(j, ctrl);
    if (j.contains("acceleration")) ctrl.acceleration = j["acceleration"].get<f32>();
    if (j.contains("deceleration")) ctrl.deceleration = j["deceleration"].get<f32>();
    if (j.contains("jumpForce")) ctrl.jumpForce = j["jumpForce"].get<f32>();
    if (j.contains("gravity")) ctrl.gravity = j["gravity"].get<f32>();
    if (j.contains("rotateToFaceMovement")) ctrl.rotateToFaceMovement = JB(j["rotateToFaceMovement"]);
    if (j.contains("rotateToFaceCamera")) ctrl.rotateToFaceCamera = JB(j["rotateToFaceCamera"]);
    if (j.contains("rotationSpeed")) ctrl.rotationSpeed = j["rotationSpeed"].get<f32>();
    if (j.contains("cameraDistance")) ctrl.cameraDistance = j["cameraDistance"].get<f32>();
    if (j.contains("cameraHeight")) ctrl.cameraHeight = j["cameraHeight"].get<f32>();
    if (j.contains("cameraMinDistance")) ctrl.cameraMinDistance = j["cameraMinDistance"].get<f32>();
    if (j.contains("cameraMaxDistance")) ctrl.cameraMaxDistance = j["cameraMaxDistance"].get<f32>();
    if (j.contains("cameraPitch")) ctrl.cameraPitch = j["cameraPitch"].get<f32>();
    if (j.contains("cameraYaw")) ctrl.cameraYaw = j["cameraYaw"].get<f32>();
    if (j.contains("cameraMinPitch")) ctrl.cameraMinPitch = j["cameraMinPitch"].get<f32>();
    if (j.contains("cameraMaxPitch")) ctrl.cameraMaxPitch = j["cameraMaxPitch"].get<f32>();
    if (j.contains("cameraSensitivity")) ctrl.cameraSensitivity = j["cameraSensitivity"].get<f32>();
    if (j.contains("cameraLerpSpeed")) ctrl.cameraLerpSpeed = j["cameraLerpSpeed"].get<f32>();
    if (j.contains("enableCameraCollision")) ctrl.enableCameraCollision = JB(j["enableCameraCollision"]);
    if (j.contains("enableLockOn")) ctrl.enableLockOn = JB(j["enableLockOn"]);
    if (j.contains("frameSide")) { u8 v = j["frameSide"].get<u8>(); if (v <= 2) ctrl.frameSide = static_cast<ECS::ThirdPersonController::FrameSide>(v); }
    if (j.contains("frameHorizontalBias")) ctrl.frameHorizontalBias = j["frameHorizontalBias"].get<f32>();
    return ctrl;
}

json SerializeFirstPerson(const ECS::FirstPersonController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["acceleration"] = RF(ctrl.acceleration);
    j["deceleration"] = RF(ctrl.deceleration);
    j["jumpForce"] = RF(ctrl.jumpForce);
    j["gravity"] = RF(ctrl.gravity);
    j["mouseSensitivity"] = RF(ctrl.mouseSensitivity);
    j["minPitch"] = RF(ctrl.minPitch);
    j["maxPitch"] = RF(ctrl.maxPitch);
    j["invertY"] = ctrl.invertY;
    j["enableHeadBob"] = ctrl.enableHeadBob;
    j["headBobFrequency"] = RF(ctrl.headBobFrequency);
    j["headBobAmplitude"] = RF(ctrl.headBobAmplitude);
    j["enableCrouch"] = ctrl.enableCrouch;
    j["standingHeight"] = RF(ctrl.standingHeight);
    j["crouchingHeight"] = RF(ctrl.crouchingHeight);
    j["crouchSpeed"] = RF(ctrl.crouchSpeed);
    j["sprintFOVIncrease"] = RF(ctrl.sprintFOVIncrease);
    j["dungeonCrawlerMode"] = ctrl.dungeonCrawlerMode;
    j["snapTurnAngle"] = RF(ctrl.snapTurnAngle);
    j["enableDash"] = ctrl.enableDash;
    j["dashSpeed"] = RF(ctrl.dashSpeed);
    j["dashDuration"] = RF(ctrl.dashDuration);
    j["dashCooldown"] = RF(ctrl.dashCooldown);
    return j;
}

ECS::FirstPersonController DeserializeFirstPerson(const json& j) {
    ECS::FirstPersonController ctrl;
    DeserializeControllerBase(j, ctrl);
    if (j.contains("acceleration")) ctrl.acceleration = j["acceleration"].get<f32>();
    if (j.contains("deceleration")) ctrl.deceleration = j["deceleration"].get<f32>();
    if (j.contains("jumpForce")) ctrl.jumpForce = j["jumpForce"].get<f32>();
    if (j.contains("gravity")) ctrl.gravity = j["gravity"].get<f32>();
    if (j.contains("mouseSensitivity")) ctrl.mouseSensitivity = j["mouseSensitivity"].get<f32>();
    if (j.contains("minPitch")) ctrl.minPitch = j["minPitch"].get<f32>();
    if (j.contains("maxPitch")) ctrl.maxPitch = j["maxPitch"].get<f32>();
    if (j.contains("invertY")) ctrl.invertY = JB(j["invertY"]);
    if (j.contains("enableHeadBob")) ctrl.enableHeadBob = JB(j["enableHeadBob"]);
    if (j.contains("headBobFrequency")) ctrl.headBobFrequency = j["headBobFrequency"].get<f32>();
    if (j.contains("headBobAmplitude")) ctrl.headBobAmplitude = j["headBobAmplitude"].get<f32>();
    if (j.contains("enableCrouch")) ctrl.enableCrouch = JB(j["enableCrouch"]);
    if (j.contains("standingHeight")) ctrl.standingHeight = j["standingHeight"].get<f32>();
    if (j.contains("crouchingHeight")) ctrl.crouchingHeight = j["crouchingHeight"].get<f32>();
    if (j.contains("crouchSpeed")) ctrl.crouchSpeed = j["crouchSpeed"].get<f32>();
    if (j.contains("sprintFOVIncrease")) ctrl.sprintFOVIncrease = j["sprintFOVIncrease"].get<f32>();
    if (j.contains("dungeonCrawlerMode")) ctrl.dungeonCrawlerMode = JB(j["dungeonCrawlerMode"]);
    if (j.contains("snapTurnAngle")) ctrl.snapTurnAngle = j["snapTurnAngle"].get<f32>();
    if (j.contains("enableDash")) ctrl.enableDash = JB(j["enableDash"]);
    if (j.contains("dashSpeed")) ctrl.dashSpeed = j["dashSpeed"].get<f32>();
    if (j.contains("dashDuration")) ctrl.dashDuration = j["dashDuration"].get<f32>();
    if (j.contains("dashCooldown")) ctrl.dashCooldown = j["dashCooldown"].get<f32>();
    return ctrl;
}

json SerializeVehicle(const ECS::VehicleController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["maxSpeed"] = RF(ctrl.maxSpeed);
    j["reverseMaxSpeed"] = RF(ctrl.reverseMaxSpeed);
    j["acceleration"] = RF(ctrl.acceleration);
    j["brakeForce"] = RF(ctrl.brakeForce);
    j["engineBrake"] = RF(ctrl.engineBrake);
    j["maxSteerAngle"] = RF(ctrl.maxSteerAngle);
    j["steerSpeed"] = RF(ctrl.steerSpeed);
    j["steerReturnSpeed"] = RF(ctrl.steerReturnSpeed);
    j["wheelBase"] = RF(ctrl.wheelBase);
    j["grip"] = RF(ctrl.grip);
    j["driftFactor"] = RF(ctrl.driftFactor);
    j["downforceMultiplier"] = RF(ctrl.downforceMultiplier);
    j["mass"] = RF(ctrl.mass);
    j["cameraDistance"] = RF(ctrl.cameraDistance);
    j["cameraHeight"] = RF(ctrl.cameraHeight);
    j["cameraLerpSpeed"] = RF(ctrl.cameraLerpSpeed);
    j["cameraLookAhead"] = RF(ctrl.cameraLookAhead);
    j["bodyRollAmount"] = RF(ctrl.bodyRollAmount);
    j["bodyPitchAmount"] = RF(ctrl.bodyPitchAmount);
    j["modelForwardYaw"] = RF(ctrl.modelForwardYaw);
    return j;
}

ECS::VehicleController DeserializeVehicle(const json& j) {
    ECS::VehicleController ctrl;
    DeserializeControllerBase(j, ctrl);
    if (j.contains("maxSpeed")) ctrl.maxSpeed = j["maxSpeed"].get<f32>();
    if (j.contains("reverseMaxSpeed")) ctrl.reverseMaxSpeed = j["reverseMaxSpeed"].get<f32>();
    if (j.contains("acceleration")) ctrl.acceleration = j["acceleration"].get<f32>();
    if (j.contains("brakeForce")) ctrl.brakeForce = j["brakeForce"].get<f32>();
    if (j.contains("engineBrake")) ctrl.engineBrake = j["engineBrake"].get<f32>();
    if (j.contains("maxSteerAngle")) ctrl.maxSteerAngle = j["maxSteerAngle"].get<f32>();
    if (j.contains("steerSpeed")) ctrl.steerSpeed = j["steerSpeed"].get<f32>();
    if (j.contains("steerReturnSpeed")) ctrl.steerReturnSpeed = j["steerReturnSpeed"].get<f32>();
    if (j.contains("wheelBase")) ctrl.wheelBase = j["wheelBase"].get<f32>();
    if (j.contains("grip")) ctrl.grip = j["grip"].get<f32>();
    if (j.contains("driftFactor")) ctrl.driftFactor = j["driftFactor"].get<f32>();
    if (j.contains("downforceMultiplier")) ctrl.downforceMultiplier = j["downforceMultiplier"].get<f32>();
    if (j.contains("mass")) ctrl.mass = j["mass"].get<f32>();
    if (j.contains("cameraDistance")) ctrl.cameraDistance = j["cameraDistance"].get<f32>();
    if (j.contains("cameraHeight")) ctrl.cameraHeight = j["cameraHeight"].get<f32>();
    if (j.contains("cameraLerpSpeed")) ctrl.cameraLerpSpeed = j["cameraLerpSpeed"].get<f32>();
    if (j.contains("cameraLookAhead")) ctrl.cameraLookAhead = j["cameraLookAhead"].get<f32>();
    if (j.contains("bodyRollAmount")) ctrl.bodyRollAmount = j["bodyRollAmount"].get<f32>();
    if (j.contains("bodyPitchAmount")) ctrl.bodyPitchAmount = j["bodyPitchAmount"].get<f32>();
    if (j.contains("modelForwardYaw")) ctrl.modelForwardYaw = j["modelForwardYaw"].get<f32>();
    return ctrl;
}

json SerializeSurfaceAligned(const ECS::SurfaceAlignedController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["acceleration"] = RF(ctrl.acceleration);
    j["deceleration"] = RF(ctrl.deceleration);
    j["jumpForce"] = RF(ctrl.jumpForce);
    j["cameraDistance"] = RF(ctrl.cameraDistance);
    j["cameraHeight"] = RF(ctrl.cameraHeight);
    j["cameraPitch"] = RF(ctrl.cameraPitch);
    j["cameraYaw"] = RF(ctrl.cameraYaw);
    j["cameraMinPitch"] = RF(ctrl.cameraMinPitch);
    j["cameraMaxPitch"] = RF(ctrl.cameraMaxPitch);
    j["cameraSensitivity"] = RF(ctrl.cameraSensitivity);
    j["cameraLerpSpeed"] = RF(ctrl.cameraLerpSpeed);
    j["alignSpeed"] = RF(ctrl.alignSpeed);
    j["groundCheckDistance"] = RF(ctrl.groundCheckDistance);
    return j;
}

ECS::SurfaceAlignedController DeserializeSurfaceAligned(const json& j) {
    ECS::SurfaceAlignedController ctrl;
    DeserializeControllerBase(j, ctrl);
    if (j.contains("acceleration")) ctrl.acceleration = j["acceleration"].get<f32>();
    if (j.contains("deceleration")) ctrl.deceleration = j["deceleration"].get<f32>();
    if (j.contains("jumpForce")) ctrl.jumpForce = j["jumpForce"].get<f32>();
    if (j.contains("cameraDistance")) ctrl.cameraDistance = j["cameraDistance"].get<f32>();
    if (j.contains("cameraHeight")) ctrl.cameraHeight = j["cameraHeight"].get<f32>();
    if (j.contains("cameraPitch")) ctrl.cameraPitch = j["cameraPitch"].get<f32>();
    if (j.contains("cameraYaw")) ctrl.cameraYaw = j["cameraYaw"].get<f32>();
    if (j.contains("cameraMinPitch")) ctrl.cameraMinPitch = j["cameraMinPitch"].get<f32>();
    if (j.contains("cameraMaxPitch")) ctrl.cameraMaxPitch = j["cameraMaxPitch"].get<f32>();
    if (j.contains("cameraSensitivity")) ctrl.cameraSensitivity = j["cameraSensitivity"].get<f32>();
    if (j.contains("cameraLerpSpeed")) ctrl.cameraLerpSpeed = j["cameraLerpSpeed"].get<f32>();
    if (j.contains("alignSpeed")) ctrl.alignSpeed = j["alignSpeed"].get<f32>();
    if (j.contains("groundCheckDistance")) ctrl.groundCheckDistance = j["groundCheckDistance"].get<f32>();
    return ctrl;
}

json SerializePossessable(const ECS::PossessableComponent& comp) {
    json j;
    j["isPossessed"] = comp.isPossessed;
    j["autoDetect"] = comp.autoDetect;
    j["playerIndex"] = comp.playerIndex;
    j["possessRange"] = RF(comp.possessRange);
    j["promptText"] = comp.promptText;
    j["transitionDuration"] = RF(comp.transitionDuration);
    j["disableOnUnpossess"] = comp.disableOnUnpossess;
    return j;
}

ECS::PossessableComponent DeserializePossessable(const json& j) {
    ECS::PossessableComponent comp;
    if (j.contains("isPossessed")) comp.isPossessed = JB(j["isPossessed"]);
    if (j.contains("autoDetect")) comp.autoDetect = JB(j["autoDetect"]);
    if (j.contains("playerIndex")) comp.playerIndex = j["playerIndex"].get<i32>();
    if (j.contains("possessRange")) comp.possessRange = j["possessRange"].get<f32>();
    if (j.contains("promptText")) comp.promptText = SafeStr(j["promptText"], MAX_STR_NAME);
    if (j.contains("transitionDuration")) comp.transitionDuration = j["transitionDuration"].get<f32>();
    if (j.contains("disableOnUnpossess")) comp.disableOnUnpossess = JB(j["disableOnUnpossess"]);
    return comp;
}

json SerializeContentFlags(const Accessibility::SceneContentFlags& flags) {
    json j;
    j["flags"] = static_cast<u32>(flags.flags);
    if (!flags.customWarnings.empty()) {
        j["customWarnings"] = flags.customWarnings;
    }
    return j;
}

Accessibility::SceneContentFlags DeserializeContentFlags(const json& j) {
    Accessibility::SceneContentFlags flags;
    if (j.contains("flags")) { u32 v = j["flags"].get<u32>(); if (v <= 0xFF) flags.flags = static_cast<Accessibility::ContentWarningType>(v); }
    if (j.contains("customWarnings")) {
        for (const auto& w : j["customWarnings"]) {
            flags.customWarnings.push_back(w.get<std::string>());
        }
    }
    return flags;
}

json SerializeAudioSourceComponent(const ECS::AudioSourceComponent& audio) {
    json j;
    j["clipPath"] = audio.clipPath;
    j["volume"] = RF(audio.volume);
    j["pitch"] = RF(audio.pitch);
    j["minDistance"] = RF(audio.minDistance);
    j["maxDistance"] = RF(audio.maxDistance);
    j["playOnAwake"] = RF(audio.playOnAwake);
    j["loop"] = audio.loop;
    j["is3D"] = RF(audio.is3D);
    j["spatialBlend"] = RF(audio.spatialBlend);
    j["rolloff"] = static_cast<u8>(audio.rolloff);
    j["channel"] = static_cast<u8>(audio.channel);
    j["priority"] = audio.priority;
    j["pitchMin"] = RF(audio.pitchMin);
    j["pitchMax"] = RF(audio.pitchMax);
    j["volumeMin"] = RF(audio.volumeMin);
    j["volumeMax"] = RF(audio.volumeMax);
    if (!audio.clipVariations.empty()) j["clipVariations"] = audio.clipVariations;
    j["noRepeat"] = audio.noRepeat;
    j["usePooling"] = audio.usePooling;
    j["poolSize"] = audio.poolSize;
    if (!audio.audioDescription.empty()) j["audioDescription"] = audio.audioDescription;
    return j;
}

ECS::AudioSourceComponent DeserializeAudioSourceComponent(const json& j) {
    ECS::AudioSourceComponent audio;
    if (j.contains("clipPath")) audio.clipPath = SafeStr(j["clipPath"], MAX_STR_PATH);
    if (j.contains("volume")) audio.volume = j["volume"].get<f32>();
    if (j.contains("pitch")) audio.pitch = j["pitch"].get<f32>();
    if (j.contains("minDistance")) audio.minDistance = j["minDistance"].get<f32>();
    if (j.contains("maxDistance")) audio.maxDistance = j["maxDistance"].get<f32>();
    if (j.contains("playOnAwake")) audio.playOnAwake = JB(j["playOnAwake"]);
    if (j.contains("loop")) audio.loop = JB(j["loop"]);
    if (j.contains("is3D")) audio.is3D = JB(j["is3D"]);
    if (j.contains("spatialBlend")) audio.spatialBlend = j["spatialBlend"].get<f32>();
    if (j.contains("rolloff")) { u8 v = j["rolloff"].get<u8>(); if (v <= 2) audio.rolloff = static_cast<ECS::AudioSourceComponent::Rolloff>(v); }
    if (j.contains("channel")) { u8 v = j["channel"].get<u8>(); if (v < static_cast<u8>(ECS::AudioChannel::Count)) audio.channel = static_cast<ECS::AudioChannel>(v); }
    if (j.contains("priority")) audio.priority = j["priority"].get<i32>();
    if (j.contains("pitchMin")) audio.pitchMin = j["pitchMin"].get<f32>();
    if (j.contains("pitchMax")) audio.pitchMax = j["pitchMax"].get<f32>();
    if (j.contains("volumeMin")) audio.volumeMin = j["volumeMin"].get<f32>();
    if (j.contains("volumeMax")) audio.volumeMax = j["volumeMax"].get<f32>();
    if (j.contains("clipVariations") && j["clipVariations"].is_array()) {
        for (const auto& c : j["clipVariations"]) {
            if (audio.clipVariations.size() >= 64) break;
            audio.clipVariations.push_back(SafeStr(c, MAX_STR_PATH));
        }
    }
    if (j.contains("noRepeat")) audio.noRepeat = JB(j["noRepeat"]);
    if (j.contains("usePooling")) audio.usePooling = JB(j["usePooling"]);
    if (j.contains("poolSize")) { u32 v = j["poolSize"].get<u32>(); if (v >= 1 && v <= 64) audio.poolSize = v; }
    if (j.contains("audioDescription")) audio.audioDescription = SafeStr(j["audioDescription"], 512);
    return audio;
}

json SerializeAudioListenerComponent(const ECS::AudioListenerComponent& listener) {
    json j;
    j["isActive"] = listener.isActive;
    j["volumeScale"] = RF(listener.volumeScale);
    return j;
}

ECS::AudioListenerComponent DeserializeAudioListenerComponent(const json& j) {
    ECS::AudioListenerComponent listener;
    if (j.contains("isActive")) listener.isActive = JB(j["isActive"]);
    if (j.contains("volumeScale")) listener.volumeScale = j["volumeScale"].get<f32>();
    return listener;
}

// ============================================================================
// Physics & Collision
// ============================================================================

json SerializeRigidbodyComponent(const ECS::RigidbodyComponent& rb) {
    json j;
    j["mass"] = RF(rb.mass);
    j["drag"] = RF(rb.drag);
    j["angularDrag"] = RF(rb.angularDrag);
    j["useGravity"] = rb.useGravity;
    j["gravityScale"] = RF(rb.gravityScale);
    j["freezePositionX"] = RF(rb.freezePositionX);
    j["freezePositionY"] = RF(rb.freezePositionY);
    j["freezePositionZ"] = RF(rb.freezePositionZ);
    j["freezeRotationX"] = RF(rb.freezeRotationX);
    j["freezeRotationY"] = RF(rb.freezeRotationY);
    j["freezeRotationZ"] = RF(rb.freezeRotationZ);
    j["bodyType"] = static_cast<u8>(rb.bodyType);
    j["collisionMode"] = static_cast<u8>(rb.collisionMode);
    // R3 fix: serialize runtime state for mid-play save/load
    j["velocity"] = SerializeVector3(rb.velocity);
    j["angularVelocity"] = SerializeVector3(rb.angularVelocity);
    j["maxVelocity"] = RF(rb.maxVelocity);
    j["maxAngularVelocity"] = RF(rb.maxAngularVelocity);
    j["isGrounded"] = rb.isGrounded;
    j["isSleeping"] = rb.isSleeping;
    return j;
}

ECS::RigidbodyComponent DeserializeRigidbodyComponent(const json& j) {
    ECS::RigidbodyComponent rb;
    if (j.contains("mass")) rb.mass = j["mass"].get<f32>();
    if (j.contains("drag")) rb.drag = j["drag"].get<f32>();
    if (j.contains("angularDrag")) rb.angularDrag = j["angularDrag"].get<f32>();
    if (j.contains("useGravity")) rb.useGravity = JB(j["useGravity"]);
    if (j.contains("gravityScale")) rb.gravityScale = j["gravityScale"].get<f32>();
    if (j.contains("freezePositionX")) rb.freezePositionX = JB(j["freezePositionX"]);
    if (j.contains("freezePositionY")) rb.freezePositionY = JB(j["freezePositionY"]);
    if (j.contains("freezePositionZ")) rb.freezePositionZ = JB(j["freezePositionZ"]);
    if (j.contains("freezeRotationX")) rb.freezeRotationX = JB(j["freezeRotationX"]);
    if (j.contains("freezeRotationY")) rb.freezeRotationY = JB(j["freezeRotationY"]);
    if (j.contains("freezeRotationZ")) rb.freezeRotationZ = JB(j["freezeRotationZ"]);
    if (j.contains("bodyType")) { u8 v = j["bodyType"].get<u8>(); if (v <= 2) rb.bodyType = static_cast<ECS::RigidbodyComponent::BodyType>(v); }
    if (j.contains("collisionMode")) { u8 v = j["collisionMode"].get<u8>(); if (v <= 2) rb.collisionMode = static_cast<ECS::RigidbodyComponent::CollisionMode>(v); }
    // R3 fix: deserialize runtime state for mid-play save/load
    if (j.contains("velocity")) rb.velocity = DeserializeVector3(j["velocity"]);
    if (j.contains("angularVelocity")) rb.angularVelocity = DeserializeVector3(j["angularVelocity"]);
    if (j.contains("maxVelocity")) rb.maxVelocity = j["maxVelocity"].get<f32>();
    if (j.contains("maxAngularVelocity")) rb.maxAngularVelocity = j["maxAngularVelocity"].get<f32>();
    if (j.contains("isGrounded")) rb.isGrounded = JB(j["isGrounded"]);
    if (j.contains("isSleeping")) rb.isSleeping = JB(j["isSleeping"]);
    return rb;
}

json SerializeBoxColliderComponent(const ECS::BoxColliderComponent& col) {
    json j;
    j["center"] = SerializeVector3(col.center);
    j["size"] = SerializeVector3(col.size);
    j["isTrigger"] = col.isTrigger;
    j["friction"] = RF(col.friction);
    j["bounciness"] = RF(col.bounciness);
    j["categoryBits"] = col.categoryBits;
    j["collisionMask"] = col.collisionMask;
    return j;
}

ECS::BoxColliderComponent DeserializeBoxColliderComponent(const json& j) {
    ECS::BoxColliderComponent col;
    if (j.contains("center")) col.center = DeserializeVector3(j["center"]);
    if (j.contains("size")) col.size = DeserializeVector3(j["size"]);
    if (j.contains("isTrigger")) col.isTrigger = JB(j["isTrigger"]);
    if (j.contains("friction")) col.friction = j["friction"].get<f32>();
    if (j.contains("bounciness")) col.bounciness = j["bounciness"].get<f32>();
    if (j.contains("categoryBits")) {
        col.categoryBits = j["categoryBits"].get<u32>();
    } else if (j.contains("layer")) {
        // Migrate old "layer" field: layer 0 â†’ bit 0, layer N â†’ bit N
        u32 oldLayer = j["layer"].get<u32>();
        col.categoryBits = (oldLayer == 0) ? 1 : (1u << oldLayer);
    }
    if (j.contains("collisionMask")) col.collisionMask = j["collisionMask"].get<u32>();
    return col;
}

// ============================================================================
// Per-Frame Collider Component
// ============================================================================

json SerializePerFrameColliderComponent(const ECS::PerFrameColliderComponent& pfc) {
    json j;
    j["autoApply"] = pfc.autoApply;
    json frames = json::array();
    for (const auto& fc : pfc.frameColliders) {
        json f;
        f["offset"] = json::array({fc.offset.x, fc.offset.y});
        f["size"] = json::array({fc.size.x, fc.size.y});
        f["enabled"] = fc.enabled;
        frames.push_back(f);
    }
    j["frameColliders"] = frames;
    return j;
}

ECS::PerFrameColliderComponent DeserializePerFrameColliderComponent(const json& j) {
    ECS::PerFrameColliderComponent pfc;
    if (j.contains("autoApply")) pfc.autoApply = JB(j["autoApply"]);
    if (j.contains("frameColliders") && j["frameColliders"].is_array()) {
        for (const auto& f : j["frameColliders"]) {
            ECS::PerFrameColliderComponent::FrameCollider fc;
            if (f.contains("offset") && f["offset"].is_array() && f["offset"].size() >= 2) {
                fc.offset = Math::Vector2(f["offset"][0].get<f32>(), f["offset"][1].get<f32>());
            }
            if (f.contains("size") && f["size"].is_array() && f["size"].size() >= 2) {
                fc.size = Math::Vector2(f["size"][0].get<f32>(), f["size"][1].get<f32>());
            }
            if (f.contains("enabled")) fc.enabled = JB(f["enabled"]);
            pfc.frameColliders.push_back(fc);
        }
    }
    return pfc;
}

// ============================================================================
// Polygon Collider 2D Component
// ============================================================================

json SerializePolygonCollider2DComponent(const ECS::PolygonCollider2DComponent& poly) {
    json j;
    json verts = json::array();
    for (const auto& v : poly.vertices) {
        verts.push_back(json::array({v.x, v.y}));
    }
    j["vertices"] = verts;
    j["isTrigger"] = poly.isTrigger;
    j["friction"] = RF(poly.friction);
    j["bounciness"] = RF(poly.bounciness);
    j["categoryBits"] = poly.categoryBits;
    j["collisionMask"] = poly.collisionMask;
    return j;
}

ECS::PolygonCollider2DComponent DeserializePolygonCollider2DComponent(const json& j) {
    ECS::PolygonCollider2DComponent poly;
    if (j.contains("vertices") && j["vertices"].is_array()) {
        for (const auto& v : j["vertices"]) {
            if (v.is_array() && v.size() >= 2) {
                poly.vertices.push_back(Math::Vector2(v[0].get<f32>(), v[1].get<f32>()));
            }
        }
    }
    if (j.contains("isTrigger")) poly.isTrigger = JB(j["isTrigger"]);
    if (j.contains("friction")) poly.friction = j["friction"].get<f32>();
    if (j.contains("bounciness")) poly.bounciness = j["bounciness"].get<f32>();
    if (j.contains("categoryBits")) poly.categoryBits = j["categoryBits"].get<u32>();
    if (j.contains("collisionMask")) poly.collisionMask = j["collisionMask"].get<u32>();
    return poly;
}

// Body2D component
json SerializeBody2DComponent(const Physics::Body2DComponent& body) {
    json j;
    j["shapeType"] = static_cast<int>(body.shapeType);
    // Circle
    j["circleRadius"] = RF(body.circle.radius);
    j["circleOffset"] = json::array({RF(body.circle.offset.x), RF(body.circle.offset.y)});
    // Box
    j["boxHalfExtents"] = json::array({RF(body.box.halfExtents.x), RF(body.box.halfExtents.y)});
    j["boxOffset"] = json::array({RF(body.box.offset.x), RF(body.box.offset.y)});
    j["boxRotation"] = RF(body.box.rotation);
    // Capsule
    j["capsuleRadius"] = RF(body.capsule.radius);
    j["capsuleHeight"] = RF(body.capsule.height);
    j["capsuleOffset"] = json::array({RF(body.capsule.offset.x), RF(body.capsule.offset.y)});
    // Body properties
    j["isStatic"] = body.isStatic;
    j["isKinematic"] = body.isKinematic;
    j["isSensor"] = body.isSensor;
    j["fixedRotation"] = body.fixedRotation;
    j["gravityScale"] = RF(body.gravityScale);
    j["linearDamping"] = RF(body.linearDamping);
    j["angularDamping"] = RF(body.angularDamping);
    // Material
    j["friction"] = RF(body.material.friction);
    j["restitution"] = RF(body.material.restitution);
    j["density"] = RF(body.material.density);
    // Collision filtering
    j["categoryBits"] = body.categoryBits;
    j["collisionMask"] = body.collisionMask;
    return j;
}

Physics::Body2DComponent DeserializeBody2DComponent(const json& j) {
    Physics::Body2DComponent body;
    if (j.contains("shapeType")) { int v = j["shapeType"].get<int>(); if (v >= 0 && v <= 2) body.shapeType = static_cast<Physics::Shape2DType>(v); }
    if (j.contains("circleRadius")) body.circle.radius = j["circleRadius"].get<f32>();
    if (j.contains("circleOffset") && j["circleOffset"].is_array() && j["circleOffset"].size() >= 2) {
        body.circle.offset = Math::Vector2(j["circleOffset"][0].get<f32>(), j["circleOffset"][1].get<f32>());
    }
    if (j.contains("boxHalfExtents") && j["boxHalfExtents"].is_array() && j["boxHalfExtents"].size() >= 2) {
        body.box.halfExtents = Math::Vector2(j["boxHalfExtents"][0].get<f32>(), j["boxHalfExtents"][1].get<f32>());
    }
    if (j.contains("boxOffset") && j["boxOffset"].is_array() && j["boxOffset"].size() >= 2) {
        body.box.offset = Math::Vector2(j["boxOffset"][0].get<f32>(), j["boxOffset"][1].get<f32>());
    }
    if (j.contains("boxRotation")) body.box.rotation = j["boxRotation"].get<f32>();
    if (j.contains("capsuleRadius")) body.capsule.radius = j["capsuleRadius"].get<f32>();
    if (j.contains("capsuleHeight")) body.capsule.height = j["capsuleHeight"].get<f32>();
    if (j.contains("capsuleOffset") && j["capsuleOffset"].is_array() && j["capsuleOffset"].size() >= 2) {
        body.capsule.offset = Math::Vector2(j["capsuleOffset"][0].get<f32>(), j["capsuleOffset"][1].get<f32>());
    }
    if (j.contains("isStatic")) body.isStatic = JB(j["isStatic"]);
    if (j.contains("isKinematic")) body.isKinematic = JB(j["isKinematic"]);
    if (j.contains("isSensor")) body.isSensor = JB(j["isSensor"]);
    if (j.contains("fixedRotation")) body.fixedRotation = JB(j["fixedRotation"]);
    if (j.contains("gravityScale")) body.gravityScale = j["gravityScale"].get<f32>();
    if (j.contains("linearDamping")) body.linearDamping = j["linearDamping"].get<f32>();
    if (j.contains("angularDamping")) body.angularDamping = j["angularDamping"].get<f32>();
    if (j.contains("friction")) body.material.friction = j["friction"].get<f32>();
    if (j.contains("restitution")) body.material.restitution = j["restitution"].get<f32>();
    if (j.contains("density")) body.material.density = j["density"].get<f32>();
    if (j.contains("categoryBits")) body.categoryBits = j["categoryBits"].get<u32>();
    if (j.contains("collisionMask")) body.collisionMask = j["collisionMask"].get<u32>();
    return body;
}

json SerializeJoint2DComponent(const Physics::Joint2DComponent& joint) {
    json j;
    j["type"] = static_cast<int>(joint.type);
    j["connectedEntity"] = static_cast<u64>(joint.connectedEntity);
    j["anchorA"] = json::array({RF(joint.anchorA.x), RF(joint.anchorA.y)});
    j["anchorB"] = json::array({RF(joint.anchorB.x), RF(joint.anchorB.y)});
    j["enableLimit"] = joint.enableLimit;
    j["lowerAngle"] = RF(joint.lowerAngle);
    j["upperAngle"] = RF(joint.upperAngle);
    j["enableMotor"] = joint.enableMotor;
    j["motorSpeed"] = RF(joint.motorSpeed);
    j["maxMotorTorque"] = RF(joint.maxMotorTorque);
    j["axis"] = json::array({RF(joint.axis.x), RF(joint.axis.y)});
    j["lowerTranslation"] = RF(joint.lowerTranslation);
    j["upperTranslation"] = RF(joint.upperTranslation);
    j["length"] = RF(joint.length);
    j["minLength"] = RF(joint.minLength);
    j["maxLength"] = RF(joint.maxLength);
    j["stiffness"] = RF(joint.stiffness);
    j["damping"] = RF(joint.damping);
    j["collideConnected"] = joint.collideConnected;
    return j;
}

Physics::Joint2DComponent DeserializeJoint2DComponent(const json& j) {
    Physics::Joint2DComponent joint;
    if (j.contains("type")) { int v = j["type"].get<int>(); if (v >= 0 && v <= 4) joint.type = static_cast<Physics::Joint2DType>(v); }
    if (j.contains("connectedEntity")) joint.connectedEntity = static_cast<ECS::Entity>(j["connectedEntity"].get<u64>());
    if (j.contains("anchorA") && j["anchorA"].is_array() && j["anchorA"].size() >= 2) {
        joint.anchorA = Math::Vector2(j["anchorA"][0].get<f32>(), j["anchorA"][1].get<f32>());
    }
    if (j.contains("anchorB") && j["anchorB"].is_array() && j["anchorB"].size() >= 2) {
        joint.anchorB = Math::Vector2(j["anchorB"][0].get<f32>(), j["anchorB"][1].get<f32>());
    }
    if (j.contains("enableLimit")) joint.enableLimit = JB(j["enableLimit"]);
    if (j.contains("lowerAngle")) joint.lowerAngle = j["lowerAngle"].get<f32>();
    if (j.contains("upperAngle")) joint.upperAngle = j["upperAngle"].get<f32>();
    if (j.contains("enableMotor")) joint.enableMotor = JB(j["enableMotor"]);
    if (j.contains("motorSpeed")) joint.motorSpeed = j["motorSpeed"].get<f32>();
    if (j.contains("maxMotorTorque")) joint.maxMotorTorque = j["maxMotorTorque"].get<f32>();
    if (j.contains("axis") && j["axis"].is_array() && j["axis"].size() >= 2) {
        joint.axis = Math::Vector2(j["axis"][0].get<f32>(), j["axis"][1].get<f32>());
    }
    if (j.contains("lowerTranslation")) joint.lowerTranslation = j["lowerTranslation"].get<f32>();
    if (j.contains("upperTranslation")) joint.upperTranslation = j["upperTranslation"].get<f32>();
    if (j.contains("length")) joint.length = j["length"].get<f32>();
    if (j.contains("minLength")) joint.minLength = j["minLength"].get<f32>();
    if (j.contains("maxLength")) joint.maxLength = j["maxLength"].get<f32>();
    if (j.contains("stiffness")) joint.stiffness = j["stiffness"].get<f32>();
    if (j.contains("damping")) joint.damping = j["damping"].get<f32>();
    if (j.contains("collideConnected")) joint.collideConnected = JB(j["collideConnected"]);
    return joint;
}

// Network components
json SerializeNetworkIdentityComponent(const ECS::NetworkIdentityComponent& net) {
    json j;
    j["networkId"] = net.networkId;
    j["ownerId"] = net.ownerId;
    j["syncTransform"] = net.syncTransform;
    j["syncInterval"] = RF(net.syncInterval);
    return j;
}

ECS::NetworkIdentityComponent DeserializeNetworkIdentityComponent(const json& j) {
    ECS::NetworkIdentityComponent net;
    if (j.contains("networkId")) net.networkId = j["networkId"].get<u32>();
    if (j.contains("ownerId")) net.ownerId = j["ownerId"].get<u8>();
    if (j.contains("syncTransform")) net.syncTransform = JB(j["syncTransform"]);
    if (j.contains("syncInterval")) net.syncInterval = j["syncInterval"].get<f32>();
    return net;
}

json SerializeNetworkTransformComponent(const ECS::NetworkTransformComponent& nt) {
    json j;
    j["lastSyncedPosition"] = SerializeVector3(nt.lastSyncedPosition);
    j["lastSyncedRotation"] = SerializeQuaternion(nt.lastSyncedRotation);
    j["lastSyncedScale"] = SerializeVector3(nt.lastSyncedScale);
    j["interpDuration"] = RF(nt.interpDuration);
    return j;
}

ECS::NetworkTransformComponent DeserializeNetworkTransformComponent(const json& j) {
    ECS::NetworkTransformComponent nt;
    if (j.contains("lastSyncedPosition")) nt.lastSyncedPosition = DeserializeVector3(j["lastSyncedPosition"]);
    if (j.contains("lastSyncedRotation")) nt.lastSyncedRotation = DeserializeQuaternion(j["lastSyncedRotation"]);
    if (j.contains("lastSyncedScale")) nt.lastSyncedScale = DeserializeVector3(j["lastSyncedScale"]);
    if (j.contains("interpDuration")) nt.interpDuration = j["interpDuration"].get<f32>();
    return nt;
}

json SerializeSphereColliderComponent(const ECS::SphereColliderComponent& col) {
    json j;
    j["center"] = SerializeVector3(col.center);
    j["radius"] = RF(col.radius);
    j["isTrigger"] = col.isTrigger;
    j["friction"] = RF(col.friction);
    j["bounciness"] = RF(col.bounciness);
    j["categoryBits"] = col.categoryBits;
    j["collisionMask"] = col.collisionMask;
    return j;
}

ECS::SphereColliderComponent DeserializeSphereColliderComponent(const json& j) {
    ECS::SphereColliderComponent col;
    if (j.contains("center")) col.center = DeserializeVector3(j["center"]);
    if (j.contains("radius")) col.radius = j["radius"].get<f32>();
    if (j.contains("isTrigger")) col.isTrigger = JB(j["isTrigger"]);
    if (j.contains("friction")) col.friction = j["friction"].get<f32>();
    if (j.contains("bounciness")) col.bounciness = j["bounciness"].get<f32>();
    if (j.contains("categoryBits")) {
        col.categoryBits = j["categoryBits"].get<u32>();
    } else if (j.contains("layer")) {
        u32 oldLayer = j["layer"].get<u32>();
        col.categoryBits = (oldLayer == 0) ? 1 : (1u << oldLayer);
    }
    if (j.contains("collisionMask")) col.collisionMask = j["collisionMask"].get<u32>();
    return col;
}

json SerializeCapsuleColliderComponent(const ECS::CapsuleColliderComponent& col) {
    json j;
    j["center"] = SerializeVector3(col.center);
    j["radius"] = RF(col.radius);
    j["height"] = col.height;
    j["direction"] = static_cast<u8>(col.direction);
    j["isTrigger"] = col.isTrigger;
    j["friction"] = RF(col.friction);
    j["bounciness"] = RF(col.bounciness);
    j["categoryBits"] = col.categoryBits;
    j["collisionMask"] = col.collisionMask;
    return j;
}

ECS::CapsuleColliderComponent DeserializeCapsuleColliderComponent(const json& j) {
    ECS::CapsuleColliderComponent col;
    if (j.contains("center")) col.center = DeserializeVector3(j["center"]);
    if (j.contains("radius")) col.radius = j["radius"].get<f32>();
    if (j.contains("height")) col.height = j["height"].get<f32>();
    if (j.contains("direction")) { u8 v = j["direction"].get<u8>(); if (v <= 2) col.direction = static_cast<ECS::CapsuleColliderComponent::Direction>(v); }
    if (j.contains("isTrigger")) col.isTrigger = JB(j["isTrigger"]);
    if (j.contains("friction")) col.friction = j["friction"].get<f32>();
    if (j.contains("bounciness")) col.bounciness = j["bounciness"].get<f32>();
    if (j.contains("categoryBits")) {
        col.categoryBits = j["categoryBits"].get<u32>();
    } else if (j.contains("layer")) {
        u32 oldLayer = j["layer"].get<u32>();
        col.categoryBits = (oldLayer == 0) ? 1 : (1u << oldLayer);
    }
    if (j.contains("collisionMask")) col.collisionMask = j["collisionMask"].get<u32>();
    return col;
}

// ============================================================================
// Mesh Collider Component
// ============================================================================

json SerializeMeshColliderComponent(const ECS::MeshColliderComponent& col) {
    json j;
    j["convex"] = col.convex;
    j["autoGenerate"] = col.autoGenerate;
    j["isTrigger"] = col.isTrigger;
    j["friction"] = RF(col.friction);
    j["bounciness"] = RF(col.bounciness);
    j["categoryBits"] = col.categoryBits;
    j["collisionMask"] = col.collisionMask;
    // Serialize cached vertices if generated (avoid re-generating on load)
    if (col.generated && !col.vertices.empty()) {
        json verts = json::array();
        for (const auto& v : col.vertices) {
            verts.push_back(SerializeVector3(v));
        }
        j["vertices"] = verts;
        if (!col.convex && !col.indices.empty()) {
            j["indices"] = col.indices;
        }
    }
    return j;
}

ECS::MeshColliderComponent DeserializeMeshColliderComponent(const json& j) {
    ECS::MeshColliderComponent col;
    if (j.contains("convex")) col.convex = JB(j["convex"]);
    if (j.contains("autoGenerate")) col.autoGenerate = JB(j["autoGenerate"]);
    if (j.contains("isTrigger")) col.isTrigger = JB(j["isTrigger"]);
    if (j.contains("friction")) col.friction = j["friction"].get<f32>();
    if (j.contains("bounciness")) col.bounciness = j["bounciness"].get<f32>();
    if (j.contains("categoryBits")) col.categoryBits = j["categoryBits"].get<u32>();
    if (j.contains("collisionMask")) col.collisionMask = j["collisionMask"].get<u32>();
    if (j.contains("vertices") && j["vertices"].is_array()) {
        for (const auto& vj : j["vertices"]) {
            col.vertices.push_back(DeserializeVector3(vj));
        }
        col.generated = !col.vertices.empty();
    }
    if (j.contains("indices") && j["indices"].is_array()) {
        col.indices = j["indices"].get<std::vector<u32>>();
    }
    return col;
}

// ============================================================================
// Mesh Renderer
// ============================================================================

json SerializeMeshRendererComponent(const ECS::MeshRendererComponent& mr) {
    json j;
    j["enabled"] = mr.enabled;
    j["frustumCull"] = mr.frustumCull;
    j["occlusionCull"] = mr.occlusionCull;
    j["maxDrawDistance"] = RF(mr.maxDrawDistance);
    j["renderQueue"] = mr.renderQueue;
    j["renderLayerMask"] = mr.renderLayerMask;
    j["lodBias"] = RF(mr.lodBias);
    j["shadowMode"] = static_cast<u8>(mr.shadowMode);
    j["contributeMotionVectors"] = mr.contributeMotionVectors;
    j["allowInstancing"] = mr.allowInstancing;
    j["wireframe"] = mr.wireframe;
    if (mr.wireframe) {
        j["wireframeColor"] = { RF(mr.wireframeColor.x), RF(mr.wireframeColor.y), RF(mr.wireframeColor.z) };
        j["wireframeOpacity"] = RF(mr.wireframeOpacity);
    }
    if (!mr.customShaderName.empty()) j["customShaderName"] = mr.customShaderName;
    return j;
}

ECS::MeshRendererComponent DeserializeMeshRendererComponent(const json& j) {
    ECS::MeshRendererComponent mr;
    if (j.contains("enabled")) mr.enabled = JB(j["enabled"]);
    if (j.contains("frustumCull")) mr.frustumCull = JB(j["frustumCull"]);
    if (j.contains("occlusionCull")) mr.occlusionCull = JB(j["occlusionCull"]);
    if (j.contains("maxDrawDistance")) mr.maxDrawDistance = j["maxDrawDistance"].get<f32>();
    if (j.contains("renderQueue")) mr.renderQueue = j["renderQueue"].get<i32>();
    if (j.contains("renderLayerMask")) mr.renderLayerMask = j["renderLayerMask"].get<u32>();
    if (j.contains("lodBias")) mr.lodBias = j["lodBias"].get<f32>();
    if (j.contains("shadowMode")) mr.shadowMode = static_cast<ECS::MeshRendererComponent::ShadowMode>(j["shadowMode"].get<u8>());
    if (j.contains("contributeMotionVectors")) mr.contributeMotionVectors = JB(j["contributeMotionVectors"]);
    if (j.contains("allowInstancing")) mr.allowInstancing = JB(j["allowInstancing"]);
    if (j.contains("wireframe")) mr.wireframe = JB(j["wireframe"]);
    if (j.contains("wireframeColor") && j["wireframeColor"].is_array() && j["wireframeColor"].size() == 3) {
        mr.wireframeColor = Math::Vector3(j["wireframeColor"][0].get<f32>(), j["wireframeColor"][1].get<f32>(), j["wireframeColor"][2].get<f32>());
    }
    if (j.contains("wireframeOpacity")) mr.wireframeOpacity = j["wireframeOpacity"].get<f32>();
    if (j.contains("customShaderName")) mr.customShaderName = j["customShaderName"].get<std::string>();
    return mr;
}

// ============================================================================
// Health & Damage
// ============================================================================

json SerializeHealthComponent(const ECS::HealthComponent& h) {
    json j;
    j["maxHealth"] = RF(h.maxHealth);
    j["regenRate"] = RF(h.regenRate);
    j["regenDelay"] = RF(h.regenDelay);
    j["isInvulnerable"] = RF(h.isInvulnerable);
    j["invulnerabilityTime"] = RF(h.invulnerabilityTime);
    j["maxShield"] = RF(h.maxShield);
    j["shieldRegenRate"] = RF(h.shieldRegenRate);
    j["shieldRegenDelay"] = RF(h.shieldRegenDelay);
    // R2 fix: serialize runtime state for mid-play save/load
    j["currentHealth"] = RF(h.currentHealth);
    j["currentShield"] = RF(h.currentShield);
    j["isDead"] = h.isDead;
    j["invulnerabilityTimer"] = RF(h.invulnerabilityTimer);
    j["timeSinceLastDamage"] = RF(h.timeSinceLastDamage);
    return j;
}

ECS::HealthComponent DeserializeHealthComponent(const json& j) {
    ECS::HealthComponent h;
    if (j.contains("maxHealth")) h.maxHealth = j["maxHealth"].get<f32>();
    h.currentHealth = h.maxHealth;  // Default: full health
    if (j.contains("regenRate")) h.regenRate = j["regenRate"].get<f32>();
    if (j.contains("regenDelay")) h.regenDelay = j["regenDelay"].get<f32>();
    if (j.contains("isInvulnerable")) h.isInvulnerable = JB(j["isInvulnerable"]);
    if (j.contains("invulnerabilityTime")) h.invulnerabilityTime = j["invulnerabilityTime"].get<f32>();
    if (j.contains("maxShield")) h.maxShield = j["maxShield"].get<f32>();
    h.currentShield = h.maxShield;  // Default: full shield
    if (j.contains("shieldRegenRate")) h.shieldRegenRate = j["shieldRegenRate"].get<f32>();
    if (j.contains("shieldRegenDelay")) h.shieldRegenDelay = j["shieldRegenDelay"].get<f32>();
    // R2 fix: restore runtime state if present (mid-play save/load)
    if (j.contains("currentHealth")) h.currentHealth = j["currentHealth"].get<f32>();
    if (j.contains("currentShield")) h.currentShield = j["currentShield"].get<f32>();
    if (j.contains("isDead")) h.isDead = JB(j["isDead"]);
    if (j.contains("invulnerabilityTimer")) h.invulnerabilityTimer = j["invulnerabilityTimer"].get<f32>();
    if (j.contains("timeSinceLastDamage")) h.timeSinceLastDamage = j["timeSinceLastDamage"].get<f32>();
    return h;
}

// ============================================================================
// Record & Rewind (configuration only — runtime state is transient)
// ============================================================================

json SerializeRecordRewindComponent(const ECS::RecordRewindComponent& rr) {
    json j;
    j["maxDuration"] = RF(rr.maxDuration);
    j["recordInterval"] = RF(rr.recordInterval);
    j["rewindSpeed"] = RF(rr.rewindSpeed);
    j["cooldown"] = RF(rr.cooldown);
    j["rewindKey"] = rr.rewindKey;
    j["channels"] = rr.channels;
    j["enabled"] = rr.enabled;
    j["vignetteStrength"] = RF(rr.rewindVignetteStrength);
    j["rewindTint"] = { RF(rr.rewindTint.x), RF(rr.rewindTint.y), RF(rr.rewindTint.z) };
    return j;
}

ECS::RecordRewindComponent DeserializeRecordRewindComponent(const json& j) {
    ECS::RecordRewindComponent rr;
    if (j.contains("maxDuration")) rr.maxDuration = j["maxDuration"].get<f32>();
    if (j.contains("recordInterval")) rr.recordInterval = j["recordInterval"].get<f32>();
    if (j.contains("rewindSpeed")) rr.rewindSpeed = j["rewindSpeed"].get<f32>();
    if (j.contains("cooldown")) rr.cooldown = j["cooldown"].get<f32>();
    if (j.contains("rewindKey")) rr.rewindKey = j["rewindKey"].get<i32>();
    if (j.contains("channels")) rr.channels = j["channels"].get<u32>();
    if (j.contains("enabled")) rr.enabled = JB(j["enabled"]);
    if (j.contains("vignetteStrength")) rr.rewindVignetteStrength = j["vignetteStrength"].get<f32>();
    if (j.contains("rewindTint") && j["rewindTint"].is_array() && j["rewindTint"].size() == 3) {
        rr.rewindTint = Math::Vector3(j["rewindTint"][0].get<f32>(), j["rewindTint"][1].get<f32>(), j["rewindTint"][2].get<f32>());
    }
    return rr;
}

json SerializeSceneRewindComponent(const ECS::SceneRewindComponent& sr) {
    json j;
    j["maxDuration"] = RF(sr.maxDuration);
    j["recordInterval"] = RF(sr.recordInterval);
    j["rewindSpeed"] = RF(sr.rewindSpeed);
    j["cooldown"] = RF(sr.cooldown);
    j["charges"] = sr.charges;
    j["rewindKey"] = sr.rewindKey;
    j["channels"] = sr.channels;
    j["keyframeInterval"] = sr.keyframeInterval;
    j["enabled"] = sr.enabled;
    j["deltaCompression"] = sr.deltaCompression;
    j["vignetteStrength"] = RF(sr.rewindVignetteStrength);
    j["rewindTint"] = { RF(sr.rewindTint.x), RF(sr.rewindTint.y), RF(sr.rewindTint.z) };
    return j;
}

ECS::SceneRewindComponent DeserializeSceneRewindComponent(const json& j) {
    ECS::SceneRewindComponent sr;
    if (j.contains("maxDuration")) sr.maxDuration = j["maxDuration"].get<f32>();
    if (j.contains("recordInterval")) sr.recordInterval = j["recordInterval"].get<f32>();
    if (j.contains("rewindSpeed")) sr.rewindSpeed = j["rewindSpeed"].get<f32>();
    if (j.contains("cooldown")) sr.cooldown = j["cooldown"].get<f32>();
    if (j.contains("charges")) sr.charges = j["charges"].get<i32>();
    if (j.contains("rewindKey")) sr.rewindKey = j["rewindKey"].get<i32>();
    if (j.contains("channels")) sr.channels = j["channels"].get<u32>();
    if (j.contains("keyframeInterval")) sr.keyframeInterval = j["keyframeInterval"].get<u32>();
    if (j.contains("enabled")) sr.enabled = JB(j["enabled"]);
    if (j.contains("deltaCompression")) sr.deltaCompression = JB(j["deltaCompression"]);
    if (j.contains("vignetteStrength")) sr.rewindVignetteStrength = j["vignetteStrength"].get<f32>();
    if (j.contains("rewindTint") && j["rewindTint"].is_array() && j["rewindTint"].size() == 3) {
        sr.rewindTint = Math::Vector3(j["rewindTint"][0].get<f32>(), j["rewindTint"][1].get<f32>(), j["rewindTint"][2].get<f32>());
    }
    return sr;
}

// ============================================================================
// Audio-Reactive Components (16 serializers)
// ============================================================================

json SerializeAudioReactiveComponent(const ECS::AudioReactiveComponent& ar) {
    json j;
    j["busName"] = ar.busName;
    j["target"] = static_cast<u8>(ar.target);
    j["threshold"] = RF(ar.threshold);
    j["multiplier"] = RF(ar.multiplier);
    j["smoothing"] = RF(ar.smoothing);
    j["invert"] = ar.invert;
    j["baseValue"] = RF(ar.baseValue);
    j["maxValue"] = RF(ar.maxValue);
    j["enabled"] = ar.enabled;
    return j;
}
ECS::AudioReactiveComponent DeserializeAudioReactiveComponent(const json& j) {
    ECS::AudioReactiveComponent ar;
    if (j.contains("busName")) ar.busName = j["busName"].get<std::string>();
    if (j.contains("target")) ar.target = static_cast<ECS::AudioTargetProperty>(j["target"].get<u8>());
    if (j.contains("threshold")) ar.threshold = j["threshold"].get<f32>();
    if (j.contains("multiplier")) ar.multiplier = j["multiplier"].get<f32>();
    if (j.contains("smoothing")) ar.smoothing = j["smoothing"].get<f32>();
    if (j.contains("invert")) ar.invert = JB(j["invert"]);
    if (j.contains("baseValue")) ar.baseValue = j["baseValue"].get<f32>();
    if (j.contains("maxValue")) ar.maxValue = j["maxValue"].get<f32>();
    if (j.contains("enabled")) ar.enabled = JB(j["enabled"]);
    return ar;
}

json SerializeAudioThresholdTriggerComponent(const ECS::AudioThresholdTriggerComponent& t) {
    json j;
    j["busName"] = t.busName;
    j["threshold"] = RF(t.threshold);
    j["cooldown"] = RF(t.cooldown);
    j["action"] = static_cast<u8>(t.action);
    j["effectDuration"] = RF(t.effectDuration);
    j["effectIntensity"] = RF(t.effectIntensity);
    j["eventName"] = t.eventName;
    j["enabled"] = t.enabled;
    return j;
}
ECS::AudioThresholdTriggerComponent DeserializeAudioThresholdTriggerComponent(const json& j) {
    ECS::AudioThresholdTriggerComponent t;
    if (j.contains("busName")) t.busName = j["busName"].get<std::string>();
    if (j.contains("threshold")) t.threshold = j["threshold"].get<f32>();
    if (j.contains("cooldown")) t.cooldown = j["cooldown"].get<f32>();
    if (j.contains("action")) t.action = static_cast<ECS::AudioThresholdTriggerComponent::Action>(j["action"].get<u8>());
    if (j.contains("effectDuration")) t.effectDuration = j["effectDuration"].get<f32>();
    if (j.contains("effectIntensity")) t.effectIntensity = j["effectIntensity"].get<f32>();
    if (j.contains("eventName")) t.eventName = j["eventName"].get<std::string>();
    if (j.contains("enabled")) t.enabled = JB(j["enabled"]);
    return t;
}

json SerializeRTPCComponent(const ECS::RTPCComponent& r) {
    json j;
    j["enabled"] = r.enabled;
    json mappings = json::array();
    for (const auto& m : r.mappings) {
        json mj;
        mj["parameterName"] = m.parameterName;
        mj["paramMin"] = RF(m.paramMin);
        mj["paramMax"] = RF(m.paramMax);
        mj["audioTarget"] = static_cast<u8>(m.audioTarget);
        mj["outputMin"] = RF(m.outputMin);
        mj["outputMax"] = RF(m.outputMax);
        mj["curve"] = RF(m.curve);
        mappings.push_back(mj);
    }
    j["mappings"] = mappings;
    return j;
}
ECS::RTPCComponent DeserializeRTPCComponent(const json& j) {
    ECS::RTPCComponent r;
    if (j.contains("enabled")) r.enabled = JB(j["enabled"]);
    if (j.contains("mappings") && j["mappings"].is_array()) {
        for (const auto& mj : j["mappings"]) {
            ECS::RTPCComponent::Mapping m;
            if (mj.contains("parameterName")) m.parameterName = mj["parameterName"].get<std::string>();
            if (mj.contains("paramMin")) m.paramMin = mj["paramMin"].get<f32>();
            if (mj.contains("paramMax")) m.paramMax = mj["paramMax"].get<f32>();
            if (mj.contains("audioTarget")) m.audioTarget = static_cast<ECS::RTPCComponent::Mapping::AudioTarget>(mj["audioTarget"].get<u8>());
            if (mj.contains("outputMin")) m.outputMin = mj["outputMin"].get<f32>();
            if (mj.contains("outputMax")) m.outputMax = mj["outputMax"].get<f32>();
            if (mj.contains("curve")) m.curve = mj["curve"].get<f32>();
            r.mappings.push_back(m);
        }
    }
    return r;
}

json SerializeBeatClockComponent(const ECS::BeatClockComponent& bc) {
    json j;
    j["bpm"] = RF(bc.bpm);
    j["beatsPerBar"] = bc.beatsPerBar;
    j["playing"] = bc.playing;
    return j;
}
ECS::BeatClockComponent DeserializeBeatClockComponent(const json& j) {
    ECS::BeatClockComponent bc;
    if (j.contains("bpm")) bc.bpm = j["bpm"].get<f32>();
    if (j.contains("beatsPerBar")) bc.beatsPerBar = j["beatsPerBar"].get<i32>();
    if (j.contains("playing")) bc.playing = JB(j["playing"]);
    return bc;
}

json SerializeBeatSyncComponent(const ECS::BeatSyncComponent& bs) {
    json j;
    j["mode"] = static_cast<u8>(bs.mode);
    j["beatDivisor"] = bs.beatDivisor;
    j["target"] = static_cast<u8>(bs.target);
    j["baseValue"] = RF(bs.baseValue);
    j["pulseValue"] = RF(bs.pulseValue);
    j["decaySpeed"] = RF(bs.decaySpeed);
    j["enabled"] = bs.enabled;
    return j;
}
ECS::BeatSyncComponent DeserializeBeatSyncComponent(const json& j) {
    ECS::BeatSyncComponent bs;
    if (j.contains("mode")) bs.mode = static_cast<ECS::BeatSyncComponent::SyncMode>(j["mode"].get<u8>());
    if (j.contains("beatDivisor")) bs.beatDivisor = j["beatDivisor"].get<u32>();
    if (j.contains("target")) bs.target = static_cast<ECS::AudioTargetProperty>(j["target"].get<u8>());
    if (j.contains("baseValue")) bs.baseValue = j["baseValue"].get<f32>();
    if (j.contains("pulseValue")) bs.pulseValue = j["pulseValue"].get<f32>();
    if (j.contains("decaySpeed")) bs.decaySpeed = j["decaySpeed"].get<f32>();
    if (j.contains("enabled")) bs.enabled = JB(j["enabled"]);
    return bs;
}

json SerializeConductorComponent(const ECS::ConductorComponent& c) {
    json j;
    j["enabled"] = c.enabled;
    j["masterVolume"] = RF(c.masterVolume);
    j["crossfadeTime"] = RF(c.crossfadeTime);
    j["autoDetect"] = c.autoDetect;
    j["combatRadius"] = RF(c.combatRadius);
    j["stateChangeDelay"] = RF(c.stateChangeDelay);
    json stems = json::array();
    for (const auto& s : c.stems) {
        json sj;
        sj["clipPath"] = s.clipPath;
        sj["name"] = s.name;
        sj["fadeSpeed"] = RF(s.fadeSpeed);
        sj["playDuringExplore"] = s.playDuringExplore;
        sj["playDuringCombat"] = s.playDuringCombat;
        sj["playDuringStealth"] = s.playDuringStealth;
        sj["playDuringCutscene"] = s.playDuringCutscene;
        stems.push_back(sj);
    }
    j["stems"] = stems;
    return j;
}
ECS::ConductorComponent DeserializeConductorComponent(const json& j) {
    ECS::ConductorComponent c;
    if (j.contains("enabled")) c.enabled = JB(j["enabled"]);
    if (j.contains("masterVolume")) c.masterVolume = j["masterVolume"].get<f32>();
    if (j.contains("crossfadeTime")) c.crossfadeTime = j["crossfadeTime"].get<f32>();
    if (j.contains("autoDetect")) c.autoDetect = JB(j["autoDetect"]);
    if (j.contains("combatRadius")) c.combatRadius = j["combatRadius"].get<f32>();
    if (j.contains("stateChangeDelay")) c.stateChangeDelay = j["stateChangeDelay"].get<f32>();
    if (j.contains("stems") && j["stems"].is_array()) {
        for (const auto& sj : j["stems"]) {
            ECS::ConductorComponent::Stem s;
            if (sj.contains("clipPath")) s.clipPath = sj["clipPath"].get<std::string>();
            if (sj.contains("name")) s.name = sj["name"].get<std::string>();
            if (sj.contains("fadeSpeed")) s.fadeSpeed = sj["fadeSpeed"].get<f32>();
            if (sj.contains("playDuringExplore")) s.playDuringExplore = JB(sj["playDuringExplore"]);
            if (sj.contains("playDuringCombat")) s.playDuringCombat = JB(sj["playDuringCombat"]);
            if (sj.contains("playDuringStealth")) s.playDuringStealth = JB(sj["playDuringStealth"]);
            if (sj.contains("playDuringCutscene")) s.playDuringCutscene = JB(sj["playDuringCutscene"]);
            c.stems.push_back(s);
        }
    }
    return c;
}

json SerializeAudioCollisionComponent(const ECS::AudioCollisionComponent& ac) {
    json j;
    j["material"] = static_cast<u8>(ac.material);
    j["impactSoftClip"] = ac.impactSoftClip;
    j["impactHardClip"] = ac.impactHardClip;
    j["scrapeClip"] = ac.scrapeClip;
    j["rollClip"] = ac.rollClip;
    j["softThreshold"] = RF(ac.softThreshold);
    j["hardThreshold"] = RF(ac.hardThreshold);
    j["volumeScale"] = RF(ac.volumeScale);
    j["pitchVariance"] = RF(ac.pitchVariance);
    j["cooldown"] = RF(ac.cooldown);
    j["hollowness"] = RF(ac.hollowness);
    j["mass"] = RF(ac.mass);
    j["detailDistance"] = RF(ac.detailDistance);
    j["cullDistance"] = RF(ac.cullDistance);
    j["enabled"] = ac.enabled;
    return j;
}
ECS::AudioCollisionComponent DeserializeAudioCollisionComponent(const json& j) {
    ECS::AudioCollisionComponent ac;
    if (j.contains("material")) ac.material = static_cast<ECS::SurfaceMaterial>(j["material"].get<u8>());
    if (j.contains("impactSoftClip")) ac.impactSoftClip = j["impactSoftClip"].get<std::string>();
    if (j.contains("impactHardClip")) ac.impactHardClip = j["impactHardClip"].get<std::string>();
    if (j.contains("scrapeClip")) ac.scrapeClip = j["scrapeClip"].get<std::string>();
    if (j.contains("rollClip")) ac.rollClip = j["rollClip"].get<std::string>();
    if (j.contains("softThreshold")) ac.softThreshold = j["softThreshold"].get<f32>();
    if (j.contains("hardThreshold")) ac.hardThreshold = j["hardThreshold"].get<f32>();
    if (j.contains("volumeScale")) ac.volumeScale = j["volumeScale"].get<f32>();
    if (j.contains("pitchVariance")) ac.pitchVariance = j["pitchVariance"].get<f32>();
    if (j.contains("cooldown")) ac.cooldown = j["cooldown"].get<f32>();
    if (j.contains("hollowness")) ac.hollowness = j["hollowness"].get<f32>();
    if (j.contains("mass")) ac.mass = j["mass"].get<f32>();
    if (j.contains("detailDistance")) ac.detailDistance = j["detailDistance"].get<f32>();
    if (j.contains("cullDistance")) ac.cullDistance = j["cullDistance"].get<f32>();
    if (j.contains("enabled")) ac.enabled = JB(j["enabled"]);
    return ac;
}

json SerializeSidechainComponent(const ECS::SidechainComponent& sc) {
    json j;
    j["sourceBus"] = sc.sourceBus;
    j["targetBus"] = sc.targetBus;
    j["threshold"] = RF(sc.threshold);
    j["ratio"] = RF(sc.ratio);
    j["attackTime"] = RF(sc.attackTime);
    j["releaseTime"] = RF(sc.releaseTime);
    j["holdTime"] = RF(sc.holdTime);
    j["enabled"] = sc.enabled;
    return j;
}
ECS::SidechainComponent DeserializeSidechainComponent(const json& j) {
    ECS::SidechainComponent sc;
    if (j.contains("sourceBus")) sc.sourceBus = j["sourceBus"].get<std::string>();
    if (j.contains("targetBus")) sc.targetBus = j["targetBus"].get<std::string>();
    if (j.contains("threshold")) sc.threshold = j["threshold"].get<f32>();
    if (j.contains("ratio")) sc.ratio = j["ratio"].get<f32>();
    if (j.contains("attackTime")) sc.attackTime = j["attackTime"].get<f32>();
    if (j.contains("releaseTime")) sc.releaseTime = j["releaseTime"].get<f32>();
    if (j.contains("holdTime")) sc.holdTime = j["holdTime"].get<f32>();
    if (j.contains("enabled")) sc.enabled = JB(j["enabled"]);
    return sc;
}

json SerializeReverbZoneComponent(const ECS::ReverbZoneComponent& rz) {
    json j;
    j["shape"] = static_cast<u8>(rz.shape);
    j["halfExtents"] = {RF(rz.halfExtents.x), RF(rz.halfExtents.y), RF(rz.halfExtents.z)};
    j["priority"] = rz.priority;
    j["isActive"] = rz.isActive;
    j["isGlobal"] = rz.isGlobal;
    j["blendRadius"] = RF(rz.blendRadius);
    j["roomSize"] = RF(rz.roomSize);
    j["damping"] = RF(rz.damping);
    j["wetDryMix"] = RF(rz.wetDryMix);
    j["decayTime"] = RF(rz.decayTime);
    j["preDelay"] = RF(rz.preDelay);
    j["preset"] = static_cast<u8>(rz.preset);
    return j;
}
ECS::ReverbZoneComponent DeserializeReverbZoneComponent(const json& j) {
    ECS::ReverbZoneComponent rz;
    if (j.contains("shape")) rz.shape = static_cast<ECS::ReverbZoneComponent::Shape>(j["shape"].get<u8>());
    if (j.contains("halfExtents") && j["halfExtents"].is_array() && j["halfExtents"].size() == 3)
        rz.halfExtents = Math::Vector3(j["halfExtents"][0].get<f32>(), j["halfExtents"][1].get<f32>(), j["halfExtents"][2].get<f32>());
    if (j.contains("priority")) rz.priority = j["priority"].get<i32>();
    if (j.contains("isActive")) rz.isActive = JB(j["isActive"]);
    if (j.contains("isGlobal")) rz.isGlobal = JB(j["isGlobal"]);
    if (j.contains("blendRadius")) rz.blendRadius = j["blendRadius"].get<f32>();
    if (j.contains("roomSize")) rz.roomSize = j["roomSize"].get<f32>();
    if (j.contains("damping")) rz.damping = j["damping"].get<f32>();
    if (j.contains("wetDryMix")) rz.wetDryMix = j["wetDryMix"].get<f32>();
    if (j.contains("decayTime")) rz.decayTime = j["decayTime"].get<f32>();
    if (j.contains("preDelay")) rz.preDelay = j["preDelay"].get<f32>();
    if (j.contains("preset")) rz.preset = static_cast<ECS::ReverbZoneComponent::Preset>(j["preset"].get<u8>());
    return rz;
}

json SerializeMusicZoneComponent(const ECS::MusicZoneComponent& mz) {
    json j;
    j["trackPath"] = mz.trackPath;
    j["fadeInTime"] = RF(mz.fadeInTime);
    j["fadeOutTime"] = RF(mz.fadeOutTime);
    j["priority"] = mz.priority;
    j["halfExtents"] = {RF(mz.halfExtents.x), RF(mz.halfExtents.y), RF(mz.halfExtents.z)};
    j["isActive"] = mz.isActive;
    return j;
}
ECS::MusicZoneComponent DeserializeMusicZoneComponent(const json& j) {
    ECS::MusicZoneComponent mz;
    if (j.contains("trackPath")) mz.trackPath = j["trackPath"].get<std::string>();
    if (j.contains("fadeInTime")) mz.fadeInTime = j["fadeInTime"].get<f32>();
    if (j.contains("fadeOutTime")) mz.fadeOutTime = j["fadeOutTime"].get<f32>();
    if (j.contains("priority")) mz.priority = j["priority"].get<i32>();
    if (j.contains("halfExtents") && j["halfExtents"].is_array() && j["halfExtents"].size() == 3)
        mz.halfExtents = Math::Vector3(j["halfExtents"][0].get<f32>(), j["halfExtents"][1].get<f32>(), j["halfExtents"][2].get<f32>());
    if (j.contains("isActive")) mz.isActive = JB(j["isActive"]);
    return mz;
}

json SerializeAudioSnapshotTriggerComponent(const ECS::AudioSnapshotTriggerComponent& st) {
    json j;
    j["snapshotName"] = st.snapshotName;
    j["halfExtents"] = {RF(st.halfExtents.x), RF(st.halfExtents.y), RF(st.halfExtents.z)};
    j["isActive"] = st.isActive;
    return j;
}
ECS::AudioSnapshotTriggerComponent DeserializeAudioSnapshotTriggerComponent(const json& j) {
    ECS::AudioSnapshotTriggerComponent st;
    if (j.contains("snapshotName")) st.snapshotName = j["snapshotName"].get<std::string>();
    if (j.contains("halfExtents") && j["halfExtents"].is_array() && j["halfExtents"].size() == 3)
        st.halfExtents = Math::Vector3(j["halfExtents"][0].get<f32>(), j["halfExtents"][1].get<f32>(), j["halfExtents"][2].get<f32>());
    if (j.contains("isActive")) st.isActive = JB(j["isActive"]);
    return st;
}

json SerializeAudioOcclusionComponent(const ECS::AudioOcclusionComponent& oc) {
    json j;
    j["enabled"] = oc.enabled;
    j["lowPassCutoff"] = RF(oc.lowPassCutoff);
    j["volumeReduction"] = RF(oc.volumeReduction);
    j["updateRate"] = RF(oc.updateRate);
    return j;
}
ECS::AudioOcclusionComponent DeserializeAudioOcclusionComponent(const json& j) {
    ECS::AudioOcclusionComponent oc;
    if (j.contains("enabled")) oc.enabled = JB(j["enabled"]);
    if (j.contains("lowPassCutoff")) oc.lowPassCutoff = j["lowPassCutoff"].get<f32>();
    if (j.contains("volumeReduction")) oc.volumeReduction = j["volumeReduction"].get<f32>();
    if (j.contains("updateRate")) oc.updateRate = j["updateRate"].get<f32>();
    return oc;
}

json SerializePoseLibraryComponent(const ECS::PoseLibraryComponent& pl) {
    json j;
    j["blendWeight"] = RF(pl.blendWeight);
    j["blendSpeed"] = RF(pl.blendSpeed);
    j["activePose"] = pl.activePose;
    json poses = json::array();
    for (const auto& p : pl.poses) {
        json pj;
        pj["name"] = p.name;
        pj["category"] = p.category;
        json overrides = json::array();
        for (const auto& o : p.overrides) {
            json oj;
            oj["boneName"] = o.boneName;
            oj["rotation"] = {RF(o.rotation.x), RF(o.rotation.y), RF(o.rotation.z), RF(o.rotation.w)};
            oj["weight"] = RF(o.weight);
            overrides.push_back(oj);
        }
        pj["overrides"] = overrides;
        poses.push_back(pj);
    }
    j["poses"] = poses;
    return j;
}
ECS::PoseLibraryComponent DeserializePoseLibraryComponent(const json& j) {
    ECS::PoseLibraryComponent pl;
    if (j.contains("blendWeight")) pl.blendWeight = j["blendWeight"].get<f32>();
    if (j.contains("blendSpeed")) pl.blendSpeed = j["blendSpeed"].get<f32>();
    if (j.contains("activePose")) pl.activePose = j["activePose"].get<std::string>();
    if (j.contains("poses") && j["poses"].is_array()) {
        for (const auto& pj : j["poses"]) {
            ECS::PoseLibraryComponent::NamedPose p;
            if (pj.contains("name")) p.name = pj["name"].get<std::string>();
            if (pj.contains("category")) p.category = pj["category"].get<std::string>();
            if (pj.contains("overrides") && pj["overrides"].is_array()) {
                for (const auto& oj : pj["overrides"]) {
                    ECS::PoseLibraryComponent::BoneOverride o;
                    if (oj.contains("boneName")) o.boneName = oj["boneName"].get<std::string>();
                    if (oj.contains("rotation") && oj["rotation"].is_array() && oj["rotation"].size() == 4)
                        o.rotation = Math::Quaternion(oj["rotation"][0].get<f32>(), oj["rotation"][1].get<f32>(), oj["rotation"][2].get<f32>(), oj["rotation"][3].get<f32>());
                    if (oj.contains("weight")) o.weight = oj["weight"].get<f32>();
                    p.overrides.push_back(o);
                }
            }
            pl.poses.push_back(p);
        }
    }
    return pl;
}

json SerializeAudioFidelityComponent(const ECS::AudioFidelityComponent& af) {
    json j;
    j["mode"] = static_cast<u8>(af.mode);
    j["intensity"] = RF(af.intensity);
    j["autoMatchArtStyle"] = af.autoMatchArtStyle;
    j["sampleRateReduction"] = RF(af.sampleRateReduction);
    j["bitDepthReduction"] = RF(af.bitDepthReduction);
    j["lowPassCutoff"] = RF(af.lowPassCutoff);
    j["noiseFloor"] = RF(af.noiseFloor);
    j["wobble"] = RF(af.wobble);
    j["wobbleSpeed"] = RF(af.wobbleSpeed);
    j["saturation"] = RF(af.saturation);
    j["stereoWidth"] = RF(af.stereoWidth);
    j["enabled"] = af.enabled;
    return j;
}
ECS::AudioFidelityComponent DeserializeAudioFidelityComponent(const json& j) {
    ECS::AudioFidelityComponent af;
    if (j.contains("mode")) af.mode = static_cast<ECS::AudioFidelityMode>(j["mode"].get<u8>());
    if (j.contains("intensity")) af.intensity = j["intensity"].get<f32>();
    if (j.contains("autoMatchArtStyle")) af.autoMatchArtStyle = JB(j["autoMatchArtStyle"]);
    if (j.contains("sampleRateReduction")) af.sampleRateReduction = j["sampleRateReduction"].get<f32>();
    if (j.contains("bitDepthReduction")) af.bitDepthReduction = j["bitDepthReduction"].get<f32>();
    if (j.contains("lowPassCutoff")) af.lowPassCutoff = j["lowPassCutoff"].get<f32>();
    if (j.contains("noiseFloor")) af.noiseFloor = j["noiseFloor"].get<f32>();
    if (j.contains("wobble")) af.wobble = j["wobble"].get<f32>();
    if (j.contains("wobbleSpeed")) af.wobbleSpeed = j["wobbleSpeed"].get<f32>();
    if (j.contains("saturation")) af.saturation = j["saturation"].get<f32>();
    if (j.contains("stereoWidth")) af.stereoWidth = j["stereoWidth"].get<f32>();
    if (j.contains("enabled")) af.enabled = JB(j["enabled"]);
    return af;
}

json SerializeMIDIBindingComponent(const ECS::MIDIBindingComponent& mb) {
    json j;
    j["enabled"] = mb.enabled;
    json bindings = json::array();
    for (const auto& b : mb.bindings) {
        json bj;
        bj["source"] = static_cast<u8>(b.source);
        bj["midiCC"] = b.midiCC;
        bj["midiChannel"] = b.midiChannel;
        bj["target"] = static_cast<u8>(b.target);
        bj["customTarget"] = b.customTarget;
        bj["outputMin"] = RF(b.outputMin);
        bj["outputMax"] = RF(b.outputMax);
        bj["smoothing"] = RF(b.smoothing);
        bindings.push_back(bj);
    }
    j["bindings"] = bindings;
    return j;
}
ECS::MIDIBindingComponent DeserializeMIDIBindingComponent(const json& j) {
    ECS::MIDIBindingComponent mb;
    if (j.contains("enabled")) mb.enabled = JB(j["enabled"]);
    if (j.contains("bindings") && j["bindings"].is_array()) {
        for (const auto& bj : j["bindings"]) {
            ECS::MIDIBindingComponent::Binding b;
            if (bj.contains("source")) b.source = static_cast<ECS::MIDIBindingComponent::Binding::Source>(bj["source"].get<u8>());
            if (bj.contains("midiCC")) b.midiCC = bj["midiCC"].get<u8>();
            if (bj.contains("midiChannel")) b.midiChannel = bj["midiChannel"].get<u8>();
            if (bj.contains("target")) b.target = static_cast<ECS::AudioTargetProperty>(bj["target"].get<u8>());
            if (bj.contains("customTarget")) b.customTarget = bj["customTarget"].get<std::string>();
            if (bj.contains("outputMin")) b.outputMin = bj["outputMin"].get<f32>();
            if (bj.contains("outputMax")) b.outputMax = bj["outputMax"].get<f32>();
            if (bj.contains("smoothing")) b.smoothing = bj["smoothing"].get<f32>();
            mb.bindings.push_back(b);
        }
    }
    return mb;
}

json SerializeMaterialInteractionTableComponent(const ECS::MaterialInteractionTableComponent& mit) {
    json j;
    json arr = json::array();
    for (const auto& inter : mit.interactions) {
        json ij;
        ij["a"] = static_cast<u8>(inter.a);
        ij["b"] = static_cast<u8>(inter.b);
        ij["softClip"] = inter.softClip;
        ij["hardClip"] = inter.hardClip;
        ij["scrapeClip"] = inter.scrapeClip;
        ij["pitchOffset"] = RF(inter.pitchOffset);
        ij["volumeMultiplier"] = RF(inter.volumeMultiplier);
        arr.push_back(ij);
    }
    j["interactions"] = arr;
    return j;
}
ECS::MaterialInteractionTableComponent DeserializeMaterialInteractionTableComponent(const json& j) {
    ECS::MaterialInteractionTableComponent mit;
    if (j.contains("interactions") && j["interactions"].is_array()) {
        for (const auto& ij : j["interactions"]) {
            ECS::MaterialInteractionTableComponent::Interaction inter;
            if (ij.contains("a")) inter.a = static_cast<ECS::SurfaceMaterial>(ij["a"].get<u8>());
            if (ij.contains("b")) inter.b = static_cast<ECS::SurfaceMaterial>(ij["b"].get<u8>());
            if (ij.contains("softClip")) inter.softClip = ij["softClip"].get<std::string>();
            if (ij.contains("hardClip")) inter.hardClip = ij["hardClip"].get<std::string>();
            if (ij.contains("scrapeClip")) inter.scrapeClip = ij["scrapeClip"].get<std::string>();
            if (ij.contains("pitchOffset")) inter.pitchOffset = ij["pitchOffset"].get<f32>();
            if (ij.contains("volumeMultiplier")) inter.volumeMultiplier = ij["volumeMultiplier"].get<f32>();
            mit.interactions.push_back(inter);
        }
    }
    return mit;
}

json SerializeDamageComponent(const ECS::DamageComponent& d) {
    json j;
    j["damage"] = RF(d.damage);
    j["knockbackForce"] = RF(d.knockbackForce);
    j["destroyOnHit"] = RF(d.destroyOnHit);
    j["damageOnce"] = RF(d.damageOnce);
    j["damageInterval"] = RF(d.damageInterval);
    j["type"] = static_cast<u8>(d.type);
    // R5 fix: serialize runtime tracking for mid-play save/load
    if (!d.damagedEntities.empty()) {
        json arr = json::array();
        for (auto e : d.damagedEntities) arr.push_back(static_cast<u64>(e));
        j["damagedEntities"] = arr;
    }
    return j;
}

ECS::DamageComponent DeserializeDamageComponent(const json& j) {
    ECS::DamageComponent d;
    if (j.contains("damage")) d.damage = j["damage"].get<f32>();
    if (j.contains("knockbackForce")) d.knockbackForce = j["knockbackForce"].get<f32>();
    if (j.contains("destroyOnHit")) d.destroyOnHit = JB(j["destroyOnHit"]);
    if (j.contains("damageOnce")) d.damageOnce = JB(j["damageOnce"]);
    if (j.contains("damageInterval")) d.damageInterval = j["damageInterval"].get<f32>();
    if (j.contains("type")) { u8 v = j["type"].get<u8>(); if (v <= 5) d.type = static_cast<ECS::DamageComponent::DamageType>(v); }
    // R5 fix: deserialize runtime tracking for mid-play save/load
    if (j.contains("damagedEntities") && j["damagedEntities"].is_array()) {
        for (auto& e : j["damagedEntities"]) {
            d.damagedEntities.push_back(static_cast<ECS::Entity>(e.get<u64>()));
        }
    }
    return d;
}

// ============================================================================
// Game Over
// ============================================================================

json SerializeGameOverComponent(const ECS::GameOverComponent& go) {
    json j;
    j["delay"] = RF(go.delay);
    j["victoryMessage"] = go.victoryMessage;
    j["defeatMessage"] = go.defeatMessage;
    j["allowRestart"] = go.allowRestart;
    j["returnToMenu"] = go.returnToMenu;
    j["victoryOnAllEnemiesDefeated"] = go.victoryOnAllEnemiesDefeated;
    j["victoryTriggerEntity"] = static_cast<u64>(go.victoryTriggerEntity);
    j["victoryRequiresAllCoins"] = go.victoryRequiresAllCoins;
    return j;
}

ECS::GameOverComponent DeserializeGameOverComponent(const json& j) {
    ECS::GameOverComponent go;
    if (j.contains("delay")) go.delay = j["delay"].get<f32>();
    if (j.contains("victoryMessage")) go.victoryMessage = j["victoryMessage"].get<std::string>();
    if (j.contains("defeatMessage")) go.defeatMessage = j["defeatMessage"].get<std::string>();
    if (j.contains("allowRestart")) go.allowRestart = JB(j["allowRestart"]);
    if (j.contains("returnToMenu")) go.returnToMenu = JB(j["returnToMenu"]);
    if (j.contains("victoryOnAllEnemiesDefeated")) go.victoryOnAllEnemiesDefeated = JB(j["victoryOnAllEnemiesDefeated"]);
    if (j.contains("victoryTriggerEntity")) go.victoryTriggerEntity = static_cast<ECS::Entity>(j["victoryTriggerEntity"].get<u64>());
    if (j.contains("victoryRequiresAllCoins")) go.victoryRequiresAllCoins = JB(j["victoryRequiresAllCoins"]);
    return go;
}

// Camera lens effects (distortion/aberration/vignette). Was silently NEVER
// serialized -- editor-tuned lenses reset on every save/load.
json SerializeLensComponent(const ECS::LensComponent& lens) {
    json j;
    j["enabled"] = lens.enabled;
    j["type"] = static_cast<u32>(lens.type);
    j["distortion"] = RF(lens.distortion);
    j["anamorphicSqueeze"] = RF(lens.anamorphicSqueeze);
    j["chromaticAberration"] = RF(lens.chromaticAberration);
    j["vignetteIntensity"] = RF(lens.vignetteIntensity);
    j["vignetteSoftness"] = RF(lens.vignetteSoftness);
    return j;
}

ECS::LensComponent DeserializeLensComponent(const json& j) {
    ECS::LensComponent lens;
    if (j.contains("enabled")) lens.enabled = JB(j["enabled"]);
    if (j.contains("type")) {
        u32 t = j["type"].get<u32>();
        if (t < static_cast<u32>(ECS::LensType::COUNT)) lens.type = static_cast<ECS::LensType>(t);
    }
    if (j.contains("distortion")) lens.distortion = j["distortion"].get<f32>();
    if (j.contains("anamorphicSqueeze")) lens.anamorphicSqueeze = j["anamorphicSqueeze"].get<f32>();
    if (j.contains("chromaticAberration")) lens.chromaticAberration = j["chromaticAberration"].get<f32>();
    if (j.contains("vignetteIntensity")) lens.vignetteIntensity = j["vignetteIntensity"].get<f32>();
    if (j.contains("vignetteSoftness")) lens.vignetteSoftness = j["vignetteSoftness"].get<f32>();
    return lens;
}

// Morph targets (blend shapes). Deltas are per-vertex import data -- serialized
// like mesh vertices so a saved scene keeps its blend shapes; weights always saved.
json SerializeMorphTargetComponent(const ECS::MorphTargetComponent& morph, bool includeDeltas) {
    json j;
    json targetsJson = json::array();
    for (const auto& target : morph.targets) {
        json t;
        t["name"] = target.name;
        if (includeDeltas) {
            std::vector<f32> flat;
            flat.reserve(target.deltas.size() * 6);
            for (const auto& d : target.deltas) {
                flat.push_back(d.positionDelta.x); flat.push_back(d.positionDelta.y); flat.push_back(d.positionDelta.z);
                flat.push_back(d.normalDelta.x);   flat.push_back(d.normalDelta.y);   flat.push_back(d.normalDelta.z);
            }
            t["deltas"] = flat;
        }
        targetsJson.push_back(t);
    }
    j["targets"] = targetsJson;
    j["weights"] = morph.weights;
    return j;
}

ECS::MorphTargetComponent DeserializeMorphTargetComponent(const json& j) {
    ECS::MorphTargetComponent morph;
    if (j.contains("targets") && j["targets"].is_array()) {
        for (const auto& t : j["targets"]) {
            ECS::MorphTarget target;
            if (t.contains("name")) target.name = SafeStr(t["name"]);
            if (t.contains("deltas") && t["deltas"].is_array()) {
                const auto& flat = t["deltas"];
                usize count = flat.size() / 6;
                target.deltas.reserve(count);
                for (usize i = 0; i + 5 < flat.size(); i += 6) {
                    ECS::MorphTargetDelta d;
                    d.positionDelta = Math::Vector3(flat[i].get<f32>(), flat[i+1].get<f32>(), flat[i+2].get<f32>());
                    d.normalDelta   = Math::Vector3(flat[i+3].get<f32>(), flat[i+4].get<f32>(), flat[i+5].get<f32>());
                    target.deltas.push_back(d);
                }
            }
            morph.targets.push_back(std::move(target));
        }
    }
    if (j.contains("weights") && j["weights"].is_array()) {
        for (const auto& w : j["weights"]) morph.weights.push_back(w.get<f32>());
    }
    morph.weights.resize(morph.targets.size(), 0.0f);
    morph.weightsDirty = true;
    morph.deltasDirty = true;
    return morph;
}

// ============================================================================
// Triggers & Interaction
// ============================================================================

json SerializeTriggerZoneComponent(const ECS::TriggerZoneComponent& tz) {
    json j;
    j["shape"] = static_cast<u8>(tz.shape);
    j["boxSize"] = SerializeVector3(tz.boxSize);
    j["sphereRadius"] = RF(tz.sphereRadius);
    j["triggerMask"] = RF(tz.triggerMask);
    j["triggerOnce"] = RF(tz.triggerOnce);
    if (tz.onEnterNotify != 0) j["onEnterNotify"] = static_cast<u64>(tz.onEnterNotify);
    if (tz.onExitNotify != 0)  j["onExitNotify"]  = static_cast<u64>(tz.onExitNotify);
    if (tz.onStayNotify != 0)  j["onStayNotify"]  = static_cast<u64>(tz.onStayNotify);
    return j;
}

ECS::TriggerZoneComponent DeserializeTriggerZoneComponent(const json& j) {
    ECS::TriggerZoneComponent tz;
    if (j.contains("shape")) { u8 v = j["shape"].get<u8>(); if (v <= 1) tz.shape = static_cast<ECS::TriggerZoneComponent::Shape>(v); }
    if (j.contains("boxSize")) tz.boxSize = DeserializeVector3(j["boxSize"]);
    if (j.contains("sphereRadius")) tz.sphereRadius = j["sphereRadius"].get<f32>();
    if (j.contains("triggerMask")) tz.triggerMask = j["triggerMask"].get<u32>();
    if (j.contains("triggerOnce")) tz.triggerOnce = JB(j["triggerOnce"]);
    if (j.contains("onEnterNotify")) tz.onEnterNotify = static_cast<ECS::Entity>(j["onEnterNotify"].get<u64>());
    if (j.contains("onExitNotify"))  tz.onExitNotify  = static_cast<ECS::Entity>(j["onExitNotify"].get<u64>());
    if (j.contains("onStayNotify"))  tz.onStayNotify  = static_cast<ECS::Entity>(j["onStayNotify"].get<u64>());
    return tz;
}

json SerializeInteractableComponent(const ECS::InteractableComponent& ic) {
    json j;
    j["promptText"] = ic.promptText;
    j["interactionRange"] = RF(ic.interactionRange);
    j["requiresLookAt"] = RF(ic.requiresLookAt);
    j["lookAtAngle"] = RF(ic.lookAtAngle);
    j["isEnabled"] = RF(ic.isEnabled);
    j["singleUse"] = RF(ic.singleUse);
    j["highlightOnHover"] = RF(ic.highlightOnHover);
    j["highlightColor"] = SerializeVector3(ic.highlightColor);
    return j;
}

ECS::InteractableComponent DeserializeInteractableComponent(const json& j) {
    ECS::InteractableComponent ic;
    if (j.contains("promptText")) ic.promptText = SafeStr(j["promptText"], MAX_STR_NAME);
    if (j.contains("interactionRange")) ic.interactionRange = j["interactionRange"].get<f32>();
    if (j.contains("requiresLookAt")) ic.requiresLookAt = JB(j["requiresLookAt"]);
    if (j.contains("lookAtAngle")) ic.lookAtAngle = j["lookAtAngle"].get<f32>();
    if (j.contains("isEnabled")) ic.isEnabled = JB(j["isEnabled"]);
    if (j.contains("singleUse")) ic.singleUse = JB(j["singleUse"]);
    if (j.contains("highlightOnHover")) ic.highlightOnHover = JB(j["highlightOnHover"]);
    if (j.contains("highlightColor")) ic.highlightColor = DeserializeVector3(j["highlightColor"]);
    return ic;
}

json SerializePickupComponent(const ECS::PickupComponent& p) {
    json j;
    j["type"] = static_cast<u8>(p.type);
    j["value"] = RF(p.value);
    j["customId"] = p.customId;
    j["pickupRange"] = RF(p.pickupRange);
    j["destroyOnPickup"] = RF(p.destroyOnPickup);
    j["magnetToPlayer"] = RF(p.magnetToPlayer);
    j["magnetRange"] = RF(p.magnetRange);
    j["magnetSpeed"] = RF(p.magnetSpeed);
    j["canRespawn"] = RF(p.canRespawn);
    j["respawnTime"] = RF(p.respawnTime);
    j["bobSpeed"] = RF(p.bobSpeed);
    j["bobHeight"] = RF(p.bobHeight);
    j["rotationSpeed"] = RF(p.rotationSpeed);
    return j;
}

ECS::PickupComponent DeserializePickupComponent(const json& j) {
    ECS::PickupComponent p;
    if (j.contains("type")) { u8 v = j["type"].get<u8>(); if (v <= 5) p.type = static_cast<ECS::PickupComponent::PickupType>(v); }
    if (j.contains("value")) p.value = j["value"].get<f32>();
    if (j.contains("customId")) p.customId = j["customId"].get<std::string>();
    if (j.contains("pickupRange")) p.pickupRange = j["pickupRange"].get<f32>();
    if (j.contains("destroyOnPickup")) p.destroyOnPickup = JB(j["destroyOnPickup"]);
    if (j.contains("magnetToPlayer")) p.magnetToPlayer = JB(j["magnetToPlayer"]);
    if (j.contains("magnetRange")) p.magnetRange = j["magnetRange"].get<f32>();
    if (j.contains("magnetSpeed")) p.magnetSpeed = j["magnetSpeed"].get<f32>();
    if (j.contains("canRespawn")) p.canRespawn = JB(j["canRespawn"]);
    if (j.contains("respawnTime")) p.respawnTime = j["respawnTime"].get<f32>();
    if (j.contains("bobSpeed")) p.bobSpeed = j["bobSpeed"].get<f32>();
    if (j.contains("bobHeight")) p.bobHeight = j["bobHeight"].get<f32>();
    if (j.contains("rotationSpeed")) p.rotationSpeed = j["rotationSpeed"].get<f32>();
    return p;
}

// ============================================================================
// Tags, Layers, Billboard
// ============================================================================

json SerializeTagComponent(const ECS::TagComponent& t) {
    json j;
    j["tags"] = t.tags;
    return j;
}

ECS::TagComponent DeserializeTagComponent(const json& j) {
    ECS::TagComponent t;
    if (j.contains("tags")) t.tags = j["tags"].get<std::vector<std::string>>();
    return t;
}

json SerializeLayerComponent(const ECS::LayerComponent& l) {
    json j;
    j["layer"] = l.layer;
    j["layerName"] = l.layerName;
    return j;
}

ECS::LayerComponent DeserializeLayerComponent(const json& j) {
    ECS::LayerComponent l;
    if (j.contains("layer")) l.layer = j["layer"].get<u32>();
    if (j.contains("layerName")) l.layerName = j["layerName"].get<std::string>();
    return l;
}

json SerializeBillboardComponent(const ECS::BillboardComponent& b) {
    json j;
    j["faceCamera"] = b.faceCamera;
    j["lockY"] = RF(b.lockY);
    j["rotationOffset"] = RF(b.rotationOffset);
    return j;
}

ECS::BillboardComponent DeserializeBillboardComponent(const json& j) {
    ECS::BillboardComponent b;
    if (j.contains("faceCamera")) b.faceCamera = JB(j["faceCamera"]);
    if (j.contains("lockY")) b.lockY = JB(j["lockY"]);
    if (j.contains("rotationOffset")) b.rotationOffset = j["rotationOffset"].get<f32>();
    return b;
}

// ============================================================================
// Particles
// ============================================================================

json SerializeParticleEmitterComponent(const ECS::ParticleEmitterComponent& pe) {
    json j;
    j["playOnAwake"] = RF(pe.playOnAwake);
    j["loop"] = pe.loop;
    j["emissionRate"] = RF(pe.emissionRate);
    j["burstCount"] = pe.burstCount;
    j["burstInterval"] = RF(pe.burstInterval);
    j["lifetime"] = RF(pe.lifetime);
    j["lifetimeVariance"] = RF(pe.lifetimeVariance);
    j["startSpeed"] = RF(pe.startSpeed);
    j["speedVariance"] = RF(pe.speedVariance);
    j["startSize"] = RF(pe.startSize);
    j["endSize"] = RF(pe.endSize);
    j["startColor"] = SerializeVector3(pe.startColor);
    j["endColor"] = SerializeVector3(pe.endColor);
    j["startAlpha"] = RF(pe.startAlpha);
    j["endAlpha"] = RF(pe.endAlpha);
    j["shape"] = static_cast<u8>(pe.shape);
    j["shapeRadius"] = RF(pe.shapeRadius);
    j["coneAngle"] = RF(pe.coneAngle);
    j["gravity"] = SerializeVector3(pe.gravity);
    j["drag"] = RF(pe.drag);
    j["useSceneWind"] = pe.useSceneWind;
    j["windInfluence"] = RF(pe.windInfluence);
    j["texturePath"] = pe.texturePath;
    j["textureSheetX"] = RF(pe.textureSheetX);
    j["textureSheetY"] = RF(pe.textureSheetY);
    j["sizeMid"] = RF(pe.sizeMid);
    j["speedMultiplierMid"] = RF(pe.speedMultiplierMid);
    j["speedMultiplierEnd"] = RF(pe.speedMultiplierEnd);
    j["startRotation"] = RF(pe.startRotation);
    j["rotationVariance"] = RF(pe.rotationVariance);
    j["rotationSpeed"] = RF(pe.rotationSpeed);
    j["rotationSpeedVariance"] = RF(pe.rotationSpeedVariance);
    j["maxParticles"] = pe.maxParticles;
    j["simulationSpace"] = static_cast<u8>(pe.simulationSpace);
    j["renderMode"] = static_cast<u8>(pe.renderMode);
    j["velocityStretchScale"] = RF(pe.velocityStretchScale);
    return j;
}

ECS::ParticleEmitterComponent DeserializeParticleEmitterComponent(const json& j) {
    ECS::ParticleEmitterComponent pe;
    if (j.contains("playOnAwake")) pe.playOnAwake = JB(j["playOnAwake"]);
    if (j.contains("loop")) pe.loop = JB(j["loop"]);
    if (j.contains("emissionRate")) pe.emissionRate = j["emissionRate"].get<f32>();
    if (j.contains("burstCount")) pe.burstCount = j["burstCount"].get<i32>();
    if (j.contains("burstInterval")) pe.burstInterval = j["burstInterval"].get<f32>();
    if (j.contains("lifetime")) pe.lifetime = j["lifetime"].get<f32>();
    if (j.contains("lifetimeVariance")) pe.lifetimeVariance = j["lifetimeVariance"].get<f32>();
    if (j.contains("startSpeed")) pe.startSpeed = j["startSpeed"].get<f32>();
    if (j.contains("speedVariance")) pe.speedVariance = j["speedVariance"].get<f32>();
    if (j.contains("startSize")) pe.startSize = j["startSize"].get<f32>();
    if (j.contains("endSize")) pe.endSize = j["endSize"].get<f32>();
    if (j.contains("startColor")) pe.startColor = DeserializeVector3(j["startColor"]);
    if (j.contains("endColor")) pe.endColor = DeserializeVector3(j["endColor"]);
    if (j.contains("startAlpha")) pe.startAlpha = j["startAlpha"].get<f32>();
    if (j.contains("endAlpha")) pe.endAlpha = j["endAlpha"].get<f32>();
    if (j.contains("shape")) { u8 v = j["shape"].get<u8>(); if (v <= 4) pe.shape = static_cast<ECS::ParticleEmitterComponent::EmitterShape>(v); }
    if (j.contains("shapeRadius")) pe.shapeRadius = j["shapeRadius"].get<f32>();
    if (j.contains("coneAngle")) pe.coneAngle = j["coneAngle"].get<f32>();
    if (j.contains("gravity")) pe.gravity = DeserializeVector3(j["gravity"]);
    if (j.contains("drag")) pe.drag = j["drag"].get<f32>();
    if (j.contains("useSceneWind")) pe.useSceneWind = JB(j["useSceneWind"]);
    if (j.contains("windInfluence")) pe.windInfluence = j["windInfluence"].get<f32>();
    if (j.contains("texturePath")) pe.texturePath = SafeStr(j["texturePath"], MAX_STR_PATH);
    if (j.contains("textureSheetX")) pe.textureSheetX = j["textureSheetX"].get<i32>();
    if (j.contains("textureSheetY")) pe.textureSheetY = j["textureSheetY"].get<i32>();
    if (j.contains("sizeMid")) pe.sizeMid = j["sizeMid"].get<f32>();
    if (j.contains("speedMultiplierMid")) pe.speedMultiplierMid = j["speedMultiplierMid"].get<f32>();
    if (j.contains("speedMultiplierEnd")) pe.speedMultiplierEnd = j["speedMultiplierEnd"].get<f32>();
    if (j.contains("startRotation")) pe.startRotation = j["startRotation"].get<f32>();
    if (j.contains("rotationVariance")) pe.rotationVariance = j["rotationVariance"].get<f32>();
    if (j.contains("rotationSpeed")) pe.rotationSpeed = j["rotationSpeed"].get<f32>();
    if (j.contains("rotationSpeedVariance")) pe.rotationSpeedVariance = j["rotationSpeedVariance"].get<f32>();
    if (j.contains("maxParticles")) pe.maxParticles = j["maxParticles"].get<u32>();
    if (j.contains("simulationSpace")) { u8 v = j["simulationSpace"].get<u8>(); if (v <= 1) pe.simulationSpace = static_cast<ECS::ParticleEmitterComponent::SimulationSpace>(v); }
    if (j.contains("renderMode")) {
        u8 rm = j["renderMode"].get<u8>();
        if (rm <= 1) pe.renderMode = static_cast<ECS::ParticleEmitterComponent::RenderMode>(rm);
    }
    if (j.contains("velocityStretchScale")) pe.velocityStretchScale = j["velocityStretchScale"].get<f32>();
    return pe;
}

// ============================================================================
// 2D Rendering
// ============================================================================

json SerializeSprite2DComponent(const ECS::Sprite2DComponent& s) {
    json j;
    j["texturePath"] = s.texturePath;
    if (!s.normalMapPath.empty()) j["normalMapPath"] = s.normalMapPath;
    j["srcX"] = RF(s.srcX);
    j["srcY"] = RF(s.srcY);
    j["srcWidth"] = RF(s.srcWidth);
    j["srcHeight"] = RF(s.srcHeight);
    j["size"] = SerializeVector2(s.size);
    j["pivot"] = SerializeVector2(s.pivot);
    j["tint"] = SerializeVector3(s.tint);
    j["alpha"] = RF(s.alpha);
    j["sortingLayer"] = RF(s.sortingLayer);
    j["orderInLayer"] = RF(s.orderInLayer);
    j["flipX"] = s.flipX;
    j["flipY"] = s.flipY;
    j["visible"] = RF(s.visible);
    if (s.dropShadow) {
        j["dropShadow"] = true;
        j["shadowOffset"] = SerializeVector2(s.shadowOffset);
        j["shadowColor"] = { RF(s.shadowColor.x), RF(s.shadowColor.y), RF(s.shadowColor.z), RF(s.shadowColor.w) };
        j["shadowScale"] = RF(s.shadowScale);
    }
    return j;
}

ECS::Sprite2DComponent DeserializeSprite2DComponent(const json& j) {
    ECS::Sprite2DComponent s;
    if (j.contains("texturePath")) s.texturePath = SafeStr(j["texturePath"], MAX_STR_PATH);
    if (j.contains("normalMapPath")) s.normalMapPath = SafeStr(j["normalMapPath"], MAX_STR_PATH);
    if (j.contains("srcX")) s.srcX = j["srcX"].get<f32>();
    if (j.contains("srcY")) s.srcY = j["srcY"].get<f32>();
    if (j.contains("srcWidth")) s.srcWidth = j["srcWidth"].get<f32>();
    if (j.contains("srcHeight")) s.srcHeight = j["srcHeight"].get<f32>();
    if (j.contains("size")) s.size = DeserializeVector2(j["size"]);
    if (j.contains("pivot")) s.pivot = DeserializeVector2(j["pivot"]);
    if (j.contains("tint")) s.tint = DeserializeVector3(j["tint"]);
    if (j.contains("alpha")) s.alpha = j["alpha"].get<f32>();
    if (j.contains("sortingLayer")) s.sortingLayer = j["sortingLayer"].get<i32>();
    if (j.contains("orderInLayer")) s.orderInLayer = j["orderInLayer"].get<i32>();
    if (j.contains("flipX")) s.flipX = JB(j["flipX"]);
    if (j.contains("flipY")) s.flipY = JB(j["flipY"]);
    if (j.contains("visible")) s.visible = JB(j["visible"]);
    if (j.contains("dropShadow")) s.dropShadow = JB(j["dropShadow"]);
    if (j.contains("shadowOffset")) s.shadowOffset = DeserializeVector2(j["shadowOffset"]);
    if (j.contains("shadowColor") && j["shadowColor"].is_array() && j["shadowColor"].size() >= 4) {
        s.shadowColor = Math::Vector4(j["shadowColor"][0].get<f32>(), j["shadowColor"][1].get<f32>(),
                                      j["shadowColor"][2].get<f32>(), j["shadowColor"][3].get<f32>());
    }
    if (j.contains("shadowScale")) s.shadowScale = j["shadowScale"].get<f32>();
    return s;
}

json SerializeAnimatedSprite2DComponent(const ECS::AnimatedSprite2DComponent& a) {
    json j;
    json framesArr = json::array();
    for (const auto& f : a.frames) {
        json frame;
        frame["srcX"] = RF(f.srcX);
        frame["srcY"] = RF(f.srcY);
        frame["duration"] = RF(f.duration);
        framesArr.push_back(frame);
    }
    j["frames"] = framesArr;
    j["playing"] = a.playing;
    j["loop"] = a.loop;
    j["playbackSpeed"] = RF(a.playbackSpeed);
    return j;
}

ECS::AnimatedSprite2DComponent DeserializeAnimatedSprite2DComponent(const json& j) {
    ECS::AnimatedSprite2DComponent a;
    if (j.contains("frames") && j["frames"].is_array()) {
        for (const auto& fj : j["frames"]) {
            ECS::AnimatedSprite2DComponent::Frame f;
            if (fj.contains("srcX")) f.srcX = fj["srcX"].get<f32>();
            if (fj.contains("srcY")) f.srcY = fj["srcY"].get<f32>();
            if (fj.contains("duration")) f.duration = fj["duration"].get<f32>();
            a.frames.push_back(f);
        }
    }
    if (j.contains("playing")) a.playing = JB(j["playing"]);
    if (j.contains("loop")) a.loop = JB(j["loop"]);
    if (j.contains("playbackSpeed")) a.playbackSpeed = j["playbackSpeed"].get<f32>();
    return a;
}

json SerializeTilemapComponent(const ECS::TilemapComponent& tm) {
    json j;
    j["tiles"] = tm.tiles;
    j["width"] = tm.width;
    j["height"] = tm.height;
    j["tilesetPath"] = tm.tilesetPath;
    j["tileWidth"] = RF(tm.tileWidth);
    j["tileHeight"] = RF(tm.tileHeight);
    j["tilesetColumns"] = tm.tilesetColumns;
    j["worldTileWidth"] = RF(tm.worldTileWidth);
    j["worldTileHeight"] = RF(tm.worldTileHeight);
    j["hasCollision"] = RF(tm.hasCollision);
    if (tm.hasCollision && !tm.collisionMask.empty()) {
        j["collisionMask"] = tm.collisionMask;
    }
    return j;
}

ECS::TilemapComponent DeserializeTilemapComponent(const json& j) {
    ECS::TilemapComponent tm;
    if (j.contains("width")) tm.width = std::min(j["width"].get<u32>(), 4096u); // SN-H3: cap
    if (j.contains("height")) tm.height = std::min(j["height"].get<u32>(), 4096u);
    if (j.contains("tiles")) {
        auto tiles = j["tiles"].get<std::vector<i32>>();
        usize maxTiles = static_cast<usize>(tm.width) * tm.height;
        if (tiles.size() <= maxTiles) tm.tiles = std::move(tiles); // SN-H3: validate
    }
    if (j.contains("tilesetPath")) tm.tilesetPath = SafeStr(j["tilesetPath"], MAX_STR_PATH);
    if (j.contains("tileWidth")) tm.tileWidth = j["tileWidth"].get<f32>();
    if (j.contains("tileHeight")) tm.tileHeight = j["tileHeight"].get<f32>();
    if (j.contains("tilesetColumns")) tm.tilesetColumns = j["tilesetColumns"].get<u32>();
    if (j.contains("worldTileWidth")) tm.worldTileWidth = j["worldTileWidth"].get<f32>();
    if (j.contains("worldTileHeight")) tm.worldTileHeight = j["worldTileHeight"].get<f32>();
    if (j.contains("hasCollision")) tm.hasCollision = JB(j["hasCollision"]);
    if (j.contains("collisionMask") && j["collisionMask"].is_array()) {
        usize maxSize = static_cast<usize>(tm.width) * tm.height;
        if (j["collisionMask"].size() <= maxSize) {
            tm.collisionMask = j["collisionMask"].get<std::vector<bool>>();
        }
    }
    return tm;
}

json SerializeCamera2DBoundsComponent(const ECS::Camera2DBoundsComponent& cb) {
    json j;
    j["useBounds"] = cb.useBounds;
    j["minBounds"] = SerializeVector2(cb.minBounds);
    j["maxBounds"] = SerializeVector2(cb.maxBounds);
    j["boundsPadding"] = RF(cb.boundsPadding);
    j["followSmoothing"] = RF(cb.followSmoothing);
    j["followOffset"] = SerializeVector2(cb.followOffset);
    j["minZoom"] = RF(cb.minZoom);
    j["maxZoom"] = RF(cb.maxZoom);
    j["currentZoom"] = RF(cb.currentZoom);
    j["targetZoom"] = RF(cb.targetZoom);
    j["zoomSmoothing"] = RF(cb.zoomSmoothing);
    j["deadZoneSize"] = SerializeVector2(cb.deadZoneSize);
    j["lookAheadDistance"] = RF(cb.lookAheadDistance);
    j["lookAheadSmoothing"] = RF(cb.lookAheadSmoothing);
    j["shakeFrequency"] = RF(cb.shakeFrequency);
    j["multiTargetPadding"] = RF(cb.multiTargetPadding);
    j["autoZoomToFitTargets"] = RF(cb.autoZoomToFitTargets);
    j["followTarget"] = static_cast<u64>(cb.followTarget);
    if (!cb.additionalTargets.empty()) {
        json targets = json::array();
        for (ECS::Entity e : cb.additionalTargets) {
            targets.push_back(static_cast<u64>(e));
        }
        j["additionalTargets"] = targets;
    }
    return j;
}

ECS::Camera2DBoundsComponent DeserializeCamera2DBoundsComponent(const json& j) {
    ECS::Camera2DBoundsComponent cb;
    if (j.contains("useBounds")) cb.useBounds = JB(j["useBounds"]);
    if (j.contains("minBounds")) cb.minBounds = DeserializeVector2(j["minBounds"]);
    if (j.contains("maxBounds")) cb.maxBounds = DeserializeVector2(j["maxBounds"]);
    if (j.contains("boundsPadding")) cb.boundsPadding = j["boundsPadding"].get<f32>();
    if (j.contains("followSmoothing")) cb.followSmoothing = j["followSmoothing"].get<f32>();
    if (j.contains("followOffset")) cb.followOffset = DeserializeVector2(j["followOffset"]);
    if (j.contains("minZoom")) cb.minZoom = j["minZoom"].get<f32>();
    if (j.contains("maxZoom")) cb.maxZoom = j["maxZoom"].get<f32>();
    if (j.contains("currentZoom")) cb.currentZoom = j["currentZoom"].get<f32>();
    if (j.contains("targetZoom")) cb.targetZoom = j["targetZoom"].get<f32>();
    if (j.contains("zoomSmoothing")) cb.zoomSmoothing = j["zoomSmoothing"].get<f32>();
    if (j.contains("deadZoneSize")) cb.deadZoneSize = DeserializeVector2(j["deadZoneSize"]);
    if (j.contains("lookAheadDistance")) cb.lookAheadDistance = j["lookAheadDistance"].get<f32>();
    if (j.contains("lookAheadSmoothing")) cb.lookAheadSmoothing = j["lookAheadSmoothing"].get<f32>();
    if (j.contains("shakeFrequency")) cb.shakeFrequency = j["shakeFrequency"].get<f32>();
    if (j.contains("multiTargetPadding")) cb.multiTargetPadding = j["multiTargetPadding"].get<f32>();
    if (j.contains("autoZoomToFitTargets")) cb.autoZoomToFitTargets = JB(j["autoZoomToFitTargets"]);
    if (j.contains("followTarget")) cb.followTarget = static_cast<ECS::Entity>(j["followTarget"].get<u64>());
    if (j.contains("additionalTargets")) {
        for (const auto& t : j["additionalTargets"]) {
            cb.additionalTargets.push_back(static_cast<ECS::Entity>(t.get<u64>()));
        }
    }
    return cb;
}

// ============================================================================
// Parallax Machine
// ============================================================================

json SerializeParallaxLayer(const ECS::ParallaxLayer& layer) {
    json j;
    j["texturePath"] = layer.texturePath;
    j["distance"] = RF(layer.distance);
    j["speedMultiplier"] = RF(layer.speedMultiplier);
    j["offset"] = SerializeVector2(layer.offset);
    j["scale"] = SerializeVector2(layer.scale);
    j["tint"] = SerializeVector3(layer.tint);
    j["alpha"] = RF(layer.alpha);
    j["repeatX"] = layer.repeatX;
    j["repeatY"] = layer.repeatY;
    j["visible"] = layer.visible;
    j["sortOrder"] = layer.sortOrder;
    return j;
}

ECS::ParallaxLayer DeserializeParallaxLayer(const json& j) {
    ECS::ParallaxLayer layer;
    if (j.contains("texturePath")) layer.texturePath = j["texturePath"].get<std::string>().substr(0, MAX_STR_PATH);
    if (j.contains("distance")) layer.distance = j["distance"].get<f32>();
    if (j.contains("speedMultiplier")) layer.speedMultiplier = j["speedMultiplier"].get<f32>();
    if (j.contains("offset")) layer.offset = DeserializeVector2(j["offset"]);
    if (j.contains("scale")) layer.scale = DeserializeVector2(j["scale"]);
    if (j.contains("tint")) layer.tint = DeserializeVector3(j["tint"]);
    if (j.contains("alpha")) layer.alpha = j["alpha"].get<f32>();
    if (j.contains("repeatX")) layer.repeatX = JB(j["repeatX"]);
    if (j.contains("repeatY")) layer.repeatY = JB(j["repeatY"]);
    if (j.contains("visible")) layer.visible = JB(j["visible"]);
    if (j.contains("sortOrder")) layer.sortOrder = j["sortOrder"].get<i32>();
    return layer;
}

json SerializeParallaxMachineComponent(const ECS::ParallaxMachineComponent& pm) {
    json j;
    j["enabled"] = pm.enabled;
    j["globalSpeed"] = RF(pm.globalSpeed);
    j["origin"] = SerializeVector2(pm.origin);
    j["autoScrollSpeed"] = SerializeVector2(pm.autoScrollSpeed);
    json layersArr = json::array();
    for (const auto& layer : pm.layers) {
        layersArr.push_back(SerializeParallaxLayer(layer));
    }
    j["layers"] = layersArr;
    return j;
}

ECS::ParallaxMachineComponent DeserializeParallaxMachineComponent(const json& j) {
    ECS::ParallaxMachineComponent pm;
    if (j.contains("enabled")) pm.enabled = JB(j["enabled"]);
    if (j.contains("globalSpeed")) pm.globalSpeed = j["globalSpeed"].get<f32>();
    if (j.contains("origin")) pm.origin = DeserializeVector2(j["origin"]);
    if (j.contains("autoScrollSpeed")) pm.autoScrollSpeed = DeserializeVector2(j["autoScrollSpeed"]);
    if (j.contains("layers") && j["layers"].is_array()) {
        for (const auto& layerJson : j["layers"]) {
            if (pm.layers.size() >= 64) break;  // Cap layer count
            pm.layers.push_back(DeserializeParallaxLayer(layerJson));
        }
    }
    return pm;
}

// Per-sprite parallax layer. Only the authored fields round-trip; the anchor /
// camStart / elapsed are runtime state captured on play start.
json SerializeParallaxLayerComponent(const ECS::ParallaxLayerComponent& pl) {
    json j;
    j["factor"] = SerializeVector2(pl.factor);
    j["autoScroll"] = SerializeVector2(pl.autoScroll);
    return j;
}

ECS::ParallaxLayerComponent DeserializeParallaxLayerComponent(const json& j) {
    ECS::ParallaxLayerComponent pl;
    if (j.contains("factor")) pl.factor = DeserializeVector2(j["factor"]);
    if (j.contains("autoScroll")) pl.autoScroll = DeserializeVector2(j["autoScroll"]);
    return pl;
}

// ============================================================================
// Logic
// ============================================================================

json SerializeSMCondition(const ECS::SMTransitionCondition& cond) {
    json j;
    j["param"] = cond.paramName;
    j["type"] = static_cast<i32>(cond.type);
    if (cond.type == ECS::SMConditionType::FloatGreater || cond.type == ECS::SMConditionType::FloatLess) {
        j["threshold"] = RF(cond.threshold);
    }
    if (cond.type == ECS::SMConditionType::IntEquals || cond.type == ECS::SMConditionType::IntNotEquals) {
        j["intValue"] = RF(cond.intValue);
    }
    return j;
}

ECS::SMTransitionCondition DeserializeSMCondition(const json& j) {
    ECS::SMTransitionCondition cond;
    if (j.contains("param")) cond.paramName = j["param"].get<std::string>();
    if (j.contains("type")) {
        i32 t = j["type"].get<i32>();
        if (t >= 0 && t < static_cast<i32>(ECS::SMConditionType::COUNT))
            cond.type = static_cast<ECS::SMConditionType>(t);
    }
    if (j.contains("threshold")) cond.threshold = j["threshold"].get<f32>();
    if (j.contains("intValue")) cond.intValue = j["intValue"].get<i32>();
    return cond;
}

json SerializeSMTransition(const ECS::SMTransition& trans) {
    json j;
    j["toState"] = trans.toState;
    json conds = json::array();
    for (const auto& c : trans.conditions) {
        conds.push_back(SerializeSMCondition(c));
    }
    j["conditions"] = conds;
    return j;
}

ECS::SMTransition DeserializeSMTransition(const json& j) {
    ECS::SMTransition trans;
    if (j.contains("toState")) trans.toState = j["toState"].get<std::string>();
    if (j.contains("conditions") && j["conditions"].is_array()) {
        for (const auto& c : j["conditions"]) {
            trans.conditions.push_back(DeserializeSMCondition(c));
        }
    }
    return trans;
}

json SerializeSMState(const ECS::SMState& state) {
    json j;
    j["name"] = state.name;
    json transitions = json::array();
    for (const auto& t : state.transitions) {
        transitions.push_back(SerializeSMTransition(t));
    }
    j["transitions"] = transitions;
    j["editorPosition"] = { RF(state.editorPosition.x), RF(state.editorPosition.y) };
    if (!state.onEnter.empty()) j["onEnter"] = state.onEnter;
    if (!state.onUpdate.empty()) j["onUpdate"] = state.onUpdate;
    if (!state.onExit.empty()) j["onExit"] = state.onExit;
    return j;
}

ECS::SMState DeserializeSMState(const json& j) {
    ECS::SMState state;
    if (j.contains("name")) state.name = j["name"].get<std::string>();
    if (j.contains("transitions") && j["transitions"].is_array()) {
        for (const auto& t : j["transitions"]) {
            state.transitions.push_back(DeserializeSMTransition(t));
        }
    }
    if (j.contains("editorPosition") && j["editorPosition"].is_array() && j["editorPosition"].size() >= 2) {
        state.editorPosition.x = j["editorPosition"][0].get<f32>();
        state.editorPosition.y = j["editorPosition"][1].get<f32>();
    }
    if (j.contains("onEnter")) state.onEnter = SafeStr(j["onEnter"]);
    if (j.contains("onUpdate")) state.onUpdate = SafeStr(j["onUpdate"]);
    if (j.contains("onExit")) state.onExit = SafeStr(j["onExit"]);
    return state;
}

json SerializeStateMachineComponent(const ECS::StateMachineComponent& sm) {
    json j;
    j["currentState"] = sm.currentState;

    // Parameters as objects (key -> value)
    json bools = json::object();
    for (const auto& [name, val] : sm.boolParams) bools[name] = val;
    j["boolParams"] = bools;

    json floats = json::object();
    for (const auto& [name, val] : sm.floatParams) floats[name] = val;
    j["floatParams"] = floats;

    json ints = json::object();
    for (const auto& [name, val] : sm.intParams) ints[name] = val;
    j["intParams"] = ints;

    // States
    json statesArr = json::array();
    for (const auto& s : sm.states) {
        statesArr.push_back(SerializeSMState(s));
    }
    j["states"] = statesArr;

    return j;
}

ECS::StateMachineComponent DeserializeStateMachineComponent(const json& j) {
    ECS::StateMachineComponent sm;
    if (j.contains("currentState")) sm.currentState = j["currentState"].get<std::string>();

    // New format: params as objects
    if (j.contains("boolParams") && j["boolParams"].is_object()) {
        for (auto& [key, val] : j["boolParams"].items()) {
            sm.boolParams[key] = JB(val);
        }
    }
    if (j.contains("floatParams") && j["floatParams"].is_object()) {
        for (auto& [key, val] : j["floatParams"].items()) {
            sm.floatParams[key] = val.get<f32>();
        }
    }
    if (j.contains("intParams") && j["intParams"].is_object()) {
        for (auto& [key, val] : j["intParams"].items()) {
            sm.intParams[key] = val.get<i32>();
        }
    }

    // Backward compat: old format used arrays of [name, value] pairs
    if (j.contains("floatParams") && j["floatParams"].is_array()) {
        for (const auto& p : j["floatParams"]) {
            if (p.is_array() && p.size() >= 2) {
                sm.floatParams[p[0].get<std::string>()] = p[1].get<f32>();
            }
        }
    }
    if (j.contains("intParams") && j["intParams"].is_array()) {
        for (const auto& p : j["intParams"]) {
            if (p.is_array() && p.size() >= 2) {
                sm.intParams[p[0].get<std::string>()] = p[1].get<i32>();
            }
        }
    }
    if (j.contains("boolParams") && j["boolParams"].is_array()) {
        for (const auto& p : j["boolParams"]) {
            if (p.is_array() && p.size() >= 2) {
                sm.boolParams[p[0].get<std::string>()] = JB(p[1]);
            }
        }
    }

    // States
    if (j.contains("states") && j["states"].is_array()) {
        for (const auto& s : j["states"]) {
            sm.states.push_back(DeserializeSMState(s));
        }
    }

    return sm;
}

json SerializeDialogueComponent(const ECS::DialogueComponent& d) {
    json j;
    j["dialogueLines"] = d.dialogueLines;
    j["charDelay"] = RF(d.charDelay);
    j["speakerName"] = d.speakerName;
    j["portraitPath"] = d.portraitPath;
    j["typeSound"] = d.typeSound;
    j["playTypeSound"] = d.playTypeSound;
    json choicesArr = json::array();
    for (const auto& c : d.choices) {
        json choice;
        choice["text"] = c.text;
        choice["nextDialogueId"] = c.nextDialogueId;
        choicesArr.push_back(choice);
    }
    j["choices"] = choicesArr;
    if (!d.dialogueTree.nodes.empty()) {
        j["dialogueTree"] = d.dialogueTree.ToJson();
    }
    if (!d.variables.empty()) {
        json vars = json::object();
        for (const auto& [k, v] : d.variables) vars[k] = v;
        j["variables"] = vars;
    }
    return j;
}

ECS::DialogueComponent DeserializeDialogueComponent(const json& j) {
    ECS::DialogueComponent d;
    if (j.contains("dialogueLines") && j["dialogueLines"].is_array() && j["dialogueLines"].size() <= 10000) {
        d.dialogueLines = j["dialogueLines"].get<std::vector<std::string>>();
    }
    if (j.contains("charDelay")) d.charDelay = j["charDelay"].get<f32>();
    if (j.contains("speakerName")) d.speakerName = SafeStr(j["speakerName"], MAX_STR_NAME);
    if (j.contains("portraitPath")) d.portraitPath = SafeStr(j["portraitPath"], MAX_STR_PATH);
    if (j.contains("typeSound")) d.typeSound = SafeStr(j["typeSound"], MAX_STR_PATH);
    if (j.contains("playTypeSound")) d.playTypeSound = JB(j["playTypeSound"]);
    if (j.contains("choices") && j["choices"].is_array()) {
        for (const auto& cj : j["choices"]) {
            ECS::DialogueComponent::Choice c;
            if (cj.contains("text")) c.text = SafeStr(cj["text"]);
            if (cj.contains("nextDialogueId")) c.nextDialogueId = SafeStr(cj["nextDialogueId"], MAX_STR_NAME);
            d.choices.push_back(c);
        }
    }
    if (j.contains("dialogueTree") && j["dialogueTree"].is_object()) {
        d.dialogueTree = GUI::DialogueTreeData::FromJson(j["dialogueTree"]);
    }
    if (j.contains("variables") && j["variables"].is_object()) {
        for (auto& [key, val] : j["variables"].items()) {
            d.variables[key] = val.get<std::string>();
        }
    }
    return d;
}

// ============================================================================
// Dialogue Box
// ============================================================================

json SerializeDialogueBoxComponent(const ECS::DialogueBoxComponent& b) {
    json j;
    j["boxHeight"] = RF(b.boxHeight);
    j["boxMargin"] = RF(b.boxMargin);
    j["boxPadding"] = RF(b.boxPadding);
    j["boxColor"] = { RF(b.boxColor.x), RF(b.boxColor.y), RF(b.boxColor.z) };
    j["boxAlpha"] = RF(b.boxAlpha);
    j["boxBorderRadius"] = RF(b.boxBorderRadius);
    j["speakerFontSize"] = RF(b.speakerFontSize);
    j["defaultSpeakerColor"] = { RF(b.defaultSpeakerColor.x), RF(b.defaultSpeakerColor.y), RF(b.defaultSpeakerColor.z) };
    j["textFontSize"] = RF(b.textFontSize);
    j["textColor"] = { RF(b.textColor.x), RF(b.textColor.y), RF(b.textColor.z) };
    j["showPortrait"] = b.showPortrait;
    j["portraitSize"] = RF(b.portraitSize);
    j["choiceSpacing"] = RF(b.choiceSpacing);
    j["choiceColor"] = { RF(b.choiceColor.x), RF(b.choiceColor.y), RF(b.choiceColor.z) };
    j["choiceTextColor"] = { RF(b.choiceTextColor.x), RF(b.choiceTextColor.y), RF(b.choiceTextColor.z) };
    j["continueText"] = b.continueText;
    j["continueBlinkSpeed"] = RF(b.continueBlinkSpeed);
    return j;
}

ECS::DialogueBoxComponent DeserializeDialogueBoxComponent(const json& j) {
    ECS::DialogueBoxComponent b;
    if (j.contains("boxHeight")) b.boxHeight = j["boxHeight"].get<f32>();
    if (j.contains("boxMargin")) b.boxMargin = j["boxMargin"].get<f32>();
    if (j.contains("boxPadding")) b.boxPadding = j["boxPadding"].get<f32>();
    if (j.contains("boxColor") && j["boxColor"].is_array() && j["boxColor"].size() >= 3)
        b.boxColor = Math::Vector3(j["boxColor"][0].get<f32>(), j["boxColor"][1].get<f32>(), j["boxColor"][2].get<f32>());
    if (j.contains("boxAlpha")) b.boxAlpha = j["boxAlpha"].get<f32>();
    if (j.contains("boxBorderRadius")) b.boxBorderRadius = j["boxBorderRadius"].get<f32>();
    if (j.contains("speakerFontSize")) b.speakerFontSize = j["speakerFontSize"].get<f32>();
    if (j.contains("defaultSpeakerColor") && j["defaultSpeakerColor"].is_array() && j["defaultSpeakerColor"].size() >= 3)
        b.defaultSpeakerColor = Math::Vector3(j["defaultSpeakerColor"][0].get<f32>(), j["defaultSpeakerColor"][1].get<f32>(), j["defaultSpeakerColor"][2].get<f32>());
    if (j.contains("textFontSize")) b.textFontSize = j["textFontSize"].get<f32>();
    if (j.contains("textColor") && j["textColor"].is_array() && j["textColor"].size() >= 3)
        b.textColor = Math::Vector3(j["textColor"][0].get<f32>(), j["textColor"][1].get<f32>(), j["textColor"][2].get<f32>());
    if (j.contains("showPortrait")) b.showPortrait = JB(j["showPortrait"]);
    if (j.contains("portraitSize")) b.portraitSize = j["portraitSize"].get<f32>();
    if (j.contains("choiceSpacing")) b.choiceSpacing = j["choiceSpacing"].get<f32>();
    if (j.contains("choiceColor") && j["choiceColor"].is_array() && j["choiceColor"].size() >= 3)
        b.choiceColor = Math::Vector3(j["choiceColor"][0].get<f32>(), j["choiceColor"][1].get<f32>(), j["choiceColor"][2].get<f32>());
    if (j.contains("choiceTextColor") && j["choiceTextColor"].is_array() && j["choiceTextColor"].size() >= 3)
        b.choiceTextColor = Math::Vector3(j["choiceTextColor"][0].get<f32>(), j["choiceTextColor"][1].get<f32>(), j["choiceTextColor"][2].get<f32>());
    if (j.contains("continueText")) b.continueText = j["continueText"].get<std::string>();
    if (j.contains("continueBlinkSpeed")) b.continueBlinkSpeed = j["continueBlinkSpeed"].get<f32>();
    return b;
}

// ============================================================================
// Visual Script
// ============================================================================

json SerializeVisualScriptVariable(const ECS::VisualScriptVariable& var) {
    json j;
    j["name"] = var.name;
    j["type"] = static_cast<i32>(var.type);
    j["exposed"] = RF(var.exposed);

    // Serialize value based on type
    std::visit([&j](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            j["value"] = arg;
        } else if constexpr (std::is_same_v<T, i32>) {
            j["value"] = arg;
        } else if constexpr (std::is_same_v<T, f32>) {
            j["value"] = arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            j["value"] = arg;
        } else if constexpr (std::is_same_v<T, Math::Vector2>) {
            j["value"] = json::array({arg.x, arg.y});
        } else if constexpr (std::is_same_v<T, Math::Vector3>) {
            j["value"] = json::array({arg.x, arg.y, arg.z});
        } else if constexpr (std::is_same_v<T, Math::Vector4>) {
            j["value"] = json::array({arg.x, arg.y, arg.z, arg.w});
        } else if constexpr (std::is_same_v<T, ECS::Entity>) {
            j["value"] = static_cast<u64>(arg);
        }
    }, var.value);

    return j;
}

ECS::VisualScriptVariable DeserializeVisualScriptVariable(const json& j) {
    ECS::VisualScriptVariable var;
    if (j.contains("name")) var.name = j["name"].get<std::string>();
    if (j.contains("type")) { i32 v = j["type"].get<i32>(); if (v >= 0 && v < static_cast<i32>(Editor::PinType::COUNT)) var.type = static_cast<Editor::PinType>(v); }
    if (j.contains("exposed")) var.exposed = JB(j["exposed"]);

    if (j.contains("value")) {
        switch (var.type) {
            case Editor::PinType::Bool:
                var.value = JB(j["value"]);
                break;
            case Editor::PinType::Int:
                var.value = j["value"].get<i32>();
                break;
            case Editor::PinType::Float:
                var.value = j["value"].get<f32>();
                break;
            case Editor::PinType::String:
                var.value = j["value"].get<std::string>();
                break;
            case Editor::PinType::Vector2:
                if (j["value"].is_array() && j["value"].size() >= 2)
                    var.value = Math::Vector2(j["value"][0].get<f32>(), j["value"][1].get<f32>());
                break;
            case Editor::PinType::Vector3:
                if (j["value"].is_array() && j["value"].size() >= 3)
                    var.value = Math::Vector3(j["value"][0].get<f32>(), j["value"][1].get<f32>(), j["value"][2].get<f32>());
                break;
            case Editor::PinType::Vector4:
                if (j["value"].is_array() && j["value"].size() >= 4)
                    var.value = Math::Vector4(j["value"][0].get<f32>(), j["value"][1].get<f32>(), j["value"][2].get<f32>(), j["value"][3].get<f32>());
                break;
            case Editor::PinType::Entity:
                var.value = static_cast<ECS::Entity>(j["value"].get<u64>());
                break;
            default:
                break;
        }
    }
    return var;
}

json SerializeVisualScriptNodeMeta(const ECS::VisualScriptNodeMeta& meta) {
    json j;
    j["nodeType"] = meta.nodeType;
    if (!meta.customEventName.empty()) j["customEventName"] = meta.customEventName;
    if (!meta.properties.empty()) {
        json props = json::object();
        for (const auto& [k, v] : meta.properties) props[k] = v;
        j["properties"] = props;
    }
    return j;
}

ECS::VisualScriptNodeMeta DeserializeVisualScriptNodeMeta(const json& j) {
    ECS::VisualScriptNodeMeta meta;
    if (j.contains("nodeType")) meta.nodeType = j["nodeType"].get<std::string>();
    if (j.contains("customEventName")) meta.customEventName = j["customEventName"].get<std::string>();
    if (j.contains("properties") && j["properties"].is_object()) {
        for (auto& [k, v] : j["properties"].items()) {
            meta.properties[k] = v.get<std::string>();
        }
    }
    return meta;
}

json SerializeVisualScriptComponent(const ECS::VisualScriptComponent& vs) {
    json j;

    // Serialize graph data
    j["graph"] = vs.graph.ToJson();

    // Serialize variables
    json varsArr = json::array();
    for (const auto& var : vs.variables) {
        varsArr.push_back(SerializeVisualScriptVariable(var));
    }
    j["variables"] = varsArr;

    // Serialize event nodes
    json eventNodes = json::object();
    for (const auto& [eventType, nodeId] : vs.eventNodes) {
        eventNodes[std::to_string(eventType)] = nodeId;
    }
    j["eventNodes"] = eventNodes;

    // Serialize custom event nodes
    if (!vs.customEventNodes.empty()) {
        json customEvents = json::object();
        for (const auto& [name, nodeId] : vs.customEventNodes) {
            customEvents[name] = nodeId;
        }
        j["customEventNodes"] = customEvents;
    }

    // Serialize node metadata
    json nodeMeta = json::object();
    for (const auto& [nodeId, meta] : vs.nodeMeta) {
        nodeMeta[std::to_string(nodeId)] = SerializeVisualScriptNodeMeta(meta);
    }
    j["nodeMeta"] = nodeMeta;

    j["enabled"] = vs.enabled;

    return j;
}

ECS::VisualScriptComponent DeserializeVisualScriptComponent(const json& j) {
    ECS::VisualScriptComponent vs;

    // Deserialize graph data
    if (j.contains("graph") && j["graph"].is_object()) {
        vs.graph.FromJson(j["graph"]);
    }

    // Deserialize variables
    if (j.contains("variables") && j["variables"].is_array()) {
        for (const auto& v : j["variables"]) {
            vs.variables.push_back(DeserializeVisualScriptVariable(v));
        }
    }

    // Deserialize event nodes
    if (j.contains("eventNodes") && j["eventNodes"].is_object()) {
        for (auto& [key, val] : j["eventNodes"].items()) {
            try {
                int parsed = std::stoi(key);
                if (parsed < 0 || parsed > 255) {
                    ENJIN_LOG_WARN(Editor, "Event node ID out of u8 range: %d", parsed);
                    continue;
                }
                u8 eventType = static_cast<u8>(parsed);
                vs.eventNodes[eventType] = val.get<Editor::NodeId>();
            } catch (const std::exception&) {
                ENJIN_LOG_WARN(Editor, "Invalid event node ID in scene: %s", key.c_str());
                continue;
            }
        }
    }

    // Deserialize custom event nodes
    if (j.contains("customEventNodes") && j["customEventNodes"].is_object()) {
        for (auto& [key, val] : j["customEventNodes"].items()) {
            vs.customEventNodes[key] = val.get<Editor::NodeId>();
        }
    }

    // Deserialize node metadata
    if (j.contains("nodeMeta") && j["nodeMeta"].is_object()) {
        for (auto& [key, val] : j["nodeMeta"].items()) {
            try {
                unsigned long parsed = std::stoul(key);
                if (parsed > static_cast<unsigned long>(std::numeric_limits<u32>::max())) {
                    ENJIN_LOG_WARN(Editor, "Node ID overflow in scene: %s", key.c_str());
                    continue;
                }
                Editor::NodeId nodeId = static_cast<Editor::NodeId>(parsed);
                vs.nodeMeta[nodeId] = DeserializeVisualScriptNodeMeta(val);
            } catch (const std::exception&) {
                ENJIN_LOG_WARN(Editor, "Invalid node ID in scene: %s", key.c_str());
                continue;
            }
        }
    }

    if (j.contains("enabled")) vs.enabled = JB(j["enabled"]);

    return vs;
}

// ============================================================================
// Tween
// ============================================================================

json SerializeTweenEntry(const ECS::TweenEntry& te) {
    json j;
    j["property"] = static_cast<i32>(te.property);
    j["easing"] = static_cast<i32>(te.easing);
    j["mode"] = static_cast<i32>(te.mode);
    j["startValue"] = SerializeVector3(te.startValue);
    j["endValue"] = SerializeVector3(te.endValue);
    j["duration"] = RF(te.duration);
    j["delay"] = RF(te.delay);
    j["useCurrentAsStart"] = RF(te.useCurrentAsStart);
    if (!te.onCompleteCallback.empty()) {
        j["onCompleteCallback"] = te.onCompleteCallback;
    }
    return j;
}

ECS::TweenEntry DeserializeTweenEntry(const json& j) {
    ECS::TweenEntry te;
    if (j.contains("property")) { i32 v = j["property"].get<i32>(); if (v >= 0 && v < static_cast<i32>(ECS::TweenProperty::COUNT)) te.property = static_cast<ECS::TweenProperty>(v); }
    if (j.contains("easing")) { i32 v = j["easing"].get<i32>(); if (v >= 0 && v < static_cast<i32>(ECS::EasingType::COUNT)) te.easing = static_cast<ECS::EasingType>(v); }
    if (j.contains("mode")) { i32 v = j["mode"].get<i32>(); if (v >= 0 && v < static_cast<i32>(ECS::TweenMode::COUNT)) te.mode = static_cast<ECS::TweenMode>(v); }
    if (j.contains("startValue")) te.startValue = DeserializeVector3(j["startValue"]);
    if (j.contains("endValue")) te.endValue = DeserializeVector3(j["endValue"]);
    if (j.contains("duration")) te.duration = j["duration"].get<f32>();
    if (j.contains("delay")) te.delay = j["delay"].get<f32>();
    if (j.contains("useCurrentAsStart")) te.useCurrentAsStart = JB(j["useCurrentAsStart"]);
    if (j.contains("onCompleteCallback")) te.onCompleteCallback = j["onCompleteCallback"].get<std::string>();
    return te;
}

json SerializeTweenComponent(const ECS::TweenComponent& tc) {
    json j;
    j["autoPlay"] = RF(tc.autoPlay);
    json tweensArr = json::array();
    for (const auto& te : tc.tweens) {
        tweensArr.push_back(SerializeTweenEntry(te));
    }
    j["tweens"] = tweensArr;
    return j;
}

ECS::TweenComponent DeserializeTweenComponent(const json& j) {
    ECS::TweenComponent tc;
    if (j.contains("autoPlay")) tc.autoPlay = JB(j["autoPlay"]);
    if (j.contains("tweens") && j["tweens"].is_array()) {
        for (const auto& te : j["tweens"]) {
            tc.tweens.push_back(DeserializeTweenEntry(te));
        }
    }
    return tc;
}

// ============================================================================
// AI & Navigation
// ============================================================================

json SerializeAIControllerComponent(const ECS::AIControllerComponent& ai) {
    json j;
    j["detectionRange"] = RF(ai.detectionRange);
    j["attackRange"] = RF(ai.attackRange);
    j["loseTargetRange"] = RF(ai.loseTargetRange);
    j["fieldOfView"] = RF(ai.fieldOfView);
    j["moveSpeed"] = RF(ai.moveSpeed);
    j["turnSpeed"] = RF(ai.turnSpeed);
    j["stoppingDistance"] = RF(ai.stoppingDistance);
    j["attackCooldown"] = RF(ai.attackCooldown);
    j["attackDamage"] = RF(ai.attackDamage);
    json patrolArr = json::array();
    for (const auto& p : ai.patrolPoints) {
        patrolArr.push_back(SerializeVector3(p));
    }
    j["patrolPoints"] = patrolArr;
    j["patrolWaitTime"] = RF(ai.patrolWaitTime);
    j["patrolLoop"] = ai.patrolLoop;
    j["useNavmesh"] = ai.useNavmesh;
    j["repathInterval"] = RF(ai.repathInterval);
    j["arrivalRadius"] = RF(ai.arrivalRadius);
    j["chaseSpeed"] = RF(ai.chaseSpeed);
    j["fleeSpeed"] = RF(ai.fleeSpeed);
    j["fleeDistance"] = RF(ai.fleeDistance);
    j["debugDrawPath"] = ai.debugDrawPath;
    j["debugDrawDetection"] = RF(ai.debugDrawDetection);
    j["is2D"] = ai.is2D;
    return j;
}

ECS::AIControllerComponent DeserializeAIControllerComponent(const json& j) {
    ECS::AIControllerComponent ai;
    if (j.contains("detectionRange")) ai.detectionRange = j["detectionRange"].get<f32>();
    if (j.contains("attackRange")) ai.attackRange = j["attackRange"].get<f32>();
    if (j.contains("loseTargetRange")) ai.loseTargetRange = j["loseTargetRange"].get<f32>();
    if (j.contains("fieldOfView")) ai.fieldOfView = j["fieldOfView"].get<f32>();
    if (j.contains("moveSpeed")) ai.moveSpeed = j["moveSpeed"].get<f32>();
    if (j.contains("turnSpeed")) ai.turnSpeed = j["turnSpeed"].get<f32>();
    if (j.contains("stoppingDistance")) ai.stoppingDistance = j["stoppingDistance"].get<f32>();
    if (j.contains("attackCooldown")) ai.attackCooldown = j["attackCooldown"].get<f32>();
    if (j.contains("attackDamage")) ai.attackDamage = j["attackDamage"].get<f32>();
    if (j.contains("patrolPoints") && j["patrolPoints"].is_array()) {
        for (const auto& p : j["patrolPoints"]) {
            ai.patrolPoints.push_back(DeserializeVector3(p));
        }
    }
    if (j.contains("patrolWaitTime")) ai.patrolWaitTime = j["patrolWaitTime"].get<f32>();
    if (j.contains("patrolLoop")) ai.patrolLoop = JB(j["patrolLoop"]);
    if (j.contains("useNavmesh")) ai.useNavmesh = JB(j["useNavmesh"]);
    if (j.contains("repathInterval")) ai.repathInterval = j["repathInterval"].get<f32>();
    if (j.contains("arrivalRadius")) ai.arrivalRadius = j["arrivalRadius"].get<f32>();
    if (j.contains("chaseSpeed")) ai.chaseSpeed = j["chaseSpeed"].get<f32>();
    if (j.contains("fleeSpeed")) ai.fleeSpeed = j["fleeSpeed"].get<f32>();
    if (j.contains("fleeDistance")) ai.fleeDistance = j["fleeDistance"].get<f32>();
    if (j.contains("debugDrawPath")) ai.debugDrawPath = JB(j["debugDrawPath"]);
    if (j.contains("debugDrawDetection")) ai.debugDrawDetection = JB(j["debugDrawDetection"]);
    if (j.contains("is2D")) ai.is2D = JB(j["is2D"]);
    return ai;
}

// ============================================================================
// Behavior Tree Component
// ============================================================================

static json SerializeBlackboardValue(const AI::BlackboardValue& val) {
    json j;
    if (auto* b = std::get_if<bool>(&val)) {
        j["type"] = "bool"; j["value"] = *b;
    } else if (auto* i = std::get_if<i32>(&val)) {
        j["type"] = "int"; j["value"] = *i;
    } else if (auto* f = std::get_if<f32>(&val)) {
        j["type"] = "float"; j["value"] = *f;
    } else if (auto* s = std::get_if<std::string>(&val)) {
        j["type"] = "string"; j["value"] = *s;
    } else if (auto* v = std::get_if<Math::Vector3>(&val)) {
        j["type"] = "vector3"; j["value"] = SerializeVector3(*v);
    } else if (auto* e = std::get_if<ECS::Entity>(&val)) {
        j["type"] = "entity"; j["value"] = static_cast<u64>(*e);
    }
    return j;
}

static AI::BlackboardValue DeserializeBlackboardValue(const json& j) {
    std::string type = j.value("type", "string");
    if (type == "bool") return j.contains("value") ? JB(j["value"]) : false;
    if (type == "int") return j.value("value", 0);
    if (type == "float") return j.value("value", 0.0f);
    if (type == "vector3") return DeserializeVector3(j["value"]);
    if (type == "entity") return static_cast<ECS::Entity>(j.value("value", (u64)0));
    return j.value("value", std::string(""));
}

json SerializeBehaviorTreeComponent(const ECS::BehaviorTreeComponent& bt) {
    json j;
    j["graph"] = bt.graph.ToJson();
    j["rootNodeId"] = bt.rootNodeId;
    j["enabled"] = bt.enabled;
    j["tickInterval"] = RF(bt.tickInterval);
    j["debugEnabled"] = RF(bt.debugEnabled);

    // Node meta
    json metaArr = json::array();
    for (const auto& [nodeId, meta] : bt.nodeMeta) {
        json m;
        m["nodeId"] = nodeId;
        m["nodeType"] = static_cast<u8>(meta.nodeType);
        if (!meta.properties.empty()) {
            json props = json::object();
            for (const auto& [k, v] : meta.properties) {
                props[k] = v;
            }
            m["properties"] = props;
        }
        metaArr.push_back(m);
    }
    j["nodeMeta"] = metaArr;

    // Blackboard defaults
    json bbArr = json::array();
    for (const auto& entry : bt.blackboardDefaults) {
        json e;
        e["key"] = entry.key;
        // NOT RF-wrapped: the blackboard value serializes as an OBJECT — same
        // Feb-09 sweep accident as the canvas theme; this one ALWAYS threw,
        // so scenes with AI blackboard entries could not save at all.
        e["val"] = SerializeBlackboardValue(entry.value);
        bbArr.push_back(e);
    }
    j["blackboardDefaults"] = bbArr;

    return j;
}

ECS::BehaviorTreeComponent DeserializeBehaviorTreeComponent(const json& j) {
    ECS::BehaviorTreeComponent bt;

    if (j.contains("graph")) bt.graph.FromJson(j["graph"]);
    if (j.contains("rootNodeId")) bt.rootNodeId = j["rootNodeId"].get<Editor::NodeId>();
    if (j.contains("enabled")) bt.enabled = JB(j["enabled"]);
    if (j.contains("tickInterval")) bt.tickInterval = j["tickInterval"].get<f32>();
    if (j.contains("debugEnabled")) bt.debugEnabled = JB(j["debugEnabled"]);

    if (j.contains("nodeMeta") && j["nodeMeta"].is_array()) {
        for (const auto& m : j["nodeMeta"]) {
            Editor::NodeId nodeId = m.value("nodeId", (Editor::NodeId)0);
            AI::BTNodeMeta meta;
            u8 nt = m.value("nodeType", (u8)0); meta.nodeType = (nt < static_cast<u8>(AI::BTNodeType::COUNT)) ? static_cast<AI::BTNodeType>(nt) : AI::BTNodeType::Root;
            if (m.contains("properties") && m["properties"].is_object()) {
                for (auto& [k, v] : m["properties"].items()) {
                    meta.properties[k] = v.get<std::string>();
                }
            }
            bt.nodeMeta[nodeId] = meta;
        }
    }

    if (j.contains("blackboardDefaults") && j["blackboardDefaults"].is_array()) {
        for (const auto& e : j["blackboardDefaults"]) {
            AI::BlackboardEntry entry;
            entry.key = e.value("key", "");
            if (e.contains("val")) {
                entry.value = DeserializeBlackboardValue(e["val"]);
            }
            bt.blackboardDefaults.push_back(entry);
        }
    }

    return bt;
}

// ============================================================================
// Quest Flow Component
// ============================================================================

json SerializeQuestFlowComponent(const ECS::QuestFlowComponent& qf) {
    json j;
    j["graph"] = qf.graph.ToJson();
    j["startNodeId"] = qf.startNodeId;
    j["questId"] = qf.questId;
    j["questTitle"] = qf.questTitle;
    j["questDescription"] = qf.questDescription;
    j["enabled"] = qf.enabled;

    // Node meta
    json metaArr = json::array();
    for (const auto& [nodeId, meta] : qf.nodeMeta) {
        json m;
        m["nodeId"] = nodeId;
        m["nodeType"] = static_cast<u8>(meta.nodeType);
        if (!meta.properties.empty()) {
            json props = json::object();
            for (const auto& [k, v] : meta.properties) {
                props[k] = v;
            }
            m["properties"] = props;
        }
        metaArr.push_back(m);
    }
    j["nodeMeta"] = metaArr;

    return j;
}

ECS::QuestFlowComponent DeserializeQuestFlowComponent(const json& j) {
    ECS::QuestFlowComponent qf;

    if (j.contains("graph")) qf.graph.FromJson(j["graph"]);
    if (j.contains("startNodeId")) qf.startNodeId = j["startNodeId"].get<Editor::NodeId>();
    if (j.contains("questId")) qf.questId = SafeStr(j["questId"], MAX_STR_NAME);
    if (j.contains("questTitle")) qf.questTitle = SafeStr(j["questTitle"], MAX_STR_NAME);
    if (j.contains("questDescription")) qf.questDescription = SafeStr(j["questDescription"], MAX_STR_LARGE);
    if (j.contains("enabled")) qf.enabled = JB(j["enabled"]);

    if (j.contains("nodeMeta") && j["nodeMeta"].is_array()) {
        for (const auto& m : j["nodeMeta"]) {
            Editor::NodeId nodeId = m.value("nodeId", (Editor::NodeId)0);
            Gameplay::QuestNodeMeta meta;
            u8 nt = m.value("nodeType", (u8)0); meta.nodeType = (nt < static_cast<u8>(Gameplay::QuestNodeType::COUNT)) ? static_cast<Gameplay::QuestNodeType>(nt) : Gameplay::QuestNodeType::Start;
            if (m.contains("properties") && m["properties"].is_object()) {
                for (auto& [k, v] : m["properties"].items()) {
                    meta.properties[k] = v.get<std::string>();
                }
            }
            qf.nodeMeta[nodeId] = meta;
        }
    }

    return qf;
}

// ============================================================================

json SerializeFollowTargetComponent(const ECS::FollowTargetComponent& ft) {
    json j;
    j["target"] = static_cast<u64>(ft.target);
    j["followDistance"] = RF(ft.followDistance);
    j["minDistance"] = RF(ft.minDistance);
    j["maxDistance"] = RF(ft.maxDistance);
    j["moveSpeed"] = RF(ft.moveSpeed);
    j["smoothTime"] = RF(ft.smoothTime);
    j["matchTargetRotation"] = RF(ft.matchTargetRotation);
    j["rotationSpeed"] = RF(ft.rotationSpeed);
    j["offset"] = SerializeVector3(ft.offset);
    j["useLocalOffset"] = RF(ft.useLocalOffset);
    return j;
}

ECS::FollowTargetComponent DeserializeFollowTargetComponent(const json& j) {
    ECS::FollowTargetComponent ft;
    if (j.contains("target")) ft.target = static_cast<ECS::Entity>(j["target"].get<u64>());
    if (j.contains("followDistance")) ft.followDistance = j["followDistance"].get<f32>();
    if (j.contains("minDistance")) ft.minDistance = j["minDistance"].get<f32>();
    if (j.contains("maxDistance")) ft.maxDistance = j["maxDistance"].get<f32>();
    if (j.contains("moveSpeed")) ft.moveSpeed = j["moveSpeed"].get<f32>();
    if (j.contains("smoothTime")) ft.smoothTime = j["smoothTime"].get<f32>();
    if (j.contains("matchTargetRotation")) ft.matchTargetRotation = JB(j["matchTargetRotation"]);
    if (j.contains("rotationSpeed")) ft.rotationSpeed = j["rotationSpeed"].get<f32>();
    if (j.contains("offset")) ft.offset = DeserializeVector3(j["offset"]);
    if (j.contains("useLocalOffset")) ft.useLocalOffset = JB(j["useLocalOffset"]);
    return ft;
}

json SerializeLookAtTargetComponent(const ECS::LookAtTargetComponent& la) {
    json j;
    j["target"] = static_cast<u64>(la.target);
    j["worldTarget"] = SerializeVector3(la.worldTarget);
    j["useWorldTarget"] = RF(la.useWorldTarget);
    j["rotationSpeed"] = RF(la.rotationSpeed);
    j["instant"] = RF(la.instant);
    j["constrainX"] = RF(la.constrainX);
    j["constrainY"] = RF(la.constrainY);
    j["constrainZ"] = RF(la.constrainZ);
    j["minYaw"] = RF(la.minYaw);
    j["maxYaw"] = RF(la.maxYaw);
    j["minPitch"] = RF(la.minPitch);
    j["maxPitch"] = RF(la.maxPitch);
    return j;
}

ECS::LookAtTargetComponent DeserializeLookAtTargetComponent(const json& j) {
    ECS::LookAtTargetComponent la;
    if (j.contains("target")) la.target = static_cast<ECS::Entity>(j["target"].get<u64>());
    if (j.contains("worldTarget")) la.worldTarget = DeserializeVector3(j["worldTarget"]);
    if (j.contains("useWorldTarget")) la.useWorldTarget = JB(j["useWorldTarget"]);
    if (j.contains("rotationSpeed")) la.rotationSpeed = j["rotationSpeed"].get<f32>();
    if (j.contains("instant")) la.instant = JB(j["instant"]);
    if (j.contains("constrainX")) la.constrainX = JB(j["constrainX"]);
    if (j.contains("constrainY")) la.constrainY = JB(j["constrainY"]);
    if (j.contains("constrainZ")) la.constrainZ = JB(j["constrainZ"]);
    if (j.contains("minYaw")) la.minYaw = j["minYaw"].get<f32>();
    if (j.contains("maxYaw")) la.maxYaw = j["maxYaw"].get<f32>();
    if (j.contains("minPitch")) la.minPitch = j["minPitch"].get<f32>();
    if (j.contains("maxPitch")) la.maxPitch = j["maxPitch"].get<f32>();
    return la;
}

json SerializeWaypointComponent(const ECS::WaypointComponent& wp) {
    json j;
    j["waypointId"] = wp.waypointId;
    j["index"] = wp.index;
    j["waitTime"] = RF(wp.waitTime);
    j["radius"] = RF(wp.radius);
    return j;
}

ECS::WaypointComponent DeserializeWaypointComponent(const json& j) {
    ECS::WaypointComponent wp;
    if (j.contains("waypointId")) wp.waypointId = j["waypointId"].get<std::string>();
    if (j.contains("index")) wp.index = j["index"].get<i32>();
    if (j.contains("waitTime")) wp.waitTime = j["waitTime"].get<f32>();
    if (j.contains("radius")) wp.radius = j["radius"].get<f32>();
    return wp;
}

// ============================================================================
// Spawning & Timers
// ============================================================================

json SerializeSpawnPointComponent(const ECS::SpawnPointComponent& sp) {
    json j;
    j["spawnId"] = sp.spawnId;
    j["prefabToSpawn"] = sp.prefabToSpawn;
    j["spawnOnStart"] = RF(sp.spawnOnStart);
    j["spawnDelay"] = RF(sp.spawnDelay);
    j["respawnTime"] = RF(sp.respawnTime);
    j["maxSpawns"] = RF(sp.maxSpawns);
    j["spawnRadius"] = RF(sp.spawnRadius);
    j["randomRotation"] = RF(sp.randomRotation);
    return j;
}

ECS::SpawnPointComponent DeserializeSpawnPointComponent(const json& j) {
    ECS::SpawnPointComponent sp;
    if (j.contains("spawnId")) sp.spawnId = j["spawnId"].get<std::string>();
    if (j.contains("prefabToSpawn")) sp.prefabToSpawn = j["prefabToSpawn"].get<std::string>();
    if (j.contains("spawnOnStart")) sp.spawnOnStart = JB(j["spawnOnStart"]);
    if (j.contains("spawnDelay")) sp.spawnDelay = j["spawnDelay"].get<f32>();
    if (j.contains("respawnTime")) sp.respawnTime = j["respawnTime"].get<f32>();
    if (j.contains("maxSpawns")) sp.maxSpawns = j["maxSpawns"].get<i32>();
    if (j.contains("spawnRadius")) sp.spawnRadius = j["spawnRadius"].get<f32>();
    if (j.contains("randomRotation")) sp.randomRotation = JB(j["randomRotation"]);
    return sp;
}

// ============================================================================
// Streaming
// ============================================================================

json SerializeStreamingVolumeComponent(const Scene::StreamingVolumeComponent& sv) {
    json j;
    j["chunkId"] = sv.chunkId;
    j["scenePath"] = sv.scenePath;
    j["halfExtents"] = {RF(sv.halfExtents.x), RF(sv.halfExtents.y), RF(sv.halfExtents.z)};
    j["loadDistance"] = RF(sv.loadDistance);
    j["unloadDistance"] = RF(sv.unloadDistance);
    j["priority"] = static_cast<int>(sv.priority);
    return j;
}

Scene::StreamingVolumeComponent DeserializeStreamingVolumeComponent(const json& j) {
    Scene::StreamingVolumeComponent sv;
    if (j.contains("chunkId")) sv.chunkId = j["chunkId"].get<std::string>();
    if (j.contains("scenePath")) sv.scenePath = j["scenePath"].get<std::string>();
    if (j.contains("halfExtents") && j["halfExtents"].is_array() && j["halfExtents"].size() >= 3) {
        sv.halfExtents = Math::Vector3(j["halfExtents"][0].get<f32>(), j["halfExtents"][1].get<f32>(), j["halfExtents"][2].get<f32>());
    }
    if (j.contains("loadDistance")) sv.loadDistance = j["loadDistance"].get<f32>();
    if (j.contains("unloadDistance")) sv.unloadDistance = j["unloadDistance"].get<f32>();
    if (j.contains("priority")) {
        int p = j["priority"].get<int>();
        if (p >= 0 && p <= 3) sv.priority = static_cast<Scene::StreamPriority>(p);
    }
    return sv;
}

json SerializeStreamingPortalComponent(const Scene::StreamingPortalComponent& sp) {
    json j;
    j["chunkA"] = sp.chunkA;
    j["chunkB"] = sp.chunkB;
    j["halfExtents"] = {RF(sp.halfExtents.x), RF(sp.halfExtents.y), RF(sp.halfExtents.z)};
    j["bidirectional"] = sp.bidirectional;
    return j;
}

Scene::StreamingPortalComponent DeserializeStreamingPortalComponent(const json& j) {
    Scene::StreamingPortalComponent sp;
    if (j.contains("chunkA")) sp.chunkA = j["chunkA"].get<std::string>();
    if (j.contains("chunkB")) sp.chunkB = j["chunkB"].get<std::string>();
    if (j.contains("halfExtents") && j["halfExtents"].is_array() && j["halfExtents"].size() >= 3) {
        sp.halfExtents = Math::Vector3(j["halfExtents"][0].get<f32>(), j["halfExtents"][1].get<f32>(), j["halfExtents"][2].get<f32>());
    }
    if (j.contains("bidirectional")) sp.bidirectional = JB(j["bidirectional"]);
    return sp;
}

json SerializeInteractiveWaterComponent(const Effects::InteractiveWaterComponent& iw) {
    json j;
    j["gridResolution"] = iw.gridResolution;
    j["gridSize"] = RF(iw.gridSize);
    j["baseHeight"] = RF(iw.baseHeight);
    j["waveSpeed"] = RF(iw.waveSpeed);
    j["damping"] = RF(iw.damping);
    j["tension"] = RF(iw.tension);
    j["shallowColor"] = {RF(iw.shallowColor.x), RF(iw.shallowColor.y), RF(iw.shallowColor.z)};
    j["deepColor"] = {RF(iw.deepColor.x), RF(iw.deepColor.y), RF(iw.deepColor.z)};
    j["foamColor"] = {RF(iw.foamColor.x), RF(iw.foamColor.y), RF(iw.foamColor.z)};
    j["depthColorThreshold"] = RF(iw.depthColorThreshold);
    j["foamThreshold"] = RF(iw.foamThreshold);
    j["opacity"] = RF(iw.opacity);
    j["uvScrollSpeed"] = RF(iw.uvScrollSpeed);
    j["uvTiling"] = RF(iw.uvTiling);
    j["interactionRadius"] = RF(iw.interactionRadius);
    j["interactionStrength"] = RF(iw.interactionStrength);
    j["enableBuoyancy"] = iw.enableBuoyancy;
    j["buoyancyForce"] = RF(iw.buoyancyForce);
    j["waterDrag"] = RF(iw.waterDrag);
    j["entryVelocityThreshold"] = RF(iw.entryVelocityThreshold);
    j["currentDirection"] = {RF(iw.currentDirection.x), RF(iw.currentDirection.y), RF(iw.currentDirection.z)};
    j["currentSpeed"] = RF(iw.currentSpeed);
    j["currentPull"] = RF(iw.currentPull);
    j["boundaryMode"] = static_cast<int>(iw.boundaryMode);
    return j;
}

Effects::InteractiveWaterComponent DeserializeInteractiveWaterComponent(const json& j) {
    Effects::InteractiveWaterComponent iw;
    if (j.contains("gridResolution")) iw.gridResolution = j["gridResolution"].get<i32>();
    if (j.contains("gridSize")) iw.gridSize = j["gridSize"].get<f32>();
    if (j.contains("baseHeight")) iw.baseHeight = j["baseHeight"].get<f32>();
    if (j.contains("waveSpeed")) iw.waveSpeed = j["waveSpeed"].get<f32>();
    if (j.contains("damping")) iw.damping = j["damping"].get<f32>();
    if (j.contains("tension")) iw.tension = j["tension"].get<f32>();
    if (j.contains("shallowColor") && j["shallowColor"].is_array() && j["shallowColor"].size() >= 3)
        iw.shallowColor = Math::Vector3(j["shallowColor"][0].get<f32>(), j["shallowColor"][1].get<f32>(), j["shallowColor"][2].get<f32>());
    if (j.contains("deepColor") && j["deepColor"].is_array() && j["deepColor"].size() >= 3)
        iw.deepColor = Math::Vector3(j["deepColor"][0].get<f32>(), j["deepColor"][1].get<f32>(), j["deepColor"][2].get<f32>());
    if (j.contains("foamColor") && j["foamColor"].is_array() && j["foamColor"].size() >= 3)
        iw.foamColor = Math::Vector3(j["foamColor"][0].get<f32>(), j["foamColor"][1].get<f32>(), j["foamColor"][2].get<f32>());
    if (j.contains("depthColorThreshold")) iw.depthColorThreshold = j["depthColorThreshold"].get<f32>();
    if (j.contains("foamThreshold")) iw.foamThreshold = j["foamThreshold"].get<f32>();
    if (j.contains("opacity")) iw.opacity = j["opacity"].get<f32>();
    if (j.contains("uvScrollSpeed")) iw.uvScrollSpeed = j["uvScrollSpeed"].get<f32>();
    if (j.contains("uvTiling")) iw.uvTiling = j["uvTiling"].get<f32>();
    if (j.contains("interactionRadius")) iw.interactionRadius = j["interactionRadius"].get<f32>();
    if (j.contains("interactionStrength")) iw.interactionStrength = j["interactionStrength"].get<f32>();
    if (j.contains("enableBuoyancy")) iw.enableBuoyancy = JB(j["enableBuoyancy"]);
    if (j.contains("buoyancyForce")) iw.buoyancyForce = j["buoyancyForce"].get<f32>();
    if (j.contains("waterDrag")) iw.waterDrag = j["waterDrag"].get<f32>();
    if (j.contains("entryVelocityThreshold")) iw.entryVelocityThreshold = j["entryVelocityThreshold"].get<f32>();
    if (j.contains("currentDirection") && j["currentDirection"].is_array() && j["currentDirection"].size() >= 3)
        iw.currentDirection = Math::Vector3(j["currentDirection"][0].get<f32>(), j["currentDirection"][1].get<f32>(), j["currentDirection"][2].get<f32>());
    if (j.contains("currentSpeed")) iw.currentSpeed = j["currentSpeed"].get<f32>();
    if (j.contains("currentPull")) iw.currentPull = j["currentPull"].get<f32>();
    if (j.contains("boundaryMode")) { int v = j["boundaryMode"].get<int>(); if (v >= 0 && v <= 2) iw.boundaryMode = static_cast<Effects::InteractiveWaterComponent::BoundaryMode>(v); }
    return iw;
}

json SerializeWaterInteractorComponent(const Effects::WaterInteractorComponent& wi) {
    json j;
    j["splashMultiplier"] = RF(wi.splashMultiplier);
    j["wakeWidth"] = RF(wi.wakeWidth);
    j["generateWake"] = wi.generateWake;
    j["applyBuoyancy"] = wi.applyBuoyancy;
    // Per-object density model (currentWaterlog is runtime, not serialized)
    j["density"] = RF(wi.density);
    j["volume"] = RF(wi.volume);
    j["waterlogRate"] = RF(wi.waterlogRate);
    j["waterlogMaxDensity"] = RF(wi.waterlogMaxDensity);
    return j;
}

Effects::WaterInteractorComponent DeserializeWaterInteractorComponent(const json& j) {
    Effects::WaterInteractorComponent wi;
    if (j.contains("splashMultiplier")) wi.splashMultiplier = j["splashMultiplier"].get<f32>();
    if (j.contains("wakeWidth")) wi.wakeWidth = j["wakeWidth"].get<f32>();
    if (j.contains("generateWake")) wi.generateWake = JB(j["generateWake"]);
    if (j.contains("applyBuoyancy")) wi.applyBuoyancy = JB(j["applyBuoyancy"]);
    if (j.contains("density")) wi.density = j["density"].get<f32>();
    if (j.contains("volume")) wi.volume = j["volume"].get<f32>();
    if (j.contains("waterlogRate")) wi.waterlogRate = j["waterlogRate"].get<f32>();
    if (j.contains("waterlogMaxDensity")) wi.waterlogMaxDensity = j["waterlogMaxDensity"].get<f32>();
    return wi;
}

json SerializeTimerComponent(const ECS::TimerComponent& t) {
    json j;
    j["duration"] = RF(t.duration);
    j["loop"] = t.loop;
    j["autoStart"] = t.autoStart;
    return j;
}

ECS::TimerComponent DeserializeTimerComponent(const json& j) {
    ECS::TimerComponent t;
    if (j.contains("duration")) t.duration = j["duration"].get<f32>();
    if (j.contains("loop")) t.loop = JB(j["loop"]);
    if (j.contains("autoStart")) t.autoStart = JB(j["autoStart"]);
    return t;
}

// ============================================================================
// Inventory & Save Data
// ============================================================================

json SerializeInventoryComponent(const ECS::InventoryComponent& inv) {
    json j;
    json slotsArr = json::array();
    for (const auto& s : inv.slots) {
        json slot;
        slot["itemId"] = s.itemId;
        slot["quantity"] = RF(s.quantity);
        slot["maxStack"] = s.maxStack;
        slotsArr.push_back(slot);
    }
    j["slots"] = slotsArr;
    j["maxSlots"] = static_cast<u64>(inv.maxSlots);
    j["coins"] = RF(inv.coins);
    j["gems"] = RF(inv.gems);
    j["keys"] = inv.keys;
    return j;
}

ECS::InventoryComponent DeserializeInventoryComponent(const json& j) {
    ECS::InventoryComponent inv;
    if (j.contains("slots") && j["slots"].is_array()) {
        for (const auto& sj : j["slots"]) {
            ECS::InventoryComponent::InventorySlot s;
            if (sj.contains("itemId")) s.itemId = sj["itemId"].get<std::string>();
            if (sj.contains("quantity")) s.quantity = sj["quantity"].get<i32>();
            if (sj.contains("maxStack")) s.maxStack = sj["maxStack"].get<i32>();
            inv.slots.push_back(s);
        }
    }
    if (j.contains("maxSlots")) inv.maxSlots = j["maxSlots"].get<u64>();
    if (j.contains("coins")) inv.coins = j["coins"].get<i32>();
    if (j.contains("gems")) inv.gems = j["gems"].get<i32>();
    if (j.contains("keys") && j["keys"].is_array() && j["keys"].size() <= 10000) {
        inv.keys = j["keys"].get<std::vector<std::string>>();
    }
    return inv;
}

json SerializeSaveDataComponent(const ECS::SaveDataComponent& sd) {
    json j;
    j["savePosition"] = RF(sd.savePosition);
    j["saveRotation"] = RF(sd.saveRotation);
    j["saveScale"] = RF(sd.saveScale);
    j["saveEnabled"] = RF(sd.saveEnabled);
    j["tier"] = static_cast<u8>(sd.tier);
    if (!sd.tags.empty()) {
        json tagsArr = json::array();
        for (const auto& t : sd.tags) tagsArr.push_back(t);
        j["tags"] = tagsArr;
    }
    json dataArr = json::array();
    for (const auto& p : sd.customData) {
        dataArr.push_back(json::array({p.first, p.second}));
    }
    j["customData"] = dataArr;
    return j;
}

ECS::SaveDataComponent DeserializeSaveDataComponent(const json& j) {
    ECS::SaveDataComponent sd;
    if (j.contains("savePosition")) sd.savePosition = JB(j["savePosition"]);
    if (j.contains("saveRotation")) sd.saveRotation = JB(j["saveRotation"]);
    if (j.contains("saveScale")) sd.saveScale = JB(j["saveScale"]);
    if (j.contains("saveEnabled")) sd.saveEnabled = JB(j["saveEnabled"]);
    if (j.contains("tier")) {
        u8 t = j["tier"].get<u8>();
        if (t < static_cast<u8>(ECS::PersistenceTier::COUNT))
            sd.tier = static_cast<ECS::PersistenceTier>(t);
    }
    if (j.contains("tags") && j["tags"].is_array() && j["tags"].size() <= 1000) {
        for (const auto& t : j["tags"]) {
            sd.tags.push_back(t.get<std::string>());
        }
    }
    if (j.contains("customData") && j["customData"].is_array()) {
        for (const auto& p : j["customData"]) {
            if (p.is_array() && p.size() >= 2) {
                sd.customData.push_back({p[0].get<std::string>(), p[1].get<std::string>()});
            }
        }
    }
    return sd;
}

json SerializeSaveLoadMenuComponent(const ECS::SaveLoadMenuComponent& m) {
    json j;
    j["showOnPause"] = m.showOnPause;
    j["allowManualSave"] = m.allowManualSave;
    j["allowManualLoad"] = m.allowManualLoad;
    j["allowDelete"] = m.allowDelete;
    j["showAutoSaves"] = m.showAutoSaves;
    j["columnsPerRow"] = m.columnsPerRow;
    j["headerText"] = m.headerText;
    return j;
}

ECS::SaveLoadMenuComponent DeserializeSaveLoadMenuComponent(const json& j) {
    ECS::SaveLoadMenuComponent m;
    if (j.contains("showOnPause")) m.showOnPause = JB(j["showOnPause"]);
    if (j.contains("allowManualSave")) m.allowManualSave = JB(j["allowManualSave"]);
    if (j.contains("allowManualLoad")) m.allowManualLoad = JB(j["allowManualLoad"]);
    if (j.contains("allowDelete")) m.allowDelete = JB(j["allowDelete"]);
    if (j.contains("showAutoSaves")) m.showAutoSaves = JB(j["showAutoSaves"]);
    if (j.contains("columnsPerRow")) m.columnsPerRow = j["columnsPerRow"].get<i32>();
    if (j.contains("headerText")) m.headerText = j["headerText"].get<std::string>();
    return m;
}

// ============================================================================
// Flower Components
// ============================================================================

json SerializeJellyMeshComponent(const ECS::JellyMeshComponent& jm) {
    json j;
    j["springStiffness"] = RF(jm.springStiffness);
    j["damping"] = RF(jm.damping);
    j["maxStretch"] = RF(jm.maxStretch);
    return j;
}

ECS::JellyMeshComponent DeserializeJellyMeshComponent(const json& j) {
    ECS::JellyMeshComponent jm;
    if (j.contains("springStiffness")) jm.springStiffness = j["springStiffness"].get<f32>();
    if (j.contains("damping")) jm.damping = j["damping"].get<f32>();
    if (j.contains("maxStretch")) jm.maxStretch = j["maxStretch"].get<f32>();
    return jm;
}

json SerializeTetherComponent(const ECS::TetherComponent& t) {
    json j;
    j["stemEntity"] = static_cast<u64>(t.stemEntity);
    j["connectedEntity"] = static_cast<u64>(t.connectedEntity);
    j["attachLocalPos"] = SerializeVector3(t.attachLocalPos);
    j["maxDistance"] = RF(t.maxDistance);
    j["relativeSpeedThreshold"] = RF(t.relativeSpeedThreshold);
    j["ownSpeedThreshold"] = RF(t.ownSpeedThreshold);
    j["absoluteTravelThreshold"] = RF(t.absoluteTravelThreshold);
    j["relativeTravelThreshold"] = RF(t.relativeTravelThreshold);
    j["armDelay"] = RF(t.armDelay);
    j["autoMass"] = RF(t.autoMass);
    j["autoSpringK"] = RF(t.autoSpringK);
    j["autoDamping"] = RF(t.autoDamping);
    j["autoDrag"] = RF(t.autoDrag);
    j["driveMaxForce"] = RF(t.driveMaxForce);
    return j;
}

ECS::TetherComponent DeserializeTetherComponent(const json& j) {
    ECS::TetherComponent t;
    if (j.contains("stemEntity")) t.stemEntity = static_cast<ECS::Entity>(j["stemEntity"].get<u64>());
    if (j.contains("connectedEntity")) t.connectedEntity = static_cast<ECS::Entity>(j["connectedEntity"].get<u64>());
    if (j.contains("attachLocalPos")) t.attachLocalPos = DeserializeVector3(j["attachLocalPos"]);
    if (j.contains("maxDistance")) t.maxDistance = j["maxDistance"].get<f32>();
    // Backward compat: old breakDistance maps to maxDistance
    else if (j.contains("breakDistance")) t.maxDistance = j["breakDistance"].get<f32>();
    if (j.contains("relativeSpeedThreshold")) t.relativeSpeedThreshold = j["relativeSpeedThreshold"].get<f32>();
    if (j.contains("ownSpeedThreshold")) t.ownSpeedThreshold = j["ownSpeedThreshold"].get<f32>();
    if (j.contains("absoluteTravelThreshold")) t.absoluteTravelThreshold = j["absoluteTravelThreshold"].get<f32>();
    if (j.contains("relativeTravelThreshold")) t.relativeTravelThreshold = j["relativeTravelThreshold"].get<f32>();
    if (j.contains("armDelay")) t.armDelay = j["armDelay"].get<f32>();
    if (j.contains("autoMass")) t.autoMass = j["autoMass"].get<f32>();
    if (j.contains("autoSpringK")) t.autoSpringK = j["autoSpringK"].get<f32>();
    if (j.contains("autoDamping")) t.autoDamping = j["autoDamping"].get<f32>();
    if (j.contains("autoDrag")) t.autoDrag = j["autoDrag"].get<f32>();
    if (j.contains("driveMaxForce")) t.driveMaxForce = j["driveMaxForce"].get<f32>();
    // Backward compat: if connectedEntity missing, default to stemEntity
    if (!j.contains("connectedEntity") && t.connectedEntity == ECS::INVALID_ENTITY) {
        t.connectedEntity = t.stemEntity;
    }
    return t;
}

json SerializeGrabbableComponent(const ECS::GrabbableComponent& g) {
    json j;
    j["grabSpring"] = RF(g.grabSpring);
    j["grabDamper"] = RF(g.grabDamper);
    j["maxAccel"] = RF(g.maxAccel);
    j["maxSpeed"] = RF(g.maxSpeed);
    j["grabRadius"] = RF(g.grabRadius);
    j["windSwayScale"] = RF(g.windSwayScale);
    return j;
}

ECS::GrabbableComponent DeserializeGrabbableComponent(const json& j) {
    ECS::GrabbableComponent g;
    if (j.contains("grabSpring")) g.grabSpring = j["grabSpring"].get<f32>();
    if (j.contains("grabDamper")) g.grabDamper = j["grabDamper"].get<f32>();
    if (j.contains("maxAccel")) g.maxAccel = j["maxAccel"].get<f32>();
    if (j.contains("maxSpeed")) g.maxSpeed = j["maxSpeed"].get<f32>();
    if (j.contains("grabRadius")) g.grabRadius = j["grabRadius"].get<f32>();
    if (j.contains("windSwayScale")) g.windSwayScale = j["windSwayScale"].get<f32>();
    return g;
}

json SerializeFlowerStemComponent(const ECS::FlowerStemComponent& fs) {
    json j;
    j["healthyBonus"] = RF(fs.healthyBonus);
    j["witheredPenalty"] = RF(fs.witheredPenalty);
    j["liquidIntensity"] = RF(fs.liquidIntensity);
    j["groundLevel"] = RF(fs.groundLevel);
    j["sapColor"] = SerializeVector3(fs.sapColor);
    j["stemSwayAmplitude"] = RF(fs.stemSwayAmplitude);
    return j;
}

ECS::FlowerStemComponent DeserializeFlowerStemComponent(const json& j) {
    ECS::FlowerStemComponent fs;
    if (j.contains("healthyBonus")) fs.healthyBonus = j["healthyBonus"].get<f32>();
    if (j.contains("witheredPenalty")) fs.witheredPenalty = j["witheredPenalty"].get<f32>();
    if (j.contains("liquidIntensity")) fs.liquidIntensity = j["liquidIntensity"].get<f32>();
    if (j.contains("groundLevel")) fs.groundLevel = j["groundLevel"].get<f32>();
    if (j.contains("sapColor")) fs.sapColor = DeserializeVector3(j["sapColor"]);
    if (j.contains("stemSwayAmplitude")) fs.stemSwayAmplitude = j["stemSwayAmplitude"].get<f32>();
    return fs;
}

json SerializeFlowerParticleConfigComponent(const ECS::FlowerParticleConfigComponent& fp) {
    json j;
    j["breakBurstCount"] = RF(fp.breakBurstCount);
    j["breakBurstSpeed"] = RF(fp.breakBurstSpeed);
    j["breakBurstUpKick"] = RF(fp.breakBurstUpKick);
    j["breakBurstLifetime"] = RF(fp.breakBurstLifetime);
    j["breakBurstScale"] = RF(fp.breakBurstScale);
    j["breakDripCount"] = RF(fp.breakDripCount);
    j["breakDripSpeed"] = RF(fp.breakDripSpeed);
    j["breakDripLifetime"] = RF(fp.breakDripLifetime);
    j["splashCount"] = RF(fp.splashCount);
    j["splashSpeed"] = RF(fp.splashSpeed);
    j["splashUpKick"] = RF(fp.splashUpKick);
    j["splashLifetime"] = RF(fp.splashLifetime);
    j["tensionDripRate"] = RF(fp.tensionDripRate);
    j["tensionDripThreshold"] = RF(fp.tensionDripThreshold);
    j["tensionSquirtSpeed"] = RF(fp.tensionSquirtSpeed);
    j["particleGravity"] = RF(fp.particleGravity);
    return j;
}

ECS::FlowerParticleConfigComponent DeserializeFlowerParticleConfigComponent(const json& j) {
    ECS::FlowerParticleConfigComponent fp;
    if (j.contains("breakBurstCount")) fp.breakBurstCount = j["breakBurstCount"].get<i32>();
    if (j.contains("breakBurstSpeed")) fp.breakBurstSpeed = j["breakBurstSpeed"].get<f32>();
    if (j.contains("breakBurstUpKick")) fp.breakBurstUpKick = j["breakBurstUpKick"].get<f32>();
    if (j.contains("breakBurstLifetime")) fp.breakBurstLifetime = j["breakBurstLifetime"].get<f32>();
    if (j.contains("breakBurstScale")) fp.breakBurstScale = j["breakBurstScale"].get<f32>();
    if (j.contains("breakDripCount")) fp.breakDripCount = j["breakDripCount"].get<i32>();
    if (j.contains("breakDripSpeed")) fp.breakDripSpeed = j["breakDripSpeed"].get<f32>();
    if (j.contains("breakDripLifetime")) fp.breakDripLifetime = j["breakDripLifetime"].get<f32>();
    if (j.contains("splashCount")) fp.splashCount = j["splashCount"].get<i32>();
    if (j.contains("splashSpeed")) fp.splashSpeed = j["splashSpeed"].get<f32>();
    if (j.contains("splashUpKick")) fp.splashUpKick = j["splashUpKick"].get<f32>();
    if (j.contains("splashLifetime")) fp.splashLifetime = j["splashLifetime"].get<f32>();
    if (j.contains("tensionDripRate")) fp.tensionDripRate = j["tensionDripRate"].get<f32>();
    if (j.contains("tensionDripThreshold")) fp.tensionDripThreshold = j["tensionDripThreshold"].get<f32>();
    if (j.contains("tensionSquirtSpeed")) fp.tensionSquirtSpeed = j["tensionSquirtSpeed"].get<f32>();
    if (j.contains("particleGravity")) fp.particleGravity = j["particleGravity"].get<f32>();
    return fp;
}

// ============================================================================
// LOD, Grass, Vegetation
// ============================================================================

json SerializeLODComponent(const ECS::LODComponent& lod) {
    json j;
    j["levelCount"] = RF(lod.levelCount);
    j["baseDistance"] = RF(lod.baseDistance);
    j["distanceMultiplier"] = RF(lod.distanceMultiplier);
    j["enabled"] = lod.enabled;
    j["autoGenerated"] = RF(lod.autoGenerated);
    j["sourceMaxExtent"] = RF(lod.sourceMaxExtent);
    json ratios = json::array();
    for (int i = 0; i < ECS::LODComponent::MAX_LEVELS; ++i) {
        ratios.push_back(lod.reductionRatios[i]);
    }
    j["reductionRatios"] = ratios;
    json levelsArr = json::array();
    for (int i = 0; i < lod.levelCount; ++i) {
        json level;
        level["mesh"] = SerializeMeshComponent(lod.levels[i].mesh, true);
        level["maxDistance"] = RF(lod.levels[i].maxDistance);
        level["reductionRatio"] = RF(lod.levels[i].reductionRatio);
        levelsArr.push_back(level);
    }
    j["levels"] = levelsArr;
    return j;
}

ECS::LODComponent DeserializeLODComponent(const json& j) {
    ECS::LODComponent lod;
    if (j.contains("levelCount")) lod.levelCount = j["levelCount"].get<i32>();
    if (j.contains("baseDistance")) lod.baseDistance = j["baseDistance"].get<f32>();
    if (j.contains("distanceMultiplier")) lod.distanceMultiplier = j["distanceMultiplier"].get<f32>();
    if (j.contains("enabled")) lod.enabled = JB(j["enabled"]);
    if (j.contains("autoGenerated")) lod.autoGenerated = JB(j["autoGenerated"]);
    if (j.contains("sourceMaxExtent")) lod.sourceMaxExtent = j["sourceMaxExtent"].get<f32>();
    if (j.contains("reductionRatios") && j["reductionRatios"].is_array()) {
        for (int i = 0; i < ECS::LODComponent::MAX_LEVELS && i < static_cast<int>(j["reductionRatios"].size()); ++i) {
            lod.reductionRatios[i] = j["reductionRatios"][i].get<f32>();
        }
    }
    if (j.contains("levels") && j["levels"].is_array()) {
        for (int i = 0; i < lod.levelCount && i < static_cast<int>(j["levels"].size()); ++i) {
            const auto& lj = j["levels"][i];
            if (lj.contains("mesh")) lod.levels[i].mesh = DeserializeMeshComponent(lj["mesh"]);
            if (lj.contains("maxDistance")) lod.levels[i].maxDistance = lj["maxDistance"].get<f32>();
            if (lj.contains("reductionRatio")) lod.levels[i].reductionRatio = lj["reductionRatio"].get<f32>();
            lod.levels[i].vertexCount = static_cast<u32>(lod.levels[i].mesh.vertices.size());
            lod.levels[i].triangleCount = static_cast<u32>(lod.levels[i].mesh.indices.size() / 3);
        }
    }
    return lod;
}

json SerializeGrassVolumeComponent(const ECS::GrassVolumeComponent& gv) {
    json j;
    j["halfExtents"] = SerializeVector3(gv.halfExtents);
    j["density"] = gv.density;
    j["bladeHeight"] = RF(gv.bladeHeight);
    j["bladeHeightVariance"] = RF(gv.bladeHeightVariance);
    j["bladeWidth"] = RF(gv.bladeWidth);
    j["baseColor"] = SerializeVector3(gv.baseColor);
    j["tipColor"] = SerializeVector3(gv.tipColor);
    j["windSwayStrength"] = RF(gv.windSwayStrength);
    if (!gv.customAssetPath.empty()) j["customAssetPath"] = gv.customAssetPath;
    return j;
}

ECS::GrassVolumeComponent DeserializeGrassVolumeComponent(const json& j) {
    ECS::GrassVolumeComponent gv;
    if (j.contains("halfExtents")) gv.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("density")) gv.density = j["density"].get<u32>();
    if (j.contains("bladeHeight")) gv.bladeHeight = j["bladeHeight"].get<f32>();
    if (j.contains("bladeHeightVariance")) gv.bladeHeightVariance = j["bladeHeightVariance"].get<f32>();
    if (j.contains("bladeWidth")) gv.bladeWidth = j["bladeWidth"].get<f32>();
    if (j.contains("baseColor")) gv.baseColor = DeserializeVector3(j["baseColor"]);
    if (j.contains("tipColor")) gv.tipColor = DeserializeVector3(j["tipColor"]);
    if (j.contains("windSwayStrength")) gv.windSwayStrength = j["windSwayStrength"].get<f32>();
    if (j.contains("customAssetPath")) gv.customAssetPath = SafeStr(j["customAssetPath"], MAX_STR_PATH);
    return gv;
}

json SerializeVegetationComponent(const ECS::VegetationComponent& v) {
    json j;
    j["swayStrength"] = RF(v.swayStrength);
    j["swayFrequency"] = RF(v.swayFrequency);
    j["useVertexColorWeight"] = RF(v.useVertexColorWeight);
    return j;
}

ECS::VegetationComponent DeserializeVegetationComponent(const json& j) {
    ECS::VegetationComponent v;
    if (j.contains("swayStrength")) v.swayStrength = j["swayStrength"].get<f32>();
    if (j.contains("swayFrequency")) v.swayFrequency = j["swayFrequency"].get<f32>();
    if (j.contains("useVertexColorWeight")) v.useVertexColorWeight = JB(j["useVertexColorWeight"]);
    return v;
}

json SerializeViewmodelComponent(const ECS::ViewmodelComponent& v) {
    json j;
    j["enabled"] = v.enabled;
    return j;
}

ECS::ViewmodelComponent DeserializeViewmodelComponent(const json& j) {
    ECS::ViewmodelComponent v;
    if (j.contains("enabled")) v.enabled = JB(j["enabled"]);
    return v;
}

json SerializeVirtualCameraComponent(const ECS::VirtualCameraComponent& vc) {
    json j;
    j["enabled"] = vc.enabled;
    j["priority"] = vc.priority;
    j["shot"] = static_cast<u8>(vc.shot);
    j["follow"] = static_cast<u64>(vc.follow);
    j["lookAt"] = static_cast<u64>(vc.lookAt);
    j["offset"] = SerializeVector3(vc.offset);
    j["offsetInFollowSpace"] = vc.offsetInFollowSpace;
    j["lookOffset"] = SerializeVector3(vc.lookOffset);
    j["fov"] = RF(vc.fov);
    j["damping"] = RF(vc.damping);
    j["blendTime"] = RF(vc.blendTime);
    return j;
}

ECS::VirtualCameraComponent DeserializeVirtualCameraComponent(const json& j) {
    ECS::VirtualCameraComponent vc;
    if (j.contains("enabled")) vc.enabled = JB(j["enabled"]);
    if (j.contains("priority")) vc.priority = j["priority"].get<i32>();
    if (j.contains("shot")) { u8 v = j["shot"].get<u8>(); if (v < static_cast<u8>(ECS::VCamShot::Count)) vc.shot = static_cast<ECS::VCamShot>(v); }
    if (j.contains("follow")) vc.follow = static_cast<ECS::Entity>(j["follow"].get<u64>());
    if (j.contains("lookAt")) vc.lookAt = static_cast<ECS::Entity>(j["lookAt"].get<u64>());
    if (j.contains("offset")) vc.offset = DeserializeVector3(j["offset"]);
    if (j.contains("offsetInFollowSpace")) vc.offsetInFollowSpace = JB(j["offsetInFollowSpace"]);
    if (j.contains("lookOffset")) vc.lookOffset = DeserializeVector3(j["lookOffset"]);
    if (j.contains("fov")) vc.fov = j["fov"].get<f32>();
    if (j.contains("damping")) vc.damping = j["damping"].get<f32>();
    if (j.contains("blendTime")) vc.blendTime = j["blendTime"].get<f32>();
    return vc;
}

json SerializeDamageResistanceComponent(const ECS::DamageResistanceComponent& r) {
    json j;
    j["physicalMult"] = RF(r.physicalMult);
    j["fireMult"] = RF(r.fireMult);
    j["iceMult"] = RF(r.iceMult);
    j["electricMult"] = RF(r.electricMult);
    j["poisonMult"] = RF(r.poisonMult);
    j["magicMult"] = RF(r.magicMult);
    return j;
}

ECS::DamageResistanceComponent DeserializeDamageResistanceComponent(const json& j) {
    ECS::DamageResistanceComponent r;
    if (j.contains("physicalMult")) r.physicalMult = j["physicalMult"].get<f32>();
    if (j.contains("fireMult")) r.fireMult = j["fireMult"].get<f32>();
    if (j.contains("iceMult")) r.iceMult = j["iceMult"].get<f32>();
    if (j.contains("electricMult")) r.electricMult = j["electricMult"].get<f32>();
    if (j.contains("poisonMult")) r.poisonMult = j["poisonMult"].get<f32>();
    if (j.contains("magicMult")) r.magicMult = j["magicMult"].get<f32>();
    return r;
}

json SerializeResourceComponent(const ECS::ResourceComponent& r) {
    json j;
    j["resourceName"] = r.resourceName;
    j["maxValue"] = RF(r.maxValue);
    j["currentValue"] = RF(r.currentValue);
    j["regenRate"] = RF(r.regenRate);
    j["regenDelay"] = RF(r.regenDelay);
    j["depletedThreshold"] = RF(r.depletedThreshold);
    j["sprintCostPerSec"] = RF(r.sprintCostPerSec);
    j["jumpCost"] = RF(r.jumpCost);
    j["dashCost"] = RF(r.dashCost);
    j["attackCost"] = RF(r.attackCost);
    return j;
}

ECS::ResourceComponent DeserializeResourceComponent(const json& j) {
    ECS::ResourceComponent r;
    if (j.contains("resourceName")) r.resourceName = j["resourceName"].get<std::string>();
    if (j.contains("maxValue")) r.maxValue = j["maxValue"].get<f32>();
    if (j.contains("currentValue")) r.currentValue = j["currentValue"].get<f32>();
    if (j.contains("regenRate")) r.regenRate = j["regenRate"].get<f32>();
    if (j.contains("regenDelay")) r.regenDelay = j["regenDelay"].get<f32>();
    if (j.contains("depletedThreshold")) r.depletedThreshold = j["depletedThreshold"].get<f32>();
    if (j.contains("sprintCostPerSec")) r.sprintCostPerSec = j["sprintCostPerSec"].get<f32>();
    if (j.contains("jumpCost")) r.jumpCost = j["jumpCost"].get<f32>();
    if (j.contains("dashCost")) r.dashCost = j["dashCost"].get<f32>();
    if (j.contains("attackCost")) r.attackCost = j["attackCost"].get<f32>();
    return r;
}

json SerializeFootstepComponent(const ECS::FootstepComponent& f) {
    json j;
    j["defaultWalkSound"] = f.defaultWalkSound;
    j["defaultRunSound"] = f.defaultRunSound;
    j["walkStepInterval"] = RF(f.walkStepInterval);
    j["runStepInterval"] = RF(f.runStepInterval);
    j["volume"] = RF(f.volume);
    j["pitchVariance"] = RF(f.pitchVariance);
    j["currentSurface"] = f.currentSurface;
    json surfaces = json::array();
    for (const auto& s : f.surfaceSounds) {
        json sj;
        sj["surfaceTag"] = s.surfaceTag;
        sj["walkSound"] = s.walkSound;
        sj["runSound"] = s.runSound;
        sj["volumeScale"] = RF(s.volumeScale);
        surfaces.push_back(sj);
    }
    j["surfaceSounds"] = surfaces;
    return j;
}

ECS::FootstepComponent DeserializeFootstepComponent(const json& j) {
    ECS::FootstepComponent f;
    if (j.contains("defaultWalkSound")) f.defaultWalkSound = SafeStr(j["defaultWalkSound"], MAX_STR_PATH);
    if (j.contains("defaultRunSound")) f.defaultRunSound = SafeStr(j["defaultRunSound"], MAX_STR_PATH);
    if (j.contains("walkStepInterval")) f.walkStepInterval = j["walkStepInterval"].get<f32>();
    if (j.contains("runStepInterval")) f.runStepInterval = j["runStepInterval"].get<f32>();
    if (j.contains("volume")) f.volume = j["volume"].get<f32>();
    if (j.contains("pitchVariance")) f.pitchVariance = j["pitchVariance"].get<f32>();
    if (j.contains("currentSurface")) f.currentSurface = SafeStr(j["currentSurface"], MAX_STR_NAME);
    if (j.contains("surfaceSounds") && j["surfaceSounds"].is_array()) {
        for (const auto& sj : j["surfaceSounds"]) {
            ECS::FootstepComponent::SurfaceSound s;
            if (sj.contains("surfaceTag")) s.surfaceTag = SafeStr(sj["surfaceTag"], MAX_STR_NAME);
            if (sj.contains("walkSound")) s.walkSound = SafeStr(sj["walkSound"], MAX_STR_PATH);
            if (sj.contains("runSound")) s.runSound = SafeStr(sj["runSound"], MAX_STR_PATH);
            if (sj.contains("volumeScale")) s.volumeScale = sj["volumeScale"].get<f32>();
            f.surfaceSounds.push_back(s);
        }
    }
    return f;
}

json SerializePoolableComponent(const ECS::PoolableComponent& p) {
    json j;
    j["poolId"] = p.poolId;
    j["lifetime"] = RF(p.lifetime);
    return j;
}

ECS::PoolableComponent DeserializePoolableComponent(const json& j) {
    ECS::PoolableComponent p;
    if (j.contains("poolId")) p.poolId = j["poolId"].get<std::string>();
    if (j.contains("lifetime")) p.lifetime = j["lifetime"].get<f32>();
    return p;
}

json SerializeDynamicDifficultyComponent(const ECS::DynamicDifficultyComponent& dd) {
    json j;
    j["enabled"] = dd.enabled;
    j["visibleToPlayer"] = dd.visibleToPlayer;
    j["baseDifficulty"] = dd.baseDifficulty;
    j["adjustmentRange"] = RF(dd.adjustmentRange);
    j["smoothingRate"] = RF(dd.smoothingRate);

    // Input metrics config
    j["trackDeaths"] = dd.trackDeaths;
    j["deathWeight"] = RF(dd.deathWeight);
    j["deathWindow"] = dd.deathWindow;
    j["trackHealth"] = dd.trackHealth;
    j["healthWeight"] = RF(dd.healthWeight);
    j["trackAccuracy"] = dd.trackAccuracy;
    j["accuracyWeight"] = RF(dd.accuracyWeight);
    j["trackTime"] = dd.trackTime;
    j["timeWeight"] = RF(dd.timeWeight);
    j["expectedCompletionTime"] = RF(dd.expectedCompletionTime);
    j["trackResources"] = dd.trackResources;
    j["resourceWeight"] = RF(dd.resourceWeight);
    j["trackCheckpointHealth"] = dd.trackCheckpointHealth;
    j["checkpointHealthWeight"] = RF(dd.checkpointHealthWeight);
    j["playerEntity"] = static_cast<u64>(dd.playerEntity);

    // Output adjustments config
    j["adjustEnemyDamage"] = dd.adjustEnemyDamage;
    j["enemyDamageRange"] = RF(dd.enemyDamageRange);
    j["adjustEnemyHealth"] = dd.adjustEnemyHealth;
    j["enemyHealthRange"] = RF(dd.enemyHealthRange);
    j["adjustAIAggression"] = dd.adjustAIAggression;
    j["aiAggressionRange"] = RF(dd.aiAggressionRange);
    j["adjustResourceDrops"] = dd.adjustResourceDrops;
    j["resourceDropRange"] = RF(dd.resourceDropRange);
    j["adjustHintFrequency"] = dd.adjustHintFrequency;
    j["deathsBeforeHint"] = dd.deathsBeforeHint;
    j["adjustCheckpointFrequency"] = dd.adjustCheckpointFrequency;
    j["checkpointRange"] = RF(dd.checkpointRange);
    return j;
}

ECS::DynamicDifficultyComponent DeserializeDynamicDifficultyComponent(const json& j) {
    ECS::DynamicDifficultyComponent dd;
    if (j.contains("enabled")) dd.enabled = JB(j["enabled"]);
    if (j.contains("visibleToPlayer")) dd.visibleToPlayer = JB(j["visibleToPlayer"]);
    if (j.contains("baseDifficulty")) dd.baseDifficulty = std::min(j["baseDifficulty"].get<u32>(), 3u);
    if (j.contains("adjustmentRange")) dd.adjustmentRange = j["adjustmentRange"].get<f32>();
    if (j.contains("smoothingRate")) dd.smoothingRate = j["smoothingRate"].get<f32>();

    if (j.contains("trackDeaths")) dd.trackDeaths = JB(j["trackDeaths"]);
    if (j.contains("deathWeight")) dd.deathWeight = j["deathWeight"].get<f32>();
    if (j.contains("deathWindow")) dd.deathWindow = j["deathWindow"].get<u32>();
    if (j.contains("trackHealth")) dd.trackHealth = JB(j["trackHealth"]);
    if (j.contains("healthWeight")) dd.healthWeight = j["healthWeight"].get<f32>();
    if (j.contains("trackAccuracy")) dd.trackAccuracy = JB(j["trackAccuracy"]);
    if (j.contains("accuracyWeight")) dd.accuracyWeight = j["accuracyWeight"].get<f32>();
    if (j.contains("trackTime")) dd.trackTime = JB(j["trackTime"]);
    if (j.contains("timeWeight")) dd.timeWeight = j["timeWeight"].get<f32>();
    if (j.contains("expectedCompletionTime")) dd.expectedCompletionTime = j["expectedCompletionTime"].get<f32>();
    if (j.contains("trackResources")) dd.trackResources = JB(j["trackResources"]);
    if (j.contains("resourceWeight")) dd.resourceWeight = j["resourceWeight"].get<f32>();
    if (j.contains("trackCheckpointHealth")) dd.trackCheckpointHealth = JB(j["trackCheckpointHealth"]);
    if (j.contains("checkpointHealthWeight")) dd.checkpointHealthWeight = j["checkpointHealthWeight"].get<f32>();
    if (j.contains("playerEntity")) dd.playerEntity = static_cast<ECS::Entity>(j["playerEntity"].get<u64>());

    if (j.contains("adjustEnemyDamage")) dd.adjustEnemyDamage = JB(j["adjustEnemyDamage"]);
    if (j.contains("enemyDamageRange")) dd.enemyDamageRange = j["enemyDamageRange"].get<f32>();
    if (j.contains("adjustEnemyHealth")) dd.adjustEnemyHealth = JB(j["adjustEnemyHealth"]);
    if (j.contains("enemyHealthRange")) dd.enemyHealthRange = j["enemyHealthRange"].get<f32>();
    if (j.contains("adjustAIAggression")) dd.adjustAIAggression = JB(j["adjustAIAggression"]);
    if (j.contains("aiAggressionRange")) dd.aiAggressionRange = j["aiAggressionRange"].get<f32>();
    if (j.contains("adjustResourceDrops")) dd.adjustResourceDrops = JB(j["adjustResourceDrops"]);
    if (j.contains("resourceDropRange")) dd.resourceDropRange = j["resourceDropRange"].get<f32>();
    if (j.contains("adjustHintFrequency")) dd.adjustHintFrequency = JB(j["adjustHintFrequency"]);
    if (j.contains("deathsBeforeHint")) dd.deathsBeforeHint = j["deathsBeforeHint"].get<u32>();
    if (j.contains("adjustCheckpointFrequency")) dd.adjustCheckpointFrequency = JB(j["adjustCheckpointFrequency"]);
    if (j.contains("checkpointRange")) dd.checkpointRange = j["checkpointRange"].get<f32>();
    return dd;
}

json SerializeArtStyleComponent(const ECS::ArtStyleComponent& as) {
    json j;
    j["style"] = static_cast<u32>(as.style);
    j["propagateToChildren"] = as.propagateToChildren;

    // Pre-PBR
    j["prePBR_halfLambert"] = as.prePBR_halfLambert;
    j["prePBR_flatShading"] = as.prePBR_flatShading;
    j["prePBR_gouraudOnly"] = as.prePBR_gouraudOnly;
    j["prePBR_specularStrength"] = RF(as.prePBR_specularStrength);

    // Hand-Painted
    j["handPainted_lightWrapAmount"] = RF(as.handPainted_lightWrapAmount);
    j["handPainted_lightRampMode"] = as.handPainted_lightRampMode;
    j["handPainted_saturationBoost"] = RF(as.handPainted_saturationBoost);

    // Cel/Toon
    j["cel_diffuseBands"] = RF(as.cel_diffuseBands);
    j["cel_specularCutoff"] = RF(as.cel_specularCutoff);
    j["cel_outlineWidth"] = RF(as.cel_outlineWidth);
    j["cel_outlineColor"] = SerializeVector3(as.cel_outlineColor);
    j["cel_rimStrength"] = RF(as.cel_rimStrength);
    j["cel_shadowMode"] = as.cel_shadowMode;
    j["cel_lightRampMode"] = as.cel_lightRampMode;

    // NPR
    j["npr_celOutline"] = as.npr_celOutline;
    j["npr_outlineThickness"] = RF(as.npr_outlineThickness);
    j["npr_curvatureWeight"] = RF(as.npr_curvatureWeight);
    j["npr_stipplePatternMask"] = as.npr_stipplePatternMask;
    j["npr_stippleDensity"] = RF(as.npr_stippleDensity);
    j["npr_stippleStrength"] = RF(as.npr_stippleStrength);
    j["npr_diffuseBands"] = as.npr_diffuseBands;

    // Retro
    j["retro_vertexSnapping"] = as.retro_vertexSnapping;
    j["retro_snapResolution"] = as.retro_snapResolution;
    j["retro_affineTexturing"] = as.retro_affineTexturing;
    j["retro_uvQuantize"] = as.retro_uvQuantize;
    j["retro_flatShading"] = as.retro_flatShading;
    j["retro_texturePageSize"] = RF(as.retro_texturePageSize);
    j["retro_posterizeLevels"] = RF(as.retro_posterizeLevels);

    // Pixel Art
    j["pixel_paletteColors"] = as.pixel_paletteColors;
    j["pixel_paletteMode"] = as.pixel_paletteMode;
    j["pixel_pointFiltering"] = as.pixel_pointFiltering;
    j["pixel_normalQuantizeSteps"] = as.pixel_normalQuantizeSteps;

    // Material Expression
    j["matExpr_surfaceNoiseScale"] = RF(as.matExpr_surfaceNoiseScale);
    j["matExpr_surfaceNoiseStrength"] = RF(as.matExpr_surfaceNoiseStrength);
    j["matExpr_sssIntensity"] = RF(as.matExpr_sssIntensity);
    j["matExpr_sssRadius"] = RF(as.matExpr_sssRadius);
    j["matExpr_sssColor"] = SerializeVector3(as.matExpr_sssColor);

    // Analog
    j["analog_filmGrain"] = as.analog_filmGrain;
    j["analog_filmGrainIntensity"] = RF(as.analog_filmGrainIntensity);
    j["analog_chromaticAberration"] = as.analog_chromaticAberration;
    j["analog_chromaticIntensity"] = RF(as.analog_chromaticIntensity);
    j["analog_vhsEnabled"] = as.analog_vhsEnabled;
    j["analog_vhsTrackingIntensity"] = RF(as.analog_vhsTrackingIntensity);
    j["analog_crtEnabled"] = as.analog_crtEnabled;
    j["analog_scanlineIntensity"] = RF(as.analog_scanlineIntensity);
    j["analog_filmGateWeave"] = as.analog_filmGateWeave;
    j["analog_gateWeaveIntensity"] = RF(as.analog_gateWeaveIntensity);
    j["analog_lightLeaks"] = as.analog_lightLeaks;
    j["analog_lightLeakIntensity"] = RF(as.analog_lightLeakIntensity);

    return j;
}

ECS::ArtStyleComponent DeserializeArtStyleComponent(const json& j) {
    ECS::ArtStyleComponent as;
    if (j.contains("style")) as.style = static_cast<ECS::ArtStyleType>(std::min(j["style"].get<u32>(), static_cast<u32>(ECS::ArtStyleType::Count) - 1));
    if (j.contains("propagateToChildren")) as.propagateToChildren = JB(j["propagateToChildren"]);

    // Pre-PBR
    if (j.contains("prePBR_halfLambert")) as.prePBR_halfLambert = JB(j["prePBR_halfLambert"]);
    if (j.contains("prePBR_flatShading")) as.prePBR_flatShading = JB(j["prePBR_flatShading"]);
    if (j.contains("prePBR_gouraudOnly")) as.prePBR_gouraudOnly = JB(j["prePBR_gouraudOnly"]);
    if (j.contains("prePBR_specularStrength")) as.prePBR_specularStrength = j["prePBR_specularStrength"].get<f32>();

    // Hand-Painted
    if (j.contains("handPainted_lightWrapAmount")) as.handPainted_lightWrapAmount = j["handPainted_lightWrapAmount"].get<f32>();
    if (j.contains("handPainted_lightRampMode")) as.handPainted_lightRampMode = j["handPainted_lightRampMode"].get<u8>();
    if (j.contains("handPainted_saturationBoost")) as.handPainted_saturationBoost = j["handPainted_saturationBoost"].get<f32>();

    // Cel/Toon
    if (j.contains("cel_diffuseBands")) as.cel_diffuseBands = j["cel_diffuseBands"].get<f32>();
    if (j.contains("cel_specularCutoff")) as.cel_specularCutoff = j["cel_specularCutoff"].get<f32>();
    if (j.contains("cel_outlineWidth")) as.cel_outlineWidth = j["cel_outlineWidth"].get<f32>();
    if (j.contains("cel_outlineColor")) as.cel_outlineColor = DeserializeVector3(j["cel_outlineColor"]);
    if (j.contains("cel_rimStrength")) as.cel_rimStrength = j["cel_rimStrength"].get<f32>();
    if (j.contains("cel_shadowMode")) as.cel_shadowMode = j["cel_shadowMode"].get<u8>();
    if (j.contains("cel_lightRampMode")) as.cel_lightRampMode = j["cel_lightRampMode"].get<u8>();

    // NPR
    if (j.contains("npr_celOutline")) as.npr_celOutline = JB(j["npr_celOutline"]);
    if (j.contains("npr_outlineThickness")) as.npr_outlineThickness = j["npr_outlineThickness"].get<f32>();
    if (j.contains("npr_curvatureWeight")) as.npr_curvatureWeight = j["npr_curvatureWeight"].get<f32>();
    if (j.contains("npr_stipplePatternMask")) as.npr_stipplePatternMask = j["npr_stipplePatternMask"].get<u32>();
    if (j.contains("npr_stippleDensity")) as.npr_stippleDensity = j["npr_stippleDensity"].get<f32>();
    if (j.contains("npr_stippleStrength")) as.npr_stippleStrength = j["npr_stippleStrength"].get<f32>();
    if (j.contains("npr_diffuseBands")) as.npr_diffuseBands = j["npr_diffuseBands"].get<u8>();

    // Retro
    if (j.contains("retro_vertexSnapping")) as.retro_vertexSnapping = JB(j["retro_vertexSnapping"]);
    if (j.contains("retro_snapResolution")) as.retro_snapResolution = j["retro_snapResolution"].get<u8>();
    if (j.contains("retro_affineTexturing")) as.retro_affineTexturing = JB(j["retro_affineTexturing"]);
    if (j.contains("retro_uvQuantize")) as.retro_uvQuantize = JB(j["retro_uvQuantize"]);
    if (j.contains("retro_flatShading")) as.retro_flatShading = JB(j["retro_flatShading"]);
    if (j.contains("retro_texturePageSize")) as.retro_texturePageSize = j["retro_texturePageSize"].get<f32>();
    if (j.contains("retro_posterizeLevels")) as.retro_posterizeLevels = j["retro_posterizeLevels"].get<f32>();

    // Pixel Art
    if (j.contains("pixel_paletteColors")) as.pixel_paletteColors = j["pixel_paletteColors"].get<u32>();
    if (j.contains("pixel_paletteMode")) as.pixel_paletteMode = j["pixel_paletteMode"].get<u32>();
    if (j.contains("pixel_pointFiltering")) as.pixel_pointFiltering = JB(j["pixel_pointFiltering"]);
    if (j.contains("pixel_normalQuantizeSteps")) as.pixel_normalQuantizeSteps = j["pixel_normalQuantizeSteps"].get<u32>();

    // Material Expression
    if (j.contains("matExpr_surfaceNoiseScale")) as.matExpr_surfaceNoiseScale = j["matExpr_surfaceNoiseScale"].get<f32>();
    if (j.contains("matExpr_surfaceNoiseStrength")) as.matExpr_surfaceNoiseStrength = j["matExpr_surfaceNoiseStrength"].get<f32>();
    if (j.contains("matExpr_sssIntensity")) as.matExpr_sssIntensity = j["matExpr_sssIntensity"].get<f32>();
    if (j.contains("matExpr_sssRadius")) as.matExpr_sssRadius = j["matExpr_sssRadius"].get<f32>();
    if (j.contains("matExpr_sssColor")) as.matExpr_sssColor = DeserializeVector3(j["matExpr_sssColor"]);

    // Analog
    if (j.contains("analog_filmGrain")) as.analog_filmGrain = JB(j["analog_filmGrain"]);
    if (j.contains("analog_filmGrainIntensity")) as.analog_filmGrainIntensity = j["analog_filmGrainIntensity"].get<f32>();
    if (j.contains("analog_chromaticAberration")) as.analog_chromaticAberration = JB(j["analog_chromaticAberration"]);
    if (j.contains("analog_chromaticIntensity")) as.analog_chromaticIntensity = j["analog_chromaticIntensity"].get<f32>();
    if (j.contains("analog_vhsEnabled")) as.analog_vhsEnabled = JB(j["analog_vhsEnabled"]);
    if (j.contains("analog_vhsTrackingIntensity")) as.analog_vhsTrackingIntensity = j["analog_vhsTrackingIntensity"].get<f32>();
    if (j.contains("analog_crtEnabled")) as.analog_crtEnabled = JB(j["analog_crtEnabled"]);
    if (j.contains("analog_scanlineIntensity")) as.analog_scanlineIntensity = j["analog_scanlineIntensity"].get<f32>();
    if (j.contains("analog_filmGateWeave")) as.analog_filmGateWeave = JB(j["analog_filmGateWeave"]);
    if (j.contains("analog_gateWeaveIntensity")) as.analog_gateWeaveIntensity = j["analog_gateWeaveIntensity"].get<f32>();
    if (j.contains("analog_lightLeaks")) as.analog_lightLeaks = JB(j["analog_lightLeaks"]);
    if (j.contains("analog_lightLeakIntensity")) as.analog_lightLeakIntensity = j["analog_lightLeakIntensity"].get<f32>();

    return as;
}

json SerializeQuestStateComponent(const ECS::QuestStateComponent& q) {
    json j;
    j["questId"] = q.questId;
    j["status"] = static_cast<u8>(q.status);
    j["currentObjective"] = RF(q.currentObjective);
    j["timeElapsed"] = RF(q.timeElapsed);
    json flags = json::array();
    for (const auto& [name, complete] : q.objectiveFlags) {
        json fj;
        fj["name"] = name;
        fj["complete"] = complete;
        flags.push_back(fj);
    }
    j["objectiveFlags"] = flags;
    return j;
}

ECS::QuestStateComponent DeserializeQuestStateComponent(const json& j) {
    ECS::QuestStateComponent q;
    if (j.contains("questId")) q.questId = j["questId"].get<std::string>();
    if (j.contains("status")) { u8 v = j["status"].get<u8>(); if (v <= 3) q.status = static_cast<ECS::QuestStateComponent::Status>(v); }
    if (j.contains("currentObjective")) q.currentObjective = j["currentObjective"].get<i32>();
    if (j.contains("timeElapsed")) q.timeElapsed = j["timeElapsed"].get<f32>();
    if (j.contains("objectiveFlags") && j["objectiveFlags"].is_array()) {
        for (const auto& fj : j["objectiveFlags"]) {
            std::string name = fj.value("name", "");
            bool complete = fj.contains("complete") ? JB(fj["complete"]) : false;
            q.objectiveFlags.push_back({name, complete});
        }
    }
    return q;
}

json SerializeHUDWidgetComponent(const ECS::HUDWidgetComponent& h) {
    json j;
    j["type"] = static_cast<u8>(h.type);
    j["visible"] = RF(h.visible);
    j["screenSpace"] = RF(h.screenSpace);
    j["anchorX"] = RF(h.anchorX);
    j["anchorY"] = RF(h.anchorY);
    j["width"] = h.width;
    j["height"] = h.height;
    j["fillColor"] = SerializeVector3(h.fillColor);
    j["bgColor"] = SerializeVector3(h.bgColor);
    j["textColor"] = SerializeVector3(h.textColor);
    j["fontSize"] = RF(h.fontSize);
    j["text"] = h.text;
    j["sourceEntity"] = static_cast<u64>(h.sourceEntity);
    j["bindField"] = h.bindField;
    j["currentValue"] = RF(h.currentValue);
    j["maxValue"] = RF(h.maxValue);
    j["worldOffset"] = SerializeVector3(h.worldOffset);
    j["maxRenderDistance"] = RF(h.maxRenderDistance);
    return j;
}

ECS::HUDWidgetComponent DeserializeHUDWidgetComponent(const json& j) {
    ECS::HUDWidgetComponent h;
    if (j.contains("type")) { u8 v = j["type"].get<u8>(); if (v <= 5) h.type = static_cast<ECS::HUDWidgetComponent::WidgetType>(v); }
    if (j.contains("visible")) h.visible = JB(j["visible"]);
    if (j.contains("screenSpace")) h.screenSpace = JB(j["screenSpace"]);
    if (j.contains("anchorX")) h.anchorX = j["anchorX"].get<f32>();
    if (j.contains("anchorY")) h.anchorY = j["anchorY"].get<f32>();
    if (j.contains("width")) h.width = j["width"].get<f32>();
    if (j.contains("height")) h.height = j["height"].get<f32>();
    if (j.contains("fillColor")) h.fillColor = DeserializeVector3(j["fillColor"]);
    if (j.contains("bgColor")) h.bgColor = DeserializeVector3(j["bgColor"]);
    if (j.contains("textColor")) h.textColor = DeserializeVector3(j["textColor"]);
    if (j.contains("fontSize")) h.fontSize = j["fontSize"].get<f32>();
    if (j.contains("text")) h.text = j["text"].get<std::string>();
    if (j.contains("sourceEntity")) h.sourceEntity = static_cast<ECS::Entity>(j["sourceEntity"].get<u64>());
    if (j.contains("bindField")) h.bindField = j["bindField"].get<std::string>();
    if (j.contains("currentValue")) h.currentValue = j["currentValue"].get<f32>();
    if (j.contains("maxValue")) h.maxValue = j["maxValue"].get<f32>();
    if (j.contains("worldOffset")) h.worldOffset = DeserializeVector3(j["worldOffset"]);
    if (j.contains("maxRenderDistance")) h.maxRenderDistance = j["maxRenderDistance"].get<f32>();
    return h;
}

json SerializeUIElement(const GUI::UIElement& e) {
    json j;
    j["id"] = RF(e.id);
    j["name"] = e.name;
    j["type"] = static_cast<u8>(e.type);
    j["visible"] = RF(e.visible);
    j["enabled"] = e.enabled;
    j["focusable"] = e.focusable;
    j["tabOrder"] = e.tabOrder;
    // Container layout (unified display P3)
    j["layoutMode"] = static_cast<u8>(e.layoutMode);
    j["layoutSpacing"] = RF(e.layoutSpacing);
    j["layoutPaddingX"] = RF(e.layoutPaddingX);
    j["layoutPaddingY"] = RF(e.layoutPaddingY);
    j["layoutAlign"] = static_cast<u8>(e.layoutAlign);
    j["parentId"] = e.parentId;
    j["childIds"] = e.childIds;

    // Anchor
    json anchor;
    anchor["anchorMin"] = SerializeVector2(e.anchor.anchorMin);
    anchor["anchorMax"] = SerializeVector2(e.anchor.anchorMax);
    anchor["pivot"] = SerializeVector2(e.anchor.pivot);
    anchor["offsetLeft"] = RF(e.anchor.offsetLeft);
    anchor["offsetRight"] = RF(e.anchor.offsetRight);
    anchor["offsetTop"] = RF(e.anchor.offsetTop);
    anchor["offsetBottom"] = RF(e.anchor.offsetBottom);
    j["anchor"] = anchor;

    // Style overrides
    json style;
    style["bgColor"] = SerializeVector3(e.style.bgColor);
    style["textColor"] = SerializeVector3(e.style.textColor);
    style["borderColor"] = SerializeVector3(e.style.borderColor);
    style["bgAlpha"] = RF(e.style.bgAlpha);
    style["borderRadius"] = RF(e.style.borderRadius);
    style["borderWidth"] = RF(e.style.borderWidth);
    style["fontSize"] = RF(e.style.fontSize);
    style["focusColor"] = SerializeVector3(e.style.focusColor);
    if (e.style.nineSlice.IsActive()) {
        json ns;
        ns["texturePath"] = e.style.nineSlice.texturePath;
        ns["borderLeft"] = RF(e.style.nineSlice.borderLeft);
        ns["borderRight"] = RF(e.style.nineSlice.borderRight);
        ns["borderTop"] = RF(e.style.nineSlice.borderTop);
        ns["borderBottom"] = RF(e.style.nineSlice.borderBottom);
        style["nineSlice"] = ns;
    }
    j["style"] = style;

    // Widget data
    json data;
    data["text"] = e.data.text;
    data["textAlignH"] = RF(e.data.textAlignH);
    data["textAlignV"] = RF(e.data.textAlignV);
    data["imagePath"] = e.data.imagePath;
    data["imageTint"] = SerializeVector3(e.data.imageTint);
    data["imageAlpha"] = RF(e.data.imageAlpha);
    data["progressValue"] = RF(e.data.progressValue);
    if (!e.data.bindField.empty()) data["bindField"] = e.data.bindField;
    if (e.data.bindMaxValue != 0.0f) data["bindMaxValue"] = RF(e.data.bindMaxValue);
    data["progressFillColor"] = SerializeVector3(e.data.progressFillColor);
    data["sliderValue"] = RF(e.data.sliderValue);
    data["sliderMin"] = RF(e.data.sliderMin);
    data["sliderMax"] = RF(e.data.sliderMax);
    data["checked"] = RF(e.data.checked);
    if (!e.data.options.empty()) data["options"] = e.data.options;
    data["selectedOption"] = RF(e.data.selectedOption);
    data["inputText"] = e.data.inputText;
    data["placeholder"] = e.data.placeholder;
    data["gridColumns"] = RF(e.data.gridColumns);
    data["activeTabIndex"] = RF(e.data.activeTabIndex);
    data["tooltipDelay"] = RF(e.data.tooltipDelay);
    if (!e.data.tooltipText.empty()) data["tooltipText"] = e.data.tooltipText;
    data["listSelectedIndex"] = RF(e.data.listSelectedIndex);
    // World-space anchoring (migrated HUD billboards). worldSourceEntity is
    // runtime-only (entity ids don't survive save/load; scripts re-set it).
    if (e.data.worldSpace) {
        data["worldSpace"] = true;
        data["worldOffset"] = SerializeVector3(e.data.worldOffset);
        data["maxRenderDistance"] = RF(e.data.maxRenderDistance);
    }
    j["data"] = data;

    // Accessibility
    if (!e.accessibleLabel.empty()) j["accessibleLabel"] = e.accessibleLabel;

    // Events
    j["onClickEvent"] = e.onClickEvent;
    j["onValueChangedEvent"] = e.onValueChangedEvent;
    j["onSubmitEvent"] = e.onSubmitEvent;

    return j;
}

GUI::UIElement DeserializeUIElement(const json& j) {
    GUI::UIElement e;
    if (j.contains("id")) e.id = j["id"].get<u32>();
    if (j.contains("name")) e.name = SafeStr(j["name"], MAX_STR_NAME);
    if (j.contains("type")) { u8 v = j["type"].get<u8>(); if (v < static_cast<u8>(GUI::UIWidgetType::Count)) e.type = static_cast<GUI::UIWidgetType>(v); }
    if (j.contains("visible")) e.visible = JB(j["visible"]);
    if (j.contains("enabled")) e.enabled = JB(j["enabled"]);
    if (j.contains("focusable")) e.focusable = JB(j["focusable"]);
    if (j.contains("tabOrder")) e.tabOrder = j["tabOrder"].get<i32>();
    if (j.contains("layoutMode")) {
        u8 v = j["layoutMode"].get<u8>();
        if (v <= static_cast<u8>(GUI::UILayoutMode::Grid)) e.layoutMode = static_cast<GUI::UILayoutMode>(v);
    }
    e.layoutSpacing = j.value("layoutSpacing", -1.0f);
    e.layoutPaddingX = j.value("layoutPaddingX", 0.0f);
    e.layoutPaddingY = j.value("layoutPaddingY", 0.0f);
    if (j.contains("layoutAlign")) {
        u8 v = j["layoutAlign"].get<u8>();
        if (v <= static_cast<u8>(GUI::UILayoutAlign::Stretch)) e.layoutAlign = static_cast<GUI::UILayoutAlign>(v);
    }
    if (j.contains("parentId")) e.parentId = j["parentId"].get<u32>();
    if (j.contains("childIds") && j["childIds"].is_array()) {
        static constexpr usize MAX_CHILD_IDS = 10000;
        usize count = std::min(j["childIds"].size(), MAX_CHILD_IDS);
        for (usize ci = 0; ci < count; ci++) e.childIds.push_back(j["childIds"][ci].get<u32>());
    }

    if (j.contains("anchor")) {
        const auto& a = j["anchor"];
        if (a.contains("anchorMin")) e.anchor.anchorMin = DeserializeVector2(a["anchorMin"]);
        if (a.contains("anchorMax")) e.anchor.anchorMax = DeserializeVector2(a["anchorMax"]);
        if (a.contains("pivot")) e.anchor.pivot = DeserializeVector2(a["pivot"]);
        if (a.contains("offsetLeft")) e.anchor.offsetLeft = a["offsetLeft"].get<f32>();
        if (a.contains("offsetRight")) e.anchor.offsetRight = a["offsetRight"].get<f32>();
        if (a.contains("offsetTop")) e.anchor.offsetTop = a["offsetTop"].get<f32>();
        if (a.contains("offsetBottom")) e.anchor.offsetBottom = a["offsetBottom"].get<f32>();
    }

    if (j.contains("style")) {
        const auto& s = j["style"];
        if (s.contains("bgColor")) e.style.bgColor = DeserializeVector3(s["bgColor"]);
        if (s.contains("textColor")) e.style.textColor = DeserializeVector3(s["textColor"]);
        if (s.contains("borderColor")) e.style.borderColor = DeserializeVector3(s["borderColor"]);
        if (s.contains("bgAlpha")) e.style.bgAlpha = s["bgAlpha"].get<f32>();
        if (s.contains("borderRadius")) e.style.borderRadius = s["borderRadius"].get<f32>();
        if (s.contains("borderWidth")) e.style.borderWidth = s["borderWidth"].get<f32>();
        if (s.contains("fontSize")) e.style.fontSize = s["fontSize"].get<f32>();
        if (s.contains("focusColor")) e.style.focusColor = DeserializeVector3(s["focusColor"]);
        if (s.contains("nineSlice")) {
            const auto& ns = s["nineSlice"];
            if (ns.contains("texturePath")) e.style.nineSlice.texturePath = SafeStr(ns["texturePath"], MAX_STR_PATH);
            if (ns.contains("borderLeft")) e.style.nineSlice.borderLeft = ns["borderLeft"].get<f32>();
            if (ns.contains("borderRight")) e.style.nineSlice.borderRight = ns["borderRight"].get<f32>();
            if (ns.contains("borderTop")) e.style.nineSlice.borderTop = ns["borderTop"].get<f32>();
            if (ns.contains("borderBottom")) e.style.nineSlice.borderBottom = ns["borderBottom"].get<f32>();
        }
    }

    if (j.contains("data")) {
        const auto& d = j["data"];
        if (d.contains("text")) e.data.text = SafeStr(d["text"]);
        if (d.contains("textAlignH")) e.data.textAlignH = d["textAlignH"].get<u8>();
        if (d.contains("textAlignV")) e.data.textAlignV = d["textAlignV"].get<u8>();
        if (d.contains("imagePath")) e.data.imagePath = SafeStr(d["imagePath"], MAX_STR_PATH);
        if (d.contains("imageTint")) e.data.imageTint = DeserializeVector3(d["imageTint"]);
        if (d.contains("imageAlpha")) e.data.imageAlpha = d["imageAlpha"].get<f32>();
        if (d.contains("progressValue")) e.data.progressValue = d["progressValue"].get<f32>();
        if (d.contains("bindField")) e.data.bindField = SafeStr(d["bindField"]);
        if (d.contains("bindMaxValue")) e.data.bindMaxValue = d["bindMaxValue"].get<f32>();
        if (d.contains("progressFillColor")) e.data.progressFillColor = DeserializeVector3(d["progressFillColor"]);
        if (d.contains("sliderValue")) e.data.sliderValue = d["sliderValue"].get<f32>();
        if (d.contains("sliderMin")) e.data.sliderMin = d["sliderMin"].get<f32>();
        if (d.contains("sliderMax")) e.data.sliderMax = d["sliderMax"].get<f32>();
        if (d.contains("checked")) e.data.checked = JB(d["checked"]);
        if (d.contains("options") && d["options"].is_array()) {
            static constexpr usize MAX_OPTIONS = 1000;
            usize optCount = std::min(d["options"].size(), MAX_OPTIONS);
            for (usize oi = 0; oi < optCount; oi++) e.data.options.push_back(SafeStr(d["options"][oi]));
        }
        if (d.contains("selectedOption")) e.data.selectedOption = d["selectedOption"].get<i32>();
        if (d.contains("inputText")) e.data.inputText = SafeStr(d["inputText"]);
        if (d.contains("placeholder")) e.data.placeholder = SafeStr(d["placeholder"]);
        if (d.contains("gridColumns")) e.data.gridColumns = d["gridColumns"].get<i32>();
        if (d.contains("activeTabIndex")) e.data.activeTabIndex = d["activeTabIndex"].get<i32>();
        if (d.contains("tooltipDelay")) e.data.tooltipDelay = d["tooltipDelay"].get<f32>();
        if (d.contains("tooltipText")) e.data.tooltipText = SafeStr(d["tooltipText"]);
        if (d.contains("listSelectedIndex")) e.data.listSelectedIndex = d["listSelectedIndex"].get<i32>();
        if (d.contains("worldSpace")) e.data.worldSpace = JB(d["worldSpace"]);
        if (d.contains("worldOffset")) e.data.worldOffset = DeserializeVector3(d["worldOffset"]);
        if (d.contains("maxRenderDistance")) e.data.maxRenderDistance = d["maxRenderDistance"].get<f32>();
    }

    if (j.contains("accessibleLabel")) e.accessibleLabel = SafeStr(j["accessibleLabel"], MAX_STR_NAME);

    if (j.contains("onClickEvent")) e.onClickEvent = SafeStr(j["onClickEvent"], MAX_STR_NAME);
    if (j.contains("onValueChangedEvent")) e.onValueChangedEvent = SafeStr(j["onValueChangedEvent"], MAX_STR_NAME);
    if (j.contains("onSubmitEvent")) e.onSubmitEvent = SafeStr(j["onSubmitEvent"], MAX_STR_NAME);

    return e;
}

json SerializeUITheme(const GUI::UITheme& t) {
    json j;
    j["name"] = t.name;
    j["primary"] = SerializeVector3(t.primary);
    j["secondary"] = SerializeVector3(t.secondary);
    j["background"] = SerializeVector3(t.background);
    j["surface"] = SerializeVector3(t.surface);
    j["error"] = SerializeVector3(t.error);
    j["textPrimary"] = SerializeVector3(t.textPrimary);
    j["textSecondary"] = SerializeVector3(t.textSecondary);
    j["textDisabled"] = SerializeVector3(t.textDisabled);
    j["buttonDefault"] = SerializeVector3(t.buttonDefault);
    j["buttonHovered"] = SerializeVector3(t.buttonHovered);
    j["buttonPressed"] = SerializeVector3(t.buttonPressed);
    j["buttonDisabled"] = SerializeVector3(t.buttonDisabled);
    j["inputBg"] = SerializeVector3(t.inputBg);
    j["inputBorder"] = SerializeVector3(t.inputBorder);
    j["inputFocused"] = SerializeVector3(t.inputFocused);
    j["sliderTrack"] = SerializeVector3(t.sliderTrack);
    j["sliderFill"] = SerializeVector3(t.sliderFill);
    j["sliderThumb"] = SerializeVector3(t.sliderThumb);
    j["checkboxBg"] = SerializeVector3(t.checkboxBg);
    j["checkboxChecked"] = SerializeVector3(t.checkboxChecked);
    j["toggleOffBg"] = SerializeVector3(t.toggleOffBg);
    j["toggleOnBg"] = SerializeVector3(t.toggleOnBg);
    j["toggleKnob"] = SerializeVector3(t.toggleKnob);
    j["borderRadius"] = RF(t.borderRadius);
    j["borderWidth"] = RF(t.borderWidth);
    j["fontSizeBody"] = RF(t.fontSizeBody);
    j["fontSizeHeading"] = RF(t.fontSizeHeading);
    j["fontSizeSmall"] = RF(t.fontSizeSmall);
    j["spacing"] = RF(t.spacing);
    j["bgAlpha"] = RF(t.bgAlpha);
    j["focusBorderWidth"] = RF(t.focusBorderWidth);
    if (t.panelNineSlice.IsActive()) {
        json ns;
        ns["texturePath"] = t.panelNineSlice.texturePath;
        ns["borderLeft"] = RF(t.panelNineSlice.borderLeft);
        ns["borderRight"] = RF(t.panelNineSlice.borderRight);
        ns["borderTop"] = RF(t.panelNineSlice.borderTop);
        ns["borderBottom"] = RF(t.panelNineSlice.borderBottom);
        j["panelNineSlice"] = ns;
    }
    if (t.buttonNineSlice.IsActive()) {
        json ns;
        ns["texturePath"] = t.buttonNineSlice.texturePath;
        ns["borderLeft"] = RF(t.buttonNineSlice.borderLeft);
        ns["borderRight"] = RF(t.buttonNineSlice.borderRight);
        ns["borderTop"] = RF(t.buttonNineSlice.borderTop);
        ns["borderBottom"] = RF(t.buttonNineSlice.borderBottom);
        j["buttonNineSlice"] = ns;
    }
    return j;
}

GUI::UITheme DeserializeUITheme(const json& j) {
    GUI::UITheme t;
    if (j.contains("name")) t.name = j["name"].get<std::string>();
    if (j.contains("primary")) t.primary = DeserializeVector3(j["primary"]);
    if (j.contains("secondary")) t.secondary = DeserializeVector3(j["secondary"]);
    if (j.contains("background")) t.background = DeserializeVector3(j["background"]);
    if (j.contains("surface")) t.surface = DeserializeVector3(j["surface"]);
    if (j.contains("error")) t.error = DeserializeVector3(j["error"]);
    if (j.contains("textPrimary")) t.textPrimary = DeserializeVector3(j["textPrimary"]);
    if (j.contains("textSecondary")) t.textSecondary = DeserializeVector3(j["textSecondary"]);
    if (j.contains("textDisabled")) t.textDisabled = DeserializeVector3(j["textDisabled"]);
    if (j.contains("buttonDefault")) t.buttonDefault = DeserializeVector3(j["buttonDefault"]);
    if (j.contains("buttonHovered")) t.buttonHovered = DeserializeVector3(j["buttonHovered"]);
    if (j.contains("buttonPressed")) t.buttonPressed = DeserializeVector3(j["buttonPressed"]);
    if (j.contains("buttonDisabled")) t.buttonDisabled = DeserializeVector3(j["buttonDisabled"]);
    if (j.contains("inputBg")) t.inputBg = DeserializeVector3(j["inputBg"]);
    if (j.contains("inputBorder")) t.inputBorder = DeserializeVector3(j["inputBorder"]);
    if (j.contains("inputFocused")) t.inputFocused = DeserializeVector3(j["inputFocused"]);
    if (j.contains("sliderTrack")) t.sliderTrack = DeserializeVector3(j["sliderTrack"]);
    if (j.contains("sliderFill")) t.sliderFill = DeserializeVector3(j["sliderFill"]);
    if (j.contains("sliderThumb")) t.sliderThumb = DeserializeVector3(j["sliderThumb"]);
    if (j.contains("checkboxBg")) t.checkboxBg = DeserializeVector3(j["checkboxBg"]);
    if (j.contains("checkboxChecked")) t.checkboxChecked = DeserializeVector3(j["checkboxChecked"]);
    if (j.contains("toggleOffBg")) t.toggleOffBg = DeserializeVector3(j["toggleOffBg"]);
    if (j.contains("toggleOnBg")) t.toggleOnBg = DeserializeVector3(j["toggleOnBg"]);
    if (j.contains("toggleKnob")) t.toggleKnob = DeserializeVector3(j["toggleKnob"]);
    if (j.contains("borderRadius")) t.borderRadius = j["borderRadius"].get<f32>();
    if (j.contains("borderWidth")) t.borderWidth = j["borderWidth"].get<f32>();
    if (j.contains("fontSizeBody")) t.fontSizeBody = j["fontSizeBody"].get<f32>();
    if (j.contains("fontSizeHeading")) t.fontSizeHeading = j["fontSizeHeading"].get<f32>();
    if (j.contains("fontSizeSmall")) t.fontSizeSmall = j["fontSizeSmall"].get<f32>();
    if (j.contains("spacing")) t.spacing = j["spacing"].get<f32>();
    if (j.contains("bgAlpha")) t.bgAlpha = j["bgAlpha"].get<f32>();
    if (j.contains("focusBorderWidth")) t.focusBorderWidth = j["focusBorderWidth"].get<f32>();
    auto deserializeNS = [](const json& ns, GUI::NineSliceConfig& cfg) {
        if (ns.contains("texturePath")) cfg.texturePath = SafeStr(ns["texturePath"], MAX_STR_PATH);
        if (ns.contains("borderLeft")) cfg.borderLeft = ns["borderLeft"].get<f32>();
        if (ns.contains("borderRight")) cfg.borderRight = ns["borderRight"].get<f32>();
        if (ns.contains("borderTop")) cfg.borderTop = ns["borderTop"].get<f32>();
        if (ns.contains("borderBottom")) cfg.borderBottom = ns["borderBottom"].get<f32>();
    };
    if (j.contains("panelNineSlice")) deserializeNS(j["panelNineSlice"], t.panelNineSlice);
    if (j.contains("buttonNineSlice")) deserializeNS(j["buttonNineSlice"], t.buttonNineSlice);
    return t;
}

json SerializeUICanvasComponent(const GUI::UICanvasComponent& c) {
    json j;
    j["canvasName"] = c.canvasName;
    j["visible"] = RF(c.visible);
    j["sortOrder"] = RF(c.sortOrder);
    j["designWidth"] = RF(c.designWidth);
    j["designHeight"] = RF(c.designHeight);
    j["scaleMode"] = static_cast<u8>(c.scaleMode);
    // NOT RF-wrapped: RF takes f32 and the theme is an OBJECT — the Feb-09
    // deterministic-serialization sweep wrapped it by accident, making any
    // scene save with a UICanvas throw type_error.302 (dormant until the
    // HUD->canvas migration put canvases in ordinary scenes).
    j["theme"] = SerializeUITheme(c.theme);
    j["nextElementId"] = c.nextElementId;

    json elementsArr = json::array();
    for (const auto& elem : c.elements) {
        elementsArr.push_back(SerializeUIElement(elem));
    }
    j["elements"] = elementsArr;
    return j;
}

GUI::UICanvasComponent DeserializeUICanvasComponent(const json& j) {
    GUI::UICanvasComponent c;
    if (j.contains("canvasName")) c.canvasName = SafeStr(j["canvasName"], MAX_STR_NAME);
    if (j.contains("visible")) c.visible = JB(j["visible"]);
    if (j.contains("sortOrder")) c.sortOrder = j["sortOrder"].get<i32>();
    if (j.contains("designWidth")) c.designWidth = j["designWidth"].get<f32>();
    if (j.contains("designHeight")) c.designHeight = j["designHeight"].get<f32>();
    if (j.contains("scaleMode")) { u8 v = j["scaleMode"].get<u8>(); if (v <= 2) c.scaleMode = static_cast<GUI::UIScaleMode>(v); }
    if (j.contains("theme")) c.theme = DeserializeUITheme(j["theme"]);
    if (j.contains("nextElementId")) c.nextElementId = j["nextElementId"].get<u32>();

    if (j.contains("elements") && j["elements"].is_array()) {
        static constexpr usize MAX_UI_ELEMENTS = 10000;
        usize count = std::min(j["elements"].size(), MAX_UI_ELEMENTS);
        for (usize ei = 0; ei < count; ei++) {
            c.elements.push_back(DeserializeUIElement(j["elements"][ei]));
        }
    }
    return c;
}

json SerializeCinematicCameraComponent(const ECS::CinematicCameraComponent& c) {
    json j;
    j["loop"] = c.loop;
    j["autoPlay"] = RF(c.autoPlay);
    j["hideHUD"] = RF(c.hideHUD);
    j["disableInput"] = RF(c.disableInput);
    j["onCompleteNotify"] = static_cast<u64>(c.onCompleteNotify);
    j["onWaypointReachNotify"] = static_cast<u64>(c.onWaypointReachNotify);
    json wps = json::array();
    for (const auto& wp : c.waypoints) {
        json wj;
        wj["position"] = SerializeVector3(wp.position);
        wj["lookAt"] = SerializeVector3(wp.lookAt);
        wj["fov"] = RF(wp.fov);
        wj["duration"] = RF(wp.duration);
        wj["holdTime"] = RF(wp.holdTime);
        wj["easing"] = static_cast<u8>(wp.easing);
        wps.push_back(wj);
    }
    j["waypoints"] = wps;
    return j;
}

ECS::CinematicCameraComponent DeserializeCinematicCameraComponent(const json& j) {
    ECS::CinematicCameraComponent c;
    if (j.contains("loop")) c.loop = JB(j["loop"]);
    if (j.contains("autoPlay")) c.autoPlay = JB(j["autoPlay"]);
    if (j.contains("hideHUD")) c.hideHUD = JB(j["hideHUD"]);
    if (j.contains("disableInput")) c.disableInput = JB(j["disableInput"]);
    if (j.contains("onCompleteNotify")) c.onCompleteNotify = static_cast<ECS::Entity>(j["onCompleteNotify"].get<u64>());
    if (j.contains("onWaypointReachNotify")) c.onWaypointReachNotify = static_cast<ECS::Entity>(j["onWaypointReachNotify"].get<u64>());
    if (j.contains("waypoints") && j["waypoints"].is_array()) {
        for (const auto& wj : j["waypoints"]) {
            ECS::CinematicCameraComponent::Waypoint wp;
            if (wj.contains("position")) wp.position = DeserializeVector3(wj["position"]);
            if (wj.contains("lookAt")) wp.lookAt = DeserializeVector3(wj["lookAt"]);
            if (wj.contains("fov")) wp.fov = wj["fov"].get<f32>();
            if (wj.contains("duration")) wp.duration = wj["duration"].get<f32>();
            if (wj.contains("holdTime")) wp.holdTime = wj["holdTime"].get<f32>();
            if (wj.contains("easing")) { u8 v = wj["easing"].get<u8>(); if (v <= 4) wp.easing = static_cast<ECS::CinematicCameraComponent::Waypoint::Easing>(v); }
            c.waypoints.push_back(wp);
        }
    }
    return c;
}

// ============================================================================
// Joint & Ragdoll Components
// ============================================================================

json SerializeDistanceJointComponent(const ECS::DistanceJointComponent& j) {
    json o;
    o["entityA"] = static_cast<u64>(j.entityA);
    o["entityB"] = static_cast<u64>(j.entityB);
    o["anchorA"] = SerializeVector3(j.anchorA);
    o["anchorB"] = SerializeVector3(j.anchorB);
    o["restDistance"] = RF(j.restDistance);
    o["tolerance"] = RF(j.tolerance);
    o["stiffness"] = RF(j.stiffness);
    o["breakable"] = RF(j.breakable);
    o["breakForce"] = RF(j.breakForce);
    return o;
}

ECS::DistanceJointComponent DeserializeDistanceJointComponent(const json& j) {
    ECS::DistanceJointComponent c;
    if (j.contains("entityA")) c.entityA = static_cast<ECS::Entity>(j["entityA"].get<u64>());
    if (j.contains("entityB")) c.entityB = static_cast<ECS::Entity>(j["entityB"].get<u64>());
    if (j.contains("anchorA")) c.anchorA = DeserializeVector3(j["anchorA"]);
    if (j.contains("anchorB")) c.anchorB = DeserializeVector3(j["anchorB"]);
    if (j.contains("restDistance")) c.restDistance = j["restDistance"].get<f32>();
    if (j.contains("tolerance")) c.tolerance = j["tolerance"].get<f32>();
    if (j.contains("stiffness")) c.stiffness = j["stiffness"].get<f32>();
    if (j.contains("breakable")) c.breakable = JB(j["breakable"]);
    if (j.contains("breakForce")) c.breakForce = j["breakForce"].get<f32>();
    return c;
}

json SerializeHingeJointComponent(const ECS::HingeJointComponent& j) {
    json o;
    o["entityA"] = static_cast<u64>(j.entityA);
    o["entityB"] = static_cast<u64>(j.entityB);
    o["anchorA"] = SerializeVector3(j.anchorA);
    o["anchorB"] = SerializeVector3(j.anchorB);
    o["axis"] = SerializeVector3(j.axis);
    o["useLimits"] = RF(j.useLimits);
    o["lowerLimit"] = RF(j.lowerLimit);
    o["upperLimit"] = RF(j.upperLimit);
    o["useMotor"] = RF(j.useMotor);
    o["motorSpeed"] = RF(j.motorSpeed);
    o["motorMaxForce"] = RF(j.motorMaxForce);
    o["breakable"] = RF(j.breakable);
    o["breakForce"] = RF(j.breakForce);
    return o;
}

ECS::HingeJointComponent DeserializeHingeJointComponent(const json& j) {
    ECS::HingeJointComponent c;
    if (j.contains("entityA")) c.entityA = static_cast<ECS::Entity>(j["entityA"].get<u64>());
    if (j.contains("entityB")) c.entityB = static_cast<ECS::Entity>(j["entityB"].get<u64>());
    if (j.contains("anchorA")) c.anchorA = DeserializeVector3(j["anchorA"]);
    if (j.contains("anchorB")) c.anchorB = DeserializeVector3(j["anchorB"]);
    if (j.contains("axis")) c.axis = DeserializeVector3(j["axis"]);
    if (j.contains("useLimits")) c.useLimits = JB(j["useLimits"]);
    if (j.contains("lowerLimit")) c.lowerLimit = j["lowerLimit"].get<f32>();
    if (j.contains("upperLimit")) c.upperLimit = j["upperLimit"].get<f32>();
    if (j.contains("useMotor")) c.useMotor = JB(j["useMotor"]);
    if (j.contains("motorSpeed")) c.motorSpeed = j["motorSpeed"].get<f32>();
    if (j.contains("motorMaxForce")) c.motorMaxForce = j["motorMaxForce"].get<f32>();
    if (j.contains("breakable")) c.breakable = JB(j["breakable"]);
    if (j.contains("breakForce")) c.breakForce = j["breakForce"].get<f32>();
    return c;
}

json SerializeBallSocketJointComponent(const ECS::BallSocketJointComponent& j) {
    json o;
    o["entityA"] = static_cast<u64>(j.entityA);
    o["entityB"] = static_cast<u64>(j.entityB);
    o["anchorA"] = SerializeVector3(j.anchorA);
    o["anchorB"] = SerializeVector3(j.anchorB);
    o["useConeLimit"] = RF(j.useConeLimit);
    o["coneAngleLimit"] = RF(j.coneAngleLimit);
    o["useTwistLimit"] = RF(j.useTwistLimit);
    o["twistLowerLimit"] = RF(j.twistLowerLimit);
    o["twistUpperLimit"] = RF(j.twistUpperLimit);
    o["breakable"] = RF(j.breakable);
    o["breakForce"] = RF(j.breakForce);
    return o;
}

ECS::BallSocketJointComponent DeserializeBallSocketJointComponent(const json& j) {
    ECS::BallSocketJointComponent c;
    if (j.contains("entityA")) c.entityA = static_cast<ECS::Entity>(j["entityA"].get<u64>());
    if (j.contains("entityB")) c.entityB = static_cast<ECS::Entity>(j["entityB"].get<u64>());
    if (j.contains("anchorA")) c.anchorA = DeserializeVector3(j["anchorA"]);
    if (j.contains("anchorB")) c.anchorB = DeserializeVector3(j["anchorB"]);
    if (j.contains("useConeLimit")) c.useConeLimit = JB(j["useConeLimit"]);
    if (j.contains("coneAngleLimit")) c.coneAngleLimit = j["coneAngleLimit"].get<f32>();
    if (j.contains("useTwistLimit")) c.useTwistLimit = JB(j["useTwistLimit"]);
    if (j.contains("twistLowerLimit")) c.twistLowerLimit = j["twistLowerLimit"].get<f32>();
    if (j.contains("twistUpperLimit")) c.twistUpperLimit = j["twistUpperLimit"].get<f32>();
    if (j.contains("breakable")) c.breakable = JB(j["breakable"]);
    if (j.contains("breakForce")) c.breakForce = j["breakForce"].get<f32>();
    return c;
}

json SerializeSpringJointComponent(const ECS::SpringJointComponent& j) {
    json o;
    o["entityA"] = static_cast<u64>(j.entityA);
    o["entityB"] = static_cast<u64>(j.entityB);
    o["anchorA"] = SerializeVector3(j.anchorA);
    o["anchorB"] = SerializeVector3(j.anchorB);
    o["restLength"] = RF(j.restLength);
    o["springConstant"] = RF(j.springConstant);
    o["dampingCoefficient"] = RF(j.dampingCoefficient);
    o["minDistance"] = RF(j.minDistance);
    o["maxDistance"] = RF(j.maxDistance);
    o["breakable"] = RF(j.breakable);
    o["breakForce"] = RF(j.breakForce);
    return o;
}

ECS::SpringJointComponent DeserializeSpringJointComponent(const json& j) {
    ECS::SpringJointComponent c;
    if (j.contains("entityA")) c.entityA = static_cast<ECS::Entity>(j["entityA"].get<u64>());
    if (j.contains("entityB")) c.entityB = static_cast<ECS::Entity>(j["entityB"].get<u64>());
    if (j.contains("anchorA")) c.anchorA = DeserializeVector3(j["anchorA"]);
    if (j.contains("anchorB")) c.anchorB = DeserializeVector3(j["anchorB"]);
    if (j.contains("restLength")) c.restLength = j["restLength"].get<f32>();
    if (j.contains("springConstant")) c.springConstant = j["springConstant"].get<f32>();
    if (j.contains("dampingCoefficient")) c.dampingCoefficient = j["dampingCoefficient"].get<f32>();
    if (j.contains("minDistance")) c.minDistance = j["minDistance"].get<f32>();
    if (j.contains("maxDistance")) c.maxDistance = j["maxDistance"].get<f32>();
    if (j.contains("breakable")) c.breakable = JB(j["breakable"]);
    if (j.contains("breakForce")) c.breakForce = j["breakForce"].get<f32>();
    return c;
}

json SerializeFixedJointComponent(const ECS::FixedJointComponent& j) {
    json o;
    o["entityA"] = static_cast<u64>(j.entityA);
    o["entityB"] = static_cast<u64>(j.entityB);
    o["anchorA"] = SerializeVector3(j.anchorA);
    o["anchorB"] = SerializeVector3(j.anchorB);
    o["relativePosition"] = SerializeVector3(j.relativePosition);
    o["relativeRotation"] = SerializeVector3(j.relativeRotation);
    o["initialized"] = RF(j.initialized);
    o["breakable"] = RF(j.breakable);
    o["breakForce"] = RF(j.breakForce);
    return o;
}

ECS::FixedJointComponent DeserializeFixedJointComponent(const json& j) {
    ECS::FixedJointComponent c;
    if (j.contains("entityA")) c.entityA = static_cast<ECS::Entity>(j["entityA"].get<u64>());
    if (j.contains("entityB")) c.entityB = static_cast<ECS::Entity>(j["entityB"].get<u64>());
    if (j.contains("anchorA")) c.anchorA = DeserializeVector3(j["anchorA"]);
    if (j.contains("anchorB")) c.anchorB = DeserializeVector3(j["anchorB"]);
    if (j.contains("relativePosition")) c.relativePosition = DeserializeVector3(j["relativePosition"]);
    if (j.contains("relativeRotation")) c.relativeRotation = DeserializeVector3(j["relativeRotation"]);
    if (j.contains("initialized")) c.initialized = JB(j["initialized"]);
    if (j.contains("breakable")) c.breakable = JB(j["breakable"]);
    if (j.contains("breakForce")) c.breakForce = j["breakForce"].get<f32>();
    return c;
}

json SerializeSliderJointComponent(const ECS::SliderJointComponent& j) {
    json o;
    o["entityA"] = static_cast<u64>(j.entityA);
    o["entityB"] = static_cast<u64>(j.entityB);
    o["anchorA"] = SerializeVector3(j.anchorA);
    o["anchorB"] = SerializeVector3(j.anchorB);
    o["slideAxis"] = SerializeVector3(j.slideAxis);
    o["useLimits"] = RF(j.useLimits);
    o["lowerLimit"] = RF(j.lowerLimit);
    o["upperLimit"] = RF(j.upperLimit);
    o["useMotor"] = RF(j.useMotor);
    o["motorSpeed"] = RF(j.motorSpeed);
    o["motorMaxForce"] = RF(j.motorMaxForce);
    o["breakable"] = RF(j.breakable);
    o["breakForce"] = RF(j.breakForce);
    return o;
}

ECS::SliderJointComponent DeserializeSliderJointComponent(const json& j) {
    ECS::SliderJointComponent c;
    if (j.contains("entityA")) c.entityA = static_cast<ECS::Entity>(j["entityA"].get<u64>());
    if (j.contains("entityB")) c.entityB = static_cast<ECS::Entity>(j["entityB"].get<u64>());
    if (j.contains("anchorA")) c.anchorA = DeserializeVector3(j["anchorA"]);
    if (j.contains("anchorB")) c.anchorB = DeserializeVector3(j["anchorB"]);
    if (j.contains("slideAxis")) c.slideAxis = DeserializeVector3(j["slideAxis"]);
    if (j.contains("useLimits")) c.useLimits = JB(j["useLimits"]);
    if (j.contains("lowerLimit")) c.lowerLimit = j["lowerLimit"].get<f32>();
    if (j.contains("upperLimit")) c.upperLimit = j["upperLimit"].get<f32>();
    if (j.contains("useMotor")) c.useMotor = JB(j["useMotor"]);
    if (j.contains("motorSpeed")) c.motorSpeed = j["motorSpeed"].get<f32>();
    if (j.contains("motorMaxForce")) c.motorMaxForce = j["motorMaxForce"].get<f32>();
    if (j.contains("breakable")) c.breakable = JB(j["breakable"]);
    if (j.contains("breakForce")) c.breakForce = j["breakForce"].get<f32>();
    return c;
}

json SerializeRagdollComponent(const ECS::RagdollComponent& r) {
    json o;
    o["enabled"] = r.enabled;
    o["autoDisableAfterSettle"] = RF(r.autoDisableAfterSettle);
    o["settleThreshold"] = RF(r.settleThreshold);
    o["settleTime"] = RF(r.settleTime);
    o["blendWeight"] = RF(r.blendWeight);
    o["blendSpeed"] = RF(r.blendSpeed);
    o["blendTime"] = RF(r.blendTime);
    o["autoActivateOnDeath"] = r.autoActivateOnDeath;
    o["gravityScale"] = RF(r.gravityScale);
    o["linearDamping"] = RF(r.linearDamping);
    o["angularDamping"] = RF(r.angularDamping);
    json joints = json::array();
    for (const auto& bj : r.boneJoints) {
        json bjJson;
        bjJson["boneName"] = bj.boneName;
        bjJson["boneIndex"] = RF(bj.boneIndex);
        bjJson["jointType"] = static_cast<u8>(bj.jointType);
        bjJson["jointEntity"] = static_cast<u64>(bj.jointEntity);
        bjJson["mass"] = RF(bj.mass);
        bjJson["colliderRadius"] = RF(bj.colliderRadius);
        bjJson["colliderSize"] = SerializeVector3(bj.colliderSize);
        bjJson["coneAngleLimit"] = RF(bj.coneAngleLimit);
        bjJson["twistLimit"] = RF(bj.twistLimit);
        joints.push_back(bjJson);
    }
    o["boneJoints"] = joints;
    return o;
}

ECS::RagdollComponent DeserializeRagdollComponent(const json& j) {
    ECS::RagdollComponent r;
    if (j.contains("enabled")) r.enabled = JB(j["enabled"]);
    if (j.contains("autoDisableAfterSettle")) r.autoDisableAfterSettle = JB(j["autoDisableAfterSettle"]);
    if (j.contains("settleThreshold")) r.settleThreshold = j["settleThreshold"].get<f32>();
    if (j.contains("settleTime")) r.settleTime = j["settleTime"].get<f32>();
    if (j.contains("blendWeight")) r.blendWeight = j["blendWeight"].get<f32>();
    if (j.contains("blendSpeed")) r.blendSpeed = j["blendSpeed"].get<f32>();
    if (j.contains("blendTime")) r.blendTime = j["blendTime"].get<f32>();
    if (j.contains("autoActivateOnDeath")) r.autoActivateOnDeath = JB(j["autoActivateOnDeath"]);
    if (j.contains("gravityScale")) r.gravityScale = j["gravityScale"].get<f32>();
    if (j.contains("linearDamping")) r.linearDamping = j["linearDamping"].get<f32>();
    if (j.contains("angularDamping")) r.angularDamping = j["angularDamping"].get<f32>();
    if (j.contains("boneJoints") && j["boneJoints"].is_array()) {
        for (const auto& bjJson : j["boneJoints"]) {
            ECS::RagdollComponent::BoneJoint bj;
            if (bjJson.contains("boneName")) bj.boneName = bjJson["boneName"].get<std::string>();
            if (bjJson.contains("boneIndex")) bj.boneIndex = bjJson["boneIndex"].get<i32>();
            if (bjJson.contains("jointType")) { u8 v = bjJson["jointType"].get<u8>(); if (v <= 5) bj.jointType = static_cast<ECS::JointType>(v); }
            if (bjJson.contains("jointEntity")) bj.jointEntity = static_cast<ECS::Entity>(bjJson["jointEntity"].get<u64>());
            if (bjJson.contains("mass")) bj.mass = bjJson["mass"].get<f32>();
            if (bjJson.contains("colliderRadius")) bj.colliderRadius = bjJson["colliderRadius"].get<f32>();
            if (bjJson.contains("colliderSize") && bjJson["colliderSize"].is_array()) bj.colliderSize = DeserializeVector3(bjJson["colliderSize"]);
            if (bjJson.contains("coneAngleLimit")) bj.coneAngleLimit = bjJson["coneAngleLimit"].get<f32>();
            if (bjJson.contains("twistLimit")) bj.twistLimit = bjJson["twistLimit"].get<f32>();
            r.boneJoints.push_back(bj);
        }
    }
    return r;
}

// ============================================================================
// Animation Recorder
// ============================================================================

json SerializeAnimationRecorderComponent(const ECS::AnimationRecorderComponent& rec) {
    json o;
    o["recordedAnimName"] = rec.recordedAnimName;
    o["recordInterval"] = RF(rec.recordInterval);
    o["recordCount"] = rec.recordCount;
    // recording state and tracks are transient -- not serialized
    return o;
}

ECS::AnimationRecorderComponent DeserializeAnimationRecorderComponent(const json& j) {
    ECS::AnimationRecorderComponent rec;
    if (j.contains("recordedAnimName")) rec.recordedAnimName = j["recordedAnimName"].get<std::string>();
    if (j.contains("recordInterval")) rec.recordInterval = j["recordInterval"].get<f32>();
    if (j.contains("recordCount")) rec.recordCount = j["recordCount"].get<i32>();
    return rec;
}

json SerializeScriptPropertyValue(const ECS::ScriptPropertyValue& val, ECS::ScriptPropertyType type) {
    switch (type) {
        case ECS::ScriptPropertyType::Int:
        case ECS::ScriptPropertyType::Enum:
            return val.intVal;
        case ECS::ScriptPropertyType::Float:
            return val.floatVal;
        case ECS::ScriptPropertyType::Bool:
            return val.boolVal;
        case ECS::ScriptPropertyType::String:
            return val.stringVal;
        case ECS::ScriptPropertyType::Vector2:
            return SerializeVector2(val.vec2Val);
        case ECS::ScriptPropertyType::Vector3:
            return SerializeVector3(val.vec3Val);
        case ECS::ScriptPropertyType::Vector4:
            return SerializeVector4(val.vec4Val);
        case ECS::ScriptPropertyType::Entity:
            return val.entityVal;
        case ECS::ScriptPropertyType::EntityArray: {
            json arr = json::array();
            for (u64 id : val.entityArrayVal) arr.push_back(id);
            return arr;
        }
    }
    return nullptr;
}

ECS::ScriptPropertyValue DeserializeScriptPropertyValue(const json& j, ECS::ScriptPropertyType type) {
    ECS::ScriptPropertyValue val;
    switch (type) {
        case ECS::ScriptPropertyType::Int:
        case ECS::ScriptPropertyType::Enum:
            if (j.is_number_integer()) val.intVal = j.get<i32>();
            break;
        case ECS::ScriptPropertyType::Float:
            if (j.is_number()) val.floatVal = j.get<f32>();
            break;
        case ECS::ScriptPropertyType::Bool:
            if (j.is_boolean()) val.boolVal = j.get<bool>();
            break;
        case ECS::ScriptPropertyType::String:
            if (j.is_string()) val.stringVal = j.get<std::string>();
            break;
        case ECS::ScriptPropertyType::Vector2:
            if (j.is_array() && j.size() >= 2) val.vec2Val = DeserializeVector2(j);
            break;
        case ECS::ScriptPropertyType::Vector3:
            if (j.is_array() && j.size() >= 3) val.vec3Val = DeserializeVector3(j);
            break;
        case ECS::ScriptPropertyType::Vector4:
            if (j.is_array() && j.size() >= 4) val.vec4Val = DeserializeVector4(j);
            break;
        case ECS::ScriptPropertyType::Entity:
            if (j.is_number_unsigned()) val.entityVal = j.get<u64>();
            break;
        case ECS::ScriptPropertyType::EntityArray:
            if (j.is_array()) {
                for (const auto& e : j) if (e.is_number_unsigned()) val.entityArrayVal.push_back(e.get<u64>());
            }
            break;
    }
    return val;
}

json SerializeScriptComponent(const ECS::ScriptComponent& sc) {
    json j;
    json scriptsArr = json::array();
    for (const auto& script : sc.scripts) {
        json sj;
        sj["path"] = script.scriptPath;
        sj["class"] = script.className;
        sj["enabled"] = script.enabled;
        // Only serialize overridden properties
        json props;
        for (const auto& prop : script.properties) {
            if (prop.isOverridden) {
                json pj;
                pj["type"] = static_cast<int>(prop.type);
                pj["value"] = SerializeScriptPropertyValue(prop.instanceValue, prop.type);
                props[prop.name] = pj;
            }
        }
        if (!props.empty()) {
            sj["properties"] = props;
        }
        scriptsArr.push_back(sj);
    }
    j["scripts"] = scriptsArr;
    return j;
}

ECS::ScriptComponent DeserializeScriptComponent(const json& j) {
    ECS::ScriptComponent sc;
    if (j.contains("scripts") && j["scripts"].is_array()) {
        for (const auto& sj : j["scripts"]) {
            ECS::ScriptAttachment script;
            if (sj.contains("path")) script.scriptPath = SafeStr(sj["path"], MAX_STR_PATH);
            if (sj.contains("class")) script.className = SafeStr(sj["class"], MAX_STR_NAME);
            if (sj.contains("enabled")) script.enabled = JB(sj["enabled"]);
            if (sj.contains("properties") && sj["properties"].is_object()) {
                for (auto it = sj["properties"].begin(); it != sj["properties"].end(); ++it) {
                    ECS::ScriptProperty prop;
                    prop.name = it.key();
                    prop.isOverridden = true;
                    if (it.value().contains("type")) {
                        int v = it.value()["type"].get<int>();
                        // 0..EntityArray(9) — keep in sync with ScriptPropertyType.
                        if (v >= 0 && v <= 9) prop.type = static_cast<ECS::ScriptPropertyType>(v);
                    }
                    if (it.value().contains("value")) {
                        prop.instanceValue = DeserializeScriptPropertyValue(it.value()["value"], prop.type);
                    }
                    script.properties.push_back(prop);
                }
            }
            sc.scripts.push_back(script);
        }
    }
    return sc;
}

// ============================================================================
// Puzzle & Interaction
// ============================================================================

json SerializeLockComponent(const ECS::LockComponent& lk) {
    json j;
    j["requiredKey"] = lk.requiredKey;
    j["isLocked"] = lk.isLocked;
    j["consumeKey"] = lk.consumeKey;
    j["autoOpen"] = RF(lk.autoOpen);
    j["interactRange"] = RF(lk.interactRange);
    j["openMode"] = static_cast<i32>(lk.openMode);
    j["openDuration"] = RF(lk.openDuration);
    j["closedPosition"] = SerializeVector3(lk.closedPosition);
    j["openPosition"] = SerializeVector3(lk.openPosition);
    j["closedRotation"] = SerializeVector3(lk.closedRotation);
    j["openRotation"] = SerializeVector3(lk.openRotation);
    j["openSpeed"] = RF(lk.openSpeed);
    j["lockedPrompt"] = lk.lockedPrompt;
    j["unlockedPrompt"] = lk.unlockedPrompt;
    return j;
}

ECS::LockComponent DeserializeLockComponent(const json& j) {
    ECS::LockComponent lk;
    if (j.contains("requiredKey")) lk.requiredKey = j["requiredKey"].get<std::string>();
    if (j.contains("isLocked")) lk.isLocked = JB(j["isLocked"]);
    if (j.contains("consumeKey")) lk.consumeKey = JB(j["consumeKey"]);
    if (j.contains("autoOpen")) lk.autoOpen = JB(j["autoOpen"]);
    if (j.contains("interactRange")) lk.interactRange = j["interactRange"].get<f32>();
    if (j.contains("openMode")) { i32 v = j["openMode"].get<i32>(); if (v >= 0 && v <= 2) lk.openMode = static_cast<ECS::LockComponent::OpenMode>(v); }
    if (j.contains("openDuration")) lk.openDuration = j["openDuration"].get<f32>();
    if (j.contains("closedPosition")) lk.closedPosition = DeserializeVector3(j["closedPosition"]);
    if (j.contains("openPosition")) lk.openPosition = DeserializeVector3(j["openPosition"]);
    if (j.contains("closedRotation")) lk.closedRotation = DeserializeVector3(j["closedRotation"]);
    if (j.contains("openRotation")) lk.openRotation = DeserializeVector3(j["openRotation"]);
    if (j.contains("openSpeed")) lk.openSpeed = j["openSpeed"].get<f32>();
    if (j.contains("lockedPrompt")) lk.lockedPrompt = j["lockedPrompt"].get<std::string>();
    if (j.contains("unlockedPrompt")) lk.unlockedPrompt = j["unlockedPrompt"].get<std::string>();
    return lk;
}

json SerializePushableComponent(const ECS::PushableComponent& pb) {
    json j;
    j["mass"] = RF(pb.mass);
    j["pushSpeed"] = RF(pb.pushSpeed);
    j["friction"] = RF(pb.friction);
    j["gridSnap"] = RF(pb.gridSnap);
    j["gridCellSize"] = RF(pb.gridCellSize);
    j["gridMoveSpeed"] = RF(pb.gridMoveSpeed);
    j["pushableX"] = RF(pb.pushableX);
    j["pushableY"] = RF(pb.pushableY);
    j["pushableZ"] = RF(pb.pushableZ);
    j["canBePushedOff"] = RF(pb.canBePushedOff);
    return j;
}

ECS::PushableComponent DeserializePushableComponent(const json& j) {
    ECS::PushableComponent pb;
    if (j.contains("mass")) pb.mass = j["mass"].get<f32>();
    if (j.contains("pushSpeed")) pb.pushSpeed = j["pushSpeed"].get<f32>();
    if (j.contains("friction")) pb.friction = j["friction"].get<f32>();
    if (j.contains("gridSnap")) pb.gridSnap = JB(j["gridSnap"]);
    if (j.contains("gridCellSize")) pb.gridCellSize = j["gridCellSize"].get<f32>();
    if (j.contains("gridMoveSpeed")) pb.gridMoveSpeed = j["gridMoveSpeed"].get<f32>();
    if (j.contains("pushableX")) pb.pushableX = JB(j["pushableX"]);
    if (j.contains("pushableY")) pb.pushableY = JB(j["pushableY"]);
    if (j.contains("pushableZ")) pb.pushableZ = JB(j["pushableZ"]);
    if (j.contains("canBePushedOff")) pb.canBePushedOff = JB(j["canBePushedOff"]);
    return pb;
}

json SerializeSwitchComponent(const ECS::SwitchComponent& sw) {
    json j;
    j["type"] = static_cast<i32>(sw.type);
    j["requireSpecificTag"] = sw.requireSpecificTag;
    j["requiredTag"] = sw.requiredTag;
    j["activationWeight"] = RF(sw.activationWeight);
    j["activeDuration"] = RF(sw.activeDuration);
    j["sequenceIndex"] = RF(sw.sequenceIndex);
    j["sequenceGroup"] = RF(sw.sequenceGroup);
    json linked = json::array();
    for (auto e : sw.linkedEntities) {
        linked.push_back(static_cast<u64>(e));
    }
    j["linkedEntities"] = linked;
    j["offPosition"] = SerializeVector3(sw.offPosition);
    j["onPosition"] = SerializeVector3(sw.onPosition);
    j["transitionSpeed"] = RF(sw.transitionSpeed);
    j["promptText"] = sw.promptText;
    j["showPrompt"] = sw.showPrompt;
    return j;
}

ECS::SwitchComponent DeserializeSwitchComponent(const json& j) {
    ECS::SwitchComponent sw;
    if (j.contains("type")) { i32 v = j["type"].get<i32>(); if (v >= 0 && v <= 4) sw.type = static_cast<ECS::SwitchComponent::SwitchType>(v); }
    if (j.contains("requireSpecificTag")) sw.requireSpecificTag = JB(j["requireSpecificTag"]);
    if (j.contains("requiredTag")) sw.requiredTag = j["requiredTag"].get<std::string>();
    if (j.contains("activationWeight")) sw.activationWeight = j["activationWeight"].get<f32>();
    if (j.contains("activeDuration")) sw.activeDuration = j["activeDuration"].get<f32>();
    if (j.contains("sequenceIndex")) sw.sequenceIndex = j["sequenceIndex"].get<i32>();
    if (j.contains("sequenceGroup")) sw.sequenceGroup = j["sequenceGroup"].get<i32>();
    if (j.contains("linkedEntities") && j["linkedEntities"].is_array()) {
        for (const auto& e : j["linkedEntities"]) {
            sw.linkedEntities.push_back(static_cast<ECS::Entity>(e.get<u64>()));
        }
    }
    if (j.contains("offPosition")) sw.offPosition = DeserializeVector3(j["offPosition"]);
    if (j.contains("onPosition")) sw.onPosition = DeserializeVector3(j["onPosition"]);
    if (j.contains("transitionSpeed")) sw.transitionSpeed = j["transitionSpeed"].get<f32>();
    if (j.contains("promptText")) sw.promptText = SafeStr(j["promptText"], MAX_STR_NAME);
    if (j.contains("showPrompt")) sw.showPrompt = JB(j["showPrompt"]);
    return sw;
}

json SerializeGoalZoneComponent(const ECS::GoalZoneComponent& gz) {
    json j;
    j["type"] = static_cast<i32>(gz.type);
    j["requiredTag"] = gz.requiredTag;
    j["requiredItem"] = gz.requiredItem;
    j["goalGroup"] = RF(gz.goalGroup);
    j["inactiveColor"] = SerializeVector3(gz.inactiveColor);
    j["activeColor"] = SerializeVector3(gz.activeColor);
    j["nextScene"] = gz.nextScene;
    return j;
}

ECS::GoalZoneComponent DeserializeGoalZoneComponent(const json& j) {
    ECS::GoalZoneComponent gz;
    if (j.contains("type")) { i32 v = j["type"].get<i32>(); if (v >= 0 && v <= 4) gz.type = static_cast<ECS::GoalZoneComponent::GoalType>(v); }
    if (j.contains("requiredTag")) gz.requiredTag = j["requiredTag"].get<std::string>();
    if (j.contains("requiredItem")) gz.requiredItem = j["requiredItem"].get<std::string>();
    if (j.contains("goalGroup")) gz.goalGroup = j["goalGroup"].get<i32>();
    if (j.contains("inactiveColor")) gz.inactiveColor = DeserializeVector3(j["inactiveColor"]);
    if (j.contains("activeColor")) gz.activeColor = DeserializeVector3(j["activeColor"]);
    if (j.contains("nextScene")) gz.nextScene = j["nextScene"].get<std::string>();
    return gz;
}

json SerializeConveyorComponent(const ECS::ConveyorComponent& cv) {
    json j;
    j["direction"] = SerializeVector3(cv.direction);
    j["speed"] = RF(cv.speed);
    j["affectsPlayer"] = RF(cv.affectsPlayer);
    j["affectsPushables"] = RF(cv.affectsPushables);
    j["isActive"] = cv.isActive;
    return j;
}

ECS::ConveyorComponent DeserializeConveyorComponent(const json& j) {
    ECS::ConveyorComponent cv;
    if (j.contains("direction")) cv.direction = DeserializeVector3(j["direction"]);
    if (j.contains("speed")) cv.speed = j["speed"].get<f32>();
    if (j.contains("affectsPlayer")) cv.affectsPlayer = JB(j["affectsPlayer"]);
    if (j.contains("affectsPushables")) cv.affectsPushables = JB(j["affectsPushables"]);
    if (j.contains("isActive")) cv.isActive = JB(j["isActive"]);
    return cv;
}

json SerializeTeleporterComponent(const ECS::TeleporterComponent& tp) {
    json j;
    j["targetPosition"] = SerializeVector3(tp.targetPosition);
    j["targetRotation"] = SerializeVector3(tp.targetRotation);
    j["linkedTeleporter"] = static_cast<u64>(tp.linkedTeleporter);
    j["cooldown"] = RF(tp.cooldown);
    j["preserveVelocity"] = RF(tp.preserveVelocity);
    j["requiredTag"] = tp.requiredTag;
    return j;
}

ECS::TeleporterComponent DeserializeTeleporterComponent(const json& j) {
    ECS::TeleporterComponent tp;
    if (j.contains("targetPosition")) tp.targetPosition = DeserializeVector3(j["targetPosition"]);
    if (j.contains("targetRotation")) tp.targetRotation = DeserializeVector3(j["targetRotation"]);
    if (j.contains("linkedTeleporter")) tp.linkedTeleporter = static_cast<ECS::Entity>(j["linkedTeleporter"].get<u64>());
    if (j.contains("cooldown")) tp.cooldown = j["cooldown"].get<f32>();
    if (j.contains("preserveVelocity")) tp.preserveVelocity = JB(j["preserveVelocity"]);
    if (j.contains("requiredTag")) tp.requiredTag = j["requiredTag"].get<std::string>();
    return tp;
}

json SerializeDestructibleComponent(const ECS::DestructibleComponent& dc) {
    json j;
    j["health"] = RF(dc.health);
    j["destroyOnHit"] = RF(dc.destroyOnHit);
    j["spawnPickup"] = RF(dc.spawnPickup);
    j["pickupId"] = dc.pickupId;
    j["pickupCount"] = RF(dc.pickupCount);
    j["canRespawn"] = RF(dc.canRespawn);
    j["respawnTime"] = RF(dc.respawnTime);
    j["shakeOnHit"] = RF(dc.shakeOnHit);
    return j;
}

ECS::DestructibleComponent DeserializeDestructibleComponent(const json& j) {
    ECS::DestructibleComponent dc;
    if (j.contains("health")) dc.health = j["health"].get<f32>();
    if (j.contains("destroyOnHit")) dc.destroyOnHit = JB(j["destroyOnHit"]);
    if (j.contains("spawnPickup")) dc.spawnPickup = JB(j["spawnPickup"]);
    if (j.contains("pickupId")) dc.pickupId = j["pickupId"].get<std::string>();
    if (j.contains("pickupCount")) dc.pickupCount = j["pickupCount"].get<i32>();
    if (j.contains("canRespawn")) dc.canRespawn = JB(j["canRespawn"]);
    if (j.contains("respawnTime")) dc.respawnTime = j["respawnTime"].get<f32>();
    if (j.contains("shakeOnHit")) dc.shakeOnHit = j["shakeOnHit"].get<f32>();
    return dc;
}

json SerializeCurlNoiseFieldComponent(const ECS::CurlNoiseFieldComponent& cn) {
    json j;
    j["octaves"] = cn.octaves;
    j["frequency"] = RF(cn.frequency);
    j["amplitude"] = RF(cn.amplitude);
    j["lacunarity"] = RF(cn.lacunarity);
    j["persistence"] = RF(cn.persistence);
    j["seed"] = cn.seed;
    j["timeScale"] = RF(cn.timeScale);
    j["halfExtents"] = SerializeVector3(cn.halfExtents);
    j["falloff"] = static_cast<i32>(cn.falloff);
    j["affectParticles"] = cn.affectParticles;
    j["affectMeshVertices"] = cn.affectMeshVertices;
    j["showDebugArrows"] = cn.showDebugArrows;
    j["debugArrowResolution"] = cn.debugArrowResolution;
    return j;
}

ECS::CurlNoiseFieldComponent DeserializeCurlNoiseFieldComponent(const json& j) {
    ECS::CurlNoiseFieldComponent cn;
    if (j.contains("octaves")) cn.octaves = j["octaves"].get<i32>();
    if (j.contains("frequency")) cn.frequency = j["frequency"].get<f32>();
    if (j.contains("amplitude")) cn.amplitude = j["amplitude"].get<f32>();
    if (j.contains("lacunarity")) cn.lacunarity = j["lacunarity"].get<f32>();
    if (j.contains("persistence")) cn.persistence = j["persistence"].get<f32>();
    if (j.contains("seed")) cn.seed = j["seed"].get<u32>();
    if (j.contains("timeScale")) cn.timeScale = j["timeScale"].get<f32>();
    if (j.contains("halfExtents")) cn.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("falloff")) { i32 v = j["falloff"].get<i32>(); if (v >= 0 && v <= 2) cn.falloff = static_cast<ECS::CurlNoiseFieldComponent::Falloff>(v); }
    if (j.contains("affectParticles")) cn.affectParticles = JB(j["affectParticles"]);
    if (j.contains("affectMeshVertices")) cn.affectMeshVertices = JB(j["affectMeshVertices"]);
    if (j.contains("showDebugArrows")) cn.showDebugArrows = JB(j["showDebugArrows"]);
    if (j.contains("debugArrowResolution")) cn.debugArrowResolution = j["debugArrowResolution"].get<u32>();
    return cn;
}

json SerializeFractureConfigComponent(const ECS::FractureConfigComponent& fc) {
    json j;
    j["fragmentCount"] = fc.fragmentCount;
    j["explosionForce"] = RF(fc.explosionForce);
    j["persistentFragments"] = fc.persistentFragments;
    j["allowRefracture"] = fc.allowRefracture;
    j["maxRefractureDepth"] = fc.maxRefractureDepth;
    j["maxFragmentEntities"] = fc.maxFragmentEntities;
    j["autoCleanup"] = fc.autoCleanup;
    j["cleanupDelay"] = RF(fc.cleanupDelay);
    j["preFracture"] = fc.preFracture;
    j["jointBreakForce"] = RF(fc.jointBreakForce);
    j["fragmentDensity"] = RF(fc.fragmentDensity);
    j["fragmentFriction"] = RF(fc.fragmentFriction);
    j["fragmentBounciness"] = RF(fc.fragmentBounciness);
    j["impactBias"] = RF(fc.impactBias);
    return j;
}

ECS::FractureConfigComponent DeserializeFractureConfigComponent(const json& j) {
    ECS::FractureConfigComponent fc;
    if (j.contains("fragmentCount")) fc.fragmentCount = j["fragmentCount"].get<u32>();
    if (j.contains("explosionForce")) fc.explosionForce = j["explosionForce"].get<f32>();
    if (j.contains("persistentFragments")) fc.persistentFragments = JB(j["persistentFragments"]);
    if (j.contains("allowRefracture")) fc.allowRefracture = JB(j["allowRefracture"]);
    if (j.contains("maxRefractureDepth")) fc.maxRefractureDepth = j["maxRefractureDepth"].get<u32>();
    if (j.contains("maxFragmentEntities")) fc.maxFragmentEntities = j["maxFragmentEntities"].get<u32>();
    if (j.contains("autoCleanup")) fc.autoCleanup = JB(j["autoCleanup"]);
    if (j.contains("cleanupDelay")) fc.cleanupDelay = j["cleanupDelay"].get<f32>();
    if (j.contains("preFracture")) fc.preFracture = JB(j["preFracture"]);
    if (j.contains("jointBreakForce")) fc.jointBreakForce = j["jointBreakForce"].get<f32>();
    if (j.contains("fragmentDensity")) fc.fragmentDensity = j["fragmentDensity"].get<f32>();
    if (j.contains("fragmentFriction")) fc.fragmentFriction = j["fragmentFriction"].get<f32>();
    if (j.contains("fragmentBounciness")) fc.fragmentBounciness = j["fragmentBounciness"].get<f32>();
    if (j.contains("impactBias")) fc.impactBias = j["impactBias"].get<f32>();
    return fc;
}

json SerializeMovingPlatformComponent(const ECS::MovingPlatformComponent& mp) {
    json j;
    json wpArr = json::array();
    for (const auto& wp : mp.waypoints) {
        wpArr.push_back(SerializeVector3(wp));
    }
    j["waypoints"] = wpArr;
    j["speed"] = RF(mp.speed);
    j["waitTime"] = RF(mp.waitTime);
    j["mode"] = static_cast<i32>(mp.mode);
    j["carryEntities"] = RF(mp.carryEntities);
    return j;
}

ECS::MovingPlatformComponent DeserializeMovingPlatformComponent(const json& j) {
    ECS::MovingPlatformComponent mp;
    if (j.contains("waypoints") && j["waypoints"].is_array()) {
        for (const auto& wp : j["waypoints"]) {
            mp.waypoints.push_back(DeserializeVector3(wp));
        }
    }
    if (j.contains("speed")) mp.speed = j["speed"].get<f32>();
    if (j.contains("waitTime")) mp.waitTime = j["waitTime"].get<f32>();
    if (j.contains("mode")) { i32 v = j["mode"].get<i32>(); if (v >= 0 && v <= 3) mp.mode = static_cast<ECS::MovingPlatformComponent::PlatformMode>(v); }
    if (j.contains("carryEntities")) mp.carryEntities = JB(j["carryEntities"]);
    return mp;
}

// ============================================================================
// Skeleton & Animator serialization
// ============================================================================

json SerializeSkeletonComponent(const ECS::SkeletonComponent& skelComp) {
    json j;
    j["sourceAssetPath"] = skelComp.sourceAssetPath;
    // Only emit when grouped, so existing single-mesh scenes don't churn (0 = ungrouped default).
    if (skelComp.skeletonGroupId != 0) j["skeletonGroupId"] = skelComp.skeletonGroupId;

    if (skelComp.skeleton) {
        j["name"] = skelComp.skeleton->name;
        json bonesArr = json::array();
        for (const auto& bone : skelComp.skeleton->bones) {
            json bj;
            bj["name"] = bone.name;
            bj["parentIndex"] = RF(bone.parentIndex);
            bj["bindPosition"] = SerializeVector3(bone.bindPosition);
            bj["bindRotation"] = SerializeQuaternion(bone.bindRotation);
            bj["bindScale"] = SerializeVector3(bone.bindScale);
            bj["inverseBindMatrix"] = SerializeMatrix4(bone.inverseBindMatrix);
            bonesArr.push_back(bj);
        }
        j["bones"] = bonesArr;
    }
    return j;
}

ECS::SkeletonComponent DeserializeSkeletonComponent(const json& j) {
    ECS::SkeletonComponent skelComp;
    if (j.contains("sourceAssetPath")) skelComp.sourceAssetPath = SafeStr(j["sourceAssetPath"], MAX_STR_PATH);
    if (j.contains("skeletonGroupId")) skelComp.skeletonGroupId = j.value("skeletonGroupId", u64{0});

    auto skeleton = std::make_shared<Animation::Skeleton>();
    if (j.contains("name")) skeleton->name = SafeStr(j["name"], MAX_STR_NAME);

    if (j.contains("bones") && j["bones"].is_array() && j["bones"].size() <= 1000) {
        for (const auto& bj : j["bones"]) {
            Animation::Bone bone;
            if (bj.contains("name")) bone.name = bj["name"].get<std::string>();
            if (bj.contains("parentIndex")) bone.parentIndex = bj["parentIndex"].get<i32>();
            if (bj.contains("bindPosition")) bone.bindPosition = DeserializeVector3(bj["bindPosition"]);
            if (bj.contains("bindRotation")) bone.bindRotation = DeserializeQuaternion(bj["bindRotation"]);
            if (bj.contains("bindScale")) bone.bindScale = DeserializeVector3(bj["bindScale"]);
            if (bj.contains("inverseBindMatrix")) bone.inverseBindMatrix = DeserializeMatrix4(bj["inverseBindMatrix"]);
            skeleton->bones.push_back(bone);
        }
    }
    skelComp.skeleton = skeleton;
    return skelComp;
}

// After a full scene load, re-establish the import-time invariant that every mesh in one model
// instance shares a single Skeleton (see SkeletonComponent::skeletonGroupId). DeserializeSkeletonComponent
// rebuilds an independent Skeleton per entity, so without this the leader and its followers end up with
// distinct pointers and RenderSystem::ResolveAnimator can no longer match followers to the leader's
// animator. Canonical-per-group is the member that owns the AnimatorComponent (the leader); order does
// not matter. Groups of one and ungrouped (id 0) entities are left untouched.
static void ReshareSkeletonGroups(ECS::World* world) {
    if (!world) return;
    const std::vector<ECS::Entity> ents = world->GetEntitiesWithComponent<ECS::SkeletonComponent>();

    std::unordered_map<u64, ECS::Entity> canonical;
    for (ECS::Entity e : ents) {
        const auto* sc = world->GetComponent<ECS::SkeletonComponent>(e);
        if (!sc || sc->skeletonGroupId == 0) continue;
        auto it = canonical.find(sc->skeletonGroupId);
        if (it == canonical.end()) { canonical.emplace(sc->skeletonGroupId, e); continue; }
        // Prefer the animator owner (the leader) as the canonical skeleton holder.
        if (world->HasComponent<ECS::AnimatorComponent>(e) &&
            !world->HasComponent<ECS::AnimatorComponent>(it->second)) {
            it->second = e;
        }
    }

    for (ECS::Entity e : ents) {
        auto* sc = world->GetComponent<ECS::SkeletonComponent>(e);
        if (!sc || sc->skeletonGroupId == 0) continue;
        ECS::Entity canon = canonical[sc->skeletonGroupId];
        if (canon == e) continue;
        const auto* canonSc = world->GetComponent<ECS::SkeletonComponent>(canon);
        if (canonSc && canonSc->skeleton) sc->skeleton = canonSc->skeleton;
    }
}

json SerializeAnimatorComponent(const ECS::AnimatorComponent& animComp) {
    json j;
    const auto& animator = animComp.animator;

    j["speed"] = RF(animator.GetSpeed());
    j["currentAnimation"] = animator.GetCurrentAnimationName();

    // Movement drive (only when configured — keeps clip-less animators compact)
    if (animComp.movement.HasAnyClip() || !animComp.movement.enabled) {
        json mv;
        mv["enabled"] = animComp.movement.enabled;
        mv["idleClip"] = animComp.movement.idleClip;
        mv["walkClip"] = animComp.movement.walkClip;
        mv["runClip"] = animComp.movement.runClip;
        mv["jumpClip"] = animComp.movement.jumpClip;
        mv["walkThreshold"] = RF(animComp.movement.walkThreshold);
        mv["runThreshold"] = RF(animComp.movement.runThreshold);
        mv["jumpThreshold"] = RF(animComp.movement.jumpThreshold);
        mv["fadeTime"] = RF(animComp.movement.fadeTime);
        j["movement"] = mv;
    }

    // Serialize all animation clips
    json animsObj = json::object();
    for (const auto& [name, anim] : animator.GetAnimations()) {
        json aj;
        aj["duration"] = RF(anim.duration);
        aj["ticksPerSecond"] = RF(anim.ticksPerSecond);
        aj["playMode"] = static_cast<i32>(anim.playMode);

        json tracksArr = json::array();
        for (const auto& track : anim.tracks) {
            json tj;
            tj["boneName"] = track.boneName;
            tj["boneIndex"] = RF(track.boneIndex);

            // Position keyframes
            json posTimes = json::array();
            for (f32 t : track.positionTimes) posTimes.push_back(t);
            tj["positionTimes"] = posTimes;

            json positions = json::array();
            for (const auto& p : track.positions) positions.push_back(SerializeVector3(p));
            tj["positions"] = positions;

            // Rotation keyframes
            json rotTimes = json::array();
            for (f32 t : track.rotationTimes) rotTimes.push_back(t);
            tj["rotationTimes"] = rotTimes;

            json rotations = json::array();
            for (const auto& r : track.rotations) rotations.push_back(SerializeQuaternion(r));
            tj["rotations"] = rotations;

            // Scale keyframes
            json scaleTimes = json::array();
            for (f32 t : track.scaleTimes) scaleTimes.push_back(t);
            tj["scaleTimes"] = scaleTimes;

            json scales = json::array();
            for (const auto& s : track.scales) scales.push_back(SerializeVector3(s));
            tj["scales"] = scales;

            tracksArr.push_back(tj);
        }
        aj["tracks"] = tracksArr;

        // Animation events
        json eventsArr = json::array();
        for (const auto& evt : anim.events) {
            json ej;
            ej["time"] = RF(evt.time);
            ej["name"] = evt.name;
            eventsArr.push_back(ej);
        }
        aj["events"] = eventsArr;

        animsObj[name] = aj;
    }
    j["animations"] = animsObj;

    // Serialize state machine
    const auto& sm = animComp.stateMachine;
    json smJson;
    smJson["defaultState"] = sm.GetDefaultState();

    // States
    json statesObj = json::object();
    for (const auto& [name, state] : sm.GetStates()) {
        json sj;
        sj["animationName"] = state.animationName;
        sj["speed"] = RF(state.speed);
        sj["playMode"] = static_cast<i32>(state.playMode);
        sj["editorPosition"] = SerializeVector2(state.editorPosition);
        statesObj[name] = sj;
    }
    smJson["states"] = statesObj;

    // Transitions
    json transArr = json::array();
    for (const auto& trans : sm.GetTransitions()) {
        json tj;
        tj["fromState"] = trans.fromState;
        tj["toState"] = trans.toState;
        tj["blendTime"] = RF(trans.blendTime);
        tj["hasExitTime"] = RF(trans.hasExitTime);
        tj["exitTime"] = RF(trans.exitTime);

        json condArr = json::array();
        for (const auto& cond : trans.conditions) {
            json cj;
            cj["parameterName"] = cond.parameterName;
            cj["type"] = static_cast<i32>(cond.type);
            cj["comparison"] = static_cast<i32>(cond.comparison);
            switch (cond.type) {
                case Animation::TransitionCondition::Type::Bool:
                case Animation::TransitionCondition::Type::Trigger:
                    cj["boolValue"] = RF(cond.value.boolValue);
                    break;
                case Animation::TransitionCondition::Type::Float:
                    cj["floatValue"] = RF(cond.value.floatValue);
                    break;
                case Animation::TransitionCondition::Type::Int:
                    cj["intValue"] = RF(cond.value.intValue);
                    break;
            }
            condArr.push_back(cj);
        }
        tj["conditions"] = condArr;
        transArr.push_back(tj);
    }
    smJson["transitions"] = transArr;

    // Parameters
    json boolParams = json::object();
    for (const auto& [k, v] : sm.GetBoolParams()) boolParams[k] = v;
    smJson["boolParams"] = boolParams;

    json floatParams = json::object();
    for (const auto& [k, v] : sm.GetFloatParams()) floatParams[k] = v;
    smJson["floatParams"] = floatParams;

    json intParams = json::object();
    for (const auto& [k, v] : sm.GetIntParams()) intParams[k] = v;
    smJson["intParams"] = intParams;

    j["stateMachine"] = smJson;

    // Serialize blend tree
    const auto& bt = animComp.blendTree;
    json btJson;
    btJson["enabled"] = bt.enabled;
    btJson["parameterName"] = bt.parameterName;
    json btNodesArr = json::array();
    for (const auto& node : bt.nodes) {
        json nj;
        nj["animationName"] = node.animationName;
        nj["threshold"] = RF(node.threshold);
        btNodesArr.push_back(nj);
    }
    btJson["nodes"] = btNodesArr;
    j["blendTree"] = btJson;

    // Serialize blend parameters
    json blendParamsObj = json::object();
    for (const auto& [k, v] : animComp.blendParameters) {
        blendParamsObj[k] = RF(v);
    }
    j["blendParameters"] = blendParamsObj;

    return j;
}

ECS::AnimatorComponent DeserializeAnimatorComponent(const json& j, std::shared_ptr<Animation::Skeleton> skeleton) {
    ECS::AnimatorComponent animComp;
    if (skeleton) {
        animComp.Initialize(skeleton);
    }

    f32 speed = 1.0f;
    if (j.contains("speed")) speed = j["speed"].get<f32>();
    animComp.animator.SetSpeed(speed);

    if (j.contains("movement") && j["movement"].is_object()) {
        const auto& mv = j["movement"];
        auto& m = animComp.movement;
        if (mv.contains("enabled")) m.enabled = mv["enabled"].get<bool>();
        if (mv.contains("idleClip")) m.idleClip = SafeStr(mv["idleClip"]);
        if (mv.contains("walkClip")) m.walkClip = SafeStr(mv["walkClip"]);
        if (mv.contains("runClip")) m.runClip = SafeStr(mv["runClip"]);
        if (mv.contains("jumpClip")) m.jumpClip = SafeStr(mv["jumpClip"]);
        if (mv.contains("walkThreshold")) m.walkThreshold = mv["walkThreshold"].get<f32>();
        if (mv.contains("runThreshold")) m.runThreshold = mv["runThreshold"].get<f32>();
        if (mv.contains("jumpThreshold")) m.jumpThreshold = mv["jumpThreshold"].get<f32>();
        if (mv.contains("fadeTime")) m.fadeTime = mv["fadeTime"].get<f32>();
    }

    // Deserialize animation clips
    if (j.contains("animations") && j["animations"].is_object()) {
        for (auto& [name, aj] : j["animations"].items()) {
            Animation::SkeletalAnimation anim;
            anim.name = name;
            if (aj.contains("duration")) anim.duration = aj["duration"].get<f32>();
            if (aj.contains("ticksPerSecond")) anim.ticksPerSecond = aj["ticksPerSecond"].get<f32>();
            if (aj.contains("playMode")) { i32 v = aj["playMode"].get<i32>(); if (v >= 0 && v <= 3) anim.playMode = static_cast<Animation::PlayMode>(v); }

            static constexpr usize kMaxTracks = 1000;
            static constexpr usize kMaxKeyframes = 100'000;
            if (aj.contains("tracks") && aj["tracks"].is_array() && aj["tracks"].size() <= kMaxTracks) {
                for (const auto& tj : aj["tracks"]) {
                    Animation::BoneTrack track;
                    if (tj.contains("boneName")) track.boneName = tj["boneName"].get<std::string>();
                    if (tj.contains("boneIndex")) track.boneIndex = tj["boneIndex"].get<i32>();

                    // Position keyframes
                    if (tj.contains("positionTimes") && tj["positionTimes"].is_array() && tj["positionTimes"].size() <= kMaxKeyframes) {
                        for (const auto& t : tj["positionTimes"]) track.positionTimes.push_back(t.get<f32>());
                    }
                    if (tj.contains("positions") && tj["positions"].is_array() && tj["positions"].size() <= kMaxKeyframes) {
                        for (const auto& p : tj["positions"]) track.positions.push_back(DeserializeVector3(p));
                    }

                    // Rotation keyframes
                    if (tj.contains("rotationTimes") && tj["rotationTimes"].is_array() && tj["rotationTimes"].size() <= kMaxKeyframes) {
                        for (const auto& t : tj["rotationTimes"]) track.rotationTimes.push_back(t.get<f32>());
                    }
                    if (tj.contains("rotations") && tj["rotations"].is_array() && tj["rotations"].size() <= kMaxKeyframes) {
                        for (const auto& r : tj["rotations"]) track.rotations.push_back(DeserializeQuaternion(r));
                    }

                    // Scale keyframes
                    if (tj.contains("scaleTimes") && tj["scaleTimes"].is_array() && tj["scaleTimes"].size() <= kMaxKeyframes) {
                        for (const auto& t : tj["scaleTimes"]) track.scaleTimes.push_back(t.get<f32>());
                    }
                    if (tj.contains("scales") && tj["scales"].is_array() && tj["scales"].size() <= kMaxKeyframes) {
                        for (const auto& s : tj["scales"]) track.scales.push_back(DeserializeVector3(s));
                    }

                    anim.tracks.push_back(track);
                }
            }

            // Animation events
            if (aj.contains("events") && aj["events"].is_array()) {
                for (const auto& ej : aj["events"]) {
                    Animation::SkeletalAnimation::AnimEvent evt;
                    if (ej.contains("time")) evt.time = ej["time"].get<f32>();
                    if (ej.contains("name")) evt.name = ej["name"].get<std::string>();
                    anim.events.push_back(evt);
                }
            }

            animComp.animator.AddAnimation(anim);
        }
    }

    // Deserialize state machine
    if (j.contains("stateMachine") && j["stateMachine"].is_object()) {
        const auto& smJson = j["stateMachine"];

        if (smJson.contains("defaultState"))
            animComp.stateMachine.SetDefaultState(smJson["defaultState"].get<std::string>());

        // States
        if (smJson.contains("states") && smJson["states"].is_object()) {
            for (auto& [name, sj] : smJson["states"].items()) {
                Animation::AnimationState state;
                state.name = name;
                if (sj.contains("animationName")) state.animationName = sj["animationName"].get<std::string>();
                if (sj.contains("speed")) state.speed = sj["speed"].get<f32>();
                if (sj.contains("playMode")) { i32 v = sj["playMode"].get<i32>(); if (v >= 0 && v <= 3) state.playMode = static_cast<Animation::PlayMode>(v); }
                if (sj.contains("editorPosition")) state.editorPosition = DeserializeVector2(sj["editorPosition"]);
                animComp.stateMachine.AddState(state);
            }
        }

        // Transitions
        if (smJson.contains("transitions") && smJson["transitions"].is_array()) {
            for (const auto& tj : smJson["transitions"]) {
                Animation::AnimationTransition trans;
                if (tj.contains("fromState")) trans.fromState = tj["fromState"].get<std::string>();
                if (tj.contains("toState")) trans.toState = tj["toState"].get<std::string>();
                if (tj.contains("blendTime")) trans.blendTime = tj["blendTime"].get<f32>();
                if (tj.contains("hasExitTime")) trans.hasExitTime = JB(tj["hasExitTime"]);
                if (tj.contains("exitTime")) trans.exitTime = tj["exitTime"].get<f32>();

                if (tj.contains("conditions") && tj["conditions"].is_array()) {
                    for (const auto& cj : tj["conditions"]) {
                        Animation::TransitionCondition cond;
                        if (cj.contains("parameterName")) cond.parameterName = cj["parameterName"].get<std::string>();
                        if (cj.contains("type")) { i32 v = cj["type"].get<i32>(); if (v >= 0 && v <= 3) cond.type = static_cast<Animation::TransitionCondition::Type>(v); }
                        if (cj.contains("comparison")) { i32 v = cj["comparison"].get<i32>(); if (v >= 0 && v <= 5) cond.comparison = static_cast<Animation::TransitionCondition::Comparison>(v); }

                        switch (cond.type) {
                            case Animation::TransitionCondition::Type::Bool:
                            case Animation::TransitionCondition::Type::Trigger:
                                if (cj.contains("boolValue")) cond.value.boolValue = JB(cj["boolValue"]);
                                break;
                            case Animation::TransitionCondition::Type::Float:
                                if (cj.contains("floatValue")) cond.value.floatValue = cj["floatValue"].get<f32>();
                                break;
                            case Animation::TransitionCondition::Type::Int:
                                if (cj.contains("intValue")) cond.value.intValue = cj["intValue"].get<i32>();
                                break;
                        }
                        trans.conditions.push_back(cond);
                    }
                }

                animComp.stateMachine.AddTransition(trans);
            }
        }

        // Parameters
        if (smJson.contains("boolParams") && smJson["boolParams"].is_object()) {
            for (auto& [k, v] : smJson["boolParams"].items())
                animComp.stateMachine.SetBool(k, JB(v));
        }
        if (smJson.contains("floatParams") && smJson["floatParams"].is_object()) {
            for (auto& [k, v] : smJson["floatParams"].items())
                animComp.stateMachine.SetFloat(k, v.get<f32>());
        }
        if (smJson.contains("intParams") && smJson["intParams"].is_object()) {
            for (auto& [k, v] : smJson["intParams"].items())
                animComp.stateMachine.SetInt(k, v.get<i32>());
        }
    }

    // Deserialize blend tree
    if (j.contains("blendTree") && j["blendTree"].is_object()) {
        const auto& btJson = j["blendTree"];
        if (btJson.contains("enabled")) animComp.blendTree.enabled = JB(btJson["enabled"]);
        if (btJson.contains("parameterName")) animComp.blendTree.parameterName = btJson["parameterName"].get<std::string>();
        if (btJson.contains("nodes") && btJson["nodes"].is_array()) {
            for (const auto& nj : btJson["nodes"]) {
                Animation::BlendNode node;
                if (nj.contains("animationName")) node.animationName = nj["animationName"].get<std::string>();
                if (nj.contains("threshold")) node.threshold = nj["threshold"].get<f32>();
                animComp.blendTree.nodes.push_back(node);
            }
        }
    }

    // Deserialize blend parameters
    if (j.contains("blendParameters") && j["blendParameters"].is_object()) {
        for (auto& [k, v] : j["blendParameters"].items()) {
            animComp.blendParameters[k] = v.get<f32>();
        }
    }

    // Restore current animation playback
    std::string currentAnim;
    if (j.contains("currentAnimation")) currentAnim = j["currentAnimation"].get<std::string>();
    if (!currentAnim.empty() && animComp.animator.HasAnimation(currentAnim)) {
        animComp.animator.Play(currentAnim);
    }

    return animComp;
}

} // anonymous namespace

SceneSerializer::SceneSerializer(ECS::World* world)
    : m_World(world) {
}

void SceneSerializer::MigrateScene(json& root, u32 fromVersion) {
    // Apply incremental migrations from fromVersion up to SCENE_FORMAT_VERSION.
    // Each case falls through to apply all subsequent migrations in order.
    //
    // Example for future migrations:
    //   if (fromVersion < 2) {
    //       // Migration from version 1 -> 2: e.g. rename a component field
    //       for (auto& entity : root["entities"]) {
    //           if (entity.contains("oldField")) {
    //               entity["newField"] = entity["oldField"];
    //               entity.erase("oldField");
    //           }
    //       }
    //   }

    // Version 0 -> 1: No structural changes needed (first versioned format)
    (void)root;
    (void)fromVersion;

    // Stamp the migrated version so re-saves use the current format
    root["formatVersion"] = SCENE_FORMAT_VERSION;
}

SerializationResult SceneSerializer::Save(const std::string& filepath, const SerializationOptions& options) {
    if (!m_World) {
        SerializationResult result;
        result.success = false;
        result.error = "No world set";
        result.filepath = filepath;
        return result;
    }

    return SaveEntities(filepath, m_World->GetAllEntities(), options);
}

// ----------------------------------------------------------------------------
// Component (de)serialization registry - single source of truth. Each entry wires
// a scene-JSON key to its component has / serialize / deserialize / remove ops.
// The three per-key helpers below iterate this table, so a component is registered
// ONCE instead of edited into a dozen hand-maintained else-if chains (which had hit
// MSVC's C1061 nesting limit and silently dropped components). Uniform components
// use ENJIN_SERDES; the few irregular ones are explicit entries.
// ----------------------------------------------------------------------------
struct ComponentSerdes {
    const char* key;
    bool (*has)(ECS::World*, ECS::Entity);
    json (*ser)(ECS::World*, ECS::Entity);
    void (*de)(ECS::World*, ECS::Entity, const json&);
    void (*rem)(ECS::World*, ECS::Entity);
};

#define ENJIN_SERDES(KEY, TYPE, SERFN, DESERFN) ComponentSerdes{ KEY, [](ECS::World* w, ECS::Entity e){ return w->HasComponent<TYPE>(e); }, [](ECS::World* w, ECS::Entity e)->json{ return SERFN(*w->GetComponent<TYPE>(e)); }, [](ECS::World* w, ECS::Entity e, const json& j){ w->AddComponent<TYPE>(e, DESERFN(j)); }, [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<TYPE>(e); } }

static const std::vector<ComponentSerdes>& ComponentRegistry() {
    static const std::vector<ComponentSerdes> reg = {
        ENJIN_SERDES("aiController", ECS::AIControllerComponent, SerializeAIControllerComponent, DeserializeAIControllerComponent),
        ENJIN_SERDES("animatedSprite2D", ECS::AnimatedSprite2DComponent, SerializeAnimatedSprite2DComponent, DeserializeAnimatedSprite2DComponent),
        ENJIN_SERDES("animationRecorder", ECS::AnimationRecorderComponent, SerializeAnimationRecorderComponent, DeserializeAnimationRecorderComponent),
        ENJIN_SERDES("artStyle", ECS::ArtStyleComponent, SerializeArtStyleComponent, DeserializeArtStyleComponent),
        ENJIN_SERDES("audioCollision", ECS::AudioCollisionComponent, SerializeAudioCollisionComponent, DeserializeAudioCollisionComponent),
        ENJIN_SERDES("audioFidelity", ECS::AudioFidelityComponent, SerializeAudioFidelityComponent, DeserializeAudioFidelityComponent),
        ENJIN_SERDES("audioListener", ECS::AudioListenerComponent, SerializeAudioListenerComponent, DeserializeAudioListenerComponent),
        ENJIN_SERDES("audioOcclusion", ECS::AudioOcclusionComponent, SerializeAudioOcclusionComponent, DeserializeAudioOcclusionComponent),
        ENJIN_SERDES("audioReactive", ECS::AudioReactiveComponent, SerializeAudioReactiveComponent, DeserializeAudioReactiveComponent),
        ENJIN_SERDES("audioSnapshotTrigger", ECS::AudioSnapshotTriggerComponent, SerializeAudioSnapshotTriggerComponent, DeserializeAudioSnapshotTriggerComponent),
        ENJIN_SERDES("audioSource", ECS::AudioSourceComponent, SerializeAudioSourceComponent, DeserializeAudioSourceComponent),
        ENJIN_SERDES("audioThresholdTrigger", ECS::AudioThresholdTriggerComponent, SerializeAudioThresholdTriggerComponent, DeserializeAudioThresholdTriggerComponent),
        ENJIN_SERDES("ballSocketJoint", ECS::BallSocketJointComponent, SerializeBallSocketJointComponent, DeserializeBallSocketJointComponent),
        ENJIN_SERDES("beatClock", ECS::BeatClockComponent, SerializeBeatClockComponent, DeserializeBeatClockComponent),
        ENJIN_SERDES("beatSync", ECS::BeatSyncComponent, SerializeBeatSyncComponent, DeserializeBeatSyncComponent),
        ENJIN_SERDES("behaviorTree", ECS::BehaviorTreeComponent, SerializeBehaviorTreeComponent, DeserializeBehaviorTreeComponent),
        ENJIN_SERDES("billboard", ECS::BillboardComponent, SerializeBillboardComponent, DeserializeBillboardComponent),
        ENJIN_SERDES("body2D", Physics::Body2DComponent, SerializeBody2DComponent, DeserializeBody2DComponent),
        ENJIN_SERDES("boxCollider", ECS::BoxColliderComponent, SerializeBoxColliderComponent, DeserializeBoxColliderComponent),
        ENJIN_SERDES("camera", ECS::CameraComponent, SerializeCameraComponent, DeserializeCameraComponent),
        ENJIN_SERDES("camera2DBounds", ECS::Camera2DBoundsComponent, SerializeCamera2DBoundsComponent, DeserializeCamera2DBoundsComponent),
        ENJIN_SERDES("cameraTrigger", ECS::CameraTriggerComponent, SerializeCameraTriggerComponent, DeserializeCameraTriggerComponent),
        ENJIN_SERDES("capsuleCollider", ECS::CapsuleColliderComponent, SerializeCapsuleColliderComponent, DeserializeCapsuleColliderComponent),
        ENJIN_SERDES("cinematicCamera", ECS::CinematicCameraComponent, SerializeCinematicCameraComponent, DeserializeCinematicCameraComponent),
        ENJIN_SERDES("cloth", ECS::ClothComponent, SerializeClothComponent, DeserializeClothComponent),
        ENJIN_SERDES("conductor", ECS::ConductorComponent, SerializeConductorComponent, DeserializeConductorComponent),
        ENJIN_SERDES("conveyor", ECS::ConveyorComponent, SerializeConveyorComponent, DeserializeConveyorComponent),
        ENJIN_SERDES("curlNoiseField", ECS::CurlNoiseFieldComponent, SerializeCurlNoiseFieldComponent, DeserializeCurlNoiseFieldComponent),
        ENJIN_SERDES("damage", ECS::DamageComponent, SerializeDamageComponent, DeserializeDamageComponent),
        ENJIN_SERDES("damageResistance", ECS::DamageResistanceComponent, SerializeDamageResistanceComponent, DeserializeDamageResistanceComponent),
        ENJIN_SERDES("destructible", ECS::DestructibleComponent, SerializeDestructibleComponent, DeserializeDestructibleComponent),
        ENJIN_SERDES("dialogue", ECS::DialogueComponent, SerializeDialogueComponent, DeserializeDialogueComponent),
        ENJIN_SERDES("dialogueBox", ECS::DialogueBoxComponent, SerializeDialogueBoxComponent, DeserializeDialogueBoxComponent),
        ENJIN_SERDES("distanceJoint", ECS::DistanceJointComponent, SerializeDistanceJointComponent, DeserializeDistanceJointComponent),
        ENJIN_SERDES("dungeonGenerator", ECS::DungeonGeneratorComponent, SerializeDungeonGeneratorComponent, DeserializeDungeonGeneratorComponent),
        ENJIN_SERDES("dynamicDifficulty", ECS::DynamicDifficultyComponent, SerializeDynamicDifficultyComponent, DeserializeDynamicDifficultyComponent),
        ENJIN_SERDES("elementalEmitter", ECS::ElementalEmitterComponent, SerializeElementalEmitterComponent, DeserializeElementalEmitterComponent),
        ENJIN_SERDES("elementalSurface", ECS::ElementalSurfaceComponent, SerializeElementalSurfaceComponent, DeserializeElementalSurfaceComponent),
        ENJIN_SERDES("elementalVolume", ECS::ElementalVolumeComponent, SerializeElementalVolumeComponent, DeserializeElementalVolumeComponent),
        ENJIN_SERDES("firstPerson", ECS::FirstPersonController, SerializeFirstPerson, DeserializeFirstPerson),
        ENJIN_SERDES("fixedJoint", ECS::FixedJointComponent, SerializeFixedJointComponent, DeserializeFixedJointComponent),
        ENJIN_SERDES("flowerParticleConfig", ECS::FlowerParticleConfigComponent, SerializeFlowerParticleConfigComponent, DeserializeFlowerParticleConfigComponent),
        ENJIN_SERDES("flowerStem", ECS::FlowerStemComponent, SerializeFlowerStemComponent, DeserializeFlowerStemComponent),
        ENJIN_SERDES("fluidVolume", ECS::FluidVolumeComponent, SerializeFluidVolumeComponent, DeserializeFluidVolumeComponent),
        ENJIN_SERDES("followTarget", ECS::FollowTargetComponent, SerializeFollowTargetComponent, DeserializeFollowTargetComponent),
        ENJIN_SERDES("footstep", ECS::FootstepComponent, SerializeFootstepComponent, DeserializeFootstepComponent),
        ENJIN_SERDES("fractureConfig", ECS::FractureConfigComponent, SerializeFractureConfigComponent, DeserializeFractureConfigComponent),
        ENJIN_SERDES("gameOver", ECS::GameOverComponent, SerializeGameOverComponent, DeserializeGameOverComponent),
        ENJIN_SERDES("goalZone", ECS::GoalZoneComponent, SerializeGoalZoneComponent, DeserializeGoalZoneComponent),
        ENJIN_SERDES("gpuParticleEmitter", ECS::GPUParticleEmitterComponent, SerializeGPUParticleEmitterComponent, DeserializeGPUParticleEmitterComponent),
        ENJIN_SERDES("grabbable", ECS::GrabbableComponent, SerializeGrabbableComponent, DeserializeGrabbableComponent),
        ENJIN_SERDES("grassVolume", ECS::GrassVolumeComponent, SerializeGrassVolumeComponent, DeserializeGrassVolumeComponent),
        ENJIN_SERDES("gravityZone", ECS::GravityZoneComponent, SerializeGravityZoneComponent, DeserializeGravityZoneComponent),
        ENJIN_SERDES("health", ECS::HealthComponent, SerializeHealthComponent, DeserializeHealthComponent),
        ENJIN_SERDES("hingeJoint", ECS::HingeJointComponent, SerializeHingeJointComponent, DeserializeHingeJointComponent),
        ENJIN_SERDES("hudWidget", ECS::HUDWidgetComponent, SerializeHUDWidgetComponent, DeserializeHUDWidgetComponent),
        ENJIN_SERDES("interactable", ECS::InteractableComponent, SerializeInteractableComponent, DeserializeInteractableComponent),
        ENJIN_SERDES("interactiveWater", Effects::InteractiveWaterComponent, SerializeInteractiveWaterComponent, DeserializeInteractiveWaterComponent),
        ENJIN_SERDES("inventory", ECS::InventoryComponent, SerializeInventoryComponent, DeserializeInventoryComponent),
        ENJIN_SERDES("jellyMesh", ECS::JellyMeshComponent, SerializeJellyMeshComponent, DeserializeJellyMeshComponent),
        ENJIN_SERDES("joint2D", Physics::Joint2DComponent, SerializeJoint2DComponent, DeserializeJoint2DComponent),
        ENJIN_SERDES("lens", ECS::LensComponent, SerializeLensComponent, DeserializeLensComponent),
        ENJIN_SERDES("light", ECS::LightComponent, SerializeLightComponent, DeserializeLightComponent),
        ENJIN_SERDES("lock", ECS::LockComponent, SerializeLockComponent, DeserializeLockComponent),
        ENJIN_SERDES("lod", ECS::LODComponent, SerializeLODComponent, DeserializeLODComponent),
        ENJIN_SERDES("lookAtTarget", ECS::LookAtTargetComponent, SerializeLookAtTargetComponent, DeserializeLookAtTargetComponent),
        ENJIN_SERDES("material", ECS::MaterialComponent, SerializeMaterialComponent, DeserializeMaterialComponent),
        ENJIN_SERDES("materialInteractionTable", ECS::MaterialInteractionTableComponent, SerializeMaterialInteractionTableComponent, DeserializeMaterialInteractionTableComponent),
        ENJIN_SERDES("materialSlots", ECS::MaterialSlotsComponent, SerializeMaterialSlotsComponent, DeserializeMaterialSlotsComponent),
        ENJIN_SERDES("meshCollider", ECS::MeshColliderComponent, SerializeMeshColliderComponent, DeserializeMeshColliderComponent),
        ENJIN_SERDES("meshRenderer", ECS::MeshRendererComponent, SerializeMeshRendererComponent, DeserializeMeshRendererComponent),
        ENJIN_SERDES("midiBinding", ECS::MIDIBindingComponent, SerializeMIDIBindingComponent, DeserializeMIDIBindingComponent),
        ComponentSerdes{ "morphTargets",
            [](ECS::World* w, ECS::Entity e){ return w->HasComponent<ECS::MorphTargetComponent>(e); },
            [](ECS::World* w, ECS::Entity e)->json{ return SerializeMorphTargetComponent(*w->GetComponent<ECS::MorphTargetComponent>(e), true); },
            [](ECS::World* w, ECS::Entity e, const json& j){ w->AddComponent<ECS::MorphTargetComponent>(e, DeserializeMorphTargetComponent(j)); },
            [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<ECS::MorphTargetComponent>(e); } },
        ENJIN_SERDES("movingPlatform", ECS::MovingPlatformComponent, SerializeMovingPlatformComponent, DeserializeMovingPlatformComponent),
        ENJIN_SERDES("musicZone", ECS::MusicZoneComponent, SerializeMusicZoneComponent, DeserializeMusicZoneComponent),
        ENJIN_SERDES("name", ECS::NameComponent, SerializeNameComponent, DeserializeNameComponent),
        ENJIN_SERDES("networkIdentity", ECS::NetworkIdentityComponent, SerializeNetworkIdentityComponent, DeserializeNetworkIdentityComponent),
        ENJIN_SERDES("networkTransform", ECS::NetworkTransformComponent, SerializeNetworkTransformComponent, DeserializeNetworkTransformComponent),
        ENJIN_SERDES("notes", ECS::NotesComponent, SerializeNotesComponent, DeserializeNotesComponent),
        ENJIN_SERDES("parallaxMachine", ECS::ParallaxMachineComponent, SerializeParallaxMachineComponent, DeserializeParallaxMachineComponent),
        ENJIN_SERDES("parallaxLayer", ECS::ParallaxLayerComponent, SerializeParallaxLayerComponent, DeserializeParallaxLayerComponent),
        ENJIN_SERDES("particleEmitter", ECS::ParticleEmitterComponent, SerializeParticleEmitterComponent, DeserializeParticleEmitterComponent),
        ENJIN_SERDES("perFrameCollider", ECS::PerFrameColliderComponent, SerializePerFrameColliderComponent, DeserializePerFrameColliderComponent),
        ENJIN_SERDES("pickup", ECS::PickupComponent, SerializePickupComponent, DeserializePickupComponent),
        ENJIN_SERDES("platformer2D", ECS::Platformer2DController, SerializePlatformer2D, DeserializePlatformer2D),
        ENJIN_SERDES("polygonCollider2D", ECS::PolygonCollider2DComponent, SerializePolygonCollider2DComponent, DeserializePolygonCollider2DComponent),
        ENJIN_SERDES("poolable", ECS::PoolableComponent, SerializePoolableComponent, DeserializePoolableComponent),
        ENJIN_SERDES("poseLibrary", ECS::PoseLibraryComponent, SerializePoseLibraryComponent, DeserializePoseLibraryComponent),
        ENJIN_SERDES("possessable", ECS::PossessableComponent, SerializePossessable, DeserializePossessable),
        ENJIN_SERDES("postProcessVolume", ECS::PostProcessVolumeComponent, SerializePostProcessVolumeComponent, DeserializePostProcessVolumeComponent),
        ENJIN_SERDES("pushable", ECS::PushableComponent, SerializePushableComponent, DeserializePushableComponent),
        ENJIN_SERDES("questFlow", ECS::QuestFlowComponent, SerializeQuestFlowComponent, DeserializeQuestFlowComponent),
        ENJIN_SERDES("questState", ECS::QuestStateComponent, SerializeQuestStateComponent, DeserializeQuestStateComponent),
        ENJIN_SERDES("ragdoll", ECS::RagdollComponent, SerializeRagdollComponent, DeserializeRagdollComponent),
        ENJIN_SERDES("randomBag", ECS::RandomBagComponent, SerializeRandomBagComponent, DeserializeRandomBagComponent),
        ENJIN_SERDES("recordRewind", ECS::RecordRewindComponent, SerializeRecordRewindComponent, DeserializeRecordRewindComponent),
        ENJIN_SERDES("reflectionProbe", ECS::ReflectionProbeComponent, SerializeReflectionProbeComponent, DeserializeReflectionProbeComponent),
        ENJIN_SERDES("reflectivePlane", ECS::ReflectivePlaneComponent, SerializeReflectivePlaneComponent, DeserializeReflectivePlaneComponent),
        ENJIN_SERDES("ladder", ECS::LadderComponent, SerializeLadderComponent, DeserializeLadderComponent),
        ENJIN_SERDES("rope", ECS::RopeComponent, SerializeRopeComponent, DeserializeRopeComponent),
        ENJIN_SERDES("door", ECS::DoorComponent, SerializeDoorComponent, DeserializeDoorComponent),
        ENJIN_SERDES("resource", ECS::ResourceComponent, SerializeResourceComponent, DeserializeResourceComponent),
        ENJIN_SERDES("reverbZone", ECS::ReverbZoneComponent, SerializeReverbZoneComponent, DeserializeReverbZoneComponent),
        ENJIN_SERDES("rigidbody", ECS::RigidbodyComponent, SerializeRigidbodyComponent, DeserializeRigidbodyComponent),
        ENJIN_SERDES("rtpc", ECS::RTPCComponent, SerializeRTPCComponent, DeserializeRTPCComponent),
        ENJIN_SERDES("saveData", ECS::SaveDataComponent, SerializeSaveDataComponent, DeserializeSaveDataComponent),
        ENJIN_SERDES("saveLoadMenu", ECS::SaveLoadMenuComponent, SerializeSaveLoadMenuComponent, DeserializeSaveLoadMenuComponent),
        ENJIN_SERDES("scatter", ECS::ScatterComponent, SerializeScatterComponent, DeserializeScatterComponent),
        ENJIN_SERDES("sceneRewind", ECS::SceneRewindComponent, SerializeSceneRewindComponent, DeserializeSceneRewindComponent),
        ENJIN_SERDES("scriptComponent", ECS::ScriptComponent, SerializeScriptComponent, DeserializeScriptComponent),
        ENJIN_SERDES("shrubVolume", ECS::ShrubVolumeComponent, SerializeShrubVolumeComponent, DeserializeShrubVolumeComponent),
        ENJIN_SERDES("sidechain", ECS::SidechainComponent, SerializeSidechainComponent, DeserializeSidechainComponent),
        ENJIN_SERDES("skeleton", ECS::SkeletonComponent, SerializeSkeletonComponent, DeserializeSkeletonComponent),
        ENJIN_SERDES("sliderJoint", ECS::SliderJointComponent, SerializeSliderJointComponent, DeserializeSliderJointComponent),
        ENJIN_SERDES("spawnPoint", ECS::SpawnPointComponent, SerializeSpawnPointComponent, DeserializeSpawnPointComponent),
        ENJIN_SERDES("sphereCollider", ECS::SphereColliderComponent, SerializeSphereColliderComponent, DeserializeSphereColliderComponent),
        ENJIN_SERDES("springJoint", ECS::SpringJointComponent, SerializeSpringJointComponent, DeserializeSpringJointComponent),
        ENJIN_SERDES("sprite2D", ECS::Sprite2DComponent, SerializeSprite2DComponent, DeserializeSprite2DComponent),
        ENJIN_SERDES("stateMachine", ECS::StateMachineComponent, SerializeStateMachineComponent, DeserializeStateMachineComponent),
        ENJIN_SERDES("streamingPortal", Scene::StreamingPortalComponent, SerializeStreamingPortalComponent, DeserializeStreamingPortalComponent),
        ENJIN_SERDES("streamingVolume", Scene::StreamingVolumeComponent, SerializeStreamingVolumeComponent, DeserializeStreamingVolumeComponent),
        ENJIN_SERDES("surfaceAligned", ECS::SurfaceAlignedController, SerializeSurfaceAligned, DeserializeSurfaceAligned),
        ENJIN_SERDES("swarm", ECS::SwarmComponent, SerializeSwarmComponent, DeserializeSwarmComponent),
        ENJIN_SERDES("switch", ECS::SwitchComponent, SerializeSwitchComponent, DeserializeSwitchComponent),
        ENJIN_SERDES("tag", ECS::TagComponent, SerializeTagComponent, DeserializeTagComponent),
        ENJIN_SERDES("teleporter", ECS::TeleporterComponent, SerializeTeleporterComponent, DeserializeTeleporterComponent),
        ENJIN_SERDES("temperatureZone", ECS::TemperatureZoneComponent, SerializeTemperatureZoneComponent, DeserializeTemperatureZoneComponent),
        ENJIN_SERDES("terrain", ECS::TerrainComponent, SerializeTerrainComponent, DeserializeTerrainComponent),
        ENJIN_SERDES("gaussianSplat", ECS::GaussianSplatComponent, SerializeGaussianSplatComponent, DeserializeGaussianSplatComponent),
        ENJIN_SERDES("terrain2d", ECS::Terrain2DComponent, SerializeTerrain2DComponent, DeserializeTerrain2DComponent),
        ENJIN_SERDES("terrainGenerator", ECS::TerrainGeneratorComponent, SerializeTerrainGeneratorComponent, DeserializeTerrainGeneratorComponent),
        ENJIN_SERDES("tether", ECS::TetherComponent, SerializeTetherComponent, DeserializeTetherComponent),
        ENJIN_SERDES("text", ECS::TextComponent, SerializeTextComponent, DeserializeTextComponent),
        ENJIN_SERDES("displayGraphic", ECS::DisplayGraphicComponent, SerializeDisplayGraphicComponent, DeserializeDisplayGraphicComponent),
        ENJIN_SERDES("thirdPerson", ECS::ThirdPersonController, SerializeThirdPerson, DeserializeThirdPerson),
        ENJIN_SERDES("tilemap", ECS::TilemapComponent, SerializeTilemapComponent, DeserializeTilemapComponent),
        ENJIN_SERDES("timer", ECS::TimerComponent, SerializeTimerComponent, DeserializeTimerComponent),
        ENJIN_SERDES("topDown2D", ECS::TopDown2DController, SerializeTopDown2D, DeserializeTopDown2D),
        ENJIN_SERDES("topDown3D", ECS::TopDown3DController, SerializeTopDown3D, DeserializeTopDown3D),
        ENJIN_SERDES("transform", ECS::TransformComponent, SerializeTransformComponent, DeserializeTransformComponent),
        ENJIN_SERDES("treeVolume", ECS::TreeVolumeComponent, SerializeTreeVolumeComponent, DeserializeTreeVolumeComponent),
        ENJIN_SERDES("triggerZone", ECS::TriggerZoneComponent, SerializeTriggerZoneComponent, DeserializeTriggerZoneComponent),
        ENJIN_SERDES("tween", ECS::TweenComponent, SerializeTweenComponent, DeserializeTweenComponent),
        ENJIN_SERDES("uiCanvas", GUI::UICanvasComponent, SerializeUICanvasComponent, DeserializeUICanvasComponent),
        ENJIN_SERDES("vegetation", ECS::VegetationComponent, SerializeVegetationComponent, DeserializeVegetationComponent),
        ENJIN_SERDES("vehicle", ECS::VehicleController, SerializeVehicle, DeserializeVehicle),
        ENJIN_SERDES("viewmodel", ECS::ViewmodelComponent, SerializeViewmodelComponent, DeserializeViewmodelComponent),
        ENJIN_SERDES("virtualCamera", ECS::VirtualCameraComponent, SerializeVirtualCameraComponent, DeserializeVirtualCameraComponent),
        ENJIN_SERDES("visualScript", ECS::VisualScriptComponent, SerializeVisualScriptComponent, DeserializeVisualScriptComponent),
        ENJIN_SERDES("water3D", ECS::Water3DComponent, SerializeWater3DComponent, DeserializeWater3DComponent),
        ENJIN_SERDES("waterInteractor", Effects::WaterInteractorComponent, SerializeWaterInteractorComponent, DeserializeWaterInteractorComponent),
        ENJIN_SERDES("waterVolume", ECS::WaterVolumeComponent, SerializeWaterVolumeComponent, DeserializeWaterVolumeComponent),
        ENJIN_SERDES("waypoint", ECS::WaypointComponent, SerializeWaypointComponent, DeserializeWaypointComponent),
        ENJIN_SERDES("weatherZone", ECS::WeatherZoneComponent, SerializeWeatherZoneComponent, DeserializeWeatherZoneComponent),
        ENJIN_SERDES("wfc", ECS::WFCComponent, SerializeWFCComponent, DeserializeWFCComponent),
        // --- special cases (behavior-identical to the old chains) ---
        ComponentSerdes{ "stableId",
            [](ECS::World* w, ECS::Entity e){ return w->HasComponent<ECS::StableIdComponent>(e); },
            [](ECS::World* w, ECS::Entity e)->json{ json j; j["id"] = w->GetComponent<ECS::StableIdComponent>(e)->id; return j; },
            [](ECS::World* w, ECS::Entity e, const json& j){ w->AddComponent<ECS::StableIdComponent>(e, ECS::StableIdComponent{ j.value("id", u64{0}) }); },
            [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<ECS::StableIdComponent>(e); } },
        ComponentSerdes{ "mesh",
            [](ECS::World* w, ECS::Entity e){ return w->HasComponent<ECS::MeshComponent>(e); },
            [](ECS::World* w, ECS::Entity e)->json{ return SerializeMeshComponent(*w->GetComponent<ECS::MeshComponent>(e), true); },
            [](ECS::World* w, ECS::Entity e, const json& j){ w->AddComponent<ECS::MeshComponent>(e, DeserializeMeshComponent(j)); },
            [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<ECS::MeshComponent>(e); } },
        ComponentSerdes{ "animator",
            [](ECS::World* w, ECS::Entity e){ return w->HasComponent<ECS::AnimatorComponent>(e); },
            [](ECS::World* w, ECS::Entity e)->json{ return SerializeAnimatorComponent(*w->GetComponent<ECS::AnimatorComponent>(e)); },
            [](ECS::World* w, ECS::Entity e, const json& j){
                std::shared_ptr<Animation::Skeleton> skel;
                if (w->HasComponent<ECS::SkeletonComponent>(e)) skel = w->GetComponent<ECS::SkeletonComponent>(e)->skeleton;
                w->AddComponent<ECS::AnimatorComponent>(e, DeserializeAnimatorComponent(j, skel));
            },
            [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<ECS::AnimatorComponent>(e); } },
        ComponentSerdes{ "boneAttachment",
            [](ECS::World* w, ECS::Entity e){ return w->HasComponent<ECS::BoneAttachmentComponent>(e); },
            [](ECS::World* w, ECS::Entity e)->json{
                auto* ba = w->GetComponent<ECS::BoneAttachmentComponent>(e); json j;
                j["targetEntity"] = static_cast<u64>(ba->targetEntity);
                j["targetBoneName"] = ba->targetBoneName;
                j["positionOffset"] = { ba->positionOffset.x, ba->positionOffset.y, ba->positionOffset.z };
                j["rotationOffset"] = { ba->rotationOffset.x, ba->rotationOffset.y, ba->rotationOffset.z, ba->rotationOffset.w };
                return j;
            },
            [](ECS::World* w, ECS::Entity e, const json& j){
                auto& ba = w->AddComponent<ECS::BoneAttachmentComponent>(e);
                if (j.contains("targetEntity")) ba.targetEntity = static_cast<ECS::Entity>(j["targetEntity"].get<u64>());
                if (j.contains("targetBoneName")) ba.targetBoneName = j["targetBoneName"].get<std::string>();
                if (j.contains("positionOffset") && j["positionOffset"].is_array() && j["positionOffset"].size() >= 3) {
                    auto& a = j["positionOffset"]; ba.positionOffset = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>());
                }
                if (j.contains("rotationOffset") && j["rotationOffset"].is_array() && j["rotationOffset"].size() >= 4) {
                    auto& a = j["rotationOffset"]; ba.rotationOffset = Math::Quaternion(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>(), a[3].get<f32>());
                }
            },
            [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<ECS::BoneAttachmentComponent>(e); } },
        // --- absorbed from the scene save/load loops (Stage 2): these were wired
        // only in the loops, so the per-key helpers could not copy/undo them ---
        ENJIN_SERDES("customShader", ECS::CustomShaderComponent, SerializeCustomShaderComponent, DeserializeCustomShaderComponent),
        ENJIN_SERDES("layer", ECS::LayerComponent, SerializeLayerComponent, DeserializeLayerComponent),
        ENJIN_SERDES("cineComponent", ECS::CineComponent, SerializeCineComponent, DeserializeCineComponent),
        // scatterInstance: presence-only marker for procgen-spawned instances.
        ComponentSerdes{ "scatterInstance",
            [](ECS::World* w, ECS::Entity e){ return w->HasComponent<ECS::ScatterInstanceComponent>(e); },
            [](ECS::World* w, ECS::Entity e)->json{ (void)w; (void)e; return json::object(); },
            [](ECS::World* w, ECS::Entity e, const json& j){ (void)j; w->AddComponent<ECS::ScatterInstanceComponent>(e); },
            [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<ECS::ScatterInstanceComponent>(e); } },
        // prefabInstance: persisted link fields only (overrides map is runtime state).
        ComponentSerdes{ "prefabInstance",
            [](ECS::World* w, ECS::Entity e){ return w->HasComponent<Assets::PrefabInstanceComponent>(e); },
            [](ECS::World* w, ECS::Entity e)->json{
                auto* pi = w->GetComponent<Assets::PrefabInstanceComponent>(e); json j;
                j["prefabId"] = pi->prefabId;
                j["prefabPath"] = pi->prefabPath;
                return j;
            },
            [](ECS::World* w, ECS::Entity e, const json& j){
                auto& pi = w->AddComponent<Assets::PrefabInstanceComponent>(e);
                pi.prefabId = j.value("prefabId", static_cast<u64>(0));
                pi.prefabPath = j.value("prefabPath", std::string(""));
            },
            [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<Assets::PrefabInstanceComponent>(e); } },
        ComponentSerdes{ "lookAtIK",
            [](ECS::World* w, ECS::Entity e){ return w->HasComponent<ECS::LookAtIKComponent>(e); },
            [](ECS::World* w, ECS::Entity e)->json{
                auto* ik = w->GetComponent<ECS::LookAtIKComponent>(e); json j;
                j["headBone"] = ik->headBoneName;
                j["neckBone"] = ik->neckBoneName;
                j["targetEntity"] = static_cast<u64>(ik->targetEntity);
                j["targetPos"] = { ik->targetWorldPos.x, ik->targetWorldPos.y, ik->targetWorldPos.z };
                j["useEntityTarget"] = ik->useEntityTarget;
                j["maxRotation"] = ik->maxRotation;
                j["smoothSpeed"] = ik->smoothSpeed;
                j["lookWeight"] = ik->lookWeight;
                return j;
            },
            [](ECS::World* w, ECS::Entity e, const json& ikJson){
                auto& ik = w->AddComponent<ECS::LookAtIKComponent>(e);
                if (ikJson.contains("headBone")) ik.headBoneName = ikJson["headBone"].get<std::string>();
                if (ikJson.contains("neckBone")) ik.neckBoneName = ikJson["neckBone"].get<std::string>();
                if (ikJson.contains("targetEntity")) ik.targetEntity = static_cast<ECS::Entity>(ikJson["targetEntity"].get<u64>());
                if (ikJson.contains("targetPos") && ikJson["targetPos"].is_array() && ikJson["targetPos"].size() >= 3) {
                    auto& arr = ikJson["targetPos"];
                    ik.targetWorldPos = Math::Vector3(arr[0].get<f32>(), arr[1].get<f32>(), arr[2].get<f32>());
                }
                if (ikJson.contains("useEntityTarget")) ik.useEntityTarget = JB(ikJson["useEntityTarget"]);
                if (ikJson.contains("maxRotation")) ik.maxRotation = ikJson["maxRotation"].get<f32>();
                if (ikJson.contains("smoothSpeed")) ik.smoothSpeed = ikJson["smoothSpeed"].get<f32>();
                if (ikJson.contains("lookWeight")) ik.lookWeight = ikJson["lookWeight"].get<f32>();
            },
            [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<ECS::LookAtIKComponent>(e); } },
        ComponentSerdes{ "interactionIK",
            [](ECS::World* w, ECS::Entity e){ return w->HasComponent<ECS::InteractionIKComponent>(e); },
            [](ECS::World* w, ECS::Entity e)->json{
                auto* ik = w->GetComponent<ECS::InteractionIKComponent>(e); json j;
                j["handBone"] = ik->handBoneName;
                j["elbowBone"] = ik->elbowBoneName;
                j["shoulderBone"] = ik->shoulderBoneName;
                j["interactionRadius"] = ik->interactionRadius;
                j["ikWeight"] = ik->ikWeight;
                j["smoothSpeed"] = ik->smoothSpeed;
                j["interactionTag"] = ik->interactionTag;
                return j;
            },
            [](ECS::World* w, ECS::Entity e, const json& ikJson){
                auto& ik = w->AddComponent<ECS::InteractionIKComponent>(e);
                if (ikJson.contains("handBone")) ik.handBoneName = ikJson["handBone"].get<std::string>();
                if (ikJson.contains("elbowBone")) ik.elbowBoneName = ikJson["elbowBone"].get<std::string>();
                if (ikJson.contains("shoulderBone")) ik.shoulderBoneName = ikJson["shoulderBone"].get<std::string>();
                if (ikJson.contains("interactionRadius")) ik.interactionRadius = ikJson["interactionRadius"].get<f32>();
                if (ikJson.contains("ikWeight")) ik.ikWeight = ikJson["ikWeight"].get<f32>();
                if (ikJson.contains("smoothSpeed")) ik.smoothSpeed = ikJson["smoothSpeed"].get<f32>();
                if (ikJson.contains("interactionTag")) ik.interactionTag = ikJson["interactionTag"].get<std::string>();
            },
            [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<ECS::InteractionIKComponent>(e); } },
        ComponentSerdes{ "twoBoneIK",
            [](ECS::World* w, ECS::Entity e){ return w->HasComponent<ECS::TwoBoneIKComponent>(e); },
            [](ECS::World* w, ECS::Entity e)->json{
                auto* ik = w->GetComponent<ECS::TwoBoneIKComponent>(e); json j;
                j["rootBone"] = ik->rootBoneName;
                j["midBone"] = ik->midBoneName;
                j["endBone"] = ik->endBoneName;
                j["targetPos"] = { ik->targetPosition.x, ik->targetPosition.y, ik->targetPosition.z };
                j["targetEntity"] = static_cast<u64>(ik->targetEntity);
                j["useEntityTarget"] = ik->useEntityTarget;
                j["weight"] = ik->weight;
                j["poleVector"] = { ik->poleVector.x, ik->poleVector.y, ik->poleVector.z };
                return j;
            },
            [](ECS::World* w, ECS::Entity e, const json& ikJson){
                auto& ik = w->AddComponent<ECS::TwoBoneIKComponent>(e);
                if (ikJson.contains("rootBone")) ik.rootBoneName = ikJson["rootBone"].get<std::string>();
                if (ikJson.contains("midBone")) ik.midBoneName = ikJson["midBone"].get<std::string>();
                if (ikJson.contains("endBone")) ik.endBoneName = ikJson["endBone"].get<std::string>();
                if (ikJson.contains("targetPos") && ikJson["targetPos"].is_array() && ikJson["targetPos"].size() >= 3) {
                    auto& a = ikJson["targetPos"];
                    ik.targetPosition = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>());
                }
                if (ikJson.contains("targetEntity")) ik.targetEntity = static_cast<ECS::Entity>(ikJson["targetEntity"].get<u64>());
                if (ikJson.contains("useEntityTarget")) ik.useEntityTarget = JB(ikJson["useEntityTarget"]);
                if (ikJson.contains("weight")) ik.weight = ikJson["weight"].get<f32>();
                if (ikJson.contains("poleVector") && ikJson["poleVector"].is_array() && ikJson["poleVector"].size() >= 3) {
                    auto& a = ikJson["poleVector"];
                    ik.poleVector = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>());
                }
            },
            [](ECS::World* w, ECS::Entity e){ w->RemoveComponent<ECS::TwoBoneIKComponent>(e); } },
    };
    return reg;
}

SerializationResult SceneSerializer::SaveEntities(const std::string& filepath, const std::vector<ECS::Entity>& entities, const SerializationOptions& options) {
    if (!m_World) {
        SerializationResult result;
        result.success = false;
        result.error = "No world set";
        result.filepath = filepath;
        return result;
    }

    if (filepath.empty()) {
        SerializationResult result;
        result.success = false;
        result.error = "Cannot save: file path is empty";
        result.filepath = filepath;
        return result;
    }

    // Reject path traversal in save paths (symmetric with Load)
    auto normalized = std::filesystem::path(filepath).lexically_normal().string();
    if (normalized.find("..") != std::string::npos) {
        SerializationResult result;
        result.success = false;
        result.error = "Path traversal rejected: " + filepath;
        result.filepath = filepath;
        return result;
    }

    // Ensure parent directory exists
    auto parentDir = std::filesystem::path(filepath).parent_path();
    if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
        std::error_code ec;
        std::filesystem::create_directories(parentDir, ec);
        if (ec) {
            SerializationResult result;
            result.success = false;
            result.error = "Failed to create directory: " + parentDir.string() + " (" + ec.message() + ")";
            result.filepath = filepath;
            return result;
        }
    }

    try {
        json sceneJson;
        sceneJson["formatVersion"] = SCENE_FORMAT_VERSION;
        sceneJson["version"] = "1.0";
        sceneJson["entityCount"] = static_cast<u32>(entities.size());

        json entitiesArray = json::array();

        // Sort entities by ID for deterministic output
        std::vector<ECS::Entity> sortedEntities(entities.begin(), entities.end());
        if (options.deterministic) {
            std::sort(sortedEntities.begin(), sortedEntities.end());
        }

        for (ECS::Entity entity : sortedEntities) {
            if (!m_World->IsValid(entity)) {
                continue;
            }
            // Runtime-spawned junk (generated colliders, pooled spawns) never
            // belongs in a scene file - a mid-play save must not leak it.
            if (m_World->HasComponent<ECS::TransientComponent>(entity)) {
                continue;
            }

            json entityJson;
            entityJson["id"] = static_cast<u64>(entity);

            // Durable scene-authoring identity (see StableId.h). Assigned here if
            // missing so every persisted entity carries a stable address for the
            // override-layer system. The runtime "id" above is remapped on load;
            // "stableId" survives reload unchanged.
            if (!m_World->HasComponent<ECS::StableIdComponent>(entity)) {
                m_World->AddComponent<ECS::StableIdComponent>(entity, ECS::StableIdComponent{ ECS::GenerateStableId() });
            }
            entityJson["stableId"] = m_World->GetComponent<ECS::StableIdComponent>(entity)->id;

            // Serialize components
                        // Serialize components via the registry (single source of truth).
            // mesh/morphTargets stay explicit below (they honor vertex-data options);
            // stableId/id/parent are entity-level and written outside the registry.
            for (const auto& reg : ComponentRegistry()) {
                std::string_view rk(reg.key);
                if (rk == "mesh" || rk == "morphTargets" || rk == "stableId") continue;
                if (reg.has(m_World, entity)) entityJson[reg.key] = reg.ser(m_World, entity);
            }
            if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
                const auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
                entityJson["mesh"] = SerializeMeshComponent(*mesh, options.includeVertexData, options.useMeshReferences);
            }
            if (m_World->HasComponent<ECS::ParentComponent>(entity)) {
                entityJson["parent"] = static_cast<u64>(m_World->GetComponent<ECS::ParentComponent>(entity)->parent);
            }
            if (m_World->HasComponent<ECS::MorphTargetComponent>(entity)) {
                entityJson["morphTargets"] = SerializeMorphTargetComponent(*m_World->GetComponent<ECS::MorphTargetComponent>(entity), options.includeVertexData);
            }


            entitiesArray.push_back(entityJson);
        }

        sceneJson["entities"] = entitiesArray;

        // Serialize accessibility content flags
        if (static_cast<u32>(m_ContentFlags.flags) != 0 || !m_ContentFlags.customWarnings.empty()) {
            sceneJson["accessibility"] = SerializeContentFlags(m_ContentFlags);
        }

        // Serialize skybox configuration
        if (m_SkyboxConfig.type != Renderer::SkyboxType::None) {
            json skyboxJson;
            skyboxJson["type"] = static_cast<u32>(m_SkyboxConfig.type);
            skyboxJson["topColor"] = { RF(m_SkyboxConfig.topColor.x), RF(m_SkyboxConfig.topColor.y), RF(m_SkyboxConfig.topColor.z) };
            skyboxJson["bottomColor"] = { RF(m_SkyboxConfig.bottomColor.x), RF(m_SkyboxConfig.bottomColor.y), RF(m_SkyboxConfig.bottomColor.z) };
            skyboxJson["horizonColor"] = { RF(m_SkyboxConfig.horizonColor.x), RF(m_SkyboxConfig.horizonColor.y), RF(m_SkyboxConfig.horizonColor.z) };
            skyboxJson["solidColor"] = { RF(m_SkyboxConfig.solidColor.x), RF(m_SkyboxConfig.solidColor.y), RF(m_SkyboxConfig.solidColor.z) };
            skyboxJson["rotation"] = RF(m_SkyboxConfig.rotation);
            skyboxJson["sunDirection"] = { RF(m_SkyboxConfig.sunDirection.x), RF(m_SkyboxConfig.sunDirection.y), RF(m_SkyboxConfig.sunDirection.z) };
            skyboxJson["sunIntensity"] = RF(m_SkyboxConfig.sunIntensity);
            skyboxJson["sunSize"] = RF(m_SkyboxConfig.sunSize);
            skyboxJson["sunColor"] = { RF(m_SkyboxConfig.sunColor.x), RF(m_SkyboxConfig.sunColor.y), RF(m_SkyboxConfig.sunColor.z) };
            skyboxJson["cloudCoverage"] = RF(m_SkyboxConfig.cloudCoverage);
            skyboxJson["cloudScale"] = RF(m_SkyboxConfig.cloudScale);
            skyboxJson["cloudSpeed"] = RF(m_SkyboxConfig.cloudSpeed);
            skyboxJson["cloudColor"] = { RF(m_SkyboxConfig.cloudColor.x), RF(m_SkyboxConfig.cloudColor.y), RF(m_SkyboxConfig.cloudColor.z) };
            skyboxJson["cloud2Coverage"] = RF(m_SkyboxConfig.cloud2Coverage);
            skyboxJson["cloud2Scale"] = RF(m_SkyboxConfig.cloud2Scale);
            skyboxJson["horizonHaze"] = RF(m_SkyboxConfig.horizonHaze);
            skyboxJson["cloudShadowStrength"] = RF(m_SkyboxConfig.cloudShadowStrength);
            skyboxJson["cloudSoftness"] = RF(m_SkyboxConfig.cloudSoftness);
            if (!m_SkyboxConfig.cloudTexturePath.empty()) skyboxJson["cloudTexturePath"] = m_SkyboxConfig.cloudTexturePath;
            json faces = json::array();
            for (const auto& p : m_SkyboxConfig.cubemapPaths) faces.push_back(p);
            skyboxJson["cubemapPaths"] = faces;
            sceneJson["skybox"] = skyboxJson;
        }

        // Serialize 2D water (scene-level, only when enabled so old scenes stay clean)
        if (m_Water2DConfig.enabled) {
            const auto& wc = m_Water2DConfig;
            json wj;
            wj["enabled"] = true;
            wj["waterLineY"] = RF(wc.waterLineY);
            wj["surfaceColor"] = { RF(wc.surfaceColor.x), RF(wc.surfaceColor.y), RF(wc.surfaceColor.z) };
            wj["deepColor"] = { RF(wc.deepColor.x), RF(wc.deepColor.y), RF(wc.deepColor.z) };
            wj["opacity"] = RF(wc.opacity);
            wj["depthFalloff"] = RF(wc.depthFalloff);
            wj["waveAmplitude"] = RF(wc.waveAmplitude);
            wj["waveLength"] = RF(wc.waveLength);
            wj["waveSpeed"] = RF(wc.waveSpeed);
            wj["foamColor"] = { RF(wc.foamColor.x), RF(wc.foamColor.y), RF(wc.foamColor.z) };
            wj["foamWidth"] = RF(wc.foamWidth);
            wj["causticStrength"] = RF(wc.causticStrength);
            sceneJson["water2d"] = wj;
        }

        // Serialize render settings
        sceneJson["renderSettings"] = Renderer::SerializeRenderSettings(m_RenderSettings);

        // Atomic file save: write to temp file, then rename
        std::string tmpPath = filepath + ".tmp";
        {
            std::ofstream file(tmpPath);
            if (!file.is_open()) {
                SerializationResult result;
                result.success = false;
                result.error = "Failed to open temp file for writing: " + tmpPath;
                result.filepath = filepath;
                return result;
            }

            if (options.prettyPrint) {
                file << sceneJson.dump(static_cast<int>(options.indentSize));
            } else {
                file << sceneJson.dump();
            }

            file.close();

            if (!file.good()) {
                std::error_code ec;
                std::filesystem::remove(tmpPath, ec);
                SerializationResult result;
                result.success = false;
                result.error = "Write failed for temp file: " + tmpPath;
                result.filepath = filepath;
                return result;
            }
        }

        {
            std::error_code ec;
            std::filesystem::rename(tmpPath, filepath, ec);
            if (ec) {
                std::filesystem::remove(tmpPath, ec);
                SerializationResult result;
                result.success = false;
                result.error = "Failed to rename temp file to target: " + ec.message();
                result.filepath = filepath;
                return result;
            }
        }

        ENJIN_LOG_INFO(Asset, "Saved scene to %s (%zu entities)", filepath.c_str(), entities.size());

        SerializationResult result;
        result.success = true;
        result.filepath = filepath;
        return result;

    } catch (const std::exception& e) {
        SerializationResult result;
        result.success = false;
        result.error = std::string("JSON serialization error: ") + e.what();
        result.filepath = filepath;
        return result;
    }
}

DeserializationResult SceneSerializer::Load(const std::string& filepath, bool clearExisting) {
    if (!m_World) {
        DeserializationResult result;
        result.success = false;
        result.error = "No world set";
        result.filepath = filepath;
        return result;
    }

    if (clearExisting) {
        m_World->Clear();
    }

    return LoadAdditive(filepath);
}

DeserializationResult SceneSerializer::LoadAdditive(const std::string& filepath) {
    if (!m_World) {
        DeserializationResult result;
        result.success = false;
        result.error = "No world set";
        result.filepath = filepath;
        return result;
    }

    DeserializationResult result;
    result.filepath = filepath;

    // SN-C4: Reject directory traversal in additive load paths
    {
        auto normalized = std::filesystem::path(filepath).lexically_normal().string();
        if (normalized.find("..") != std::string::npos) {
            result.error = "Path traversal rejected: " + filepath;
            ENJIN_LOG_ERROR(Asset, "LoadAdditive: %s", result.error.c_str());
            return result;
        }
    }

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            result.error = "Failed to open file: " + filepath;
            return result;
        }

        json sceneJson;
        file >> sceneJson;
        file.close();

        // Read format version and apply migrations
        u32 formatVersion = sceneJson.value("formatVersion", u32(0));
        if (formatVersion == 0) {
            ENJIN_LOG_WARN(Asset, "Loading legacy scene file (no format version): %s", filepath.c_str());
        } else if (formatVersion > SCENE_FORMAT_VERSION) {
            ENJIN_LOG_ERROR(Asset, "Scene file version %u is newer than engine version %u: %s",
                            formatVersion, SCENE_FORMAT_VERSION, filepath.c_str());
        }
        if (formatVersion < SCENE_FORMAT_VERSION) {
            MigrateScene(sceneJson, formatVersion);
        }

        // Deserialize skybox configuration (file-based load)
        if (sceneJson.contains("skybox")) {
            const auto& sj = sceneJson["skybox"];
            m_SkyboxConfig = Renderer::SkyboxConfig{};
            if (sj.contains("type")) { u32 v = sj["type"].get<u32>(); if (v <= static_cast<u32>(Renderer::SkyboxType::SolidColor)) m_SkyboxConfig.type = static_cast<Renderer::SkyboxType>(v); }
            if (sj.contains("topColor") && sj["topColor"].is_array() && sj["topColor"].size() >= 3) { auto& a = sj["topColor"]; m_SkyboxConfig.topColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("bottomColor") && sj["bottomColor"].is_array() && sj["bottomColor"].size() >= 3) { auto& a = sj["bottomColor"]; m_SkyboxConfig.bottomColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("horizonColor") && sj["horizonColor"].is_array() && sj["horizonColor"].size() >= 3) { auto& a = sj["horizonColor"]; m_SkyboxConfig.horizonColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("solidColor") && sj["solidColor"].is_array() && sj["solidColor"].size() >= 3) { auto& a = sj["solidColor"]; m_SkyboxConfig.solidColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("rotation")) m_SkyboxConfig.rotation = sj["rotation"].get<f32>();
            if (sj.contains("sunDirection") && sj["sunDirection"].is_array() && sj["sunDirection"].size() >= 3) { auto& a = sj["sunDirection"]; m_SkyboxConfig.sunDirection = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("sunIntensity")) m_SkyboxConfig.sunIntensity = sj["sunIntensity"].get<f32>();
            if (sj.contains("sunSize")) m_SkyboxConfig.sunSize = sj["sunSize"].get<f32>();
            if (sj.contains("sunColor") && sj["sunColor"].is_array() && sj["sunColor"].size() >= 3) { auto& a = sj["sunColor"]; m_SkyboxConfig.sunColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("cloudCoverage")) m_SkyboxConfig.cloudCoverage = sj["cloudCoverage"].get<f32>();
            if (sj.contains("cloudScale")) m_SkyboxConfig.cloudScale = sj["cloudScale"].get<f32>();
            if (sj.contains("cloudSpeed")) m_SkyboxConfig.cloudSpeed = sj["cloudSpeed"].get<f32>();
            if (sj.contains("cloudColor") && sj["cloudColor"].is_array() && sj["cloudColor"].size() >= 3) { auto& a = sj["cloudColor"]; m_SkyboxConfig.cloudColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("cloud2Coverage")) m_SkyboxConfig.cloud2Coverage = sj["cloud2Coverage"].get<f32>();
            if (sj.contains("cloud2Scale")) m_SkyboxConfig.cloud2Scale = sj["cloud2Scale"].get<f32>();
            if (sj.contains("horizonHaze")) m_SkyboxConfig.horizonHaze = sj["horizonHaze"].get<f32>();
            if (sj.contains("cloudShadowStrength")) m_SkyboxConfig.cloudShadowStrength = sj["cloudShadowStrength"].get<f32>();
            if (sj.contains("cloudSoftness")) m_SkyboxConfig.cloudSoftness = sj["cloudSoftness"].get<f32>();
            if (sj.contains("cloudTexturePath")) m_SkyboxConfig.cloudTexturePath = SafeStr(sj["cloudTexturePath"], MAX_STR_PATH);
            if (sj.contains("cubemapPaths") && sj["cubemapPaths"].is_array()) {
                for (usize i = 0; i < 6 && i < sj["cubemapPaths"].size(); ++i) {
                    m_SkyboxConfig.cubemapPaths[i] = SafeStr(sj["cubemapPaths"][i], MAX_STR_PATH);
                }
            }
        } else {
            m_SkyboxConfig = Renderer::SkyboxConfig{};
        }

        // Deserialize 2D water configuration
        m_Water2DConfig = Renderer::Water2DConfig{};
        if (sceneJson.contains("water2d") && sceneJson["water2d"].is_object()) {
            const auto& wj = sceneJson["water2d"];
            m_Water2DConfig.enabled = wj.value("enabled", false);
            if (wj.contains("waterLineY")) m_Water2DConfig.waterLineY = wj["waterLineY"].get<f32>();
            if (wj.contains("surfaceColor") && wj["surfaceColor"].is_array() && wj["surfaceColor"].size() >= 3) { auto& a = wj["surfaceColor"]; m_Water2DConfig.surfaceColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (wj.contains("deepColor") && wj["deepColor"].is_array() && wj["deepColor"].size() >= 3) { auto& a = wj["deepColor"]; m_Water2DConfig.deepColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (wj.contains("opacity")) m_Water2DConfig.opacity = wj["opacity"].get<f32>();
            if (wj.contains("depthFalloff")) m_Water2DConfig.depthFalloff = wj["depthFalloff"].get<f32>();
            if (wj.contains("waveAmplitude")) m_Water2DConfig.waveAmplitude = wj["waveAmplitude"].get<f32>();
            if (wj.contains("waveLength")) m_Water2DConfig.waveLength = wj["waveLength"].get<f32>();
            if (wj.contains("waveSpeed")) m_Water2DConfig.waveSpeed = wj["waveSpeed"].get<f32>();
            if (wj.contains("foamColor") && wj["foamColor"].is_array() && wj["foamColor"].size() >= 3) { auto& a = wj["foamColor"]; m_Water2DConfig.foamColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (wj.contains("foamWidth")) m_Water2DConfig.foamWidth = wj["foamWidth"].get<f32>();
            if (wj.contains("causticStrength")) m_Water2DConfig.causticStrength = wj["causticStrength"].get<f32>();
        }

        // Deserialize render settings
        if (sceneJson.contains("renderSettings")) {
            m_RenderSettings = Renderer::DeserializeRenderSettings(sceneJson["renderSettings"]);
        } else {
            m_RenderSettings = Renderer::SceneRenderSettings{};
        }

        // Check version
        std::string version = sceneJson.value("version", "1.0");
        if (version != "1.0") {
            ENJIN_LOG_WARN(Asset, "Scene file version %s may not be fully compatible", version.c_str());
        }

        // Deserialize accessibility content flags
        if (sceneJson.contains("accessibility")) {
            m_ContentFlags = DeserializeContentFlags(sceneJson["accessibility"]);
        } else {
            m_ContentFlags = Accessibility::SceneContentFlags{};
        }

        // Deserialize entities
        if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
            result.error = "Invalid scene format: missing entities array";
            return result;
        }

        // Map old entity IDs -> new entity IDs for remapping references
        std::unordered_map<u64, ECS::Entity> oldToNew;

        // Recognized entity-level keys, so we can warn about unknown/mistyped
        // ones. Unknown keys are silently ignored on load and erased on the next
        // save (e.g. "script" instead of "scriptComponent"), an invisible way to
        // lose authored data. Set = registry keys + the entity-level keys handled
        // outside the registry below.
        std::unordered_set<std::string> knownEntityKeys = {
            "id", "stableId", "mesh", "parent", "morphTargets"
        };
        for (const auto& reg : ComponentRegistry()) knownEntityKeys.insert(reg.key);
        constexpr usize kMaxLoadWarnings = 25;

        for (const auto& entityJson : sceneJson["entities"]) {
            ECS::Entity entity = m_World->CreateEntity();
            result.entities.push_back(entity);

            // Track old-to-new ID mapping for hierarchy/reference remapping
            if (entityJson.contains("id")) {
                u64 oldId = entityJson["id"].get<u64>();
                oldToNew[oldId] = entity;
            }

            // Restore the durable scene-authoring identity. Legacy scenes saved
            // before stableId existed get a fresh one backfilled here; the scene
            // then persists it on next save (a visible, one-time VCS diff).
            {
                u64 stableId = (entityJson.contains("stableId") && entityJson["stableId"].is_number_unsigned())
                                   ? entityJson["stableId"].get<u64>()
                                   : ECS::GenerateStableId();
                m_World->AddComponent<ECS::StableIdComponent>(entity, ECS::StableIdComponent{ stableId });
            }

            // Set root entity to first entity
            if (result.rootEntity == ECS::INVALID_ENTITY) {
                result.rootEntity = entity;
            }

            // Deserialize components
                        // Deserialize components via the registry (single source of truth).
            // mesh stays explicit below (IsValid guard); stableId/id/parent are
            // entity-level and handled outside the registry.
            for (const auto& reg : ComponentRegistry()) {
                std::string_view rk(reg.key);
                if (rk == "mesh" || rk == "stableId") continue;
                auto regIt = entityJson.find(reg.key);
                if (regIt != entityJson.end()) reg.de(m_World, entity, *regIt);
            }
            if (entityJson.contains("mesh")) {
                auto mesh = DeserializeMeshComponent(entityJson["mesh"]);
                if (mesh.IsValid()) {
                    m_World->AddComponent<ECS::MeshComponent>(entity, mesh);
                }
            }
            if (entityJson.contains("parent")) {
                auto& pc = m_World->AddComponent<ECS::ParentComponent>(entity);
                pc.parent = static_cast<ECS::Entity>(entityJson["parent"].get<u64>());
            }

            // Warn about unknown/mistyped entity keys that were silently dropped.
            if (entityJson.is_object() && result.warnings.size() < kMaxLoadWarnings) {
                std::string entName;
                if (auto nit = entityJson.find("name"); nit != entityJson.end() && nit->is_string())
                    entName = nit->get<std::string>();
                for (auto kit = entityJson.begin(); kit != entityJson.end(); ++kit) {
                    if (result.warnings.size() >= kMaxLoadWarnings) break;
                    const std::string& key = kit.key();
                    if (knownEntityKeys.count(key)) continue;
                    // Suggest a near-miss key (e.g. script -> scriptComponent).
                    std::string suggestion;
                    for (const std::string& known : knownEntityKeys) {
                        if (known == key + "Component" || key == known + "Component" ||
                            (known.size() > key.size() && known.rfind(key, 0) == 0)) {
                            suggestion = known;
                            break;
                        }
                    }
                    std::string who = entName.empty()
                        ? ("entity " + std::to_string(static_cast<u64>(entity)))
                        : ("'" + entName + "'");
                    std::string msg = "Unknown key '" + key + "' on " + who + " was ignored";
                    if (!suggestion.empty()) msg += " (did you mean '" + suggestion + "'?)";
                    result.warnings.push_back(std::move(msg));
                }
            }

        }

        // Remap entity references (parent, IK target) from old IDs to new IDs
        for (ECS::Entity entity : result.entities) {
            if (m_World->HasComponent<ECS::ParentComponent>(entity)) {
                auto* pc = m_World->GetComponent<ECS::ParentComponent>(entity);
                auto it = oldToNew.find(static_cast<u64>(pc->parent));
                if (it != oldToNew.end()) {
                    pc->parent = it->second;
                } else {
                    pc->parent = ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::LookAtIKComponent>(entity)) {
                auto* ik = m_World->GetComponent<ECS::LookAtIKComponent>(entity);
                if (ik->useEntityTarget && ik->targetEntity != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(ik->targetEntity));
                    ik->targetEntity = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::TwoBoneIKComponent>(entity)) {
                auto* ik = m_World->GetComponent<ECS::TwoBoneIKComponent>(entity);
                if (ik->useEntityTarget && ik->targetEntity != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(ik->targetEntity));
                    ik->targetEntity = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::TetherComponent>(entity)) {
                auto* tc = m_World->GetComponent<ECS::TetherComponent>(entity);
                if (tc->stemEntity != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(tc->stemEntity));
                    tc->stemEntity = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
                if (tc->connectedEntity != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(tc->connectedEntity));
                    tc->connectedEntity = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::FollowTargetComponent>(entity)) {
                auto* ft = m_World->GetComponent<ECS::FollowTargetComponent>(entity);
                if (ft->target != 0 && ft->target != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(ft->target));
                    ft->target = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::LookAtTargetComponent>(entity)) {
                auto* la = m_World->GetComponent<ECS::LookAtTargetComponent>(entity);
                if (la->target != 0 && la->target != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(la->target));
                    la->target = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::VirtualCameraComponent>(entity)) {
                auto* vc = m_World->GetComponent<ECS::VirtualCameraComponent>(entity);
                if (vc->follow != 0 && vc->follow != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(vc->follow));
                    vc->follow = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
                if (vc->lookAt != 0 && vc->lookAt != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(vc->lookAt));
                    vc->lookAt = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
        }

        // Rebuild ChildrenComponent from ParentComponent references
        for (ECS::Entity entity : result.entities) {
            if (m_World->HasComponent<ECS::ParentComponent>(entity)) {
                ECS::Entity parent = m_World->GetComponent<ECS::ParentComponent>(entity)->parent;
                if (parent != ECS::INVALID_ENTITY && m_World->IsValid(parent)) {
                    if (!m_World->HasComponent<ECS::ChildrenComponent>(parent)) {
                        m_World->AddComponent<ECS::ChildrenComponent>(parent);
                    }
                    m_World->GetComponent<ECS::ChildrenComponent>(parent)->children.push_back(entity);
                }
            }
        }

        // Re-share one Skeleton per imported-model group so co-skeleton meshes (body + joints)
        // resolve to a single animator after reload (see ReshareSkeletonGroups).
        ReshareSkeletonGroups(m_World);

        // UI unification: convert legacy hudWidget components to UICanvases
        MigrateHUDWidgetsToCanvases(m_World);

        result.success = true;
        ENJIN_LOG_INFO(Asset, "Loaded scene from %s (%zu entities)", filepath.c_str(), result.entities.size());

    } catch (const std::exception& e) {
        result.error = std::string("JSON parsing error: ") + e.what();
    }

    return result;
}

std::string SceneSerializer::SaveToString(const SerializationOptions& options) {
    if (!m_World) {
        return "";
    }

    try {
        json sceneJson;
        sceneJson["formatVersion"] = SCENE_FORMAT_VERSION;
        sceneJson["version"] = "1.0";
        const auto& entities = m_World->GetAllEntities();
        sceneJson["entityCount"] = static_cast<u32>(entities.size());

        json entitiesArray = json::array();

        // Sort entities by ID for deterministic output
        std::vector<ECS::Entity> sortedEntities(entities.begin(), entities.end());
        if (options.deterministic) {
            std::sort(sortedEntities.begin(), sortedEntities.end());
        }

        for (ECS::Entity entity : sortedEntities) {
            if (!m_World->IsValid(entity)) {
                continue;
            }
            // Runtime-spawned junk (generated colliders, pooled spawns) never
            // belongs in a scene file - a mid-play save must not leak it.
            if (m_World->HasComponent<ECS::TransientComponent>(entity)) {
                continue;
            }

            json entityJson;
            entityJson["id"] = static_cast<u64>(entity);

            // Durable scene-authoring identity (see StableId.h). Assigned here if
            // missing so every persisted entity carries a stable address for the
            // override-layer system. The runtime "id" above is remapped on load;
            // "stableId" survives reload unchanged.
            if (!m_World->HasComponent<ECS::StableIdComponent>(entity)) {
                m_World->AddComponent<ECS::StableIdComponent>(entity, ECS::StableIdComponent{ ECS::GenerateStableId() });
            }
            entityJson["stableId"] = m_World->GetComponent<ECS::StableIdComponent>(entity)->id;

            // Serialize components
                        // Serialize components via the registry (single source of truth).
            // mesh/morphTargets stay explicit below (they honor vertex-data options);
            // stableId/id/parent are entity-level and written outside the registry.
            for (const auto& reg : ComponentRegistry()) {
                std::string_view rk(reg.key);
                if (rk == "mesh" || rk == "morphTargets" || rk == "stableId") continue;
                if (reg.has(m_World, entity)) entityJson[reg.key] = reg.ser(m_World, entity);
            }
            if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
                const auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
                entityJson["mesh"] = SerializeMeshComponent(*mesh, options.includeVertexData, options.useMeshReferences);
            }
            if (m_World->HasComponent<ECS::ParentComponent>(entity)) {
                entityJson["parent"] = static_cast<u64>(m_World->GetComponent<ECS::ParentComponent>(entity)->parent);
            }
            if (m_World->HasComponent<ECS::MorphTargetComponent>(entity)) {
                entityJson["morphTargets"] = SerializeMorphTargetComponent(*m_World->GetComponent<ECS::MorphTargetComponent>(entity), options.includeVertexData);
            }


            entitiesArray.push_back(entityJson);
        }

        sceneJson["entities"] = entitiesArray;

        // Serialize accessibility content flags
        if (static_cast<u32>(m_ContentFlags.flags) != 0 || !m_ContentFlags.customWarnings.empty()) {
            sceneJson["accessibility"] = SerializeContentFlags(m_ContentFlags);
        }

        // Serialize skybox configuration
        if (m_SkyboxConfig.type != Renderer::SkyboxType::None) {
            json skyboxJson;
            skyboxJson["type"] = static_cast<u32>(m_SkyboxConfig.type);
            skyboxJson["topColor"] = { RF(m_SkyboxConfig.topColor.x), RF(m_SkyboxConfig.topColor.y), RF(m_SkyboxConfig.topColor.z) };
            skyboxJson["bottomColor"] = { RF(m_SkyboxConfig.bottomColor.x), RF(m_SkyboxConfig.bottomColor.y), RF(m_SkyboxConfig.bottomColor.z) };
            skyboxJson["horizonColor"] = { RF(m_SkyboxConfig.horizonColor.x), RF(m_SkyboxConfig.horizonColor.y), RF(m_SkyboxConfig.horizonColor.z) };
            skyboxJson["solidColor"] = { RF(m_SkyboxConfig.solidColor.x), RF(m_SkyboxConfig.solidColor.y), RF(m_SkyboxConfig.solidColor.z) };
            skyboxJson["rotation"] = RF(m_SkyboxConfig.rotation);
            skyboxJson["sunDirection"] = { RF(m_SkyboxConfig.sunDirection.x), RF(m_SkyboxConfig.sunDirection.y), RF(m_SkyboxConfig.sunDirection.z) };
            skyboxJson["sunIntensity"] = RF(m_SkyboxConfig.sunIntensity);
            skyboxJson["sunSize"] = RF(m_SkyboxConfig.sunSize);
            skyboxJson["sunColor"] = { RF(m_SkyboxConfig.sunColor.x), RF(m_SkyboxConfig.sunColor.y), RF(m_SkyboxConfig.sunColor.z) };
            skyboxJson["cloudCoverage"] = RF(m_SkyboxConfig.cloudCoverage);
            skyboxJson["cloudScale"] = RF(m_SkyboxConfig.cloudScale);
            skyboxJson["cloudSpeed"] = RF(m_SkyboxConfig.cloudSpeed);
            skyboxJson["cloudColor"] = { RF(m_SkyboxConfig.cloudColor.x), RF(m_SkyboxConfig.cloudColor.y), RF(m_SkyboxConfig.cloudColor.z) };
            skyboxJson["cloud2Coverage"] = RF(m_SkyboxConfig.cloud2Coverage);
            skyboxJson["cloud2Scale"] = RF(m_SkyboxConfig.cloud2Scale);
            skyboxJson["horizonHaze"] = RF(m_SkyboxConfig.horizonHaze);
            skyboxJson["cloudShadowStrength"] = RF(m_SkyboxConfig.cloudShadowStrength);
            skyboxJson["cloudSoftness"] = RF(m_SkyboxConfig.cloudSoftness);
            if (!m_SkyboxConfig.cloudTexturePath.empty()) skyboxJson["cloudTexturePath"] = m_SkyboxConfig.cloudTexturePath;
            json faces = json::array();
            for (const auto& p : m_SkyboxConfig.cubemapPaths) faces.push_back(p);
            skyboxJson["cubemapPaths"] = faces;
            sceneJson["skybox"] = skyboxJson;
        }

        // Serialize 2D water (scene-level, only when enabled so old scenes stay clean)
        if (m_Water2DConfig.enabled) {
            const auto& wc = m_Water2DConfig;
            json wj;
            wj["enabled"] = true;
            wj["waterLineY"] = RF(wc.waterLineY);
            wj["surfaceColor"] = { RF(wc.surfaceColor.x), RF(wc.surfaceColor.y), RF(wc.surfaceColor.z) };
            wj["deepColor"] = { RF(wc.deepColor.x), RF(wc.deepColor.y), RF(wc.deepColor.z) };
            wj["opacity"] = RF(wc.opacity);
            wj["depthFalloff"] = RF(wc.depthFalloff);
            wj["waveAmplitude"] = RF(wc.waveAmplitude);
            wj["waveLength"] = RF(wc.waveLength);
            wj["waveSpeed"] = RF(wc.waveSpeed);
            wj["foamColor"] = { RF(wc.foamColor.x), RF(wc.foamColor.y), RF(wc.foamColor.z) };
            wj["foamWidth"] = RF(wc.foamWidth);
            wj["causticStrength"] = RF(wc.causticStrength);
            sceneJson["water2d"] = wj;
        }

        // Serialize render settings
        sceneJson["renderSettings"] = Renderer::SerializeRenderSettings(m_RenderSettings);

        if (options.prettyPrint) {
            return sceneJson.dump(static_cast<int>(options.indentSize));
        } else {
            return sceneJson.dump();
        }

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Failed to serialize scene to string: %s", e.what());
        return "";
    }
}

DeserializationResult SceneSerializer::LoadFromString(const std::string& jsonString, bool clearExisting) {
    DeserializationResult result;

    if (!m_World) {
        result.error = "No world set";
        return result;
    }

    if (jsonString.empty()) {
        result.error = "Empty JSON string";
        return result;
    }

    // Parse JSON FIRST â€" only clear the world after we know the JSON is valid
    json sceneJson;
    try {
        sceneJson = ParseSceneJson(jsonString);
    } catch (const std::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
        return result;  // Return without clearing -- scene is untouched
    }

    if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
        result.error = "Invalid scene format: missing entities array";
        return result;  // Return without clearing -- scene is untouched
    }

    {
        u32 fv = sceneJson.value("formatVersion", u32(0));
        if (fv == 0)
            ENJIN_LOG_WARN(Asset, "Loading legacy scene (no formatVersion)");
        else if (fv > SCENE_FORMAT_VERSION)
            ENJIN_LOG_ERROR(Asset, "Scene formatVersion %u > engine %u", fv, SCENE_FORMAT_VERSION);
        if (fv < SCENE_FORMAT_VERSION)
            MigrateScene(sceneJson, fv);
    }

    // Structurally valid, safe to clear world now
    if (clearExisting) {
        m_World->Clear();
    }

    try {
        // Deserialize skybox configuration (string-based load)
        if (sceneJson.contains("skybox")) {
            const auto& sj = sceneJson["skybox"];
            m_SkyboxConfig = Renderer::SkyboxConfig{};
            if (sj.contains("type")) { u32 v = sj["type"].get<u32>(); if (v <= static_cast<u32>(Renderer::SkyboxType::SolidColor)) m_SkyboxConfig.type = static_cast<Renderer::SkyboxType>(v); }
            if (sj.contains("topColor") && sj["topColor"].is_array() && sj["topColor"].size() >= 3) { auto& a = sj["topColor"]; m_SkyboxConfig.topColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("bottomColor") && sj["bottomColor"].is_array() && sj["bottomColor"].size() >= 3) { auto& a = sj["bottomColor"]; m_SkyboxConfig.bottomColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("horizonColor") && sj["horizonColor"].is_array() && sj["horizonColor"].size() >= 3) { auto& a = sj["horizonColor"]; m_SkyboxConfig.horizonColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("solidColor") && sj["solidColor"].is_array() && sj["solidColor"].size() >= 3) { auto& a = sj["solidColor"]; m_SkyboxConfig.solidColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("rotation")) m_SkyboxConfig.rotation = sj["rotation"].get<f32>();
            if (sj.contains("sunDirection") && sj["sunDirection"].is_array() && sj["sunDirection"].size() >= 3) { auto& a = sj["sunDirection"]; m_SkyboxConfig.sunDirection = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("sunIntensity")) m_SkyboxConfig.sunIntensity = sj["sunIntensity"].get<f32>();
            if (sj.contains("sunSize")) m_SkyboxConfig.sunSize = sj["sunSize"].get<f32>();
            if (sj.contains("sunColor") && sj["sunColor"].is_array() && sj["sunColor"].size() >= 3) { auto& a = sj["sunColor"]; m_SkyboxConfig.sunColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("cloudCoverage")) m_SkyboxConfig.cloudCoverage = sj["cloudCoverage"].get<f32>();
            if (sj.contains("cloudScale")) m_SkyboxConfig.cloudScale = sj["cloudScale"].get<f32>();
            if (sj.contains("cloudSpeed")) m_SkyboxConfig.cloudSpeed = sj["cloudSpeed"].get<f32>();
            if (sj.contains("cloudColor") && sj["cloudColor"].is_array() && sj["cloudColor"].size() >= 3) { auto& a = sj["cloudColor"]; m_SkyboxConfig.cloudColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("cloud2Coverage")) m_SkyboxConfig.cloud2Coverage = sj["cloud2Coverage"].get<f32>();
            if (sj.contains("cloud2Scale")) m_SkyboxConfig.cloud2Scale = sj["cloud2Scale"].get<f32>();
            if (sj.contains("horizonHaze")) m_SkyboxConfig.horizonHaze = sj["horizonHaze"].get<f32>();
            if (sj.contains("cloudShadowStrength")) m_SkyboxConfig.cloudShadowStrength = sj["cloudShadowStrength"].get<f32>();
            if (sj.contains("cloudSoftness")) m_SkyboxConfig.cloudSoftness = sj["cloudSoftness"].get<f32>();
            if (sj.contains("cloudTexturePath")) m_SkyboxConfig.cloudTexturePath = SafeStr(sj["cloudTexturePath"], MAX_STR_PATH);
            if (sj.contains("cubemapPaths") && sj["cubemapPaths"].is_array()) {
                for (usize i = 0; i < 6 && i < sj["cubemapPaths"].size(); ++i) {
                    m_SkyboxConfig.cubemapPaths[i] = SafeStr(sj["cubemapPaths"][i], MAX_STR_PATH);
                }
            }
        } else {
            m_SkyboxConfig = Renderer::SkyboxConfig{};
        }

        // Deserialize 2D water configuration
        m_Water2DConfig = Renderer::Water2DConfig{};
        if (sceneJson.contains("water2d") && sceneJson["water2d"].is_object()) {
            const auto& wj = sceneJson["water2d"];
            m_Water2DConfig.enabled = wj.value("enabled", false);
            if (wj.contains("waterLineY")) m_Water2DConfig.waterLineY = wj["waterLineY"].get<f32>();
            if (wj.contains("surfaceColor") && wj["surfaceColor"].is_array() && wj["surfaceColor"].size() >= 3) { auto& a = wj["surfaceColor"]; m_Water2DConfig.surfaceColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (wj.contains("deepColor") && wj["deepColor"].is_array() && wj["deepColor"].size() >= 3) { auto& a = wj["deepColor"]; m_Water2DConfig.deepColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (wj.contains("opacity")) m_Water2DConfig.opacity = wj["opacity"].get<f32>();
            if (wj.contains("depthFalloff")) m_Water2DConfig.depthFalloff = wj["depthFalloff"].get<f32>();
            if (wj.contains("waveAmplitude")) m_Water2DConfig.waveAmplitude = wj["waveAmplitude"].get<f32>();
            if (wj.contains("waveLength")) m_Water2DConfig.waveLength = wj["waveLength"].get<f32>();
            if (wj.contains("waveSpeed")) m_Water2DConfig.waveSpeed = wj["waveSpeed"].get<f32>();
            if (wj.contains("foamColor") && wj["foamColor"].is_array() && wj["foamColor"].size() >= 3) { auto& a = wj["foamColor"]; m_Water2DConfig.foamColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (wj.contains("foamWidth")) m_Water2DConfig.foamWidth = wj["foamWidth"].get<f32>();
            if (wj.contains("causticStrength")) m_Water2DConfig.causticStrength = wj["causticStrength"].get<f32>();
        }

        // Deserialize render settings
        if (sceneJson.contains("renderSettings")) {
            m_RenderSettings = Renderer::DeserializeRenderSettings(sceneJson["renderSettings"]);
        } else {
            m_RenderSettings = Renderer::SceneRenderSettings{};
        }

        // Check version
        std::string version = sceneJson.value("version", "1.0");

        // Deserialize accessibility content flags
        if (sceneJson.contains("accessibility")) {
            m_ContentFlags = DeserializeContentFlags(sceneJson["accessibility"]);
        } else {
            m_ContentFlags = Accessibility::SceneContentFlags{};
        }

        // Map old entity IDs -> new entity IDs for remapping references
        std::unordered_map<u64, ECS::Entity> oldToNew;

        for (const auto& entityJson : sceneJson["entities"]) {
            ECS::Entity entity = m_World->CreateEntity();
            result.entities.push_back(entity);

            // Track old-to-new ID mapping for hierarchy/reference remapping
            if (entityJson.contains("id")) {
                u64 oldId = entityJson["id"].get<u64>();
                oldToNew[oldId] = entity;
            }

            // Restore the durable scene-authoring identity. Legacy scenes saved
            // before stableId existed get a fresh one backfilled here; the scene
            // then persists it on next save (a visible, one-time VCS diff).
            {
                u64 stableId = (entityJson.contains("stableId") && entityJson["stableId"].is_number_unsigned())
                                   ? entityJson["stableId"].get<u64>()
                                   : ECS::GenerateStableId();
                m_World->AddComponent<ECS::StableIdComponent>(entity, ECS::StableIdComponent{ stableId });
            }

            if (result.rootEntity == ECS::INVALID_ENTITY) {
                result.rootEntity = entity;
            }

            // Deserialize components
                        // Deserialize components via the registry (single source of truth).
            // mesh stays explicit below (IsValid guard); stableId/id/parent are
            // entity-level and handled outside the registry.
            for (const auto& reg : ComponentRegistry()) {
                std::string_view rk(reg.key);
                if (rk == "mesh" || rk == "stableId") continue;
                auto regIt = entityJson.find(reg.key);
                if (regIt != entityJson.end()) reg.de(m_World, entity, *regIt);
            }
            if (entityJson.contains("mesh")) {
                auto mesh = DeserializeMeshComponent(entityJson["mesh"]);
                if (mesh.IsValid()) {
                    m_World->AddComponent<ECS::MeshComponent>(entity, mesh);
                }
            }
            if (entityJson.contains("parent")) {
                auto& pc = m_World->AddComponent<ECS::ParentComponent>(entity);
                pc.parent = static_cast<ECS::Entity>(entityJson["parent"].get<u64>());
            }

        }

        // Remap entity references (parent, IK target) from old IDs to new IDs
        for (ECS::Entity entity : result.entities) {
            if (m_World->HasComponent<ECS::ParentComponent>(entity)) {
                auto* pc = m_World->GetComponent<ECS::ParentComponent>(entity);
                auto it = oldToNew.find(static_cast<u64>(pc->parent));
                if (it != oldToNew.end()) {
                    pc->parent = it->second;
                } else {
                    pc->parent = ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::LookAtIKComponent>(entity)) {
                auto* ik = m_World->GetComponent<ECS::LookAtIKComponent>(entity);
                if (ik->useEntityTarget && ik->targetEntity != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(ik->targetEntity));
                    ik->targetEntity = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::TwoBoneIKComponent>(entity)) {
                auto* ik = m_World->GetComponent<ECS::TwoBoneIKComponent>(entity);
                if (ik->useEntityTarget && ik->targetEntity != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(ik->targetEntity));
                    ik->targetEntity = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::TetherComponent>(entity)) {
                auto* tc = m_World->GetComponent<ECS::TetherComponent>(entity);
                if (tc->stemEntity != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(tc->stemEntity));
                    tc->stemEntity = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
                if (tc->connectedEntity != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(tc->connectedEntity));
                    tc->connectedEntity = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::FollowTargetComponent>(entity)) {
                auto* ft = m_World->GetComponent<ECS::FollowTargetComponent>(entity);
                if (ft->target != 0 && ft->target != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(ft->target));
                    ft->target = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::LookAtTargetComponent>(entity)) {
                auto* la = m_World->GetComponent<ECS::LookAtTargetComponent>(entity);
                if (la->target != 0 && la->target != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(la->target));
                    la->target = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
            if (m_World->HasComponent<ECS::VirtualCameraComponent>(entity)) {
                auto* vc = m_World->GetComponent<ECS::VirtualCameraComponent>(entity);
                if (vc->follow != 0 && vc->follow != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(vc->follow));
                    vc->follow = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
                if (vc->lookAt != 0 && vc->lookAt != ECS::INVALID_ENTITY) {
                    auto it = oldToNew.find(static_cast<u64>(vc->lookAt));
                    vc->lookAt = (it != oldToNew.end()) ? it->second : ECS::INVALID_ENTITY;
                }
            }
        }

        // Rebuild ChildrenComponent from ParentComponent references
        for (ECS::Entity entity : result.entities) {
            if (m_World->HasComponent<ECS::ParentComponent>(entity)) {
                ECS::Entity parent = m_World->GetComponent<ECS::ParentComponent>(entity)->parent;
                if (parent != ECS::INVALID_ENTITY && m_World->IsValid(parent)) {
                    if (!m_World->HasComponent<ECS::ChildrenComponent>(parent)) {
                        m_World->AddComponent<ECS::ChildrenComponent>(parent);
                    }
                    m_World->GetComponent<ECS::ChildrenComponent>(parent)->children.push_back(entity);
                }
            }
        }

        // Re-share one Skeleton per imported-model group (see ReshareSkeletonGroups).
        ReshareSkeletonGroups(m_World);

        // UI unification: convert legacy hudWidget components to UICanvases
        MigrateHUDWidgetsToCanvases(m_World);

        result.success = true;
        ENJIN_LOG_DEBUG(Asset, "Loaded scene from string (%zu entities)", result.entities.size());

    } catch (const std::exception& e) {
        result.error = std::string("JSON parsing error: ") + e.what();
    }

    return result;
}

// ============================================================================
// Static per-entity serialization (for undo/redo)
// ============================================================================

std::string SceneSerializer::SerializeEntityToString(ECS::World* world, ECS::Entity entity, bool includeVertexData) {
    if (!world || !world->IsValid(entity)) return "";

    try {
        json entityJson;
        entityJson["id"] = static_cast<u64>(entity);

        // Serialize all components (mirrors SaveEntities loop)
                // Serialize components via the registry (single source of truth).
        // mesh/morphTargets stay explicit below (they honor vertex-data options);
        // stableId/id/parent are entity-level and written outside the registry.
        for (const auto& reg : ComponentRegistry()) {
            std::string_view rk(reg.key);
            if (rk == "mesh" || rk == "morphTargets" || rk == "stableId") continue;
            if (reg.has(world, entity)) entityJson[reg.key] = reg.ser(world, entity);
        }
        if (world->HasComponent<ECS::MeshComponent>(entity))
            entityJson["mesh"] = SerializeMeshComponent(*world->GetComponent<ECS::MeshComponent>(entity), includeVertexData);
        if (world->HasComponent<ECS::ParentComponent>(entity))
            entityJson["parent"] = static_cast<u64>(world->GetComponent<ECS::ParentComponent>(entity)->parent);
        if (world->HasComponent<ECS::MorphTargetComponent>(entity))
            entityJson["morphTargets"] = SerializeMorphTargetComponent(*world->GetComponent<ECS::MorphTargetComponent>(entity), true);


        return entityJson.dump();

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Failed to serialize entity to string: %s", e.what());
        return "";
    }
}

ECS::Entity SceneSerializer::DeserializeEntityFromString(ECS::World* world, const std::string& jsonStr) {
    if (!world || jsonStr.empty()) return ECS::INVALID_ENTITY;

    try {
        json entityJson = ParseSceneJson(jsonStr);
        ECS::Entity entity = world->CreateEntity();

        // Deserialize all components (mirrors LoadFromString entity loop)
                // Deserialize components via the registry (single source of truth).
        // mesh stays explicit below (IsValid guard); stableId/id/parent are
        // entity-level and handled outside the registry.
        for (const auto& reg : ComponentRegistry()) {
            std::string_view rk(reg.key);
            if (rk == "mesh" || rk == "stableId") continue;
            auto regIt = entityJson.find(reg.key);
            if (regIt != entityJson.end()) reg.de(world, entity, *regIt);
        }
        if (entityJson.contains("mesh")) {
            world->AddComponent<ECS::MeshComponent>(entity, DeserializeMeshComponent(entityJson["mesh"]));
        }
        if (entityJson.contains("parent")) {
            auto& pc = world->AddComponent<ECS::ParentComponent>(entity);
            pc.parent = static_cast<ECS::Entity>(entityJson["parent"].get<u64>());
            // Rebuild child relationship if parent exists in world
            if (pc.parent != ECS::INVALID_ENTITY && world->IsValid(pc.parent)) {
                if (!world->HasComponent<ECS::ChildrenComponent>(pc.parent)) {
                    world->AddComponent<ECS::ChildrenComponent>(pc.parent);
                }
                world->GetComponent<ECS::ChildrenComponent>(pc.parent)->children.push_back(entity);
            }
        }


        return entity;

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Failed to deserialize entity from string: %s", e.what());
        return ECS::INVALID_ENTITY;
    }
}

// ============================================================================
// Static per-component serialization (for component remove undo)
// ============================================================================


std::vector<std::string> SceneSerializer::ComponentKeysOn(ECS::World* world, ECS::Entity entity) {
    std::vector<std::string> keys;
    if (!world || !world->IsValid(entity)) return keys;
    for (const auto& r : ComponentRegistry())
        if (r.has(world, entity)) keys.emplace_back(r.key);
    return keys;
}

std::vector<std::string> SceneSerializer::RegisteredComponentKeys() {
    std::vector<std::string> keys;
    for (const auto& r : ComponentRegistry()) keys.emplace_back(r.key);
    return keys;
}

std::string SceneSerializer::SerializeOneComponent(ECS::World* world, ECS::Entity entity, const std::string& key) {
    if (!world || !world->IsValid(entity)) return "";
    for (const auto& r : ComponentRegistry()) {
        if (key == r.key && r.has(world, entity)) {
            try { return r.ser(world, entity).dump(); }
            catch (const std::exception&) { return ""; }
        }
    }
    return "";
}

bool SceneSerializer::DeserializeOneComponent(ECS::World* world, ECS::Entity entity,
                                               const std::string& key, const std::string& jsonStr) {
    if (!world || !world->IsValid(entity) || jsonStr.empty()) return false;
    try {
        json j = ParseSceneJson(jsonStr);
        for (const auto& r : ComponentRegistry()) {
            if (key == r.key) { r.de(world, entity, j); return true; }
        }
    } catch (const std::exception&) { return false; }
    return false;
}

bool SceneSerializer::RemoveOneComponent(ECS::World* world, ECS::Entity entity, const std::string& key) {
    if (!world || !world->IsValid(entity)) return false;
    for (const auto& r : ComponentRegistry()) {
        if (key == r.key) { r.rem(world, entity); return true; }
    }
    return false;
}


// UI unification: legacy HUDWidgetComponents become per-entity UICanvases on
// load. HUDSystem is RETIRED — UICanvas is the one authorable UI system on
// every platform. Entity names are preserved so Scene_FindEntity-based
// scripts keep working; the HUD_* script API drives these canvases now.
// Saved scenes write uiCanvas from here on (hudWidget disappears on save).
void SceneSerializer::MigrateHUDWidgetsToCanvases(ECS::World* world) {
    if (!world) return;
    std::vector<ECS::Entity> hudEntities = world->GetEntitiesWithComponent<ECS::HUDWidgetComponent>();
    usize migrated = 0;
    for (ECS::Entity e : hudEntities) {
        auto* hw = world->GetComponent<ECS::HUDWidgetComponent>(e);
        if (!hw) continue;
        // Copy the widget: adding components below may reallocate storage
        ECS::HUDWidgetComponent w = *hw;

        if (!world->HasComponent<GUI::UICanvasComponent>(e)) {
            world->AddComponent<GUI::UICanvasComponent>(e);
        }
        auto* canvas = world->GetComponent<GUI::UICanvasComponent>(e);
        if (!canvas) continue;
        auto* nameComp = world->GetComponent<ECS::NameComponent>(e);
        canvas->canvasName = nameComp ? nameComp->name : std::string("HUD");
        canvas->visible = w.visible;
        canvas->sortOrder = 100;  // HUD sits above menu canvases

        auto pointAnchor = [&](GUI::UIElement& el, f32 ax, f32 ay, f32 wFrac, f32 hFrac) {
            el.anchor.anchorMin = Math::Vector2(ax, ay);
            el.anchor.anchorMax = Math::Vector2(ax, ay);
            el.anchor.offsetLeft = 0.0f;
            el.anchor.offsetTop = 0.0f;
            el.anchor.offsetRight = wFrac * canvas->designWidth;
            el.anchor.offsetBottom = hFrac * canvas->designHeight;
        };
        auto applyWorldSpace = [&](GUI::UIElement& el) {
            if (w.screenSpace) return;
            el.data.worldSpace = true;
            el.data.worldSourceEntity = w.sourceEntity;
            el.data.worldOffset = w.worldOffset;
            el.data.maxRenderDistance = w.maxRenderDistance;
        };

        using WT = ECS::HUDWidgetComponent::WidgetType;
        switch (w.type) {
            case WT::HealthBar:
            case WT::ResourceBar: {
                u32 barId = canvas->AddElement(GUI::UIWidgetType::ProgressBar, "bar");
                GUI::UIElement* bar = canvas->GetElement(barId);
                pointAnchor(*bar, w.anchorX, w.anchorY, w.width, w.height);
                bar->focusable = false;
                bar->data.progressValue = w.maxValue > 0.0f ? w.currentValue / w.maxValue : 0.0f;
                bar->data.bindMaxValue = w.maxValue;
                bar->data.progressFillColor = w.fillColor;
                bar->style.bgColor = w.bgColor;
                bar->data.bindField = (w.bindField == "custom") ? std::string("") : w.bindField;
                applyWorldSpace(*bar);
                if (!w.text.empty()) {
                    u32 lblId = canvas->AddElement(GUI::UIWidgetType::Label, "label", barId);
                    GUI::UIElement* lbl = canvas->GetElement(lblId);
                    lbl->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
                    lbl->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
                    lbl->anchor.offsetLeft = lbl->anchor.offsetRight = 0.0f;
                    lbl->anchor.offsetTop = lbl->anchor.offsetBottom = 0.0f;
                    lbl->focusable = false;
                    lbl->data.text = w.text;
                    lbl->style.fontSize = w.fontSize;
                    lbl->style.textColor = w.textColor;
                }
                break;
            }
            case WT::Label:
            case WT::ObjectiveMarker: {
                u32 id = canvas->AddElement(GUI::UIWidgetType::Label, "text");
                GUI::UIElement* el = canvas->GetElement(id);
                if (w.screenSpace) {
                    // Screen labels anchored top-left at the point, generous
                    // wrap box, left/top aligned — mirrors old HUD drawing
                    pointAnchor(*el, w.anchorX, w.anchorY, 0.8f, 0.10f);
                    el->data.textAlignH = 0;
                    el->data.textAlignV = 0;
                } else {
                    // World tags: small box centered on the projected point
                    pointAnchor(*el, w.anchorX, w.anchorY, 0.12f, 0.04f);
                    el->data.textAlignH = 1;
                    el->data.textAlignV = 1;
                }
                el->focusable = false;
                el->data.text = w.text;
                el->style.fontSize = w.fontSize;
                el->style.textColor = w.textColor;
                el->data.bindField = (w.bindField == "custom") ? std::string("") : w.bindField;
                applyWorldSpace(*el);
                break;
            }
            case WT::Crosshair: {
                // Two thin centered panels replace the bespoke crosshair draw
                for (int axis = 0; axis < 2; ++axis) {
                    u32 id = canvas->AddElement(GUI::UIWidgetType::Panel, axis == 0 ? "crossH" : "crossV");
                    GUI::UIElement* el = canvas->GetElement(id);
                    el->anchor.anchorMin = Math::Vector2(0.5f, 0.5f);
                    el->anchor.anchorMax = Math::Vector2(0.5f, 0.5f);
                    el->anchor.offsetLeft   = axis == 0 ? -10.0f : -1.0f;
                    el->anchor.offsetRight  = axis == 0 ?  10.0f :  1.0f;
                    el->anchor.offsetTop    = axis == 0 ?  -1.0f : -10.0f;
                    el->anchor.offsetBottom = axis == 0 ?   1.0f :  10.0f;
                    el->focusable = false;
                    el->style.bgColor = Math::Vector3(1.0f, 1.0f, 1.0f);
                    el->style.bgAlpha = 0.8f;
                }
                break;
            }
            default:
                ENJIN_LOG_WARN(Asset, "HUD widget type %d on entity %llu had no renderer — dropped in UICanvas migration",
                               static_cast<int>(w.type), static_cast<unsigned long long>(e));
                break;
        }
        world->RemoveComponent<ECS::HUDWidgetComponent>(e);
        ++migrated;
    }
    if (migrated > 0) {
        ENJIN_LOG_INFO(Asset, "UI unification: migrated %zu legacy HUD widget(s) to UICanvas", migrated);
    }
}

} // namespace Scene
} // namespace Enjin
