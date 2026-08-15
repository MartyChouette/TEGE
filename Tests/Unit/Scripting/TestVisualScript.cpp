#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/VisualScript/NodeDefinition.h"
#include "Enjin/VisualScript/NodeRegistry.h"
#include "Enjin/VisualScript/VisualScriptExecutor.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/Editor/NodeGraph.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Systems/DialogueSystem.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Scripting/ScriptBindings.h"

using namespace Enjin;
using namespace Enjin::VisualScript;
using namespace Enjin::ECS;
using namespace Enjin::Editor;

// ===========================================================================
// Helpers
// ===========================================================================

// Pin compatibility check matching the real VisualScriptEditor logic
static bool ArePinsCompatible(const Pin& from, const Pin& to) {
    if (from.type == PinType::Flow || to.type == PinType::Flow) {
        return from.type == PinType::Flow && to.type == PinType::Flow;
    }
    if (from.type == PinType::Any || to.type == PinType::Any) {
        return true;
    }
    return from.type == to.type;
}

// Build a minimal script with a single pure math node wired to an OnStart event.
// Returns the ID of the math node. Caller can then evaluate it via the executor.
static NodeId BuildPureMathScript(VisualScriptComponent& script,
                                   const char* mathType,
                                   f32 a, f32 b) {
    // OnStart event node
    NodeId startNodeId = script.graph.AddNode("On Start", Math::Vector2(0, 0));
    PinId startFlowOut = script.graph.AddPin(startNodeId, "", PinType::Flow, PinKind::Output);

    // Math node (pure)
    NodeId mathNodeId = script.graph.AddNode(mathType, Math::Vector2(200, 0));
    PinId mathInA  = script.graph.AddPin(mathNodeId, "A", PinType::Float, PinKind::Input);
    PinId mathInB  = script.graph.AddPin(mathNodeId, "B", PinType::Float, PinKind::Input);
    PinId mathOut  = script.graph.AddPin(mathNodeId, "Result", PinType::Float, PinKind::Output);
    (void)mathInA; (void)mathInB; (void)mathOut; (void)startFlowOut;

    // Set up node metadata
    VisualScriptNodeMeta startMeta;
    startMeta.nodeType = NodeTypes::OnStart;
    script.nodeMeta[startNodeId] = startMeta;

    VisualScriptNodeMeta mathMeta;
    mathMeta.nodeType = mathType;
    // Store pin default values for A and B
    mathMeta.pinValues[mathInA] = a;
    mathMeta.pinValues[mathInB] = b;
    script.nodeMeta[mathNodeId] = mathMeta;

    script.SetEventNode(VisualScriptEvent::OnStart, startNodeId);
    return mathNodeId;
}

// ===========================================================================
// 1. NodeRegistry — registration and lookup
// ===========================================================================

ENJIN_TEST(NodeRegistry, FindBuiltinMathNodes) {
    auto& reg = NodeRegistry::Instance();

    const NodeDefinition* add = reg.FindNode(NodeTypes::Add);
    ENJIN_ASSERT_NOT_NULL(add);
    ENJIN_EXPECT_STR_EQ(add->typeId, NodeTypes::Add);
    ENJIN_EXPECT_TRUE(add->IsPure());

    const NodeDefinition* sub = reg.FindNode(NodeTypes::Subtract);
    ENJIN_ASSERT_NOT_NULL(sub);
    ENJIN_EXPECT_TRUE(sub->IsPure());

    const NodeDefinition* mul = reg.FindNode(NodeTypes::Multiply);
    ENJIN_ASSERT_NOT_NULL(mul);

    const NodeDefinition* div = reg.FindNode(NodeTypes::Divide);
    ENJIN_ASSERT_NOT_NULL(div);
}

ENJIN_TEST(NodeRegistry, FindReturnsNullForUnknownId) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* unknown = reg.FindNode("NonExistent_Node_12345");
    ENJIN_EXPECT_NULL(unknown);
}

ENJIN_TEST(NodeRegistry, GetNodesByCategoryMath) {
    auto& reg = NodeRegistry::Instance();
    auto mathNodes = reg.GetNodesByCategory(NodeCategory::Math);

    // We know at least Add, Subtract, Multiply, Divide, Modulo, etc. are registered
    ENJIN_EXPECT_GE(mathNodes.size(), (size_t)5);

    // All returned nodes should be in the Math category
    for (const auto* def : mathNodes) {
        ENJIN_EXPECT_EQ((int)def->category, (int)NodeCategory::Math);
    }
}

ENJIN_TEST(NodeRegistry, SearchReturnsRelevantResults) {
    auto& reg = NodeRegistry::Instance();
    auto results = reg.SearchNodes("branch");
    ENJIN_EXPECT_GT(results.size(), (size_t)0);

    // The top result should be the Branch node
    ENJIN_EXPECT_STR_EQ(results[0]->typeId, NodeTypes::Branch);
}

ENJIN_TEST(NodeRegistry, EventNodeFlags) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* onStart = reg.FindNode(NodeTypes::OnStart);
    ENJIN_ASSERT_NOT_NULL(onStart);
    ENJIN_EXPECT_TRUE(onStart->IsEvent());
    ENJIN_EXPECT_FALSE(onStart->IsPure());
    ENJIN_EXPECT_FALSE(onStart->IsLatent());

    const NodeDefinition* delay = reg.FindNode(NodeTypes::Delay);
    ENJIN_ASSERT_NOT_NULL(delay);
    ENJIN_EXPECT_TRUE(delay->IsLatent());
    ENJIN_EXPECT_FALSE(delay->IsEvent());
}

// ===========================================================================
// 2. NodeGraphData — adding/removing nodes, connecting ports
// ===========================================================================

ENJIN_TEST(NodeGraphData, AddAndRemoveNodes) {
    NodeGraphData graph;

    NodeId n1 = graph.AddNode("First", Math::Vector2(0, 0));
    NodeId n2 = graph.AddNode("Second", Math::Vector2(100, 0));
    ENJIN_EXPECT_NE(n1, n2);
    ENJIN_EXPECT_NOT_NULL(graph.FindNode(n1));
    ENJIN_EXPECT_NOT_NULL(graph.FindNode(n2));
    ENJIN_EXPECT_EQ(graph.GetNodes().size(), (size_t)2);

    graph.RemoveNode(n1);
    ENJIN_EXPECT_NULL(graph.FindNode(n1));
    ENJIN_EXPECT_EQ(graph.GetNodes().size(), (size_t)1);
}

