// RT probe-project EXPORT TOOL (registered as a test target, but it is a
// tool, not a correctness test — same pattern as TestRTSceneEmit).
//
// Runs the real BuildPipeline on the RT probe project so the exported game
// can verify ray tracing in the standalone Player (the editor game view and
// the player share the RT dispatch path, but the player has its own display
// hookup and historically force-disabled RT).
//
// Gated: without both env vars set, the test is a no-op pass. To export:
//
//   $env:ENJIN_RT_PROBE_DIR  = 'D:\TEGE_Projects\_RTProbe'     (existing probe project)
//   $env:ENJIN_RT_EXPORT_DIR = 'D:\TEGE_Projects\_RTProbe\_export'
//   ctest -C Release -R TestRTPlayerExport
//
// Then boot the player under validation: pwsh -File tools/probes/rt_player_probe.ps1
// (copies EnjinPlayer.exe next to game.enjpak — the pipeline's player lookup
// does not know the test-exe directory layout, which is fine: it warns and
// packs everything else).

#include "EnjinTest.h"

#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Build/BuildReport.h"

#include <cstdlib>
#include <filesystem>

using namespace Enjin;
namespace fs = std::filesystem;

ENJIN_TEST(RTPlayerExport, ExportsProbeProjectForPlayerVerification) {
    // Arrange: opt-in only — no env vars means this run is a normal test pass.
    const char* probeDir  = std::getenv("ENJIN_RT_PROBE_DIR");
    const char* exportDir = std::getenv("ENJIN_RT_EXPORT_DIR");
    if (!probeDir || !*probeDir || !exportDir || !*exportDir) {
        return;
    }

    fs::path projectFile = fs::path(probeDir) / "RTProbe.enjinproject";
    ENJIN_ASSERT_TRUE(fs::exists(projectFile));

    Build::BuildConfig cfg;
    cfg.projectPath  = projectFile.string();
    cfg.outputDir    = exportDir;
    cfg.windowTitle  = "RTProbe";
    cfg.windowWidth  = 1280;
    cfg.windowHeight = 720;
    cfg.engineSplash = false;  // probe boots straight into the scene

    // Act: run the real export pipeline.
    Build::BuildPipeline pipeline;
    Build::BuildResult result = pipeline.Execute(cfg);

    // Assert: pack written with content.
    ENJIN_ASSERT_TRUE(result.success);
    ENJIN_EXPECT_TRUE(result.filesPacked > 0);
    ENJIN_EXPECT_TRUE(fs::exists(fs::path(exportDir) / "game.enjpak"));
}

ENJIN_TEST_MAIN()
