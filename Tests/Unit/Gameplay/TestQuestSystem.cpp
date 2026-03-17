#include "EnjinTest.h"
#include "Enjin/Gameplay/QuestFlow.h"

#include <string>
#include <set>

using namespace Enjin;
using namespace Enjin::Gameplay;

// ===========================================================================
// QuestFlowStatus Enum
// ===========================================================================

ENJIN_TEST(QuestFlowStatus, EnumValues) {
    ENJIN_EXPECT_EQ((int)QuestFlowStatus::Inactive, 0);
    ENJIN_EXPECT_EQ((int)QuestFlowStatus::Active, 1);
    ENJIN_EXPECT_EQ((int)QuestFlowStatus::Completed, 2);
    ENJIN_EXPECT_EQ((int)QuestFlowStatus::Failed, 3);
}

ENJIN_TEST(QuestFlowStatus, ToStringValues) {
    ENJIN_EXPECT_STR_EQ(QuestFlowStatusToString(QuestFlowStatus::Inactive), "Inactive");
    ENJIN_EXPECT_STR_EQ(QuestFlowStatusToString(QuestFlowStatus::Active), "Active");
    ENJIN_EXPECT_STR_EQ(QuestFlowStatusToString(QuestFlowStatus::Completed), "Completed");
    ENJIN_EXPECT_STR_EQ(QuestFlowStatusToString(QuestFlowStatus::Failed), "Failed");
}

// ===========================================================================
// QuestNodeType Enum
// ===========================================================================

ENJIN_TEST(QuestNodeType, EnumCount) {
    ENJIN_EXPECT_EQ((int)QuestNodeType::COUNT, 8);
}

ENJIN_TEST(QuestNodeType, SequentialValues) {
    ENJIN_EXPECT_EQ((int)QuestNodeType::Start, 0);
    ENJIN_EXPECT_EQ((int)QuestNodeType::Objective, 1);
    ENJIN_EXPECT_EQ((int)QuestNodeType::Delay, 2);
    ENJIN_EXPECT_EQ((int)QuestNodeType::Event, 3);
    ENJIN_EXPECT_EQ((int)QuestNodeType::Condition, 4);
    ENJIN_EXPECT_EQ((int)QuestNodeType::Branch, 5);
    ENJIN_EXPECT_EQ((int)QuestNodeType::Reward, 6);
    ENJIN_EXPECT_EQ((int)QuestNodeType::End, 7);
}

ENJIN_TEST(QuestNodeType, ToStringNonNull) {
    for (int i = 0; i < (int)QuestNodeType::COUNT; ++i) {
        const char* name = QuestNodeTypeToString((QuestNodeType)i);
        ENJIN_EXPECT_NOT_NULL(name);
        ENJIN_EXPECT_GT(strlen(name), (size_t)0);
    }
}

// ===========================================================================
// QuestNodeCategory Mapping
// ===========================================================================

ENJIN_TEST(NodeCategory, StartIsEntry) {
    ENJIN_EXPECT_EQ((int)GetQuestNodeCategory(QuestNodeType::Start), (int)QuestNodeCategory::Entry);
}

ENJIN_TEST(NodeCategory, FlowNodes) {
    ENJIN_EXPECT_EQ((int)GetQuestNodeCategory(QuestNodeType::Objective), (int)QuestNodeCategory::Flow);
    ENJIN_EXPECT_EQ((int)GetQuestNodeCategory(QuestNodeType::Delay), (int)QuestNodeCategory::Flow);
    ENJIN_EXPECT_EQ((int)GetQuestNodeCategory(QuestNodeType::Event), (int)QuestNodeCategory::Flow);
}

ENJIN_TEST(NodeCategory, CheckNodes) {
    ENJIN_EXPECT_EQ((int)GetQuestNodeCategory(QuestNodeType::Condition), (int)QuestNodeCategory::Check);
    ENJIN_EXPECT_EQ((int)GetQuestNodeCategory(QuestNodeType::Branch), (int)QuestNodeCategory::Check);
}

