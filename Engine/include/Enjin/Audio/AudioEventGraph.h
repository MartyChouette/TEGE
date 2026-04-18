#pragma once

// Runtime audio event graph data structures and executor.
// These are engine-level (not editor-level) — used by Player, VisualScript, and ScriptBindings.
// The editor-only graph node UI lives in Enjin/Editor/AudioEventGraph.h.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Enjin {

namespace Audio {
    class SimpleAudio;
}

namespace Audio {

enum class AudioNodeType : u8 {
    // Triggers
    EventTrigger,
    ParameterTrigger,

    // Sources
    SoundClip,
    RandomClip,
    SequenceClip,

    // Processing
    Volume, Pitch, Pan, Delay, LowPass, HighPass,

    // Mixing
    Mixer,
    Crossfade,

    // Output
    MasterOutput,
    BusOutput
};

struct AudioGraphNode {
    u32 id = 0;
    AudioNodeType type = AudioNodeType::SoundClip;
    Math::Vector2 position;
    std::string label;

    // Properties
    std::string audioPath;
    std::vector<std::string> audioPaths;
    f32 floatValue = 1.0f;
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

// Runtime execution of audio event graphs — no editor/ImGui dependency.
// Walks the graph from trigger nodes, applies processing chain, plays sounds.
class ENJIN_API AudioEventGraphRuntime {
public:
    void Initialize(SimpleAudio* audio);
    void Shutdown();

    void SetGraph(const AudioEventGraphData* graph);
    void TriggerEvent(const std::string& name);
    void SetParameter(const std::string& name, f32 value);
    f32 GetParameter(const std::string& name) const;
    void StopAll();
    void Update(f32 deltaTime);

private:
    SimpleAudio* m_Audio = nullptr;
    const AudioEventGraphData* m_Graph = nullptr;

    std::unordered_map<std::string, f32> m_Parameters;

    struct DelayedSound {
        u32 clip;
        f32 volume;
        f32 pitch;
        f32 pan;
        f32 remainingDelay;
    };
    std::vector<DelayedSound> m_DelayedSounds;
    std::unordered_map<u32, u32> m_SequenceIndices;
    std::vector<u32> m_ActiveSounds;

    void ExecuteFromNode(u32 nodeId);
    std::vector<u32> GetConnectedNodes(u32 fromNodeId) const;
    u32 GetInputNode(u32 toNodeId) const;

    struct AudioChainResult {
        u32 clip = 0;
        f32 volume = 1.0f;
        f32 pitch = 1.0f;
        f32 pan = 0.0f;
        f32 delay = 0.0f;
        std::string busName;
    };
    AudioChainResult ResolveChain(u32 nodeId);
};

} // namespace Audio
} // namespace Enjin
