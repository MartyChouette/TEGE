// Regression net for the "clean exit when Vulkan is unavailable" path: a
// VulkanRenderer whose Initialize never ran (or failed before device creation)
// must destruct without touching Vulkan. The original bug indexed the empty
// sync-object vectors by MAX_FRAMES_IN_FLIGHT in DestroySyncObjects - a null
// read that turned the polite "no driver found" exit into an access violation.
#include "EnjinTest.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"

#include <memory>

using namespace Enjin;

ENJIN_TEST(RendererLifecycle, DestroyWithoutInitializeDoesNotCrash) {
    // Arrange: a renderer that never saw Initialize (no context, empty vectors).
    auto renderer = std::make_unique<Renderer::VulkanRenderer>();

    // Act: destroy it - runs the full Shutdown path.
    renderer.reset();

    // Assert: reaching this line IS the test (the bug was an access violation).
    ENJIN_EXPECT_TRUE(true);
}

ENJIN_TEST(RendererLifecycle, ExplicitShutdownWithoutInitializeIsSafe) {
    // Arrange
    Renderer::VulkanRenderer renderer;

    // Act: Shutdown twice - explicit call plus the destructor's own.
    renderer.Shutdown();
    renderer.Shutdown();

    // Assert
    ENJIN_EXPECT_TRUE(true);
}

ENJIN_TEST_MAIN()
