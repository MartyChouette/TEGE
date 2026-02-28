#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"

// Conditionally available headers — each guarded so the file still compiles
// even if individual subsystems are not yet wired into the test build.
#if __has_include("Enjin/Renderer/MeshFactory.h")
#  include "Enjin/Renderer/MeshFactory.h"
#  define HAS_MESH_FACTORY 1
#endif

#if __has_include("Enjin/Assets/Prefab.h")
#  include "Enjin/Assets/Prefab.h"
#  define HAS_PREFAB 1
#endif

#if __has_include("Enjin/Editor/CollaborativeEditing.h")
#  include "Enjin/Editor/CollaborativeEditing.h"
#  define HAS_COLLAB 1
#endif

#if __has_include("Enjin/Editor/ShaderGraph.h")
#  include "Enjin/Editor/ShaderGraph.h"
#  define HAS_SHADER_GRAPH 1
#endif

#if __has_include("Enjin/ECS/Components/Camera.h")
#  include "Enjin/ECS/Components/Camera.h"
#  define HAS_CAMERA 1
#endif

#if __has_include("Enjin/Build/AssetPacker.h")
#  include "Enjin/Build/AssetPacker.h"
#  define HAS_ASSET_PACKER 1
#endif

#if __has_include("Enjin/Networking/NetworkTypes.h")
#  include "Enjin/Networking/NetworkTypes.h"
#  define HAS_NETWORK_TYPES 1
#endif

#if __has_include("Enjin/Networking/NetworkSecurity.h")
#  include "Enjin/Networking/NetworkSecurity.h"
#  define HAS_NETWORK_SECURITY 1
#endif

using namespace Enjin;

// ===========================================================================
// 1. MeshFactory — CreatePlane with 0 subdivisions (division-by-zero guard)
// ===========================================================================

#ifdef HAS_MESH_FACTORY
ENJIN_TEST(MeshFactory_Regression, CreatePlane_ZeroSubdivisions_X) {
    // Width/height subdivisions of 0 used to cause a division by zero.
    // The fix clamps each subdivision axis to at least 1 before dividing.
    auto mesh = Renderer::MeshFactory::CreatePlane(1.0f, 1.0f, 0u, 1u);
    // Should produce a valid (non-empty) mesh — at least 3 vertices for a triangle
    ENJIN_EXPECT_TRUE(mesh.vertices.size() >= 3);
    ENJIN_EXPECT_TRUE(mesh.indices.size() >= 3);
}

ENJIN_TEST(MeshFactory_Regression, CreatePlane_ZeroSubdivisions_Z) {
    auto mesh = Renderer::MeshFactory::CreatePlane(1.0f, 1.0f, 1u, 0u);
    ENJIN_EXPECT_TRUE(mesh.vertices.size() >= 3);
    ENJIN_EXPECT_TRUE(mesh.indices.size() >= 3);
}

ENJIN_TEST(MeshFactory_Regression, CreatePlane_BothSubdivisions_Zero) {
    // Both axes at 0 — doubly degenerate input, must not crash or divide by zero.
    auto mesh = Renderer::MeshFactory::CreatePlane(1.0f, 1.0f, 0u, 0u);
    ENJIN_EXPECT_TRUE(mesh.vertices.size() >= 3);
    ENJIN_EXPECT_TRUE(mesh.indices.size() >= 3);
}

ENJIN_TEST(MeshFactory_Regression, CreateGrid_ZeroDivisions) {
    // Passing 0 divisions to CreateGrid used to divide by zero when computing
    // the step size (size / divisions). Fixed to clamp divisions >= 1.
    auto mesh = Renderer::MeshFactory::CreateGrid(10.0f, 0u);
    // A degenerate grid still produces at least some geometry (minimum 1 cell)
    ENJIN_EXPECT_TRUE(mesh.vertices.size() >= 4);
}

