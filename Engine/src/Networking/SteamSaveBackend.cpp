#include "Enjin/Networking/SteamSaveBackend.h"

#ifdef ENJIN_STEAM

#include "Enjin/Logging/Log.h"
#include <steam/steam_api.h>

namespace Enjin {
namespace Networking {

bool SteamSaveBackend::IsAvailable() {
    return SteamAPI_IsSteamRunning() && SteamRemoteStorage() &&
           SteamRemoteStorage()->IsCloudEnabledForAccount() &&
           SteamRemoteStorage()->IsCloudEnabledForApp();
}

bool SteamSaveBackend::Write(const std::string& key, const std::string& data) {
    if (!IsAvailable()) return false;
    bool ok = SteamRemoteStorage()->FileWrite(key.c_str(),
        data.c_str(), static_cast<int32>(data.size()));
    if (!ok) {
        ENJIN_LOG_ERROR(Editor, "SteamSaveBackend: FileWrite failed for '%s'", key.c_str());
    }
    return ok;
}

bool SteamSaveBackend::Read(const std::string& key, std::string& outData) {
    if (!IsAvailable()) return false;
    if (!SteamRemoteStorage()->FileExists(key.c_str())) return false;

    int32 fileSize = SteamRemoteStorage()->GetFileSize(key.c_str());
    if (fileSize <= 0) return false;

    outData.resize(static_cast<size_t>(fileSize));
    int32 bytesRead = SteamRemoteStorage()->FileRead(key.c_str(),
        outData.data(), fileSize);
    if (bytesRead != fileSize) {
        ENJIN_LOG_ERROR(Editor, "SteamSaveBackend: FileRead size mismatch for '%s'", key.c_str());
        outData.clear();
        return false;
    }
    return true;
}

bool SteamSaveBackend::Delete(const std::string& key) {
    if (!IsAvailable()) return false;
    return SteamRemoteStorage()->FileDelete(key.c_str());
}

bool SteamSaveBackend::Exists(const std::string& key) {
    if (!IsAvailable()) return false;
    return SteamRemoteStorage()->FileExists(key.c_str());
}

} // namespace Networking
} // namespace Enjin

#endif // ENJIN_STEAM
