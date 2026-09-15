// A field a component does not have must be reported, not swallowed.
//
// Every component deserializer is a list of `if (j.contains("x"))`. A field
// spelled wrong is simply never asked for: nothing throws, nothing logs, and
// the component loads with that value at its default. Unknown keys at ENTITY
// level were already reported; unknown keys INSIDE a component were not, and
// that is the far larger surface.
//
// It cost a day. Three generated demo scenes wrote `"isStatic": true` into
// every rigidbody -- a field the 3D component does not have, because it uses
// `bodyType` -- so every wall and floor loaded as a DYNAMIC body and the rooms
// slowly shook themselves apart. The JSON looked right, the scene looked right,
// and the only symptom was geometry drifting a few seconds after pressing play.
//
// The trap is a good one: `isStatic` IS a real field, on the 2D body
// (PhysicsTypes2D.h). So the name is correct somewhere, which is exactly the
// kind of wrong that survives review.

#include "EnjinTest.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"

#include <cstdio>
#include <string>

using namespace Enjin;

namespace {

// One entity, one rigidbody, one bogus field.
std::string SceneWithField(const char* field, const char* value) {
    std::string s = R"({"version":"1.0","entities":[{"id":1,"name":{"name":"Slab"},)";
    s += R"("transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1],"visible":true},)";
    s += std::string(R"("rigidbody":{"bodyType":2,")") + field + R"(":)" + value + "}}]}";
    return s;
}

bool MentionsField(const Scene::DeserializationResult& r, const char* field) {
    for (const std::string& w : r.warnings) {
        if (w.find(field) != std::string::npos) return true;
    }
    return false;
}

} // namespace

ENJIN_TEST(UnknownComponentField, AFieldTheComponentDoesNotHaveIsReported) {
    // Arrange: the exact mistake, in the exact component it was made in.
    ECS::World world;
    Scene::SceneSerializer s(&world);

    // Act
    const auto result = s.LoadFromString(SceneWithField("isStatic", "true"));

    // Assert: the scene still loads -- an unknown field is not a reason to
    // throw away a level -- but it says so.
    ENJIN_ASSERT_TRUE(result.success);
    for (const std::string& w : result.warnings) std::printf("    %s\n", w.c_str());
    ENJIN_EXPECT_TRUE(MentionsField(result, "isStatic"));

    // And the component is otherwise intact: the fields that DO exist still
    // loaded, so the warning is about one field and not a failed component.
    bool found = false;
    for (ECS::Entity e : world.GetEntitiesWithComponent<ECS::RigidbodyComponent>()) {
        const auto* rb = world.GetComponent<ECS::RigidbodyComponent>(e);
        ENJIN_ASSERT_TRUE(rb != nullptr);
        ENJIN_EXPECT_TRUE(rb->bodyType == ECS::RigidbodyComponent::BodyType::Static);
        found = true;
    }
    ENJIN_EXPECT_TRUE(found);
}

ENJIN_TEST(UnknownComponentField, AWellSpelledSceneWarnsAboutNothing) {
    // Arrange / Act: the same scene without the bogus field.
    ECS::World world;
    Scene::SceneSerializer s(&world);
    const auto result = s.LoadFromString(
        R"({"version":"1.0","entities":[{"id":1,"name":{"name":"Slab"},)"
        R"("transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1],"visible":true},)"
        R"("rigidbody":{"bodyType":2,"mass":0.0,"useGravity":false}}]})");

    // Assert: no false positives. A check that cried wolf on every valid scene
    // would be turned off within a day, and then the real one would not be
    // seen either.
    ENJIN_ASSERT_TRUE(result.success);
    for (const std::string& w : result.warnings) std::printf("    unexpected: %s\n", w.c_str());
    ENJIN_EXPECT_TRUE(!MentionsField(result, "mass"));
    ENJIN_EXPECT_TRUE(!MentionsField(result, "bodyType"));
    ENJIN_EXPECT_TRUE(!MentionsField(result, "useGravity"));
}

