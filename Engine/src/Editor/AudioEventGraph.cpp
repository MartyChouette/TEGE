#include "Enjin/Editor/AudioEventGraph.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>

namespace Enjin {
namespace Editor {

// Category colors for audio node types
static const ImU32 COLOR_TRIGGER    = IM_COL32(60, 160, 60, 255);   // Green
static const ImU32 COLOR_SOURCE     = IM_COL32(60, 120, 180, 255);  // Blue
static const ImU32 COLOR_PROCESSING = IM_COL32(160, 120, 50, 255);  // Orange
static const ImU32 COLOR_MIXING     = IM_COL32(140, 60, 140, 255);  // Purple
static const ImU32 COLOR_OUT        = IM_COL32(160, 50, 50, 255);   // Red

static const f32 NODE_WIDTH  = 160.0f;
static const f32 NODE_HEADER = 28.0f;
static const f32 NODE_BODY   = 50.0f;
static const f32 PIN_RADIUS  = 5.0f;

static ImU32 GetAudioNodeColor(AudioNodeType type) {
    switch (type) {
        case AudioNodeType::EventTrigger:
        case AudioNodeType::ParameterTrigger:
            return COLOR_TRIGGER;

        case AudioNodeType::SoundClip:
        case AudioNodeType::RandomClip:
        case AudioNodeType::SequenceClip:
            return COLOR_SOURCE;

        case AudioNodeType::Volume:
        case AudioNodeType::Pitch:
        case AudioNodeType::Pan:
        case AudioNodeType::Delay:
        case AudioNodeType::LowPass:
        case AudioNodeType::HighPass:
            return COLOR_PROCESSING;

        case AudioNodeType::Mixer:
        case AudioNodeType::Crossfade:
            return COLOR_MIXING;

        case AudioNodeType::MasterOutput:
        case AudioNodeType::BusOutput:
            return COLOR_OUT;

        default:
            return IM_COL32(100, 100, 100, 255);
    }
}

// ============================================================================
// Node Names
// ============================================================================

const char* AudioEventGraphEditor::GetNodeName(AudioNodeType type) {
    switch (type) {
        case AudioNodeType::EventTrigger:     return "Event Trigger";
        case AudioNodeType::ParameterTrigger: return "Parameter Trigger";
        case AudioNodeType::SoundClip:        return "Sound Clip";
        case AudioNodeType::RandomClip:       return "Random Clip";
        case AudioNodeType::SequenceClip:     return "Sequence Clip";
        case AudioNodeType::Volume:           return "Volume";
        case AudioNodeType::Pitch:            return "Pitch";
        case AudioNodeType::Pan:              return "Pan";
        case AudioNodeType::Delay:            return "Delay";
        case AudioNodeType::LowPass:          return "Low Pass";
        case AudioNodeType::HighPass:         return "High Pass";
        case AudioNodeType::Mixer:            return "Mixer";
        case AudioNodeType::Crossfade:        return "Crossfade";
        case AudioNodeType::MasterOutput:     return "Master Output";
        case AudioNodeType::BusOutput:        return "Bus Output";
        default:                              return "Unknown";
    }
}

// ============================================================================
// Graph Management
// ============================================================================

void AudioEventGraphEditor::SetGraph(AudioEventGraphData* graph) {
    m_Graph = graph;
    m_SelectedNodeId = 0;
}

// ============================================================================
// Render
// ============================================================================

void AudioEventGraphEditor::Render() {
    if (!m_Open) return;

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Audio Event Graph", &m_Open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (!m_Graph) {
        ImGui::TextDisabled("No audio event graph loaded");
        ImGui::End();
        return;
    }

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                m_Graph->nodes.clear();
                m_Graph->links.clear();
                m_Graph->nextNodeId = 1;
                m_Graph->nextLinkId = 1;
                m_SelectedNodeId = 0;
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                Save("audio_event_graph.enjaudiopkg");
            }
            if (ImGui::MenuItem("Load")) {
                Load("audio_event_graph.enjaudiopkg");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Toolbar
    ImGui::Text("Event: %s", m_Graph->name.c_str());
    ImGui::Separator();

    // Layout: canvas on left, inspector on right
    f32 inspectorWidth = 250.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    f32 canvasWidth = avail.x - inspectorWidth - 8.0f;
    if (canvasWidth < 300.0f) canvasWidth = avail.x;

    // Canvas area
    ImGui::BeginChild("##AEGCanvas", ImVec2(canvasWidth, avail.y), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background grid
    f32 gridStep = 32.0f * m_Zoom;
    for (f32 x = fmodf(m_ScrollOffset.x, gridStep); x < canvasSize.x; x += gridStep) {
        drawList->AddLine(
            ImVec2(canvasPos.x + x, canvasPos.y),
            ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
            IM_COL32(50, 50, 50, 80));
    }
    for (f32 y = fmodf(m_ScrollOffset.y, gridStep); y < canvasSize.y; y += gridStep) {
        drawList->AddLine(
            ImVec2(canvasPos.x, canvasPos.y + y),
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
            IM_COL32(50, 50, 50, 80));
    }

    // Draw connections first (behind nodes)
    DrawConnections();

    // Draw nodes
    for (auto& node : m_Graph->nodes) {
        DrawNode(node);
    }

    // Handle canvas interaction
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive()) {
        // Pan with middle mouse button
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_ScrollOffset.x += delta.x;
            m_ScrollOffset.y += delta.y;
        }

        // Zoom with scroll wheel
        f32 scroll = ImGui::GetIO().MouseWheel;
        if (scroll != 0.0f) {
            m_Zoom += scroll * 0.1f;
            if (m_Zoom < 0.25f) m_Zoom = 0.25f;
            if (m_Zoom > 3.0f) m_Zoom = 3.0f;
        }

        // Right-click context menu
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("##AEGContextMenu");
        }

