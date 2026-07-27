// OrbCollector — the C++ tier of the three-workflow demo.
//
// The SAME bite-size game exists three ways:
//   1. "Components Only" editor template  — zero code, Inspector components
//   2. "Script Only" editor template      — one AngelScript file
//   3. This file                          — pure C++ against Enjin::App
//
// Fly with WASD + mouse. Touch the three spinning orange orbs to collect
// them. Collect all three to win.

#include "Enjin/App.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/ECS/Components/Transform.h"

using Enjin::f32;
namespace Math = Enjin::Math;
namespace ECS = Enjin::ECS;

class OrbCollector : public Enjin::App {
public:
    void OnStart() override {
        AddPlane({0, 0, 0}, 20.0f, {0.35f, 0.5f, 0.35f});
        AddDirectionalLight({0.4f, -1.0f, -0.6f}, {1.0f, 0.95f, 0.85f}, 2.0f);

        m_Orbs[0] = AddCube({ 3.0f, 0.6f, -4.0f}, {0.5f, 0.5f, 0.5f}, {0.9f, 0.65f, 0.2f});
        m_Orbs[1] = AddCube({-3.0f, 0.6f, -6.0f}, {0.5f, 0.5f, 0.5f}, {0.9f, 0.65f, 0.2f});
        m_Orbs[2] = AddCube({ 0.0f, 0.6f, -9.0f}, {0.5f, 0.5f, 0.5f}, {0.9f, 0.65f, 0.2f});

        SetCameraPosition({0.0f, 1.7f, 3.0f});
        EnableFlyCam(true);

        ENJIN_LOG_INFO(Game, "Collect the 3 orbs! (WASD + mouse to fly)");
    }

    void OnUpdate(f32 dt) override {
        m_Spin += 120.0f * dt;
        Math::Vector3 camPos = GetCamera()->GetPosition();

        for (int i = 0; i < 3; ++i) {
            if (m_Orbs[i] == ECS::INVALID_ENTITY) continue;

            // spin so they feel alive
            SetRotation(m_Orbs[i],
                Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(m_Spin)));

            // collect by proximity
            auto* t = GetWorld()->GetComponent<ECS::TransformComponent>(m_Orbs[i]);
            if (t && (t->position - camPos).Length() <= 1.5f) {
                DestroyEntity(m_Orbs[i]);
                m_Orbs[i] = ECS::INVALID_ENTITY;
                ++m_Collected;
                if (m_Collected >= 3) {
                    ENJIN_LOG_INFO(Game, "YOU WIN!");
                } else {
                    ENJIN_LOG_INFO(Game, "%d / 3", m_Collected);
                }
            }
        }
    }

private:
    ECS::Entity m_Orbs[3] = { ECS::INVALID_ENTITY, ECS::INVALID_ENTITY, ECS::INVALID_ENTITY };
    int m_Collected = 0;
    f32 m_Spin = 0.0f;
};

ENJIN_SIMPLE_MAIN(OrbCollector)
