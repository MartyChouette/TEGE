#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace Editor {

// Shader graph node categories
enum class ShaderNodeCategory : u8 {
    Input,          // Vertex position, UV, normal, time, camera
    Math,           // Add, multiply, lerp, clamp, sin, cos, pow
    Texture,        // Sample2D, sample cubemap, UV transform
    Color,          // Color constant, HSV convert, blend
    Vector,         // Split, combine, normalize, dot, cross
    Output,         // Fragment color, vertex displacement, alpha
    Utility         // Comment, reroute
};

// Shader graph node types
enum class ShaderNodeType : u8 {
    // Inputs
    VertexPosition, VertexNormal, VertexUV, VertexColor,
    Time, CameraPosition, ViewDirection,
    FloatConstant, Vec2Constant, Vec3Constant, Vec4Constant, ColorConstant,
    TextureParameter, FloatParameter,

    // Math
    Add, Subtract, Multiply, Divide,
    Lerp, Clamp, Saturate, Abs, Negate,
    Sin, Cos, Pow, Sqrt, Floor, Ceil, Fract,
    Min, Max, Step, SmoothStep,

    // Texture
    SampleTexture2D, SampleCubemap,
    UVTransform, Parallax, Flipbook,

    // Color
    HSVToRGB, RGBToHSV, Brightness, Contrast, Blend,

    // Vector
    SplitVec2, SplitVec3, SplitVec4,
    CombineVec2, CombineVec3, CombineVec4,
    Normalize, DotProduct, CrossProduct, Reflect,

    // Output
    FragmentOutput,     // Final color + alpha
    VertexOutput,       // Vertex displacement

    // Screen-space inputs
    SceneColor, SceneNormal, SceneDepth,

    // Flow control
    StaticSwitch,

    // Utility
    Comment, Reroute
};

// Shader graph node definition
struct ShaderGraphNode {
    u32 id = 0;
    ShaderNodeType type = ShaderNodeType::FloatConstant;
    Math::Vector2 position;
    std::string label;

    // Parameter values
    f32 floatValue = 0.0f;
    Math::Vector2 vec2Value;
    Math::Vector3 vec3Value;
    Math::Vector4 vec4Value;
    std::string texturePath;
    std::string parameterName;
};

// Shader graph link
struct ShaderGraphLink {
    u32 id = 0;
    u32 fromNode = 0;
    u32 fromPin = 0;
    u32 toNode = 0;
    u32 toPin = 0;
};

// Shader graph data
struct ShaderGraphData {
    std::string name = "New Shader";
    std::vector<ShaderGraphNode> nodes;
    std::vector<ShaderGraphLink> links;
    u32 nextNodeId = 1;
    u32 nextLinkId = 1;

    // Target shader stage
    enum class Stage : u8 { Vertex, Fragment, Both } stage = Stage::Fragment;
};

// GLSL code generation result
struct ShaderCodeResult {
    std::string vertexCode;
    std::string fragmentCode;
    std::vector<std::string> errors;
    bool success = false;
};

// Shader graph editor (skeleton)
class ENJIN_API ShaderGraphEditor {
public:
    void Render();
    void SetGraph(ShaderGraphData* graph);
    bool IsOpen() const { return m_Open; }
    void SetOpen(bool open) { m_Open = open; }

    // Code generation — topological sort + per-node GLSL emission
    ShaderCodeResult GenerateGLSL() const;

    // Save/Load (.enjshader JSON format)
    bool Save(const std::string& path) const;
    bool Load(const std::string& path);

private:
    ShaderGraphData* m_Graph = nullptr;
    bool m_Open = false;
    u32 m_SelectedNodeId = 0;
    Math::Vector2 m_ScrollOffset;
    f32 m_Zoom = 1.0f;

    // Last generation result for display
    ShaderCodeResult m_LastResult;
    bool m_ShowCodeWindow = false;

    void DrawNode(ShaderGraphNode& node);
    void DrawConnections();
    void DrawInspector();
    void DrawContextMenu();

    static ShaderNodeCategory GetCategory(ShaderNodeType type);
    static const char* GetNodeName(ShaderNodeType type);
};

} // namespace Editor
} // namespace Enjin
