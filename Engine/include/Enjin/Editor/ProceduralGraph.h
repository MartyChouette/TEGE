#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace Editor {

// Procedural graph node categories
enum class ProcNodeCategory : u8 {
    Generator,      // Algorithm source nodes (CA, BSP, WFC, etc.)
    Transform,      // Resize, rotate, mirror, offset
    Filter,         // Threshold, erosion, dilation, smooth
    Combine,        // Blend, overlay, mask, add, subtract
    Output,         // Tilemap output, heightmap output, entity spawner
    Parameter       // Seed, size, float/int constants
};

// Procedural graph node types
enum class ProcNodeType : u8 {
    // Generators — each wraps a ProceduralAlgorithms class
    Gen_CellularAutomata,
    Gen_RandomWalker,
    Gen_BSP,
    Gen_DiamondSquare,
    Gen_LSystem,
    Gen_WFC,
    Gen_Voronoi,
    Gen_Grammar,
    Gen_PrefabAssembler,
    Gen_Noise,              // Perlin/Simplex/Value noise

    // Transforms
    Xform_Resize,           // Resize grid to target dimensions
    Xform_Rotate90,         // Rotate 90/180/270
    Xform_Mirror,           // Flip H/V
    Xform_Offset,           // Translate with wrapping
    Xform_Invert,           // Invert 0<->1

    // Filters
    Filt_Threshold,         // Binarize at threshold value
    Filt_Erode,             // Shrink filled regions
    Filt_Dilate,            // Grow filled regions
    Filt_Smooth,            // Gaussian blur
    Filt_EdgeDetect,        // Sobel edge detection

    // Combine
    Comb_Blend,             // Weighted blend of two grids
    Comb_Overlay,           // Place B on A where B > 0
    Comb_Mask,              // Multiply A by B as mask
    Comb_Add,               // Clamp(A + B)
    Comb_Subtract,          // Clamp(A - B)
    Comb_Min,               // Per-cell min
    Comb_Max,               // Per-cell max

    // Output
    Out_Tilemap,            // Apply result to tilemap component
    Out_Heightmap,          // Apply as terrain heightmap
    Out_EntitySpawner,      // Spawn entities at filled cells

    // Parameters
    Param_Seed,             // Seed input (u32)
    Param_Size,             // Width/Height (u32 x2)
    Param_Float,            // Float constant
    Param_Int,              // Int constant

    COUNT
};

// Procedural graph node definition
struct ProcGraphNode {
    u32 id = 0;
    ProcNodeType type = ProcNodeType::Gen_CellularAutomata;
    Math::Vector2 position;
    std::string label;

    // Parameter storage (type-dependent)
    f32 floatParams[8] = {};     // Generic float parameters
    i32 intParams[8] = {};       // Generic int parameters
    std::string stringParam;     // Name/path parameter

    // Generator-specific defaults are set on creation
};

// Procedural graph link
struct ProcGraphLink {
    u32 id = 0;
    u32 fromNode = 0;
    u32 fromPin = 0;     // 0 = grid output, 1+ = secondary outputs
    u32 toNode = 0;
    u32 toPin = 0;       // 0 = primary grid input, 1+ = secondary inputs
};

// Procedural graph data
struct ProcGraphData {
    std::string name = "New Procedural Graph";
    std::vector<ProcGraphNode> nodes;
    std::vector<ProcGraphLink> links;
    u32 nextNodeId = 1;
    u32 nextLinkId = 1;

    // Preview settings
    u32 previewWidth = 256;
    u32 previewHeight = 256;
    u32 globalSeed = 0;         // 0 = random
};

// Execution result from graph evaluation
struct ProcGraphResult {
    std::vector<std::vector<f32>> grid;     // Output grid (0.0-1.0)
    u32 width = 0;
    u32 height = 0;
    bool success = false;
    std::string error;
};

// Procedural graph editor (visual node-based pipeline composition)
class ENJIN_API ProceduralGraphEditor {
public:
    void Render();
    void SetGraph(ProcGraphData* graph);
    ProcGraphData* GetGraph() { return m_Graph; }
    bool IsOpen() const { return m_Open; }
    void SetOpen(bool open) { m_Open = open; }

    // Execute the graph to produce output
    ProcGraphResult Execute() const;

    // Get the preview grid (cached from last execution)
    const std::vector<std::vector<f32>>& GetPreviewGrid() const { return m_PreviewGrid; }
    u32 GetPreviewWidth() const { return m_PreviewW; }
    u32 GetPreviewHeight() const { return m_PreviewH; }

private:
    ProcGraphData* m_Graph = nullptr;
    bool m_Open = false;
    u32 m_SelectedNodeId = 0;
    Math::Vector2 m_ScrollOffset;
    f32 m_Zoom = 1.0f;

    // Cached preview
    std::vector<std::vector<f32>> m_PreviewGrid;
    u32 m_PreviewW = 0;
    u32 m_PreviewH = 0;
    bool m_PreviewDirty = true;

    void DrawNode(ProcGraphNode& node);
    void DrawConnections();
    void DrawInspector();
    void DrawContextMenu();
    void DrawPreview();

    // Evaluate a single node (recursive, follows links)
    ProcGraphResult EvaluateNode(u32 nodeId, u32 width, u32 height, u32 seed,
                                  std::unordered_map<u32, ProcGraphResult>& cache) const;

    static ProcNodeCategory GetCategory(ProcNodeType type);
    static const char* GetNodeName(ProcNodeType type);
    static u32 GetInputPinCount(ProcNodeType type);
    static u32 GetOutputPinCount(ProcNodeType type);
};

} // namespace Editor
} // namespace Enjin
