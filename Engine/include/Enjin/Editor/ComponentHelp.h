#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"
#include <vector>

// ---------------------------------------------------------------------------
// ComponentHelp — the "GarageBand for game engines" self-documenting layer.
//
// Every inspector panel should answer three questions without the user leaving
// the window: WHAT does this do, HOW do I use it, and WHAT is it connected to.
// Rather than hand-write that prose into 140 panels, each panel calls
// DrawComponentHelp("<serializerKey>", world, entity) once, right after its
// header opens, and this module renders a consistent help block driven by a
// single central registry.
//
// The "connections" line is live: it inspects the actual entity and tells the
// user which partner components are present (green check) or missing (with an
// inline "Add" button), plus informational links like "pulled by Script".
// ---------------------------------------------------------------------------

namespace Enjin {
namespace ECS { class World; }

namespace Editor {

enum class RelationKind : u8 {
    Requires,        // must have this partner to work (missing = offer Add)
    PairsWith,       // commonly used together (informational + presence)
    Paints,          // this component writes into the partner (e.g. DungeonGenerator -> Tilemap)
    PulledByScript,  // gameplay code reads/pulls from this component
    DrivenByScript,  // gameplay code writes into this component
    FeedsRenderer,   // consumed by the render pipeline
    FeedsPhysics     // consumed by the physics backend
};

// One relationship from a component to something else in the engine. `present`
// and `add` are optional function pointers used only when the relation targets
// another live component (so the panel can show a check / offer to add it).
struct ComponentRelation {
    RelationKind kind;
    const char*  label;                              // e.g. "Tilemap", "Script", "Camera"
    bool (*present)(ECS::World*, ECS::Entity) = nullptr; // null = not a live component check
    void (*add)(ECS::World*, ECS::Entity)     = nullptr; // null = no inline Add offered
};

struct ComponentHelp {
    const char* whatItDoes   = nullptr;  // one sentence, plain language
    const char* howToUse     = nullptr;  // short, imperative; may be multi-line
    const char* scriptSnippet = nullptr; // optional monospace example (null = none)
    std::vector<ComponentRelation> relations;
};

// Look up help by the component's serializer key ("randomBag", "dungeonGenerator",
// "transform", ...). Returns nullptr if the component has no registered help yet.
const ComponentHelp* GetComponentHelp(const char* key);

// Render the standard help block for `key` inside the current panel. Safe to call
// with an unknown key (renders nothing). world/entity drive the live connections.
void DrawComponentHelp(const char* key, ECS::World* world, ECS::Entity entity);

} // namespace Editor
} // namespace Enjin
