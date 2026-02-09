#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

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
// FullDeleteEntityCommand
// ============================================================================

FullDeleteEntityCommand::FullDeleteEntityCommand(ECS::World* world, ECS::Entity entity,
                                                  SelectionCallback onRestore)
    : m_World(world)
    , m_Entity(entity)
    , m_OnRestore(onRestore) {
    // Snapshot all components before deletion
    m_Snapshot = Scene::SceneSerializer::SerializeEntityToString(world, entity);
}

void FullDeleteEntityCommand::Execute() {
    if (m_World->IsValid(m_Entity)) {
        m_World->DestroyEntity(m_Entity);
    }
}

void FullDeleteEntityCommand::Undo() {
    ECS::Entity restored = Scene::SceneSerializer::DeserializeEntityFromString(m_World, m_Snapshot);
    if (restored != ECS::INVALID_ENTITY) {
        m_Entity = restored;
        if (m_OnRestore) m_OnRestore(restored);
    }
}

// ============================================================================
// FullCreateEntityCommand
// ============================================================================

FullCreateEntityCommand::FullCreateEntityCommand(ECS::World* world, ECS::Entity entity,
                                                  SelectionCallback onRestore)
    : m_World(world)
    , m_Entity(entity)
    , m_OnRestore(onRestore) {
    // Snapshot the newly created entity so we can redo
    m_Snapshot = Scene::SceneSerializer::SerializeEntityToString(world, entity);
}

void FullCreateEntityCommand::Execute() {
    if (m_FirstExecute) {
        // Entity already exists on first execute — just capture snapshot
        m_FirstExecute = false;
        return;
    }
    // Redo: recreate from snapshot
    ECS::Entity restored = Scene::SceneSerializer::DeserializeEntityFromString(m_World, m_Snapshot);
    if (restored != ECS::INVALID_ENTITY) {
        m_Entity = restored;
        if (m_OnRestore) m_OnRestore(restored);
    }
}

void FullCreateEntityCommand::Undo() {
    // Re-snapshot in case entity was modified since creation
    m_Snapshot = Scene::SceneSerializer::SerializeEntityToString(m_World, m_Entity);
    if (m_World->IsValid(m_Entity)) {
        m_World->DestroyEntity(m_Entity);
    }
}

// ============================================================================
// ReparentEntityCommand
// ============================================================================

ReparentEntityCommand::ReparentEntityCommand(ECS::World* world, ECS::Entity child,
                                              ECS::Entity oldParent, ECS::Entity newParent)
    : m_World(world)
    , m_Child(child)
    , m_OldParent(oldParent)
    , m_NewParent(newParent) {
}

void ReparentEntityCommand::Execute() {
    if (m_World->IsValid(m_Child) &&
        (m_NewParent == ECS::INVALID_ENTITY || m_World->IsValid(m_NewParent))) {
        ECS::SetParent(m_World, m_Child, m_NewParent);
    }
}

void ReparentEntityCommand::Undo() {
    if (m_World->IsValid(m_Child) &&
        (m_OldParent == ECS::INVALID_ENTITY || m_World->IsValid(m_OldParent))) {
        ECS::SetParent(m_World, m_Child, m_OldParent);
    }
}

// ============================================================================
// AddComponentCommand
// ============================================================================

AddComponentCommand::AddComponentCommand(const std::string& componentName,
                                          AddFunc addFunc, RemoveFunc removeFunc)
    : m_Description("Add " + componentName)
    , m_AddFunc(addFunc)
    , m_RemoveFunc(removeFunc) {
}

void AddComponentCommand::Execute() {
    if (m_FirstExecute) {
        m_FirstExecute = false;
        return; // Component was already added by the caller
    }
    if (m_AddFunc) m_AddFunc();
}

void AddComponentCommand::Undo() {
    if (m_RemoveFunc) m_RemoveFunc();
}

// ============================================================================
// RemoveComponentCommand
// ============================================================================

