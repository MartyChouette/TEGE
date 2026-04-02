#include "EnjinTest.h"
#include "Enjin/Animation/Animation.h"

using namespace Enjin;
using namespace Enjin::Animation;

// ===========================================================================
// BlendTree::Evaluate
// ===========================================================================

ENJIN_TEST(BlendTree, EmptyNodesReturnsNothing) {
    BlendTree bt;
    std::string a, b;
    f32 blend = 0.0f;
    bt.Evaluate(0.5f, a, b, blend);
    ENJIN_EXPECT_TRUE(a.empty());
    ENJIN_EXPECT_TRUE(b.empty());
}

ENJIN_TEST(BlendTree, SingleNodeReturnsFull) {
    BlendTree bt;
    bt.nodes.push_back({"Walk", 0.0f});
    std::string a, b;
    f32 blend = 0.0f;
    bt.Evaluate(0.5f, a, b, blend);
    ENJIN_EXPECT_STR_EQ(a, "Walk");
    ENJIN_EXPECT_TRUE(b.empty());
}

ENJIN_TEST(BlendTree, BelowMinReturnsFirst) {
    BlendTree bt;
    bt.nodes.push_back({"Idle", 0.0f});
    bt.nodes.push_back({"Walk", 1.0f});
    bt.nodes.push_back({"Run", 2.0f});
    std::string a, b;
    f32 blend = 0.0f;
    bt.Evaluate(-5.0f, a, b, blend);
    ENJIN_EXPECT_STR_EQ(a, "Idle");
    ENJIN_EXPECT_TRUE(b.empty());
}

ENJIN_TEST(BlendTree, AboveMaxReturnsLast) {
    BlendTree bt;
    bt.nodes.push_back({"Idle", 0.0f});
    bt.nodes.push_back({"Walk", 1.0f});
    bt.nodes.push_back({"Run", 2.0f});
    std::string a, b;
    f32 blend = 0.0f;
    bt.Evaluate(10.0f, a, b, blend);
    ENJIN_EXPECT_STR_EQ(a, "Run");
    ENJIN_EXPECT_TRUE(b.empty());
}

ENJIN_TEST(BlendTree, ExactThresholdReturnsSingle) {
    BlendTree bt;
    bt.nodes.push_back({"Idle", 0.0f});
    bt.nodes.push_back({"Walk", 1.0f});
    std::string a, b;
    f32 blend = 0.0f;
    bt.Evaluate(0.0f, a, b, blend);
    ENJIN_EXPECT_STR_EQ(a, "Idle");
    ENJIN_EXPECT_TRUE(b.empty());
}

ENJIN_TEST(BlendTree, MidpointBlendsFiftyFifty) {
    BlendTree bt;
    bt.nodes.push_back({"Idle", 0.0f});
    bt.nodes.push_back({"Walk", 1.0f});
    std::string a, b;
    f32 blend = 0.0f;
    bt.Evaluate(0.5f, a, b, blend);
    ENJIN_EXPECT_STR_EQ(a, "Idle");
    ENJIN_EXPECT_STR_EQ(b, "Walk");
    // Blend factor should be ~0.5 (50% each)
    ENJIN_EXPECT_TRUE(blend > 0.45f && blend < 0.55f);
}

ENJIN_TEST(BlendTree, ThreeNodesMiddleRegion) {
    BlendTree bt;
    bt.nodes.push_back({"Idle", 0.0f});
    bt.nodes.push_back({"Walk", 1.0f});
    bt.nodes.push_back({"Run", 2.0f});
    std::string a, b;
    f32 blend = 0.0f;
    // Between Walk and Run
    bt.Evaluate(1.5f, a, b, blend);
    ENJIN_EXPECT_STR_EQ(a, "Walk");
    ENJIN_EXPECT_STR_EQ(b, "Run");
    ENJIN_EXPECT_TRUE(blend > 0.45f && blend < 0.55f);
}

ENJIN_TEST(BlendTree, NegativeThresholds) {
    BlendTree bt;
    bt.nodes.push_back({"Left", -1.0f});
    bt.nodes.push_back({"Center", 0.0f});
    bt.nodes.push_back({"Right", 1.0f});
    std::string a, b;
    f32 blend = 0.0f;
    bt.Evaluate(-0.5f, a, b, blend);
    ENJIN_EXPECT_STR_EQ(a, "Left");
    ENJIN_EXPECT_STR_EQ(b, "Center");
}

ENJIN_TEST(BlendTree, DefaultDisabled) {
    BlendTree bt;
    ENJIN_EXPECT_FALSE(bt.enabled);
    ENJIN_EXPECT_TRUE(bt.parameterName.empty());
    ENJIN_EXPECT_TRUE(bt.nodes.empty());
}

ENJIN_TEST(BlendTree, BlendNodeDefaults) {
    BlendNode node;
    ENJIN_EXPECT_TRUE(node.animationName.empty());
    ENJIN_EXPECT_EQ(node.threshold, 0.0f);
}

ENJIN_TEST_MAIN()
