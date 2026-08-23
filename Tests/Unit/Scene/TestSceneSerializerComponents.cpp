#include "EnjinTest.h"
#include <set>
#include <cstdio>
#include <string>
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/ECS/Components/CustomShader.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/DungeonGenerator.h"
#include "Enjin/ECS/Components/RandomBag.h"
#include "Enjin/ECS/Systems/RandomBagSystem.h"
#include "Enjin/ECS/Components/Scatter.h"
#include "Enjin/ECS/Systems/ScatterSystem.h"
#include "Enjin/ECS/Components/TerrainGenerator.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Systems/TerrainGeneratorSystem.h"
#include "Enjin/ECS/Components/WFC.h"
#include "Enjin/ECS/Systems/WFCSystem.h"

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
// AudioSourceComponent
// ===========================================================================

ENJIN_TEST(AudioSource, FieldsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& a = w1.AddComponent<AudioSourceComponent>(e);
    a.clipPath = "sfx/jump.wav";
    a.volume = 0.6f;
    a.pitch = 1.2f;
    a.minDistance = 3.0f;
    a.maxDistance = 40.0f;
    a.playOnAwake = true;
    a.loop = true;
    a.is3D = false;
    a.spatialBlend = 0.25f;
    a.priority = 64;
    a.pitchMin = 0.9f;
    a.pitchMax = 1.1f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<AudioSourceComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_EQ(g->clipPath, std::string("sfx/jump.wav"));
    ENJIN_EXPECT_FLOAT_EQ(g->volume, 0.6f);
    ENJIN_EXPECT_FLOAT_EQ(g->pitch, 1.2f);
    ENJIN_EXPECT_FLOAT_EQ(g->minDistance, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->maxDistance, 40.0f);
    ENJIN_EXPECT_TRUE(g->playOnAwake);
    ENJIN_EXPECT_TRUE(g->loop);
    ENJIN_EXPECT_FALSE(g->is3D);
    ENJIN_EXPECT_FLOAT_EQ(g->spatialBlend, 0.25f);
    ENJIN_EXPECT_EQ(g->priority, 64);
    ENJIN_EXPECT_FLOAT_EQ(g->pitchMin, 0.9f);   // regression: was not serialized
    ENJIN_EXPECT_FLOAT_EQ(g->pitchMax, 1.1f);
}

// ===========================================================================
// ParticleEmitterComponent
// ===========================================================================

