#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS {

// Types of properties that can be exposed to the editor via [Property] metadata
enum class ScriptPropertyType : u8 {
    Int,
    Float,
    Bool,
    String,
    Vector2,
    Vector3,
    Vector4,
    Entity,
    Enum,
    EntityArray   // array<uint64> / array<Entity> — a drag-assignable list of entity refs
};

// Value storage for script properties
struct ScriptPropertyValue {
    i32 intVal = 0;
    f32 floatVal = 0.0f;
    bool boolVal = false;
    std::string stringVal;
    Math::Vector2 vec2Val;
    Math::Vector3 vec3Val;
    Math::Vector4 vec4Val;
    u64 entityVal = 0;
    std::vector<std::string> enumNames;   // For Enum display
    std::vector<u64> entityArrayVal;       // For EntityArray
};

// A single exposed property from a script class
struct ScriptProperty {
    std::string name;
    ScriptPropertyType type = ScriptPropertyType::Float;
    ScriptPropertyValue defaultValue;
    ScriptPropertyValue instanceValue;
    bool isOverridden = false;

    // Editor hints parsed from metadata attributes
    f32 rangeMin = 0.0f;
    f32 rangeMax = 0.0f;
    bool hasRange = false;
    std::string tooltip;
    std::string header;  // Section header from [Header("...")] annotation
};

// A single script attached to an entity
struct ScriptAttachment {
    std::string scriptPath;     // "scripts/PlayerController.as"
    std::string className;      // "PlayerController"
    void* instance = nullptr;   // AngelScript object handle (asIScriptObject*)
    std::vector<ScriptProperty> properties;
    bool enabled = true;
    bool initialized = false;   // OnCreate called
    bool started = false;       // OnStart called
    // OnStart and the first OnUpdate used to land in the SAME ScriptSystem::
    // Update, so a script that started this frame was handed the whole frame's
    // dt, including the time before it existed. With the fixed-step accumulator
    // sitting between them it also took every pending OnFixedUpdate tick, up to
    // six of them at the 0.1s dt clamp. Anything integrating `pos += v * dt`
    // teleported on its first frame alive. Ticks now begin the NEXT frame.
    // Cleared at the end of ScriptSystem::Update.
    bool startedThisFrame = false;
    bool hasError = false;
    std::string lastError;

    // Cached method IDs for fast dispatch (-1 = not found)
    int methodOnCreate = -1;
    int methodOnStart = -1;
    int methodOnUpdate = -1;
    int methodOnFixedUpdate = -1;
    int methodOnLateUpdate = -1;
    int methodOnDestroy = -1;
    int methodOnEnable = -1;
    int methodOnDisable = -1;
    int methodOnCollisionEnter = -1;
    int methodOnCollisionStay = -1;
    int methodOnCollisionExit = -1;
    int methodOnTriggerEnter = -1;
    int methodOnTriggerExit = -1;
    int methodOnAnimationEvent = -1;   // void OnAnimationEvent(string) — optional
    int methodOnMouseEnter = -1;       // void OnMouseEnter() — cursor ray first hits this entity
    int methodOnMouseExit = -1;        // void OnMouseExit()  — cursor ray leaves this entity
    int methodOnClick = -1;            // void OnClick()      — left click while hovered
};

// Component that holds all scripts attached to an entity
struct ScriptComponent {
    std::vector<ScriptAttachment> scripts;
};

} // namespace ECS
} // namespace Enjin
