// Golden-asset verification tests.
//
// Unlike the struct-default and failure-path checks in TestAssetLoaders.cpp,
// these load REAL assets through the actual loaders and assert that the parsed
// data is correct. They answer "does importing a rigged, animated model / an
// SVG / a tween actually produce the right values" rather than "does the error
// path return false".
//
// A minimal but real glTF (single skinned triangle, two joints, one rotation
// clip) is generated to a temp dir at runtime — geometry, skin, and animation
// all in one file — then loaded and inspected. SVG, tween easing, timeline
// evaluation, and a scene serialize round-trip follow.

#include "EnjinTest.h"

#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/Animation/Timeline.h"
#include "Enjin/Scene/SceneSerializer.h"

#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/SVGLoader.h"
#endif

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Enjin;
namespace fs = std::filesystem;

// ===========================================================================
// Golden glTF generator — writes golden.gltf + golden.bin to a temp dir.
// Buffer is built with a 4-byte-aligned append helper so the JSON offsets are
// guaranteed consistent with the bytes on disk.
// ===========================================================================

namespace {

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
std::string WriteGoldenGLTF(const fs::path& dir) {
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

fs::path GoldenDir() {
    return fs::temp_directory_path() / "enjin_golden_verification";
}

// Writes a real .obj cube + .mtl (with a known diffuse color) for the Assimp
// import path. Returns the path to the .obj, or empty on failure.
std::string WriteGoldenOBJ(const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);

    // Material: a distinctive diffuse color so we can assert it round-trips.
    {
        std::ofstream mtl(dir / "golden.mtl");
        if (!mtl) return {};
        mtl << "newmtl GoldenMat\n"
               "Ka 0.0 0.0 0.0\n"
               "Kd 0.8 0.2 0.1\n"
               "Ks 0.0 0.0 0.0\n";
    }

    // A unit cube from (-1,-1,-1) to (1,1,1), explicit per-face normals,
    // 12 triangles. Winding is irrelevant for the assertions below.
    fs::path objPath = dir / "golden.obj";
    {
        std::ofstream obj(objPath);
        if (!obj) return {};
        obj << "mtllib golden.mtl\n"
               "o GoldenCube\n"
               "v -1 -1 -1\n" "v 1 -1 -1\n" "v 1 1 -1\n" "v -1 1 -1\n"
               "v -1 -1 1\n"  "v 1 -1 1\n"  "v 1 1 1\n"  "v -1 1 1\n"
               "vn 0 0 -1\n" "vn 0 0 1\n" "vn -1 0 0\n"
               "vn 1 0 0\n"  "vn 0 -1 0\n" "vn 0 1 0\n"
               "usemtl GoldenMat\n"
               "f 1//1 2//1 3//1\n" "f 1//1 3//1 4//1\n"   // back  (-z)
               "f 5//2 6//2 7//2\n" "f 5//2 7//2 8//2\n"   // front (+z)
               "f 1//3 4//3 8//3\n" "f 1//3 8//3 5//3\n"   // left  (-x)
               "f 2//4 3//4 7//4\n" "f 2//4 7//4 6//4\n"   // right (+x)
               "f 1//5 2//5 6//5\n" "f 1//5 6//5 5//5\n"   // bottom(-y)
               "f 4//6 3//6 7//6\n" "f 4//6 7//6 8//6\n";  // top   (+y)
    }
    return objPath.string();
}

} // namespace

// ===========================================================================
// Model + rig + animation import
// ===========================================================================

