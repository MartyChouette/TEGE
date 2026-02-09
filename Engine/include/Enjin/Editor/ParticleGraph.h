#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Editor {

enum class ParticleNodeType : u8 {
    // Emitters
    PointEmitter, SphereEmitter, BoxEmitter, ConeEmitter, MeshEmitter,

    // Modifiers
    Gravity, Wind, Turbulence, Drag, Vortex,
    ColorOverLife, SizeOverLife, SpeedOverLife, RotationOverLife,

    // Sub-emitters
    SubEmitterOnBirth, SubEmitterOnDeath, SubEmitterOnCollision,

    // Collision
    PlaneCollider, WorldCollider,

    // Output
    BillboardRenderer, MeshRenderer, TrailRenderer,

    // Control
    Burst, Loop, Delay
};

struct ParticleGraphNode {
    u32 id = 0;
    ParticleNodeType type = ParticleNodeType::PointEmitter;
    Math::Vector2 position;
    std::string label;

    // Common properties
    f32 rate = 10.0f;
    f32 lifetime = 2.0f;
    f32 startSpeed = 5.0f;
    Math::Vector3 direction = Math::Vector3(0, 1, 0);
    f32 spread = 0.5f;
    f32 strength = 1.0f;

    // Curve data (simplified)
    std::vector<Math::Vector2> curve;  // time (0-1) -> value pairs
};

struct ParticleGraphLink {
    u32 id = 0;
    u32 fromNode, fromPin, toNode, toPin;
};

struct ParticleGraphData {
    std::string name = "New Particle System";
    std::vector<ParticleGraphNode> nodes;
    std::vector<ParticleGraphLink> links;
    u32 nextNodeId = 1;
    u32 nextLinkId = 1;
};

class ENJIN_API ParticleGraphEditor {
public:
    void Render();
    void SetGraph(ParticleGraphData* graph);
    bool IsOpen() const { return m_Open; }
    void SetOpen(bool open) { m_Open = open; }

private:
    ParticleGraphData* m_Graph = nullptr;
    bool m_Open = false;
    u32 m_SelectedNodeId = 0;
    Math::Vector2 m_ScrollOffset;
    f32 m_Zoom = 1.0f;

    void DrawNode(ParticleGraphNode& node);
    void DrawConnections();
    void DrawInspector();
    void DrawContextMenu();
    static const char* GetNodeName(ParticleNodeType type);
};

} // namespace Editor
} // namespace Enjin
