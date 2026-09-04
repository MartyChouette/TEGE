// Components a designer configures must survive a save.
//
// An audit found six components with full editor UI, live systems and no
// serializer at all: set a value, save, reload, it is gone. SaveSystemComponent
// was the sharpest case - its own header says it is exposed to "the editor
// inspector, serialization, and scripting", and serialization was the missing
// third. BoundaryPolygonComponent holds an outline the designer drags out in the
// viewport by hand.
//
// These tests round-trip each component through the real SceneSerializer, so a
// field added to the struct and forgotten in the serializer fails here rather
// than silently discarding someone's work.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/BoundaryPolygon.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/DynamicDifficulty.h"
#include "Enjin/ECS/Components/GPUParticleEmitter.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/Ladder.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/Animation/Timeline.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Scene/SceneSerializer.h"
#include <cmath>
#include <variant>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

namespace {

bool Near(f32 a, f32 b, f32 eps = 0.001f) { return std::fabs(a - b) < eps; }

// Save one entity to a string and load it into a fresh world.
Entity RoundTrip(World& src, Entity e, World& dst) {
    const std::string json = Scene::SceneSerializer::SerializeEntityToString(&src, e, false);
    return Scene::SceneSerializer::DeserializeEntityFromString(&dst, json);
}

// Vertex data is off in RoundTrip so the component tests stay small; a mesh test
// has to ask for it.
Entity RoundTripWithVertices(World& src, Entity e, World& dst) {
    const std::string json = Scene::SceneSerializer::SerializeEntityToString(&src, e, true);
    return Scene::SceneSerializer::DeserializeEntityFromString(&dst, json);
}

Entity Base(World& w) {
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"Subject"});
    w.AddComponent<TransformComponent>(e, TransformComponent{});
    return e;
}

} // namespace

ENJIN_TEST(SerdesCoverage, SaveSystemConfigSurvivesASave) {
    // Arrange: a non-default configuration, the kind someone actually sets.
    World src;
    Entity e = Base(src);
    SaveSystemComponent c;
    c.maxManualSlots = 5;
    c.allowManualSave = false;
    c.autoSaveEnabled = true;
    c.autoSaveIntervalSeconds = 90.0f;
    c.autoSaveSlotCount = 7;
    c.savePointRadius = 3.5f;
    c.savePointKey = 70;
    c.enableCloudSync = true;
    c.saveIndicatorDuration = 4.25f;
    src.AddComponent<SaveSystemComponent>(e, c);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<SaveSystemComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_EQ(r->maxManualSlots, 5u);
    ENJIN_EXPECT_FALSE(r->allowManualSave);
    ENJIN_EXPECT_TRUE(r->autoSaveEnabled);
    ENJIN_EXPECT_TRUE(Near(r->autoSaveIntervalSeconds, 90.0f));
    ENJIN_EXPECT_EQ(r->autoSaveSlotCount, 7u);
    ENJIN_EXPECT_TRUE(Near(r->savePointRadius, 3.5f));
    ENJIN_EXPECT_EQ(r->savePointKey, 70);
    ENJIN_EXPECT_TRUE(r->enableCloudSync);
    ENJIN_EXPECT_TRUE(Near(r->saveIndicatorDuration, 4.25f));
}

ENJIN_TEST(SerdesCoverage, SavePointSurvivesASave) {
    World src;
    Entity e = Base(src);
    SavePointComponent c;
    c.slotTarget = 3;
    c.saveOnEnter = true;
    c.oneTimeUse = true;
    c.radius = 4.5f;
    c.saveMessage = "Rest here";
    src.AddComponent<SavePointComponent>(e, c);

    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    const auto* r = dst.GetComponent<SavePointComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_EQ(r->slotTarget, 3);
    ENJIN_EXPECT_TRUE(r->saveOnEnter);
    ENJIN_EXPECT_TRUE(r->oneTimeUse);
    ENJIN_EXPECT_TRUE(Near(r->radius, 4.5f));
    ENJIN_EXPECT_TRUE(r->saveMessage == "Rest here");
}

