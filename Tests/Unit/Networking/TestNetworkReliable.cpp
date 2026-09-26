#include "EnjinTest.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/Networking/INetworkTransport.h"
#include "Enjin/Networking/NetworkTypes.h"
#include "LoopbackTransport.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <cstring>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

using namespace Enjin;
using namespace Enjin::Networking;

namespace {

using EnjinTestNet::LoopbackBus;
using EnjinTestNet::LoopbackTransport;

// Host and client on one bus, pumped together.
struct Pair {
    LoopbackBus bus;
    NetworkSystem host;
    NetworkSystem client;
    static constexpr u16 kHostPort = 7787;

    bool Connect() {
        host.SetTransport(std::make_unique<LoopbackTransport>(&bus));
        client.SetTransport(std::make_unique<LoopbackTransport>(&bus));
        host.SetEnabled(true);
        client.SetEnabled(true);
        if (!host.HostGame(kHostPort, "host")) return false;
        if (!client.JoinGame("127.0.0.1", kHostPort, "client")) return false;
        Pump(8);
        return client.IsConnected();
    }

    void Pump(int frames, f32 dt = 1.0f / 60.0f) {
        for (int i = 0; i < frames; i++) {
            host.Update(dt);
            client.Update(dt);
        }
    }
};

} // namespace

// ============================================================================
// THE HANDSHAKE
// ============================================================================

ENJIN_TEST(NetworkHandshake, ClientReachesConnected) {
    Pair net;
    ENJIN_ASSERT_TRUE(net.Connect());
    ENJIN_EXPECT_TRUE(net.client.IsConnected());
    // The host assigns 1 to the first client; 0 is the host itself.
    ENJIN_EXPECT_EQ(net.client.GetLocalPlayerId(), static_cast<PlayerId>(1));
    // Counts self plus connections, so a host with one client reports 2.
    ENJIN_EXPECT_EQ(net.host.GetConnectedPlayerCount(), 2u);
}

// The whole handshake is exempt from HMAC authentication, because a client has
// no session key while it is in flight. Authenticating the accept made every
// client drop it as a payload-size mismatch exactly 36 bytes over.
ENJIN_TEST(NetworkHandshake, CompletesWithAuthenticationEnabled) {
    Pair net;
    net.host.SetAuthenticationEnabled(true);
    net.client.SetAuthenticationEnabled(true);
    ENJIN_ASSERT_TRUE(net.Connect());
    ENJIN_EXPECT_TRUE(net.client.IsConnected());
}

// The session key goes out as one plain datagram, followed at once by signed
// join-time state. SD-10: a LOST key banned both sides within seconds (every
// signed host packet was a violation on the client, every unsigned client
// heartbeat one on the host), and a LATE key made the client drop the entity
// spawns sent ahead of it, which nothing resent. The host now resends the key
// until the client signs a packet with it, neither side counts the other's
// packets from that window against it, and the entities go out after the key
// is confirmed.
namespace {

// A host world holding one networked entity, registered before the client
// joins, and an empty client world to receive it.
struct Worlds {
    ECS::World hostWorld;
    ECS::World clientWorld;
    NetworkId netId = 0;

    void Attach(Pair& net) {
        net.host.SetWorld(&hostWorld);
        net.client.SetWorld(&clientWorld);
    }
    void SpawnOnHost(Pair& net) {
        const ECS::Entity e = hostWorld.CreateEntity();
        ECS::TransformComponent t;
        t.position = Math::Vector3(3.0f, 0.0f, 0.0f);
        hostWorld.AddComponent<ECS::TransformComponent>(e, t);
        hostWorld.AddComponent<ECS::NetworkIdentityComponent>(e);
        netId = net.host.RegisterNetworkEntity(e, 0);
    }
    bool ClientHasIt() {
        for (ECS::Entity e : clientWorld.GetEntitiesWithComponent<ECS::NetworkIdentityComponent>()) {
            const auto* n = clientWorld.GetComponent<ECS::NetworkIdentityComponent>(e);
            if (n && n->networkId == netId) return true;
        }
        return false;
    }
};

bool StartJoin(Pair& net) {
    net.host.SetTransport(std::make_unique<LoopbackTransport>(&net.bus));
    net.client.SetTransport(std::make_unique<LoopbackTransport>(&net.bus));
    net.host.SetEnabled(true);
    net.client.SetEnabled(true);
    net.host.SetAuthenticationEnabled(true);
    net.client.SetAuthenticationEnabled(true);
    return net.host.HostGame(Pair::kHostPort, "host");
}

} // namespace

