// Four Procedural Graph generators returned a uniform 0.5 grid and reported
// success.
//
//     case ProcNodeType::Gen_LSystem:
//     case ProcNodeType::Gen_WFC:
//     case ProcNodeType::Gen_Grammar:
//     case ProcNodeType::Gen_PrefabAssembler: {
//         // These produce non-grid output; generate a placeholder
//         result.grid.resize(height, std::vector<f32>(width, 0.5f));
//         break;
//     }
//
// `result.success` was left true, so the preview showed flat grey and the node
// said it had worked. Two of the four had a real implementation sitting in
// Procedural::LSystemGenerator and Procedural::WaveFunctionCollapse the whole time;
// the node simply never called it. The other two genuinely have no grid form, and
// now say so instead of returning grey.
//
// Separately, Param_Seed and Param_Size emitted 0.5 regardless of the numbers typed
// into them -- while the sibling Param_Int node correctly emits its own value --
// and nothing read them at all: Execute took the graph's global seed and preview
// size, so placing a Seed node and typing into it changed nothing.
//
// These tests assert on the produced grid, because "it is not a constant" is
// exactly the property that was missing and the one a preview cannot show you at a
// glance: flat grey and a legitimately uniform result look identical.
#include "EnjinTest.h"
#include "Enjin/Editor/ProceduralGraph.h"
#include <algorithm>
#include <string>

using namespace Enjin;
using namespace Enjin::Editor;

namespace {

// How many DISTINCT values the grid holds. A placeholder has exactly one.
usize DistinctValues(const ProcGraphResult& r) {
    std::vector<f32> all;
    for (const auto& row : r.grid) all.insert(all.end(), row.begin(), row.end());
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end()), all.end());
    return all.size();
}

bool AllEqualTo(const ProcGraphResult& r, f32 v) {
    for (const auto& row : r.grid) {
        for (f32 f : row) {
            if (f > v + 0.0001f || f < v - 0.0001f) return false;
        }
    }
    return !r.grid.empty();
}

ProcGraphResult RunSingleNode(ProcNodeType type,
                              const std::string& stringParam = "",
                              int i0 = 0, int i1 = 0, f32 f0 = 0.0f) {
    ProcGraphData g;
    g.previewWidth = 48;
    g.previewHeight = 48;
    g.globalSeed = 1234;

    ProcGraphNode n;
    n.id = 1;
    n.type = type;
    n.stringParam = stringParam;
    n.intParams[0] = i0;
    n.intParams[1] = i1;
    n.floatParams[0] = f0;
    g.nodes.push_back(n);

    ProceduralGraphEditor editor;
    editor.SetGraph(&g);
    return editor.Execute();
}

} // namespace

// ---------------------------------------------------------------------------
// The two that had a real generator all along
// ---------------------------------------------------------------------------

ENJIN_TEST(ProceduralGraphGenerators, LSystemDrawsSomethingInsteadOfGrey) {
    // Arrange / act: the Koch-ish default rule, four iterations.
    const ProcGraphResult r = RunSingleNode(ProcNodeType::Gen_LSystem, "F", 4, 0, 25.0f);

    // Assert
    ENJIN_ASSERT_TRUE(r.success);
    ENJIN_EXPECT_FALSE(AllEqualTo(r, 0.5f));   // the exact placeholder value
    // A drawing has both drawn and undrawn cells.
    ENJIN_EXPECT_TRUE(DistinctValues(r) >= 2);

    usize drawn = 0, total = 0;
    for (const auto& row : r.grid) {
        for (f32 f : row) { if (f > 0.5f) ++drawn; ++total; }
    }
    ENJIN_EXPECT_TRUE(drawn > 0);
    // And it is a LINE drawing, not a fill: a curve cannot cover the whole grid.
    ENJIN_EXPECT_TRUE(drawn < total / 2);
}

ENJIN_TEST(ProceduralGraphGenerators, MoreLSystemIterationsDrawMore) {
    // The iteration count has to actually reach the generator. A node that ignored
    // it would look plausible at any single setting.
    const ProcGraphResult few = RunSingleNode(ProcNodeType::Gen_LSystem, "F", 1, 0, 25.0f);
    const ProcGraphResult many = RunSingleNode(ProcNodeType::Gen_LSystem, "F", 4, 0, 25.0f);

    auto drawnCount = [](const ProcGraphResult& r) {
        usize n = 0;
        for (const auto& row : r.grid) for (f32 f : row) if (f > 0.5f) ++n;
        return n;
    };
    ENJIN_ASSERT_TRUE(few.success && many.success);
    ENJIN_EXPECT_TRUE(drawnCount(many) > drawnCount(few));
}

