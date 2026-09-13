// The hand IK demo, held to its own claim.
//
// Examples/HandIK exists to be looked at in first person: a placeholder hand
// held over a counter, a shelf lip and a railing. The claim it makes by
// existing is that five fingers of different lengths settle at the right
// heights on whatever is under them, and that the ones which cannot reach are
// left alone.
//
// A demo that quietly stopped demonstrating that would be worse than no demo,
// and nobody notices it by glancing at a scene file. So the scene is loaded
// here and the rig's ACTUAL bone positions are pulled out of it and solved
// against the counter's ACTUAL height. If somebody moves the hand, changes a
// finger length or raises the worktop, these numbers move and this says so.
//
// What is not covered: whether it renders. That needs a renderer and a device,
// and it is what opening the project is for.

#include "EnjinTest.h"
#include "Enjin/Animation/HandIK.h"
#include "Enjin/ECS/Components/HandIKComponent.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace Enjin;
using namespace Enjin::Animation;
using Enjin::Math::Vector3;

namespace {

std::string FindScene() {
    const char* candidates[] = {
        "Examples/HandIK/scenes/Main.enjin",
        "../Examples/HandIK/scenes/Main.enjin",
        "../../Examples/HandIK/scenes/Main.enjin",
        "../../../Examples/HandIK/scenes/Main.enjin",
        "../../../../Examples/HandIK/scenes/Main.enjin",
    };
    for (const char* p : candidates) {
        std::ifstream f(p);
        if (f.good()) return p;
    }
    return std::string();
}

bool Load(ECS::World& world) {
    const std::string path = FindScene();
    if (path.empty()) return false;
    Scene::SceneSerializer s(&world);
    std::ifstream f(path);
    std::stringstream b;
    b << f.rdbuf();
    return s.LoadFromString(b.str()).success;
}

ECS::Entity FindByName(ECS::World& world, const char* name) {
    for (ECS::Entity e : world.GetEntitiesWithComponent<ECS::NameComponent>()) {
        const auto* n = world.GetComponent<ECS::NameComponent>(e);
        if (n && n->name == name) return e;
    }
    return ECS::INVALID_ENTITY;
}

// The counter's top face, computed from the scene rather than repeated here, so
// raising the worktop moves the expectation with it.
f32 CounterTopY(ECS::World& world) {
    const ECS::Entity e = FindByName(world, "Counter Top");
    if (e == ECS::INVALID_ENTITY) return 0.0f;
    const auto* t = world.GetComponent<ECS::TransformComponent>(e);
    return t ? (t->position.y + t->scale.y * 0.5f) : 0.0f;
}

// A plane standing in for the counter, so the solve is exact.
class Counter : public ISurfaceQuery {
public:
    explicit Counter(f32 y) : m_Y(y) {}
    bool Cast(const Vector3& o, const Vector3& d, f32 maxDist, SurfaceHit& out) const override {
        if (d.y >= -1.0e-6f) return false;
        const f32 t = (o.y - m_Y) / -d.y;
        if (t < 0.0f || t > maxDist) return false;
        out.hit = true;
        out.distance = t;
        out.point = Vector3(o.x + d.x * t, m_Y, o.z + d.z * t);
        out.normal = Vector3(0.0f, 1.0f, 0.0f);
        return true;
    }
private:
    f32 m_Y;
};

} // namespace

ENJIN_TEST(HandIKDemo, TheSceneLoadsWithARiggedHandInIt) {
    ECS::World world;
    if (!Load(world)) { ENJIN_SKIP("Examples/HandIK/scenes/Main.enjin not found"); return; }

    const ECS::Entity hand = FindByName(world, "RightHand");
    ENJIN_ASSERT_TRUE(hand != ECS::INVALID_ENTITY);

    const auto* skel = world.GetComponent<ECS::SkeletonComponent>(hand);
    ENJIN_ASSERT_TRUE(skel != nullptr && skel->skeleton != nullptr);
    std::printf("    %zu bones\n", skel->skeleton->bones.size());

    // Wrist plus five fingers of three bones and a tip.
    ENJIN_EXPECT_EQ(skel->skeleton->bones.size(), usize{21});

    const auto* ik = world.GetComponent<ECS::HandIKComponent>(hand);
    ENJIN_ASSERT_TRUE(ik != nullptr);

    // Every bone the component names has to exist in the skeleton. A typo here
    // does not throw: the finger is skipped and the hand simply never conforms,
    // which looks exactly like the feature being off.
    ENJIN_EXPECT_TRUE(skel->skeleton->FindBoneIndex(ik->handBoneName) >= 0);
    for (u32 f = 0; f < kFingerCount; ++f) {
        const auto& fb = ik->fingers[f];
        ENJIN_ASSERT_TRUE(fb.IsSet());
        ENJIN_EXPECT_TRUE(skel->skeleton->FindBoneIndex(fb.proximal) >= 0);
        ENJIN_EXPECT_TRUE(skel->skeleton->FindBoneIndex(fb.intermediate) >= 0);
        ENJIN_EXPECT_TRUE(skel->skeleton->FindBoneIndex(fb.distal) >= 0);
        ENJIN_EXPECT_TRUE(skel->skeleton->FindBoneIndex(fb.tip) >= 0);
    }
}

