#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/GravityZone.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/Gameplay.h"
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
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/Accessibility/ContentWarning.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

using json = nlohmann::json;

namespace Enjin {
namespace Scene {

// JSON serialization helpers for math types
namespace {

// Round float to 6 decimal places for deterministic serialization
static f32 RF(f32 val) {
    if (std::isnan(val) || std::isinf(val)) return val;
    if (std::fabs(val) < 1e-6f) return 0.0f;
    constexpr f32 mult = 1000000.0f;
    return std::round(val * mult) / mult;
}

// Tolerant bool deserialization — handles numbers (from RF() bug) and booleans
static bool JB(const json& val) {
    if (val.is_boolean()) return val.get<bool>();
    if (val.is_number()) return val.get<double>() != 0.0;
    return false;
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
    j["normalTexturePath"] = material.normalTexturePath;
    j["metallicRoughnessTexturePath"] = material.metallicRoughnessTexturePath;
    j["emissiveTexturePath"] = material.emissiveTexturePath;
    j["doubleSided"] = material.doubleSided;
    j["castShadows"] = material.castShadows;
    j["receiveShadows"] = material.receiveShadows;
    j["alphaMode"] = static_cast<i32>(material.alphaMode);
    j["alphaCutoff"] = RF(material.alphaCutoff);
    // Height/parallax mapping
    j["heightTexturePath"] = material.heightTexturePath;
    j["parallaxScale"] = RF(material.parallaxScale);
    // Retro rendering flags
    j["flatShading"] = material.flatShading;
    j["affineTexturing"] = material.affineTexturing;
    j["vertexSnapping"] = material.vertexSnapping;
    j["stippleTransparency"] = material.stippleTransparency;
    j["uvQuantize"] = material.uvQuantize;
    j["gouraudOnly"] = material.gouraudOnly;
    j["vertexSnapResolution"] = material.vertexSnapResolution;
    j["shadowDitherMode"] = material.shadowDitherMode;
    j["reflectivity"] = material.reflectivity;
    j["fresnelPower"] = material.fresnelPower;
    j["rimLightStrength"] = material.rimLightStrength;
    j["excludeFromCelShading"] = material.excludeFromCelShading;
    return j;
}

json SerializeMeshComponent(const ECS::MeshComponent& mesh, bool includeVertexData) {
    json j;
    j["vertexCount"] = static_cast<u32>(mesh.vertices.size());
    j["indexCount"] = static_cast<u32>(mesh.indices.size());

    if (includeVertexData) {
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
            }
            vertices.push_back(vertex);
        }
        j["vertices"] = vertices;
        j["indices"] = mesh.indices;
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
    j["cullingMask"] = RF(camera.cullingMask);
    return j;
}

// Deserialize components
ECS::NameComponent DeserializeNameComponent(const json& j) {
    ECS::NameComponent name;
    name.name = j["name"].get<std::string>();
    return name;
}

ECS::TransformComponent DeserializeTransformComponent(const json& j) {
    ECS::TransformComponent transform;
    transform.position = DeserializeVector3(j["position"]);
    transform.rotation = DeserializeQuaternion(j["rotation"]);
    transform.scale = DeserializeVector3(j["scale"]);
    if (j.contains("visible")) transform.visible = j["visible"].get<bool>();
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
    if (j.contains("baseColorTexturePath")) {
        material.baseColorTexturePath = j["baseColorTexturePath"].get<std::string>();
    }
    if (j.contains("normalTexturePath")) {
        material.normalTexturePath = j["normalTexturePath"].get<std::string>();
    }
    if (j.contains("metallicRoughnessTexturePath")) {
        material.metallicRoughnessTexturePath = j["metallicRoughnessTexturePath"].get<std::string>();
    }
    if (j.contains("emissiveTexturePath")) {
        material.emissiveTexturePath = j["emissiveTexturePath"].get<std::string>();
    }
    material.doubleSided = j.value("doubleSided", false);
    material.castShadows = j.value("castShadows", true);
    material.receiveShadows = j.value("receiveShadows", true);
    if (j.contains("alphaMode")) material.alphaMode = static_cast<ECS::MaterialComponent::AlphaMode>(j["alphaMode"].get<i32>());
    material.alphaCutoff = j.value("alphaCutoff", 0.5f);
    // Height/parallax mapping (optional, added in later versions)
    if (j.contains("heightTexturePath")) material.heightTexturePath = j["heightTexturePath"].get<std::string>();
    if (j.contains("parallaxScale")) material.parallaxScale = j["parallaxScale"].get<f32>();
    // Retro rendering flags (optional, added in later versions)
    if (j.contains("flatShading")) material.flatShading = j["flatShading"].get<bool>();
    if (j.contains("affineTexturing")) material.affineTexturing = j["affineTexturing"].get<bool>();
    if (j.contains("vertexSnapping")) material.vertexSnapping = j["vertexSnapping"].get<bool>();
    if (j.contains("stippleTransparency")) material.stippleTransparency = j["stippleTransparency"].get<bool>();
    if (j.contains("uvQuantize")) material.uvQuantize = j["uvQuantize"].get<bool>();
    if (j.contains("gouraudOnly")) material.gouraudOnly = j["gouraudOnly"].get<bool>();
    if (j.contains("vertexSnapResolution")) material.vertexSnapResolution = j["vertexSnapResolution"].get<u8>();
    if (j.contains("shadowDitherMode")) material.shadowDitherMode = j["shadowDitherMode"].get<u8>();
    if (j.contains("reflectivity")) material.reflectivity = j["reflectivity"].get<f32>();
    if (j.contains("fresnelPower")) material.fresnelPower = j["fresnelPower"].get<f32>();
    if (j.contains("rimLightStrength")) material.rimLightStrength = j["rimLightStrength"].get<f32>();
    if (j.contains("excludeFromCelShading")) material.excludeFromCelShading = j["excludeFromCelShading"].get<bool>();
    return material;
}

ECS::MeshComponent DeserializeMeshComponent(const json& j) {
    ECS::MeshComponent mesh;

    if (j.contains("vertices") && j["vertices"].is_array()) {
        for (const auto& v : j["vertices"]) {
            ECS::MeshComponent::Vertex vertex;
            vertex.position = DeserializeVector3(v["position"]);
            vertex.normal = DeserializeVector3(v["normal"]);
            vertex.uv = DeserializeVector2(v["uv"]);
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
                vertex.boneIndices[0] = v["boneIndices"][0].get<u32>();
                vertex.boneIndices[1] = v["boneIndices"][1].get<u32>();
                vertex.boneIndices[2] = v["boneIndices"][2].get<u32>();
                vertex.boneIndices[3] = v["boneIndices"][3].get<u32>();
            }
            mesh.vertices.push_back(vertex);
        }
    }

    if (j.contains("indices") && j["indices"].is_array()) {
        mesh.indices = j["indices"].get<std::vector<u32>>();
    }

    return mesh;
}

ECS::LightComponent DeserializeLightComponent(const json& j) {
    ECS::LightComponent light;
    if (j.contains("type")) light.type = static_cast<ECS::LightType>(j["type"].get<i32>());
    if (j.contains("color")) light.color = DeserializeVector3(j["color"]);
    light.intensity = j.value("intensity", 1.0f);
    light.range = j.value("range", 10.0f);
    light.constantAttenuation = j.value("constantAttenuation", 1.0f);
    light.linearAttenuation = j.value("linearAttenuation", 0.09f);
    light.quadraticAttenuation = j.value("quadraticAttenuation", 0.032f);
    light.innerConeAngle = j.value("innerConeAngle", 12.5f);
    light.outerConeAngle = j.value("outerConeAngle", 17.5f);
    light.castShadows = j.value("castShadows", false);
    // Note: old scenes may contain "shadowMapResolution" — silently ignored
    return light;
}

ECS::NotesComponent DeserializeNotesComponent(const json& j) {
    ECS::NotesComponent notes;
    notes.notes = j["notes"].get<std::string>();
    return notes;
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
        text.horizontalAlign = static_cast<ECS::TextAlign>(j["horizontalAlign"].get<i32>());
    }
    text.paddingX = j.value("paddingX", 16.0f);
    text.paddingY = j.value("paddingY", 16.0f);
    text.dirty = true; // Re-rasterize on load
    return text;
}

ECS::CameraComponent DeserializeCameraComponent(const json& j) {
    ECS::CameraComponent camera;
    if (j.contains("projectionType")) camera.projectionType = static_cast<ECS::ProjectionType>(j["projectionType"].get<i32>());
    camera.fieldOfView = j.value("fieldOfView", 60.0f);
    camera.nearPlane = j.value("nearPlane", 0.1f);
    camera.farPlane = j.value("farPlane", 1000.0f);
    camera.orthoSize = j.value("orthoSize", 10.0f);
    camera.priority = j.value("priority", 0);
    camera.isActive = j.value("isActive", true);
    camera.clearDepth = j.value("clearDepth", true);
    camera.clearColor = j.value("clearColor", true);
    if (j.contains("backgroundColor")) camera.backgroundColor = DeserializeVector3(j["backgroundColor"]);
    camera.viewportX = j.value("viewportX", 0.0f);
    camera.viewportY = j.value("viewportY", 0.0f);
    camera.viewportWidth = j.value("viewportWidth", 1.0f);
    camera.viewportHeight = j.value("viewportHeight", 1.0f);
    camera.cullingMask = j.value("cullingMask", 0xFFFFFFFFu);
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
    return j;
}

ECS::WeatherZoneComponent DeserializeWeatherZoneComponent(const json& j) {
    ECS::WeatherZoneComponent zone;
    zone.halfExtents = DeserializeVector3(j["halfExtents"]);
    zone.weatherType = j["weatherType"].get<u32>();
    zone.rainIntensity = j["rainIntensity"].get<f32>();
    zone.snowIntensity = j["snowIntensity"].get<f32>();
    zone.fogDensity = j["fogDensity"].get<f32>();
    zone.fogColor = DeserializeVector3(j["fogColor"]);
    zone.fogStart = j["fogStart"].get<f32>();
    zone.fogEnd = j["fogEnd"].get<f32>();
    zone.lightningEnabled = j["lightningEnabled"].get<bool>();
    if (j.contains("lightningMinInterval")) zone.lightningMinInterval = j["lightningMinInterval"].get<f32>();
    if (j.contains("lightningMaxInterval")) zone.lightningMaxInterval = j["lightningMaxInterval"].get<f32>();
    if (j.contains("windDirection")) zone.windDirection = DeserializeVector3(j["windDirection"]);
    if (j.contains("windStrength")) zone.windStrength = j["windStrength"].get<f32>();
    zone.priority = j["priority"].get<i32>();
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
    j["priority"] = volume.priority;
    j["iceColor"] = SerializeVector3(volume.iceColor);
    j["iceOpacity"] = RF(volume.iceOpacity);
    j["freezeRate"] = RF(volume.freezeRate);
    j["thawRate"] = RF(volume.thawRate);
    return j;
}

