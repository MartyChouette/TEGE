// Exporting a dialogue tree must not lose the far side of it.
//
// Both exporters switch on the node type with no default. QuestAction,
// PlayCinematic and SetGameFlag were added to the enum and never added to either
// switch, so those nodes wrote nothing — and writing nothing also meant never
// advancing to the next node, so everything downstream of one of them vanished
// from the file. A tree with a quest node in the middle exported its first half
// and silently dropped the rest.
//
// The assertion that matters in each test below is that text AFTER the untyped
// node still appears. The node writing something of its own is secondary.
#include "EnjinTest.h"
#include "Enjin/GUI/DialogueTree.h"
#include "Enjin/GUI/DialogueImportExport.h"

#include <string>

using namespace Enjin;
using namespace Enjin::GUI;

namespace {

bool Has(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

// A -> <node under test> -> B, so a break in the middle is visible as B missing.
DialogueTreeData TreeWithMiddleNode(DialogueNodeType middleType,
                                    DialogueNode** outMiddle) {
    DialogueTreeData tree;
    const u32 first = tree.AddNode(DialogueNodeType::Text);
    const u32 middle = tree.AddNode(middleType);
    const u32 last = tree.AddNode(DialogueNodeType::Text);

    tree.rootNodeId = first;
    tree.GetNode(first)->text = "BEFORE_THE_NODE";
    tree.GetNode(first)->nextNodeId = middle;
    tree.GetNode(middle)->nextNodeId = last;
    tree.GetNode(last)->text = "AFTER_THE_NODE";

    if (outMiddle) *outMiddle = tree.GetNode(middle);
    return tree;
}

} // namespace

// --- Yarn -------------------------------------------------------------------

ENJIN_TEST(DialogueExport, YarnKeepsTextAfterAQuestNode) {
    // Arrange
    DialogueNode* quest = nullptr;
    DialogueTreeData tree = TreeWithMiddleNode(DialogueNodeType::QuestAction, &quest);
    ENJIN_ASSERT_TRUE(quest != nullptr);
    quest->questAction = DialogueNode::QuestActionType::StartQuest;
    quest->questId = "find_the_lantern";

    // Act
    const std::string out = YarnSpinnerIO::ExportToString(tree);

    // Assert: both sides of the quest node survive.
    ENJIN_EXPECT_TRUE(Has(out, "BEFORE_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "AFTER_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "find_the_lantern"));
}

ENJIN_TEST(DialogueExport, YarnKeepsTextAfterACinematicNode) {
    // Arrange
    DialogueNode* cine = nullptr;
    DialogueTreeData tree = TreeWithMiddleNode(DialogueNodeType::PlayCinematic, &cine);
    ENJIN_ASSERT_TRUE(cine != nullptr);
    cine->cinematicEntityName = "TowerReveal";

    // Act
    const std::string out = YarnSpinnerIO::ExportToString(tree);

    // Assert
    ENJIN_EXPECT_TRUE(Has(out, "BEFORE_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "AFTER_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "TowerReveal"));
}

ENJIN_TEST(DialogueExport, YarnKeepsTextAfterAGameFlagNode) {
    // Arrange
    DialogueNode* flag = nullptr;
    DialogueTreeData tree = TreeWithMiddleNode(DialogueNodeType::SetGameFlag, &flag);
    ENJIN_ASSERT_TRUE(flag != nullptr);
    flag->gameFlagKey = "met_the_smith";
    flag->gameFlagValue = "true";

    // Act
    const std::string out = YarnSpinnerIO::ExportToString(tree);

    // Assert
    ENJIN_EXPECT_TRUE(Has(out, "BEFORE_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "AFTER_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "met_the_smith"));
}

// --- Twine ------------------------------------------------------------------

ENJIN_TEST(DialogueExport, TwineKeepsTextAfterAQuestNode) {
    // Arrange
    DialogueNode* quest = nullptr;
    DialogueTreeData tree = TreeWithMiddleNode(DialogueNodeType::QuestAction, &quest);
    ENJIN_ASSERT_TRUE(quest != nullptr);
    quest->questAction = DialogueNode::QuestActionType::FailQuest;
    quest->questId = "save_the_dog";

    // Act
    const std::string out = TwineIO::ExportToString(tree);

    // Assert
    ENJIN_EXPECT_TRUE(Has(out, "BEFORE_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "AFTER_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "save_the_dog"));
}

ENJIN_TEST(DialogueExport, TwineKeepsTextAfterACinematicNode) {
    // Arrange
    DialogueNode* cine = nullptr;
    DialogueTreeData tree = TreeWithMiddleNode(DialogueNodeType::PlayCinematic, &cine);
    ENJIN_ASSERT_TRUE(cine != nullptr);
    cine->cinematicEntityName = "BridgeCollapse";

    // Act
    const std::string out = TwineIO::ExportToString(tree);

    // Assert
    ENJIN_EXPECT_TRUE(Has(out, "BEFORE_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "AFTER_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "BridgeCollapse"));
}

ENJIN_TEST(DialogueExport, TwineKeepsTextAfterAGameFlagNode) {
    // Arrange
    DialogueNode* flag = nullptr;
    DialogueTreeData tree = TreeWithMiddleNode(DialogueNodeType::SetGameFlag, &flag);
    ENJIN_ASSERT_TRUE(flag != nullptr);
    flag->gameFlagKey = "lantern_lit";
    flag->gameFlagValue = "1";

    // Act
    const std::string out = TwineIO::ExportToString(tree);

    // Assert
    ENJIN_EXPECT_TRUE(Has(out, "BEFORE_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "AFTER_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(out, "lantern_lit"));
}

// --- The ordinary path still works ------------------------------------------

ENJIN_TEST(DialogueExport, APlainChainStillExportsEndToEnd) {
    // Arrange: no exotic node types at all, so a regression in the walk itself
    // shows up here rather than only in the cases above.
    DialogueTreeData tree;
    const u32 a = tree.AddNode(DialogueNodeType::Text);
    const u32 b = tree.AddNode(DialogueNodeType::Text);
    tree.rootNodeId = a;
    tree.GetNode(a)->speakerName = "Smith";
    tree.GetNode(a)->text = "BEFORE_THE_NODE";
    tree.GetNode(a)->nextNodeId = b;
    tree.GetNode(b)->text = "AFTER_THE_NODE";

    // Act
    const std::string yarn = YarnSpinnerIO::ExportToString(tree);
    const std::string twine = TwineIO::ExportToString(tree);

    // Assert
    ENJIN_EXPECT_TRUE(Has(yarn, "Smith"));
    ENJIN_EXPECT_TRUE(Has(yarn, "BEFORE_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(yarn, "AFTER_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(twine, "BEFORE_THE_NODE"));
    ENJIN_EXPECT_TRUE(Has(twine, "AFTER_THE_NODE"));
}

ENJIN_TEST_MAIN()