ENJIN_TEST(NetworkHandshake, ALostSessionKeyIsResentAndTheSessionSurvives) {
    // Arrange: the host's first key datagram is lost.
    Worlds w;   // before net: the network systems hold pointers into it
    Pair net;
    w.Attach(net);
    ENJIN_ASSERT_TRUE(StartJoin(net));
    w.SpawnOnHost(net);
    net.bus.dropType = static_cast<u8>(MessageType::SessionKeyExchange);
    net.bus.dropTypeRemaining = 1;

    // Act: join, then run past the 10 s violation window and the timeout.
    ENJIN_ASSERT_TRUE(net.client.JoinGame("127.0.0.1", Pair::kHostPort, "client"));
    net.Pump(12 * 60);

    // Assert: the key was lost and resent, both sides are still talking, and
    // the entity the host already had reached the client.
    ENJIN_EXPECT_EQ(net.bus.dropTypeRemaining, 0u);
    ENJIN_EXPECT_TRUE(net.client.IsConnected());
    ENJIN_EXPECT_EQ(net.host.GetConnectedPlayerCount(), 2u);
    ENJIN_EXPECT_TRUE(w.ClientHasIt());
}

ENJIN_TEST(NetworkHandshake, ALateSessionKeyStillDeliversTheExistingEntities) {
    // Arrange: every key datagram is held back behind the rest of the join.
    Worlds w;   // before net: the network systems hold pointers into it
    Pair net;
    w.Attach(net);
    ENJIN_ASSERT_TRUE(StartJoin(net));
    w.SpawnOnHost(net);
    net.bus.holdType = static_cast<u8>(MessageType::SessionKeyExchange);

    // Act: let the accept, the lobby and the snapshots arrive keyless for
    // half a second, then deliver the key.
    ENJIN_ASSERT_TRUE(net.client.JoinGame("127.0.0.1", Pair::kHostPort, "client"));
    net.Pump(30);
    ENJIN_EXPECT_FALSE(net.bus.held.empty());
    net.bus.ReleaseHeld();
    net.Pump(12 * 60);

    // Assert
    ENJIN_EXPECT_TRUE(net.client.IsConnected());
    ENJIN_EXPECT_EQ(net.host.GetConnectedPlayerCount(), 2u);
    ENJIN_EXPECT_TRUE(w.ClientHasIt());
}

// ============================================================================
// OWNERSHIP AND THE LOBBY
// ============================================================================

namespace {

// A host and two clients on one bus, each with its own world, joined in turn.
// `withC` adds a third client, for what a bystander sees.
struct Trio {
    ECS::World hostWorld, worldA, worldB, worldC;   // declared first: the systems point into them
    LoopbackBus bus;
    NetworkSystem host, a, b, c;

