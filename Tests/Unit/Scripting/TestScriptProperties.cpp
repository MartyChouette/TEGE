#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptPropertyParser.h"
#include "Enjin/ECS/Components/Script.h"

using namespace Enjin;
using namespace Enjin::Scripting;

// The [Property] parser must recognize entity-array types (array<uint64>/array<Entity>)
// as EntityArray, so scripts can expose drag-assignable lists of entity references
// instead of the name-indexed OnStart lookups (scoreCarriers[4], goblinEntities[4], ...).
ENJIN_TEST(ScriptProperties, ParsesEntityAndEntityArrayTypes) {
    const char* src =
        "class Foo {\n"
        "  [Property] Entity target;\n"
        "  [Property] uint64 other;\n"
        "  [Property] array<uint64> scoreCarriers;\n"
        "  [Property] array<Entity> goblins;\n"
        "  [Property] float speed = 5.0f;\n"
        "  [Property] Vector3 spawn;\n"
        "}\n";

    auto props = ParseProperties(src);

    auto typeOf = [&](const std::string& name) -> int {
        for (const auto& p : props) if (p.name == name) return static_cast<int>(p.type);
        return -1;  // not found
    };

    ENJIN_EXPECT_EQ(typeOf("target"),        (int)ECS::ScriptPropertyType::Entity);
    ENJIN_EXPECT_EQ(typeOf("other"),         (int)ECS::ScriptPropertyType::Entity);
    ENJIN_EXPECT_EQ(typeOf("scoreCarriers"), (int)ECS::ScriptPropertyType::EntityArray);
    ENJIN_EXPECT_EQ(typeOf("goblins"),       (int)ECS::ScriptPropertyType::EntityArray);
    ENJIN_EXPECT_EQ(typeOf("speed"),         (int)ECS::ScriptPropertyType::Float);
    ENJIN_EXPECT_EQ(typeOf("spawn"),         (int)ECS::ScriptPropertyType::Vector3);
}

ENJIN_TEST_MAIN()
