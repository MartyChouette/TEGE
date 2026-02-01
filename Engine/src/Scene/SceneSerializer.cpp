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
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/Vegetation.h"
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
    j["iceColor"] = SerializeVector3(volume.iceColor);
    j["iceOpacity"] = volume.iceOpacity;
    j["freezeRate"] = volume.freezeRate;
    j["thawRate"] = volume.thawRate;
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

json SerializeAudioSourceComponent(const ECS::AudioSourceComponent& audio) {
    json j;
    j["clipPath"] = audio.clipPath;
    j["volume"] = audio.volume;
    j["pitch"] = audio.pitch;
    j["minDistance"] = audio.minDistance;
    j["maxDistance"] = audio.maxDistance;
    j["playOnAwake"] = audio.playOnAwake;
    j["loop"] = audio.loop;
    j["is3D"] = audio.is3D;
    j["spatialBlend"] = audio.spatialBlend;
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
    j["volumeScale"] = listener.volumeScale;
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
    j["mass"] = rb.mass;
    j["drag"] = rb.drag;
    j["angularDrag"] = rb.angularDrag;
    j["useGravity"] = rb.useGravity;
    j["gravityScale"] = rb.gravityScale;
    j["freezePositionX"] = rb.freezePositionX;
    j["freezePositionY"] = rb.freezePositionY;
    j["freezePositionZ"] = rb.freezePositionZ;
    j["freezeRotationX"] = rb.freezeRotationX;
    j["freezeRotationY"] = rb.freezeRotationY;
    j["freezeRotationZ"] = rb.freezeRotationZ;
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
    j["friction"] = col.friction;
    j["bounciness"] = col.bounciness;
    j["layer"] = col.layer;
    j["collisionMask"] = col.collisionMask;
    return j;
}

ECS::BoxColliderComponent DeserializeBoxColliderComponent(const json& j) {
    ECS::BoxColliderComponent col;
    if (j.contains("center")) col.center = DeserializeVector3(j["center"]);
    if (j.contains("size")) col.size = DeserializeVector3(j["size"]);
    if (j.contains("isTrigger")) col.isTrigger = j["isTrigger"].get<bool>();
    if (j.contains("friction")) col.friction = j["friction"].get<f32>();
    if (j.contains("bounciness")) col.bounciness = j["bounciness"].get<f32>();
    if (j.contains("layer")) col.layer = j["layer"].get<u32>();
    if (j.contains("collisionMask")) col.collisionMask = j["collisionMask"].get<u32>();
    return col;
}

json SerializeSphereColliderComponent(const ECS::SphereColliderComponent& col) {
    json j;
    j["center"] = SerializeVector3(col.center);
    j["radius"] = col.radius;
    j["isTrigger"] = col.isTrigger;
    j["friction"] = col.friction;
    j["bounciness"] = col.bounciness;
    j["layer"] = col.layer;
    j["collisionMask"] = col.collisionMask;
    return j;
}

ECS::SphereColliderComponent DeserializeSphereColliderComponent(const json& j) {
    ECS::SphereColliderComponent col;
    if (j.contains("center")) col.center = DeserializeVector3(j["center"]);
    if (j.contains("radius")) col.radius = j["radius"].get<f32>();
    if (j.contains("isTrigger")) col.isTrigger = j["isTrigger"].get<bool>();
    if (j.contains("friction")) col.friction = j["friction"].get<f32>();
    if (j.contains("bounciness")) col.bounciness = j["bounciness"].get<f32>();
    if (j.contains("layer")) col.layer = j["layer"].get<u32>();
    if (j.contains("collisionMask")) col.collisionMask = j["collisionMask"].get<u32>();
    return col;
}

