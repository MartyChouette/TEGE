#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/AI/BehaviorTreeExecutor.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"

using namespace Enjin;
using namespace Enjin::AI;
using namespace Enjin::Editor;

// ===========================================================================
// Helper: Build a BT graph node with metadata, input pin, and output pin.
// Returns the NodeId. Pins are wired so children can be linked via AddLink.
// ===========================================================================

struct BTBuilder {
    ECS::BehaviorTreeComponent comp;

    // Add a BT node with one input and one output pin.
    // Returns the NodeId.
    NodeId AddBTNode(BTNodeType type, Math::Vector2 pos = {0, 0}) {
        NodeId id = comp.graph.AddNode(BTNodeTypeToString(type), pos);
        comp.graph.AddPin(id, "In",  PinType::Flow, PinKind::Input);
        comp.graph.AddPin(id, "Out", PinType::Flow, PinKind::Output);

        BTNodeMeta meta;
        meta.nodeType = type;
        comp.nodeMeta[id] = meta;
        return id;
    }

    // Add a BT node and set a property on its metadata.
    NodeId AddBTNodeWithProp(BTNodeType type, const std::string& key,
                             const std::string& value, Math::Vector2 pos = {0, 0}) {
        NodeId id = AddBTNode(type, pos);
        comp.nodeMeta[id].properties[key] = value;
        return id;
    }

    // Link parent output to child input.
    void Link(NodeId parent, NodeId child) {
        // Output pin is always the second pin added to the parent node
        auto* parentNode = comp.graph.FindNode(parent);
        auto* childNode  = comp.graph.FindNode(child);
        if (!parentNode || !childNode) return;
        if (parentNode->outputs.empty() || childNode->inputs.empty()) return;
        comp.graph.AddLink(parentNode->outputs[0].id, childNode->inputs[0].id);
    }

    void SetRoot(NodeId id) {
        comp.rootNodeId = id;
    }
};

// A stub leaf that always returns a given status.
// Uses SetVariable action node -- we intercept by setting it up so the
// blackboard result determines success/failure.
// Alternatively: use a CheckVariable condition that we preload in blackboard.
// For simplicity, we create a thin condition node that checks a boolean var.

static NodeId AddAlwaysSucceedLeaf(BTBuilder& b, const std::string& varName, Math::Vector2 pos = {0, 0}) {
    // CheckVariable with key=varName, operator="==", value="true"
    // Pre-set the blackboard to have varName = true
    NodeId id = b.AddBTNode(BTNodeType::CheckVariable, pos);
    b.comp.nodeMeta[id].properties["key"] = varName;
    b.comp.nodeMeta[id].properties["operator"] = "==";
    b.comp.nodeMeta[id].properties["value"] = "true";
    b.comp.blackboard.Set(varName, true);
    return id;
}

static NodeId AddAlwaysFailLeaf(BTBuilder& b, const std::string& varName, Math::Vector2 pos = {0, 0}) {
    NodeId id = b.AddBTNode(BTNodeType::CheckVariable, pos);
    b.comp.nodeMeta[id].properties["key"] = varName;
    b.comp.nodeMeta[id].properties["operator"] = "==";
    b.comp.nodeMeta[id].properties["value"] = "true";
    // Set the blackboard var to false so the check fails
    b.comp.blackboard.Set(varName, false);
    return id;
}

// ===========================================================================
// 1. Data Model — Adding Nodes and Setting Root
// ===========================================================================

ENJIN_TEST(BehaviorTree, DataModel_AddNodesAndRoot) {
    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId seq  = b.AddBTNode(BTNodeType::Sequence);
    b.Link(root, seq);
    b.SetRoot(root);

    ENJIN_EXPECT_EQ(b.comp.rootNodeId, root);
    ENJIN_EXPECT_EQ(b.comp.graph.GetNodes().size(), (usize)2);
    ENJIN_EXPECT_EQ(b.comp.nodeMeta.size(), (usize)2);
    ENJIN_EXPECT_EQ(b.comp.nodeMeta[root].nodeType, BTNodeType::Root);
    ENJIN_EXPECT_EQ(b.comp.nodeMeta[seq].nodeType, BTNodeType::Sequence);
}

// ===========================================================================
// 2. Node Type Categories
// ===========================================================================

