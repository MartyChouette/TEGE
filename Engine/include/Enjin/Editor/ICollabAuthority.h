#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vector>

namespace Enjin {
namespace Editor {

struct EditOperation;

// ============================================================================
// ICollabAuthority — abstract authority model for collaborative editing
// ============================================================================
// Defines how operations are ordered and validated. Two implementations:
//   P2PAuthority  — host is authority, direct peer-to-peer (current default)
//   ServerAuthority — relay server assigns total ordering (future)
//
// The CollaborativeEditingSystem delegates operation submission and incoming
// processing through this interface, allowing the authority model to be
// swapped at runtime.

class ICollabAuthority {
public:
    virtual ~ICollabAuthority() = default;

    // Submit a local operation for ordering/validation/broadcast.
    // The authority decides how and when to send it.
    virtual void SubmitOperation(const EditOperation& op) = 0;

    // Process incoming network data and return ordered operations to apply.
    // Called each frame from Update().
    virtual std::vector<EditOperation> ProcessIncoming() = 0;

    // Check if an operation should be applied to the local world.
    // The authority may reject ops that fail ordering or permission checks.
    virtual bool ShouldApply(const EditOperation& op) const = 0;

    // Get the total ordering sequence number for an operation.
    // P2P: derived from vector clock. Server: assigned by the relay.
    virtual u64 GetTotalOrder(const EditOperation& op) const = 0;

    // True if this is a server-authoritative model
    virtual bool IsServerAuthority() const = 0;
};

} // namespace Editor
} // namespace Enjin
