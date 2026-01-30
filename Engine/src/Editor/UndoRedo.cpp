#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Editor {

// ============================================================================
// TransformCommand
// ============================================================================

TransformCommand::TransformCommand(ECS::World* world, ECS::Entity entity,
                                  const ECS::TransformComponent& oldTransform,
                                  const ECS::TransformComponent& newTransform)
    : m_World(world)
    , m_Entity(entity)
    , m_OldTransform(oldTransform)
    , m_NewTransform(newTransform) {
}

void TransformCommand::Execute() {
    if (m_World->HasComponent<ECS::TransformComponent>(m_Entity)) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_Entity);
        *transform = m_NewTransform;
    }
}

void TransformCommand::Undo() {
    if (m_World->HasComponent<ECS::TransformComponent>(m_Entity)) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_Entity);
        *transform = m_OldTransform;
    }
}

bool TransformCommand::CanMergeWith(const ICommand* other) const {
    auto* otherTransform = dynamic_cast<const TransformCommand*>(other);
    return otherTransform && otherTransform->m_Entity == m_Entity;
}

void TransformCommand::MergeWith(const ICommand* other) {
    auto* otherTransform = dynamic_cast<const TransformCommand*>(other);
    if (otherTransform) {
        m_NewTransform = otherTransform->m_NewTransform;
    }
}

// ============================================================================
// MoveCommand
// ============================================================================

MoveCommand::MoveCommand(ECS::World* world, ECS::Entity entity,
                        const Math::Vector3& oldPosition,
                        const Math::Vector3& newPosition)
    : m_World(world)
    , m_Entity(entity)
    , m_OldPosition(oldPosition)
    , m_NewPosition(newPosition) {
}

void MoveCommand::Execute() {
    if (m_World->HasComponent<ECS::TransformComponent>(m_Entity)) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_Entity);
        transform->position = m_NewPosition;
    }
}

void MoveCommand::Undo() {
    if (m_World->HasComponent<ECS::TransformComponent>(m_Entity)) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_Entity);
        transform->position = m_OldPosition;
    }
}

bool MoveCommand::CanMergeWith(const ICommand* other) const {
    auto* otherMove = dynamic_cast<const MoveCommand*>(other);
    return otherMove && otherMove->m_Entity == m_Entity;
}

void MoveCommand::MergeWith(const ICommand* other) {
    auto* otherMove = dynamic_cast<const MoveCommand*>(other);
    if (otherMove) {
        m_NewPosition = otherMove->m_NewPosition;
    }
}

// ============================================================================
// RotateCommand
// ============================================================================

RotateCommand::RotateCommand(ECS::World* world, ECS::Entity entity,
                            const Math::Quaternion& oldRotation,
                            const Math::Quaternion& newRotation)
    : m_World(world)
    , m_Entity(entity)
    , m_OldRotation(oldRotation)
    , m_NewRotation(newRotation) {
}

void RotateCommand::Execute() {
    if (m_World->HasComponent<ECS::TransformComponent>(m_Entity)) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_Entity);
        transform->rotation = m_NewRotation;
    }
}

void RotateCommand::Undo() {
    if (m_World->HasComponent<ECS::TransformComponent>(m_Entity)) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_Entity);
        transform->rotation = m_OldRotation;
    }
}

bool RotateCommand::CanMergeWith(const ICommand* other) const {
    auto* otherRotate = dynamic_cast<const RotateCommand*>(other);
    return otherRotate && otherRotate->m_Entity == m_Entity;
}

void RotateCommand::MergeWith(const ICommand* other) {
    auto* otherRotate = dynamic_cast<const RotateCommand*>(other);
    if (otherRotate) {
        m_NewRotation = otherRotate->m_NewRotation;
    }
}

// ============================================================================
// ScaleCommand
// ============================================================================

ScaleCommand::ScaleCommand(ECS::World* world, ECS::Entity entity,
                          const Math::Vector3& oldScale,
                          const Math::Vector3& newScale)
    : m_World(world)
    , m_Entity(entity)
    , m_OldScale(oldScale)
    , m_NewScale(newScale) {
}

void ScaleCommand::Execute() {
    if (m_World->HasComponent<ECS::TransformComponent>(m_Entity)) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_Entity);
        transform->scale = m_NewScale;
    }
}

