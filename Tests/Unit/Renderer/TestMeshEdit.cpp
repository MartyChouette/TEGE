// Editing a mesh in place: reducing it, undoing that, and the one thing that
// makes either visible -- telling the renderer the data changed.
//
// The renderer builds an entity's GPU buffers once and reuses them, so a
// MeshComponent rewritten in place keeps drawing the old geometry unless
// something asks for a re-upload. That signal is the whole reason a mesh tool
// works or silently does nothing, so it is what these tests pin.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/MeshEdit.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/ProceduralMesh.h"
#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/Renderer/MeshSimplifier.h"

using namespace Enjin;
using namespace Enjin::Math;

namespace {

// A subdivided grid: enough triangles that a decimator has interior edges to
// collapse. A cube is nearly all boundary and barely reduces.
ECS::MeshComponent Grid(u32 n) {
    ECS::MeshComponent mesh;
    for (u32 z = 0; z <= n; ++z) {
        for (u32 x = 0; x <= n; ++x) {
            ECS::Vertex v;
            v.position = Vector3(static_cast<f32>(x), 0.0f, static_cast<f32>(z));
            v.normal = Vector3(0.0f, 1.0f, 0.0f);
            v.uv = Vector2(static_cast<f32>(x) / static_cast<f32>(n),
                           static_cast<f32>(z) / static_cast<f32>(n));
            mesh.vertices.push_back(v);
        }
    }
    const u32 stride = n + 1;
    for (u32 z = 0; z < n; ++z) {
        for (u32 x = 0; x < n; ++x) {
            u32 a = z * stride + x;
            mesh.indices.push_back(a);
            mesh.indices.push_back(a + stride);
            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(a + stride);
            mesh.indices.push_back(a + stride + 1);
        }
    }
    return mesh;
}

ECS::Entity MakeMeshEntity(ECS::World& w, u32 n = 16) {
    ECS::Entity e = w.CreateEntity();
    w.AddComponent<ECS::MeshComponent>(e, Grid(n));
    return e;
}

} // namespace

// ---------------------------------------------------------------------------
// The re-upload signal
// ---------------------------------------------------------------------------

ENJIN_TEST(MeshEdit, MarkingAMeshChangedAddsTheUploadMarker) {
    ECS::World w;
    ECS::Entity e = MakeMeshEntity(w);

    ENJIN_EXPECT_FALSE(w.HasComponent<ECS::ProceduralMeshComponent>(e));

    ECS::MarkMeshChanged(w, e, ECS::ProceduralMeshComponent::Source::Edited, true);

    ENJIN_ASSERT_TRUE(w.HasComponent<ECS::ProceduralMeshComponent>(e));
    auto* pm = w.GetComponent<ECS::ProceduralMeshComponent>(e);
    ENJIN_EXPECT_EQ((int)pm->source, (int)ECS::ProceduralMeshComponent::Source::Edited);
    ENJIN_EXPECT_TRUE(pm->topologyDirty);
}

// A mesh that has never been uploaded needs its buffers BUILT, whatever kind
// of change brought us here -- so the first call always asks for topology,
// even when the caller only moved vertices.
ENJIN_TEST(MeshEdit, TheFirstMarkAlwaysAsksForBuffers) {
    ECS::World w;
    ECS::Entity e = MakeMeshEntity(w);

    ECS::MarkMeshChanged(w, e, ECS::ProceduralMeshComponent::Source::Edited, false);

    auto* pm = w.GetComponent<ECS::ProceduralMeshComponent>(e);
    ENJIN_ASSERT_TRUE(pm != nullptr);
    ENJIN_EXPECT_TRUE(pm->topologyDirty);
}

// Resizing the buffers and re-filling them are different requests. Asking for
// the wrong one leaves either a stale buffer on screen or a reallocation on
// every edit.
ENJIN_TEST(MeshEdit, VertexMovesReuseTheBufferAndCountChangesRebuildIt) {
    ECS::World w;
    ECS::Entity e = MakeMeshEntity(w);

    ECS::MarkMeshChanged(w, e, ECS::ProceduralMeshComponent::Source::Edited, true);
    auto* pm = w.GetComponent<ECS::ProceduralMeshComponent>(e);
    pm->topologyDirty = false;
    pm->meshDirty = false;

    ECS::MarkMeshChanged(w, e, ECS::ProceduralMeshComponent::Source::Edited, false);
    ENJIN_EXPECT_FALSE(pm->topologyDirty);
    ENJIN_EXPECT_TRUE(pm->meshDirty);

    pm->meshDirty = false;
    ECS::MarkMeshChanged(w, e, ECS::ProceduralMeshComponent::Source::Edited, true);
    ENJIN_EXPECT_TRUE(pm->topologyDirty);
}

