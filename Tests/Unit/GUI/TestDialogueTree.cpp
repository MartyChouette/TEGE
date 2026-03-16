#include "EnjinTest.h"
#include "Enjin/GUI/DialogueTree.h"

using namespace Enjin;
using namespace Enjin::GUI;

// ===========================================================================
// DialogueTreeData
// ===========================================================================

ENJIN_TEST(DialogueTree, EmptyTree) {
    DialogueTreeData tree;
    ENJIN_EXPECT_EQ(tree.rootNodeId, 0u);
    ENJIN_EXPECT_EQ(tree.nodes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(tree.nextId, 1u);
}

ENJIN_TEST(DialogueTree, AddNode) {
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::Text);
    ENJIN_EXPECT_NE(id, 0u);
    ENJIN_EXPECT_EQ(tree.nodes.size(), (size_t)1);
}

ENJIN_TEST(DialogueTree, AddMultipleNodes) {
    DialogueTreeData tree;
    u32 id1 = tree.AddNode(DialogueNodeType::Text);
    u32 id2 = tree.AddNode(DialogueNodeType::Choice);
    u32 id3 = tree.AddNode(DialogueNodeType::Condition);
    ENJIN_EXPECT_NE(id1, id2);
    ENJIN_EXPECT_NE(id2, id3);
    ENJIN_EXPECT_EQ(tree.nodes.size(), (size_t)3);
}

ENJIN_TEST(DialogueTree, GetNode) {
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::Text);
    DialogueNode* node = tree.GetNode(id);
    ENJIN_ASSERT_NOT_NULL(node);
    ENJIN_EXPECT_EQ((int)node->type, (int)DialogueNodeType::Text);
}

ENJIN_TEST(DialogueTree, GetNonexistentReturnsNull) {
    DialogueTreeData tree;
    DialogueNode* node = tree.GetNode(999);
    ENJIN_EXPECT_NULL(node);
}

ENJIN_TEST(DialogueTree, RemoveNode) {
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::Text);
    tree.RemoveNode(id);
    ENJIN_EXPECT_NULL(tree.GetNode(id));
}

ENJIN_TEST(DialogueTree, NodeTypePreserved) {
    DialogueTreeData tree;
    u32 id1 = tree.AddNode(DialogueNodeType::Event);
    u32 id2 = tree.AddNode(DialogueNodeType::SetVariable);
    u32 id3 = tree.AddNode(DialogueNodeType::End);
    ENJIN_EXPECT_EQ((int)tree.GetNode(id1)->type, (int)DialogueNodeType::Event);
    ENJIN_EXPECT_EQ((int)tree.GetNode(id2)->type, (int)DialogueNodeType::SetVariable);
    ENJIN_EXPECT_EQ((int)tree.GetNode(id3)->type, (int)DialogueNodeType::End);
}

// ===========================================================================
// DialogueNode Defaults
// ===========================================================================

ENJIN_TEST(DialogueNode, Defaults) {
    DialogueNode node;
    ENJIN_EXPECT_EQ(node.id, 0u);
    ENJIN_EXPECT_EQ((int)node.type, (int)DialogueNodeType::Text);
    ENJIN_EXPECT_EQ(node.nextNodeId, 0u);
    ENJIN_EXPECT_EQ(node.trueNodeId, 0u);
    ENJIN_EXPECT_EQ(node.falseNodeId, 0u);
    ENJIN_EXPECT_TRUE(node.speakerName.empty());
    ENJIN_EXPECT_TRUE(node.text.empty());
}

ENJIN_TEST(DialogueNode, SpeakerColor) {
    DialogueNode node;
    ENJIN_EXPECT_FLOAT_EQ(node.speakerColor.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(node.speakerColor.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(node.speakerColor.z, 1.0f);
}

// ===========================================================================
// DialogueChoice
// ===========================================================================

ENJIN_TEST(DialogueChoice, Defaults) {
    DialogueChoice choice;
    ENJIN_EXPECT_EQ(choice.targetNodeId, 0u);
    ENJIN_EXPECT_FALSE(choice.hasCondition);
    ENJIN_EXPECT_TRUE(choice.text.empty());
}

// ===========================================================================
// DialogueCondition
// ===========================================================================

ENJIN_TEST(DialogueCondition, DefaultOp) {
    DialogueCondition cond;
    ENJIN_EXPECT_EQ((int)cond.op, (int)DialogueCondition::Op::Equals);
}

// ===========================================================================
// DialoguePlayer
// ===========================================================================

ENJIN_TEST(DialoguePlayer, InitiallyInactive) {
    DialoguePlayer player;
    ENJIN_EXPECT_FALSE(player.IsActive());
}

ENJIN_TEST(DialoguePlayer, StartActivatesPlayer) {
    DialogueTreeData tree;
    tree.treeName = "Test";
    u32 rootId = tree.AddNode(DialogueNodeType::Text);
    tree.rootNodeId = rootId;
    auto* node = tree.GetNode(rootId);
    node->text = "Hello!";
    node->speakerName = "NPC";

    DialoguePlayer player;
    player.Start(tree);
    ENJIN_EXPECT_TRUE(player.IsActive());
}

ENJIN_TEST(DialoguePlayer, Variables) {
    DialoguePlayer player;
    player.SetVariable("gold", "100");
    ENJIN_EXPECT_STR_EQ(player.GetVariable("gold"), "100");
}

ENJIN_TEST(DialoguePlayer, UnknownVariableEmpty) {
    DialoguePlayer player;
    ENJIN_EXPECT_STR_EQ(player.GetVariable("nonexistent"), "");
}

ENJIN_TEST(DialoguePlayer, EventCallback) {
    DialogueTreeData tree;
    u32 eventNode = tree.AddNode(DialogueNodeType::Event);
    tree.rootNodeId = eventNode;
    tree.GetNode(eventNode)->eventName = "quest_complete";

    DialoguePlayer player;
    std::string receivedEvent;
    player.SetEventCallback([&](const std::string& e) { receivedEvent = e; });
    player.Start(tree);
    ENJIN_EXPECT_STR_EQ(receivedEvent, "quest_complete");
}

ENJIN_TEST_MAIN()
