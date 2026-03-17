#include "Enjin/Networking/NetworkTypes.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;

namespace Enjin {
namespace Networking {

std::string NetworkConfig::GetDefaultPath() {
    std::filesystem::path base = std::filesystem::current_path();
    std::filesystem::path path = base / "config" / "network_settings.json";
    return path.string();
}

bool NetworkConfig::SaveToFile(const std::string& path) const {
    std::string savePath = path.empty() ? GetDefaultPath() : path;
    std::filesystem::path filePath(savePath);

    std::error_code ec;
    std::filesystem::create_directories(filePath.parent_path(), ec);

    json j;
    j["port"] = port;
    j["maxPlayers"] = maxPlayers;
    j["serverIP"] = serverIP;
    j["syncRate"] = syncRate;

    j["rateLimit"] = {
        {"maxPacketsPerSecond", maxPacketsPerSecond},
        {"maxBytesPerSecond", maxBytesPerSecond},
        {"burstPackets", burstPackets},
        {"burstBytes", burstBytes}
    };

    j["security"] = {
        {"maxViolations", maxViolations},
        {"violationWindowSeconds", violationWindowSeconds},
        {"banSeconds", banSeconds},
        {"kickOnViolation", kickOnViolation}
    };

    j["interpolation"] = {
        {"interpDelay", interpDelay},
        {"predictionCorrectionSpeed", predictionCorrectionSpeed}
    };

    std::ofstream out(savePath, std::ios::binary);
    if (!out) {
        ENJIN_LOG_WARN(Network, "NetworkConfig: Failed to open '%s' for writing", savePath.c_str());
        return false;
    }

    out << j.dump(2);
    return true;
}

bool NetworkConfig::LoadFromFile(const std::string& path) {
    std::string loadPath = path.empty() ? GetDefaultPath() : path;
    if (!std::filesystem::exists(loadPath)) return false;

    std::ifstream in(loadPath, std::ios::binary);
    if (!in) {
        ENJIN_LOG_WARN(Network, "NetworkConfig: Failed to open '%s' for reading", loadPath.c_str());
        return false;
    }

    json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        ENJIN_LOG_WARN(Network, "NetworkConfig: Failed to parse '%s' (%s)", loadPath.c_str(), e.what());
        return false;
    }

    if (j.contains("port")) port = j["port"].get<u16>();
    if (j.contains("maxPlayers")) maxPlayers = j["maxPlayers"].get<u32>();
    if (j.contains("serverIP")) serverIP = j["serverIP"].get<std::string>();
    if (j.contains("syncRate")) syncRate = j["syncRate"].get<f32>();

    if (j.contains("rateLimit") && j["rateLimit"].is_object()) {
        auto& rl = j["rateLimit"];
        if (rl.contains("maxPacketsPerSecond")) maxPacketsPerSecond = rl["maxPacketsPerSecond"].get<f32>();
        if (rl.contains("maxBytesPerSecond")) maxBytesPerSecond = rl["maxBytesPerSecond"].get<f32>();
        if (rl.contains("burstPackets")) burstPackets = rl["burstPackets"].get<f32>();
        if (rl.contains("burstBytes")) burstBytes = rl["burstBytes"].get<f32>();
    }

    if (j.contains("security") && j["security"].is_object()) {
        auto& sec = j["security"];
        if (sec.contains("maxViolations")) maxViolations = sec["maxViolations"].get<u32>();
        if (sec.contains("violationWindowSeconds")) violationWindowSeconds = sec["violationWindowSeconds"].get<f32>();
        if (sec.contains("banSeconds")) banSeconds = sec["banSeconds"].get<f32>();
        if (sec.contains("kickOnViolation")) kickOnViolation = sec["kickOnViolation"].get<bool>();
    }

    if (j.contains("interpolation") && j["interpolation"].is_object()) {
        auto& interp = j["interpolation"];
        if (interp.contains("interpDelay")) interpDelay = interp["interpDelay"].get<f32>();
        if (interp.contains("predictionCorrectionSpeed")) predictionCorrectionSpeed = interp["predictionCorrectionSpeed"].get<f32>();
    }

    ENJIN_LOG_INFO(Network, "NetworkConfig: Loaded settings from '%s'", loadPath.c_str());
    return true;
}

} // namespace Networking
} // namespace Enjin