ENJIN_TEST(MeshEdit, MarkingIsHarmlessOnAnEntityWithoutAMesh) {
    ECS::World w;
    ECS::Entity e = w.CreateEntity();
    ECS::MarkMeshChanged(w, e, ECS::ProceduralMeshComponent::Source::Edited, true);
    // It marks whatever it is told to; the caller owns whether that makes
    // sense. What matters is that it does not crash or corrupt the world.
    ENJIN_EXPECT_TRUE(w.IsValid(e));
}

// ---------------------------------------------------------------------------
// Reducing a model
// ---------------------------------------------------------------------------

// The whole point of the tool: the engine could already do this and no one
// could ask it to.
ENJIN_TEST(MeshEdit, SimplifyProducesFewerTrianglesAndAValidMesh) {
    ECS::MeshComponent source = Grid(16);
    const usize before = source.indices.size() / 3;

    ECS::MeshComponent reduced = Renderer::MeshSimplifier::Simplify(source, 0.5f);

    ENJIN_ASSERT_TRUE(reduced.IsValid());
    ENJIN_EXPECT_TRUE(reduced.indices.size() / 3 < before);
    ENJIN_EXPECT_EQ(reduced.indices.size() % 3, (usize)0);
    ENJIN_EXPECT_TRUE(reduced.vertices.size() > 0);

    // Every index has to address a vertex that exists, or the draw call reads
    // off the end of the buffer.
    for (u32 index : reduced.indices) {
        ENJIN_ASSERT_TRUE(index < reduced.vertices.size());
    }
}

ENJIN_TEST(MeshEdit, SimplifyLeavesTheSourceMeshAlone) {
    ECS::MeshComponent source = Grid(16);
    const usize vertices = source.vertices.size();
    const usize indices  = source.indices.size();

    Renderer::MeshSimplifier::Simplify(source, 0.25f);

    ENJIN_EXPECT_EQ(source.vertices.size(), vertices);
    ENJIN_EXPECT_EQ(source.indices.size(), indices);
}

// ---------------------------------------------------------------------------
// Undo
// ---------------------------------------------------------------------------

ENJIN_TEST(MeshEdit, UndoRestoresTheGeometryExactly) {
    ECS::World w;
    ECS::Entity e = MakeMeshEntity(w);
    auto* mesh = w.GetComponent<ECS::MeshComponent>(e);

    const std::vector<ECS::Vertex> oldVertices = mesh->vertices;
    const std::vector<u32>         oldIndices  = mesh->indices;
    const auto                     oldSubs     = mesh->subMeshes;

    ECS::MeshComponent reduced = Renderer::MeshSimplifier::Simplify(*mesh, 0.5f);
    ENJIN_ASSERT_TRUE(reduced.IsValid());

    Editor::MeshEditCommand cmd(&w, e, "Simplify Mesh",
                                oldVertices, oldIndices, oldSubs, mesh->source,
                                reduced.vertices, reduced.indices, reduced.subMeshes,
                                ECS::MeshComponent::SourceRef{});

    cmd.Execute();
    ENJIN_ASSERT_TRUE(mesh->indices.size() < oldIndices.size());

    cmd.Undo();
    ENJIN_ASSERT_EQ(mesh->vertices.size(), oldVertices.size());
    ENJIN_ASSERT_EQ(mesh->indices.size(), oldIndices.size());
    for (usize i = 0; i < oldIndices.size(); ++i) {
        ENJIN_ASSERT_EQ(mesh->indices[i], oldIndices[i]);
    }
    for (usize i = 0; i < oldVertices.size(); ++i) {
        ENJIN_EXPECT_VEC3_EQ(mesh->vertices[i].position,
                             oldVertices[i].position.x,
                             oldVertices[i].position.y,
                             oldVertices[i].position.z);
    }
}

