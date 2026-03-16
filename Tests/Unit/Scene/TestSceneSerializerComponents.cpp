#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/Scene/SceneSerializer.h"

using namespace Enjin;
using namespace Enjin::ECS;

// ===========================================================================
// Helper: serialize entity, load into fresh world, return first entity
// ===========================================================================
static Entity RoundTrip(World& src, World& dst) {
    Scene::SceneSerializer ser(&src);
    std::string json = ser.SaveToString();
    Scene::SceneSerializer de(&dst);
    auto result = de.LoadFromString(json);
    if (!result.success || result.entities.empty()) return INVALID_ENTITY;
    return result.entities[0];
}

// ===========================================================================
// MaterialComponent
// ===========================================================================

ENJIN_TEST(Material, PBRFieldsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& mat = w1.AddComponent<MaterialComponent>(e);
    mat.baseColor = Math::Vector3(0.8f, 0.2f, 0.1f);
    mat.opacity = 0.75f;
    mat.metallic = 0.9f;
    mat.roughness = 0.3f;
    mat.emissiveColor = Math::Vector3(1.0f, 0.5f, 0.0f);
    mat.emissiveStrength = 2.5f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* m = w2.GetComponent<MaterialComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(m);
    ENJIN_EXPECT_FLOAT_EQ(m->baseColor.x, 0.8f);
    ENJIN_EXPECT_FLOAT_EQ(m->baseColor.y, 0.2f);
    ENJIN_EXPECT_FLOAT_EQ(m->baseColor.z, 0.1f);
    ENJIN_EXPECT_FLOAT_EQ(m->opacity, 0.75f);
    ENJIN_EXPECT_FLOAT_EQ(m->metallic, 0.9f);
    ENJIN_EXPECT_FLOAT_EQ(m->roughness, 0.3f);
    ENJIN_EXPECT_FLOAT_EQ(m->emissiveColor.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m->emissiveStrength, 2.5f);
}

ENJIN_TEST(Material, AlphaModeRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& mat = w1.AddComponent<MaterialComponent>(e);
    mat.alphaMode = MaterialComponent::AlphaMode::Blend;
    mat.alphaCutoff = 0.3f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* m = w2.GetComponent<MaterialComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(m);
    ENJIN_EXPECT_EQ((int)m->alphaMode, (int)MaterialComponent::AlphaMode::Blend);
    ENJIN_EXPECT_FLOAT_EQ(m->alphaCutoff, 0.3f);
}

ENJIN_TEST(Material, RetroFlagsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& mat = w1.AddComponent<MaterialComponent>(e);
    mat.flatShading = true;
    mat.affineTexturing = true;
    mat.vertexSnapping = true;
    mat.stippleTransparency = true;
    mat.uvQuantize = true;
    mat.gouraudOnly = true;
    mat.vertexSnapResolution = 20; // stored as /8 encoded, max 31

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* m = w2.GetComponent<MaterialComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(m);
    ENJIN_EXPECT_TRUE(m->flatShading);
    ENJIN_EXPECT_TRUE(m->affineTexturing);
    ENJIN_EXPECT_TRUE(m->vertexSnapping);
    ENJIN_EXPECT_TRUE(m->stippleTransparency);
    ENJIN_EXPECT_TRUE(m->uvQuantize);
    ENJIN_EXPECT_TRUE(m->gouraudOnly);
    ENJIN_EXPECT_EQ(m->vertexSnapResolution, (u8)20);
}

ENJIN_TEST(Material, DitherGradientRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& mat = w1.AddComponent<MaterialComponent>(e);
    mat.ditherGradient = true;
    mat.ditherGradientBands = 6;
    mat.ditherGradientPattern = 3; // Halftone

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* m = w2.GetComponent<MaterialComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(m);
    ENJIN_EXPECT_TRUE(m->ditherGradient);
    ENJIN_EXPECT_EQ(m->ditherGradientBands, (u8)6);
    ENJIN_EXPECT_EQ(m->ditherGradientPattern, (u8)3);
}

ENJIN_TEST(Material, ShadowDitherRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& mat = w1.AddComponent<MaterialComponent>(e);
    mat.shadowDitherMode = 2;    // By Distance
    mat.shadowDitherPattern = 4; // Crosshatch

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* m = w2.GetComponent<MaterialComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(m);
    ENJIN_EXPECT_EQ(m->shadowDitherMode, (u8)2);
    ENJIN_EXPECT_EQ(m->shadowDitherPattern, (u8)4);
}

ENJIN_TEST(Material, ParallaxRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& mat = w1.AddComponent<MaterialComponent>(e);
    mat.parallaxScale = 0.1f;
    mat.parallaxMode = 3;  // ReliefMapping
    mat.pomMaxSteps = 64;
    mat.pomHeightScale = 0.08f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* m = w2.GetComponent<MaterialComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(m);
    ENJIN_EXPECT_FLOAT_EQ(m->parallaxScale, 0.1f);
    ENJIN_EXPECT_EQ(m->parallaxMode, 3u);
    ENJIN_EXPECT_EQ(m->pomMaxSteps, 64u);
    ENJIN_EXPECT_FLOAT_EQ(m->pomHeightScale, 0.08f);
}

