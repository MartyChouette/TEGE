#include "Enjin/Effects/AudioReactive.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace Enjin {
namespace Effects {

// ============================================================================
// FFT ANALYZER — Initialization
// ============================================================================

void FFTAnalyzer::Initialize(i32 fftSize) {
    // Ensure power of 2
    if (fftSize < 4) fftSize = 4;
    // Round up to next power of 2 if needed
    i32 rounded = 1;
    while (rounded < fftSize) rounded <<= 1;
    m_FFTSize = rounded;

    i32 halfSize = m_FFTSize / 2;
    m_Spectrum.assign(halfSize, 0.0f);
    m_SmoothedSpectrum.assign(halfSize, 0.0f);

    // Pre-compute Hanning window: w(n) = 0.5 * (1 - cos(2*pi*n / (N-1)))
    m_Window.resize(m_FFTSize);
    f32 invN = 1.0f / static_cast<f32>(m_FFTSize - 1);
    for (i32 i = 0; i < m_FFTSize; ++i) {
        m_Window[i] = 0.5f * (1.0f - Math::Cos(Math::PI_2 * static_cast<f32>(i) * invN));
    }
}

// ============================================================================
// FFT ANALYZER — Hanning Window
// ============================================================================

void FFTAnalyzer::ApplyHanningWindow(std::vector<f32>& samples) {
    i32 n = static_cast<i32>(samples.size());
    i32 windowSize = static_cast<i32>(m_Window.size());
    for (i32 i = 0; i < n && i < windowSize; ++i) {
        samples[i] *= m_Window[i];
    }
}

// ============================================================================
// FFT ANALYZER — Bit-Reversal Permutation
// ============================================================================

void FFTAnalyzer::BitReversalPermutation(std::vector<f32>& real, std::vector<f32>& imag) {
    i32 n = static_cast<i32>(real.size());
    i32 j = 0;

    for (i32 i = 0; i < n - 1; ++i) {
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }

        i32 k = n >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }
}

// ============================================================================
// FFT ANALYZER — Cooley-Tukey Radix-2 DIT FFT
// ============================================================================

void FFTAnalyzer::ComputeFFT(std::vector<f32>& real, std::vector<f32>& imag) {
    i32 n = static_cast<i32>(real.size());
    if (n < 2) return;

    // Step 1: Bit-reversal permutation
    BitReversalPermutation(real, imag);

    // Step 2: Butterfly operations at each stage
    // For stage s = 1, 2, ..., log2(N):
    //   halfSize = 2^(s-1)
    //   groupSize = 2^s
    i32 numStages = 0;
    {
        i32 temp = n;
        while (temp > 1) {
            temp >>= 1;
            ++numStages;
        }
    }

    for (i32 stage = 1; stage <= numStages; ++stage) {
        i32 halfSize = 1 << (stage - 1);      // 2^(stage-1)
        i32 groupSize = halfSize << 1;         // 2^stage
        i32 numGroups = n / groupSize;

        // Twiddle factor angle step: -2*PI / groupSize
        f32 angleStep = -Math::PI_2 / static_cast<f32>(groupSize);

        for (i32 k = 0; k < numGroups; ++k) {
            i32 groupStart = k * groupSize;

            for (i32 j = 0; j < halfSize; ++j) {
                i32 idx1 = groupStart + j;
                i32 idx2 = idx1 + halfSize;

                // Twiddle factor: W_N^j = exp(-2*pi*i*j/groupSize)
                f32 angle = angleStep * static_cast<f32>(j);
                f32 twReal = Math::Cos(angle);
                f32 twImag = Math::Sin(angle);

                // Butterfly computation
                f32 tempReal = real[idx2] * twReal - imag[idx2] * twImag;
                f32 tempImag = real[idx2] * twImag + imag[idx2] * twReal;

                real[idx2] = real[idx1] - tempReal;
                imag[idx2] = imag[idx1] - tempImag;
                real[idx1] += tempReal;
                imag[idx1] += tempImag;
            }
        }
    }
}

