// ParallaxMachine layers become ordinary parallax sprites (EP-6).
//
// The machine was drawn only by the editor, as an ImGui overlay behind the Game
// View image, so it never reached a built game. It now converts to one Sprite2D
// entity per layer with a ParallaxLayerComponent, which every runtime draws.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/ParallaxLayer.h"
#include "Enjin/ECS/Components/ParallaxMachine.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Systems/ParallaxSystem.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::ECS;

namespace {
bool Near(f32 a, f32 b) { return std::fabs(a - b) < 1e-4f; }
}

ENJIN_TEST(ParallaxConvert, test_parallax_machine_becomes_one_sprite_per_layer) {
    // Arrange
    World world;
    const Entity m = world.CreateEntity();
    TransformComponent xf;
    xf.position = Math::Vector3(2.0f, 1.0f, 0.0f);
    world.AddComponent<TransformComponent>(m, xf);
    ParallaxMachineComponent pm;
    ParallaxLayer sky;  sky.texturePath = "sky.png";   sky.distance = 10.0f; sky.scale = Math::Vector2(20.0f, 10.0f); sky.sortOrder = 0;
    ParallaxLayer front; front.texturePath = "near.png"; front.distance = 1.0f; front.offset = Math::Vector2(0.0f, -3.0f); front.sortOrder = 2;
    pm.layers = { sky, front };
    pm.autoScrollSpeed = Math::Vector2(0.5f, 0.0f);
    world.AddComponent<ParallaxMachineComponent>(m, pm);

    // Act
    const u32 made = ParallaxSystem::ConvertMachinesToLayers(&world);

    // Assert
    ENJIN_EXPECT_EQ(made, 2u);
    ENJIN_EXPECT_FALSE(world.HasComponent<ParallaxMachineComponent>(m));
    const auto layers = world.GetEntitiesWithComponent<ParallaxLayerComponent>();
    ENJIN_ASSERT_TRUE(layers.size() == 2);
    bool sawSky = false, sawNear = false;
    for (Entity e : layers) {
        const auto* s = world.GetComponent<Sprite2DComponent>(e);
        const auto* pl = world.GetComponent<ParallaxLayerComponent>(e);
        const auto* t = world.GetComponent<TransformComponent>(e);
        ENJIN_ASSERT_TRUE(s && pl && t);
        ENJIN_EXPECT_TRUE(Near(pl->autoScroll.x, -0.5f));
        if (s->texturePath == "sky.png") {
            sawSky = true;
            ENJIN_EXPECT_TRUE(Near(pl->factor.x, 0.1f));   // a tenth of world motion at distance 10
            ENJIN_EXPECT_TRUE(Near(s->size.x, 20.0f));
        } else if (s->texturePath == "near.png") {
            sawNear = true;
            ENJIN_EXPECT_TRUE(Near(pl->factor.x, 1.0f));   // distance 1 moves with the world
            ENJIN_EXPECT_TRUE(Near(t->position.y, -2.0f)); // machine at y 1, layer offset -3
            ENJIN_EXPECT_EQ(s->orderInLayer, 2);
        }
    }
    ENJIN_EXPECT_TRUE(sawSky && sawNear);
}

ENJIN_TEST(ParallaxConvert, test_parallax_convert_with_no_machine_makes_nothing) {
    // Arrange
    World world;
    world.CreateEntity();

    // Act
    const u32 made = ParallaxSystem::ConvertMachinesToLayers(&world);

    // Assert
    ENJIN_EXPECT_EQ(made, 0u);
}

ENJIN_TEST_MAIN()
