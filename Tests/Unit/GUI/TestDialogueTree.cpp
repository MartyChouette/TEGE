#include "EnjinTest.h"
#include "Enjin/GUI/DialogueTree.h"
#include <nlohmann/json.hpp>

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

// ===========================================================================
// New node types: QuestAction, PlayCinematic, SetGameFlag
// ===========================================================================

ENJIN_TEST(DialogueTree, QuestActionNodeType) {
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::QuestAction);
    auto* node = tree.GetNode(id);
    ENJIN_ASSERT_NOT_NULL(node);
    ENJIN_EXPECT_EQ((int)node->type, (int)DialogueNodeType::QuestAction);
    node->questId = "main_quest";
    node->questAction = DialogueNode::QuestActionType::CompleteObjective;
    node->questObjectiveIndex = 2;
    ENJIN_EXPECT_STR_EQ(node->questId, "main_quest");
    ENJIN_EXPECT_EQ(node->questObjectiveIndex, 2);
}

ENJIN_TEST(DialogueTree, PlayCinematicNodeType) {
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::PlayCinematic);
    auto* node = tree.GetNode(id);
    ENJIN_ASSERT_NOT_NULL(node);
    ENJIN_EXPECT_EQ((int)node->type, (int)DialogueNodeType::PlayCinematic);
    node->cinematicEntityName = "intro_camera";
    ENJIN_EXPECT_STR_EQ(node->cinematicEntityName, "intro_camera");
}

ENJIN_TEST(DialogueTree, SetGameFlagNodeType) {
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::SetGameFlag);
    auto* node = tree.GetNode(id);
    ENJIN_ASSERT_NOT_NULL(node);
    ENJIN_EXPECT_EQ((int)node->type, (int)DialogueNodeType::SetGameFlag);
    node->gameFlagKey = "talked_to_npc";
    node->gameFlagValue = "true";
    ENJIN_EXPECT_STR_EQ(node->gameFlagKey, "talked_to_npc");
}

ENJIN_TEST(DialogueTree, QuestActionNodeProcessed) {
    DialogueTreeData tree;
    u32 root = tree.AddNode(DialogueNodeType::Root);
    u32 quest = tree.AddNode(DialogueNodeType::QuestAction);
    u32 end = tree.AddNode(DialogueNodeType::End);
    tree.rootNodeId = root;
    tree.GetNode(root)->nextNodeId = quest;
    tree.GetNode(quest)->nextNodeId = end;
    tree.GetNode(quest)->questId = "sword_quest";
    tree.GetNode(quest)->questAction = DialogueNode::QuestActionType::StartQuest;

    DialoguePlayer player;
    const DialogueNode* received = nullptr;
    player.SetActionCallback([&](const DialogueNode& n) { received = &n; });
    player.Start(tree);
    // Quest action is a pass-through — should auto-advance to End and deactivate
    ENJIN_EXPECT_FALSE(player.IsActive());
    // Action callback should have fired
    ENJIN_ASSERT_NOT_NULL(received);
    ENJIN_EXPECT_EQ((int)received->type, (int)DialogueNodeType::QuestAction);
}

ENJIN_TEST(DialogueTree, SetGameFlagNodeProcessed) {
    DialogueTreeData tree;
    u32 root = tree.AddNode(DialogueNodeType::Root);
    u32 flag = tree.AddNode(DialogueNodeType::SetGameFlag);
    u32 end = tree.AddNode(DialogueNodeType::End);
    tree.rootNodeId = root;
    tree.GetNode(root)->nextNodeId = flag;
    tree.GetNode(flag)->nextNodeId = end;
    tree.GetNode(flag)->gameFlagKey = "visited_town";
    tree.GetNode(flag)->gameFlagValue = "yes";

    DialoguePlayer player;
    std::string flagKey, flagValue;
    player.SetActionCallback([&](const DialogueNode& n) {
        flagKey = n.gameFlagKey;
        flagValue = n.gameFlagValue;
    });
    player.Start(tree);
    ENJIN_EXPECT_FALSE(player.IsActive());
    ENJIN_EXPECT_STR_EQ(flagKey, "visited_town");
    ENJIN_EXPECT_STR_EQ(flagValue, "yes");
}

// ===========================================================================
// DialogueCondition::Source
// ===========================================================================

ENJIN_TEST(DialogueTree, ConditionSourceDefault) {
    DialogueCondition cond;
    ENJIN_EXPECT_EQ((int)cond.source, (int)DialogueCondition::Source::Variable);
}

ENJIN_TEST(DialogueTree, ConditionResolverQuestStatus) {
    // Build a tree: Root -> Condition(quest) -> true:Text / false:End
    DialogueTreeData tree;
    u32 root = tree.AddNode(DialogueNodeType::Root);
    u32 cond = tree.AddNode(DialogueNodeType::Condition);
    u32 textNode = tree.AddNode(DialogueNodeType::Text);
    u32 end = tree.AddNode(DialogueNodeType::End);
    tree.rootNodeId = root;
    tree.GetNode(root)->nextNodeId = cond;
    tree.GetNode(cond)->condition.source = DialogueCondition::Source::QuestStatus;
    tree.GetNode(cond)->condition.variable = "main_quest";
    tree.GetNode(cond)->condition.op = DialogueCondition::Op::Equals;
    tree.GetNode(cond)->condition.value = "active";
    tree.GetNode(cond)->trueNodeId = textNode;
    tree.GetNode(cond)->falseNodeId = end;
    tree.GetNode(textNode)->text = "Quest is active!";
    tree.GetNode(textNode)->nextNodeId = end;

    DialoguePlayer player;
    // Resolver returns true for "active" match
    player.SetConditionResolver([](const DialogueCondition& c) -> bool {
        return c.variable == "main_quest" && c.value == "active";
    });
    player.Start(tree);
    ENJIN_EXPECT_TRUE(player.IsActive());
    auto* current = player.GetCurrentNode();
    ENJIN_ASSERT_NOT_NULL(current);
    ENJIN_EXPECT_EQ((int)current->type, (int)DialogueNodeType::Text);
}