ENJIN_TEST(UnknownComponentField, TheNameIsRightOnADIFFERENTComponent) {
    // Arrange: this is why the mistake survived. `isStatic` is a real field on
    // the 2D body, so somebody moving between 2D and 3D writes a name that is
    // correct in the other half of the engine.
    ECS::World world;
    Scene::SceneSerializer s(&world);

    // Act
    const auto result = s.LoadFromString(
        R"({"version":"1.0","entities":[{"id":1,"name":{"name":"Ground"},)"
        R"("transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1],"visible":true},)"
        R"("body2D":{"isStatic":true}}]})");

    // Assert: on the 2D body it is a real field and must NOT be warned about.
    ENJIN_ASSERT_TRUE(result.success);
    for (const std::string& w : result.warnings) std::printf("    %s\n", w.c_str());
    ENJIN_EXPECT_TRUE(!MentionsField(result, "isStatic"));
}

// ---------------------------------------------------------------------------
// The probe cannot tell "ignored" from "rejected" with one candidate value.
//
// The warning above is produced by CHANGING a field and seeing whether the
// component changes. That is sound until the deserializer range-checks: a
// probe value outside the accepted range is refused, the component is
// unchanged, and a field that was read perfectly reports as unknown.
//
// It flipped integers between 0 and 1, which is safe for a 0-based enum and
// wrong for any field whose range starts higher. `pomMaxSteps` accepts 1..256
// and `ditherGradientBands` accepts 2..8, so the probe value 0 was refused by
// both. Every one of Playground's 62 materials logged two warnings naming
// fields that SceneSerializer reads twenty lines apart from where it writes
// them, on every single load.
//
// What made it invisible for so long is that components serialize only what
// differs from the default, so a field authored AT its default never appears
// in the comparison at all -- the probe is the only thing that can see it, and
// the probe was the broken part.

namespace {

// One entity, one material, one field pinned to a chosen value.
std::string SceneWithMaterialField(const char* field, const char* value) {
    std::string s = R"({"version":"1.0","entities":[{"id":1,"name":{"name":"Block"},)";
    s += R"("transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1],"visible":true},)";
    s += std::string(R"("material":{"baseColor":[1,1,1,1],")") + field + R"(":)" + value + "}}]}";
    return s;
}

} // namespace

ENJIN_TEST(UnknownComponentField, ARangeCheckedFieldAtItsDefaultIsNotReported) {
    // Arrange: pomMaxSteps accepts 1..256 and defaults to 32. Authored at its
    // default, which is the case the probe could not see.
    ECS::World world;
    Scene::SceneSerializer s(&world);

    // Act
    const auto result = s.LoadFromString(SceneWithMaterialField("pomMaxSteps", "32"));

    // Assert: read correctly, so it must not be reported. Under the 0/1 probe
    // this warned, because 0 is below the range and was refused.
    ENJIN_ASSERT_TRUE(result.success);
    for (const std::string& w : result.warnings) std::printf("    unexpected: %s\n", w.c_str());
    ENJIN_EXPECT_TRUE(!MentionsField(result, "pomMaxSteps"));
}

ENJIN_TEST(UnknownComponentField, ARangeFloorAboveOneIsNotReported) {
    // Arrange: ditherGradientBands accepts 2..8 and defaults to 4, so BOTH of
    // the old probe's candidates (0 and 1) sit below its floor.
    ECS::World world;
    Scene::SceneSerializer s(&world);

    // Act
    const auto result = s.LoadFromString(SceneWithMaterialField("ditherGradientBands", "4"));

    // Assert
    ENJIN_ASSERT_TRUE(result.success);
    for (const std::string& w : result.warnings) std::printf("    unexpected: %s\n", w.c_str());
    ENJIN_EXPECT_TRUE(!MentionsField(result, "ditherGradientBands"));
}

ENJIN_TEST(UnknownComponentField, ARealTypoInAMaterialIsStillReported) {
    // Arrange: the fix must not be a blanket silencer. A field the material
    // genuinely does not have has to survive every probe candidate and warn.
    ECS::World world;
    Scene::SceneSerializer s(&world);

    // Act
    const auto result = s.LoadFromString(SceneWithMaterialField("pomMaxStepz", "32"));

    // Assert
    ENJIN_ASSERT_TRUE(result.success);
    for (const std::string& w : result.warnings) std::printf("    %s\n", w.c_str());
    ENJIN_EXPECT_TRUE(MentionsField(result, "pomMaxStepz"));
}