ENJIN_TEST(SerdesCoverage, BoundaryPolygonPointsSurviveASave) {
    // The hand-dragged outline. Losing this means re-shaping it every session.
    World src;
    Entity e = Base(src);
    BoundaryPolygonComponent c;
    c.points = {Vector2(0.0f, 0.0f), Vector2(4.0f, 0.5f),
                Vector2(3.25f, -2.75f), Vector2(-1.5f, -2.0f)};
    src.AddComponent<BoundaryPolygonComponent>(e, c);

    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    const auto* r = dst.GetComponent<BoundaryPolygonComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_ASSERT_TRUE(r->points.size() == 4);
    ENJIN_EXPECT_TRUE(Near(r->points[1].x, 4.0f));
    ENJIN_EXPECT_TRUE(Near(r->points[1].y, 0.5f));
    ENJIN_EXPECT_TRUE(Near(r->points[2].x, 3.25f));
    ENJIN_EXPECT_TRUE(Near(r->points[3].y, -2.0f));
    // The dependent mesh must rebuild on load.
    ENJIN_EXPECT_TRUE(r->dirty);
}

ENJIN_TEST(SerdesCoverage, SwimTuningSurvivesASave) {
    // The swim feel fields, so a heavy character keeps its weaker stroke.
    World src;
    Entity e = Base(src);
    ThirdPersonController c;
    c.swimSpeedScale = 0.35f;
    c.swimStrokeImpulse = 5.5f;
    c.swimSinkRate = -1.25f;
    c.swimDrag = 6.0f;
    c.swimSurfaceBand = 0.4f;
    c.swimSurfaceStrokeScale = 0.15f;
    // and the two camera fields the audit found dropped
    c.cameraCollisionRadius = 0.75f;
    c.lockOnRange = 22.0f;
    src.AddComponent<ThirdPersonController>(e, c);

    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    const auto* r = dst.GetComponent<ThirdPersonController>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(Near(r->swimSpeedScale, 0.35f));
    ENJIN_EXPECT_TRUE(Near(r->swimStrokeImpulse, 5.5f));
    ENJIN_EXPECT_TRUE(Near(r->swimSinkRate, -1.25f));
    ENJIN_EXPECT_TRUE(Near(r->swimDrag, 6.0f));
    ENJIN_EXPECT_TRUE(Near(r->swimSurfaceBand, 0.4f));
    ENJIN_EXPECT_TRUE(Near(r->swimSurfaceStrokeScale, 0.15f));
    ENJIN_EXPECT_TRUE(Near(r->cameraCollisionRadius, 0.75f));
    ENJIN_EXPECT_TRUE(Near(r->lockOnRange, 22.0f));
}

ENJIN_TEST(SerdesCoverage, WaypointLinkSurvivesASave) {
    // nextWaypoint is the field that makes a path a path, and it was the one
    // waypoint field not saved.
    World src;
    Entity target = Base(src);
    Entity e = Base(src);
    WaypointComponent c;
    c.waypointId = 2;
    c.waitTime = 1.5f;
    c.nextWaypoint = target;
    src.AddComponent<WaypointComponent>(e, c);

    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    const auto* r = dst.GetComponent<WaypointComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(Near(r->waitTime, 1.5f));
    ENJIN_EXPECT_TRUE(r->nextWaypoint != INVALID_ENTITY);
}

ENJIN_TEST(SerdesCoverage, AmbientSoundLayersSurviveASave) {
    // A layered ambience zone: clips, captions and the time-of-day and weather
    // conditions that decide when each layer plays.
    World src;
    Entity e = Base(src);
    AmbientSoundLayerComponent c;
    AmbientSoundLayerComponent::Layer birds;
    birds.clipPath = "assets/sfx/birds.wav";
    birds.volume = 0.65f;
    birds.pitch = 1.1f;
    birds.caption = "[Birds chirping]";
    birds.minTimeOfDay = 6.0f;
    birds.maxTimeOfDay = 18.0f;
    AmbientSoundLayerComponent::Layer rain;
    rain.clipPath = "assets/sfx/rain.wav";
    rain.minWeatherIntensity = 0.4f;
    rain.loop = false;
    c.layers = {birds, rain};
    c.halfExtents = Vector3(12.0f, 4.0f, 9.0f);
    c.blendRadius = 6.5f;
    src.AddComponent<AmbientSoundLayerComponent>(e, c);

    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    const auto* r = dst.GetComponent<AmbientSoundLayerComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_ASSERT_TRUE(r->layers.size() == 2);
    ENJIN_EXPECT_TRUE(r->layers[0].clipPath == "assets/sfx/birds.wav");
    ENJIN_EXPECT_TRUE(r->layers[0].caption == "[Birds chirping]");
    ENJIN_EXPECT_TRUE(Near(r->layers[0].volume, 0.65f));
    ENJIN_EXPECT_TRUE(Near(r->layers[0].minTimeOfDay, 6.0f));
    ENJIN_EXPECT_TRUE(Near(r->layers[1].minWeatherIntensity, 0.4f));
    ENJIN_EXPECT_FALSE(r->layers[1].loop);
    ENJIN_EXPECT_TRUE(Near(r->blendRadius, 6.5f));
    ENJIN_EXPECT_TRUE(Near(r->halfExtents.x, 12.0f));
}

