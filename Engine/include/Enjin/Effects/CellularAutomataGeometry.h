#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <string>
#include <functional>

namespace Enjin {
namespace Effects {

// CA rule types
enum class CARule : u8 {
    GameOfLife,         // B3/S23 — Conway's Game of Life
    HighLife,           // B36/S23 — Replicator variant
    DayAndNight,        // B3678/S34678 — Symmetric
    Seeds,              // B2/S — Explosive growth
    BriansBrain,        // 3-state: On → Dying → Dead → On
    Rule110,            // Elementary 1D automaton (2D layered as time axis)
    Diamoeba,           // B35678/S5678 — Diamond-shaped amoeba
    Custom              // User-defined birth/survival rules
};

// Output mesh generation mode
enum class CAMeshMode : u8 {
    Voxels,             // Each live cell = a unit cube
    MarchingCubes,      // Smooth iso-surface from density field
    PointCloud          // Each live cell = a point/sphere
};

// Configuration
struct CAGeoConfig {
    u32 width = 32;
    u32 height = 32;
    u32 depth = 1;              // >1 for 3D CA (layered slices)
    CARule rule = CARule::GameOfLife;
    CAMeshMode meshMode = CAMeshMode::Voxels;
    f32 cellSize = 1.0f;        // World-space size of each cell
    f32 updateInterval = 0.2f;  // Seconds between CA steps
    u32 birthRule = 0b00001000; // Bit N = born if N neighbors (default: B3)
    u32 survivalRule = 0b00001100; // Bit N = survive if N neighbors (default: S23)
    u32 states = 2;             // 2 for binary, 3 for Brian's Brain
    f32 initialFillPercent = 30.0f; // Random fill percentage for init
    u32 seed = 0;
    bool wrapEdges = true;
    f32 isoLevel = 0.5f;       // Marching cubes threshold
    Math::Vector3 liveColor = {0.2f, 0.8f, 0.3f};
    Math::Vector3 dyingColor = {0.8f, 0.3f, 0.1f};
};

// Single vertex for generated mesh
struct CAVertex {
    Math::Vector3 position;
    Math::Vector3 normal;
    Math::Vector3 color;
};

// Generated mesh data
struct CAMeshData {
    std::vector<CAVertex> vertices;
    std::vector<u32> indices;
    u32 liveCellCount = 0;
    u32 generation = 0;
};

// Cellular automata simulation that generates geometry
class ENJIN_API CellularAutomataGeometry {
public:
    CellularAutomataGeometry() = default;
    ~CellularAutomataGeometry() = default;

    // Initialize the CA grid
    void Initialize(const CAGeoConfig& config);

    // Update simulation (steps based on updateInterval)
    void Update(f32 dt);

    // Force a single step
    void Step();

    // Reset to initial random state
    void Reset();

    // Set a specific cell state
    void SetCell(u32 x, u32 y, u32 z, u8 state);
    u8 GetCell(u32 x, u32 y, u32 z) const;

    // Stamp a pattern (e.g., glider, blinker) at position
    void StampGlider(u32 x, u32 y);
    void StampPulsar(u32 x, u32 y);
    void StampGosperGun(u32 x, u32 y);

    // Generate mesh from current state
    CAMeshData GenerateMesh() const;

    // Access
    const CAGeoConfig& GetConfig() const { return m_Config; }
    CAGeoConfig& GetConfig() { return m_Config; }
    u32 GetGeneration() const { return m_Generation; }
    u32 GetLiveCellCount() const;

    // Rule helpers
    static const char* GetRuleName(CARule rule);
    static void GetRuleBits(CARule rule, u32& birth, u32& survival, u32& states);

private:
    CAGeoConfig m_Config;
    std::vector<u8> m_Grid;         // Current state (x + y*width + z*width*height)
    std::vector<u8> m_NextGrid;     // Next state buffer
    u32 m_Generation = 0;
    f32 m_Timer = 0.0f;

    u32 CellIndex(u32 x, u32 y, u32 z) const;
    u32 CountNeighbors2D(u32 x, u32 y) const;
    u32 CountNeighbors3D(u32 x, u32 y, u32 z) const;
    u32 WrapCoord(i32 v, u32 max) const;

    // Mesh generation methods
    void GenerateVoxelMesh(CAMeshData& mesh) const;
    void GenerateMarchingCubesMesh(CAMeshData& mesh) const;
    void GeneratePointCloudMesh(CAMeshData& mesh) const;

    // Face visibility check for voxels (skip internal faces)
    bool IsFaceVisible(u32 x, u32 y, u32 z, i32 dx, i32 dy, i32 dz) const;
};

} // namespace Effects
} // namespace Enjin
