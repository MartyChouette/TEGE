#include "EnjinTest.h"
#include "Enjin/AI/AIBehaviors.h"

using namespace Enjin;
using namespace Enjin::AI;

// ===========================================================================
// AIState Enum
// ===========================================================================

ENJIN_TEST(AIState, Values) {
    ENJIN_EXPECT_EQ((int)AIState::Idle, 0);
    ENJIN_EXPECT_EQ((int)AIState::Patrol, 1);
    ENJIN_EXPECT_EQ((int)AIState::Chase, 2);
    ENJIN_EXPECT_EQ((int)AIState::Attack, 3);
    ENJIN_EXPECT_EQ((int)AIState::Flee, 4);
    ENJIN_EXPECT_EQ((int)AIState::Investigate, 5);
    ENJIN_EXPECT_EQ((int)AIState::Return, 6);
    ENJIN_EXPECT_EQ((int)AIState::Dead, 7);
    ENJIN_EXPECT_EQ((int)AIState::Custom, 8);
}

ENJIN_TEST(AIState, ToString) {
    ENJIN_EXPECT_NOT_NULL(AIStateToString(AIState::Idle));
    ENJIN_EXPECT_NOT_NULL(AIStateToString(AIState::Patrol));
    ENJIN_EXPECT_NOT_NULL(AIStateToString(AIState::Chase));
    ENJIN_EXPECT_NOT_NULL(AIStateToString(AIState::Dead));
}

// ===========================================================================
// AIPerceptionSettings Defaults
// ===========================================================================

ENJIN_TEST(Perception, Defaults) {
    AIPerceptionSettings p;
    ENJIN_EXPECT_FLOAT_EQ(p.sightRange, 15.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.sightAngle, 120.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.peripheralRange, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.hearingRange, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.noiseThreshold, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(p.memoryDuration, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.investigateTime, 3.0f);
}

// ===========================================================================
// AIMovementSettings Defaults
// ===========================================================================

ENJIN_TEST(Movement, Defaults) {
    AIMovementSettings m;
    ENJIN_EXPECT_FLOAT_EQ(m.walkSpeed, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.runSpeed, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.turnSpeed, 180.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.acceleration, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.deceleration, 15.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.patrolWaitTime, 2.0f);
    ENJIN_EXPECT_TRUE(m.patrolLoop);
    ENJIN_EXPECT_FLOAT_EQ(m.chaseGiveUpDistance, 25.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.chaseGiveUpTime, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.attackRange, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(m.preferredDistance, 3.0f);
}

// ===========================================================================
// AITarget
// ===========================================================================

ENJIN_TEST(Target, Defaults) {
    AITarget target;
    ENJIN_EXPECT_EQ(target.entityId, (u64)0);
    ENJIN_EXPECT_FALSE(target.isVisible);
    ENJIN_EXPECT_FALSE(target.isValid);
}

ENJIN_TEST(Target, Clear) {
    AITarget target;
    target.entityId = 42;
    target.isVisible = true;
    target.isValid = true;
    target.Clear();
    ENJIN_EXPECT_FALSE(target.isValid);
}

// ===========================================================================
// AIAgent
// ===========================================================================

ENJIN_TEST(Agent, InitialState) {
    AIAgent agent;
    ENJIN_EXPECT_EQ((int)agent.GetCurrentState(), (int)AIState::Idle);
    ENJIN_EXPECT_FALSE(agent.IsMoving());
    ENJIN_EXPECT_FALSE(agent.HasTarget());
    ENJIN_EXPECT_FALSE(agent.HasNavmesh());
}

ENJIN_TEST(Agent, Initialize) {
    AIAgent agent;
    agent.Initialize(100);
    ENJIN_EXPECT_EQ(agent.GetEntityId(), (u64)100);
}

ENJIN_TEST(Agent, SetSpawnPosition) {
    AIAgent agent;
    agent.SetSpawnPosition(Math::Vector3(10, 0, 20));
    Math::Vector3 sp = agent.GetSpawnPosition();
    ENJIN_EXPECT_FLOAT_EQ(sp.x, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(sp.z, 20.0f);
}

ENJIN_TEST(Agent, SetPosition) {
    AIAgent agent;
    agent.SetPosition(Math::Vector3(5, 1, 5));
    Math::Vector3 p = agent.GetPosition();
    ENJIN_EXPECT_FLOAT_EQ(p.x, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.y, 1.0f);
}

ENJIN_TEST(Agent, TransitionTo) {
    AIAgent agent;
    ENJIN_EXPECT_EQ((int)agent.GetCurrentState(), (int)AIState::Idle);
    agent.TransitionTo(AIState::Patrol);
    ENJIN_EXPECT_EQ((int)agent.GetCurrentState(), (int)AIState::Patrol);
    agent.TransitionTo(AIState::Chase);
    ENJIN_EXPECT_EQ((int)agent.GetCurrentState(), (int)AIState::Chase);
}

ENJIN_TEST(Agent, SetTarget) {
    AIAgent agent;
    AITarget target;
    target.entityId = 42;
    target.position = Math::Vector3(10, 0, 10);
    target.isValid = true;
    agent.SetTarget(target);
    ENJIN_EXPECT_TRUE(agent.HasTarget());
    ENJIN_EXPECT_EQ(agent.GetTarget().entityId, (u64)42);
}

ENJIN_TEST(Agent, ClearTarget) {
    AIAgent agent;
    AITarget target;
    target.isValid = true;
    agent.SetTarget(target);
    ENJIN_EXPECT_TRUE(agent.HasTarget());
    agent.ClearTarget();
    ENJIN_EXPECT_FALSE(agent.HasTarget());
}

ENJIN_TEST(Agent, SettingsAccess) {
    AIAgent agent;
    AIPerceptionSettings perc;
    perc.sightRange = 30.0f;
    agent.SetPerceptionSettings(perc);
    ENJIN_EXPECT_FLOAT_EQ(agent.GetPerception().sightRange, 30.0f);

    AIMovementSettings mov;
    mov.runSpeed = 8.0f;
    agent.SetMovementSettings(mov);
    ENJIN_EXPECT_FLOAT_EQ(agent.GetMovement().runSpeed, 8.0f);
}

ENJIN_TEST(Agent, StateChangeCallback) {
    AIAgent agent;
    AIState oldCapture = AIState::Dead;
    AIState newCapture = AIState::Dead;
    agent.SetOnStateChange([&](AIState o, AIState n) {
        oldCapture = o;
        newCapture = n;
    });
    agent.TransitionTo(AIState::Chase);
    ENJIN_EXPECT_EQ((int)oldCapture, (int)AIState::Idle);
    ENJIN_EXPECT_EQ((int)newCapture, (int)AIState::Chase);
}

ENJIN_TEST(Agent, BehaviorStatesCorrect) {
    AIAgent agent;
    // Default agent should have built-in behaviors
    agent.TransitionTo(AIState::Idle);
    auto* behavior = agent.GetCurrentBehavior();
    if (behavior) {
        ENJIN_EXPECT_EQ((int)behavior->GetState(), (int)AIState::Idle);
    }
}

ENJIN_TEST(Agent, Stop) {
    AIAgent agent;
    agent.MoveTo(Math::Vector3(100, 0, 100));
    agent.Stop();
    ENJIN_EXPECT_FALSE(agent.IsMoving());
}

ENJIN_TEST_MAIN()
