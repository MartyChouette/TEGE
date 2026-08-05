#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/GUI/UIElement.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Enjin {
namespace Editor {

// ============================================================================
// Command Interface
// ============================================================================

/**
 * @brief Base class for undoable commands
 */
class ENJIN_API ICommand {
public:
    virtual ~ICommand() = default;

    /// Execute the command
    virtual void Execute() = 0;

    /// Undo the command
    virtual void Undo() = 0;

    /// Get a description of the command for UI display
    virtual const char* GetDescription() const = 0;

    /// Check if this command can be merged with another
    virtual bool CanMergeWith(const ICommand* other) const { (void)other; return false; }

    /// Merge another command into this one
    virtual void MergeWith(const ICommand* other) { (void)other; }
};

// ============================================================================
// Transform Commands
// ============================================================================

/**
 * @brief Command to change an entity's transform
 */
class ENJIN_API TransformCommand : public ICommand {
public:
    TransformCommand(ECS::World* world, ECS::Entity entity,
                    const ECS::TransformComponent& oldTransform,
                    const ECS::TransformComponent& newTransform);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Transform"; }

    bool CanMergeWith(const ICommand* other) const override;
    void MergeWith(const ICommand* other) override;

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    ECS::TransformComponent m_OldTransform;
    ECS::TransformComponent m_NewTransform;
};

/**
 * @brief Command to change just position
 */
class ENJIN_API MoveCommand : public ICommand {
public:
    MoveCommand(ECS::World* world, ECS::Entity entity,
               const Math::Vector3& oldPosition,
               const Math::Vector3& newPosition);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Move"; }

    bool CanMergeWith(const ICommand* other) const override;
    void MergeWith(const ICommand* other) override;

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    Math::Vector3 m_OldPosition;
    Math::Vector3 m_NewPosition;
};

/**
 * @brief Command to change rotation
 */
class ENJIN_API RotateCommand : public ICommand {
public:
    RotateCommand(ECS::World* world, ECS::Entity entity,
                 const Math::Quaternion& oldRotation,
                 const Math::Quaternion& newRotation);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Rotate"; }

    bool CanMergeWith(const ICommand* other) const override;
    void MergeWith(const ICommand* other) override;

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    Math::Quaternion m_OldRotation;
    Math::Quaternion m_NewRotation;
};

/**
 * @brief Command to change scale
 */
class ENJIN_API ScaleCommand : public ICommand {
public:
    ScaleCommand(ECS::World* world, ECS::Entity entity,
                const Math::Vector3& oldScale,
                const Math::Vector3& newScale);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Scale"; }

    bool CanMergeWith(const ICommand* other) const override;
    void MergeWith(const ICommand* other) override;

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    Math::Vector3 m_OldScale;
    Math::Vector3 m_NewScale;
};

// ============================================================================
// Entity Commands
// ============================================================================

/**
 * @brief Command to create an entity
 */
class ENJIN_API CreateEntityCommand : public ICommand {
public:
    using EntitySetupFunc = std::function<void(ECS::World*, ECS::Entity)>;

    CreateEntityCommand(ECS::World* world, const std::string& name,
                       EntitySetupFunc setupFunc = nullptr);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Create Entity"; }

