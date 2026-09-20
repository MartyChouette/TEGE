#pragma once

// ============================================================================
// LOOPBACK TRANSPORT
//
// Two NetworkSystems in one process, no sockets. This is what makes the
// handshake, the reliable channel and a collaboration session testable at all:
// the alternative is launching two editors and reading their logs, which is
// slow, needs a screen, and cannot assert anything.
//
// Ports are the only routing key. Every address uses the loopback IP, which is
// also what a real client connecting to 127.0.0.1 does.
// ============================================================================

#include "Enjin/Networking/INetworkTransport.h"
#include "Enjin/Networking/NetworkTypes.h"
#include <cstring>
#include <deque>
#include <unordered_map>
#include <vector>

namespace EnjinTestNet {

using namespace Enjin;
using namespace Enjin::Networking;

struct Datagram {
    NetworkAddress from;
    std::vector<u8> bytes;
};

// One shared switchboard. Ports are the only routing key; every address uses
// the same loopback IP, which is also what the real client does.
struct LoopbackBus {
    std::unordered_map<u16, std::deque<Datagram>> inboxes;
    u16 nextEphemeral = 40000;
    u32 dropsRemaining = 0;   // Swallow the next N datagrams, to force retransmits
    u32 reliableSent = 0;     // Datagrams whose header type is ReliableMessage
    u32 reliableDropped = 0;
};

class LoopbackTransport : public INetworkTransport {
public:
    explicit LoopbackTransport(LoopbackBus* bus) : m_Bus(bus) {}

    bool Bind(u16 port) override {
        m_Port = (port != 0) ? port : m_Bus->nextEphemeral++;
        m_Bus->inboxes[m_Port];
        m_Open = true;
        return true;
    }

    bool SendTo(const NetworkAddress& addr, const u8* data, u32 size) override {
        if (!m_Open) return false;
        const bool isReliable = (size > 0 && data[0] == static_cast<u8>(MessageType::ReliableMessage));
        if (isReliable) m_Bus->reliableSent++;
        if (m_Bus->dropsRemaining > 0) {
            m_Bus->dropsRemaining--;
            if (isReliable) m_Bus->reliableDropped++;
            return true;   // The sender cannot tell a swallowed datagram from a delivered one
        }
        NetworkAddress self;
        self.ip = NetworkAddress::ParseIP("127.0.0.1");
        self.port = m_Port;
        m_Bus->inboxes[addr.port].push_back({self, std::vector<u8>(data, data + size)});
        return true;
    }

    i32 ReceiveFrom(NetworkAddress& sender, u8* buffer, u32 bufferSize) override {
        if (!m_Open) return -1;
        auto& inbox = m_Bus->inboxes[m_Port];
        if (inbox.empty()) return 0;
        Datagram dg = std::move(inbox.front());
        inbox.pop_front();
        if (dg.bytes.size() > bufferSize) return 0;
        std::memcpy(buffer, dg.bytes.data(), dg.bytes.size());
        sender = dg.from;
        return static_cast<i32>(dg.bytes.size());
    }

    void Close() override { m_Open = false; }
    bool IsOpen() const override { return m_Open; }
    u16 GetBoundPort() const override { return m_Port; }

private:
    LoopbackBus* m_Bus = nullptr;
    u16 m_Port = 0;
    bool m_Open = false;
};

} // namespace EnjinTestNet
