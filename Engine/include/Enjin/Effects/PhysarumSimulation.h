#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <string>

namespace Enjin {
namespace Effects {

// Physarum behavior presets
enum class PhysarumPreset : u8 {
    ClassicSlime,       // Standard Physarum behavior
    BranchingNetwork,   // More branching, less merging
    DenseWeb,           // Thick interconnected web
    Tendrils,           // Thin extending tendrils
    Pulsating,          // Oscillating density
    Custom              // User-defined parameters
};

// Configuration for the simulation
struct PhysarumConfig {
    u32 width = 512;            // Trail map resolution
    u32 height = 512;
    u32 agentCount = 50000;     // Number of simulated agents
    f32 sensorAngle = 22.5f;    // Degrees — angle between sensors (typical: 22.5-45)
    f32 sensorDistance = 9.0f;  // Pixels — how far ahead sensors sample
    f32 turnSpeed = 45.0f;      // Degrees/step — max turn rate per step
    f32 moveSpeed = 1.0f;       // Pixels/step — agent movement speed
    f32 trailDecay = 0.02f;     // Trail evaporation rate per step (0-1)
    f32 trailDeposit = 5.0f;    // Trail amount deposited per agent per step
    f32 diffuseRadius = 1.0f;   // Diffusion kernel radius (0=none, 1=3x3, 2=5x5)
    bool wrapEdges = true;      // Toroidal boundary
    PhysarumPreset preset = PhysarumPreset::ClassicSlime;
    u32 stepsPerFrame = 1;
    u32 seed = 0;               // 0 = random
};

// A single Physarum agent
struct PhysarumAgent {
    f32 x, y;          // Position (fractional pixels)
    f32 angle;          // Heading in radians
};

// Simulation state
struct PhysarumState {
    std::vector<f32> trailMap;      // Trail chemical concentration (row-major)
    std::vector<PhysarumAgent> agents;
    u32 width = 0;
    u32 height = 0;
    u32 stepCount = 0;
};

// Physarum polycephalum simulation — agent-based trail formation
class ENJIN_API PhysarumSimulation {
public:
    PhysarumSimulation() = default;
    ~PhysarumSimulation() = default;

    // Initialize simulation
    void Initialize(const PhysarumConfig& config);

    // Update simulation (runs stepsPerFrame sub-steps)
    void Update(f32 dt);

    // Single simulation step
    void Step();

    // Reset agents to random positions
    void Reset();

    // Place agents in specific patterns
    void SeedCircle(f32 cx, f32 cy, f32 radius, u32 count);
    void SeedPoint(f32 cx, f32 cy, u32 count);
    void SeedRing(f32 cx, f32 cy, f32 innerRadius, f32 outerRadius, u32 count);

    // Add food source (high trail deposit at location)
    void AddFoodSource(f32 x, f32 y, f32 radius, f32 strength);

    // Get preset config
    static PhysarumConfig GetPresetConfig(PhysarumPreset preset);
    static const char* GetPresetName(PhysarumPreset preset);

    // Access state
    const PhysarumState& GetState() const { return m_State; }
    const PhysarumConfig& GetConfig() const { return m_Config; }
    PhysarumConfig& GetConfig() { return m_Config; }
    void SetConfig(const PhysarumConfig& config) { m_Config = config; }

    // Bake trail map to RGBA8 texture
    std::vector<u8> BakeToRGBA8(const Math::Vector3& trailColor = {0.8f, 0.9f, 0.3f},
                                 const Math::Vector3& bgColor = {0.02f, 0.02f, 0.05f}) const;

    // Bake to single-channel heightmap
    std::vector<f32> BakeToHeightmap() const;

private:
    PhysarumConfig m_Config;
    PhysarumState m_State;
    std::vector<f32> m_TempTrail;   // Double-buffer for diffusion

    // Sense the trail map at a sensor position
    f32 Sense(const PhysarumAgent& agent, f32 sensorAngleOffset) const;

    // Sample trail with bilinear interpolation
    f32 SampleTrail(f32 x, f32 y) const;

    // Diffuse and decay the trail map
    void DiffuseAndDecay();

    // Wrap coordinates
    f32 WrapX(f32 x) const;
    f32 WrapY(f32 y) const;

    // PRNG state
    u32 m_RngState = 12345;
    f32 RandomFloat();  // Returns 0-1
};

} // namespace Effects
} // namespace Enjin
