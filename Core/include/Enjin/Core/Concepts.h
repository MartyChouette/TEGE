#pragma once

#include "Enjin/Platform/Types.h"
#include <concepts>
#include <type_traits>

namespace Enjin {

// ============================================================================
// C++20 Concepts for type-safe template constraints
// ============================================================================

// -- ECS Concepts --

/// A Component must be default-constructible and destructible (value-type semantics).
template<typename T>
concept IsComponent = std::default_initializable<T> && std::destructible<T>;

/// A System must inherit from ISystem (checked via duck-typing: must have Update(f32)).
template<typename T>
concept IsSystem = requires(T sys, f32 dt) {
    { sys.Update(dt) } -> std::same_as<void>;
};

// -- Arithmetic Concepts --

/// Any integral or floating-point numeric type.
template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

/// Floating-point types only (for math operations).
template<typename T>
concept FloatingPoint = std::floating_point<T>;

// -- Asset Concepts --

/// An Asset type must be loadable from a file path and report validity.
template<typename T>
concept IsAsset = requires(T asset, const char* path) {
    { asset.LoadFromFile(path) } -> std::convertible_to<bool>;
    { asset.IsValid() } -> std::convertible_to<bool>;
};

// -- Renderer Concepts --

/// A GPU resource that can be created and destroyed.
template<typename T>
concept IsGPUResource = requires(T resource) {
    { resource.Destroy() } -> std::same_as<void>;
};

} // namespace Enjin
