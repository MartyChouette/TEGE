#pragma once

#include "Enjin/Platform/Types.h"
#include <string_view>

namespace Enjin {

// ============================================================================
// Compile-time utilities using C++20 consteval / constexpr
// ============================================================================

/// Compile-time string hash using DJB2 algorithm.
/// The compiler evaluates this entirely at compile time — zero runtime cost.
///
/// Usage:
///   constexpr auto id = StringHash("TransformComponent");
///   // or force compile-time evaluation:
///   constexpr auto id2 = CompileTimeHash("MeshComponent");
///
constexpr u64 StringHash(std::string_view str) noexcept {
    u64 hash = 5381;
    for (char c : str) {
        hash = ((hash << 5) + hash) + static_cast<u64>(c);  // hash * 33 + c
    }
    return hash;
}

/// consteval version — guaranteed compile-time only.
/// Use this when you want a hard compiler error if the value cannot be
/// computed at compile time.
consteval u64 CompileTimeHash(std::string_view str) noexcept {
    return StringHash(str);
}

/// Compile-time FNV-1a hash (alternative algorithm, better distribution).
constexpr u64 FNV1aHash(std::string_view str) noexcept {
    u64 hash = 14695981039346656037ULL;  // FNV offset basis
    for (char c : str) {
        hash ^= static_cast<u64>(c);
        hash *= 1099511628211ULL;  // FNV prime
    }
    return hash;
}

/// consteval FNV-1a — compile-time only.
consteval u64 CompileTimeFNV1a(std::string_view str) noexcept {
    return FNV1aHash(str);
}

/// User-defined literal for compile-time string hashing.
/// Usage:  constexpr auto id = "TransformComponent"_hash;
consteval u64 operator""_hash(const char* str, usize len) noexcept {
    return StringHash(std::string_view(str, len));
}

// ============================================================================
// Compile-time math helpers
// ============================================================================

/// Compile-time power of 2 check.
consteval bool IsPowerOfTwo(u64 value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

/// Compile-time next power of 2.
consteval u64 NextPowerOfTwo(u64 value) noexcept {
    if (value == 0) return 1;
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return value + 1;
}

/// Compile-time log2 (for power-of-two values).
consteval u32 Log2(u64 value) noexcept {
    u32 result = 0;
    while (value >>= 1) {
        ++result;
    }
    return result;
}

} // namespace Enjin