ENJIN_TEST(Material, TexturePathsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& mat = w1.AddComponent<MaterialComponent>(e);
    mat.baseColorTexturePath = "textures/albedo.png";
    mat.normalTexturePath = "textures/normal.png";
    mat.heightTexturePath = "textures/height.png";

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* m = w2.GetComponent<MaterialComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(m);
    ENJIN_EXPECT_STR_EQ(m->baseColorTexturePath.c_str(), "textures/albedo.png");
    ENJIN_EXPECT_STR_EQ(m->normalTexturePath.c_str(), "textures/normal.png");
    ENJIN_EXPECT_STR_EQ(m->heightTexturePath.c_str(), "textures/height.png");
}

ENJIN_TEST(Material, RenderingFlagsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& mat = w1.AddComponent<MaterialComponent>(e);
    mat.doubleSided = true;
    mat.castShadows = false;
    mat.receiveShadows = false;
    mat.excludeFromCelShading = true;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* m = w2.GetComponent<MaterialComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(m);
    ENJIN_EXPECT_TRUE(m->doubleSided);
    ENJIN_EXPECT_FALSE(m->castShadows);
    ENJIN_EXPECT_FALSE(m->receiveShadows);
    ENJIN_EXPECT_TRUE(m->excludeFromCelShading);
}

