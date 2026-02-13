#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Enjin {

namespace Audio {
    class SimpleAudio;
}

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

    // Save/Load graph data to/from JSON file
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

// ============================================================================
// RUNTIME EXECUTION
// ============================================================================

// Runtime execution of audio event graphs
class ENJIN_API AudioEventGraphRuntime {
public:
    void Initialize(Audio::SimpleAudio* audio);
    void Shutdown();

    // Set the active graph for execution
    void SetGraph(const AudioEventGraphData* graph);

    // Trigger an event by name
    void TriggerEvent(const std::string& name);

    // Set a parameter value (triggers ParameterTrigger nodes when crossing threshold)
    void SetParameter(const std::string& name, f32 value);
    f32 GetParameter(const std::string& name) const;

    // Stop all playing sounds from this graph
    void StopAll();

    // Update (processes delays, sequences, parameter triggers)
    void Update(f32 deltaTime);

private:
    Audio::SimpleAudio* m_Audio = nullptr;
    const AudioEventGraphData* m_Graph = nullptr;

    // Parameter state
    std::unordered_map<std::string, f32> m_Parameters;

    // Pending delayed sounds
    struct DelayedSound {
        u32 clip;     // AudioClipHandle
        f32 volume;
        f32 pitch;
        f32 pan;
        f32 remainingDelay;
    };
    std::vector<DelayedSound> m_DelayedSounds;

    // Sequence state (per SequenceClip node)
    std::unordered_map<u32, u32> m_SequenceIndices;

    // Active sound handles for StopAll
    std::vector<u32> m_ActiveSounds;  // SoundHandle

    // Walk the graph from a trigger node, accumulating processing, and play
    void ExecuteFromNode(u32 nodeId);

    // Find nodes connected to a node's output
    std::vector<u32> GetConnectedNodes(u32 fromNodeId) const;

    // Find node connected to a node's input (returns 0 if none)
    u32 GetInputNode(u32 toNodeId) const;

    // Resolve the processing chain forward from a source node
    struct AudioChainResult {
        u32 clip = 0;     // AudioClipHandle (0 = invalid)
        f32 volume = 1.0f;
        f32 pitch = 1.0f;
        f32 pan = 0.0f;
        f32 delay = 0.0f;
        std::string busName;
    };
    AudioChainResult ResolveChain(u32 nodeId);
};

} // namespace Editor
} // namespace Enjin