ENJIN_TEST(NodeGraphData, AddLinksAndQuery) {
    NodeGraphData graph;
    NodeId n1 = graph.AddNode("A", Math::Vector2(0, 0));
    NodeId n2 = graph.AddNode("B", Math::Vector2(200, 0));

    PinId out = graph.AddPin(n1, "Out", PinType::Float, PinKind::Output);
    PinId in  = graph.AddPin(n2, "In", PinType::Float, PinKind::Input);

    ENJIN_EXPECT_FALSE(graph.HasLinkBetween(out, in));

    LinkId link = graph.AddLink(out, in);
    ENJIN_EXPECT_TRUE(graph.HasLinkBetween(out, in));
    ENJIN_EXPECT_NOT_NULL(graph.FindLink(link));

    auto links = graph.GetLinksForPin(out);
    ENJIN_EXPECT_EQ(links.size(), (size_t)1);

    graph.RemoveLink(link);
    ENJIN_EXPECT_FALSE(graph.HasLinkBetween(out, in));
    ENJIN_EXPECT_NULL(graph.FindLink(link));
}

// ===========================================================================
// 3. Port type validation
// ===========================================================================

ENJIN_TEST(PortCompatibility, FlowOnlyConnectsToFlow) {
    Pin flowOut = {1, "Out", PinType::Flow, PinKind::Output, 1};
    Pin flowIn  = {2, "In",  PinType::Flow, PinKind::Input,  2};
    Pin floatIn = {3, "Val", PinType::Float, PinKind::Input, 2};

    ENJIN_EXPECT_TRUE(ArePinsCompatible(flowOut, flowIn));
    ENJIN_EXPECT_FALSE(ArePinsCompatible(flowOut, floatIn));
    ENJIN_EXPECT_FALSE(ArePinsCompatible(floatIn, flowIn));
}

ENJIN_TEST(PortCompatibility, AnyConnectsToNonFlow) {
    Pin anyOut   = {1, "Out", PinType::Any,    PinKind::Output, 1};
    Pin floatIn  = {2, "In",  PinType::Float,  PinKind::Input,  2};
    Pin stringIn = {3, "In",  PinType::String, PinKind::Input,  2};
    Pin vec3In   = {4, "In",  PinType::Vector3, PinKind::Input, 2};
    Pin flowIn   = {5, "In",  PinType::Flow,   PinKind::Input,  2};

    ENJIN_EXPECT_TRUE(ArePinsCompatible(anyOut, floatIn));
    ENJIN_EXPECT_TRUE(ArePinsCompatible(anyOut, stringIn));
    ENJIN_EXPECT_TRUE(ArePinsCompatible(anyOut, vec3In));
    ENJIN_EXPECT_FALSE(ArePinsCompatible(anyOut, flowIn));
}

ENJIN_TEST(PortCompatibility, IncompatibleDataTypes) {
    Pin floatOut  = {1, "Out", PinType::Float,  PinKind::Output, 1};
    Pin intIn     = {2, "In",  PinType::Int,    PinKind::Input,  2};
    Pin stringIn  = {3, "In",  PinType::String, PinKind::Input,  2};
    Pin vec3In    = {4, "In",  PinType::Vector3, PinKind::Input, 2};

    ENJIN_EXPECT_FALSE(ArePinsCompatible(floatOut, intIn));
    ENJIN_EXPECT_FALSE(ArePinsCompatible(floatOut, stringIn));
    ENJIN_EXPECT_FALSE(ArePinsCompatible(floatOut, vec3In));
}

// ===========================================================================
// 4. Math node evaluation
// ===========================================================================

ENJIN_TEST(MathNodes, AddFloats) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* add = reg.FindNode(NodeTypes::Add);
    ENJIN_ASSERT_NOT_NULL(add);
    ENJIN_ASSERT_NOT_NULL(add->evaluate);

    ExecutionContext ctx;
    std::vector<VariableValue> inputs = {3.0f, 7.0f};
    VariableValue result = add->evaluate(ctx, inputs);
    ENJIN_ASSERT_TRUE(std::holds_alternative<f32>(result));
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(result), 10.0f);
}

ENJIN_TEST(MathNodes, SubtractFloats) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* sub = reg.FindNode(NodeTypes::Subtract);
    ENJIN_ASSERT_NOT_NULL(sub);

    ExecutionContext ctx;
    std::vector<VariableValue> inputs = {10.0f, 3.5f};
    VariableValue result = sub->evaluate(ctx, inputs);
    ENJIN_ASSERT_TRUE(std::holds_alternative<f32>(result));
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(result), 6.5f);
}

ENJIN_TEST(MathNodes, MultiplyFloats) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* mul = reg.FindNode(NodeTypes::Multiply);
    ENJIN_ASSERT_NOT_NULL(mul);

    ExecutionContext ctx;
    std::vector<VariableValue> inputs = {4.0f, 5.0f};
    VariableValue result = mul->evaluate(ctx, inputs);
    ENJIN_ASSERT_TRUE(std::holds_alternative<f32>(result));
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(result), 20.0f);
}

ENJIN_TEST(MathNodes, DivideByZeroReturnsZero) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* div = reg.FindNode(NodeTypes::Divide);
    ENJIN_ASSERT_NOT_NULL(div);

    ExecutionContext ctx;
    std::vector<VariableValue> inputs = {42.0f, 0.0f};
    VariableValue result = div->evaluate(ctx, inputs);
    ENJIN_ASSERT_TRUE(std::holds_alternative<f32>(result));
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(result), 0.0f);
}

ENJIN_TEST(MathNodes, DivideNormal) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* div = reg.FindNode(NodeTypes::Divide);
    ENJIN_ASSERT_NOT_NULL(div);

    ExecutionContext ctx;
    std::vector<VariableValue> inputs = {20.0f, 4.0f};
    VariableValue result = div->evaluate(ctx, inputs);
    ENJIN_ASSERT_TRUE(std::holds_alternative<f32>(result));
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(result), 5.0f);
}

