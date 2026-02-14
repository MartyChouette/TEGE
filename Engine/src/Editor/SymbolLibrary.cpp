#include "Enjin/Editor/SymbolLibrary.h"
#include "Enjin/Editor/VectorDrawingEditor.h"
#include "Enjin/Editor/FlashTimeline.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <set>

namespace Enjin {
namespace Editor {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Lifecycle
// ============================================================================

SymbolLibrary::~SymbolLibrary() {
    EndEditSymbol(false);
}

void SymbolLibrary::Initialize(const std::string& libraryDir) {
    m_LibraryDir = libraryDir;

    // Ensure the library root directory exists
    std::error_code ec;
    if (!fs::exists(m_LibraryDir, ec)) {
        fs::create_directories(m_LibraryDir, ec);
        if (ec) {
            ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to create library dir '%s': %s",
                            m_LibraryDir.c_str(), ec.message().c_str());
            return;
        }
    }

    LoadCatalog();
    ScanLibrary();

    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Initialized with %zu symbols from '%s'",
                   m_Symbols.size(), m_LibraryDir.c_str());
}

// ============================================================================
// Catalog Persistence
// ============================================================================

void SymbolLibrary::SaveCatalog() {
    if (m_LibraryDir.empty()) return;

    json j;
    j["version"] = 1;
    j["symbols"] = json::array();

    for (const auto& sym : m_Symbols) {
        json sj;
        sj["id"] = sym.id;
        sj["name"] = sym.name;
        sj["category"] = sym.category;
        sj["type"] = static_cast<u32>(sym.type);
        sj["assetPath"] = sym.assetPath;
        sj["thumbnailPath"] = sym.thumbnailPath;
        sj["useCount"] = sym.useCount;
        sj["isBuiltin"] = sym.isBuiltin;

        if (!sym.tags.empty()) {
            sj["tags"] = sym.tags;
        }

        j["symbols"].push_back(sj);
    }

    std::string catalogPath = m_LibraryDir + "/catalog.json";
    std::ofstream file(catalogPath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to save catalog to '%s'", catalogPath.c_str());
        return;
    }

    file << j.dump(2);
    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Saved catalog with %zu symbols", m_Symbols.size());
}

void SymbolLibrary::LoadCatalog() {
    if (m_LibraryDir.empty()) return;

    std::string catalogPath = m_LibraryDir + "/catalog.json";
    std::ifstream file(catalogPath);
    if (!file.is_open()) {
        // No catalog yet — not an error on first run
        return;
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to parse catalog JSON: %s", e.what());
        return;
    }

    m_Symbols.clear();

    if (!j.contains("symbols") || !j["symbols"].is_array()) return;

    for (const auto& sj : j["symbols"]) {
        SymbolEntry sym;
        sym.id = sj.value("id", "");
        sym.name = sj.value("name", "");
        sym.category = sj.value("category", "");
        sym.type = static_cast<SymbolType>(sj.value("type", 0u));
        sym.assetPath = sj.value("assetPath", "");
        sym.thumbnailPath = sj.value("thumbnailPath", "");
        sym.useCount = sj.value("useCount", 0u);
        sym.isBuiltin = sj.value("isBuiltin", false);

        if (sj.contains("tags") && sj["tags"].is_array()) {
            for (const auto& tag : sj["tags"]) {
                if (tag.is_string()) {
                    sym.tags.push_back(tag.get<std::string>());
                }
            }
        }

        if (!sym.id.empty()) {
            m_Symbols.push_back(std::move(sym));
        }
    }

    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Loaded catalog with %zu symbols", m_Symbols.size());
}

void SymbolLibrary::ScanLibrary() {
    if (m_LibraryDir.empty()) return;

    std::error_code ec;
    if (!fs::exists(m_LibraryDir, ec) || !fs::is_directory(m_LibraryDir, ec)) return;

    // Build a set of known symbol IDs for fast lookup
    std::set<std::string> knownIds;
    for (const auto& sym : m_Symbols) {
        knownIds.insert(sym.id);
    }

    u32 discovered = 0;

    for (auto& entry : fs::directory_iterator(m_LibraryDir, ec)) {
        if (!entry.is_directory(ec)) continue;

        std::string dirName = entry.path().filename().string();

        // Skip if already cataloged
        if (knownIds.count(dirName)) continue;

        // Look for an asset file inside: .enjprefab or .svg
        std::string prefabPath;
        std::string svgPath;

        for (auto& child : fs::directory_iterator(entry.path(), ec)) {
            if (!child.is_regular_file(ec)) continue;
            std::string ext = child.path().extension().string();
            if (ext == ".enjprefab") {
                prefabPath = child.path().string();
            } else if (ext == ".svg") {
                svgPath = child.path().string();
            }
        }

        if (prefabPath.empty() && svgPath.empty()) continue;

        SymbolEntry sym;
        sym.id = dirName;
        sym.name = dirName;  // Default display name = folder name

        if (!prefabPath.empty()) {
            sym.type = SymbolType::EntityPrefab;
            sym.assetPath = prefabPath;
        } else {
            sym.type = SymbolType::VectorDrawing;
            sym.assetPath = svgPath;
        }

        // Check for thumbnail
        std::string thumbPath = entry.path().string() + "/thumbnail.png";
        if (fs::exists(thumbPath, ec)) {
            sym.thumbnailPath = thumbPath;
        }

        m_Symbols.push_back(std::move(sym));
        discovered++;
    }

    if (discovered > 0) {
        ENJIN_LOG_INFO(Editor, "SymbolLibrary: Discovered %u new symbols on disk", discovered);
        SaveCatalog();
    }
}