ENJIN_TEST(HandIKDemo, TheRigsOwnFingersLandOnTheCountersOwnHeight) {
    ECS::World world;
    if (!Load(world)) { ENJIN_SKIP("demo scene not found"); return; }

    const ECS::Entity handEnt = FindByName(world, "RightHand");
    ENJIN_ASSERT_TRUE(handEnt != ECS::INVALID_ENTITY);
    const auto* skel = world.GetComponent<ECS::SkeletonComponent>(handEnt);
    const auto* ik = world.GetComponent<ECS::HandIKComponent>(handEnt);
    ENJIN_ASSERT_TRUE(skel && skel->skeleton && ik);

    // Where the hand actually is: player + camera + hand, read from the scene.
    const auto* playerT = world.GetComponent<ECS::TransformComponent>(FindByName(world, "Player"));
    const auto* camT = world.GetComponent<ECS::TransformComponent>(FindByName(world, "MainCam"));
    const auto* handT = world.GetComponent<ECS::TransformComponent>(handEnt);
    ENJIN_ASSERT_TRUE(playerT && camT && handT);

    // The eye is NOT player.y + the camera child's offset.
    //
    // FirstPersonController owns the camera: it overwrites that transform every
    // frame with player.position.y + currentHeight, which follows
    // standingHeight. Modelling it the other way is what made an earlier version
    // of this test agree with a scene the runtime did not produce -- the numbers
    // happened to match while the camera offset happened to equal the standing
    // height, and stopped matching the moment either moved.
    const auto* fp = world.GetComponent<ECS::FirstPersonController>(FindByName(world, "Player"));
    ENJIN_ASSERT_TRUE(fp != nullptr);
    const f32 eyeY = playerT->position.y + fp->standingHeight;

    // The hand hangs off the PLAYER, not the camera, so its local Y is measured
    // from the capsule centre. It is parented that way because a child of the
    // camera entity does not render correctly in that camera's view -- the
    // controller rewrites the camera transform every frame without invalidating
    // its children's cached world matrices. See the note in build_scene.py.
    const f32 handY = playerT->position.y + handT->position.y;
    std::printf("    capsule %.2f + standing %.2f = eye %.2f\n",
                playerT->position.y, fp->standingHeight, eyeY);
    (void)camT;
    const f32 counterY = CounterTopY(world);
    const f32 gap = handY - counterY;

    std::printf("    hand at y %.3f, counter top at y %.3f, gap %.3f\n",
                handY, counterY, gap);

    // Build the pose from the rig's bind positions, which for this skeleton are
    // cumulative local offsets with no rotation.
    HandPose pose;
    pose.palmPosition = Vector3(0.0f, handY, 0.0f);
    pose.palmNormal = Vector3(0.0f, -1.0f, 0.0f);

    auto worldOf = [&](const std::string& boneName) {
        Vector3 acc(0.0f, 0.0f, 0.0f);
        i32 idx = skel->skeleton->FindBoneIndex(boneName);
        while (idx >= 0) {
            const auto& b = skel->skeleton->bones[static_cast<usize>(idx)];
            acc = Vector3(acc.x + b.bindPosition.x,
                          acc.y + b.bindPosition.y,
                          acc.z + b.bindPosition.z);
            idx = b.parentIndex;
        }
        return Vector3(acc.x, handY + acc.y, acc.z);
    };

    for (u32 f = 0; f < kFingerCount; ++f) {
        const auto& fb = ik->fingers[f];
        FingerChain& chain = pose.fingers[f];
        chain.joints[0] = worldOf(fb.proximal);
        chain.joints[1] = worldOf(fb.intermediate);
        chain.joints[2] = worldOf(fb.distal);
        chain.joints[3] = worldOf(fb.tip);
        chain.curlDirection = Vector3(0.0f, -1.0f, 0.0f);
        chain.approachWeight = 1.0f;
    }

    const Counter counter(counterY);
    const HandIKResult r = HandIK::SolveSurface(pose, counter);

    static const char* kNames[] = { "Thumb", "Index", "Middle", "Ring", "Little" };
    u32 reached = 0;
    for (u32 f = 0; f < kFingerCount; ++f) {
        const FingerChain& c = pose.fingers[f];
        const f32 reach = c.Reach();
        const f32 tipY = c.joints[kFingerJoints].y;
        std::printf("    %-7s reach %.3f  tip y %.4f  %s\n",
                    kNames[f], reach, tipY, c.contacted ? "contact" : "-");

        if (c.contacted) {
            ++reached;
            // ON the surface. In first person a fingertip a centimetre off the
            // worktop is the thing you notice first.
            ENJIN_EXPECT_FLOAT_NEAR(tipY, counterY, 0.003f);
        } else {
            // Too short. Left where it was, not stretched down.
            ENJIN_EXPECT_TRUE(tipY > counterY);
        }
    }

    // The point of the whole exercise: the gap sits INSIDE the spread of finger
    // lengths, so some reach and some do not. A hand parked where all five
    // reach would look the same whether the solve were per-finger or one canned
    // pose, and would prove nothing.
    std::printf("    %u of 5 reached\n", r.fingersContacted);
    ENJIN_EXPECT_EQ(reached, r.fingersContacted);
    ENJIN_EXPECT_TRUE(r.fingersContacted >= 2);
    ENJIN_EXPECT_TRUE(r.fingersContacted <= 4);
}

