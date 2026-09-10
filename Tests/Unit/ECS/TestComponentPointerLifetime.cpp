// A component pointer does not survive an AddComponent of the same type.
//
// `ComponentStorage<T>` keeps a dense `std::vector<T>` and `Add` is a
// `push_back`, so the moment that vector outgrows its capacity every `T*` handed
// out by `GetComponent<T>` dangles. Four places in the engine were holding one
// across exactly that call:
//
//   Destructible.cpp   fracture any destructible, and every fragment was placed
//                      from freed memory -- and inherited a MaterialComponent
//                      whose std::string texture paths were read off the freed
//                      heap, then handed to the texture loader
//   SWFConverter.cpp   import a .swf whose display list reuses a character that
//                      already has a sprite
//
// This test does not test those call sites. It pins the PROPERTY that makes them
// bugs, so the rule stays legible to whoever reads it next: hold a pointer across
// an Add of the same type and you are reading freed memory. The fix in all four
// places was the same -- copy the values out before the first Add.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

ENJIN_TEST(ComponentPointerLifetime, StorageIsDenseSoAddCanMoveEverything) {
    // Arrange: one entity with a transform, and its address.
    World w;
    Entity first = w.CreateEntity();
    w.AddComponent<TransformComponent>(first, TransformComponent{});
    const TransformComponent* before = w.GetComponent<TransformComponent>(first);
    ENJIN_ASSERT_TRUE(before != nullptr);

    // Act: add enough transforms to force at least one reallocation.
    for (int i = 0; i < 512; ++i) {
        w.AddComponent<TransformComponent>(w.CreateEntity(), TransformComponent{});
    }
    const TransformComponent* after = w.GetComponent<TransformComponent>(first);

    // Assert: the entity is fine, its component is fine, and the ADDRESS moved.
    // If this ever stops being true the storage changed shape and the
    // copy-before-you-add rule can be revisited -- until then it stands.
    ENJIN_ASSERT_TRUE(after != nullptr);
    ENJIN_EXPECT_TRUE(before != after);
}

ENJIN_TEST(ComponentPointerLifetime, ValuesCopiedBeforeAnAddAreStillCorrect) {
    // The shape of the fix, stated once. Copy what you need, then add.
    World w;
    Entity src = w.CreateEntity();
    TransformComponent t;
    t.position = Vector3(3.0f, 1.5f, -2.0f);
    t.scale = Vector3(2.0f, 2.0f, 2.0f);
    w.AddComponent<TransformComponent>(src, t);
    w.AddComponent<NameComponent>(src, NameComponent{"Source"});

    // Copy BEFORE the loop, exactly as Destructible now does.
    const TransformComponent srcTransform = *w.GetComponent<TransformComponent>(src);
    const std::string srcName = w.GetComponent<NameComponent>(src)->name;

    std::vector<Entity> fragments;
    for (int i = 0; i < 256; ++i) {
        Entity frag = w.CreateEntity();
        auto& ft = w.AddComponent<TransformComponent>(frag, TransformComponent{});
        ft.position = srcTransform.position;   // reading a copy, not a pointer
        ft.scale = srcTransform.scale;
        w.AddComponent<NameComponent>(frag, NameComponent{srcName + "_frag"});
        fragments.push_back(frag);
    }

    // Every fragment got the real values, including the last one -- placed long
    // after the storage had reallocated several times over.
    for (Entity frag : fragments) {
        const auto* ft = w.GetComponent<TransformComponent>(frag);
        ENJIN_ASSERT_TRUE(ft != nullptr);
        ENJIN_EXPECT_TRUE(ft->position.x > 2.99f && ft->position.x < 3.01f);
        ENJIN_EXPECT_TRUE(ft->scale.y > 1.99f && ft->scale.y < 2.01f);
    }
    ENJIN_EXPECT_EQ(w.GetComponent<NameComponent>(fragments.back())->name,
                    std::string("Source_frag"));
}

ENJIN_TEST(ComponentPointerLifetime, AStringCopiedBeforeAnAddIsNotAFreedHeapRead) {
    // The sharpest half of the destructible bug: the dangling read was a
    // std::string texture path, and it went to the texture loader. A copy of a
    // string owns its own buffer and cannot be invalidated by a reallocation of
    // the vector the original lived in.
    World w;
    Entity src = w.CreateEntity();
    // Long enough to be heap-allocated rather than in the small-string buffer,
    // which is what makes the freed read reachable at all.
    const std::string authored = "assets/textures/very/long/path/to/a/material/albedo_2048.png";
    w.AddComponent<NameComponent>(src, NameComponent{authored});

    const std::string copied = w.GetComponent<NameComponent>(src)->name;
    for (int i = 0; i < 512; ++i) {
        w.AddComponent<NameComponent>(w.CreateEntity(), NameComponent{"filler"});
    }

    ENJIN_EXPECT_EQ(copied, authored);
    ENJIN_EXPECT_EQ(w.GetComponent<NameComponent>(src)->name, authored);
}

ENJIN_TEST(ComponentPointerLifetime, RefetchingAfterAnAddIsTheOtherValidFix) {
    // Where copying is not practical, the pointer has to be taken again after the
    // add. Recorded so both fixes are on the page and neither is folklore.
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e, TransformComponent{});
    w.GetComponent<TransformComponent>(e)->position = Vector3(1.0f, 2.0f, 3.0f);

    for (int i = 0; i < 512; ++i) {
        w.AddComponent<TransformComponent>(w.CreateEntity(), TransformComponent{});
    }

    const auto* fresh = w.GetComponent<TransformComponent>(e);   // re-fetched
    ENJIN_ASSERT_TRUE(fresh != nullptr);
    ENJIN_EXPECT_TRUE(fresh->position.y > 1.99f && fresh->position.y < 2.01f);
}

ENJIN_TEST_MAIN()