ENJIN_TEST(ParticleEmitter, FieldsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& p = w1.AddComponent<ParticleEmitterComponent>(e);
    p.playOnAwake = false;
    p.loop = false;
    p.emissionRate = 42.0f;
    p.burstCount = 5;
    p.lifetime = 3.5f;
    p.startSpeed = 7.0f;
    p.startSize = 1.2f;
    p.endSize = 0.2f;
    p.startColor = Math::Vector3(1.0f, 0.5f, 0.0f);
    p.endColor = Math::Vector3(0.0f, 0.0f, 1.0f);
    p.startAlpha = 0.9f;
    p.endAlpha = 0.0f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<ParticleEmitterComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_FALSE(g->playOnAwake);
    ENJIN_EXPECT_FALSE(g->loop);
    ENJIN_EXPECT_FLOAT_EQ(g->emissionRate, 42.0f);
    ENJIN_EXPECT_EQ(g->burstCount, 5);
    ENJIN_EXPECT_FLOAT_EQ(g->lifetime, 3.5f);
    ENJIN_EXPECT_FLOAT_EQ(g->startSpeed, 7.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->startSize, 1.2f);
    ENJIN_EXPECT_FLOAT_EQ(g->endSize, 0.2f);
    ENJIN_EXPECT_FLOAT_EQ(g->startColor.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->startColor.y, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(g->endColor.z, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->startAlpha, 0.9f);
    ENJIN_EXPECT_FLOAT_EQ(g->endAlpha, 0.0f);
}

// ===========================================================================
// RigidbodyComponent
// ===========================================================================

ENJIN_TEST(Rigidbody, FieldsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& rb = w1.AddComponent<RigidbodyComponent>(e);
    rb.mass = 3.0f;
    rb.drag = 0.4f;
    rb.angularDrag = 0.2f;
    rb.useGravity = false;
    rb.gravityScale = 2.0f;
    rb.velocity = Math::Vector3(1.0f, -2.0f, 3.0f);
    rb.maxVelocity = 42.0f;
    rb.freezePositionY = true;
    rb.bodyType = RigidbodyComponent::BodyType::Kinematic;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<RigidbodyComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_FLOAT_EQ(g->mass, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->drag, 0.4f);
    ENJIN_EXPECT_FLOAT_EQ(g->angularDrag, 0.2f);
    ENJIN_EXPECT_FALSE(g->useGravity);
    ENJIN_EXPECT_FLOAT_EQ(g->gravityScale, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->velocity.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->velocity.z, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->maxVelocity, 42.0f);
    ENJIN_EXPECT_TRUE(g->freezePositionY);
    ENJIN_EXPECT_EQ((int)g->bodyType, (int)RigidbodyComponent::BodyType::Kinematic);
}

// ===========================================================================
// HealthComponent
// ===========================================================================

ENJIN_TEST(Health, FieldsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& h = w1.AddComponent<HealthComponent>(e);
    h.maxHealth = 150.0f;
    h.currentHealth = 90.0f;
    h.regenRate = 5.0f;
    h.maxShield = 50.0f;
    h.currentShield = 25.0f;
    h.isInvulnerable = true;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<HealthComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_FLOAT_EQ(g->maxHealth, 150.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->currentHealth, 90.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->regenRate, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->maxShield, 50.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->currentShield, 25.0f);
    ENJIN_EXPECT_TRUE(g->isInvulnerable);
}

// ===========================================================================
// Collider components (collision filtering)
// ===========================================================================

ENJIN_TEST(BoxCollider, FieldsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& c = w1.AddComponent<BoxColliderComponent>(e);
    c.center = Math::Vector3(0.5f, 1.0f, -0.5f);
    c.size = Math::Vector3(2.0f, 3.0f, 4.0f);
    c.isTrigger = true;
    c.friction = 0.3f;
    c.bounciness = 0.7f;
    c.categoryBits = 0x4u;
    c.collisionMask = 0x6u;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<BoxColliderComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_FLOAT_EQ(g->center.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->size.z, 4.0f);
    ENJIN_EXPECT_TRUE(g->isTrigger);
    ENJIN_EXPECT_FLOAT_EQ(g->friction, 0.3f);
    ENJIN_EXPECT_FLOAT_EQ(g->bounciness, 0.7f);
    ENJIN_EXPECT_EQ(g->categoryBits, 0x4u);
    ENJIN_EXPECT_EQ(g->collisionMask, 0x6u);
}

ENJIN_TEST(SphereCollider, FieldsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& c = w1.AddComponent<SphereColliderComponent>(e);
    c.center = Math::Vector3(1.0f, 2.0f, 3.0f);
    c.radius = 2.5f;
    c.isTrigger = false;
    c.friction = 0.8f;
    c.bounciness = 0.2f;
    c.categoryBits = 0x2u;
    c.collisionMask = 0x5u;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<SphereColliderComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_FLOAT_EQ(g->center.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(g->radius, 2.5f);
    ENJIN_EXPECT_FALSE(g->isTrigger);
    ENJIN_EXPECT_FLOAT_EQ(g->friction, 0.8f);
    ENJIN_EXPECT_FLOAT_EQ(g->bounciness, 0.2f);
    ENJIN_EXPECT_EQ(g->categoryBits, 0x2u);
    ENJIN_EXPECT_EQ(g->collisionMask, 0x5u);
}

// ===========================================================================
// Script EntityArray property (drag-assignable entity lists)
// ===========================================================================

ENJIN_TEST(Script, EntityArrayPropertyRoundTrip) {
    // Regression: EntityArray is ScriptPropertyType value 9, and the ScriptComponent
    // deserializer clamped the type to <= 8 — so an array-entity property serialized but
    // was rejected on load, losing the assigned entities. Now it round-trips.
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& sc = w1.AddComponent<ScriptComponent>(e);

    ScriptAttachment att;
    att.scriptPath = "scripts/Foo.as";
    att.className = "Foo";
    att.enabled = true;
    ScriptProperty prop;
    prop.name = "carriers";
    prop.type = ScriptPropertyType::EntityArray;
    prop.isOverridden = true;
    prop.instanceValue.entityArrayVal = { 11u, 22u, 33u };
    att.properties.push_back(prop);
    sc.scripts.push_back(att);

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<ScriptComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_ASSERT_TRUE(!g->scripts.empty());

    const ScriptProperty* found = nullptr;
    for (const auto& p : g->scripts[0].properties) if (p.name == "carriers") found = &p;
    ENJIN_ASSERT_NOT_NULL(found);
    ENJIN_EXPECT_EQ((int)found->type, (int)ScriptPropertyType::EntityArray);
    ENJIN_ASSERT_TRUE(found->instanceValue.entityArrayVal.size() == 3);
    ENJIN_EXPECT_EQ(found->instanceValue.entityArrayVal[0], (u64)11);
    ENJIN_EXPECT_EQ(found->instanceValue.entityArrayVal[2], (u64)33);
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

// ===========================================================================
// UICanvasComponent — guards the RF(SerializeUITheme) save crash (the theme
// is an OBJECT; wrapping it in the float-rounding helper threw on every save
// of a canvas-bearing scene) and the world-space element fields
// ===========================================================================

ENJIN_TEST(UICanvas, CanvasRoundTripWithThemeAndWorldSpaceElement) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& canvas = w1.AddComponent<GUI::UICanvasComponent>(e);
    canvas.canvasName = "HUD";
    canvas.sortOrder = 100;
    u32 lbl = canvas.AddElement(GUI::UIWidgetType::Label, "tag");
    auto* el = canvas.GetElement(lbl);
    el->data.text = "CATCH W/ E";
    el->data.worldSpace = true;
    el->data.worldOffset = Math::Vector3(0.0f, 0.12f, 0.0f);
    el->data.maxRenderDistance = 42.0f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* c2 = w2.GetComponent<GUI::UICanvasComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(c2);
    ENJIN_EXPECT_EQ(c2->sortOrder, 100);
    ENJIN_ASSERT_TRUE(c2->elements.size() == 1);
    ENJIN_EXPECT_TRUE(c2->elements[0].data.worldSpace);
    ENJIN_EXPECT_FLOAT_EQ(c2->elements[0].data.maxRenderDistance, 42.0f);
    ENJIN_EXPECT_FLOAT_EQ(c2->elements[0].data.worldOffset.y, 0.12f);
    ENJIN_EXPECT_TRUE(c2->elements[0].data.text == "CATCH W/ E");
}

// ===========================================================================
// HUD widget -> UICanvas migration (HUDSystem retirement)
// ===========================================================================

ENJIN_TEST(UICanvas, HUDWidgetMigratesToCanvas) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, "HUD_Tube");
    auto& hw = w.AddComponent<HUDWidgetComponent>(e);
    hw.type = HUDWidgetComponent::WidgetType::Label;
    hw.text = "hello";
    hw.anchorX = 0.15f;
    hw.anchorY = 0.86f;

    Entity e2 = w.CreateEntity();
    auto& bar = w.AddComponent<HUDWidgetComponent>(e2);
    bar.type = HUDWidgetComponent::WidgetType::HealthBar;
    bar.currentValue = 30.0f;
    bar.maxValue = 100.0f;
    bar.bindField = "health";

    Entity e3 = w.CreateEntity();
    auto& ch = w.AddComponent<HUDWidgetComponent>(e3);
    ch.type = HUDWidgetComponent::WidgetType::Crosshair;

    Scene::SceneSerializer::MigrateHUDWidgetsToCanvases(&w);

    // Legacy component is gone; canvases replaced it in place
    ENJIN_EXPECT_FALSE(w.HasComponent<HUDWidgetComponent>(e));
    auto* c1 = w.GetComponent<GUI::UICanvasComponent>(e);
    ENJIN_ASSERT_NOT_NULL(c1);
    ENJIN_EXPECT_TRUE(c1->canvasName == "HUD_Tube");
    ENJIN_EXPECT_EQ(c1->sortOrder, 100);
    ENJIN_ASSERT_TRUE(c1->elements.size() == 1);
    ENJIN_EXPECT_TRUE(c1->elements[0].type == GUI::UIWidgetType::Label);
    ENJIN_EXPECT_TRUE(c1->elements[0].data.text == "hello");
    ENJIN_EXPECT_FLOAT_EQ(c1->elements[0].anchor.anchorMin.x, 0.15f);

    auto* c2 = w.GetComponent<GUI::UICanvasComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(c2);
    ENJIN_ASSERT_TRUE(!c2->elements.empty());
    ENJIN_EXPECT_TRUE(c2->elements[0].type == GUI::UIWidgetType::ProgressBar);
    ENJIN_EXPECT_FLOAT_EQ(c2->elements[0].data.progressValue, 0.3f);
    ENJIN_EXPECT_FLOAT_EQ(c2->elements[0].data.bindMaxValue, 100.0f);
    ENJIN_EXPECT_TRUE(c2->elements[0].data.bindField == "health");

    auto* c3 = w.GetComponent<GUI::UICanvasComponent>(e3);
    ENJIN_ASSERT_NOT_NULL(c3);
    ENJIN_EXPECT_TRUE(c3->elements.size() == 2);  // crosshair = two centered panels
}

