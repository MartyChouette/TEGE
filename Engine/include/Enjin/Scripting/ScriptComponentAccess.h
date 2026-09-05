#pragma once
// Fetching a component for a script binding, and SAYING SO when it is missing.
//
// A setter called on an entity that lacks the component it edits used to do
// nothing and report nothing. Three real instances from one project, each
// costing hours:
//
//   Material_SetBaseColor on an entity with no material  -> invisible board
//   HUD_SetText on an entity carrying a uiCanvas         -> prompt never appeared
//   Particle_ApplyPreset with no particleEmitter         -> no particles
//
// In every case the entity existed, the script ran, the call returned, and the
// screen stayed wrong. There is no way to tell that from working code, which
// is why one of them was diagnosed as a rendering bug.
//
// The check costs a pointer compare that the binding already does. The only
// new thing is that it speaks.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"

// Defined at GLOBAL scope in ScriptBindings.cpp, which is where every other
// binding translation unit externs it from. Declaring it inside
// Enjin::Scripting names a different symbol that nothing defines, and the link
// only survived while the objects that include this header were stale.
extern Enjin::ECS::World* s_BindingsWorld;

namespace Enjin {
namespace Scripting {

// Warns once per entity + component name. A setter called every frame would
// otherwise bury the console under one true message.
ENJIN_API void WarnMissingScriptComponent(u64 entityId, const char* componentName,
                                          const char* bindingName);

template <typename T>
T* ScriptComponentOrWarn(u64 entityId, const char* componentName, const char* bindingName) {
    if (!s_BindingsWorld) return nullptr;
    T* c = s_BindingsWorld->GetComponent<T>(static_cast<ECS::Entity>(entityId));
    if (!c) WarnMissingScriptComponent(entityId, componentName, bindingName);
    return c;
}

} // namespace Scripting
} // namespace Enjin

// The binding's own name comes from __func__, so a site never has to repeat it
// and cannot get it wrong.
#define ENJIN_SCRIPT_COMPONENT(T, idExpr) \
    ::Enjin::Scripting::ScriptComponentOrWarn<T>((idExpr), #T, __func__)
