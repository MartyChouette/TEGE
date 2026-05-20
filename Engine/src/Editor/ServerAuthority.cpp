#include "Enjin/Editor/ServerAuthority.h"
#include "Enjin/Editor/CollaborativeEditing.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Editor {

void ServerAuthority::SubmitOperation(const EditOperation& /*op*/) {
    ENJIN_LOG_WARN(Editor, "ServerAuthority: Not implemented — requires a relay server. "
        "Use P2PAuthority for local/LAN collaboration.");
}

std::vector<EditOperation> ServerAuthority::ProcessIncoming() {
    // No server to poll — return empty
    return {};
}

bool ServerAuthority::ShouldApply(const EditOperation& /*op*/) const {
    // Stub: always allow (mirrors P2P behavior)
    return true;
}

u64 ServerAuthority::GetTotalOrder(const EditOperation& op) const {
    // No server-assigned ordering — fall back to vector clock max
    return op.vclock.MaxComponent();
}

} // namespace Editor
} // namespace Enjin