ENJIN_TEST(SerdesCoverage, LipSyncMorphMappingSurvivesASave) {
    // The viseme-to-morph-target table is the part that takes real authoring
    // effort, and it was indexed per viseme, so the array shape matters.
    World src;
    Entity e = Base(src);
    LipSyncComponent c;
    c.blendSpeed = 14.0f;
    c.autoFromAmplitude = false;
    LipSyncComponent::VisemeKey k;
    k.time = 0.25f; k.viseme = Viseme::AA; k.weight = 0.8f;
    c.visemeData.push_back(k);
    c.visemeMorphMap[static_cast<usize>(Viseme::AA)].push_back({"jawOpen", 0.8f});
    c.visemeMorphMap[static_cast<usize>(Viseme::AA)].push_back({"mouthOpen", 0.6f});
    c.visemeMorphMap[static_cast<usize>(Viseme::OH)].push_back({"mouthRound", 0.9f});
    src.AddComponent<LipSyncComponent>(e, c);

    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    const auto* r = dst.GetComponent<LipSyncComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(Near(r->blendSpeed, 14.0f));
    ENJIN_EXPECT_FALSE(r->autoFromAmplitude);
    ENJIN_ASSERT_TRUE(r->visemeData.size() == 1);
    ENJIN_EXPECT_TRUE(r->visemeData[0].viseme == Viseme::AA);
    ENJIN_EXPECT_TRUE(Near(r->visemeData[0].time, 0.25f));
    const auto& aa = r->visemeMorphMap[static_cast<usize>(Viseme::AA)];
    ENJIN_ASSERT_TRUE(aa.size() == 2);
    ENJIN_EXPECT_TRUE(aa[0].morphTargetName == "jawOpen");
    ENJIN_EXPECT_TRUE(Near(aa[1].weight, 0.6f));
    ENJIN_EXPECT_TRUE(r->visemeMorphMap[static_cast<usize>(Viseme::OH)].size() == 1);
    // A viseme nobody mapped must stay empty rather than pick up a neighbour's.
    ENJIN_EXPECT_TRUE(r->visemeMorphMap[static_cast<usize>(Viseme::FF)].empty());
}

ENJIN_TEST(SerdesCoverage, VisualScriptFunctionsSurviveASave) {
    // User-defined subgraphs. This is authored work with no other copy: if the
    // save drops it, it is gone.
    World src;
    Entity e = Base(src);
    VisualScriptComponent vs;

    VisualScriptFunction fn;
    fn.name = "ApplyKnockback";
    VisualScriptFunction::Parameter force;
    force.name = "force";
    force.type = Enjin::Editor::PinType::Float;
    force.defaultValue = 12.5f;
    VisualScriptFunction::Parameter flag;
    flag.name = "ignoreArmour";
    flag.type = Enjin::Editor::PinType::Bool;
    flag.defaultValue = true;
    fn.inputParams = {force, flag};
    VisualScriptFunction::Parameter out;
    out.name = "applied";
    out.type = Enjin::Editor::PinType::Bool;
    out.defaultValue = false;
    fn.outputParams = {out};
    vs.functions.push_back(fn);

    VisualScriptFunction fn2;
    fn2.name = "Heal";
    vs.functions.push_back(fn2);
    src.AddComponent<VisualScriptComponent>(e, vs);

    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    const auto* r = dst.GetComponent<VisualScriptComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_ASSERT_TRUE(r->functions.size() == 2);
    ENJIN_EXPECT_TRUE(r->functions[0].name == "ApplyKnockback");
    ENJIN_EXPECT_TRUE(r->functions[1].name == "Heal");

    // Parameters, including their default VALUES, which is the part that would
    // quietly come back as zero if only names and types were written.
    ENJIN_ASSERT_TRUE(r->functions[0].inputParams.size() == 2);
    ENJIN_EXPECT_TRUE(r->functions[0].inputParams[0].name == "force");
    ENJIN_EXPECT_TRUE(std::holds_alternative<f32>(r->functions[0].inputParams[0].defaultValue));
    ENJIN_EXPECT_TRUE(Near(std::get<f32>(r->functions[0].inputParams[0].defaultValue), 12.5f));
    ENJIN_EXPECT_TRUE(r->functions[0].inputParams[1].name == "ignoreArmour");
    ENJIN_EXPECT_TRUE(std::holds_alternative<bool>(r->functions[0].inputParams[1].defaultValue));
    ENJIN_EXPECT_TRUE(std::get<bool>(r->functions[0].inputParams[1].defaultValue));
    ENJIN_ASSERT_TRUE(r->functions[0].outputParams.size() == 1);
    ENJIN_EXPECT_TRUE(r->functions[0].outputParams[0].name == "applied");
}