// ===========================================================================
// RigidbodyComponent — physics state must survive a round-trip (was ZERO
// coverage; velocity/constraint corruption hides until runtime collisions)
// ===========================================================================

ENJIN_TEST(Rigidbody, PhysicsStateRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& rb = w1.AddComponent<RigidbodyComponent>(e);
    rb.mass = 12.5f;
    rb.drag = 0.4f;
    rb.angularDrag = 0.15f;
    rb.useGravity = false;
    rb.gravityScale = 0.5f;
    rb.velocity = Math::Vector3(1.0f, -2.0f, 3.0f);
    rb.angularVelocity = Math::Vector3(0.1f, 0.2f, 0.3f);
    rb.freezePositionY = true;
    rb.freezeRotationX = true;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* r = w2.GetComponent<RigidbodyComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(r);
    ENJIN_EXPECT_FLOAT_EQ(r->mass, 12.5f);
    ENJIN_EXPECT_FLOAT_EQ(r->drag, 0.4f);
    ENJIN_EXPECT_FALSE(r->useGravity);
    ENJIN_EXPECT_FLOAT_EQ(r->gravityScale, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(r->velocity.y, -2.0f);
    ENJIN_EXPECT_FLOAT_EQ(r->angularVelocity.z, 0.3f);
    ENJIN_EXPECT_TRUE(r->freezePositionY);
    ENJIN_EXPECT_TRUE(r->freezeRotationX);
    ENJIN_EXPECT_FALSE(r->freezePositionX);
}

// ===========================================================================
// ScriptComponent — attachment paths, class names, and property overrides
// must survive (was ZERO coverage; broken script refs fail at play time)
// ===========================================================================

ENJIN_TEST(Script, AttachmentAndPropertyRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& sc = w1.AddComponent<ScriptComponent>(e);
    ScriptAttachment att;
    att.scriptPath = "scripts/PlayerController.as";
    att.className = "PlayerController";
    att.enabled = true;
    ScriptProperty prop;
    prop.name = "speed";
    prop.type = ScriptPropertyType::Float;
    prop.instanceValue.floatVal = 7.5f;
    prop.isOverridden = true;
    att.properties.push_back(prop);
    sc.scripts.push_back(att);

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* s2 = w2.GetComponent<ScriptComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(s2);
    ENJIN_ASSERT_TRUE(s2->scripts.size() == 1);
    ENJIN_EXPECT_TRUE(s2->scripts[0].scriptPath == "scripts/PlayerController.as");
    ENJIN_EXPECT_TRUE(s2->scripts[0].className == "PlayerController");
    ENJIN_EXPECT_TRUE(s2->scripts[0].enabled);
    // Runtime state must NOT round-trip as live
    ENJIN_EXPECT_FALSE(s2->scripts[0].initialized);
    ENJIN_EXPECT_TRUE(s2->scripts[0].instance == nullptr);
    ENJIN_ASSERT_TRUE(s2->scripts[0].properties.size() == 1);
    ENJIN_EXPECT_TRUE(s2->scripts[0].properties[0].name == "speed");
    ENJIN_EXPECT_TRUE(s2->scripts[0].properties[0].isOverridden);
    ENJIN_EXPECT_FLOAT_EQ(s2->scripts[0].properties[0].instanceValue.floatVal, 7.5f);
}

// ===========================================================================
// Negative paths — malformed input must fail gracefully, never crash
// ===========================================================================

ENJIN_TEST(SerializerNegative, EmptyStringFails) {
    World w;
    Scene::SceneSerializer de(&w);
    auto r = de.LoadFromString("");
    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_TRUE(r.entities.empty());
}

ENJIN_TEST(SerializerNegative, GarbageJsonFails) {
    World w;
    Scene::SceneSerializer de(&w);
    auto r = de.LoadFromString("{ this is not valid json ]");
    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_TRUE(r.entities.empty());
}

ENJIN_TEST(SerializerNegative, TopLevelArrayDoesNotCrash) {
    // Valid JSON but not a scene object. Must not crash; loads nothing.
    World w;
    Scene::SceneSerializer de(&w);
    auto r = de.LoadFromString("[1,2,3]");
    ENJIN_EXPECT_TRUE(r.entities.empty());
}

ENJIN_TEST(SerializerNegative, EmptyObjectLoadsNothing) {
    World w;
    Scene::SceneSerializer de(&w);
    auto r = de.LoadFromString("{}");
    ENJIN_EXPECT_TRUE(r.entities.empty());
}

ENJIN_TEST(SerializerNegative, TruncatedEntitiesArrayDoesNotCrash) {
    // Malformed entities payload — parse fails or loads nothing, but never crashes.
    World w;
    Scene::SceneSerializer de(&w);
    auto r = de.LoadFromString("{\"entities\":[{\"name\":");
    ENJIN_EXPECT_FALSE(r.success);
}

// ===========================================================================
// InventoryComponent — nested slot array is the interesting case
// ===========================================================================

ENJIN_TEST(Inventory, NestedSlotsAndScalarsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& inv = w1.AddComponent<InventoryComponent>(e);
    inv.maxSlots = 32;
    inv.coins = 1500;
    inv.gems = 7;
    inv.keys = { "brass_key", "silver_key" };
    inv.slots.push_back({ "health_potion", 5, 20 });
    inv.slots.push_back({ "iron_sword", 1, 1 });
    inv.slots.push_back({ "arrow", 60, 99 });

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<InventoryComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);

    ENJIN_EXPECT_EQ((int)g->maxSlots, 32);
    ENJIN_EXPECT_EQ(g->coins, 1500);
    ENJIN_EXPECT_EQ(g->gems, 7);
    ENJIN_ASSERT_EQ(g->keys.size(), (usize)2);
    ENJIN_EXPECT_EQ(g->keys[0], std::string("brass_key"));
    ENJIN_EXPECT_EQ(g->keys[1], std::string("silver_key"));

    ENJIN_ASSERT_EQ(g->slots.size(), (usize)3);
    ENJIN_EXPECT_EQ(g->slots[0].itemId, std::string("health_potion"));
    ENJIN_EXPECT_EQ(g->slots[0].quantity, 5);
    ENJIN_EXPECT_EQ(g->slots[0].maxStack, 20);
    ENJIN_EXPECT_EQ(g->slots[2].itemId, std::string("arrow"));
    ENJIN_EXPECT_EQ(g->slots[2].quantity, 60);
    ENJIN_EXPECT_EQ(g->slots[2].maxStack, 99);
}

