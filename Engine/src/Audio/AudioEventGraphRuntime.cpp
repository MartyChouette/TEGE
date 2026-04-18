// Audio event graph runtime — extracted from Editor/AudioEventGraph.cpp.
// No ImGui or editor dependency. Used by Player, VisualScript, and ScriptBindings.

#include "Enjin/Audio/AudioEventGraph.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Logging/Log.h"
#include <random>

namespace Enjin {
namespace Audio {

void AudioEventGraphRuntime::Initialize(SimpleAudio* audio) {
    m_Audio = audio;
    m_Graph = nullptr;
    m_Parameters.clear();
    m_DelayedSounds.clear();
    m_SequenceIndices.clear();
    m_ActiveSounds.clear();
}

void AudioEventGraphRuntime::Shutdown() {
    StopAll();
    m_Audio = nullptr;
    m_Graph = nullptr;
    m_Parameters.clear();
    m_DelayedSounds.clear();
    m_SequenceIndices.clear();
    m_ActiveSounds.clear();
}

void AudioEventGraphRuntime::SetGraph(const AudioEventGraphData* graph) {
    m_Graph = graph;
    m_SequenceIndices.clear();
}

void AudioEventGraphRuntime::TriggerEvent(const std::string& name) {
    if (!m_Graph || !m_Audio) return;

    for (const auto& node : m_Graph->nodes) {
        if (node.type == AudioNodeType::EventTrigger && node.parameterName == name) {
            ExecuteFromNode(node.id);
        }
    }
}

void AudioEventGraphRuntime::SetParameter(const std::string& name, f32 value) {
    if (!m_Graph || !m_Audio) return;

    f32 oldValue = 0.0f;
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end()) {
        oldValue = it->second;
    }
    m_Parameters[name] = value;

