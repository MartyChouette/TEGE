#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace Enjin {
namespace Effects {

// Fracture pattern types
enum class FracturePattern : u8 {
    Voronoi,        // Random Voronoi cell splitting
    Grid,           // Regular grid fragmentation
    Radial,         // Radial cracks from impact point
    Shatter         // Glass-like shattering
};

// Individual debris fragment
struct DebrisFragment {
    Math::Vector3 position;
    Math::Vector3 velocity;
    Math::Vector3 angularVelocity;
    Math::Vector3 scale;
    f32 lifetime;
    f32 age = 0.0f;

    // Mesh data (triangles from fracture)
    std::vector<Math::Vector3> vertices;
    std::vector<u32> indices;
    Math::Vector3 color;
};

// Fracture configuration
struct FractureConfig {
    FracturePattern pattern = FracturePattern::Voronoi;
    u32 fragmentCount = 8;          // Number of pieces
    f32 explosionForce = 5.0f;      // Outward force on fragments
    f32 debrisLifetime = 3.0f;      // How long fragments exist
    f32 debrisGravity = 9.81f;      // Gravity on fragments
    f32 fragmentScaleMin = 0.3f;    // Min scale of fragments
    f32 fragmentScaleMax = 1.0f;    // Max scale of fragments
    bool chainDestruction = false;  // Can trigger nearby destructibles
    f32 chainRadius = 2.0f;         // Radius for chain destruction
    f32 chainDelay = 0.1f;          // Delay between chain triggers
    f32 chainDamageFalloff = 0.5f;  // Damage multiplier per chain hop
};

// Active destruction event
struct DestructionEvent {
    ECS::Entity entity;
    Math::Vector3 impactPoint;
    Math::Vector3 impactDirection;
    f32 impactForce;
    f32 chainDamage = 1.0f;
};

// Manages destructible entity fracturing and debris
class ENJIN_API DestructibleSystem {
public:
    DestructibleSystem();
    ~DestructibleSystem();

    void Initialize(ECS::World* world);
    void Update(f32 deltaTime);
    void Shutdown();

    // Trigger destruction of an entity
    void Destroy(ECS::Entity entity, const Math::Vector3& impactPoint,
                 const Math::Vector3& impactDirection, f32 force = 5.0f);

    // Apply damage (triggers destroy if health <= 0)
    void ApplyDamage(ECS::Entity entity, f32 damage,
                     const Math::Vector3& impactPoint = Math::Vector3(0,0,0),
                     const Math::Vector3& impactDir = Math::Vector3(0,-1,0));

    // Set fracture config for an entity (or default)
    void SetFractureConfig(ECS::Entity entity, const FractureConfig& config);
    FractureConfig GetFractureConfig(ECS::Entity entity) const;
    void SetDefaultFractureConfig(const FractureConfig& config);

    // Get active debris count
    u32 GetActiveDebrisCount() const;

private:
    void ProcessDestructionQueue();
    void GenerateVoronoiFragments(const DestructionEvent& event, std::vector<DebrisFragment>& outFragments);
    void GenerateGridFragments(const DestructionEvent& event, std::vector<DebrisFragment>& outFragments);
    void GenerateRadialFragments(const DestructionEvent& event, std::vector<DebrisFragment>& outFragments);
    void GenerateShatterFragments(const DestructionEvent& event, std::vector<DebrisFragment>& outFragments);
    void UpdateDebris(f32 deltaTime);
    void CheckChainDestruction(const DestructionEvent& event);

    // Simple LCG random
    u32 Random();
    f32 RandomFloat();
    f32 RandomRange(f32 min, f32 max);

    ECS::World* m_World = nullptr;
    FractureConfig m_DefaultConfig;
    std::unordered_map<ECS::Entity, FractureConfig> m_EntityConfigs;
    std::vector<DestructionEvent> m_DestructionQueue;
    std::vector<DebrisFragment> m_ActiveDebris;
    u32 m_RandomState = 54321;
};

} // namespace Effects
} // namespace Enjin