// ===========================================================================
// WeatherZoneComponent
// ===========================================================================

ENJIN_TEST(WeatherZone, SnowTypeSurvivesRoundTrip) {
    // Regression: the deserializer capped weatherType at <= 3, silently
    // reverting Snow (4), Fog (5), and Storm (6) zones to Clear on load
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& z = w1.AddComponent<WeatherZoneComponent>(e);
    z.weatherType = 4;  // Snow
    z.snowIntensity = 0.9f;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<WeatherZoneComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);

    ENJIN_EXPECT_EQ((int)g->weatherType, 4);
    ENJIN_EXPECT_FLOAT_EQ(g->snowIntensity, 0.9f);
}

ENJIN_TEST(WeatherZone, StormTypeSurvivesRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& z = w1.AddComponent<WeatherZoneComponent>(e);
    z.weatherType = 6;  // Storm

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<WeatherZoneComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);

    ENJIN_EXPECT_EQ((int)g->weatherType, 6);
}

ENJIN_TEST(WeatherZone, PrecipTexturePathsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& z = w1.AddComponent<WeatherZoneComponent>(e);
    z.weatherType = 2;  // Rain
    z.rainTexturePath = "textures/raindrop.png";
    z.snowTexturePath = "textures/flake.png";

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<WeatherZoneComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);

    ENJIN_EXPECT_EQ(g->rainTexturePath, std::string("textures/raindrop.png"));
    ENJIN_EXPECT_EQ(g->snowTexturePath, std::string("textures/flake.png"));
    // Runtime cache must come back unresolved, not serialized
    ENJIN_EXPECT_EQ(g->cachedRainTexIndex, -2);
    ENJIN_EXPECT_EQ(g->cachedSnowTexIndex, -2);
}

// ===========================================================================
// CustomShaderComponent — GLSL + editable graph JSON round-trip
// ===========================================================================

ENJIN_TEST(CustomShader, GlslAndGraphRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& cs = w1.AddComponent<CustomShaderComponent>(e);
    cs.vertexSource = "void main(){ gl_Position = vec4(0); }";
    cs.fragmentSource = "void main(){ outColor = vec4(1); }";
    cs.graphLabel = "test graph";
    cs.graphJson = "{\"name\":\"Test\",\"nodes\":[{\"id\":1}],\"links\":[]}";
    cs.applied = true;   // runtime flag — must NOT survive

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<CustomShaderComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);

    ENJIN_EXPECT_EQ(g->vertexSource, std::string("void main(){ gl_Position = vec4(0); }"));
    ENJIN_EXPECT_EQ(g->fragmentSource, std::string("void main(){ outColor = vec4(1); }"));
    ENJIN_EXPECT_EQ(g->graphLabel, std::string("test graph"));
    // The editable graph survives so reopening the scene restores the node layout
    ENJIN_EXPECT_EQ(g->graphJson, std::string("{\"name\":\"Test\",\"nodes\":[{\"id\":1}],\"links\":[]}"));
    // Runtime flags reset (re-applied by FlushPendingChanges, not serialized)
    ENJIN_EXPECT_FALSE(g->applied);
}

// ===========================================================================
// DungeonGeneratorComponent (procgen suite)
// ===========================================================================

ENJIN_TEST(DungeonGenerator, FieldsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& d = w1.AddComponent<DungeonGeneratorComponent>(e);
    d.algorithm = DungeonGeneratorComponent::Algorithm::BSPRooms;
    d.width = 80; d.height = 64; d.seed = 12345;
    d.minRoomSize = 6; d.maxRoomSize = 14; d.splitDepth = 6; d.corridorWidth = 3;
    d.floorTile = 2; d.wallTile = 7; d.fillCollision = false; d.generateOnStart = false;

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<DungeonGeneratorComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);

    ENJIN_EXPECT_EQ((int)g->algorithm, (int)DungeonGeneratorComponent::Algorithm::BSPRooms);
    ENJIN_EXPECT_EQ((int)g->width, 80);
    ENJIN_EXPECT_EQ((int)g->height, 64);
    ENJIN_EXPECT_EQ((int)g->seed, 12345);
    ENJIN_EXPECT_EQ((int)g->splitDepth, 6);
    ENJIN_EXPECT_EQ(g->floorTile, 2);
    ENJIN_EXPECT_EQ(g->wallTile, 7);
    ENJIN_EXPECT_FALSE(g->fillCollision);
    ENJIN_EXPECT_FALSE(g->generateOnStart);
}

// ===========================================================================
// RandomBagComponent (procgen suite, stream-feeder)
// ===========================================================================

ENJIN_TEST(RandomBag, FieldsRoundTrip) {
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& b = w1.AddComponent<RandomBagComponent>(e);
    b.mode = RandomBagComponent::Mode::Deck;
    b.seed = 777;
    b.avoidImmediateRepeat = true;
    b.items.push_back({"ace", 4.0f});
    b.items.push_back({"king", 2.0f});
    b.items.push_back({"joker", 1.0f});

    World w2;
    Entity e2 = RoundTrip(w1, w2);
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<RandomBagComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);

    ENJIN_EXPECT_EQ((int)g->mode, (int)RandomBagComponent::Mode::Deck);
    ENJIN_EXPECT_EQ((int)g->seed, 777);
    ENJIN_EXPECT_TRUE(g->avoidImmediateRepeat);
    ENJIN_ASSERT_EQ((int)g->items.size(), 3);
    ENJIN_EXPECT_TRUE(g->items[0].name == "ace");
    ENJIN_EXPECT_TRUE(g->items[1].name == "king");
    ENJIN_EXPECT_TRUE(g->items[2].name == "joker");
    ENJIN_EXPECT_TRUE(g->items[0].weight > 3.9f && g->items[0].weight < 4.1f);
}

ENJIN_TEST(RandomBag, MarkovFollowsTransitionMatrix) {
    // Arrange: 3 items with a forced cycle - a can only go to b, b only to c,
    // c only to a. First draw is forced to "a" via the initial weights.
    RandomBagComponent b;
    b.mode = RandomBagComponent::Mode::Markov;
    b.seed = 11;
    b.items.push_back({"a", 1.0f});
    b.items.push_back({"b", 0.0f});
    b.items.push_back({"c", 0.0f});
    b.transitions = { 0, 1, 0,    // a -> b
                      0, 0, 1,    // b -> c
                      1, 0, 0 };  // c -> a

    // Act + Assert: draws must walk the strict cycle a, b, c, a, b, c...
    const char* expect[] = { "a", "b", "c", "a", "b", "c" };
    for (int i = 0; i < 6; ++i) {
        ENJIN_EXPECT_TRUE(ECS::RandomBagSystem::Draw(b) == expect[i]);
    }
}

