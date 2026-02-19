#include "EnjinTest.h"
#include "Enjin/Editor/TemplateMarketplace.h"

#include <string>
#include <unordered_set>
#include <algorithm>

using namespace Enjin;
using namespace Enjin::Editor;

// ===========================================================================
// Catalog Integrity
// ===========================================================================

ENJIN_TEST(CatalogIntegrity, CatalogIsNotEmpty) {
    TemplateMarketplace mp;
    mp.Initialize("");
    ENJIN_EXPECT_GT(mp.GetCatalog().size(), (size_t)0);
}

ENJIN_TEST(CatalogIntegrity, Exactly15Entries) {
    TemplateMarketplace mp;
    mp.Initialize("");
    ENJIN_EXPECT_EQ(mp.GetCatalog().size(), (size_t)15);
}

ENJIN_TEST(CatalogIntegrity, AllIDsAreUnique) {
    TemplateMarketplace mp;
    mp.Initialize("");
    std::unordered_set<std::string> ids;
    for (auto& e : mp.GetCatalog()) {
        ENJIN_EXPECT_TRUE(ids.insert(e.id).second); // insert returns false on duplicate
    }
    ENJIN_EXPECT_EQ(ids.size(), mp.GetCatalog().size());
}

ENJIN_TEST(CatalogIntegrity, AllNamesNonEmpty) {
    TemplateMarketplace mp;
    mp.Initialize("");
    for (auto& e : mp.GetCatalog()) {
        ENJIN_EXPECT_FALSE(e.name.empty());
        ENJIN_EXPECT_FALSE(e.description.empty());
        ENJIN_EXPECT_FALSE(e.author.empty());
        ENJIN_EXPECT_FALSE(e.category.empty());
    }
}

ENJIN_TEST(CatalogIntegrity, AllRatingsInRange) {
    TemplateMarketplace mp;
    mp.Initialize("");
    for (auto& e : mp.GetCatalog()) {
        ENJIN_EXPECT_GE(e.rating, 0.0f);
        ENJIN_EXPECT_LE(e.rating, 5.0f);
    }
}

ENJIN_TEST(CatalogIntegrity, AllFileSizesPositive) {
    TemplateMarketplace mp;
    mp.Initialize("");
    for (auto& e : mp.GetCatalog()) {
        ENJIN_EXPECT_GT(e.fileSizeBytes, (usize)0);
    }
}

ENJIN_TEST(CatalogIntegrity, AllProjectModesValid) {
    TemplateMarketplace mp;
    mp.Initialize("");
    std::unordered_set<std::string> validModes = { "2D", "3D", "Mixed" };
    for (auto& e : mp.GetCatalog()) {
        ENJIN_EXPECT_TRUE(validModes.count(e.projectMode) > 0);
    }
}

ENJIN_TEST(CatalogIntegrity, AllEntriesHaveTags) {
    TemplateMarketplace mp;
    mp.Initialize("");
    for (auto& e : mp.GetCatalog()) {
        ENJIN_EXPECT_GT(e.tags.size(), (size_t)0);
    }
}

ENJIN_TEST(CatalogIntegrity, AccentColorsAreNormalized) {
    TemplateMarketplace mp;
    mp.Initialize("");
    for (auto& e : mp.GetCatalog()) {
        for (int i = 0; i < 4; ++i) {
            ENJIN_EXPECT_GE(e.accentColor[i], 0.0f);
            ENJIN_EXPECT_LE(e.accentColor[i], 1.0f);
        }
        ENJIN_EXPECT_FLOAT_EQ(e.accentColor[3], 1.0f); // alpha always 1
    }
}

// ===========================================================================
// Maturity Tiers
// ===========================================================================

ENJIN_TEST(MaturityTier, AllTiersRepresented) {
    TemplateMarketplace mp;
    mp.Initialize("");
    bool hasStable = false, hasBeta = false, hasPreview = false, hasExperimental = false;
    for (auto& e : mp.GetCatalog()) {
        switch (e.maturity) {
            case MaturityTier::Stable:       hasStable = true; break;
            case MaturityTier::Beta:         hasBeta = true; break;
            case MaturityTier::Preview:      hasPreview = true; break;
            case MaturityTier::Experimental: hasExperimental = true; break;
        }
    }
    ENJIN_EXPECT_TRUE(hasStable);
    ENJIN_EXPECT_TRUE(hasBeta);
    ENJIN_EXPECT_TRUE(hasPreview);
    ENJIN_EXPECT_TRUE(hasExperimental);
}

ENJIN_TEST(MaturityTier, GetMaturityNameReturnsNonNull) {
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetMaturityName(MaturityTier::Stable), "Stable");
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetMaturityName(MaturityTier::Beta), "Beta");
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetMaturityName(MaturityTier::Preview), "Preview");
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetMaturityName(MaturityTier::Experimental), "Experimental");
}

ENJIN_TEST(MaturityTier, StarterTemplatesAreStable) {
    TemplateMarketplace mp;
    mp.Initialize("");
    for (auto& e : mp.GetCatalog()) {
        if (e.category == "Starter") {
            ENJIN_EXPECT_EQ((int)e.maturity, (int)MaturityTier::Stable);
        }
    }
}