json SerializeCapsuleColliderComponent(const ECS::CapsuleColliderComponent& col) {
    json j;
    j["center"] = SerializeVector3(col.center);
    j["radius"] = col.radius;
    j["height"] = col.height;
    j["direction"] = static_cast<u8>(col.direction);
    j["isTrigger"] = col.isTrigger;
    j["friction"] = col.friction;
    j["bounciness"] = col.bounciness;
    j["layer"] = col.layer;
    j["collisionMask"] = col.collisionMask;
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
    if (j.contains("layer")) col.layer = j["layer"].get<u32>();
    if (j.contains("collisionMask")) col.collisionMask = j["collisionMask"].get<u32>();
    return col;
}

// ============================================================================
// Health & Damage
// ============================================================================

json SerializeHealthComponent(const ECS::HealthComponent& h) {
    json j;
    j["maxHealth"] = h.maxHealth;
    j["regenRate"] = h.regenRate;
    j["regenDelay"] = h.regenDelay;
    j["isInvulnerable"] = h.isInvulnerable;
    j["invulnerabilityTime"] = h.invulnerabilityTime;
    j["maxShield"] = h.maxShield;
    j["shieldRegenRate"] = h.shieldRegenRate;
    j["shieldRegenDelay"] = h.shieldRegenDelay;
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
    j["damage"] = d.damage;
    j["knockbackForce"] = d.knockbackForce;
    j["destroyOnHit"] = d.destroyOnHit;
    j["damageOnce"] = d.damageOnce;
    j["damageInterval"] = d.damageInterval;
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
    j["sphereRadius"] = tz.sphereRadius;
    j["triggerMask"] = tz.triggerMask;
    j["triggerOnce"] = tz.triggerOnce;
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
    j["interactionRange"] = ic.interactionRange;
    j["requiresLookAt"] = ic.requiresLookAt;
    j["lookAtAngle"] = ic.lookAtAngle;
    j["isEnabled"] = ic.isEnabled;
    j["singleUse"] = ic.singleUse;
    j["highlightOnHover"] = ic.highlightOnHover;
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
    j["value"] = p.value;
    j["customId"] = p.customId;
    j["pickupRange"] = p.pickupRange;
    j["destroyOnPickup"] = p.destroyOnPickup;
    j["magnetToPlayer"] = p.magnetToPlayer;
    j["magnetRange"] = p.magnetRange;
    j["magnetSpeed"] = p.magnetSpeed;
    j["canRespawn"] = p.canRespawn;
    j["respawnTime"] = p.respawnTime;
    j["bobSpeed"] = p.bobSpeed;
    j["bobHeight"] = p.bobHeight;
    j["rotationSpeed"] = p.rotationSpeed;
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
    j["lockY"] = b.lockY;
    j["rotationOffset"] = b.rotationOffset;
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
    j["playOnAwake"] = pe.playOnAwake;
    j["loop"] = pe.loop;
    j["emissionRate"] = pe.emissionRate;
    j["burstCount"] = pe.burstCount;
    j["burstInterval"] = pe.burstInterval;
    j["lifetime"] = pe.lifetime;
    j["lifetimeVariance"] = pe.lifetimeVariance;
    j["startSpeed"] = pe.startSpeed;
    j["speedVariance"] = pe.speedVariance;
    j["startSize"] = pe.startSize;
    j["endSize"] = pe.endSize;
    j["startColor"] = SerializeVector3(pe.startColor);
    j["endColor"] = SerializeVector3(pe.endColor);
    j["startAlpha"] = pe.startAlpha;
    j["endAlpha"] = pe.endAlpha;
    j["shape"] = static_cast<u8>(pe.shape);
    j["shapeRadius"] = pe.shapeRadius;
    j["coneAngle"] = pe.coneAngle;
    j["gravity"] = SerializeVector3(pe.gravity);
    j["drag"] = pe.drag;
    j["texturePath"] = pe.texturePath;
    j["textureSheetX"] = pe.textureSheetX;
    j["textureSheetY"] = pe.textureSheetY;
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
    return pe;
}

// ============================================================================
// 2D Rendering
// ============================================================================