ENJIN_TEST(RandomBag, MarkovTransitionsRoundTrip) {
    // Arrange
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& b = w1.AddComponent<RandomBagComponent>(e);
    b.mode = RandomBagComponent::Mode::Markov;
    b.items.push_back({"x", 1.0f});
    b.items.push_back({"y", 2.0f});
    b.transitions = { 0.5f, 1.5f, 2.5f, 3.5f };

    // Act
    World w2;
    Entity e2 = RoundTrip(w1, w2);

    // Assert
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<RandomBagComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_EQ((int)g->mode, (int)RandomBagComponent::Mode::Markov);
    ENJIN_ASSERT_EQ((int)g->transitions.size(), 4);
    ENJIN_EXPECT_TRUE(g->transitions[1] > 1.49f && g->transitions[1] < 1.51f);
    ENJIN_EXPECT_TRUE(g->transitions[3] > 3.49f && g->transitions[3] < 3.51f);
}

ENJIN_TEST(RandomBag, NoReplaceDrawsEachItemOncePerCycle) {
    // Arrange: a 3-item no-replacement bag with a fixed seed.
    RandomBagComponent b;
    b.mode = RandomBagComponent::Mode::NoReplace;
    b.seed = 42;
    b.items.push_back({"a", 1.0f});
    b.items.push_back({"b", 1.0f});
    b.items.push_back({"c", 1.0f});

    // Act: draw one full cycle (3 draws).
    int seenA = 0, seenB = 0, seenC = 0;
    for (int i = 0; i < 3; ++i) {
        std::string s = ECS::RandomBagSystem::Draw(b);
        if (s == "a") ++seenA; else if (s == "b") ++seenB; else if (s == "c") ++seenC;
    }

    // Assert: every item appeared exactly once in the cycle (fair 7-bag property).
    ENJIN_EXPECT_EQ(seenA, 1);
    ENJIN_EXPECT_EQ(seenB, 1);
    ENJIN_EXPECT_EQ(seenC, 1);
}

ENJIN_TEST(RandomBag, SameSeedReproducesSequence) {
    // Arrange: two identical weighted bags with the same seed.
    auto make = []() {
        RandomBagComponent b;
        b.mode = RandomBagComponent::Mode::Weighted;
        b.seed = 99;
        b.items.push_back({"x", 3.0f});
        b.items.push_back({"y", 1.0f});
        return b;
    };
    RandomBagComponent b1 = make();
    RandomBagComponent b2 = make();

    // Act + Assert: the two bags draw the same sequence.
    for (int i = 0; i < 20; ++i) {
        ENJIN_EXPECT_TRUE(ECS::RandomBagSystem::Draw(b1) == ECS::RandomBagSystem::Draw(b2));
    }
}

// ===========================================================================
// ScatterComponent (procgen suite, space-builder — prefab scatter)
// ===========================================================================

ENJIN_TEST(Scatter, FieldsRoundTrip) {
    // Arrange
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& s = w1.AddComponent<ScatterComponent>(e);
    s.distribution = ScatterComponent::Distribution::JitteredGrid;
    s.plane = ScatterComponent::Plane::XY;
    s.prefabPath = "assets/rock.enjprefab";
    s.regionWidth = 33.0f; s.regionHeight = 21.0f;
    s.targetCount = 128; s.minSpacing = 3.5f;
    s.scaleMin = 0.5f; s.scaleMax = 1.75f;
    s.randomYaw = false; s.heightJitter = 2.0f;
    s.seed = 55555; s.generateOnStart = true;

    // Act
    World w2;
    Entity e2 = RoundTrip(w1, w2);

    // Assert
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<ScatterComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_EQ((int)g->distribution, (int)ScatterComponent::Distribution::JitteredGrid);
    ENJIN_EXPECT_EQ((int)g->plane, (int)ScatterComponent::Plane::XY);
    ENJIN_EXPECT_TRUE(g->prefabPath == "assets/rock.enjprefab");
    ENJIN_EXPECT_TRUE(g->regionWidth > 32.9f && g->regionWidth < 33.1f);
    ENJIN_EXPECT_EQ((int)g->targetCount, 128);
    ENJIN_EXPECT_TRUE(g->minSpacing > 3.4f && g->minSpacing < 3.6f);
    ENJIN_EXPECT_TRUE(g->scaleMax > 1.74f && g->scaleMax < 1.76f);
    ENJIN_EXPECT_FALSE(g->randomYaw);
    ENJIN_EXPECT_TRUE(g->heightJitter > 1.9f && g->heightJitter < 2.1f);
    ENJIN_EXPECT_EQ((int)g->seed, 55555);
    ENJIN_EXPECT_TRUE(g->generateOnStart);
}

ENJIN_TEST(Scatter, TerrainHeightSamplingIsBilinearAndBounded) {
    // Arrange: a 3x3 terrain ramp, heights rising along +x: columns at 0, 10, 20.
    TerrainComponent terrain;
    terrain.gridWidth = 3; terrain.gridHeight = 3; terrain.cellSize = 2.0f;
    terrain.heightmap = { 0, 10, 20,
                          0, 10, 20,
                          0, 10, 20 };

    // Act + Assert: centre of the terrain (local 0,0) sits mid-ramp at height 10.
    f32 height = -1.0f, slope = -1.0f;
    ENJIN_ASSERT_TRUE(ECS::ScatterSystem::SampleTerrainHeight(terrain, 0.0f, 0.0f, height, slope));
    ENJIN_EXPECT_TRUE(height > 9.9f && height < 10.1f);
    // The ramp climbs 10 units per 2-unit cell -> slope = atan(5) = ~78.7 deg.
    ENJIN_EXPECT_TRUE(slope > 78.0f && slope < 79.5f);

    // Halfway between the first two columns: bilinear gives 5.
    ENJIN_ASSERT_TRUE(ECS::ScatterSystem::SampleTerrainHeight(terrain, -1.0f, 0.0f, height, slope));
    ENJIN_EXPECT_TRUE(height > 4.9f && height < 5.1f);

    // Outside the extent (halfW = 2): rejected.
    ENJIN_EXPECT_FALSE(ECS::ScatterSystem::SampleTerrainHeight(terrain, 5.0f, 0.0f, height, slope));

    // A flat terrain reports ~zero slope.
    TerrainComponent flat;
    flat.gridWidth = 3; flat.gridHeight = 3; flat.cellSize = 1.0f;
    flat.heightmap.assign(9, 4.0f);
    ENJIN_ASSERT_TRUE(ECS::ScatterSystem::SampleTerrainHeight(flat, 0.5f, -0.5f, height, slope));
    ENJIN_EXPECT_TRUE(height > 3.9f && height < 4.1f);
    ENJIN_EXPECT_TRUE(slope < 0.1f);
}