// ---------------------------------------------------------------------------
// The scene the bug was actually found in.
//
// The two cases above are minimal on purpose, but a minimal material is not
// what ships: Playground writes all 66 fields on every one of its 62
// materials, and any of them could carry a range check the probe trips over.
// This is one of those blocks copied out of Examples/Playground/scenes/Main.enjin
// verbatim, so the whole authored surface is probed at once rather than the
// two fields we already know about.

namespace {

const char* kRealPlaygroundMaterial =
        R"({"affineTexturing":false,"alphaCutoff":0.5,"alphaMode":0,"baseColor":[0.4000000059604645,)"
        R"(0.5199999809265137,0.36000001430511475],"baseColorTexture":-1,"baseColorTexturePath":"",)"
        R"("castShadows":true,"ditherGradient":false,"ditherGradientBands":4,)"
        R"("ditherGradientPattern":0,"ditherTransBlendColor":[0.699999988079071,0.8500000238418579,)"
        R"(1.0],"ditherTransOpacity":0.5,"ditherTransPattern":0,"ditherTransparency":false,)"
        R"("doubleSided":false,"emissiveColor":[0.0,0.0,0.0],"emissiveStrength":0.0,)"
        R"("emissiveTexture":-1,"emissiveTexturePath":"","excludeFromCelShading":false,)"
        R"("flatShading":false,"footstepVolume":1.0,"fresnelPower":5.0,"gouraudOnly":false,)"
        R"("heightTexturePath":"","impactThreshold":3.0,"ior":1.5,"lightRampOverride":0,)"
        R"("matcapTexturePath":"","metallic":0.0,"metallicRoughnessTexture":-1,)"
        R"("metallicRoughnessTexturePath":"","normalTexture":-1,"normalTexturePath":"",)"
        R"("opacity":1.0,"outlineColor":[0.0,0.0,0.0],"outlineWidth":0.0,"parallaxMode":0,)"
        R"("parallaxScale":0.05000000074505806,"pomHeightScale":0.05000000074505806,)"
        R"("pomMaxSteps":32,"receiveShadows":true,"reflectivity":0.0,"rimLightStrength":0.0,)"
        R"("roughness":0.800000011920929,"scrollReflectionSpeed":[0.05000000074505806,)"
        R"(0.029999999329447746],"scrollReflectionStrength":0.5,"scrollReflectionTexturePath":"",)"
        R"("sdfText":false,"shadowDitherMode":0,"shadowDitherPattern":0,"specularTexturePath":"",)"
        R"("sssColor":[1.0,0.20000000298023224,0.10000000149011612],"sssIntensity":0.0,)"
        R"("sssRadius":1.0,"stippleTransparency":false,"surfaceNoiseScale":0.0,)"
        R"("surfaceNoiseStrength":0.0,"textureFilterOverride":0,"thickness":0.0,"transmission":0.0,)"
        R"("uvQuantize":false,"uvRegionOffset":[0.0,0.0],"uvRegionScale":[1.0,1.0],)"
        R"("vertexSnapResolution":160,"vertexSnapping":false})";

} // namespace

ENJIN_TEST(UnknownComponentField, ARealAuthoredMaterialWarnsAboutNothing) {
    // Arrange: a real 66-field material, every value as Playground saved it.
    ECS::World world;
    Scene::SceneSerializer s(&world);

    std::string scene = R"({"version":"1.0","entities":[{"id":1,"name":{"name":"Block"},)";
    scene += R"("transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1],"visible":true},)";
    scene += std::string(R"("material":)") + kRealPlaygroundMaterial + "}]}";

    // Act
    const auto result = s.LoadFromString(scene);

    // Assert: the serializer reads all 66, so the load must be silent. Under
    // the 0/1 probe this produced two warnings, times 62 materials, on every
    // load of the flagship demo.
    ENJIN_ASSERT_TRUE(result.success);
    for (const std::string& w : result.warnings) std::printf("    unexpected: %s\n", w.c_str());
    ENJIN_EXPECT_TRUE(result.warnings.empty());
}

ENJIN_TEST_MAIN()
