#include "Enjin/Editor/CollabUndoCommands.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Editor {

// ============================================================================
// CollabEditCommand
// ============================================================================

CollabEditCommand::CollabEditCommand(
    std::unique_ptr<ICommand> inner,
    CollaborativeEditingSystem* collab,
    const EditOperation& forwardOp,
    const EditOperation& inverseOp)
    : m_Inner(std::move(inner))
    , m_Collab(collab)
    , m_ForwardOp(forwardOp)
    , m_InverseOp(inverseOp)
{}

void CollabEditCommand::Execute() {
    if (m_Inner) m_Inner->Execute();

    // On first execute, the edit was already broadcast by the caller
    // (via OnTransformChanged, OnEntityCreated, etc.). On redo, we
    // need to broadcast the forward op.
    if (!m_FirstExecute && m_Collab && m_Collab->IsActive()) {
        // Re-record in CRDT and broadcast
        m_Collab->OnTransformChanged(
            static_cast<ECS::Entity>(m_ForwardOp.entityId),
            m_ForwardOp.position, m_ForwardOp.rotation, m_ForwardOp.scale);
    }
    m_FirstExecute = false;
}

void CollabEditCommand::Undo() {
    if (m_Inner) m_Inner->Undo();

    // Broadcast the inverse to peers so they see the undo
    if (m_Collab && m_Collab->IsActive()) {
        m_Collab->OnTransformChanged(
            static_cast<ECS::Entity>(m_InverseOp.entityId),
            m_InverseOp.position, m_InverseOp.rotation, m_InverseOp.scale);
    }
}

const char* CollabEditCommand::GetDescription() const {
    return m_Inner ? m_Inner->GetDescription() : "Collaborative Edit";
}

bool CollabEditCommand::CanMergeWith(const ICommand* other) const {
    if (!m_Inner) return false;
    auto* collabOther = dynamic_cast<const CollabEditCommand*>(other);
    if (collabOther && collabOther->m_Inner) {
        return m_Inner->CanMergeWith(collabOther->m_Inner.get());
    }
    return false;
}

void CollabEditCommand::MergeWith(const ICommand* other) {
    auto* collabOther = dynamic_cast<const CollabEditCommand*>(other);
    if (collabOther && collabOther->m_Inner && m_Inner) {
        m_Inner->MergeWith(collabOther->m_Inner.get());
        // Update the forward op to the latest merged state
        m_ForwardOp = collabOther->m_ForwardOp;
    }
}

// ============================================================================
// RemoteEditCommand
// ============================================================================

RemoteEditCommand::RemoteEditCommand(
    CollaborativeEditingSystem* collab,
    const EditOperation& op,
    const std::string& description)
    : m_Collab(collab)
    , m_Op(op)
    , m_InverseOp(ComputeInverseOp(op))
    , m_Description(description)
{}

void RemoteEditCommand::Execute() {
    // Re-apply the remote op (used on redo). The CRDT state was already
    // updated when the op first arrived; this just reapplies to the ECS.
    // Note: the actual ECS application is handled by HandleRemoteOperation
    // in CollaborativeEditingUI, not here. This command exists primarily
    // so undo works.
}

void RemoteEditCommand::Undo() {
    // Broadcast the inverse so peers see that we undid their edit
    if (m_Collab && m_Collab->IsActive()) {
        switch (m_InverseOp.type) {
            case EditOpType::ModifyTransform:
                m_Collab->OnTransformChanged(
                    static_cast<ECS::Entity>(m_InverseOp.entityId),
                    m_InverseOp.position, m_InverseOp.rotation, m_InverseOp.scale);
                break;
            case EditOpType::SetComponent:
                m_Collab->OnComponentChanged(
                    static_cast<ECS::Entity>(m_InverseOp.entityId),
                    m_InverseOp.componentKey,
                    m_InverseOp.dataJson,
                    m_InverseOp.previousJson);
                break;
            case EditOpType::RenameEntity:
                m_Collab->OnEntityRenamed(
                    static_cast<ECS::Entity>(m_InverseOp.entityId),
                    m_InverseOp.previousJson,
                    m_InverseOp.dataJson);
                break;
            case EditOpType::CreateEntity:
                // Inverse of create is delete
                m_Collab->OnEntityDeleted(
                    static_cast<ECS::Entity>(m_InverseOp.entityId),
                    m_InverseOp.dataJson);
                break;
            case EditOpType::DeleteEntity:
                // Inverse of delete is create
                m_Collab->OnEntityCreated(
                    static_cast<ECS::Entity>(m_InverseOp.entityId),
                    m_InverseOp.dataJson);
                break;
            default:
                break;
        }
    }
}

const char* RemoteEditCommand::GetDescription() const {
    return m_Description.c_str();
}

// ============================================================================
// ComputeInverseOp
// ============================================================================

EditOperation ComputeInverseOp(const EditOperation& op) {
    EditOperation inv = op;

    switch (op.type) {
        case EditOpType::CreateEntity:
            inv.type = EditOpType::DeleteEntity;
            break;
        case EditOpType::DeleteEntity:
            inv.type = EditOpType::CreateEntity;
            break;
        case EditOpType::RenameEntity:
            // Swap old and new names
            inv.dataJson = op.previousJson;
            inv.previousJson = op.dataJson;
            break;
        case EditOpType::SetComponent:
            // Swap current and previous JSON
            inv.dataJson = op.previousJson;
            inv.previousJson = op.dataJson;
            break;
        case EditOpType::RemoveComponent:
            // Inverse of remove is set (restore from previousJson)
            inv.type = EditOpType::SetComponent;
            inv.dataJson = op.previousJson;
            inv.previousJson = "";
            break;
        case EditOpType::ModifyTransform:
            // Previous transform is in previousJson as "px,py,pz;rx,ry,rz;sx,sy,sz"
            // or we can just use the position/rotation/scale fields.
            // The inverse restores whatever was there before — but we only have
            // the current state in the op. The previousJson should contain it.
            // For now, swap is handled by the undo command's inner ICommand.
            break;
        case EditOpType::SetParent:
            inv.dataJson = op.previousJson;
            inv.previousJson = op.dataJson;
            break;
        case EditOpType::LockEntity:
            inv.type = EditOpType::UnlockEntity;
            break;
        case EditOpType::UnlockEntity:
            inv.type = EditOpType::LockEntity;
            break;
    }

    return inv;
}

} // namespace Editor
} // namespace Enjin
