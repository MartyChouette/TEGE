#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Gameplay/SaveBackend.h"
#include "Enjin/Networking/NewgroundsAPI.h"
#include <unordered_map>
#include <string>

namespace Enjin {
namespace Networking {

// ISaveBackend wrapping the existing Newgrounds.io cloud save API.
// Uses slot 1 as a metadata slot storing key-to-slot mapping.
// Data slots are 2-10 (9 data slots max).
class ENJIN_API NewgroundsSaveBackend : public Gameplay::ISaveBackend {
public:
    explicit NewgroundsSaveBackend(NewgroundsAPI* api);

    bool Write(const std::string& key, const std::string& data) override;
    bool Read(const std::string& key, std::string& outData) override;
    bool Delete(const std::string& key) override;
    bool Exists(const std::string& key) override;
    std::string GetName() const override { return "Newgrounds"; }

private:
    // S15: Resolve key to a stable slot via metadata in slot 1.
    // Returns 0 if no slot available.
    i32 ResolveSlot(const std::string& key, bool allocateIfMissing);

    // Load/save the metadata mapping from/to NG slot 1
    void LoadMetadata();
    void SaveMetadata();

    NewgroundsAPI* m_Api = nullptr;
    std::unordered_map<std::string, i32> m_KeyToSlot;  // cached metadata
    bool m_MetadataLoaded = false;

    static constexpr i32 METADATA_SLOT = 1;
    static constexpr i32 FIRST_DATA_SLOT = 2;
    static constexpr i32 LAST_DATA_SLOT = 10;
};

} // namespace Networking
} // namespace Enjin
