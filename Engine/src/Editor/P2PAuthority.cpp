#include "Enjin/Editor/P2PAuthority.h"
#include "Enjin/Editor/CollaborativeEditing.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Editor {

static const char* RPC_EDIT_OP_P2P = "collab_edit_op";

void P2PAuthority::SubmitOperation(const EditOperation& op) {
    if (!m_Network || !m_Network->IsConnected()) return;
    if (!m_Serialize) return;

    auto data = m_Serialize(op);
    m_Network->CallRPCAll(RPC_EDIT_OP_P2P, data.data(), static_cast<u32>(data.size()));
}

std::vector<EditOperation> P2PAuthority::ProcessIncoming() {
    // In P2P mode, incoming ops are handled via RPC callbacks registered
    // in CollaborativeEditingSystem::Initialize, not polled here.
    // This method exists for the ServerAuthority path where the server
    // batches and reorders ops before delivering them.
    return {};
}

bool P2PAuthority::ShouldApply(const EditOperation& /*op*/) const {
    // In P2P mode, the CRDT document decides — always return true here,
    // the CRDTDocument::ApplyRemoteOp handles the actual merge logic.
    return true;
}

u64 P2PAuthority::GetTotalOrder(const EditOperation& op) const {
    // P2P has no total ordering — use the vector clock max as a best-effort
    // scalar for display/log sorting purposes.
    return op.vclock.MaxComponent();
}

} // namespace Editor
} // namespace Enjin
