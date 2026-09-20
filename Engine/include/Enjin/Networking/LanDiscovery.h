#pragma once

// LAN discovery: hosts announce on the subnet, clients list what they find.
//
// This is step 1 of adr-0007's Track B, and it is deliberately the first piece
// because it needs no decision from anyone and no service from us. It makes the
// host model that ALREADY EXISTS usable: before this, joining a game meant
// somebody reading an IP address out loud. With the internet unplugged and no
// server running anywhere, a host announces and a client sees it.
//
// The rule adr-0007 puts above everything else applies here first: nothing we
// build may become required. Discovery is a convenience for finding an address.
// JoinGame(ip, port) still works with an address typed by hand, and nothing in
// the connect path routes through discovery.
//
// Transport-injected on purpose. The real one is a UDP socket with broadcast
// enabled; a test can hand it a loopback transport and drive a host and a client
// in one process with no sockets at all (Tests/Framework/LoopbackTransport.h).

#include "Enjin/Networking/INetworkTransport.h"
#include "Enjin/Networking/NetworkTypes.h"
#include <memory>
#include <string>
#include <vector>

namespace Enjin {
namespace Networking {

// The port announcements are sent to and listened on. Separate from the game
// port so that discovery works before anyone has agreed on one.
static constexpr u16 DISCOVERY_PORT = 7778;

// Wire identity. Versioned from the first byte, for the reason adr-0007 gives
// about the relay: an installed build WILL meet a newer announcement, and it
// has to be able to ignore it rather than misread it.
static constexpr u32 DISCOVERY_MAGIC = 0x454E4A44;  // 'ENJD'
static constexpr u8  DISCOVERY_VERSION = 1;

static constexpr f32 DISCOVERY_ANNOUNCE_INTERVAL = 1.0f;
// Three missed announcements before a host drops off the list. Long enough that
// one lost datagram does not make a game flicker out of the browser.
static constexpr f32 DISCOVERY_SESSION_TIMEOUT = 3.5f;

// A host somebody could join. `address` is what to hand JoinGame.
struct DiscoveredSession {
    NetworkAddress address;
    std::string sessionName;
    std::string gameId;
    u8 playerCount = 0;
    u8 maxPlayers = 0;
    f32 lastSeen = 0.0f;

    bool IsFull() const { return maxPlayers > 0 && playerCount >= maxPlayers; }
};

class ENJIN_API LanDiscovery {
public:
    LanDiscovery() = default;
    ~LanDiscovery() = default;

    // Inject a transport. Without one, Start* creates a UDP transport with
    // broadcast enabled.
    void SetTransport(std::unique_ptr<INetworkTransport> transport) {
        m_Transport = std::move(transport);
    }

    // HOST: begin announcing this session once per DISCOVERY_ANNOUNCE_INTERVAL.
    // gamePort is the port a client should actually connect to, which is NOT the
    // discovery port. gameId keeps two different games on one subnet from
    // listing each other.
    bool StartAnnouncing(u16 gamePort, const std::string& sessionName,
                         const std::string& gameId, u8 maxPlayers);

    // CLIENT: begin listening. An empty gameId lists everything, which is what a
    // debug tool wants and what a game never does.
    bool StartListening(const std::string& gameId);

    void Stop();

    // Pump. Sends the announcement when due, drains inbound, expires the stale.
    void Update(f32 deltaTime);

    // Announced to clients so a full game can be shown as full rather than
    // discovered and then refused.
    void SetPlayerCount(u8 count) { m_PlayerCount = count; }

    const std::vector<DiscoveredSession>& GetSessions() const { return m_Sessions; }
    bool IsAnnouncing() const { return m_Announcing; }
    bool IsListening() const { return m_Listening; }

    // Test seam: the exact bytes of one announcement.
    std::vector<u8> BuildAnnouncement() const;

private:
    void SendAnnouncement();
    void ReceiveAnnouncements();
    void HandleAnnouncement(const NetworkAddress& from, const u8* data, u32 size);
    void ExpireStaleSessions();

    std::unique_ptr<INetworkTransport> m_Transport;
    bool m_Announcing = false;
    bool m_Listening = false;

    u16 m_GamePort = 0;
    std::string m_SessionName;
    std::string m_GameId;
    u8 m_MaxPlayers = 0;
    u8 m_PlayerCount = 0;

    f32 m_Time = 0.0f;
    f32 m_NextAnnounceTime = 0.0f;
    std::vector<DiscoveredSession> m_Sessions;
};

} // namespace Networking
} // namespace Enjin
