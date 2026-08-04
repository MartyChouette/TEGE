#pragma once

// Golden glTF generator — writes golden.gltf + golden.bin (a minimal but REAL
// skinned, animated model: single triangle, two joints, one rotation clip) to
// a directory at runtime. Shared by TestGoldenVerification.cpp (import
// correctness tests) and TestAdr3SceneEmit.cpp (probe-scene emit tool).
// The buffer is built with a 4-byte-aligned append helper so the JSON offsets
// are guaranteed consistent with the bytes on disk.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace GoldenGLTF {

struct BufView { size_t offset; size_t length; };

struct GoldenBuffer {
    std::vector<uint8_t> bytes;
    void pad4() { while (bytes.size() % 4 != 0) bytes.push_back(0); }
    BufView append(const void* p, size_t n) {
        pad4();
        size_t off = bytes.size();
        const uint8_t* b = static_cast<const uint8_t*>(p);
        bytes.insert(bytes.end(), b, b + n);
        return { off, n };
    }
};

// Returns the full path to golden.gltf, or empty string on failure.
inline std::string WriteGoldenGLTF(const std::filesystem::path& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(dir, ec);

    GoldenBuffer buf;

    // 1. Positions (3 verts, VEC3 float): a unit triangle in the XY plane.
    const float positions[9] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
    };
    BufView vPos = buf.append(positions, sizeof(positions));

    // 2. Indices (3, SCALAR u16).
    const uint16_t indices[3] = { 0, 1, 2 };
    BufView vIdx = buf.append(indices, sizeof(indices));

    // 3. Joints (3 verts, VEC4 u8). Each vertex bound to joint 0 (or 1).
    const uint8_t joints[12] = {
        0, 0, 0, 0,
        1, 0, 0, 0,
        0, 0, 0, 0,
    };
    BufView vJoint = buf.append(joints, sizeof(joints));

    // 4. Weights (3 verts, VEC4 float). Fully weighted to the first joint.
    const float weights[12] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f,
    };
    BufView vWeight = buf.append(weights, sizeof(weights));

    // 5. Inverse bind matrices (2 joints, MAT4 float) — both identity.
    float ibm[32];
    for (int m = 0; m < 2; ++m) {
        for (int i = 0; i < 16; ++i) ibm[m * 16 + i] = 0.0f;
        ibm[m * 16 + 0] = 1.0f; ibm[m * 16 + 5] = 1.0f;
        ibm[m * 16 + 10] = 1.0f; ibm[m * 16 + 15] = 1.0f;
    }
    BufView vIbm = buf.append(ibm, sizeof(ibm));

    // 6. Animation input times (2, SCALAR float).
    const float times[2] = { 0.0f, 1.0f };
    BufView vTime = buf.append(times, sizeof(times));

    // 7. Animation output rotations (2, VEC4 float): identity -> 90deg about Y.
    const float rots[8] = {
        0.0f, 0.0f,        0.0f, 1.0f,         // identity
        0.0f, 0.70710678f, 0.0f, 0.70710678f,  // 90 deg about Y
    };
    BufView vRot = buf.append(rots, sizeof(rots));

    // Write the binary blob.
    fs::path binPath = dir / "golden.bin";
    {
        std::ofstream bin(binPath, std::ios::binary);
        if (!bin) return {};
        bin.write(reinterpret_cast<const char*>(buf.bytes.data()),
                  static_cast<std::streamsize>(buf.bytes.size()));
    }

    // Build the glTF JSON with the computed offsets.
    auto bv = [](const BufView& v, const char* target) {
        std::string s = "{\"buffer\":0,\"byteOffset\":" + std::to_string(v.offset) +
                        ",\"byteLength\":" + std::to_string(v.length);
        if (target) { s += ",\"target\":"; s += target; }
        return s + "}";
    };

    std::string json;
    json += "{\n";
    json += "\"asset\":{\"version\":\"2.0\",\"generator\":\"EnjinGoldenTest\"},\n";
    json += "\"scene\":0,\n";
    json += "\"scenes\":[{\"nodes\":[0,1]}],\n";
    json += "\"nodes\":[";
    json += "{\"name\":\"SkinnedMesh\",\"mesh\":0,\"skin\":0},";
    json += "{\"name\":\"Joint_Root\",\"children\":[2]},";
    json += "{\"name\":\"Joint_Tip\"}";
    json += "],\n";
    json += "\"meshes\":[{\"name\":\"Tri\",\"primitives\":[{\"attributes\":{"
            "\"POSITION\":0,\"JOINTS_0\":2,\"WEIGHTS_0\":3},\"indices\":1}]}],\n";
    json += "\"skins\":[{\"name\":\"Armature\",\"inverseBindMatrices\":4,"
            "\"skeleton\":1,\"joints\":[1,2]}],\n";
    json += "\"animations\":[{\"name\":\"Spin\","
            "\"channels\":[{\"sampler\":0,\"target\":{\"node\":1,\"path\":\"rotation\"}}],"
            "\"samplers\":[{\"input\":5,\"output\":6,\"interpolation\":\"LINEAR\"}]}],\n";
    json += "\"buffers\":[{\"uri\":\"golden.bin\",\"byteLength\":" +
            std::to_string(buf.bytes.size()) + "}],\n";
    json += "\"bufferViews\":[";
    json += bv(vPos, "34962") + ",";
    json += bv(vIdx, "34963") + ",";
    json += bv(vJoint, "34962") + ",";
    json += bv(vWeight, "34962") + ",";
    json += bv(vIbm, nullptr) + ",";
    json += bv(vTime, nullptr) + ",";
    json += bv(vRot, nullptr);
    json += "],\n";
    json += "\"accessors\":[";
    json += "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
            "\"min\":[0,0,0],\"max\":[1,1,0]},";
    json += "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"},";
    json += "{\"bufferView\":2,\"componentType\":5121,\"count\":3,\"type\":\"VEC4\"},";
    json += "{\"bufferView\":3,\"componentType\":5126,\"count\":3,\"type\":\"VEC4\"},";
    json += "{\"bufferView\":4,\"componentType\":5126,\"count\":2,\"type\":\"MAT4\"},";
    json += "{\"bufferView\":5,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\","
            "\"min\":[0],\"max\":[1]},";
    json += "{\"bufferView\":6,\"componentType\":5126,\"count\":2,\"type\":\"VEC4\"}";
    json += "]\n";
    json += "}\n";

    fs::path gltfPath = dir / "golden.gltf";
    {
        std::ofstream out(gltfPath);
        if (!out) return {};
        out << json;
    }
    return gltfPath.string();
}

} // namespace GoldenGLTF
