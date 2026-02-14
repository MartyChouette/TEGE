#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Procedural/LevelGenerator.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <utility>

namespace Enjin {
namespace Procedural {

// ============================================================================
// CellularAutomata - Cave/organic generation on a 2D grid
// ============================================================================

class CellularAutomata {
public:
    struct Params {
        u32 width         = 64;
        u32 height        = 64;
        u32 fillPercent   = 45;        // Initial fill percentage (0-100)
        u32 birthLimit    = 4;         // Neighbors required to birth a cell
        u32 deathLimit    = 3;         // Neighbors below which a cell dies
        u32 iterations    = 5;         // Smoothing passes
        u32 seed          = 0;         // 0 = random seed
    };

    struct Result {
        std::vector<std::vector<u8>> grid;   // 0 = empty, 1 = solid
        u32 width  = 0;
        u32 height = 0;
    };

    static ENJIN_API Result Generate(const Params& params);
};

// ============================================================================
// RandomWalker - Dungeon carving via random walk
// ============================================================================

class RandomWalker {
public:
    struct Params {
        u32 width           = 64;
        u32 height          = 64;
        u32 steps           = 2000;      // Total steps the walker takes
        f32 turnChance      = 0.5f;      // Probability of changing direction each step
        Math::Vector2 directionalBias;   // Bias toward a direction (0,0 = unbiased)
        u32 seed            = 0;
    };

    struct Result {
        std::vector<std::vector<u8>> grid;   // 0 = carved, 1 = solid
        Math::Vector2 startPos;
        Math::Vector2 endPos;
    };

    static ENJIN_API Result Generate(const Params& params);
};

// ============================================================================
// BSPGenerator - Binary Space Partition rooms + corridors
// ============================================================================

class BSPGenerator {
public:
    struct Rect {
        u32 x = 0;
        u32 y = 0;
        u32 w = 0;
        u32 h = 0;
    };

    struct Params {
        u32 width          = 64;
        u32 height         = 64;
        u32 minRoomSize    = 5;
        u32 maxRoomSize    = 15;
        u32 splitDepth     = 5;         // Number of recursive splits
        u32 corridorWidth  = 2;
        u32 seed           = 0;
    };

    struct Result {
        std::vector<std::vector<u8>> grid;   // 0 = empty, 1 = room/corridor
        std::vector<Rect> rooms;
    };

    static ENJIN_API Result Generate(const Params& params);
};

// ============================================================================
// DiamondSquare - Heightmap terrain generation
// ============================================================================

class DiamondSquare {
public:
    struct Params {
        u32 size       = 129;    // Must be 2^n + 1 (e.g. 33, 65, 129, 257, 513)
        f32 roughness  = 0.5f;   // Controls height variation decay per subdivision
        u32 seed       = 0;
        f32 minHeight  = 0.0f;
        f32 maxHeight  = 1.0f;
    };

    struct Result {
        std::vector<std::vector<f32>> heightmap;
        u32 size = 0;
    };

    static ENJIN_API Result Generate(const Params& params);
};

// ============================================================================
// LSystemGenerator - String rewriting with turtle graphics interpretation
// ============================================================================

class LSystemGenerator {
public:
    struct LineSegment {
        Math::Vector2 start;
        Math::Vector2 end;
    };

    struct Params {
        std::string axiom;                              // Starting string (e.g. "F")
        std::unordered_map<char, std::string> rules;    // Rewriting rules (e.g. 'F' -> "F+F-F")
        u32 iterations  = 4;
        f32 angle       = 25.0f;     // Turn angle in degrees
        f32 stepLength  = 1.0f;      // Length of each forward step
    };

    struct Result {
        std::vector<LineSegment> segments;
        Math::Vector2 boundsMin;
        Math::Vector2 boundsMax;
    };

    static ENJIN_API Result Generate(const Params& params);
};

// ============================================================================
// WaveFunctionCollapse - Tile-based constraint propagation
// ============================================================================

class WaveFunctionCollapse {
public:
    struct WFCTile {
        u32 id = 0;
        // Allowed neighbor tile IDs per direction: [0]=North, [1]=South, [2]=East, [3]=West
        std::vector<u32> allowedNeighbors[4];
    };