// A second audit pass found tuning fields dropped from serializers that already
// existed - worse than a missing serializer, because the component looks saved.
// Each of these is a value a designer sets in the inspector and never sees again.

ENJIN_TEST(SerdesCoverage, TetherFeelTuningSurvivesASave) {
    // Arrange: the pluck/release/adaptive block, which sits above the runtime
    // divider next to an already-serialized spring block.
    World src;
    Entity e = Base(src);
    TetherComponent t;
    t.pluckDwellThreshold = 0.42f;
    t.pluckDwellSeconds = 1.75f;
    t.releasePopHighThreshold = 0.9f;
    t.releasePopLowThreshold = 0.15f;
    t.adaptiveMinSpringMult = 0.3f;
    t.adaptiveMaxSpringMult = 2.4f;
    t.adaptiveMinDamperMult = 0.6f;
    t.adaptiveMaxDamperMult = 1.9f;
    src.AddComponent<TetherComponent>(e, t);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<TetherComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(Near(r->pluckDwellThreshold, 0.42f));
    ENJIN_EXPECT_TRUE(Near(r->pluckDwellSeconds, 1.75f));
    ENJIN_EXPECT_TRUE(Near(r->releasePopHighThreshold, 0.9f));
    ENJIN_EXPECT_TRUE(Near(r->releasePopLowThreshold, 0.15f));
    ENJIN_EXPECT_TRUE(Near(r->adaptiveMinSpringMult, 0.3f));
    ENJIN_EXPECT_TRUE(Near(r->adaptiveMaxSpringMult, 2.4f));
    ENJIN_EXPECT_TRUE(Near(r->adaptiveMinDamperMult, 0.6f));
    ENJIN_EXPECT_TRUE(Near(r->adaptiveMaxDamperMult, 1.9f));
}

ENJIN_TEST(SerdesCoverage, FaceCardExpressionsSurviveASave) {
    // Arrange: the portrait set is authored art paths, the expensive part.
    World src;
    Entity e = Base(src);
    FaceCardComponent c;
    c.expressions["neutral"] = "art/faces/neutral.png";
    c.expressions["angry"] = "art/faces/angry.png";
    c.currentExpression = "angry";
    c.transitionDuration = 0.35f;
    c.flipX = true;
    c.enabled = false;
    src.AddComponent<FaceCardComponent>(e, c);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<FaceCardComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_ASSERT_TRUE(r->expressions.size() == 2);
    ENJIN_EXPECT_TRUE(r->expressions.at("neutral") == "art/faces/neutral.png");
    ENJIN_EXPECT_TRUE(r->expressions.at("angry") == "art/faces/angry.png");
    ENJIN_EXPECT_TRUE(r->currentExpression == "angry");
    ENJIN_EXPECT_TRUE(Near(r->transitionDuration, 0.35f));
    ENJIN_EXPECT_TRUE(r->flipX);
    ENJIN_EXPECT_TRUE(!r->enabled);
}

