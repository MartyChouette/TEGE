// What the Asset Browser does with a file, and the file operations it lacked
// (GR-9).
//
// Double-click did something for five kinds of file and nothing for the rest,
// treated every .json as a scene (a localization table or a meta.json opened
// as one and replaced the world), and auditioned audio only in the list view,
// because the grid and the list each carried their own copy of the dispatch.
// Both views now call OpenAssetFromBrowser. The browser also could not create,
// rename or delete anything.

#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/AssetClassify.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Platform/Paths.h"
#include "Enjin/Platform/Desktop.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <cstring>

namespace Enjin {
namespace Editor {

namespace fs = std::filesystem;

namespace {

std::string LowerExt(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

}  // namespace

void EditorLayer::OpenAssetFromBrowser(const std::string& path) {
    const fs::path p(path);
    const std::string ext = LowerExt(p);

    if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj" || ext == ".dae" || ext == ".3ds") {
        ImportModel(path);
    } else if (ext == ".enjin" || (ext == ".json" && JsonLooksLikeScene(p))) {
        RequestOpenScene(path);
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
        OpenTextureInPixelEditor(path);
    } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac") {
        AuditionSound(path);
    } else if (ext == ".enjprefab") {
        // Into the scene, in front of the camera, as the Entity menu's Prefab
        // submenu does
        if (!m_World) return;
        auto prefab = Assets::PrefabManager::Get().LoadPrefab(path);
        if (!prefab) { ShowNotification("Could not load " + p.filename().string(), NotificationType::Error); return; }
        const ECS::Entity root = Assets::PrefabManager::Get().Instantiate(m_World, *prefab, EntitySpawnPosition());
        if (root != ECS::INVALID_ENTITY) FinishMenuEntity(root);
    } else if (ext == ".enjdata" || ext == ".enjschema") {
        // The Data Asset Editor, on this record or schema. Files are saved as
        // <name>.enjdata / <name>.enjschema, so the name is the stem.
        SetPanelVisibility(EditorPanel::DataAssets, true);
        const std::string name = p.stem().string();
        auto& registry = Assets::DataAssetRegistry::Get();
        if (ext == ".enjschema") {
            m_SelectedSchemaName = name;
            m_SelectedAssetName.clear();
        } else if (const Assets::DataAsset* asset = registry.FindAsset(name)) {
            m_SelectedSchemaName = asset->schemaName;
            m_SelectedAssetName = asset->name;
        }
    } else if (ext == ".enjinproject") {
        RequestOpenProject(path);
    } else if (ext == ".enjshader") {
        m_ShaderGraphEditor.SetGraph(&m_ShaderGraphData);
        if (m_ShaderGraphEditor.Open(path)) m_ShaderGraphEditor.SetOpen(true);
        else ShowNotification("Could not read " + p.filename().string(), NotificationType::Error);
    } else if (ext == ".tegereplay") {
        PlayReplayFile(path);
    } else if (ext == ".yarn" || ext == ".twee" || ext == ".tw") {
        // Dialogue lives on an entity; these are files to import into one
        SetPanelVisibility(EditorPanel::Dialogue, true);
        ShowNotification("Select the entity with the dialogue, then use Import in the Dialogue Editor",
                         NotificationType::Info);
    } else if (ext == ".as" || ext == ".angelscript" || ext == ".vert" || ext == ".frag" ||
               ext == ".glsl" || ext == ".comp" || ext == ".json" || ext == ".txt" || ext == ".md" ||
               ext == ".csv" || ext == ".srt") {
        OpenInExternalIDE(path);
    } else {
        // Anything else: whatever the system opens it with, which is at least
        // something rather than nothing
        if (!Platform::OpenInDesktop(path))
            ShowNotification("Nothing here opens " + p.filename().string(), NotificationType::Info);
    }
}

void EditorLayer::DrawAssetFileOps(const std::string& path) {
    ImGui::Separator();
    if (ImGui::MenuItem("Rename...")) {
        m_AssetRenamePath = path;
        const std::string name = fs::path(path).filename().string();
        std::snprintf(m_AssetRenameBuf, sizeof(m_AssetRenameBuf), "%s", name.c_str());
        m_AssetOpenRenamePopup = true;
    }
    if (ImGui::MenuItem("Delete...")) {
        m_AssetDeletePath = path;
        m_AssetOpenDeletePopup = true;
    }
}

void EditorLayer::DrawAssetBrowserBackgroundMenu() {
    // Right-click on empty space in the browser
    if (!ImGui::BeginPopupContextWindow("##AssetBrowserBg",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) return;
    const fs::path dir(m_AssetBrowserPath);
    auto freeName = [&](const std::string& stem, const std::string& ext) {
        fs::path candidate = dir / (stem + ext);
        std::error_code ec;
        for (int n = 2; fs::exists(candidate, ec); ++n)
            candidate = dir / (stem + std::to_string(n) + ext);
        return candidate;
    };
    if (ImGui::MenuItem("New Folder")) {
        const fs::path folder = freeName("New Folder", "");
        std::error_code ec;
        if (fs::create_directory(folder, ec)) {
            m_AssetBrowserCacheDirty = true;
            m_AssetBrowserSelected = folder.string();
        } else {
            ShowNotification("Could not create a folder here: " + ec.message(), NotificationType::Error);
        }
    }
    if (ImGui::MenuItem("New Script")) {
        const fs::path file = freeName("NewScript", ".as");
        const std::string cls = file.stem().string();
        std::ofstream out(file);
        if (out) {
            // The same starting point as the Script component's Empty template
            out << "class " << cls << " : TegeBehavior {\n"
                << "    void OnCreate() {\n"
                << "        // Called when the entity is created\n"
                << "    }\n\n"
                << "    void OnUpdate(float dt) {\n"
                << "        // Called every frame\n"
                << "    }\n\n"
                << "    void OnDestroy() {\n"
                << "        // Called when the entity is destroyed\n"
                << "    }\n"
                << "}\n";
            m_AssetBrowserCacheDirty = true;
            m_AssetBrowserSelected = file.string();
        } else {
            ShowNotification("Could not create a script here", NotificationType::Error);
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Show in Explorer")) Platform::RevealInFileManager(m_AssetBrowserPath);
    ImGui::EndPopup();
}

void EditorLayer::DrawAssetOpsPopups() {
    if (m_AssetOpenRenamePopup) { ImGui::OpenPopup("Rename Asset"); m_AssetOpenRenamePopup = false; }
    if (m_AssetOpenDeletePopup) { ImGui::OpenPopup("Delete Asset"); m_AssetOpenDeletePopup = false; }

    if (ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter = ImGui::InputText("##rename", m_AssetRenameBuf, sizeof(m_AssetRenameBuf),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        const std::string newName = m_AssetRenameBuf;
        const bool valid = Platform::IsSafeFileName(newName);
        if (!valid) ImGui::TextDisabled("Not a usable file name");
        ImGui::BeginDisabled(!valid);
        if (ImGui::Button("Rename") || (enter && valid)) {
            const fs::path from(m_AssetRenamePath);
            const fs::path to = from.parent_path() / newName;
            std::error_code ec;
            if (to == from) {
                ImGui::CloseCurrentPopup();
            } else if (fs::exists(to, ec)) {
                ShowNotification(newName + " already exists here", NotificationType::Warning);
            } else {
                fs::rename(from, to, ec);
                if (ec) {
                    ShowNotification("Could not rename: " + ec.message(), NotificationType::Error);
                } else {
                    // A rename is a move: the open scene and the project's
                    // scene list follow it the same way
                    OnAssetMoved(from.string(), to.string());
                    m_AssetBrowserSelected = to.string();
                    m_AssetBrowserCacheDirty = true;
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const fs::path target(m_AssetDeletePath);
        std::error_code ec;
        const bool isOpenScene = !m_CurrentScenePath.empty() &&
            fs::weakly_canonical(target, ec) == fs::weakly_canonical(m_CurrentScenePath, ec);
        ImGui::Text("Move %s to the Recycle Bin?", target.filename().string().c_str());
        if (isOpenScene) ImGui::TextDisabled("This is the open scene. Open another scene first.");
        ImGui::BeginDisabled(isOpenScene);
        if (ImGui::Button("Delete")) {
            // To the trash, so it can be undone from there
            if (Platform::MoveToTrash(target.string())) {
                if (m_AssetBrowserSelected == target.string()) m_AssetBrowserSelected.clear();
                m_AssetBrowserCacheDirty = true;
            } else {
                ShowNotification("Could not move it to the Recycle Bin", NotificationType::Error);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace Editor
} // namespace Enjin