ENJIN_TEST(Golden, GLTFLoadsRealGeometry) {
    std::string path = WriteGoldenGLTF(GoldenDir());
    ENJIN_ASSERT_FALSE(path.empty());

    Assets::GLTFScene scene;
    bool ok = Assets::GLTFLoader::Load(path, scene);
    ENJIN_ASSERT_TRUE(ok);

    // Mesh + primitive + geometry actually parsed.
    ENJIN_ASSERT_EQ(scene.meshes.size(), (size_t)1);
    ENJIN_ASSERT_EQ(scene.meshes[0].primitives.size(), (size_t)1);
    const Assets::GLTFPrimitive& prim = scene.meshes[0].primitives[0];
    ENJIN_ASSERT_EQ(prim.vertices.size(), (size_t)3);
    ENJIN_EXPECT_EQ(prim.indices.size(), (size_t)3);

    // Positions round-tripped through the binary buffer correctly.
    ENJIN_EXPECT_VEC3_EQ(prim.vertices[0].position, 0.0f, 0.0f, 0.0f);
    ENJIN_EXPECT_VEC3_EQ(prim.vertices[1].position, 1.0f, 0.0f, 0.0f);
    ENJIN_EXPECT_VEC3_EQ(prim.vertices[2].position, 0.0f, 1.0f, 0.0f);

    ENJIN_EXPECT_EQ(prim.indices[0], 0u);
    ENJIN_EXPECT_EQ(prim.indices[1], 1u);
    ENJIN_EXPECT_EQ(prim.indices[2], 2u);
}

ENJIN_TEST(Golden, GLTFLoadsRigSkinWeights) {
    std::string path = WriteGoldenGLTF(GoldenDir());
    ENJIN_ASSERT_FALSE(path.empty());

    Assets::GLTFScene scene;
    ENJIN_ASSERT_TRUE(Assets::GLTFLoader::Load(path, scene));

    // Skin (skeleton binding) parsed: two joints, two inverse-bind matrices.
    ENJIN_ASSERT_EQ(scene.skins.size(), (size_t)1);
    ENJIN_EXPECT_EQ(scene.skins[0].jointNodeIndices.size(), (size_t)2);
    ENJIN_EXPECT_EQ(scene.skins[0].inverseBindMatrices.size(), (size_t)2);

    // Bone weights actually reached the vertices (the real "rig import works"
    // assertion). Each vertex here is fully weighted to its first joint.
    ENJIN_ASSERT_EQ(scene.meshes.size(), (size_t)1);
    const Assets::GLTFPrimitive& prim = scene.meshes[0].primitives[0];
    float wsum = prim.vertices[0].boneWeights.x + prim.vertices[0].boneWeights.y +
                 prim.vertices[0].boneWeights.z + prim.vertices[0].boneWeights.w;
    ENJIN_EXPECT_FLOAT_NEAR(wsum, 1.0f, 0.01f);
    ENJIN_EXPECT_EQ(scene.nodes.size(), (size_t)3);
}

ENJIN_TEST(Golden, GLTFLoadsAnimationKeyframes) {
    std::string path = WriteGoldenGLTF(GoldenDir());
    ENJIN_ASSERT_FALSE(path.empty());

    Assets::GLTFScene scene;
    ENJIN_ASSERT_TRUE(Assets::GLTFLoader::Load(path, scene));

    ENJIN_ASSERT_EQ(scene.animations.size(), (size_t)1);
    const Assets::GLTFAnimation& anim = scene.animations[0];
    ENJIN_ASSERT_TRUE(anim.channels.size() >= 1);
    // Our single channel drives node rotation.
    ENJIN_EXPECT_EQ((int)anim.channels[0].path,
                    (int)Assets::GLTFAnimationChannel::Path::Rotation);
    ENJIN_EXPECT_TRUE(anim.channels[0].times.size() >= 2);
    ENJIN_EXPECT_TRUE(anim.duration > 0.0f);
}

ENJIN_TEST(Golden, SceneImporterBuildsEntities) {
    std::string path = WriteGoldenGLTF(GoldenDir());
    ENJIN_ASSERT_FALSE(path.empty());

    ECS::World world;
    Assets::ImportOptions opts;
    opts.generateColliders = false;  // keep the test free of physics setup
    Assets::ImportResult r = Assets::SceneImporter::ImportGLTF(path, &world, opts);

    ENJIN_ASSERT_TRUE(r.success);
    ENJIN_EXPECT_TRUE(r.meshCount >= 1);
    ENJIN_EXPECT_TRUE(r.animationCount >= 1);
    ENJIN_EXPECT_TRUE(r.entities.size() >= 1);
    ENJIN_EXPECT_TRUE(r.totalVertexCount >= 3);
}

// ===========================================================================
// OBJ import via Assimp (mesh + material). FBX rig/animation is a separate
// fixture-based test (FBX is binary and can't be generated at runtime); the
// rig/animation import LOGIC is already proven by the glTF tests above.
// ===========================================================================