    bool Join(bool withC = false) {
        host.SetTransport(std::make_unique<LoopbackTransport>(&bus));
        a.SetTransport(std::make_unique<LoopbackTransport>(&bus));
        b.SetTransport(std::make_unique<LoopbackTransport>(&bus));
        host.SetWorld(&hostWorld);
        a.SetWorld(&worldA);
        b.SetWorld(&worldB);
        for (NetworkSystem* n : {&host, &a, &b}) n->SetEnabled(true);
        if (!host.HostGame(Pair::kHostPort, "host")) return false;
        if (!a.JoinGame("127.0.0.1", Pair::kHostPort, "a")) return false;
        Pump(10);
        if (!b.JoinGame("127.0.0.1", Pair::kHostPort, "b")) return false;
        Pump(10);
        if (withC) {
            c.SetTransport(std::make_unique<LoopbackTransport>(&bus));
            c.SetWorld(&worldC);
            c.SetEnabled(true);
            if (!c.JoinGame("127.0.0.1", Pair::kHostPort, "c")) return false;
            Pump(10);
            if (!c.IsConnected()) return false;
        }
        return a.IsConnected() && b.IsConnected();
    }
    void Pump(int frames) {
        for (int i = 0; i < frames; ++i) {
            host.Update(1.0f / 60.0f);
            a.Update(1.0f / 60.0f);
            b.Update(1.0f / 60.0f);
            c.Update(1.0f / 60.0f);   // returns at once when c never joined
        }
    }
    // A host entity owned by `owner`. Registering broadcasts the spawn.
    NetworkId SpawnOwnedBy(PlayerId owner) {
        const ECS::Entity e = hostWorld.CreateEntity();
        hostWorld.AddComponent<ECS::TransformComponent>(e);
        hostWorld.AddComponent<ECS::NetworkIdentityComponent>(e);
        const NetworkId id = host.RegisterNetworkEntity(e, owner);
        Pump(10);
        return id;
    }
    static ECS::Entity Find(ECS::World& w, NetworkId id) {
        for (ECS::Entity x : w.GetEntitiesWithComponent<ECS::NetworkIdentityComponent>()) {
            const auto* n = w.GetComponent<ECS::NetworkIdentityComponent>(x);
            if (n && n->networkId == id) return x;
        }
        return ECS::INVALID_ENTITY;
    }
    static const ECS::NetworkIdentityComponent* Identity(ECS::World& w, NetworkId id) {
        const ECS::Entity e = Find(w, id);
        return e == ECS::INVALID_ENTITY ? nullptr : w.GetComponent<ECS::NetworkIdentityComponent>(e);
    }
};

} // namespace

// SD-3: the host sent OwnershipRevoke, PlayerJoined and PlayerLeft and no
// client had a case for any of them. A revoked owner kept isLocallyOwned and
// went on driving an entity it no longer owned.
ENJIN_TEST(NetworkOwnership, TheOldOwnerIsToldWhenOwnershipMoves) {
    // Arrange: the host's entity belongs to client A.
    Trio net;
    ENJIN_ASSERT_TRUE(net.Join());
    const NetworkId id = net.SpawnOwnedBy(net.a.GetLocalPlayerId());
    ENJIN_ASSERT_NOT_NULL(Trio::Identity(net.worldA, id));
    ENJIN_ASSERT_TRUE(Trio::Identity(net.worldA, id)->isLocallyOwned);

    // Act: client B asks for it.
    net.b.RequestOwnership(id);
    net.Pump(10);

    // Assert: B owns it, and A has been told it does not.
    ENJIN_ASSERT_NOT_NULL(Trio::Identity(net.worldB, id));
    ENJIN_EXPECT_TRUE(Trio::Identity(net.worldB, id)->isLocallyOwned);
    ENJIN_EXPECT_FALSE(Trio::Identity(net.worldA, id)->isLocallyOwned);
    ENJIN_EXPECT_EQ(Trio::Identity(net.worldA, id)->ownerId, net.b.GetLocalPlayerId());
}

ENJIN_TEST(NetworkOwnership, ALeavingPlayerLeavesTheLobbyAtOnce) {
    // Arrange
    Trio net;
    ENJIN_ASSERT_TRUE(net.Join());
    ENJIN_ASSERT_EQ(net.a.GetLobbyPlayers().size(), static_cast<usize>(3));

    // Act: B leaves. Every LobbyState that follows is lost, so only
    // PlayerLeft can tell A.
    const PlayerId bId = net.b.GetLocalPlayerId();
    net.bus.dropType = static_cast<u8>(MessageType::LobbyState);
    net.bus.dropTypeRemaining = 1000;
    net.b.Disconnect();
    net.Pump(10);

    // Assert
    bool stillListed = false;
    for (const auto& lp : net.a.GetLobbyPlayers()) stillListed |= (lp.id == bId);
    ENJIN_EXPECT_FALSE(stillListed);
}