ENJIN_TEST(Material, SurfaceControlsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& mat = w1.AddComponent<MaterialComponent>(e);
    mat.reflectivity = 0.8f;
    mat.fresnelPower = 3.0f;
    mat.rimLightStrength = 0.5f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* m = w2.GetComponent<MaterialComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(m);
    ENJIN_EXPECT_FLOAT_EQ(m->reflectivity, 0.8f);
    ENJIN_EXPECT_FLOAT_EQ(m->fresnelPower, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(m->rimLightStrength, 0.5f);
}

// ===========================================================================
// LightComponent
// ===========================================================================

ENJIN_TEST(Light, PointLightRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& light = w1.AddComponent<LightComponent>(e);
    light.type = LightType::Point;
    light.color = Math::Vector3(1.0f, 0.9f, 0.7f);
    light.intensity = 2.5f;
    light.range = 25.0f;
    light.castShadows = false;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* l = w2.GetComponent<LightComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(l);
    ENJIN_EXPECT_EQ((int)l->type, (int)LightType::Point);
    ENJIN_EXPECT_FLOAT_EQ(l->color.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(l->color.y, 0.9f);
    ENJIN_EXPECT_FLOAT_EQ(l->intensity, 2.5f);
    ENJIN_EXPECT_FLOAT_EQ(l->range, 25.0f);
    ENJIN_EXPECT_FALSE(l->castShadows);
}

ENJIN_TEST(Light, SpotLightRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& light = w1.AddComponent<LightComponent>(e);
    light.type = LightType::Spot;
    light.innerConeAngle = 15.0f;
    light.outerConeAngle = 30.0f;
    light.linearAttenuation = 0.14f;
    light.quadraticAttenuation = 0.07f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* l = w2.GetComponent<LightComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(l);
    ENJIN_EXPECT_EQ((int)l->type, (int)LightType::Spot);
    ENJIN_EXPECT_FLOAT_EQ(l->innerConeAngle, 15.0f);
    ENJIN_EXPECT_FLOAT_EQ(l->outerConeAngle, 30.0f);
    ENJIN_EXPECT_FLOAT_EQ(l->linearAttenuation, 0.14f);
    ENJIN_EXPECT_FLOAT_EQ(l->quadraticAttenuation, 0.07f);
}

ENJIN_TEST(Light, DirectionalLightRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& light = w1.AddComponent<LightComponent>(e);
    light.type = LightType::Directional;
    light.color = Math::Vector3(1.0f, 1.0f, 0.95f);
    light.intensity = 1.2f;
    light.castShadows = true;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* l = w2.GetComponent<LightComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(l);
    ENJIN_EXPECT_EQ((int)l->type, (int)LightType::Directional);
    ENJIN_EXPECT_FLOAT_EQ(l->intensity, 1.2f);
    ENJIN_EXPECT_TRUE(l->castShadows);
}

// ===========================================================================
// CameraComponent
// ===========================================================================

ENJIN_TEST(Camera, PerspectiveRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& cam = w1.AddComponent<CameraComponent>(e);
    cam.projectionType = ProjectionType::Perspective;
    cam.fieldOfView = 75.0f;
    cam.nearPlane = 0.5f;
    cam.farPlane = 500.0f;
    cam.priority = 5;
    cam.isActive = false;
    cam.backgroundColor = Math::Vector3(0.2f, 0.3f, 0.4f);

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* c = w2.GetComponent<CameraComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(c);
    ENJIN_EXPECT_EQ((int)c->projectionType, (int)ProjectionType::Perspective);
    ENJIN_EXPECT_FLOAT_EQ(c->fieldOfView, 75.0f);
    ENJIN_EXPECT_FLOAT_EQ(c->nearPlane, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(c->farPlane, 500.0f);
    ENJIN_EXPECT_EQ(c->priority, 5);
    ENJIN_EXPECT_FALSE(c->isActive);
    ENJIN_EXPECT_FLOAT_EQ(c->backgroundColor.x, 0.2f);
}

ENJIN_TEST(Camera, OrthographicRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& cam = w1.AddComponent<CameraComponent>(e);
    cam.projectionType = ProjectionType::Orthographic;
    cam.orthoSize = 20.0f;
    cam.cullingMask = 0x0000FFFF;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* c = w2.GetComponent<CameraComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(c);
    ENJIN_EXPECT_EQ((int)c->projectionType, (int)ProjectionType::Orthographic);
    ENJIN_EXPECT_FLOAT_EQ(c->orthoSize, 20.0f);
    ENJIN_EXPECT_EQ(c->cullingMask, 0x0000FFFFu);
}

ENJIN_TEST(Camera, ViewportRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& cam = w1.AddComponent<CameraComponent>(e);
    cam.viewportX = 0.5f;
    cam.viewportY = 0.0f;
    cam.viewportWidth = 0.5f;
    cam.viewportHeight = 1.0f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* c = w2.GetComponent<CameraComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(c);
    ENJIN_EXPECT_FLOAT_EQ(c->viewportX, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(c->viewportY, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(c->viewportWidth, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(c->viewportHeight, 1.0f);
}

// ===========================================================================
// NotesComponent
// ===========================================================================

ENJIN_TEST(Notes, RoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    w1.AddComponent<NotesComponent>(e).notes = "This is a test note with special chars: <>&\"";

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* n = w2.GetComponent<NotesComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(n);
    ENJIN_EXPECT_STR_EQ(n->notes.c_str(), "This is a test note with special chars: <>&\"");
}

ENJIN_TEST(Notes, EmptyString) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    w1.AddComponent<NotesComponent>(e); // default empty

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* n = w2.GetComponent<NotesComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(n);
    ENJIN_EXPECT_TRUE(n->notes.empty());
}

// ===========================================================================
// PostProcessVolumeComponent
// ===========================================================================

ENJIN_TEST(PostProcessVolume, BasicRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& ppv = w1.AddComponent<PostProcessVolumeComponent>(e);
    ppv.shape = PPVolumeShape::Sphere;
    ppv.halfExtents = Math::Vector3(5.0f, 5.0f, 5.0f);
    ppv.priority = 10;
    ppv.isActive = true;
    ppv.isGlobal = true;
    ppv.blendRadius = 3.5f;
    ppv.weight = 0.8f;
    ppv.overrideMask = 0x00FF00FF;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* p = w2.GetComponent<PostProcessVolumeComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(p);
    ENJIN_EXPECT_EQ((int)p->shape, (int)PPVolumeShape::Sphere);
    ENJIN_EXPECT_FLOAT_EQ(p->halfExtents.x, 5.0f);
    ENJIN_EXPECT_EQ(p->priority, 10);
    ENJIN_EXPECT_TRUE(p->isActive);
    ENJIN_EXPECT_TRUE(p->isGlobal);
    ENJIN_EXPECT_FLOAT_EQ(p->blendRadius, 3.5f);
    ENJIN_EXPECT_FLOAT_EQ(p->weight, 0.8f);
    ENJIN_EXPECT_EQ(p->overrideMask, 0x00FF00FFu);
}

ENJIN_TEST(PostProcessVolume, BoxShapeDefaults) {
    PostProcessVolumeComponent ppv;
    ENJIN_EXPECT_EQ((int)ppv.shape, (int)PPVolumeShape::Box);
    ENJIN_EXPECT_EQ(ppv.priority, 0);
    ENJIN_EXPECT_TRUE(ppv.isActive);
    ENJIN_EXPECT_FALSE(ppv.isGlobal);
    ENJIN_EXPECT_FLOAT_EQ(ppv.blendRadius, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(ppv.weight, 1.0f);
    ENJIN_EXPECT_EQ(ppv.overrideMask, 0xFFFFFFFFu);
}

// ===========================================================================
// Multi-component entity
// ===========================================================================

ENJIN_TEST(MultiComponent, MaterialAndLightOnSameEntity) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& mat = w1.AddComponent<MaterialComponent>(e);
    mat.metallic = 0.7f;
    mat.ditherGradient = true;
    auto& light = w1.AddComponent<LightComponent>(e);
    light.type = LightType::Point;
    light.intensity = 3.0f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* m = w2.GetComponent<MaterialComponent>(e2);
    auto* l = w2.GetComponent<LightComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(m);
    ENJIN_ASSERT_NOT_NULL(l);
    ENJIN_EXPECT_FLOAT_EQ(m->metallic, 0.7f);
    ENJIN_EXPECT_TRUE(m->ditherGradient);
    ENJIN_EXPECT_FLOAT_EQ(l->intensity, 3.0f);
}

ENJIN_TEST_MAIN()