ENJIN_TEST(HandIKDemo, ThereIsAGapInTheCounterToProveTheNegative) {
    ECS::World world;
    if (!Load(world)) { ENJIN_SKIP("demo scene not found"); return; }

    // Two worktops with clear air between them. Walking across it is the only
    // part of the scene that tests a NEGATIVE: over nothing, the fingers must
    // hang where they are rather than snapping to a surface that is not there.
    const ECS::Entity a = FindByName(world, "Counter Top");
    const ECS::Entity b = FindByName(world, "Counter Top 2");
    ENJIN_ASSERT_TRUE(a != ECS::INVALID_ENTITY && b != ECS::INVALID_ENTITY);

    const auto* ta = world.GetComponent<ECS::TransformComponent>(a);
    const auto* tb = world.GetComponent<ECS::TransformComponent>(b);
    ENJIN_ASSERT_TRUE(ta && tb);

    const f32 endOfA = ta->position.x + ta->scale.x * 0.5f;
    const f32 startOfB = tb->position.x - tb->scale.x * 0.5f;
    std::printf("    counter runs out at x %.2f, resumes at x %.2f (gap %.2f m)\n",
                endOfA, startOfB, startOfB - endOfA);

    // Wide enough to walk into and notice, not so wide the hand is over air for
    // most of the scene.
    ENJIN_EXPECT_TRUE(startOfB - endOfA > 0.8f);
}

ENJIN_TEST(HandIKDemo, TheShelfLipHasFreeAirUnderItSoFingersCanCurlOver) {
    ECS::World world;
    if (!Load(world)) { ENJIN_SKIP("demo scene not found"); return; }

    const ECS::Entity plank = FindByName(world, "Shelf Plank");
    ENJIN_ASSERT_TRUE(plank != ECS::INVALID_ENTITY);
    const auto* t = world.GetComponent<ECS::TransformComponent>(plank);
    ENJIN_ASSERT_TRUE(t != nullptr);

    // The edge case needs an EDGE: something for the tips to pass and drop
    // behind. A plank flush against a wall is a surface, not a lip, and the
    // curl would have nowhere to go.
    std::printf("    shelf at y %.2f, %.2f deep, top at %.2f\n",
                t->position.y, t->scale.z, t->position.y + t->scale.y * 0.5f);
    ENJIN_EXPECT_TRUE(t->scale.z < 0.6f);
    ENJIN_EXPECT_TRUE(t->position.y > 1.0f);
}

ENJIN_TEST_MAIN()