// SD-12b: the grant went to the new owner and the revoke to the old one, and
// nobody else heard, so a bystander kept the previous ownerId.
ENJIN_TEST(NetworkOwnership, ABystanderLearnsTheNewOwner) {
    // Arrange: three clients; A owns the entity.
    Trio net;
    ENJIN_ASSERT_TRUE(net.Join(/*withC=*/true));
    const NetworkId id = net.SpawnOwnedBy(net.a.GetLocalPlayerId());
    ENJIN_ASSERT_NOT_NULL(Trio::Identity(net.worldC, id));

    // Act: B takes it.
    net.b.RequestOwnership(id);
    net.Pump(10);

    // Assert: C, which neither gave nor took it, knows B owns it now.
    ENJIN_EXPECT_EQ(Trio::Identity(net.worldC, id)->ownerId, net.b.GetLocalPlayerId());
    ENJIN_EXPECT_FALSE(Trio::Identity(net.worldC, id)->isLocallyOwned);
}

// SD-12: a client's snapshots of an entity it owns went to the host, which
// applied them and passed them on to nobody, and a client takes snapshots only
// from the host. With three players, what one client owned stood still on the
// third.
ENJIN_TEST(NetworkOwnership, AClientOwnedEntityMovesOnTheOtherClient) {
    // Arrange: client A owns an entity that every world has.
    Trio net;
    ENJIN_ASSERT_TRUE(net.Join());
    const NetworkId id = net.SpawnOwnedBy(net.a.GetLocalPlayerId());
    const ECS::Entity onA = Trio::Find(net.worldA, id);
    const ECS::Entity onB = Trio::Find(net.worldB, id);
    ENJIN_ASSERT_TRUE(onA != ECS::INVALID_ENTITY && onB != ECS::INVALID_ENTITY);

    // Act: A moves it and keeps it there for a second.
    net.worldA.GetComponent<ECS::TransformComponent>(onA)->position = Math::Vector3(5.0f, 0.0f, 0.0f);
    net.Pump(60);

    // Assert: B sees it arrive (interpolation delay is well under a second).
    const auto* seen = net.worldB.GetComponent<ECS::TransformComponent>(onB);
    ENJIN_ASSERT_NOT_NULL(seen);
    ENJIN_EXPECT_FLOAT_NEAR(seen->position.x, 5.0f, 0.05f);
    // And B knows who owns it.
    ENJIN_EXPECT_EQ(Trio::Identity(net.worldB, id)->ownerId, net.a.GetLocalPlayerId());
}

// ============================================================================
// THE RELIABLE CHANNEL
// ============================================================================

// SendReliable wraps its message in a ReliableMessage packet. Until 2026-09-19
// the receive switch had no case for that type, so every reliable message fell
// into `default:` and was discarded -- while the ack rode back on the next
// header and told the sender it had arrived.
ENJIN_TEST(NetworkReliable, ReliableRPCReachesTheHost) {
    Pair net;

    int calls = 0;
    u8 received = 0;
    net.host.RegisterRPC("test_reliable",
        [&](PlayerId sender, const u8* data, u32 size) {
            calls++;
            if (size >= 1) received = data[0];
            ENJIN_EXPECT_EQ(sender, static_cast<PlayerId>(1));
        }, true);
    // The CALLER's registry is what decides reliability, so the client has to
    // know the name too. A caller needs no handler, only the declaration.
    net.client.RegisterRPC("test_reliable", [](PlayerId, const u8*, u32) {}, true);

    ENJIN_ASSERT_TRUE(net.Connect());

    u8 payload = 42;
    net.client.CallRPC("test_reliable", 0, &payload, 1);
    net.Pump(4);

    ENJIN_EXPECT_EQ(calls, 1);
    ENJIN_EXPECT_EQ(received, static_cast<u8>(42));
    // And it arrived through the ReliableMessage wrapper, not by quietly
    // falling back to an unreliable RPCCall. Without that assertion this test
    // passes either way, and the unreliable path was never the broken one.
    ENJIN_EXPECT_EQ(net.bus.reliableSent, 1u);
}

