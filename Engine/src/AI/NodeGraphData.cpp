// NodeGraphData: the DATA layer behind behavior trees, visual scripts, quest
// flows and state machines. Deliberately free of ImGui so it compiles on every
// platform — the editor canvas that draws it lives in Editor/NodeGraph.cpp.
//
// This used to live only in that editor file, with a partial copy in
// AI/NodeGraphStub.cpp for web whose ToJson returned empty arrays and whose
// FromJson called Clear(). The web build linked the stub, so loading a scene
// silently erased every graph in it while the systems that consume them kept
// ticking and reporting healthy counts. One implementation now, everywhere.

#include "Enjin/Editor/NodeGraph.h"

#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

namespace Enjin {
namespace Editor {

NodeId NodeGraphData::AddNode(const std::string& title, Math::Vector2 position,
                               Math::Vector3 headerColor) {
    GraphNode node;
    node.id = m_NextNodeId++;
    node.title = title;
    node.position = position;
    node.headerColor = headerColor;
    m_Nodes.push_back(std::move(node));
    return m_Nodes.back().id;
}

void NodeGraphData::RemoveNode(NodeId id) {
    // Remove all links connected to this node's pins
    auto* node = FindNode(id);
    if (!node) return;

    std::vector<PinId> pinIds;
    for (auto& p : node->inputs) pinIds.push_back(p.id);
    for (auto& p : node->outputs) pinIds.push_back(p.id);

    m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(),
        [&](const GraphLink& l) {
            for (auto pid : pinIds) {
                if (l.startPinId == pid || l.endPinId == pid) return true;
            }
            return false;
        }), m_Links.end());

    m_Nodes.erase(std::remove_if(m_Nodes.begin(), m_Nodes.end(),
        [id](const GraphNode& n) { return n.id == id; }), m_Nodes.end());
}