    ECS::Entity GetCreatedEntity() const { return m_Entity; }

private:
    ECS::World* m_World;
    std::string m_Name;
    ECS::Entity m_Entity = ECS::INVALID_ENTITY;
    EntitySetupFunc m_SetupFunc;
    bool m_Executed = false;
};

/**
 * @brief Command to delete an entity
 */
class ENJIN_API DeleteEntityCommand : public ICommand {
public:
    DeleteEntityCommand(ECS::World* world, ECS::Entity entity);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Delete Entity"; }

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    std::string m_Name;
    bool m_HadTransform = false;
    ECS::TransformComponent m_Transform;
    // Add more component storage as needed
};

/**
 * @brief Command to rename an entity
 */
class ENJIN_API RenameEntityCommand : public ICommand {
public:
    RenameEntityCommand(ECS::World* world, ECS::Entity entity,
                       const std::string& oldName, const std::string& newName);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Rename"; }

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    std::string m_OldName;
    std::string m_NewName;
};

// ============================================================================
// Full-fidelity Entity Commands (JSON snapshot-based)
// ============================================================================

/// Callback to update editor selection after entity ID changes on undo/redo
using SelectionCallback = std::function<void(ECS::Entity)>;

/**
 * @brief Delete entity with full JSON snapshot of all components.
 * Replaces the old DeleteEntityCommand which only saved Name+Transform.
 */
class ENJIN_API FullDeleteEntityCommand : public ICommand {
public:
    FullDeleteEntityCommand(ECS::World* world, ECS::Entity entity,
                            SelectionCallback onRestore = nullptr);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Delete Entity"; }

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    std::string m_Snapshot; // Full JSON of all components
    SelectionCallback m_OnRestore;
};

/**
 * @brief Tracks a created entity (duplicate/paste) for undo.
 * First Execute() is a no-op (entity already exists). Undo destroys it.
 * Subsequent Execute() (redo) recreates from snapshot.
 */
class ENJIN_API FullCreateEntityCommand : public ICommand {
public:
    FullCreateEntityCommand(ECS::World* world, ECS::Entity entity,
                            SelectionCallback onRestore = nullptr);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Create Entity"; }

    ECS::Entity GetEntity() const { return m_Entity; }

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    std::string m_Snapshot;
    bool m_FirstExecute = true;
    SelectionCallback m_OnRestore;
};

/**
 * @brief Generic property-edit command: before/after JSON snapshots of ONE
 * entity's full component state. This is what makes arbitrary inspector
 * edits undoable across all 140 component types without a bespoke command
 * per component. Applying a state deserializes every component key present
 * in the snapshot and removes components the snapshot doesn't have.
 */
class ENJIN_API EntityEditCommand : public ICommand {
public:
    EntityEditCommand(ECS::World* world, ECS::Entity entity,
                      std::string beforeJson, std::string afterJson);

    void Execute() override;   // first call is a no-op: the edit is already live
    void Undo() override;
    const char* GetDescription() const override { return m_Description.c_str(); }

private:
    void ApplyState(const std::string& targetJson);

    ECS::World* m_World;
    ECS::Entity m_Entity;
    std::string m_Before;
    std::string m_After;
    std::string m_Description;
    bool m_FirstExecute = true;
};

/**
 * @brief Command to reparent an entity (drag-drop or unparent).
 */
class ENJIN_API ReparentEntityCommand : public ICommand {
public:
    ReparentEntityCommand(ECS::World* world, ECS::Entity child,
                          ECS::Entity oldParent, ECS::Entity newParent);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Reparent Entity"; }

private:
    ECS::World* m_World;
    ECS::Entity m_Child;
    ECS::Entity m_OldParent;
    ECS::Entity m_NewParent;
};

/**
 * @brief Command to add a component (undo removes it).
 */
class ENJIN_API AddComponentCommand : public ICommand {
public:
    using AddFunc = std::function<void()>;
    using RemoveFunc = std::function<void()>;

    AddComponentCommand(const std::string& componentName,
                        AddFunc addFunc, RemoveFunc removeFunc);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return m_Description.c_str(); }

private:
    std::string m_Description;
    AddFunc m_AddFunc;
    RemoveFunc m_RemoveFunc;
    bool m_FirstExecute = true;
};

/**
 * @brief Command to remove a component (undo restores it from JSON snapshot).
 */
class ENJIN_API RemoveComponentCommand : public ICommand {
public:
    using RemoveFunc = std::function<void()>;

    RemoveComponentCommand(ECS::World* world, ECS::Entity entity,
                           const std::string& componentKey,
                           const std::string& componentName,
                           RemoveFunc removeFunc);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return m_Description.c_str(); }

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    std::string m_ComponentKey;
    std::string m_Description;
    std::string m_Snapshot; // JSON of the component data
    RemoveFunc m_RemoveFunc;
};

// ============================================================================
// Tilemap Paint Command
// ============================================================================

