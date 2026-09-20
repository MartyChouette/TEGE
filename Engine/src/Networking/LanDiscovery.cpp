#include "Enjin/Networking/LanDiscovery.h"
#include "Enjin/Networking/NetworkSerializer.h"
#include "Enjin/Networking/TransportFactory.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>

namespace Enjin {
namespace Networking {

namespace {
// Caps on what an announcement may claim. An announcement is unauthenticated by
// nature -- it is a broadcast from a stranger on the subnet -- so every length
// is bounded before it is believed. The cost of getting this wrong is an
// attacker on your own LAN filling the browser with garbage.
constexpr usize kMaxNameLength = 64;
constexpr usize kMaxSessions = 64;
}

bool LanDiscovery::StartAnnouncing(u16 gamePort, const std::string& sessionName,
                                   const std::string& gameId, u8 maxPlayers) {
    if (!m_Transport) {
        m_Transport = CreateTransport(TransportType::Auto);
        if (!m_Transport) {
            ENJIN_LOG_ERROR(Network, "LanDiscovery: no transport available on this platform");
            return false;
        }
    }
    // Bind an ephemeral port: a host SENDS announcements and does not need to
    // own the discovery port, which a listener on the same machine may hold.
    if (!m_Transport->IsOpen() && !m_Transport->Bind(0)) {
        ENJIN_LOG_ERROR(Network, "LanDiscovery: failed to bind announce socket");
        return false;
    }

    m_GamePort = gamePort;
    m_SessionName = sessionName.substr(0, kMaxNameLength);
    m_GameId = gameId.substr(0, kMaxNameLength);
    m_MaxPlayers = maxPlayers;
    m_Announcing = true;
    m_NextAnnounceTime = m_Time;  // announce immediately, do not wait an interval

    ENJIN_LOG_INFO(Network, "LanDiscovery: announcing '%s' on game port %u as '%s'",
                   m_SessionName.c_str(), gamePort, m_GameId.c_str());
    return true;
}

bool LanDiscovery::StartListening(const std::string& gameId) {
    if (!m_Transport) {
        m_Transport = CreateTransport(TransportType::Auto);
        if (!m_Transport) {
            ENJIN_LOG_ERROR(Network, "LanDiscovery: no transport available on this platform");
            return false;
        }
    }
    if (!m_Transport->IsOpen() && !m_Transport->Bind(DISCOVERY_PORT)) {
        ENJIN_LOG_ERROR(Network, "LanDiscovery: failed to bind discovery port %u "
                                 "(another instance listening?)", DISCOVERY_PORT);
        return false;
    }

    m_GameId = gameId.substr(0, kMaxNameLength);
    m_Listening = true;
    m_Sessions.clear();

    ENJIN_LOG_INFO(Network, "LanDiscovery: listening on port %u for '%s'",
                   DISCOVERY_PORT, m_GameId.empty() ? "(any game)" : m_GameId.c_str());
    return true;
}

void LanDiscovery::Stop() {
    if (m_Transport) m_Transport->Close();
    m_Announcing = false;
    m_Listening = false;
    m_Sessions.clear();
}

void LanDiscovery::Update(f32 deltaTime) {
    if (!m_Transport || (!m_Announcing && !m_Listening)) return;

    m_Time += deltaTime;

    if (m_Announcing && m_Time >= m_NextAnnounceTime) {
        SendAnnouncement();
        m_NextAnnounceTime = m_Time + DISCOVERY_ANNOUNCE_INTERVAL;
    }
    if (m_Listening) {
        ReceiveAnnouncements();
        ExpireStaleSessions();
    }
}

std::vector<u8> LanDiscovery::BuildAnnouncement() const {
    std::vector<u8> buf;
    WriteU32(buf, DISCOVERY_MAGIC);
    WriteU8(buf, DISCOVERY_VERSION);
    WriteU16(buf, m_GamePort);
    WriteU8(buf, m_PlayerCount);
    WriteU8(buf, m_MaxPlayers);
    WriteString(buf, m_SessionName);
    WriteString(buf, m_GameId);
    return buf;
}

void LanDiscovery::SendAnnouncement() {
    const std::vector<u8> payload = BuildAnnouncement();

    NetworkAddress broadcast;
    broadcast.ip = NetworkAddress::ParseIP("255.255.255.255");
    broadcast.port = DISCOVERY_PORT;
    m_Transport->SendTo(broadcast, payload.data(), static_cast<u32>(payload.size()));
}

void LanDiscovery::ReceiveAnnouncements() {
    u8 buffer[512];
    NetworkAddress sender;
    // Bounded like the game receive loop: a flooded subnet must not turn into an
    // unbounded frame.
    for (int i = 0; i < 32; i++) {
        const i32 received = m_Transport->ReceiveFrom(sender, buffer, sizeof(buffer));
        if (received <= 0) break;
        HandleAnnouncement(sender, buffer, static_cast<u32>(received));
    }
}

void LanDiscovery::HandleAnnouncement(const NetworkAddress& from, const u8* data, u32 size) {
    // magic(4) + version(1) + port(2) + players(1) + max(1) = 9 before the strings
    if (size < 9) return;

    u32 offset = 0;
    if (ReadU32(data, offset, size) != DISCOVERY_MAGIC) return;

    const u8 version = ReadU8(data, offset, size);
    if (version != DISCOVERY_VERSION) {
        // Versioned from the first byte precisely so this is a shrug rather than
        // a misparse. Do not try to read a format from the future.
        return;
    }

    const u16 gamePort = ReadU16(data, offset, size);
    const u8 playerCount = ReadU8(data, offset, size);
    const u8 maxPlayers = ReadU8(data, offset, size);
    const std::string sessionName = ReadString(data, offset, size);
    const std::string gameId = ReadString(data, offset, size);

    if (gamePort == 0) return;
    // Listening for one game means ignoring every other game on the subnet.
    if (!m_GameId.empty() && gameId != m_GameId) return;
    if (sessionName.size() > kMaxNameLength || gameId.size() > kMaxNameLength) return;

    // The address to JOIN is the announcer's IP with the announced GAME port,
    // never the port the announcement arrived from -- that is an ephemeral
    // sending port and connecting to it reaches nothing.
    NetworkAddress joinAddr;
    joinAddr.ip = from.ip;
    joinAddr.port = gamePort;

    for (auto& s : m_Sessions) {
        if (s.address == joinAddr) {
            s.sessionName = sessionName;
            s.playerCount = playerCount;
            s.maxPlayers = maxPlayers;
            s.lastSeen = m_Time;
            return;
        }
    }

    if (m_Sessions.size() >= kMaxSessions) return;

    DiscoveredSession s;
    s.address = joinAddr;
    s.sessionName = sessionName;
    s.gameId = gameId;
    s.playerCount = playerCount;
    s.maxPlayers = maxPlayers;
    s.lastSeen = m_Time;
    m_Sessions.push_back(s);

    ENJIN_LOG_INFO(Network, "LanDiscovery: found '%s' at %s:%u (%u/%u players)",
                   sessionName.c_str(), NetworkAddress::IPToString(joinAddr.ip).c_str(),
                   gamePort, playerCount, maxPlayers);
}

void LanDiscovery::ExpireStaleSessions() {
    const f32 now = m_Time;
    m_Sessions.erase(
        std::remove_if(m_Sessions.begin(), m_Sessions.end(),
                       [now](const DiscoveredSession& s) {
                           return now - s.lastSeen > DISCOVERY_SESSION_TIMEOUT;
                       }),
        m_Sessions.end());
}

} // namespace Networking
} // namespace Enjin