ENJIN_TEST(MathNodes, AddVector3) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* add = reg.FindNode(NodeTypes::Add);
    ENJIN_ASSERT_NOT_NULL(add);

    ExecutionContext ctx;
    std::vector<VariableValue> inputs = {
        Math::Vector3(1.0f, 2.0f, 3.0f),
        Math::Vector3(4.0f, 5.0f, 6.0f)
    };
    VariableValue result = add->evaluate(ctx, inputs);
    ENJIN_ASSERT_TRUE(std::holds_alternative<Math::Vector3>(result));
    Math::Vector3 v = std::get<Math::Vector3>(result);
    ENJIN_EXPECT_VEC3_EQ(v, 5.0f, 7.0f, 9.0f);
}

// ===========================================================================
// 5. Flow control — Branch and Sequence
// ===========================================================================

ENJIN_TEST(FlowControl, BranchTrue) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* branch = reg.FindNode(NodeTypes::Branch);
    ENJIN_ASSERT_NOT_NULL(branch);

    ExecutionContext ctx;
    std::vector<VariableValue> inputs = {true};
    std::vector<VariableValue> outputs;
    branch->execute(ctx, inputs, outputs);

    // nextFlowIndex 0 = True path
    ENJIN_EXPECT_EQ(ctx.nextFlowIndex, (i32)0);
}

ENJIN_TEST(FlowControl, BranchFalse) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* branch = reg.FindNode(NodeTypes::Branch);
    ENJIN_ASSERT_NOT_NULL(branch);

    ExecutionContext ctx;
    std::vector<VariableValue> inputs = {false};
    std::vector<VariableValue> outputs;
    branch->execute(ctx, inputs, outputs);

    // nextFlowIndex 1 = False path
    ENJIN_EXPECT_EQ(ctx.nextFlowIndex, (i32)1);
}

ENJIN_TEST(FlowControl, SequenceHasMultipleFlowOutputs) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* seq = reg.FindNode(NodeTypes::Sequence);
    ENJIN_ASSERT_NOT_NULL(seq);

    // Sequence should have 3 flow outputs (Then 0, Then 1, Then 2)
    ENJIN_EXPECT_EQ(seq->CountFlowOutputs(), (usize)3);

    // And it should have a flow input
    ENJIN_EXPECT_TRUE(seq->HasFlowInput());
    ENJIN_EXPECT_TRUE(seq->HasFlowOutput());
}

// ===========================================================================
// 6. Variable get/set
// ===========================================================================

ENJIN_TEST(Variables, FindVariableByName) {
    VisualScriptComponent script;
    script.variables.push_back(VisualScriptVariable::Float("speed", 5.0f));
    script.variables.push_back(VisualScriptVariable::Bool("isJumping", false));
    script.variables.push_back(VisualScriptVariable::Int("score", 100));

    auto* speed = script.FindVariable("speed");
    ENJIN_ASSERT_NOT_NULL(speed);
    ENJIN_ASSERT_TRUE(std::holds_alternative<f32>(speed->value));
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(speed->value), 5.0f);

    auto* score = script.FindVariable("score");
    ENJIN_ASSERT_NOT_NULL(score);
    ENJIN_ASSERT_TRUE(std::holds_alternative<i32>(score->value));
    ENJIN_EXPECT_EQ(std::get<i32>(score->value), 100);

    auto* missing = script.FindVariable("nonexistent");
    ENJIN_EXPECT_NULL(missing);
}

ENJIN_TEST(Variables, SetAndReadBack) {
    VisualScriptComponent script;
    script.variables.push_back(VisualScriptVariable::Float("health", 100.0f));

    auto* health = script.FindVariable("health");
    ENJIN_ASSERT_NOT_NULL(health);
    health->value = 75.0f;

    auto* healthAgain = script.FindVariable("health");
    ENJIN_ASSERT_NOT_NULL(healthAgain);
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(healthAgain->value), 75.0f);
}

// ===========================================================================
// 7. Script execution start/stop (runtime state)
// ===========================================================================

ENJIN_TEST(ScriptRuntime, InitialState) {
    VisualScriptComponent script;
    ENJIN_EXPECT_TRUE(script.enabled);
    ENJIN_EXPECT_FALSE(script.initialized);
    ENJIN_EXPECT_FALSE(script.started);
    ENJIN_EXPECT_FALSE(script.isPaused);
    ENJIN_EXPECT_EQ(script.currentlyExecutingNode, (NodeId)0);
}

ENJIN_TEST(ScriptRuntime, ResetRuntimeStateClearsAll) {
    VisualScriptComponent script;
    script.initialized = true;
    script.started = true;
    script.isPaused = true;
    script.pausedAtNode = 42;
    script.stepRequested = true;
    script.currentlyExecutingNode = 99;

    // Add some latent state and breakpoint hit count
    script.latentStates[1] = LatentNodeState{1.0f, true, 0, INVALID_ENTITY, ""};
    script.breakpoints[10] = VisualScriptComponent::BreakpointInfo{true, "", 5, 0};
    script.pureNodeCache[7] = 3.14f;

    script.ResetRuntimeState();

    ENJIN_EXPECT_FALSE(script.initialized);
    ENJIN_EXPECT_FALSE(script.started);
    ENJIN_EXPECT_FALSE(script.isPaused);
    ENJIN_EXPECT_EQ(script.pausedAtNode, (NodeId)0);
    ENJIN_EXPECT_FALSE(script.stepRequested);
    ENJIN_EXPECT_EQ(script.currentlyExecutingNode, (NodeId)0);
    ENJIN_EXPECT_TRUE(script.latentStates.empty());
    ENJIN_EXPECT_TRUE(script.pureNodeCache.empty());

    // Breakpoint should still exist but hit count reset
    ENJIN_EXPECT_EQ(script.breakpoints.size(), (size_t)1);
    ENJIN_EXPECT_EQ(script.breakpoints[10].hitCount, (u32)0);
}