ENJIN_TEST(BehaviorTree, NodeTypeCategories) {
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::Root), BTNodeCategory::Root);

    // Composites
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::Selector), BTNodeCategory::Composite);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::Sequence), BTNodeCategory::Composite);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::Parallel), BTNodeCategory::Composite);

    // Decorators
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::Inverter), BTNodeCategory::Decorator);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::Repeater), BTNodeCategory::Decorator);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::Timeout), BTNodeCategory::Decorator);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::ForceSuccess), BTNodeCategory::Decorator);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::ForceFailure), BTNodeCategory::Decorator);

    // Actions
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::MoveTo), BTNodeCategory::Action);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::Attack), BTNodeCategory::Action);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::Wait), BTNodeCategory::Action);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::SetVariable), BTNodeCategory::Action);

    // Conditions
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::CanSeeTarget), BTNodeCategory::Condition);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::IsInRange), BTNodeCategory::Condition);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::HasTarget), BTNodeCategory::Condition);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::CheckVariable), BTNodeCategory::Condition);
    ENJIN_EXPECT_EQ(GetNodeCategory(BTNodeType::IsAlive), BTNodeCategory::Condition);
}

// ===========================================================================
// 3. Blackboard — Set/Get/Has/Remove/Clear with Different Types
// ===========================================================================

ENJIN_TEST(BehaviorTree, Blackboard_BasicOperations) {
    Blackboard bb;

    // Initially empty
    ENJIN_EXPECT_FALSE(bb.Has("health"));
    ENJIN_EXPECT_NULL(bb.Get("health"));

    // Set and get bool
    bb.Set("alive", true);
    ENJIN_EXPECT_TRUE(bb.Has("alive"));
    auto* val = bb.Get("alive");
    ENJIN_ASSERT_NOT_NULL(val);
    ENJIN_EXPECT_TRUE(std::holds_alternative<bool>(*val));
    ENJIN_EXPECT_EQ(std::get<bool>(*val), true);

    // Set and get int
    bb.Set("score", (i32)42);
    val = bb.Get("score");
    ENJIN_ASSERT_NOT_NULL(val);
    ENJIN_EXPECT_EQ(std::get<i32>(*val), (i32)42);

    // Set and get float
    bb.Set("speed", 3.5f);
    val = bb.Get("speed");
    ENJIN_ASSERT_NOT_NULL(val);
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(*val), 3.5f);

    // Set and get string
    bb.Set("name", std::string("Enemy"));
    val = bb.Get("name");
    ENJIN_ASSERT_NOT_NULL(val);
    ENJIN_EXPECT_STR_EQ(std::get<std::string>(*val), "Enemy");

    // Set and get Vector3
    bb.Set("position", Math::Vector3(1.0f, 2.0f, 3.0f));
    val = bb.Get("position");
    ENJIN_ASSERT_NOT_NULL(val);
    auto& v = std::get<Math::Vector3>(*val);
    ENJIN_EXPECT_FLOAT_EQ(v.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.y, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.z, 3.0f);

    // Set and get Entity
    ECS::Entity ent = 99;
    bb.Set("target", ent);
    val = bb.Get("target");
    ENJIN_ASSERT_NOT_NULL(val);
    ENJIN_EXPECT_EQ(std::get<ECS::Entity>(*val), (ECS::Entity)99);
}

ENJIN_TEST(BehaviorTree, Blackboard_RemoveAndClear) {
    Blackboard bb;
    bb.Set("a", (i32)1);
    bb.Set("b", (i32)2);
    bb.Set("c", (i32)3);

    ENJIN_EXPECT_TRUE(bb.Has("a"));
    bb.Remove("a");
    ENJIN_EXPECT_FALSE(bb.Has("a"));
    ENJIN_EXPECT_TRUE(bb.Has("b"));

    bb.Clear();
    ENJIN_EXPECT_FALSE(bb.Has("b"));
    ENJIN_EXPECT_FALSE(bb.Has("c"));
    ENJIN_EXPECT_EQ(bb.GetAll().size(), (usize)0);
}

ENJIN_TEST(BehaviorTree, Blackboard_OverwriteValue) {
    Blackboard bb;
    bb.Set("val", (i32)10);
    ENJIN_EXPECT_EQ(std::get<i32>(*bb.Get("val")), (i32)10);
    bb.Set("val", (i32)20);
    ENJIN_EXPECT_EQ(std::get<i32>(*bb.Get("val")), (i32)20);

    // Overwrite with different type
    bb.Set("val", std::string("now a string"));
    ENJIN_EXPECT_TRUE(std::holds_alternative<std::string>(*bb.Get("val")));
}

