// Will the hand rest on things OTHER than the counter?
//
// Marty, 2026-09-13: "i want the hand to collide and rest on other things, do
// they need rails or bones or what". The answer should be "nothing, they need a
// collider", because the runtime query is a physics raycast fired per finger.
// This asserts that instead of asserting it in a sentence: it loads the demo
// scene's REAL colliders into a REAL Jolt world and holds the rig's REAL bone
// positions over each prop in turn.
//
// TestHandIKDemo solves against an analytic plane, which is what pins the
// solver to the millimetre. This one exists to catch the other half: a prop
// that renders and has no collider is invisible to the hand, and that failure
// looks exactly like the IK being broken.

#include "EnjinTest.h"
#include "Enjin/Animation/HandIK.h"
#include "Enjin/Animation/PhysicsSurfaceQuery.h"
#include "Enjin/ECS/Components/HandIKComponent.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <fstream>
#include <memory>
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

ECS::Entity FindByName(ECS::World& world, const std::string& name) {
    for (ECS::Entity e : world.GetEntitiesWithComponent<ECS::NameComponent>()) {
        auto* n = world.GetComponent<ECS::NameComponent>(e);
        if (n && n->name == name) return e;
    }
    return ECS::INVALID_ENTITY;
}

// The rig's own finger geometry, lifted out of the scene and parked at a chosen
// palm position. Same construction TestHandIKDemo uses.
struct Rig {
    ECS::World world;
    const ECS::SkeletonComponent* skel = nullptr;
    const ECS::HandIKComponent* ik = nullptr;

    bool Open() {
        if (!Load(world)) return false;
        const ECS::Entity hand = FindByName(world, "RightHand");
        if (hand == ECS::INVALID_ENTITY) return false;
        skel = world.GetComponent<ECS::SkeletonComponent>(hand);
        ik = world.GetComponent<ECS::HandIKComponent>(hand);
        return skel && skel->skeleton && ik;
    }

    HandPose PoseAt(const Vector3& palm) const {
        HandPose pose;
        pose.palmPosition = palm;
        pose.palmNormal = Vector3(0.0f, -1.0f, 0.0f);
        auto worldOf = [&](const std::string& boneName) {
            Vector3 acc(0.0f, 0.0f, 0.0f);
            i32 idx = skel->skeleton->FindBoneIndex(boneName);
            while (idx >= 0) {
                const auto& b = skel->skeleton->bones[static_cast<usize>(idx)];
                acc = Vector3(acc.x + b.bindPosition.x, acc.y + b.bindPosition.y,
                              acc.z + b.bindPosition.z);
                idx = b.parentIndex;
            }
            return Vector3(palm.x + acc.x, palm.y + acc.y, palm.z + acc.z);
        };
        for (u32 f = 0; f < kFingerCount; ++f) {
            const auto& fb = ik->fingers[f];
            FingerChain& c = pose.fingers[f];
            c.joints[0] = worldOf(fb.proximal);
            c.joints[1] = worldOf(fb.intermediate);
            c.joints[2] = worldOf(fb.distal);
            c.joints[3] = worldOf(fb.tip);
            c.curlDirection = Vector3(0.0f, -1.0f, 0.0f);
            c.approachWeight = 1.0f;
        }
        return pose;
    }
};

// The top face of a named box prop, so the test asks the scene for its own
// numbers instead of carrying a copy that can drift away from it.
f32 TopOf(ECS::World& world, const std::string& name) {
    const ECS::Entity e = FindByName(world, name);
    if (e == ECS::INVALID_ENTITY) return 0.0f;
    const auto* t = world.GetComponent<ECS::TransformComponent>(e);
    const auto* b = world.GetComponent<ECS::BoxColliderComponent>(e);
    if (!t || !b) return 0.0f;
    return t->position.y + b->center.y + b->size.y * 0.5f;
}

Vector3 CentreOf(ECS::World& world, const std::string& name) {
    const ECS::Entity e = FindByName(world, name);
    if (e == ECS::INVALID_ENTITY) return Vector3(0.0f, 0.0f, 0.0f);
    const auto* t = world.GetComponent<ECS::TransformComponent>(e);
    return t ? t->position : Vector3(0.0f, 0.0f, 0.0f);
}

}  // namespace