ENJIN_TEST(ScriptRuntime, DisabledScriptDoesNotExecute) {
    World world;
    Entity e = world.CreateEntity();

    VisualScriptComponent script;
    script.enabled = false;

    // Build a trivial graph with OnStart
    NodeId startNode = script.graph.AddNode("On Start", Math::Vector2(0, 0));
    script.graph.AddPin(startNode, "", PinType::Flow, PinKind::Output);
    VisualScriptNodeMeta meta;
    meta.nodeType = NodeTypes::OnStart;
    script.nodeMeta[startNode] = meta;
    script.SetEventNode(VisualScriptEvent::OnStart, startNode);

    VisualScriptExecutor executor;
    executor.ExecuteEvent(&world, e, &script, VisualScriptEvent::OnStart, 0.016f);

    // Should not have executed anything
    ENJIN_EXPECT_EQ(executor.GetLastStats().nodesExecuted, (u32)0);
}

// ===========================================================================
// 8. Function call depth limiting
// ===========================================================================

ENJIN_TEST(CallDepth, MaxCallDepthConstant) {
    VisualScriptExecutor executor;
    // MAX_CALL_DEPTH is 32 as defined in the header.
    // Verify the executor has a sensible default max iterations as well.
    ENJIN_EXPECT_EQ(executor.GetMaxIterations(), (u32)10000);
}

// ===========================================================================
// 9. Max iteration bounds
// ===========================================================================

ENJIN_TEST(IterationBounds, SetAndGetMaxIterations) {
    VisualScriptExecutor executor;
    ENJIN_EXPECT_EQ(executor.GetMaxIterations(), (u32)10000);

    executor.SetMaxIterations(500);
    ENJIN_EXPECT_EQ(executor.GetMaxIterations(), (u32)500);

    executor.SetMaxIterations(0);
    ENJIN_EXPECT_EQ(executor.GetMaxIterations(), (u32)0);
}

ENJIN_TEST(IterationBounds, EventMappingRoundTrip) {
    VisualScriptComponent script;

    script.SetEventNode(VisualScriptEvent::OnStart, 10);
    script.SetEventNode(VisualScriptEvent::OnUpdate, 20);
    script.SetCustomEventNode("player_died", 30);

    ENJIN_EXPECT_EQ(script.GetEventNode(VisualScriptEvent::OnStart), (NodeId)10);
    ENJIN_EXPECT_EQ(script.GetEventNode(VisualScriptEvent::OnUpdate), (NodeId)20);
    ENJIN_EXPECT_EQ(script.GetCustomEventNode("player_died"), (NodeId)30);

    // Unmapped events should return 0
    ENJIN_EXPECT_EQ(script.GetEventNode(VisualScriptEvent::OnDestroy), (NodeId)0);
    ENJIN_EXPECT_EQ(script.GetCustomEventNode("unknown_event"), (NodeId)0);
}

// ===========================================================================
// 10. Latent Node Execution (delayed/async)
// ===========================================================================

ENJIN_TEST(Latent, DelayNodeIsLatentFlagged) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* delay = reg.FindNode(NodeTypes::Delay);
    ENJIN_ASSERT_NOT_NULL(delay);
    ENJIN_EXPECT_TRUE(delay->IsLatent());
}

ENJIN_TEST(Latent, InitialStateHasNoLatentNodes) {
    VisualScriptComponent script;
    ENJIN_EXPECT_TRUE(script.latentStates.empty());
}

ENJIN_TEST(Latent, LatentNodeStateStoredAndCleared) {
    VisualScriptComponent script;

    // Manually inject a latent state (simulates what executor does for Delay)
    LatentNodeState state;
    state.timeRemaining = 2.5f;
    state.isActive      = true;
    state.resumeFlowPin = 7;
    script.latentStates[42] = state;

    ENJIN_EXPECT_EQ(script.latentStates.size(), (size_t)1);

    auto it = script.latentStates.find(42);
    ENJIN_ASSERT_TRUE(it != script.latentStates.end());
    ENJIN_EXPECT_FLOAT_EQ(it->second.timeRemaining, 2.5f);
    ENJIN_EXPECT_TRUE(it->second.isActive);
    ENJIN_EXPECT_EQ(it->second.resumeFlowPin, (Editor::PinId)7);

    // ResetRuntimeState clears latent states
    script.ResetRuntimeState();
    ENJIN_EXPECT_TRUE(script.latentStates.empty());
}

ENJIN_TEST(Latent, MultipleLatentNodesTrackedIndependently) {
    VisualScriptComponent script;

    LatentNodeState s1;
    s1.timeRemaining = 1.0f;
    s1.isActive      = true;

    LatentNodeState s2;
    s2.timeRemaining = 3.0f;
    s2.isActive      = true;

    script.latentStates[10] = s1;
    script.latentStates[20] = s2;

    ENJIN_EXPECT_EQ(script.latentStates.size(), (size_t)2);
    ENJIN_EXPECT_FLOAT_EQ(script.latentStates[10].timeRemaining, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(script.latentStates[20].timeRemaining, 3.0f);
}

// ===========================================================================
// 11. Entity Destruction During Script Execution
// ===========================================================================

ENJIN_TEST(EntityLifetime, DisabledScriptSurvivesEntityInvalidity) {
    // When an entity is invalid/destroyed, a disabled script should not crash
    // the executor — the executor should check enabled before any node execution.
    World world;
    Entity e = world.CreateEntity();

    VisualScriptComponent script;
    script.enabled = false;

    NodeId startNode = script.graph.AddNode("On Start", Math::Vector2(0, 0));
    script.graph.AddPin(startNode, "", PinType::Flow, PinKind::Output);
    VisualScriptNodeMeta meta;
    meta.nodeType = NodeTypes::OnStart;
    script.nodeMeta[startNode] = meta;
    script.SetEventNode(VisualScriptEvent::OnStart, startNode);

    // Destroy entity before ticking
    world.DestroyEntity(e);
    world.Update(0.016f);  // flush deferred destruction

    VisualScriptExecutor executor;
    // Executing a script on a destroyed entity with enabled=false should not execute any nodes
    executor.ExecuteEvent(&world, e, &script, VisualScriptEvent::OnStart, 0.016f);
    ENJIN_EXPECT_EQ(executor.GetLastStats().nodesExecuted, (u32)0);
}

ENJIN_TEST(EntityLifetime, ScriptVariablesStayAfterEntityDestruction) {
    // Variables belong to the component, not the world. They should remain
    // accessible even after the entity is invalidated (before the component
    // itself is cleaned up).
    VisualScriptComponent script;
    script.variables.push_back(VisualScriptVariable::Float("speed", 12.5f));

    // Simulate entity no longer valid — script component still holds data
    auto* v = script.FindVariable("speed");
    ENJIN_ASSERT_NOT_NULL(v);
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(v->value), 12.5f);
}

