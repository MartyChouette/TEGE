#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Editor {

enum class AudioNodeType : u8 {
    // Triggers
    EventTrigger,       // Start audio event
    ParameterTrigger,   // Trigger based on parameter value

    // Sources
    SoundClip,          // Play a single audio file
    RandomClip,         // Play random from list
    SequenceClip,       // Play in sequence

    // Processing
    Volume, Pitch, Pan, Delay, LowPass, HighPass,

    // Mixing
    Mixer,              // Combine multiple inputs
    Crossfade,          // Blend between two sources

    // Output
    MasterOutput,
    BusOutput           // Route to audio bus
};

struct AudioGraphNode {
    u32 id = 0;
    AudioNodeType type = AudioNodeType::SoundClip;
    Math::Vector2 position;
    std::string label;

    // Properties
    std::string audioPath;
    std::vector<std::string> audioPaths;  // For random/sequence
    f32 floatValue = 1.0f;               // Volume, pitch, etc.
    f32 minValue = 0.0f;
    f32 maxValue = 1.0f;
    std::string parameterName;
    std::string busName;
};

struct AudioGraphLink {
    u32 id = 0;
    u32 fromNode, fromPin, toNode, toPin;
};

struct AudioEventGraphData {
    std::string name = "New Audio Event";
    std::vector<AudioGraphNode> nodes;
    std::vector<AudioGraphLink> links;
    u32 nextNodeId = 1;
    u32 nextLinkId = 1;
};

class ENJIN_API AudioEventGraphEditor {
public:
    void Render();
    void SetGraph(AudioEventGraphData* graph);
    bool IsOpen() const { return m_Open; }
    void SetOpen(bool open) { m_Open = open; }

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