// ===========================================================================
// 4. Sequence Node — All Children Succeed -> Success
// ===========================================================================

ENJIN_TEST(BehaviorTree, Sequence_AllSucceed) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId seq  = b.AddBTNode(BTNodeType::Sequence, {0, 0});
    NodeId c1   = AddAlwaysSucceedLeaf(b, "s1", {0, 10});
    NodeId c2   = AddAlwaysSucceedLeaf(b, "s2", {0, 20});
    NodeId c3   = AddAlwaysSucceedLeaf(b, "s3", {0, 30});

    b.Link(root, seq);
    b.Link(seq, c1);
    b.Link(seq, c2);
    b.Link(seq, c3);
    b.SetRoot(root);
    b.comp.initialized = true;  // skip blackboard default init

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Success);
}

// ===========================================================================
// 5. Sequence Node — One Child Fails -> Failure
// ===========================================================================

ENJIN_TEST(BehaviorTree, Sequence_OneChildFails) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId seq  = b.AddBTNode(BTNodeType::Sequence, {0, 0});
    NodeId c1   = AddAlwaysSucceedLeaf(b, "s1", {0, 10});
    NodeId c2   = AddAlwaysFailLeaf(b, "f1", {0, 20});
    NodeId c3   = AddAlwaysSucceedLeaf(b, "s2", {0, 30});

    b.Link(root, seq);
    b.Link(seq, c1);
    b.Link(seq, c2);
    b.Link(seq, c3);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Failure);
}

// ===========================================================================
// 6. Selector Node — First Child Succeeds -> Success
// ===========================================================================

ENJIN_TEST(BehaviorTree, Selector_FirstSucceeds) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId sel  = b.AddBTNode(BTNodeType::Selector, {0, 0});
    NodeId c1   = AddAlwaysFailLeaf(b, "f1", {0, 10});
    NodeId c2   = AddAlwaysSucceedLeaf(b, "s1", {0, 20});
    NodeId c3   = AddAlwaysFailLeaf(b, "f2", {0, 30});

    b.Link(root, sel);
    b.Link(sel, c1);
    b.Link(sel, c2);
    b.Link(sel, c3);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Success);
}

// ===========================================================================
// 7. Selector Node — All Children Fail -> Failure
// ===========================================================================

ENJIN_TEST(BehaviorTree, Selector_AllFail) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId sel  = b.AddBTNode(BTNodeType::Selector, {0, 0});
    NodeId c1   = AddAlwaysFailLeaf(b, "f1", {0, 10});
    NodeId c2   = AddAlwaysFailLeaf(b, "f2", {0, 20});

    b.Link(root, sel);
    b.Link(sel, c1);
    b.Link(sel, c2);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Failure);
}

// ===========================================================================
// 8. Inverter Decorator — Flips Child Result
// ===========================================================================

ENJIN_TEST(BehaviorTree, Inverter_FlipsResult) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    // Invert a success leaf -> should be Failure
    {
        BTBuilder b;
        NodeId root = b.AddBTNode(BTNodeType::Root);
        NodeId inv  = b.AddBTNode(BTNodeType::Inverter, {0, 0});
        NodeId leaf = AddAlwaysSucceedLeaf(b, "s1", {0, 10});

        b.Link(root, inv);
        b.Link(inv, leaf);
        b.SetRoot(root);
        b.comp.initialized = true;

        BehaviorTreeExecutor executor;
        BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
        ENJIN_EXPECT_EQ(result, BTStatus::Failure);
    }

    // Invert a failure leaf -> should be Success
    {
        BTBuilder b;
        NodeId root = b.AddBTNode(BTNodeType::Root);
        NodeId inv  = b.AddBTNode(BTNodeType::Inverter, {0, 0});
        NodeId leaf = AddAlwaysFailLeaf(b, "f1", {0, 10});

        b.Link(root, inv);
        b.Link(inv, leaf);
        b.SetRoot(root);
        b.comp.initialized = true;

        BehaviorTreeExecutor executor;
        BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
        ENJIN_EXPECT_EQ(result, BTStatus::Success);
    }
}

// ===========================================================================
// 9. Repeater Decorator — Runs Child N Times Then Returns
// ===========================================================================

