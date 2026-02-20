#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/GUI/DialogueTree.h"
#include <nlohmann/json.hpp>

using namespace Enjin;
using namespace Enjin::GUI;

// ============================================================================
// DialogueTreeData
// ============================================================================

ENJIN_TEST(DialogueTreeData, AddNodeIncrementsId) {
    DialogueTreeData tree;
    tree.treeName = "Test";

    u32 id1 = tree.AddNode(DialogueNodeType::Text);
    u32 id2 = tree.AddNode(DialogueNodeType::Choice);
    ENJIN_EXPECT_EQ(id1, 1u);
    ENJIN_EXPECT_EQ(id2, 2u);
    ENJIN_EXPECT_EQ(tree.nodes.size(), (usize)2);
}

ENJIN_TEST(DialogueTreeData, GetNodeReturnsCorrect) {
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::Text);
    auto* node = tree.GetNode(id);
    ENJIN_ASSERT_NOT_NULL(node);
    ENJIN_EXPECT_EQ(node->id, id);
    ENJIN_EXPECT_EQ((int)node->type, (int)DialogueNodeType::Text);
}

ENJIN_TEST(DialogueTreeData, GetNodeInvalidReturnsNull) {
    DialogueTreeData tree;
    auto* node = tree.GetNode(999);
    ENJIN_EXPECT_NULL(node);
}

ENJIN_TEST(DialogueTreeData, RemoveNode) {
    DialogueTreeData tree;
    u32 id = tree.AddNode(DialogueNodeType::Text);
    ENJIN_EXPECT_EQ(tree.nodes.size(), (usize)1);

    tree.RemoveNode(id);
    ENJIN_EXPECT_EQ(tree.nodes.size(), (usize)0);
    ENJIN_EXPECT_NULL(tree.GetNode(id));
}

// ============================================================================
// JSON Round-Trip
// ============================================================================

ENJIN_TEST(DialogueTreeData, JsonRoundTrip) {
    DialogueTreeData tree;
    tree.treeName = "MyTree";
    tree.rootNodeId = 1;

    u32 textId = tree.AddNode(DialogueNodeType::Text);
    auto* textNode = tree.GetNode(textId);
    textNode->speakerName = "Alice";
    textNode->text = "Hello, world!";
    textNode->nextNodeId = 2;

    u32 choiceId = tree.AddNode(DialogueNodeType::Choice);
    auto* choiceNode = tree.GetNode(choiceId);
    choiceNode->text = "Choose:";
    DialogueChoice c1;
    c1.text = "Option A";
    c1.targetNodeId = 0;
    choiceNode->choices.push_back(c1);

    auto json = tree.ToJson();
    auto loaded = DialogueTreeData::FromJson(json);

    ENJIN_EXPECT_STR_EQ(loaded.treeName, "MyTree");
    ENJIN_EXPECT_EQ(loaded.rootNodeId, 1u);
    ENJIN_EXPECT_EQ(loaded.nodes.size(), (usize)2);

    auto* lt = loaded.GetNode(textId);
    ENJIN_ASSERT_NOT_NULL(lt);
    ENJIN_EXPECT_STR_EQ(lt->speakerName, "Alice");
    ENJIN_EXPECT_STR_EQ(lt->text, "Hello, world!");
    ENJIN_EXPECT_EQ(lt->nextNodeId, 2u);

    auto* lc = loaded.GetNode(choiceId);
    ENJIN_ASSERT_NOT_NULL(lc);
    ENJIN_EXPECT_EQ(lc->choices.size(), (usize)1);
    ENJIN_EXPECT_STR_EQ(lc->choices[0].text, "Option A");
}

// ============================================================================
// DialoguePlayer — Basic Flow
// ============================================================================

ENJIN_TEST(DialoguePlayer, StartActivatesPlayer) {
    DialogueTreeData tree;
    tree.rootNodeId = 1;
    u32 id = tree.AddNode(DialogueNodeType::Text);
    auto* node = tree.GetNode(id);
    node->text = "Hello";

    DialoguePlayer player;
    ENJIN_EXPECT_FALSE(player.IsActive());

    player.Start(tree);
    ENJIN_EXPECT_TRUE(player.IsActive());
}

