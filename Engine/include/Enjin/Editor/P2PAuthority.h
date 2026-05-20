#pragma once

#include "Enjin/Editor/ICollabAuthority.h"
#include <vector>
#include <functional>

namespace Enjin {
namespace Networking { class NetworkSystem; }
namespace Editor {

// ============================================================================
// P2PAuthority — peer-to-peer authority model (current default)
// ============================================================================
// The host is the authority. Operations are broadcast directly to all peers.
// CRDT merge rules determine which values win for concurrent edits.
// This is the production implementation for small indie teams (2-8 users).

class ENJIN_API P2PAuthority : public ICollabAuthority {
public:
    P2PAuthority() = default;
    ~P2PAuthority() override = default;

    void SetNetwork(Networking::NetworkSystem* network) { m_Network = network; }
    void SetIsHost(bool isHost) { m_IsHost = isHost; }

    // Callback for serializing an operation to binary for the wire
    using SerializeFunc = std::function<std::vector<u8>(const EditOperation&)>;
    void SetSerializer(SerializeFunc fn) { m_Serialize = std::move(fn); }

    // ICollabAuthority interface
    void SubmitOperation(const EditOperation& op) override;
    std::vector<EditOperation> ProcessIncoming() override;
    bool ShouldApply(const EditOperation& op) const override;
    u64 GetTotalOrder(const EditOperation& op) const override;
    bool IsServerAuthority() const override { return false; }

private:
    Networking::NetworkSystem* m_Network = nullptr;
    bool m_IsHost = false;
    SerializeFunc m_Serialize;
};

} // namespace Editor
} // namespace Enjin