ENJIN_TEST(BehaviorTree, Repeater_RunsNTimes) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId rep  = b.AddBTNodeWithProp(BTNodeType::Repeater, "count", "3", {0, 0});
    NodeId leaf = AddAlwaysSucceedLeaf(b, "s1", {0, 10});

    b.Link(root, rep);
    b.Link(rep, leaf);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;

    // First two ticks: counter increments but hasn't reached 3 yet -> Running
    BTStatus r1 = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(r1, BTStatus::Running);

    BTStatus r2 = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(r2, BTStatus::Running);

    // Third tick: counter reaches 3, returns child's actual status (Success)
    BTStatus r3 = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(r3, BTStatus::Success);
}

// ===========================================================================
// 10. Graph Serialization Round-Trip (NodeGraphData ToJson/FromJson)
// ===========================================================================

ENJIN_TEST(BehaviorTree, GraphSerializationRoundTrip) {
    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root, {10, 20});
    NodeId seq  = b.AddBTNode(BTNodeType::Sequence, {100, 200});
    NodeId leaf = b.AddBTNode(BTNodeType::Wait, {200, 300});

    b.Link(root, seq);
    b.Link(seq, leaf);

    // Serialize
    auto json = b.comp.graph.ToJson();

    // Deserialize into a fresh graph
    NodeGraphData restored;
    restored.FromJson(json);

    // Same number of nodes and links
    ENJIN_EXPECT_EQ(restored.GetNodes().size(), b.comp.graph.GetNodes().size());
    ENJIN_EXPECT_EQ(restored.GetLinks().size(), b.comp.graph.GetLinks().size());

    // Verify node IDs and positions survived
    auto* restoredRoot = restored.FindNode(root);
    ENJIN_ASSERT_NOT_NULL(restoredRoot);
    ENJIN_EXPECT_FLOAT_EQ(restoredRoot->position.x, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(restoredRoot->position.y, 20.0f);

    auto* restoredSeq = restored.FindNode(seq);
    ENJIN_ASSERT_NOT_NULL(restoredSeq);
    ENJIN_EXPECT_FLOAT_EQ(restoredSeq->position.x, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(restoredSeq->position.y, 200.0f);

    // Verify link connectivity
    ENJIN_EXPECT_EQ(restored.GetLinks().size(), (usize)2);
}

// ===========================================================================
// 11. Empty Tree Handling — Tick Returns Failure
// ===========================================================================

ENJIN_TEST(BehaviorTree, EmptyTree_ReturnsFailure) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();

    // No root set (rootNodeId = 0)
    ECS::BehaviorTreeComponent comp;
    ENJIN_EXPECT_EQ(comp.rootNodeId, (NodeId)0);

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Failure);
}

ENJIN_TEST(BehaviorTree, DisabledTree_ReturnsFailure) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    b.SetRoot(root);
    b.comp.enabled = false;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Failure);
}

// ===========================================================================
// 12. Node Connection / Parent-Child Relationships
// ===========================================================================

ENJIN_TEST(BehaviorTree, NodeConnections) {
    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId seq  = b.AddBTNode(BTNodeType::Sequence, {0, 0});
    NodeId c1   = b.AddBTNode(BTNodeType::Wait, {0, 10});
    NodeId c2   = b.AddBTNode(BTNodeType::Attack, {0, 20});

    b.Link(root, seq);
    b.Link(seq, c1);
    b.Link(seq, c2);

    // Root's output should have 1 link
    auto* rootNode = b.comp.graph.FindNode(root);
    ENJIN_ASSERT_NOT_NULL(rootNode);
    ENJIN_EXPECT_EQ(rootNode->outputs.size(), (usize)1);
    auto rootLinks = b.comp.graph.GetLinksForPin(rootNode->outputs[0].id);
    ENJIN_EXPECT_EQ(rootLinks.size(), (usize)1);

    // Seq's output should have 2 links (one to c1, one to c2)
    auto* seqNode = b.comp.graph.FindNode(seq);
    ENJIN_ASSERT_NOT_NULL(seqNode);
    auto seqLinks = b.comp.graph.GetLinksForPin(seqNode->outputs[0].id);
    ENJIN_EXPECT_EQ(seqLinks.size(), (usize)2);

    // Each child's input should have 1 link
    auto* c1Node = b.comp.graph.FindNode(c1);
    ENJIN_ASSERT_NOT_NULL(c1Node);
    auto c1Links = b.comp.graph.GetLinksForPin(c1Node->inputs[0].id);
    ENJIN_EXPECT_EQ(c1Links.size(), (usize)1);

    // Pin owner lookup
    ENJIN_EXPECT_EQ(b.comp.graph.GetPinOwner(rootNode->outputs[0].id), root);
    ENJIN_EXPECT_EQ(b.comp.graph.GetPinOwner(c1Node->inputs[0].id), c1);
}

