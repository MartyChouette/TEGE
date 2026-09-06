#include "EnjinTest.h"
#include "Enjin/Platform/Paths.h"

#include <filesystem>

using namespace Enjin;

// ===========================================================================
// IsSafeRelativePath
// ===========================================================================

ENJIN_TEST(PathSanitize, SafeRelativePath_PlainNestedPath_Accepted) {
    ENJIN_EXPECT_TRUE(Platform::IsSafeRelativePath("scenes/level1.enjin"));
    ENJIN_EXPECT_TRUE(Platform::IsSafeRelativePath("assets/textures/rock.png"));
}

ENJIN_TEST(PathSanitize, SafeRelativePath_DotDotInsideName_Accepted) {
    // ".." as a substring of a component is a legal file name, not traversal
    ENJIN_EXPECT_TRUE(Platform::IsSafeRelativePath("a..b/tex.png"));
    ENJIN_EXPECT_TRUE(Platform::IsSafeRelativePath("notes..txt"));
}

ENJIN_TEST(PathSanitize, SafeRelativePath_SelfCancellingDotDot_Accepted) {
    // "dir/../file" normalizes to "file" without escaping upward
    ENJIN_EXPECT_TRUE(Platform::IsSafeRelativePath("dir/../file.png"));
}

ENJIN_TEST(PathSanitize, SafeRelativePath_Traversal_Rejected) {
    ENJIN_EXPECT_FALSE(Platform::IsSafeRelativePath("../x"));
    ENJIN_EXPECT_FALSE(Platform::IsSafeRelativePath("a/../../x"));
    ENJIN_EXPECT_FALSE(Platform::IsSafeRelativePath("..\\x"));
}

ENJIN_TEST(PathSanitize, SafeRelativePath_AbsoluteOrEmpty_Rejected) {
    ENJIN_EXPECT_FALSE(Platform::IsSafeRelativePath(""));
    ENJIN_EXPECT_FALSE(Platform::IsSafeRelativePath("/abs/path"));
    ENJIN_EXPECT_FALSE(Platform::IsSafeRelativePath("\\abs\\path"));
    ENJIN_EXPECT_FALSE(Platform::IsSafeRelativePath("C:/abs/path"));
    ENJIN_EXPECT_FALSE(Platform::IsSafeRelativePath("C:relative"));
}

// ===========================================================================
// IsSafeFileName
// ===========================================================================

ENJIN_TEST(PathSanitize, SafeFileName_PlainName_Accepted) {
    ENJIN_EXPECT_TRUE(Platform::IsSafeFileName("save1.json"));
    ENJIN_EXPECT_TRUE(Platform::IsSafeFileName("profile_2"));
}

ENJIN_TEST(PathSanitize, SafeFileName_SeparatorsOrDotDot_Rejected) {
    ENJIN_EXPECT_FALSE(Platform::IsSafeFileName(""));
    ENJIN_EXPECT_FALSE(Platform::IsSafeFileName("a/b"));
    ENJIN_EXPECT_FALSE(Platform::IsSafeFileName("a\\b"));
    ENJIN_EXPECT_FALSE(Platform::IsSafeFileName(".."));
    ENJIN_EXPECT_FALSE(Platform::IsSafeFileName("a..b"));
}

// ===========================================================================
// ResolveWithinRoot
// ===========================================================================

ENJIN_TEST(PathSanitize, ResolveWithinRoot_PathInsideRoot_ReturnsNormalized) {
    // Arrange
    std::string root = "C:/proj";
    std::string rel = "scenes/level1.enjin";
    std::string expected =
        (std::filesystem::path(root) / rel).lexically_normal().string();

    // Act
    std::string resolved = Platform::ResolveWithinRoot(root, rel);

    // Assert
    ENJIN_EXPECT_TRUE(resolved == expected);
}

ENJIN_TEST(PathSanitize, ResolveWithinRoot_TraversalEscape_ReturnsEmpty) {
    ENJIN_EXPECT_TRUE(Platform::ResolveWithinRoot("C:/proj", "../outside.txt").empty());
    ENJIN_EXPECT_TRUE(Platform::ResolveWithinRoot("C:/proj", "a/../../outside.txt").empty());
}

ENJIN_TEST(PathSanitize, ResolveWithinRoot_SiblingPrefixRoot_ReturnsEmpty) {
    // "C:/proj2/x" shares the string prefix "C:/proj" but is outside the root
    ENJIN_EXPECT_TRUE(Platform::ResolveWithinRoot("C:/proj", "../proj2/x").empty());
}

