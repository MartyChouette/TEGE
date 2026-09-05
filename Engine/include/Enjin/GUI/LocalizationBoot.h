#pragma once
// Loading a project's string tables, for every runtime.
//
// LocalizationManager has existed complete-looking since it was written -- and
// with zero consumers. Nothing in the editor, the desktop player or the web
// player ever called SetLocale or a loader, so the table was always empty and
// every lookup returned its own key back. This is the call that changes that,
// and it lives in one place so the three runtimes cannot drift.
//
// The `localization` block of a .enjinproject (carried verbatim into an
// exported game's manifest) looks like:
//
//   "localization": {
//     "defaultLocale": "en",
//     "locales": [ { "code": "en", "name": "English" },
//                  { "code": "fr", "name": "Francais" } ],
//     "tables":  [ "assets/strings.csv" ]
//   }
//
// Absent, or with no tables, every lookup falls back to the literal the call
// site passed -- which is exactly how an untranslated project behaves today,
// so adding this changes nothing until a project opts in.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#include <string>

namespace Enjin {

namespace Build { class AssetReader; }

namespace GUI {

// Applies a `localization` JSON block: registers its locales, loads every
// table it names, then selects the locale.
//
// `assetRoot` resolves relative table paths for the loose-file runtimes.
// `reader`, when non-null, is tried FIRST and is the only path that works on
// web or inside a packed game -- there are no loose files there, and the
// table loaders were ifstream-only until LoadFromMemory existed.
//
// Returns the number of tables successfully loaded.
ENJIN_API u32 ApplyLocalizationSettings(const std::string& localizationJson,
                                        const std::string& assetRoot,
                                        Build::AssetReader* reader = nullptr);

} // namespace GUI
} // namespace Enjin