struct TilemapTileChange {
    u32 x, y;
    i32 oldIndex, newIndex;
};

/**
 * @brief Command to undo/redo an entire tilemap brush stroke.
 * Stores per-cell old/new tile indices, deduplicated per cell.
 */
class ENJIN_API TilemapPaintCommand : public ICommand {
public:
    TilemapPaintCommand(ECS::World* world, ECS::Entity entity,
                        std::vector<TilemapTileChange> changes)
        : m_World(world), m_Entity(entity), m_Changes(std::move(changes)) {}

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Tilemap Paint"; }

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    std::vector<TilemapTileChange> m_Changes;
};

// ============================================================================
// Terrain Sculpt Command
// ============================================================================

/**
 * @brief Command to undo/redo a terrain sculpt stroke.
 * Stores snapshots of the full heightmap and splatmap before/after the stroke.
 */
class ENJIN_API TerrainSculptCommand : public ICommand {
public:
    TerrainSculptCommand(ECS::World* world, ECS::Entity entity,
                         std::vector<f32> oldHeightmap, std::vector<f32> newHeightmap,
                         std::vector<f32> oldSplatmap, std::vector<f32> newSplatmap)
        : m_World(world), m_Entity(entity),
          m_OldHeightmap(std::move(oldHeightmap)), m_NewHeightmap(std::move(newHeightmap)),
          m_OldSplatmap(std::move(oldSplatmap)), m_NewSplatmap(std::move(newSplatmap)) {}

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "Terrain Sculpt"; }

private:
    ECS::World* m_World;
    ECS::Entity m_Entity;
    std::vector<f32> m_OldHeightmap;
    std::vector<f32> m_NewHeightmap;
    std::vector<f32> m_OldSplatmap;
    std::vector<f32> m_NewSplatmap;
};

// ============================================================================
// UI Anchor Edit Command
// ============================================================================

/**
 * @brief Command to undo/redo UI element anchor changes (move, resize, nudge).
 * Supports merging for arrow key nudges.
 */
class ENJIN_API UIAnchorEditCommand : public ICommand {
public:
    UIAnchorEditCommand(ECS::World* world, ECS::Entity canvasEntity, u32 elementId,
                        const GUI::UIAnchor& oldAnchor, const GUI::UIAnchor& newAnchor,
                        const char* desc = "UI Move")
        : m_World(world), m_CanvasEntity(canvasEntity), m_ElementId(elementId),
          m_OldAnchor(oldAnchor), m_NewAnchor(newAnchor), m_Desc(desc) {}

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return m_Desc; }

    bool CanMergeWith(const ICommand* other) const override;
    void MergeWith(const ICommand* other) override;

private:
    ECS::World* m_World;
    ECS::Entity m_CanvasEntity;
    u32 m_ElementId;
    GUI::UIAnchor m_OldAnchor;
    GUI::UIAnchor m_NewAnchor;
    const char* m_Desc;
};

// ============================================================================
// UI Element Delete Command
// ============================================================================

/**
 * @brief Command to undo/redo UI element deletion.
 * Stores a full copy of the UIElement for restoration.
 */
class ENJIN_API UIElementDeleteCommand : public ICommand {
public:
    UIElementDeleteCommand(ECS::World* world, ECS::Entity canvasEntity,
                           const GUI::UIElement& element)
        : m_World(world), m_CanvasEntity(canvasEntity), m_Element(element) {}

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return "UI Delete Element"; }

private:
    ECS::World* m_World;
    ECS::Entity m_CanvasEntity;
    GUI::UIElement m_Element;
};

// ============================================================================
// Compound Command
// ============================================================================

/**
 * @brief Groups multiple commands into one undoable action
 */
class ENJIN_API CompoundCommand : public ICommand {
public:
    CompoundCommand(const std::string& description);

    void AddCommand(std::unique_ptr<ICommand> cmd);

    void Execute() override;
    void Undo() override;
    const char* GetDescription() const override { return m_Description.c_str(); }

    bool IsEmpty() const { return m_Commands.empty(); }

private:
    std::string m_Description;
    std::vector<std::unique_ptr<ICommand>> m_Commands;
};