// ============================================================================
// Symbol CRUD
// ============================================================================

std::string SymbolLibrary::CreateSymbolFromEntity(ECS::World* world, ECS::Entity entity,
                                                   const std::string& name, const std::string& category) {
    if (!world || entity == ECS::INVALID_ENTITY) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Cannot create symbol from invalid entity");
        return "";
    }

    std::string symbolId = GenerateSymbolId(name);
    if (symbolId.empty()) return "";

    // Create the symbol directory
    std::string symbolDir = m_LibraryDir + "/" + symbolId;
    std::error_code ec;
    fs::create_directories(symbolDir, ec);
    if (ec) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to create symbol dir '%s': %s",
                        symbolDir.c_str(), ec.message().c_str());
        return "";
    }

    // Create prefab from entity via PrefabManager
    auto& pm = Assets::PrefabManager::Get();
    auto prefab = pm.CreateFromEntity(world, entity, name);
    if (!prefab) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to create prefab from entity");
        return "";
    }

    // Save the prefab to the symbol directory
    std::string prefabPath = symbolDir + "/" + symbolId + ".enjprefab";
    if (!pm.SavePrefab(*prefab, prefabPath)) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to save prefab to '%s'", prefabPath.c_str());
        return "";
    }

    // Create catalog entry
    SymbolEntry sym;
    sym.id = symbolId;
    sym.name = name;
    sym.category = category;
    sym.type = SymbolType::EntityPrefab;
    sym.assetPath = prefabPath;
    sym.thumbnailPath = GenerateThumbnail(symbolId);

    m_Symbols.push_back(std::move(sym));
    SaveCatalog();

    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Created entity symbol '%s' (id: %s)",
                   name.c_str(), symbolId.c_str());
    return symbolId;
}

std::string SymbolLibrary::CreateSymbolFromVector(VectorDrawingEditor& editor,
                                                   const std::string& name, const std::string& category) {
    if (!editor.HasDocument()) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Cannot create symbol from empty vector document");
        return "";
    }

    std::string symbolId = GenerateSymbolId(name);
    if (symbolId.empty()) return "";

    // Create the symbol directory
    std::string symbolDir = m_LibraryDir + "/" + symbolId;
    std::error_code ec;
    fs::create_directories(symbolDir, ec);
    if (ec) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to create symbol dir '%s': %s",
                        symbolDir.c_str(), ec.message().c_str());
        return "";
    }

    // Export SVG via VectorDrawingEditor::SaveAsSymbol
    if (!editor.SaveAsSymbol(name, symbolDir)) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to export SVG for symbol '%s'", name.c_str());
        return "";
    }

    std::string svgPath = symbolDir + "/" + name + ".svg";

    // Create catalog entry
    SymbolEntry sym;
    sym.id = symbolId;
    sym.name = name;
    sym.category = category;
    sym.type = SymbolType::VectorDrawing;
    sym.assetPath = svgPath;
    sym.thumbnailPath = GenerateThumbnail(symbolId);

    m_Symbols.push_back(std::move(sym));
    SaveCatalog();

    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Created vector symbol '%s' (id: %s)",
                   name.c_str(), symbolId.c_str());
    return symbolId;
}

