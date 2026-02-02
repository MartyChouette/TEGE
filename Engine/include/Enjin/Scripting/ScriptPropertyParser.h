#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Components/Script.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Scripting {

// Parses [Property] metadata annotations from AngelScript source files
// before compilation. Extracts property names, types, defaults, and hints.
//
// Supported annotations:
//   [Property] float moveSpeed = 5.0f;
//   [Property, Range(0, 100)] int health = 100;
//   [Property] Vector3 targetPos;
//   [Property] Entity targetEntity;
//   [Property] string label = "default";
struct ParsedProperty {
    std::string name;
    std::string typeName;       // "float", "int", "bool", "string", "Vector2", "Vector3", "Vector4", "Entity"
    std::string defaultLiteral; // Raw text of default value, e.g. "5.0f", "true", "\"hello\""
    ECS::ScriptPropertyType type = ECS::ScriptPropertyType::Float;
    f32 rangeMin = 0.0f;
    f32 rangeMax = 0.0f;
    bool hasRange = false;
    std::string tooltip;
};

// Parse all [Property]-annotated fields from a .as source string.
// Returns one ParsedProperty per annotated field.
ENJIN_API std::vector<ParsedProperty> ParseProperties(const std::string& source);

// Strip [Property] annotations from source so AngelScript can compile it.
// AngelScript doesn't understand metadata decorators, so we remove them
// and replace with plain member declarations.
ENJIN_API std::string StripPropertyAnnotations(const std::string& source);

// Convert a ParsedProperty to a ScriptProperty with default values filled in
ENJIN_API ECS::ScriptProperty ToScriptProperty(const ParsedProperty& parsed);

} // namespace Scripting
} // namespace Enjin