ENJIN_TEST(NetworkReliable, UnreliableRPCStillReachesTheHost) {
    Pair net;

    int calls = 0;
    net.host.RegisterRPC("test_unreliable",
        [&](PlayerId, const u8*, u32) { calls++; }, false);
    net.client.RegisterRPC("test_unreliable", [](PlayerId, const u8*, u32) {}, false);

    ENJIN_ASSERT_TRUE(net.Connect());

    u8 payload = 1;
    net.client.CallRPC("test_unreliable", 0, &payload, 1);
    net.Pump(4);

    ENJIN_EXPECT_EQ(calls, 1);
}

// A lost datagram must be retransmitted until it lands.
ENJIN_TEST(NetworkReliable, RetransmitsUntilDelivered) {
    Pair net;

    int calls = 0;
    net.host.RegisterRPC("test_retry",
        [&](PlayerId, const u8*, u32) { calls++; }, true);
    net.client.RegisterRPC("test_retry", [](PlayerId, const u8*, u32) {}, true);

    ENJIN_ASSERT_TRUE(net.Connect());

    net.bus.dropsRemaining = 1;   // Swallow the first attempt
    u8 payload = 7;
    net.client.CallRPC("test_retry", 0, &payload, 1);
    net.Pump(2);
    ENJIN_EXPECT_EQ(calls, 0);    // It was dropped, so nothing yet

    // RELIABLE_RETRY_INTERVAL is 0.2s; one frame of that length is enough.
    net.Pump(3, 0.25f);
    ENJIN_EXPECT_EQ(calls, 1);
    ENJIN_EXPECT_EQ(net.bus.reliableDropped, 1u);   // exactly one attempt was lost
    ENJIN_EXPECT_TRUE(net.bus.reliableSent >= 2u);  // the original plus at least one retry
}

// And a retransmit that follows a LOST ACK must not be applied twice. The outer
// sequence is rewritten on every retransmit and the auth sequence with it, so
// the message id is the only field that can tell the two apart.
ENJIN_TEST(NetworkReliable, DeliversOnceWhenTheAckIsLost) {
    Pair net;

    int calls = 0;
    net.host.RegisterRPC("test_once",
        [&](PlayerId, const u8*, u32) { calls++; }, true);
    net.client.RegisterRPC("test_once", [](PlayerId, const u8*, u32) {}, true);

    ENJIN_ASSERT_TRUE(net.Connect());

    u8 payload = 3;
    net.client.CallRPC("test_once", 0, &payload, 1);

    // Deliver the message, then swallow everything the host sends back for a
    // while, which includes the packet that would have carried the ack.
    net.host.Update(1.0f / 60.0f);
    ENJIN_EXPECT_EQ(calls, 1);

    net.bus.dropsRemaining = 64;
    net.Pump(6, 0.25f);           // Several retry intervals, all acks lost
    net.bus.dropsRemaining = 0;
    net.Pump(6, 0.25f);           // Retries now land

    ENJIN_EXPECT_EQ(calls, 1);
}

// ============================================================================
// FRAGMENTATION
//
// Nothing below the reliable layer fragments, and the receive buffer is
// MAX_PACKET_SIZE. An oversized datagram is DISCARDED by recvfrom rather than
// truncated, so before this a scene sync of any real size simply vanished.
// ============================================================================

namespace {
std::vector<u8> MakePattern(usize bytes) {
    std::vector<u8> v(bytes);
    for (usize i = 0; i < bytes; i++) v[i] = static_cast<u8>((i * 31 + 7) & 0xFF);
    return v;
}
}

ENJIN_TEST(NetworkFragment, PayloadLargerThanADatagramArrivesWhole) {
    Pair net;

    // Arrange: a payload several datagrams long, and a receiver that keeps it.
    const std::vector<u8> sent = MakePattern(RELIABLE_CHUNK_PAYLOAD * 5 + 137);
    std::vector<u8> got;
    int calls = 0;
    net.host.RegisterRPC("test_big",
        [&](PlayerId, const u8* data, u32 size) {
            calls++;
            got.assign(data, data + size);
        }, true);
    net.client.RegisterRPC("test_big", [](PlayerId, const u8*, u32) {}, true);
    ENJIN_ASSERT_TRUE(net.Connect());

    // Act
    net.client.CallRPC("test_big", 0, sent.data(), static_cast<u32>(sent.size()));
    net.Pump(6);

    // Assert: delivered once, byte for byte, and it really did cross in pieces.
    ENJIN_EXPECT_EQ(calls, 1);
    ENJIN_ASSERT_TRUE(got.size() == sent.size());
    ENJIN_EXPECT_TRUE(got == sent);
    ENJIN_EXPECT_TRUE(net.bus.reliableSent >= 6u);
}

