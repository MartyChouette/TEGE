#include "Enjin/Editor/FlashAuthoring.h"
#include "Enjin/Editor/PlayMode.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Editor {

// ============================================================================
// LIFECYCLE
// ============================================================================

void FlashAuthoringMode::Initialize(ECS::World* world, SymbolLibrary* symbolLibrary) {
    m_SymbolLibrary = symbolLibrary;

    // Create default scene if empty
    if (m_Scenes.empty()) {
        NewDocument();
    }

    ENJIN_LOG_INFO(Editor, "FlashAuthoringMode: Initialized");
}

void FlashAuthoringMode::Shutdown() {
    m_Scenes.clear();
    m_SymbolLibrary = nullptr;
    m_Open = false;
    m_TestingMovie = false;
    ENJIN_LOG_INFO(Editor, "FlashAuthoringMode: Shutdown");
}

// ============================================================================
// DOCUMENT & STAGE
// ============================================================================

void FlashAuthoringMode::NewDocument() {
    m_Scenes.clear();

    FlashScene defaultScene;
    defaultScene.name = "Scene 1";
    defaultScene.timeline.name = "Scene 1";
    defaultScene.timeline.totalFrames = m_StageConfig.totalFrames;
    defaultScene.timeline.frameRate = m_StageConfig.frameRate;
    defaultScene.timeline.stageWidth = m_StageConfig.width;
    defaultScene.timeline.stageHeight = m_StageConfig.height;
    m_Scenes.push_back(defaultScene);
    m_CurrentSceneIndex = 0;

    // Apply stage config to HTML5/NG publish configs
    m_HTML5Config.title = m_StageConfig.title;
    m_HTML5Config.width = static_cast<u32>(m_StageConfig.width);
    m_HTML5Config.height = static_cast<u32>(m_StageConfig.height);
    m_NGConfig.title = m_StageConfig.title;
    m_NGConfig.canvasWidth = static_cast<u32>(m_StageConfig.width);
    m_NGConfig.canvasHeight = static_cast<u32>(m_StageConfig.height);
}

void FlashAuthoringMode::CreateStageEntity(ECS::World* world) {
    if (!world) return;

    m_StageEntity = world->CreateEntity();
    world->AddComponent<ECS::NameComponent>(m_StageEntity, ECS::NameComponent{"__FlashStage__"});

    ECS::TransformComponent transform;
    transform.position = Math::Vector3(0, 0, 0);
    transform.scale = Math::Vector3(m_StageConfig.width, m_StageConfig.height, 1.0f);
    world->AddComponent<ECS::TransformComponent>(m_StageEntity, transform);

    ENJIN_LOG_INFO(Editor, "FlashAuthoringMode: Created stage entity (%gx%g)",
                   m_StageConfig.width, m_StageConfig.height);
}

// ============================================================================
// SCENE MANAGEMENT
// ============================================================================

void FlashAuthoringMode::AddScene(const std::string& name) {
    FlashScene scene;
    scene.name = name;
    scene.timeline.name = name;
    scene.timeline.totalFrames = m_StageConfig.totalFrames;
    scene.timeline.frameRate = m_StageConfig.frameRate;
    scene.timeline.stageWidth = m_StageConfig.width;
    scene.timeline.stageHeight = m_StageConfig.height;
    m_Scenes.push_back(scene);
}

void FlashAuthoringMode::RemoveScene(u32 index) {
    if (index >= static_cast<u32>(m_Scenes.size()) || m_Scenes.size() <= 1) return;
    m_Scenes.erase(m_Scenes.begin() + index);
    if (m_CurrentSceneIndex >= static_cast<u32>(m_Scenes.size())) {
        m_CurrentSceneIndex = static_cast<u32>(m_Scenes.size()) - 1;
    }
}

void FlashAuthoringMode::SwitchToScene(u32 index) {
    if (index >= static_cast<u32>(m_Scenes.size())) return;
    m_CurrentSceneIndex = index;
}

// ============================================================================
// TEST MOVIE
// ============================================================================

void FlashAuthoringMode::TestMovie(PlayMode* playMode) {
    if (!playMode || m_TestingMovie) return;

    playMode->Play();
    m_TestingMovie = true;
    ENJIN_LOG_INFO(Editor, "FlashAuthoringMode: Test Movie started");
}

void FlashAuthoringMode::StopTestMovie(PlayMode* playMode) {
    if (!playMode || !m_TestingMovie) return;

    playMode->Stop();
    m_TestingMovie = false;
    ENJIN_LOG_INFO(Editor, "FlashAuthoringMode: Test Movie stopped");
}

