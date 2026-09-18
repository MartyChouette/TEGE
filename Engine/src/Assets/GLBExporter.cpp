#include "Enjin/Assets/GLBExporter.h"

#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <fstream>
#include <limits>

using json = nlohmann::json;

namespace Enjin {
namespace Assets {

namespace {

// glTF component and accessor constants, spelled out rather than magic numbers.
constexpr u32 kFloat = 5126;
constexpr u32 kUnsignedInt = 5125;
constexpr u32 kArrayBuffer = 34962;         // vertex attributes
constexpr u32 kElementArrayBuffer = 34963;  // indices
constexpr u32 kTriangles = 4;

constexpr u32 kGlbMagic = 0x46546C67;       // "glTF"
constexpr u32 kGlbVersion = 2;
constexpr u32 kChunkJson = 0x4E4F534A;      // "JSON"
constexpr u32 kChunkBin = 0x004E4942;       // "BIN\0"

void Append(std::vector<u8>& out, const void* data, usize bytes) {
    const u8* p = static_cast<const u8*>(data);
    out.insert(out.end(), p, p + bytes);
}

// Every glTF bufferView must start on a multiple of its component size, and the
// spec requires 4-byte alignment for the ones used here. Getting this wrong
// produces a file that most viewers open and one rejects, which is the worst
// kind of broken.
void PadTo4(std::vector<u8>& out, u8 fill = 0) {
    while (out.size() % 4 != 0) out.push_back(fill);
}

} // namespace

GLBExportResult ExportEntitiesToGLB(ECS::World* world,
                                    const std::vector<ECS::Entity>& entities,
                                    const std::string& path) {
    GLBExportResult result;
    if (!world) {
        result.error = "no world";
        return result;
    }
    if (entities.empty()) {
        result.error = "nothing selected to export";
        return result;
    }

    json j;
    j["asset"] = {{"version", "2.0"}, {"generator", "Enjin (TEGE) GLB exporter"}};

    json meshes = json::array();
    json nodes = json::array();
    json accessors = json::array();
    json bufferViews = json::array();
    json materials = json::array();
    json sceneNodes = json::array();

    std::vector<u8> bin;

    for (ECS::Entity e : entities) {
        const auto* mesh = world->GetComponent<ECS::MeshComponent>(e);
        if (!mesh || mesh->vertices.empty() || mesh->indices.size() < 3) {
            // A selected light or camera is not a failure, it is just not
            // geometry. Counted so the caller can say so.
            ++result.skipped;
            continue;
        }

        // --- positions, normals, uvs, indices into the binary chunk ---
        std::vector<f32> pos, nrm, uv;
        pos.reserve(mesh->vertices.size() * 3);
        nrm.reserve(mesh->vertices.size() * 3);
        uv.reserve(mesh->vertices.size() * 2);

        // glTF requires accessor min/max for POSITION, and a viewer uses it to
        // frame the model. Computed rather than left out.
        f32 lo[3] = {std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(),
                     std::numeric_limits<f32>::max()};
        f32 hi[3] = {-std::numeric_limits<f32>::max(), -std::numeric_limits<f32>::max(),
                     -std::numeric_limits<f32>::max()};

        for (const auto& v : mesh->vertices) {
            const f32 p[3] = {v.position.x, v.position.y, v.position.z};
            for (int i = 0; i < 3; ++i) {
                pos.push_back(p[i]);
                if (p[i] < lo[i]) lo[i] = p[i];
                if (p[i] > hi[i]) hi[i] = p[i];
            }
            nrm.push_back(v.normal.x); nrm.push_back(v.normal.y); nrm.push_back(v.normal.z);
            uv.push_back(v.uv.x); uv.push_back(v.uv.y);
        }

        const u32 posView = static_cast<u32>(bufferViews.size());
        usize offset = bin.size();
        Append(bin, pos.data(), pos.size() * sizeof(f32));
        bufferViews.push_back({{"buffer", 0}, {"byteOffset", offset},
                               {"byteLength", pos.size() * sizeof(f32)},
                               {"target", kArrayBuffer}});
        PadTo4(bin);

        const u32 nrmView = static_cast<u32>(bufferViews.size());
        offset = bin.size();
        Append(bin, nrm.data(), nrm.size() * sizeof(f32));
        bufferViews.push_back({{"buffer", 0}, {"byteOffset", offset},
                               {"byteLength", nrm.size() * sizeof(f32)},
                               {"target", kArrayBuffer}});
        PadTo4(bin);

        const u32 uvView = static_cast<u32>(bufferViews.size());
        offset = bin.size();
        Append(bin, uv.data(), uv.size() * sizeof(f32));
        bufferViews.push_back({{"buffer", 0}, {"byteOffset", offset},
                               {"byteLength", uv.size() * sizeof(f32)},
                               {"target", kArrayBuffer}});
        PadTo4(bin);

        const u32 idxView = static_cast<u32>(bufferViews.size());
        offset = bin.size();
        Append(bin, mesh->indices.data(), mesh->indices.size() * sizeof(u32));
        bufferViews.push_back({{"buffer", 0}, {"byteOffset", offset},
                               {"byteLength", mesh->indices.size() * sizeof(u32)},
                               {"target", kElementArrayBuffer}});
        PadTo4(bin);

        const u32 posAcc = static_cast<u32>(accessors.size());
        accessors.push_back({{"bufferView", posView}, {"componentType", kFloat},
                             {"count", mesh->vertices.size()}, {"type", "VEC3"},
                             {"min", {lo[0], lo[1], lo[2]}},
                             {"max", {hi[0], hi[1], hi[2]}}});
        const u32 nrmAcc = static_cast<u32>(accessors.size());
        accessors.push_back({{"bufferView", nrmView}, {"componentType", kFloat},
                             {"count", mesh->vertices.size()}, {"type", "VEC3"}});
        const u32 uvAcc = static_cast<u32>(accessors.size());
        accessors.push_back({{"bufferView", uvView}, {"componentType", kFloat},
                             {"count", mesh->vertices.size()}, {"type", "VEC2"}});
        const u32 idxAcc = static_cast<u32>(accessors.size());
        accessors.push_back({{"bufferView", idxView}, {"componentType", kUnsignedInt},
                             {"count", mesh->indices.size()}, {"type", "SCALAR"}});

        // --- material: factors only, no textures (see the header) ---
        u32 matIndex = 0;
        {
            const auto* mat = world->GetComponent<ECS::MaterialComponent>(e);
            json pbr;
            if (mat) {
                pbr["baseColorFactor"] = {mat->baseColor.x, mat->baseColor.y,
                                          mat->baseColor.z, mat->opacity};
                pbr["metallicFactor"] = mat->metallic;
                pbr["roughnessFactor"] = mat->roughness;
            } else {
                pbr["baseColorFactor"] = {1.0f, 1.0f, 1.0f, 1.0f};
                pbr["metallicFactor"] = 0.0f;
                pbr["roughnessFactor"] = 1.0f;
            }
            matIndex = static_cast<u32>(materials.size());
            materials.push_back({{"pbrMetallicRoughness", pbr}});
        }

        json prim;
        prim["attributes"] = {{"POSITION", posAcc}, {"NORMAL", nrmAcc}, {"TEXCOORD_0", uvAcc}};
        prim["indices"] = idxAcc;
        prim["material"] = matIndex;
        prim["mode"] = kTriangles;

        const u32 meshIndex = static_cast<u32>(meshes.size());
        json meshJson;
        meshJson["primitives"] = json::array({prim});
        if (const auto* nameComp = world->GetComponent<ECS::NameComponent>(e)) {
            if (!nameComp->name.empty()) meshJson["name"] = nameComp->name;
        }
        meshes.push_back(meshJson);

        // The WORLD matrix, so a selection exports in the arrangement it has on
        // screen. glTF wants column-major 16 floats, which is the layout
        // Matrix4 already uses (flat f32 m[16]).
        const Math::Matrix4 wm = ECS::ComputeWorldMatrix(world, e);
        json node;
        node["mesh"] = meshIndex;
        node["matrix"] = json::array();
        for (int i = 0; i < 16; ++i) node["matrix"].push_back(wm.m[i]);
        if (meshJson.contains("name")) node["name"] = meshJson["name"];

        sceneNodes.push_back(static_cast<u32>(nodes.size()));
        nodes.push_back(node);

        ++result.meshesWritten;
        result.verticesWritten += static_cast<u32>(mesh->vertices.size());
    }

    if (result.meshesWritten == 0) {
        result.error = "none of the selected entities had a mesh to export";
        return result;
    }

    j["meshes"] = meshes;
    j["nodes"] = nodes;
    j["accessors"] = accessors;
    j["bufferViews"] = bufferViews;
    j["materials"] = materials;
    j["buffers"] = json::array({{{"byteLength", bin.size()}}});
    j["scenes"] = json::array({{{"nodes", sceneNodes}}});
    j["scene"] = 0;

    // --- container ---
    std::string jsonText = j.dump();
    // The JSON chunk pads with SPACES and the binary chunk with ZEROES. Padding
    // JSON with nulls is the classic way to produce a file that half the
    // viewers in the world refuse.
    while (jsonText.size() % 4 != 0) jsonText.push_back(' ');
    PadTo4(bin);

    const u32 jsonLen = static_cast<u32>(jsonText.size());
    const u32 binLen = static_cast<u32>(bin.size());
    const u32 total = 12 + 8 + jsonLen + (binLen ? (8 + binLen) : 0);

    std::ofstream f(path, std::ios::binary);
    if (!f) {
        result.error = "could not open " + path + " for writing";
        return result;
    }

    auto put = [&f](u32 v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    put(kGlbMagic); put(kGlbVersion); put(total);
    put(jsonLen); put(kChunkJson);
    f.write(jsonText.data(), static_cast<std::streamsize>(jsonLen));
    if (binLen) {
        put(binLen); put(kChunkBin);
        f.write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(binLen));
    }

    if (!f) {
        result.error = "write failed partway through " + path;
        return result;
    }

    result.success = true;
    ENJIN_LOG_INFO(Asset, "Exported %u mesh(es), %u vertices to %s",
                   result.meshesWritten, result.verticesWritten, path.c_str());
    return result;
}

} // namespace Assets
} // namespace Enjin