ENJIN_TEST(NodeCategory, GrantNode) {
    ENJIN_EXPECT_EQ((int)GetQuestNodeCategory(QuestNodeType::Reward), (int)QuestNodeCategory::Grant);
}

ENJIN_TEST(NodeCategory, EndIsTerminal) {
    ENJIN_EXPECT_EQ((int)GetQuestNodeCategory(QuestNodeType::End), (int)QuestNodeCategory::Terminal);
}

ENJIN_TEST(NodeCategory, CategoryToStringNonNull) {
    const char* names[] = {
        QuestNodeCategoryToString(QuestNodeCategory::Entry),
        QuestNodeCategoryToString(QuestNodeCategory::Flow),
        QuestNodeCategoryToString(QuestNodeCategory::Check),
        QuestNodeCategoryToString(QuestNodeCategory::Grant),
        QuestNodeCategoryToString(QuestNodeCategory::Terminal)
    };
    for (int i = 0; i < 5; ++i) {
        ENJIN_EXPECT_NOT_NULL(names[i]);
    }
}

// ===========================================================================
// QuestNodeMeta Defaults
// ===========================================================================

ENJIN_TEST(QuestNodeMeta, DefaultValues) {
    QuestNodeMeta meta;
    ENJIN_EXPECT_EQ((int)meta.nodeType, (int)QuestNodeType::Start);
    ENJIN_EXPECT_FALSE(meta.reached);
    ENJIN_EXPECT_EQ(meta.properties.size(), (size_t)0);
}

ENJIN_TEST(QuestNodeMeta, PropertyStorage) {
    QuestNodeMeta meta;
    meta.properties["description"] = "Find the sword";
    meta.properties["reward"] = "100 gold";
    ENJIN_EXPECT_EQ(meta.properties.size(), (size_t)2);
    ENJIN_EXPECT_STR_EQ(meta.properties["description"].c_str(), "Find the sword");
}

// ===========================================================================
// QuestFlowComponent Defaults & Reset
// ===========================================================================

ENJIN_TEST(QuestFlowComponent, DefaultValues) {
    ECS::QuestFlowComponent comp;
    ENJIN_EXPECT_TRUE(comp.questId.empty());
    ENJIN_EXPECT_TRUE(comp.questTitle.empty());
    ENJIN_EXPECT_TRUE(comp.enabled);
    ENJIN_EXPECT_EQ((int)comp.status, (int)QuestFlowStatus::Inactive);
    ENJIN_EXPECT_EQ(comp.activeNodes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(comp.completedNodes.size(), (size_t)0);
}

ENJIN_TEST(QuestFlowComponent, ResetRuntimeState) {
    ECS::QuestFlowComponent comp;
    comp.status = QuestFlowStatus::Active;
    comp.activeNodes.insert(1);
    comp.activeNodes.insert(2);
    comp.completedNodes.insert(3);
    comp.nodeTimers[1] = 5.0f;
    comp.nodeCounters[2] = 3;

    comp.ResetRuntimeState();

    ENJIN_EXPECT_EQ((int)comp.status, (int)QuestFlowStatus::Inactive);
    ENJIN_EXPECT_EQ(comp.activeNodes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(comp.completedNodes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(comp.nodeTimers.size(), (size_t)0);
    ENJIN_EXPECT_EQ(comp.nodeCounters.size(), (size_t)0);
}

ENJIN_TEST(QuestFlowComponent, ResetPreservesConfig) {
    ECS::QuestFlowComponent comp;
    comp.questId = "main_quest";
    comp.questTitle = "The Great Adventure";
    comp.enabled = true;

    comp.status = QuestFlowStatus::Completed;
    comp.ResetRuntimeState();

    // Config fields should be preserved
    ENJIN_EXPECT_STR_EQ(comp.questId.c_str(), "main_quest");
    ENJIN_EXPECT_STR_EQ(comp.questTitle.c_str(), "The Great Adventure");
    ENJIN_EXPECT_TRUE(comp.enabled);
}

ENJIN_TEST_MAIN()