// ============================================================================
// FFT ANALYZER — Process Samples
// ============================================================================

void FFTAnalyzer::ProcessSamples(const f32* samples, i32 count) {
    if (m_FFTSize <= 0 || !samples) return;

    // Copy samples into working buffer, zero-pad or truncate
    std::vector<f32> real(m_FFTSize, 0.0f);
    std::vector<f32> imag(m_FFTSize, 0.0f);

    i32 copyCount = (count < m_FFTSize) ? count : m_FFTSize;
    for (i32 i = 0; i < copyCount; ++i) {
        real[i] = samples[i];
    }

    // Apply Hanning window to reduce spectral leakage
    ApplyHanningWindow(real);

    // Compute FFT
    ComputeFFT(real, imag);

    // Compute magnitude spectrum: |X[k]| = sqrt(real[k]^2 + imag[k]^2) / N
    i32 halfSize = m_FFTSize / 2;
    f32 invN = 1.0f / static_cast<f32>(m_FFTSize);

    for (i32 i = 0; i < halfSize; ++i) {
        m_Spectrum[i] = Math::Sqrt(real[i] * real[i] + imag[i] * imag[i]) * invN;
    }

    // Exponential smoothing
    for (i32 i = 0; i < halfSize; ++i) {
        m_SmoothedSpectrum[i] = m_SmoothingFactor * m_SmoothedSpectrum[i] +
                                (1.0f - m_SmoothingFactor) * m_Spectrum[i];
    }
}

// ============================================================================
// FFT ANALYZER — Queries
// ============================================================================

f32 FFTAnalyzer::GetBandEnergy(i32 bandIndex) const {
    if (bandIndex < 0 || bandIndex >= static_cast<i32>(m_SmoothedSpectrum.size())) {
        return 0.0f;
    }
    return m_SmoothedSpectrum[bandIndex];
}

f32 FFTAnalyzer::GetBandFrequency(i32 bandIndex) const {
    if (m_FFTSize <= 0) return 0.0f;
    // Frequency of bin k = k * sampleRate / fftSize
    return static_cast<f32>(bandIndex) * m_SampleRate / static_cast<f32>(m_FFTSize);
}

f32 FFTAnalyzer::GetEnergyInRange(f32 freqLow, f32 freqHigh) const {
    if (m_FFTSize <= 0 || m_SmoothedSpectrum.empty()) return 0.0f;

    f32 binWidth = m_SampleRate / static_cast<f32>(m_FFTSize);
    if (binWidth < Math::EPSILON) return 0.0f;

    i32 binLow = static_cast<i32>(freqLow / binWidth);
    i32 binHigh = static_cast<i32>(freqHigh / binWidth);

    i32 halfSize = static_cast<i32>(m_SmoothedSpectrum.size());
    if (binLow < 0) binLow = 0;
    if (binLow >= halfSize) binLow = halfSize - 1;
    if (binHigh < 0) binHigh = 0;
    if (binHigh >= halfSize) binHigh = halfSize - 1;

    if (binLow > binHigh) return 0.0f;

    f32 sum = 0.0f;
    i32 count = 0;
    for (i32 i = binLow; i <= binHigh; ++i) {
        sum += m_SmoothedSpectrum[i];
        ++count;
    }

    return (count > 0) ? (sum / static_cast<f32>(count)) : 0.0f;
}

f32 FFTAnalyzer::GetBassEnergy() const {
    return GetEnergyInRange(20.0f, 250.0f);
}

f32 FFTAnalyzer::GetMidEnergy() const {
    return GetEnergyInRange(250.0f, 4000.0f);
}

f32 FFTAnalyzer::GetTrebleEnergy() const {
    return GetEnergyInRange(4000.0f, 20000.0f);
}

