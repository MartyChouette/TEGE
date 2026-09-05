#include "Enjin/Renderer/TextCodepointSet.h"
#include "Enjin/Renderer/TextEncoding.h"
#include "Enjin/GUI/Localization.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Text.h"

#include <algorithm>
#include <unordered_set>

namespace Enjin {
namespace Renderer {

namespace {

// Below this the atlas bakes unconditionally, so collecting it would only
// inflate the set and churn the cache key.
constexpr u32 kAlwaysBakedMax = 255u;

void AddString(const std::string& s, std::unordered_set<u32>& out) {
    usize i = 0;
    while (i < s.size()) {
        const u32 cp = DecodeUTF8(s, i);
        if (cp > kAlwaysBakedMax && cp != 0xFFFDu) out.insert(cp);
    }
}

} // namespace

std::vector<u32> CollectProjectCodepoints(ECS::World* world) {
    std::unordered_set<u32> seen;

    // Every string in the current locale. This is what makes a translated UI
    // renderable: the table is loaded before the first frame draws, so the
    // atlas can be baked knowing exactly which letters the game will show.
    {
        GUI::LocalizationManager& loc = GUI::LocalizationManager::Get();
        for (const std::string& key : loc.GetAllKeys()) {
            AddString(loc.GetString(key), seen);
        }
    }

    // Plus anything authored directly into world text, which a string table
    // never sees.
    if (world) {
        for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::TextComponent>()) {
            if (auto* tc = world->GetComponent<ECS::TextComponent>(e)) {
                AddString(tc->text, seen);
            }
        }
    }

    std::vector<u32> out(seen.begin(), seen.end());
    // Sorted so the same content always produces the same hash, whatever order
    // the hash set happened to iterate in.
    std::sort(out.begin(), out.end());
    return out;
}

u64 HashCodepoints(const std::vector<u32>& codepoints) {
    // FNV-1a. Not cryptographic -- this only has to distinguish one project's
    // character set from another's.
    u64 h = 1469598103934665603ull;
    for (u32 cp : codepoints) {
        for (int byte = 0; byte < 4; ++byte) {
            h ^= static_cast<u64>((cp >> (byte * 8)) & 0xFFu);
            h *= 1099511628211ull;
        }
    }
    return h;
}

} // namespace Renderer
} // namespace Enjin
