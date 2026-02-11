#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Gameplay/SaveBackend.h"
#include "Enjin/Networking/NewgroundsAPI.h"
#include <unordered_map>

namespace Enjin {
namespace Networking {

// ISaveBackend wrapping the existing Newgrounds.io cloud save API.
// Maps string keys to NG slot IDs via a simple hash.
class ENJIN_API NewgroundsSaveBackend : public Gameplay::ISaveBackend {
public:
    explicit NewgroundsSaveBackend(NewgroundsAPI* api);

    bool Write(const std::string& key, const std::string& data) override;
    bool Read(const std::string& key, std::string& outData) override;
    bool Delete(const std::string& key) override;
    bool Exists(const std::string& key) override;
    std::string GetName() const override { return "Newgrounds"; }

private:
    // Map key names to NG slot IDs (1-based, up to 10 on NG)
    i32 GetSlotId(const std::string& key) const;

    NewgroundsAPI* m_Api = nullptr;
};

} // namespace Networking
} // namespace Enjin