f32 FFTAnalyzer::GetOverallEnergy() const {
    if (m_SmoothedSpectrum.empty()) return 0.0f;

    f32 sum = 0.0f;
    for (f32 v : m_SmoothedSpectrum) {
        sum += v;
    }
    return sum / static_cast<f32>(m_SmoothedSpectrum.size());
}

// ============================================================================
// MESH DISPLACER — Static Helpers
// ============================================================================

void MeshDisplacer::DisplaceVertices(
    std::vector<ECS::MeshComponent::Vertex>& vertices,
    const std::vector<Math::Vector3>& originalVerts,
    const std::vector<Math::Vector3>& originalNorms,
    const std::vector<f32>& displacements,
    AudioReactiveComponent::DisplacementAxis axis,
    const Math::Vector3& customAxis,
    const Math::Vector3& meshCenter) {

    usize count = vertices.size();
    if (count != originalVerts.size() || count != displacements.size()) return;

    for (usize i = 0; i < count; ++i) {
        f32 d = displacements[i];
        Math::Vector3 direction;

        switch (axis) {
            case AudioReactiveComponent::DisplacementAxis::Normal:
                direction = (i < originalNorms.size()) ? originalNorms[i] : Math::Vector3(0, 1, 0);
                break;

            case AudioReactiveComponent::DisplacementAxis::Radial: {
                Math::Vector3 toVert = originalVerts[i] - meshCenter;
                f32 len = toVert.Length();
                direction = (len > Math::EPSILON) ? toVert / len : Math::Vector3(0, 1, 0);
                break;
            }

            case AudioReactiveComponent::DisplacementAxis::YUp:
                direction = Math::Vector3(0, 1, 0);
                break;

            case AudioReactiveComponent::DisplacementAxis::Custom:
                direction = customAxis.Normalized();
                break;
        }

        vertices[i].position = originalVerts[i] + direction * d;
    }
}

void MeshDisplacer::RecomputeNormals(std::vector<ECS::MeshComponent::Vertex>& vertices,
                                      const std::vector<u32>& indices) {
    // Zero all normals
    for (auto& v : vertices) {
        v.normal = Math::Vector3(0, 0, 0);
    }

    // Accumulate face normals
    usize triCount = indices.size() / 3;
    for (usize t = 0; t < triCount; ++t) {
        u32 i0 = indices[t * 3 + 0];
        u32 i1 = indices[t * 3 + 1];
        u32 i2 = indices[t * 3 + 2];

        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) continue;

        Math::Vector3 edge1 = vertices[i1].position - vertices[i0].position;
        Math::Vector3 edge2 = vertices[i2].position - vertices[i0].position;
        Math::Vector3 faceNormal = edge1.Cross(edge2);

        vertices[i0].normal += faceNormal;
        vertices[i1].normal += faceNormal;
        vertices[i2].normal += faceNormal;
    }

    // Normalize
    for (auto& v : vertices) {
        f32 len = v.normal.Length();
        if (len > Math::EPSILON) {
            v.normal = v.normal / len;
        } else {
            v.normal = Math::Vector3(0, 1, 0);
        }
    }
}

void MeshDisplacer::ComputeMeshBounds(const std::vector<ECS::MeshComponent::Vertex>& vertices,
                                       Math::Vector3& outMin, Math::Vector3& outMax) {
    if (vertices.empty()) {
        outMin = Math::Vector3(0);
        outMax = Math::Vector3(0);
        return;
    }

    outMin = vertices[0].position;
    outMax = vertices[0].position;

    for (usize i = 1; i < vertices.size(); ++i) {
        const auto& p = vertices[i].position;
        outMin.x = Math::Min(outMin.x, p.x);
        outMin.y = Math::Min(outMin.y, p.y);
        outMin.z = Math::Min(outMin.z, p.z);
        outMax.x = Math::Max(outMax.x, p.x);
        outMax.y = Math::Max(outMax.y, p.y);
        outMax.z = Math::Max(outMax.z, p.z);
    }
}