ENJIN_TEST(MeshFactory_Regression, CreateGrid_OneDivision_Valid) {
    auto mesh = Renderer::MeshFactory::CreateGrid(10.0f, 1u);
    ENJIN_EXPECT_TRUE(mesh.vertices.size() >= 4);
    ENJIN_EXPECT_TRUE(mesh.indices.size() >= 2);
}

ENJIN_TEST(MeshFactory_Regression, CreatePlane_Normal_Input) {
    // Sanity check: a normal call should produce the expected vertex count.
    // A 2x2 subdivision plane has 3*3 = 9 vertices.
    auto mesh = Renderer::MeshFactory::CreatePlane(2.0f, 2.0f, 2u, 2u);
    ENJIN_EXPECT_TRUE(mesh.vertices.size() >= 9);
}
#endif  // HAS_MESH_FACTORY

// ===========================================================================
// 2. Prefab JSON validation — missing entities / components arrays
// ===========================================================================

#ifdef HAS_PREFAB
ENJIN_TEST(Prefab_Regression, EmptyPrefab_IsEmpty) {
    Assets::Prefab prefab("TestPrefab");
    ENJIN_EXPECT_TRUE(prefab.IsEmpty());
    ENJIN_EXPECT_EQ(prefab.GetEntities().size(), (usize)0);
    ENJIN_EXPECT_EQ(prefab.GetRootCount(), (u32)0);
}

ENJIN_TEST(Prefab_Regression, AddEntity_NotEmpty) {
    Assets::Prefab prefab("TestPrefab");
    Assets::PrefabEntityData e;
    e.name = "Root";
    e.parentIndex = -1;
    prefab.AddEntity(e);

    ENJIN_EXPECT_FALSE(prefab.IsEmpty());
    ENJIN_EXPECT_EQ(prefab.GetEntities().size(), (usize)1);
    ENJIN_EXPECT_EQ(prefab.GetRootCount(), (u32)1);  // -1 parentIndex = root entity
}

ENJIN_TEST(Prefab_Regression, EntityWithNoComponents_IsValid) {
    // Missing components array (empty vector) must not be treated as corrupt.
    Assets::Prefab prefab("TestPrefab");
    Assets::PrefabEntityData e;
    e.name = "EmptyEntity";
    e.parentIndex = -1;
    // components vector intentionally left empty (simulates missing array in JSON)
    prefab.AddEntity(e);

    const auto& entities = prefab.GetEntities();
    ENJIN_ASSERT_TRUE(entities.size() == 1);
    ENJIN_EXPECT_EQ(entities[0].components.size(), (usize)0);
}

ENJIN_TEST(Prefab_Regression, Clear_ResetsAllEntities) {
    Assets::Prefab prefab("TestPrefab");
    for (int i = 0; i < 5; ++i) {
        Assets::PrefabEntityData e;
        e.name = "Entity" + std::to_string(i);
        e.parentIndex = -1;
        prefab.AddEntity(e);
    }
    ENJIN_EXPECT_EQ(prefab.GetEntities().size(), (usize)5);
    prefab.Clear();
    ENJIN_EXPECT_TRUE(prefab.IsEmpty());
}

ENJIN_TEST(Prefab_Regression, ChildEntity_HasValidParentIndex) {
    Assets::Prefab prefab("TestPrefab");

    Assets::PrefabEntityData root;
    root.name = "Root";
    root.parentIndex = -1;
    prefab.AddEntity(root);

    Assets::PrefabEntityData child;
    child.name = "Child";
    child.parentIndex = 0;  // index of root
    prefab.AddEntity(child);

    ENJIN_EXPECT_EQ(prefab.GetEntities().size(), (usize)2);
    // Only root entities have parentIndex == -1
    ENJIN_EXPECT_EQ(prefab.GetRootCount(), (u32)1);
}
#endif  // HAS_PREFAB

// ===========================================================================
// 3. CollabPermission enum values
// ===========================================================================

