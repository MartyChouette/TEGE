#pragma once

// Editor-side audio event graph — node-based UI for designing audio events.
// Runtime data structures and executor live in Enjin/Audio/AudioEventGraph.h.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Audio/AudioEventGraph.h"
#include "Enjin/Math/Vector.h"
#include <string>

namespace Enjin {
namespace Editor {

// Backward compatibility — re-export runtime types into Editor namespace so
// existing code (Player, ScriptBindings, VisualScript) compiles unchanged.
// New code should use Audio::AudioEventGraphRuntime directly.
using AudioNodeType = Audio::AudioNodeType;
using AudioGraphNode = Audio::AudioGraphNode;
using AudioGraphLink = Audio::AudioGraphLink;
using AudioEventGraphData = Audio::AudioEventGraphData;
using AudioEventGraphRuntime = Audio::AudioEventGraphRuntime;

// Editor-only: ImGui node graph UI for designing audio event graphs.
class ENJIN_API AudioEventGraphEditor {
public:
    void Render();
    void SetGraph(AudioEventGraphData* graph);
    bool IsOpen() const { return m_Open; }
    void SetOpen(bool open) { m_Open = open; }

    bool Save(const std::string& path) const;
    bool Load(const std::string& path);

private:
    AudioEventGraphData* m_Graph = nullptr;
    bool m_Open = false;
    u32 m_SelectedNodeId = 0;
    Math::Vector2 m_ScrollOffset;
    f32 m_Zoom = 1.0f;

    void DrawNode(AudioGraphNode& node);
    void DrawConnections();
    void DrawInspector();
    void DrawContextMenu();
    static const char* GetNodeName(AudioNodeType type);
};

} // namespace Editor
} // namespace Enjin