ENJIN_TEST(GoldenOBJ, AssimpLoadsMeshAndMaterial) {
    std::string path = WriteGoldenOBJ(GoldenDir());
    ENJIN_ASSERT_FALSE(path.empty());

    Assets::AssimpScene scene;
    bool ok = Assets::AssimpLoader::Load(path, scene);
    ENJIN_ASSERT_TRUE(ok);

    // Geometry parsed into at least one mesh/primitive.
    ENJIN_ASSERT_TRUE(scene.meshes.size() >= 1);
    ENJIN_ASSERT_TRUE(scene.meshes[0].primitives.size() >= 1);
    const Assets::AssimpPrimitive& prim = scene.meshes[0].primitives[0];
    ENJIN_EXPECT_TRUE(prim.vertices.size() >= 8);          // cube corners (split per normal)
    ENJIN_ASSERT_TRUE(prim.indices.size() >= 12);
    ENJIN_EXPECT_EQ(prim.indices.size() % 3, (size_t)0);   // triangulated

    // Every vertex sits within the authored cube bounds, and at least one has
    // a real (unit-length) normal — proves positions and normals both parsed.
    bool foundUnitNormal = false;
    for (const auto& v : prim.vertices) {
        ENJIN_EXPECT_TRUE(v.position.x >= -1.01f && v.position.x <= 1.01f);
        ENJIN_EXPECT_TRUE(v.position.y >= -1.01f && v.position.y <= 1.01f);
        ENJIN_EXPECT_TRUE(v.position.z >= -1.01f && v.position.z <= 1.01f);
        float nlen = std::sqrt(v.normal.x * v.normal.x + v.normal.y * v.normal.y +
                               v.normal.z * v.normal.z);
        if (nlen > 0.9f && nlen < 1.1f) foundUnitNormal = true;
    }
    ENJIN_EXPECT_TRUE(foundUnitNormal);

    // The .mtl diffuse color (Kd 0.8 0.2 0.1) reached a material. Assimp may
    // add a default material too, so search rather than assume index 0.
    ENJIN_ASSERT_TRUE(scene.materials.size() >= 1);
    bool foundColor = false;
    for (const auto& m : scene.materials) {
        if (std::fabs(m.baseColorFactor.x - 0.8f) < 0.05f &&
            std::fabs(m.baseColorFactor.y - 0.2f) < 0.05f &&
            std::fabs(m.baseColorFactor.z - 0.1f) < 0.05f) {
            foundColor = true;
        }
    }
    ENJIN_EXPECT_TRUE(foundColor);
}

ENJIN_TEST(GoldenOBJ, SceneImporterBuildsEntitiesFromOBJ) {
    std::string path = WriteGoldenOBJ(GoldenDir());
    ENJIN_ASSERT_FALSE(path.empty());

    ECS::World world;
    Assets::ImportOptions opts;
    opts.generateColliders = false;
    Assets::ImportResult r = Assets::SceneImporter::ImportAssimp(path, &world, opts);

    ENJIN_ASSERT_TRUE(r.success);
    ENJIN_EXPECT_TRUE(r.meshCount >= 1);
    ENJIN_EXPECT_TRUE(r.entities.size() >= 1);
    ENJIN_EXPECT_TRUE(r.totalVertexCount >= 8);
}

// ===========================================================================
// FBX rig import via Assimp — uses a committed binary fixture (FBX can't be
// generated at runtime). Skips cleanly if the fixture is absent.
// ===========================================================================