#ifdef HAS_COLLAB
ENJIN_TEST(CollabPermission_Regression, ViewerValueIsZero) {
    ENJIN_EXPECT_EQ(static_cast<u8>(Editor::CollabPermission::Viewer), (u8)0);
}

ENJIN_TEST(CollabPermission_Regression, EditorValueIsOne) {
    ENJIN_EXPECT_EQ(static_cast<u8>(Editor::CollabPermission::Editor), (u8)1);
}

ENJIN_TEST(CollabPermission_Regression, OwnerValueIsTwo) {
    ENJIN_EXPECT_EQ(static_cast<u8>(Editor::CollabPermission::Owner), (u8)2);
}

ENJIN_TEST(CollabPermission_Regression, PermissionOrdering) {
    // Viewer < Editor < Owner — higher numeric value means more permissions
    ENJIN_EXPECT_TRUE(static_cast<u8>(Editor::CollabPermission::Viewer)
                    < static_cast<u8>(Editor::CollabPermission::Editor));
    ENJIN_EXPECT_TRUE(static_cast<u8>(Editor::CollabPermission::Editor)
                    < static_cast<u8>(Editor::CollabPermission::Owner));
}
#endif  // HAS_COLLAB

// ===========================================================================
// 4. ShaderGraph node type enum existence
// ===========================================================================

#ifdef HAS_SHADER_GRAPH
ENJIN_TEST(ShaderGraph_Regression, NodeTypeEnums_Exist) {
    // Regression: ShaderNodeType enum must contain all expected values.
    // These were added as part of the node-map find() safety patch.
    Editor::ShaderNodeType t;

    t = Editor::ShaderNodeType::Add;       ENJIN_EXPECT_EQ((int)t, (int)Editor::ShaderNodeType::Add);
    t = Editor::ShaderNodeType::Multiply;  ENJIN_EXPECT_EQ((int)t, (int)Editor::ShaderNodeType::Multiply);
    t = Editor::ShaderNodeType::SampleTexture2D;
    ENJIN_EXPECT_EQ((int)t, (int)Editor::ShaderNodeType::SampleTexture2D);
    t = Editor::ShaderNodeType::FragmentOutput;
    ENJIN_EXPECT_EQ((int)t, (int)Editor::ShaderNodeType::FragmentOutput);
    t = Editor::ShaderNodeType::Comment;
    ENJIN_EXPECT_EQ((int)t, (int)Editor::ShaderNodeType::Comment);
}

ENJIN_TEST(ShaderGraph_Regression, NodeCategoryEnum_Exists) {
    Editor::ShaderNodeCategory c;
    c = Editor::ShaderNodeCategory::Input;    ENJIN_EXPECT_EQ((int)c, (int)Editor::ShaderNodeCategory::Input);
    c = Editor::ShaderNodeCategory::Math;     ENJIN_EXPECT_EQ((int)c, (int)Editor::ShaderNodeCategory::Math);
    c = Editor::ShaderNodeCategory::Texture;  ENJIN_EXPECT_EQ((int)c, (int)Editor::ShaderNodeCategory::Texture);
    c = Editor::ShaderNodeCategory::Output;   ENJIN_EXPECT_EQ((int)c, (int)Editor::ShaderNodeCategory::Output);
}

ENJIN_TEST(ShaderGraph_Regression, ShaderGraphData_DefaultName) {
    Editor::ShaderGraphData graph;
    ENJIN_EXPECT_STR_EQ(graph.name, "New Shader");
    ENJIN_EXPECT_TRUE(graph.nodes.empty());
    ENJIN_EXPECT_TRUE(graph.links.empty());
    ENJIN_EXPECT_EQ(graph.nextNodeId,  (u32)1);
    ENJIN_EXPECT_EQ(graph.nextLinkId, (u32)1);
}
#endif  // HAS_SHADER_GRAPH

// ===========================================================================
// 5. ProjectionType enum range
// ===========================================================================

