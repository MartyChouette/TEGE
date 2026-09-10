#include "EnjinTest.h"
#include "Enjin/Plugin/PluginSystem.h"

#include <string>

using namespace Enjin;
using namespace Enjin::Plugin;

// ===========================================================================
// PluginManifest Defaults
// ===========================================================================

ENJIN_TEST(Manifest, Defaults) {
    PluginManifest manifest;
    ENJIN_EXPECT_TRUE(manifest.name.empty());
    ENJIN_EXPECT_TRUE(manifest.version.empty());
    ENJIN_EXPECT_TRUE(manifest.libraryPath.empty());
    ENJIN_EXPECT_TRUE(manifest.description.empty());
    ENJIN_EXPECT_TRUE(manifest.author.empty());
    ENJIN_EXPECT_TRUE(manifest.category.empty());
    ENJIN_EXPECT_EQ(manifest.dependencies.size(), (size_t)0);
    ENJIN_EXPECT_EQ(manifest.tags.size(), (size_t)0);
    ENJIN_EXPECT_TRUE(manifest.loadOnStartup);
}

ENJIN_TEST(Manifest, SetFields) {
    PluginManifest m;
    m.name = "TestPlugin";
    m.version = "1.0.0";
    m.author = "Engine Team";
    m.category = "Tools";
    m.dependencies.push_back("CoreLib");
    m.tags.push_back("editor");
    m.tags.push_back("debug");

    ENJIN_EXPECT_STR_EQ(m.name.c_str(), "TestPlugin");
    ENJIN_EXPECT_STR_EQ(m.version.c_str(), "1.0.0");
    ENJIN_EXPECT_EQ(m.dependencies.size(), (size_t)1);
    ENJIN_EXPECT_EQ(m.tags.size(), (size_t)2);
}

// ===========================================================================
// PluginEntry Defaults
// ===========================================================================

ENJIN_TEST(PluginEntry, Defaults) {
    PluginEntry entry;
    ENJIN_EXPECT_TRUE(entry.instance == nullptr);
    ENJIN_EXPECT_TRUE(entry.libraryHandle == nullptr);
    ENJIN_EXPECT_FALSE(entry.loaded);
    ENJIN_EXPECT_TRUE(entry.enabled);
    ENJIN_EXPECT_TRUE(entry.error.empty());
}

// ===========================================================================
// PluginContext Defaults
// ===========================================================================

ENJIN_TEST(PluginContext, Defaults) {
    PluginContext ctx;
    ENJIN_EXPECT_TRUE(ctx.world == nullptr);
    ENJIN_EXPECT_TRUE(ctx.renderSystem == nullptr);
    ENJIN_EXPECT_TRUE(ctx.scriptEngine == nullptr);
    ENJIN_EXPECT_TRUE(ctx.audio == nullptr);
    ENJIN_EXPECT_TRUE(ctx.sceneManager == nullptr);
}

// ===========================================================================
// PluginSystem — Query Methods (no DLL loading)
// ===========================================================================

ENJIN_TEST(System, InitiallyEmpty) {
    PluginSystem sys;
    ENJIN_EXPECT_EQ(sys.GetPlugins().size(), (size_t)0);
}

ENJIN_TEST(System, FindPluginReturnsNull) {
    PluginSystem sys;
    ENJIN_EXPECT_TRUE(sys.FindPlugin("NonexistentPlugin") == nullptr);
}

ENJIN_TEST(System, IsLoadedReturnsFalse) {
    PluginSystem sys;
    ENJIN_EXPECT_FALSE(sys.IsLoaded("NonexistentPlugin"));
}

ENJIN_TEST(System, SetContext) {
    // "Should not crash" tested nothing -- a SetContext with an empty body would
    // have passed. Assert the state it is supposed to leave behind instead.
    PluginSystem sys;
    ENJIN_EXPECT_TRUE(sys.GetPlugins().empty());

    PluginContext ctx;
    ctx.world = nullptr;
    sys.SetContext(ctx);

    // Setting a context is not loading a plugin.
    ENJIN_EXPECT_TRUE(sys.GetPlugins().empty());
    // And it must not have made the system unusable: a bad load still reports
    // failure rather than crashing or silently succeeding.
    ENJIN_EXPECT_FALSE(sys.LoadPlugin("no_such_plugin"));
}

ENJIN_TEST(System, LoadNonexistentPluginFails) {
    PluginSystem sys;
    bool ok = sys.LoadPlugin("no_such_plugin");
    ENJIN_EXPECT_FALSE(ok);
}

ENJIN_TEST(System, UnloadAllSafe) {
    PluginSystem sys;
    ENJIN_EXPECT_TRUE(sys.GetPlugins().empty());

    sys.UnloadAll();                              // on an empty system
    ENJIN_EXPECT_TRUE(sys.GetPlugins().empty());

    // Twice, because an unload that corrupted its own list would show here and
    // not in a single call.
    sys.UnloadAll();
    ENJIN_EXPECT_TRUE(sys.GetPlugins().empty());

    // And the system still works afterwards.
    ENJIN_EXPECT_FALSE(sys.LoadPlugin("no_such_plugin"));
}

ENJIN_TEST_MAIN()
