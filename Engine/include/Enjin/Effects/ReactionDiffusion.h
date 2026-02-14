#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <string>

namespace Enjin {
namespace Effects {

// Predefined reaction-diffusion pattern presets
enum class RDPreset : u8 {
    MitosisSpots,       // Cell-like splitting dots
    CoralGrowth,        // Coral/branching patterns
    Fingerprints,       // Ridged fingerprint-like lines
    Leopard,            // Leopard spot patterns
    Labyrinth,          // Maze-like connected patterns
    WormHoles,          // Isolated holes in solid field
    BubblePacking,      // Tightly packed circular spots
    Spirals,            // Rotating spiral patterns
    Custom              // User-defined feed/kill rates
};

// Configuration for a reaction-diffusion simulation
struct RDConfig {
    u32 width = 256;
    u32 height = 256;
    f32 feedRate = 0.055f;      // Feed rate for chemical U (0.01-0.1)
    f32 killRate = 0.062f;      // Kill rate for chemical V (0.04-0.07)
    f32 diffusionU = 1.0f;      // Diffusion rate of chemical U
    f32 diffusionV = 0.5f;      // Diffusion rate of chemical V
    f32 deltaTime = 1.0f;       // Simulation time step
    u32 stepsPerFrame = 10;     // Sub-steps per update call
    RDPreset preset = RDPreset::MitosisSpots;
    bool wrapEdges = true;      // Toroidal boundary conditions
    u32 seed = 0;               // Seed for initial perturbation (0 = random)
};

// Results/state of the simulation
struct RDState {
    std::vector<f32> chemU;     // Chemical U concentration grid (row-major)
    std::vector<f32> chemV;     // Chemical V concentration grid (row-major)
    u32 width = 0;
    u32 height = 0;
    u32 stepCount = 0;
};

// Gray-Scott reaction-diffusion simulation
// Runs on a 2D grid; output can be mapped to mesh UVs for surface patterns
class ENJIN_API ReactionDiffusion {
public:
    ReactionDiffusion() = default;
    ~ReactionDiffusion() = default;

    // Initialize/reset the simulation with given config
    void Initialize(const RDConfig& config);

    // Step the simulation forward (runs config.stepsPerFrame sub-steps)
    void Update(f32 dt);

    // Single step
    void Step();

    // Seed a circular region of chemical V at (cx, cy) with radius r
    void SeedCircle(f32 cx, f32 cy, f32 radius);

    // Seed random spots
    void SeedRandom(u32 count, f32 spotRadius);

    // Apply a preset's feed/kill rates
    static RDConfig GetPresetConfig(RDPreset preset);
    static const char* GetPresetName(RDPreset preset);

    // Access state
    const RDState& GetState() const { return m_State; }
    const RDConfig& GetConfig() const { return m_Config; }
    RDConfig& GetConfig() { return m_Config; }
    void SetConfig(const RDConfig& config) { m_Config = config; }

    // Bake to RGBA8 texture (maps chemV to a color gradient)
    std::vector<u8> BakeToRGBA8(const Math::Vector3& colorLow = {0.05f, 0.05f, 0.15f},
                                 const Math::Vector3& colorHigh = {0.9f, 0.7f, 0.2f}) const;

    // Bake to single-channel heightmap (chemV values)
    std::vector<f32> BakeToHeightmap() const;

private:
    RDConfig m_Config;
    RDState m_State;
    std::vector<f32> m_TempU;   // Double-buffer for simulation
    std::vector<f32> m_TempV;

    f32 LaplacianU(u32 x, u32 y) const;
    f32 LaplacianV(u32 x, u32 y) const;
    u32 WrapX(i32 x) const;
    u32 WrapY(i32 y) const;
};

} // namespace Effects
} // namespace Enjin