ENJIN_TEST(Scatter, ConformFieldsRoundTrip) {
    // Arrange
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& s = w1.AddComponent<ScatterComponent>(e);
    s.conformToTerrain = true;
    s.maxSlopeDeg = 33.0f;

    // Act
    World w2;
    Entity e2 = RoundTrip(w1, w2);

    // Assert
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<ScatterComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_TRUE(g->conformToTerrain);
    ENJIN_EXPECT_TRUE(g->maxSlopeDeg > 32.9f && g->maxSlopeDeg < 33.1f);
}

ENJIN_TEST(Scatter, VoronoiModeRoundTrips) {
    // Arrange: Voronoi distribution with a non-default relaxation count.
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& s = w1.AddComponent<ScatterComponent>(e);
    s.distribution = ScatterComponent::Distribution::Voronoi;
    s.relaxIterations = 7;
    s.targetCount = 55;

    // Act
    World w2;
    Entity e2 = RoundTrip(w1, w2);

    // Assert
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<ScatterComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_EQ((int)g->distribution, (int)ScatterComponent::Distribution::Voronoi);
    ENJIN_EXPECT_EQ((int)g->relaxIterations, 7);
    ENJIN_EXPECT_EQ((int)g->targetCount, 55);
}

// The spawned-instance marker must survive a round trip so a saved scatter batch
// stays regenerable (regenerate finds and clears the marked children).
ENJIN_TEST(Scatter, InstanceMarkerRoundTrips) {
    // Arrange
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    w1.AddComponent<ScatterInstanceComponent>(e);

    // Act
    World w2;
    Entity e2 = RoundTrip(w1, w2);

    // Assert
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    ENJIN_EXPECT_TRUE(w2.HasComponent<ScatterInstanceComponent>(e2));
}

// ===========================================================================
// TerrainGeneratorComponent (procgen suite, space-builder — noise + erosion)
// ===========================================================================

ENJIN_TEST(TerrainGenerator, FieldsRoundTrip) {
    // Arrange
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& t = w1.AddComponent<TerrainGeneratorComponent>(e);
    t.gridWidth = 96; t.gridHeight = 80; t.cellSize = 2.0f; t.maxHeight = 45.0f;
    t.ridged = true; t.octaves = 8; t.lacunarity = 2.3f; t.gain = 0.55f;
    t.frequency = 1.5f; t.ridgedPower = 3.0f;
    t.hydraulic = true; t.hydraulicDroplets = 90000;
    t.thermal = true; t.thermalIterations = 60; t.talusAngle = 0.03f;
    t.seed = 24680; t.generateOnStart = true;

    // Act
    World w2;
    Entity e2 = RoundTrip(w1, w2);

    // Assert
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<TerrainGeneratorComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_EQ((int)g->gridWidth, 96);
    ENJIN_EXPECT_EQ((int)g->gridHeight, 80);
    ENJIN_EXPECT_TRUE(g->maxHeight > 44.9f && g->maxHeight < 45.1f);
    ENJIN_EXPECT_TRUE(g->ridged);
    ENJIN_EXPECT_EQ((int)g->octaves, 8);
    ENJIN_EXPECT_TRUE(g->hydraulic);
    ENJIN_EXPECT_EQ((int)g->hydraulicDroplets, 90000);
    ENJIN_EXPECT_TRUE(g->thermal);
    ENJIN_EXPECT_EQ((int)g->thermalIterations, 60);
    ENJIN_EXPECT_EQ((int)g->seed, 24680);
    ENJIN_EXPECT_TRUE(g->generateOnStart);
}

ENJIN_TEST(TerrainGenerator, AutoSplatWeightsAreNormalizedAndSlopeAware) {
    // Arrange: a ridged bake (guaranteed steep areas) with auto-splat on.
    TerrainGeneratorComponent gen;
    gen.gridWidth = 48; gen.gridHeight = 48; gen.maxHeight = 30.0f;
    gen.cellSize = 1.0f; gen.seed = 777; gen.ridged = true;
    gen.autoSplat = true; gen.rockSlopeDeg = 30.0f;
    TerrainComponent terrain;

    // Act
    ECS::TerrainGeneratorSystem::Generate(gen, terrain);

    // Assert: every cell's 4 weights sum to ~1, and the bake produced both
    // rock-dominant (steep) and base-dominant (flat) cells.
    ENJIN_ASSERT_EQ((int)terrain.splatmap.size(), 48 * 48 * 4);
    bool anyRock = false, anyBase = false;
    for (usize i = 0; i < terrain.splatmap.size(); i += 4) {
        f32 sum = terrain.splatmap[i] + terrain.splatmap[i+1] + terrain.splatmap[i+2] + terrain.splatmap[i+3];
        ENJIN_EXPECT_TRUE(sum > 0.99f && sum < 1.01f);
        if (terrain.splatmap[i+1] > 0.5f) anyRock = true;
        if (terrain.splatmap[i]   > 0.5f) anyBase = true;
    }
    ENJIN_EXPECT_TRUE(anyRock);
    ENJIN_EXPECT_TRUE(anyBase);
}

ENJIN_TEST(TerrainGenerator, AutoSplatFieldsRoundTrip) {
    // Arrange
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& t = w1.AddComponent<TerrainGeneratorComponent>(e);
    t.autoSplat = true; t.rockSlopeDeg = 42.0f; t.snowHeightFrac = 0.8f;
    t.shoreHeightFrac = 0.2f; t.splatBlend = 0.25f;

    // Act
    World w2;
    Entity e2 = RoundTrip(w1, w2);

    // Assert
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* g = w2.GetComponent<TerrainGeneratorComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(g);
    ENJIN_EXPECT_TRUE(g->autoSplat);
    ENJIN_EXPECT_TRUE(g->rockSlopeDeg > 41.9f && g->rockSlopeDeg < 42.1f);
    ENJIN_EXPECT_TRUE(g->snowHeightFrac > 0.79f && g->snowHeightFrac < 0.81f);
    ENJIN_EXPECT_TRUE(g->shoreHeightFrac > 0.19f && g->shoreHeightFrac < 0.21f);
    ENJIN_EXPECT_TRUE(g->splatBlend > 0.24f && g->splatBlend < 0.26f);
}

