#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin { namespace ECS { struct ParticleEmitterComponent; } }

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

    // Renderer properties (Billboard/Mesh/Trail)
    std::string texturePath;            // Billboard/Trail texture
    std::string meshPath;               // MeshRenderer mesh path
    i32 billboardMode = 0;              // 0=Camera-facing, 1=Velocity-stretched
    i32 sortMode = 0;                   // 0=None, 1=Back-to-front, 2=Front-to-back
    i32 blendMode = 0;                  // 0=Alpha, 1=Additive, 2=Multiply
    f32 sizeMultiplier = 1.0f;
    Math::Vector3 colorTint{1,1,1};
    Math::Vector3 meshScale{1,1,1};
    i32 rotationAlignment = 0;          // 0=None, 1=Velocity, 2=Custom axis
    f32 trailWidth = 0.5f;
    f32 trailEndWidth = 0.1f;
    i32 trailTextureMode = 0;           // 0=Stretch, 1=Tile
    f32 trailMinVertexDistance = 0.1f;
    Math::Vector3 trailStartColor{1,1,1};
    Math::Vector3 trailEndColor{1,1,1};

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

    bool Save(const std::string& path) const;
    bool Load(const std::string& path);

    bool WasCompileRequested() { bool r = m_CompileRequested; m_CompileRequested = false; return r; }

private:
    ParticleGraphData* m_Graph = nullptr;
    bool m_Open = false;
    bool m_CompileRequested = false;
    u32 m_SelectedNodeId = 0;
    Math::Vector2 m_ScrollOffset;
    f32 m_Zoom = 1.0f;

    void DrawNode(ParticleGraphNode& node);
    void DrawConnections();
    void DrawInspector();
    void DrawContextMenu();
    static const char* GetNodeName(ParticleNodeType type);
};

// Compile particle graph into ParticleEmitterComponent fields
struct ParticleCompileResult {
    bool success = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class ENJIN_API ParticleGraphCompiler {
public:
    // Compile graph -> write fields into target component
    static ParticleCompileResult Compile(const ParticleGraphData& graph,
                                          ECS::ParticleEmitterComponent& target);
};

} // namespace Editor
} // namespace Enjin
