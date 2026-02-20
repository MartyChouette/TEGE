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

ENJIN_TEST_MAIN()