    struct Params {
        u32 width          = 16;
        u32 height         = 16;
        std::vector<WFCTile> tiles;
        u32 maxBacktracks  = 1000;     // Maximum backtrack attempts before failure
        u32 seed           = 0;
    };

    struct Result {
        std::vector<std::vector<u32>> grid;   // Tile IDs per cell
        bool success = false;
    };

    static ENJIN_API Result Generate(const Params& params);
};

// ============================================================================
// VoronoiGenerator - Seed points partitioned into regions
// ============================================================================

class VoronoiGenerator {
public:
    enum class DistanceType : u8 {
        Euclidean,
        Manhattan,
        Chebyshev
    };

    struct Params {
        u32 width        = 128;
        u32 height       = 128;
        u32 numSeeds     = 20;
        u32 seed         = 0;
        DistanceType distanceType = DistanceType::Euclidean;
    };

    struct Result {
        std::vector<std::vector<u32>> grid;          // Region ID per cell
        std::vector<Math::Vector2> seedPoints;
    };

    static ENJIN_API Result Generate(const Params& params);
};

// ============================================================================
// GrammarGenerator - Shape grammar for procedural buildings/structures
// ============================================================================

class GrammarGenerator {
public:
    struct GrammarRule {
        std::string symbol;
        std::vector<std::string> replacements;   // Weighted alternatives
        f32 weight = 1.0f;                       // Selection weight for this rule
    };

    enum class ShapeType : u8 {
        Box,
        Cylinder,
        Roof,
        Wall,
        Floor,
        Window,
        Door
    };

    struct GrammarShape {
        ShapeType type = ShapeType::Box;
        Math::Vector3 position;
        Math::Vector3 size;
        f32 rotation = 0.0f;       // Degrees around Y axis
    };

    struct Params {
        std::vector<GrammarRule> rules;
        std::string startSymbol;
        u32 iterations = 4;
        u32 seed       = 0;
    };

    struct Result {
        std::vector<GrammarShape> shapes;
    };

    static ENJIN_API Result Generate(const Params& params);
};

// ============================================================================
// PrefabAssembler - Snap-together rooms with connection points
// ============================================================================

class PrefabAssembler {
public:
    // Reuse ConnectionDirection from LevelGenerator (North/South/East/West)
    using ConnectionDir = ConnectionDirection;

    struct PrefabSlot {
        std::string id;
        Math::Vector3 position;
        Math::Vector3 size;
        std::vector<ConnectionDir> connections;
    };

    struct Placement {
        u32 slotIndex  = 0;
        Math::Vector3 position;
        f32 rotation   = 0.0f;       // Degrees around Y axis
    };

    struct Params {
        std::vector<PrefabSlot> slots;
        u32 maxRooms = 10;
        u32 seed     = 0;
    };

    struct Result {
        std::vector<Placement> placements;
    };

    static ENJIN_API Result Generate(const Params& params);
};

// ============================================================================
// FBMTerrain - Fractional Brownian Motion terrain heightmap generation
// ============================================================================

class FBMTerrain {
public:
    enum class NoiseMode : u8 {
        Standard,           // Classic fBm (octaved Perlin noise)
        RidgedMultifractal  // Ridged abs(noise) with power term for mountain ridges
    };

    struct Params {
        u32 width         = 256;
        u32 height        = 256;
        u32 octaves       = 6;         // Number of noise octaves (1-12)
        f32 lacunarity    = 2.0f;      // Frequency multiplier per octave (1.5-3.0)
        f32 gain          = 0.5f;      // Amplitude multiplier per octave (0.3-0.7), also called persistence
        f32 frequency     = 1.0f;      // Base frequency of the noise
        f32 ridgedPower   = 1.0f;      // Power exponent for ridged variant (higher = sharper ridges)
        f32 ridgedOffset  = 1.0f;      // Offset for ridged variant before squaring
        NoiseMode mode    = NoiseMode::Standard;
        u32 seed          = 0;
    };

