// Verifies the drag-and-drop entity-creation helpers that EditorLayer::OnFileDrop
// calls. The drop itself is a GUI action that can't run headless, but these are
// the exact functions OnFileDrop invokes, so they prove the audio/sprite drop
// behavior end-to-end at the data level.

#include "EnjinTest.h"
#include "Enjin/Editor/DropImport.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"

using namespace Enjin;

ENJIN_TEST(DropImport, AudioDropCreatesSoundEntity) {
    // Arrange
    ECS::World world;
    // Act
    ECS::Entity e = Editor::CreateAudioSourceEntity(&world, "sounds/boom.wav");
    // Assert
    ENJIN_ASSERT_TRUE(world.IsValid(e));
    ENJIN_ASSERT_TRUE(world.HasComponent<ECS::AudioSourceComponent>(e));
    auto* src = world.GetComponent<ECS::AudioSourceComponent>(e);
    ENJIN_ASSERT_NOT_NULL(src);
    ENJIN_EXPECT_STR_EQ(src->clipPath, "sounds/boom.wav");
    ENJIN_EXPECT_TRUE(world.HasComponent<ECS::TransformComponent>(e));
    auto* name = world.GetComponent<ECS::NameComponent>(e);
    ENJIN_ASSERT_NOT_NULL(name);
    ENJIN_EXPECT_STR_EQ(name->name, "boom");  // file stem, no extension
}

ENJIN_TEST(DropImport, TextureDropCreatesSprite) {
    // Arrange
    ECS::World world;
    // Act
    ECS::Entity e = Editor::CreateSpriteEntity(&world, "art/hero.png");
    // Assert: a quad mesh + blended material carrying the texture path.
    ENJIN_ASSERT_TRUE(world.IsValid(e));
    ENJIN_EXPECT_TRUE(world.HasComponent<ECS::MeshComponent>(e));
    auto* mat = world.GetComponent<ECS::MaterialComponent>(e);
    ENJIN_ASSERT_NOT_NULL(mat);
    ENJIN_EXPECT_STR_EQ(mat->baseColorTexturePath, "art/hero.png");
    ENJIN_EXPECT_EQ((int)mat->alphaMode, (int)ECS::MaterialComponent::AlphaMode::Blend);
    auto* name = world.GetComponent<ECS::NameComponent>(e);
    ENJIN_ASSERT_NOT_NULL(name);
    ENJIN_EXPECT_STR_EQ(name->name, "hero");
}

ENJIN_TEST(DropImport, NullWorldIsSafe) {
    // Arrange / Act / Assert: helpers must no-op on a null world, not crash.
    ENJIN_EXPECT_EQ(Editor::CreateAudioSourceEntity(nullptr, "x.wav"), ECS::INVALID_ENTITY);
    ENJIN_EXPECT_EQ(Editor::CreateSpriteEntity(nullptr, "x.png"), ECS::INVALID_ENTITY);
}

ENJIN_TEST_MAIN()