json SerializeSprite2DComponent(const ECS::Sprite2DComponent& s) {
    json j;
    j["texturePath"] = s.texturePath;
    j["srcX"] = s.srcX;
    j["srcY"] = s.srcY;
    j["srcWidth"] = s.srcWidth;
    j["srcHeight"] = s.srcHeight;
    j["size"] = SerializeVector2(s.size);
    j["pivot"] = SerializeVector2(s.pivot);
    j["tint"] = SerializeVector3(s.tint);
    j["alpha"] = s.alpha;
    j["sortingLayer"] = s.sortingLayer;
    j["orderInLayer"] = s.orderInLayer;
    j["flipX"] = s.flipX;
    j["flipY"] = s.flipY;
    j["visible"] = s.visible;
    return j;
}

ECS::Sprite2DComponent DeserializeSprite2DComponent(const json& j) {
    ECS::Sprite2DComponent s;
    if (j.contains("texturePath")) s.texturePath = j["texturePath"].get<std::string>();
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
        frame["srcX"] = f.srcX;
        frame["srcY"] = f.srcY;
        frame["duration"] = f.duration;
        framesArr.push_back(frame);
    }
    j["frames"] = framesArr;
    j["playing"] = a.playing;
    j["loop"] = a.loop;
    j["playbackSpeed"] = a.playbackSpeed;
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
    j["tileWidth"] = tm.tileWidth;
    j["tileHeight"] = tm.tileHeight;
    j["tilesetColumns"] = tm.tilesetColumns;
    j["worldTileWidth"] = tm.worldTileWidth;
    j["worldTileHeight"] = tm.worldTileHeight;
    j["hasCollision"] = tm.hasCollision;
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
    j["boundsPadding"] = cb.boundsPadding;
    j["followSmoothing"] = cb.followSmoothing;
    j["followOffset"] = SerializeVector2(cb.followOffset);
    j["minZoom"] = cb.minZoom;
    j["maxZoom"] = cb.maxZoom;
    j["currentZoom"] = cb.currentZoom;
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
    return cb;
}

// ============================================================================
// Logic
// ============================================================================

json SerializeStateMachineComponent(const ECS::StateMachineComponent& sm) {
    json j;
    j["currentState"] = sm.currentState;
    json floats = json::array();
    for (const auto& p : sm.floatParams) {
        floats.push_back(json::array({p.first, p.second}));
    }
    j["floatParams"] = floats;
    json ints = json::array();
    for (const auto& p : sm.intParams) {
        ints.push_back(json::array({p.first, p.second}));
    }
    j["intParams"] = ints;
    json bools = json::array();
    for (const auto& p : sm.boolParams) {
        bools.push_back(json::array({p.first, p.second}));
    }
    j["boolParams"] = bools;
    return j;
}

ECS::StateMachineComponent DeserializeStateMachineComponent(const json& j) {
    ECS::StateMachineComponent sm;
    if (j.contains("currentState")) sm.currentState = j["currentState"].get<std::string>();
    if (j.contains("floatParams") && j["floatParams"].is_array()) {
        for (const auto& p : j["floatParams"]) {
            sm.floatParams.push_back({p[0].get<std::string>(), p[1].get<f32>()});
        }
    }
    if (j.contains("intParams") && j["intParams"].is_array()) {
        for (const auto& p : j["intParams"]) {
            sm.intParams.push_back({p[0].get<std::string>(), p[1].get<i32>()});
        }
    }
    if (j.contains("boolParams") && j["boolParams"].is_array()) {
        for (const auto& p : j["boolParams"]) {
            sm.boolParams.push_back({p[0].get<std::string>(), p[1].get<bool>()});
        }
    }
    return sm;
}

json SerializeDialogueComponent(const ECS::DialogueComponent& d) {
    json j;
    j["dialogueLines"] = d.dialogueLines;
    j["charDelay"] = d.charDelay;
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
    return d;
}

// ============================================================================
// AI & Navigation
// ============================================================================