// ============================================================================
// AUDIO REACTIVE SYSTEM — Initialization
// ============================================================================

void AudioReactiveSystem::InitializeComponent(AudioReactiveComponent& comp,
                                               const ECS::MeshComponent& mesh) {
    usize vertCount = mesh.vertices.size();

    // Cache original positions and normals
    comp.originalPositions.resize(vertCount);
    comp.originalNormals.resize(vertCount);
    for (usize i = 0; i < vertCount; ++i) {
        comp.originalPositions[i] = mesh.vertices[i].position;
        comp.originalNormals[i] = mesh.vertices[i].normal;
    }

    // Initialize displacement buffer
    comp.currentDisplacements.assign(vertCount, 0.0f);

    // Compute mesh bounds
    MeshDisplacer::ComputeMeshBounds(mesh.vertices, comp.meshBoundsMin, comp.meshBoundsMax);
    comp.meshCenter = (comp.meshBoundsMin + comp.meshBoundsMax) * 0.5f;

    // Compute mesh radius (max distance from center)
    comp.meshRadius = 0.0f;
    for (usize i = 0; i < vertCount; ++i) {
        f32 dist = (comp.originalPositions[i] - comp.meshCenter).Length();
        if (dist > comp.meshRadius) {
            comp.meshRadius = dist;
        }
    }

    // Initialize FFT analyzer
    if (!m_Analyzer.IsInitialized() || m_Analyzer.GetFFTSize() != comp.fftSize) {
        m_Analyzer.Initialize(comp.fftSize);
    }
    m_Analyzer.SetSmoothingFactor(comp.smoothing);

    // Load test audio if a clip path is specified but no live input
    if (!comp.audioClipPath.empty() && !comp.useLiveInput && m_TestSamples.empty()) {
        // Generate a multi-frequency test signal as placeholder
        // A real implementation would load the audio file here
        m_TestSamples = GenerateTestAudio(110.0f, 4.0f, 44100.0f);

        // Add harmonics for richer spectrum
        auto mid = GenerateTestAudio(880.0f, 4.0f, 44100.0f);
        auto treble = GenerateTestAudio(5000.0f, 4.0f, 44100.0f);
        for (usize i = 0; i < m_TestSamples.size() && i < mid.size(); ++i) {
            m_TestSamples[i] = m_TestSamples[i] * 0.5f + mid[i] * 0.3f + treble[i] * 0.2f;
        }

        m_TestPlaybackPos = 0;
        m_TestSampleRate = 44100.0f;
    }

    comp.initialized = true;
}

// ============================================================================
// AUDIO REACTIVE SYSTEM — Frequency Weight Computation
// ============================================================================

