// Full implementations of NodeGraphData methods for web builds.
// Editor/NodeGraph.cpp (ImGui canvas rendering) is excluded from web,
// but these data methods are needed by BehaviorTreeExecutor and VisualScriptSystem.
#if ENJIN_PLATFORM_WEB

#include "Enjin/Editor/NodeGraph.h"
#include <algorithm>

namespace Enjin {
namespace Editor {

NodeId NodeGraphData::AddNode(const std::string& title, Math::Vector2 position, Math::Vector3 headerColor) {
    GraphNode node; node.id = m_NextNodeId++; node.title = title;
    node.position = position; node.headerColor = headerColor;
    m_Nodes.push_back(node); return node.id;
}
void NodeGraphData::RemoveNode(NodeId id) {
    m_Nodes.erase(std::remove_if(m_Nodes.begin(), m_Nodes.end(),
        [id](const GraphNode& n) { return n.id == id; }), m_Nodes.end());
}
GraphNode* NodeGraphData::FindNode(NodeId id) {
    for (auto& n : m_Nodes) { if (n.id == id) return &n; } return nullptr;
}
const GraphNode* NodeGraphData::FindNode(NodeId id) const {
    for (auto& n : m_Nodes) { if (n.id == id) return &n; } return nullptr;
}
PinId NodeGraphData::AddPin(NodeId nodeId, const std::string& name, PinType type, PinKind kind) {
    auto* node = FindNode(nodeId); if (!node) return 0;
    Pin pin; pin.id = m_NextPinId++; pin.name = name; pin.type = type;
    if (kind == PinKind::Input) node->inputs.push_back(pin);
    else node->outputs.push_back(pin);
    return pin.id;
}
Pin* NodeGraphData::FindPin(PinId id) {
    for (auto& n : m_Nodes) {
        for (auto& p : n.inputs) { if (p.id == id) return &p; }
        for (auto& p : n.outputs) { if (p.id == id) return &p; }
    } return nullptr;
}
const Pin* NodeGraphData::FindPin(PinId id) const {
    for (auto& n : m_Nodes) {
        for (auto& p : n.inputs) { if (p.id == id) return &p; }
        for (auto& p : n.outputs) { if (p.id == id) return &p; }
    } return nullptr;
}
NodeId NodeGraphData::GetPinOwner(PinId id) const {
    for (auto& n : m_Nodes) {
        for (auto& p : n.inputs) { if (p.id == id) return n.id; }
        for (auto& p : n.outputs) { if (p.id == id) return n.id; }
    } return 0;
}
LinkId NodeGraphData::AddLink(PinId startPin, PinId endPin, u64 userData) {
    GraphLink link; link.id = m_NextLinkId++;
    link.startPinId = startPin; link.endPinId = endPin; link.userData = userData;
    m_Links.push_back(link); return link.id;
}
void NodeGraphData::RemoveLink(LinkId id) {
    m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(),
        [id](const GraphLink& l) { return l.id == id; }), m_Links.end());
}
GraphLink* NodeGraphData::FindLink(LinkId id) {
    for (auto& l : m_Links) { if (l.id == id) return &l; } return nullptr;
}
const GraphLink* NodeGraphData::FindLink(LinkId id) const {
    for (auto& l : m_Links) { if (l.id == id) return &l; } return nullptr;
}
std::vector<LinkId> NodeGraphData::GetLinksForPin(PinId pinId) const {
    std::vector<LinkId> r;
    for (auto& l : m_Links) { if (l.startPinId == pinId || l.endPinId == pinId) r.push_back(l.id); }
    return r;
}
std::vector<LinkId> NodeGraphData::GetLinksForNode(NodeId nodeId) const {
    std::vector<LinkId> r; auto* node = FindNode(nodeId); if (!node) return r;
    for (auto& p : node->inputs) { auto x = GetLinksForPin(p.id); r.insert(r.end(), x.begin(), x.end()); }
    for (auto& p : node->outputs) { auto x = GetLinksForPin(p.id); r.insert(r.end(), x.begin(), x.end()); }
    return r;
}
bool NodeGraphData::HasLinkBetween(PinId a, PinId b) const {
    for (auto& l : m_Links) {
        if ((l.startPinId == a && l.endPinId == b) || (l.startPinId == b && l.endPinId == a)) return true;
    } return false;
}
nlohmann::json NodeGraphData::ToJson() const { return nlohmann::json{{"nodes",nlohmann::json::array()},{"links",nlohmann::json::array()}}; }
void NodeGraphData::FromJson(const nlohmann::json&) { Clear(); }
void NodeGraphData::Clear() { m_Nodes.clear(); m_Links.clear(); m_NextNodeId=1; m_NextPinId=1; m_NextLinkId=1; }

} // namespace Editor
} // namespace Enjin
#endif
