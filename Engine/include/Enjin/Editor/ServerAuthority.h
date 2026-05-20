#pragma once

#include "Enjin/Editor/ICollabAuthority.h"

namespace Enjin {
namespace Editor {

// ============================================================================
// ServerAuthority — server-relay authority model (stub)
// ============================================================================
// All operations are sent to a central relay server which assigns total
// ordering and broadcasts to all peers. This ensures perfect consistency
// but requires infrastructure.
//
// NOT YET IMPLEMENTED — this is an interface placeholder. All methods log
// a warning and fall back to P2P-like behavior.

class ENJIN_API ServerAuthority : public ICollabAuthority {
public:
    ServerAuthority() = default;
    ~ServerAuthority() override = default;

    void SubmitOperation(const EditOperation& op) override;
    std::vector<EditOperation> ProcessIncoming() override;
    bool ShouldApply(const EditOperation& op) const override;
    u64 GetTotalOrder(const EditOperation& op) const override;
    bool IsServerAuthority() const override { return true; }
};

} // namespace Editor
} // namespace Enjin