bool SymbolLibrary::DeleteSymbol(const std::string& symbolId) {
    auto* sym = FindSymbolMutable(symbolId);
    if (!sym) {
        ENJIN_LOG_WARN(Editor, "SymbolLibrary: Cannot delete unknown symbol '%s'", symbolId.c_str());
        return false;
    }

    if (sym->isBuiltin) {
        ENJIN_LOG_WARN(Editor, "SymbolLibrary: Cannot delete built-in symbol '%s'", symbolId.c_str());
        return false;
    }

    // Remove the symbol directory from disk
    std::string symbolDir = m_LibraryDir + "/" + symbolId;
    std::error_code ec;
    if (fs::exists(symbolDir, ec)) {
        fs::remove_all(symbolDir, ec);
        if (ec) {
            ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to remove symbol dir '%s': %s",
                            symbolDir.c_str(), ec.message().c_str());
        }
    }

    // Remove from catalog vector
    m_Symbols.erase(
        std::remove_if(m_Symbols.begin(), m_Symbols.end(),
                        [&](const SymbolEntry& s) { return s.id == symbolId; }),
        m_Symbols.end());

    // Clear selection if this was the selected symbol
    if (m_SelectedSymbolId == symbolId) {
        m_SelectedSymbolId.clear();
    }

    SaveCatalog();
    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Deleted symbol '%s'", symbolId.c_str());
    return true;
}

bool SymbolLibrary::RenameSymbol(const std::string& symbolId, const std::string& newName) {
    auto* sym = FindSymbolMutable(symbolId);
    if (!sym) {
        ENJIN_LOG_WARN(Editor, "SymbolLibrary: Cannot rename unknown symbol '%s'", symbolId.c_str());
        return false;
    }

    if (newName.empty()) return false;

    sym->name = newName;
    SaveCatalog();

    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Renamed symbol '%s' to '%s'", symbolId.c_str(), newName.c_str());
    return true;
}

// ============================================================================
// Instantiation
// ============================================================================

ECS::Entity SymbolLibrary::InstantiateSymbol(ECS::World* world, const std::string& symbolId,
                                              const Math::Vector3& position) {
    if (!world) return ECS::INVALID_ENTITY;

    const auto* sym = FindSymbol(symbolId);
    if (!sym) {
        ENJIN_LOG_WARN(Editor, "SymbolLibrary: Cannot instantiate unknown symbol '%s'", symbolId.c_str());
        return ECS::INVALID_ENTITY;
    }

    if (sym->type == SymbolType::VectorDrawing) {
        // For vector symbols, create an entity with a NameComponent referencing the SVG.
        // Full SVG-to-entity conversion would go through SVGLoader; here we create a
        // placeholder entity that a renderer or import step can resolve later.
        ECS::Entity entity = world->CreateEntity();
        world->AddComponent<ECS::TransformComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) {
            transform->position = position;
        }
        world->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent(sym->name));

        // Increment use count
        auto* mutableSym = FindSymbolMutable(symbolId);
        if (mutableSym) mutableSym->useCount++;

        ENJIN_LOG_INFO(Editor, "SymbolLibrary: Instantiated vector symbol '%s'", sym->name.c_str());
        return entity;
    }

    // EntityPrefab / SpriteSheet / Custom — load via PrefabManager
    auto& pm = Assets::PrefabManager::Get();
    auto prefab = pm.LoadPrefab(sym->assetPath);
    if (!prefab) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to load prefab '%s'", sym->assetPath.c_str());
        return ECS::INVALID_ENTITY;
    }

    ECS::Entity root = pm.Instantiate(world, *prefab, position);
    if (root != ECS::INVALID_ENTITY) {
        // Ensure the PrefabInstanceComponent records the symbol's asset path
        auto* pi = world->GetComponent<Assets::PrefabInstanceComponent>(root);
        if (pi) {
            pi->prefabPath = sym->assetPath;
        }

        // Increment use count
        auto* mutableSym = FindSymbolMutable(symbolId);
        if (mutableSym) mutableSym->useCount++;
    }

    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Instantiated symbol '%s' (entity %llu)",
                   sym->name.c_str(), static_cast<unsigned long long>(root));
    return root;
}

// ============================================================================
// Update Propagation
// ============================================================================

void SymbolLibrary::UpdateSymbolFromEntity(ECS::World* world, ECS::Entity entity,
                                            const std::string& symbolId) {
    if (!world || entity == ECS::INVALID_ENTITY) return;

    auto* sym = FindSymbolMutable(symbolId);
    if (!sym) {
        ENJIN_LOG_WARN(Editor, "SymbolLibrary: Cannot update unknown symbol '%s'", symbolId.c_str());
        return;
    }

    if (sym->type != SymbolType::EntityPrefab) {
        ENJIN_LOG_WARN(Editor, "SymbolLibrary: UpdateSymbolFromEntity only supports EntityPrefab symbols");
        return;
    }

    // Recreate the prefab from the entity
    auto& pm = Assets::PrefabManager::Get();
    auto prefab = pm.CreateFromEntity(world, entity, sym->name);
    if (!prefab) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to create prefab from entity for update");
        return;
    }

    // Overwrite the asset file on disk
    if (!pm.SavePrefab(*prefab, sym->assetPath)) {
        ENJIN_LOG_ERROR(Editor, "SymbolLibrary: Failed to save updated prefab to '%s'", sym->assetPath.c_str());
        return;
    }

    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Updated symbol '%s' from entity %llu",
                   sym->name.c_str(), static_cast<unsigned long long>(entity));
}