// ============================================================================
// PUBLISH
// ============================================================================

bool FlashAuthoringMode::PublishHTML5(const std::string& outputDir) {
    m_HTML5Config.outputDir = outputDir;
    m_HTML5Config.title = m_StageConfig.title;
    m_HTML5Config.width = static_cast<u32>(m_StageConfig.width);
    m_HTML5Config.height = static_cast<u32>(m_StageConfig.height);

    // Convert background color to hex
    u8 r = static_cast<u8>(m_StageConfig.backgroundColor.x * 255.0f);
    u8 g = static_cast<u8>(m_StageConfig.backgroundColor.y * 255.0f);
    u8 b = static_cast<u8>(m_StageConfig.backgroundColor.z * 255.0f);
    char hexBuf[8];
    snprintf(hexBuf, sizeof(hexBuf), "#%02x%02x%02x", r, g, b);
    m_HTML5Config.backgroundColor = hexBuf;

    // Use a default BuildConfig for the exporter
    Build::BuildConfig buildConfig;
    buildConfig.windowTitle = m_StageConfig.title;
    buildConfig.windowWidth = static_cast<u32>(m_StageConfig.width);
    buildConfig.windowHeight = static_cast<u32>(m_StageConfig.height);

    auto result = Build::HTML5Exporter::Export(m_HTML5Config, buildConfig);
    if (result.success) {
        m_LastPublishResult = "Published to: " + result.outputPath;
        ENJIN_LOG_INFO(Editor, "FlashAuthoringMode: HTML5 publish succeeded -> %s", result.outputPath.c_str());
    } else {
        m_LastPublishResult = "Publish failed: " + result.error;
        ENJIN_LOG_ERROR(Editor, "FlashAuthoringMode: HTML5 publish failed: %s", result.error.c_str());
    }
    return result.success;
}

bool FlashAuthoringMode::PublishNewgrounds(const std::string& outputDir) {
    m_NGConfig.outputDir = outputDir;
    m_NGConfig.title = m_StageConfig.title;
    m_NGConfig.canvasWidth = static_cast<u32>(m_StageConfig.width);
    m_NGConfig.canvasHeight = static_cast<u32>(m_StageConfig.height);
    m_NGConfig.ngAppId = m_StageConfig.ngAppId;
    m_NGConfig.ngEncryptionKey = m_StageConfig.ngEncryptionKey;

    auto result = Build::NewgroundsGamePage::Generate(m_NGConfig);
    if (result.success) {
        m_LastPublishResult = "Published to: " + result.indexPath;
        ENJIN_LOG_INFO(Editor, "FlashAuthoringMode: Newgrounds publish succeeded -> %s", result.indexPath.c_str());
    } else {
        m_LastPublishResult = "Publish failed: " + result.error;
        ENJIN_LOG_ERROR(Editor, "FlashAuthoringMode: Newgrounds publish failed: %s", result.error.c_str());
    }
    return result.success;
}

// ============================================================================
// MAIN PANEL RENDER
// ============================================================================

