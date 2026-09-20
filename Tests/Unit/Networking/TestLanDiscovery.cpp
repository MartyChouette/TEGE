#include "EnjinTest.h"
#include "LoopbackTransport.h"
#include "Enjin/Networking/LanDiscovery.h"
#include "Enjin/Networking/NetworkSerializer.h"
#include "Enjin/Networking/NetworkSystem.h"
#include <memory>
#include <string>

using namespace Enjin;
using namespace Enjin::Networking;

// ============================================================================
// LAN DISCOVERY (adr-0007 Track B, step 1)
//
// A host announces on the subnet and a client lists what it finds, with no
// server, no account and no internet -- which is the rule adr-0007 puts above
// everything else. Driven here through the loopback bus, so a host and a client
// discover each other in one process with no sockets at all.
//
// The loopback bus routes by port, so the broadcast address in SendAnnouncement
// lands in whatever inbox is bound to DISCOVERY_PORT. That is exactly the
// behaviour a real subnet broadcast has from the listener's point of view.
// ============================================================================

namespace {

using EnjinTestNet::LoopbackBus;
using EnjinTestNet::LoopbackTransport;

struct Pair {
    LoopbackBus bus;
    LanDiscovery host;
    LanDiscovery client;

    void Wire() {
        host.SetTransport(std::make_unique<LoopbackTransport>(&bus));
        client.SetTransport(std::make_unique<LoopbackTransport>(&bus));
    }

    void Pump(int frames, f32 dt = 1.0f / 60.0f) {
        for (int i = 0; i < frames; i++) {
            host.Update(dt);
            client.Update(dt);
        }
    }
};

} // namespace

ENJIN_TEST(LanDiscovery, ClientFindsAnAnnouncedHost) {
    // Arrange
    Pair net;
    net.Wire();
    ENJIN_ASSERT_TRUE(net.client.StartListening("tornado"));
    ENJIN_ASSERT_TRUE(net.host.StartAnnouncing(7777, "Marty's game", "tornado", 4));
    ENJIN_EXPECT_EQ(net.client.GetSessions().size(), static_cast<usize>(0));

    // Act: the first announcement goes out immediately rather than after a full
    // interval, so a listener that was already running sees a host appear at once.
    net.Pump(4);

    // Assert
    ENJIN_ASSERT_TRUE(net.client.GetSessions().size() == 1);
    const auto& s = net.client.GetSessions()[0];
    ENJIN_EXPECT_TRUE(s.sessionName == "Marty's game");
    ENJIN_EXPECT_TRUE(s.gameId == "tornado");
    ENJIN_EXPECT_EQ(s.maxPlayers, static_cast<u8>(4));
    // The address to JOIN carries the announced GAME port, not the ephemeral
    // port the announcement was sent from. Connecting to the latter reaches
    // nothing, and it is the easiest thing in here to get wrong.
    ENJIN_EXPECT_EQ(s.address.port, static_cast<u16>(7777));
}

ENJIN_TEST(LanDiscovery, PlayerCountReachesTheBrowser) {
    Pair net;
    net.Wire();
    net.client.StartListening("tornado");
    net.host.StartAnnouncing(7777, "Marty's game", "tornado", 4);
    net.host.SetPlayerCount(4);

    net.Pump(4);

    ENJIN_ASSERT_TRUE(net.client.GetSessions().size() == 1);
    // A full game should be shown as full rather than discovered and then
    // refused at connect time.
    ENJIN_EXPECT_EQ(net.client.GetSessions()[0].playerCount, static_cast<u8>(4));
    ENJIN_EXPECT_TRUE(net.client.GetSessions()[0].IsFull());
}

ENJIN_TEST(LanDiscovery, AnotherGameOnTheSubnetIsIgnored) {
    Pair net;
    net.Wire();
    net.client.StartListening("tornado");
    net.host.StartAnnouncing(7777, "somebody else's game", "chess", 2);

    net.Pump(8);

    // Two different games on one subnet must not list each other.
    ENJIN_EXPECT_EQ(net.client.GetSessions().size(), static_cast<usize>(0));
}

