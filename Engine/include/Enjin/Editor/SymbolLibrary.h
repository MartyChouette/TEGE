#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Enjin {
namespace Editor {

// Forward declarations
class VectorDrawingEditor;
struct FlashTimelineData;

// ============================================================================
// Symbol Types
// ============================================================================

enum class SymbolType : u8 {
    VectorDrawing,   // SVG-based vector symbol
    EntityPrefab,    // ECS entity hierarchy (.enjprefab)
    SpriteSheet,     // Sprite sheet reference
    Custom           // User-defined / external asset
};

// ============================================================================
// Symbol Entry
// ============================================================================

/**
 * @brief A single entry in the symbol library catalog
 *
 * Each symbol has a unique filesystem-safe ID, a display name, optional
 * category/tags for organization, and a path to the underlying asset
 * (.enjprefab or .svg).
 */
struct SymbolEntry {
    std::string id;             // Unique ID (filesystem-safe)
    std::string name;           // Display name
    std::string category;       // "Characters", "UI", "Effects", etc.
    SymbolType type = SymbolType::EntityPrefab;
    std::string assetPath;      // Path to .enjprefab or .svg
    std::string thumbnailPath;  // Preview image path
    std::vector<std::string> tags;
    u32 useCount = 0;           // Reference count across scenes
    bool isBuiltin = false;     // true for engine-provided symbols
};

// ============================================================================
// Symbol Edit Context
// ============================================================================

/**
 * @brief Editing context for nested (double-click-to-edit) symbol editing
 *
 * When the user enters a symbol for editing, an isolated ECS::World is
 * created, the prefab is loaded into it, and changes are rendered in a
 * dedicated viewport. On save, the prefab is re-serialized and optionally
 * propagated to all instances.
 */
struct SymbolEditContext {
    std::string symbolId;
    ECS::World* editWorld = nullptr;   // Isolated world for editing
    bool isDirty = false;
    std::string breadcrumbPath;        // "Scene > Button > Icon"
};

// ============================================================================
// Symbol Library
// ============================================================================

/**
 * @brief Central catalog and manager for reusable symbols
 *
 * The SymbolLibrary stores symbols in a `symbols/` directory alongside the
 * project. Each symbol lives in `symbols/{id}/` containing the asset file
 * (.enjprefab or .svg) and an optional thumbnail. The catalog is persisted
 * as `symbols/catalog.json`.
 *
 * Integrates with:
 *   - PrefabManager for entity-based symbols
 *   - VectorDrawingEditor for SVG-based symbols
 *   - FlashTimelineData::symbolLibrary for timeline symbol references
 */
class ENJIN_API SymbolLibrary {
public:
    SymbolLibrary() = default;
    ~SymbolLibrary();

    // --- Catalog management ---

    /// Initialize the library from a directory (typically "symbols/" next to the project)
    void Initialize(const std::string& libraryDir);

    /// Re-scan the library directory for symbols not yet in the catalog
    void ScanLibrary();

    /// Persist catalog to symbols/catalog.json
    void SaveCatalog();

    /// Load catalog from symbols/catalog.json
    void LoadCatalog();

    // --- Symbol CRUD ---

    /// Create a symbol from an entity hierarchy. Returns the new symbol ID.
    std::string CreateSymbolFromEntity(ECS::World* world, ECS::Entity entity,
                                       const std::string& name, const std::string& category);

    /// Create a symbol from the current VectorDrawingEditor document. Returns the new symbol ID.
    std::string CreateSymbolFromVector(VectorDrawingEditor& editor,
                                       const std::string& name, const std::string& category);

    /// Delete a symbol and its on-disk files
    bool DeleteSymbol(const std::string& symbolId);

    /// Rename a symbol (updates catalog, not filesystem ID)
    bool RenameSymbol(const std::string& symbolId, const std::string& newName);

    // --- Instantiation ---

    /// Instantiate a symbol into a world at the given position. Returns the root entity.
    ECS::Entity InstantiateSymbol(ECS::World* world, const std::string& symbolId,
                                  const Math::Vector3& position);

    // --- Update propagation ---

    /// Overwrite a symbol's prefab from the given entity hierarchy
    void UpdateSymbolFromEntity(ECS::World* world, ECS::Entity entity,
                                const std::string& symbolId);

    /// Re-apply the symbol's prefab data to all PrefabInstanceComponent entities that reference it
    void PropagateToInstances(ECS::World* world, const std::string& symbolId);

    // --- Nested editing ---

    /// Enter nested edit mode for a symbol. Returns the edit context (or nullptr on failure).
    SymbolEditContext* BeginEditSymbol(const std::string& symbolId);

    /// Exit nested edit mode. If saveChanges is true, the symbol's prefab is updated on disk.
    void EndEditSymbol(bool saveChanges);

    /// Is the library currently in nested edit mode?
    bool IsEditingSymbol() const { return m_IsEditing; }

    /// Get the current edit context (nullptr if not editing)
    const SymbolEditContext* GetEditContext() const { return m_IsEditing ? &m_EditContext : nullptr; }

    // --- Query ---

    /// Get all symbols in the catalog
    const std::vector<SymbolEntry>& GetAllSymbols() const { return m_Symbols; }

    /// Fuzzy search symbols by name and tags
    std::vector<const SymbolEntry*> SearchSymbols(const std::string& query) const;

    /// Get symbols in a specific category
    std::vector<const SymbolEntry*> GetByCategory(const std::string& category) const;

    /// Get a sorted list of all unique categories
    std::vector<std::string> GetCategories() const;

    /// Find a symbol by ID (nullptr if not found)
    const SymbolEntry* FindSymbol(const std::string& symbolId) const;

    // --- ImGui panels ---

    /// Draw the symbol browser panel (grid/list of symbols with search + category tabs)
    void DrawBrowserPanel();

    /// Draw the symbol edit viewport (when in nested edit mode)
    void DrawEditViewport();

    // --- Flash Timeline integration ---

    /// Sync the catalog into a FlashTimelineData::symbolLibrary map (name -> asset path)
    void SyncToTimeline(FlashTimelineData& timeline);

private:
    std::string m_LibraryDir;
    std::vector<SymbolEntry> m_Symbols;
    SymbolEditContext m_EditContext;
    bool m_IsEditing = false;

    // Browser UI state
    std::string m_SearchQuery;
    std::string m_SelectedCategory;   // "" = All
    std::string m_SelectedSymbolId;
    bool m_ShowGrid = true;           // true = grid view, false = list view
    char m_SearchBuffer[256] = {};

    // Rename dialog state
    bool m_ShowRenameDialog = false;
    char m_RenameBuffer[256] = {};
    std::string m_RenameTargetId;

    // Create dialog state
    bool m_ShowCreateDialog = false;
    char m_CreateNameBuffer[256] = {};
    char m_CreateCategoryBuffer[128] = {};

    // --- Internal helpers ---

    /// Generate a filesystem-safe symbol ID from a display name
    std::string GenerateSymbolId(const std::string& name);

    /// Generate a placeholder thumbnail for a symbol (returns the thumbnail path)
    std::string GenerateThumbnail(const std::string& symbolId);

    /// Fuzzy score a candidate string against a query (higher = better match, 0 = no match)
    i32 FuzzyScore(const std::string& text, const std::string& query) const;

    /// Get a mutable pointer to a symbol entry by ID
    SymbolEntry* FindSymbolMutable(const std::string& symbolId);
};

} // namespace Editor
} // namespace Enjin