void FlashAuthoringMode::DrawFlashAuthoringPanel(
    ECS::World* world, const EditorSettings& settings,
    FlashTimelineEditor& timelineEditor,
    VectorDrawingEditor& vectorEditor,
    PlayMode* playMode) {

    if (!m_Open) return;

    ImGui::SetNextWindowSize(ImVec2(1100, 750), ImGuiCond_FirstUseEver);
    bool open = m_Open;
    if (!ImGui::Begin("Flash Authoring", &open)) {
        m_Open = open;
        ImGui::End();
        return;
    }
    m_Open = open;

    // Top toolbar
    DrawToolbar();

    // Test Movie controls
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    if (m_TestingMovie) {
        if (ImGui::Button("Stop")) StopTestMovie(playMode);
    } else {
        if (ImGui::Button("Test Movie")) TestMovie(playMode);
    }
    ImGui::SameLine();
    if (ImGui::Button("Publish...")) m_ShowPublishSettings = !m_ShowPublishSettings;

    ImGui::Separator();

    // Main layout: [Library | Stage | Properties]
    f32 availW = ImGui::GetContentRegionAvail().x;
    f32 availH = ImGui::GetContentRegionAvail().y;
    f32 libraryWidth = 180.0f;
    f32 propertiesWidth = 200.0f;
    f32 stageWidth = availW - libraryWidth - propertiesWidth - 16.0f;
    f32 timelineHeight = 200.0f;
    f32 stageHeight = availH - timelineHeight - 8.0f;

    if (stageWidth < 200.0f) stageWidth = 200.0f;
    if (stageHeight < 150.0f) stageHeight = 150.0f;

    // --- Left column: Library ---
    ImGui::BeginChild("##flash_library", ImVec2(libraryWidth, stageHeight), true);
    DrawLibraryPanel(m_SymbolLibrary, world);
    ImGui::EndChild();

    ImGui::SameLine();

    // --- Center column: Stage viewport ---
    ImGui::BeginChild("##flash_stage", ImVec2(stageWidth, stageHeight), true);
    DrawStageViewport(world, vectorEditor);
    ImGui::EndChild();

    ImGui::SameLine();

    // --- Right column: Properties ---
    ImGui::BeginChild("##flash_properties", ImVec2(propertiesWidth, stageHeight), true);
    DrawPropertiesPanel();
    ImGui::EndChild();

    // --- Bottom: Timeline ---
    ImGui::BeginChild("##flash_timeline", ImVec2(-1, timelineHeight), true);

    // Scene selector tabs
    DrawSceneList();
    ImGui::Separator();

    // Wire the timeline editor to the current scene's timeline
    if (m_CurrentSceneIndex < static_cast<u32>(m_Scenes.size())) {
        timelineEditor.SetTimeline(&m_Scenes[m_CurrentSceneIndex].timeline);
        timelineEditor.Render(world, settings);
    }
    ImGui::EndChild();

    // Publish settings popup
    if (m_ShowPublishSettings) {
        DrawPublishSettings();
    }

    ImGui::End();
}

// ============================================================================
// TOOLBAR
// ============================================================================

void FlashAuthoringMode::DrawToolbar() {
    struct ToolDef {
        FlashAuthoringTool tool;
        const char* label;
        const char* tooltip;
    };

    static const ToolDef tools[] = {
        { FlashAuthoringTool::Selection,    "V", "Selection Tool (V)" },
        { FlashAuthoringTool::FreeTransform,"Q", "Free Transform (Q)" },
        { FlashAuthoringTool::Line,         "N", "Line Tool (N)" },
        { FlashAuthoringTool::Rectangle,    "R", "Rectangle Tool (R)" },
        { FlashAuthoringTool::Oval,         "O", "Oval Tool (O)" },
        { FlashAuthoringTool::Pencil,       "Y", "Pencil Tool (Y)" },
        { FlashAuthoringTool::Pen,          "P", "Pen Tool (P)" },
        { FlashAuthoringTool::Text,         "T", "Text Tool (T)" },
        { FlashAuthoringTool::Brush,        "B", "Brush Tool (B)" },
        { FlashAuthoringTool::Eraser,       "E", "Eraser Tool (E)" },
        { FlashAuthoringTool::Fill,         "K", "Fill Tool (K)" },
    };

    for (const auto& td : tools) {
        bool selected = (m_CurrentTool == td.tool);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
        }
        if (ImGui::Button(td.label, ImVec2(28, 28))) {
            m_CurrentTool = td.tool;
        }
        if (selected) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", td.tooltip);
        }
        ImGui::SameLine();
    }
}

// ============================================================================
// STAGE VIEWPORT
// ============================================================================