// The failure this guards is the quiet one: undo puts the data back and the
// screen keeps showing the reduced model, because nothing asked for the
// buffers again.
ENJIN_TEST(MeshEdit, UndoAlsoAsksForTheBuffersBack) {
    ECS::World w;
    ECS::Entity e = MakeMeshEntity(w);
    auto* mesh = w.GetComponent<ECS::MeshComponent>(e);

    ECS::MeshComponent reduced = Renderer::MeshSimplifier::Simplify(*mesh, 0.5f);
    Editor::MeshEditCommand cmd(&w, e, "Simplify Mesh",
                                mesh->vertices, mesh->indices, mesh->subMeshes, mesh->source,
                                reduced.vertices, reduced.indices, reduced.subMeshes,
                                ECS::MeshComponent::SourceRef{});

    cmd.Execute();
    auto* pm = w.GetComponent<ECS::ProceduralMeshComponent>(e);
    ENJIN_ASSERT_TRUE(pm != nullptr);
    pm->topologyDirty = false;
    pm->meshDirty = false;

    cmd.Undo();
    ENJIN_EXPECT_TRUE(pm->topologyDirty || pm->meshDirty);
}

ENJIN_TEST(MeshEdit, RedoAppliesTheReductionAgain) {
    ECS::World w;
    ECS::Entity e = MakeMeshEntity(w);
    auto* mesh = w.GetComponent<ECS::MeshComponent>(e);
    const usize original = mesh->indices.size();

    ECS::MeshComponent reduced = Renderer::MeshSimplifier::Simplify(*mesh, 0.5f);
    const usize reducedCount = reduced.indices.size();

    Editor::MeshEditCommand cmd(&w, e, "Simplify Mesh",
                                mesh->vertices, mesh->indices, mesh->subMeshes, mesh->source,
                                reduced.vertices, reduced.indices, reduced.subMeshes,
                                ECS::MeshComponent::SourceRef{});

    cmd.Execute();
    ENJIN_EXPECT_EQ(mesh->indices.size(), reducedCount);
    cmd.Undo();
    ENJIN_EXPECT_EQ(mesh->indices.size(), original);
    cmd.Execute();
    ENJIN_EXPECT_EQ(mesh->indices.size(), reducedCount);
}

// The quiet revert: a mesh that still names the file it was imported from is
// SERIALIZED AS A REFERENCE, and the loader re-imports the original at full
// resolution. Comparing only the source file's hash, nothing notices the live
// geometry has changed -- so simplifying an imported model, saving and
// reopening put every triangle back with no warning.
ENJIN_TEST(MeshEdit, SimplifyingDetachesTheImportReferenceAndUndoRestoresIt) {
    ECS::World w;
    ECS::Entity e = MakeMeshEntity(w);
    auto* mesh = w.GetComponent<ECS::MeshComponent>(e);

    mesh->source.sourcePath = "models/statue.fbx";
    mesh->source.meshIndex = 2;
    ENJIN_ASSERT_TRUE(mesh->source.Valid());
    const ECS::MeshComponent::SourceRef before = mesh->source;

    ECS::MeshComponent reduced = Renderer::MeshSimplifier::Simplify(*mesh, 0.5f);
    Editor::MeshEditCommand cmd(&w, e, "Simplify Mesh",
                                mesh->vertices, mesh->indices, mesh->subMeshes, mesh->source,
                                reduced.vertices, reduced.indices, reduced.subMeshes,
                                ECS::MeshComponent::SourceRef{});

    cmd.Execute();
    // Detached, so the next save writes the reduced geometry inline.
    ENJIN_EXPECT_FALSE(mesh->source.Valid());

    cmd.Undo();
    // And undo puts the link back, so the mesh is a reference again.
    ENJIN_EXPECT_TRUE(mesh->source.Valid());
    ENJIN_EXPECT_TRUE(mesh->source.sourcePath == before.sourcePath);
    ENJIN_EXPECT_EQ(mesh->source.meshIndex, before.meshIndex);
}

ENJIN_TEST(MeshEdit, UndoOnADestroyedEntityDoesNotCrash) {
    ECS::World w;
    ECS::Entity e = MakeMeshEntity(w);
    auto* mesh = w.GetComponent<ECS::MeshComponent>(e);

    ECS::MeshComponent reduced = Renderer::MeshSimplifier::Simplify(*mesh, 0.5f);
    Editor::MeshEditCommand cmd(&w, e, "Simplify Mesh",
                                mesh->vertices, mesh->indices, mesh->subMeshes, mesh->source,
                                reduced.vertices, reduced.indices, reduced.subMeshes,
                                ECS::MeshComponent::SourceRef{});
    cmd.Execute();

    w.DestroyEntity(e);
    w.Update(0.016f);      // flushes the deferred destroy

    cmd.Undo();            // the entity it names is gone
    ENJIN_EXPECT_FALSE(w.IsValid(e));
}

ENJIN_TEST_MAIN()