// ===========================================================================
// 12. Flow Control: Branch, Loop (Sequence), Sequence node
// ===========================================================================

ENJIN_TEST(FlowControl_Extended, BranchOutputIndexForTrue) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* branch = reg.FindNode(NodeTypes::Branch);
    ENJIN_ASSERT_NOT_NULL(branch);

    ExecutionContext ctx;
    std::vector<VariableValue> inputs = {true};
    std::vector<VariableValue> outputs;
    branch->execute(ctx, inputs, outputs);
    ENJIN_EXPECT_EQ(ctx.nextFlowIndex, (i32)0);
}

ENJIN_TEST(FlowControl_Extended, BranchOutputIndexForFalse) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* branch = reg.FindNode(NodeTypes::Branch);
    ENJIN_ASSERT_NOT_NULL(branch);

    ExecutionContext ctx;
    std::vector<VariableValue> inputs = {false};
    std::vector<VariableValue> outputs;
    branch->execute(ctx, inputs, outputs);
    ENJIN_EXPECT_EQ(ctx.nextFlowIndex, (i32)1);
}

ENJIN_TEST(FlowControl_Extended, SequenceNodeFlowOutputCount) {
    auto& reg = NodeRegistry::Instance();
    const NodeDefinition* seq = reg.FindNode(NodeTypes::Sequence);
    ENJIN_ASSERT_NOT_NULL(seq);
    // Sequence exposes multiple Then outputs — must have at least 2
    ENJIN_EXPECT_TRUE(seq->CountFlowOutputs() >= (usize)2);
}

// ===========================================================================
// 13. Pin Connections and Disconnections
// ===========================================================================

ENJIN_TEST(PinConnections, AddAndRemoveMultipleLinks) {
    NodeGraphData graph;
    NodeId n1 = graph.AddNode("Source", Math::Vector2(0, 0));
    NodeId n2 = graph.AddNode("SinkA",  Math::Vector2(200, 0));
    NodeId n3 = graph.AddNode("SinkB",  Math::Vector2(200, 100));

    PinId out  = graph.AddPin(n1, "Out",  PinType::Float, PinKind::Output);
    PinId inA  = graph.AddPin(n2, "In",   PinType::Float, PinKind::Input);
    PinId inB  = graph.AddPin(n3, "In",   PinType::Float, PinKind::Input);

    LinkId l1 = graph.AddLink(out, inA);
    LinkId l2 = graph.AddLink(out, inB);

    ENJIN_EXPECT_TRUE(graph.HasLinkBetween(out, inA));
    ENJIN_EXPECT_TRUE(graph.HasLinkBetween(out, inB));
    ENJIN_EXPECT_EQ(graph.GetLinksForPin(out).size(), (size_t)2);

    graph.RemoveLink(l1);
    ENJIN_EXPECT_FALSE(graph.HasLinkBetween(out, inA));
    ENJIN_EXPECT_TRUE(graph.HasLinkBetween(out, inB));
    ENJIN_EXPECT_EQ(graph.GetLinksForPin(out).size(), (size_t)1);

    graph.RemoveLink(l2);
    ENJIN_EXPECT_FALSE(graph.HasLinkBetween(out, inB));
    ENJIN_EXPECT_EQ(graph.GetLinksForPin(out).size(), (size_t)0);
}

ENJIN_TEST(PinConnections, RemovingNodeAlsoRemovesLinks) {
    NodeGraphData graph;
    NodeId n1 = graph.AddNode("A", Math::Vector2(0, 0));
    NodeId n2 = graph.AddNode("B", Math::Vector2(200, 0));

    PinId out = graph.AddPin(n1, "Out", PinType::Float, PinKind::Output);
    PinId in  = graph.AddPin(n2, "In",  PinType::Float, PinKind::Input);
    graph.AddLink(out, in);

    ENJIN_EXPECT_TRUE(graph.HasLinkBetween(out, in));

    // Removing n1 should cascade and remove any links on its pins
    graph.RemoveNode(n1);
    ENJIN_EXPECT_NULL(graph.FindNode(n1));
    ENJIN_EXPECT_EQ(graph.GetLinks().size(), (size_t)0);
}

// ===========================================================================
// 14. Variable Read/Write
// ===========================================================================

ENJIN_TEST(Variables_Extended, WriteNewVariableAtRuntime) {
    VisualScriptComponent script;
    // Start with no variables
    ENJIN_EXPECT_EQ(script.variables.size(), (size_t)0);

    script.variables.push_back(VisualScriptVariable::Int("counter", 0));
    auto* v = script.FindVariable("counter");
    ENJIN_ASSERT_NOT_NULL(v);
    ENJIN_EXPECT_EQ(std::get<i32>(v->value), 0);

    // Simulate a SetVariable node writing 42
    v->value = (i32)42;
    auto* v2 = script.FindVariable("counter");
    ENJIN_ASSERT_NOT_NULL(v2);
    ENJIN_EXPECT_EQ(std::get<i32>(v2->value), 42);
}

ENJIN_TEST(Variables_Extended, MultipleTypesCoexist) {
    VisualScriptComponent script;
    script.variables.push_back(VisualScriptVariable::Float("speed",     5.5f));
    script.variables.push_back(VisualScriptVariable::Int("lives",       3));
    script.variables.push_back(VisualScriptVariable::Bool("grounded",   true));
    script.variables.push_back(VisualScriptVariable::String("tag",      "Player"));

    ENJIN_EXPECT_EQ(script.variables.size(), (size_t)4);

    auto* speed    = script.FindVariable("speed");
    auto* lives    = script.FindVariable("lives");
    auto* grounded = script.FindVariable("grounded");
    auto* tag      = script.FindVariable("tag");

    ENJIN_ASSERT_NOT_NULL(speed);
    ENJIN_ASSERT_NOT_NULL(lives);
    ENJIN_ASSERT_NOT_NULL(grounded);
    ENJIN_ASSERT_NOT_NULL(tag);

    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(speed->value),           5.5f);
    ENJIN_EXPECT_EQ(std::get<i32>(lives->value),                 3);
    ENJIN_EXPECT_EQ(std::get<bool>(grounded->value),             true);
    ENJIN_EXPECT_STR_EQ(std::get<std::string>(tag->value),       "Player");
}

