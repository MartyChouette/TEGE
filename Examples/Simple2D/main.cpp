// Simple2D — Minimal 2D scene using Enjin's SimpleApp2D
//
// Creates a simple 2D scene with sprites and colored rectangles.
// Coordinates are in pixels from bottom-left.

#include "Enjin/SimpleApp.h"
#include "Enjin/Platform/Input.h"
#include <cmath>

class Simple2DApp : public Enjin::SimpleApp2D {
public:
    void OnStart() override {
        // Background rectangles
        AddRect({100, 100}, {200, 150}, {0.2f, 0.3f, 0.5f});
        AddRect({400, 200}, {150, 100}, {0.5f, 0.2f, 0.3f});

        // Player square
        m_Player = AddRect({400, 300}, {48, 48}, {0.1f, 0.8f, 0.3f});

        // Some scenery
        AddRect({0, 0}, {800, 40}, {0.3f, 0.3f, 0.3f});   // Ground bar
        AddRect({600, 40}, {80, 120}, {0.4f, 0.35f, 0.3f}); // Pillar
    }

    void OnUpdate(Enjin::f32 dt) override {
        // Simple WASD movement for the player square
        Enjin::f32 speed = 200.0f;
        if (Enjin::Input::IsKeyDown(Enjin::KeyCode::W)) m_PlayerY += speed * dt;
        if (Enjin::Input::IsKeyDown(Enjin::KeyCode::S)) m_PlayerY -= speed * dt;
        if (Enjin::Input::IsKeyDown(Enjin::KeyCode::A)) m_PlayerX -= speed * dt;
        if (Enjin::Input::IsKeyDown(Enjin::KeyCode::D)) m_PlayerX += speed * dt;

        SetPosition2D(m_Player, {m_PlayerX, m_PlayerY});

        // Gentle bob animation
        m_Time += dt;
        Enjin::f32 bob = std::sin(m_Time * 3.0f) * 4.0f;
        SetPosition2D(m_Player, {m_PlayerX, m_PlayerY + bob});
    }

private:
    Enjin::ECS::Entity m_Player{};
    Enjin::f32 m_PlayerX = 400.0f;
    Enjin::f32 m_PlayerY = 300.0f;
    Enjin::f32 m_Time = 0.0f;
};

ENJIN_SIMPLE_MAIN(Simple2DApp)
