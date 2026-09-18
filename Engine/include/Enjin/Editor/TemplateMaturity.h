#pragma once

#include "Enjin/Platform/Platform.h"
#include <cstdint>

// What is left of TemplateMarketplace.h after the marketplace was cut
// (2026-09-18, Marty: "cut the marketplace ... we will have our templates they
// can open from").
//
// The marketplace was a mock presented as a storefront: Install never fetched
// or copied anything -- it wrote a meta.json and a literal {"entities": []}
// scene, so installing a template produced an empty file while the real one sat
// in builtin_templates/. Its catalog was hardcoded with invented download
// counts and star ratings. The 16 built-in templates are the way in and always
// were.
//
// The maturity tier survives because the built-in template picker badges each
// card with it, and that badge is honest: it says how finished the features a
// template leans on actually are.

namespace Enjin {
namespace Editor {

// Gates a template by the lowest-maturity feature it depends on.
enum class MaturityTier : std::uint8_t {
    Stable = 0,      // Audited, tested, production-ready (Blue)
    Beta = 1,        // Feature-complete, needs hardening (Green)
    Preview = 2,     // Functional but incomplete (Amber)
    Experimental = 3 // Prototypes, may break (Red)
};

ENJIN_API const char* GetMaturityName(MaturityTier tier);

} // namespace Editor
} // namespace Enjin