ENJIN_TEST(DialoguePlayer, TextNodeWaitsForInput) {
    DialogueTreeData tree;
    tree.rootNodeId = 1;
    u32 id = tree.AddNode(DialogueNodeType::Text);
    auto* node = tree.GetNode(id);
    node->text = "Hello";

    DialoguePlayer player;
    player.Start(tree);

    ENJIN_EXPECT_TRUE(player.IsWaitingForInput());
    auto* current = player.GetCurrentNode();
    ENJIN_ASSERT_NOT_NULL(current);
    ENJIN_EXPECT_STR_EQ(current->text, "Hello");
}

ENJIN_TEST(DialoguePlayer, AdvanceThroughTextNodes) {
    DialogueTreeData tree;
    tree.rootNodeId = 1;

    u32 id1 = tree.AddNode(DialogueNodeType::Text);
    u32 id2 = tree.AddNode(DialogueNodeType::Text);
    u32 id3 = tree.AddNode(DialogueNodeType::End);

    tree.GetNode(id1)->text = "First";
    tree.GetNode(id1)->nextNodeId = id2;
    tree.GetNode(id2)->text = "Second";
    tree.GetNode(id2)->nextNodeId = id3;

    DialoguePlayer player;
    player.Start(tree);

    // First node
    ENJIN_EXPECT_STR_EQ(player.GetCurrentNode()->text, "First");
    player.Advance();

    // Second node
    ENJIN_EXPECT_STR_EQ(player.GetCurrentNode()->text, "Second");
    player.Advance();

    // Ended
    ENJIN_EXPECT_FALSE(player.IsActive());
}

// ============================================================================
// DialoguePlayer — Choices
// ============================================================================

ENJIN_TEST(DialoguePlayer, ChoiceNodePresentsOptions) {
    DialogueTreeData tree;
    tree.rootNodeId = 1;

    u32 choiceId = tree.AddNode(DialogueNodeType::Choice);
    u32 resultA = tree.AddNode(DialogueNodeType::Text);
    u32 resultB = tree.AddNode(DialogueNodeType::Text);
    u32 endId = tree.AddNode(DialogueNodeType::End);

    auto* choiceNode = tree.GetNode(choiceId);
    choiceNode->text = "Pick one:";
    DialogueChoice a, b;
    a.text = "Alpha";
    a.targetNodeId = resultA;
    b.text = "Beta";
    b.targetNodeId = resultB;
    choiceNode->choices = {a, b};

    tree.GetNode(resultA)->text = "You chose Alpha";
    tree.GetNode(resultA)->nextNodeId = endId;
    tree.GetNode(resultB)->text = "You chose Beta";
    tree.GetNode(resultB)->nextNodeId = endId;

    DialoguePlayer player;
    player.Start(tree);

    auto choices = player.GetAvailableChoices();
    ENJIN_EXPECT_EQ(choices.size(), (usize)2);
    ENJIN_EXPECT_STR_EQ(choices[0].text, "Alpha");
    ENJIN_EXPECT_STR_EQ(choices[1].text, "Beta");
}

ENJIN_TEST(DialoguePlayer, SelectChoiceNavigates) {
    DialogueTreeData tree;
    tree.rootNodeId = 1;

    u32 choiceId = tree.AddNode(DialogueNodeType::Choice);
    u32 resultA = tree.AddNode(DialogueNodeType::Text);
    u32 endId = tree.AddNode(DialogueNodeType::End);

    auto* choiceNode = tree.GetNode(choiceId);
    DialogueChoice a;
    a.text = "Alpha";
    a.targetNodeId = resultA;
    choiceNode->choices = {a};

    tree.GetNode(resultA)->text = "You chose Alpha";
    tree.GetNode(resultA)->nextNodeId = endId;

    DialoguePlayer player;
    player.Start(tree);
    player.SelectChoice(0);

    ENJIN_EXPECT_TRUE(player.IsActive());
    ENJIN_EXPECT_STR_EQ(player.GetCurrentNode()->text, "You chose Alpha");
}

// ============================================================================
// DialoguePlayer — Variables
// ============================================================================

ENJIN_TEST(DialoguePlayer, SetAndGetVariable) {
    DialoguePlayer player;
    player.SetVariable("health", "100");
    ENJIN_EXPECT_STR_EQ(player.GetVariable("health"), "100");
}