// Every prop in the room, asked the same question: hold the hand above it at a
// distance a hand would actually be, let Auto mode decide what kind of thing it
// is, and count the fingers that land.
ENJIN_TEST(HandOnRealProps, EveryPropWithAColliderIsRestedOn) {
    Rig rig;
    if (!rig.Open()) { ENJIN_SKIP("demo scene not found"); return; }

    auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
    ENJIN_ASSERT_NOT_NULL(backend.get());
    backend->SetWorld(&rig.world);
    backend->Update(1.0f / 60.0f);   // cook the bodies

    PhysicsSurfaceQuery query(backend.get());

    // expectEdge is the classification this prop SHOULD get, and it is half the
    // point of the test: a worktop read as a railing curls five fingers into
    // thin air, and a railing read as a worktop puts them through the bar.
    // gap is how far above the prop the palm is held, and it is NOT the same
    // number for a face and a bar.
    //
    // On a face each finger drops straight down from its own knuckle, so the
    // distance it must cover is the gap. On a bar each finger must reach the
    // LINE, and the knuckles sit forward of the bar as well as above it, so the
    // distance is sqrt(gap^2 + forward^2) -- always more. Measured: at an 8 cm
    // gap every finger on this rig misses a railing by two to seven
    // millimetres, which is correct behaviour and reads as "the railing does
    // not work". A hand actually taking hold of a rail is closer to it than a
    // hand resting on a worktop, so the test holds it where a hand would be.
    struct Prop { const char* name; const char* label; bool expectEdge; f32 gap; };
    const Prop props[] = {
        {"Counter Top",   "worktop",                false, 0.08f},
        {"Counter Top 2", "worktop across the gap",  false, 0.08f},
        {"Shelf Plank",   "shelf",                   false, 0.08f},
        {"Rail",          "railing",                 true,  0.04f},
    };

    for (const Prop& p : props) {
        const f32 top = TopOf(rig.world, p.name);
        ENJIN_ASSERT_TRUE(top > 0.0f);
        const Vector3 c = CentreOf(rig.world, p.name);
        HandPose pose = rig.PoseAt(Vector3(c.x, top + p.gap, c.z));

        // Auto mode, the way the runtime runs it: ask the geometry which solve
        // this is, then run that one.
        const Vector3 a = pose.fingers[0].joints[0];
        const Vector3 b = pose.fingers[kFingerCount - 1].joints[0];
        const f32 radius = Vector3(b.x - a.x, b.y - a.y, b.z - a.z).Length() * 0.5f;
        const auto klass = HandIK::ClassifyContact(
            pose.palmPosition, pose.palmNormal,
            Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f),
            radius, 0.35f, query);

        HandIKResult r;
        if (klass.mode == HandTargetMode::SurfaceEdge) {
            r = HandIK::SolveEdge(pose, klass.centre.point, klass.edgeDirection,
                                  klass.centre.normal);
        } else {
            r = HandIK::SolveSurface(pose, query);
        }

        std::printf("    %-22s top y %.3f -> %u of 5 fingers, %s\n",
                    p.label, top, r.fingersContacted,
                    klass.mode == HandTargetMode::SurfaceEdge ? "edge" : "face");
        // Per-finger numbers, because "0 of 5" on its own cannot tell a finger
        // that is too short from a finger the solve never considered.
        for (u32 f = 0; f < kFingerCount; ++f) {
            const FingerChain& fc = pose.fingers[f];
            const Vector3 k = fc.joints[0];
            const Vector3 d(k.x - klass.centre.point.x, k.y - klass.centre.point.y,
                            k.z - klass.centre.point.z);
            const f32 alongAmt = d.x * klass.edgeDirection.x + d.y * klass.edgeDirection.y +
                                 d.z * klass.edgeDirection.z;
            const Vector3 perp(d.x - klass.edgeDirection.x * alongAmt,
                               d.y - klass.edgeDirection.y * alongAmt,
                               d.z - klass.edgeDirection.z * alongAmt);
            std::printf("        f%u reach %.4f  knuckle-to-target %.4f  %s\n",
                        f, fc.Reach(), perp.Length(), fc.contacted ? "contact" : "-");
        }

        ENJIN_EXPECT_TRUE(klass.hit);
        ENJIN_EXPECT_TRUE(p.expectEdge == (klass.mode == HandTargetMode::SurfaceEdge));
        // The whole claim: no markers, no bones, no per-prop authoring. Having
        // a collider is the entire requirement.
        ENJIN_EXPECT_TRUE(r.fingersContacted >= 2);
    }
}

// The negative, which is the half that proves the fingers are reading geometry
// rather than a remembered plane: over the gap between the two worktops there
// is nothing within reach, and nothing should claim contact.
ENJIN_TEST(HandOnRealProps, OverTheGapNothingIsContacted) {
    Rig rig;
    if (!rig.Open()) { ENJIN_SKIP("demo scene not found"); return; }

    auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
    backend->SetWorld(&rig.world);
    backend->Update(1.0f / 60.0f);
    PhysicsSurfaceQuery query(backend.get());

    // Counter Top spans x -7..0 and Counter Top 2 spans x 1.3..3.9, so the gap
    // is between them, with the floor far below any finger's reach.
    const f32 top = TopOf(rig.world, "Counter Top");
    HandPose pose = rig.PoseAt(Vector3(0.65f, top + 0.08f, -3.2f));

    const HandIKResult r = HandIK::SolveSurface(pose, query);
    std::printf("    over the gap -> %u of 5 fingers\n", r.fingersContacted);
    ENJIN_EXPECT_EQ(r.fingersContacted, 0u);
}

ENJIN_TEST_MAIN()
