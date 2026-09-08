// A game choosing its own typeface.
//
// Until this existed, every label a game drew came out in whatever face the
// editor happened to be using: UISystem resolved the font with a bare
// ImGui::GetFont() and there was no field, at any level, to say otherwise.
//
// The tests that matter here are the ones about SAFETY and FALLBACK, because
// those are the two ways this feature can do real harm: a scene file naming a
// path outside the project, and a missing font turning into missing text.
#include "EnjinTest.h"
#include "Enjin/GUI/UIFontRegistry.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UITemplates.h"

using namespace Enjin;
using namespace Enjin::GUI;

namespace {

// Any absolute path works: the registry's containment check is lexical and
// never touches the filesystem.
const char* kRoot = "D:/projects/twister";

UIFontRegistry& FreshRegistry() {
    UIFontRegistry& fonts = UIFontRegistry::Get();
    fonts.Reset();
    fonts.SetRoot(kRoot);
    return fonts;
}

} // namespace

// ---------------------------------------------------------------------------
// Containment: a scene file is untrusted input
// ---------------------------------------------------------------------------

ENJIN_TEST(UIFont, AProjectRelativeFontIsAccepted) {
    UIFontRegistry& fonts = FreshRegistry();

    ENJIN_EXPECT_TRUE(fonts.Request("assets/fonts/Kenney Pixel.ttf"));
    ENJIN_EXPECT_EQ(fonts.RequestedPaths().size(), (usize)1);
    ENJIN_EXPECT_FALSE(fonts.ResolvedPath("assets/fonts/Kenney Pixel.ttf").empty());
}

// The whole reason paths go through ResolveWithinRoot: a scene is a data file
// that can name anything, including somewhere it has no business reading.
ENJIN_TEST(UIFont, PathsThatEscapeTheProjectAreRefused) {
    UIFontRegistry& fonts = FreshRegistry();

    const char* hostile[] = {
        "../../../Windows/Fonts/arial.ttf",
        "assets/../../secrets/arial.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/etc/fonts/arial.ttf",
    };

    for (const char* path : hostile) {
        ENJIN_EXPECT_FALSE(fonts.Request(path));
        ENJIN_EXPECT_TRUE(fonts.ResolvedPath(path).empty());
    }
    ENJIN_EXPECT_EQ(fonts.RequestedPaths().size(), (usize)0);
}

// ".." inside a NAME is not a traversal, and refusing it would be a bug of its
// own -- a font legitimately called "Roboto..old.ttf" must still load.
ENJIN_TEST(UIFont, DotsInsideAFileNameAreNotTraversal) {
    UIFontRegistry& fonts = FreshRegistry();
    ENJIN_EXPECT_TRUE(fonts.Request("assets/fonts/Roboto..old.ttf"));
}

ENJIN_TEST(UIFont, NothingLoadsBeforeAProjectIsOpen) {
    UIFontRegistry& fonts = UIFontRegistry::Get();
    fonts.Reset();   // no root: the state a runtime is in before opening a project

    ENJIN_EXPECT_FALSE(fonts.Request("assets/fonts/Kenney Pixel.ttf"));
    ENJIN_EXPECT_FALSE(fonts.NeedsRebuild());
}

// Switching projects must not leave the previous project's faces resolvable,
// or one game's HUD renders in another game's typeface.
ENJIN_TEST(UIFont, ChangingProjectDropsTheOldProjectsFonts) {
    UIFontRegistry& fonts = FreshRegistry();
    fonts.Request("assets/fonts/Kenney Pixel.ttf");
    fonts.SetLoaded("assets/fonts/Kenney Pixel.ttf", reinterpret_cast<ImFont*>(0x1));
    ENJIN_ASSERT_TRUE(fonts.Find("assets/fonts/Kenney Pixel.ttf") != nullptr);

    fonts.SetRoot("D:/projects/something-else");

    ENJIN_EXPECT_TRUE(fonts.Find("assets/fonts/Kenney Pixel.ttf") == nullptr);
    ENJIN_EXPECT_EQ(fonts.RequestedPaths().size(), (usize)0);
}

// ---------------------------------------------------------------------------
// Rebuilds: the reason this is a registry and not a cached pointer
// ---------------------------------------------------------------------------

ENJIN_TEST(UIFont, AskingForAnUnloadedFaceRequestsARebuild) {
    UIFontRegistry& fonts = FreshRegistry();
    ENJIN_EXPECT_FALSE(fonts.NeedsRebuild());

    fonts.Request("assets/fonts/Kenney Pixel.ttf");
    ENJIN_EXPECT_TRUE(fonts.NeedsRebuild());

    fonts.SetLoaded("assets/fonts/Kenney Pixel.ttf", reinterpret_cast<ImFont*>(0x1));
    fonts.MarkBuilt();
    ENJIN_EXPECT_FALSE(fonts.NeedsRebuild());
}

