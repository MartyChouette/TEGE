// The symbol library wrote zero-byte files named thumbnail.png.
//
//     // Create a placeholder thumbnail path. Actual rendering to PNG would
//     // require an offscreen framebuffer pass. For now we create the path
//     // so the catalog entry is valid; the rendering subsystem can populate
//     // the file later when the browser requests it.
//     std::ofstream marker(thumbPath, std::ios::binary);
//
// Nothing ever populated it and nothing was going to -- there is no code
// anywhere that looks for an empty thumbnail and fills it in. So the catalog
// filled up with paths to files that are not images. A loader handed one gets a
// decode failure, which reads as a corrupt asset rather than as a thumbnail that
// was never rendered, and an empty path would have said the right thing for free.
//
// A VECTOR symbol can be rendered right there, headlessly, through the
// tessellator and rasterizer the engine already has -- the same pair that fixed
// "Export PNG writes an SVG". A prefab needs a render target and a camera, which
// this class has no access to, so it returns nothing and says nothing.
#include "EnjinTest.h"
#include "Enjin/Editor/SymbolLibrary.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace Enjin;
using namespace Enjin::Editor;

namespace {

namespace fs = std::filesystem;

// A scratch library directory, removed on the way out.
struct TempLibrary {
    fs::path dir;
    explicit TempLibrary(const char* tag) {
        static int counter = 0;
        dir = fs::temp_directory_path() /
              ("enjin_symlib_" + std::string(tag) + "_" + std::to_string(++counter));
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
    }
    ~TempLibrary() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    std::string str() const { return dir.string(); }
};

usize FileSize(const std::string& path) {
    std::error_code ec;
    const auto n = fs::file_size(path, ec);
    return ec ? 0 : static_cast<usize>(n);
}

bool LooksLikeAPng(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    unsigned char sig[8] = {};
    f.read(reinterpret_cast<char*>(sig), 8);
    const unsigned char kPng[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    for (int i = 0; i < 8; ++i) {
        if (sig[i] != kPng[i]) return false;
    }
    return true;
}

} // namespace

ENJIN_TEST(SymbolThumbnails, AVectorSymbolGetsARealImageNotAnEmptyFile) {
    // Arrange: a symbol on disk with a real drawing in it.
    TempLibrary lib("vec");
    const fs::path symDir = lib.dir / "square";
    std::error_code ec;
    fs::create_directories(symDir, ec);
    {
        std::ofstream svg(symDir / "square.svg", std::ios::binary);
        svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"64\" height=\"64\">"
               "<rect x=\"8\" y=\"8\" width=\"48\" height=\"48\" fill=\"#19ffcc\"/></svg>";
    }

    SymbolLibrary library;
    library.Initialize(lib.str());
    library.ScanLibrary();
    const SymbolEntry* found = library.FindSymbol("square");
    if (!found) {
        ENJIN_SKIP("scan did not discover the fixture symbol");
        return;
    }

    // Act
    const std::string thumb = library.RegenerateThumbnail("square");

    // Assert
    ENJIN_ASSERT_FALSE(thumb.empty());
    ENJIN_ASSERT_TRUE(fs::exists(thumb));
    // The regression: the old version produced exactly zero bytes.
    ENJIN_EXPECT_TRUE(FileSize(thumb) > 0);
    // And it is a PNG, not whatever else got written. Checking the signature is
    // the difference between "a file exists" and "a loader can open it".
    ENJIN_EXPECT_TRUE(LooksLikeAPng(thumb));
    // The catalog points at it too, rather than the path being returned and
    // dropped.
    ENJIN_EXPECT_TRUE(library.FindSymbol("square")->thumbnailPath == thumb);
}

ENJIN_TEST(SymbolThumbnails, ADrawingWithNothingInItGetsNoThumbnail) {
    // An empty SVG tessellates to nothing. The honest result is no thumbnail --
    // which is exactly the case the old code turned into a zero-byte PNG.
    TempLibrary lib("empty");
    const fs::path symDir = lib.dir / "blank";
    std::error_code ec;
    fs::create_directories(symDir, ec);
    {
        std::ofstream svg(symDir / "blank.svg", std::ios::binary);
        svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\"></svg>";
    }

    SymbolLibrary library;
    library.Initialize(lib.str());
    library.ScanLibrary();
    if (!library.FindSymbol("blank")) {
        ENJIN_SKIP("scan did not discover the fixture symbol");
        return;
    }

    ENJIN_EXPECT_TRUE(library.RegenerateThumbnail("blank").empty());
    ENJIN_EXPECT_FALSE(fs::exists(symDir / "thumbnail.png"));
}

ENJIN_TEST(SymbolThumbnails, APrefabSymbolGetsNoPathRatherThanAnEmptyFile) {
    // A prefab thumbnail needs an offscreen render this class cannot do. An empty
    // path is how a browser knows to draw its own placeholder; a path to a
    // zero-byte PNG is how it gets a decode error instead.
    TempLibrary lib("prefab");
    SymbolLibrary library;
    library.Initialize(lib.str());

    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    world.AddComponent<ECS::NameComponent>(e);
    world.GetComponent<ECS::NameComponent>(e)->name = "Thing";

    const std::string id = library.CreateSymbolFromEntity(&world, e, "Thing", "Props");
    if (id.empty()) {
        ENJIN_SKIP("prefab export unavailable in this environment");
        return;
    }

    const SymbolEntry* sym = library.FindSymbol(id);
    ENJIN_ASSERT_NOT_NULL(sym);

    // The point: nothing, rather than a zero-byte file presented as an image.
    ENJIN_EXPECT_TRUE(sym->thumbnailPath.empty());
    ENJIN_EXPECT_FALSE(fs::exists(lib.dir / id / "thumbnail.png"));
}

ENJIN_TEST(SymbolThumbnails, AZeroByteThumbnailOnDiskIsNotAdopted) {
    // Those files still exist next to symbols made by an older build, and a
    // rescan would put the same broken paths into a fresh catalog. Existence is
    // not the test; size is.
    TempLibrary lib("scan");
    const fs::path symDir = lib.dir / "legacy_symbol";
    std::error_code ec;
    fs::create_directories(symDir, ec);
    {
        std::ofstream svg(symDir / "legacy_symbol.svg", std::ios::binary);
        svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"8\"></svg>";
    }
    {
        std::ofstream marker(symDir / "thumbnail.png", std::ios::binary);   // zero bytes
    }
    ENJIN_ASSERT_TRUE(fs::exists(symDir / "thumbnail.png"));
    ENJIN_ASSERT_EQ(FileSize((symDir / "thumbnail.png").string()), static_cast<usize>(0));

    SymbolLibrary library;
    library.Initialize(lib.str());
    library.ScanLibrary();

    for (const auto& sym : library.GetAllSymbols()) {
        if (sym.thumbnailPath.empty()) continue;
        // Anything the catalog does point at has to be a real image.
        ENJIN_EXPECT_TRUE(FileSize(sym.thumbnailPath) > 0);
        ENJIN_EXPECT_TRUE(LooksLikeAPng(sym.thumbnailPath));
    }
}

ENJIN_TEST_MAIN()