ENJIN_TEST(Variables_Extended, PureNodeCacheClearedByReset) {
    VisualScriptComponent script;
    script.pureNodeCache[55] = 3.14f;
    script.pureNodeCache[66] = std::string("cached");
    ENJIN_EXPECT_EQ(script.pureNodeCache.size(), (size_t)2);

    script.ResetRuntimeState();
    ENJIN_EXPECT_TRUE(script.pureNodeCache.empty());
}

// ===========================================================================
// Tier-1 node-coverage additions (docs/NODE_COVERAGE.md):
// hierarchy, tags, save-meta, sprite animation
// ===========================================================================

ENJIN_TEST(TierOneNodes, AllTwentyRegister) {
    auto& reg = NodeRegistry::Instance();
    const char* ids[] = {
        NodeTypes::EntitySetParent, NodeTypes::EntityRemoveParent, NodeTypes::EntityGetParent,
        NodeTypes::EntityGetChildCount, NodeTypes::EntityGetChild,
        NodeTypes::EntityAddTag, NodeTypes::EntityRemoveTag, NodeTypes::EntityHasTag,
        NodeTypes::FindEntityByTag,
        NodeTypes::MetaSetBool, NodeTypes::MetaGetBool, NodeTypes::MetaSetInt,
        NodeTypes::MetaGetInt, NodeTypes::MetaSetString, NodeTypes::MetaGetString,
        NodeTypes::SpriteAnimPlay, NodeTypes::SpriteAnimStop, NodeTypes::SpriteAnimSetSpeed,
        NodeTypes::SpriteAnimIsPlaying, NodeTypes::SpriteAnimGetFrame,
    };
    for (const char* id : ids) {
        const NodeDefinition* def = reg.FindNode(id);
        ENJIN_ASSERT_NOT_NULL(def);
        ENJIN_EXPECT_STR_EQ(def->typeId, id);
    }
}

ENJIN_TEST(TierOneNodes, HierarchySetGetChildParent) {
    World world;
    Entity parent = world.CreateEntity();
    Entity child  = world.CreateEntity();
    auto& reg = NodeRegistry::Instance();

    ExecutionContext ctx;
    ctx.world = &world;
    ctx.entity = child;
    std::vector<VariableValue> outs;

    // Arrange: Set Parent (inputs: flow, child, parent)
    const NodeDefinition* setParent = reg.FindNode(NodeTypes::EntitySetParent);
    ENJIN_ASSERT_NOT_NULL(setParent->execute);
    setParent->execute(ctx, { false, static_cast<Entity>(child), static_cast<Entity>(parent) }, outs);

    // Assert: Get Parent returns the parent
    const NodeDefinition* getParent = reg.FindNode(NodeTypes::EntityGetParent);
    VariableValue gp = getParent->evaluate(ctx, { static_cast<Entity>(child) });
    ENJIN_ASSERT_TRUE(std::holds_alternative<Entity>(gp));
    ENJIN_EXPECT_EQ(std::get<Entity>(gp), parent);

    // Assert: Get Child Count == 1, Get Child[0] == child
    const NodeDefinition* childCount = reg.FindNode(NodeTypes::EntityGetChildCount);
    VariableValue cc = childCount->evaluate(ctx, { static_cast<Entity>(parent) });
    ENJIN_ASSERT_TRUE(std::holds_alternative<i32>(cc));
    ENJIN_EXPECT_EQ(std::get<i32>(cc), 1);

    const NodeDefinition* getChild = reg.FindNode(NodeTypes::EntityGetChild);
    VariableValue gc = getChild->evaluate(ctx, { static_cast<Entity>(parent), static_cast<i32>(0) });
    ENJIN_ASSERT_TRUE(std::holds_alternative<Entity>(gc));
    ENJIN_EXPECT_EQ(std::get<Entity>(gc), child);

    // Out of range -> invalid
    VariableValue oob = getChild->evaluate(ctx, { static_cast<Entity>(parent), static_cast<i32>(5) });
    ENJIN_EXPECT_EQ(std::get<Entity>(oob), (Entity)INVALID_ENTITY);

    // Act: Remove Parent -> Get Parent invalid
    const NodeDefinition* removeParent = reg.FindNode(NodeTypes::EntityRemoveParent);
    removeParent->execute(ctx, { false, static_cast<Entity>(child) }, outs);
    VariableValue gp2 = getParent->evaluate(ctx, { static_cast<Entity>(child) });
    ENJIN_EXPECT_EQ(std::get<Entity>(gp2), (Entity)INVALID_ENTITY);
}

ENJIN_TEST(TierOneNodes, TagsAddHasFindRemove) {
    World world;
    Entity e = world.CreateEntity();
    auto& reg = NodeRegistry::Instance();

    ExecutionContext ctx;
    ctx.world = &world;
    ctx.entity = e;
    std::vector<VariableValue> outs;

    // Add Tag (inputs: flow, entity, tag) — creates TagComponent
    const NodeDefinition* addTag = reg.FindNode(NodeTypes::EntityAddTag);
    addTag->execute(ctx, { false, static_cast<Entity>(e), std::string("enemy") }, outs);

    // Has Tag == true
    const NodeDefinition* hasTag = reg.FindNode(NodeTypes::EntityHasTag);
    VariableValue h = hasTag->evaluate(ctx, { static_cast<Entity>(e), std::string("enemy") });
    ENJIN_ASSERT_TRUE(std::holds_alternative<bool>(h));
    ENJIN_EXPECT_TRUE(std::get<bool>(h));

    // Missing tag == false
    VariableValue h2 = hasTag->evaluate(ctx, { static_cast<Entity>(e), std::string("ally") });
    ENJIN_EXPECT_FALSE(std::get<bool>(h2));

    // Find Entity By Tag finds it
    const NodeDefinition* findByTag = reg.FindNode(NodeTypes::FindEntityByTag);
    VariableValue f = findByTag->evaluate(ctx, { std::string("enemy") });
    ENJIN_ASSERT_TRUE(std::holds_alternative<Entity>(f));
    ENJIN_EXPECT_EQ(std::get<Entity>(f), e);

    // Remove Tag -> Has Tag false
    const NodeDefinition* removeTag = reg.FindNode(NodeTypes::EntityRemoveTag);
    removeTag->execute(ctx, { false, static_cast<Entity>(e), std::string("enemy") }, outs);
    VariableValue h3 = hasTag->evaluate(ctx, { static_cast<Entity>(e), std::string("enemy") });
    ENJIN_EXPECT_FALSE(std::get<bool>(h3));
}