        // Click to deselect
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_SelectedNodeId = 0;
        }
    }

    // Context menu popup
    DrawContextMenu();

    ImGui::EndChild();

    // Inspector sidebar
    if (canvasWidth < avail.x) {
        ImGui::SameLine();
        ImGui::BeginChild("##AEGInspector", ImVec2(inspectorWidth, avail.y), true);
        DrawInspector();
        ImGui::EndChild();
    }

    ImGui::End();
}

// ============================================================================
// Draw Node
// ============================================================================

void AudioEventGraphEditor::DrawNode(AudioGraphNode& node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    f32 nodeW = NODE_WIDTH * m_Zoom;
    f32 headerH = NODE_HEADER * m_Zoom;
    f32 bodyH = NODE_BODY * m_Zoom;
    f32 pinR = PIN_RADIUS * m_Zoom;

    ImVec2 nodePos(
        canvasPos.x + (node.position.x + m_ScrollOffset.x) * m_Zoom,
        canvasPos.y + (node.position.y + m_ScrollOffset.y) * m_Zoom
    );

    ImVec2 nodeEnd(nodePos.x + nodeW, nodePos.y + headerH + bodyH);

    ImU32 headerColor = GetAudioNodeColor(node.type);
    ImU32 bodyColor = IM_COL32(40, 40, 40, 230);
    ImU32 borderColor = (node.id == m_SelectedNodeId)
        ? IM_COL32(255, 200, 50, 255)
        : IM_COL32(80, 80, 80, 255);

    // Node body
    drawList->AddRectFilled(nodePos, nodeEnd, bodyColor, 6.0f * m_Zoom);

    // Node header
    drawList->AddRectFilled(
        nodePos,
        ImVec2(nodeEnd.x, nodePos.y + headerH),
        headerColor, 6.0f * m_Zoom, ImDrawFlags_RoundCornersTop);

    // Border
    drawList->AddRect(nodePos, nodeEnd, borderColor, 6.0f * m_Zoom, 0, 2.0f * m_Zoom);

    // Title text
    const char* name = node.label.empty() ? GetNodeName(node.type) : node.label.c_str();
    ImVec2 textPos(nodePos.x + 8.0f * m_Zoom, nodePos.y + 5.0f * m_Zoom);
    drawList->AddText(nullptr, 13.0f * m_Zoom, textPos, IM_COL32(255, 255, 255, 255), name);

    // Input pin (left side) - not for triggers
    if (node.type != AudioNodeType::EventTrigger &&
        node.type != AudioNodeType::ParameterTrigger) {
        ImVec2 inputPinPos(nodePos.x, nodePos.y + headerH + bodyH * 0.5f);
        drawList->AddCircleFilled(inputPinPos, pinR, IM_COL32(180, 220, 180, 255));
    }

    // Output pin (right side) - not for outputs
    if (node.type != AudioNodeType::MasterOutput &&
        node.type != AudioNodeType::BusOutput) {
        ImVec2 outputPinPos(nodeEnd.x, nodePos.y + headerH + bodyH * 0.5f);
        drawList->AddCircleFilled(outputPinPos, pinR, IM_COL32(180, 180, 220, 255));
    }

    // Invisible button for selection and dragging
    ImGui::SetCursorScreenPos(nodePos);
    char btnId[32];
    snprintf(btnId, sizeof(btnId), "##aegnode_%u", node.id);
    ImGui::InvisibleButton(btnId, ImVec2(nodeW, headerH + bodyH));

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_SelectedNodeId = node.id;
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        node.position.x += delta.x / m_Zoom;
        node.position.y += delta.y / m_Zoom;
    }
}

