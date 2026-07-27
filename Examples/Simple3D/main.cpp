// Simple3D — Minimal 3D scene using Enjin's App
//
// Creates a lit scene with a ground plane and some cubes.
// Use RMB + WASD to fly around, scroll wheel to adjust speed.

#include "Enjin/App.h"

class Simple3DApp : public Enjin::App {
public:
    void OnStart() override {
        // Ground plane
        AddPlane({0, 0, 0}, 20.0f, {0.3f, 0.35f, 0.3f});

        // Some cubes
        m_RedCube = AddCube({-2, 0.5f, 0}, {1, 1, 1}, {0.9f, 0.2f, 0.2f});
        AddCube({0, 0.5f, 0}, {1, 1, 1}, {0.2f, 0.9f, 0.2f});
        AddCube({2, 0.5f, 0}, {1, 1, 1}, {0.2f, 0.2f, 0.9f});

        // Stacked cubes
        AddCube({0, 1.5f, 0}, {0.6f, 0.6f, 0.6f}, {1.0f, 0.9f, 0.3f});

        // Sunlight
        AddDirectionalLight({0.5f, -1.0f, -0.3f}, {1.0f, 0.95f, 0.9f}, 2.0f);

        // Accent light
        AddPointLight({0, 3, 2}, {0.5f, 0.7f, 1.0f}, 3.0f, 8.0f);

        // Camera
        SetCameraPosition({0, 4, 8});
        SetCameraRotation(0.0f, -20.0f);
    }

    void OnUpdate(Enjin::f32 dt) override {
        // Rotate the red cube
        m_Angle += dt * 45.0f;  // 45 degrees per second
        SetRotation(m_RedCube, Enjin::Math::Quaternion::FromEuler(
            Enjin::Math::Vector3(0, m_Angle, 0)));
    }

private:
    Enjin::ECS::Entity m_RedCube{};
    float m_Angle = 0.0f;
};

ENJIN_SIMPLE_MAIN(Simple3DApp)