json SerializeAIControllerComponent(const ECS::AIControllerComponent& ai) {
    json j;
    j["detectionRange"] = ai.detectionRange;
    j["attackRange"] = ai.attackRange;
    j["loseTargetRange"] = ai.loseTargetRange;
    j["fieldOfView"] = ai.fieldOfView;
    j["moveSpeed"] = ai.moveSpeed;
    j["turnSpeed"] = ai.turnSpeed;
    j["stoppingDistance"] = ai.stoppingDistance;
    j["attackCooldown"] = ai.attackCooldown;
    j["attackDamage"] = ai.attackDamage;
    json patrolArr = json::array();
    for (const auto& p : ai.patrolPoints) {
        patrolArr.push_back(SerializeVector3(p));
    }
    j["patrolPoints"] = patrolArr;
    j["patrolWaitTime"] = ai.patrolWaitTime;
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
    return ai;
}

json SerializeFollowTargetComponent(const ECS::FollowTargetComponent& ft) {
    json j;
    j["followDistance"] = ft.followDistance;
    j["minDistance"] = ft.minDistance;
    j["maxDistance"] = ft.maxDistance;
    j["moveSpeed"] = ft.moveSpeed;
    j["smoothTime"] = ft.smoothTime;
    j["matchTargetRotation"] = ft.matchTargetRotation;
    j["rotationSpeed"] = ft.rotationSpeed;
    j["offset"] = SerializeVector3(ft.offset);
    j["useLocalOffset"] = ft.useLocalOffset;
    return j;
}

ECS::FollowTargetComponent DeserializeFollowTargetComponent(const json& j) {
    ECS::FollowTargetComponent ft;
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
    j["worldTarget"] = SerializeVector3(la.worldTarget);
    j["useWorldTarget"] = la.useWorldTarget;
    j["rotationSpeed"] = la.rotationSpeed;
    j["instant"] = la.instant;
    j["constrainX"] = la.constrainX;
    j["constrainY"] = la.constrainY;
    j["constrainZ"] = la.constrainZ;
    j["minYaw"] = la.minYaw;
    j["maxYaw"] = la.maxYaw;
    j["minPitch"] = la.minPitch;
    j["maxPitch"] = la.maxPitch;
    return j;
}

ECS::LookAtTargetComponent DeserializeLookAtTargetComponent(const json& j) {
    ECS::LookAtTargetComponent la;
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
    j["waitTime"] = wp.waitTime;
    j["radius"] = wp.radius;
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
    j["spawnOnStart"] = sp.spawnOnStart;
    j["spawnDelay"] = sp.spawnDelay;
    j["respawnTime"] = sp.respawnTime;
    j["maxSpawns"] = sp.maxSpawns;
    j["spawnRadius"] = sp.spawnRadius;
    j["randomRotation"] = sp.randomRotation;
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
    j["duration"] = t.duration;
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
        slot["quantity"] = s.quantity;
        slot["maxStack"] = s.maxStack;
        slotsArr.push_back(slot);
    }
    j["slots"] = slotsArr;
    j["maxSlots"] = static_cast<u64>(inv.maxSlots);
    j["coins"] = inv.coins;
    j["gems"] = inv.gems;
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
    j["savePosition"] = sd.savePosition;
    j["saveRotation"] = sd.saveRotation;
    j["saveScale"] = sd.saveScale;
    j["saveEnabled"] = sd.saveEnabled;
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
    if (j.contains("customData") && j["customData"].is_array()) {
        for (const auto& p : j["customData"]) {
            sd.customData.push_back({p[0].get<std::string>(), p[1].get<std::string>()});
        }
    }
    return sd;
}

// ============================================================================
// Flower Components
// ============================================================================

json SerializeJellyMeshComponent(const ECS::JellyMeshComponent& jm) {
    json j;
    j["springStiffness"] = jm.springStiffness;
    j["damping"] = jm.damping;
    j["maxStretch"] = jm.maxStretch;
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
    j["attachLocalPos"] = SerializeVector3(t.attachLocalPos);
    j["restLength"] = t.restLength;
    j["tetherStiffness"] = t.tetherStiffness;
    j["tetherDamping"] = t.tetherDamping;
    j["breakDistance"] = t.breakDistance;
    j["tensionRamp"] = t.tensionRamp;
    return j;
}

