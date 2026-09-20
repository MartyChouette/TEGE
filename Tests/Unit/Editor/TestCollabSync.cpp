#include "EnjinTest.h"
#include "LoopbackTransport.h"
#include "Enjin/Editor/CollaborativeEditing.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"
#include <memory>
#include <string>

using namespace Enjin;
using namespace Enjin::Editor;

// ============================================================================
// COLLABORATIVE SCENE SYNC, HEADLESS
//
// The whole collaboration chain above the socket, with no editor and no screen:
// two Worlds, two NetworkSystems on a loopback bus, two CollaborativeEditing
// systems, and the same SceneSerializer callbacks EditorLayer installs.
//
// This is the thing that could not be asserted before. The feature's own triage
// recorded that a live two-instance test "CANNOT PASS", and every fix in the
// chain was judged by reading two editors' logs side by side. A joining client
// reaching Connected with the host's scene in its World is the actual claim,
// and it is checkable in a second.
// ============================================================================

namespace {

using EnjinTestNet::LoopbackBus;
using EnjinTestNet::LoopbackTransport;

// A host and a client, each with the world/network/collab stack the editor
// builds, wired with the same three callbacks EditorLayer installs.
struct CollabPair {
    LoopbackBus bus;

    ECS::World hostWorld;
    ECS::World clientWorld;
    Networking::NetworkSystem hostNet;
    Networking::NetworkSystem clientNet;
    CollaborativeEditingSystem hostCollab;
    CollaborativeEditingSystem clientCollab;

    int clientSyncsReceived = 0;

    static constexpr u16 kPort = 7799;

    void WireCallbacks() {
        // Host: serialize the whole scene for a joining peer.
        hostCollab.SetOnSceneSyncRequest([this]() -> std::string {
            Scene::SceneSerializer serializer(&hostWorld);
            return serializer.SaveToString();
        });
        // Client: load what arrives.
        clientCollab.SetOnSceneSyncReceived([this](const std::string& json) {
            clientSyncsReceived++;
            Scene::SceneSerializer serializer(&clientWorld);
            serializer.LoadFromString(json, true);
        });
    }

    bool Start() {
        hostNet.SetTransport(std::make_unique<LoopbackTransport>(&bus));
        clientNet.SetTransport(std::make_unique<LoopbackTransport>(&bus));
        hostNet.SetWorld(&hostWorld);
        clientNet.SetWorld(&clientWorld);
        // The editor enables this itself for edit-mode collaboration; PlayMode's
        // play-start path is the only other SetEnabled(true) in the engine.
        hostNet.SetEnabled(true);
        clientNet.SetEnabled(true);

        hostCollab.Initialize(&hostWorld, &hostNet);
        clientCollab.Initialize(&clientWorld, &clientNet);
        WireCallbacks();

        if (!hostCollab.HostSession(kPort, "host")) return false;
        if (!clientCollab.JoinSession("127.0.0.1", kPort, "client")) return false;
        return true;
    }

    void Pump(int frames, f32 dt = 1.0f / 60.0f) {
        for (int i = 0; i < frames; i++) {
            hostNet.Update(dt);
            hostCollab.Update(dt);
            clientNet.Update(dt);
            clientCollab.Update(dt);
        }
    }

    // Run until the client has applied a sync, or give up.
    int PumpUntilSynced(int maxFrames = 600) {
        int frames = 0;
        while (clientSyncsReceived == 0 && frames < maxFrames) {
            Pump(1);
            frames++;
        }
        return frames;
    }
};

usize CountNamed(ECS::World& world, const std::string& name) {
    usize found = 0;
    for (auto e : world.GetAllEntities()) {
        if (auto* n = world.GetComponent<ECS::NameComponent>(e)) {
            if (n->name == name) found++;
        }
    }
    return found;
}

} // namespace

// ============================================================================