// ===========================================================================
// 13. BTNodeTypeToString / BTStatusToString Coverage
// ===========================================================================

ENJIN_TEST(BehaviorTree, StringConversions) {
    ENJIN_EXPECT_STR_EQ(BTNodeTypeToString(BTNodeType::Root), "Root");
    ENJIN_EXPECT_STR_EQ(BTNodeTypeToString(BTNodeType::Selector), "Selector");
    ENJIN_EXPECT_STR_EQ(BTNodeTypeToString(BTNodeType::Sequence), "Sequence");
    ENJIN_EXPECT_STR_EQ(BTNodeTypeToString(BTNodeType::Inverter), "Inverter");
    ENJIN_EXPECT_STR_EQ(BTNodeTypeToString(BTNodeType::Wait), "Wait");
    ENJIN_EXPECT_STR_EQ(BTNodeTypeToString(BTNodeType::CheckVariable), "Check Variable");

    ENJIN_EXPECT_STR_EQ(BTStatusToString(BTStatus::Running), "Running");
    ENJIN_EXPECT_STR_EQ(BTStatusToString(BTStatus::Success), "Success");
    ENJIN_EXPECT_STR_EQ(BTStatusToString(BTStatus::Failure), "Failure");
    ENJIN_EXPECT_STR_EQ(BTStatusToString(BTStatus::Invalid), "Invalid");

    ENJIN_EXPECT_STR_EQ(BTNodeCategoryToString(BTNodeCategory::Root), "Root");
    ENJIN_EXPECT_STR_EQ(BTNodeCategoryToString(BTNodeCategory::Composite), "Composite");
    ENJIN_EXPECT_STR_EQ(BTNodeCategoryToString(BTNodeCategory::Decorator), "Decorator");
    ENJIN_EXPECT_STR_EQ(BTNodeCategoryToString(BTNodeCategory::Action), "Action");
    ENJIN_EXPECT_STR_EQ(BTNodeCategoryToString(BTNodeCategory::Condition), "Condition");
}

// ===========================================================================
// 14. Component Reset Runtime State
// ===========================================================================

ENJIN_TEST(BehaviorTree, ResetRuntimeState) {
    ECS::BehaviorTreeComponent comp;
    comp.blackboard.Set("key", (i32)10);
    comp.nodeStatuses[1] = BTStatus::Success;
    comp.activeNode = 5;
    comp.tickTimer = 1.5f;
    comp.initialized = true;
    comp.nodeTimers[1] = 2.0f;
    comp.nodeCounters[1] = 3;

    comp.ResetRuntimeState();

    ENJIN_EXPECT_FALSE(comp.blackboard.Has("key"));
    ENJIN_EXPECT_EQ(comp.nodeStatuses.size(), (usize)0);
    ENJIN_EXPECT_EQ(comp.activeNode, (NodeId)0);
    ENJIN_EXPECT_FLOAT_EQ(comp.tickTimer, 0.0f);
    ENJIN_EXPECT_FALSE(comp.initialized);
    ENJIN_EXPECT_EQ(comp.nodeTimers.size(), (usize)0);
    ENJIN_EXPECT_EQ(comp.nodeCounters.size(), (usize)0);
}

// ===========================================================================
// 15. ForceSuccess / ForceFailure Decorators
// ===========================================================================

ENJIN_TEST(BehaviorTree, ForceSuccessDecorator) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    // ForceSuccess wrapping a failing leaf -> should still return Success
    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId fs   = b.AddBTNode(BTNodeType::ForceSuccess, {0, 0});
    NodeId leaf = AddAlwaysFailLeaf(b, "f1", {0, 10});

    b.Link(root, fs);
    b.Link(fs, leaf);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Success);
}

ENJIN_TEST(BehaviorTree, ForceFailureDecorator) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    // ForceFailure wrapping a succeeding leaf -> should return Failure
    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId ff   = b.AddBTNode(BTNodeType::ForceFailure, {0, 0});
    NodeId leaf = AddAlwaysSucceedLeaf(b, "s1", {0, 10});

    b.Link(root, ff);
    b.Link(ff, leaf);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Failure);
}

// ===========================================================================
// 16. Executor with Mock Conditions — Return Success / Failure / Running
// ===========================================================================

