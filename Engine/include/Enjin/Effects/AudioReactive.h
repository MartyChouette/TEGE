#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Math.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include <vector>
#include <string>
#include <cmath>

/**
 * @file AudioReactive.h
 * @brief Real-time FFT analysis of audio drives per-vertex mesh displacement
 *
 * Implements a Cooley-Tukey radix-2 FFT for spectrum analysis and maps
 * frequency band energies to per-vertex displacement on meshes, enabling
 * audio-reactive visual effects.
 */

namespace Enjin {
namespace Effects {

// ============================================================================
// FFT ANALYZER (Cooley-Tukey Radix-2)
// ============================================================================

class ENJIN_API FFTAnalyzer {
public:
    FFTAnalyzer() = default;
    ~FFTAnalyzer() = default;

    /**
     * @brief Initialize the FFT with a given window size
     * @param fftSize Must be a power of 2 (256, 512, 1024, 2048)
     */
    void Initialize(i32 fftSize);

    /**
     * @brief Process audio samples and compute the frequency spectrum
     * @param samples  Array of mono audio samples (normalized -1 to 1)
     * @param count    Number of samples (will be zero-padded or truncated to fftSize)
     */
    void ProcessSamples(const f32* samples, i32 count);

    /**
     * @brief Get the energy (magnitude) of a specific frequency band
     * @param bandIndex 0-based index into the spectrum (0 to fftSize/2 - 1)
     * @return Magnitude at that frequency bin
     */
    f32 GetBandEnergy(i32 bandIndex) const;

    /**
     * @brief Get the frequency in Hz corresponding to a band index
     * @param bandIndex 0-based frequency bin index
     * @return Frequency in Hz
     */
    f32 GetBandFrequency(i32 bandIndex) const;

    /**
     * @brief Get the full smoothed magnitude spectrum
     * @return Reference to the spectrum vector (fftSize/2 entries)
     */
    const std::vector<f32>& GetSpectrum() const { return m_SmoothedSpectrum; }

    /**
     * @brief Get the energy in the bass frequency range (20-250 Hz)
     * @return Summed and normalized energy in the bass range
     */
    f32 GetBassEnergy() const;

    /**
     * @brief Get the energy in the mid frequency range (250-4000 Hz)
     * @return Summed and normalized energy in the mid range
     */
    f32 GetMidEnergy() const;

    /**
     * @brief Get the energy in the treble frequency range (4000-20000 Hz)
     * @return Summed and normalized energy in the treble range
     */
    f32 GetTrebleEnergy() const;

    /**
     * @brief Get the overall energy across all frequencies
     * @return Average magnitude across the spectrum
     */
    f32 GetOverallEnergy() const;

    /**
     * @brief Set the sample rate for frequency calculations
     * @param sampleRate Sample rate in Hz (default 44100)
     */
    void SetSampleRate(f32 sampleRate) { m_SampleRate = sampleRate; }

    /**
     * @brief Set the smoothing factor for spectrum display
     * @param factor Exponential smoothing (0 = no smoothing, 1 = frozen)
     */
    void SetSmoothingFactor(f32 factor) { m_SmoothingFactor = Math::Clamp(factor, 0.0f, 0.99f); }

    /**
     * @brief Get the FFT size
     */
    i32 GetFFTSize() const { return m_FFTSize; }

    /**
     * @brief Check if analyzer has been initialized
     */
    bool IsInitialized() const { return m_FFTSize > 0 && !m_Spectrum.empty(); }

private:
    /**
     * @brief In-place Cooley-Tukey radix-2 decimation-in-time FFT
     * @param real Real parts (modified in-place)
     * @param imag Imaginary parts (modified in-place)
     */
    void ComputeFFT(std::vector<f32>& real, std::vector<f32>& imag);

    /**
     * @brief Apply a Hanning window to reduce spectral leakage
     * @param samples Samples to window (modified in-place)
     */
    void ApplyHanningWindow(std::vector<f32>& samples);

    /**
     * @brief Bit-reversal permutation of array indices
     * @param real Real parts
     * @param imag Imaginary parts
     */
    void BitReversalPermutation(std::vector<f32>& real, std::vector<f32>& imag);

    /**
     * @brief Get the energy sum in a frequency range, normalized by bin count
     * @param freqLow  Lower frequency bound in Hz
     * @param freqHigh Upper frequency bound in Hz
     * @return Normalized energy
     */
    f32 GetEnergyInRange(f32 freqLow, f32 freqHigh) const;

    i32 m_FFTSize = 0;
    f32 m_SampleRate = 44100.0f;
    std::vector<f32> m_Spectrum;           // Raw magnitude spectrum (fftSize/2)
    std::vector<f32> m_SmoothedSpectrum;   // Exponentially smoothed spectrum
    f32 m_SmoothingFactor = 0.8f;

    // Pre-computed Hanning window coefficients
    std::vector<f32> m_Window;
};

// ============================================================================
// AUDIO REACTIVE COMPONENT (ECS)
// ============================================================================

struct AudioReactiveComponent {
    // --- FFT settings ---
    i32 fftSize = 1024;            // FFT window size (power of 2)
    f32 smoothing = 0.8f;          // Spectrum smoothing factor

    // --- Displacement axis ---
    enum class DisplacementAxis : u8 {
        Normal,     // Displace along vertex normal
        Radial,     // Displace radially from mesh center
        YUp,        // Displace along world Y axis
        Custom      // Displace along a custom direction
    };
    DisplacementAxis axis = DisplacementAxis::Normal;
    Math::Vector3 customAxis = {0.0f, 1.0f, 0.0f};