ENJIN_TEST(CollabSync, ClientReceivesTheHostsScene) {
    // Arrange: a host with a scene the client does not have.
    CollabPair net;
    ENJIN_ASSERT_TRUE(net.Start());

    for (int i = 0; i < 3; i++) {
        auto e = net.hostWorld.CreateEntity();
        net.hostWorld.AddComponent<ECS::TransformComponent>(e);
        net.hostWorld.SetEntityName(e, "HostEntity" + std::to_string(i));
    }
    ENJIN_ASSERT_TRUE(net.clientWorld.GetEntityCount() == 0);

    // Act
    const int frames = net.PumpUntilSynced();

    // Assert: the sync arrived and the host's entities are in the client's world.
    ENJIN_EXPECT_EQ(net.clientSyncsReceived, 1);
    ENJIN_EXPECT_TRUE(frames < 600);
    ENJIN_EXPECT_EQ(CountNamed(net.clientWorld, "HostEntity0"), static_cast<usize>(1));
    ENJIN_EXPECT_EQ(CountNamed(net.clientWorld, "HostEntity1"), static_cast<usize>(1));
    ENJIN_EXPECT_EQ(CountNamed(net.clientWorld, "HostEntity2"), static_cast<usize>(1));
}

// The client sat in Syncing forever, which is what "connects but never
// completes" looked like from the outside.
ENJIN_TEST(CollabSync, ClientLeavesSyncingAndBecomesActive) {
    CollabPair net;
    ENJIN_ASSERT_TRUE(net.Start());
    auto e = net.hostWorld.CreateEntity();
    net.hostWorld.AddComponent<ECS::TransformComponent>(e);
    net.hostWorld.SetEntityName(e, "Only");

    net.PumpUntilSynced();
    net.Pump(4);

    ENJIN_EXPECT_TRUE(net.clientCollab.GetState() == CollabSessionState::Connected);
    ENJIN_EXPECT_TRUE(net.clientCollab.IsActive());
    // The id the host assigned during the handshake; 0 is the host itself.
    ENJIN_EXPECT_EQ(net.clientNet.GetLocalPlayerId(), static_cast<Networking::PlayerId>(1));
}

// The host stayed at peers=1 -- itself -- because HandleSyncRequest, which is
// what registers a joining peer, was never reached.
ENJIN_TEST(CollabSync, HostRegistersTheJoiningPeer) {
    CollabPair net;
    ENJIN_ASSERT_TRUE(net.Start());
    auto e = net.hostWorld.CreateEntity();
    net.hostWorld.AddComponent<ECS::TransformComponent>(e);

    ENJIN_EXPECT_EQ(net.hostCollab.GetPeers().size(), static_cast<usize>(1));  // itself
    net.PumpUntilSynced();
    net.Pump(4);

    ENJIN_EXPECT_EQ(net.hostCollab.GetPeers().size(), static_cast<usize>(2));
}

// A scene big enough to need fragmenting is the ordinary case, not the edge
// one: this is the size at which the sync used to vanish without an error.
ENJIN_TEST(CollabSync, SceneLargerThanOneDatagramArrives) {
    CollabPair net;
    ENJIN_ASSERT_TRUE(net.Start());

    // 400 named entities serialize to far more than MAX_PACKET_SIZE.
    for (int i = 0; i < 400; i++) {
        auto e = net.hostWorld.CreateEntity();
        net.hostWorld.AddComponent<ECS::TransformComponent>(e);
        net.hostWorld.SetEntityName(e, "Bulk" + std::to_string(i));
    }
    Scene::SceneSerializer probe(&net.hostWorld);
    ENJIN_ASSERT_TRUE(probe.SaveToString().size() > Networking::MAX_PACKET_SIZE);

    net.PumpUntilSynced(1800);

    ENJIN_EXPECT_EQ(net.clientSyncsReceived, 1);
    ENJIN_EXPECT_EQ(CountNamed(net.clientWorld, "Bulk0"), static_cast<usize>(1));
    ENJIN_EXPECT_EQ(CountNamed(net.clientWorld, "Bulk399"), static_cast<usize>(1));
}

ENJIN_TEST_MAIN()