ECS::WaterVolumeComponent DeserializeWaterVolumeComponent(const json& j) {
    ECS::WaterVolumeComponent volume;
    volume.halfExtents = DeserializeVector3(j["halfExtents"]);
    if (j.contains("waterType")) volume.waterType = static_cast<ECS::WaterType>(j["waterType"].get<u32>());
    volume.waterColor = DeserializeVector3(j["waterColor"]);
    volume.opacity = j["opacity"].get<f32>();
    volume.waveSpeed = j["waveSpeed"].get<f32>();
    volume.waveHeight = j["waveHeight"].get<f32>();
    if (j.contains("enableShore")) volume.enableShore = j["enableShore"].get<bool>();
    if (j.contains("shoreWidth")) volume.shoreWidth = j["shoreWidth"].get<f32>();
    if (j.contains("foamIntensity")) volume.foamIntensity = j["foamIntensity"].get<f32>();
    if (j.contains("foamScale")) volume.foamScale = j["foamScale"].get<f32>();
    if (j.contains("shoreColor")) volume.shoreColor = DeserializeVector3(j["shoreColor"]);
    volume.priority = j["priority"].get<i32>();
    if (j.contains("iceColor")) volume.iceColor = DeserializeVector3(j["iceColor"]);
    if (j.contains("iceOpacity")) volume.iceOpacity = j["iceOpacity"].get<f32>();
    if (j.contains("freezeRate")) volume.freezeRate = j["freezeRate"].get<f32>();
    if (j.contains("thawRate")) volume.thawRate = j["thawRate"].get<f32>();
    return volume;
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
    if (j.contains("customAssetPath")) shrub.customAssetPath = j["customAssetPath"].get<std::string>();
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
    if (j.contains("trunkColor")) tree.trunkColor = DeserializeVector3(j["trunkColor"]);
    if (j.contains("canopyBaseColor")) tree.canopyBaseColor = DeserializeVector3(j["canopyBaseColor"]);
    if (j.contains("canopyTipColor")) tree.canopyTipColor = DeserializeVector3(j["canopyTipColor"]);
    if (j.contains("windSwayStrength")) tree.windSwayStrength = j["windSwayStrength"].get<f32>();
    if (j.contains("canopyQuads")) tree.canopyQuads = j["canopyQuads"].get<u32>();
    if (j.contains("minHeightScale")) tree.minHeightScale = j["minHeightScale"].get<f32>();
    if (j.contains("maxHeightScale")) tree.maxHeightScale = j["maxHeightScale"].get<f32>();
    if (j.contains("treeType")) tree.treeType = static_cast<ECS::TreeType>(j["treeType"].get<u32>());
    if (j.contains("springCanopyColor")) tree.springCanopyColor = DeserializeVector3(j["springCanopyColor"]);
    if (j.contains("summerCanopyColor")) tree.summerCanopyColor = DeserializeVector3(j["summerCanopyColor"]);
    if (j.contains("fallCanopyColor")) tree.fallCanopyColor = DeserializeVector3(j["fallCanopyColor"]);
    if (j.contains("barkTexturePath")) tree.barkTexturePath = j["barkTexturePath"].get<std::string>();
    if (j.contains("canopyTexturePath")) tree.canopyTexturePath = j["canopyTexturePath"].get<std::string>();
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
    if (j.contains("gridWidth")) terrain.gridWidth = j["gridWidth"].get<u32>();
    if (j.contains("gridHeight")) terrain.gridHeight = j["gridHeight"].get<u32>();
    if (j.contains("cellSize")) terrain.cellSize = j["cellSize"].get<f32>();
    if (j.contains("maxHeight")) terrain.maxHeight = j["maxHeight"].get<f32>();
    if (j.contains("heightmap")) terrain.heightmap = j["heightmap"].get<std::vector<f32>>();
    if (j.contains("splatmap")) terrain.splatmap = j["splatmap"].get<std::vector<f32>>();
    if (j.contains("layers")) {
        const auto& layersArr = j["layers"];
        for (int i = 0; i < 4 && i < static_cast<int>(layersArr.size()); ++i) {
            if (layersArr[i].contains("texturePath"))
                terrain.layers[i].texturePath = layersArr[i]["texturePath"].get<std::string>();
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
    if (j.contains("texturePath")) terrain.texturePath = j["texturePath"].get<std::string>();
    if (j.contains("autoColliders")) terrain.autoColliders = j["autoColliders"].get<bool>();
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
    trigger.halfExtents = DeserializeVector3(j["halfExtents"]);
    trigger.targetCamera = j["targetCamera"].get<u64>();
    trigger.priority = j["priority"].get<i32>();
    trigger.blendTime = j["blendTime"].get<f32>();
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
    zone.halfExtents = DeserializeVector3(j["halfExtents"]);
    zone.temperature = j["temperature"].get<f32>();
    zone.priority = j["priority"].get<i32>();
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
    if (j.contains("shape")) zone.shape = static_cast<ECS::GravityZoneShape>(j["shape"].get<u32>());
    if (j.contains("mode")) zone.mode = static_cast<ECS::GravityZoneMode>(j["mode"].get<u32>());
    zone.halfExtents = DeserializeVector3(j["halfExtents"]);
    zone.gravityDirection = DeserializeVector3(j["gravityDirection"]);
    zone.gravityStrength = j["gravityStrength"].get<f32>();
    zone.priority = j["priority"].get<i32>();
    if (j.contains("isActive")) zone.isActive = j["isActive"].get<bool>();
    return zone;
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
    if (j.contains("fluidType")) vol.fluidType = static_cast<ECS::FluidType>(j["fluidType"].get<u32>());
    if (j.contains("dimension")) vol.dimension = static_cast<ECS::FluidDimension>(j["dimension"].get<u32>());
    if (j.contains("gridSize")) vol.gridSize = j["gridSize"].get<u32>();
    if (j.contains("viscosity")) vol.viscosity = j["viscosity"].get<f32>();
    if (j.contains("diffusion")) vol.diffusion = j["diffusion"].get<f32>();
    if (j.contains("dissipation")) vol.dissipation = j["dissipation"].get<f32>();
    if (j.contains("velocityDissipation")) vol.velocityDissipation = j["velocityDissipation"].get<f32>();
    if (j.contains("solverIterations")) vol.solverIterations = j["solverIterations"].get<i32>();
    if (j.contains("buoyancy")) vol.buoyancy = j["buoyancy"].get<f32>();
    if (j.contains("fluidColor")) vol.fluidColor = DeserializeVector3(j["fluidColor"]);
    if (j.contains("opacity")) vol.opacity = j["opacity"].get<f32>();
    if (j.contains("densityThreshold")) vol.densityThreshold = j["densityThreshold"].get<f32>();
    if (j.contains("renderEnabled")) vol.renderEnabled = j["renderEnabled"].get<bool>();
    if (j.contains("sourceRadius")) vol.sourceRadius = j["sourceRadius"].get<f32>();
    if (j.contains("sourceDensity")) vol.sourceDensity = j["sourceDensity"].get<f32>();
    if (j.contains("sourceVelocityScale")) vol.sourceVelocityScale = j["sourceVelocityScale"].get<f32>();
    if (j.contains("isActive")) vol.isActive = j["isActive"].get<bool>();
    if (j.contains("priority")) vol.priority = j["priority"].get<i32>();
    return vol;
}

// Base controller fields helper
json SerializeControllerBase(const ECS::CharacterControllerBase& base) {
    json j;
    j["moveSpeed"] = RF(base.moveSpeed);
    j["sprintMultiplier"] = RF(base.sprintMultiplier);
    j["isEnabled"] = base.isEnabled;
    j["useWASD"] = base.useWASD;
    j["useArrowKeys"] = base.useArrowKeys;
    j["useGamepad"] = base.useGamepad;
    j["gamepadIndex"] = RF(base.gamepadIndex);
    j["gamepadLookSensitivity"] = RF(base.gamepadLookSensitivity);
    j["disableMouseLook"] = base.disableMouseLook;
    j["gridMovement"] = base.gridMovement;
    j["gridCellSize"] = RF(base.gridCellSize);
    j["gridMoveSpeed"] = RF(base.gridMoveSpeed);
    return j;
}

void DeserializeControllerBase(const json& j, ECS::CharacterControllerBase& base) {
    if (j.contains("moveSpeed")) base.moveSpeed = j["moveSpeed"].get<f32>();
    if (j.contains("sprintMultiplier")) base.sprintMultiplier = j["sprintMultiplier"].get<f32>();
    if (j.contains("isEnabled")) base.isEnabled = JB(j["isEnabled"]);
    if (j.contains("useWASD")) base.useWASD = JB(j["useWASD"]);
    if (j.contains("useArrowKeys")) base.useArrowKeys = JB(j["useArrowKeys"]);
    if (j.contains("useGamepad")) base.useGamepad = JB(j["useGamepad"]);
    if (j.contains("gamepadIndex")) base.gamepadIndex = j["gamepadIndex"].get<i32>();
    if (j.contains("gamepadLookSensitivity")) base.gamepadLookSensitivity = j["gamepadLookSensitivity"].get<f32>();
    if (j.contains("disableMouseLook")) base.disableMouseLook = JB(j["disableMouseLook"]);
    if (j.contains("gridMovement")) base.gridMovement = JB(j["gridMovement"]);
    if (j.contains("gridCellSize")) base.gridCellSize = j["gridCellSize"].get<f32>();
    if (j.contains("gridMoveSpeed")) base.gridMoveSpeed = j["gridMoveSpeed"].get<f32>();
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
    if (j.contains("promptText")) comp.promptText = j["promptText"].get<std::string>();
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
    if (j.contains("flags")) flags.flags = static_cast<Accessibility::ContentWarningType>(j["flags"].get<u32>());
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
    j["priority"] = audio.priority;
    return j;
}

ECS::AudioSourceComponent DeserializeAudioSourceComponent(const json& j) {
    ECS::AudioSourceComponent audio;
    if (j.contains("clipPath")) audio.clipPath = j["clipPath"].get<std::string>();
    if (j.contains("volume")) audio.volume = j["volume"].get<f32>();
    if (j.contains("pitch")) audio.pitch = j["pitch"].get<f32>();
    if (j.contains("minDistance")) audio.minDistance = j["minDistance"].get<f32>();
    if (j.contains("maxDistance")) audio.maxDistance = j["maxDistance"].get<f32>();
    if (j.contains("playOnAwake")) audio.playOnAwake = j["playOnAwake"].get<bool>();
    if (j.contains("loop")) audio.loop = j["loop"].get<bool>();
    if (j.contains("is3D")) audio.is3D = j["is3D"].get<bool>();
    if (j.contains("spatialBlend")) audio.spatialBlend = j["spatialBlend"].get<f32>();
    if (j.contains("rolloff")) audio.rolloff = static_cast<ECS::AudioSourceComponent::Rolloff>(j["rolloff"].get<u8>());
    if (j.contains("priority")) audio.priority = j["priority"].get<i32>();
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
    if (j.contains("isActive")) listener.isActive = j["isActive"].get<bool>();
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
    return j;
}

ECS::RigidbodyComponent DeserializeRigidbodyComponent(const json& j) {
    ECS::RigidbodyComponent rb;
    if (j.contains("mass")) rb.mass = j["mass"].get<f32>();
    if (j.contains("drag")) rb.drag = j["drag"].get<f32>();
    if (j.contains("angularDrag")) rb.angularDrag = j["angularDrag"].get<f32>();
    if (j.contains("useGravity")) rb.useGravity = j["useGravity"].get<bool>();
    if (j.contains("gravityScale")) rb.gravityScale = j["gravityScale"].get<f32>();
    if (j.contains("freezePositionX")) rb.freezePositionX = j["freezePositionX"].get<bool>();
    if (j.contains("freezePositionY")) rb.freezePositionY = j["freezePositionY"].get<bool>();
    if (j.contains("freezePositionZ")) rb.freezePositionZ = j["freezePositionZ"].get<bool>();
    if (j.contains("freezeRotationX")) rb.freezeRotationX = j["freezeRotationX"].get<bool>();
    if (j.contains("freezeRotationY")) rb.freezeRotationY = j["freezeRotationY"].get<bool>();
    if (j.contains("freezeRotationZ")) rb.freezeRotationZ = j["freezeRotationZ"].get<bool>();
    if (j.contains("bodyType")) rb.bodyType = static_cast<ECS::RigidbodyComponent::BodyType>(j["bodyType"].get<u8>());
    if (j.contains("collisionMode")) rb.collisionMode = static_cast<ECS::RigidbodyComponent::CollisionMode>(j["collisionMode"].get<u8>());
    return rb;
}

json SerializeBoxColliderComponent(const ECS::BoxColliderComponent& col) {
    json j;
    j["center"] = SerializeVector3(col.center);
    j["size"] = SerializeVector3(col.size);
    j["isTrigger"] = col.isTrigger;
    j["friction"] = RF(col.friction);
    j["bounciness"] = RF(col.bounciness);
    j["categoryBits"] = RF(col.categoryBits);
    j["collisionMask"] = RF(col.collisionMask);
    return j;
}

ECS::BoxColliderComponent DeserializeBoxColliderComponent(const json& j) {
    ECS::BoxColliderComponent col;
    if (j.contains("center")) col.center = DeserializeVector3(j["center"]);
    if (j.contains("size")) col.size = DeserializeVector3(j["size"]);
    if (j.contains("isTrigger")) col.isTrigger = j["isTrigger"].get<bool>();
    if (j.contains("friction")) col.friction = j["friction"].get<f32>();
    if (j.contains("bounciness")) col.bounciness = j["bounciness"].get<f32>();
    if (j.contains("categoryBits")) {
        col.categoryBits = j["categoryBits"].get<u32>();
    } else if (j.contains("layer")) {
        // Migrate old "layer" field: layer 0 → bit 0, layer N → bit N
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
    j["autoApply"] = RF(pfc.autoApply);
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
    if (j.contains("autoApply")) pfc.autoApply = j["autoApply"].get<bool>();
    if (j.contains("frameColliders") && j["frameColliders"].is_array()) {
        for (const auto& f : j["frameColliders"]) {
            ECS::PerFrameColliderComponent::FrameCollider fc;
            if (f.contains("offset") && f["offset"].is_array() && f["offset"].size() >= 2) {
                fc.offset = Math::Vector2(f["offset"][0].get<f32>(), f["offset"][1].get<f32>());
            }
            if (f.contains("size") && f["size"].is_array() && f["size"].size() >= 2) {
                fc.size = Math::Vector2(f["size"][0].get<f32>(), f["size"][1].get<f32>());
            }
            if (f.contains("enabled")) fc.enabled = f["enabled"].get<bool>();
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
    j["categoryBits"] = RF(poly.categoryBits);
    j["collisionMask"] = RF(poly.collisionMask);
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
    if (j.contains("isTrigger")) poly.isTrigger = j["isTrigger"].get<bool>();
    if (j.contains("friction")) poly.friction = j["friction"].get<f32>();
    if (j.contains("bounciness")) poly.bounciness = j["bounciness"].get<f32>();
    if (j.contains("categoryBits")) poly.categoryBits = j["categoryBits"].get<u32>();
    if (j.contains("collisionMask")) poly.collisionMask = j["collisionMask"].get<u32>();
    return poly;
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
    if (j.contains("syncTransform")) net.syncTransform = j["syncTransform"].get<bool>();
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
    j["categoryBits"] = RF(col.categoryBits);
    j["collisionMask"] = RF(col.collisionMask);
    return j;
}

ECS::SphereColliderComponent DeserializeSphereColliderComponent(const json& j) {
    ECS::SphereColliderComponent col;
    if (j.contains("center")) col.center = DeserializeVector3(j["center"]);
    if (j.contains("radius")) col.radius = j["radius"].get<f32>();
    if (j.contains("isTrigger")) col.isTrigger = j["isTrigger"].get<bool>();
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
    j["categoryBits"] = RF(col.categoryBits);
    j["collisionMask"] = RF(col.collisionMask);
    return j;
}

ECS::CapsuleColliderComponent DeserializeCapsuleColliderComponent(const json& j) {
    ECS::CapsuleColliderComponent col;
    if (j.contains("center")) col.center = DeserializeVector3(j["center"]);
    if (j.contains("radius")) col.radius = j["radius"].get<f32>();
    if (j.contains("height")) col.height = j["height"].get<f32>();
    if (j.contains("direction")) col.direction = static_cast<ECS::CapsuleColliderComponent::Direction>(j["direction"].get<u8>());
    if (j.contains("isTrigger")) col.isTrigger = j["isTrigger"].get<bool>();
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
    return j;
}

ECS::HealthComponent DeserializeHealthComponent(const json& j) {
    ECS::HealthComponent h;
    if (j.contains("maxHealth")) h.maxHealth = j["maxHealth"].get<f32>();
    h.currentHealth = h.maxHealth;
    if (j.contains("regenRate")) h.regenRate = j["regenRate"].get<f32>();
    if (j.contains("regenDelay")) h.regenDelay = j["regenDelay"].get<f32>();
    if (j.contains("isInvulnerable")) h.isInvulnerable = j["isInvulnerable"].get<bool>();
    if (j.contains("invulnerabilityTime")) h.invulnerabilityTime = j["invulnerabilityTime"].get<f32>();
    if (j.contains("maxShield")) h.maxShield = j["maxShield"].get<f32>();
    h.currentShield = h.maxShield;
    if (j.contains("shieldRegenRate")) h.shieldRegenRate = j["shieldRegenRate"].get<f32>();
    if (j.contains("shieldRegenDelay")) h.shieldRegenDelay = j["shieldRegenDelay"].get<f32>();
    return h;
}

json SerializeDamageComponent(const ECS::DamageComponent& d) {
    json j;
    j["damage"] = RF(d.damage);
    j["knockbackForce"] = RF(d.knockbackForce);
    j["destroyOnHit"] = RF(d.destroyOnHit);
    j["damageOnce"] = RF(d.damageOnce);
    j["damageInterval"] = RF(d.damageInterval);
    j["type"] = static_cast<u8>(d.type);
    return j;
}

ECS::DamageComponent DeserializeDamageComponent(const json& j) {
    ECS::DamageComponent d;
    if (j.contains("damage")) d.damage = j["damage"].get<f32>();
    if (j.contains("knockbackForce")) d.knockbackForce = j["knockbackForce"].get<f32>();
    if (j.contains("destroyOnHit")) d.destroyOnHit = j["destroyOnHit"].get<bool>();
    if (j.contains("damageOnce")) d.damageOnce = j["damageOnce"].get<bool>();
    if (j.contains("damageInterval")) d.damageInterval = j["damageInterval"].get<f32>();
    if (j.contains("type")) d.type = static_cast<ECS::DamageComponent::DamageType>(j["type"].get<u8>());
    return d;
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
    return j;
}

ECS::TriggerZoneComponent DeserializeTriggerZoneComponent(const json& j) {
    ECS::TriggerZoneComponent tz;
    if (j.contains("shape")) tz.shape = static_cast<ECS::TriggerZoneComponent::Shape>(j["shape"].get<u8>());
    if (j.contains("boxSize")) tz.boxSize = DeserializeVector3(j["boxSize"]);
    if (j.contains("sphereRadius")) tz.sphereRadius = j["sphereRadius"].get<f32>();
    if (j.contains("triggerMask")) tz.triggerMask = j["triggerMask"].get<u32>();
    if (j.contains("triggerOnce")) tz.triggerOnce = j["triggerOnce"].get<bool>();
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
    if (j.contains("promptText")) ic.promptText = j["promptText"].get<std::string>();
    if (j.contains("interactionRange")) ic.interactionRange = j["interactionRange"].get<f32>();
    if (j.contains("requiresLookAt")) ic.requiresLookAt = j["requiresLookAt"].get<bool>();
    if (j.contains("lookAtAngle")) ic.lookAtAngle = j["lookAtAngle"].get<f32>();
    if (j.contains("isEnabled")) ic.isEnabled = j["isEnabled"].get<bool>();
    if (j.contains("singleUse")) ic.singleUse = j["singleUse"].get<bool>();
    if (j.contains("highlightOnHover")) ic.highlightOnHover = j["highlightOnHover"].get<bool>();
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
    if (j.contains("type")) p.type = static_cast<ECS::PickupComponent::PickupType>(j["type"].get<u8>());
    if (j.contains("value")) p.value = j["value"].get<f32>();
    if (j.contains("customId")) p.customId = j["customId"].get<std::string>();
    if (j.contains("pickupRange")) p.pickupRange = j["pickupRange"].get<f32>();
    if (j.contains("destroyOnPickup")) p.destroyOnPickup = j["destroyOnPickup"].get<bool>();
    if (j.contains("magnetToPlayer")) p.magnetToPlayer = j["magnetToPlayer"].get<bool>();
    if (j.contains("magnetRange")) p.magnetRange = j["magnetRange"].get<f32>();
    if (j.contains("magnetSpeed")) p.magnetSpeed = j["magnetSpeed"].get<f32>();
    if (j.contains("canRespawn")) p.canRespawn = j["canRespawn"].get<bool>();
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
    j["layer"] = RF(l.layer);
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
    if (j.contains("faceCamera")) b.faceCamera = j["faceCamera"].get<bool>();
    if (j.contains("lockY")) b.lockY = j["lockY"].get<bool>();
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
    if (j.contains("playOnAwake")) pe.playOnAwake = j["playOnAwake"].get<bool>();
    if (j.contains("loop")) pe.loop = j["loop"].get<bool>();
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
    if (j.contains("shape")) pe.shape = static_cast<ECS::ParticleEmitterComponent::EmitterShape>(j["shape"].get<u8>());
    if (j.contains("shapeRadius")) pe.shapeRadius = j["shapeRadius"].get<f32>();
    if (j.contains("coneAngle")) pe.coneAngle = j["coneAngle"].get<f32>();
    if (j.contains("gravity")) pe.gravity = DeserializeVector3(j["gravity"]);
    if (j.contains("drag")) pe.drag = j["drag"].get<f32>();
    if (j.contains("texturePath")) pe.texturePath = j["texturePath"].get<std::string>();
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
    if (j.contains("simulationSpace")) pe.simulationSpace = static_cast<ECS::ParticleEmitterComponent::SimulationSpace>(j["simulationSpace"].get<u8>());
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
    return j;
}

ECS::Sprite2DComponent DeserializeSprite2DComponent(const json& j) {
    ECS::Sprite2DComponent s;
    if (j.contains("texturePath")) s.texturePath = j["texturePath"].get<std::string>();
    if (j.contains("normalMapPath")) s.normalMapPath = j["normalMapPath"].get<std::string>();
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
    if (j.contains("flipX")) s.flipX = j["flipX"].get<bool>();
    if (j.contains("flipY")) s.flipY = j["flipY"].get<bool>();
    if (j.contains("visible")) s.visible = j["visible"].get<bool>();
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
    if (j.contains("playing")) a.playing = j["playing"].get<bool>();
    if (j.contains("loop")) a.loop = j["loop"].get<bool>();
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
    if (j.contains("tiles")) tm.tiles = j["tiles"].get<std::vector<i32>>();
    if (j.contains("width")) tm.width = j["width"].get<u32>();
    if (j.contains("height")) tm.height = j["height"].get<u32>();
    if (j.contains("tilesetPath")) tm.tilesetPath = j["tilesetPath"].get<std::string>();
    if (j.contains("tileWidth")) tm.tileWidth = j["tileWidth"].get<f32>();
    if (j.contains("tileHeight")) tm.tileHeight = j["tileHeight"].get<f32>();
    if (j.contains("tilesetColumns")) tm.tilesetColumns = j["tilesetColumns"].get<u32>();
    if (j.contains("worldTileWidth")) tm.worldTileWidth = j["worldTileWidth"].get<f32>();
    if (j.contains("worldTileHeight")) tm.worldTileHeight = j["worldTileHeight"].get<f32>();
    if (j.contains("hasCollision")) tm.hasCollision = j["hasCollision"].get<bool>();
    if (j.contains("collisionMask")) tm.collisionMask = j["collisionMask"].get<std::vector<bool>>();
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
    if (j.contains("useBounds")) cb.useBounds = j["useBounds"].get<bool>();
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
    if (j.contains("autoZoomToFitTargets")) cb.autoZoomToFitTargets = j["autoZoomToFitTargets"].get<bool>();
    if (j.contains("additionalTargets")) {
        for (const auto& t : j["additionalTargets"]) {
            cb.additionalTargets.push_back(static_cast<ECS::Entity>(t.get<u64>()));
        }
    }
    return cb;
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
    if (j.contains("onEnter")) state.onEnter = j["onEnter"].get<std::string>();
    if (j.contains("onUpdate")) state.onUpdate = j["onUpdate"].get<std::string>();
    if (j.contains("onExit")) state.onExit = j["onExit"].get<std::string>();
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
            sm.boolParams[key] = val.get<bool>();
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
                sm.boolParams[p[0].get<std::string>()] = p[1].get<bool>();
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
        j["dialogueTree"] = RF(d.dialogueTree.ToJson());
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
    if (j.contains("dialogueLines")) d.dialogueLines = j["dialogueLines"].get<std::vector<std::string>>();
    if (j.contains("charDelay")) d.charDelay = j["charDelay"].get<f32>();
    if (j.contains("speakerName")) d.speakerName = j["speakerName"].get<std::string>();
    if (j.contains("portraitPath")) d.portraitPath = j["portraitPath"].get<std::string>();
    if (j.contains("typeSound")) d.typeSound = j["typeSound"].get<std::string>();
    if (j.contains("playTypeSound")) d.playTypeSound = j["playTypeSound"].get<bool>();
    if (j.contains("choices") && j["choices"].is_array()) {
        for (const auto& cj : j["choices"]) {
            ECS::DialogueComponent::Choice c;
            if (cj.contains("text")) c.text = cj["text"].get<std::string>();
            if (cj.contains("nextDialogueId")) c.nextDialogueId = cj["nextDialogueId"].get<std::string>();
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
        b.boxColor = Math::Vector3(j["boxColor"][0], j["boxColor"][1], j["boxColor"][2]);
    if (j.contains("boxAlpha")) b.boxAlpha = j["boxAlpha"].get<f32>();
    if (j.contains("boxBorderRadius")) b.boxBorderRadius = j["boxBorderRadius"].get<f32>();
    if (j.contains("speakerFontSize")) b.speakerFontSize = j["speakerFontSize"].get<f32>();
    if (j.contains("defaultSpeakerColor") && j["defaultSpeakerColor"].is_array() && j["defaultSpeakerColor"].size() >= 3)
        b.defaultSpeakerColor = Math::Vector3(j["defaultSpeakerColor"][0], j["defaultSpeakerColor"][1], j["defaultSpeakerColor"][2]);
    if (j.contains("textFontSize")) b.textFontSize = j["textFontSize"].get<f32>();
    if (j.contains("textColor") && j["textColor"].is_array() && j["textColor"].size() >= 3)
        b.textColor = Math::Vector3(j["textColor"][0], j["textColor"][1], j["textColor"][2]);
    if (j.contains("showPortrait")) b.showPortrait = j["showPortrait"].get<bool>();
    if (j.contains("portraitSize")) b.portraitSize = j["portraitSize"].get<f32>();
    if (j.contains("choiceSpacing")) b.choiceSpacing = j["choiceSpacing"].get<f32>();
    if (j.contains("choiceColor") && j["choiceColor"].is_array() && j["choiceColor"].size() >= 3)
        b.choiceColor = Math::Vector3(j["choiceColor"][0], j["choiceColor"][1], j["choiceColor"][2]);
    if (j.contains("choiceTextColor") && j["choiceTextColor"].is_array() && j["choiceTextColor"].size() >= 3)
        b.choiceTextColor = Math::Vector3(j["choiceTextColor"][0], j["choiceTextColor"][1], j["choiceTextColor"][2]);
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
    if (j.contains("type")) var.type = static_cast<Editor::PinType>(j["type"].get<i32>());
    if (j.contains("exposed")) var.exposed = j["exposed"].get<bool>();

    if (j.contains("value")) {
        switch (var.type) {
            case Editor::PinType::Bool:
                var.value = j["value"].get<bool>();
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
    j["graph"] = RF(vs.graph.ToJson());

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
            u8 eventType = static_cast<u8>(std::stoi(key));
            vs.eventNodes[eventType] = val.get<Editor::NodeId>();
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
            Editor::NodeId nodeId = static_cast<Editor::NodeId>(std::stoul(key));
            vs.nodeMeta[nodeId] = DeserializeVisualScriptNodeMeta(val);
        }
    }

    if (j.contains("enabled")) vs.enabled = j["enabled"].get<bool>();

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
    if (j.contains("property")) te.property = static_cast<ECS::TweenProperty>(j["property"].get<i32>());
    if (j.contains("easing")) te.easing = static_cast<ECS::EasingType>(j["easing"].get<i32>());
    if (j.contains("mode")) te.mode = static_cast<ECS::TweenMode>(j["mode"].get<i32>());
    if (j.contains("startValue")) te.startValue = DeserializeVector3(j["startValue"]);
    if (j.contains("endValue")) te.endValue = DeserializeVector3(j["endValue"]);
    if (j.contains("duration")) te.duration = j["duration"].get<f32>();
    if (j.contains("delay")) te.delay = j["delay"].get<f32>();
    if (j.contains("useCurrentAsStart")) te.useCurrentAsStart = j["useCurrentAsStart"].get<bool>();
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
    if (j.contains("autoPlay")) tc.autoPlay = j["autoPlay"].get<bool>();
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
    if (j.contains("patrolLoop")) ai.patrolLoop = j["patrolLoop"].get<bool>();
    if (j.contains("useNavmesh")) ai.useNavmesh = j["useNavmesh"].get<bool>();
    if (j.contains("repathInterval")) ai.repathInterval = j["repathInterval"].get<f32>();
    if (j.contains("arrivalRadius")) ai.arrivalRadius = j["arrivalRadius"].get<f32>();
    if (j.contains("chaseSpeed")) ai.chaseSpeed = j["chaseSpeed"].get<f32>();
    if (j.contains("fleeSpeed")) ai.fleeSpeed = j["fleeSpeed"].get<f32>();
    if (j.contains("fleeDistance")) ai.fleeDistance = j["fleeDistance"].get<f32>();
    if (j.contains("debugDrawPath")) ai.debugDrawPath = j["debugDrawPath"].get<bool>();
    if (j.contains("debugDrawDetection")) ai.debugDrawDetection = j["debugDrawDetection"].get<bool>();
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
    if (type == "bool") return j.value("value", false);
    if (type == "int") return j.value("value", 0);
    if (type == "float") return j.value("value", 0.0f);
    if (type == "vector3") return DeserializeVector3(j["value"]);
    if (type == "entity") return static_cast<ECS::Entity>(j.value("value", (u64)0));
    return j.value("value", std::string(""));
}

json SerializeBehaviorTreeComponent(const ECS::BehaviorTreeComponent& bt) {
    json j;
    j["graph"] = RF(bt.graph.ToJson());
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
        e["val"] = RF(SerializeBlackboardValue(entry.value));
        bbArr.push_back(e);
    }
    j["blackboardDefaults"] = bbArr;

    return j;
}

ECS::BehaviorTreeComponent DeserializeBehaviorTreeComponent(const json& j) {
    ECS::BehaviorTreeComponent bt;

    if (j.contains("graph")) bt.graph.FromJson(j["graph"]);
    if (j.contains("rootNodeId")) bt.rootNodeId = j["rootNodeId"].get<Editor::NodeId>();
    if (j.contains("enabled")) bt.enabled = j["enabled"].get<bool>();
    if (j.contains("tickInterval")) bt.tickInterval = j["tickInterval"].get<f32>();
    if (j.contains("debugEnabled")) bt.debugEnabled = j["debugEnabled"].get<bool>();

    if (j.contains("nodeMeta") && j["nodeMeta"].is_array()) {
        for (const auto& m : j["nodeMeta"]) {
            Editor::NodeId nodeId = m.value("nodeId", (Editor::NodeId)0);
            AI::BTNodeMeta meta;
            meta.nodeType = static_cast<AI::BTNodeType>(m.value("nodeType", (u8)0));
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
    j["graph"] = RF(qf.graph.ToJson());
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
    if (j.contains("questId")) qf.questId = j["questId"].get<std::string>();
    if (j.contains("questTitle")) qf.questTitle = j["questTitle"].get<std::string>();
    if (j.contains("questDescription")) qf.questDescription = j["questDescription"].get<std::string>();
    if (j.contains("enabled")) qf.enabled = j["enabled"].get<bool>();

    if (j.contains("nodeMeta") && j["nodeMeta"].is_array()) {
        for (const auto& m : j["nodeMeta"]) {
            Editor::NodeId nodeId = m.value("nodeId", (Editor::NodeId)0);
            Gameplay::QuestNodeMeta meta;
            meta.nodeType = static_cast<Gameplay::QuestNodeType>(m.value("nodeType", (u8)0));
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
    if (j.contains("matchTargetRotation")) ft.matchTargetRotation = j["matchTargetRotation"].get<bool>();
    if (j.contains("rotationSpeed")) ft.rotationSpeed = j["rotationSpeed"].get<f32>();
    if (j.contains("offset")) ft.offset = DeserializeVector3(j["offset"]);
    if (j.contains("useLocalOffset")) ft.useLocalOffset = j["useLocalOffset"].get<bool>();
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
    if (j.contains("useWorldTarget")) la.useWorldTarget = j["useWorldTarget"].get<bool>();
    if (j.contains("rotationSpeed")) la.rotationSpeed = j["rotationSpeed"].get<f32>();
    if (j.contains("instant")) la.instant = j["instant"].get<bool>();
    if (j.contains("constrainX")) la.constrainX = j["constrainX"].get<bool>();
    if (j.contains("constrainY")) la.constrainY = j["constrainY"].get<bool>();
    if (j.contains("constrainZ")) la.constrainZ = j["constrainZ"].get<bool>();
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
    if (j.contains("spawnOnStart")) sp.spawnOnStart = j["spawnOnStart"].get<bool>();
    if (j.contains("spawnDelay")) sp.spawnDelay = j["spawnDelay"].get<f32>();
    if (j.contains("respawnTime")) sp.respawnTime = j["respawnTime"].get<f32>();
    if (j.contains("maxSpawns")) sp.maxSpawns = j["maxSpawns"].get<i32>();
    if (j.contains("spawnRadius")) sp.spawnRadius = j["spawnRadius"].get<f32>();
    if (j.contains("randomRotation")) sp.randomRotation = j["randomRotation"].get<bool>();
    return sp;
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
    if (j.contains("loop")) t.loop = j["loop"].get<bool>();
    if (j.contains("autoStart")) t.autoStart = j["autoStart"].get<bool>();
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
    if (j.contains("keys")) inv.keys = j["keys"].get<std::vector<std::string>>();
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
    if (j.contains("savePosition")) sd.savePosition = j["savePosition"].get<bool>();
    if (j.contains("saveRotation")) sd.saveRotation = j["saveRotation"].get<bool>();
    if (j.contains("saveScale")) sd.saveScale = j["saveScale"].get<bool>();
    if (j.contains("saveEnabled")) sd.saveEnabled = j["saveEnabled"].get<bool>();
    if (j.contains("tier")) {
        u8 t = j["tier"].get<u8>();
        if (t < static_cast<u8>(ECS::PersistenceTier::COUNT))
            sd.tier = static_cast<ECS::PersistenceTier>(t);
    }
    if (j.contains("tags") && j["tags"].is_array()) {
        for (const auto& t : j["tags"]) {
            sd.tags.push_back(t.get<std::string>());
        }
    }
    if (j.contains("customData") && j["customData"].is_array()) {
        for (const auto& p : j["customData"]) {
            sd.customData.push_back({p[0].get<std::string>(), p[1].get<std::string>()});
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
    if (j.contains("showOnPause")) m.showOnPause = j["showOnPause"].get<bool>();
    if (j.contains("allowManualSave")) m.allowManualSave = j["allowManualSave"].get<bool>();
    if (j.contains("allowManualLoad")) m.allowManualLoad = j["allowManualLoad"].get<bool>();
    if (j.contains("allowDelete")) m.allowDelete = j["allowDelete"].get<bool>();
    if (j.contains("showAutoSaves")) m.showAutoSaves = j["showAutoSaves"].get<bool>();
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
    j["breakDistance"] = RF(t.breakDistance);
    j["tensionRamp"] = RF(t.tensionRamp);
    j["autoMass"] = RF(t.autoMass);
    j["autoSpringK"] = RF(t.autoSpringK);
    j["autoDamping"] = RF(t.autoDamping);
    j["autoBreakForce"] = RF(t.autoBreakForce);
    j["autoDrag"] = RF(t.autoDrag);
    return j;
}

ECS::TetherComponent DeserializeTetherComponent(const json& j) {
    ECS::TetherComponent t;
    if (j.contains("stemEntity")) t.stemEntity = static_cast<ECS::Entity>(j["stemEntity"].get<u64>());
    if (j.contains("connectedEntity")) t.connectedEntity = static_cast<ECS::Entity>(j["connectedEntity"].get<u64>());
    if (j.contains("attachLocalPos")) t.attachLocalPos = DeserializeVector3(j["attachLocalPos"]);
    if (j.contains("breakDistance")) t.breakDistance = j["breakDistance"].get<f32>();
    if (j.contains("tensionRamp")) t.tensionRamp = j["tensionRamp"].get<f32>();
    if (j.contains("autoMass")) t.autoMass = j["autoMass"].get<f32>();
    if (j.contains("autoSpringK")) t.autoSpringK = j["autoSpringK"].get<f32>();
    if (j.contains("autoDamping")) t.autoDamping = j["autoDamping"].get<f32>();
    if (j.contains("autoBreakForce")) t.autoBreakForce = j["autoBreakForce"].get<f32>();
    if (j.contains("autoDrag")) t.autoDrag = j["autoDrag"].get<f32>();
    // Backward compat: if connectedEntity missing, default to stemEntity
    if (!j.contains("connectedEntity") && t.connectedEntity == ECS::INVALID_ENTITY) {
        t.connectedEntity = t.stemEntity;
    }
    return t;
}

json SerializeGrabbableComponent(const ECS::GrabbableComponent& g) {
    json j;
    j["pullForce"] = RF(g.pullForce);
    j["grabRadius"] = RF(g.grabRadius);
    j["maxPullDistance"] = RF(g.maxPullDistance);
    j["maxVelocity"] = RF(g.maxVelocity);
    j["windSwayScale"] = RF(g.windSwayScale);
    return j;
}

ECS::GrabbableComponent DeserializeGrabbableComponent(const json& j) {
    ECS::GrabbableComponent g;
    if (j.contains("pullForce")) g.pullForce = j["pullForce"].get<f32>();
    if (j.contains("grabRadius")) g.grabRadius = j["grabRadius"].get<f32>();
    if (j.contains("maxPullDistance")) g.maxPullDistance = j["maxPullDistance"].get<f32>();
    if (j.contains("maxVelocity")) g.maxVelocity = j["maxVelocity"].get<f32>();
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
    if (j.contains("enabled")) lod.enabled = j["enabled"].get<bool>();
    if (j.contains("autoGenerated")) lod.autoGenerated = j["autoGenerated"].get<bool>();
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
    if (j.contains("customAssetPath")) gv.customAssetPath = j["customAssetPath"].get<std::string>();
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
    if (j.contains("useVertexColorWeight")) v.useVertexColorWeight = j["useVertexColorWeight"].get<bool>();
    return v;
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
    if (j.contains("defaultWalkSound")) f.defaultWalkSound = j["defaultWalkSound"].get<std::string>();
    if (j.contains("defaultRunSound")) f.defaultRunSound = j["defaultRunSound"].get<std::string>();
    if (j.contains("walkStepInterval")) f.walkStepInterval = j["walkStepInterval"].get<f32>();
    if (j.contains("runStepInterval")) f.runStepInterval = j["runStepInterval"].get<f32>();
    if (j.contains("volume")) f.volume = j["volume"].get<f32>();
    if (j.contains("pitchVariance")) f.pitchVariance = j["pitchVariance"].get<f32>();
    if (j.contains("currentSurface")) f.currentSurface = j["currentSurface"].get<std::string>();
    if (j.contains("surfaceSounds") && j["surfaceSounds"].is_array()) {
        for (const auto& sj : j["surfaceSounds"]) {
            ECS::FootstepComponent::SurfaceSound s;
            if (sj.contains("surfaceTag")) s.surfaceTag = sj["surfaceTag"].get<std::string>();
            if (sj.contains("walkSound")) s.walkSound = sj["walkSound"].get<std::string>();
            if (sj.contains("runSound")) s.runSound = sj["runSound"].get<std::string>();
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
    if (j.contains("status")) q.status = static_cast<ECS::QuestStateComponent::Status>(j["status"].get<u8>());
    if (j.contains("currentObjective")) q.currentObjective = j["currentObjective"].get<i32>();
    if (j.contains("timeElapsed")) q.timeElapsed = j["timeElapsed"].get<f32>();
    if (j.contains("objectiveFlags") && j["objectiveFlags"].is_array()) {
        for (const auto& fj : j["objectiveFlags"]) {
            std::string name = fj.value("name", "");
            bool complete = fj.value("complete", false);
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
    if (j.contains("type")) h.type = static_cast<ECS::HUDWidgetComponent::WidgetType>(j["type"].get<u8>());
    if (j.contains("visible")) h.visible = j["visible"].get<bool>();
    if (j.contains("screenSpace")) h.screenSpace = j["screenSpace"].get<bool>();
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
    data["progressFillColor"] = SerializeVector3(e.data.progressFillColor);
    data["sliderValue"] = RF(e.data.sliderValue);
    data["sliderMin"] = RF(e.data.sliderMin);
    data["sliderMax"] = RF(e.data.sliderMax);
    data["checked"] = RF(e.data.checked);
    if (!e.data.options.empty()) data["options"] = e.data.options;
    data["selectedOption"] = RF(e.data.selectedOption);
    data["inputText"] = e.data.inputText;
    data["placeholder"] = e.data.placeholder;
    j["data"] = data;

    // Events
    j["onClickEvent"] = e.onClickEvent;
    j["onValueChangedEvent"] = e.onValueChangedEvent;
    j["onSubmitEvent"] = e.onSubmitEvent;

    return j;
}

GUI::UIElement DeserializeUIElement(const json& j) {
    GUI::UIElement e;
    if (j.contains("id")) e.id = j["id"].get<u32>();
    if (j.contains("name")) e.name = j["name"].get<std::string>();
    if (j.contains("type")) e.type = static_cast<GUI::UIWidgetType>(j["type"].get<u8>());
    if (j.contains("visible")) e.visible = j["visible"].get<bool>();
    if (j.contains("enabled")) e.enabled = j["enabled"].get<bool>();
    if (j.contains("parentId")) e.parentId = j["parentId"].get<u32>();
    if (j.contains("childIds") && j["childIds"].is_array()) {
        for (const auto& cid : j["childIds"]) e.childIds.push_back(cid.get<u32>());
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
        if (s.contains("nineSlice")) {
            const auto& ns = s["nineSlice"];
            if (ns.contains("texturePath")) e.style.nineSlice.texturePath = ns["texturePath"].get<std::string>();
            if (ns.contains("borderLeft")) e.style.nineSlice.borderLeft = ns["borderLeft"].get<f32>();
            if (ns.contains("borderRight")) e.style.nineSlice.borderRight = ns["borderRight"].get<f32>();
            if (ns.contains("borderTop")) e.style.nineSlice.borderTop = ns["borderTop"].get<f32>();
            if (ns.contains("borderBottom")) e.style.nineSlice.borderBottom = ns["borderBottom"].get<f32>();
        }
    }

    if (j.contains("data")) {
        const auto& d = j["data"];
        if (d.contains("text")) e.data.text = d["text"].get<std::string>();
        if (d.contains("textAlignH")) e.data.textAlignH = d["textAlignH"].get<u8>();
        if (d.contains("textAlignV")) e.data.textAlignV = d["textAlignV"].get<u8>();
        if (d.contains("imagePath")) e.data.imagePath = d["imagePath"].get<std::string>();
        if (d.contains("imageTint")) e.data.imageTint = DeserializeVector3(d["imageTint"]);
        if (d.contains("imageAlpha")) e.data.imageAlpha = d["imageAlpha"].get<f32>();
        if (d.contains("progressValue")) e.data.progressValue = d["progressValue"].get<f32>();
        if (d.contains("progressFillColor")) e.data.progressFillColor = DeserializeVector3(d["progressFillColor"]);
        if (d.contains("sliderValue")) e.data.sliderValue = d["sliderValue"].get<f32>();
        if (d.contains("sliderMin")) e.data.sliderMin = d["sliderMin"].get<f32>();
        if (d.contains("sliderMax")) e.data.sliderMax = d["sliderMax"].get<f32>();
        if (d.contains("checked")) e.data.checked = d["checked"].get<bool>();
        if (d.contains("options") && d["options"].is_array()) {
            for (const auto& opt : d["options"]) e.data.options.push_back(opt.get<std::string>());
        }
        if (d.contains("selectedOption")) e.data.selectedOption = d["selectedOption"].get<i32>();
        if (d.contains("inputText")) e.data.inputText = d["inputText"].get<std::string>();
        if (d.contains("placeholder")) e.data.placeholder = d["placeholder"].get<std::string>();
    }

    if (j.contains("onClickEvent")) e.onClickEvent = j["onClickEvent"].get<std::string>();
    if (j.contains("onValueChangedEvent")) e.onValueChangedEvent = j["onValueChangedEvent"].get<std::string>();
    if (j.contains("onSubmitEvent")) e.onSubmitEvent = j["onSubmitEvent"].get<std::string>();

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
    auto deserializeNS = [](const json& ns, GUI::NineSliceConfig& cfg) {
        if (ns.contains("texturePath")) cfg.texturePath = ns["texturePath"].get<std::string>();
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
    j["theme"] = RF(SerializeUITheme(c.theme));
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
    if (j.contains("canvasName")) c.canvasName = j["canvasName"].get<std::string>();
    if (j.contains("visible")) c.visible = j["visible"].get<bool>();
    if (j.contains("sortOrder")) c.sortOrder = j["sortOrder"].get<i32>();
    if (j.contains("designWidth")) c.designWidth = j["designWidth"].get<f32>();
    if (j.contains("designHeight")) c.designHeight = j["designHeight"].get<f32>();
    if (j.contains("scaleMode")) c.scaleMode = static_cast<GUI::UIScaleMode>(j["scaleMode"].get<u8>());
    if (j.contains("theme")) c.theme = DeserializeUITheme(j["theme"]);
    if (j.contains("nextElementId")) c.nextElementId = j["nextElementId"].get<u32>();

    if (j.contains("elements") && j["elements"].is_array()) {
        for (const auto& ej : j["elements"]) {
            c.elements.push_back(DeserializeUIElement(ej));
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
    if (j.contains("loop")) c.loop = j["loop"].get<bool>();
    if (j.contains("autoPlay")) c.autoPlay = j["autoPlay"].get<bool>();
    if (j.contains("hideHUD")) c.hideHUD = j["hideHUD"].get<bool>();
    if (j.contains("disableInput")) c.disableInput = j["disableInput"].get<bool>();
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
            if (wj.contains("easing")) wp.easing = static_cast<ECS::CinematicCameraComponent::Waypoint::Easing>(wj["easing"].get<u8>());
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
    if (j.contains("breakable")) c.breakable = j["breakable"].get<bool>();
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
    if (j.contains("useLimits")) c.useLimits = j["useLimits"].get<bool>();
    if (j.contains("lowerLimit")) c.lowerLimit = j["lowerLimit"].get<f32>();
    if (j.contains("upperLimit")) c.upperLimit = j["upperLimit"].get<f32>();
    if (j.contains("useMotor")) c.useMotor = j["useMotor"].get<bool>();
    if (j.contains("motorSpeed")) c.motorSpeed = j["motorSpeed"].get<f32>();
    if (j.contains("motorMaxForce")) c.motorMaxForce = j["motorMaxForce"].get<f32>();
    if (j.contains("breakable")) c.breakable = j["breakable"].get<bool>();
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
    if (j.contains("useConeLimit")) c.useConeLimit = j["useConeLimit"].get<bool>();
    if (j.contains("coneAngleLimit")) c.coneAngleLimit = j["coneAngleLimit"].get<f32>();
    if (j.contains("useTwistLimit")) c.useTwistLimit = j["useTwistLimit"].get<bool>();
    if (j.contains("twistLowerLimit")) c.twistLowerLimit = j["twistLowerLimit"].get<f32>();
    if (j.contains("twistUpperLimit")) c.twistUpperLimit = j["twistUpperLimit"].get<f32>();
    if (j.contains("breakable")) c.breakable = j["breakable"].get<bool>();
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
    if (j.contains("breakable")) c.breakable = j["breakable"].get<bool>();
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
    if (j.contains("initialized")) c.initialized = j["initialized"].get<bool>();
    if (j.contains("breakable")) c.breakable = j["breakable"].get<bool>();
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
    if (j.contains("useLimits")) c.useLimits = j["useLimits"].get<bool>();
    if (j.contains("lowerLimit")) c.lowerLimit = j["lowerLimit"].get<f32>();
    if (j.contains("upperLimit")) c.upperLimit = j["upperLimit"].get<f32>();
    if (j.contains("useMotor")) c.useMotor = j["useMotor"].get<bool>();
    if (j.contains("motorSpeed")) c.motorSpeed = j["motorSpeed"].get<f32>();
    if (j.contains("motorMaxForce")) c.motorMaxForce = j["motorMaxForce"].get<f32>();
    if (j.contains("breakable")) c.breakable = j["breakable"].get<bool>();
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
        bjJson["coneAngleLimit"] = RF(bj.coneAngleLimit);
        bjJson["twistLimit"] = RF(bj.twistLimit);
        joints.push_back(bjJson);
    }
    o["boneJoints"] = joints;
    return o;
}

ECS::RagdollComponent DeserializeRagdollComponent(const json& j) {
    ECS::RagdollComponent r;
    if (j.contains("enabled")) r.enabled = j["enabled"].get<bool>();
    if (j.contains("autoDisableAfterSettle")) r.autoDisableAfterSettle = j["autoDisableAfterSettle"].get<bool>();
    if (j.contains("settleThreshold")) r.settleThreshold = j["settleThreshold"].get<f32>();
    if (j.contains("settleTime")) r.settleTime = j["settleTime"].get<f32>();
    if (j.contains("blendWeight")) r.blendWeight = j["blendWeight"].get<f32>();
    if (j.contains("blendSpeed")) r.blendSpeed = j["blendSpeed"].get<f32>();
    if (j.contains("gravityScale")) r.gravityScale = j["gravityScale"].get<f32>();
    if (j.contains("linearDamping")) r.linearDamping = j["linearDamping"].get<f32>();
    if (j.contains("angularDamping")) r.angularDamping = j["angularDamping"].get<f32>();
    if (j.contains("boneJoints") && j["boneJoints"].is_array()) {
        for (const auto& bjJson : j["boneJoints"]) {
            ECS::RagdollComponent::BoneJoint bj;
            if (bjJson.contains("boneName")) bj.boneName = bjJson["boneName"].get<std::string>();
            if (bjJson.contains("boneIndex")) bj.boneIndex = bjJson["boneIndex"].get<i32>();
            if (bjJson.contains("jointType")) bj.jointType = static_cast<ECS::JointType>(bjJson["jointType"].get<u8>());
            if (bjJson.contains("jointEntity")) bj.jointEntity = static_cast<ECS::Entity>(bjJson["jointEntity"].get<u64>());
            if (bjJson.contains("mass")) bj.mass = bjJson["mass"].get<f32>();
            if (bjJson.contains("colliderRadius")) bj.colliderRadius = bjJson["colliderRadius"].get<f32>();
            if (bjJson.contains("coneAngleLimit")) bj.coneAngleLimit = bjJson["coneAngleLimit"].get<f32>();
            if (bjJson.contains("twistLimit")) bj.twistLimit = bjJson["twistLimit"].get<f32>();
            r.boneJoints.push_back(bj);
        }
    }
    return r;
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
            if (sj.contains("path")) script.scriptPath = sj["path"].get<std::string>();
            if (sj.contains("class")) script.className = sj["class"].get<std::string>();
            if (sj.contains("enabled")) script.enabled = sj["enabled"].get<bool>();
            if (sj.contains("properties") && sj["properties"].is_object()) {
                for (auto it = sj["properties"].begin(); it != sj["properties"].end(); ++it) {
                    ECS::ScriptProperty prop;
                    prop.name = it.key();
                    prop.isOverridden = true;
                    if (it.value().contains("type")) {
                        prop.type = static_cast<ECS::ScriptPropertyType>(it.value()["type"].get<int>());
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
    if (j.contains("isLocked")) lk.isLocked = j["isLocked"].get<bool>();
    if (j.contains("consumeKey")) lk.consumeKey = j["consumeKey"].get<bool>();
    if (j.contains("autoOpen")) lk.autoOpen = j["autoOpen"].get<bool>();
    if (j.contains("interactRange")) lk.interactRange = j["interactRange"].get<f32>();
    if (j.contains("openMode")) lk.openMode = static_cast<ECS::LockComponent::OpenMode>(j["openMode"].get<i32>());
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
    if (j.contains("gridSnap")) pb.gridSnap = j["gridSnap"].get<bool>();
    if (j.contains("gridCellSize")) pb.gridCellSize = j["gridCellSize"].get<f32>();
    if (j.contains("gridMoveSpeed")) pb.gridMoveSpeed = j["gridMoveSpeed"].get<f32>();
    if (j.contains("pushableX")) pb.pushableX = j["pushableX"].get<bool>();
    if (j.contains("pushableY")) pb.pushableY = j["pushableY"].get<bool>();
    if (j.contains("pushableZ")) pb.pushableZ = j["pushableZ"].get<bool>();
    if (j.contains("canBePushedOff")) pb.canBePushedOff = j["canBePushedOff"].get<bool>();
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
    if (j.contains("type")) sw.type = static_cast<ECS::SwitchComponent::SwitchType>(j["type"].get<i32>());
    if (j.contains("requireSpecificTag")) sw.requireSpecificTag = j["requireSpecificTag"].get<bool>();
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
    if (j.contains("promptText")) sw.promptText = j["promptText"].get<std::string>();
    if (j.contains("showPrompt")) sw.showPrompt = j["showPrompt"].get<bool>();
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
    if (j.contains("type")) gz.type = static_cast<ECS::GoalZoneComponent::GoalType>(j["type"].get<i32>());
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
    if (j.contains("affectsPlayer")) cv.affectsPlayer = j["affectsPlayer"].get<bool>();
    if (j.contains("affectsPushables")) cv.affectsPushables = j["affectsPushables"].get<bool>();
    if (j.contains("isActive")) cv.isActive = j["isActive"].get<bool>();
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
    if (j.contains("preserveVelocity")) tp.preserveVelocity = j["preserveVelocity"].get<bool>();
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
    if (j.contains("destroyOnHit")) dc.destroyOnHit = j["destroyOnHit"].get<bool>();
    if (j.contains("spawnPickup")) dc.spawnPickup = j["spawnPickup"].get<bool>();
    if (j.contains("pickupId")) dc.pickupId = j["pickupId"].get<std::string>();
    if (j.contains("pickupCount")) dc.pickupCount = j["pickupCount"].get<i32>();
    if (j.contains("canRespawn")) dc.canRespawn = j["canRespawn"].get<bool>();
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
    if (j.contains("falloff")) cn.falloff = static_cast<ECS::CurlNoiseFieldComponent::Falloff>(j["falloff"].get<i32>());
    if (j.contains("affectParticles")) cn.affectParticles = j["affectParticles"].get<bool>();
    if (j.contains("affectMeshVertices")) cn.affectMeshVertices = j["affectMeshVertices"].get<bool>();
    if (j.contains("showDebugArrows")) cn.showDebugArrows = j["showDebugArrows"].get<bool>();
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
    if (j.contains("persistentFragments")) fc.persistentFragments = j["persistentFragments"].get<bool>();
    if (j.contains("allowRefracture")) fc.allowRefracture = j["allowRefracture"].get<bool>();
    if (j.contains("maxRefractureDepth")) fc.maxRefractureDepth = j["maxRefractureDepth"].get<u32>();
    if (j.contains("maxFragmentEntities")) fc.maxFragmentEntities = j["maxFragmentEntities"].get<u32>();
    if (j.contains("autoCleanup")) fc.autoCleanup = j["autoCleanup"].get<bool>();
    if (j.contains("cleanupDelay")) fc.cleanupDelay = j["cleanupDelay"].get<f32>();
    if (j.contains("preFracture")) fc.preFracture = j["preFracture"].get<bool>();
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
    if (j.contains("mode")) mp.mode = static_cast<ECS::MovingPlatformComponent::PlatformMode>(j["mode"].get<i32>());
    if (j.contains("carryEntities")) mp.carryEntities = j["carryEntities"].get<bool>();
    return mp;
}

// ============================================================================
// Skeleton & Animator serialization
// ============================================================================

json SerializeSkeletonComponent(const ECS::SkeletonComponent& skelComp) {
    json j;
    j["sourceAssetPath"] = skelComp.sourceAssetPath;

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
    if (j.contains("sourceAssetPath")) skelComp.sourceAssetPath = j["sourceAssetPath"].get<std::string>();

    auto skeleton = std::make_shared<Animation::Skeleton>();
    if (j.contains("name")) skeleton->name = j["name"].get<std::string>();

    if (j.contains("bones") && j["bones"].is_array()) {
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

json SerializeAnimatorComponent(const ECS::AnimatorComponent& animComp) {
    json j;
    const auto& animator = animComp.animator;

    j["speed"] = RF(animator.GetSpeed());
    j["currentAnimation"] = animator.GetCurrentAnimationName();

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

    // Deserialize animation clips
    if (j.contains("animations") && j["animations"].is_object()) {
        for (auto& [name, aj] : j["animations"].items()) {
            Animation::SkeletalAnimation anim;
            anim.name = name;
            if (aj.contains("duration")) anim.duration = aj["duration"].get<f32>();
            if (aj.contains("ticksPerSecond")) anim.ticksPerSecond = aj["ticksPerSecond"].get<f32>();
            if (aj.contains("playMode")) anim.playMode = static_cast<Animation::PlayMode>(aj["playMode"].get<i32>());

            if (aj.contains("tracks") && aj["tracks"].is_array()) {
                for (const auto& tj : aj["tracks"]) {
                    Animation::BoneTrack track;
                    if (tj.contains("boneName")) track.boneName = tj["boneName"].get<std::string>();
                    if (tj.contains("boneIndex")) track.boneIndex = tj["boneIndex"].get<i32>();

                    // Position keyframes
                    if (tj.contains("positionTimes") && tj["positionTimes"].is_array()) {
                        for (const auto& t : tj["positionTimes"]) track.positionTimes.push_back(t.get<f32>());
                    }
                    if (tj.contains("positions") && tj["positions"].is_array()) {
                        for (const auto& p : tj["positions"]) track.positions.push_back(DeserializeVector3(p));
                    }

                    // Rotation keyframes
                    if (tj.contains("rotationTimes") && tj["rotationTimes"].is_array()) {
                        for (const auto& t : tj["rotationTimes"]) track.rotationTimes.push_back(t.get<f32>());
                    }
                    if (tj.contains("rotations") && tj["rotations"].is_array()) {
                        for (const auto& r : tj["rotations"]) track.rotations.push_back(DeserializeQuaternion(r));
                    }

                    // Scale keyframes
                    if (tj.contains("scaleTimes") && tj["scaleTimes"].is_array()) {
                        for (const auto& t : tj["scaleTimes"]) track.scaleTimes.push_back(t.get<f32>());
                    }
                    if (tj.contains("scales") && tj["scales"].is_array()) {
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
                if (sj.contains("playMode")) state.playMode = static_cast<Animation::PlayMode>(sj["playMode"].get<i32>());
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
                if (tj.contains("hasExitTime")) trans.hasExitTime = tj["hasExitTime"].get<bool>();
                if (tj.contains("exitTime")) trans.exitTime = tj["exitTime"].get<f32>();

                if (tj.contains("conditions") && tj["conditions"].is_array()) {
                    for (const auto& cj : tj["conditions"]) {
                        Animation::TransitionCondition cond;
                        if (cj.contains("parameterName")) cond.parameterName = cj["parameterName"].get<std::string>();
                        if (cj.contains("type")) cond.type = static_cast<Animation::TransitionCondition::Type>(cj["type"].get<i32>());
                        if (cj.contains("comparison")) cond.comparison = static_cast<Animation::TransitionCondition::Comparison>(cj["comparison"].get<i32>());

                        switch (cond.type) {
                            case Animation::TransitionCondition::Type::Bool:
                            case Animation::TransitionCondition::Type::Trigger:
                                if (cj.contains("boolValue")) cond.value.boolValue = cj["boolValue"].get<bool>();
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
                animComp.stateMachine.SetBool(k, v.get<bool>());
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

SerializationResult SceneSerializer::SaveEntities(const std::string& filepath, const std::vector<ECS::Entity>& entities, const SerializationOptions& options) {
    if (!m_World) {
        SerializationResult result;
        result.success = false;
        result.error = "No world set";
        result.filepath = filepath;
        return result;
    }

    try {
        json sceneJson;
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

            json entityJson;
            entityJson["id"] = static_cast<u64>(entity);

            // Serialize components
            if (m_World->HasComponent<ECS::NameComponent>(entity)) {
                const auto* name = m_World->GetComponent<ECS::NameComponent>(entity);
                entityJson["name"] = SerializeNameComponent(*name);
            }

            if (m_World->HasComponent<ECS::TransformComponent>(entity)) {
                const auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                entityJson["transform"] = SerializeTransformComponent(*transform);
            }

            if (m_World->HasComponent<ECS::MaterialComponent>(entity)) {
                const auto* material = m_World->GetComponent<ECS::MaterialComponent>(entity);
                entityJson["material"] = SerializeMaterialComponent(*material);
            }

            if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
                const auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
                entityJson["mesh"] = SerializeMeshComponent(*mesh, options.includeVertexData);
            }

            if (m_World->HasComponent<ECS::SkeletonComponent>(entity)) {
                const auto* skel = m_World->GetComponent<ECS::SkeletonComponent>(entity);
                entityJson["skeleton"] = SerializeSkeletonComponent(*skel);
            }

            if (m_World->HasComponent<ECS::AnimatorComponent>(entity)) {
                const auto* anim = m_World->GetComponent<ECS::AnimatorComponent>(entity);
                entityJson["animator"] = SerializeAnimatorComponent(*anim);
            }

            if (m_World->HasComponent<ECS::LightComponent>(entity)) {
                const auto* light = m_World->GetComponent<ECS::LightComponent>(entity);
                entityJson["light"] = SerializeLightComponent(*light);
            }

            if (m_World->HasComponent<ECS::NotesComponent>(entity)) {
                const auto* notes = m_World->GetComponent<ECS::NotesComponent>(entity);
                entityJson["notes"] = SerializeNotesComponent(*notes);
            }

            if (m_World->HasComponent<ECS::TextComponent>(entity)) {
                const auto* text = m_World->GetComponent<ECS::TextComponent>(entity);
                entityJson["text"] = SerializeTextComponent(*text);
            }

            if (m_World->HasComponent<ECS::CameraComponent>(entity)) {
                const auto* camera = m_World->GetComponent<ECS::CameraComponent>(entity);
                entityJson["camera"] = SerializeCameraComponent(*camera);
            }

            if (m_World->HasComponent<ECS::WeatherZoneComponent>(entity)) {
                const auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
                entityJson["weatherZone"] = SerializeWeatherZoneComponent(*zone);
            }

            if (m_World->HasComponent<ECS::WaterVolumeComponent>(entity)) {
                const auto* volume = m_World->GetComponent<ECS::WaterVolumeComponent>(entity);
                entityJson["waterVolume"] = SerializeWaterVolumeComponent(*volume);
            }

            if (m_World->HasComponent<ECS::ShrubVolumeComponent>(entity)) {
                const auto* shrub = m_World->GetComponent<ECS::ShrubVolumeComponent>(entity);
                entityJson["shrubVolume"] = SerializeShrubVolumeComponent(*shrub);
            }

            if (m_World->HasComponent<ECS::TreeVolumeComponent>(entity)) {
                const auto* tree = m_World->GetComponent<ECS::TreeVolumeComponent>(entity);
                entityJson["treeVolume"] = SerializeTreeVolumeComponent(*tree);
            }

            if (m_World->HasComponent<ECS::TerrainComponent>(entity)) {
                const auto* terrain = m_World->GetComponent<ECS::TerrainComponent>(entity);
                entityJson["terrain"] = SerializeTerrainComponent(*terrain);
            }

            if (m_World->HasComponent<ECS::Terrain2DComponent>(entity)) {
                const auto* terrain2d = m_World->GetComponent<ECS::Terrain2DComponent>(entity);
                entityJson["terrain2d"] = SerializeTerrain2DComponent(*terrain2d);
            }

            if (m_World->HasComponent<ECS::CameraTriggerComponent>(entity)) {
                const auto* trigger = m_World->GetComponent<ECS::CameraTriggerComponent>(entity);
                entityJson["cameraTrigger"] = SerializeCameraTriggerComponent(*trigger);
            }

            if (m_World->HasComponent<ECS::TemperatureZoneComponent>(entity)) {
                const auto* zone = m_World->GetComponent<ECS::TemperatureZoneComponent>(entity);
                entityJson["temperatureZone"] = SerializeTemperatureZoneComponent(*zone);
            }

            if (m_World->HasComponent<ECS::GravityZoneComponent>(entity)) {
                const auto* zone = m_World->GetComponent<ECS::GravityZoneComponent>(entity);
                entityJson["gravityZone"] = SerializeGravityZoneComponent(*zone);
            }

            if (m_World->HasComponent<ECS::FluidVolumeComponent>(entity)) {
                const auto* vol = m_World->GetComponent<ECS::FluidVolumeComponent>(entity);
                entityJson["fluidVolume"] = SerializeFluidVolumeComponent(*vol);
            }

            // Character controllers
            if (m_World->HasComponent<ECS::Platformer2DController>(entity)) {
                entityJson["platformer2D"] = SerializePlatformer2D(*m_World->GetComponent<ECS::Platformer2DController>(entity));
            }
            if (m_World->HasComponent<ECS::TopDown2DController>(entity)) {
                entityJson["topDown2D"] = SerializeTopDown2D(*m_World->GetComponent<ECS::TopDown2DController>(entity));
            }
            if (m_World->HasComponent<ECS::TopDown3DController>(entity)) {
                entityJson["topDown3D"] = SerializeTopDown3D(*m_World->GetComponent<ECS::TopDown3DController>(entity));
            }
            if (m_World->HasComponent<ECS::ThirdPersonController>(entity)) {
                entityJson["thirdPerson"] = SerializeThirdPerson(*m_World->GetComponent<ECS::ThirdPersonController>(entity));
            }
            if (m_World->HasComponent<ECS::FirstPersonController>(entity)) {
                entityJson["firstPerson"] = SerializeFirstPerson(*m_World->GetComponent<ECS::FirstPersonController>(entity));
            }
            if (m_World->HasComponent<ECS::VehicleController>(entity)) {
                entityJson["vehicle"] = SerializeVehicle(*m_World->GetComponent<ECS::VehicleController>(entity));
            }
            if (m_World->HasComponent<ECS::SurfaceAlignedController>(entity)) {
                entityJson["surfaceAligned"] = SerializeSurfaceAligned(*m_World->GetComponent<ECS::SurfaceAlignedController>(entity));
            }
            if (m_World->HasComponent<ECS::NetworkIdentityComponent>(entity)) {
                entityJson["networkIdentity"] = SerializeNetworkIdentityComponent(*m_World->GetComponent<ECS::NetworkIdentityComponent>(entity));
            }
            if (m_World->HasComponent<ECS::NetworkTransformComponent>(entity)) {
                entityJson["networkTransform"] = SerializeNetworkTransformComponent(*m_World->GetComponent<ECS::NetworkTransformComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PossessableComponent>(entity)) {
                entityJson["possessable"] = SerializePossessable(*m_World->GetComponent<ECS::PossessableComponent>(entity));
            }
            if (m_World->HasComponent<ECS::LockComponent>(entity)) {
                entityJson["lock"] = SerializeLockComponent(*m_World->GetComponent<ECS::LockComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PushableComponent>(entity)) {
                entityJson["pushable"] = SerializePushableComponent(*m_World->GetComponent<ECS::PushableComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SwitchComponent>(entity)) {
                entityJson["switch"] = SerializeSwitchComponent(*m_World->GetComponent<ECS::SwitchComponent>(entity));
            }
            if (m_World->HasComponent<ECS::GoalZoneComponent>(entity)) {
                entityJson["goalZone"] = SerializeGoalZoneComponent(*m_World->GetComponent<ECS::GoalZoneComponent>(entity));
            }
            if (m_World->HasComponent<ECS::ConveyorComponent>(entity)) {
                entityJson["conveyor"] = SerializeConveyorComponent(*m_World->GetComponent<ECS::ConveyorComponent>(entity));
            }
            if (m_World->HasComponent<ECS::TeleporterComponent>(entity)) {
                entityJson["teleporter"] = SerializeTeleporterComponent(*m_World->GetComponent<ECS::TeleporterComponent>(entity));
            }
            if (m_World->HasComponent<ECS::DestructibleComponent>(entity)) {
                entityJson["destructible"] = SerializeDestructibleComponent(*m_World->GetComponent<ECS::DestructibleComponent>(entity));
            }
            if (m_World->HasComponent<ECS::CurlNoiseFieldComponent>(entity)) {
                entityJson["curlNoiseField"] = SerializeCurlNoiseFieldComponent(*m_World->GetComponent<ECS::CurlNoiseFieldComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FractureConfigComponent>(entity)) {
                entityJson["fractureConfig"] = SerializeFractureConfigComponent(*m_World->GetComponent<ECS::FractureConfigComponent>(entity));
            }
            if (m_World->HasComponent<ECS::MovingPlatformComponent>(entity)) {
                entityJson["movingPlatform"] = SerializeMovingPlatformComponent(*m_World->GetComponent<ECS::MovingPlatformComponent>(entity));
            }
            if (m_World->HasComponent<ECS::ScriptComponent>(entity)) {
                entityJson["scriptComponent"] = SerializeScriptComponent(*m_World->GetComponent<ECS::ScriptComponent>(entity));
            }

            // Hierarchy
            if (m_World->HasComponent<ECS::ParentComponent>(entity)) {
                entityJson["parent"] = static_cast<u64>(m_World->GetComponent<ECS::ParentComponent>(entity)->parent);
            }

            // Prefab instance link
            if (m_World->HasComponent<Assets::PrefabInstanceComponent>(entity)) {
                auto* pi = m_World->GetComponent<Assets::PrefabInstanceComponent>(entity);
                json piJson;
                piJson["prefabId"] = pi->prefabId;
                piJson["prefabPath"] = pi->prefabPath;
                entityJson["prefabInstance"] = piJson;
            }

            // IK Components
            if (m_World->HasComponent<ECS::LookAtIKComponent>(entity)) {
                auto* ik = m_World->GetComponent<ECS::LookAtIKComponent>(entity);
                json ikJson;
                ikJson["headBone"] = ik->headBoneName;
                ikJson["neckBone"] = ik->neckBoneName;
                ikJson["targetEntity"] = static_cast<u64>(ik->targetEntity);
                ikJson["targetPos"] = { ik->targetWorldPos.x, ik->targetWorldPos.y, ik->targetWorldPos.z };
                ikJson["useEntityTarget"] = ik->useEntityTarget;
                ikJson["maxRotation"] = ik->maxRotation;
                ikJson["smoothSpeed"] = ik->smoothSpeed;
                ikJson["lookWeight"] = ik->lookWeight;
                entityJson["lookAtIK"] = ikJson;
            }
            if (m_World->HasComponent<ECS::InteractionIKComponent>(entity)) {
                auto* ik = m_World->GetComponent<ECS::InteractionIKComponent>(entity);
                json ikJson;
                ikJson["handBone"] = ik->handBoneName;
                ikJson["elbowBone"] = ik->elbowBoneName;
                ikJson["shoulderBone"] = ik->shoulderBoneName;
                ikJson["interactionRadius"] = ik->interactionRadius;
                ikJson["ikWeight"] = ik->ikWeight;
                ikJson["smoothSpeed"] = ik->smoothSpeed;
                ikJson["interactionTag"] = ik->interactionTag;
                entityJson["interactionIK"] = ikJson;
            }

            // Audio Components
            if (m_World->HasComponent<ECS::AudioSourceComponent>(entity)) {
                const auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
                entityJson["audioSource"] = SerializeAudioSourceComponent(*audio);
            }
            if (m_World->HasComponent<ECS::AudioListenerComponent>(entity)) {
                const auto* listener = m_World->GetComponent<ECS::AudioListenerComponent>(entity);
                entityJson["audioListener"] = SerializeAudioListenerComponent(*listener);
            }

            // Physics & Collision
            if (m_World->HasComponent<ECS::RigidbodyComponent>(entity)) {
                entityJson["rigidbody"] = SerializeRigidbodyComponent(*m_World->GetComponent<ECS::RigidbodyComponent>(entity));
            }
            if (m_World->HasComponent<ECS::BoxColliderComponent>(entity)) {
                entityJson["boxCollider"] = SerializeBoxColliderComponent(*m_World->GetComponent<ECS::BoxColliderComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PolygonCollider2DComponent>(entity)) {
                entityJson["polygonCollider2D"] = SerializePolygonCollider2DComponent(*m_World->GetComponent<ECS::PolygonCollider2DComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PerFrameColliderComponent>(entity)) {
                entityJson["perFrameCollider"] = SerializePerFrameColliderComponent(*m_World->GetComponent<ECS::PerFrameColliderComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SphereColliderComponent>(entity)) {
                entityJson["sphereCollider"] = SerializeSphereColliderComponent(*m_World->GetComponent<ECS::SphereColliderComponent>(entity));
            }
            if (m_World->HasComponent<ECS::CapsuleColliderComponent>(entity)) {
                entityJson["capsuleCollider"] = SerializeCapsuleColliderComponent(*m_World->GetComponent<ECS::CapsuleColliderComponent>(entity));
            }

            // Health & Damage
            if (m_World->HasComponent<ECS::HealthComponent>(entity)) {
                entityJson["health"] = SerializeHealthComponent(*m_World->GetComponent<ECS::HealthComponent>(entity));
            }
            if (m_World->HasComponent<ECS::DamageComponent>(entity)) {
                entityJson["damage"] = SerializeDamageComponent(*m_World->GetComponent<ECS::DamageComponent>(entity));
            }

            // Damage Resistance
            if (m_World->HasComponent<ECS::DamageResistanceComponent>(entity)) {
                entityJson["damageResistance"] = SerializeDamageResistanceComponent(*m_World->GetComponent<ECS::DamageResistanceComponent>(entity));
            }

            // Triggers & Interaction
            if (m_World->HasComponent<ECS::TriggerZoneComponent>(entity)) {
                entityJson["triggerZone"] = SerializeTriggerZoneComponent(*m_World->GetComponent<ECS::TriggerZoneComponent>(entity));
            }
            if (m_World->HasComponent<ECS::InteractableComponent>(entity)) {
                entityJson["interactable"] = SerializeInteractableComponent(*m_World->GetComponent<ECS::InteractableComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PickupComponent>(entity)) {
                entityJson["pickup"] = SerializePickupComponent(*m_World->GetComponent<ECS::PickupComponent>(entity));
            }

            // Tags, Layers, Billboard
            if (m_World->HasComponent<ECS::TagComponent>(entity)) {
                entityJson["tag"] = SerializeTagComponent(*m_World->GetComponent<ECS::TagComponent>(entity));
            }
            if (m_World->HasComponent<ECS::LayerComponent>(entity)) {
                entityJson["layer"] = SerializeLayerComponent(*m_World->GetComponent<ECS::LayerComponent>(entity));
            }
            if (m_World->HasComponent<ECS::BillboardComponent>(entity)) {
                entityJson["billboard"] = SerializeBillboardComponent(*m_World->GetComponent<ECS::BillboardComponent>(entity));
            }

            // Particles
            if (m_World->HasComponent<ECS::ParticleEmitterComponent>(entity)) {
                entityJson["particleEmitter"] = SerializeParticleEmitterComponent(*m_World->GetComponent<ECS::ParticleEmitterComponent>(entity));
            }

            // 2D Rendering
            if (m_World->HasComponent<ECS::Sprite2DComponent>(entity)) {
                entityJson["sprite2D"] = SerializeSprite2DComponent(*m_World->GetComponent<ECS::Sprite2DComponent>(entity));
            }
            if (m_World->HasComponent<ECS::AnimatedSprite2DComponent>(entity)) {
                entityJson["animatedSprite2D"] = SerializeAnimatedSprite2DComponent(*m_World->GetComponent<ECS::AnimatedSprite2DComponent>(entity));
            }
            if (m_World->HasComponent<ECS::TilemapComponent>(entity)) {
                entityJson["tilemap"] = SerializeTilemapComponent(*m_World->GetComponent<ECS::TilemapComponent>(entity));
            }
            if (m_World->HasComponent<ECS::Camera2DBoundsComponent>(entity)) {
                entityJson["camera2DBounds"] = SerializeCamera2DBoundsComponent(*m_World->GetComponent<ECS::Camera2DBoundsComponent>(entity));
            }

            // Logic
            if (m_World->HasComponent<ECS::StateMachineComponent>(entity)) {
                entityJson["stateMachine"] = SerializeStateMachineComponent(*m_World->GetComponent<ECS::StateMachineComponent>(entity));
            }
            if (m_World->HasComponent<ECS::DialogueComponent>(entity)) {
                entityJson["dialogue"] = SerializeDialogueComponent(*m_World->GetComponent<ECS::DialogueComponent>(entity));
            }
            if (m_World->HasComponent<ECS::DialogueBoxComponent>(entity)) {
                entityJson["dialogueBox"] = SerializeDialogueBoxComponent(*m_World->GetComponent<ECS::DialogueBoxComponent>(entity));
            }
            if (m_World->HasComponent<ECS::TweenComponent>(entity)) {
                entityJson["tween"] = SerializeTweenComponent(*m_World->GetComponent<ECS::TweenComponent>(entity));
            }
            if (m_World->HasComponent<ECS::VisualScriptComponent>(entity)) {
                entityJson["visualScript"] = SerializeVisualScriptComponent(*m_World->GetComponent<ECS::VisualScriptComponent>(entity));
            }

            // AI & Navigation
            if (m_World->HasComponent<ECS::AIControllerComponent>(entity)) {
                entityJson["aiController"] = SerializeAIControllerComponent(*m_World->GetComponent<ECS::AIControllerComponent>(entity));
            }
            if (m_World->HasComponent<ECS::BehaviorTreeComponent>(entity)) {
                entityJson["behaviorTree"] = SerializeBehaviorTreeComponent(*m_World->GetComponent<ECS::BehaviorTreeComponent>(entity));
            }
            if (m_World->HasComponent<ECS::QuestFlowComponent>(entity)) {
                entityJson["questFlow"] = SerializeQuestFlowComponent(*m_World->GetComponent<ECS::QuestFlowComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FollowTargetComponent>(entity)) {
                entityJson["followTarget"] = SerializeFollowTargetComponent(*m_World->GetComponent<ECS::FollowTargetComponent>(entity));
            }
            if (m_World->HasComponent<ECS::LookAtTargetComponent>(entity)) {
                entityJson["lookAtTarget"] = SerializeLookAtTargetComponent(*m_World->GetComponent<ECS::LookAtTargetComponent>(entity));
            }
            if (m_World->HasComponent<ECS::WaypointComponent>(entity)) {
                entityJson["waypoint"] = SerializeWaypointComponent(*m_World->GetComponent<ECS::WaypointComponent>(entity));
            }

            // Spawning & Timers
            if (m_World->HasComponent<ECS::SpawnPointComponent>(entity)) {
                entityJson["spawnPoint"] = SerializeSpawnPointComponent(*m_World->GetComponent<ECS::SpawnPointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::TimerComponent>(entity)) {
                entityJson["timer"] = SerializeTimerComponent(*m_World->GetComponent<ECS::TimerComponent>(entity));
            }

            // Inventory & Save Data
            if (m_World->HasComponent<ECS::InventoryComponent>(entity)) {
                entityJson["inventory"] = SerializeInventoryComponent(*m_World->GetComponent<ECS::InventoryComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SaveDataComponent>(entity)) {
                entityJson["saveData"] = SerializeSaveDataComponent(*m_World->GetComponent<ECS::SaveDataComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SaveLoadMenuComponent>(entity)) {
                entityJson["saveLoadMenu"] = SerializeSaveLoadMenuComponent(*m_World->GetComponent<ECS::SaveLoadMenuComponent>(entity));
            }

            // New Gameplay Components
            if (m_World->HasComponent<ECS::ResourceComponent>(entity)) {
                entityJson["resource"] = SerializeResourceComponent(*m_World->GetComponent<ECS::ResourceComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FootstepComponent>(entity)) {
                entityJson["footstep"] = SerializeFootstepComponent(*m_World->GetComponent<ECS::FootstepComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PoolableComponent>(entity)) {
                entityJson["poolable"] = SerializePoolableComponent(*m_World->GetComponent<ECS::PoolableComponent>(entity));
            }
            if (m_World->HasComponent<ECS::QuestStateComponent>(entity)) {
                entityJson["questState"] = SerializeQuestStateComponent(*m_World->GetComponent<ECS::QuestStateComponent>(entity));
            }
            if (m_World->HasComponent<ECS::HUDWidgetComponent>(entity)) {
                entityJson["hudWidget"] = SerializeHUDWidgetComponent(*m_World->GetComponent<ECS::HUDWidgetComponent>(entity));
            }
            if (m_World->HasComponent<GUI::UICanvasComponent>(entity)) {
                entityJson["uiCanvas"] = SerializeUICanvasComponent(*m_World->GetComponent<GUI::UICanvasComponent>(entity));
            }
            if (m_World->HasComponent<ECS::CinematicCameraComponent>(entity)) {
                entityJson["cinematicCamera"] = SerializeCinematicCameraComponent(*m_World->GetComponent<ECS::CinematicCameraComponent>(entity));
            }

            // Joint & Ragdoll Components
            if (m_World->HasComponent<ECS::DistanceJointComponent>(entity)) {
                entityJson["distanceJoint"] = SerializeDistanceJointComponent(*m_World->GetComponent<ECS::DistanceJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::HingeJointComponent>(entity)) {
                entityJson["hingeJoint"] = SerializeHingeJointComponent(*m_World->GetComponent<ECS::HingeJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::BallSocketJointComponent>(entity)) {
                entityJson["ballSocketJoint"] = SerializeBallSocketJointComponent(*m_World->GetComponent<ECS::BallSocketJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SpringJointComponent>(entity)) {
                entityJson["springJoint"] = SerializeSpringJointComponent(*m_World->GetComponent<ECS::SpringJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FixedJointComponent>(entity)) {
                entityJson["fixedJoint"] = SerializeFixedJointComponent(*m_World->GetComponent<ECS::FixedJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SliderJointComponent>(entity)) {
                entityJson["sliderJoint"] = SerializeSliderJointComponent(*m_World->GetComponent<ECS::SliderJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::RagdollComponent>(entity)) {
                entityJson["ragdoll"] = SerializeRagdollComponent(*m_World->GetComponent<ECS::RagdollComponent>(entity));
            }

            // Flower Components
            if (m_World->HasComponent<ECS::JellyMeshComponent>(entity)) {
                entityJson["jellyMesh"] = SerializeJellyMeshComponent(*m_World->GetComponent<ECS::JellyMeshComponent>(entity));
            }
            if (m_World->HasComponent<ECS::TetherComponent>(entity)) {
                entityJson["tether"] = SerializeTetherComponent(*m_World->GetComponent<ECS::TetherComponent>(entity));
            }
            if (m_World->HasComponent<ECS::GrabbableComponent>(entity)) {
                entityJson["grabbable"] = SerializeGrabbableComponent(*m_World->GetComponent<ECS::GrabbableComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FlowerStemComponent>(entity)) {
                entityJson["flowerStem"] = SerializeFlowerStemComponent(*m_World->GetComponent<ECS::FlowerStemComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FlowerParticleConfigComponent>(entity)) {
                entityJson["flowerParticleConfig"] = SerializeFlowerParticleConfigComponent(*m_World->GetComponent<ECS::FlowerParticleConfigComponent>(entity));
            }

            // LOD, Grass, Vegetation
            if (m_World->HasComponent<ECS::LODComponent>(entity)) {
                entityJson["lod"] = SerializeLODComponent(*m_World->GetComponent<ECS::LODComponent>(entity));
            }
            if (m_World->HasComponent<ECS::GrassVolumeComponent>(entity)) {
                entityJson["grassVolume"] = SerializeGrassVolumeComponent(*m_World->GetComponent<ECS::GrassVolumeComponent>(entity));
            }
            if (m_World->HasComponent<ECS::VegetationComponent>(entity)) {
                entityJson["vegetation"] = SerializeVegetationComponent(*m_World->GetComponent<ECS::VegetationComponent>(entity));
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
            json faces = json::array();
            for (const auto& p : m_SkyboxConfig.cubemapPaths) faces.push_back(p);
            skyboxJson["cubemapPaths"] = faces;
            sceneJson["skybox"] = skyboxJson;
        }

        // Serialize render settings
        sceneJson["renderSettings"] = Renderer::SerializeRenderSettings(m_RenderSettings);

        // Write to file
        std::ofstream file(filepath);
        if (!file.is_open()) {
            SerializationResult result;
            result.success = false;
            result.error = "Failed to open file for writing: " + filepath;
            result.filepath = filepath;
            return result;
        }

        if (options.prettyPrint) {
            file << sceneJson.dump(static_cast<int>(options.indentSize));
        } else {
            file << sceneJson.dump();
        }

        file.close();

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

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            result.error = "Failed to open file: " + filepath;
            return result;
        }

        json sceneJson;
        file >> sceneJson;
        file.close();

        // Deserialize skybox configuration (file-based load)
        if (sceneJson.contains("skybox")) {
            const auto& sj = sceneJson["skybox"];
            m_SkyboxConfig = Renderer::SkyboxConfig{};
            if (sj.contains("type")) m_SkyboxConfig.type = static_cast<Renderer::SkyboxType>(sj["type"].get<u32>());
            if (sj.contains("topColor")) { auto& a = sj["topColor"]; m_SkyboxConfig.topColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("bottomColor")) { auto& a = sj["bottomColor"]; m_SkyboxConfig.bottomColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("horizonColor")) { auto& a = sj["horizonColor"]; m_SkyboxConfig.horizonColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("solidColor")) { auto& a = sj["solidColor"]; m_SkyboxConfig.solidColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("rotation")) m_SkyboxConfig.rotation = sj["rotation"].get<f32>();
            if (sj.contains("sunDirection")) { auto& a = sj["sunDirection"]; m_SkyboxConfig.sunDirection = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("cubemapPaths") && sj["cubemapPaths"].is_array()) {
                for (usize i = 0; i < 6 && i < sj["cubemapPaths"].size(); ++i) {
                    m_SkyboxConfig.cubemapPaths[i] = sj["cubemapPaths"][i].get<std::string>();
                }
            }
        } else {
            m_SkyboxConfig = Renderer::SkyboxConfig{};
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

        for (const auto& entityJson : sceneJson["entities"]) {
            ECS::Entity entity = m_World->CreateEntity();
            result.entities.push_back(entity);

            // Track old-to-new ID mapping for hierarchy/reference remapping
            if (entityJson.contains("id")) {
                u64 oldId = entityJson["id"].get<u64>();
                oldToNew[oldId] = entity;
            }

            // Set root entity to first entity
            if (result.rootEntity == ECS::INVALID_ENTITY) {
                result.rootEntity = entity;
            }

            // Deserialize components
            if (entityJson.contains("name")) {
                auto name = DeserializeNameComponent(entityJson["name"]);
                m_World->AddComponent<ECS::NameComponent>(entity, name);
            }

            if (entityJson.contains("transform")) {
                auto transform = DeserializeTransformComponent(entityJson["transform"]);
                m_World->AddComponent<ECS::TransformComponent>(entity, transform);
            }

            if (entityJson.contains("material")) {
                auto material = DeserializeMaterialComponent(entityJson["material"]);
                m_World->AddComponent<ECS::MaterialComponent>(entity, material);
            }

            if (entityJson.contains("mesh")) {
                auto mesh = DeserializeMeshComponent(entityJson["mesh"]);
                if (mesh.IsValid()) {
                    m_World->AddComponent<ECS::MeshComponent>(entity, mesh);
                }
            }

            // Skeleton must be deserialized before animator
            std::shared_ptr<Animation::Skeleton> loadedSkeleton;
            if (entityJson.contains("skeleton")) {
                auto skelComp = DeserializeSkeletonComponent(entityJson["skeleton"]);
                loadedSkeleton = skelComp.skeleton;
                m_World->AddComponent<ECS::SkeletonComponent>(entity, skelComp);
            }

            if (entityJson.contains("animator")) {
                auto animComp = DeserializeAnimatorComponent(entityJson["animator"], loadedSkeleton);
                m_World->AddComponent<ECS::AnimatorComponent>(entity, std::move(animComp));
            }

            if (entityJson.contains("light")) {
                auto light = DeserializeLightComponent(entityJson["light"]);
                m_World->AddComponent<ECS::LightComponent>(entity, light);
            }

            if (entityJson.contains("notes")) {
                auto notes = DeserializeNotesComponent(entityJson["notes"]);
                m_World->AddComponent<ECS::NotesComponent>(entity, notes);
            }

            if (entityJson.contains("text")) {
                auto text = DeserializeTextComponent(entityJson["text"]);
                m_World->AddComponent<ECS::TextComponent>(entity, text);
            }

            if (entityJson.contains("camera")) {
                auto camera = DeserializeCameraComponent(entityJson["camera"]);
                m_World->AddComponent<ECS::CameraComponent>(entity, camera);
            }

            if (entityJson.contains("weatherZone")) {
                auto zone = DeserializeWeatherZoneComponent(entityJson["weatherZone"]);
                m_World->AddComponent<ECS::WeatherZoneComponent>(entity, zone);
            }

            if (entityJson.contains("waterVolume")) {
                auto volume = DeserializeWaterVolumeComponent(entityJson["waterVolume"]);
                m_World->AddComponent<ECS::WaterVolumeComponent>(entity, volume);
            }

            if (entityJson.contains("shrubVolume")) {
                auto shrub = DeserializeShrubVolumeComponent(entityJson["shrubVolume"]);
                m_World->AddComponent<ECS::ShrubVolumeComponent>(entity, shrub);
            }

            if (entityJson.contains("treeVolume")) {
                auto tree = DeserializeTreeVolumeComponent(entityJson["treeVolume"]);
                m_World->AddComponent<ECS::TreeVolumeComponent>(entity, tree);
            }

            if (entityJson.contains("terrain")) {
                auto terrain = DeserializeTerrainComponent(entityJson["terrain"]);
                m_World->AddComponent<ECS::TerrainComponent>(entity, terrain);
            }

            if (entityJson.contains("terrain2d")) {
                auto terrain2d = DeserializeTerrain2DComponent(entityJson["terrain2d"]);
                m_World->AddComponent<ECS::Terrain2DComponent>(entity, terrain2d);
            }

            if (entityJson.contains("cameraTrigger")) {
                auto trigger = DeserializeCameraTriggerComponent(entityJson["cameraTrigger"]);
                m_World->AddComponent<ECS::CameraTriggerComponent>(entity, trigger);
            }

            if (entityJson.contains("temperatureZone")) {
                auto zone = DeserializeTemperatureZoneComponent(entityJson["temperatureZone"]);
                m_World->AddComponent<ECS::TemperatureZoneComponent>(entity, zone);
            }

            if (entityJson.contains("gravityZone")) {
                auto zone = DeserializeGravityZoneComponent(entityJson["gravityZone"]);
                m_World->AddComponent<ECS::GravityZoneComponent>(entity, zone);
            }

            if (entityJson.contains("fluidVolume")) {
                auto vol = DeserializeFluidVolumeComponent(entityJson["fluidVolume"]);
                m_World->AddComponent<ECS::FluidVolumeComponent>(entity, vol);
            }

            // Character controllers
            if (entityJson.contains("platformer2D")) {
                m_World->AddComponent<ECS::Platformer2DController>(entity, DeserializePlatformer2D(entityJson["platformer2D"]));
            }
            if (entityJson.contains("topDown2D")) {
                m_World->AddComponent<ECS::TopDown2DController>(entity, DeserializeTopDown2D(entityJson["topDown2D"]));
            }
            if (entityJson.contains("topDown3D")) {
                m_World->AddComponent<ECS::TopDown3DController>(entity, DeserializeTopDown3D(entityJson["topDown3D"]));
            }
            if (entityJson.contains("thirdPerson")) {
                m_World->AddComponent<ECS::ThirdPersonController>(entity, DeserializeThirdPerson(entityJson["thirdPerson"]));
            }
            if (entityJson.contains("firstPerson")) {
                m_World->AddComponent<ECS::FirstPersonController>(entity, DeserializeFirstPerson(entityJson["firstPerson"]));
            }
            if (entityJson.contains("vehicle")) {
                m_World->AddComponent<ECS::VehicleController>(entity, DeserializeVehicle(entityJson["vehicle"]));
            }
            if (entityJson.contains("surfaceAligned")) {
                m_World->AddComponent<ECS::SurfaceAlignedController>(entity, DeserializeSurfaceAligned(entityJson["surfaceAligned"]));
            }
            if (entityJson.contains("networkIdentity")) {
                m_World->AddComponent<ECS::NetworkIdentityComponent>(entity, DeserializeNetworkIdentityComponent(entityJson["networkIdentity"]));
            }
            if (entityJson.contains("networkTransform")) {
                m_World->AddComponent<ECS::NetworkTransformComponent>(entity, DeserializeNetworkTransformComponent(entityJson["networkTransform"]));
            }
            if (entityJson.contains("possessable")) {
                m_World->AddComponent<ECS::PossessableComponent>(entity, DeserializePossessable(entityJson["possessable"]));
            }
            if (entityJson.contains("lock")) {
                m_World->AddComponent<ECS::LockComponent>(entity, DeserializeLockComponent(entityJson["lock"]));
            }
            if (entityJson.contains("pushable")) {
                m_World->AddComponent<ECS::PushableComponent>(entity, DeserializePushableComponent(entityJson["pushable"]));
            }
            if (entityJson.contains("switch")) {
                m_World->AddComponent<ECS::SwitchComponent>(entity, DeserializeSwitchComponent(entityJson["switch"]));
            }
            if (entityJson.contains("goalZone")) {
                m_World->AddComponent<ECS::GoalZoneComponent>(entity, DeserializeGoalZoneComponent(entityJson["goalZone"]));
            }
            if (entityJson.contains("conveyor")) {
                m_World->AddComponent<ECS::ConveyorComponent>(entity, DeserializeConveyorComponent(entityJson["conveyor"]));
            }
            if (entityJson.contains("teleporter")) {
                m_World->AddComponent<ECS::TeleporterComponent>(entity, DeserializeTeleporterComponent(entityJson["teleporter"]));
            }
            if (entityJson.contains("destructible")) {
                m_World->AddComponent<ECS::DestructibleComponent>(entity, DeserializeDestructibleComponent(entityJson["destructible"]));
            }
            if (entityJson.contains("curlNoiseField")) {
                m_World->AddComponent<ECS::CurlNoiseFieldComponent>(entity, DeserializeCurlNoiseFieldComponent(entityJson["curlNoiseField"]));
            }
            if (entityJson.contains("fractureConfig")) {
                m_World->AddComponent<ECS::FractureConfigComponent>(entity, DeserializeFractureConfigComponent(entityJson["fractureConfig"]));
            }
            if (entityJson.contains("movingPlatform")) {
                m_World->AddComponent<ECS::MovingPlatformComponent>(entity, DeserializeMovingPlatformComponent(entityJson["movingPlatform"]));
            }
            if (entityJson.contains("scriptComponent")) {
                m_World->AddComponent<ECS::ScriptComponent>(entity, DeserializeScriptComponent(entityJson["scriptComponent"]));
            }

            // Hierarchy
            if (entityJson.contains("parent")) {
                auto& pc = m_World->AddComponent<ECS::ParentComponent>(entity);
                pc.parent = static_cast<ECS::Entity>(entityJson["parent"].get<u64>());
            }

            // Prefab instance link
            if (entityJson.contains("prefabInstance")) {
                auto& pi = m_World->AddComponent<Assets::PrefabInstanceComponent>(entity);
                const auto& piJson = entityJson["prefabInstance"];
                pi.prefabId = piJson.value("prefabId", static_cast<u64>(0));
                pi.prefabPath = piJson.value("prefabPath", std::string(""));
            }

            // IK Components
            if (entityJson.contains("lookAtIK")) {
                auto& ik = m_World->AddComponent<ECS::LookAtIKComponent>(entity);
                auto& ikJson = entityJson["lookAtIK"];
                if (ikJson.contains("headBone")) ik.headBoneName = ikJson["headBone"].get<std::string>();
                if (ikJson.contains("neckBone")) ik.neckBoneName = ikJson["neckBone"].get<std::string>();
                if (ikJson.contains("targetEntity")) ik.targetEntity = static_cast<ECS::Entity>(ikJson["targetEntity"].get<u64>());
                if (ikJson.contains("targetPos")) {
                    auto arr = ikJson["targetPos"];
                    ik.targetWorldPos = Math::Vector3(arr[0].get<f32>(), arr[1].get<f32>(), arr[2].get<f32>());
                }
                if (ikJson.contains("useEntityTarget")) ik.useEntityTarget = ikJson["useEntityTarget"].get<bool>();
                if (ikJson.contains("maxRotation")) ik.maxRotation = ikJson["maxRotation"].get<f32>();
                if (ikJson.contains("smoothSpeed")) ik.smoothSpeed = ikJson["smoothSpeed"].get<f32>();
                if (ikJson.contains("lookWeight")) ik.lookWeight = ikJson["lookWeight"].get<f32>();
            }
            if (entityJson.contains("interactionIK")) {
                auto& ik = m_World->AddComponent<ECS::InteractionIKComponent>(entity);
                auto& ikJson = entityJson["interactionIK"];
                if (ikJson.contains("handBone")) ik.handBoneName = ikJson["handBone"].get<std::string>();
                if (ikJson.contains("elbowBone")) ik.elbowBoneName = ikJson["elbowBone"].get<std::string>();
                if (ikJson.contains("shoulderBone")) ik.shoulderBoneName = ikJson["shoulderBone"].get<std::string>();
                if (ikJson.contains("interactionRadius")) ik.interactionRadius = ikJson["interactionRadius"].get<f32>();
                if (ikJson.contains("ikWeight")) ik.ikWeight = ikJson["ikWeight"].get<f32>();
                if (ikJson.contains("smoothSpeed")) ik.smoothSpeed = ikJson["smoothSpeed"].get<f32>();
                if (ikJson.contains("interactionTag")) ik.interactionTag = ikJson["interactionTag"].get<std::string>();
            }

            // Audio Components
            if (entityJson.contains("audioSource")) {
                auto audio = DeserializeAudioSourceComponent(entityJson["audioSource"]);
                m_World->AddComponent<ECS::AudioSourceComponent>(entity, audio);
            }
            if (entityJson.contains("audioListener")) {
                auto listener = DeserializeAudioListenerComponent(entityJson["audioListener"]);
                m_World->AddComponent<ECS::AudioListenerComponent>(entity, listener);
            }

            // Physics & Collision
            if (entityJson.contains("rigidbody")) {
                m_World->AddComponent<ECS::RigidbodyComponent>(entity, DeserializeRigidbodyComponent(entityJson["rigidbody"]));
            }
            if (entityJson.contains("boxCollider")) {
                m_World->AddComponent<ECS::BoxColliderComponent>(entity, DeserializeBoxColliderComponent(entityJson["boxCollider"]));
            }
            if (entityJson.contains("polygonCollider2D")) {
                m_World->AddComponent<ECS::PolygonCollider2DComponent>(entity, DeserializePolygonCollider2DComponent(entityJson["polygonCollider2D"]));
            }
            if (entityJson.contains("perFrameCollider")) {
                m_World->AddComponent<ECS::PerFrameColliderComponent>(entity, DeserializePerFrameColliderComponent(entityJson["perFrameCollider"]));
            }
            if (entityJson.contains("sphereCollider")) {
                m_World->AddComponent<ECS::SphereColliderComponent>(entity, DeserializeSphereColliderComponent(entityJson["sphereCollider"]));
            }
            if (entityJson.contains("capsuleCollider")) {
                m_World->AddComponent<ECS::CapsuleColliderComponent>(entity, DeserializeCapsuleColliderComponent(entityJson["capsuleCollider"]));
            }

            // Health & Damage
            if (entityJson.contains("health")) {
                m_World->AddComponent<ECS::HealthComponent>(entity, DeserializeHealthComponent(entityJson["health"]));
            }
            if (entityJson.contains("damage")) {
                m_World->AddComponent<ECS::DamageComponent>(entity, DeserializeDamageComponent(entityJson["damage"]));
            }
            if (entityJson.contains("damageResistance")) {
                m_World->AddComponent<ECS::DamageResistanceComponent>(entity, DeserializeDamageResistanceComponent(entityJson["damageResistance"]));
            }

            // Triggers & Interaction
            if (entityJson.contains("triggerZone")) {
                m_World->AddComponent<ECS::TriggerZoneComponent>(entity, DeserializeTriggerZoneComponent(entityJson["triggerZone"]));
            }
            if (entityJson.contains("interactable")) {
                m_World->AddComponent<ECS::InteractableComponent>(entity, DeserializeInteractableComponent(entityJson["interactable"]));
            }
            if (entityJson.contains("pickup")) {
                m_World->AddComponent<ECS::PickupComponent>(entity, DeserializePickupComponent(entityJson["pickup"]));
            }

            // Tags, Layers, Billboard
            if (entityJson.contains("tag")) {
                m_World->AddComponent<ECS::TagComponent>(entity, DeserializeTagComponent(entityJson["tag"]));
            }
            if (entityJson.contains("layer")) {
                m_World->AddComponent<ECS::LayerComponent>(entity, DeserializeLayerComponent(entityJson["layer"]));
            }
            if (entityJson.contains("billboard")) {
                m_World->AddComponent<ECS::BillboardComponent>(entity, DeserializeBillboardComponent(entityJson["billboard"]));
            }

            // Particles
            if (entityJson.contains("particleEmitter")) {
                m_World->AddComponent<ECS::ParticleEmitterComponent>(entity, DeserializeParticleEmitterComponent(entityJson["particleEmitter"]));
            }

            // 2D Rendering
            if (entityJson.contains("sprite2D")) {
                m_World->AddComponent<ECS::Sprite2DComponent>(entity, DeserializeSprite2DComponent(entityJson["sprite2D"]));
            }
            if (entityJson.contains("animatedSprite2D")) {
                m_World->AddComponent<ECS::AnimatedSprite2DComponent>(entity, DeserializeAnimatedSprite2DComponent(entityJson["animatedSprite2D"]));
            }
            if (entityJson.contains("tilemap")) {
                m_World->AddComponent<ECS::TilemapComponent>(entity, DeserializeTilemapComponent(entityJson["tilemap"]));
            }
            if (entityJson.contains("camera2DBounds")) {
                m_World->AddComponent<ECS::Camera2DBoundsComponent>(entity, DeserializeCamera2DBoundsComponent(entityJson["camera2DBounds"]));
            }

            // Logic
            if (entityJson.contains("stateMachine")) {
                m_World->AddComponent<ECS::StateMachineComponent>(entity, DeserializeStateMachineComponent(entityJson["stateMachine"]));
            }
            if (entityJson.contains("dialogue")) {
                m_World->AddComponent<ECS::DialogueComponent>(entity, DeserializeDialogueComponent(entityJson["dialogue"]));
            }
            if (entityJson.contains("dialogueBox")) {
                m_World->AddComponent<ECS::DialogueBoxComponent>(entity, DeserializeDialogueBoxComponent(entityJson["dialogueBox"]));
            }
            if (entityJson.contains("tween")) {
                m_World->AddComponent<ECS::TweenComponent>(entity, DeserializeTweenComponent(entityJson["tween"]));
            }
            if (entityJson.contains("visualScript")) {
                m_World->AddComponent<ECS::VisualScriptComponent>(entity, DeserializeVisualScriptComponent(entityJson["visualScript"]));
            }

            // AI & Navigation
            if (entityJson.contains("aiController")) {
                m_World->AddComponent<ECS::AIControllerComponent>(entity, DeserializeAIControllerComponent(entityJson["aiController"]));
            }
            if (entityJson.contains("behaviorTree")) {
                m_World->AddComponent<ECS::BehaviorTreeComponent>(entity, DeserializeBehaviorTreeComponent(entityJson["behaviorTree"]));
            }
            if (entityJson.contains("questFlow")) {
                m_World->AddComponent<ECS::QuestFlowComponent>(entity, DeserializeQuestFlowComponent(entityJson["questFlow"]));
            }
            if (entityJson.contains("followTarget")) {
                m_World->AddComponent<ECS::FollowTargetComponent>(entity, DeserializeFollowTargetComponent(entityJson["followTarget"]));
            }
            if (entityJson.contains("lookAtTarget")) {
                m_World->AddComponent<ECS::LookAtTargetComponent>(entity, DeserializeLookAtTargetComponent(entityJson["lookAtTarget"]));
            }
            if (entityJson.contains("waypoint")) {
                m_World->AddComponent<ECS::WaypointComponent>(entity, DeserializeWaypointComponent(entityJson["waypoint"]));
            }

            // Spawning & Timers
            if (entityJson.contains("spawnPoint")) {
                m_World->AddComponent<ECS::SpawnPointComponent>(entity, DeserializeSpawnPointComponent(entityJson["spawnPoint"]));
            }
            if (entityJson.contains("timer")) {
                m_World->AddComponent<ECS::TimerComponent>(entity, DeserializeTimerComponent(entityJson["timer"]));
            }

            // Inventory & Save Data
            if (entityJson.contains("inventory")) {
                m_World->AddComponent<ECS::InventoryComponent>(entity, DeserializeInventoryComponent(entityJson["inventory"]));
            }
            if (entityJson.contains("saveData")) {
                m_World->AddComponent<ECS::SaveDataComponent>(entity, DeserializeSaveDataComponent(entityJson["saveData"]));
            }
            if (entityJson.contains("saveLoadMenu")) {
                m_World->AddComponent<ECS::SaveLoadMenuComponent>(entity, DeserializeSaveLoadMenuComponent(entityJson["saveLoadMenu"]));
            }

            // New Gameplay Components
            if (entityJson.contains("resource")) {
                m_World->AddComponent<ECS::ResourceComponent>(entity, DeserializeResourceComponent(entityJson["resource"]));
            }
            if (entityJson.contains("footstep")) {
                m_World->AddComponent<ECS::FootstepComponent>(entity, DeserializeFootstepComponent(entityJson["footstep"]));
            }
            if (entityJson.contains("poolable")) {
                m_World->AddComponent<ECS::PoolableComponent>(entity, DeserializePoolableComponent(entityJson["poolable"]));
            }
            if (entityJson.contains("questState")) {
                m_World->AddComponent<ECS::QuestStateComponent>(entity, DeserializeQuestStateComponent(entityJson["questState"]));
            }
            if (entityJson.contains("hudWidget")) {
                m_World->AddComponent<ECS::HUDWidgetComponent>(entity, DeserializeHUDWidgetComponent(entityJson["hudWidget"]));
            }
            if (entityJson.contains("uiCanvas")) {
                m_World->AddComponent<GUI::UICanvasComponent>(entity, DeserializeUICanvasComponent(entityJson["uiCanvas"]));
            }
            if (entityJson.contains("cinematicCamera")) {
                m_World->AddComponent<ECS::CinematicCameraComponent>(entity, DeserializeCinematicCameraComponent(entityJson["cinematicCamera"]));
            }

            // Joint & Ragdoll Components
            if (entityJson.contains("distanceJoint")) {
                m_World->AddComponent<ECS::DistanceJointComponent>(entity, DeserializeDistanceJointComponent(entityJson["distanceJoint"]));
            }
            if (entityJson.contains("hingeJoint")) {
                m_World->AddComponent<ECS::HingeJointComponent>(entity, DeserializeHingeJointComponent(entityJson["hingeJoint"]));
            }
            if (entityJson.contains("ballSocketJoint")) {
                m_World->AddComponent<ECS::BallSocketJointComponent>(entity, DeserializeBallSocketJointComponent(entityJson["ballSocketJoint"]));
            }
            if (entityJson.contains("springJoint")) {
                m_World->AddComponent<ECS::SpringJointComponent>(entity, DeserializeSpringJointComponent(entityJson["springJoint"]));
            }
            if (entityJson.contains("fixedJoint")) {
                m_World->AddComponent<ECS::FixedJointComponent>(entity, DeserializeFixedJointComponent(entityJson["fixedJoint"]));
            }
            if (entityJson.contains("sliderJoint")) {
                m_World->AddComponent<ECS::SliderJointComponent>(entity, DeserializeSliderJointComponent(entityJson["sliderJoint"]));
            }
            if (entityJson.contains("ragdoll")) {
                m_World->AddComponent<ECS::RagdollComponent>(entity, DeserializeRagdollComponent(entityJson["ragdoll"]));
            }

            // Flower Components
            if (entityJson.contains("jellyMesh")) {
                m_World->AddComponent<ECS::JellyMeshComponent>(entity, DeserializeJellyMeshComponent(entityJson["jellyMesh"]));
            }
            if (entityJson.contains("tether")) {
                m_World->AddComponent<ECS::TetherComponent>(entity, DeserializeTetherComponent(entityJson["tether"]));
            }
            if (entityJson.contains("grabbable")) {
                m_World->AddComponent<ECS::GrabbableComponent>(entity, DeserializeGrabbableComponent(entityJson["grabbable"]));
            }
            if (entityJson.contains("flowerStem")) {
                m_World->AddComponent<ECS::FlowerStemComponent>(entity, DeserializeFlowerStemComponent(entityJson["flowerStem"]));
            }
            if (entityJson.contains("flowerParticleConfig")) {
                m_World->AddComponent<ECS::FlowerParticleConfigComponent>(entity, DeserializeFlowerParticleConfigComponent(entityJson["flowerParticleConfig"]));
            }

            // LOD, Grass, Vegetation
            if (entityJson.contains("lod")) {
                m_World->AddComponent<ECS::LODComponent>(entity, DeserializeLODComponent(entityJson["lod"]));
            }
            if (entityJson.contains("grassVolume")) {
                m_World->AddComponent<ECS::GrassVolumeComponent>(entity, DeserializeGrassVolumeComponent(entityJson["grassVolume"]));
            }
            if (entityJson.contains("vegetation")) {
                m_World->AddComponent<ECS::VegetationComponent>(entity, DeserializeVegetationComponent(entityJson["vegetation"]));
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
        }

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

            json entityJson;
            entityJson["id"] = static_cast<u64>(entity);

            // Serialize components
            if (m_World->HasComponent<ECS::NameComponent>(entity)) {
                const auto* name = m_World->GetComponent<ECS::NameComponent>(entity);
                entityJson["name"] = SerializeNameComponent(*name);
            }

            if (m_World->HasComponent<ECS::TransformComponent>(entity)) {
                const auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                entityJson["transform"] = SerializeTransformComponent(*transform);
            }

            if (m_World->HasComponent<ECS::MaterialComponent>(entity)) {
                const auto* material = m_World->GetComponent<ECS::MaterialComponent>(entity);
                entityJson["material"] = SerializeMaterialComponent(*material);
            }

            if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
                const auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
                entityJson["mesh"] = SerializeMeshComponent(*mesh, options.includeVertexData);
            }

            if (m_World->HasComponent<ECS::SkeletonComponent>(entity)) {
                const auto* skel = m_World->GetComponent<ECS::SkeletonComponent>(entity);
                entityJson["skeleton"] = SerializeSkeletonComponent(*skel);
            }

            if (m_World->HasComponent<ECS::AnimatorComponent>(entity)) {
                const auto* anim = m_World->GetComponent<ECS::AnimatorComponent>(entity);
                entityJson["animator"] = SerializeAnimatorComponent(*anim);
            }

            if (m_World->HasComponent<ECS::LightComponent>(entity)) {
                const auto* light = m_World->GetComponent<ECS::LightComponent>(entity);
                entityJson["light"] = SerializeLightComponent(*light);
            }

            if (m_World->HasComponent<ECS::NotesComponent>(entity)) {
                const auto* notes = m_World->GetComponent<ECS::NotesComponent>(entity);
                entityJson["notes"] = SerializeNotesComponent(*notes);
            }

            if (m_World->HasComponent<ECS::TextComponent>(entity)) {
                const auto* text = m_World->GetComponent<ECS::TextComponent>(entity);
                entityJson["text"] = SerializeTextComponent(*text);
            }

            if (m_World->HasComponent<ECS::CameraComponent>(entity)) {
                const auto* camera = m_World->GetComponent<ECS::CameraComponent>(entity);
                entityJson["camera"] = SerializeCameraComponent(*camera);
            }

            if (m_World->HasComponent<ECS::WeatherZoneComponent>(entity)) {
                const auto* zone = m_World->GetComponent<ECS::WeatherZoneComponent>(entity);
                entityJson["weatherZone"] = SerializeWeatherZoneComponent(*zone);
            }

            if (m_World->HasComponent<ECS::WaterVolumeComponent>(entity)) {
                const auto* volume = m_World->GetComponent<ECS::WaterVolumeComponent>(entity);
                entityJson["waterVolume"] = SerializeWaterVolumeComponent(*volume);
            }

            if (m_World->HasComponent<ECS::ShrubVolumeComponent>(entity)) {
                const auto* shrub = m_World->GetComponent<ECS::ShrubVolumeComponent>(entity);
                entityJson["shrubVolume"] = SerializeShrubVolumeComponent(*shrub);
            }

            if (m_World->HasComponent<ECS::TreeVolumeComponent>(entity)) {
                const auto* tree = m_World->GetComponent<ECS::TreeVolumeComponent>(entity);
                entityJson["treeVolume"] = SerializeTreeVolumeComponent(*tree);
            }

            if (m_World->HasComponent<ECS::TerrainComponent>(entity)) {
                const auto* terrain = m_World->GetComponent<ECS::TerrainComponent>(entity);
                entityJson["terrain"] = SerializeTerrainComponent(*terrain);
            }

            if (m_World->HasComponent<ECS::Terrain2DComponent>(entity)) {
                const auto* terrain2d = m_World->GetComponent<ECS::Terrain2DComponent>(entity);
                entityJson["terrain2d"] = SerializeTerrain2DComponent(*terrain2d);
            }

            if (m_World->HasComponent<ECS::CameraTriggerComponent>(entity)) {
                const auto* trigger = m_World->GetComponent<ECS::CameraTriggerComponent>(entity);
                entityJson["cameraTrigger"] = SerializeCameraTriggerComponent(*trigger);
            }

            if (m_World->HasComponent<ECS::TemperatureZoneComponent>(entity)) {
                const auto* zone = m_World->GetComponent<ECS::TemperatureZoneComponent>(entity);
                entityJson["temperatureZone"] = SerializeTemperatureZoneComponent(*zone);
            }

            if (m_World->HasComponent<ECS::GravityZoneComponent>(entity)) {
                const auto* zone = m_World->GetComponent<ECS::GravityZoneComponent>(entity);
                entityJson["gravityZone"] = SerializeGravityZoneComponent(*zone);
            }

            if (m_World->HasComponent<ECS::FluidVolumeComponent>(entity)) {
                const auto* vol = m_World->GetComponent<ECS::FluidVolumeComponent>(entity);
                entityJson["fluidVolume"] = SerializeFluidVolumeComponent(*vol);
            }

            // Character controllers
            if (m_World->HasComponent<ECS::Platformer2DController>(entity)) {
                entityJson["platformer2D"] = SerializePlatformer2D(*m_World->GetComponent<ECS::Platformer2DController>(entity));
            }
            if (m_World->HasComponent<ECS::TopDown2DController>(entity)) {
                entityJson["topDown2D"] = SerializeTopDown2D(*m_World->GetComponent<ECS::TopDown2DController>(entity));
            }
            if (m_World->HasComponent<ECS::TopDown3DController>(entity)) {
                entityJson["topDown3D"] = SerializeTopDown3D(*m_World->GetComponent<ECS::TopDown3DController>(entity));
            }
            if (m_World->HasComponent<ECS::ThirdPersonController>(entity)) {
                entityJson["thirdPerson"] = SerializeThirdPerson(*m_World->GetComponent<ECS::ThirdPersonController>(entity));
            }
            if (m_World->HasComponent<ECS::FirstPersonController>(entity)) {
                entityJson["firstPerson"] = SerializeFirstPerson(*m_World->GetComponent<ECS::FirstPersonController>(entity));
            }
            if (m_World->HasComponent<ECS::VehicleController>(entity)) {
                entityJson["vehicle"] = SerializeVehicle(*m_World->GetComponent<ECS::VehicleController>(entity));
            }
            if (m_World->HasComponent<ECS::SurfaceAlignedController>(entity)) {
                entityJson["surfaceAligned"] = SerializeSurfaceAligned(*m_World->GetComponent<ECS::SurfaceAlignedController>(entity));
            }
            if (m_World->HasComponent<ECS::NetworkIdentityComponent>(entity)) {
                entityJson["networkIdentity"] = SerializeNetworkIdentityComponent(*m_World->GetComponent<ECS::NetworkIdentityComponent>(entity));
            }
            if (m_World->HasComponent<ECS::NetworkTransformComponent>(entity)) {
                entityJson["networkTransform"] = SerializeNetworkTransformComponent(*m_World->GetComponent<ECS::NetworkTransformComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PossessableComponent>(entity)) {
                entityJson["possessable"] = SerializePossessable(*m_World->GetComponent<ECS::PossessableComponent>(entity));
            }
            if (m_World->HasComponent<ECS::LockComponent>(entity)) {
                entityJson["lock"] = SerializeLockComponent(*m_World->GetComponent<ECS::LockComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PushableComponent>(entity)) {
                entityJson["pushable"] = SerializePushableComponent(*m_World->GetComponent<ECS::PushableComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SwitchComponent>(entity)) {
                entityJson["switch"] = SerializeSwitchComponent(*m_World->GetComponent<ECS::SwitchComponent>(entity));
            }
            if (m_World->HasComponent<ECS::GoalZoneComponent>(entity)) {
                entityJson["goalZone"] = SerializeGoalZoneComponent(*m_World->GetComponent<ECS::GoalZoneComponent>(entity));
            }
            if (m_World->HasComponent<ECS::ConveyorComponent>(entity)) {
                entityJson["conveyor"] = SerializeConveyorComponent(*m_World->GetComponent<ECS::ConveyorComponent>(entity));
            }
            if (m_World->HasComponent<ECS::TeleporterComponent>(entity)) {
                entityJson["teleporter"] = SerializeTeleporterComponent(*m_World->GetComponent<ECS::TeleporterComponent>(entity));
            }
            if (m_World->HasComponent<ECS::DestructibleComponent>(entity)) {
                entityJson["destructible"] = SerializeDestructibleComponent(*m_World->GetComponent<ECS::DestructibleComponent>(entity));
            }
            if (m_World->HasComponent<ECS::CurlNoiseFieldComponent>(entity)) {
                entityJson["curlNoiseField"] = SerializeCurlNoiseFieldComponent(*m_World->GetComponent<ECS::CurlNoiseFieldComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FractureConfigComponent>(entity)) {
                entityJson["fractureConfig"] = SerializeFractureConfigComponent(*m_World->GetComponent<ECS::FractureConfigComponent>(entity));
            }
            if (m_World->HasComponent<ECS::MovingPlatformComponent>(entity)) {
                entityJson["movingPlatform"] = SerializeMovingPlatformComponent(*m_World->GetComponent<ECS::MovingPlatformComponent>(entity));
            }
            if (m_World->HasComponent<ECS::ScriptComponent>(entity)) {
                entityJson["scriptComponent"] = SerializeScriptComponent(*m_World->GetComponent<ECS::ScriptComponent>(entity));
            }

            // Hierarchy
            if (m_World->HasComponent<ECS::ParentComponent>(entity)) {
                entityJson["parent"] = static_cast<u64>(m_World->GetComponent<ECS::ParentComponent>(entity)->parent);
            }

            // Prefab instance link
            if (m_World->HasComponent<Assets::PrefabInstanceComponent>(entity)) {
                auto* pi = m_World->GetComponent<Assets::PrefabInstanceComponent>(entity);
                json piJson;
                piJson["prefabId"] = pi->prefabId;
                piJson["prefabPath"] = pi->prefabPath;
                entityJson["prefabInstance"] = piJson;
            }

            // IK Components
            if (m_World->HasComponent<ECS::LookAtIKComponent>(entity)) {
                auto* ik = m_World->GetComponent<ECS::LookAtIKComponent>(entity);
                json ikJson;
                ikJson["headBone"] = ik->headBoneName;
                ikJson["neckBone"] = ik->neckBoneName;
                ikJson["targetEntity"] = static_cast<u64>(ik->targetEntity);
                ikJson["targetPos"] = { ik->targetWorldPos.x, ik->targetWorldPos.y, ik->targetWorldPos.z };
                ikJson["useEntityTarget"] = ik->useEntityTarget;
                ikJson["maxRotation"] = ik->maxRotation;
                ikJson["smoothSpeed"] = ik->smoothSpeed;
                ikJson["lookWeight"] = ik->lookWeight;
                entityJson["lookAtIK"] = ikJson;
            }
            if (m_World->HasComponent<ECS::InteractionIKComponent>(entity)) {
                auto* ik = m_World->GetComponent<ECS::InteractionIKComponent>(entity);
                json ikJson;
                ikJson["handBone"] = ik->handBoneName;
                ikJson["elbowBone"] = ik->elbowBoneName;
                ikJson["shoulderBone"] = ik->shoulderBoneName;
                ikJson["interactionRadius"] = ik->interactionRadius;
                ikJson["ikWeight"] = ik->ikWeight;
                ikJson["smoothSpeed"] = ik->smoothSpeed;
                ikJson["interactionTag"] = ik->interactionTag;
                entityJson["interactionIK"] = ikJson;
            }

            // Audio Components
            if (m_World->HasComponent<ECS::AudioSourceComponent>(entity)) {
                const auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
                entityJson["audioSource"] = SerializeAudioSourceComponent(*audio);
            }
            if (m_World->HasComponent<ECS::AudioListenerComponent>(entity)) {
                const auto* listener = m_World->GetComponent<ECS::AudioListenerComponent>(entity);
                entityJson["audioListener"] = SerializeAudioListenerComponent(*listener);
            }

            // Physics & Collision
            if (m_World->HasComponent<ECS::RigidbodyComponent>(entity)) {
                entityJson["rigidbody"] = SerializeRigidbodyComponent(*m_World->GetComponent<ECS::RigidbodyComponent>(entity));
            }
            if (m_World->HasComponent<ECS::BoxColliderComponent>(entity)) {
                entityJson["boxCollider"] = SerializeBoxColliderComponent(*m_World->GetComponent<ECS::BoxColliderComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PolygonCollider2DComponent>(entity)) {
                entityJson["polygonCollider2D"] = SerializePolygonCollider2DComponent(*m_World->GetComponent<ECS::PolygonCollider2DComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PerFrameColliderComponent>(entity)) {
                entityJson["perFrameCollider"] = SerializePerFrameColliderComponent(*m_World->GetComponent<ECS::PerFrameColliderComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SphereColliderComponent>(entity)) {
                entityJson["sphereCollider"] = SerializeSphereColliderComponent(*m_World->GetComponent<ECS::SphereColliderComponent>(entity));
            }
            if (m_World->HasComponent<ECS::CapsuleColliderComponent>(entity)) {
                entityJson["capsuleCollider"] = SerializeCapsuleColliderComponent(*m_World->GetComponent<ECS::CapsuleColliderComponent>(entity));
            }

            // Health & Damage
            if (m_World->HasComponent<ECS::HealthComponent>(entity)) {
                entityJson["health"] = SerializeHealthComponent(*m_World->GetComponent<ECS::HealthComponent>(entity));
            }
            if (m_World->HasComponent<ECS::DamageComponent>(entity)) {
                entityJson["damage"] = SerializeDamageComponent(*m_World->GetComponent<ECS::DamageComponent>(entity));
            }

            // Damage Resistance
            if (m_World->HasComponent<ECS::DamageResistanceComponent>(entity)) {
                entityJson["damageResistance"] = SerializeDamageResistanceComponent(*m_World->GetComponent<ECS::DamageResistanceComponent>(entity));
            }

            // Triggers & Interaction
            if (m_World->HasComponent<ECS::TriggerZoneComponent>(entity)) {
                entityJson["triggerZone"] = SerializeTriggerZoneComponent(*m_World->GetComponent<ECS::TriggerZoneComponent>(entity));
            }
            if (m_World->HasComponent<ECS::InteractableComponent>(entity)) {
                entityJson["interactable"] = SerializeInteractableComponent(*m_World->GetComponent<ECS::InteractableComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PickupComponent>(entity)) {
                entityJson["pickup"] = SerializePickupComponent(*m_World->GetComponent<ECS::PickupComponent>(entity));
            }

            // Tags, Layers, Billboard
            if (m_World->HasComponent<ECS::TagComponent>(entity)) {
                entityJson["tag"] = SerializeTagComponent(*m_World->GetComponent<ECS::TagComponent>(entity));
            }
            if (m_World->HasComponent<ECS::LayerComponent>(entity)) {
                entityJson["layer"] = SerializeLayerComponent(*m_World->GetComponent<ECS::LayerComponent>(entity));
            }
            if (m_World->HasComponent<ECS::BillboardComponent>(entity)) {
                entityJson["billboard"] = SerializeBillboardComponent(*m_World->GetComponent<ECS::BillboardComponent>(entity));
            }

            // Particles
            if (m_World->HasComponent<ECS::ParticleEmitterComponent>(entity)) {
                entityJson["particleEmitter"] = SerializeParticleEmitterComponent(*m_World->GetComponent<ECS::ParticleEmitterComponent>(entity));
            }

            // 2D Rendering
            if (m_World->HasComponent<ECS::Sprite2DComponent>(entity)) {
                entityJson["sprite2D"] = SerializeSprite2DComponent(*m_World->GetComponent<ECS::Sprite2DComponent>(entity));
            }
            if (m_World->HasComponent<ECS::AnimatedSprite2DComponent>(entity)) {
                entityJson["animatedSprite2D"] = SerializeAnimatedSprite2DComponent(*m_World->GetComponent<ECS::AnimatedSprite2DComponent>(entity));
            }
            if (m_World->HasComponent<ECS::TilemapComponent>(entity)) {
                entityJson["tilemap"] = SerializeTilemapComponent(*m_World->GetComponent<ECS::TilemapComponent>(entity));
            }
            if (m_World->HasComponent<ECS::Camera2DBoundsComponent>(entity)) {
                entityJson["camera2DBounds"] = SerializeCamera2DBoundsComponent(*m_World->GetComponent<ECS::Camera2DBoundsComponent>(entity));
            }

            // Logic
            if (m_World->HasComponent<ECS::StateMachineComponent>(entity)) {
                entityJson["stateMachine"] = SerializeStateMachineComponent(*m_World->GetComponent<ECS::StateMachineComponent>(entity));
            }
            if (m_World->HasComponent<ECS::DialogueComponent>(entity)) {
                entityJson["dialogue"] = SerializeDialogueComponent(*m_World->GetComponent<ECS::DialogueComponent>(entity));
            }
            if (m_World->HasComponent<ECS::DialogueBoxComponent>(entity)) {
                entityJson["dialogueBox"] = SerializeDialogueBoxComponent(*m_World->GetComponent<ECS::DialogueBoxComponent>(entity));
            }
            if (m_World->HasComponent<ECS::TweenComponent>(entity)) {
                entityJson["tween"] = SerializeTweenComponent(*m_World->GetComponent<ECS::TweenComponent>(entity));
            }
            if (m_World->HasComponent<ECS::VisualScriptComponent>(entity)) {
                entityJson["visualScript"] = SerializeVisualScriptComponent(*m_World->GetComponent<ECS::VisualScriptComponent>(entity));
            }

            // AI & Navigation
            if (m_World->HasComponent<ECS::AIControllerComponent>(entity)) {
                entityJson["aiController"] = SerializeAIControllerComponent(*m_World->GetComponent<ECS::AIControllerComponent>(entity));
            }
            if (m_World->HasComponent<ECS::BehaviorTreeComponent>(entity)) {
                entityJson["behaviorTree"] = SerializeBehaviorTreeComponent(*m_World->GetComponent<ECS::BehaviorTreeComponent>(entity));
            }
            if (m_World->HasComponent<ECS::QuestFlowComponent>(entity)) {
                entityJson["questFlow"] = SerializeQuestFlowComponent(*m_World->GetComponent<ECS::QuestFlowComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FollowTargetComponent>(entity)) {
                entityJson["followTarget"] = SerializeFollowTargetComponent(*m_World->GetComponent<ECS::FollowTargetComponent>(entity));
            }
            if (m_World->HasComponent<ECS::LookAtTargetComponent>(entity)) {
                entityJson["lookAtTarget"] = SerializeLookAtTargetComponent(*m_World->GetComponent<ECS::LookAtTargetComponent>(entity));
            }
            if (m_World->HasComponent<ECS::WaypointComponent>(entity)) {
                entityJson["waypoint"] = SerializeWaypointComponent(*m_World->GetComponent<ECS::WaypointComponent>(entity));
            }

            // Spawning & Timers
            if (m_World->HasComponent<ECS::SpawnPointComponent>(entity)) {
                entityJson["spawnPoint"] = SerializeSpawnPointComponent(*m_World->GetComponent<ECS::SpawnPointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::TimerComponent>(entity)) {
                entityJson["timer"] = SerializeTimerComponent(*m_World->GetComponent<ECS::TimerComponent>(entity));
            }

            // Inventory & Save Data
            if (m_World->HasComponent<ECS::InventoryComponent>(entity)) {
                entityJson["inventory"] = SerializeInventoryComponent(*m_World->GetComponent<ECS::InventoryComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SaveDataComponent>(entity)) {
                entityJson["saveData"] = SerializeSaveDataComponent(*m_World->GetComponent<ECS::SaveDataComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SaveLoadMenuComponent>(entity)) {
                entityJson["saveLoadMenu"] = SerializeSaveLoadMenuComponent(*m_World->GetComponent<ECS::SaveLoadMenuComponent>(entity));
            }

            // New Gameplay Components
            if (m_World->HasComponent<ECS::ResourceComponent>(entity)) {
                entityJson["resource"] = SerializeResourceComponent(*m_World->GetComponent<ECS::ResourceComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FootstepComponent>(entity)) {
                entityJson["footstep"] = SerializeFootstepComponent(*m_World->GetComponent<ECS::FootstepComponent>(entity));
            }
            if (m_World->HasComponent<ECS::PoolableComponent>(entity)) {
                entityJson["poolable"] = SerializePoolableComponent(*m_World->GetComponent<ECS::PoolableComponent>(entity));
            }
            if (m_World->HasComponent<ECS::QuestStateComponent>(entity)) {
                entityJson["questState"] = SerializeQuestStateComponent(*m_World->GetComponent<ECS::QuestStateComponent>(entity));
            }
            if (m_World->HasComponent<ECS::HUDWidgetComponent>(entity)) {
                entityJson["hudWidget"] = SerializeHUDWidgetComponent(*m_World->GetComponent<ECS::HUDWidgetComponent>(entity));
            }
            if (m_World->HasComponent<GUI::UICanvasComponent>(entity)) {
                entityJson["uiCanvas"] = SerializeUICanvasComponent(*m_World->GetComponent<GUI::UICanvasComponent>(entity));
            }
            if (m_World->HasComponent<ECS::CinematicCameraComponent>(entity)) {
                entityJson["cinematicCamera"] = SerializeCinematicCameraComponent(*m_World->GetComponent<ECS::CinematicCameraComponent>(entity));
            }

            // Joint & Ragdoll Components
            if (m_World->HasComponent<ECS::DistanceJointComponent>(entity)) {
                entityJson["distanceJoint"] = SerializeDistanceJointComponent(*m_World->GetComponent<ECS::DistanceJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::HingeJointComponent>(entity)) {
                entityJson["hingeJoint"] = SerializeHingeJointComponent(*m_World->GetComponent<ECS::HingeJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::BallSocketJointComponent>(entity)) {
                entityJson["ballSocketJoint"] = SerializeBallSocketJointComponent(*m_World->GetComponent<ECS::BallSocketJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SpringJointComponent>(entity)) {
                entityJson["springJoint"] = SerializeSpringJointComponent(*m_World->GetComponent<ECS::SpringJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FixedJointComponent>(entity)) {
                entityJson["fixedJoint"] = SerializeFixedJointComponent(*m_World->GetComponent<ECS::FixedJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::SliderJointComponent>(entity)) {
                entityJson["sliderJoint"] = SerializeSliderJointComponent(*m_World->GetComponent<ECS::SliderJointComponent>(entity));
            }
            if (m_World->HasComponent<ECS::RagdollComponent>(entity)) {
                entityJson["ragdoll"] = SerializeRagdollComponent(*m_World->GetComponent<ECS::RagdollComponent>(entity));
            }

            // Flower Components
            if (m_World->HasComponent<ECS::JellyMeshComponent>(entity)) {
                entityJson["jellyMesh"] = SerializeJellyMeshComponent(*m_World->GetComponent<ECS::JellyMeshComponent>(entity));
            }
            if (m_World->HasComponent<ECS::TetherComponent>(entity)) {
                entityJson["tether"] = SerializeTetherComponent(*m_World->GetComponent<ECS::TetherComponent>(entity));
            }
            if (m_World->HasComponent<ECS::GrabbableComponent>(entity)) {
                entityJson["grabbable"] = SerializeGrabbableComponent(*m_World->GetComponent<ECS::GrabbableComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FlowerStemComponent>(entity)) {
                entityJson["flowerStem"] = SerializeFlowerStemComponent(*m_World->GetComponent<ECS::FlowerStemComponent>(entity));
            }
            if (m_World->HasComponent<ECS::FlowerParticleConfigComponent>(entity)) {
                entityJson["flowerParticleConfig"] = SerializeFlowerParticleConfigComponent(*m_World->GetComponent<ECS::FlowerParticleConfigComponent>(entity));
            }

            // LOD, Grass, Vegetation
            if (m_World->HasComponent<ECS::LODComponent>(entity)) {
                entityJson["lod"] = SerializeLODComponent(*m_World->GetComponent<ECS::LODComponent>(entity));
            }
            if (m_World->HasComponent<ECS::GrassVolumeComponent>(entity)) {
                entityJson["grassVolume"] = SerializeGrassVolumeComponent(*m_World->GetComponent<ECS::GrassVolumeComponent>(entity));
            }
            if (m_World->HasComponent<ECS::VegetationComponent>(entity)) {
                entityJson["vegetation"] = SerializeVegetationComponent(*m_World->GetComponent<ECS::VegetationComponent>(entity));
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
            json faces = json::array();
            for (const auto& p : m_SkyboxConfig.cubemapPaths) faces.push_back(p);
            skyboxJson["cubemapPaths"] = faces;
            sceneJson["skybox"] = skyboxJson;
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

    if (clearExisting) {
        m_World->Clear();
    }

    try {
        json sceneJson = json::parse(jsonString);

        // Deserialize skybox configuration (string-based load)
        if (sceneJson.contains("skybox")) {
            const auto& sj = sceneJson["skybox"];
            m_SkyboxConfig = Renderer::SkyboxConfig{};
            if (sj.contains("type")) m_SkyboxConfig.type = static_cast<Renderer::SkyboxType>(sj["type"].get<u32>());
            if (sj.contains("topColor")) { auto& a = sj["topColor"]; m_SkyboxConfig.topColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("bottomColor")) { auto& a = sj["bottomColor"]; m_SkyboxConfig.bottomColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("horizonColor")) { auto& a = sj["horizonColor"]; m_SkyboxConfig.horizonColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("solidColor")) { auto& a = sj["solidColor"]; m_SkyboxConfig.solidColor = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("rotation")) m_SkyboxConfig.rotation = sj["rotation"].get<f32>();
            if (sj.contains("sunDirection")) { auto& a = sj["sunDirection"]; m_SkyboxConfig.sunDirection = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (sj.contains("cubemapPaths") && sj["cubemapPaths"].is_array()) {
                for (usize i = 0; i < 6 && i < sj["cubemapPaths"].size(); ++i) {
                    m_SkyboxConfig.cubemapPaths[i] = sj["cubemapPaths"][i].get<std::string>();
                }
            }
        } else {
            m_SkyboxConfig = Renderer::SkyboxConfig{};
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

        // Deserialize entities
        if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
            result.error = "Invalid scene format: missing entities array";
            return result;
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

            if (result.rootEntity == ECS::INVALID_ENTITY) {
                result.rootEntity = entity;
            }

            // Deserialize components
            if (entityJson.contains("name")) {
                auto name = DeserializeNameComponent(entityJson["name"]);
                m_World->AddComponent<ECS::NameComponent>(entity, name);
            }

            if (entityJson.contains("transform")) {
                auto transform = DeserializeTransformComponent(entityJson["transform"]);
                m_World->AddComponent<ECS::TransformComponent>(entity, transform);
            }

            if (entityJson.contains("material")) {
                auto material = DeserializeMaterialComponent(entityJson["material"]);
                m_World->AddComponent<ECS::MaterialComponent>(entity, material);
            }

            if (entityJson.contains("mesh")) {
                auto mesh = DeserializeMeshComponent(entityJson["mesh"]);
                if (mesh.IsValid()) {
                    m_World->AddComponent<ECS::MeshComponent>(entity, mesh);
                }
            }

            // Skeleton must be deserialized before animator
            std::shared_ptr<Animation::Skeleton> loadedSkeleton;
            if (entityJson.contains("skeleton")) {
                auto skelComp = DeserializeSkeletonComponent(entityJson["skeleton"]);
                loadedSkeleton = skelComp.skeleton;
                m_World->AddComponent<ECS::SkeletonComponent>(entity, skelComp);
            }

            if (entityJson.contains("animator")) {
                auto animComp = DeserializeAnimatorComponent(entityJson["animator"], loadedSkeleton);
                m_World->AddComponent<ECS::AnimatorComponent>(entity, std::move(animComp));
            }

            if (entityJson.contains("light")) {
                auto light = DeserializeLightComponent(entityJson["light"]);
                m_World->AddComponent<ECS::LightComponent>(entity, light);
            }

            if (entityJson.contains("notes")) {
                auto notes = DeserializeNotesComponent(entityJson["notes"]);
                m_World->AddComponent<ECS::NotesComponent>(entity, notes);
            }

            if (entityJson.contains("text")) {
                auto text = DeserializeTextComponent(entityJson["text"]);
                m_World->AddComponent<ECS::TextComponent>(entity, text);
            }

            if (entityJson.contains("camera")) {
                auto camera = DeserializeCameraComponent(entityJson["camera"]);
                m_World->AddComponent<ECS::CameraComponent>(entity, camera);
            }

            if (entityJson.contains("weatherZone")) {
                auto zone = DeserializeWeatherZoneComponent(entityJson["weatherZone"]);
                m_World->AddComponent<ECS::WeatherZoneComponent>(entity, zone);
            }

            if (entityJson.contains("waterVolume")) {
                auto volume = DeserializeWaterVolumeComponent(entityJson["waterVolume"]);
                m_World->AddComponent<ECS::WaterVolumeComponent>(entity, volume);
            }

            if (entityJson.contains("shrubVolume")) {
                auto shrub = DeserializeShrubVolumeComponent(entityJson["shrubVolume"]);
                m_World->AddComponent<ECS::ShrubVolumeComponent>(entity, shrub);
            }

            if (entityJson.contains("treeVolume")) {
                auto tree = DeserializeTreeVolumeComponent(entityJson["treeVolume"]);
                m_World->AddComponent<ECS::TreeVolumeComponent>(entity, tree);
            }

            if (entityJson.contains("terrain")) {
                auto terrain = DeserializeTerrainComponent(entityJson["terrain"]);
                m_World->AddComponent<ECS::TerrainComponent>(entity, terrain);
            }

            if (entityJson.contains("terrain2d")) {
                auto terrain2d = DeserializeTerrain2DComponent(entityJson["terrain2d"]);
                m_World->AddComponent<ECS::Terrain2DComponent>(entity, terrain2d);
            }

            if (entityJson.contains("cameraTrigger")) {
                auto trigger = DeserializeCameraTriggerComponent(entityJson["cameraTrigger"]);
                m_World->AddComponent<ECS::CameraTriggerComponent>(entity, trigger);
            }

            if (entityJson.contains("temperatureZone")) {
                auto zone = DeserializeTemperatureZoneComponent(entityJson["temperatureZone"]);
                m_World->AddComponent<ECS::TemperatureZoneComponent>(entity, zone);
            }

            if (entityJson.contains("gravityZone")) {
                auto zone = DeserializeGravityZoneComponent(entityJson["gravityZone"]);
                m_World->AddComponent<ECS::GravityZoneComponent>(entity, zone);
            }

            if (entityJson.contains("fluidVolume")) {
                auto vol = DeserializeFluidVolumeComponent(entityJson["fluidVolume"]);
                m_World->AddComponent<ECS::FluidVolumeComponent>(entity, vol);
            }

            // Character controllers
            if (entityJson.contains("platformer2D")) {
                m_World->AddComponent<ECS::Platformer2DController>(entity, DeserializePlatformer2D(entityJson["platformer2D"]));
            }
            if (entityJson.contains("topDown2D")) {
                m_World->AddComponent<ECS::TopDown2DController>(entity, DeserializeTopDown2D(entityJson["topDown2D"]));
            }
            if (entityJson.contains("topDown3D")) {
                m_World->AddComponent<ECS::TopDown3DController>(entity, DeserializeTopDown3D(entityJson["topDown3D"]));
            }
            if (entityJson.contains("thirdPerson")) {
                m_World->AddComponent<ECS::ThirdPersonController>(entity, DeserializeThirdPerson(entityJson["thirdPerson"]));
            }
            if (entityJson.contains("firstPerson")) {
                m_World->AddComponent<ECS::FirstPersonController>(entity, DeserializeFirstPerson(entityJson["firstPerson"]));
            }
            if (entityJson.contains("vehicle")) {
                m_World->AddComponent<ECS::VehicleController>(entity, DeserializeVehicle(entityJson["vehicle"]));
            }
            if (entityJson.contains("surfaceAligned")) {
                m_World->AddComponent<ECS::SurfaceAlignedController>(entity, DeserializeSurfaceAligned(entityJson["surfaceAligned"]));
            }
            if (entityJson.contains("networkIdentity")) {
                m_World->AddComponent<ECS::NetworkIdentityComponent>(entity, DeserializeNetworkIdentityComponent(entityJson["networkIdentity"]));
            }
            if (entityJson.contains("networkTransform")) {
                m_World->AddComponent<ECS::NetworkTransformComponent>(entity, DeserializeNetworkTransformComponent(entityJson["networkTransform"]));
            }
            if (entityJson.contains("possessable")) {
                m_World->AddComponent<ECS::PossessableComponent>(entity, DeserializePossessable(entityJson["possessable"]));
            }
            if (entityJson.contains("lock")) {
                m_World->AddComponent<ECS::LockComponent>(entity, DeserializeLockComponent(entityJson["lock"]));
            }
            if (entityJson.contains("pushable")) {
                m_World->AddComponent<ECS::PushableComponent>(entity, DeserializePushableComponent(entityJson["pushable"]));
            }
            if (entityJson.contains("switch")) {
                m_World->AddComponent<ECS::SwitchComponent>(entity, DeserializeSwitchComponent(entityJson["switch"]));
            }
            if (entityJson.contains("goalZone")) {
                m_World->AddComponent<ECS::GoalZoneComponent>(entity, DeserializeGoalZoneComponent(entityJson["goalZone"]));
            }
            if (entityJson.contains("conveyor")) {
                m_World->AddComponent<ECS::ConveyorComponent>(entity, DeserializeConveyorComponent(entityJson["conveyor"]));
            }
            if (entityJson.contains("teleporter")) {
                m_World->AddComponent<ECS::TeleporterComponent>(entity, DeserializeTeleporterComponent(entityJson["teleporter"]));
            }
            if (entityJson.contains("destructible")) {
                m_World->AddComponent<ECS::DestructibleComponent>(entity, DeserializeDestructibleComponent(entityJson["destructible"]));
            }
            if (entityJson.contains("curlNoiseField")) {
                m_World->AddComponent<ECS::CurlNoiseFieldComponent>(entity, DeserializeCurlNoiseFieldComponent(entityJson["curlNoiseField"]));
            }
            if (entityJson.contains("fractureConfig")) {
                m_World->AddComponent<ECS::FractureConfigComponent>(entity, DeserializeFractureConfigComponent(entityJson["fractureConfig"]));
            }
            if (entityJson.contains("movingPlatform")) {
                m_World->AddComponent<ECS::MovingPlatformComponent>(entity, DeserializeMovingPlatformComponent(entityJson["movingPlatform"]));
            }
            if (entityJson.contains("scriptComponent")) {
                m_World->AddComponent<ECS::ScriptComponent>(entity, DeserializeScriptComponent(entityJson["scriptComponent"]));
            }

            // Hierarchy
            if (entityJson.contains("parent")) {
                auto& pc = m_World->AddComponent<ECS::ParentComponent>(entity);
                pc.parent = static_cast<ECS::Entity>(entityJson["parent"].get<u64>());
            }

            // Prefab instance link
            if (entityJson.contains("prefabInstance")) {
                auto& pi = m_World->AddComponent<Assets::PrefabInstanceComponent>(entity);
                const auto& piJson = entityJson["prefabInstance"];
                pi.prefabId = piJson.value("prefabId", static_cast<u64>(0));
                pi.prefabPath = piJson.value("prefabPath", std::string(""));
            }

            // IK Components
            if (entityJson.contains("lookAtIK")) {
                auto& ik = m_World->AddComponent<ECS::LookAtIKComponent>(entity);
                auto& ikJson = entityJson["lookAtIK"];
                if (ikJson.contains("headBone")) ik.headBoneName = ikJson["headBone"].get<std::string>();
                if (ikJson.contains("neckBone")) ik.neckBoneName = ikJson["neckBone"].get<std::string>();
                if (ikJson.contains("targetEntity")) ik.targetEntity = static_cast<ECS::Entity>(ikJson["targetEntity"].get<u64>());
                if (ikJson.contains("targetPos")) {
                    auto arr = ikJson["targetPos"];
                    ik.targetWorldPos = Math::Vector3(arr[0].get<f32>(), arr[1].get<f32>(), arr[2].get<f32>());
                }
                if (ikJson.contains("useEntityTarget")) ik.useEntityTarget = ikJson["useEntityTarget"].get<bool>();
                if (ikJson.contains("maxRotation")) ik.maxRotation = ikJson["maxRotation"].get<f32>();
                if (ikJson.contains("smoothSpeed")) ik.smoothSpeed = ikJson["smoothSpeed"].get<f32>();
                if (ikJson.contains("lookWeight")) ik.lookWeight = ikJson["lookWeight"].get<f32>();
            }
            if (entityJson.contains("interactionIK")) {
                auto& ik = m_World->AddComponent<ECS::InteractionIKComponent>(entity);
                auto& ikJson = entityJson["interactionIK"];
                if (ikJson.contains("handBone")) ik.handBoneName = ikJson["handBone"].get<std::string>();
                if (ikJson.contains("elbowBone")) ik.elbowBoneName = ikJson["elbowBone"].get<std::string>();
                if (ikJson.contains("shoulderBone")) ik.shoulderBoneName = ikJson["shoulderBone"].get<std::string>();
                if (ikJson.contains("interactionRadius")) ik.interactionRadius = ikJson["interactionRadius"].get<f32>();
                if (ikJson.contains("ikWeight")) ik.ikWeight = ikJson["ikWeight"].get<f32>();
                if (ikJson.contains("smoothSpeed")) ik.smoothSpeed = ikJson["smoothSpeed"].get<f32>();
                if (ikJson.contains("interactionTag")) ik.interactionTag = ikJson["interactionTag"].get<std::string>();
            }

            // Audio Components
            if (entityJson.contains("audioSource")) {
                auto audio = DeserializeAudioSourceComponent(entityJson["audioSource"]);
                m_World->AddComponent<ECS::AudioSourceComponent>(entity, audio);
            }
            if (entityJson.contains("audioListener")) {
                auto listener = DeserializeAudioListenerComponent(entityJson["audioListener"]);
                m_World->AddComponent<ECS::AudioListenerComponent>(entity, listener);
            }

            // Physics & Collision
            if (entityJson.contains("rigidbody")) {
                m_World->AddComponent<ECS::RigidbodyComponent>(entity, DeserializeRigidbodyComponent(entityJson["rigidbody"]));
            }
            if (entityJson.contains("boxCollider")) {
                m_World->AddComponent<ECS::BoxColliderComponent>(entity, DeserializeBoxColliderComponent(entityJson["boxCollider"]));
            }
            if (entityJson.contains("polygonCollider2D")) {
                m_World->AddComponent<ECS::PolygonCollider2DComponent>(entity, DeserializePolygonCollider2DComponent(entityJson["polygonCollider2D"]));
            }
            if (entityJson.contains("perFrameCollider")) {
                m_World->AddComponent<ECS::PerFrameColliderComponent>(entity, DeserializePerFrameColliderComponent(entityJson["perFrameCollider"]));
            }
            if (entityJson.contains("sphereCollider")) {
                m_World->AddComponent<ECS::SphereColliderComponent>(entity, DeserializeSphereColliderComponent(entityJson["sphereCollider"]));
            }
            if (entityJson.contains("capsuleCollider")) {
                m_World->AddComponent<ECS::CapsuleColliderComponent>(entity, DeserializeCapsuleColliderComponent(entityJson["capsuleCollider"]));
            }

            // Health & Damage
            if (entityJson.contains("health")) {
                m_World->AddComponent<ECS::HealthComponent>(entity, DeserializeHealthComponent(entityJson["health"]));
            }
            if (entityJson.contains("damage")) {
                m_World->AddComponent<ECS::DamageComponent>(entity, DeserializeDamageComponent(entityJson["damage"]));
            }
            if (entityJson.contains("damageResistance")) {
                m_World->AddComponent<ECS::DamageResistanceComponent>(entity, DeserializeDamageResistanceComponent(entityJson["damageResistance"]));
            }

            // Triggers & Interaction
            if (entityJson.contains("triggerZone")) {
                m_World->AddComponent<ECS::TriggerZoneComponent>(entity, DeserializeTriggerZoneComponent(entityJson["triggerZone"]));
            }
            if (entityJson.contains("interactable")) {
                m_World->AddComponent<ECS::InteractableComponent>(entity, DeserializeInteractableComponent(entityJson["interactable"]));
            }
            if (entityJson.contains("pickup")) {
                m_World->AddComponent<ECS::PickupComponent>(entity, DeserializePickupComponent(entityJson["pickup"]));
            }

            // Tags, Layers, Billboard
            if (entityJson.contains("tag")) {
                m_World->AddComponent<ECS::TagComponent>(entity, DeserializeTagComponent(entityJson["tag"]));
            }
            if (entityJson.contains("layer")) {
                m_World->AddComponent<ECS::LayerComponent>(entity, DeserializeLayerComponent(entityJson["layer"]));
            }
            if (entityJson.contains("billboard")) {
                m_World->AddComponent<ECS::BillboardComponent>(entity, DeserializeBillboardComponent(entityJson["billboard"]));
            }

            // Particles
            if (entityJson.contains("particleEmitter")) {
                m_World->AddComponent<ECS::ParticleEmitterComponent>(entity, DeserializeParticleEmitterComponent(entityJson["particleEmitter"]));
            }

            // 2D Rendering
            if (entityJson.contains("sprite2D")) {
                m_World->AddComponent<ECS::Sprite2DComponent>(entity, DeserializeSprite2DComponent(entityJson["sprite2D"]));
            }
            if (entityJson.contains("animatedSprite2D")) {
                m_World->AddComponent<ECS::AnimatedSprite2DComponent>(entity, DeserializeAnimatedSprite2DComponent(entityJson["animatedSprite2D"]));
            }
            if (entityJson.contains("tilemap")) {
                m_World->AddComponent<ECS::TilemapComponent>(entity, DeserializeTilemapComponent(entityJson["tilemap"]));
            }
            if (entityJson.contains("camera2DBounds")) {
                m_World->AddComponent<ECS::Camera2DBoundsComponent>(entity, DeserializeCamera2DBoundsComponent(entityJson["camera2DBounds"]));
            }

            // Logic
            if (entityJson.contains("stateMachine")) {
                m_World->AddComponent<ECS::StateMachineComponent>(entity, DeserializeStateMachineComponent(entityJson["stateMachine"]));
            }
            if (entityJson.contains("dialogue")) {
                m_World->AddComponent<ECS::DialogueComponent>(entity, DeserializeDialogueComponent(entityJson["dialogue"]));
            }
            if (entityJson.contains("dialogueBox")) {
                m_World->AddComponent<ECS::DialogueBoxComponent>(entity, DeserializeDialogueBoxComponent(entityJson["dialogueBox"]));
            }
            if (entityJson.contains("tween")) {
                m_World->AddComponent<ECS::TweenComponent>(entity, DeserializeTweenComponent(entityJson["tween"]));
            }
            if (entityJson.contains("visualScript")) {
                m_World->AddComponent<ECS::VisualScriptComponent>(entity, DeserializeVisualScriptComponent(entityJson["visualScript"]));
            }

            // AI & Navigation
            if (entityJson.contains("aiController")) {
                m_World->AddComponent<ECS::AIControllerComponent>(entity, DeserializeAIControllerComponent(entityJson["aiController"]));
            }
            if (entityJson.contains("behaviorTree")) {
                m_World->AddComponent<ECS::BehaviorTreeComponent>(entity, DeserializeBehaviorTreeComponent(entityJson["behaviorTree"]));
            }
            if (entityJson.contains("questFlow")) {
                m_World->AddComponent<ECS::QuestFlowComponent>(entity, DeserializeQuestFlowComponent(entityJson["questFlow"]));
            }
            if (entityJson.contains("followTarget")) {
                m_World->AddComponent<ECS::FollowTargetComponent>(entity, DeserializeFollowTargetComponent(entityJson["followTarget"]));
            }
            if (entityJson.contains("lookAtTarget")) {
                m_World->AddComponent<ECS::LookAtTargetComponent>(entity, DeserializeLookAtTargetComponent(entityJson["lookAtTarget"]));
            }
            if (entityJson.contains("waypoint")) {
                m_World->AddComponent<ECS::WaypointComponent>(entity, DeserializeWaypointComponent(entityJson["waypoint"]));
            }

            // Spawning & Timers
            if (entityJson.contains("spawnPoint")) {
                m_World->AddComponent<ECS::SpawnPointComponent>(entity, DeserializeSpawnPointComponent(entityJson["spawnPoint"]));
            }
            if (entityJson.contains("timer")) {
                m_World->AddComponent<ECS::TimerComponent>(entity, DeserializeTimerComponent(entityJson["timer"]));
            }

            // Inventory & Save Data
            if (entityJson.contains("inventory")) {
                m_World->AddComponent<ECS::InventoryComponent>(entity, DeserializeInventoryComponent(entityJson["inventory"]));
            }
            if (entityJson.contains("saveData")) {
                m_World->AddComponent<ECS::SaveDataComponent>(entity, DeserializeSaveDataComponent(entityJson["saveData"]));
            }
            if (entityJson.contains("saveLoadMenu")) {
                m_World->AddComponent<ECS::SaveLoadMenuComponent>(entity, DeserializeSaveLoadMenuComponent(entityJson["saveLoadMenu"]));
            }

            // New Gameplay Components
            if (entityJson.contains("resource")) {
                m_World->AddComponent<ECS::ResourceComponent>(entity, DeserializeResourceComponent(entityJson["resource"]));
            }
            if (entityJson.contains("footstep")) {
                m_World->AddComponent<ECS::FootstepComponent>(entity, DeserializeFootstepComponent(entityJson["footstep"]));
            }
            if (entityJson.contains("poolable")) {
                m_World->AddComponent<ECS::PoolableComponent>(entity, DeserializePoolableComponent(entityJson["poolable"]));
            }
            if (entityJson.contains("questState")) {
                m_World->AddComponent<ECS::QuestStateComponent>(entity, DeserializeQuestStateComponent(entityJson["questState"]));
            }
            if (entityJson.contains("hudWidget")) {
                m_World->AddComponent<ECS::HUDWidgetComponent>(entity, DeserializeHUDWidgetComponent(entityJson["hudWidget"]));
            }
            if (entityJson.contains("uiCanvas")) {
                m_World->AddComponent<GUI::UICanvasComponent>(entity, DeserializeUICanvasComponent(entityJson["uiCanvas"]));
            }
            if (entityJson.contains("cinematicCamera")) {
                m_World->AddComponent<ECS::CinematicCameraComponent>(entity, DeserializeCinematicCameraComponent(entityJson["cinematicCamera"]));
            }

            // Joint & Ragdoll Components
            if (entityJson.contains("distanceJoint")) {
                m_World->AddComponent<ECS::DistanceJointComponent>(entity, DeserializeDistanceJointComponent(entityJson["distanceJoint"]));
            }
            if (entityJson.contains("hingeJoint")) {
                m_World->AddComponent<ECS::HingeJointComponent>(entity, DeserializeHingeJointComponent(entityJson["hingeJoint"]));
            }
            if (entityJson.contains("ballSocketJoint")) {
                m_World->AddComponent<ECS::BallSocketJointComponent>(entity, DeserializeBallSocketJointComponent(entityJson["ballSocketJoint"]));
            }
            if (entityJson.contains("springJoint")) {
                m_World->AddComponent<ECS::SpringJointComponent>(entity, DeserializeSpringJointComponent(entityJson["springJoint"]));
            }
            if (entityJson.contains("fixedJoint")) {
                m_World->AddComponent<ECS::FixedJointComponent>(entity, DeserializeFixedJointComponent(entityJson["fixedJoint"]));
            }
            if (entityJson.contains("sliderJoint")) {
                m_World->AddComponent<ECS::SliderJointComponent>(entity, DeserializeSliderJointComponent(entityJson["sliderJoint"]));
            }
            if (entityJson.contains("ragdoll")) {
                m_World->AddComponent<ECS::RagdollComponent>(entity, DeserializeRagdollComponent(entityJson["ragdoll"]));
            }

            // Flower Components
            if (entityJson.contains("jellyMesh")) {
                m_World->AddComponent<ECS::JellyMeshComponent>(entity, DeserializeJellyMeshComponent(entityJson["jellyMesh"]));
            }
            if (entityJson.contains("tether")) {
                m_World->AddComponent<ECS::TetherComponent>(entity, DeserializeTetherComponent(entityJson["tether"]));
            }
            if (entityJson.contains("grabbable")) {
                m_World->AddComponent<ECS::GrabbableComponent>(entity, DeserializeGrabbableComponent(entityJson["grabbable"]));
            }
            if (entityJson.contains("flowerStem")) {
                m_World->AddComponent<ECS::FlowerStemComponent>(entity, DeserializeFlowerStemComponent(entityJson["flowerStem"]));
            }
            if (entityJson.contains("flowerParticleConfig")) {
                m_World->AddComponent<ECS::FlowerParticleConfigComponent>(entity, DeserializeFlowerParticleConfigComponent(entityJson["flowerParticleConfig"]));
            }

            // LOD, Grass, Vegetation
            if (entityJson.contains("lod")) {
                m_World->AddComponent<ECS::LODComponent>(entity, DeserializeLODComponent(entityJson["lod"]));
            }
            if (entityJson.contains("grassVolume")) {
                m_World->AddComponent<ECS::GrassVolumeComponent>(entity, DeserializeGrassVolumeComponent(entityJson["grassVolume"]));
            }
            if (entityJson.contains("vegetation")) {
                m_World->AddComponent<ECS::VegetationComponent>(entity, DeserializeVegetationComponent(entityJson["vegetation"]));
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
        }

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

std::string SceneSerializer::SerializeEntityToString(ECS::World* world, ECS::Entity entity) {
    if (!world || !world->IsValid(entity)) return "";

    try {
        json entityJson;
        entityJson["id"] = static_cast<u64>(entity);

        // Serialize all components (mirrors SaveEntities loop)
        if (world->HasComponent<ECS::NameComponent>(entity))
            entityJson["name"] = SerializeNameComponent(*world->GetComponent<ECS::NameComponent>(entity));
        if (world->HasComponent<ECS::TransformComponent>(entity))
            entityJson["transform"] = SerializeTransformComponent(*world->GetComponent<ECS::TransformComponent>(entity));
        if (world->HasComponent<ECS::MaterialComponent>(entity))
            entityJson["material"] = SerializeMaterialComponent(*world->GetComponent<ECS::MaterialComponent>(entity));
        if (world->HasComponent<ECS::MeshComponent>(entity))
            entityJson["mesh"] = SerializeMeshComponent(*world->GetComponent<ECS::MeshComponent>(entity), true);
        if (world->HasComponent<ECS::SkeletonComponent>(entity))
            entityJson["skeleton"] = SerializeSkeletonComponent(*world->GetComponent<ECS::SkeletonComponent>(entity));
        if (world->HasComponent<ECS::AnimatorComponent>(entity))
            entityJson["animator"] = SerializeAnimatorComponent(*world->GetComponent<ECS::AnimatorComponent>(entity));
        if (world->HasComponent<ECS::LightComponent>(entity))
            entityJson["light"] = SerializeLightComponent(*world->GetComponent<ECS::LightComponent>(entity));
        if (world->HasComponent<ECS::NotesComponent>(entity))
            entityJson["notes"] = SerializeNotesComponent(*world->GetComponent<ECS::NotesComponent>(entity));
        if (world->HasComponent<ECS::TextComponent>(entity))
            entityJson["text"] = SerializeTextComponent(*world->GetComponent<ECS::TextComponent>(entity));
        if (world->HasComponent<ECS::CameraComponent>(entity))
            entityJson["camera"] = SerializeCameraComponent(*world->GetComponent<ECS::CameraComponent>(entity));
        if (world->HasComponent<ECS::WeatherZoneComponent>(entity))
            entityJson["weatherZone"] = SerializeWeatherZoneComponent(*world->GetComponent<ECS::WeatherZoneComponent>(entity));
        if (world->HasComponent<ECS::WaterVolumeComponent>(entity))
            entityJson["waterVolume"] = SerializeWaterVolumeComponent(*world->GetComponent<ECS::WaterVolumeComponent>(entity));
        if (world->HasComponent<ECS::ShrubVolumeComponent>(entity))
            entityJson["shrubVolume"] = SerializeShrubVolumeComponent(*world->GetComponent<ECS::ShrubVolumeComponent>(entity));
        if (world->HasComponent<ECS::TreeVolumeComponent>(entity))
            entityJson["treeVolume"] = SerializeTreeVolumeComponent(*world->GetComponent<ECS::TreeVolumeComponent>(entity));
        if (world->HasComponent<ECS::TerrainComponent>(entity))
            entityJson["terrain"] = SerializeTerrainComponent(*world->GetComponent<ECS::TerrainComponent>(entity));
        if (world->HasComponent<ECS::Terrain2DComponent>(entity))
            entityJson["terrain2d"] = SerializeTerrain2DComponent(*world->GetComponent<ECS::Terrain2DComponent>(entity));
        if (world->HasComponent<ECS::CameraTriggerComponent>(entity))
            entityJson["cameraTrigger"] = SerializeCameraTriggerComponent(*world->GetComponent<ECS::CameraTriggerComponent>(entity));
        if (world->HasComponent<ECS::TemperatureZoneComponent>(entity))
            entityJson["temperatureZone"] = SerializeTemperatureZoneComponent(*world->GetComponent<ECS::TemperatureZoneComponent>(entity));
        if (world->HasComponent<ECS::GravityZoneComponent>(entity))
            entityJson["gravityZone"] = SerializeGravityZoneComponent(*world->GetComponent<ECS::GravityZoneComponent>(entity));
        if (world->HasComponent<ECS::FluidVolumeComponent>(entity))
            entityJson["fluidVolume"] = SerializeFluidVolumeComponent(*world->GetComponent<ECS::FluidVolumeComponent>(entity));
        // Controllers
        if (world->HasComponent<ECS::Platformer2DController>(entity))
            entityJson["platformer2D"] = SerializePlatformer2D(*world->GetComponent<ECS::Platformer2DController>(entity));
        if (world->HasComponent<ECS::TopDown2DController>(entity))
            entityJson["topDown2D"] = SerializeTopDown2D(*world->GetComponent<ECS::TopDown2DController>(entity));
        if (world->HasComponent<ECS::TopDown3DController>(entity))
            entityJson["topDown3D"] = SerializeTopDown3D(*world->GetComponent<ECS::TopDown3DController>(entity));
        if (world->HasComponent<ECS::ThirdPersonController>(entity))
            entityJson["thirdPerson"] = SerializeThirdPerson(*world->GetComponent<ECS::ThirdPersonController>(entity));
        if (world->HasComponent<ECS::FirstPersonController>(entity))
            entityJson["firstPerson"] = SerializeFirstPerson(*world->GetComponent<ECS::FirstPersonController>(entity));
        if (world->HasComponent<ECS::VehicleController>(entity))
            entityJson["vehicle"] = SerializeVehicle(*world->GetComponent<ECS::VehicleController>(entity));
        if (world->HasComponent<ECS::SurfaceAlignedController>(entity))
            entityJson["surfaceAligned"] = SerializeSurfaceAligned(*world->GetComponent<ECS::SurfaceAlignedController>(entity));
        if (world->HasComponent<ECS::NetworkIdentityComponent>(entity))
            entityJson["networkIdentity"] = SerializeNetworkIdentityComponent(*world->GetComponent<ECS::NetworkIdentityComponent>(entity));
        if (world->HasComponent<ECS::NetworkTransformComponent>(entity))
            entityJson["networkTransform"] = SerializeNetworkTransformComponent(*world->GetComponent<ECS::NetworkTransformComponent>(entity));
        if (world->HasComponent<ECS::PossessableComponent>(entity))
            entityJson["possessable"] = SerializePossessable(*world->GetComponent<ECS::PossessableComponent>(entity));
        // Puzzle
        if (world->HasComponent<ECS::LockComponent>(entity))
            entityJson["lock"] = SerializeLockComponent(*world->GetComponent<ECS::LockComponent>(entity));
        if (world->HasComponent<ECS::PushableComponent>(entity))
            entityJson["pushable"] = SerializePushableComponent(*world->GetComponent<ECS::PushableComponent>(entity));
        if (world->HasComponent<ECS::SwitchComponent>(entity))
            entityJson["switch"] = SerializeSwitchComponent(*world->GetComponent<ECS::SwitchComponent>(entity));
        if (world->HasComponent<ECS::GoalZoneComponent>(entity))
            entityJson["goalZone"] = SerializeGoalZoneComponent(*world->GetComponent<ECS::GoalZoneComponent>(entity));
        if (world->HasComponent<ECS::ConveyorComponent>(entity))
            entityJson["conveyor"] = SerializeConveyorComponent(*world->GetComponent<ECS::ConveyorComponent>(entity));
        if (world->HasComponent<ECS::TeleporterComponent>(entity))
            entityJson["teleporter"] = SerializeTeleporterComponent(*world->GetComponent<ECS::TeleporterComponent>(entity));
        if (world->HasComponent<ECS::DestructibleComponent>(entity))
            entityJson["destructible"] = SerializeDestructibleComponent(*world->GetComponent<ECS::DestructibleComponent>(entity));
        if (world->HasComponent<ECS::CurlNoiseFieldComponent>(entity))
            entityJson["curlNoiseField"] = SerializeCurlNoiseFieldComponent(*world->GetComponent<ECS::CurlNoiseFieldComponent>(entity));
        if (world->HasComponent<ECS::FractureConfigComponent>(entity))
            entityJson["fractureConfig"] = SerializeFractureConfigComponent(*world->GetComponent<ECS::FractureConfigComponent>(entity));
        if (world->HasComponent<ECS::MovingPlatformComponent>(entity))
            entityJson["movingPlatform"] = SerializeMovingPlatformComponent(*world->GetComponent<ECS::MovingPlatformComponent>(entity));
        // Script
        if (world->HasComponent<ECS::ScriptComponent>(entity))
            entityJson["scriptComponent"] = SerializeScriptComponent(*world->GetComponent<ECS::ScriptComponent>(entity));
        // Hierarchy
        if (world->HasComponent<ECS::ParentComponent>(entity))
            entityJson["parent"] = static_cast<u64>(world->GetComponent<ECS::ParentComponent>(entity)->parent);
        // Prefab instance link
        if (world->HasComponent<Assets::PrefabInstanceComponent>(entity)) {
            auto* pi = world->GetComponent<Assets::PrefabInstanceComponent>(entity);
            json piJson;
            piJson["prefabId"] = pi->prefabId;
            piJson["prefabPath"] = pi->prefabPath;
            entityJson["prefabInstance"] = piJson;
        }
        // IK
        if (world->HasComponent<ECS::LookAtIKComponent>(entity)) {
            auto* ik = world->GetComponent<ECS::LookAtIKComponent>(entity);
            json ikJson;
            ikJson["headBone"] = ik->headBoneName;
            ikJson["neckBone"] = ik->neckBoneName;
            ikJson["targetEntity"] = static_cast<u64>(ik->targetEntity);
            ikJson["targetPos"] = { ik->targetWorldPos.x, ik->targetWorldPos.y, ik->targetWorldPos.z };
            ikJson["useEntityTarget"] = ik->useEntityTarget;
            ikJson["maxRotation"] = ik->maxRotation;
            ikJson["smoothSpeed"] = ik->smoothSpeed;
            ikJson["lookWeight"] = ik->lookWeight;
            entityJson["lookAtIK"] = ikJson;
        }
        if (world->HasComponent<ECS::InteractionIKComponent>(entity)) {
            auto* ik = world->GetComponent<ECS::InteractionIKComponent>(entity);
            json ikJson;
            ikJson["handBone"] = ik->handBoneName;
            ikJson["elbowBone"] = ik->elbowBoneName;
            ikJson["shoulderBone"] = ik->shoulderBoneName;
            ikJson["interactionRadius"] = ik->interactionRadius;
            ikJson["ikWeight"] = ik->ikWeight;
            ikJson["smoothSpeed"] = ik->smoothSpeed;
            ikJson["interactionTag"] = ik->interactionTag;
            entityJson["interactionIK"] = ikJson;
        }
        // Audio
        if (world->HasComponent<ECS::AudioSourceComponent>(entity))
            entityJson["audioSource"] = SerializeAudioSourceComponent(*world->GetComponent<ECS::AudioSourceComponent>(entity));
        if (world->HasComponent<ECS::AudioListenerComponent>(entity))
            entityJson["audioListener"] = SerializeAudioListenerComponent(*world->GetComponent<ECS::AudioListenerComponent>(entity));
        // Physics
        if (world->HasComponent<ECS::RigidbodyComponent>(entity))
            entityJson["rigidbody"] = SerializeRigidbodyComponent(*world->GetComponent<ECS::RigidbodyComponent>(entity));
        if (world->HasComponent<ECS::BoxColliderComponent>(entity))
            entityJson["boxCollider"] = SerializeBoxColliderComponent(*world->GetComponent<ECS::BoxColliderComponent>(entity));
        if (world->HasComponent<ECS::PolygonCollider2DComponent>(entity))
            entityJson["polygonCollider2D"] = SerializePolygonCollider2DComponent(*world->GetComponent<ECS::PolygonCollider2DComponent>(entity));
        if (world->HasComponent<ECS::PerFrameColliderComponent>(entity))
            entityJson["perFrameCollider"] = SerializePerFrameColliderComponent(*world->GetComponent<ECS::PerFrameColliderComponent>(entity));
        if (world->HasComponent<ECS::SphereColliderComponent>(entity))
            entityJson["sphereCollider"] = SerializeSphereColliderComponent(*world->GetComponent<ECS::SphereColliderComponent>(entity));
        if (world->HasComponent<ECS::CapsuleColliderComponent>(entity))
            entityJson["capsuleCollider"] = SerializeCapsuleColliderComponent(*world->GetComponent<ECS::CapsuleColliderComponent>(entity));
        // Gameplay
        if (world->HasComponent<ECS::HealthComponent>(entity))
            entityJson["health"] = SerializeHealthComponent(*world->GetComponent<ECS::HealthComponent>(entity));
        if (world->HasComponent<ECS::DamageComponent>(entity))
            entityJson["damage"] = SerializeDamageComponent(*world->GetComponent<ECS::DamageComponent>(entity));
        if (world->HasComponent<ECS::DamageResistanceComponent>(entity))
            entityJson["damageResistance"] = SerializeDamageResistanceComponent(*world->GetComponent<ECS::DamageResistanceComponent>(entity));
        if (world->HasComponent<ECS::TriggerZoneComponent>(entity))
            entityJson["triggerZone"] = SerializeTriggerZoneComponent(*world->GetComponent<ECS::TriggerZoneComponent>(entity));
        if (world->HasComponent<ECS::InteractableComponent>(entity))
            entityJson["interactable"] = SerializeInteractableComponent(*world->GetComponent<ECS::InteractableComponent>(entity));
        if (world->HasComponent<ECS::PickupComponent>(entity))
            entityJson["pickup"] = SerializePickupComponent(*world->GetComponent<ECS::PickupComponent>(entity));
        if (world->HasComponent<ECS::TagComponent>(entity))
            entityJson["tag"] = SerializeTagComponent(*world->GetComponent<ECS::TagComponent>(entity));
        if (world->HasComponent<ECS::LayerComponent>(entity))
            entityJson["layer"] = SerializeLayerComponent(*world->GetComponent<ECS::LayerComponent>(entity));
        if (world->HasComponent<ECS::BillboardComponent>(entity))
            entityJson["billboard"] = SerializeBillboardComponent(*world->GetComponent<ECS::BillboardComponent>(entity));
        if (world->HasComponent<ECS::ParticleEmitterComponent>(entity))
            entityJson["particleEmitter"] = SerializeParticleEmitterComponent(*world->GetComponent<ECS::ParticleEmitterComponent>(entity));
        // 2D
        if (world->HasComponent<ECS::Sprite2DComponent>(entity))
            entityJson["sprite2D"] = SerializeSprite2DComponent(*world->GetComponent<ECS::Sprite2DComponent>(entity));
        if (world->HasComponent<ECS::AnimatedSprite2DComponent>(entity))
            entityJson["animatedSprite2D"] = SerializeAnimatedSprite2DComponent(*world->GetComponent<ECS::AnimatedSprite2DComponent>(entity));
        if (world->HasComponent<ECS::TilemapComponent>(entity))
            entityJson["tilemap"] = SerializeTilemapComponent(*world->GetComponent<ECS::TilemapComponent>(entity));
        if (world->HasComponent<ECS::Camera2DBoundsComponent>(entity))
            entityJson["camera2DBounds"] = SerializeCamera2DBoundsComponent(*world->GetComponent<ECS::Camera2DBoundsComponent>(entity));
        // State/Dialogue/Tween
        if (world->HasComponent<ECS::StateMachineComponent>(entity))
            entityJson["stateMachine"] = SerializeStateMachineComponent(*world->GetComponent<ECS::StateMachineComponent>(entity));
        if (world->HasComponent<ECS::DialogueComponent>(entity))
            entityJson["dialogue"] = SerializeDialogueComponent(*world->GetComponent<ECS::DialogueComponent>(entity));
        if (world->HasComponent<ECS::DialogueBoxComponent>(entity))
            entityJson["dialogueBox"] = SerializeDialogueBoxComponent(*world->GetComponent<ECS::DialogueBoxComponent>(entity));
        if (world->HasComponent<ECS::TweenComponent>(entity))
            entityJson["tween"] = SerializeTweenComponent(*world->GetComponent<ECS::TweenComponent>(entity));
        if (world->HasComponent<ECS::VisualScriptComponent>(entity))
            entityJson["visualScript"] = SerializeVisualScriptComponent(*world->GetComponent<ECS::VisualScriptComponent>(entity));
        // AI
        if (world->HasComponent<ECS::AIControllerComponent>(entity))
            entityJson["aiController"] = SerializeAIControllerComponent(*world->GetComponent<ECS::AIControllerComponent>(entity));
        if (world->HasComponent<ECS::BehaviorTreeComponent>(entity))
            entityJson["behaviorTree"] = SerializeBehaviorTreeComponent(*world->GetComponent<ECS::BehaviorTreeComponent>(entity));
        if (world->HasComponent<ECS::QuestFlowComponent>(entity))
            entityJson["questFlow"] = SerializeQuestFlowComponent(*world->GetComponent<ECS::QuestFlowComponent>(entity));
        if (world->HasComponent<ECS::FollowTargetComponent>(entity))
            entityJson["followTarget"] = SerializeFollowTargetComponent(*world->GetComponent<ECS::FollowTargetComponent>(entity));
        if (world->HasComponent<ECS::LookAtTargetComponent>(entity))
            entityJson["lookAtTarget"] = SerializeLookAtTargetComponent(*world->GetComponent<ECS::LookAtTargetComponent>(entity));
        if (world->HasComponent<ECS::WaypointComponent>(entity))
            entityJson["waypoint"] = SerializeWaypointComponent(*world->GetComponent<ECS::WaypointComponent>(entity));
        // Misc gameplay
        if (world->HasComponent<ECS::SpawnPointComponent>(entity))
            entityJson["spawnPoint"] = SerializeSpawnPointComponent(*world->GetComponent<ECS::SpawnPointComponent>(entity));
        if (world->HasComponent<ECS::TimerComponent>(entity))
            entityJson["timer"] = SerializeTimerComponent(*world->GetComponent<ECS::TimerComponent>(entity));
        if (world->HasComponent<ECS::InventoryComponent>(entity))
            entityJson["inventory"] = SerializeInventoryComponent(*world->GetComponent<ECS::InventoryComponent>(entity));
        if (world->HasComponent<ECS::SaveDataComponent>(entity))
            entityJson["saveData"] = SerializeSaveDataComponent(*world->GetComponent<ECS::SaveDataComponent>(entity));
        if (world->HasComponent<ECS::SaveLoadMenuComponent>(entity))
            entityJson["saveLoadMenu"] = SerializeSaveLoadMenuComponent(*world->GetComponent<ECS::SaveLoadMenuComponent>(entity));
        if (world->HasComponent<ECS::ResourceComponent>(entity))
            entityJson["resource"] = SerializeResourceComponent(*world->GetComponent<ECS::ResourceComponent>(entity));
        if (world->HasComponent<ECS::FootstepComponent>(entity))
            entityJson["footstep"] = SerializeFootstepComponent(*world->GetComponent<ECS::FootstepComponent>(entity));
        if (world->HasComponent<ECS::PoolableComponent>(entity))
            entityJson["poolable"] = SerializePoolableComponent(*world->GetComponent<ECS::PoolableComponent>(entity));
        if (world->HasComponent<ECS::QuestStateComponent>(entity))
            entityJson["questState"] = SerializeQuestStateComponent(*world->GetComponent<ECS::QuestStateComponent>(entity));
        if (world->HasComponent<ECS::HUDWidgetComponent>(entity))
            entityJson["hudWidget"] = SerializeHUDWidgetComponent(*world->GetComponent<ECS::HUDWidgetComponent>(entity));
        if (world->HasComponent<GUI::UICanvasComponent>(entity))
            entityJson["uiCanvas"] = SerializeUICanvasComponent(*world->GetComponent<GUI::UICanvasComponent>(entity));
        if (world->HasComponent<ECS::CinematicCameraComponent>(entity))
            entityJson["cinematicCamera"] = SerializeCinematicCameraComponent(*world->GetComponent<ECS::CinematicCameraComponent>(entity));
        // Joints
        if (world->HasComponent<ECS::DistanceJointComponent>(entity))
            entityJson["distanceJoint"] = SerializeDistanceJointComponent(*world->GetComponent<ECS::DistanceJointComponent>(entity));
        if (world->HasComponent<ECS::HingeJointComponent>(entity))
            entityJson["hingeJoint"] = SerializeHingeJointComponent(*world->GetComponent<ECS::HingeJointComponent>(entity));
        if (world->HasComponent<ECS::BallSocketJointComponent>(entity))
            entityJson["ballSocketJoint"] = SerializeBallSocketJointComponent(*world->GetComponent<ECS::BallSocketJointComponent>(entity));
        if (world->HasComponent<ECS::SpringJointComponent>(entity))
            entityJson["springJoint"] = SerializeSpringJointComponent(*world->GetComponent<ECS::SpringJointComponent>(entity));
        if (world->HasComponent<ECS::FixedJointComponent>(entity))
            entityJson["fixedJoint"] = SerializeFixedJointComponent(*world->GetComponent<ECS::FixedJointComponent>(entity));
        if (world->HasComponent<ECS::SliderJointComponent>(entity))
            entityJson["sliderJoint"] = SerializeSliderJointComponent(*world->GetComponent<ECS::SliderJointComponent>(entity));
        if (world->HasComponent<ECS::RagdollComponent>(entity))
            entityJson["ragdoll"] = SerializeRagdollComponent(*world->GetComponent<ECS::RagdollComponent>(entity));
        // Flower
        if (world->HasComponent<ECS::JellyMeshComponent>(entity))
            entityJson["jellyMesh"] = SerializeJellyMeshComponent(*world->GetComponent<ECS::JellyMeshComponent>(entity));
        if (world->HasComponent<ECS::TetherComponent>(entity))
            entityJson["tether"] = SerializeTetherComponent(*world->GetComponent<ECS::TetherComponent>(entity));
        if (world->HasComponent<ECS::GrabbableComponent>(entity))
            entityJson["grabbable"] = SerializeGrabbableComponent(*world->GetComponent<ECS::GrabbableComponent>(entity));
        if (world->HasComponent<ECS::FlowerStemComponent>(entity))
            entityJson["flowerStem"] = SerializeFlowerStemComponent(*world->GetComponent<ECS::FlowerStemComponent>(entity));
        if (world->HasComponent<ECS::FlowerParticleConfigComponent>(entity))
            entityJson["flowerParticleConfig"] = SerializeFlowerParticleConfigComponent(*world->GetComponent<ECS::FlowerParticleConfigComponent>(entity));
        // LOD/Vegetation
        if (world->HasComponent<ECS::LODComponent>(entity))
            entityJson["lod"] = SerializeLODComponent(*world->GetComponent<ECS::LODComponent>(entity));
        if (world->HasComponent<ECS::GrassVolumeComponent>(entity))
            entityJson["grassVolume"] = SerializeGrassVolumeComponent(*world->GetComponent<ECS::GrassVolumeComponent>(entity));
        if (world->HasComponent<ECS::VegetationComponent>(entity))
            entityJson["vegetation"] = SerializeVegetationComponent(*world->GetComponent<ECS::VegetationComponent>(entity));

        return entityJson.dump();

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Failed to serialize entity to string: %s", e.what());
        return "";
    }
}

ECS::Entity SceneSerializer::DeserializeEntityFromString(ECS::World* world, const std::string& jsonStr) {
    if (!world || jsonStr.empty()) return ECS::INVALID_ENTITY;

    try {
        json entityJson = json::parse(jsonStr);
        ECS::Entity entity = world->CreateEntity();

        // Deserialize all components (mirrors LoadFromString entity loop)
        if (entityJson.contains("name")) {
            world->AddComponent<ECS::NameComponent>(entity, DeserializeNameComponent(entityJson["name"]));
        }
        if (entityJson.contains("transform")) {
            world->AddComponent<ECS::TransformComponent>(entity, DeserializeTransformComponent(entityJson["transform"]));
        }
        if (entityJson.contains("material")) {
            world->AddComponent<ECS::MaterialComponent>(entity, DeserializeMaterialComponent(entityJson["material"]));
        }
        if (entityJson.contains("mesh")) {
            world->AddComponent<ECS::MeshComponent>(entity, DeserializeMeshComponent(entityJson["mesh"]));
        }
        // Skeleton must be deserialized before animator
        std::shared_ptr<Animation::Skeleton> loadedSkeleton;
        if (entityJson.contains("skeleton")) {
            auto skelComp = DeserializeSkeletonComponent(entityJson["skeleton"]);
            loadedSkeleton = skelComp.skeleton;
            world->AddComponent<ECS::SkeletonComponent>(entity, skelComp);
        }
        if (entityJson.contains("animator")) {
            auto animComp = DeserializeAnimatorComponent(entityJson["animator"], loadedSkeleton);
            world->AddComponent<ECS::AnimatorComponent>(entity, std::move(animComp));
        }
        if (entityJson.contains("light")) {
            world->AddComponent<ECS::LightComponent>(entity, DeserializeLightComponent(entityJson["light"]));
        }
        if (entityJson.contains("notes")) {
            world->AddComponent<ECS::NotesComponent>(entity, DeserializeNotesComponent(entityJson["notes"]));
        }
        if (entityJson.contains("text")) {
            world->AddComponent<ECS::TextComponent>(entity, DeserializeTextComponent(entityJson["text"]));
        }
        if (entityJson.contains("camera")) {
            world->AddComponent<ECS::CameraComponent>(entity, DeserializeCameraComponent(entityJson["camera"]));
        }
        if (entityJson.contains("weatherZone")) {
            world->AddComponent<ECS::WeatherZoneComponent>(entity, DeserializeWeatherZoneComponent(entityJson["weatherZone"]));
        }
        if (entityJson.contains("waterVolume")) {
            world->AddComponent<ECS::WaterVolumeComponent>(entity, DeserializeWaterVolumeComponent(entityJson["waterVolume"]));
        }
        if (entityJson.contains("shrubVolume")) {
            world->AddComponent<ECS::ShrubVolumeComponent>(entity, DeserializeShrubVolumeComponent(entityJson["shrubVolume"]));
        }
        if (entityJson.contains("treeVolume")) {
            world->AddComponent<ECS::TreeVolumeComponent>(entity, DeserializeTreeVolumeComponent(entityJson["treeVolume"]));
        }
        if (entityJson.contains("terrain")) {
            world->AddComponent<ECS::TerrainComponent>(entity, DeserializeTerrainComponent(entityJson["terrain"]));
        }
        if (entityJson.contains("terrain2d")) {
            world->AddComponent<ECS::Terrain2DComponent>(entity, DeserializeTerrain2DComponent(entityJson["terrain2d"]));
        }
        if (entityJson.contains("cameraTrigger")) {
            world->AddComponent<ECS::CameraTriggerComponent>(entity, DeserializeCameraTriggerComponent(entityJson["cameraTrigger"]));
        }
        if (entityJson.contains("temperatureZone")) {
            world->AddComponent<ECS::TemperatureZoneComponent>(entity, DeserializeTemperatureZoneComponent(entityJson["temperatureZone"]));
        }
        if (entityJson.contains("gravityZone")) {
            world->AddComponent<ECS::GravityZoneComponent>(entity, DeserializeGravityZoneComponent(entityJson["gravityZone"]));
        }
        if (entityJson.contains("fluidVolume")) {
            world->AddComponent<ECS::FluidVolumeComponent>(entity, DeserializeFluidVolumeComponent(entityJson["fluidVolume"]));
        }
        // Controllers
        if (entityJson.contains("platformer2D"))
            world->AddComponent<ECS::Platformer2DController>(entity, DeserializePlatformer2D(entityJson["platformer2D"]));
        if (entityJson.contains("topDown2D"))
            world->AddComponent<ECS::TopDown2DController>(entity, DeserializeTopDown2D(entityJson["topDown2D"]));
        if (entityJson.contains("topDown3D"))
            world->AddComponent<ECS::TopDown3DController>(entity, DeserializeTopDown3D(entityJson["topDown3D"]));
        if (entityJson.contains("thirdPerson"))
            world->AddComponent<ECS::ThirdPersonController>(entity, DeserializeThirdPerson(entityJson["thirdPerson"]));
        if (entityJson.contains("firstPerson"))
            world->AddComponent<ECS::FirstPersonController>(entity, DeserializeFirstPerson(entityJson["firstPerson"]));
        if (entityJson.contains("vehicle"))
            world->AddComponent<ECS::VehicleController>(entity, DeserializeVehicle(entityJson["vehicle"]));
        if (entityJson.contains("surfaceAligned"))
            world->AddComponent<ECS::SurfaceAlignedController>(entity, DeserializeSurfaceAligned(entityJson["surfaceAligned"]));
        if (entityJson.contains("networkIdentity"))
            world->AddComponent<ECS::NetworkIdentityComponent>(entity, DeserializeNetworkIdentityComponent(entityJson["networkIdentity"]));
        if (entityJson.contains("networkTransform"))
            world->AddComponent<ECS::NetworkTransformComponent>(entity, DeserializeNetworkTransformComponent(entityJson["networkTransform"]));
        if (entityJson.contains("possessable"))
            world->AddComponent<ECS::PossessableComponent>(entity, DeserializePossessable(entityJson["possessable"]));
        // Puzzle
        if (entityJson.contains("lock"))
            world->AddComponent<ECS::LockComponent>(entity, DeserializeLockComponent(entityJson["lock"]));
        if (entityJson.contains("pushable"))
            world->AddComponent<ECS::PushableComponent>(entity, DeserializePushableComponent(entityJson["pushable"]));
        if (entityJson.contains("switch"))
            world->AddComponent<ECS::SwitchComponent>(entity, DeserializeSwitchComponent(entityJson["switch"]));
        if (entityJson.contains("goalZone"))
            world->AddComponent<ECS::GoalZoneComponent>(entity, DeserializeGoalZoneComponent(entityJson["goalZone"]));
        if (entityJson.contains("conveyor"))
            world->AddComponent<ECS::ConveyorComponent>(entity, DeserializeConveyorComponent(entityJson["conveyor"]));
        if (entityJson.contains("teleporter"))
            world->AddComponent<ECS::TeleporterComponent>(entity, DeserializeTeleporterComponent(entityJson["teleporter"]));
        if (entityJson.contains("destructible"))
            world->AddComponent<ECS::DestructibleComponent>(entity, DeserializeDestructibleComponent(entityJson["destructible"]));
        if (entityJson.contains("curlNoiseField"))
            world->AddComponent<ECS::CurlNoiseFieldComponent>(entity, DeserializeCurlNoiseFieldComponent(entityJson["curlNoiseField"]));
        if (entityJson.contains("fractureConfig"))
            world->AddComponent<ECS::FractureConfigComponent>(entity, DeserializeFractureConfigComponent(entityJson["fractureConfig"]));
        if (entityJson.contains("movingPlatform"))
            world->AddComponent<ECS::MovingPlatformComponent>(entity, DeserializeMovingPlatformComponent(entityJson["movingPlatform"]));
        // Script
        if (entityJson.contains("scriptComponent"))
            world->AddComponent<ECS::ScriptComponent>(entity, DeserializeScriptComponent(entityJson["scriptComponent"]));
        // Hierarchy (parent reference — stored as entity ID, may need remapping by caller)
        if (entityJson.contains("parent")) {
            auto& pc = world->AddComponent<ECS::ParentComponent>(entity);
            pc.parent = static_cast<ECS::Entity>(entityJson["parent"].get<u64>());
        }
        // Prefab instance link
        if (entityJson.contains("prefabInstance")) {
            auto& pi = world->AddComponent<Assets::PrefabInstanceComponent>(entity);
            const auto& piJson = entityJson["prefabInstance"];
            pi.prefabId = piJson.value("prefabId", static_cast<u64>(0));
            pi.prefabPath = piJson.value("prefabPath", std::string(""));
        }
        // IK
        if (entityJson.contains("lookAtIK")) {
            auto& ikJson = entityJson["lookAtIK"];
            auto& ik = world->AddComponent<ECS::LookAtIKComponent>(entity);
            if (ikJson.contains("headBone")) ik.headBoneName = ikJson["headBone"].get<std::string>();
            if (ikJson.contains("neckBone")) ik.neckBoneName = ikJson["neckBone"].get<std::string>();
            if (ikJson.contains("targetEntity")) ik.targetEntity = static_cast<ECS::Entity>(ikJson["targetEntity"].get<u64>());
            if (ikJson.contains("targetPos")) { auto& a = ikJson["targetPos"]; ik.targetWorldPos = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
            if (ikJson.contains("useEntityTarget")) ik.useEntityTarget = ikJson["useEntityTarget"].get<bool>();
            if (ikJson.contains("maxRotation")) ik.maxRotation = ikJson["maxRotation"].get<f32>();
            if (ikJson.contains("smoothSpeed")) ik.smoothSpeed = ikJson["smoothSpeed"].get<f32>();
            if (ikJson.contains("lookWeight")) ik.lookWeight = ikJson["lookWeight"].get<f32>();
        }
        if (entityJson.contains("interactionIK")) {
            auto& ikJson = entityJson["interactionIK"];
            auto& ik = world->AddComponent<ECS::InteractionIKComponent>(entity);
            if (ikJson.contains("handBone")) ik.handBoneName = ikJson["handBone"].get<std::string>();
            if (ikJson.contains("elbowBone")) ik.elbowBoneName = ikJson["elbowBone"].get<std::string>();
            if (ikJson.contains("shoulderBone")) ik.shoulderBoneName = ikJson["shoulderBone"].get<std::string>();
            if (ikJson.contains("interactionRadius")) ik.interactionRadius = ikJson["interactionRadius"].get<f32>();
            if (ikJson.contains("ikWeight")) ik.ikWeight = ikJson["ikWeight"].get<f32>();
            if (ikJson.contains("smoothSpeed")) ik.smoothSpeed = ikJson["smoothSpeed"].get<f32>();
            if (ikJson.contains("interactionTag")) ik.interactionTag = ikJson["interactionTag"].get<std::string>();
        }
        // Audio
        if (entityJson.contains("audioSource"))
            world->AddComponent<ECS::AudioSourceComponent>(entity, DeserializeAudioSourceComponent(entityJson["audioSource"]));
        if (entityJson.contains("audioListener"))
            world->AddComponent<ECS::AudioListenerComponent>(entity, DeserializeAudioListenerComponent(entityJson["audioListener"]));
        // Physics
        if (entityJson.contains("rigidbody"))
            world->AddComponent<ECS::RigidbodyComponent>(entity, DeserializeRigidbodyComponent(entityJson["rigidbody"]));
        if (entityJson.contains("boxCollider"))
            world->AddComponent<ECS::BoxColliderComponent>(entity, DeserializeBoxColliderComponent(entityJson["boxCollider"]));
        if (entityJson.contains("polygonCollider2D"))
            world->AddComponent<ECS::PolygonCollider2DComponent>(entity, DeserializePolygonCollider2DComponent(entityJson["polygonCollider2D"]));
        if (entityJson.contains("perFrameCollider"))
            world->AddComponent<ECS::PerFrameColliderComponent>(entity, DeserializePerFrameColliderComponent(entityJson["perFrameCollider"]));
        if (entityJson.contains("sphereCollider"))
            world->AddComponent<ECS::SphereColliderComponent>(entity, DeserializeSphereColliderComponent(entityJson["sphereCollider"]));
        if (entityJson.contains("capsuleCollider"))
            world->AddComponent<ECS::CapsuleColliderComponent>(entity, DeserializeCapsuleColliderComponent(entityJson["capsuleCollider"]));
        // Gameplay
        if (entityJson.contains("health"))
            world->AddComponent<ECS::HealthComponent>(entity, DeserializeHealthComponent(entityJson["health"]));
        if (entityJson.contains("damage"))
            world->AddComponent<ECS::DamageComponent>(entity, DeserializeDamageComponent(entityJson["damage"]));
        if (entityJson.contains("damageResistance"))
            world->AddComponent<ECS::DamageResistanceComponent>(entity, DeserializeDamageResistanceComponent(entityJson["damageResistance"]));
        if (entityJson.contains("triggerZone"))
            world->AddComponent<ECS::TriggerZoneComponent>(entity, DeserializeTriggerZoneComponent(entityJson["triggerZone"]));
        if (entityJson.contains("interactable"))
            world->AddComponent<ECS::InteractableComponent>(entity, DeserializeInteractableComponent(entityJson["interactable"]));
        if (entityJson.contains("pickup"))
            world->AddComponent<ECS::PickupComponent>(entity, DeserializePickupComponent(entityJson["pickup"]));
        if (entityJson.contains("tag"))
            world->AddComponent<ECS::TagComponent>(entity, DeserializeTagComponent(entityJson["tag"]));
        if (entityJson.contains("layer"))
            world->AddComponent<ECS::LayerComponent>(entity, DeserializeLayerComponent(entityJson["layer"]));
        if (entityJson.contains("billboard"))
            world->AddComponent<ECS::BillboardComponent>(entity, DeserializeBillboardComponent(entityJson["billboard"]));
        if (entityJson.contains("particleEmitter"))
            world->AddComponent<ECS::ParticleEmitterComponent>(entity, DeserializeParticleEmitterComponent(entityJson["particleEmitter"]));
        // 2D
        if (entityJson.contains("sprite2D"))
            world->AddComponent<ECS::Sprite2DComponent>(entity, DeserializeSprite2DComponent(entityJson["sprite2D"]));
        if (entityJson.contains("animatedSprite2D"))
            world->AddComponent<ECS::AnimatedSprite2DComponent>(entity, DeserializeAnimatedSprite2DComponent(entityJson["animatedSprite2D"]));
        if (entityJson.contains("tilemap"))
            world->AddComponent<ECS::TilemapComponent>(entity, DeserializeTilemapComponent(entityJson["tilemap"]));
        if (entityJson.contains("camera2DBounds"))
            world->AddComponent<ECS::Camera2DBoundsComponent>(entity, DeserializeCamera2DBoundsComponent(entityJson["camera2DBounds"]));
        // State/Dialogue/Tween
        if (entityJson.contains("stateMachine"))
            world->AddComponent<ECS::StateMachineComponent>(entity, DeserializeStateMachineComponent(entityJson["stateMachine"]));
        if (entityJson.contains("dialogue"))
            world->AddComponent<ECS::DialogueComponent>(entity, DeserializeDialogueComponent(entityJson["dialogue"]));
        if (entityJson.contains("tween"))
            world->AddComponent<ECS::TweenComponent>(entity, DeserializeTweenComponent(entityJson["tween"]));
        if (entityJson.contains("visualScript"))
            world->AddComponent<ECS::VisualScriptComponent>(entity, DeserializeVisualScriptComponent(entityJson["visualScript"]));
        // AI
        if (entityJson.contains("aiController"))
            world->AddComponent<ECS::AIControllerComponent>(entity, DeserializeAIControllerComponent(entityJson["aiController"]));
        if (entityJson.contains("behaviorTree"))
            world->AddComponent<ECS::BehaviorTreeComponent>(entity, DeserializeBehaviorTreeComponent(entityJson["behaviorTree"]));
        if (entityJson.contains("questFlow"))
            world->AddComponent<ECS::QuestFlowComponent>(entity, DeserializeQuestFlowComponent(entityJson["questFlow"]));
        if (entityJson.contains("followTarget"))
            world->AddComponent<ECS::FollowTargetComponent>(entity, DeserializeFollowTargetComponent(entityJson["followTarget"]));
        if (entityJson.contains("lookAtTarget"))
            world->AddComponent<ECS::LookAtTargetComponent>(entity, DeserializeLookAtTargetComponent(entityJson["lookAtTarget"]));
        if (entityJson.contains("waypoint"))
            world->AddComponent<ECS::WaypointComponent>(entity, DeserializeWaypointComponent(entityJson["waypoint"]));
        // Misc gameplay
        if (entityJson.contains("spawnPoint"))
            world->AddComponent<ECS::SpawnPointComponent>(entity, DeserializeSpawnPointComponent(entityJson["spawnPoint"]));
        if (entityJson.contains("timer"))
            world->AddComponent<ECS::TimerComponent>(entity, DeserializeTimerComponent(entityJson["timer"]));
        if (entityJson.contains("inventory"))
            world->AddComponent<ECS::InventoryComponent>(entity, DeserializeInventoryComponent(entityJson["inventory"]));
        if (entityJson.contains("saveData"))
            world->AddComponent<ECS::SaveDataComponent>(entity, DeserializeSaveDataComponent(entityJson["saveData"]));
        if (entityJson.contains("saveLoadMenu"))
            world->AddComponent<ECS::SaveLoadMenuComponent>(entity, DeserializeSaveLoadMenuComponent(entityJson["saveLoadMenu"]));
        if (entityJson.contains("resource"))
            world->AddComponent<ECS::ResourceComponent>(entity, DeserializeResourceComponent(entityJson["resource"]));
        if (entityJson.contains("footstep"))
            world->AddComponent<ECS::FootstepComponent>(entity, DeserializeFootstepComponent(entityJson["footstep"]));
        if (entityJson.contains("poolable"))
            world->AddComponent<ECS::PoolableComponent>(entity, DeserializePoolableComponent(entityJson["poolable"]));
        if (entityJson.contains("questState"))
            world->AddComponent<ECS::QuestStateComponent>(entity, DeserializeQuestStateComponent(entityJson["questState"]));
        if (entityJson.contains("hudWidget"))
            world->AddComponent<ECS::HUDWidgetComponent>(entity, DeserializeHUDWidgetComponent(entityJson["hudWidget"]));
        if (entityJson.contains("uiCanvas"))
            world->AddComponent<GUI::UICanvasComponent>(entity, DeserializeUICanvasComponent(entityJson["uiCanvas"]));
        if (entityJson.contains("cinematicCamera"))
            world->AddComponent<ECS::CinematicCameraComponent>(entity, DeserializeCinematicCameraComponent(entityJson["cinematicCamera"]));
        // Joints
        if (entityJson.contains("distanceJoint"))
            world->AddComponent<ECS::DistanceJointComponent>(entity, DeserializeDistanceJointComponent(entityJson["distanceJoint"]));
        if (entityJson.contains("hingeJoint"))
            world->AddComponent<ECS::HingeJointComponent>(entity, DeserializeHingeJointComponent(entityJson["hingeJoint"]));
        if (entityJson.contains("ballSocketJoint"))
            world->AddComponent<ECS::BallSocketJointComponent>(entity, DeserializeBallSocketJointComponent(entityJson["ballSocketJoint"]));
        if (entityJson.contains("springJoint"))
            world->AddComponent<ECS::SpringJointComponent>(entity, DeserializeSpringJointComponent(entityJson["springJoint"]));
        if (entityJson.contains("fixedJoint"))
            world->AddComponent<ECS::FixedJointComponent>(entity, DeserializeFixedJointComponent(entityJson["fixedJoint"]));
        if (entityJson.contains("sliderJoint"))
            world->AddComponent<ECS::SliderJointComponent>(entity, DeserializeSliderJointComponent(entityJson["sliderJoint"]));
        if (entityJson.contains("ragdoll"))
            world->AddComponent<ECS::RagdollComponent>(entity, DeserializeRagdollComponent(entityJson["ragdoll"]));
        // Flower
        if (entityJson.contains("jellyMesh"))
            world->AddComponent<ECS::JellyMeshComponent>(entity, DeserializeJellyMeshComponent(entityJson["jellyMesh"]));
        if (entityJson.contains("tether"))
            world->AddComponent<ECS::TetherComponent>(entity, DeserializeTetherComponent(entityJson["tether"]));
        if (entityJson.contains("grabbable"))
            world->AddComponent<ECS::GrabbableComponent>(entity, DeserializeGrabbableComponent(entityJson["grabbable"]));
        if (entityJson.contains("flowerStem"))
            world->AddComponent<ECS::FlowerStemComponent>(entity, DeserializeFlowerStemComponent(entityJson["flowerStem"]));
        if (entityJson.contains("flowerParticleConfig"))
            world->AddComponent<ECS::FlowerParticleConfigComponent>(entity, DeserializeFlowerParticleConfigComponent(entityJson["flowerParticleConfig"]));
        // LOD/Vegetation
        if (entityJson.contains("lod"))
            world->AddComponent<ECS::LODComponent>(entity, DeserializeLODComponent(entityJson["lod"]));
        if (entityJson.contains("grassVolume"))
            world->AddComponent<ECS::GrassVolumeComponent>(entity, DeserializeGrassVolumeComponent(entityJson["grassVolume"]));
        if (entityJson.contains("vegetation"))
            world->AddComponent<ECS::VegetationComponent>(entity, DeserializeVegetationComponent(entityJson["vegetation"]));

        return entity;

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Failed to deserialize entity from string: %s", e.what());
        return ECS::INVALID_ENTITY;
    }
}

// ============================================================================
// Static per-component serialization (for component remove undo)
// ============================================================================

std::string SceneSerializer::SerializeOneComponent(ECS::World* world, ECS::Entity entity, const std::string& key) {
    if (!world || !world->IsValid(entity)) return "";

    try {
        json j;
        // Dispatch by component key — must match the keys used in scene JSON
        if (key == "name" && world->HasComponent<ECS::NameComponent>(entity))
            j = SerializeNameComponent(*world->GetComponent<ECS::NameComponent>(entity));
        else if (key == "transform" && world->HasComponent<ECS::TransformComponent>(entity))
            j = SerializeTransformComponent(*world->GetComponent<ECS::TransformComponent>(entity));
        else if (key == "material" && world->HasComponent<ECS::MaterialComponent>(entity))
            j = SerializeMaterialComponent(*world->GetComponent<ECS::MaterialComponent>(entity));
        else if (key == "mesh" && world->HasComponent<ECS::MeshComponent>(entity))
            j = SerializeMeshComponent(*world->GetComponent<ECS::MeshComponent>(entity), true);
        else if (key == "skeleton" && world->HasComponent<ECS::SkeletonComponent>(entity))
            j = SerializeSkeletonComponent(*world->GetComponent<ECS::SkeletonComponent>(entity));
        else if (key == "animator" && world->HasComponent<ECS::AnimatorComponent>(entity))
            j = SerializeAnimatorComponent(*world->GetComponent<ECS::AnimatorComponent>(entity));
        else if (key == "light" && world->HasComponent<ECS::LightComponent>(entity))
            j = SerializeLightComponent(*world->GetComponent<ECS::LightComponent>(entity));
        else if (key == "notes" && world->HasComponent<ECS::NotesComponent>(entity))
            j = SerializeNotesComponent(*world->GetComponent<ECS::NotesComponent>(entity));
        else if (key == "text" && world->HasComponent<ECS::TextComponent>(entity))
            j = SerializeTextComponent(*world->GetComponent<ECS::TextComponent>(entity));
        else if (key == "camera" && world->HasComponent<ECS::CameraComponent>(entity))
            j = SerializeCameraComponent(*world->GetComponent<ECS::CameraComponent>(entity));
        else if (key == "weatherZone" && world->HasComponent<ECS::WeatherZoneComponent>(entity))
            j = SerializeWeatherZoneComponent(*world->GetComponent<ECS::WeatherZoneComponent>(entity));
        else if (key == "waterVolume" && world->HasComponent<ECS::WaterVolumeComponent>(entity))
            j = SerializeWaterVolumeComponent(*world->GetComponent<ECS::WaterVolumeComponent>(entity));
        else if (key == "shrubVolume" && world->HasComponent<ECS::ShrubVolumeComponent>(entity))
            j = SerializeShrubVolumeComponent(*world->GetComponent<ECS::ShrubVolumeComponent>(entity));
        else if (key == "treeVolume" && world->HasComponent<ECS::TreeVolumeComponent>(entity))
            j = SerializeTreeVolumeComponent(*world->GetComponent<ECS::TreeVolumeComponent>(entity));
        else if (key == "terrain" && world->HasComponent<ECS::TerrainComponent>(entity))
            j = SerializeTerrainComponent(*world->GetComponent<ECS::TerrainComponent>(entity));
        else if (key == "terrain2d" && world->HasComponent<ECS::Terrain2DComponent>(entity))
            j = SerializeTerrain2DComponent(*world->GetComponent<ECS::Terrain2DComponent>(entity));
        else if (key == "cameraTrigger" && world->HasComponent<ECS::CameraTriggerComponent>(entity))
            j = SerializeCameraTriggerComponent(*world->GetComponent<ECS::CameraTriggerComponent>(entity));
        else if (key == "temperatureZone" && world->HasComponent<ECS::TemperatureZoneComponent>(entity))
            j = SerializeTemperatureZoneComponent(*world->GetComponent<ECS::TemperatureZoneComponent>(entity));
        else if (key == "gravityZone" && world->HasComponent<ECS::GravityZoneComponent>(entity))
            j = SerializeGravityZoneComponent(*world->GetComponent<ECS::GravityZoneComponent>(entity));
        else if (key == "fluidVolume" && world->HasComponent<ECS::FluidVolumeComponent>(entity))
            j = SerializeFluidVolumeComponent(*world->GetComponent<ECS::FluidVolumeComponent>(entity));
        else if (key == "platformer2D" && world->HasComponent<ECS::Platformer2DController>(entity))
            j = SerializePlatformer2D(*world->GetComponent<ECS::Platformer2DController>(entity));
        else if (key == "topDown2D" && world->HasComponent<ECS::TopDown2DController>(entity))
            j = SerializeTopDown2D(*world->GetComponent<ECS::TopDown2DController>(entity));
        else if (key == "topDown3D" && world->HasComponent<ECS::TopDown3DController>(entity))
            j = SerializeTopDown3D(*world->GetComponent<ECS::TopDown3DController>(entity));
        else if (key == "thirdPerson" && world->HasComponent<ECS::ThirdPersonController>(entity))
            j = SerializeThirdPerson(*world->GetComponent<ECS::ThirdPersonController>(entity));
        else if (key == "firstPerson" && world->HasComponent<ECS::FirstPersonController>(entity))
            j = SerializeFirstPerson(*world->GetComponent<ECS::FirstPersonController>(entity));
        else if (key == "vehicle" && world->HasComponent<ECS::VehicleController>(entity))
            j = SerializeVehicle(*world->GetComponent<ECS::VehicleController>(entity));
        else if (key == "surfaceAligned" && world->HasComponent<ECS::SurfaceAlignedController>(entity))
            j = SerializeSurfaceAligned(*world->GetComponent<ECS::SurfaceAlignedController>(entity));
        else if (key == "networkIdentity" && world->HasComponent<ECS::NetworkIdentityComponent>(entity))
            j = SerializeNetworkIdentityComponent(*world->GetComponent<ECS::NetworkIdentityComponent>(entity));
        else if (key == "networkTransform" && world->HasComponent<ECS::NetworkTransformComponent>(entity))
            j = SerializeNetworkTransformComponent(*world->GetComponent<ECS::NetworkTransformComponent>(entity));
        else if (key == "possessable" && world->HasComponent<ECS::PossessableComponent>(entity))
            j = SerializePossessable(*world->GetComponent<ECS::PossessableComponent>(entity));
        else if (key == "lock" && world->HasComponent<ECS::LockComponent>(entity))
            j = SerializeLockComponent(*world->GetComponent<ECS::LockComponent>(entity));
        else if (key == "pushable" && world->HasComponent<ECS::PushableComponent>(entity))
            j = SerializePushableComponent(*world->GetComponent<ECS::PushableComponent>(entity));
        else if (key == "switch" && world->HasComponent<ECS::SwitchComponent>(entity))
            j = SerializeSwitchComponent(*world->GetComponent<ECS::SwitchComponent>(entity));
        else if (key == "goalZone" && world->HasComponent<ECS::GoalZoneComponent>(entity))
            j = SerializeGoalZoneComponent(*world->GetComponent<ECS::GoalZoneComponent>(entity));
        else if (key == "conveyor" && world->HasComponent<ECS::ConveyorComponent>(entity))
            j = SerializeConveyorComponent(*world->GetComponent<ECS::ConveyorComponent>(entity));
        else if (key == "teleporter" && world->HasComponent<ECS::TeleporterComponent>(entity))
            j = SerializeTeleporterComponent(*world->GetComponent<ECS::TeleporterComponent>(entity));
        else if (key == "destructible" && world->HasComponent<ECS::DestructibleComponent>(entity))
            j = SerializeDestructibleComponent(*world->GetComponent<ECS::DestructibleComponent>(entity));
        else if (key == "curlNoiseField" && world->HasComponent<ECS::CurlNoiseFieldComponent>(entity))
            j = SerializeCurlNoiseFieldComponent(*world->GetComponent<ECS::CurlNoiseFieldComponent>(entity));
        else if (key == "fractureConfig" && world->HasComponent<ECS::FractureConfigComponent>(entity))
            j = SerializeFractureConfigComponent(*world->GetComponent<ECS::FractureConfigComponent>(entity));
        else if (key == "movingPlatform" && world->HasComponent<ECS::MovingPlatformComponent>(entity))
            j = SerializeMovingPlatformComponent(*world->GetComponent<ECS::MovingPlatformComponent>(entity));
        else if (key == "scriptComponent" && world->HasComponent<ECS::ScriptComponent>(entity))
            j = SerializeScriptComponent(*world->GetComponent<ECS::ScriptComponent>(entity));
        else if (key == "audioSource" && world->HasComponent<ECS::AudioSourceComponent>(entity))
            j = SerializeAudioSourceComponent(*world->GetComponent<ECS::AudioSourceComponent>(entity));
        else if (key == "audioListener" && world->HasComponent<ECS::AudioListenerComponent>(entity))
            j = SerializeAudioListenerComponent(*world->GetComponent<ECS::AudioListenerComponent>(entity));
        else if (key == "rigidbody" && world->HasComponent<ECS::RigidbodyComponent>(entity))
            j = SerializeRigidbodyComponent(*world->GetComponent<ECS::RigidbodyComponent>(entity));
        else if (key == "boxCollider" && world->HasComponent<ECS::BoxColliderComponent>(entity))
            j = SerializeBoxColliderComponent(*world->GetComponent<ECS::BoxColliderComponent>(entity));
        else if (key == "sphereCollider" && world->HasComponent<ECS::SphereColliderComponent>(entity))
            j = SerializeSphereColliderComponent(*world->GetComponent<ECS::SphereColliderComponent>(entity));
        else if (key == "capsuleCollider" && world->HasComponent<ECS::CapsuleColliderComponent>(entity))
            j = SerializeCapsuleColliderComponent(*world->GetComponent<ECS::CapsuleColliderComponent>(entity));
        else if (key == "health" && world->HasComponent<ECS::HealthComponent>(entity))
            j = SerializeHealthComponent(*world->GetComponent<ECS::HealthComponent>(entity));
        else if (key == "damage" && world->HasComponent<ECS::DamageComponent>(entity))
            j = SerializeDamageComponent(*world->GetComponent<ECS::DamageComponent>(entity));
        else if (key == "damageResistance" && world->HasComponent<ECS::DamageResistanceComponent>(entity))
            j = SerializeDamageResistanceComponent(*world->GetComponent<ECS::DamageResistanceComponent>(entity));
        else if (key == "triggerZone" && world->HasComponent<ECS::TriggerZoneComponent>(entity))
            j = SerializeTriggerZoneComponent(*world->GetComponent<ECS::TriggerZoneComponent>(entity));
        else if (key == "interactable" && world->HasComponent<ECS::InteractableComponent>(entity))
            j = SerializeInteractableComponent(*world->GetComponent<ECS::InteractableComponent>(entity));
        else if (key == "pickup" && world->HasComponent<ECS::PickupComponent>(entity))
            j = SerializePickupComponent(*world->GetComponent<ECS::PickupComponent>(entity));
        else if (key == "tag" && world->HasComponent<ECS::TagComponent>(entity))
            j = SerializeTagComponent(*world->GetComponent<ECS::TagComponent>(entity));
        else if (key == "billboard" && world->HasComponent<ECS::BillboardComponent>(entity))
            j = SerializeBillboardComponent(*world->GetComponent<ECS::BillboardComponent>(entity));
        else if (key == "particleEmitter" && world->HasComponent<ECS::ParticleEmitterComponent>(entity))
            j = SerializeParticleEmitterComponent(*world->GetComponent<ECS::ParticleEmitterComponent>(entity));
        else if (key == "sprite2D" && world->HasComponent<ECS::Sprite2DComponent>(entity))
            j = SerializeSprite2DComponent(*world->GetComponent<ECS::Sprite2DComponent>(entity));
        else if (key == "animatedSprite2D" && world->HasComponent<ECS::AnimatedSprite2DComponent>(entity))
            j = SerializeAnimatedSprite2DComponent(*world->GetComponent<ECS::AnimatedSprite2DComponent>(entity));
        else if (key == "tilemap" && world->HasComponent<ECS::TilemapComponent>(entity))
            j = SerializeTilemapComponent(*world->GetComponent<ECS::TilemapComponent>(entity));
        else if (key == "camera2DBounds" && world->HasComponent<ECS::Camera2DBoundsComponent>(entity))
            j = SerializeCamera2DBoundsComponent(*world->GetComponent<ECS::Camera2DBoundsComponent>(entity));
        else if (key == "stateMachine" && world->HasComponent<ECS::StateMachineComponent>(entity))
            j = SerializeStateMachineComponent(*world->GetComponent<ECS::StateMachineComponent>(entity));
        else if (key == "dialogue" && world->HasComponent<ECS::DialogueComponent>(entity))
            j = SerializeDialogueComponent(*world->GetComponent<ECS::DialogueComponent>(entity));
        else if (key == "dialogueBox" && world->HasComponent<ECS::DialogueBoxComponent>(entity))
            j = SerializeDialogueBoxComponent(*world->GetComponent<ECS::DialogueBoxComponent>(entity));
        else if (key == "tween" && world->HasComponent<ECS::TweenComponent>(entity))
            j = SerializeTweenComponent(*world->GetComponent<ECS::TweenComponent>(entity));
        else if (key == "visualScript" && world->HasComponent<ECS::VisualScriptComponent>(entity))
            j = SerializeVisualScriptComponent(*world->GetComponent<ECS::VisualScriptComponent>(entity));
        else if (key == "aiController" && world->HasComponent<ECS::AIControllerComponent>(entity))
            j = SerializeAIControllerComponent(*world->GetComponent<ECS::AIControllerComponent>(entity));
        else if (key == "behaviorTree" && world->HasComponent<ECS::BehaviorTreeComponent>(entity))
            j = SerializeBehaviorTreeComponent(*world->GetComponent<ECS::BehaviorTreeComponent>(entity));
        else if (key == "questFlow" && world->HasComponent<ECS::QuestFlowComponent>(entity))
            j = SerializeQuestFlowComponent(*world->GetComponent<ECS::QuestFlowComponent>(entity));
        else if (key == "followTarget" && world->HasComponent<ECS::FollowTargetComponent>(entity))
            j = SerializeFollowTargetComponent(*world->GetComponent<ECS::FollowTargetComponent>(entity));
        else if (key == "lookAtTarget" && world->HasComponent<ECS::LookAtTargetComponent>(entity))
            j = SerializeLookAtTargetComponent(*world->GetComponent<ECS::LookAtTargetComponent>(entity));
        else if (key == "waypoint" && world->HasComponent<ECS::WaypointComponent>(entity))
            j = SerializeWaypointComponent(*world->GetComponent<ECS::WaypointComponent>(entity));
        else if (key == "spawnPoint" && world->HasComponent<ECS::SpawnPointComponent>(entity))
            j = SerializeSpawnPointComponent(*world->GetComponent<ECS::SpawnPointComponent>(entity));
        else if (key == "timer" && world->HasComponent<ECS::TimerComponent>(entity))
            j = SerializeTimerComponent(*world->GetComponent<ECS::TimerComponent>(entity));
        else if (key == "inventory" && world->HasComponent<ECS::InventoryComponent>(entity))
            j = SerializeInventoryComponent(*world->GetComponent<ECS::InventoryComponent>(entity));
        else if (key == "saveData" && world->HasComponent<ECS::SaveDataComponent>(entity))
            j = SerializeSaveDataComponent(*world->GetComponent<ECS::SaveDataComponent>(entity));
        else if (key == "saveLoadMenu" && world->HasComponent<ECS::SaveLoadMenuComponent>(entity))
            j = SerializeSaveLoadMenuComponent(*world->GetComponent<ECS::SaveLoadMenuComponent>(entity));
        else if (key == "resource" && world->HasComponent<ECS::ResourceComponent>(entity))
            j = SerializeResourceComponent(*world->GetComponent<ECS::ResourceComponent>(entity));
        else if (key == "footstep" && world->HasComponent<ECS::FootstepComponent>(entity))
            j = SerializeFootstepComponent(*world->GetComponent<ECS::FootstepComponent>(entity));
        else if (key == "poolable" && world->HasComponent<ECS::PoolableComponent>(entity))
            j = SerializePoolableComponent(*world->GetComponent<ECS::PoolableComponent>(entity));
        else if (key == "questState" && world->HasComponent<ECS::QuestStateComponent>(entity))
            j = SerializeQuestStateComponent(*world->GetComponent<ECS::QuestStateComponent>(entity));
        else if (key == "hudWidget" && world->HasComponent<ECS::HUDWidgetComponent>(entity))
            j = SerializeHUDWidgetComponent(*world->GetComponent<ECS::HUDWidgetComponent>(entity));
        else if (key == "uiCanvas" && world->HasComponent<GUI::UICanvasComponent>(entity))
            j = SerializeUICanvasComponent(*world->GetComponent<GUI::UICanvasComponent>(entity));
        else if (key == "cinematicCamera" && world->HasComponent<ECS::CinematicCameraComponent>(entity))
            j = SerializeCinematicCameraComponent(*world->GetComponent<ECS::CinematicCameraComponent>(entity));
        else if (key == "distanceJoint" && world->HasComponent<ECS::DistanceJointComponent>(entity))
            j = SerializeDistanceJointComponent(*world->GetComponent<ECS::DistanceJointComponent>(entity));
        else if (key == "hingeJoint" && world->HasComponent<ECS::HingeJointComponent>(entity))
            j = SerializeHingeJointComponent(*world->GetComponent<ECS::HingeJointComponent>(entity));
        else if (key == "ballSocketJoint" && world->HasComponent<ECS::BallSocketJointComponent>(entity))
            j = SerializeBallSocketJointComponent(*world->GetComponent<ECS::BallSocketJointComponent>(entity));
        else if (key == "springJoint" && world->HasComponent<ECS::SpringJointComponent>(entity))
            j = SerializeSpringJointComponent(*world->GetComponent<ECS::SpringJointComponent>(entity));
        else if (key == "fixedJoint" && world->HasComponent<ECS::FixedJointComponent>(entity))
            j = SerializeFixedJointComponent(*world->GetComponent<ECS::FixedJointComponent>(entity));
        else if (key == "sliderJoint" && world->HasComponent<ECS::SliderJointComponent>(entity))
            j = SerializeSliderJointComponent(*world->GetComponent<ECS::SliderJointComponent>(entity));
        else if (key == "ragdoll" && world->HasComponent<ECS::RagdollComponent>(entity))
            j = SerializeRagdollComponent(*world->GetComponent<ECS::RagdollComponent>(entity));
        else if (key == "jellyMesh" && world->HasComponent<ECS::JellyMeshComponent>(entity))
            j = SerializeJellyMeshComponent(*world->GetComponent<ECS::JellyMeshComponent>(entity));
        else if (key == "tether" && world->HasComponent<ECS::TetherComponent>(entity))
            j = SerializeTetherComponent(*world->GetComponent<ECS::TetherComponent>(entity));
        else if (key == "grabbable" && world->HasComponent<ECS::GrabbableComponent>(entity))
            j = SerializeGrabbableComponent(*world->GetComponent<ECS::GrabbableComponent>(entity));
        else if (key == "flowerStem" && world->HasComponent<ECS::FlowerStemComponent>(entity))
            j = SerializeFlowerStemComponent(*world->GetComponent<ECS::FlowerStemComponent>(entity));
        else if (key == "flowerParticleConfig" && world->HasComponent<ECS::FlowerParticleConfigComponent>(entity))
            j = SerializeFlowerParticleConfigComponent(*world->GetComponent<ECS::FlowerParticleConfigComponent>(entity));
        else if (key == "lod" && world->HasComponent<ECS::LODComponent>(entity))
            j = SerializeLODComponent(*world->GetComponent<ECS::LODComponent>(entity));
        else if (key == "grassVolume" && world->HasComponent<ECS::GrassVolumeComponent>(entity))
            j = SerializeGrassVolumeComponent(*world->GetComponent<ECS::GrassVolumeComponent>(entity));
        else if (key == "vegetation" && world->HasComponent<ECS::VegetationComponent>(entity))
            j = SerializeVegetationComponent(*world->GetComponent<ECS::VegetationComponent>(entity));
        else
            return "";

        return j.dump();

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Failed to serialize component '%s': %s", key.c_str(), e.what());
        return "";
    }
}

bool SceneSerializer::DeserializeOneComponent(ECS::World* world, ECS::Entity entity,
                                               const std::string& key, const std::string& jsonStr) {
    if (!world || !world->IsValid(entity) || jsonStr.empty()) return false;

    try {
        json j = json::parse(jsonStr);

        // Dispatch by component key — must match the keys used in scene JSON
        if (key == "name") { world->AddComponent<ECS::NameComponent>(entity, DeserializeNameComponent(j)); return true; }
        if (key == "transform") { world->AddComponent<ECS::TransformComponent>(entity, DeserializeTransformComponent(j)); return true; }
        if (key == "material") { world->AddComponent<ECS::MaterialComponent>(entity, DeserializeMaterialComponent(j)); return true; }
        if (key == "mesh") { world->AddComponent<ECS::MeshComponent>(entity, DeserializeMeshComponent(j)); return true; }
        if (key == "skeleton") { world->AddComponent<ECS::SkeletonComponent>(entity, DeserializeSkeletonComponent(j)); return true; }
        if (key == "animator") {
            std::shared_ptr<Animation::Skeleton> skel;
            if (world->HasComponent<ECS::SkeletonComponent>(entity))
                skel = world->GetComponent<ECS::SkeletonComponent>(entity)->skeleton;
            world->AddComponent<ECS::AnimatorComponent>(entity, DeserializeAnimatorComponent(j, skel));
            return true;
        }
        if (key == "light") { world->AddComponent<ECS::LightComponent>(entity, DeserializeLightComponent(j)); return true; }
        if (key == "notes") { world->AddComponent<ECS::NotesComponent>(entity, DeserializeNotesComponent(j)); return true; }
        if (key == "text") { world->AddComponent<ECS::TextComponent>(entity, DeserializeTextComponent(j)); return true; }
        if (key == "camera") { world->AddComponent<ECS::CameraComponent>(entity, DeserializeCameraComponent(j)); return true; }
        if (key == "weatherZone") { world->AddComponent<ECS::WeatherZoneComponent>(entity, DeserializeWeatherZoneComponent(j)); return true; }
        if (key == "waterVolume") { world->AddComponent<ECS::WaterVolumeComponent>(entity, DeserializeWaterVolumeComponent(j)); return true; }
        if (key == "shrubVolume") { world->AddComponent<ECS::ShrubVolumeComponent>(entity, DeserializeShrubVolumeComponent(j)); return true; }
        if (key == "treeVolume") { world->AddComponent<ECS::TreeVolumeComponent>(entity, DeserializeTreeVolumeComponent(j)); return true; }
        if (key == "terrain") { world->AddComponent<ECS::TerrainComponent>(entity, DeserializeTerrainComponent(j)); return true; }
        if (key == "terrain2d") { world->AddComponent<ECS::Terrain2DComponent>(entity, DeserializeTerrain2DComponent(j)); return true; }
        if (key == "cameraTrigger") { world->AddComponent<ECS::CameraTriggerComponent>(entity, DeserializeCameraTriggerComponent(j)); return true; }
        if (key == "temperatureZone") { world->AddComponent<ECS::TemperatureZoneComponent>(entity, DeserializeTemperatureZoneComponent(j)); return true; }
        if (key == "gravityZone") { world->AddComponent<ECS::GravityZoneComponent>(entity, DeserializeGravityZoneComponent(j)); return true; }
        if (key == "fluidVolume") { world->AddComponent<ECS::FluidVolumeComponent>(entity, DeserializeFluidVolumeComponent(j)); return true; }
        if (key == "platformer2D") { world->AddComponent<ECS::Platformer2DController>(entity, DeserializePlatformer2D(j)); return true; }
        if (key == "topDown2D") { world->AddComponent<ECS::TopDown2DController>(entity, DeserializeTopDown2D(j)); return true; }
        if (key == "topDown3D") { world->AddComponent<ECS::TopDown3DController>(entity, DeserializeTopDown3D(j)); return true; }
        if (key == "thirdPerson") { world->AddComponent<ECS::ThirdPersonController>(entity, DeserializeThirdPerson(j)); return true; }
        if (key == "firstPerson") { world->AddComponent<ECS::FirstPersonController>(entity, DeserializeFirstPerson(j)); return true; }
        if (key == "vehicle") { world->AddComponent<ECS::VehicleController>(entity, DeserializeVehicle(j)); return true; }
        if (key == "surfaceAligned") { world->AddComponent<ECS::SurfaceAlignedController>(entity, DeserializeSurfaceAligned(j)); return true; }
        if (key == "networkIdentity") { world->AddComponent<ECS::NetworkIdentityComponent>(entity, DeserializeNetworkIdentityComponent(j)); return true; }
        if (key == "networkTransform") { world->AddComponent<ECS::NetworkTransformComponent>(entity, DeserializeNetworkTransformComponent(j)); return true; }
        if (key == "possessable") { world->AddComponent<ECS::PossessableComponent>(entity, DeserializePossessable(j)); return true; }
        if (key == "lock") { world->AddComponent<ECS::LockComponent>(entity, DeserializeLockComponent(j)); return true; }
        if (key == "pushable") { world->AddComponent<ECS::PushableComponent>(entity, DeserializePushableComponent(j)); return true; }
        if (key == "switch") { world->AddComponent<ECS::SwitchComponent>(entity, DeserializeSwitchComponent(j)); return true; }
        if (key == "goalZone") { world->AddComponent<ECS::GoalZoneComponent>(entity, DeserializeGoalZoneComponent(j)); return true; }
        if (key == "conveyor") { world->AddComponent<ECS::ConveyorComponent>(entity, DeserializeConveyorComponent(j)); return true; }
        if (key == "teleporter") { world->AddComponent<ECS::TeleporterComponent>(entity, DeserializeTeleporterComponent(j)); return true; }
        if (key == "destructible") { world->AddComponent<ECS::DestructibleComponent>(entity, DeserializeDestructibleComponent(j)); return true; }
        if (key == "curlNoiseField") { world->AddComponent<ECS::CurlNoiseFieldComponent>(entity, DeserializeCurlNoiseFieldComponent(j)); return true; }
        if (key == "fractureConfig") { world->AddComponent<ECS::FractureConfigComponent>(entity, DeserializeFractureConfigComponent(j)); return true; }
        if (key == "movingPlatform") { world->AddComponent<ECS::MovingPlatformComponent>(entity, DeserializeMovingPlatformComponent(j)); return true; }
        if (key == "scriptComponent") { world->AddComponent<ECS::ScriptComponent>(entity, DeserializeScriptComponent(j)); return true; }
        if (key == "audioSource") { world->AddComponent<ECS::AudioSourceComponent>(entity, DeserializeAudioSourceComponent(j)); return true; }
        if (key == "audioListener") { world->AddComponent<ECS::AudioListenerComponent>(entity, DeserializeAudioListenerComponent(j)); return true; }
        if (key == "rigidbody") { world->AddComponent<ECS::RigidbodyComponent>(entity, DeserializeRigidbodyComponent(j)); return true; }
        if (key == "boxCollider") { world->AddComponent<ECS::BoxColliderComponent>(entity, DeserializeBoxColliderComponent(j)); return true; }
        if (key == "polygonCollider2D") { world->AddComponent<ECS::PolygonCollider2DComponent>(entity, DeserializePolygonCollider2DComponent(j)); return true; }
        if (key == "perFrameCollider") { world->AddComponent<ECS::PerFrameColliderComponent>(entity, DeserializePerFrameColliderComponent(j)); return true; }
        if (key == "sphereCollider") { world->AddComponent<ECS::SphereColliderComponent>(entity, DeserializeSphereColliderComponent(j)); return true; }
        if (key == "capsuleCollider") { world->AddComponent<ECS::CapsuleColliderComponent>(entity, DeserializeCapsuleColliderComponent(j)); return true; }
        if (key == "health") { world->AddComponent<ECS::HealthComponent>(entity, DeserializeHealthComponent(j)); return true; }
        if (key == "damage") { world->AddComponent<ECS::DamageComponent>(entity, DeserializeDamageComponent(j)); return true; }
        if (key == "damageResistance") { world->AddComponent<ECS::DamageResistanceComponent>(entity, DeserializeDamageResistanceComponent(j)); return true; }
        if (key == "triggerZone") { world->AddComponent<ECS::TriggerZoneComponent>(entity, DeserializeTriggerZoneComponent(j)); return true; }
        if (key == "interactable") { world->AddComponent<ECS::InteractableComponent>(entity, DeserializeInteractableComponent(j)); return true; }
        if (key == "pickup") { world->AddComponent<ECS::PickupComponent>(entity, DeserializePickupComponent(j)); return true; }
        if (key == "tag") { world->AddComponent<ECS::TagComponent>(entity, DeserializeTagComponent(j)); return true; }
        if (key == "billboard") { world->AddComponent<ECS::BillboardComponent>(entity, DeserializeBillboardComponent(j)); return true; }
        if (key == "particleEmitter") { world->AddComponent<ECS::ParticleEmitterComponent>(entity, DeserializeParticleEmitterComponent(j)); return true; }
        if (key == "sprite2D") { world->AddComponent<ECS::Sprite2DComponent>(entity, DeserializeSprite2DComponent(j)); return true; }
        if (key == "animatedSprite2D") { world->AddComponent<ECS::AnimatedSprite2DComponent>(entity, DeserializeAnimatedSprite2DComponent(j)); return true; }
        if (key == "tilemap") { world->AddComponent<ECS::TilemapComponent>(entity, DeserializeTilemapComponent(j)); return true; }
        if (key == "camera2DBounds") { world->AddComponent<ECS::Camera2DBoundsComponent>(entity, DeserializeCamera2DBoundsComponent(j)); return true; }
        if (key == "stateMachine") { world->AddComponent<ECS::StateMachineComponent>(entity, DeserializeStateMachineComponent(j)); return true; }
        if (key == "dialogue") { world->AddComponent<ECS::DialogueComponent>(entity, DeserializeDialogueComponent(j)); return true; }
        if (key == "dialogueBox") { world->AddComponent<ECS::DialogueBoxComponent>(entity, DeserializeDialogueBoxComponent(j)); return true; }
        if (key == "tween") { world->AddComponent<ECS::TweenComponent>(entity, DeserializeTweenComponent(j)); return true; }
        if (key == "visualScript") { world->AddComponent<ECS::VisualScriptComponent>(entity, DeserializeVisualScriptComponent(j)); return true; }
        if (key == "aiController") { world->AddComponent<ECS::AIControllerComponent>(entity, DeserializeAIControllerComponent(j)); return true; }
        if (key == "behaviorTree") { world->AddComponent<ECS::BehaviorTreeComponent>(entity, DeserializeBehaviorTreeComponent(j)); return true; }
        if (key == "questFlow") { world->AddComponent<ECS::QuestFlowComponent>(entity, DeserializeQuestFlowComponent(j)); return true; }
        if (key == "followTarget") { world->AddComponent<ECS::FollowTargetComponent>(entity, DeserializeFollowTargetComponent(j)); return true; }
        if (key == "lookAtTarget") { world->AddComponent<ECS::LookAtTargetComponent>(entity, DeserializeLookAtTargetComponent(j)); return true; }
        if (key == "waypoint") { world->AddComponent<ECS::WaypointComponent>(entity, DeserializeWaypointComponent(j)); return true; }
        if (key == "spawnPoint") { world->AddComponent<ECS::SpawnPointComponent>(entity, DeserializeSpawnPointComponent(j)); return true; }
        if (key == "timer") { world->AddComponent<ECS::TimerComponent>(entity, DeserializeTimerComponent(j)); return true; }
        if (key == "inventory") { world->AddComponent<ECS::InventoryComponent>(entity, DeserializeInventoryComponent(j)); return true; }
        if (key == "saveData") { world->AddComponent<ECS::SaveDataComponent>(entity, DeserializeSaveDataComponent(j)); return true; }
        if (key == "saveLoadMenu") { world->AddComponent<ECS::SaveLoadMenuComponent>(entity, DeserializeSaveLoadMenuComponent(j)); return true; }
        if (key == "resource") { world->AddComponent<ECS::ResourceComponent>(entity, DeserializeResourceComponent(j)); return true; }
        if (key == "footstep") { world->AddComponent<ECS::FootstepComponent>(entity, DeserializeFootstepComponent(j)); return true; }
        if (key == "poolable") { world->AddComponent<ECS::PoolableComponent>(entity, DeserializePoolableComponent(j)); return true; }
        if (key == "questState") { world->AddComponent<ECS::QuestStateComponent>(entity, DeserializeQuestStateComponent(j)); return true; }
        if (key == "hudWidget") { world->AddComponent<ECS::HUDWidgetComponent>(entity, DeserializeHUDWidgetComponent(j)); return true; }
        if (key == "uiCanvas") { world->AddComponent<GUI::UICanvasComponent>(entity, DeserializeUICanvasComponent(j)); return true; }
        if (key == "cinematicCamera") { world->AddComponent<ECS::CinematicCameraComponent>(entity, DeserializeCinematicCameraComponent(j)); return true; }
        if (key == "distanceJoint") { world->AddComponent<ECS::DistanceJointComponent>(entity, DeserializeDistanceJointComponent(j)); return true; }
        if (key == "hingeJoint") { world->AddComponent<ECS::HingeJointComponent>(entity, DeserializeHingeJointComponent(j)); return true; }
        if (key == "ballSocketJoint") { world->AddComponent<ECS::BallSocketJointComponent>(entity, DeserializeBallSocketJointComponent(j)); return true; }
        if (key == "springJoint") { world->AddComponent<ECS::SpringJointComponent>(entity, DeserializeSpringJointComponent(j)); return true; }
        if (key == "fixedJoint") { world->AddComponent<ECS::FixedJointComponent>(entity, DeserializeFixedJointComponent(j)); return true; }
        if (key == "sliderJoint") { world->AddComponent<ECS::SliderJointComponent>(entity, DeserializeSliderJointComponent(j)); return true; }
        if (key == "ragdoll") { world->AddComponent<ECS::RagdollComponent>(entity, DeserializeRagdollComponent(j)); return true; }
        if (key == "jellyMesh") { world->AddComponent<ECS::JellyMeshComponent>(entity, DeserializeJellyMeshComponent(j)); return true; }
        if (key == "tether") { world->AddComponent<ECS::TetherComponent>(entity, DeserializeTetherComponent(j)); return true; }
        if (key == "grabbable") { world->AddComponent<ECS::GrabbableComponent>(entity, DeserializeGrabbableComponent(j)); return true; }
        if (key == "flowerStem") { world->AddComponent<ECS::FlowerStemComponent>(entity, DeserializeFlowerStemComponent(j)); return true; }
        if (key == "flowerParticleConfig") { world->AddComponent<ECS::FlowerParticleConfigComponent>(entity, DeserializeFlowerParticleConfigComponent(j)); return true; }
        if (key == "lod") { world->AddComponent<ECS::LODComponent>(entity, DeserializeLODComponent(j)); return true; }
        if (key == "grassVolume") { world->AddComponent<ECS::GrassVolumeComponent>(entity, DeserializeGrassVolumeComponent(j)); return true; }
        if (key == "vegetation") { world->AddComponent<ECS::VegetationComponent>(entity, DeserializeVegetationComponent(j)); return true; }

        ENJIN_LOG_WARN(Asset, "Unknown component key for deserialization: '%s'", key.c_str());
        return false;

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Failed to deserialize component '%s': %s", key.c_str(), e.what());
        return false;
    }
}

} // namespace Scene
} // namespace Enjin
