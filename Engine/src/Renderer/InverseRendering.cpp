#include "Enjin/Renderer/InverseRendering.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Renderer {

// --- Loss Computation ---

f32 InverseRenderer::ComputeLoss(const std::vector<u8>& currentImage,
                                  const std::vector<u8>& targetImage,
                                  u32 width, u32 height,
                                  LossFunction lossType)
{
    u64 pixelCount = static_cast<u64>(width) * static_cast<u64>(height);
    u64 byteCount = pixelCount * 4;

    if (currentImage.size() < byteCount || targetImage.size() < byteCount) {
        return Math::FLOAT_MAX;
    }
    if (pixelCount == 0) return 0.0f;

    f32 totalLoss = 0.0f;

    switch (lossType) {
        case LossFunction::MSE: {
            // Mean Squared Error over RGB channels
            for (u64 i = 0; i < byteCount; i += 4) {
                for (u32 c = 0; c < 3; ++c) {  // RGB only, skip alpha
                    f32 diff = (static_cast<f32>(currentImage[i + c]) - static_cast<f32>(targetImage[i + c])) / 255.0f;
                    totalLoss += diff * diff;
                }
            }
            totalLoss /= static_cast<f32>(pixelCount * 3);
            break;
        }

        case LossFunction::L1: {
            // Mean Absolute Error
            for (u64 i = 0; i < byteCount; i += 4) {
                for (u32 c = 0; c < 3; ++c) {
                    f32 diff = Math::Abs(static_cast<f32>(currentImage[i + c]) - static_cast<f32>(targetImage[i + c])) / 255.0f;
                    totalLoss += diff;
                }
            }
            totalLoss /= static_cast<f32>(pixelCount * 3);
            break;
        }

        case LossFunction::SSIM: {
            // Approximate SSIM using luminance only
            // SSIM = (2*muX*muY + C1)(2*sigmaXY + C2) / ((muX^2 + muY^2 + C1)(sigmaX^2 + sigmaY^2 + C2))
            // We compute a global SSIM (not windowed for performance)
            f32 c1 = 0.0001f;  // (0.01)^2 in [0,1] range
            f32 c2 = 0.0009f;  // (0.03)^2

            f32 sumX = 0.0f, sumY = 0.0f;
            f32 sumX2 = 0.0f, sumY2 = 0.0f, sumXY = 0.0f;

            for (u64 i = 0; i < byteCount; i += 4) {
                // Luminance: 0.299*R + 0.587*G + 0.114*B
                f32 lumX = (0.299f * currentImage[i] + 0.587f * currentImage[i + 1] + 0.114f * currentImage[i + 2]) / 255.0f;
                f32 lumY = (0.299f * targetImage[i] + 0.587f * targetImage[i + 1] + 0.114f * targetImage[i + 2]) / 255.0f;

                sumX += lumX;
                sumY += lumY;
                sumX2 += lumX * lumX;
                sumY2 += lumY * lumY;
                sumXY += lumX * lumY;
            }

            f32 n = static_cast<f32>(pixelCount);
            f32 muX = sumX / n;
            f32 muY = sumY / n;
            f32 sigX2 = sumX2 / n - muX * muX;
            f32 sigY2 = sumY2 / n - muY * muY;
            f32 sigXY = sumXY / n - muX * muY;

            f32 ssim = ((2.0f * muX * muY + c1) * (2.0f * sigXY + c2)) /
                       ((muX * muX + muY * muY + c1) * (sigX2 + sigY2 + c2));

            // Return 1 - SSIM so that 0 = perfect match
            totalLoss = 1.0f - ssim;
            break;
        }
    }

    return totalLoss;
}

std::vector<f32> InverseRenderer::ComputeErrorMap(const std::vector<u8>& currentImage,
                                                    const std::vector<u8>& targetImage,
                                                    u32 width, u32 height)
{
    u64 pixelCount = static_cast<u64>(width) * static_cast<u64>(height);
    u64 byteCount = pixelCount * 4;
    std::vector<f32> errorMap(pixelCount, 0.0f);

    if (currentImage.size() < byteCount || targetImage.size() < byteCount) {
        return errorMap;
    }

    for (u64 p = 0; p < pixelCount; ++p) {
        u64 base = p * 4;
        f32 err = 0.0f;
        for (u32 c = 0; c < 3; ++c) {
            f32 diff = (static_cast<f32>(currentImage[base + c]) - static_cast<f32>(targetImage[base + c])) / 255.0f;
            err += diff * diff;
        }
        errorMap[p] = Math::Sqrt(err / 3.0f);  // Normalized RMS per pixel
    }

    return errorMap;
}

// --- Gradient Computation ---

f32 InverseRenderer::ComputeGradient(InverseRenderConfig& config, usize paramIndex, f32 currentLoss) {
    if (!m_RenderCallback || !m_ApplyParamCallback) return 0.0f;
    if (paramIndex >= config.parameters.size()) return 0.0f;

    auto& param = config.parameters[paramIndex];
    f32 originalValue = param.currentValue;
    f32 epsilon = config.epsilon;

    // Perturb parameter
    param.currentValue = originalValue + epsilon;
    param.currentValue = Math::Clamp(param.currentValue, param.minValue, param.maxValue);
    m_ApplyParamCallback(param.param, param.currentValue);

    // Render with perturbed value
    m_ScratchPixels.clear();
    m_ScratchPixels.resize(static_cast<usize>(config.imageWidth) * config.imageHeight * 4, 0);
    if (!m_RenderCallback(m_ScratchPixels, config.imageWidth, config.imageHeight)) {
        param.currentValue = originalValue;
        m_ApplyParamCallback(param.param, param.currentValue);
        return 0.0f;
    }

    // Compute perturbed loss
    f32 perturbedLoss = ComputeLoss(m_ScratchPixels, config.targetImage,
                                     config.imageWidth, config.imageHeight,
                                     config.lossFunction);

    // Restore original value
    param.currentValue = originalValue;
    m_ApplyParamCallback(param.param, param.currentValue);

    // Finite difference gradient
    f32 actualEpsilon = (originalValue + epsilon) - originalValue;
    if (Math::Abs(actualEpsilon) < Math::EPSILON) return 0.0f;

    return (perturbedLoss - currentLoss) / actualEpsilon;
}