ENJIN_TEST(DialogueTree, ConditionResolverFalse) {
    DialogueTreeData tree;
    u32 root = tree.AddNode(DialogueNodeType::Root);
    u32 cond = tree.AddNode(DialogueNodeType::Condition);
    u32 textNode = tree.AddNode(DialogueNodeType::Text);
    u32 end = tree.AddNode(DialogueNodeType::End);
    tree.rootNodeId = root;
    tree.GetNode(root)->nextNodeId = cond;
    tree.GetNode(cond)->condition.source = DialogueCondition::Source::GameFlag;
    tree.GetNode(cond)->condition.variable = "has_key";
    tree.GetNode(cond)->condition.op = DialogueCondition::Op::Equals;
    tree.GetNode(cond)->condition.value = "true";
    tree.GetNode(cond)->trueNodeId = textNode;
    tree.GetNode(cond)->falseNodeId = end;

    DialoguePlayer player;
    // Resolver always returns false
    player.SetConditionResolver([](const DialogueCondition&) -> bool { return false; });
    player.Start(tree);
    // Should take false path -> End -> inactive
    ENJIN_EXPECT_FALSE(player.IsActive());
}

ENJIN_TEST(DialogueTree, ConditionNoResolverFallback) {
    // When no resolver is set, non-Variable conditions should return false
    DialogueTreeData tree;
    u32 root = tree.AddNode(DialogueNodeType::Root);
    u32 cond = tree.AddNode(DialogueNodeType::Condition);
    u32 textNode = tree.AddNode(DialogueNodeType::Text);
    u32 end = tree.AddNode(DialogueNodeType::End);
    tree.rootNodeId = root;
    tree.GetNode(root)->nextNodeId = cond;
    tree.GetNode(cond)->condition.source = DialogueCondition::Source::QuestStatus;
    tree.GetNode(cond)->condition.variable = "test";
    tree.GetNode(cond)->trueNodeId = textNode;
    tree.GetNode(cond)->falseNodeId = end;

    DialoguePlayer player;
    // No resolver set — should fall to false branch
    player.Start(tree);
    ENJIN_EXPECT_FALSE(player.IsActive());  // Ended via false -> End
}

// ===========================================================================
// Serialization of new fields
// ===========================================================================

ENJIN_TEST(DialogueTree, SerializeQuestActionNode) {
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::QuestAction);
    auto* node = tree.GetNode(id);
    node->questId = "fetch_quest";
    node->questAction = DialogueNode::QuestActionType::FailQuest;
    node->questObjectiveIndex = 3;

    auto j = tree.ToJson();
    auto restored = DialogueTreeData::FromJson(j);
    auto* rNode = restored.GetNode(id);
    ENJIN_ASSERT_NOT_NULL(rNode);
    ENJIN_EXPECT_EQ((int)rNode->type, (int)DialogueNodeType::QuestAction);
    ENJIN_EXPECT_STR_EQ(rNode->questId, "fetch_quest");
    ENJIN_EXPECT_EQ((int)rNode->questAction, (int)DialogueNode::QuestActionType::FailQuest);
    ENJIN_EXPECT_EQ(rNode->questObjectiveIndex, 3);
}

ENJIN_TEST(DialogueTree, SerializeConditionSource) {
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::Condition);
    auto* node = tree.GetNode(id);
    node->condition.source = DialogueCondition::Source::GameFlag;
    node->condition.variable = "door_unlocked";
    node->condition.value = "yes";

    auto j = tree.ToJson();
    auto restored = DialogueTreeData::FromJson(j);
    auto* rNode = restored.GetNode(id);
    ENJIN_ASSERT_NOT_NULL(rNode);
    ENJIN_EXPECT_EQ((int)rNode->condition.source, (int)DialogueCondition::Source::GameFlag);
    ENJIN_EXPECT_STR_EQ(rNode->condition.variable, "door_unlocked");
}

ENJIN_TEST(DialogueTree, SerializeBackwardsCompatible) {
    // Old trees without new fields should deserialize with defaults
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::Text);
    auto j = tree.ToJson();
    auto restored = DialogueTreeData::FromJson(j);
    auto* rNode = restored.GetNode(id);
    ENJIN_ASSERT_NOT_NULL(rNode);
    // New fields should have defaults
    ENJIN_EXPECT_TRUE(rNode->questId.empty());
    ENJIN_EXPECT_TRUE(rNode->cinematicEntityName.empty());
    ENJIN_EXPECT_TRUE(rNode->gameFlagKey.empty());
    ENJIN_EXPECT_EQ((int)rNode->condition.source, (int)DialogueCondition::Source::Variable);
}

ENJIN_TEST_MAIN()