// ============================================================================
// Undo/Redo Manager
// ============================================================================

/**
 * @brief Manages undo/redo stack
 */
class ENJIN_API UndoRedoManager {
public:
    UndoRedoManager(u32 maxHistorySize = 100);
    ~UndoRedoManager();

    /// Execute a command and add it to the undo stack
    void Execute(std::unique_ptr<ICommand> command);

    /// Undo the last command
    void Undo();

    /// Redo the last undone command
    void Redo();

    /// Check if undo is available
    bool CanUndo() const { return !m_UndoStack.empty(); }

    /// Check if redo is available
    bool CanRedo() const { return !m_RedoStack.empty(); }

    /// Get description of command to undo
    const char* GetUndoDescription() const;

    /// Get description of command to redo
    const char* GetRedoDescription() const;

    /// Clear all history
    void Clear();

    /// Get undo stack size
    u32 GetUndoCount() const { return static_cast<u32>(m_UndoStack.size()); }

    /// Get redo stack size
    u32 GetRedoCount() const { return static_cast<u32>(m_RedoStack.size()); }

    // --- History enumeration (History panel) ---
    /// Description of undo stack entry i, i=0 is the OLDEST action
    const char* GetUndoDescriptionAt(u32 i) const {
        return i < m_UndoStack.size() ? m_UndoStack[i]->GetDescription() : "";
    }
    /// Description of redo entry i, i=0 is the NEXT action redo would apply
    const char* GetRedoDescriptionAt(u32 i) const {
        usize n = m_RedoStack.size();
        return i < n ? m_RedoStack[n - 1 - i]->GetDescription() : "";
    }
    /// Jump so that exactly `undoDepth` actions are applied (0 = before the
    /// first recorded action). Undoes or redoes as many steps as needed.
    void JumpTo(u32 undoDepth) {
        while (GetUndoCount() > undoDepth && CanUndo()) Undo();
        while (GetUndoCount() < undoDepth && CanRedo()) Redo();
    }

    /// Begin a compound command group
    void BeginCompound(const std::string& description);

    /// End the compound command group
    void EndCompound();

    /// Check if currently building a compound command
    bool IsCompounding() const { return m_CompoundCommand != nullptr; }

    /// Enable/disable merging of similar consecutive commands
    void SetMergeEnabled(bool enabled) { m_MergeEnabled = enabled; }
    bool IsMergeEnabled() const { return m_MergeEnabled; }

    /// Set callback for when undo/redo state changes
    using StateChangedCallback = std::function<void()>;
    void SetStateChangedCallback(StateChangedCallback callback) {
        m_OnStateChanged = callback;
    }

private:
    void AddToUndoStack(std::unique_ptr<ICommand> command);
    void NotifyStateChanged();

    std::vector<std::unique_ptr<ICommand>> m_UndoStack;
    std::vector<std::unique_ptr<ICommand>> m_RedoStack;
    u32 m_MaxHistorySize;
    bool m_MergeEnabled = true;

    // Compound command support
    std::unique_ptr<CompoundCommand> m_CompoundCommand;
    u32 m_CompoundDepth = 0;

    StateChangedCallback m_OnStateChanged;
};

// ============================================================================
// Transaction Helper (RAII)
// ============================================================================

/**
 * @brief RAII helper for compound commands
 *
 * Usage:
 *   {
 *       UndoTransaction transaction(manager, "Move Multiple");
 *       manager.Execute(...);
 *       manager.Execute(...);
 *   } // Automatically ends compound on destruction
 */
class ENJIN_API UndoTransaction {
public:
    UndoTransaction(UndoRedoManager& manager, const std::string& description)
        : m_Manager(manager) {
        m_Manager.BeginCompound(description);
    }

    ~UndoTransaction() {
        m_Manager.EndCompound();
    }

    // Non-copyable
    UndoTransaction(const UndoTransaction&) = delete;
    UndoTransaction& operator=(const UndoTransaction&) = delete;

private:
    UndoRedoManager& m_Manager;
};

} // namespace Editor
} // namespace Enjin