void SymbolLibrary::PropagateToInstances(ECS::World* world, const std::string& symbolId) {
    if (!world) return;

    const auto* sym = FindSymbol(symbolId);
    if (!sym || sym->type != SymbolType::EntityPrefab) return;

    // Load the updated prefab
    auto& pm = Assets::PrefabManager::Get();
    // Clear the cached version so we pick up fresh data from disk
    auto prefab = pm.GetPrefab(sym->assetPath);
    if (!prefab) return;

    // Find all PrefabInstanceComponent entities referencing this asset path and re-apply
    u32 updateCount = 0;
    for (ECS::Entity entity : world->GetEntitiesWithComponent<Assets::PrefabInstanceComponent>()) {
        auto* pi = world->GetComponent<Assets::PrefabInstanceComponent>(entity);
        if (!pi) continue;

        // Match by prefab path
        if (pi->prefabPath != sym->assetPath) continue;

        // Re-apply the prefab's component data to this instance
        pm.ApplyPrefabToInstances(world, pi->prefabId);
        updateCount++;
    }

    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Propagated '%s' to %u instances",
                   sym->name.c_str(), updateCount);
}

// ============================================================================
// Nested Editing
// ============================================================================

SymbolEditContext* SymbolLibrary::BeginEditSymbol(const std::string& symbolId) {
    if (m_IsEditing) {
        ENJIN_LOG_WARN(Editor, "SymbolLibrary: Already editing symbol '%s', end current edit first",
                       m_EditContext.symbolId.c_str());
        return nullptr;
    }

    const auto* sym = FindSymbol(symbolId);
    if (!sym) {
        ENJIN_LOG_WARN(Editor, "SymbolLibrary: Cannot edit unknown symbol '%s'", symbolId.c_str());
        return nullptr;
    }

    if (sym->type != SymbolType::EntityPrefab) {
        ENJIN_LOG_WARN(Editor, "SymbolLibrary: Nested editing only supports EntityPrefab symbols");
        return nullptr;
    }

    // Create an isolated world for editing
    m_EditContext.editWorld = new ECS::World();
    m_EditContext.symbolId = symbolId;
    m_EditContext.isDirty = false;
    m_EditContext.breadcrumbPath = std::string("Library > ") + sym->name;

    // Load the prefab into the edit world
    auto& pm = Assets::PrefabManager::Get();
    auto prefab = pm.LoadPrefab(sym->assetPath);
    if (prefab) {
        pm.Instantiate(m_EditContext.editWorld, *prefab, Math::Vector3(0, 0, 0));
    }

    m_IsEditing = true;
    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Began editing symbol '%s'", sym->name.c_str());
    return &m_EditContext;
}

void SymbolLibrary::EndEditSymbol(bool saveChanges) {
    if (!m_IsEditing) return;

    if (saveChanges && m_EditContext.editWorld && m_EditContext.isDirty) {
        auto* sym = FindSymbolMutable(m_EditContext.symbolId);
        if (sym && sym->type == SymbolType::EntityPrefab) {
            // Find the root entity in the edit world (first entity with a TransformComponent)
            const auto& entities = m_EditContext.editWorld->GetEntitiesWithComponent<ECS::TransformComponent>();
            if (!entities.empty()) {
                ECS::Entity root = entities.front();

                auto& pm = Assets::PrefabManager::Get();
                auto prefab = pm.CreateFromEntity(m_EditContext.editWorld, root, sym->name);
                if (prefab) {
                    pm.SavePrefab(*prefab, sym->assetPath);
                    ENJIN_LOG_INFO(Editor, "SymbolLibrary: Saved changes to symbol '%s'", sym->name.c_str());
                }
            }
        }
    }

    // Clean up the isolated world
    delete m_EditContext.editWorld;
    m_EditContext.editWorld = nullptr;
    m_EditContext.symbolId.clear();
    m_EditContext.isDirty = false;
    m_EditContext.breadcrumbPath.clear();
    m_IsEditing = false;
}

// ============================================================================
// Query
// ============================================================================