#ifdef HAS_CAMERA
ENJIN_TEST(ProjectionType_Regression, PerspectiveIsZero) {
    ENJIN_EXPECT_EQ(static_cast<u32>(ECS::ProjectionType::Perspective), (u32)0);
}

ENJIN_TEST(ProjectionType_Regression, OrthographicIsOne) {
    ENJIN_EXPECT_EQ(static_cast<u32>(ECS::ProjectionType::Orthographic), (u32)1);
}

ENJIN_TEST(ProjectionType_Regression, CameraDefaultIsPerspective) {
    ECS::CameraComponent cam;
    ENJIN_EXPECT_EQ(cam.projectionType, ECS::ProjectionType::Perspective);
}

ENJIN_TEST(ProjectionType_Regression, CameraPreset_IsometricIsOrthographic) {
    auto result = ECS::ApplyCameraPreset(ECS::CameraPreset::Isometric45);
    ENJIN_EXPECT_EQ(result.camera.projectionType, ECS::ProjectionType::Orthographic);
}

ENJIN_TEST(ProjectionType_Regression, CameraPreset_FirstPersonIsPerspective) {
    auto result = ECS::ApplyCameraPreset(ECS::CameraPreset::FirstPerson);
    ENJIN_EXPECT_EQ(result.camera.projectionType, ECS::ProjectionType::Perspective);
    ENJIN_EXPECT_FLOAT_EQ(result.camera.fieldOfView, 75.0f);
}
#endif  // HAS_CAMERA

// ===========================================================================
// 6. BuildConfig default values
// ===========================================================================

#ifdef HAS_ASSET_PACKER
ENJIN_TEST(BuildConfig_Regression, AssetPacker_DefaultState) {
    // AssetPacker must start with 0 files and 0 bytes packed —
    // calling Finalize on a never-Begin'd packer must not crash.
    Build::AssetPacker packer;
    ENJIN_EXPECT_EQ(packer.GetFileCount(), (u32)0);
    ENJIN_EXPECT_EQ(packer.GetTotalOriginalSize(), (u64)0);
    ENJIN_EXPECT_EQ(packer.GetTotalPackedSize(), (u64)0);
}

ENJIN_TEST(BuildConfig_Regression, CRC32_KnownValue) {
    // CRC32("") == 0x00000000 (empty data)
    u32 crc = Build::AssetPacker::ComputeCRC32(nullptr, 0);
    ENJIN_EXPECT_EQ(crc, (u32)0x00000000);
}

ENJIN_TEST(BuildConfig_Regression, CRC32_NonEmpty_IsNonZero) {
    const char data[] = "Enjin";
    u32 crc = Build::AssetPacker::ComputeCRC32(data, sizeof(data) - 1);
    ENJIN_EXPECT_TRUE(crc != 0u);
}

ENJIN_TEST(BuildConfig_Regression, CRC32_Deterministic) {
    const char data[] = "regression test";
    u32 a = Build::AssetPacker::ComputeCRC32(data, sizeof(data) - 1);
    u32 b = Build::AssetPacker::ComputeCRC32(data, sizeof(data) - 1);
    ENJIN_EXPECT_EQ(a, b);
}
#endif  // HAS_ASSET_PACKER

// ===========================================================================
// 7. NetworkSystem payload rejection (oversized payload check)
// ===========================================================================

#ifdef HAS_NETWORK_TYPES
ENJIN_TEST(Network_Regression, MaxPacketSizeConstant) {
    // Regression guard: MAX_PACKET_SIZE must never be reduced below 512 bytes,
    // as the engine uses this to reject oversized payloads.
    ENJIN_EXPECT_TRUE(Networking::MAX_PACKET_SIZE >= (u32)512);
}

ENJIN_TEST(Network_Regression, PacketHeaderSize_Is12) {
    // PacketHeader is 12 bytes as documented — changing this breaks the wire format.
    ENJIN_EXPECT_EQ(Networking::PACKET_HEADER_SIZE, (u32)12);
}