// A rebuild stalls the GPU waiting on in-flight frames. Asking for a face that
// is already loaded must not trigger one, or every frame pays for it.
ENJIN_TEST(UIFont, AskingAgainForALoadedFaceCostsNothing) {
    UIFontRegistry& fonts = FreshRegistry();
    fonts.Request("assets/fonts/Kenney Pixel.ttf");
    fonts.SetLoaded("assets/fonts/Kenney Pixel.ttf", reinterpret_cast<ImFont*>(0x1));
    fonts.MarkBuilt();

    fonts.Request("assets/fonts/Kenney Pixel.ttf");
    ENJIN_EXPECT_FALSE(fonts.NeedsRebuild());
}

// A font that failed to load is remembered as failed. Without this the resolver
// asks for it every frame, and every frame rebuilds the atlas -- a missing file
// would cost more than a present one.
ENJIN_TEST(UIFont, AFaceThatFailedToLoadIsNotRetriedForever) {
    UIFontRegistry& fonts = FreshRegistry();
    fonts.Request("assets/fonts/missing.ttf");
    fonts.SetLoaded("assets/fonts/missing.ttf", nullptr);   // the build tried and failed
    fonts.MarkBuilt();

    fonts.Request("assets/fonts/missing.ttf");
    ENJIN_EXPECT_FALSE(fonts.NeedsRebuild());
    ENJIN_EXPECT_TRUE(fonts.Find("assets/fonts/missing.ttf") == nullptr);
}

// io.Fonts->Clear() dangles every ImFont* ever handed out. The paths survive,
// because they are what the next build re-adds; the pointers must not.
ENJIN_TEST(UIFont, ClearingTheAtlasDropsPointersAndKeepsRequests) {
    UIFontRegistry& fonts = FreshRegistry();
    fonts.Request("assets/fonts/Kenney Pixel.ttf");
    fonts.SetLoaded("assets/fonts/Kenney Pixel.ttf", reinterpret_cast<ImFont*>(0x1));
    fonts.MarkBuilt();

    fonts.OnAtlasCleared();

    ENJIN_EXPECT_TRUE(fonts.Find("assets/fonts/Kenney Pixel.ttf") == nullptr);
    ENJIN_EXPECT_EQ(fonts.RequestedPaths().size(), (usize)1);
    ENJIN_EXPECT_TRUE(fonts.NeedsRebuild());   // it has to be put back
}

ENJIN_TEST(UIFont, ClearingAnEmptyAtlasAsksForNothing) {
    UIFontRegistry& fonts = FreshRegistry();
    fonts.OnAtlasCleared();
    ENJIN_EXPECT_FALSE(fonts.NeedsRebuild());
}

// ---------------------------------------------------------------------------
// The data model
// ---------------------------------------------------------------------------

ENJIN_TEST(UIFont, ACanvasHasNoTypefaceUntilOneIsSet) {
    // The default has to stay empty: an empty path is what makes every scene
    // authored before this feature render exactly as it always has.
    UICanvasComponent canvas;
    ENJIN_EXPECT_TRUE(canvas.theme.fontPath.empty());

    UIElement element;
    ENJIN_EXPECT_FALSE(element.style.HasFontPath());
}

ENJIN_TEST(UIFont, AnElementCanOverrideTheCanvasTypeface) {
    UICanvasComponent canvas;
    canvas.theme.fontPath = "assets/fonts/body.ttf";

    u32 id = canvas.AddElement(UIWidgetType::Label, "Score");
    canvas.GetElement(id)->style.fontPath = "assets/fonts/arcade.ttf";

    ENJIN_EXPECT_TRUE(canvas.GetElement(id)->style.HasFontPath());
    ENJIN_EXPECT_TRUE(canvas.GetElement(id)->style.fontPath == "assets/fonts/arcade.ttf");
    ENJIN_EXPECT_TRUE(canvas.theme.fontPath == "assets/fonts/body.ttf");
}

// The bug this feature is replacing: the editor's font choice was a static
// local that nothing serialised, so it did not survive a restart. A game's
// choice has to live in the scene.
ENJIN_TEST(UIFont, TheTypefaceIsPartOfTheCanvasAndSurvivesACopy) {
    UICanvasComponent canvas = UITemplates::CreateOptionsMenu();
    canvas.theme.fontPath = "assets/fonts/arcade.ttf";

    UICanvasComponent copy = canvas;
    ENJIN_EXPECT_TRUE(copy.theme.fontPath == "assets/fonts/arcade.ttf");
}

ENJIN_TEST_MAIN()
