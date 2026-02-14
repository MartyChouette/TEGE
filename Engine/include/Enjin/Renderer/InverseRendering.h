#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <vector>
#include <string>
#include <functional>

/**
 * @file InverseRendering.h
 * @brief CPU-based inverse/differentiable rendering via finite differences
 *
 * Optimizes scene parameters (light direction, material colors, camera position,
 * etc.) to match a target image. Uses gradient descent with finite-difference
 * gradient estimation: render the scene, perturb one parameter, render again,
 * and compute the gradient from the difference in loss.
 *
 * The actual rendering is delegated to the existing engine via a user-supplied
 * render callback (e.g., RenderSystem::RenderToTarget → readback to CPU pixels).
 */

namespace Enjin {
namespace Renderer {

// Which scene parameter to optimize
enum class OptimizableParam : u8 {
    LightDirectionX,
    LightDirectionY,
    LightDirectionZ,
    LightColorR,
    LightColorG,
    LightColorB,
    LightIntensity,
    MaterialColorR,
    MaterialColorG,
    MaterialColorB,
    MaterialRoughness,
    MaterialMetallic,
    CameraPositionX,
    CameraPositionY,
    CameraPositionZ,
    CameraFOV,
    Count
};

inline const char* OptimizableParamToString(OptimizableParam param) {
    switch (param) {
        case OptimizableParam::LightDirectionX:   return "LightDirection.X";
        case OptimizableParam::LightDirectionY:   return "LightDirection.Y";
        case OptimizableParam::LightDirectionZ:   return "LightDirection.Z";
        case OptimizableParam::LightColorR:       return "LightColor.R";
        case OptimizableParam::LightColorG:       return "LightColor.G";
        case OptimizableParam::LightColorB:       return "LightColor.B";
        case OptimizableParam::LightIntensity:    return "LightIntensity";
        case OptimizableParam::MaterialColorR:    return "MaterialColor.R";
        case OptimizableParam::MaterialColorG:    return "MaterialColor.G";
        case OptimizableParam::MaterialColorB:    return "MaterialColor.B";
        case OptimizableParam::MaterialRoughness: return "MaterialRoughness";
        case OptimizableParam::MaterialMetallic:  return "MaterialMetallic";
        case OptimizableParam::CameraPositionX:   return "CameraPosition.X";
        case OptimizableParam::CameraPositionY:   return "CameraPosition.Y";
        case OptimizableParam::CameraPositionZ:   return "CameraPosition.Z";
        case OptimizableParam::CameraFOV:         return "CameraFOV";
        default: return "Unknown";
    }
}

// Loss function type
enum class LossFunction : u8 {
    MSE,     // Mean Squared Error
    L1,      // Mean Absolute Error
    SSIM     // Structural Similarity (approximate, luminance-only)
};

// State of a single parameter being optimized
struct ParameterState {
    OptimizableParam param = OptimizableParam::LightDirectionX;
    f32 currentValue = 0.0f;
    f32 gradient = 0.0f;
    f32 minValue = -10.0f;
    f32 maxValue = 10.0f;

    // Adam optimizer state
    f32 momentFirst = 0.0f;   // First moment estimate (mean)
    f32 momentSecond = 0.0f;  // Second moment estimate (variance)
};

// Configuration for inverse rendering optimization
struct InverseRenderConfig {
    // Target image to match (RGBA8 format, row-major)
    std::vector<u8> targetImage;
    u32 imageWidth = 0;
    u32 imageHeight = 0;

    // Optimization settings
    f32 learningRate = 0.01f;
    f32 epsilon = 0.001f;          // Finite difference step size
    u32 maxIterations = 100;
    LossFunction lossFunction = LossFunction::MSE;

    // Adam optimizer hyperparameters
    f32 adamBeta1 = 0.9f;
    f32 adamBeta2 = 0.999f;
    f32 adamEpsilon = 1e-8f;

    // Convergence
    f32 convergenceThreshold = 1e-6f;  // Stop if loss change < this
    bool verbose = false;               // Log progress each iteration

    // Parameters to optimize
    std::vector<ParameterState> parameters;
};

// Result of the optimization process
struct InverseRenderResult {
    bool converged = false;
    u32 iterationsRun = 0;
    f32 finalLoss = 0.0f;
    f32 initialLoss = 0.0f;
    std::vector<ParameterState> optimizedParameters;