AudioReactiveSystem::FrequencyWeights AudioReactiveSystem::ComputeVertexWeights(
    const AudioReactiveComponent& comp,
    const Math::Vector3& vertexPos,
    const Math::Vector2& vertexUV,
    usize vertexIndex) const {

    FrequencyWeights weights;

    switch (comp.mapping) {
        case AudioReactiveComponent::MappingMode::Uniform:
            // Equal weighting for all frequencies
            weights.bass = 0.333f;
            weights.mid = 0.333f;
            weights.treble = 0.333f;
            break;

        case AudioReactiveComponent::MappingMode::HeightBased: {
            // Map vertex Y to frequency range: low Y = bass, high Y = treble
            f32 rangeY = comp.meshBoundsMax.y - comp.meshBoundsMin.y;
            f32 t = 0.5f;
            if (rangeY > Math::EPSILON) {
                t = Math::Clamp((vertexPos.y - comp.meshBoundsMin.y) / rangeY, 0.0f, 1.0f);
            }

            // Smooth transition: bass at t=0, mid at t=0.5, treble at t=1
            weights.bass = Math::Max(1.0f - t * 3.0f, 0.0f);
            weights.mid = 1.0f - Math::Abs(t - 0.5f) * 4.0f;
            weights.mid = Math::Max(weights.mid, 0.0f);
            weights.treble = Math::Max((t - 0.666f) * 3.0f, 0.0f);

            // Normalize
            f32 total = weights.bass + weights.mid + weights.treble;
            if (total > Math::EPSILON) {
                f32 inv = 1.0f / total;
                weights.bass *= inv;
                weights.mid *= inv;
                weights.treble *= inv;
            }
            break;
        }

        case AudioReactiveComponent::MappingMode::RadialBased: {
            // Map distance from center: inner = bass, outer = treble
            f32 dist = (vertexPos - comp.meshCenter).Length();
            f32 t = (comp.meshRadius > Math::EPSILON) ?
                    Math::Clamp(dist / comp.meshRadius, 0.0f, 1.0f) : 0.5f;

            weights.bass = Math::Max(1.0f - t * 3.0f, 0.0f);
            weights.mid = 1.0f - Math::Abs(t - 0.5f) * 4.0f;
            weights.mid = Math::Max(weights.mid, 0.0f);
            weights.treble = Math::Max((t - 0.666f) * 3.0f, 0.0f);

            f32 total = weights.bass + weights.mid + weights.treble;
            if (total > Math::EPSILON) {
                f32 inv = 1.0f / total;
                weights.bass *= inv;
                weights.mid *= inv;
                weights.treble *= inv;
            }
            break;
        }

        case AudioReactiveComponent::MappingMode::UVBased: {
            // U coordinate maps to frequency: left = bass, right = treble
            f32 t = Math::Clamp(vertexUV.x, 0.0f, 1.0f);

            weights.bass = Math::Max(1.0f - t * 3.0f, 0.0f);
            weights.mid = 1.0f - Math::Abs(t - 0.5f) * 4.0f;
            weights.mid = Math::Max(weights.mid, 0.0f);
            weights.treble = Math::Max((t - 0.666f) * 3.0f, 0.0f);

            f32 total = weights.bass + weights.mid + weights.treble;
            if (total > Math::EPSILON) {
                f32 inv = 1.0f / total;
                weights.bass *= inv;
                weights.mid *= inv;
                weights.treble *= inv;
            }
            break;
        }
    }

    return weights;
}

// ============================================================================
// AUDIO REACTIVE SYSTEM — Update
// ============================================================================