    struct Result {
        std::vector<f32> heightmap;    // Row-major (y * width + x), values in [0, 1]
        u32 width  = 0;
        u32 height = 0;
        f32 minValue = 0.0f;          // Actual min value before normalization
        f32 maxValue = 0.0f;          // Actual max value before normalization
    };

    static ENJIN_API Result Generate(const Params& params);
};

// ============================================================================
// HydraulicErosion - Rainfall-based droplet erosion simulation
// ============================================================================

class HydraulicErosion {
public:
    struct Params {
        u32 iterations       = 50000;   // Number of water droplets (10000-500000)
        u32 dropLifetime     = 64;      // Max steps per droplet (30-100)
        f32 sedimentCapacity = 4.0f;    // How much sediment a droplet can carry (2-8)
        f32 depositRate      = 0.3f;    // Fraction of excess sediment deposited (0.1-0.4)
        f32 erosionRate      = 0.3f;    // Fraction of capacity deficit eroded (0.2-0.5)
        f32 evaporateRate    = 0.01f;   // Water lost per step (0.01-0.05)
        f32 gravity          = 4.0f;    // Gravity acceleration factor (4-10)
        f32 minSlope         = 0.01f;   // Minimum slope for erosion
        u32 erosionRadius    = 3;       // Radius of erosion brush (1-5)
        f32 inertia          = 0.05f;   // Droplet direction inertia (0-1)
        u32 seed             = 0;
    };

    // Erodes the heightmap in-place. heightmap is row-major (y * width + x).
    static ENJIN_API void Erode(std::vector<f32>& heightmap, u32 width, u32 height,
                                const Params& params);
};

// ============================================================================
// ThermalErosion - Temperature-based talus erosion
// ============================================================================

class ThermalErosion {
public:
    struct Params {
        u32 iterations   = 50;         // Number of erosion passes (10-100)
        f32 talusAngle   = 0.8f;       // Maximum stable height difference (0.5-4.0)
        f32 erosionRate  = 0.5f;       // Fraction of excess material transferred (0.1-0.5)
    };

    // Erodes the heightmap in-place. heightmap is row-major (y * width + x).
    static ENJIN_API void Erode(std::vector<f32>& heightmap, u32 width, u32 height,
                                const Params& params);
};

// ============================================================================
// LSystem3D - 3D turtle interpretation of L-system strings
// ============================================================================

class LSystem3D {
public:
    struct LineSegment3D {
        Math::Vector3 start;
        Math::Vector3 end;
        f32 radius = 1.0f;            // Branch radius at this segment
    };

    // Stochastic rule: multiple replacements with weights
    struct StochasticRule {
        std::vector<std::pair<std::string, f32>> options;  // (replacement, weight) pairs
    };

    struct Params {
        std::string axiom;
        std::unordered_map<char, std::string> rules;                  // Deterministic rules
        std::unordered_map<char, StochasticRule> stochasticRules;     // Stochastic rules (override deterministic)
        u32 iterations   = 4;
        f32 angle        = 25.7f;      // Turn/pitch/roll angle in degrees
        f32 stepLength   = 1.0f;       // Length of each forward step
        f32 initialRadius = 1.0f;      // Starting branch radius
        f32 radiusDecay  = 0.7f;       // Radius multiplier per [ push (0.5-1.0)
        u32 seed         = 0;          // For stochastic rules (0 = random)
    };

    struct Result {
        std::vector<LineSegment3D> segments;
        Math::Vector3 boundsMin;
        Math::Vector3 boundsMax;
    };

    // 3D turtle commands:
    //   F  - Move forward and draw line segment
    //   f  - Move forward without drawing
    //   +  - Yaw left (rotate around up axis)
    //   -  - Yaw right
    //   ^  - Pitch up (rotate around left axis)
    //   &  - Pitch down
    //   /  - Roll left (rotate around heading axis)
    //   \  - Roll right
    //   [  - Push state (position + orientation + radius)
    //   ]  - Pop state
    static ENJIN_API Result Generate(const Params& params);
};

} // namespace Procedural
} // namespace Enjin