ENJIN_TEST(SerdesCoverage, LODHysteresisAndScreenSizeSurviveASave) {
    // Arrange: useScreenSize defaults ON, so a designer turning it OFF is the
    // case a missing field silently reverts.
    World src;
    Entity e = Base(src);
    LODComponent lod;
    lod.hysteresisRatio = 0.25f;
    lod.useScreenSize = false;
    src.AddComponent<LODComponent>(e, lod);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<LODComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(Near(r->hysteresisRatio, 0.25f));
    ENJIN_EXPECT_TRUE(!r->useScreenSize);
}

ENJIN_TEST(SerdesCoverage, DynamicDifficultyHintCooldownSurvivesASave) {
    // Arrange
    World src;
    Entity e = Base(src);
    DynamicDifficultyComponent dd;
    dd.adjustHintFrequency = true;
    dd.hintCooldown = 12.0f;
    src.AddComponent<DynamicDifficultyComponent>(e, dd);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<DynamicDifficultyComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(r->adjustHintFrequency);
    ENJIN_EXPECT_TRUE(Near(r->hintCooldown, 12.0f));
}

ENJIN_TEST(SerdesCoverage, GPUParticleBurstCountSurvivesASave) {
    // Arrange: burstCount is authored, burstNow is a runtime trigger and is
    // deliberately not saved.
    World src;
    Entity e = Base(src);
    GPUParticleEmitterComponent em;
    em.burstCount = 250;
    src.AddComponent<GPUParticleEmitterComponent>(e, em);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<GPUParticleEmitterComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(r->burstCount == 250u);
    ENJIN_EXPECT_TRUE(!r->burstNow);
}

ENJIN_TEST(SerdesCoverage, DestructibleDamageOverlaySurvivesASave) {
    // Arrange
    World src;
    Entity e = Base(src);
    DestructibleComponent dc;
    dc.showDamageOverlay = false;
    dc.crackTexturePath = "art/cracks/stone.png";
    dc.damageTint = Vector3(0.8f, 0.1f, 0.05f);
    src.AddComponent<DestructibleComponent>(e, dc);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<DestructibleComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(!r->showDamageOverlay);
    ENJIN_EXPECT_TRUE(r->crackTexturePath == "art/cracks/stone.png");
    ENJIN_EXPECT_TRUE(Near(r->damageTint.x, 0.8f));
    ENJIN_EXPECT_TRUE(Near(r->damageTint.z, 0.05f));
}

ENJIN_TEST(SerdesCoverage, HealthAndTimerNotifyLinksSurviveASave) {
    // Arrange: the wiring that makes a health bar or a trap actually do
    // something. TriggerZone already saved its links; these two did not.
    World src;
    Entity e = Base(src);
    HealthComponent h;
    h.onDamageNotify = 11;
    h.onDeathNotify = 22;
    h.onHealNotify = 33;
    src.AddComponent<HealthComponent>(e, h);
    TimerComponent t;
    t.onCompleteNotify = 44;
    src.AddComponent<TimerComponent>(e, t);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* rh = dst.GetComponent<HealthComponent>(loaded);
    ENJIN_ASSERT_TRUE(rh != nullptr);
    ENJIN_EXPECT_TRUE(rh->onDamageNotify == 11);
    ENJIN_EXPECT_TRUE(rh->onDeathNotify == 22);
    ENJIN_EXPECT_TRUE(rh->onHealNotify == 33);
    const auto* rt = dst.GetComponent<TimerComponent>(loaded);
    ENJIN_ASSERT_TRUE(rt != nullptr);
    ENJIN_EXPECT_TRUE(rt->onCompleteNotify == 44);
}


// A third audit pass, over all 166 serializers. These five are what it found.