ENJIN_TEST(ProceduralGraphGenerators, WFCFillsTheGridWithTiles) {
    const ProcGraphResult r = RunSingleNode(ProcNodeType::Gen_WFC, "", 4, 50, 0.0f);

    ENJIN_ASSERT_TRUE(r.success);
    ENJIN_EXPECT_FALSE(AllEqualTo(r, 0.5f));
    // Four tile types, unconstrained: more than one should appear across 48x48.
    ENJIN_EXPECT_TRUE(DistinctValues(r) >= 2);
    // Values are normalised into 0..1 rather than being raw tile ids.
    for (const auto& row : r.grid) {
        for (f32 f : row) {
            ENJIN_ASSERT_TRUE(f >= -0.001f && f <= 1.001f);
        }
    }
}

// ---------------------------------------------------------------------------
// The two that genuinely have no grid form
// ---------------------------------------------------------------------------

ENJIN_TEST(ProceduralGraphGenerators, GrammarAndAssemblerSayTheyHaveNoGrid) {
    // A shape grammar produces shapes and a prefab assembler produces placements.
    // Neither is a height field. Returning grey with success=true told an author
    // their graph was fine when it could not have been.
    for (ProcNodeType type : { ProcNodeType::Gen_Grammar,
                               ProcNodeType::Gen_PrefabAssembler }) {
        const ProcGraphResult r = RunSingleNode(type);
        ENJIN_EXPECT_FALSE(r.success);
        ENJIN_EXPECT_TRUE(!r.error.empty());
        // The message names the node, so the author knows which one to unwire.
        ENJIN_EXPECT_TRUE(r.error.find("grid") != std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// Param nodes
// ---------------------------------------------------------------------------

ENJIN_TEST(ProceduralGraphGenerators, SeedNodeEmitsItsSeedNotAConstant) {
    const ProcGraphResult r = RunSingleNode(ProcNodeType::Param_Seed, "", 7777);
    ENJIN_ASSERT_TRUE(r.success);
    ENJIN_EXPECT_FALSE(AllEqualTo(r, 0.5f));
    ENJIN_EXPECT_TRUE(AllEqualTo(r, 7777.0f));
}

ENJIN_TEST(ProceduralGraphGenerators, SizeNodeEmitsItsWidthNotAConstant) {
    const ProcGraphResult r = RunSingleNode(ProcNodeType::Param_Size, "", 32, 24);
    ENJIN_ASSERT_TRUE(r.success);
    ENJIN_EXPECT_FALSE(AllEqualTo(r, 0.5f));
    ENJIN_EXPECT_TRUE(AllEqualTo(r, 32.0f));
}

ENJIN_TEST(ProceduralGraphGenerators, ASizeNodeActuallyResizesTheOutput) {
    // The structural half: a Size node exists to override the graph's preview size,
    // and Execute ignored it entirely. Typing 32x24 into one changed nothing.
    ProcGraphData g;
    g.previewWidth = 48;
    g.previewHeight = 48;
    g.globalSeed = 99;

    ProcGraphNode size;
    size.id = 1;
    size.type = ProcNodeType::Param_Size;
    size.intParams[0] = 32;
    size.intParams[1] = 24;
    g.nodes.push_back(size);

    ProceduralGraphEditor editor;
    editor.SetGraph(&g);
    const ProcGraphResult r = editor.Execute();

    ENJIN_ASSERT_TRUE(r.success);
    ENJIN_ASSERT_EQ(r.grid.size(), static_cast<usize>(24));
    ENJIN_EXPECT_EQ(r.grid[0].size(), static_cast<usize>(32));
}

ENJIN_TEST(ProceduralGraphGenerators, ASeedNodeChangesWhatIsGenerated) {
    // Two graphs identical but for the Seed node. If the seed reached the
    // generator the results differ; if it did not, they are identical and the node
    // is decoration.
    auto runWithSeed = [](int seedValue) {
        ProcGraphData g;
        g.previewWidth = 32;
        g.previewHeight = 32;
        g.globalSeed = 1;

        ProcGraphNode seed;
        seed.id = 1;
        seed.type = ProcNodeType::Param_Seed;
        seed.intParams[0] = seedValue;
        g.nodes.push_back(seed);

        ProcGraphNode wfc;
        wfc.id = 2;
        wfc.type = ProcNodeType::Gen_WFC;
        wfc.intParams[0] = 6;
        wfc.intParams[1] = 50;
        g.nodes.push_back(wfc);

        ProceduralGraphEditor editor;
        editor.SetGraph(&g);
        return editor.Execute();
    };

    const ProcGraphResult a = runWithSeed(11);
    const ProcGraphResult b = runWithSeed(9999);
    ENJIN_ASSERT_TRUE(a.success && b.success);

    bool anyDifferent = false;
    for (usize y = 0; y < a.grid.size() && y < b.grid.size() && !anyDifferent; ++y) {
        for (usize x = 0; x < a.grid[y].size() && x < b.grid[y].size(); ++x) {
            if (a.grid[y][x] != b.grid[y][x]) { anyDifferent = true; break; }
        }
    }
    ENJIN_EXPECT_TRUE(anyDifferent);
}

ENJIN_TEST_MAIN()