void ScaleCommand::Undo() {
    if (m_World->HasComponent<ECS::TransformComponent>(m_Entity)) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(m_Entity);
        transform->scale = m_OldScale;
    }
}

bool ScaleCommand::CanMergeWith(const ICommand* other) const {
    auto* otherScale = dynamic_cast<const ScaleCommand*>(other);
    return otherScale && otherScale->m_Entity == m_Entity;
}

void ScaleCommand::MergeWith(const ICommand* other) {
    auto* otherScale = dynamic_cast<const ScaleCommand*>(other);
    if (otherScale) {
        m_NewScale = otherScale->m_NewScale;
    }
}

// ============================================================================
// CreateEntityCommand
// ============================================================================

CreateEntityCommand::CreateEntityCommand(ECS::World* world, const std::string& name,
                                        EntitySetupFunc setupFunc)
    : m_World(world)
    , m_Name(name)
    , m_SetupFunc(setupFunc) {
}

void CreateEntityCommand::Execute() {
    if (!m_Executed) {
        m_Entity = m_World->CreateEntity();
        m_Executed = true;
    }
    // Re-add components if entity was previously deleted

    // Add name
    if (!m_World->HasComponent<ECS::NameComponent>(m_Entity)) {
        auto& nameComp = m_World->AddComponent<ECS::NameComponent>(m_Entity);
        nameComp.name = m_Name;
    }

    // Add default transform
    if (!m_World->HasComponent<ECS::TransformComponent>(m_Entity)) {
        m_World->AddComponent<ECS::TransformComponent>(m_Entity);
    }

    // Run custom setup
    if (m_SetupFunc) {
        m_SetupFunc(m_World, m_Entity);
    }
}

void CreateEntityCommand::Undo() {
    if (m_Entity != ECS::INVALID_ENTITY) {
        m_World->DestroyEntity(m_Entity);
    }
}

// ============================================================================
// DeleteEntityCommand
// ============================================================================

DeleteEntityCommand::DeleteEntityCommand(ECS::World* world, ECS::Entity entity)
    : m_World(world)
    , m_Entity(entity) {
    // Save component data for undo
    if (m_World->HasComponent<ECS::NameComponent>(entity)) {
        m_Name = m_World->GetComponent<ECS::NameComponent>(entity)->name;
    }
    if (m_World->HasComponent<ECS::TransformComponent>(entity)) {
        m_HadTransform = true;
        m_Transform = *m_World->GetComponent<ECS::TransformComponent>(entity);
    }
}

void DeleteEntityCommand::Execute() {
    m_World->DestroyEntity(m_Entity);
}

void DeleteEntityCommand::Undo() {
    // Recreate entity - note: entity ID may change
    // This is a limitation of simple ECS systems
    ECS::Entity newEntity = m_World->CreateEntity();

    // Restore name
    if (!m_Name.empty()) {
        auto& nameComp = m_World->AddComponent<ECS::NameComponent>(newEntity);
        nameComp.name = m_Name;
    }

    // Restore transform
    if (m_HadTransform) {
        auto& transform = m_World->AddComponent<ECS::TransformComponent>(newEntity);
        transform = m_Transform;
    }

    // Update our reference
    m_Entity = newEntity;
}

// ============================================================================
// RenameEntityCommand
// ============================================================================

RenameEntityCommand::RenameEntityCommand(ECS::World* world, ECS::Entity entity,
                                        const std::string& oldName, const std::string& newName)
    : m_World(world)
    , m_Entity(entity)
    , m_OldName(oldName)
    , m_NewName(newName) {
}

void RenameEntityCommand::Execute() {
    if (m_World->HasComponent<ECS::NameComponent>(m_Entity)) {
        auto* nameComp = m_World->GetComponent<ECS::NameComponent>(m_Entity);
        nameComp->name = m_NewName;
    }
}

void RenameEntityCommand::Undo() {
    if (m_World->HasComponent<ECS::NameComponent>(m_Entity)) {
        auto* nameComp = m_World->GetComponent<ECS::NameComponent>(m_Entity);
        nameComp->name = m_OldName;
    }
}

// ============================================================================
// CompoundCommand
// ============================================================================

