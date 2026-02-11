#pragma once

#ifdef ENJIN_PHYSICS_JOLT

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <cstdint>

namespace Enjin {
namespace Physics {

struct CollisionFilterData;

// Buffered contact event from Jolt worker threads
struct JoltContactEvent {
    JPH::BodyID bodyA;
    JPH::BodyID bodyB;
    JPH::RVec3 contactPoint = JPH::RVec3::sZero();
    JPH::Vec3 contactNormal = JPH::Vec3::sZero();
    enum class Type : uint8_t { Added, Persisted, Removed } type = Type::Added;
    bool isSensor = false;
};

// Thread-safe contact listener that buffers events for main-thread processing.
// Jolt fires contact callbacks from worker threads, so we buffer behind a mutex
// and drain on the main thread during Update().
// Also performs bilateral collision filtering in OnContactValidate using
// per-body categoryBits/collisionMask from the backend's filter data map.
class JoltContactListener : public JPH::ContactListener {
public:
    // Set by JoltBackend to enable bilateral collision filtering
    const std::unordered_map<uint32_t, CollisionFilterData>* filterData = nullptr;

    // Called from Jolt worker threads — reject contacts that don't pass bilateral filter
    JPH::ValidateResult OnContactValidate(
        const JPH::Body& bodyA, const JPH::Body& bodyB,
        JPH::RVec3Arg baseOffset,
        const JPH::CollideShapeResult& collisionResult) override
    {
        if (filterData) {
            uint32_t idxA = bodyA.GetID().GetIndex();
            uint32_t idxB = bodyB.GetID().GetIndex();

            uint32_t catA = 1, maskA = 0xFFFFFFFF;
            uint32_t catB = 1, maskB = 0xFFFFFFFF;

            auto itA = filterData->find(idxA);
            if (itA != filterData->end()) { catA = itA->second.categoryBits; maskA = itA->second.collisionMask; }

            auto itB = filterData->find(idxB);
            if (itB != filterData->end()) { catB = itB->second.categoryBits; maskB = itB->second.collisionMask; }

            // Bilateral collision filter (same rule as SimplePhysics)
            if (!(catA & maskB) || !(catB & maskA)) {
                return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
            }
        }

        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(
        const JPH::Body& bodyA, const JPH::Body& bodyB,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings) override
    {
        JoltContactEvent evt;
        evt.bodyA = bodyA.GetID();
        evt.bodyB = bodyB.GetID();
        evt.type = JoltContactEvent::Type::Added;
        evt.isSensor = bodyA.IsSensor() || bodyB.IsSensor();
        if (manifold.mRelativeContactPointsOn1.size() > 0) {
            evt.contactPoint = manifold.mBaseOffset + manifold.mRelativeContactPointsOn1[0];
        } else {
            evt.contactPoint = manifold.mBaseOffset;
        }
        evt.contactNormal = manifold.mWorldSpaceNormal;

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_EventBuffer.push_back(evt);
    }

    void OnContactPersisted(
        const JPH::Body& bodyA, const JPH::Body& bodyB,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings) override
    {
        JoltContactEvent evt;
        evt.bodyA = bodyA.GetID();
        evt.bodyB = bodyB.GetID();
        evt.type = JoltContactEvent::Type::Persisted;
        evt.isSensor = bodyA.IsSensor() || bodyB.IsSensor();
        if (manifold.mRelativeContactPointsOn1.size() > 0) {
            evt.contactPoint = manifold.mBaseOffset + manifold.mRelativeContactPointsOn1[0];
        } else {
            evt.contactPoint = manifold.mBaseOffset;
        }
        evt.contactNormal = manifold.mWorldSpaceNormal;

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_EventBuffer.push_back(evt);
    }

    void OnContactRemoved(const JPH::SubShapeIDPair& subShapePair) override {
        JoltContactEvent evt;
        evt.bodyA = subShapePair.GetBody1ID();
        evt.bodyB = subShapePair.GetBody2ID();
        evt.type = JoltContactEvent::Type::Removed;

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_EventBuffer.push_back(evt);
    }

    // Called on main thread to harvest buffered events
    void DrainEvents(std::vector<JoltContactEvent>& out) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        out.swap(m_EventBuffer);
        m_EventBuffer.clear();
    }

private:
    std::mutex m_Mutex;
    std::vector<JoltContactEvent> m_EventBuffer;
};

} // namespace Physics
} // namespace Enjin

#endif // ENJIN_PHYSICS_JOLT