void FlashAuthoringMode::DrawStageViewport(ECS::World* world, VectorDrawingEditor& vectorEditor) {
    ImGui::Text("Stage (%gx%g @ %g fps)", m_StageConfig.width, m_StageConfig.height, m_StageConfig.frameRate);
    ImGui::Separator();

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Calculate stage rect centered in the viewport area
    f32 scaleX = canvasSize.x / m_StageConfig.width;
    f32 scaleY = canvasSize.y / m_StageConfig.height;
    f32 scale = std::min(scaleX, scaleY) * m_StageZoom;

    f32 stageW = m_StageConfig.width * scale;
    f32 stageH = m_StageConfig.height * scale;
    f32 ox = canvasPos.x + (canvasSize.x - stageW) * 0.5f + m_StagePanX;
    f32 oy = canvasPos.y + (canvasSize.y - stageH) * 0.5f + m_StagePanY;

    // Draw stage background (checkerboard behind, then solid color)
    ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(
        m_StageConfig.backgroundColor.x,
        m_StageConfig.backgroundColor.y,
        m_StageConfig.backgroundColor.z, 1.0f));

    // Stage shadow
    dl->AddRectFilled(ImVec2(ox + 3, oy + 3), ImVec2(ox + stageW + 3, oy + stageH + 3),
                      IM_COL32(0, 0, 0, 60));

    // Stage rect
    dl->AddRectFilled(ImVec2(ox, oy), ImVec2(ox + stageW, oy + stageH), bgColor);
    dl->AddRect(ImVec2(ox, oy), ImVec2(ox + stageW, oy + stageH), IM_COL32(100, 100, 100, 200));

    // Draw stage content (entities from current scene's timeline layers)
    if (world && m_CurrentSceneIndex < static_cast<u32>(m_Scenes.size())) {
        const auto& timeline = m_Scenes[m_CurrentSceneIndex].timeline;
        for (const auto& layer : timeline.layers) {
            if (!layer.visible || layer.isGuide) continue;
            if (layer.entity == 0) continue;

            auto* transform = world->GetComponent<ECS::TransformComponent>(layer.entity);
            if (!transform) continue;

            // Draw a placeholder rect for each entity on the stage
            f32 ex = ox + transform->position.x * scale;
            f32 ey = oy + transform->position.y * scale;
            f32 ew = transform->scale.x * scale;
            f32 eh = transform->scale.y * scale;

            dl->AddRectFilled(ImVec2(ex, ey), ImVec2(ex + ew, ey + eh),
                              IM_COL32(100, 150, 255, 120));
            dl->AddRect(ImVec2(ex, ey), ImVec2(ex + ew, ey + eh),
                        IM_COL32(60, 100, 200, 200));
        }
    }

    // Handle zoom with mouse wheel
    if (ImGui::IsWindowHovered()) {
        f32 wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_StageZoom += wheel * 0.1f;
            m_StageZoom = std::max(0.1f, std::min(5.0f, m_StageZoom));
        }

        // Handle pan with middle mouse
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_StagePanX += delta.x;
            m_StagePanY += delta.y;
        }
    }

    // Invisible button for interaction area
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("##stage_interact", canvasSize);
}

// ============================================================================
// PROPERTIES PANEL
// ============================================================================

void FlashAuthoringMode::DrawPropertiesPanel() {
    ImGui::Text("Properties");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Document", ImGuiTreeNodeFlags_DefaultOpen)) {
        char titleBuf[128];
        strncpy(titleBuf, m_StageConfig.title.c_str(), sizeof(titleBuf) - 1);
        titleBuf[sizeof(titleBuf) - 1] = '\0';
        if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
            m_StageConfig.title = titleBuf;
        }
    }

    if (ImGui::CollapsingHeader("Stage", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Width", &m_StageConfig.width, 1.0f, 100.0f, 4096.0f);
        ImGui::DragFloat("Height", &m_StageConfig.height, 1.0f, 100.0f, 4096.0f);
        ImGui::DragFloat("FPS", &m_StageConfig.frameRate, 0.5f, 1.0f, 120.0f);
        ImGui::DragInt("Frames", reinterpret_cast<int*>(&m_StageConfig.totalFrames), 1.0f, 1, 9999);

        f32 bgCol[3] = { m_StageConfig.backgroundColor.x,
                         m_StageConfig.backgroundColor.y,
                         m_StageConfig.backgroundColor.z };
        if (ImGui::ColorEdit3("Background", bgCol)) {
            m_StageConfig.backgroundColor = Math::Vector3(bgCol[0], bgCol[1], bgCol[2]);
        }
    }

    if (ImGui::CollapsingHeader("Tool Options")) {
        const char* toolNames[] = {
            "Selection", "Free Transform", "Line", "Rectangle", "Oval",
            "Pencil", "Pen", "Text", "Brush", "Eraser", "Fill"
        };
        i32 toolIdx = static_cast<i32>(m_CurrentTool);
        ImGui::Text("Active: %s", toolNames[toolIdx]);

        // Zoom control
        ImGui::SliderFloat("Zoom", &m_StageZoom, 0.1f, 5.0f, "%.1fx");
        if (ImGui::Button("Reset View")) {
            m_StageZoom = 1.0f;
            m_StagePanX = 0.0f;
            m_StagePanY = 0.0f;
        }
    }
}

// ============================================================================
// LIBRARY PANEL
// ============================================================================

