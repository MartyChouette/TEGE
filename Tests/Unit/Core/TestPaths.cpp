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

ENJIN_TEST_MAIN()