CompoundCommand::CompoundCommand(const std::string& description)
    : m_Description(description) {
}

void CompoundCommand::AddCommand(std::unique_ptr<ICommand> cmd) {
    m_Commands.push_back(std::move(cmd));
}

void CompoundCommand::Execute() {
    for (auto& cmd : m_Commands) {
        cmd->Execute();
    }
}

void CompoundCommand::Undo() {
    // Undo in reverse order
    for (auto it = m_Commands.rbegin(); it != m_Commands.rend(); ++it) {
        (*it)->Undo();
    }
}

// ============================================================================
// UndoRedoManager
// ============================================================================

UndoRedoManager::UndoRedoManager(u32 maxHistorySize)
    : m_MaxHistorySize(maxHistorySize) {
}

UndoRedoManager::~UndoRedoManager() {
    Clear();
}

void UndoRedoManager::Execute(std::unique_ptr<ICommand> command) {
    if (!command) return;

    // If building compound, add to it instead
    if (m_CompoundCommand) {
        command->Execute();
        m_CompoundCommand->AddCommand(std::move(command));
        return;
    }

    // Execute the command
    command->Execute();

    // Add to undo stack
    AddToUndoStack(std::move(command));

    // Clear redo stack (new action invalidates redo history)
    m_RedoStack.clear();

    NotifyStateChanged();
}

void UndoRedoManager::AddToUndoStack(std::unique_ptr<ICommand> command) {
    // Try to merge with last command if enabled
    if (m_MergeEnabled && !m_UndoStack.empty()) {
        auto& last = m_UndoStack.back();
        if (last->CanMergeWith(command.get())) {
            last->MergeWith(command.get());
            return;
        }
    }

    // Add to stack
    m_UndoStack.push_back(std::move(command));

    // Trim history if needed
    while (m_UndoStack.size() > m_MaxHistorySize) {
        m_UndoStack.erase(m_UndoStack.begin());
    }
}

void UndoRedoManager::Undo() {
    if (m_UndoStack.empty()) return;

    auto cmd = std::move(m_UndoStack.back());
    m_UndoStack.pop_back();

    cmd->Undo();

    m_RedoStack.push_back(std::move(cmd));

    ENJIN_LOG_INFO(Editor, "Undo: %s", m_RedoStack.back()->GetDescription());
    NotifyStateChanged();
}

void UndoRedoManager::Redo() {
    if (m_RedoStack.empty()) return;

    auto cmd = std::move(m_RedoStack.back());
    m_RedoStack.pop_back();

    cmd->Execute();

    m_UndoStack.push_back(std::move(cmd));

    ENJIN_LOG_INFO(Editor, "Redo: %s", m_UndoStack.back()->GetDescription());
    NotifyStateChanged();
}

const char* UndoRedoManager::GetUndoDescription() const {
    if (m_UndoStack.empty()) return nullptr;
    return m_UndoStack.back()->GetDescription();
}

const char* UndoRedoManager::GetRedoDescription() const {
    if (m_RedoStack.empty()) return nullptr;
    return m_RedoStack.back()->GetDescription();
}

void UndoRedoManager::Clear() {
    m_UndoStack.clear();
    m_RedoStack.clear();
    m_CompoundCommand.reset();
    m_CompoundDepth = 0;
    NotifyStateChanged();
}

void UndoRedoManager::BeginCompound(const std::string& description) {
    if (m_CompoundDepth == 0) {
        m_CompoundCommand = std::make_unique<CompoundCommand>(description);
    }
    m_CompoundDepth++;
}

void UndoRedoManager::EndCompound() {
    if (m_CompoundDepth > 0) {
        m_CompoundDepth--;
        if (m_CompoundDepth == 0 && m_CompoundCommand) {
            if (!m_CompoundCommand->IsEmpty()) {
                AddToUndoStack(std::move(m_CompoundCommand));
                m_RedoStack.clear();
                NotifyStateChanged();
            } else {
                m_CompoundCommand.reset();
            }
        }
    }
}

void UndoRedoManager::NotifyStateChanged() {
    if (m_OnStateChanged) {
        m_OnStateChanged();
    }
}

} // namespace Editor
} // namespace Enjin