RemoveComponentCommand::RemoveComponentCommand(ECS::World* world, ECS::Entity entity,
                                                const std::string& componentKey,
                                                const std::string& componentName,
                                                RemoveFunc removeFunc)
    : m_World(world)
    , m_Entity(entity)
    , m_ComponentKey(componentKey)
    , m_Description("Remove " + componentName)
    , m_RemoveFunc(removeFunc) {
    // Snapshot the component data before removal
    m_Snapshot = Scene::SceneSerializer::SerializeOneComponent(world, entity, componentKey);
}

void RemoveComponentCommand::Execute() {
    if (m_RemoveFunc) m_RemoveFunc();
}

void RemoveComponentCommand::Undo() {
    if (!m_Snapshot.empty()) {
        Scene::SceneSerializer::DeserializeOneComponent(m_World, m_Entity, m_ComponentKey, m_Snapshot);
    }
}

// ============================================================================
// TilemapPaintCommand
// ============================================================================

void TilemapPaintCommand::Execute() {
    auto* tilemap = m_World->GetComponent<ECS::TilemapComponent>(m_Entity);
    if (!tilemap) return;
    for (auto& c : m_Changes) {
        tilemap->SetTile(c.x, c.y, c.newIndex);
    }
}

void TilemapPaintCommand::Undo() {
    auto* tilemap = m_World->GetComponent<ECS::TilemapComponent>(m_Entity);
    if (!tilemap) return;
    for (auto it = m_Changes.rbegin(); it != m_Changes.rend(); ++it) {
        tilemap->SetTile(it->x, it->y, it->oldIndex);
    }
}

// ============================================================================
// TerrainSculptCommand
// ============================================================================

void TerrainSculptCommand::Execute() {
    auto* terrain = m_World->GetComponent<ECS::TerrainComponent>(m_Entity);
    if (!terrain) return;
    terrain->heightmap = m_NewHeightmap;
    terrain->splatmap = m_NewSplatmap;
    terrain->meshDirty = true;
}

void TerrainSculptCommand::Undo() {
    auto* terrain = m_World->GetComponent<ECS::TerrainComponent>(m_Entity);
    if (!terrain) return;
    terrain->heightmap = m_OldHeightmap;
    terrain->splatmap = m_OldSplatmap;
    terrain->meshDirty = true;
}

// ============================================================================
// UIAnchorEditCommand
// ============================================================================

void UIAnchorEditCommand::Execute() {
    auto* canvas = m_World->GetComponent<GUI::UICanvasComponent>(m_CanvasEntity);
    if (!canvas) return;
    auto* elem = canvas->GetElement(m_ElementId);
    if (elem) elem->anchor = m_NewAnchor;
}

void UIAnchorEditCommand::Undo() {
    auto* canvas = m_World->GetComponent<GUI::UICanvasComponent>(m_CanvasEntity);
    if (!canvas) return;
    auto* elem = canvas->GetElement(m_ElementId);
    if (elem) elem->anchor = m_OldAnchor;
}

bool UIAnchorEditCommand::CanMergeWith(const ICommand* other) const {
    auto* o = dynamic_cast<const UIAnchorEditCommand*>(other);
    return o && o->m_CanvasEntity == m_CanvasEntity &&
           o->m_ElementId == m_ElementId &&
           std::strcmp(o->m_Desc, m_Desc) == 0;
}

void UIAnchorEditCommand::MergeWith(const ICommand* other) {
    auto* o = dynamic_cast<const UIAnchorEditCommand*>(other);
    if (o) m_NewAnchor = o->m_NewAnchor;
}

// ============================================================================
// UIElementDeleteCommand
// ============================================================================

void UIElementDeleteCommand::Execute() {
    auto* canvas = m_World->GetComponent<GUI::UICanvasComponent>(m_CanvasEntity);
    if (!canvas) return;
    canvas->RemoveElement(m_Element.id);
}

void UIElementDeleteCommand::Undo() {
    auto* canvas = m_World->GetComponent<GUI::UICanvasComponent>(m_CanvasEntity);
    if (!canvas) return;
    // Re-insert the element into the canvas
    canvas->elements.push_back(m_Element);
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