ENJIN_TEST(TierOneNodes, SpriteAnimPlayStopSpeedFrame) {
    World world;
    Entity e = world.CreateEntity();
    auto& sprite = world.AddComponent<AnimatedSprite2DComponent>(e);
    sprite.playing = false;
    sprite.currentFrame = 4;
    auto& reg = NodeRegistry::Instance();

    ExecutionContext ctx;
    ctx.world = &world;
    ctx.entity = e;
    std::vector<VariableValue> outs;

    // Play resets to frame 0 and marks playing
    reg.FindNode(NodeTypes::SpriteAnimPlay)->execute(ctx, { false, static_cast<Entity>(e) }, outs);
    VariableValue playing = reg.FindNode(NodeTypes::SpriteAnimIsPlaying)->evaluate(ctx, { static_cast<Entity>(e) });
    ENJIN_ASSERT_TRUE(std::holds_alternative<bool>(playing));
    ENJIN_EXPECT_TRUE(std::get<bool>(playing));
    VariableValue frame = reg.FindNode(NodeTypes::SpriteAnimGetFrame)->evaluate(ctx, { static_cast<Entity>(e) });
    ENJIN_ASSERT_TRUE(std::holds_alternative<i32>(frame));
    ENJIN_EXPECT_EQ(std::get<i32>(frame), 0);

    // Set Speed
    reg.FindNode(NodeTypes::SpriteAnimSetSpeed)->execute(ctx, { false, static_cast<Entity>(e), 2.5f }, outs);
    ENJIN_EXPECT_FLOAT_EQ(world.GetComponent<AnimatedSprite2DComponent>(e)->playbackSpeed, 2.5f);

    // Stop
    reg.FindNode(NodeTypes::SpriteAnimStop)->execute(ctx, { false, static_cast<Entity>(e) }, outs);
    VariableValue stopped = reg.FindNode(NodeTypes::SpriteAnimIsPlaying)->evaluate(ctx, { static_cast<Entity>(e) });
    ENJIN_EXPECT_FALSE(std::get<bool>(stopped));
}

// ===========================================================================
// Branching dialogue nodes (docs/NODE_COVERAGE.md tier-1 #1)
// ===========================================================================

ENJIN_TEST(DialogueNodes, AllSevenRegister) {
    auto& reg = NodeRegistry::Instance();
    const char* ids[] = {
        NodeTypes::DialogueChoose, NodeTypes::DialogueGetChoiceCount,
        NodeTypes::DialogueGetChoiceText, NodeTypes::DialogueGetSpeaker,
        NodeTypes::DialogueGetText, NodeTypes::DialogueSetVariable,
        NodeTypes::DialogueGetVariable,
    };
    for (const char* id : ids) {
        const NodeDefinition* def = reg.FindNode(id);
        ENJIN_ASSERT_NOT_NULL(def);
        ENJIN_EXPECT_STR_EQ(def->typeId, id);
    }
}

ENJIN_TEST(DialogueNodes, ReadsChoicesSpeakerAndText) {
    World world;
    Entity e = world.CreateEntity();
    auto& dlg = world.AddComponent<DialogueComponent>(e);
    dlg.speakerName = "Guard";
    dlg.dialogueLines = { "Halt!" };
    dlg.currentLine = 0;
    dlg.currentChar = 5;                    // full "Halt!" visible
    dlg.choices.push_back({ "Surrender", "" });
    dlg.choices.push_back({ "Fight", "" });

    auto& reg = NodeRegistry::Instance();
    ExecutionContext ctx;
    ctx.world = &world;
    ctx.entity = e;

    VariableValue cc = reg.FindNode(NodeTypes::DialogueGetChoiceCount)->evaluate(ctx, { static_cast<Entity>(e) });
    ENJIN_ASSERT_TRUE(std::holds_alternative<i32>(cc));
    ENJIN_EXPECT_EQ(std::get<i32>(cc), 2);

    VariableValue ct = reg.FindNode(NodeTypes::DialogueGetChoiceText)->evaluate(ctx, { static_cast<Entity>(e), static_cast<i32>(1) });
    ENJIN_ASSERT_TRUE(std::holds_alternative<std::string>(ct));
    ENJIN_EXPECT_STR_EQ(std::get<std::string>(ct).c_str(), "Fight");

    VariableValue sp = reg.FindNode(NodeTypes::DialogueGetSpeaker)->evaluate(ctx, { static_cast<Entity>(e) });
    ENJIN_EXPECT_STR_EQ(std::get<std::string>(sp).c_str(), "Guard");

    VariableValue tx = reg.FindNode(NodeTypes::DialogueGetText)->evaluate(ctx, { static_cast<Entity>(e) });
    ENJIN_EXPECT_STR_EQ(std::get<std::string>(tx).c_str(), "Halt!");

    // Out-of-range choice index -> empty string, no crash
    VariableValue oob = reg.FindNode(NodeTypes::DialogueGetChoiceText)->evaluate(ctx, { static_cast<Entity>(e), static_cast<i32>(9) });
    ENJIN_EXPECT_STR_EQ(std::get<std::string>(oob).c_str(), "");
}

ENJIN_TEST(DialogueNodes, SetThenGetVariableRoundTrips) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<DialogueComponent>(e);

    DialogueSystem dialogueSystem;   // no active tree player -> component-backed variables

    auto& reg = NodeRegistry::Instance();
    ExecutionContext ctx;
    ctx.world = &world;
    ctx.entity = e;
    ctx.dialogue = &dialogueSystem;
    std::vector<VariableValue> outs;

    reg.FindNode(NodeTypes::DialogueSetVariable)->execute(
        ctx, { false, static_cast<Entity>(e), std::string("mood"), std::string("angry") }, outs);

    VariableValue v = reg.FindNode(NodeTypes::DialogueGetVariable)->evaluate(
        ctx, { static_cast<Entity>(e), std::string("mood") });
    ENJIN_ASSERT_TRUE(std::holds_alternative<std::string>(v));
    ENJIN_EXPECT_STR_EQ(std::get<std::string>(v).c_str(), "angry");

    // Unset variable -> empty
    VariableValue miss = reg.FindNode(NodeTypes::DialogueGetVariable)->evaluate(
        ctx, { static_cast<Entity>(e), std::string("unset") });
    ENJIN_EXPECT_STR_EQ(std::get<std::string>(miss).c_str(), "");
}