ENJIN_TEST(SerdesCoverage, SecondUVChannelSurvivesASave) {
    // Arrange: a mesh written inline, which is what a copy/paste, an undo, or a
    // mesh whose source file moved all fall back to. glTF TEXCOORD_1 and Assimp
    // texCoord1 fill uv1, and it is a real vertex attribute the shader reads.
    World src;
    Entity e = Base(src);
    MeshComponent mesh;
    MeshComponent::Vertex v{};
    v.position = Vector3(1.0f, 2.0f, 3.0f);
    v.uv = Vector2(0.25f, 0.5f);
    v.uv1 = Vector2(0.75f, 0.125f);
    mesh.vertices.push_back(v);
    mesh.indices = {0};
    src.AddComponent<MeshComponent>(e, mesh);

    // Act
    World dst;
    Entity loaded = RoundTripWithVertices(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<MeshComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_ASSERT_TRUE(r->vertices.size() == 1);
    ENJIN_EXPECT_TRUE(Near(r->vertices[0].uv.x, 0.25f));
    ENJIN_EXPECT_TRUE(Near(r->vertices[0].uv1.x, 0.75f));
    ENJIN_EXPECT_TRUE(Near(r->vertices[0].uv1.y, 0.125f));
}

ENJIN_TEST(SerdesCoverage, Body2DPolygonGeometrySurvivesASave) {
    // Arrange: the shape type round-tripped already, so a polygon body used to
    // come back declaring itself a polygon with no vertices at all.
    World src;
    Entity e = Base(src);
    Enjin::Physics::Body2DComponent body;
    body.shapeType = Enjin::Physics::Shape2DType::Polygon;
    body.polygon.vertices = { Vector2(0.0f, 0.0f), Vector2(1.0f, 0.0f), Vector2(0.5f, 1.5f) };
    body.polygon.offset = Vector2(0.25f, -0.5f);
    src.AddComponent<Enjin::Physics::Body2DComponent>(e, body);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<Enjin::Physics::Body2DComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(r->shapeType == Enjin::Physics::Shape2DType::Polygon);
    ENJIN_ASSERT_TRUE(r->polygon.vertices.size() == 3);
    ENJIN_EXPECT_TRUE(Near(r->polygon.vertices[2].x, 0.5f));
    ENJIN_EXPECT_TRUE(Near(r->polygon.vertices[2].y, 1.5f));
    ENJIN_EXPECT_TRUE(Near(r->polygon.offset.x, 0.25f));
    ENJIN_EXPECT_TRUE(Near(r->polygon.offset.y, -0.5f));
}

ENJIN_TEST(SerdesCoverage, AnimatorOnionSkinSurvivesASave) {
    // Arrange: seven inspector controls set these and the viewport reads them.
    World src;
    Entity e = Base(src);
    AnimatorComponent anim;
    anim.onionSkin.enabled = true;
    anim.onionSkin.framesBefore = 5;
    anim.onionSkin.framesAfter = 2;
    anim.onionSkin.opacity = 0.4f;
    anim.onionSkin.opacityFalloff = 0.8f;
    anim.onionSkin.beforeTint = Vector3(0.1f, 0.2f, 0.9f);
    anim.onionSkin.afterTint = Vector3(0.9f, 0.2f, 0.1f);
    src.AddComponent<AnimatorComponent>(e, anim);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<AnimatorComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(r->onionSkin.enabled);
    ENJIN_EXPECT_TRUE(r->onionSkin.framesBefore == 5);
    ENJIN_EXPECT_TRUE(r->onionSkin.framesAfter == 2);
    ENJIN_EXPECT_TRUE(Near(r->onionSkin.opacity, 0.4f));
    ENJIN_EXPECT_TRUE(Near(r->onionSkin.opacityFalloff, 0.8f));
    ENJIN_EXPECT_TRUE(Near(r->onionSkin.beforeTint.z, 0.9f));
    ENJIN_EXPECT_TRUE(Near(r->onionSkin.afterTint.x, 0.9f));
}

ENJIN_TEST(SerdesCoverage, InteractiveWaterAppearanceSurvivesASave) {
    // Arrange: enableShoreline defaults on, so turning it off is the case a
    // missing field silently reverts.
    World src;
    Entity e = Base(src);
    Enjin::Effects::InteractiveWaterComponent water;
    water.uvScrollDir = Vector2(-0.3f, 0.8f);
    water.enableShoreline = false;
    water.shorelineDistance = 2.75f;
    src.AddComponent<Enjin::Effects::InteractiveWaterComponent>(e, water);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<Enjin::Effects::InteractiveWaterComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(Near(r->uvScrollDir.x, -0.3f));
    ENJIN_EXPECT_TRUE(Near(r->uvScrollDir.y, 0.8f));
    ENJIN_EXPECT_TRUE(!r->enableShoreline);
    ENJIN_EXPECT_TRUE(Near(r->shorelineDistance, 2.75f));
}

ENJIN_TEST(SerdesCoverage, TimelineTracksSurviveASave) {
    // Arrange: the Flash panel's Convert button fills one of these with
    // per-keyframe property tracks, and there was no serializer at all — nor any
    // save path for the Flash data it was converted from, so the keyframes were
    // simply gone.
    World src;
    Entity e = Base(src);
    Enjin::Animation::TimelineComponent tl;

    Enjin::Animation::PropertyTrack pt;
    pt.targetProperty = "position.y";
    pt.targetEntity = 12;
    Enjin::Animation::PropertyKeyframe k0;
    k0.time = 0.0f;
    k0.value = 1.5f;
    k0.easing = Enjin::Animation::TimelineEasing::EaseInOut;
    Enjin::Animation::PropertyKeyframe k1;
    k1.time = 2.0f;
    k1.value = Vector3(1.0f, 2.0f, 3.0f);
    Enjin::Animation::PropertyKeyframe k2;
    k2.time = 3.0f;
    k2.value = std::string("open");
    pt.keyframes = {k0, k1, k2};
    tl.propertyTracks.push_back(pt);

    Enjin::Animation::EventTrack et;
    et.name = "sfx";
    Enjin::Animation::TimelineEvent ev;
    ev.time = 1.25f;
    ev.eventName = "Footstep";
    ev.eventData = "{\"foot\":\"left\"}";
    et.events.push_back(ev);
    tl.eventTracks.push_back(et);

    Enjin::Animation::AnimationTrack at;
    at.startTime = 0.5f;
    at.duration = 1.75f;
    at.animationName = "Wave";
    at.targetEntity = 9;
    at.blendWeight = 0.6f;
    tl.animationTracks.push_back(at);

    tl.duration = 4.0f;
    tl.playbackSpeed = 1.5f;
    tl.loop = true;
    tl.pingPong = true;
    tl.playOnAwake = true;
    src.AddComponent<Enjin::Animation::TimelineComponent>(e, tl);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<Enjin::Animation::TimelineComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_ASSERT_TRUE(r->propertyTracks.size() == 1);
    ENJIN_EXPECT_TRUE(r->propertyTracks[0].targetProperty == "position.y");
    ENJIN_EXPECT_TRUE(r->propertyTracks[0].targetEntity == 12);
    ENJIN_ASSERT_TRUE(r->propertyTracks[0].keyframes.size() == 3);

    // Each keyframe keeps its own type. Without the type tag every value would
    // read back as the variant's first alternative, turning a position into a
    // number and a string cue into zero.
    const auto& keys = r->propertyTracks[0].keyframes;
    ENJIN_EXPECT_TRUE(std::holds_alternative<f32>(keys[0].value));
    ENJIN_EXPECT_TRUE(Near(std::get<f32>(keys[0].value), 1.5f));
    ENJIN_EXPECT_TRUE(keys[0].easing == Enjin::Animation::TimelineEasing::EaseInOut);
    ENJIN_EXPECT_TRUE(std::holds_alternative<Vector3>(keys[1].value));
    ENJIN_EXPECT_TRUE(Near(std::get<Vector3>(keys[1].value).z, 3.0f));
    ENJIN_EXPECT_TRUE(std::holds_alternative<std::string>(keys[2].value));
    ENJIN_EXPECT_TRUE(std::get<std::string>(keys[2].value) == "open");

    ENJIN_ASSERT_TRUE(r->eventTracks.size() == 1);
    ENJIN_ASSERT_TRUE(r->eventTracks[0].events.size() == 1);
    ENJIN_EXPECT_TRUE(r->eventTracks[0].events[0].eventName == "Footstep");
    ENJIN_EXPECT_TRUE(Near(r->eventTracks[0].events[0].time, 1.25f));

    ENJIN_ASSERT_TRUE(r->animationTracks.size() == 1);
    ENJIN_EXPECT_TRUE(r->animationTracks[0].animationName == "Wave");
    ENJIN_EXPECT_TRUE(Near(r->animationTracks[0].blendWeight, 0.6f));

    ENJIN_EXPECT_TRUE(Near(r->duration, 4.0f));
    ENJIN_EXPECT_TRUE(Near(r->playbackSpeed, 1.5f));
    ENJIN_EXPECT_TRUE(r->loop);
    ENJIN_EXPECT_TRUE(r->pingPong);
    ENJIN_EXPECT_TRUE(r->playOnAwake);
}


ENJIN_TEST(SerdesCoverage, PlatformerStompFeelSurvivesASave) {
    // Arrange: stomp feel used to be three literals written into two separate
    // code paths, so there was nothing to tune. Now it is authored, which means
    // it has to come back.
    World src;
    Entity e = Base(src);
    Platformer2DController ctrl;
    ctrl.stompMinFallSpeed = 3.25f;
    ctrl.stompMinHeight = 0.75f;
    ctrl.stompBounceScale = 1.2f;
    src.AddComponent<Platformer2DController>(e, ctrl);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<Platformer2DController>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(Near(r->stompMinFallSpeed, 3.25f));
    ENJIN_EXPECT_TRUE(Near(r->stompMinHeight, 0.75f));
    ENJIN_EXPECT_TRUE(Near(r->stompBounceScale, 1.2f));
}


ENJIN_TEST(SerdesCoverage, LadderAndClimberFeelSurviveASave) {
    // Arrange: climbSpeed, topBoost and allowJumpOff were already authored on
    // the ladder while the rest of the climb's feel was literals in the shared
    // step. The grab height belongs to the climber, not the ladder: a tall
    // character reaches a rung a short one cannot.
    World src;
    Entity ladderEntity = Base(src);
    LadderComponent ladder;
    ladder.mantleWindow = 0.55f;
    ladder.pushOffScale = 1.1f;
    src.AddComponent<LadderComponent>(ladderEntity, ladder);

    Entity climber = src.CreateEntity();
    src.AddComponent<NameComponent>(climber, NameComponent{"Climber"});
    src.AddComponent<TransformComponent>(climber, TransformComponent{});
    FirstPersonController fp;
    fp.ladderGrabHeight = 1.6f;
    src.AddComponent<FirstPersonController>(climber, fp);

    // Act
    World dst;
    Entity loadedLadder = RoundTrip(src, ladderEntity, dst);
    World dst2;
    Entity loadedClimber = RoundTrip(src, climber, dst2);

    // Assert
    const auto* rl = dst.GetComponent<LadderComponent>(loadedLadder);
    ENJIN_ASSERT_TRUE(rl != nullptr);
    ENJIN_EXPECT_TRUE(Near(rl->mantleWindow, 0.55f));
    ENJIN_EXPECT_TRUE(Near(rl->pushOffScale, 1.1f));

    const auto* rc = dst2.GetComponent<FirstPersonController>(loadedClimber);
    ENJIN_ASSERT_TRUE(rc != nullptr);
    ENJIN_EXPECT_TRUE(Near(rc->ladderGrabHeight, 1.6f));
}


ENJIN_TEST(SerdesCoverage, FootstepMovementThresholdSurvivesASave) {
    // Arrange: every other number on this component was already authored. The
    // speed at which a character counts as moving was a literal written into
    // the system twice, so a slow walker made no sound and there was nothing
    // to change.
    World src;
    Entity e = Base(src);
    FootstepComponent fs;
    fs.movementThreshold = 0.05f;
    src.AddComponent<FootstepComponent>(e, fs);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<FootstepComponent>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(Near(r->movementThreshold, 0.05f));
}


ENJIN_TEST(SerdesCoverage, VehicleHandlingSurvivesASave) {
    // Arrange: top speed, acceleration, brake force, grip and drift were all
    // authored. How hard the handbrake bites, how fast it reverses, when
    // reverse input reverses instead of braking, and how much steering is taken
    // away at speed were literals in the update.
    World src;
    Entity e = Base(src);
    VehicleController v;
    v.handbrakeScale = 3.0f;
    v.reverseAccelScale = 0.25f;
    v.reverseSpeedThreshold = 2.0f;
    v.highSpeedSteerReduction = 0.85f;
    src.AddComponent<VehicleController>(e, v);

    // Act
    World dst;
    Entity loaded = RoundTrip(src, e, dst);

    // Assert
    const auto* r = dst.GetComponent<VehicleController>(loaded);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_EXPECT_TRUE(Near(r->handbrakeScale, 3.0f));
    ENJIN_EXPECT_TRUE(Near(r->reverseAccelScale, 0.25f));
    ENJIN_EXPECT_TRUE(Near(r->reverseSpeedThreshold, 2.0f));
    ENJIN_EXPECT_TRUE(Near(r->highSpeedSteerReduction, 0.85f));
}

ENJIN_TEST_MAIN()