std::vector<const SymbolEntry*> SymbolLibrary::SearchSymbols(const std::string& query) const {
    std::vector<const SymbolEntry*> results;
    if (query.empty()) {
        results.reserve(m_Symbols.size());
        for (const auto& sym : m_Symbols) {
            results.push_back(&sym);
        }
        return results;
    }

    // Score each symbol and collect matches
    struct ScoredEntry {
        const SymbolEntry* entry;
        i32 score;
    };
    std::vector<ScoredEntry> scored;
    scored.reserve(m_Symbols.size());

    for (const auto& sym : m_Symbols) {
        i32 bestScore = FuzzyScore(sym.name, query);

        // Also score against tags
        for (const auto& tag : sym.tags) {
            i32 tagScore = FuzzyScore(tag, query);
            if (tagScore > bestScore) bestScore = tagScore;
        }

        // Score against category
        i32 catScore = FuzzyScore(sym.category, query);
        if (catScore > bestScore) bestScore = catScore;

        if (bestScore > 0) {
            scored.push_back({&sym, bestScore});
        }
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
              [](const ScoredEntry& a, const ScoredEntry& b) { return a.score > b.score; });

    results.reserve(scored.size());
    for (const auto& s : scored) {
        results.push_back(s.entry);
    }

    return results;
}

std::vector<const SymbolEntry*> SymbolLibrary::GetByCategory(const std::string& category) const {
    std::vector<const SymbolEntry*> results;
    for (const auto& sym : m_Symbols) {
        if (sym.category == category) {
            results.push_back(&sym);
        }
    }
    return results;
}

std::vector<std::string> SymbolLibrary::GetCategories() const {
    std::set<std::string> categories;
    for (const auto& sym : m_Symbols) {
        if (!sym.category.empty()) {
            categories.insert(sym.category);
        }
    }
    return std::vector<std::string>(categories.begin(), categories.end());
}

const SymbolEntry* SymbolLibrary::FindSymbol(const std::string& symbolId) const {
    for (const auto& sym : m_Symbols) {
        if (sym.id == symbolId) return &sym;
    }
    return nullptr;
}

SymbolEntry* SymbolLibrary::FindSymbolMutable(const std::string& symbolId) {
    for (auto& sym : m_Symbols) {
        if (sym.id == symbolId) return &sym;
    }
    return nullptr;
}

// ============================================================================
// Flash Timeline Integration
// ============================================================================

void SymbolLibrary::SyncToTimeline(FlashTimelineData& timeline) {
    timeline.symbolLibrary.clear();
    for (const auto& sym : m_Symbols) {
        timeline.symbolLibrary[sym.name] = sym.assetPath;
    }
}

// ============================================================================
// ImGui: Browser Panel
// ============================================================================