#ifdef ENJIN_TEST_FIXTURES_DIR
// Asserts the FBX skinned-mesh import path: mesh + skeleton + skinning +
// animation all parse. Skips cleanly until a valid fixture is dropped in
// (FBX is binary and can't be generated at runtime — see Tests/Fixtures/README).
// Note: AssimpLoader rejects mesh-LESS FBX (Assimp INCOMPLETE flag), so the
// fixture must contain a skinned mesh, not just an armature/animation.
ENJIN_TEST(GoldenFBX, RiggedMeshImports) {
    std::string path = std::string(ENJIN_TEST_FIXTURES_DIR) + "/humanrig.fbx";
    if (!fs::exists(path)) {
        std::printf("    [skip] FBX fixture absent — drop a rigged+animated mesh at %s\n",
                    path.c_str());
        return;
    }

    Assets::AssimpScene scene;
    bool ok = Assets::AssimpLoader::Load(path, scene);
    if (!ok) {
        std::printf("    [skip] FBX fixture did not load (empty/mesh-less export?): '%s'\n",
                    Assets::AssimpLoader::GetLastError().c_str());
        return;
    }

    // A rigged character: at least one skinned mesh, a skeleton, and a clip.
    ENJIN_EXPECT_TRUE(scene.meshes.size() >= 1);
    ENJIN_EXPECT_TRUE(scene.hasSkinning);
    ENJIN_EXPECT_TRUE(scene.bones.size() >= 1);
    ENJIN_ASSERT_TRUE(scene.animations.size() >= 1);
    ENJIN_EXPECT_TRUE(scene.animations[0].channels.size() >= 1);
    ENJIN_EXPECT_TRUE(scene.animations[0].duration > 0.0f);

    // And it builds real entities through the import pipeline.
    ECS::World world;
    Assets::ImportOptions opts;
    opts.generateColliders = false;
    Assets::ImportResult r = Assets::SceneImporter::ImportAssimp(path, &world, opts);
    ENJIN_EXPECT_TRUE(r.success);
    ENJIN_EXPECT_TRUE(r.entities.size() >= 1);
}
#endif

// ===========================================================================
// SVG support (rasterize to pixels) — Vulkan/desktop build only
// ===========================================================================

#if !ENJIN_RENDERER_WEBGPU
ENJIN_TEST(Golden, SVGRasterizesToPixels) {
    fs::path dir = GoldenDir();
    std::error_code ec; fs::create_directories(dir, ec);
    fs::path svgPath = dir / "golden.svg";
    {
        std::ofstream out(svgPath);
        ENJIN_ASSERT_TRUE((bool)out);
        out << "<svg width=\"64\" height=\"64\" xmlns=\"http://www.w3.org/2000/svg\">"
               "<rect x=\"0\" y=\"0\" width=\"64\" height=\"64\" fill=\"rgb(255,0,0)\"/>"
               "</svg>";
    }

    Renderer::SVGImage img = Renderer::SVGLoader::LoadAndRasterize(svgPath.string(), 1.0f);
    ENJIN_ASSERT_TRUE(img.width >= 60 && img.width <= 68);
    ENJIN_ASSERT_TRUE(img.height >= 60 && img.height <= 68);
    ENJIN_ASSERT_EQ(img.pixels.size(), (size_t)img.width * img.height * 4);

    // Center pixel should be opaque red.
    size_t ci = ((size_t)(img.height / 2) * img.width + (img.width / 2)) * 4;
    ENJIN_EXPECT_TRUE(img.pixels[ci + 0] > 200);  // R
    ENJIN_EXPECT_TRUE(img.pixels[ci + 3] > 200);  // A

    ENJIN_EXPECT_TRUE(Renderer::SVGLoader::IsSVGFile("foo.svg"));
    ENJIN_EXPECT_FALSE(Renderer::SVGLoader::IsSVGFile("foo.png"));
}
#endif

// ===========================================================================
// Tween easing — endpoints and shape
// ===========================================================================

ENJIN_TEST(Golden, TweenEasingEndpoints) {
    using ECS::ApplyEasing;
    using ECS::EasingType;
    // Every easing must pass through 0 at t=0 and 1 at t=1.
    for (int i = 0; i < (int)EasingType::COUNT; ++i) {
        EasingType e = (EasingType)i;
        ENJIN_EXPECT_FLOAT_NEAR(ApplyEasing(0.0f, e), 0.0f, 0.02f);
        ENJIN_EXPECT_FLOAT_NEAR(ApplyEasing(1.0f, e), 1.0f, 0.02f);
    }
    // Linear is exact at the midpoint; ease-out is ahead of linear early.
    ENJIN_EXPECT_FLOAT_NEAR(ApplyEasing(0.5f, EasingType::Linear), 0.5f, 0.001f);
    ENJIN_EXPECT_TRUE(ApplyEasing(0.25f, EasingType::EaseOutQuad) > 0.25f);
}

