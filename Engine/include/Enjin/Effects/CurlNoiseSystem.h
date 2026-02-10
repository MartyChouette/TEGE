#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace Effects {

// Curl noise flow field system — applies divergence-free velocity fields
// to particles within field volumes. Runs after ParticleSystem::Update().
class ENJIN_API CurlNoiseSystem {
public:
    CurlNoiseSystem();
    ~CurlNoiseSystem();

    void Initialize(ECS::World* world);
    void Update(f32 deltaTime);
    void Shutdown();

    // Sample the combined flow field at a world position (sum of all active fields)
    Math::Vector3 SampleField(const Math::Vector3& worldPos) const;

private:
    ECS::World* m_World = nullptr;
};

} // namespace Effects
} // namespace Enjin