void SymbolLibrary::DrawBrowserPanel() {
    // ---- Toolbar: search bar + view toggle + create button ----
    ImGui::PushItemWidth(200.0f);
    if (ImGui::InputTextWithHint("##SymSearch", "Search symbols...", m_SearchBuffer, sizeof(m_SearchBuffer))) {
        m_SearchQuery = m_SearchBuffer;
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button(m_ShowGrid ? "List" : "Grid")) {
        m_ShowGrid = !m_ShowGrid;
    }

    ImGui::SameLine();
    if (ImGui::Button("+ New Symbol")) {
        m_ShowCreateDialog = true;
        m_CreateNameBuffer[0] = '\0';
        m_CreateCategoryBuffer[0] = '\0';
    }

    ImGui::Separator();

    // ---- Category tabs ----
    auto categories = GetCategories();

    if (ImGui::BeginTabBar("SymbolCategories")) {
        // "All" tab
        if (ImGui::BeginTabItem("All")) {
            m_SelectedCategory.clear();
            ImGui::EndTabItem();
        }

        for (const auto& cat : categories) {
            if (ImGui::BeginTabItem(cat.c_str())) {
                m_SelectedCategory = cat;
                ImGui::EndTabItem();
            }
        }

        ImGui::EndTabBar();
    }

    // ---- Collect symbols to display ----
    std::vector<const SymbolEntry*> displaySymbols;
    if (!m_SearchQuery.empty()) {
        displaySymbols = SearchSymbols(m_SearchQuery);
        // Further filter by selected category
        if (!m_SelectedCategory.empty()) {
            displaySymbols.erase(
                std::remove_if(displaySymbols.begin(), displaySymbols.end(),
                               [&](const SymbolEntry* s) { return s->category != m_SelectedCategory; }),
                displaySymbols.end());
        }
    } else if (!m_SelectedCategory.empty()) {
        displaySymbols = GetByCategory(m_SelectedCategory);
    } else {
        displaySymbols.reserve(m_Symbols.size());
        for (const auto& sym : m_Symbols) {
            displaySymbols.push_back(&sym);
        }
    }

    // ---- Render symbols (grid or list) ----
    if (displaySymbols.empty()) {
        ImGui::TextDisabled("No symbols found.");
        if (m_Symbols.empty()) {
            ImGui::TextDisabled("Create a symbol from an entity or vector drawing.");
        }
    } else if (m_ShowGrid) {
        // ---- Grid view ----
        constexpr f32 CELL_SIZE = 128.0f;
        constexpr f32 CELL_PADDING = 8.0f;

        f32 panelWidth = ImGui::GetContentRegionAvail().x;
        i32 columns = std::max(1, static_cast<i32>(panelWidth / (CELL_SIZE + CELL_PADDING)));

        i32 col = 0;
        bool shouldBreak = false;

        for (const auto* sym : displaySymbols) {
            if (shouldBreak) break;
            if (col > 0) ImGui::SameLine();

            ImGui::BeginGroup();

            // Thumbnail area (placeholder colored rect)
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Background for thumbnail — highlight if selected
            ImU32 bgColor = (m_SelectedSymbolId == sym->id)
                ? IM_COL32(80, 120, 180, 200)
                : IM_COL32(50, 50, 55, 200);
            dl->AddRectFilled(cursorPos,
                              ImVec2(cursorPos.x + CELL_SIZE, cursorPos.y + CELL_SIZE),
                              bgColor, 4.0f);

            // Type indicator in top-left
            const char* typeLabel = "?";
            switch (sym->type) {
                case SymbolType::VectorDrawing: typeLabel = "[SVG]"; break;
                case SymbolType::EntityPrefab:  typeLabel = "[PRE]"; break;
                case SymbolType::SpriteSheet:   typeLabel = "[SPR]"; break;
                case SymbolType::Custom:        typeLabel = "[CUS]"; break;
            }
            dl->AddText(ImVec2(cursorPos.x + 4, cursorPos.y + 4),
                        IM_COL32(200, 200, 200, 180), typeLabel);

            // Symbol name centered
            ImVec2 textSize = ImGui::CalcTextSize(sym->name.c_str());
            f32 textX = cursorPos.x + (CELL_SIZE - textSize.x) * 0.5f;
            f32 textY = cursorPos.y + (CELL_SIZE - textSize.y) * 0.5f;
            dl->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 230), sym->name.c_str());

            // Use count badge in top-right
            if (sym->useCount > 0) {
                char useBuf[16];
                snprintf(useBuf, sizeof(useBuf), "x%u", sym->useCount);
                ImVec2 badgeSize = ImGui::CalcTextSize(useBuf);
                dl->AddText(ImVec2(cursorPos.x + CELL_SIZE - badgeSize.x - 4, cursorPos.y + 4),
                            IM_COL32(180, 180, 100, 200), useBuf);
            }

            // Invisible button covering the cell for interaction
            std::string btnId = std::string("##sym_grid_") + sym->id;
            ImGui::InvisibleButton(btnId.c_str(), ImVec2(CELL_SIZE, CELL_SIZE));

            // Single click to select
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                m_SelectedSymbolId = sym->id;
            }

            // Double-click to enter nested edit mode
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                BeginEditSymbol(sym->id);
            }

            // Right-click context menu
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Rename")) {
                    m_ShowRenameDialog = true;
                    m_RenameTargetId = sym->id;
                    snprintf(m_RenameBuffer, sizeof(m_RenameBuffer), "%s", sym->name.c_str());
                }
                if (ImGui::MenuItem("Delete", nullptr, false, !sym->isBuiltin)) {
                    // We must break after deletion because the vector is invalidated
                    std::string idCopy = sym->id;
                    ImGui::EndPopup();
                    ImGui::EndGroup();
                    DeleteSymbol(idCopy);
                    shouldBreak = true;
                    continue;
                }
                ImGui::Separator();
                ImGui::TextDisabled("Category: %s", sym->category.empty() ? "(none)" : sym->category.c_str());
                ImGui::TextDisabled("Asset: %s", sym->assetPath.c_str());
                ImGui::EndPopup();
            }

            // Hover tooltip
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", sym->name.c_str());
                if (!sym->category.empty()) ImGui::Text("Category: %s", sym->category.c_str());
                if (!sym->tags.empty()) {
                    std::string tagStr;
                    for (usize i = 0; i < sym->tags.size(); ++i) {
                        if (i > 0) tagStr += ", ";
                        tagStr += sym->tags[i];
                    }
                    ImGui::Text("Tags: %s", tagStr.c_str());
                }
                ImGui::Text("Uses: %u", sym->useCount);
                ImGui::EndTooltip();
            }

            ImGui::EndGroup();

            col++;
            if (col >= columns) col = 0;
        }
    } else {
        // ---- List view ----
        bool shouldBreak = false;

        for (const auto* sym : displaySymbols) {
            if (shouldBreak) break;

            // Type icon prefix
            const char* typePrefix = "[?] ";
            switch (sym->type) {
                case SymbolType::VectorDrawing: typePrefix = "[SVG] "; break;
                case SymbolType::EntityPrefab:  typePrefix = "[PRE] "; break;
                case SymbolType::SpriteSheet:   typePrefix = "[SPR] "; break;
                case SymbolType::Custom:        typePrefix = "[CUS] "; break;
            }

            std::string label = std::string(typePrefix) + sym->name;
            if (!sym->category.empty()) {
                label += "  (" + sym->category + ")";
            }
            if (sym->useCount > 0) {
                label += "  [x" + std::to_string(sym->useCount) + "]";
            }

            // Unique ID for ImGui
            std::string selectId = label + "##" + sym->id;
            bool isSelected = (m_SelectedSymbolId == sym->id);

            if (ImGui::Selectable(selectId.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                m_SelectedSymbolId = sym->id;

                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    BeginEditSymbol(sym->id);
                }
            }

            // Right-click context menu
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Rename")) {
                    m_ShowRenameDialog = true;
                    m_RenameTargetId = sym->id;
                    snprintf(m_RenameBuffer, sizeof(m_RenameBuffer), "%s", sym->name.c_str());
                }
                if (ImGui::MenuItem("Delete", nullptr, false, !sym->isBuiltin)) {
                    std::string idCopy = sym->id;
                    ImGui::EndPopup();
                    DeleteSymbol(idCopy);
                    shouldBreak = true;
                    continue;
                }
                ImGui::Separator();
                if (!sym->tags.empty()) {
                    std::string tagStr;
                    for (usize i = 0; i < sym->tags.size(); ++i) {
                        if (i > 0) tagStr += ", ";
                        tagStr += sym->tags[i];
                    }
                    ImGui::TextDisabled("Tags: %s", tagStr.c_str());
                }
                ImGui::TextDisabled("Asset: %s", sym->assetPath.c_str());
                ImGui::EndPopup();
            }
        }
    }

    // ---- Rename dialog ----
    if (m_ShowRenameDialog) {
        ImGui::OpenPopup("Rename Symbol");
        m_ShowRenameDialog = false;
    }

    if (ImGui::BeginPopupModal("Rename Symbol", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter new name:");
        ImGui::InputText("##RenameInput", m_RenameBuffer, sizeof(m_RenameBuffer));

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            std::string newName(m_RenameBuffer);
            if (!newName.empty()) {
                RenameSymbol(m_RenameTargetId, newName);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ---- Create dialog ----
    if (m_ShowCreateDialog) {
        ImGui::OpenPopup("New Symbol");
        m_ShowCreateDialog = false;
    }

    if (ImGui::BeginPopupModal("New Symbol", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Symbol Name:");
        ImGui::InputText("##CreateName", m_CreateNameBuffer, sizeof(m_CreateNameBuffer));

        ImGui::Text("Category:");
        ImGui::InputText("##CreateCategory", m_CreateCategoryBuffer, sizeof(m_CreateCategoryBuffer));

        ImGui::TextDisabled("Select an entity in the hierarchy, then click OK to create.");

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            // Name and category are stored in the buffers for the caller to read.
            // The caller (EditorLayer) is responsible for calling CreateSymbolFromEntity
            // or CreateSymbolFromVector with the appropriate source after this dialog closes.
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_CreateNameBuffer[0] = '\0';
            m_CreateCategoryBuffer[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ============================================================================
// ImGui: Edit Viewport
// ============================================================================

void SymbolLibrary::DrawEditViewport() {
    if (!m_IsEditing || !m_EditContext.editWorld) {
        ImGui::TextDisabled("No symbol is being edited.");
        return;
    }

    // Breadcrumb bar
    ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "%s", m_EditContext.breadcrumbPath.c_str());

    if (m_EditContext.isDirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "(unsaved)");
    }

    ImGui::Separator();

    // Entity list with inline transform editing
    const auto& entities = m_EditContext.editWorld->GetEntitiesWithComponent<ECS::TransformComponent>();
    ImGui::Text("Entities: %zu", entities.size());
    ImGui::Separator();

    for (ECS::Entity entity : entities) {
        auto* nameComp = m_EditContext.editWorld->GetComponent<ECS::NameComponent>(entity);
        std::string entityLabel = nameComp
            ? nameComp->name
            : std::string("Entity ") + std::to_string(entity);

        ImGui::PushID(static_cast<i32>(entity));

        if (ImGui::TreeNode(entityLabel.c_str())) {
            auto* transform = m_EditContext.editWorld->GetComponent<ECS::TransformComponent>(entity);
            if (transform) {
                bool changed = false;

                f32 pos[3] = {transform->position.x, transform->position.y, transform->position.z};
                if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                    transform->position = Math::Vector3(pos[0], pos[1], pos[2]);
                    changed = true;
                }

                // Display rotation as Euler angles for user-friendliness
                Math::Vector3 euler = transform->rotation.ToEuler();
                f32 rot[3] = {euler.x, euler.y, euler.z};
                if (ImGui::DragFloat3("Rotation", rot, 0.5f)) {
                    transform->rotation = Math::Quaternion::FromEuler(Math::Vector3(rot[0], rot[1], rot[2]));
                    changed = true;
                }

                f32 scl[3] = {transform->scale.x, transform->scale.y, transform->scale.z};
                if (ImGui::DragFloat3("Scale", scl, 0.01f)) {
                    transform->scale = Math::Vector3(scl[0], scl[1], scl[2]);
                    changed = true;
                }

                ImGui::Checkbox("Visible", &transform->visible);

                if (changed) {
                    m_EditContext.isDirty = true;
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::Separator();

    // Save / Discard buttons
    if (ImGui::Button("Save & Close")) {
        EndEditSymbol(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard & Close")) {
        EndEditSymbol(false);
    }
}

// ============================================================================
// Internal Helpers
// ============================================================================

std::string SymbolLibrary::GenerateSymbolId(const std::string& name) {
    if (name.empty()) return "";

    // Convert to lowercase, replace non-alphanumeric with underscore
    std::string id;
    id.reserve(name.size() + 16);

    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (c == ' ' || c == '-' || c == '.') {
            if (!id.empty() && id.back() != '_') {
                id += '_';
            }
        }
        // Other characters are dropped
    }

    // Remove trailing underscores
    while (!id.empty() && id.back() == '_') {
        id.pop_back();
    }

    if (id.empty()) {
        id = "symbol";
    }

    // Ensure uniqueness by appending a numeric suffix if the ID already exists
    std::string base = id;
    u32 suffix = 0;
    while (FindSymbol(id) != nullptr) {
        suffix++;
        id = base + "_" + std::to_string(suffix);
    }

    return id;
}

std::string SymbolLibrary::GenerateThumbnail(const std::string& symbolId) {
    // Create a placeholder thumbnail path. Actual rendering to PNG would
    // require an offscreen framebuffer pass. For now we create the path
    // so the catalog entry is valid; the rendering subsystem can populate
    // the file later when the browser requests it.
    std::string thumbPath = m_LibraryDir + "/" + symbolId + "/thumbnail.png";

    std::error_code ec;
    if (!fs::exists(thumbPath, ec)) {
        // Write an empty placeholder marker so the path is recorded
        std::ofstream marker(thumbPath, std::ios::binary);
    }

    return thumbPath;
}

i32 SymbolLibrary::FuzzyScore(const std::string& text, const std::string& query) const {
    if (query.empty()) return 1;
    if (text.empty()) return 0;

    // Case-insensitive copies
    std::string lowerText;
    lowerText.reserve(text.size());
    for (char c : text) {
        lowerText += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string lowerQuery;
    lowerQuery.reserve(query.size());
    for (char c : query) {
        lowerQuery += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Exact substring match gets the highest score
    usize subPos = lowerText.find(lowerQuery);
    if (subPos != std::string::npos) {
        // Prefix match bonus
        if (subPos == 0) {
            return 100 + static_cast<i32>(query.size());
        }
        return 50 + static_cast<i32>(query.size());
    }

    // Fuzzy character-by-character match (all query chars must appear in order)
    usize qi = 0;
    i32 score = 0;
    bool prevMatched = false;

    for (usize ti = 0; ti < lowerText.size() && qi < lowerQuery.size(); ++ti) {
        if (lowerText[ti] == lowerQuery[qi]) {
            score += prevMatched ? 3 : 1;  // Consecutive match bonus

            // Word-boundary bonus (start of string, after space/underscore/hyphen)
            if (ti == 0 || text[ti - 1] == ' ' || text[ti - 1] == '_' || text[ti - 1] == '-') {
                score += 5;
            }

            qi++;
            prevMatched = true;
        } else {
            prevMatched = false;
        }
    }

    // All query characters must have been consumed
    if (qi < lowerQuery.size()) {
        return 0;
    }

    return score;
}

} // namespace Editor
} // namespace Enjin