// A scene sync is routinely over 64 KB, which the old u16 RPC size field could
// not express at all.
ENJIN_TEST(NetworkFragment, PayloadLargerThan64KArrivesWhole) {
    Pair net;

    const std::vector<u8> sent = MakePattern(200 * 1024);
    std::vector<u8> got;
    net.host.RegisterRPC("test_scene",
        [&](PlayerId, const u8* data, u32 size) { got.assign(data, data + size); }, true);
    net.client.RegisterRPC("test_scene", [](PlayerId, const u8*, u32) {}, true);
    ENJIN_ASSERT_TRUE(net.Connect());

    net.client.CallRPC("test_scene", 0, sent.data(), static_cast<u32>(sent.size()));

    // It cannot arrive in one frame and should not: the send budget mirrors the
    // receiver's rate limit, so a large payload is spread over several frames
    // however fast they go. How MANY frames is a function of the configured
    // rate, so this asserts that it was paced, not how slow the pacing is --
    // the first version of this pinned "more than 60 frames" and broke the day
    // the default rate went up, which is a test measuring a config value.
    int frames = 0;
    while (got.size() != sent.size() && frames < 900) { net.Pump(1); frames++; }

    ENJIN_ASSERT_TRUE(got.size() == sent.size());
    ENJIN_EXPECT_TRUE(got == sent);
    ENJIN_EXPECT_TRUE(frames > 1);
}

// Out-of-order and duplicated chunks are both ordinary on UDP.
ENJIN_TEST(NetworkFragment, SurvivesLostChunksAndRetransmits) {
    Pair net;

    const std::vector<u8> sent = MakePattern(RELIABLE_CHUNK_PAYLOAD * 4);
    std::vector<u8> got;
    int calls = 0;
    net.host.RegisterRPC("test_lossy",
        [&](PlayerId, const u8* data, u32 size) { calls++; got.assign(data, data + size); }, true);
    net.client.RegisterRPC("test_lossy", [](PlayerId, const u8*, u32) {}, true);
    ENJIN_ASSERT_TRUE(net.Connect());

    net.bus.dropsRemaining = 2;   // Lose the first two chunks
    net.client.CallRPC("test_lossy", 0, sent.data(), static_cast<u32>(sent.size()));
    net.Pump(10, 0.25f);          // Long enough for the retries to land

    ENJIN_EXPECT_EQ(calls, 1);    // Reassembled exactly once
    ENJIN_ASSERT_TRUE(got.size() == sent.size());
    ENJIN_EXPECT_TRUE(got == sent);
}

// Diagnostic sweep: find the size at which reassembly stops completing.
ENJIN_TEST(NetworkFragmentSweep, CompletesAtEachSize) {
    const usize sizes[] = { 2, 8, 20, 40, 60, 90, 120, 171 };
    for (usize fragments : sizes) {
        Pair net;
        const std::vector<u8> sent = MakePattern(RELIABLE_CHUNK_PAYLOAD * fragments - 1);
        usize gotSize = 0;
        net.host.RegisterRPC("sweep",
            [&](PlayerId, const u8*, u32 size) { gotSize = size; }, true);
        net.client.RegisterRPC("sweep", [](PlayerId, const u8*, u32) {}, true);
        ENJIN_ASSERT_TRUE(net.Connect());

        net.client.CallRPC("sweep", 0, sent.data(), static_cast<u32>(sent.size()));
        int frames = 0;
        while (gotSize != sent.size() && frames < 1200) { net.Pump(1); frames++; }
        ENJIN_EXPECT_EQ(gotSize, sent.size());
    }
}

ENJIN_TEST_MAIN()
