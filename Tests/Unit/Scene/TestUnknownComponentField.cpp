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

ENJIN_TEST_MAIN()