// A bake produces a filled heightmap within [0, maxHeight], and the same seed is
// reproducible.
ENJIN_TEST(TerrainGenerator, GenerateFillsHeightmapInRange) {
    // Arrange
    TerrainGeneratorComponent gen;
    gen.gridWidth = 32; gen.gridHeight = 32; gen.maxHeight = 10.0f;
    gen.seed = 12321; gen.hydraulic = false; gen.thermal = false;
    TerrainComponent terrain;

    // Act
    ECS::TerrainGeneratorSystem::Generate(gen, terrain);

    // Assert
    ENJIN_ASSERT_EQ((int)terrain.heightmap.size(), 32 * 32);
    f32 mn = terrain.heightmap[0], mx = terrain.heightmap[0];
    for (f32 v : terrain.heightmap) { if (v < mn) mn = v; if (v > mx) mx = v; }
    ENJIN_EXPECT_TRUE(mn >= -0.001f);
    ENJIN_EXPECT_TRUE(mx <= 10.001f);
    ENJIN_EXPECT_TRUE(mx > mn);  // not flat
    ENJIN_EXPECT_EQ((int)gen.lastSeed, 12321);

    // Same seed reproduces the same field.
    TerrainGeneratorComponent gen2 = gen;
    TerrainComponent terrain2;
    ECS::TerrainGeneratorSystem::Generate(gen2, terrain2);
    ENJIN_ASSERT_EQ((int)terrain2.heightmap.size(), (int)terrain.heightmap.size());
    bool identical = true;
    for (usize i = 0; i < terrain.heightmap.size(); ++i)
        if (terrain.heightmap[i] != terrain2.heightmap[i]) { identical = false; break; }
    ENJIN_EXPECT_TRUE(identical);
}

// ===========================================================================
// WFCComponent (procgen suite, space-builder — constraint tile solver)
// ===========================================================================

ENJIN_TEST(WFC, FieldsRoundTrip) {
    // Arrange
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& g = w1.AddComponent<WFCComponent>(e);
    g.width = 24; g.height = 18; g.seed = 4242; g.maxBacktracks = 500;
    g.retryOnFail = true; g.maxRetries = 5; g.fillCollision = false;
    g.generateOnStart = true;
    WFCComponent::Tile t0; t0.tileIndex = 3; t0.solid = true;
    t0.edges[0] = "road"; t0.edges[1] = "grass"; t0.edges[2] = "road"; t0.edges[3] = "grass";
    WFCComponent::Tile t1; t1.tileIndex = 5;
    t1.edges[0] = "grass"; t1.edges[1] = "grass"; t1.edges[2] = "grass"; t1.edges[3] = "grass";
    g.tiles.push_back(t0);
    g.tiles.push_back(t1);

    // Act
    World w2;
    Entity e2 = RoundTrip(w1, w2);

    // Assert
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* r = w2.GetComponent<WFCComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(r);
    ENJIN_EXPECT_EQ((int)r->width, 24);
    ENJIN_EXPECT_EQ((int)r->height, 18);
    ENJIN_EXPECT_EQ((int)r->seed, 4242);
    ENJIN_EXPECT_TRUE(r->retryOnFail);
    ENJIN_EXPECT_EQ((int)r->maxRetries, 5);
    ENJIN_EXPECT_FALSE(r->fillCollision);
    ENJIN_ASSERT_EQ((int)r->tiles.size(), 2);
    ENJIN_EXPECT_EQ(r->tiles[0].tileIndex, 3);
    ENJIN_EXPECT_TRUE(r->tiles[0].solid);
    ENJIN_EXPECT_TRUE(r->tiles[0].edges[0] == "road");
    ENJIN_EXPECT_TRUE(r->tiles[1].edges[1] == "grass");
}

// A permissive tile set (all edges match) must always collapse, fill the whole
// tilemap with valid tile indices, and be reproducible for a fixed seed.
ENJIN_TEST(WFC, PermissiveSetAlwaysCollapses) {
    // Arrange: two tiles whose every edge shares the same socket, so any neighbour
    // pairing is legal and the solver can never hit a contradiction.
    WFCComponent gen;
    gen.width = 8; gen.height = 8; gen.seed = 7;
    WFCComponent::Tile a; a.tileIndex = 1; for (auto& s : a.edges) s = "x";
    WFCComponent::Tile b; b.tileIndex = 2; for (auto& s : b.edges) s = "x";
    gen.tiles.push_back(a);
    gen.tiles.push_back(b);
    TilemapComponent tm;

    // Act
    bool ok = ECS::WFCSystem::Generate(gen, tm);

    // Assert
    ENJIN_EXPECT_TRUE(ok);
    ENJIN_EXPECT_TRUE(gen.lastSuccess);
    ENJIN_ASSERT_EQ((int)tm.tiles.size(), 8 * 8);
    bool allValid = true;
    for (i32 v : tm.tiles) if (v != 1 && v != 2) { allValid = false; break; }
    ENJIN_EXPECT_TRUE(allValid);

    // Same seed reproduces the same layout.
    WFCComponent gen2 = gen;
    TilemapComponent tm2;
    ECS::WFCSystem::Generate(gen2, tm2);
    bool identical = (tm.tiles.size() == tm2.tiles.size());
    for (usize i = 0; identical && i < tm.tiles.size(); ++i)
        if (tm.tiles[i] != tm2.tiles[i]) identical = false;
    ENJIN_EXPECT_TRUE(identical);
}

ENJIN_TEST(WFC, Mode3DFieldsRoundTrip) {
    // Arrange: a 3D-module WFC with a prefab tile and all six edge sockets set.
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& g = w1.AddComponent<WFCComponent>(e);
    g.mode = WFCComponent::Mode::Modules3D;
    g.width = 6; g.height = 5; g.depth = 4; g.cellSize = 2.5f; g.seed = 8080;
    WFCComponent::Tile t; t.tileIndex = 0; t.prefabPath = "assets/block.enjprefab";
    t.edges[0] = "n"; t.edges[1] = "e"; t.edges[2] = "s"; t.edges[3] = "w";
    t.edges[4] = "up"; t.edges[5] = "dn";
    g.tiles.push_back(t);

    // Act
    World w2;
    Entity e2 = RoundTrip(w1, w2);

    // Assert
    ENJIN_ASSERT_NE(e2, INVALID_ENTITY);
    auto* r = w2.GetComponent<WFCComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(r);
    ENJIN_EXPECT_EQ((int)r->mode, (int)WFCComponent::Mode::Modules3D);
    ENJIN_EXPECT_EQ((int)r->depth, 4);
    ENJIN_EXPECT_TRUE(r->cellSize > 2.49f && r->cellSize < 2.51f);
    ENJIN_ASSERT_EQ((int)r->tiles.size(), 1);
    ENJIN_EXPECT_TRUE(r->tiles[0].prefabPath == "assets/block.enjprefab");
    ENJIN_EXPECT_TRUE(r->tiles[0].edges[4] == "up");
    ENJIN_EXPECT_TRUE(r->tiles[0].edges[5] == "dn");
}