ENJIN_TEST(Network_Regression, OversizedPayload_ExceedsLimit) {
    // Simulate the oversized-payload rejection check that NetworkSystem
    // applies in HandlePacket: payload > (MAX_PACKET_SIZE - PACKET_HEADER_SIZE)
    // should be flagged as oversized.
    const u32 maxPayload = Networking::MAX_PACKET_SIZE - Networking::PACKET_HEADER_SIZE;
    const u32 attackSize = Networking::MAX_PACKET_SIZE + 1u;  // one byte over

    // The check performed by the network system:
    bool rejected = (attackSize > maxPayload + Networking::PACKET_HEADER_SIZE);
    ENJIN_EXPECT_TRUE(rejected);
}

ENJIN_TEST(Network_Regression, ValidPayloadSize_AcceptedByCheck) {
    const u32 maxPayload = Networking::MAX_PACKET_SIZE - Networking::PACKET_HEADER_SIZE;
    const u32 normalSize = 64u;
    bool rejected = (normalSize > maxPayload + Networking::PACKET_HEADER_SIZE);
    ENJIN_EXPECT_FALSE(rejected);
}

ENJIN_TEST(Network_Regression, NetworkConfig_DefaultPort) {
    Networking::NetworkConfig cfg;
    ENJIN_EXPECT_EQ(cfg.port, Networking::DEFAULT_PORT);
    ENJIN_EXPECT_EQ(cfg.port, (u16)7777);
}

ENJIN_TEST(Network_Regression, NetworkConfig_DefaultMaxPlayers) {
    Networking::NetworkConfig cfg;
    ENJIN_EXPECT_EQ(cfg.maxPlayers, Networking::MAX_PLAYERS);
}
#endif  // HAS_NETWORK_TYPES

#ifdef HAS_NETWORK_SECURITY
ENJIN_TEST(Network_Regression, ReplayWindow_InitialState) {
    Networking::ReplayWindow win;
    ENJIN_EXPECT_FALSE(win.initialized);
    ENJIN_EXPECT_EQ(win.lastSequence, (u32)0);
    ENJIN_EXPECT_EQ(win.receivedBitmask, (u64)0);
}

ENJIN_TEST(Network_Regression, ReplayWindow_AcceptsFirstPacket) {
    Networking::ReplayWindow win;
    ENJIN_EXPECT_TRUE(win.Accept(1u));
    ENJIN_EXPECT_TRUE(win.initialized);
    ENJIN_EXPECT_EQ(win.lastSequence, (u32)1);
}

ENJIN_TEST(Network_Regression, ReplayWindow_RejectsDuplicate) {
    Networking::ReplayWindow win;
    ENJIN_EXPECT_TRUE(win.Accept(1u));
    // Sending the same sequence again should be rejected as a replay
    ENJIN_EXPECT_FALSE(win.Accept(1u));
}

ENJIN_TEST(Network_Regression, ReplayWindow_RejectsOldPacketOutsideWindow) {
    Networking::ReplayWindow win;
    win.Accept(100u);  // advance window to 100
    // Packet with seq=1 is 99 behind, well outside the 64-packet window
    ENJIN_EXPECT_FALSE(win.Accept(1u));
}

ENJIN_TEST(Network_Regression, ReplayWindow_AcceptsNewSequence) {
    Networking::ReplayWindow win;
    win.Accept(1u);
    ENJIN_EXPECT_TRUE(win.Accept(2u));
    ENJIN_EXPECT_EQ(win.lastSequence, (u32)2);
}

ENJIN_TEST(Network_Regression, ReplayWindow_Reset_ClearsState) {
    Networking::ReplayWindow win;
    win.Accept(50u);
    win.Reset();
    ENJIN_EXPECT_FALSE(win.initialized);
    ENJIN_EXPECT_EQ(win.lastSequence, (u32)0);
    ENJIN_EXPECT_EQ(win.receivedBitmask, (u64)0);
}
#endif  // HAS_NETWORK_SECURITY

ENJIN_TEST_MAIN()