ENJIN_TEST(LanDiscovery, EmptyGameIdListsEverything) {
    Pair net;
    net.Wire();
    net.client.StartListening("");     // what a debug tool wants, not a game
    net.host.StartAnnouncing(7777, "somebody else's game", "chess", 2);

    net.Pump(4);

    ENJIN_EXPECT_EQ(net.client.GetSessions().size(), static_cast<usize>(1));
}

ENJIN_TEST(LanDiscovery, AHostThatStopsAnnouncingDropsOff) {
    Pair net;
    net.Wire();
    net.client.StartListening("tornado");
    net.host.StartAnnouncing(7777, "Marty's game", "tornado", 4);
    net.Pump(4);
    ENJIN_ASSERT_TRUE(net.client.GetSessions().size() == 1);

    // Host quits. The client keeps running.
    net.host.Stop();
    for (int i = 0; i < 10; i++) net.client.Update(0.5f);   // 5s > timeout

    ENJIN_EXPECT_EQ(net.client.GetSessions().size(), static_cast<usize>(0));
}

ENJIN_TEST(LanDiscovery, ARepeatedAnnouncementRefreshesRatherThanDuplicates) {
    Pair net;
    net.Wire();
    net.client.StartListening("tornado");
    net.host.StartAnnouncing(7777, "Marty's game", "tornado", 4);

    // Several announce intervals: the same host announces over and over.
    net.Pump(10, 0.5f);

    ENJIN_EXPECT_EQ(net.client.GetSessions().size(), static_cast<usize>(1));
}

// Versioned from the first byte, for the reason adr-0007 gives about the relay:
// an installed build WILL meet a newer announcement and has to shrug rather
// than misparse it.
ENJIN_TEST(LanDiscovery, AnnouncementIsVersionedAndIdentifiable) {
    Pair net;
    net.Wire();
    net.host.StartAnnouncing(7777, "Marty's game", "tornado", 4);

    const std::vector<u8> bytes = net.host.BuildAnnouncement();
    ENJIN_ASSERT_TRUE(bytes.size() >= 9);

    u32 offset = 0;
    const u32 magic = ReadU32(bytes.data(), offset, static_cast<u32>(bytes.size()));
    const u8 version = ReadU8(bytes.data(), offset, static_cast<u32>(bytes.size()));
    ENJIN_EXPECT_EQ(magic, DISCOVERY_MAGIC);
    ENJIN_EXPECT_EQ(version, DISCOVERY_VERSION);
}

ENJIN_TEST(LanDiscovery, GarbageOnThePortIsIgnored) {
    Pair net;
    net.Wire();
    net.client.StartListening("tornado");

    // Anything at all can arrive on a broadcast port: another program's
    // protocol, a scanner, a malformed packet. None of it may crash a browser
    // or appear as a joinable game.
    LoopbackTransport noise(&net.bus);
    noise.Bind(41234);
    NetworkAddress to;
    to.ip = NetworkAddress::ParseIP("127.0.0.1");
    to.port = DISCOVERY_PORT;

    const u8 tooShort[3] = { 1, 2, 3 };
    noise.SendTo(to, tooShort, sizeof(tooShort));

    std::vector<u8> wrongMagic;
    WriteU32(wrongMagic, 0xDEADBEEF);
    WriteU8(wrongMagic, DISCOVERY_VERSION);
    WriteU16(wrongMagic, 7777);
    WriteU8(wrongMagic, 0);
    WriteU8(wrongMagic, 4);
    WriteString(wrongMagic, "evil");
    WriteString(wrongMagic, "tornado");
    noise.SendTo(to, wrongMagic.data(), static_cast<u32>(wrongMagic.size()));

    std::vector<u8> futureVersion;
    WriteU32(futureVersion, DISCOVERY_MAGIC);
    WriteU8(futureVersion, DISCOVERY_VERSION + 1);
    WriteU16(futureVersion, 7777);
    WriteU8(futureVersion, 0);
    WriteU8(futureVersion, 4);
    WriteString(futureVersion, "from the future");
    WriteString(futureVersion, "tornado");
    noise.SendTo(to, futureVersion.data(), static_cast<u32>(futureVersion.size()));

    net.client.Update(1.0f / 60.0f);

    ENJIN_EXPECT_EQ(net.client.GetSessions().size(), static_cast<usize>(0));
}