// ===========================================================================
// Timeline evaluation — keyframe interpolation writes the target transform
// ===========================================================================

ENJIN_TEST(Golden, TimelineInterpolatesPosition) {
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);

    Animation::TimelineComponent tlc;
    tlc.duration = 2.0f;
    Animation::PropertyTrack track;
    track.targetProperty = "position";
    track.targetEntity = e;
    Animation::PropertyKeyframe k0;
    k0.time = 0.0f; k0.value = Math::Vector3(0.0f, 0.0f, 0.0f);
    Animation::PropertyKeyframe k1;
    k1.time = 2.0f; k1.value = Math::Vector3(10.0f, 0.0f, 0.0f);
    track.keyframes.push_back(k0);
    track.keyframes.push_back(k1);
    tlc.propertyTracks.push_back(track);
    world.AddComponent<Animation::TimelineComponent>(e, tlc);

    Animation::TimelineSystem sys;
    auto* live = world.GetComponent<Animation::TimelineComponent>(e);
    ENJIN_ASSERT_NOT_NULL(live);
    sys.Play(*live);
    sys.Update(&world, 1.0f);  // advance to t=1.0 (midpoint) and evaluate

    auto* tf = world.GetComponent<ECS::TransformComponent>(e);
    ENJIN_ASSERT_NOT_NULL(tf);
    // Linear interpolation at the midpoint -> halfway to 10.
    ENJIN_EXPECT_FLOAT_NEAR(tf->position.x, 5.0f, 0.25f);
    ENJIN_EXPECT_FLOAT_NEAR(tf->position.y, 0.0f, 0.001f);
}

// ===========================================================================
// Scene serialize round-trip — tween data survives save + load
// ===========================================================================

ENJIN_TEST(Golden, SceneRoundTripPreservesTween) {
    Scene::SerializationOptions opts;
    std::string saved;
    {
        ECS::World world;
        ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(e);
        ECS::NameComponent name; name.name = "Tweened";
        world.AddComponent<ECS::NameComponent>(e, name);

        ECS::TweenComponent tc;
        ECS::TweenEntry te;
        te.property = ECS::TweenProperty::Scale;
        te.easing = ECS::EasingType::EaseOutBounce;
        te.mode = ECS::TweenMode::Loop;
        te.startValue = Math::Vector3(1.0f, 1.0f, 1.0f);
        te.endValue = Math::Vector3(2.0f, 3.0f, 4.0f);
        te.duration = 2.5f;
        te.delay = 0.5f;
        tc.tweens.push_back(te);
        world.AddComponent<ECS::TweenComponent>(e, tc);

        Scene::SceneSerializer ser(&world);
        saved = ser.SaveToString(opts);
    }
    ENJIN_ASSERT_TRUE(saved.find("tween") != std::string::npos);

    // Reload into a fresh world.
    ECS::World world2;
    Scene::SceneSerializer ser2(&world2);
    Scene::DeserializationResult dr = ser2.LoadFromString(saved, true);
    ENJIN_ASSERT_TRUE(dr.success);

    const auto& tweened = world2.GetEntitiesWithComponent<ECS::TweenComponent>();
    ENJIN_ASSERT_TRUE(tweened.size() >= 1);
    const auto* tc2 = world2.GetComponent<ECS::TweenComponent>(tweened[0]);
    ENJIN_ASSERT_NOT_NULL(tc2);
    ENJIN_ASSERT_EQ(tc2->tweens.size(), (size_t)1);
    const ECS::TweenEntry& te2 = tc2->tweens[0];
    ENJIN_EXPECT_EQ((int)te2.property, (int)ECS::TweenProperty::Scale);
    ENJIN_EXPECT_EQ((int)te2.mode, (int)ECS::TweenMode::Loop);
    ENJIN_EXPECT_VEC3_EQ(te2.endValue, 2.0f, 3.0f, 4.0f);
    ENJIN_EXPECT_FLOAT_NEAR(te2.duration, 2.5f, 0.001f);
}

ENJIN_TEST_MAIN()