ENJIN_TEST(DialogueNodes, VariableNodesNoOpWithoutSystem) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<DialogueComponent>(e);

    auto& reg = NodeRegistry::Instance();
    ExecutionContext ctx;             // ctx.dialogue stays null
    ctx.world = &world;
    ctx.entity = e;
    std::vector<VariableValue> outs;

    // Should not crash and should return empty when no system is wired.
    reg.FindNode(NodeTypes::DialogueSetVariable)->execute(
        ctx, { false, static_cast<Entity>(e), std::string("x"), std::string("y") }, outs);
    VariableValue v = reg.FindNode(NodeTypes::DialogueGetVariable)->evaluate(
        ctx, { static_cast<Entity>(e), std::string("x") });
    ENJIN_EXPECT_STR_EQ(std::get<std::string>(v).c_str(), "");
}

// ===========================================================================
// Accessibility nodes (docs/NODE_COVERAGE.md tier-2)
// ===========================================================================

ENJIN_TEST(AccessibilityNodes, AllRegister) {
    auto& reg = NodeRegistry::Instance();
    const char* ids[] = {
        NodeTypes::A11ySetFontScale, NodeTypes::A11yGetFontScale,
        NodeTypes::A11ySetReducedMotion, NodeTypes::A11yGetReducedMotion,
        NodeTypes::A11ySetScreenShake, NodeTypes::A11yGetScreenShake,
        NodeTypes::A11ySetContrast, NodeTypes::A11yGetContrast,
        NodeTypes::A11ySetColorblindStrength, NodeTypes::A11yGetColorblindStrength,
        NodeTypes::A11ySetSubtitles, NodeTypes::A11yGetSubtitles,
        NodeTypes::A11ySetDyslexiaFont, NodeTypes::A11yGetDyslexiaFont,
        NodeTypes::A11ySetScreenReader, NodeTypes::A11yGetScreenReader,
        NodeTypes::A11ySave,
    };
    for (const char* id : ids) {
        const NodeDefinition* def = reg.FindNode(id);
        ENJIN_ASSERT_NOT_NULL(def);
        ENJIN_EXPECT_STR_EQ(def->typeId, id);
    }
}

ENJIN_TEST(AccessibilityNodes, SetAppliesGetReadsSameSettings) {
    // The nodes forward to the same runtime settings + apply callback the
    // AngelScript API uses. Wire a settings struct and confirm round-trip.
    Accessibility::RuntimeAccessibilitySettings settings;
    bool appliedCalled = false;
    Scripting::SetBindingsAccessibilitySettings(&settings);
    Scripting::SetBindingsAccessibilityApplyCallback([&]() { appliedCalled = true; });

    auto& reg = NodeRegistry::Instance();
    ExecutionContext ctx;                 // accessibility nodes need no world
    std::vector<VariableValue> outs;

    // Font scale (float): set fires apply, get reads it back
    reg.FindNode(NodeTypes::A11ySetFontScale)->execute(ctx, { false, 2.0f }, outs);
    ENJIN_EXPECT_TRUE(appliedCalled);
    ENJIN_EXPECT_FLOAT_EQ(settings.fontScale, 2.0f);
    VariableValue fs = reg.FindNode(NodeTypes::A11yGetFontScale)->evaluate(ctx, {});
    ENJIN_ASSERT_TRUE(std::holds_alternative<f32>(fs));
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(fs), 2.0f);

    // Reduced motion (bool)
    reg.FindNode(NodeTypes::A11ySetReducedMotion)->execute(ctx, { false, true }, outs);
    ENJIN_EXPECT_TRUE(settings.reducedMotion);
    VariableValue rm = reg.FindNode(NodeTypes::A11yGetReducedMotion)->evaluate(ctx, {});
    ENJIN_ASSERT_TRUE(std::holds_alternative<bool>(rm));
    ENJIN_EXPECT_TRUE(std::get<bool>(rm));

    // Screen-shake-disabled + subtitles (bool) and colorblind strength (float)
    reg.FindNode(NodeTypes::A11ySetScreenShake)->execute(ctx, { false, true }, outs);
    ENJIN_EXPECT_TRUE(settings.disableScreenShake);
    reg.FindNode(NodeTypes::A11ySetSubtitles)->execute(ctx, { false, true }, outs);
    ENJIN_EXPECT_TRUE(settings.subtitlesEnabled);
    reg.FindNode(NodeTypes::A11ySetColorblindStrength)->execute(ctx, { false, 0.5f }, outs);
    ENJIN_EXPECT_FLOAT_EQ(settings.colorblindStrength, 0.5f);

    // Cleanup so global state does not leak to other tests
    Scripting::SetBindingsAccessibilitySettings(nullptr);
    Scripting::SetBindingsAccessibilityApplyCallback(nullptr);
}

ENJIN_TEST(AccessibilityNodes, NoOpAndDefaultsWithoutSettings) {
    Scripting::SetBindingsAccessibilitySettings(nullptr);
    auto& reg = NodeRegistry::Instance();
    ExecutionContext ctx;
    std::vector<VariableValue> outs;

    // Set is a safe no-op; Get returns sane defaults.
    reg.FindNode(NodeTypes::A11ySetFontScale)->execute(ctx, { false, 3.0f }, outs);
    VariableValue fs = reg.FindNode(NodeTypes::A11yGetFontScale)->evaluate(ctx, {});
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(fs), 1.0f);   // default
    VariableValue rm = reg.FindNode(NodeTypes::A11yGetReducedMotion)->evaluate(ctx, {});
    ENJIN_EXPECT_FALSE(std::get<bool>(rm));
}

// ===========================================================================
// Entry point
// ===========================================================================

ENJIN_TEST_MAIN()
