#pragma once

// Enjin Plugin SDK — include this single header in your plugin
#include "Enjin/Plugin/PluginSystem.h"
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/Entity.h"

// Export macros for plugin DLLs
#ifdef _WIN32
    #define ENJIN_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
    #define ENJIN_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Convenience macro for plugin factory implementation
#define ENJIN_IMPLEMENT_PLUGIN(PluginClass) \
    ENJIN_PLUGIN_EXPORT Enjin::Plugin::IPlugin* CreatePlugin() { return new PluginClass(); } \
    ENJIN_PLUGIN_EXPORT void DestroyPlugin(Enjin::Plugin::IPlugin* p) { delete p; }