ENJIN_TEST(BehaviorTree, Executor_MockConditionSuccess) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId leaf = AddAlwaysSucceedLeaf(b, "mockOk", {0, 10});
    b.Link(root, leaf);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Success);
}

ENJIN_TEST(BehaviorTree, Executor_MockConditionFailure) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId leaf = AddAlwaysFailLeaf(b, "mockFail", {0, 10});
    b.Link(root, leaf);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Failure);
}

// A Running leaf: use a Wait action node with duration > deltaTime.
ENJIN_TEST(BehaviorTree, Executor_MockConditionRunning) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    // Wait node with 5-second duration returns Running on first tick
    NodeId wait = b.AddBTNodeWithProp(BTNodeType::Wait, "duration", "5.0", {0, 10});
    b.Link(root, wait);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Running);
}

// ===========================================================================
// 17. Composite: Sequence all succeed (extended explicit assertion)
// ===========================================================================

ENJIN_TEST(BehaviorTree, Sequence_AllSucceed_Extended) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId seq  = b.AddBTNode(BTNodeType::Sequence);
    NodeId c1   = AddAlwaysSucceedLeaf(b, "e1");
    NodeId c2   = AddAlwaysSucceedLeaf(b, "e2");
    NodeId c3   = AddAlwaysSucceedLeaf(b, "e3");
    NodeId c4   = AddAlwaysSucceedLeaf(b, "e4");

    b.Link(root, seq);
    b.Link(seq, c1); b.Link(seq, c2); b.Link(seq, c3); b.Link(seq, c4);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Success);
}

// ===========================================================================
// 18. Composite: Sequence first-fail stops at failing child
// ===========================================================================

ENJIN_TEST(BehaviorTree, Sequence_FirstFail_StopsEarly) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId seq  = b.AddBTNode(BTNodeType::Sequence);
    // First child fails: entire sequence must fail immediately
    NodeId c1   = AddAlwaysFailLeaf(b, "xFail");
    NodeId c2   = AddAlwaysSucceedLeaf(b, "xSucc");

    b.Link(root, seq);
    b.Link(seq, c1); b.Link(seq, c2);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Failure);
}

// ===========================================================================
// 19. Composite: Selector first-succeed
// ===========================================================================

ENJIN_TEST(BehaviorTree, Selector_FirstSucceed_SkipsRest) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId sel  = b.AddBTNode(BTNodeType::Selector);
    // Very first child succeeds: selector returns Success immediately
    NodeId c1   = AddAlwaysSucceedLeaf(b, "firstOk");
    NodeId c2   = AddAlwaysFailLeaf(b, "laterFail");

    b.Link(root, sel);
    b.Link(sel, c1); b.Link(sel, c2);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Success);
}

// ===========================================================================
// 20. Composite: Selector all-fail
// ===========================================================================

ENJIN_TEST(BehaviorTree, Selector_AllFail_Extended) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId sel  = b.AddBTNode(BTNodeType::Selector);
    NodeId c1   = AddAlwaysFailLeaf(b, "af1");
    NodeId c2   = AddAlwaysFailLeaf(b, "af2");
    NodeId c3   = AddAlwaysFailLeaf(b, "af3");

    b.Link(root, sel);
    b.Link(sel, c1); b.Link(sel, c2); b.Link(sel, c3);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Failure);
}

// ===========================================================================
// 21. Condition Evaluation: CheckVariable with == / != operators
// ===========================================================================

ENJIN_TEST(BehaviorTree, CheckVariable_EqualOperator) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    {
        // "health == 100" — blackboard has 100 -> Success
        BTBuilder b;
        NodeId root = b.AddBTNode(BTNodeType::Root);
        NodeId cond = b.AddBTNode(BTNodeType::CheckVariable);
        b.comp.nodeMeta[cond].properties["key"]      = "health";
        b.comp.nodeMeta[cond].properties["operator"] = "==";
        b.comp.nodeMeta[cond].properties["value"]    = "100";
        b.comp.blackboard.Set("health", (i32)100);
        b.Link(root, cond);
        b.SetRoot(root);
        b.comp.initialized = true;

        BehaviorTreeExecutor executor;
        ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Success);
    }
    {
        // "health == 100" — blackboard has 50 -> Failure
        BTBuilder b;
        NodeId root = b.AddBTNode(BTNodeType::Root);
        NodeId cond = b.AddBTNode(BTNodeType::CheckVariable);
        b.comp.nodeMeta[cond].properties["key"]      = "health";
        b.comp.nodeMeta[cond].properties["operator"] = "==";
        b.comp.nodeMeta[cond].properties["value"]    = "100";
        b.comp.blackboard.Set("health", (i32)50);
        b.Link(root, cond);
        b.SetRoot(root);
        b.comp.initialized = true;

        BehaviorTreeExecutor executor;
        ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Failure);
    }
}

