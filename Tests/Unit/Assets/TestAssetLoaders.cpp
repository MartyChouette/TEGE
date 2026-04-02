#include "EnjinTest.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Assets/SWFLoader.h"
#include "Enjin/Assets/SWFConverter.h"
#include "Enjin/Assets/AssetMetadata.h"
#include "Enjin/Assets/PLYLoader.h"
#include "Enjin/Assets/VOXLoader.h"

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

ENJIN_TEST_MAIN()
