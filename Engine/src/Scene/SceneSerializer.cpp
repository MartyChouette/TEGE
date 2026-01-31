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
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Accessibility/ContentWarning.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace Enjin {
namespace Scene {

// JSON serialization helpers for math types
namespace {

json SerializeVector2(const Math::Vector2& v) {
    return json::array({v.x, v.y});
}

json SerializeVector3(const Math::Vector3& v) {
    return json::array({v.x, v.y, v.z});
}

json SerializeQuaternion(const Math::Quaternion& q) {
    return json::array({q.x, q.y, q.z, q.w});
}

Math::Vector2 DeserializeVector2(const json& j) {
    return Math::Vector2(j[0].get<f32>(), j[1].get<f32>());
}

Math::Vector3 DeserializeVector3(const json& j) {
    return Math::Vector3(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>());
}

json SerializeVector4(const Math::Vector4& v) {
    return json::array({v.x, v.y, v.z, v.w});
}

Math::Vector4 DeserializeVector4(const json& j) {
    return Math::Vector4(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>(), j[3].get<f32>());
}

Math::Quaternion DeserializeQuaternion(const json& j) {
    return Math::Quaternion(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>(), j[3].get<f32>());
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
    return j;
}

json SerializeMaterialComponent(const ECS::MaterialComponent& material) {
    json j;
    j["baseColor"] = SerializeVector3(material.baseColor);
    j["opacity"] = material.opacity;
    j["metallic"] = material.metallic;
    j["roughness"] = material.roughness;
    j["emissiveColor"] = SerializeVector3(material.emissiveColor);
    j["emissiveStrength"] = material.emissiveStrength;
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
    j["alphaCutoff"] = material.alphaCutoff;
    // Height/parallax mapping
    j["heightTexturePath"] = material.heightTexturePath;
    j["parallaxScale"] = material.parallaxScale;
    // Retro rendering flags
    j["flatShading"] = material.flatShading;
    j["affineTexturing"] = material.affineTexturing;
    j["vertexSnapping"] = material.vertexSnapping;
    j["stippleTransparency"] = material.stippleTransparency;
    j["uvQuantize"] = material.uvQuantize;
    j["gouraudOnly"] = material.gouraudOnly;
    j["vertexSnapResolution"] = material.vertexSnapResolution;
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
    j["intensity"] = light.intensity;
    j["range"] = light.range;
    j["constantAttenuation"] = light.constantAttenuation;
    j["linearAttenuation"] = light.linearAttenuation;
    j["quadraticAttenuation"] = light.quadraticAttenuation;
    j["innerConeAngle"] = light.innerConeAngle;
    j["outerConeAngle"] = light.outerConeAngle;
    j["castShadows"] = light.castShadows;
    j["shadowMapResolution"] = light.shadowMapResolution;
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
    j["fontSize"] = text.fontSize;
    j["wrapWidth"] = text.wrapWidth;
    j["textureWidth"] = text.textureWidth;
    j["textureHeight"] = text.textureHeight;
    j["textColor"] = SerializeVector3(text.textColor);
    j["bgColor"] = SerializeVector3(text.bgColor);
    j["bgOpacity"] = text.bgOpacity;
    j["horizontalAlign"] = static_cast<i32>(text.horizontalAlign);
    j["paddingX"] = text.paddingX;
    j["paddingY"] = text.paddingY;
    return j;
}

json SerializeCameraComponent(const ECS::CameraComponent& camera) {
    json j;
    j["projectionType"] = static_cast<i32>(camera.projectionType);
    j["fieldOfView"] = camera.fieldOfView;
    j["nearPlane"] = camera.nearPlane;
    j["farPlane"] = camera.farPlane;
    j["orthoSize"] = camera.orthoSize;
    j["priority"] = camera.priority;
    j["isActive"] = camera.isActive;
    j["clearDepth"] = camera.clearDepth;
    j["clearColor"] = camera.clearColor;
    j["backgroundColor"] = SerializeVector3(camera.backgroundColor);
    j["viewportX"] = camera.viewportX;
    j["viewportY"] = camera.viewportY;
    j["viewportWidth"] = camera.viewportWidth;
    j["viewportHeight"] = camera.viewportHeight;
    j["cullingMask"] = camera.cullingMask;
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
    return transform;
}

ECS::MaterialComponent DeserializeMaterialComponent(const json& j) {
    ECS::MaterialComponent material;
    material.baseColor = DeserializeVector3(j["baseColor"]);
    material.opacity = j["opacity"].get<f32>();
    material.metallic = j["metallic"].get<f32>();
    material.roughness = j["roughness"].get<f32>();
    material.emissiveColor = DeserializeVector3(j["emissiveColor"]);
    material.emissiveStrength = j["emissiveStrength"].get<f32>();
    material.baseColorTexture = j["baseColorTexture"].get<i32>();
    material.normalTexture = j["normalTexture"].get<i32>();
    material.metallicRoughnessTexture = j["metallicRoughnessTexture"].get<i32>();
    material.emissiveTexture = j["emissiveTexture"].get<i32>();
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
    material.doubleSided = j["doubleSided"].get<bool>();
    material.castShadows = j["castShadows"].get<bool>();
    material.receiveShadows = j["receiveShadows"].get<bool>();
    material.alphaMode = static_cast<ECS::MaterialComponent::AlphaMode>(j["alphaMode"].get<i32>());
    material.alphaCutoff = j["alphaCutoff"].get<f32>();
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
    light.type = static_cast<ECS::LightType>(j["type"].get<i32>());
    light.color = DeserializeVector3(j["color"]);
    light.intensity = j["intensity"].get<f32>();
    light.range = j["range"].get<f32>();
    light.constantAttenuation = j["constantAttenuation"].get<f32>();
    light.linearAttenuation = j["linearAttenuation"].get<f32>();
    light.quadraticAttenuation = j["quadraticAttenuation"].get<f32>();
    light.innerConeAngle = j["innerConeAngle"].get<f32>();
    light.outerConeAngle = j["outerConeAngle"].get<f32>();
    light.castShadows = j["castShadows"].get<bool>();
    light.shadowMapResolution = j["shadowMapResolution"].get<u32>();
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
    camera.projectionType = static_cast<ECS::ProjectionType>(j["projectionType"].get<i32>());
    camera.fieldOfView = j["fieldOfView"].get<f32>();
    camera.nearPlane = j["nearPlane"].get<f32>();
    camera.farPlane = j["farPlane"].get<f32>();
    camera.orthoSize = j["orthoSize"].get<f32>();
    camera.priority = j["priority"].get<i32>();
    camera.isActive = j["isActive"].get<bool>();
    camera.clearDepth = j["clearDepth"].get<bool>();
    camera.clearColor = j["clearColor"].get<bool>();
    camera.backgroundColor = DeserializeVector3(j["backgroundColor"]);
    camera.viewportX = j["viewportX"].get<f32>();
    camera.viewportY = j["viewportY"].get<f32>();
    camera.viewportWidth = j["viewportWidth"].get<f32>();
    camera.viewportHeight = j["viewportHeight"].get<f32>();
    camera.cullingMask = j["cullingMask"].get<u32>();
    return camera;
}

json SerializeWeatherZoneComponent(const ECS::WeatherZoneComponent& zone) {
    json j;
    j["halfExtents"] = SerializeVector3(zone.halfExtents);
    j["weatherType"] = zone.weatherType;
    j["rainIntensity"] = zone.rainIntensity;
    j["snowIntensity"] = zone.snowIntensity;
    j["fogDensity"] = zone.fogDensity;
    j["fogColor"] = SerializeVector3(zone.fogColor);
    j["fogStart"] = zone.fogStart;
    j["fogEnd"] = zone.fogEnd;
    j["lightningEnabled"] = zone.lightningEnabled;
    j["lightningMinInterval"] = zone.lightningMinInterval;
    j["lightningMaxInterval"] = zone.lightningMaxInterval;
    j["windDirection"] = SerializeVector3(zone.windDirection);
    j["windStrength"] = zone.windStrength;
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
    j["opacity"] = volume.opacity;
    j["waveSpeed"] = volume.waveSpeed;
    j["waveHeight"] = volume.waveHeight;
    j["enableShore"] = volume.enableShore;
    j["shoreWidth"] = volume.shoreWidth;
    j["foamIntensity"] = volume.foamIntensity;
    j["foamScale"] = volume.foamScale;
    j["shoreColor"] = SerializeVector3(volume.shoreColor);
    j["priority"] = volume.priority;
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
    return volume;
}

json SerializeShrubVolumeComponent(const ECS::ShrubVolumeComponent& shrub) {
    json j;
    j["halfExtents"] = SerializeVector3(shrub.halfExtents);
    j["density"] = shrub.density;
    j["shrubHeight"] = shrub.shrubHeight;
    j["heightVariance"] = shrub.heightVariance;
    j["width"] = shrub.width;
    j["baseColor"] = SerializeVector3(shrub.baseColor);
    j["tipColor"] = SerializeVector3(shrub.tipColor);
    j["windSwayStrength"] = shrub.windSwayStrength;
    j["quadsPerShrub"] = shrub.quadsPerShrub;
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
    return shrub;
}

json SerializeTreeVolumeComponent(const ECS::TreeVolumeComponent& tree) {
    json j;
    j["halfExtents"] = SerializeVector3(tree.halfExtents);
    j["density"] = tree.density;
    j["trunkHeight"] = tree.trunkHeight;
    j["trunkWidth"] = tree.trunkWidth;
    j["canopyRadius"] = tree.canopyRadius;
    j["canopyOffset"] = tree.canopyOffset;
    j["trunkColor"] = SerializeVector3(tree.trunkColor);
    j["canopyBaseColor"] = SerializeVector3(tree.canopyBaseColor);
    j["canopyTipColor"] = SerializeVector3(tree.canopyTipColor);
    j["windSwayStrength"] = tree.windSwayStrength;
    j["canopyQuads"] = tree.canopyQuads;
    j["minHeightScale"] = tree.minHeightScale;
    j["maxHeightScale"] = tree.maxHeightScale;
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
    j["gridWidth"] = terrain.gridWidth;
    j["gridHeight"] = terrain.gridHeight;
    j["cellSize"] = terrain.cellSize;
    j["maxHeight"] = terrain.maxHeight;
    j["heightmap"] = terrain.heightmap;
    j["splatmap"] = terrain.splatmap;
    json layersArr = json::array();
    for (int i = 0; i < 4; ++i) {
        json layer;
        layer["texturePath"] = terrain.layers[i].texturePath;
        layer["tileScale"] = terrain.layers[i].tileScale;
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
    j["depth"] = terrain.depth;
    j["uvScale"] = terrain.uvScale;
    j["texturePath"] = terrain.texturePath;
    j["autoColliders"] = terrain.autoColliders;
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
    j["blendTime"] = trigger.blendTime;
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
    j["temperature"] = zone.temperature;
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
    j["gravityStrength"] = zone.gravityStrength;
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

// Base controller fields helper
json SerializeControllerBase(const ECS::CharacterControllerBase& base) {
    json j;
    j["moveSpeed"] = base.moveSpeed;
    j["sprintMultiplier"] = base.sprintMultiplier;
    j["isEnabled"] = base.isEnabled;
    j["useWASD"] = base.useWASD;
    j["useArrowKeys"] = base.useArrowKeys;
    j["useGamepad"] = base.useGamepad;
    j["gamepadIndex"] = base.gamepadIndex;
    j["gamepadLookSensitivity"] = base.gamepadLookSensitivity;
    j["gridMovement"] = base.gridMovement;
    j["gridCellSize"] = base.gridCellSize;
    j["gridMoveSpeed"] = base.gridMoveSpeed;
    return j;
}

void DeserializeControllerBase(const json& j, ECS::CharacterControllerBase& base) {
    if (j.contains("moveSpeed")) base.moveSpeed = j["moveSpeed"].get<f32>();
    if (j.contains("sprintMultiplier")) base.sprintMultiplier = j["sprintMultiplier"].get<f32>();
    if (j.contains("isEnabled")) base.isEnabled = j["isEnabled"].get<bool>();
    if (j.contains("useWASD")) base.useWASD = j["useWASD"].get<bool>();
    if (j.contains("useArrowKeys")) base.useArrowKeys = j["useArrowKeys"].get<bool>();
    if (j.contains("useGamepad")) base.useGamepad = j["useGamepad"].get<bool>();
    if (j.contains("gamepadIndex")) base.gamepadIndex = j["gamepadIndex"].get<i32>();
    if (j.contains("gamepadLookSensitivity")) base.gamepadLookSensitivity = j["gamepadLookSensitivity"].get<f32>();
    if (j.contains("gridMovement")) base.gridMovement = j["gridMovement"].get<bool>();
    if (j.contains("gridCellSize")) base.gridCellSize = j["gridCellSize"].get<f32>();
    if (j.contains("gridMoveSpeed")) base.gridMoveSpeed = j["gridMoveSpeed"].get<f32>();
}

json SerializePlatformer2D(const ECS::Platformer2DController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["jumpForce"] = ctrl.jumpForce;
    j["gravity"] = ctrl.gravity;
    j["maxJumps"] = ctrl.maxJumps;
    j["acceleration"] = ctrl.acceleration;
    j["deceleration"] = ctrl.deceleration;
    j["airControl"] = ctrl.airControl;
    j["coyoteTime"] = ctrl.coyoteTime;
    j["jumpBufferTime"] = ctrl.jumpBufferTime;
    j["enableWallJump"] = ctrl.enableWallJump;
    j["enableWallSlide"] = ctrl.enableWallSlide;
    j["wallSlideSpeed"] = ctrl.wallSlideSpeed;
    j["wallJumpForce"] = ctrl.wallJumpForce;
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
    if (j.contains("enableWallJump")) ctrl.enableWallJump = j["enableWallJump"].get<bool>();
    if (j.contains("enableWallSlide")) ctrl.enableWallSlide = j["enableWallSlide"].get<bool>();
    if (j.contains("wallSlideSpeed")) ctrl.wallSlideSpeed = j["wallSlideSpeed"].get<f32>();
    if (j.contains("wallJumpForce")) ctrl.wallJumpForce = j["wallJumpForce"].get<f32>();
    return ctrl;
}

json SerializeTopDown2D(const ECS::TopDown2DController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["acceleration"] = ctrl.acceleration;
    j["deceleration"] = ctrl.deceleration;
    j["rotateToFaceMovement"] = ctrl.rotateToFaceMovement;
    j["rotationSpeed"] = ctrl.rotationSpeed;
    j["enableDash"] = ctrl.enableDash;
    j["dashSpeed"] = ctrl.dashSpeed;
    j["dashDuration"] = ctrl.dashDuration;
    j["dashCooldown"] = ctrl.dashCooldown;
    return j;
}

ECS::TopDown2DController DeserializeTopDown2D(const json& j) {
    ECS::TopDown2DController ctrl;
    DeserializeControllerBase(j, ctrl);
    if (j.contains("acceleration")) ctrl.acceleration = j["acceleration"].get<f32>();
    if (j.contains("deceleration")) ctrl.deceleration = j["deceleration"].get<f32>();
    if (j.contains("rotateToFaceMovement")) ctrl.rotateToFaceMovement = j["rotateToFaceMovement"].get<bool>();
    if (j.contains("rotationSpeed")) ctrl.rotationSpeed = j["rotationSpeed"].get<f32>();
    if (j.contains("enableDash")) ctrl.enableDash = j["enableDash"].get<bool>();
    if (j.contains("dashSpeed")) ctrl.dashSpeed = j["dashSpeed"].get<f32>();
    if (j.contains("dashDuration")) ctrl.dashDuration = j["dashDuration"].get<f32>();
    if (j.contains("dashCooldown")) ctrl.dashCooldown = j["dashCooldown"].get<f32>();
    return ctrl;
}

json SerializeTopDown3D(const ECS::TopDown3DController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["acceleration"] = ctrl.acceleration;
    j["deceleration"] = ctrl.deceleration;
    j["rotateToFaceMovement"] = ctrl.rotateToFaceMovement;
    j["rotationSpeed"] = ctrl.rotationSpeed;
    j["cameraAngle"] = ctrl.cameraAngle;
    j["cameraDistance"] = ctrl.cameraDistance;
    j["cameraHeight"] = ctrl.cameraHeight;
    j["lockCameraToPlayer"] = ctrl.lockCameraToPlayer;
    j["enableClickToMove"] = ctrl.enableClickToMove;
    j["enableDash"] = ctrl.enableDash;
    j["dashSpeed"] = ctrl.dashSpeed;
    j["dashDuration"] = ctrl.dashDuration;
    j["dashCooldown"] = ctrl.dashCooldown;
    return j;
}

ECS::TopDown3DController DeserializeTopDown3D(const json& j) {
    ECS::TopDown3DController ctrl;
    DeserializeControllerBase(j, ctrl);
    if (j.contains("acceleration")) ctrl.acceleration = j["acceleration"].get<f32>();
    if (j.contains("deceleration")) ctrl.deceleration = j["deceleration"].get<f32>();
    if (j.contains("rotateToFaceMovement")) ctrl.rotateToFaceMovement = j["rotateToFaceMovement"].get<bool>();
    if (j.contains("rotationSpeed")) ctrl.rotationSpeed = j["rotationSpeed"].get<f32>();
    if (j.contains("cameraAngle")) ctrl.cameraAngle = j["cameraAngle"].get<f32>();
    if (j.contains("cameraDistance")) ctrl.cameraDistance = j["cameraDistance"].get<f32>();
    if (j.contains("cameraHeight")) ctrl.cameraHeight = j["cameraHeight"].get<f32>();
    if (j.contains("lockCameraToPlayer")) ctrl.lockCameraToPlayer = j["lockCameraToPlayer"].get<bool>();
    if (j.contains("enableClickToMove")) ctrl.enableClickToMove = j["enableClickToMove"].get<bool>();
    if (j.contains("enableDash")) ctrl.enableDash = j["enableDash"].get<bool>();
    if (j.contains("dashSpeed")) ctrl.dashSpeed = j["dashSpeed"].get<f32>();
    if (j.contains("dashDuration")) ctrl.dashDuration = j["dashDuration"].get<f32>();
    if (j.contains("dashCooldown")) ctrl.dashCooldown = j["dashCooldown"].get<f32>();
    return ctrl;
}

json SerializeThirdPerson(const ECS::ThirdPersonController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["acceleration"] = ctrl.acceleration;
    j["deceleration"] = ctrl.deceleration;
    j["jumpForce"] = ctrl.jumpForce;
    j["gravity"] = ctrl.gravity;
    j["rotateToFaceMovement"] = ctrl.rotateToFaceMovement;
    j["rotateToFaceCamera"] = ctrl.rotateToFaceCamera;
    j["rotationSpeed"] = ctrl.rotationSpeed;
    j["cameraDistance"] = ctrl.cameraDistance;
    j["cameraHeight"] = ctrl.cameraHeight;
    j["cameraMinDistance"] = ctrl.cameraMinDistance;
    j["cameraMaxDistance"] = ctrl.cameraMaxDistance;
    j["cameraPitch"] = ctrl.cameraPitch;
    j["cameraYaw"] = ctrl.cameraYaw;
    j["cameraMinPitch"] = ctrl.cameraMinPitch;
    j["cameraMaxPitch"] = ctrl.cameraMaxPitch;
    j["cameraSensitivity"] = ctrl.cameraSensitivity;
    j["cameraLerpSpeed"] = ctrl.cameraLerpSpeed;
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
    if (j.contains("rotateToFaceMovement")) ctrl.rotateToFaceMovement = j["rotateToFaceMovement"].get<bool>();
    if (j.contains("rotateToFaceCamera")) ctrl.rotateToFaceCamera = j["rotateToFaceCamera"].get<bool>();
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
    if (j.contains("enableCameraCollision")) ctrl.enableCameraCollision = j["enableCameraCollision"].get<bool>();
    if (j.contains("enableLockOn")) ctrl.enableLockOn = j["enableLockOn"].get<bool>();
    return ctrl;
}

json SerializeFirstPerson(const ECS::FirstPersonController& ctrl) {
    json j = SerializeControllerBase(ctrl);
    j["acceleration"] = ctrl.acceleration;
    j["deceleration"] = ctrl.deceleration;
    j["jumpForce"] = ctrl.jumpForce;
    j["gravity"] = ctrl.gravity;
    j["mouseSensitivity"] = ctrl.mouseSensitivity;
    j["minPitch"] = ctrl.minPitch;
    j["maxPitch"] = ctrl.maxPitch;
    j["invertY"] = ctrl.invertY;
    j["enableHeadBob"] = ctrl.enableHeadBob;
    j["headBobFrequency"] = ctrl.headBobFrequency;
    j["headBobAmplitude"] = ctrl.headBobAmplitude;
    j["enableCrouch"] = ctrl.enableCrouch;
    j["standingHeight"] = ctrl.standingHeight;
    j["crouchingHeight"] = ctrl.crouchingHeight;
    j["crouchSpeed"] = ctrl.crouchSpeed;
    j["sprintFOVIncrease"] = ctrl.sprintFOVIncrease;
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
    if (j.contains("invertY")) ctrl.invertY = j["invertY"].get<bool>();
    if (j.contains("enableHeadBob")) ctrl.enableHeadBob = j["enableHeadBob"].get<bool>();
    if (j.contains("headBobFrequency")) ctrl.headBobFrequency = j["headBobFrequency"].get<f32>();
    if (j.contains("headBobAmplitude")) ctrl.headBobAmplitude = j["headBobAmplitude"].get<f32>();
    if (j.contains("enableCrouch")) ctrl.enableCrouch = j["enableCrouch"].get<bool>();
    if (j.contains("standingHeight")) ctrl.standingHeight = j["standingHeight"].get<f32>();
    if (j.contains("crouchingHeight")) ctrl.crouchingHeight = j["crouchingHeight"].get<f32>();
    if (j.contains("crouchSpeed")) ctrl.crouchSpeed = j["crouchSpeed"].get<f32>();
    if (j.contains("sprintFOVIncrease")) ctrl.sprintFOVIncrease = j["sprintFOVIncrease"].get<f32>();
    return ctrl;
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

        for (ECS::Entity entity : entities) {
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

            // Hierarchy
            if (m_World->HasComponent<ECS::ParentComponent>(entity)) {
                entityJson["parent"] = static_cast<u64>(m_World->GetComponent<ECS::ParentComponent>(entity)->parent);
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
            skyboxJson["topColor"] = { m_SkyboxConfig.topColor.x, m_SkyboxConfig.topColor.y, m_SkyboxConfig.topColor.z };
            skyboxJson["bottomColor"] = { m_SkyboxConfig.bottomColor.x, m_SkyboxConfig.bottomColor.y, m_SkyboxConfig.bottomColor.z };
            skyboxJson["horizonColor"] = { m_SkyboxConfig.horizonColor.x, m_SkyboxConfig.horizonColor.y, m_SkyboxConfig.horizonColor.z };
            skyboxJson["solidColor"] = { m_SkyboxConfig.solidColor.x, m_SkyboxConfig.solidColor.y, m_SkyboxConfig.solidColor.z };
            skyboxJson["rotation"] = m_SkyboxConfig.rotation;
            json faces = json::array();
            for (const auto& p : m_SkyboxConfig.cubemapPaths) faces.push_back(p);
            skyboxJson["cubemapPaths"] = faces;
            sceneJson["skybox"] = skyboxJson;
        }

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
            if (sj.contains("cubemapPaths") && sj["cubemapPaths"].is_array()) {
                for (usize i = 0; i < 6 && i < sj["cubemapPaths"].size(); ++i) {
                    m_SkyboxConfig.cubemapPaths[i] = sj["cubemapPaths"][i].get<std::string>();
                }
            }
        } else {
            m_SkyboxConfig = Renderer::SkyboxConfig{};
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

        for (const auto& entityJson : sceneJson["entities"]) {
            ECS::Entity entity = m_World->CreateEntity();
            result.entities.push_back(entity);

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

            // Hierarchy
            if (entityJson.contains("parent")) {
                auto& pc = m_World->AddComponent<ECS::ParentComponent>(entity);
                pc.parent = static_cast<ECS::Entity>(entityJson["parent"].get<u64>());
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

        for (ECS::Entity entity : entities) {
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

            // Hierarchy
            if (m_World->HasComponent<ECS::ParentComponent>(entity)) {
                entityJson["parent"] = static_cast<u64>(m_World->GetComponent<ECS::ParentComponent>(entity)->parent);
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
            skyboxJson["topColor"] = { m_SkyboxConfig.topColor.x, m_SkyboxConfig.topColor.y, m_SkyboxConfig.topColor.z };
            skyboxJson["bottomColor"] = { m_SkyboxConfig.bottomColor.x, m_SkyboxConfig.bottomColor.y, m_SkyboxConfig.bottomColor.z };
            skyboxJson["horizonColor"] = { m_SkyboxConfig.horizonColor.x, m_SkyboxConfig.horizonColor.y, m_SkyboxConfig.horizonColor.z };
            skyboxJson["solidColor"] = { m_SkyboxConfig.solidColor.x, m_SkyboxConfig.solidColor.y, m_SkyboxConfig.solidColor.z };
            skyboxJson["rotation"] = m_SkyboxConfig.rotation;
            json faces = json::array();
            for (const auto& p : m_SkyboxConfig.cubemapPaths) faces.push_back(p);
            skyboxJson["cubemapPaths"] = faces;
            sceneJson["skybox"] = skyboxJson;
        }

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
            if (sj.contains("cubemapPaths") && sj["cubemapPaths"].is_array()) {
                for (usize i = 0; i < 6 && i < sj["cubemapPaths"].size(); ++i) {
                    m_SkyboxConfig.cubemapPaths[i] = sj["cubemapPaths"][i].get<std::string>();
                }
            }
        } else {
            m_SkyboxConfig = Renderer::SkyboxConfig{};
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

        for (const auto& entityJson : sceneJson["entities"]) {
            ECS::Entity entity = m_World->CreateEntity();
            result.entities.push_back(entity);

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

            // Hierarchy
            if (entityJson.contains("parent")) {
                auto& pc = m_World->AddComponent<ECS::ParentComponent>(entity);
                pc.parent = static_cast<ECS::Entity>(entityJson["parent"].get<u64>());
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
        }

        result.success = true;
        ENJIN_LOG_DEBUG(Asset, "Loaded scene from string (%zu entities)", result.entities.size());

    } catch (const std::exception& e) {
        result.error = std::string("JSON parsing error: ") + e.what();
    }

    return result;
}

} // namespace Scene
} // namespace Enjin