void FlashAuthoringMode::DrawLibraryPanel(SymbolLibrary* symbolLibrary, ECS::World* world) {
    ImGui::Text("Library");
    ImGui::Separator();

    ImGui::InputTextWithHint("##libsearch", "Search...", m_LibrarySearchBuf, sizeof(m_LibrarySearchBuf));

    if (!symbolLibrary) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No symbol library");
        return;
    }

    const auto& symbols = symbolLibrary->GetAllSymbols();
    std::string query(m_LibrarySearchBuf);

    for (const auto& sym : symbols) {
        // Simple search filter
        if (!query.empty()) {
            bool found = false;
            std::string nameLower = sym.name;
            std::string queryLower = query;
            // Case-insensitive search
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);
            if (nameLower.find(queryLower) != std::string::npos) found = true;
            if (!found) continue;
        }

        bool selected = (sym.id == m_SelectedSymbolId);
        if (ImGui::Selectable(sym.name.c_str(), selected)) {
            m_SelectedSymbolId = sym.id;
        }

        // Double-click to instantiate on stage
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (world) {
                symbolLibrary->InstantiateSymbol(world, sym.id, Math::Vector3(
                    m_StageConfig.width * 0.5f,
                    m_StageConfig.height * 0.5f, 0.0f));
                ENJIN_LOG_INFO(Editor, "FlashAuthoring: Instantiated symbol '%s' on stage", sym.name.c_str());
            }
        }

        // Tooltip with info
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", sym.name.c_str());
            ImGui::Text("Category: %s", sym.category.c_str());
            ImGui::Text("Uses: %u", sym.useCount);
            ImGui::EndTooltip();
        }
    }
}

// ============================================================================
// SCENE LIST (tabs at top of timeline)
// ============================================================================

void FlashAuthoringMode::DrawSceneList() {
    for (u32 i = 0; i < static_cast<u32>(m_Scenes.size()); i++) {
        if (i > 0) ImGui::SameLine();
        bool selected = (i == m_CurrentSceneIndex);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }
        if (ImGui::Button(m_Scenes[i].name.c_str())) {
            SwitchToScene(i);
        }
        if (selected) {
            ImGui::PopStyleColor();
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem(("##scene_ctx_" + std::to_string(i)).c_str())) {
            if (ImGui::MenuItem("Rename")) {
                // Inline rename would require additional state; skip for now
            }
            if (m_Scenes.size() > 1 && ImGui::MenuItem("Delete")) {
                RemoveScene(i);
            }
            ImGui::EndPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("+##addscene")) {
        AddScene("Scene " + std::to_string(m_Scenes.size() + 1));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add Scene");
}

// ============================================================================
// PUBLISH SETTINGS POPUP
// ============================================================================

void FlashAuthoringMode::DrawPublishSettings() {
    ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Publish Settings", &m_ShowPublishSettings)) {
        ImGui::Text("Publish Target");
        ImGui::Separator();

        ImGui::Checkbox("HTML5", &m_StageConfig.publishHTML5);
        ImGui::Checkbox("Newgrounds", &m_StageConfig.publishNewgrounds);

        if (m_StageConfig.publishNewgrounds) {
            ImGui::Separator();
            ImGui::Text("Newgrounds Settings");

            char appIdBuf[64];
            strncpy(appIdBuf, m_StageConfig.ngAppId.c_str(), sizeof(appIdBuf) - 1);
            appIdBuf[sizeof(appIdBuf) - 1] = '\0';
            if (ImGui::InputText("App ID", appIdBuf, sizeof(appIdBuf))) {
                m_StageConfig.ngAppId = appIdBuf;
            }

            char keyBuf[128];
            strncpy(keyBuf, m_StageConfig.ngEncryptionKey.c_str(), sizeof(keyBuf) - 1);
            keyBuf[sizeof(keyBuf) - 1] = '\0';
            if (ImGui::InputText("Encryption Key", keyBuf, sizeof(keyBuf))) {
                m_StageConfig.ngEncryptionKey = keyBuf;
            }
        }

        ImGui::Separator();

        char outputBuf[512];
        strncpy(outputBuf, m_StageConfig.outputDir.c_str(), sizeof(outputBuf) - 1);
        outputBuf[sizeof(outputBuf) - 1] = '\0';
        if (ImGui::InputText("Output Dir", outputBuf, sizeof(outputBuf))) {
            m_StageConfig.outputDir = outputBuf;
        }

        ImGui::Separator();

        if (ImGui::Button("Publish", ImVec2(-1, 0))) {
            if (m_StageConfig.publishHTML5) {
                PublishHTML5(m_StageConfig.outputDir);
            }
            if (m_StageConfig.publishNewgrounds) {
                PublishNewgrounds(m_StageConfig.outputDir);
            }
        }

        if (!m_LastPublishResult.empty()) {
            ImGui::TextWrapped("%s", m_LastPublishResult.c_str());
        }
    }
    ImGui::End();
}

} // namespace Editor
} // namespace Enjin