// ============================================================================
// Draw Connections
// ============================================================================

void AudioEventGraphEditor::DrawConnections() {
    if (!m_Graph) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    for (const auto& link : m_Graph->links) {
        const AudioGraphNode* fromNode = nullptr;
        const AudioGraphNode* toNode = nullptr;
        for (const auto& n : m_Graph->nodes) {
            if (n.id == link.fromNode) fromNode = &n;
            if (n.id == link.toNode) toNode = &n;
        }
        if (!fromNode || !toNode) continue;

        f32 nodeW = NODE_WIDTH * m_Zoom;
        f32 headerH = NODE_HEADER * m_Zoom;
        f32 bodyH = NODE_BODY * m_Zoom;

        // Output pin position (right side of from node)
        ImVec2 p1(
            canvasPos.x + (fromNode->position.x + m_ScrollOffset.x) * m_Zoom + nodeW,
            canvasPos.y + (fromNode->position.y + m_ScrollOffset.y) * m_Zoom + headerH + bodyH * 0.5f
        );

        // Input pin position (left side of to node)
        ImVec2 p2(
            canvasPos.x + (toNode->position.x + m_ScrollOffset.x) * m_Zoom,
            canvasPos.y + (toNode->position.y + m_ScrollOffset.y) * m_Zoom + headerH + bodyH * 0.5f
        );

        // Bezier control points
        f32 tangentLen = (p2.x - p1.x) * 0.5f;
        if (tangentLen < 50.0f * m_Zoom) tangentLen = 50.0f * m_Zoom;

        ImVec2 cp1(p1.x + tangentLen, p1.y);
        ImVec2 cp2(p2.x - tangentLen, p2.y);

        // Audio links drawn in a warm green
        drawList->AddBezierCubic(p1, cp1, cp2, p2, IM_COL32(140, 200, 140, 200), 2.0f * m_Zoom);
    }
}

// ============================================================================
// Draw Inspector
// ============================================================================

