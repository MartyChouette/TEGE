#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/Editor/CollaborativeEditing.h"
#include <memory>
#include <string>

namespace Enjin {
namespace ECS { class World; }
namespace Editor {

// ============================================================================
// CollabEditCommand — wraps any ICommand and broadcasts via collab
// ============================================================================
// When collab is active, local edits are wrapped so that:
//   Execute() → inner->Execute() + broadcast forward op to peers
//   Undo()    → inner->Undo()    + broadcast inverse op to peers
//
// On first Execute(), the forward op is NOT broadcast (the caller already
// triggered the collab notification). Subsequent Execute() calls (redo)
// DO broadcast.

class ENJIN_API CollabEditCommand : public ICommand {
public:
    CollabEditCommand(std::unique_ptr<ICommand> inner,
                      CollaborativeEditingSystem* collab,
                      const EditOperation& forwardOp,
                      const EditOperation& inverseOp);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override;
    bool CanMergeWith(const ICommand* other) const override;
    void MergeWith(const ICommand* other) override;

private:
    std::unique_ptr<ICommand> m_Inner;
    CollaborativeEditingSystem* m_Collab;
    EditOperation m_ForwardOp;
    EditOperation m_InverseOp;
    bool m_FirstExecute = true;
};

// ============================================================================
// RemoteEditCommand — pushed to undo stack when a remote op arrives
// ============================================================================
// Allows the local user to undo remote edits. Undoing broadcasts the
// inverse operation back to peers.

class ENJIN_API RemoteEditCommand : public ICommand {
public:
    RemoteEditCommand(CollaborativeEditingSystem* collab,
                      const EditOperation& op,
                      const std::string& description);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override;

private:
    CollaborativeEditingSystem* m_Collab;
    EditOperation m_Op;
    EditOperation m_InverseOp;
    std::string m_Description;
};

// ============================================================================
// Inverse operation helper
// ============================================================================
// Compute the inverse of an EditOperation. For transforms, the inverse
// restores the previous position/rotation/scale. For component changes,
// it swaps dataJson and previousJson. For create/delete, it inverts.

EditOperation ComputeInverseOp(const EditOperation& op);

} // namespace Editor
} // namespace Enjin