// --- Optimization ---

f32 InverseRenderer::OptimizationStep(InverseRenderConfig& config, u32 iteration) {
    if (!m_RenderCallback || !m_ApplyParamCallback) return Math::FLOAT_MAX;

    // Apply all current parameter values
    for (const auto& param : config.parameters) {
        m_ApplyParamCallback(param.param, param.currentValue);
    }

    // Render current state
    m_ScratchPixels.clear();
    m_ScratchPixels.resize(static_cast<usize>(config.imageWidth) * config.imageHeight * 4, 0);
    if (!m_RenderCallback(m_ScratchPixels, config.imageWidth, config.imageHeight)) {
        return Math::FLOAT_MAX;
    }

    // Compute current loss
    f32 currentLoss = ComputeLoss(m_ScratchPixels, config.targetImage,
                                   config.imageWidth, config.imageHeight,
                                   config.lossFunction);

    // Compute gradient for each parameter
    for (usize i = 0; i < config.parameters.size(); ++i) {
        config.parameters[i].gradient = ComputeGradient(config, i, currentLoss);
    }

    // Adam optimizer update
    f32 t = static_cast<f32>(iteration + 1);
    for (auto& param : config.parameters) {
        // Update biased first moment estimate
        param.momentFirst = config.adamBeta1 * param.momentFirst + (1.0f - config.adamBeta1) * param.gradient;
        // Update biased second moment estimate
        param.momentSecond = config.adamBeta2 * param.momentSecond + (1.0f - config.adamBeta2) * param.gradient * param.gradient;

        // Bias-corrected estimates
        f32 mHat = param.momentFirst / (1.0f - Math::Pow(config.adamBeta1, t));
        f32 vHat = param.momentSecond / (1.0f - Math::Pow(config.adamBeta2, t));

        // Update parameter
        param.currentValue -= config.learningRate * mHat / (Math::Sqrt(vHat) + config.adamEpsilon);
        param.currentValue = Math::Clamp(param.currentValue, param.minValue, param.maxValue);
    }

    return currentLoss;
}

InverseRenderResult InverseRenderer::Run(InverseRenderConfig& config) {
    InverseRenderResult result;

    if (!m_RenderCallback || !m_ApplyParamCallback) {
        ENJIN_LOG_ERROR(Renderer, "InverseRenderer::Run() — render or parameter callback not set");
        return result;
    }

    if (config.targetImage.empty() || config.imageWidth == 0 || config.imageHeight == 0) {
        ENJIN_LOG_ERROR(Renderer, "InverseRenderer::Run() — no target image provided");
        return result;
    }

    if (config.parameters.empty()) {
        ENJIN_LOG_WARN(Renderer, "InverseRenderer::Run() — no parameters to optimize");
        return result;
    }

    ENJIN_LOG_INFO(Renderer, "InverseRenderer: starting optimization with %u parameters, %u max iterations",
                   static_cast<u32>(config.parameters.size()), config.maxIterations);

    result.lossHistory.reserve(config.maxIterations);
    f32 previousLoss = Math::FLOAT_MAX;

    for (u32 iter = 0; iter < config.maxIterations; ++iter) {
        f32 loss = OptimizationStep(config, iter);
        result.lossHistory.push_back(loss);

        if (iter == 0) {
            result.initialLoss = loss;
        }

        if (config.verbose) {
            ENJIN_LOG_INFO(Renderer, "  Iteration %u: loss = %.8f", iter, loss);
        }

        // Check convergence
        if (iter > 0 && Math::Abs(previousLoss - loss) < config.convergenceThreshold) {
            ENJIN_LOG_INFO(Renderer, "InverseRenderer: converged at iteration %u (loss = %.8f)", iter, loss);
            result.converged = true;
            result.iterationsRun = iter + 1;
            result.finalLoss = loss;
            result.optimizedParameters = config.parameters;
            return result;
        }

        previousLoss = loss;
    }

    result.iterationsRun = config.maxIterations;
    result.finalLoss = previousLoss;
    result.optimizedParameters = config.parameters;

    ENJIN_LOG_INFO(Renderer, "InverseRenderer: completed %u iterations (final loss = %.8f)",
                   config.maxIterations, result.finalLoss);

    return result;
}

std::vector<u8> InverseRenderer::RenderToPixels(u32 width, u32 height) {
    std::vector<u8> pixels;
    if (!m_RenderCallback) {
        ENJIN_LOG_WARN(Renderer, "InverseRenderer::RenderToPixels() — no render callback set");
        return pixels;
    }

    pixels.resize(static_cast<usize>(width) * height * 4, 0);
    if (!m_RenderCallback(pixels, width, height)) {
        ENJIN_LOG_WARN(Renderer, "InverseRenderer::RenderToPixels() — render callback failed");
        pixels.clear();
    }
    return pixels;
}

} // namespace Renderer
} // namespace Enjin
