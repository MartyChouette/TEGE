#pragma once

// Precompiled header for EnjinEngine and EnjinCore.
//
// What belongs here: headers that nearly every translation unit already
// includes, and that almost never change. Log.h alone is included by 306 files.
//
// What must NOT go here, however tempting: the heavy leaf headers —
// Gameplay.h (3,111 lines, 78 includers), RenderSystem.h, EditorLayer.h,
// vulkan/vulkan.h. Putting one of those in makes every translation unit in the
// target depend on it, so touching EditorLayer.h would go from rebuilding 17
// files to rebuilding all 390. That inversion is the usual way a precompiled
// header ends up making a project slower, and it is easy to do by accident
// because adding a header here always looks like a speed-up.
//
// The rule of thumb: if a header changes during ordinary feature work, it does
// not belong here.
//
// Measured on the engine target, clean: 129s without, 111s with. Applies on
// every platform including the Emscripten build, which was checked rather than
// assumed.

// --- Standard library: pulled in by nearly everything, never changes --------
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <unordered_map>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <utility>
#include <atomic>
#include <mutex>
#include <optional>

// --- Engine foundation: stable, and already in almost every file ------------
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
