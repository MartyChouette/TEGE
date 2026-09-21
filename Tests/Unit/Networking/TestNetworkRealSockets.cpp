#include "EnjinTest.h"
#include "Enjin/Networking/LanDiscovery.h"
#include "Enjin/Networking/NetworkSystem.h"
#include <string>

using namespace Enjin;
using namespace Enjin::Networking;

// ============================================================================
// THE SAME THINGS, OVER REAL SOCKETS
//
// Every other networking suite here injects LoopbackTransport, which routes by
// port inside one process and never opens a socket. That is the right default:
// it is fast, deterministic, and it proved the reliable layer. But it means an
// entire class of defect is invisible to all of it -- anything that is wrong
// about the SOCKET rather than about the protocol.
//
// Found the hard way (adr-0007 Track B step 2): a dedicated server logged that
// it was announcing on the LAN and put nothing on the wire, and nine green
// loopback discovery tests had no opinion, because on the loopback bus a
// broadcast is just another routed port.
//
// These bind real UDP sockets on 127.0.0.1. They are deliberately few, because
// the protocol is already covered elsewhere and duplicating it here would only
// buy flakiness; what they cover is that the real transport is wired up at all.
//
// Ports are high and fixed per test. A bind failure is reported as a SKIP
// rather than a failure: a busy port on a developer's machine is not a defect
// in this engine, and a test that fails for that reason gets ignored, which is
// worse than one that says why it did not run.
// ============================================================================

namespace {

// Pump both sides. The handshake is a round trip, so nothing here may test a
// connection state on the line after JoinGame -- that is a documented trap and
// it is exactly as true over real sockets as over loopback.
bool PumpUntilConnected(NetworkSystem& host, NetworkSystem& client, int maxFrames) {
    const f32 dt = 1.0f / 60.0f;
    for (int i = 0; i < maxFrames; i++) {
        host.Update(dt);
        client.Update(dt);
        if (client.IsConnected()) return true;
    }
    return false;
}

} // namespace

ENJIN_TEST(NetworkRealSockets, AClientConnectsToAHostOverUDP) {
    // Arrange: no transport injected, so both build a real UDP socket.
    NetworkSystem host;
    NetworkSystem client;
    host.SetEnabled(true);
    client.SetEnabled(true);
    host.SetAnnounceOnLan(false);   // this test is about the game socket only

    if (!host.HostGame(47801, "host")) {
        ENJIN_SKIP("could not bind UDP 47801 (port in use on this machine)");
    }

    // Act
    ENJIN_ASSERT_TRUE(client.JoinGame("127.0.0.1", 47801, "client"));
    const bool connected = PumpUntilConnected(host, client, 600);  // 10s of frames

    // Assert
    ENJIN_EXPECT_TRUE(connected);
    ENJIN_EXPECT_TRUE(client.IsConnected());
    ENJIN_EXPECT_TRUE(host.IsHost());
    // The host counts the joiner. Getting this far means the full handshake ran
    // over a socket: request, accept, and the session key exchange that the
    // HMAC trailer is exempt from until it lands.
    ENJIN_EXPECT_TRUE(host.GetConnectedPlayerCount() >= 1);

    client.Disconnect();
    host.Disconnect();
}

ENJIN_TEST(NetworkRealSockets, AnAnnouncementReachesTheWire) {
    // The regression this file exists for. A host announces; a browser on the
    // same machine must SEE it, which requires the datagram to have actually
    // left through a real socket and come back.
    LanDiscovery browser;
    if (!browser.StartListening("realsocket-probe")) {
        ENJIN_SKIP("could not bind the discovery port (another instance listening?)");
    }

    NetworkSystem host;
    host.SetEnabled(true);
    host.SetDiscoveryGameId("realsocket-probe");
    if (!host.HostGame(47802, "announcing host")) {
        ENJIN_SKIP("could not bind UDP 47802 (port in use on this machine)");
    }

    const f32 dt = 1.0f / 60.0f;
    bool found = false;
    for (int i = 0; i < 300 && !found; i++) {   // up to 5s: announce interval is 1s
        host.Update(dt);
        browser.Update(dt);
        found = !browser.GetSessions().empty();
    }

    ENJIN_ASSERT_TRUE(found);
    const auto& s = browser.GetSessions()[0];
    ENJIN_EXPECT_TRUE(s.sessionName == "announcing host");
    // The port to JOIN, not the ephemeral port the announcement was sent from.
    ENJIN_EXPECT_EQ(s.address.port, static_cast<u16>(47802));

    host.Disconnect();
    browser.Stop();
}

ENJIN_TEST_MAIN()