ENJIN_TEST(PathSanitize, ResolveWithinRoot_EmptyInput_ReturnsEmpty) {
    ENJIN_EXPECT_TRUE(Platform::ResolveWithinRoot("", "a.txt").empty());
    ENJIN_EXPECT_TRUE(Platform::ResolveWithinRoot("C:/proj", "").empty());
}

// ===========================================================================
// MakeRelativeToRoot
// ===========================================================================

ENJIN_TEST(PathSanitize, MakeRelativeToRoot_PathInsideRoot_ReturnsRelative) {
    // Arrange
    std::string root = "C:/proj";
    std::string abs = "C:/proj/scenes/level1.enjin";

    // Act
    std::string rel = Platform::MakeRelativeToRoot(root, abs);

    // Assert
    std::string expected = std::filesystem::path("scenes/level1.enjin")
        .lexically_normal().string();
    ENJIN_EXPECT_TRUE(rel == expected);
}

ENJIN_TEST(PathSanitize, MakeRelativeToRoot_PathOutsideRoot_ReturnsEmpty) {
    ENJIN_EXPECT_TRUE(Platform::MakeRelativeToRoot("C:/proj", "C:/other/x.enjin").empty());
    ENJIN_EXPECT_TRUE(Platform::MakeRelativeToRoot("C:/proj", "C:/proj2/x.enjin").empty());
}

ENJIN_TEST(PathSanitize, MakeRelativeToRoot_EmptyInput_ReturnsEmpty) {
    ENJIN_EXPECT_TRUE(Platform::MakeRelativeToRoot("", "C:/proj/x").empty());
    ENJIN_EXPECT_TRUE(Platform::MakeRelativeToRoot("C:/proj", "").empty());
}

// ---------------------------------------------------------------------------
// HasUpwardTraversal - what it does, and what it deliberately does not
//
// Call sites spelled this as `lexically_normal().string().find("..") != npos`,
// which rejected an innocent "level..2.enjin" and told the user it was path
// traversal. This replaces the substring test with a COMPONENT test.
//
// Read the second test before trusting this for security. Normalization
// ABSORBS ".." in an absolute path -- "C:/proj/a/../../etc/passwd" collapses to
// "C:/etc/passwd" with no ".." left -- so neither this nor the substring check
// it replaces ever caught that. On absolute paths the check is near-decorative,
// and it always was. Confining a path to a directory needs
// Platform::ResolveWithinRoot against that root, which is a different (and
// currently absent) design decision for save targets.
// ---------------------------------------------------------------------------

ENJIN_TEST(PathSanitize, HasUpwardTraversal_DetectsSurvivingUpwardComponent) {
    // Relative paths that climb out: the ".." survives normalization.
    ENJIN_EXPECT_TRUE(Platform::HasUpwardTraversal("../secrets.enjin"));
    ENJIN_EXPECT_TRUE(Platform::HasUpwardTraversal("scenes/../../secrets.enjin"));
    // Backslashes count as separators on every platform, because content and
    // project files travel between them.
    ENJIN_EXPECT_TRUE(Platform::HasUpwardTraversal("..\\windows"));
}

ENJIN_TEST(PathSanitize, HasUpwardTraversal_AbsolutePathsAbsorbDotDot) {
    // NOT a bug and NOT an oversight: an absolute path with enough leading
    // components simply resolves elsewhere, it does not escape a root. Anyone
    // reaching for this expecting containment wants ResolveWithinRoot.
    ENJIN_EXPECT_FALSE(Platform::HasUpwardTraversal("C:/proj/scenes/../../etc/passwd"));
    ENJIN_EXPECT_FALSE(Platform::HasUpwardTraversal("/home/u/proj/../../etc/shadow"));
}

ENJIN_TEST(PathSanitize, HasUpwardTraversal_AllowsDotsInsideNames) {
    // The reason this helper exists: these are ordinary file names.
    ENJIN_EXPECT_FALSE(Platform::HasUpwardTraversal("C:/proj/scenes/level..2.enjin"));
    ENJIN_EXPECT_FALSE(Platform::HasUpwardTraversal("/home/u/wall..diffuse.png"));
    ENJIN_EXPECT_FALSE(Platform::HasUpwardTraversal("a..b"));
}

ENJIN_TEST(PathSanitize, HasUpwardTraversal_AllowsPlainAndCollapsingPaths) {
    ENJIN_EXPECT_FALSE(Platform::HasUpwardTraversal("C:/proj/scenes/Main.enjin"));
    ENJIN_EXPECT_FALSE(Platform::HasUpwardTraversal("/home/u/proj/Main.enjin"));
    ENJIN_EXPECT_FALSE(Platform::HasUpwardTraversal("C:/proj/a/../b/Main.enjin"));
    ENJIN_EXPECT_FALSE(Platform::HasUpwardTraversal(""));
}

ENJIN_TEST_MAIN()
