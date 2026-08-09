#include "EnjinTest.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Assets/SWFLoader.h"
#include "Enjin/Assets/SWFConverter.h"
#include "Enjin/Assets/AssetMetadata.h"
#include "Enjin/Assets/PLYLoader.h"
#include "Enjin/Assets/VOXLoader.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include <filesystem>
#include <cfloat>
#include <cmath>

using namespace Enjin;
using namespace Enjin::Assets;

// ===========================================================================
// GLTFMaterial Defaults
// ===========================================================================

ENJIN_TEST(GLTF, MaterialDefaults) {
    GLTFMaterial m;
    ENJIN_EXPECT_FLOAT_EQ(m.baseColorFactor.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.baseColorFactor.w, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.metallicFactor, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.roughnessFactor, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.emissiveFactor.x, 0.0f);
    ENJIN_EXPECT_EQ(m.baseColorTextureIndex, -1);
    ENJIN_EXPECT_EQ(m.normalTextureIndex, -1);
    ENJIN_EXPECT_FALSE(m.doubleSided);
    ENJIN_EXPECT_FLOAT_EQ(m.alphaCutoff, 0.5f);
    ENJIN_EXPECT_EQ((int)m.alphaMode, (int)GLTFMaterial::AlphaMode::Opaque);
}

ENJIN_TEST(GLTF, NodeDefaults) {
    GLTFNode n;
    ENJIN_EXPECT_EQ(n.meshIndex, -1);
    ENJIN_EXPECT_EQ(n.skinIndex, -1);
    ENJIN_EXPECT_FLOAT_EQ(n.scale.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(n.scale.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(n.scale.z, 1.0f);
}

ENJIN_TEST(GLTF, SkinDefaults) {
    GLTFSkin s;
    ENJIN_EXPECT_EQ(s.skeletonRootNode, -1);
    ENJIN_EXPECT_EQ(s.jointNodeIndices.size(), (size_t)0);
}

ENJIN_TEST(GLTF, AnimDefaults) {
    GLTFAnimation a;
    ENJIN_EXPECT_FLOAT_EQ(a.duration, 0.0f);
    ENJIN_EXPECT_EQ(a.channels.size(), (size_t)0);
}

ENJIN_TEST(GLTF, LoadNonexistentFails) {
    GLTFScene scene;
    bool ok = GLTFLoader::Load("nonexistent_file.gltf", scene);
    ENJIN_EXPECT_FALSE(ok);
}

// ===========================================================================
// AssimpLoader
// ===========================================================================

ENJIN_TEST(Assimp, MaterialDefaults) {
    AssimpMaterial m;
    ENJIN_EXPECT_FLOAT_EQ(m.baseColorFactor.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.metallicFactor, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.roughnessFactor, 0.5f);
    ENJIN_EXPECT_FALSE(m.doubleSided);
    ENJIN_EXPECT_FLOAT_EQ(m.opacity, 1.0f);
}

ENJIN_TEST(Assimp, NodeDefaults) {
    AssimpNode n;
    ENJIN_EXPECT_EQ(n.meshIndex, -1);
    ENJIN_EXPECT_FLOAT_EQ(n.scale.x, 1.0f);
}

ENJIN_TEST(Assimp, SupportedExtensions) {
    auto exts = AssimpLoader::GetSupportedExtensions();
    ENJIN_EXPECT_GT(exts.size(), (size_t)0);
}

ENJIN_TEST(Assimp, LoadNonexistentFails) {
    AssimpScene scene;
    bool ok = AssimpLoader::Load("nonexistent_model.fbx", scene);
    ENJIN_EXPECT_FALSE(ok);
}

// ===========================================================================
// SceneImporter
// ===========================================================================

ENJIN_TEST(Importer, OptionsDefaults) {
    ImportOptions opts;
    ENJIN_EXPECT_FLOAT_EQ(opts.scale, 1.0f);
    ENJIN_EXPECT_TRUE(opts.importMaterials);
    ENJIN_EXPECT_TRUE(opts.importLights);
    ENJIN_EXPECT_TRUE(opts.importAnimations);
    ENJIN_EXPECT_TRUE(opts.generateColliders);
    ENJIN_EXPECT_FALSE(opts.generateLODs);
    ENJIN_EXPECT_EQ((int)opts.sourceApp, (int)SourceApp::Auto);
    ENJIN_EXPECT_TRUE(opts.convertAxes);
    ENJIN_EXPECT_FALSE(opts.flipX);
    ENJIN_EXPECT_FALSE(opts.flipY);
    ENJIN_EXPECT_FALSE(opts.flipZ);
}

ENJIN_TEST(Importer, ResultDefaults) {
    ImportResult r;
    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_EQ(r.meshCount, 0u);
    ENJIN_EXPECT_EQ(r.materialCount, 0u);
    ENJIN_EXPECT_EQ(r.animationCount, 0u);
    ENJIN_EXPECT_EQ(r.totalVertexCount, 0u);
}

ENJIN_TEST(Importer, SourceAppPresets) {
    // Each DCC tool should have a preset with non-empty name
    auto blender = GetSourceAppPreset(SourceApp::Blender);
    ENJIN_EXPECT_NOT_NULL(blender.name);

    auto maya = GetSourceAppPreset(SourceApp::Maya);
    ENJIN_EXPECT_NOT_NULL(maya.name);

    auto unity = GetSourceAppPreset(SourceApp::Unity);
    ENJIN_EXPECT_NOT_NULL(unity.name);
}

ENJIN_TEST(Importer, SourceAppNames) {
    for (int i = 0; i <= (int)SourceApp::Custom; ++i) {
        const char* name = GetSourceAppName((SourceApp)i);
        ENJIN_EXPECT_NOT_NULL(name);
        ENJIN_EXPECT_GT(strlen(name), (size_t)0);
    }
}

// ===========================================================================
// SWF Types
// ===========================================================================

ENJIN_TEST(SWF, RectDefaults) {
    SWFRect r;
    ENJIN_EXPECT_EQ(r.xMin, 0);
    ENJIN_EXPECT_EQ(r.xMax, 0);
    ENJIN_EXPECT_EQ(r.yMin, 0);
    ENJIN_EXPECT_EQ(r.yMax, 0);
}

ENJIN_TEST(SWF, ColorDefaults) {
    SWFColor c;
    ENJIN_EXPECT_EQ(c.r, 0u);
    ENJIN_EXPECT_EQ(c.g, 0u);
    ENJIN_EXPECT_EQ(c.b, 0u);
    ENJIN_EXPECT_EQ(c.a, 255u);
}

ENJIN_TEST(SWF, MatrixDefaults) {
    SWFMatrix m;
    ENJIN_EXPECT_FLOAT_EQ(m.scaleX, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.scaleY, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.rotateSkew0, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.translateX, 0.0f);
}

ENJIN_TEST(SWF, DocumentDefaults) {
    SWFDocument doc;
    ENJIN_EXPECT_EQ(doc.version, 0u);
    ENJIN_EXPECT_FLOAT_EQ(doc.frameRate, 24.0f);
    ENJIN_EXPECT_FALSE(doc.compressed);
    ENJIN_EXPECT_EQ(doc.shapes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(doc.sprites.size(), (size_t)0);
}

ENJIN_TEST(SWF, ImportOptionsDefaults) {
    SWFImportOptions opts;
    ENJIN_EXPECT_TRUE(opts.rasterizeShapes);
    ENJIN_EXPECT_FLOAT_EQ(opts.rasterScale, 2.0f);
    ENJIN_EXPECT_TRUE(opts.importSounds);
    ENJIN_EXPECT_TRUE(opts.importTimelines);
}

ENJIN_TEST(SWF, ParseNonexistentFails) {
    SWFParseResult result = SWFLoader::Parse("nonexistent.swf");
    ENJIN_EXPECT_FALSE(result.success);
}

// ===========================================================================
// PLY Types
// ===========================================================================

ENJIN_TEST(PLY, VertexDefaults) {
    PLYVertex v;
    ENJIN_EXPECT_FALSE(v.hasNormal);
    ENJIN_EXPECT_FALSE(v.hasColor);
    ENJIN_EXPECT_FALSE(v.hasTexCoord);
}

ENJIN_TEST(PLY, MeshDefaults) {
    PLYMesh m;
    ENJIN_EXPECT_EQ(m.vertices.size(), (size_t)0);
    ENJIN_EXPECT_EQ(m.indices.size(), (size_t)0);
    ENJIN_EXPECT_FALSE(m.hasNormals);
    ENJIN_EXPECT_FALSE(m.hasColors);
    ENJIN_EXPECT_FALSE(m.hasTexCoords);
}

ENJIN_TEST(PLY, LoadNonexistentFails) {
    PLYMesh mesh;
    bool ok = PLYLoader::Load("nonexistent.ply", mesh);
    ENJIN_EXPECT_FALSE(ok);
}

// ===========================================================================
// VOX Types
// ===========================================================================

ENJIN_TEST(VOX, ModelDefaults) {
    VOXModel model;
    ENJIN_EXPECT_EQ(model.sizeX, 0u);
    ENJIN_EXPECT_EQ(model.sizeY, 0u);
    ENJIN_EXPECT_EQ(model.sizeZ, 0u);
    ENJIN_EXPECT_EQ(model.voxels.size(), (size_t)0);
    ENJIN_EXPECT_FALSE(model.hasCustomPalette);
}

ENJIN_TEST(VOX, LoadNonexistentFails) {
    VOXModel model;
    bool ok = VOXLoader::Load("nonexistent.vox", model);
    ENJIN_EXPECT_FALSE(ok);
}

// ===========================================================================
// AssetMetadata
// ===========================================================================

ENJIN_TEST(Metadata, Defaults) {
    AssetMetadata meta;
    ENJIN_EXPECT_TRUE(meta.originalPath.empty());
    ENJIN_EXPECT_EQ(meta.fileSize, (u64)0);
    ENJIN_EXPECT_EQ(meta.meshCount, 0u);
    ENJIN_EXPECT_EQ(meta.materialCount, 0u);
}

ENJIN_TEST(Metadata, GetMetadataPath) {
    std::string path = AssetMetadata::GetMetadataPath("models/hero.gltf");
    ENJIN_EXPECT_TRUE(path.find(".enjinasset") != std::string::npos);
}

// ===========================================================================
// Vertex Color Support
// ===========================================================================

ENJIN_TEST(Assimp, VertexColorField) {
    AssimpVertex v;
    ENJIN_EXPECT_FALSE(v.hasColor);
    ENJIN_EXPECT_FLOAT_EQ(v.color.x, 1.0f);  // Default white
    ENJIN_EXPECT_FLOAT_EQ(v.color.w, 1.0f);
    v.color = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
    v.hasColor = true;
    ENJIN_EXPECT_TRUE(v.hasColor);
    ENJIN_EXPECT_FLOAT_EQ(v.color.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.color.y, 0.0f);
}

ENJIN_TEST(GLTF, VertexColorField) {
    GLTFVertex v;
    ENJIN_EXPECT_FALSE(v.hasColor);
    ENJIN_EXPECT_FLOAT_EQ(v.color.x, 1.0f);  // Default white
    v.color = Math::Vector4(0.0f, 1.0f, 0.0f, 0.5f);
    v.hasColor = true;
    ENJIN_EXPECT_TRUE(v.hasColor);
    ENJIN_EXPECT_FLOAT_EQ(v.color.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.color.w, 0.5f);
}

// ===========================================================================
// Source App Presets — Axis Conversion
// ===========================================================================

ENJIN_TEST(Importer, BlenderPresetZUp) {
    auto preset = GetSourceAppPreset(SourceApp::Blender);
    ENJIN_EXPECT_TRUE(preset.zUpToYUp);
    ENJIN_EXPECT_FALSE(preset.leftToRight);
    ENJIN_EXPECT_FLOAT_EQ(preset.scale, 1.0f);
}

ENJIN_TEST(Importer, MayaPresetCentimeters) {
    auto preset = GetSourceAppPreset(SourceApp::Maya);
    ENJIN_EXPECT_FALSE(preset.zUpToYUp);
    ENJIN_EXPECT_FLOAT_EQ(preset.scale, 0.01f);
}

ENJIN_TEST(Importer, UnrealPresetLeftHand) {
    auto preset = GetSourceAppPreset(SourceApp::Unreal);
    ENJIN_EXPECT_TRUE(preset.leftToRight);
    ENJIN_EXPECT_FLOAT_EQ(preset.scale, 0.01f);
}

ENJIN_TEST(Importer, UnityPresetLeftHand) {
    auto preset = GetSourceAppPreset(SourceApp::Unity);
    ENJIN_EXPECT_TRUE(preset.leftToRight);
    ENJIN_EXPECT_FLOAT_EQ(preset.scale, 1.0f);
}

ENJIN_TEST(Importer, MaxPresetZUp) {
    auto preset = GetSourceAppPreset(SourceApp::Max3ds);
    ENJIN_EXPECT_TRUE(preset.zUpToYUp);
}

ENJIN_TEST(Importer, SketchUpInches) {
    auto preset = GetSourceAppPreset(SourceApp::SketchUp);
    ENJIN_EXPECT_TRUE(preset.zUpToYUp);
    ENJIN_EXPECT_TRUE(preset.scale > 0.02f && preset.scale < 0.03f); // 0.0254
}

ENJIN_TEST(Importer, AllPresetsHaveNames) {
    SourceApp apps[] = {
        SourceApp::Auto, SourceApp::Blender, SourceApp::Maya, SourceApp::Max3ds,
        SourceApp::Houdini, SourceApp::Cinema4D, SourceApp::ZBrush,
        SourceApp::SubstancePainter, SourceApp::Unreal, SourceApp::Unity,
        SourceApp::SketchUp, SourceApp::Custom
    };
    for (auto app : apps) {
        auto preset = GetSourceAppPreset(app);
        ENJIN_EXPECT_NOT_NULL(preset.name);
    }
}

// ===========================================================================
// Import Pipeline — Null World Safety
// ===========================================================================

ENJIN_TEST(Importer, NullWorldReturnsFailure) {
    ImportResult r = SceneImporter::Import("test.gltf", nullptr);
    ENJIN_EXPECT_FALSE(r.success);
}

ENJIN_TEST(Importer, NullWorldGLTFReturnsFailure) {
    ImportResult r = SceneImporter::ImportGLTF("test.gltf", nullptr);
    ENJIN_EXPECT_FALSE(r.success);
}

ENJIN_TEST(Importer, NullWorldAssimpReturnsFailure) {
    ImportResult r = SceneImporter::ImportAssimp("test.fbx", nullptr);
    ENJIN_EXPECT_FALSE(r.success);
}

// ===========================================================================
// Import Pipeline — Nonexistent File Handling
// ===========================================================================

ENJIN_TEST(Importer, NonexistentGLTFReturnsFailure) {
    ECS::World world;
    ImportResult r = SceneImporter::ImportGLTF("does_not_exist.gltf", &world);
    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_EQ(r.entities.size(), (size_t)0);
}

ENJIN_TEST(Importer, NonexistentFBXReturnsFailure) {
    ECS::World world;
    ImportResult r = SceneImporter::ImportAssimp("does_not_exist.fbx", &world);
    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_EQ(r.entities.size(), (size_t)0);
}

ENJIN_TEST(Importer, AutoDetectNonexistentReturnsFailure) {
    ECS::World world;
    ImportResult r = SceneImporter::Import("does_not_exist.obj", &world);
    ENJIN_EXPECT_FALSE(r.success);
}

// ===========================================================================
// Import Options — Flip Override Combinations
// ===========================================================================

ENJIN_TEST(Importer, FlipOptionsDefault) {
    ImportOptions opts;
    ENJIN_EXPECT_FALSE(opts.flipX);
    ENJIN_EXPECT_FALSE(opts.flipY);
    ENJIN_EXPECT_FALSE(opts.flipZ);
    ENJIN_EXPECT_TRUE(opts.convertAxes);
}

ENJIN_TEST(Importer, FlipOptionsIndependent) {
    ImportOptions opts;
    opts.flipX = true;
    opts.flipZ = true;
    ENJIN_EXPECT_TRUE(opts.flipX);
    ENJIN_EXPECT_FALSE(opts.flipY);
    ENJIN_EXPECT_TRUE(opts.flipZ);
}

// ===========================================================================
// Import Result — Warning Storage
// ===========================================================================

ENJIN_TEST(Importer, ResultWarningsEmpty) {
    ImportResult r;
    ENJIN_EXPECT_EQ(r.warnings.size(), (size_t)0);
    r.warnings.push_back("test warning");
    ENJIN_EXPECT_EQ(r.warnings.size(), (size_t)1);
}

ENJIN_TEST(Importer, ResultTextureTracking) {
    ImportResult r;
    r.texturePathsResolved.push_back("diffuse.png");
    r.texturePathsMissing.push_back("normal.png");
    ENJIN_EXPECT_EQ(r.texturePathsResolved.size(), (size_t)1);
    ENJIN_EXPECT_EQ(r.texturePathsMissing.size(), (size_t)1);
}

// ===========================================================================
// GLTFScene — Structure Defaults
// ===========================================================================

ENJIN_TEST(GLTF, SceneDefaults) {
    GLTFScene scene;
    ENJIN_EXPECT_EQ(scene.meshes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(scene.materials.size(), (size_t)0);
    ENJIN_EXPECT_EQ(scene.nodes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(scene.skins.size(), (size_t)0);
    ENJIN_EXPECT_EQ(scene.animations.size(), (size_t)0);
    ENJIN_EXPECT_TRUE(scene.generator.empty());
}

// ===========================================================================
// AssimpScene — Structure Defaults
// ===========================================================================

ENJIN_TEST(Assimp, SceneDefaults) {
    AssimpScene scene;
    ENJIN_EXPECT_EQ(scene.meshes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(scene.materials.size(), (size_t)0);
    ENJIN_EXPECT_EQ(scene.nodes.size(), (size_t)0);
    ENJIN_EXPECT_FALSE(scene.hasSkinning);
    ENJIN_EXPECT_FLOAT_EQ(scene.unitScaleFactor, 1.0f);
    ENJIN_EXPECT_TRUE(scene.creator.empty());
}

ENJIN_TEST(Assimp, BoneDefaults) {
    AssimpBone bone;
    ENJIN_EXPECT_TRUE(bone.name.empty());
}

ENJIN_TEST(Assimp, AnimChannelDefaults) {
    AssimpAnimChannel ch;
    ENJIN_EXPECT_TRUE(ch.nodeName.empty());
    ENJIN_EXPECT_EQ(ch.positionTimes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(ch.rotationTimes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(ch.scaleTimes.size(), (size_t)0);
}

ENJIN_TEST(Assimp, AnimDefaults) {
    AssimpAnimation anim;
    ENJIN_EXPECT_TRUE(anim.name.empty());
    ENJIN_EXPECT_FLOAT_EQ(anim.duration, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(anim.ticksPerSecond, 25.0f);
}

// ===========================================================================
// GLTFVertex — Bone Weight Defaults
// ===========================================================================

ENJIN_TEST(GLTF, VertexBoneDefaults) {
    GLTFVertex v;
    ENJIN_EXPECT_EQ(v.boneIndices[0], 0u);
    ENJIN_EXPECT_EQ(v.boneIndices[1], 0u);
    ENJIN_EXPECT_EQ(v.boneIndices[2], 0u);
    ENJIN_EXPECT_EQ(v.boneIndices[3], 0u);
}

ENJIN_TEST(Assimp, VertexBoneDefaults) {
    AssimpVertex v;
    ENJIN_EXPECT_FLOAT_EQ(v.boneWeights.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.boneWeights.y, 0.0f);
    ENJIN_EXPECT_EQ(v.boneIndices[0], 0u);
}

// ===========================================================================
// GLTF Animation Channel
// ===========================================================================

ENJIN_TEST(GLTF, AnimChannelDefaults) {
    GLTFAnimationChannel ch;
    ENJIN_EXPECT_EQ(ch.targetNode, -1);
    ENJIN_EXPECT_EQ((int)ch.path, (int)GLTFAnimationChannel::Path::Translation);
    ENJIN_EXPECT_EQ(ch.times.size(), (size_t)0);
    ENJIN_EXPECT_EQ(ch.values.size(), (size_t)0);
}

// ===========================================================================
// Multi-Material — SubMesh Primitives
// ===========================================================================

ENJIN_TEST(GLTF, PrimitiveDefaults) {
    GLTFPrimitive prim;
    ENJIN_EXPECT_EQ(prim.materialIndex, -1);
    ENJIN_EXPECT_EQ(prim.vertices.size(), (size_t)0);
    ENJIN_EXPECT_EQ(prim.indices.size(), (size_t)0);
}

ENJIN_TEST(Assimp, PrimitiveDefaults) {
    AssimpPrimitive prim;
    ENJIN_EXPECT_EQ(prim.materialIndex, -1);
    ENJIN_EXPECT_EQ(prim.vertices.size(), (size_t)0);
    ENJIN_EXPECT_EQ(prim.indices.size(), (size_t)0);
}

// ===========================================================================
// FBX end-to-end import probe (env-gated: needs a real animal FBX on disk).
// Regression net for the invisible-model class: asserts every mesh entity
// lands parented under the import root at a VISIBLE world size.
// ===========================================================================

ENJIN_TEST(FbxImportProbe, SkinnedFbxImportsAtVisibleWorldSize) {
    namespace fs = std::filesystem;
    const char* kProbePath =
        "C:/Users/jerma/Downloads/FBX-20260807T212605Z-1-001/FBX/ShibaInu.fbx";
    if (!fs::exists(kProbePath)) {
        printf("  [skip] probe FBX not present: %s\n", kProbePath);
        return;
    }

    // Arrange
    ECS::World world;
    ImportOptions options;

    // Act
    ImportResult result = SceneImporter::Import(kProbePath, &world, options);

    // Assert: import succeeded and produced a root + mesh entities
    ENJIN_ASSERT_TRUE(result.success);
    ENJIN_ASSERT_TRUE(result.rootEntity != ECS::INVALID_ENTITY);
    ENJIN_ASSERT_TRUE(result.entities.size() >= 2);

    auto* rootXf = world.GetComponent<ECS::TransformComponent>(result.rootEntity);
    ENJIN_ASSERT_NOT_NULL(rootXf);
    printf("  root scale = (%.5f, %.5f, %.5f)\n",
           rootXf->scale.x, rootXf->scale.y, rootXf->scale.z);

    u32 meshEntities = 0;
    for (ECS::Entity e : result.entities) {
        auto* mesh = world.GetComponent<ECS::MeshComponent>(e);
        if (!mesh || mesh->vertices.empty()) continue;
        meshEntities++;

        // Local-space mesh bounds
        Math::Vector3 mn(FLT_MAX, FLT_MAX, FLT_MAX), mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (const auto& v : mesh->vertices) {
            mn.x = Math::Min(mn.x, v.position.x); mx.x = Math::Max(mx.x, v.position.x);
            mn.y = Math::Min(mn.y, v.position.y); mx.y = Math::Max(mx.y, v.position.y);
            mn.z = Math::Min(mn.z, v.position.z); mx.z = Math::Max(mx.z, v.position.z);
        }

        // World size = local extent through the hierarchy world matrix
        Math::Matrix4 wm = ECS::ComputeWorldMatrix(&world, e);
        f32 sx = Math::Vector3(wm.m[0], wm.m[1], wm.m[2]).Length();
        f32 sy = Math::Vector3(wm.m[4], wm.m[5], wm.m[6]).Length();
        f32 sz = Math::Vector3(wm.m[8], wm.m[9], wm.m[10]).Length();
        f32 worldH = (mx.y - mn.y) * sy;
        f32 worldMax = Math::Max((mx.x - mn.x) * sx,
                       Math::Max(worldH, (mx.z - mn.z) * sz));

        auto* nc = world.GetComponent<ECS::NameComponent>(e);
        printf("  mesh '%s': %zu verts, localH=%.1f worldScale=(%.5f,%.5f,%.5f) worldMax=%.2f\n",
               nc ? nc->name.c_str() : "?", mesh->vertices.size(),
               mx.y - mn.y, sx, sy, sz, worldMax);

        // The whole point: pieces must be VISIBLE — not microscopic, not giant
        ENJIN_EXPECT_TRUE(worldMax > 0.05f);
        ENJIN_EXPECT_TRUE(worldMax < 50.0f);

        // And parented (directly or transitively) under the import root
        ECS::Entity p = ECS::GetParent(&world, e);
        bool underRoot = false;
        for (int guard = 0; p != ECS::INVALID_ENTITY && guard < 32; ++guard) {
            if (p == result.rootEntity) { underRoot = true; break; }
            p = ECS::GetParent(&world, p);
        }
        ENJIN_EXPECT_TRUE(underRoot);
    }
    ENJIN_ASSERT_TRUE(meshEntities >= 1);

    // Regression: the movement drive fired CrossFade before the animator's
    // first Update, blending against an EMPTY pose — zero skinning matrices,
    // collapsed (invisible) mesh. Simulate the engine's first frames in the
    // same order (crossfade, then update) and assert usable matrices.
    for (ECS::Entity e : result.entities) {
        auto* ac = world.GetComponent<ECS::AnimatorComponent>(e);
        if (!ac || !ac->animator.GetSkeleton()) continue;
        if (ac->movement.enabled && !ac->movement.idleClip.empty()) {
            ac->animator.CrossFade(ac->movement.idleClip, ac->movement.fadeTime);
        }
        ac->Update(1.0f / 60.0f);
        ac->Update(1.0f / 60.0f);
        const auto& mats = ac->animator.GetSkinningMatrices();
        ENJIN_ASSERT_TRUE(!mats.empty());
        f32 maxAbs = 0.0f;
        for (const auto& m : mats) {
            for (int i = 0; i < 16; ++i) {
                maxAbs = Math::Max(maxAbs, std::abs(m.m[i]));
            }
        }
        printf("  animator skinning: %zu bones, maxAbs=%.3f\n", mats.size(), maxAbs);
        ENJIN_EXPECT_TRUE(maxAbs > 0.01f);      // collapsed pose = all near zero
        ENJIN_EXPECT_TRUE(maxAbs < 10000.0f);   // exploded pose = garbage
    }
}

ENJIN_TEST_MAIN()