ECS::TetherComponent DeserializeTetherComponent(const json& j) {
    ECS::TetherComponent t;
    if (j.contains("attachLocalPos")) t.attachLocalPos = DeserializeVector3(j["attachLocalPos"]);
    if (j.contains("restLength")) t.restLength = j["restLength"].get<f32>();
    if (j.contains("tetherStiffness")) t.tetherStiffness = j["tetherStiffness"].get<f32>();
    if (j.contains("tetherDamping")) t.tetherDamping = j["tetherDamping"].get<f32>();
    if (j.contains("breakDistance")) t.breakDistance = j["breakDistance"].get<f32>();
    if (j.contains("tensionRamp")) t.tensionRamp = j["tensionRamp"].get<f32>();
    return t;
}

json SerializeGrabbableComponent(const ECS::GrabbableComponent& g) {
    json j;
    j["pullForce"] = g.pullForce;
    j["grabRadius"] = g.grabRadius;
    return j;
}

ECS::GrabbableComponent DeserializeGrabbableComponent(const json& j) {
    ECS::GrabbableComponent g;
    if (j.contains("pullForce")) g.pullForce = j["pullForce"].get<f32>();
    if (j.contains("grabRadius")) g.grabRadius = j["grabRadius"].get<f32>();
    return g;
}

json SerializeFlowerStemComponent(const ECS::FlowerStemComponent& fs) {
    json j;
    j["healthyBonus"] = fs.healthyBonus;
    j["witheredPenalty"] = fs.witheredPenalty;
    return j;
}

ECS::FlowerStemComponent DeserializeFlowerStemComponent(const json& j) {
    ECS::FlowerStemComponent fs;
    if (j.contains("healthyBonus")) fs.healthyBonus = j["healthyBonus"].get<f32>();
    if (j.contains("witheredPenalty")) fs.witheredPenalty = j["witheredPenalty"].get<f32>();
    return fs;
}

// ============================================================================
// LOD, Grass, Vegetation
// ============================================================================