ENJIN_TEST(BehaviorTree, CheckVariable_NotEqualOperator) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    // "alive != false" — blackboard alive = true -> Success
    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId cond = b.AddBTNode(BTNodeType::CheckVariable);
    b.comp.nodeMeta[cond].properties["key"]      = "alive";
    b.comp.nodeMeta[cond].properties["operator"] = "!=";
    b.comp.nodeMeta[cond].properties["value"]    = "false";
    b.comp.blackboard.Set("alive", true);
    b.Link(root, cond);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Success);
}

// ===========================================================================
// 22. Tree Reset / Restart
// ===========================================================================

ENJIN_TEST(BehaviorTree, TreeReset_ClearsRuntime) {
    ECS::BehaviorTreeComponent comp;
    comp.nodeStatuses[10]  = BTStatus::Running;
    comp.nodeStatuses[20]  = BTStatus::Success;
    comp.nodeCounters[10]  = 5;
    comp.nodeTimers[20]    = 3.0f;
    comp.activeNode        = 10;
    comp.tickTimer         = 0.5f;
    comp.initialized       = true;
    comp.blackboard.Set("x", (i32)99);

    comp.ResetRuntimeState();

    ENJIN_EXPECT_EQ(comp.nodeStatuses.size(), (usize)0);
    ENJIN_EXPECT_EQ(comp.nodeCounters.size(), (usize)0);
    ENJIN_EXPECT_EQ(comp.nodeTimers.size(), (usize)0);
    ENJIN_EXPECT_EQ(comp.activeNode, (NodeId)0);
    ENJIN_EXPECT_FLOAT_EQ(comp.tickTimer, 0.0f);
    ENJIN_EXPECT_FALSE(comp.initialized);
    ENJIN_EXPECT_FALSE(comp.blackboard.Has("x"));
}

ENJIN_TEST(BehaviorTree, TreeReset_ThenRetick_WorksCorrectly) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId leaf = AddAlwaysSucceedLeaf(b, "rval");
    b.Link(root, leaf);
    b.SetRoot(root);

    BehaviorTreeExecutor executor;

    // First execution
    b.comp.initialized = true;
    BTStatus r1 = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(r1, BTStatus::Success);

    // Reset runtime state
    b.comp.ResetRuntimeState();
    // Re-preload blackboard (cleared by reset)
    b.comp.blackboard.Set("rval", true);
    b.comp.initialized = true;

    // Second execution after reset should also succeed
    BTStatus r2 = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(r2, BTStatus::Success);
}

// ===========================================================================
// 23. Parallel Node Execution
// ===========================================================================

ENJIN_TEST(BehaviorTree, Parallel_AllChildrenSucceed) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId par  = b.AddBTNode(BTNodeType::Parallel);
    NodeId c1   = AddAlwaysSucceedLeaf(b, "ps1");
    NodeId c2   = AddAlwaysSucceedLeaf(b, "ps2");

    b.Link(root, par);
    b.Link(par, c1); b.Link(par, c2);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    // Parallel succeeds if all children succeed
    ENJIN_EXPECT_EQ(result, BTStatus::Success);
}

ENJIN_TEST(BehaviorTree, Parallel_OneChildFails) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId par  = b.AddBTNode(BTNodeType::Parallel);
    NodeId c1   = AddAlwaysSucceedLeaf(b, "pok");
    NodeId c2   = AddAlwaysFailLeaf(b, "pfail");

    b.Link(root, par);
    b.Link(par, c1); b.Link(par, c2);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    // Parallel fails if any child fails
    ENJIN_EXPECT_EQ(result, BTStatus::Failure);
}

// ===========================================================================
// 24. Decorator Nodes: Inverter, Repeater, Succeeder (ForceSuccess)
// ===========================================================================