void AudioEventGraphEditor::DrawInspector() {
    ImGui::Text("Inspector");
    ImGui::Separator();

    if (!m_Graph || m_SelectedNodeId == 0) {
        ImGui::TextDisabled("Select a node to inspect");
        return;
    }

    // Find selected node
    AudioGraphNode* selected = nullptr;
    for (auto& node : m_Graph->nodes) {
        if (node.id == m_SelectedNodeId) {
            selected = &node;
            break;
        }
    }

    if (!selected) {
        ImGui::TextDisabled("Node not found");
        return;
    }

    ImGui::Text("Type: %s", GetNodeName(selected->type));
    ImGui::Text("ID: %u", selected->id);
    ImGui::Separator();

    // Label editing
    char labelBuf[128];
    strncpy(labelBuf, selected->label.c_str(), sizeof(labelBuf) - 1);
    labelBuf[sizeof(labelBuf) - 1] = '\0';
    if (ImGui::InputText("Label", labelBuf, sizeof(labelBuf))) {
        selected->label = labelBuf;
    }

    // Type-specific properties
    switch (selected->type) {
        case AudioNodeType::EventTrigger: {
            char paramBuf[128];
            strncpy(paramBuf, selected->parameterName.c_str(), sizeof(paramBuf) - 1);
            paramBuf[sizeof(paramBuf) - 1] = '\0';
            if (ImGui::InputText("Event Name", paramBuf, sizeof(paramBuf))) {
                selected->parameterName = paramBuf;
            }
            break;
        }

        case AudioNodeType::ParameterTrigger: {
            char paramBuf[128];
            strncpy(paramBuf, selected->parameterName.c_str(), sizeof(paramBuf) - 1);
            paramBuf[sizeof(paramBuf) - 1] = '\0';
            if (ImGui::InputText("Parameter", paramBuf, sizeof(paramBuf))) {
                selected->parameterName = paramBuf;
            }
            ImGui::DragFloat("Threshold", &selected->floatValue, 0.01f, 0.0f, 1.0f);
            break;
        }

        case AudioNodeType::SoundClip: {
            char pathBuf[256];
            strncpy(pathBuf, selected->audioPath.c_str(), sizeof(pathBuf) - 1);
            pathBuf[sizeof(pathBuf) - 1] = '\0';
            if (ImGui::InputText("Audio File", pathBuf, sizeof(pathBuf))) {
                selected->audioPath = pathBuf;
            }
            break;
        }

        case AudioNodeType::RandomClip:
        case AudioNodeType::SequenceClip: {
            ImGui::Text("Clips: %u", static_cast<u32>(selected->audioPaths.size()));
            for (usize i = 0; i < selected->audioPaths.size(); i++) {
                ImGui::PushID(static_cast<int>(i));
                char pathBuf[256];
                strncpy(pathBuf, selected->audioPaths[i].c_str(), sizeof(pathBuf) - 1);
                pathBuf[sizeof(pathBuf) - 1] = '\0';
                char clipLabel[32];
                snprintf(clipLabel, sizeof(clipLabel), "Clip %u", static_cast<u32>(i));
                if (ImGui::InputText(clipLabel, pathBuf, sizeof(pathBuf))) {
                    selected->audioPaths[i] = pathBuf;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    selected->audioPaths.erase(selected->audioPaths.begin() + i);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add Clip")) {
                selected->audioPaths.push_back("");
            }
            break;
        }

        case AudioNodeType::Volume:
            ImGui::DragFloat("Volume", &selected->floatValue, 0.01f, 0.0f, 2.0f);
            break;

        case AudioNodeType::Pitch:
            ImGui::DragFloat("Pitch", &selected->floatValue, 0.01f, 0.1f, 4.0f);
            break;

        case AudioNodeType::Pan:
            ImGui::DragFloat("Pan", &selected->floatValue, 0.01f, -1.0f, 1.0f);
            break;

        case AudioNodeType::Delay:
            ImGui::DragFloat("Delay (s)", &selected->floatValue, 0.01f, 0.0f, 10.0f);
            break;

        case AudioNodeType::LowPass:
        case AudioNodeType::HighPass:
            ImGui::DragFloat("Cutoff (Hz)", &selected->floatValue, 10.0f, 20.0f, 20000.0f);
            break;

        case AudioNodeType::Crossfade:
            ImGui::DragFloat("Blend", &selected->floatValue, 0.01f, 0.0f, 1.0f);
            break;

        case AudioNodeType::BusOutput: {
            char busBuf[128];
            strncpy(busBuf, selected->busName.c_str(), sizeof(busBuf) - 1);
            busBuf[sizeof(busBuf) - 1] = '\0';
            if (ImGui::InputText("Bus Name", busBuf, sizeof(busBuf))) {
                selected->busName = busBuf;
            }
            break;
        }

        case AudioNodeType::Mixer:
        case AudioNodeType::MasterOutput:
        default:
            ImGui::TextDisabled("No editable properties");
            break;
    }

    ImGui::Separator();
    ImGui::Text("Position: (%.0f, %.0f)", selected->position.x, selected->position.y);

    // Delete button
    ImGui::Spacing();
    if (selected->type != AudioNodeType::MasterOutput) {
        if (ImGui::Button("Delete Node")) {
            auto& links = m_Graph->links;
            links.erase(
                std::remove_if(links.begin(), links.end(),
                    [this](const AudioGraphLink& l) {
                        return l.fromNode == m_SelectedNodeId || l.toNode == m_SelectedNodeId;
                    }),
                links.end());

            auto& nodes = m_Graph->nodes;
            nodes.erase(
                std::remove_if(nodes.begin(), nodes.end(),
                    [this](const AudioGraphNode& n) { return n.id == m_SelectedNodeId; }),
                nodes.end());

            m_SelectedNodeId = 0;
        }
    }
}

// ============================================================================
// Draw Context Menu
// ============================================================================

void AudioEventGraphEditor::DrawContextMenu() {
    if (!ImGui::BeginPopup("##AEGContextMenu")) return;

    ImVec2 mousePos = ImGui::GetMousePosOnOpeningCurrentPopup();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    Math::Vector2 spawnPos(
        (mousePos.x - canvasPos.x) / m_Zoom - m_ScrollOffset.x,
        (mousePos.y - canvasPos.y) / m_Zoom - m_ScrollOffset.y
    );

    auto addNode = [&](AudioNodeType type) {
        AudioGraphNode node;
        node.id = m_Graph->nextNodeId++;
        node.type = type;
        node.position = spawnPos;
        node.label = GetNodeName(type);

        // Set sensible defaults
        switch (type) {
            case AudioNodeType::Volume:    node.floatValue = 1.0f; break;
            case AudioNodeType::Pitch:     node.floatValue = 1.0f; break;
            case AudioNodeType::Pan:       node.floatValue = 0.0f; break;
            case AudioNodeType::Delay:     node.floatValue = 0.5f; break;
            case AudioNodeType::LowPass:   node.floatValue = 5000.0f; break;
            case AudioNodeType::HighPass:  node.floatValue = 200.0f; break;
            case AudioNodeType::Crossfade: node.floatValue = 0.5f; break;
            default: break;
        }

        m_Graph->nodes.push_back(node);
        m_SelectedNodeId = node.id;
        ImGui::CloseCurrentPopup();
    };

    if (ImGui::BeginMenu("Triggers")) {
        if (ImGui::MenuItem("Event Trigger"))     addNode(AudioNodeType::EventTrigger);
        if (ImGui::MenuItem("Parameter Trigger")) addNode(AudioNodeType::ParameterTrigger);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Sources")) {
        if (ImGui::MenuItem("Sound Clip"))    addNode(AudioNodeType::SoundClip);
        if (ImGui::MenuItem("Random Clip"))   addNode(AudioNodeType::RandomClip);
        if (ImGui::MenuItem("Sequence Clip")) addNode(AudioNodeType::SequenceClip);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Processing")) {
        if (ImGui::MenuItem("Volume"))    addNode(AudioNodeType::Volume);
        if (ImGui::MenuItem("Pitch"))     addNode(AudioNodeType::Pitch);
        if (ImGui::MenuItem("Pan"))       addNode(AudioNodeType::Pan);
        if (ImGui::MenuItem("Delay"))     addNode(AudioNodeType::Delay);
        if (ImGui::MenuItem("Low Pass"))  addNode(AudioNodeType::LowPass);
        if (ImGui::MenuItem("High Pass")) addNode(AudioNodeType::HighPass);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Mixing")) {
        if (ImGui::MenuItem("Mixer"))     addNode(AudioNodeType::Mixer);
        if (ImGui::MenuItem("Crossfade")) addNode(AudioNodeType::Crossfade);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Output")) {
        if (ImGui::MenuItem("Master Output")) addNode(AudioNodeType::MasterOutput);
        if (ImGui::MenuItem("Bus Output"))    addNode(AudioNodeType::BusOutput);
        ImGui::EndMenu();
    }

    ImGui::EndPopup();
}

// ============================================================================
// Save / Load
// ============================================================================

bool AudioEventGraphEditor::Save(const std::string& path) const {
    if (!m_Graph) return false;

    nlohmann::json j;
    j["name"] = m_Graph->name;
    j["nextNodeId"] = m_Graph->nextNodeId;
    j["nextLinkId"] = m_Graph->nextLinkId;

    auto& jNodes = j["nodes"];
    jNodes = nlohmann::json::array();
    for (const auto& node : m_Graph->nodes) {
        nlohmann::json jn;
        jn["id"] = node.id;
        jn["type"] = static_cast<u8>(node.type);
        jn["position"] = { node.position.x, node.position.y };
        jn["label"] = node.label;
        jn["audioPath"] = node.audioPath;
        jn["audioPaths"] = node.audioPaths;
        jn["floatValue"] = node.floatValue;
        jn["minValue"] = node.minValue;
        jn["maxValue"] = node.maxValue;
        jn["parameterName"] = node.parameterName;
        jn["busName"] = node.busName;
        jNodes.push_back(jn);
    }

    auto& jLinks = j["links"];
    jLinks = nlohmann::json::array();
    for (const auto& link : m_Graph->links) {
        nlohmann::json jl;
        jl["id"] = link.id;
        jl["fromNode"] = link.fromNode;
        jl["fromPin"] = link.fromPin;
        jl["toNode"] = link.toNode;
        jl["toPin"] = link.toPin;
        jLinks.push_back(jl);
    }

    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;
    ofs << j.dump(2);
    return ofs.good();
}

bool AudioEventGraphEditor::Load(const std::string& path) {
    if (!m_Graph) return false;

    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    nlohmann::json j;
    try {
        ifs >> j;
    } catch (...) {
        return false;
    }

    m_Graph->nodes.clear();
    m_Graph->links.clear();
    m_SelectedNodeId = 0;

    if (j.contains("name")) m_Graph->name = j["name"].get<std::string>();
    if (j.contains("nextNodeId")) m_Graph->nextNodeId = j["nextNodeId"].get<u32>();
    if (j.contains("nextLinkId")) m_Graph->nextLinkId = j["nextLinkId"].get<u32>();

    if (j.contains("nodes") && j["nodes"].is_array()) {
        for (const auto& jn : j["nodes"]) {
            AudioGraphNode node;
            if (jn.contains("id")) node.id = jn["id"].get<u32>();
            if (jn.contains("type")) {
                u8 t = jn["type"].get<u8>();
                if (t <= static_cast<u8>(AudioNodeType::BusOutput))
                    node.type = static_cast<AudioNodeType>(t);
            }
            if (jn.contains("position") && jn["position"].is_array() && jn["position"].size() >= 2) {
                node.position.x = jn["position"][0].get<f32>();
                node.position.y = jn["position"][1].get<f32>();
            }
            if (jn.contains("label")) node.label = jn["label"].get<std::string>();
            if (jn.contains("audioPath")) node.audioPath = jn["audioPath"].get<std::string>();
            if (jn.contains("audioPaths") && jn["audioPaths"].is_array()) {
                for (const auto& ap : jn["audioPaths"]) {
                    node.audioPaths.push_back(ap.get<std::string>());
                }
            }
            if (jn.contains("floatValue")) node.floatValue = jn["floatValue"].get<f32>();
            if (jn.contains("minValue")) node.minValue = jn["minValue"].get<f32>();
            if (jn.contains("maxValue")) node.maxValue = jn["maxValue"].get<f32>();
            if (jn.contains("parameterName")) node.parameterName = jn["parameterName"].get<std::string>();
            if (jn.contains("busName")) node.busName = jn["busName"].get<std::string>();
            m_Graph->nodes.push_back(node);
        }
    }

    if (j.contains("links") && j["links"].is_array()) {
        for (const auto& jl : j["links"]) {
            AudioGraphLink link;
            if (jl.contains("id")) link.id = jl["id"].get<u32>();
            if (jl.contains("fromNode")) link.fromNode = jl["fromNode"].get<u32>();
            if (jl.contains("fromPin")) link.fromPin = jl["fromPin"].get<u32>();
            if (jl.contains("toNode")) link.toNode = jl["toNode"].get<u32>();
            if (jl.contains("toPin")) link.toPin = jl["toPin"].get<u32>();
            m_Graph->links.push_back(link);
        }
    }

    return true;
}

// AudioEventGraphRuntime implementation moved to Engine/src/Audio/AudioEventGraphRuntime.cpp
// (runtime code has no ImGui/editor dependency)

} // namespace Editor
} // namespace Enjin