json SerializeLODComponent(const ECS::LODComponent& lod) {
    json j;
    j["levelCount"] = lod.levelCount;
    j["baseDistance"] = lod.baseDistance;
    j["distanceMultiplier"] = lod.distanceMultiplier;
    j["enabled"] = lod.enabled;
    j["autoGenerated"] = lod.autoGenerated;
    json ratios = json::array();
    for (int i = 0; i < ECS::LODComponent::MAX_LEVELS; ++i) {
        ratios.push_back(lod.reductionRatios[i]);
    }
    j["reductionRatios"] = ratios;
    json levelsArr = json::array();
    for (int i = 0; i < lod.levelCount; ++i) {
        json level;
        level["mesh"] = SerializeMeshComponent(lod.levels[i].mesh, true);
        level["maxDistance"] = lod.levels[i].maxDistance;
        level["reductionRatio"] = lod.levels[i].reductionRatio;
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
    j["bladeHeight"] = gv.bladeHeight;
    j["bladeHeightVariance"] = gv.bladeHeightVariance;
    j["bladeWidth"] = gv.bladeWidth;
    j["baseColor"] = SerializeVector3(gv.baseColor);
    j["tipColor"] = SerializeVector3(gv.tipColor);
    j["windSwayStrength"] = gv.windSwayStrength;
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
    return gv;
}

json SerializeVegetationComponent(const ECS::VegetationComponent& v) {
    json j;
    j["swayStrength"] = v.swayStrength;
    j["swayFrequency"] = v.swayFrequency;
    j["useVertexColorWeight"] = v.useVertexColorWeight;
    return j;
}

ECS::VegetationComponent DeserializeVegetationComponent(const json& j) {
    ECS::VegetationComponent v;
    if (j.contains("swayStrength")) v.swayStrength = j["swayStrength"].get<f32>();
    if (j.contains("swayFrequency")) v.swayFrequency = j["swayFrequency"].get<f32>();
    if (j.contains("useVertexColorWeight")) v.useVertexColorWeight = j["useVertexColorWeight"].get<bool>();
    return v;
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

            // AI & Navigation
            if (m_World->HasComponent<ECS::AIControllerComponent>(entity)) {
                entityJson["aiController"] = SerializeAIControllerComponent(*m_World->GetComponent<ECS::AIControllerComponent>(entity));
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
            skyboxJson["topColor"] = { m_SkyboxConfig.topColor.x, m_SkyboxConfig.topColor.y, m_SkyboxConfig.topColor.z };
            skyboxJson["bottomColor"] = { m_SkyboxConfig.bottomColor.x, m_SkyboxConfig.bottomColor.y, m_SkyboxConfig.bottomColor.z };
            skyboxJson["horizonColor"] = { m_SkyboxConfig.horizonColor.x, m_SkyboxConfig.horizonColor.y, m_SkyboxConfig.horizonColor.z };
            skyboxJson["solidColor"] = { m_SkyboxConfig.solidColor.x, m_SkyboxConfig.solidColor.y, m_SkyboxConfig.solidColor.z };
            skyboxJson["rotation"] = m_SkyboxConfig.rotation;
            skyboxJson["sunDirection"] = { m_SkyboxConfig.sunDirection.x, m_SkyboxConfig.sunDirection.y, m_SkyboxConfig.sunDirection.z };
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
            if (sj.contains("sunDirection")) { auto& a = sj["sunDirection"]; m_SkyboxConfig.sunDirection = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
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

            // AI & Navigation
            if (entityJson.contains("aiController")) {
                m_World->AddComponent<ECS::AIControllerComponent>(entity, DeserializeAIControllerComponent(entityJson["aiController"]));
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

            // AI & Navigation
            if (m_World->HasComponent<ECS::AIControllerComponent>(entity)) {
                entityJson["aiController"] = SerializeAIControllerComponent(*m_World->GetComponent<ECS::AIControllerComponent>(entity));
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
            skyboxJson["topColor"] = { m_SkyboxConfig.topColor.x, m_SkyboxConfig.topColor.y, m_SkyboxConfig.topColor.z };
            skyboxJson["bottomColor"] = { m_SkyboxConfig.bottomColor.x, m_SkyboxConfig.bottomColor.y, m_SkyboxConfig.bottomColor.z };
            skyboxJson["horizonColor"] = { m_SkyboxConfig.horizonColor.x, m_SkyboxConfig.horizonColor.y, m_SkyboxConfig.horizonColor.z };
            skyboxJson["solidColor"] = { m_SkyboxConfig.solidColor.x, m_SkyboxConfig.solidColor.y, m_SkyboxConfig.solidColor.z };
            skyboxJson["rotation"] = m_SkyboxConfig.rotation;
            skyboxJson["sunDirection"] = { m_SkyboxConfig.sunDirection.x, m_SkyboxConfig.sunDirection.y, m_SkyboxConfig.sunDirection.z };
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
            if (sj.contains("sunDirection")) { auto& a = sj["sunDirection"]; m_SkyboxConfig.sunDirection = Math::Vector3(a[0].get<f32>(), a[1].get<f32>(), a[2].get<f32>()); }
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

            // AI & Navigation
            if (entityJson.contains("aiController")) {
                m_World->AddComponent<ECS::AIControllerComponent>(entity, DeserializeAIControllerComponent(entityJson["aiController"]));
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

        result.success = true;
        ENJIN_LOG_DEBUG(Asset, "Loaded scene from string (%zu entities)", result.entities.size());

    } catch (const std::exception& e) {
        result.error = std::string("JSON parsing error: ") + e.what();
    }

    return result;
}

} // namespace Scene
} // namespace Enjin