// Discovery is a convenience for FINDING an address. It must never become the
// only way to reach a host, which is adr-0007's standing rule.
ENJIN_TEST(LanDiscovery, ListeningIsNotRequiredToHost) {
    Pair net;
    net.Wire();
    ENJIN_EXPECT_TRUE(net.host.StartAnnouncing(7777, "Marty's game", "tornado", 4));
    ENJIN_EXPECT_TRUE(net.host.IsAnnouncing());
    // Nobody is listening, and announcing neither fails nor blocks.
    net.host.Update(1.0f);
    ENJIN_EXPECT_TRUE(net.host.IsAnnouncing());
}

// ============================================================================
// INTEGRATION: a host announces itself, and discovery never becomes required.
// ============================================================================

ENJIN_TEST(LanDiscoveryIntegration, HostingAnnouncesWithoutBeingAsked) {
    LoopbackBus bus;
    NetworkSystem host;
    host.SetTransport(std::make_unique<LoopbackTransport>(&bus));
    host.SetDiscoveryTransportForTest(std::make_unique<LoopbackTransport>(&bus));
    host.SetDiscoveryGameId("tornado");
    host.SetEnabled(true);

    LanDiscovery browser;
    browser.SetTransport(std::make_unique<LoopbackTransport>(&bus));
    ENJIN_ASSERT_TRUE(browser.StartListening("tornado"));

    ENJIN_ASSERT_TRUE(host.HostGame(7787, "Marty"));
    for (int i = 0; i < 4; i++) { host.Update(1.0f / 60.0f); browser.Update(1.0f / 60.0f); }

    // Nobody called StartAnnouncing: hosting did it.
    ENJIN_ASSERT_TRUE(browser.GetSessions().size() == 1);
    ENJIN_EXPECT_TRUE(browser.GetSessions()[0].sessionName == "Marty");
    ENJIN_EXPECT_EQ(browser.GetSessions()[0].address.port, static_cast<u16>(7787));
}

ENJIN_TEST(LanDiscoveryIntegration, APrivateGameCanRefuseToAnnounce) {
    LoopbackBus bus;
    NetworkSystem host;
    host.SetTransport(std::make_unique<LoopbackTransport>(&bus));
    host.SetDiscoveryTransportForTest(std::make_unique<LoopbackTransport>(&bus));
    host.SetDiscoveryGameId("tornado");
    host.SetAnnounceOnLan(false);
    host.SetEnabled(true);

    LanDiscovery browser;
    browser.SetTransport(std::make_unique<LoopbackTransport>(&bus));
    browser.StartListening("tornado");

    ENJIN_ASSERT_TRUE(host.HostGame(7788, "Marty"));
    for (int i = 0; i < 8; i++) { host.Update(1.0f / 60.0f); browser.Update(1.0f / 60.0f); }

    // Hosting still worked; it is simply not advertised.
    ENJIN_EXPECT_TRUE(host.IsHost());
    ENJIN_EXPECT_EQ(browser.GetSessions().size(), static_cast<usize>(0));
}

ENJIN_TEST(LanDiscoveryIntegration, DisconnectStopsAdvertising) {
    LoopbackBus bus;
    NetworkSystem host;
    host.SetTransport(std::make_unique<LoopbackTransport>(&bus));
    host.SetDiscoveryTransportForTest(std::make_unique<LoopbackTransport>(&bus));
    host.SetDiscoveryGameId("tornado");
    host.SetEnabled(true);

    LanDiscovery browser;
    browser.SetTransport(std::make_unique<LoopbackTransport>(&bus));
    browser.StartListening("tornado");

    host.HostGame(7789, "Marty");
    for (int i = 0; i < 4; i++) { host.Update(1.0f / 60.0f); browser.Update(1.0f / 60.0f); }
    ENJIN_ASSERT_TRUE(browser.GetSessions().size() == 1);

    // A game that ends must stop advertising, or it lingers in every browser
    // on the subnet until the timeout expires it.
    host.Disconnect();
    for (int i = 0; i < 10; i++) browser.Update(0.5f);
    ENJIN_EXPECT_EQ(browser.GetSessions().size(), static_cast<usize>(0));
}

ENJIN_TEST_MAIN()
