#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Gameplay/SaveBackend.h"

#ifdef ENJIN_STEAM

namespace Enjin {
namespace Networking {

// ISaveBackend using Steam Cloud (ISteamRemoteStorage).
// Files are stored as key-named files in Steam Cloud.
class ENJIN_API SteamSaveBackend : public Gameplay::ISaveBackend {
public:
    SteamSaveBackend() = default;

    bool Write(const std::string& key, const std::string& data) override;
    bool Read(const std::string& key, std::string& outData) override;
    bool Delete(const std::string& key) override;
    bool Exists(const std::string& key) override;
    std::string GetName() const override { return "Steam Cloud"; }

    // Check if Steam Cloud is available
    static bool IsAvailable();
};

} // namespace Networking
} // namespace Enjin

#endif // ENJIN_STEAM
