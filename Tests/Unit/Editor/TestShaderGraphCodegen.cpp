// The Shader Graph's Sample Cubemap node emitted flat grey.
//
//     case ShaderNodeType::SampleCubemap: {
//         // Cubemaps don't live in the 2D bindless array yet - neutral output.
//         body += "    vec4 " + var + " = vec4(0.5, 0.5, 0.5, 1.0); "
//                 "// cubemap sampling not supported yet\n";
//
// The node was in the Add menu and had a Texture field in its inspector, so it
// looked like a working node with a texture you had not set yet. It emitted a
// constant, and the texture field could never have been honoured: cubemaps
// genuinely are not in the 2D bindless array.
//
// What the comment missed is that not being in the bindless array does not mean
// there is no cubemap. There are two, both already in the pipeline layout this
// generated shader shares with the main pass -- the scene's skybox and the baked
// reflection probe -- each with a 1x1 dummy bound when empty, so sampling either is
// always safe.
//
// These tests check the generated GLSL, which is the artefact that matters: the
// node is correct exactly when the text it produces samples a cubemap and the
// shader it produces still compiles.
#include "EnjinTest.h"
#include "Enjin/Editor/ShaderGraph.h"
#include <string>

using namespace Enjin;
using namespace Enjin::Editor;

namespace {

bool Has(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

// A graph with one Sample Cubemap feeding the output.
ShaderGraphData MakeCubemapGraph(f32 source) {
    ShaderGraphData g;
    g.name = "CubemapProbe";

    ShaderGraphNode cube;
    cube.id = 1;
    cube.type = ShaderNodeType::SampleCubemap;
    cube.floatValue = source;      // 0 = skybox, 1 = reflection probe
    g.nodes.push_back(cube);

    ShaderGraphNode out;
    out.id = 2;
    out.type = ShaderNodeType::FragmentOutput;
    g.nodes.push_back(out);

    ShaderGraphLink link;
    link.id = 1;
    link.fromNode = 1;
    link.fromPin = 0;
    link.toNode = 2;
    link.toPin = 0;
    g.links.push_back(link);
    return g;
}

} // namespace

ENJIN_TEST(ShaderGraphCodegen, SampleCubemapNoLongerEmitsAConstant) {
    // Arrange
    ShaderGraphEditor editor;
    ShaderGraphData graph = MakeCubemapGraph(0.0f);
    editor.SetGraph(&graph);

    // Act
    const ShaderCodeResult result = editor.GenerateGLSL();

    // Assert
    ENJIN_ASSERT_TRUE(result.success);
    // The exact constant the node used to emit, and the comment that shipped with
    // it. Either appearing again means the node went back to being a placeholder.
    ENJIN_EXPECT_FALSE(Has(result.fragmentCode, "cubemap sampling not supported"));
    ENJIN_EXPECT_FALSE(Has(result.fragmentCode, "vec4(0.5, 0.5, 0.5, 1.0)"));
    // And it samples something.
    ENJIN_EXPECT_TRUE(Has(result.fragmentCode, "texture(sgSkyboxCubemap"));
}

ENJIN_TEST(ShaderGraphCodegen, TheSourcePickerSelectsTheOtherCubemap) {
    ShaderGraphEditor editor;

    ShaderGraphData skyGraph = MakeCubemapGraph(0.0f);
    editor.SetGraph(&skyGraph);
    const std::string sky = editor.GenerateGLSL().fragmentCode;
    ENJIN_EXPECT_TRUE(Has(sky, "texture(sgSkyboxCubemap"));
    ENJIN_EXPECT_FALSE(Has(sky, "texture(sgProbeCubemap"));

    ShaderGraphData probeGraph = MakeCubemapGraph(1.0f);
    editor.SetGraph(&probeGraph);
    const std::string probe = editor.GenerateGLSL().fragmentCode;
    ENJIN_EXPECT_TRUE(Has(probe, "texture(sgProbeCubemap"));
    ENJIN_EXPECT_FALSE(Has(probe, "texture(sgSkyboxCubemap,"));
}

ENJIN_TEST(ShaderGraphCodegen, BothCubemapsAreDeclaredAtTheirRealBindings) {
    // The bindings are not free choices: they have to match the pipeline layout the
    // generated shader shares with the main pass, or the sampler reads whatever
    // else is bound at that slot. 19 is the baked reflection probe, 28 the skybox.
    ShaderGraphEditor editor;
    ShaderGraphData graph = MakeCubemapGraph(0.0f);
    editor.SetGraph(&graph);
    const std::string code = editor.GenerateGLSL().fragmentCode;

    ENJIN_EXPECT_TRUE(Has(code, "layout(set = 0, binding = 19) uniform samplerCube sgProbeCubemap;"));
    ENJIN_EXPECT_TRUE(Has(code, "layout(set = 0, binding = 28) uniform samplerCube sgSkyboxCubemap;"));
}

ENJIN_TEST(ShaderGraphCodegen, AnUnconnectedDirectionFallsBackToTheViewVector) {
    // Leaving Dir unconnected is the common case -- a sky lookup or a reflection
    // both want the view direction. A fallback of 0 would sample one texel of the
    // cube forever and look like a constant, which is the bug this node just had.
    ShaderGraphEditor editor;
    ShaderGraphData graph = MakeCubemapGraph(0.0f);
    editor.SetGraph(&graph);
    const std::string code = editor.GenerateGLSL().fragmentCode;

    ENJIN_EXPECT_TRUE(Has(code, "normalize(fragWorldPos - uCameraPos)"));
}

ENJIN_TEST(ShaderGraphCodegen, TheCubemapDeclarationsDoNotCollideWithBindlessSamplers) {
    // The generated shader already declares bindless samplers in set 1. The cube
    // samplers go in set 0, where the main pass keeps them; putting them in set 1
    // would collide with the bindless array.
    ShaderGraphEditor editor;
    ShaderGraphData graph = MakeCubemapGraph(1.0f);
    editor.SetGraph(&graph);
    const std::string code = editor.GenerateGLSL().fragmentCode;

    ENJIN_EXPECT_FALSE(Has(code, "set = 1, binding = 19"));
    ENJIN_EXPECT_FALSE(Has(code, "set = 1, binding = 28"));
}

ENJIN_TEST(ShaderGraphCodegen, AGraphWithNoCubemapStillGenerates) {
    // The declarations are unconditional, so a graph that uses no cubemap must
    // still compile -- an unused sampler is legal and costs nothing, and making
    // the declaration conditional would let the preamble and the body disagree
    // about what is in scope.
    ShaderGraphData g;
    ShaderGraphNode c;
    c.id = 1;
    c.type = ShaderNodeType::Vec4Constant;
    c.vec4Value = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
    g.nodes.push_back(c);

    ShaderGraphNode out;
    out.id = 2;
    out.type = ShaderNodeType::FragmentOutput;
    g.nodes.push_back(out);

    ShaderGraphLink l;
    l.id = 1; l.fromNode = 1; l.fromPin = 0; l.toNode = 2; l.toPin = 0;
    g.links.push_back(l);

    ShaderGraphEditor editor;
    editor.SetGraph(&g);
    const ShaderCodeResult result = editor.GenerateGLSL();

    ENJIN_EXPECT_TRUE(result.success);
    ENJIN_EXPECT_TRUE(Has(result.fragmentCode, "samplerCube sgSkyboxCubemap"));
}

ENJIN_TEST_MAIN()