void AudioReactiveSystem::Update(ECS::World* world, f32 dt) {
    if (!world || dt <= 0.0f) return;

    const auto& entities = world->GetEntitiesWithComponent<AudioReactiveComponent>();
    if (entities.empty()) return;

    for (ECS::Entity entity : entities) {
        auto* comp = world->GetComponent<AudioReactiveComponent>(entity);
        auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
        if (!comp || !mesh || mesh->vertices.empty()) continue;

        // Initialize on first use
        if (!comp->initialized) {
            InitializeComponent(*comp, *mesh);
        }

        // Ensure vertex count hasn't changed (mesh was swapped)
        if (comp->originalPositions.size() != mesh->vertices.size()) {
            comp->initialized = false;
            InitializeComponent(*comp, *mesh);
        }

        // --- Step 1: Get audio samples and run FFT ---
        bool hasAudio = false;

        if (!m_TestSamples.empty()) {
            // Feed a window of samples to the FFT
            i32 fftSize = m_Analyzer.GetFFTSize();
            if (fftSize > 0) {
                // Advance playback position
                usize samplesThisFrame = static_cast<usize>(m_TestSampleRate * dt);
                if (samplesThisFrame < 1) samplesThisFrame = 1;

                // Wrap around
                if (m_TestPlaybackPos >= m_TestSamples.size()) {
                    m_TestPlaybackPos = 0;
                }

                // Gather samples for FFT window
                std::vector<f32> windowSamples(fftSize);
                for (i32 i = 0; i < fftSize; ++i) {
                    usize srcIdx = (m_TestPlaybackPos + static_cast<usize>(i)) % m_TestSamples.size();
                    windowSamples[i] = m_TestSamples[srcIdx];
                }

                m_Analyzer.ProcessSamples(windowSamples.data(), fftSize);
                m_TestPlaybackPos += samplesThisFrame;
                hasAudio = true;
            }
        }

        // --- Step 2: Compute per-vertex displacement targets ---
        f32 bassEnergy = hasAudio ? m_Analyzer.GetBassEnergy() : 0.0f;
        f32 midEnergy = hasAudio ? m_Analyzer.GetMidEnergy() : 0.0f;
        f32 trebleEnergy = hasAudio ? m_Analyzer.GetTrebleEnergy() : 0.0f;

        usize vertCount = mesh->vertices.size();

        for (usize i = 0; i < vertCount; ++i) {
            // Compute frequency weights for this vertex
            FrequencyWeights w = ComputeVertexWeights(
                *comp, comp->originalPositions[i], mesh->vertices[i].uv, i);

            // Weighted displacement target
            f32 targetDisplacement = 0.0f;
            if (hasAudio) {
                targetDisplacement = (w.bass * bassEnergy * comp->bassScale +
                                     w.mid * midEnergy * comp->midScale +
                                     w.treble * trebleEnergy * comp->trebleScale) *
                                    comp->globalScale;
            }

            // --- Step 3: Smooth animation ---
            f32 currentDisp = comp->currentDisplacements[i];
            if (hasAudio) {
                // Move toward target at responseSpeed
                f32 lerpT = Math::Clamp(comp->responseSpeed * dt, 0.0f, 1.0f);
                currentDisp = Math::Lerp(currentDisp, targetDisplacement, lerpT);
            } else {
                // Return to rest at returnSpeed
                f32 lerpT = Math::Clamp(comp->returnSpeed * dt, 0.0f, 1.0f);
                currentDisp = Math::Lerp(currentDisp, 0.0f, lerpT);
            }

            comp->currentDisplacements[i] = currentDisp;
        }

        // --- Step 4: Apply displacement ---
        MeshDisplacer::DisplaceVertices(
            mesh->vertices,
            comp->originalPositions,
            comp->originalNormals,
            comp->currentDisplacements,
            comp->axis,
            comp->customAxis,
            comp->meshCenter);

        // --- Step 5: Recompute normals ---
        if (!mesh->indices.empty()) {
            MeshDisplacer::RecomputeNormals(mesh->vertices, mesh->indices);
        }

        // Mark AABB as dirty since vertices moved
        mesh->aabbDirty = true;
    }
}

// ============================================================================
// AUDIO REACTIVE SYSTEM — Test Audio Generation
// ============================================================================

std::vector<f32> AudioReactiveSystem::GenerateTestAudio(f32 frequency, f32 duration, f32 sampleRate) {
    if (frequency <= 0.0f || duration <= 0.0f || sampleRate <= 0.0f) return {};

    i32 sampleCount = static_cast<i32>(duration * sampleRate);
    if (sampleCount <= 0) return {};

    // Cap to reasonable maximum (10 seconds at 44100)
    sampleCount = static_cast<i32>(Math::Min(static_cast<f32>(sampleCount), 441000.0f));

    std::vector<f32> samples(sampleCount);

    f32 angularFreq = Math::PI_2 * frequency;
    f32 invSR = 1.0f / sampleRate;

    for (i32 i = 0; i < sampleCount; ++i) {
        f32 t = static_cast<f32>(i) * invSR;
        samples[i] = Math::Sin(angularFreq * t);

        // Apply a gentle amplitude envelope (fade in/out) to avoid clicks
        f32 fadeTime = 0.01f; // 10ms fade
        if (t < fadeTime) {
            samples[i] *= t / fadeTime;
        } else if (t > duration - fadeTime) {
            samples[i] *= (duration - t) / fadeTime;
        }
    }

    return samples;
}

} // namespace Effects
} // namespace Enjin