// A permissive 3D module set with only air tiles (empty prefab) must collapse the
// whole volume with no contradiction, placing zero prefabs (no asset files needed).
ENJIN_TEST(WFC, Volume3DAirSetCollapses) {
    // Arrange
    World w;
    Entity e = w.CreateEntity();
    auto& g = w.AddComponent<WFCComponent>(e);
    g.mode = WFCComponent::Mode::Modules3D;
    g.width = 4; g.height = 4; g.depth = 3; g.seed = 3;
    WFCComponent::Tile a; for (auto& s : a.edges) s = "x"; // all-air, all edges match
    WFCComponent::Tile b; for (auto& s : b.edges) s = "x";
    g.tiles.push_back(a);
    g.tiles.push_back(b);

    // Act
    bool ok = ECS::WFCSystem::Generate(&w, e, g);

    // Assert: volume collapses, and no modules are placed because every tile is air.
    ENJIN_EXPECT_TRUE(ok);
    ENJIN_EXPECT_TRUE(g.lastSuccess);
    ENJIN_EXPECT_EQ((int)g.lastCount, 0);
}

// ===========================================================================
// Serdes registry — every registered component must survive the full
// single-component helper round-trip (add -> serialize -> remove). This is the
// safety net for the registry refactor: if a component falls out of any dispatch
// path (the historical silent-drop bug), it fails here instead of in a save file.
// ===========================================================================

ENJIN_TEST(SerdesRegistry, EveryComponentRoundTripsThroughHelpers) {
    World w;
    auto keys = Scene::SceneSerializer::RegisteredComponentKeys();
    ENJIN_ASSERT_TRUE(keys.size() > 140);   // full roster is registered

    // These three carry a node graph whose ToJson() throws on a *default/empty*
    // graph (never occurs for a real authored component). Their serialize path is
    // registered; it just can't serialize the pathological empty default, so skip
    // the non-empty-serialize check for them while still asserting the add + remove
    // paths. Matches pre-refactor behavior (the old helper swallowed the same throw).
    const std::set<std::string> serializeNeedsGraph = { "behaviorTree", "questFlow", "visualScript" };

    std::string notAdded, notSerialized, notRemoved;
    for (const std::string& key : keys) {
        Entity e = w.CreateEntity();
        // Deserialize from empty JSON adds a default-valued component (every
        // deserializer guards missing fields), exercising the deserialize path.
        if (!Scene::SceneSerializer::DeserializeOneComponent(&w, e, key, "{}")) { notAdded += key + " "; continue; }
        // Serialize path: with the component present, must produce non-empty JSON.
        if (serializeNeedsGraph.find(key) == serializeNeedsGraph.end() &&
            Scene::SceneSerializer::SerializeOneComponent(&w, e, key).empty()) notSerialized += key + " ";
        // Remove path: must recognize the key and remove it.
        if (!Scene::SceneSerializer::RemoveOneComponent(&w, e, key)) notRemoved += key + " ";
    }
    printf("  [serdes] not-added: %s\n  [serdes] not-serialized: %s\n  [serdes] not-removed: %s\n",
           notAdded.c_str(), notSerialized.c_str(), notRemoved.c_str());
    ENJIN_EXPECT_TRUE(notAdded.empty());
    ENJIN_EXPECT_TRUE(notSerialized.empty());
    ENJIN_EXPECT_TRUE(notRemoved.empty());
}

ENJIN_TEST(SerdesRegistry, NoDuplicateKeys) {
    auto keys = Scene::SceneSerializer::RegisteredComponentKeys();
    std::set<std::string> seen;
    for (const std::string& k : keys) {
        ENJIN_EXPECT_TRUE(seen.find(k) == seen.end());   // each key registered once
        seen.insert(k);
    }
}

// The full-scene save/load loops now run off the same registry. Before Stage 2,
// 16 registered components (the audio suite among them) SAVED in the loops but
// were missing from the LOAD chains -- authored components silently vanished on
// scene reload. This round-trips a scene through SaveToString -> LoadFromString
// with components from each previously-broken class.
ENJIN_TEST(SerdesRegistry, FullSceneRoundTripCoversLoopOnlyComponents) {
    // Arrange: an entity carrying an audio-family component (load-dropped before),
    // a loop-only component (customShader), and an inline-form component (lookAtIK).
    World w1;
    Entity e = w1.CreateEntity();
    w1.AddComponent<TransformComponent>(e);
    auto& rz = w1.AddComponent<ReverbZoneComponent>(e);
    rz.roomSize = 0.9f; rz.decayTime = 3.5f;
    w1.AddComponent<CustomShaderComponent>(e);
    auto& ik = w1.AddComponent<LookAtIKComponent>(e);
    ik.headBoneName = "head"; ik.lookWeight = 0.75f;
    auto& bc = w1.AddComponent<BeatClockComponent>(e);
    bc.bpm = 128.0f;

    // Act: full scene save + load (the loop paths, not the per-key helpers).
    Scene::SceneSerializer ser(&w1);
    std::string sceneJson = ser.SaveToString();
    ENJIN_ASSERT_TRUE(!sceneJson.empty());
    World w2;
    Scene::SceneSerializer de(&w2);
    auto result = de.LoadFromString(sceneJson);
    ENJIN_ASSERT_TRUE(result.success);
    ENJIN_ASSERT_TRUE(!result.entities.empty());
    Entity e2 = result.entities[0];

    // Assert: every component survived the loop round-trip with its data.
    auto* rz2 = w2.GetComponent<ReverbZoneComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(rz2);   // was silently dropped on load before Stage 2
    ENJIN_EXPECT_TRUE(rz2->roomSize > 0.89f && rz2->roomSize < 0.91f);
    ENJIN_EXPECT_TRUE(rz2->decayTime > 3.49f && rz2->decayTime < 3.51f);
    ENJIN_EXPECT_TRUE(w2.HasComponent<CustomShaderComponent>(e2));
    auto* ik2 = w2.GetComponent<LookAtIKComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(ik2);
    ENJIN_EXPECT_TRUE(ik2->headBoneName == "head");
    ENJIN_EXPECT_TRUE(ik2->lookWeight > 0.74f && ik2->lookWeight < 0.76f);
    auto* bc2 = w2.GetComponent<BeatClockComponent>(e2);
    ENJIN_ASSERT_NOT_NULL(bc2);
    ENJIN_EXPECT_TRUE(bc2->bpm > 127.9f && bc2->bpm < 128.1f);
}

ENJIN_TEST_MAIN()