    for (const auto& node : m_Graph->nodes) {
        if (node.type == AudioNodeType::ParameterTrigger && node.parameterName == name) {
            f32 threshold = node.floatValue;
            if (oldValue < threshold && value >= threshold) {
                ExecuteFromNode(node.id);
            }
        }
    }
}

f32 AudioEventGraphRuntime::GetParameter(const std::string& name) const {
    auto it = m_Parameters.find(name);
    return (it != m_Parameters.end()) ? it->second : 0.0f;
}

void AudioEventGraphRuntime::StopAll() {
    if (!m_Audio) return;
    for (auto handle : m_ActiveSounds) {
        m_Audio->Stop(handle);
    }
    m_ActiveSounds.clear();
    m_DelayedSounds.clear();
}

void AudioEventGraphRuntime::Update(f32 deltaTime) {
    if (!m_Audio) return;

    for (auto it = m_DelayedSounds.begin(); it != m_DelayedSounds.end(); ) {
        it->remainingDelay -= deltaTime;
        if (it->remainingDelay <= 0.0f) {
            Audio::SoundHandle handle = m_Audio->Play(
                static_cast<Audio::AudioClipHandle>(it->clip),
                it->volume, it->pitch, false);
            if (handle != Audio::INVALID_SOUND) {
                m_ActiveSounds.push_back(handle);
            }
            it = m_DelayedSounds.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_ActiveSounds.begin(); it != m_ActiveSounds.end(); ) {
        if (!m_Audio->IsPlaying(*it)) {
            it = m_ActiveSounds.erase(it);
        } else {
            ++it;
        }
    }
}

void AudioEventGraphRuntime::ExecuteFromNode(u32 nodeId) {
    if (!m_Graph || !m_Audio) return;

    static constexpr u32 kMaxExecutionDepth = 16;
    static thread_local u32 s_ExecutionDepth = 0;
    if (s_ExecutionDepth >= kMaxExecutionDepth) return;
    s_ExecutionDepth++;
    struct DepthGuard { ~DepthGuard() { s_ExecutionDepth--; } } depthGuard;

    std::vector<u32> connected = GetConnectedNodes(nodeId);
    for (u32 connId : connected) {
        const AudioGraphNode* connNode = nullptr;
        for (const auto& n : m_Graph->nodes) {
            if (n.id == connId) { connNode = &n; break; }
        }
        if (!connNode) continue;

        if (connNode->type == AudioNodeType::SoundClip ||
            connNode->type == AudioNodeType::RandomClip ||
            connNode->type == AudioNodeType::SequenceClip) {

            AudioChainResult result = ResolveChain(connId);
            if (result.clip == 0) continue;

            if (result.delay > 0.0f) {
                DelayedSound ds;
                ds.clip = result.clip;
                ds.volume = result.volume;
                ds.pitch = result.pitch;
                ds.pan = result.pan;
                ds.remainingDelay = result.delay;
                m_DelayedSounds.push_back(ds);
            } else {
                Audio::SoundHandle handle = m_Audio->Play(
                    static_cast<Audio::AudioClipHandle>(result.clip),
                    result.volume, result.pitch, false);
                if (handle != Audio::INVALID_SOUND) {
                    m_ActiveSounds.push_back(handle);
                }
            }
        } else {
            ExecuteFromNode(connId);
        }
    }
}

std::vector<u32> AudioEventGraphRuntime::GetConnectedNodes(u32 fromNodeId) const {
    std::vector<u32> result;
    if (!m_Graph) return result;

    for (const auto& link : m_Graph->links) {
        if (link.fromNode == fromNodeId) {
            result.push_back(link.toNode);
        }
    }
    return result;
}

u32 AudioEventGraphRuntime::GetInputNode(u32 toNodeId) const {
    if (!m_Graph) return 0;

    for (const auto& link : m_Graph->links) {
        if (link.toNode == toNodeId) {
            return link.fromNode;
        }
    }
    return 0;
}

AudioEventGraphRuntime::AudioChainResult AudioEventGraphRuntime::ResolveChain(u32 nodeId) {
    AudioChainResult result;
    if (!m_Graph || !m_Audio) return result;

    const AudioGraphNode* node = nullptr;
    for (const auto& n : m_Graph->nodes) {
        if (n.id == nodeId) { node = &n; break; }
    }
    if (!node) return result;

    switch (node->type) {
        case AudioNodeType::SoundClip: {
            if (!node->audioPath.empty()) {
                result.clip = m_Audio->LoadClip(node->audioPath);
            }
            break;
        }
        case AudioNodeType::RandomClip: {
            if (!node->audioPaths.empty()) {
                thread_local static std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<size_t> dist(0, node->audioPaths.size() - 1);
                usize idx = dist(rng);
                result.clip = m_Audio->LoadClip(node->audioPaths[idx]);
            }
            break;
        }
        case AudioNodeType::SequenceClip: {
            if (!node->audioPaths.empty()) {
                u32& seqIdx = m_SequenceIndices[nodeId];
                if (seqIdx >= static_cast<u32>(node->audioPaths.size())) {
                    seqIdx = 0;
                }
                result.clip = m_Audio->LoadClip(node->audioPaths[seqIdx]);
                seqIdx++;
            }
            break;
        }
        default:
            break;
    }

    std::vector<u32> connected = GetConnectedNodes(nodeId);
    for (u32 nextId : connected) {
        const AudioGraphNode* nextNode = nullptr;
        for (const auto& n : m_Graph->nodes) {
            if (n.id == nextId) { nextNode = &n; break; }
        }
        if (!nextNode) continue;

        switch (nextNode->type) {
            case AudioNodeType::Volume:    result.volume *= nextNode->floatValue; break;
            case AudioNodeType::Pitch:     result.pitch *= nextNode->floatValue; break;
            case AudioNodeType::Pan:       result.pan = nextNode->floatValue; break;
            case AudioNodeType::Delay:     result.delay += nextNode->floatValue; break;
            case AudioNodeType::BusOutput: result.busName = nextNode->busName; break;
            case AudioNodeType::LowPass:
            case AudioNodeType::HighPass:
            case AudioNodeType::Mixer:
            case AudioNodeType::Crossfade:
            case AudioNodeType::MasterOutput:
                break;
            default:
                break;
        }

        if (nextNode->type != AudioNodeType::MasterOutput &&
            nextNode->type != AudioNodeType::BusOutput) {
            std::vector<u32> further = GetConnectedNodes(nextId);
            for (u32 furtherId : further) {
                const AudioGraphNode* furtherNode = nullptr;
                for (const auto& n : m_Graph->nodes) {
                    if (n.id == furtherId) { furtherNode = &n; break; }
                }
                if (!furtherNode) continue;

                switch (furtherNode->type) {
                    case AudioNodeType::Volume:    result.volume *= furtherNode->floatValue; break;
                    case AudioNodeType::Pitch:     result.pitch *= furtherNode->floatValue; break;
                    case AudioNodeType::Pan:       result.pan = furtherNode->floatValue; break;
                    case AudioNodeType::Delay:     result.delay += furtherNode->floatValue; break;
                    case AudioNodeType::BusOutput: result.busName = furtherNode->busName; break;
                    default: break;
                }
            }
        }
    }

    return result;
}

} // namespace Audio
} // namespace Enjin