    // Loss history for visualization
    std::vector<f32> lossHistory;
};

/**
 * @brief CPU-based inverse renderer using finite-difference gradient descent
 *
 * Usage:
 * 1. Set up an InverseRenderConfig with a target image and parameters to optimize
 * 2. Provide a render callback that renders the scene to CPU pixels
 * 3. Provide a parameter-apply callback that sets scene parameters from values
 * 4. Call Run() to execute the optimization
 */
class ENJIN_API InverseRenderer {
public:
    InverseRenderer() = default;
    ~InverseRenderer() = default;

    /**
     * @brief Callback type: render the current scene to RGBA8 pixels
     *
     * The callback should render the scene at the given resolution and
     * write the result into the provided buffer (width * height * 4 bytes).
     * Return true on success.
     */
    using RenderCallback = std::function<bool(std::vector<u8>& outPixels, u32 width, u32 height)>;

    /**
     * @brief Callback type: apply a parameter value to the scene
     *
     * Called before each render to set the current parameter values.
     */
    using ApplyParamCallback = std::function<void(OptimizableParam param, f32 value)>;

    /**
     * @brief Set the render callback
     */
    void SetRenderCallback(RenderCallback callback) { m_RenderCallback = std::move(callback); }

    /**
     * @brief Set the parameter application callback
     */
    void SetApplyParamCallback(ApplyParamCallback callback) { m_ApplyParamCallback = std::move(callback); }

    /**
     * @brief Compute loss between two images
     *
     * @param currentImage Current rendered image (RGBA8)
     * @param targetImage Target image to match (RGBA8)
     * @param width Image width
     * @param height Image height
     * @param lossType Which loss function to use
     * @return Scalar loss value
     */
    static f32 ComputeLoss(const std::vector<u8>& currentImage,
                           const std::vector<u8>& targetImage,
                           u32 width, u32 height,
                           LossFunction lossType = LossFunction::MSE);

    /**
     * @brief Compute per-pixel error map (for visualization)
     *
     * @param currentImage Current rendered image (RGBA8)
     * @param targetImage Target image to match (RGBA8)
     * @param width Image width
     * @param height Image height
     * @return Per-pixel error as grayscale float (0.0 = match, 1.0 = max error)
     */
    static std::vector<f32> ComputeErrorMap(const std::vector<u8>& currentImage,
                                            const std::vector<u8>& targetImage,
                                            u32 width, u32 height);

    /**
     * @brief Compute the gradient for a single parameter via finite differences
     *
     * Renders the scene with the parameter at (value) and (value + epsilon),
     * then estimates the gradient as (loss_perturbed - loss_base) / epsilon.
     *
     * @param config The optimization configuration
     * @param paramIndex Index into config.parameters
     * @param currentLoss The current loss value (avoids re-rendering)
     * @return Estimated gradient
     */
    f32 ComputeGradient(InverseRenderConfig& config, usize paramIndex, f32 currentLoss);

    /**
     * @brief Execute one optimization step (gradient descent)
     *
     * Computes gradients for all parameters via finite differences,
     * then applies Adam optimizer update.
     *
     * @param config The optimization configuration (parameters updated in place)
     * @param iteration Current iteration number (for Adam bias correction)
     * @return Loss after this step
     */
    f32 OptimizationStep(InverseRenderConfig& config, u32 iteration);

    /**
     * @brief Run the full optimization loop
     *
     * @param config The optimization configuration
     * @return Optimization result with final parameters and loss history
     */
    InverseRenderResult Run(InverseRenderConfig& config);

    /**
     * @brief Render the current scene to RGBA8 pixels using the registered callback
     *
     * @param width Desired image width
     * @param height Desired image height
     * @return RGBA8 pixel buffer (width * height * 4 bytes), or empty on failure
     */
    std::vector<u8> RenderToPixels(u32 width, u32 height);

    /**
     * @brief Check if this renderer has valid callbacks configured
     */
    bool IsReady() const { return m_RenderCallback && m_ApplyParamCallback; }

private:
    RenderCallback m_RenderCallback;
    ApplyParamCallback m_ApplyParamCallback;

    // Scratch buffer to avoid per-render allocation
    std::vector<u8> m_ScratchPixels;
};

} // namespace Renderer
} // namespace Enjin
