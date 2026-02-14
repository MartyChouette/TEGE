#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Editor/EditorSettings.h"
#include "Enjin/Editor/FlashTimeline.h"
#include "Enjin/Editor/VectorDrawingEditor.h"
#include "Enjin/Editor/SymbolLibrary.h"
#include "Enjin/Build/HTML5Exporter.h"
#include "Enjin/Build/NewgroundsGamePage.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace Enjin {

// Forward declarations
namespace Editor {
    class PlayMode;
}

namespace Editor {

// ============================================================================
// Flash Authoring Tool
// ============================================================================

// The active drawing tool in Flash authoring mode
enum class FlashAuthoringTool : u8 {
    Selection,      // Arrow selection tool (V)
    FreeTransform,  // Free transform (Q)
    Line,           // Line tool (N)
    Rectangle,      // Rectangle tool (R)
    Oval,           // Oval / Ellipse tool (O)
    Pencil,         // Freehand pencil (Y)
    Pen,            // Bezier pen tool (P)
    Text,           // Text tool (T)
    Brush,          // Paint brush (B)
    Eraser,         // Eraser (E)
    Fill            // Paint bucket fill (K)
};

// Stage properties (equivalent to Flash's Document Properties)
struct StageConfig {
    f32 width = 550.0f;            // Stage width in pixels
    f32 height = 400.0f;           // Stage height in pixels
    f32 frameRate = 24.0f;         // Frames per second
    Math::Vector3 backgroundColor{1.0f, 1.0f, 1.0f};  // White default
    std::string title = "Untitled";
    u32 totalFrames = 60;

    // Publish settings
    std::string outputDir;
    bool publishHTML5 = true;
    bool publishNewgrounds = false;
    std::string ngAppId;
    std::string ngEncryptionKey;
};

// Scene entry in the Flash authoring mode (scenes are like top-level timeline sections)
struct FlashScene {
    std::string name = "Scene 1";
    FlashTimelineData timeline;
};

// ============================================================================
// FlashAuthoringMode — Integration hub for Flash-style workflow
// ============================================================================

/// The FlashAuthoringMode class ties together the existing VectorDrawingEditor,
/// FlashTimelineEditor, SymbolLibrary, HTML5Exporter, and NewgroundsGamePage
/// systems into a cohesive Flash/Animate-like authoring experience.
///
/// It manages:
///   - A stage concept (root entity with fixed dimensions and background color)
///   - A library panel (integration with SymbolLibrary)
///   - Drawing tools (integration with VectorDrawingEditor)
///   - Timeline (integration with FlashTimelineEditor)
///   - Scene management (multiple scenes as sections of the timeline)
///   - Test Movie (plays the stage content via PlayMode)
///   - Publish (exports via HTML5Exporter + NewgroundsGamePage)
///   - Properties panel (stage size, FPS, background, publish settings)
class ENJIN_API FlashAuthoringMode {
public:
    FlashAuthoringMode() = default;
    ~FlashAuthoringMode() = default;

    // --- Lifecycle ---

    /// Initialize the authoring mode. Call once after editor systems are ready.
    void Initialize(ECS::World* world, SymbolLibrary* symbolLibrary);

    /// Shutdown and clean up
    void Shutdown();

    /// Is authoring mode open?
    bool IsOpen() const { return m_Open; }
    void SetOpen(bool open) { m_Open = open; }

    // --- Main render ---

    /// Draw the complete Flash authoring panel (toolbar, stage viewport, timeline, library)
    void DrawFlashAuthoringPanel(ECS::World* world, const EditorSettings& settings,
                                  FlashTimelineEditor& timelineEditor,
                                  VectorDrawingEditor& vectorEditor,
                                  PlayMode* playMode);

    // --- Stage management ---

    /// Create a new Flash document with default stage settings
    void NewDocument();

    /// Get/set the stage configuration
    const StageConfig& GetStageConfig() const { return m_StageConfig; }
    StageConfig& GetStageConfig() { return m_StageConfig; }

    /// Create the stage root entity in the world
    void CreateStageEntity(ECS::World* world);

    // --- Scene management ---

    /// Add a new scene
    void AddScene(const std::string& name);

    /// Remove a scene by index
    void RemoveScene(u32 index);

    /// Switch to a scene by index
    void SwitchToScene(u32 index);

    /// Get the current scene index
    u32 GetCurrentSceneIndex() const { return m_CurrentSceneIndex; }

    /// Get all scenes
    const std::vector<FlashScene>& GetScenes() const { return m_Scenes; }

    // --- Test Movie ---

    /// Start test movie (enters play mode with stage content)
    void TestMovie(PlayMode* playMode);

    /// Stop test movie
    void StopTestMovie(PlayMode* playMode);

    /// Is test movie running?
    bool IsTestingMovie() const { return m_TestingMovie; }

    // --- Publish ---

    /// Publish as HTML5 (uses HTML5Exporter)
    bool PublishHTML5(const std::string& outputDir);

    /// Publish for Newgrounds (uses NewgroundsGamePage)
    bool PublishNewgrounds(const std::string& outputDir);

private:
    // --- UI drawing sub-functions ---
    void DrawToolbar();
    void DrawStageViewport(ECS::World* world, VectorDrawingEditor& vectorEditor);
    void DrawPropertiesPanel();
    void DrawLibraryPanel(SymbolLibrary* symbolLibrary, ECS::World* world);
    void DrawSceneList();
    void DrawPublishSettings();

    // --- State ---
    bool m_Open = false;
    StageConfig m_StageConfig;
    std::vector<FlashScene> m_Scenes;
    u32 m_CurrentSceneIndex = 0;
    ECS::Entity m_StageEntity = 0;

    // Tool state
    FlashAuthoringTool m_CurrentTool = FlashAuthoringTool::Selection;

    // Test movie state
    bool m_TestingMovie = false;

    // Stage viewport state
    f32 m_StageZoom = 1.0f;
    f32 m_StagePanX = 0.0f;
    f32 m_StagePanY = 0.0f;

    // Properties panel state
    bool m_ShowPublishSettings = false;

    // Library panel state
    char m_LibrarySearchBuf[256] = {};
    std::string m_SelectedSymbolId;

    // Publish state
    Build::HTML5ExportConfig m_HTML5Config;
    Build::GamePageConfig m_NGConfig;
    std::string m_LastPublishResult;

    // Symbol library reference (not owned)
    SymbolLibrary* m_SymbolLibrary = nullptr;
};

} // namespace Editor
} // namespace Enjin