ENJIN_TEST(DialoguePlayer, SetVariableNode) {
    DialogueTreeData tree;
    tree.rootNodeId = 1;

    u32 setVarId = tree.AddNode(DialogueNodeType::SetVariable);
    u32 textId = tree.AddNode(DialogueNodeType::Text);
    u32 endId = tree.AddNode(DialogueNodeType::End);

    auto* setNode = tree.GetNode(setVarId);
    setNode->variableName = "mood";
    setNode->variableValue = "happy";
    setNode->nextNodeId = textId;

    tree.GetNode(textId)->text = "Done";
    tree.GetNode(textId)->nextNodeId = endId;

    DialoguePlayer player;
    player.Start(tree);

    // SetVariable should auto-advance, so we should be at the text node
    ENJIN_EXPECT_TRUE(player.IsActive());
    ENJIN_EXPECT_STR_EQ(player.GetVariable("mood"), "happy");
}

// ============================================================================
// DialoguePlayer — Conditions
// ============================================================================

ENJIN_TEST(DialoguePlayer, ConditionBranches) {
    // SetVariable node sets the flag, then condition checks it.
    // Start() clears variables, so we use SetVariable nodes to set state.
    DialogueTreeData tree;
    tree.rootNodeId = 1;

    u32 setVarId = tree.AddNode(DialogueNodeType::SetVariable);
    u32 condId = tree.AddNode(DialogueNodeType::Condition);
    u32 trueId = tree.AddNode(DialogueNodeType::Text);
    u32 falseId = tree.AddNode(DialogueNodeType::Text);
    u32 endId = tree.AddNode(DialogueNodeType::End);

    // Set "flag" = "yes" via a SetVariable node
    auto* setNode = tree.GetNode(setVarId);
    setNode->variableName = "flag";
    setNode->variableValue = "yes";
    setNode->nextNodeId = condId;

    auto* condNode = tree.GetNode(condId);
    condNode->condition.variable = "flag";
    condNode->condition.op = DialogueCondition::Op::Equals;
    condNode->condition.value = "yes";
    condNode->trueNodeId = trueId;
    condNode->falseNodeId = falseId;

    tree.GetNode(trueId)->text = "Flag was yes";
    tree.GetNode(trueId)->nextNodeId = endId;
    tree.GetNode(falseId)->text = "Flag was not yes";
    tree.GetNode(falseId)->nextNodeId = endId;

    // Test true branch: SetVariable("flag","yes") -> Condition(flag==yes) -> true
    {
        DialoguePlayer player;
        player.Start(tree);
        ENJIN_ASSERT_NOT_NULL(player.GetCurrentNode());
        ENJIN_EXPECT_STR_EQ(player.GetCurrentNode()->text, "Flag was yes");
    }

    // Test false branch: change the SetVariable value to "no"
    tree.GetNode(setVarId)->variableValue = "no";
    {
        DialoguePlayer player;
        player.Start(tree);
        ENJIN_ASSERT_NOT_NULL(player.GetCurrentNode());
        ENJIN_EXPECT_STR_EQ(player.GetCurrentNode()->text, "Flag was not yes");
    }
}

// ============================================================================
// DialoguePlayer — Events
// ============================================================================

ENJIN_TEST(DialoguePlayer, EventCallbackFired) {
    DialogueTreeData tree;
    tree.rootNodeId = 1;

    u32 eventId = tree.AddNode(DialogueNodeType::Event);
    u32 textId = tree.AddNode(DialogueNodeType::Text);
    u32 endId = tree.AddNode(DialogueNodeType::End);

    auto* eventNode = tree.GetNode(eventId);
    eventNode->eventName = "door_open";
    eventNode->nextNodeId = textId;

    tree.GetNode(textId)->text = "After event";
    tree.GetNode(textId)->nextNodeId = endId;

    std::string receivedEvent;
    DialoguePlayer player;
    player.SetEventCallback([&](const std::string& name) {
        receivedEvent = name;
    });
    player.Start(tree);

    ENJIN_EXPECT_STR_EQ(receivedEvent, "door_open");
    ENJIN_EXPECT_TRUE(player.IsActive());
}

ENJIN_TEST_MAIN()
