#include "Enjin/Editor/TemplateMaturity.h"

namespace Enjin {
namespace Editor {

const char* GetMaturityName(MaturityTier tier) {
    switch (tier) {
        case MaturityTier::Stable:       return "Stable";
        case MaturityTier::Beta:         return "Beta";
        case MaturityTier::Preview:      return "Preview";
        case MaturityTier::Experimental: return "Experimental";
        default: return "Unknown";
    }
}

} // namespace Editor
} // namespace Enjin