GraphNode* NodeGraphData::FindNode(NodeId id) {
    for (auto& n : m_Nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

const GraphNode* NodeGraphData::FindNode(NodeId id) const {
    for (auto& n : m_Nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

PinId NodeGraphData::AddPin(NodeId nodeId, const std::string& name,
                             PinType type, PinKind kind) {
    auto* node = FindNode(nodeId);
    if (!node) return 0;

    Pin pin;
    pin.id = m_NextPinId++;
    pin.name = name;
    pin.type = type;
    pin.kind = kind;
    pin.nodeId = nodeId;

    if (kind == PinKind::Input)
        node->inputs.push_back(pin);
    else
        node->outputs.push_back(pin);

    return pin.id;
}

Pin* NodeGraphData::FindPin(PinId id) {
    for (auto& n : m_Nodes) {
        for (auto& p : n.inputs) if (p.id == id) return &p;
        for (auto& p : n.outputs) if (p.id == id) return &p;
    }
    return nullptr;
}

const Pin* NodeGraphData::FindPin(PinId id) const {
    for (auto& n : m_Nodes) {
        for (auto& p : n.inputs) if (p.id == id) return &p;
        for (auto& p : n.outputs) if (p.id == id) return &p;
    }
    return nullptr;
}

NodeId NodeGraphData::GetPinOwner(PinId id) const {
    auto* pin = FindPin(id);
    return pin ? pin->nodeId : 0;
}

LinkId NodeGraphData::AddLink(PinId startPin, PinId endPin, u64 userData) {
    GraphLink link;
    link.id = m_NextLinkId++;
    link.startPinId = startPin;
    link.endPinId = endPin;
    link.userData = userData;
    m_Links.push_back(link);
    return link.id;
}

void NodeGraphData::RemoveLink(LinkId id) {
    m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(),
        [id](const GraphLink& l) { return l.id == id; }), m_Links.end());
}

GraphLink* NodeGraphData::FindLink(LinkId id) {
    for (auto& l : m_Links) if (l.id == id) return &l;
    return nullptr;
}

const GraphLink* NodeGraphData::FindLink(LinkId id) const {
    for (auto& l : m_Links) if (l.id == id) return &l;
    return nullptr;
}

std::vector<LinkId> NodeGraphData::GetLinksForPin(PinId pinId) const {
    std::vector<LinkId> result;
    for (auto& l : m_Links) {
        if (l.startPinId == pinId || l.endPinId == pinId)
            result.push_back(l.id);
    }
    return result;
}

std::vector<LinkId> NodeGraphData::GetLinksForNode(NodeId nodeId) const {
    auto* node = FindNode(nodeId);
    if (!node) return {};

    std::vector<PinId> pinIds;
    for (auto& p : node->inputs) pinIds.push_back(p.id);
    for (auto& p : node->outputs) pinIds.push_back(p.id);

    std::vector<LinkId> result;
    for (auto& l : m_Links) {
        for (auto pid : pinIds) {
            if (l.startPinId == pid || l.endPinId == pid) {
                result.push_back(l.id);
                break;
            }
        }
    }
    return result;
}

bool NodeGraphData::HasLinkBetween(PinId a, PinId b) const {
    for (auto& l : m_Links) {
        if ((l.startPinId == a && l.endPinId == b) ||
            (l.startPinId == b && l.endPinId == a))
            return true;
    }
    return false;
}

json NodeGraphData::ToJson() const {
    json j;
    j["nextNodeId"] = m_NextNodeId;
    j["nextPinId"]  = m_NextPinId;
    j["nextLinkId"] = m_NextLinkId;

    json nodes = json::array();
    for (auto& n : m_Nodes) {
        json jn;
        jn["id"] = n.id;
        jn["title"] = n.title;
        jn["position"] = { n.position.x, n.position.y };
        jn["headerColor"] = { n.headerColor.x, n.headerColor.y, n.headerColor.z };
        jn["flags"] = static_cast<u32>(n.flags);
        jn["userData"] = n.userData;

        json inputs = json::array();
        for (auto& p : n.inputs) {
            inputs.push_back({
                {"id", p.id}, {"name", p.name},
                {"type", static_cast<i32>(p.type)}, {"kind", static_cast<i32>(p.kind)},
                {"nodeId", p.nodeId}
            });
        }
        jn["inputs"] = inputs;

        json outputs = json::array();
        for (auto& p : n.outputs) {
            outputs.push_back({
                {"id", p.id}, {"name", p.name},
                {"type", static_cast<i32>(p.type)}, {"kind", static_cast<i32>(p.kind)},
                {"nodeId", p.nodeId}
            });
        }
        jn["outputs"] = outputs;
        nodes.push_back(jn);
    }
    j["nodes"] = nodes;

    json links = json::array();
    for (auto& l : m_Links) {
        links.push_back({
            {"id", l.id}, {"startPinId", l.startPinId},
            {"endPinId", l.endPinId}, {"userData", l.userData}
        });
    }
    j["links"] = links;
    return j;
}

void NodeGraphData::FromJson(const json& j) {
    Clear();
    if (j.contains("nextNodeId")) m_NextNodeId = j["nextNodeId"].get<u32>();
    if (j.contains("nextPinId"))  m_NextPinId  = j["nextPinId"].get<u32>();
    if (j.contains("nextLinkId")) m_NextLinkId = j["nextLinkId"].get<u32>();

    if (j.contains("nodes") && j["nodes"].is_array()) {
        for (auto& jn : j["nodes"]) {
            GraphNode node;
            node.id = jn.value("id", 0u);
            node.title = jn.value("title", std::string());
            if (jn.contains("position") && jn["position"].is_array() && jn["position"].size() >= 2) {
                node.position.x = jn["position"][0].get<f32>();
                node.position.y = jn["position"][1].get<f32>();
            }
            if (jn.contains("headerColor") && jn["headerColor"].is_array() && jn["headerColor"].size() >= 3) {
                node.headerColor.x = jn["headerColor"][0].get<f32>();
                node.headerColor.y = jn["headerColor"][1].get<f32>();
                node.headerColor.z = jn["headerColor"][2].get<f32>();
            }
            node.flags = static_cast<NodeFlags>(jn.value("flags", 0u));
            node.userData = jn.value("userData", (u64)0);

            auto deserializePins = [](const json& arr, std::vector<Pin>& pins) {
                if (!arr.is_array()) return;
                for (auto& jp : arr) {
                    Pin p;
                    p.id = jp.value("id", 0u);
                    p.name = jp.value("name", std::string());
                    p.type = static_cast<PinType>(jp.value("type", 0));
                    p.kind = static_cast<PinKind>(jp.value("kind", 0));
                    p.nodeId = jp.value("nodeId", 0u);
                    pins.push_back(p);
                }
            };
            if (jn.contains("inputs")) deserializePins(jn["inputs"], node.inputs);
            if (jn.contains("outputs")) deserializePins(jn["outputs"], node.outputs);
            m_Nodes.push_back(std::move(node));
        }
    }

    if (j.contains("links") && j["links"].is_array()) {
        for (auto& jl : j["links"]) {
            GraphLink link;
            link.id = jl.value("id", 0u);
            link.startPinId = jl.value("startPinId", 0u);
            link.endPinId = jl.value("endPinId", 0u);
            link.userData = jl.value("userData", (u64)0);
            m_Links.push_back(link);
        }
    }
}

void NodeGraphData::Clear() {
    m_Nodes.clear();
    m_Links.clear();
    m_NextNodeId = 1;
    m_NextPinId = 1;
    m_NextLinkId = 1;
}
} // namespace Editor
} // namespace Enjin