    // --- Band-to-vertex mapping ---
    enum class MappingMode : u8 {
        Uniform,        // All vertices react to overall energy
        HeightBased,    // Low Y vertices = bass, high Y = treble
        RadialBased,    // Center = bass, outer = treble
        UVBased         // U coordinate maps to frequency
    };
    MappingMode mapping = MappingMode::Uniform;

    // --- Amplitude scales ---
    f32 bassScale = 2.0f;          // Displacement scale for bass (20-250 Hz)
    f32 midScale = 1.0f;           // Displacement scale for mids (250-4000 Hz)
    f32 trebleScale = 0.5f;        // Displacement scale for treble (4000-20000 Hz)
    f32 globalScale = 1.0f;        // Master amplitude multiplier

    // --- Animation ---
    f32 responseSpeed = 10.0f;     // How fast displacement follows audio (lerp speed)
    f32 returnSpeed = 5.0f;        // How fast vertices return to rest position

    // --- Audio source ---
    std::string audioClipPath;     // Path to audio clip for analysis
    bool useLiveInput = false;     // Use microphone input (not yet implemented)

    // --- Runtime state (not serialized) ---
    std::vector<Math::Vector3> originalPositions;  // Unmodified vertex positions
    std::vector<Math::Vector3> originalNormals;    // Unmodified vertex normals
    std::vector<f32> currentDisplacements;         // Current per-vertex displacement
    Math::Vector3 meshBoundsMin;                   // Cached mesh AABB min
    Math::Vector3 meshBoundsMax;                   // Cached mesh AABB max
    Math::Vector3 meshCenter;                      // Cached mesh center
    f32 meshRadius = 0.0f;                         // Cached max distance from center
    bool initialized = false;
};

// ============================================================================
// MESH DISPLACER HELPER
// ============================================================================

struct MeshDisplacer {
    /**
     * @brief Apply per-vertex displacements to a mesh
     * @param vertices       Current mesh vertices (modified in-place)
     * @param originalVerts  Original undeformed vertex positions
     * @param originalNorms  Original undeformed vertex normals
     * @param displacements  Per-vertex scalar displacement amounts
     * @param axis           Displacement axis mode
     * @param customAxis     Custom axis direction (used if axis == Custom)
     * @param meshCenter     Center of the mesh (used for Radial mode)
     */
    static void DisplaceVertices(std::vector<ECS::MeshComponent::Vertex>& vertices,
                                 const std::vector<Math::Vector3>& originalVerts,
                                 const std::vector<Math::Vector3>& originalNorms,
                                 const std::vector<f32>& displacements,
                                 AudioReactiveComponent::DisplacementAxis axis,
                                 const Math::Vector3& customAxis,
                                 const Math::Vector3& meshCenter);

    /**
     * @brief Recompute vertex normals from triangle indices after displacement
     * @param vertices Mesh vertices (normals will be overwritten)
     * @param indices  Triangle index buffer
     */
    static void RecomputeNormals(std::vector<ECS::MeshComponent::Vertex>& vertices,
                                 const std::vector<u32>& indices);

    /**
     * @brief Compute the axis-aligned bounding box of a set of vertices
     * @param vertices Mesh vertices
     * @param outMin   Output minimum corner
     * @param outMax   Output maximum corner
     */
    static void ComputeMeshBounds(const std::vector<ECS::MeshComponent::Vertex>& vertices,
                                  Math::Vector3& outMin, Math::Vector3& outMax);
};

// ============================================================================
// AUDIO REACTIVE SYSTEM
// ============================================================================

class ENJIN_API AudioReactiveSystem {
public:
    AudioReactiveSystem() = default;
    ~AudioReactiveSystem() = default;

    /**
     * @brief Update all audio-reactive entities
     *
     * For each entity with AudioReactiveComponent + MeshComponent:
     * 1. Feed audio samples to FFT analyzer
     * 2. Map frequency bands to per-vertex displacement weights
     * 3. Smoothly animate displacement toward target
     * 4. Apply displacement and recompute normals
     *
     * @param world  ECS world
     * @param dt     Delta time in seconds
     */
    void Update(ECS::World* world, f32 dt);

    /**
     * @brief Generate a test sine wave audio signal
     * @param frequency  Frequency in Hz
     * @param duration   Duration in seconds
     * @param sampleRate Sample rate in Hz
     * @return Vector of audio samples (-1 to 1)
     */
    static std::vector<f32> GenerateTestAudio(f32 frequency, f32 duration, f32 sampleRate = 44100.0f);

    /**
     * @brief Get the FFT analyzer (for UI visualization)
     */
    const FFTAnalyzer& GetAnalyzer() const { return m_Analyzer; }

private:
    /**
     * @brief Initialize runtime state for an AudioReactiveComponent
     */
    void InitializeComponent(AudioReactiveComponent& comp,
                             const ECS::MeshComponent& mesh);

    /**
     * @brief Compute the frequency weight for a vertex based on mapping mode
     * @return Struct with bass, mid, treble weights (each 0-1)
     */
    struct FrequencyWeights {
        f32 bass = 0.333f;
        f32 mid = 0.333f;
        f32 treble = 0.333f;
    };

    FrequencyWeights ComputeVertexWeights(const AudioReactiveComponent& comp,
                                           const Math::Vector3& vertexPos,
                                           const Math::Vector2& vertexUV,
                                           usize vertexIndex) const;

    FFTAnalyzer m_Analyzer;

    // Test audio playback state
    std::vector<f32> m_TestSamples;
    usize m_TestPlaybackPos = 0;
    f32 m_TestSampleRate = 44100.0f;
};

} // namespace Effects
} // namespace Enjin