// ===========================================================================
// Quality Names
// ===========================================================================

ENJIN_TEST(QualityNames, GetQualityNameReturnsNonNull) {
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetQualityName(TemplateQuality::Official), "Official");
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetQualityName(TemplateQuality::Community), "Community");
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetQualityName(TemplateQuality::Experimental), "Experimental");
}

ENJIN_TEST(QualityNames, GetCategoryNameReturnsNonNull) {
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetCategoryName(MarketplaceCategory::Starter), "Starter");
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetCategoryName(MarketplaceCategory::Genre), "Genre");
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetCategoryName(MarketplaceCategory::Systems), "Systems");
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetCategoryName(MarketplaceCategory::Retro), "Retro");
    ENJIN_EXPECT_STR_EQ(TemplateMarketplace::GetCategoryName(MarketplaceCategory::Advanced), "Advanced");
}

// ===========================================================================
// Search & Filter
// ===========================================================================

ENJIN_TEST(SearchFilter, SearchByNameFindsMatch) {
    TemplateMarketplace mp;
    mp.Initialize("");
    auto results = mp.Search("Empty Canvas");
    ENJIN_EXPECT_GE(results.size(), (size_t)1);
    bool found = false;
    for (auto* r : results) {
        if (r->id == "empty_canvas") found = true;
    }
    ENJIN_EXPECT_TRUE(found);
}

ENJIN_TEST(SearchFilter, SearchByTagFindsMatch) {
    TemplateMarketplace mp;
    mp.Initialize("");
    auto results = mp.Search("roguelike");
    ENJIN_EXPECT_GE(results.size(), (size_t)1);
}

ENJIN_TEST(SearchFilter, SearchNoResultsForGarbage) {
    TemplateMarketplace mp;
    mp.Initialize("");
    auto results = mp.Search("xyzzy_nonexistent_template_name_12345");
    ENJIN_EXPECT_EQ(results.size(), (size_t)0);
}

ENJIN_TEST(SearchFilter, FilterByCategoryStarter) {
    TemplateMarketplace mp;
    mp.Initialize("");
    auto results = mp.FilterByCategory("Starter");
    ENJIN_EXPECT_GE(results.size(), (size_t)3); // Empty Canvas, Hello Sprite, Physics Playground
    for (auto* r : results) {
        ENJIN_EXPECT_STR_EQ(r->category.c_str(), "Starter");
    }
}

ENJIN_TEST(SearchFilter, FilterByCategoryAll) {
    TemplateMarketplace mp;
    mp.Initialize("");
    auto results = mp.FilterByCategory("All");
    ENJIN_EXPECT_EQ(results.size(), mp.GetCatalog().size());
}

ENJIN_TEST(SearchFilter, FilterAndSearchCombines) {
    TemplateMarketplace mp;
    mp.Initialize("");
    auto results = mp.FilterAndSearch("ray", "Advanced");
    bool foundRT = false;
    for (auto* r : results) {
        if (r->id == "ray_tracing_showcase") foundRT = true;
    }
    ENJIN_EXPECT_TRUE(foundRT);
}

// ===========================================================================
// FindById
// ===========================================================================

ENJIN_TEST(FindById, ExistingIdReturnsEntry) {
    TemplateMarketplace mp;
    mp.Initialize("");
    auto* entry = mp.FindById("empty_canvas");
    ENJIN_ASSERT_NOT_NULL(entry);
    ENJIN_EXPECT_STR_EQ(entry->name.c_str(), "Empty Canvas");
}

ENJIN_TEST(FindById, NonExistentIdReturnsNull) {
    TemplateMarketplace mp;
    mp.Initialize("");
    auto* entry = mp.FindById("this_id_does_not_exist");
    ENJIN_EXPECT_NULL(entry);
}

ENJIN_TEST(FindById, AllCatalogIdsResolvable) {
    TemplateMarketplace mp;
    mp.Initialize("");
    for (auto& e : mp.GetCatalog()) {
        auto* found = mp.FindById(e.id);
        ENJIN_ASSERT_NOT_NULL(found);
        ENJIN_EXPECT_STR_EQ(found->name.c_str(), e.name.c_str());
    }
}

// ===========================================================================
// Install / Uninstall (dry — no filesystem)
// ===========================================================================

ENJIN_TEST(InstallState, InitiallyNotInstalled) {
    TemplateMarketplace mp;
    mp.Initialize("");
    for (auto& e : mp.GetCatalog()) {
        ENJIN_EXPECT_FALSE(mp.IsInstalled(e.id));
    }
}

ENJIN_TEST(InstallState, OpenStateToggle) {
    TemplateMarketplace mp;
    mp.Initialize("");
    ENJIN_EXPECT_FALSE(mp.IsOpen());
    mp.SetOpen(true);
    ENJIN_EXPECT_TRUE(mp.IsOpen());
    mp.SetOpen(false);
    ENJIN_EXPECT_FALSE(mp.IsOpen());
}

ENJIN_TEST_MAIN()
