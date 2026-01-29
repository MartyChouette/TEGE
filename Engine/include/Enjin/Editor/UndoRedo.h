#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

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