ENJIN_TEST(BehaviorTree, Decorator_Inverter_TwiceIsIdentity) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    // Inverter(Inverter(Succeed)) == Succeed
    BTBuilder b;
    NodeId root  = b.AddBTNode(BTNodeType::Root);
    NodeId inv1  = b.AddBTNode(BTNodeType::Inverter, {0, 0});
    NodeId inv2  = b.AddBTNode(BTNodeType::Inverter, {0, 10});
    NodeId leaf  = AddAlwaysSucceedLeaf(b, "dbl");

    b.Link(root, inv1);
    b.Link(inv1, inv2);
    b.Link(inv2, leaf);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Success);
}

ENJIN_TEST(BehaviorTree, Decorator_Repeater_Count1_ReturnsChildStatus) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    // Repeater with count=1 should complete on first tick with child's status
    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId rep  = b.AddBTNodeWithProp(BTNodeType::Repeater, "count", "1");
    NodeId leaf = AddAlwaysSucceedLeaf(b, "r1leaf");

    b.Link(root, rep);
    b.Link(rep, leaf);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    BTStatus result = executor.Tick(&world, entity, b.comp, 0.016f);
    ENJIN_EXPECT_EQ(result, BTStatus::Success);
}

ENJIN_TEST(BehaviorTree, Decorator_Succeeder_WrapsFailure) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    // ForceSuccess around a failing leaf always returns Success
    BTBuilder b;
    NodeId root = b.AddBTNode(BTNodeType::Root);
    NodeId fs   = b.AddBTNode(BTNodeType::ForceSuccess);
    NodeId leaf = AddAlwaysFailLeaf(b, "wfail");

    b.Link(root, fs);
    b.Link(fs, leaf);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Success);
}

// ===========================================================================
// 25. Nested Composites
// ===========================================================================

ENJIN_TEST(BehaviorTree, Nested_SequenceInsideSelector) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    // Selector [ Sequence[fail, succeed], succeed ]
    // The inner sequence fails (first child fails), so selector tries second child (succeed)
    BTBuilder b;
    NodeId root  = b.AddBTNode(BTNodeType::Root);
    NodeId sel   = b.AddBTNode(BTNodeType::Selector);
    NodeId seq   = b.AddBTNode(BTNodeType::Sequence);
    NodeId s_c1  = AddAlwaysFailLeaf(b, "nf1");
    NodeId s_c2  = AddAlwaysSucceedLeaf(b, "ns1");
    NodeId sel_c = AddAlwaysSucceedLeaf(b, "ns2");

    b.Link(root, sel);
    b.Link(sel, seq);
    b.Link(sel, sel_c);
    b.Link(seq, s_c1);
    b.Link(seq, s_c2);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Success);
}

ENJIN_TEST(BehaviorTree, Nested_SelectorInsideSequence) {
    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(entity);

    // Sequence [ Selector[fail, succeed], succeed ]
    // Inner selector finds a success, sequence continues and also succeeds
    BTBuilder b;
    NodeId root  = b.AddBTNode(BTNodeType::Root);
    NodeId seq   = b.AddBTNode(BTNodeType::Sequence);
    NodeId sel   = b.AddBTNode(BTNodeType::Selector);
    NodeId sel_c1 = AddAlwaysFailLeaf(b, "nif1");
    NodeId sel_c2 = AddAlwaysSucceedLeaf(b, "nis1");
    NodeId seq_c2 = AddAlwaysSucceedLeaf(b, "nis2");

    b.Link(root, seq);
    b.Link(seq, sel);
    b.Link(seq, seq_c2);
    b.Link(sel, sel_c1);
    b.Link(sel, sel_c2);
    b.SetRoot(root);
    b.comp.initialized = true;

    BehaviorTreeExecutor executor;
    ENJIN_EXPECT_EQ(executor.Tick(&world, entity, b.comp, 0.016f), BTStatus::Success);
}

// ===========================================================================
// 26. Blackboard: GetAll returns all set keys
// ===========================================================================

ENJIN_TEST(BehaviorTree, Blackboard_GetAllReturnsCorrectCount) {
    Blackboard bb;
    bb.Set("a", (i32)1);
    bb.Set("b", (i32)2);
    bb.Set("c", (i32)3);
    bb.Set("d", (i32)4);
    ENJIN_EXPECT_EQ(bb.GetAll().size(), (usize)4);
    bb.Remove("b");
    ENJIN_EXPECT_EQ(bb.GetAll().size(), (usize)3);
    bb.Clear();
    ENJIN_EXPECT_EQ(bb.GetAll().size(), (usize)0);
}

ENJIN_TEST_MAIN()
