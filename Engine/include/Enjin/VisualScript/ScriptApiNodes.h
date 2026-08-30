#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

class asIScriptEngine;

namespace Enjin {
namespace VisualScript {

// VS coverage codegen: walks the AngelScript binding registry (the same
// reflection GenerateApiStub uses) and registers ONE visual-script node per
// global engine function with a marshalable signature. Closes the ~25%
// node-coverage gap without hand-writing thousands of nodes: the node's
// execute lambda captures the asIScriptFunction and calls it through the
// context pool at runtime.
//
// - Skips functions whose name already exists as a hand-made node typeId
//   (hand nodes have nicer pins) and signatures with unsupported types
//   (handles, arrays, refs-out).
// - Getters (_Get/_Is/_Has/_Count after the prefix) become PURE nodes;
//   everything else gets flow pins.
// - Idempotent; call once when the script engine with full bindings exists.
//
// Returns the number of nodes registered.
ENJIN_API u32 RegisterScriptApiNodes(asIScriptEngine* engine);

} // namespace VisualScript
} // namespace Enjin
