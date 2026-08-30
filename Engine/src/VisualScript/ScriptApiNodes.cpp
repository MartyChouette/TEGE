#include "Enjin/VisualScript/ScriptApiNodes.h"
#include "Enjin/VisualScript/NodeRegistry.h"
#include "Enjin/VisualScript/NodeDefinition.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <string>
#include <vector>

namespace Enjin {
namespace VisualScript {

namespace {

using Editor::PinKind;
using Editor::PinType;

// The marshalable slice of AngelScript's type system - everything the
// VariableValue variant and the pin editor can represent.
enum class ApiType : u8 { Void, Bool, Int, UInt64, Float, Double, String, Vec2, Vec3 };

struct TypeIds {
    int stringId = -1;
    int vec2Id = -1;
    int vec3Id = -1;
};

bool ClassifyType(int typeId, const TypeIds& ids, ApiType& out) {
    switch (typeId) {
        case asTYPEID_VOID:   out = ApiType::Void;  return true;
        case asTYPEID_BOOL:   out = ApiType::Bool;  return true;
        case asTYPEID_INT32:  out = ApiType::Int;   return true;
        case asTYPEID_UINT32: out = ApiType::Int;   return true;
        case asTYPEID_UINT64: out = ApiType::UInt64; return true;   // entity handles
        case asTYPEID_FLOAT:  out = ApiType::Float; return true;
        case asTYPEID_DOUBLE: out = ApiType::Double; return true;
        default: break;
    }
    // Object types compare on the base id (strip handle/const flags - but we
    // only accept plain value types, so require an exact match).
    if (typeId == ids.stringId) { out = ApiType::String; return true; }
    if (typeId == ids.vec2Id)   { out = ApiType::Vec2;   return true; }
    if (typeId == ids.vec3Id)   { out = ApiType::Vec3;   return true; }
    return false;
}

PinDefinition MakePin(const std::string& name, PinKind kind, ApiType t) {
    PinDefinition p;
    p.name = name;
    p.kind = kind;
    switch (t) {
        case ApiType::Bool:   p.type = PinType::Bool;   p.defaultValue = false; break;
        case ApiType::Int:    p.type = PinType::Int;    p.defaultValue = i32(0); break;
        case ApiType::UInt64: p.type = PinType::Entity; p.defaultValue = ECS::INVALID_ENTITY; break;
        case ApiType::Float:
        case ApiType::Double: p.type = PinType::Float;  p.defaultValue = 0.0f; break;
        case ApiType::String: p.type = PinType::String; p.defaultValue = std::string(); break;
        case ApiType::Vec2:   p.type = PinType::Vector2; p.defaultValue = Math::Vector2(0, 0); break;
        case ApiType::Vec3:   p.type = PinType::Vector3; p.defaultValue = Math::Vector3(0, 0, 0); break;
        default:              p.type = PinType::Any; break;
    }
    return p;
}

// Category from the binding-name prefix (the part before the first '_').
NodeCategory CategoryFor(const std::string& fnName) {
    auto us = fnName.find('_');
    std::string prefix = (us == std::string::npos) ? fnName : fnName.substr(0, us);
    if (prefix == "Physics" || prefix == "Physics2D" || prefix == "Joint") return NodeCategory::Physics;
    if (prefix == "Audio" || prefix == "AudioGraph" || prefix == "Music") return NodeCategory::Audio;
    if (prefix == "Input" || prefix == "Action") return NodeCategory::Input;
    if (prefix == "Scene") return NodeCategory::Scene;
    if (prefix == "Streaming") return NodeCategory::Streaming;
    if (prefix == "Net") return NodeCategory::Networking;
    if (prefix == "Debug") return NodeCategory::Debug;
    if (prefix == "Entity" || prefix == "Prefab" || prefix == "Pool") return NodeCategory::Entity;
    if (prefix == "Transform") return NodeCategory::Transform;
    if (prefix == "Math") return NodeCategory::Math;
    if (prefix == "Noise") return NodeCategory::Noise;
    if (prefix == "Procedural") return NodeCategory::Procedural;
    if (prefix == "Quest" || prefix == "Save" || prefix == "SaveData" || prefix == "Meta" ||
        prefix == "Dialogue" || prefix == "HUD" || prefix == "Cinematic" || prefix == "Tween" ||
        prefix == "Flow" || prefix == "Checkpoint") return NodeCategory::Gameplay;
    return NodeCategory::Utility;
}

// Getter heuristic: side-effect-free reads become PURE nodes (no flow pins,
// evaluated on demand). Only obvious read verbs qualify - when in doubt a
// function gets flow pins, which is always correct, just less convenient.
bool LooksPure(const std::string& fnName) {
    auto us = fnName.find('_');
    std::string tail = (us == std::string::npos) ? fnName : fnName.substr(us + 1);
    return tail.rfind("Get", 0) == 0 || tail.rfind("Is", 0) == 0 ||
           tail.rfind("Has", 0) == 0 || tail.rfind("Count", 0) == 0;
}

// Set one AngelScript call argument from a VariableValue (coercing the
// common mismatches - int pin into float param, etc.).
bool SetArg(asIScriptContext* c, asUINT idx, ApiType t, const ECS::VariableValue& v) {
    auto asF32 = [&]() -> f32 {
        if (std::holds_alternative<f32>(v)) return std::get<f32>(v);
        if (std::holds_alternative<i32>(v)) return static_cast<f32>(std::get<i32>(v));
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1.0f : 0.0f;
        return 0.0f;
    };
    auto asI32 = [&]() -> i32 {
        if (std::holds_alternative<i32>(v)) return std::get<i32>(v);
        if (std::holds_alternative<f32>(v)) return static_cast<i32>(std::get<f32>(v));
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
        return 0;
    };
    switch (t) {
        case ApiType::Bool:
            c->SetArgByte(idx, std::holds_alternative<bool>(v) ? (std::get<bool>(v) ? 1 : 0)
                                                               : (asI32() != 0 ? 1 : 0));
            return true;
        case ApiType::Int:    c->SetArgDWord(idx, static_cast<asDWORD>(asI32())); return true;
        case ApiType::UInt64: {
            u64 e = std::holds_alternative<ECS::Entity>(v) ? static_cast<u64>(std::get<ECS::Entity>(v))
                  : static_cast<u64>(asI32());
            c->SetArgQWord(idx, e);
            return true;
        }
        case ApiType::Float:  c->SetArgFloat(idx, asF32()); return true;
        case ApiType::Double: c->SetArgDouble(idx, static_cast<f64>(asF32())); return true;
        case ApiType::String: {
            // AngelScript string params here are value or const-ref; both take
            // the object's address for the duration of Execute. The storage
            // must outlive the call - the caller keeps the temp alive.
            return false;   // handled by the caller (needs stable storage)
        }
        case ApiType::Vec2:
        case ApiType::Vec3:
            return false;   // handled by the caller (needs stable storage)
        default: return false;
    }
}

} // namespace

u32 RegisterScriptApiNodes(asIScriptEngine* engine) {
    if (!engine) return 0;
    static bool s_Done = false;
    if (s_Done) return 0;

    auto& reg = NodeRegistry::Instance();

    TypeIds ids;
    ids.stringId = engine->GetTypeIdByDecl("string");
    ids.vec2Id = engine->GetTypeIdByDecl("Vector2");
    ids.vec3Id = engine->GetTypeIdByDecl("Vector3");

    u32 registered = 0, skippedTypes = 0, skippedDup = 0;

    const asUINT fnCount = engine->GetGlobalFunctionCount();
    for (asUINT i = 0; i < fnCount; ++i) {
        asIScriptFunction* fn = engine->GetGlobalFunctionByIndex(i);
        if (!fn || !fn->GetName()) continue;
        if (fn->GetNamespace() && fn->GetNamespace()[0] != '\0') continue;  // global ns only
        std::string name = fn->GetName();

        // Hand-made nodes use the binding name as typeId - they win (nicer
        // pins/UX). Also skip if a previous overload registered this name.
        if (reg.FindNode(name) || reg.FindNode("Api_" + name)) { ++skippedDup; continue; }

        // Classify the signature; bail on anything not marshalable.
        int retTypeId = fn->GetReturnTypeId();
        ApiType ret;
        if (!ClassifyType(retTypeId, ids, ret)) { ++skippedTypes; continue; }

        const asUINT paramCount = fn->GetParamCount();
        std::vector<ApiType> params;
        params.reserve(paramCount);
        bool ok = true;
        for (asUINT p = 0; p < paramCount; ++p) {
            int tid = 0; asDWORD flags = 0;
            fn->GetParam(p, &tid, &flags);
            if (flags & asTM_OUTREF) { ok = false; break; }   // out-params unsupported
            ApiType t;
            if (!ClassifyType(tid, ids, t) || t == ApiType::Void) { ok = false; break; }
            params.push_back(t);
        }
        if (!ok || paramCount > 8) { ++skippedTypes; continue; }

        bool pure = LooksPure(name) && ret != ApiType::Void;

        NodeDefinition def;
        def.typeId = "Api_" + name;
        def.displayName = name;
        def.description = std::string(fn->GetDeclaration(false, false, true)) +
                          "\n(auto-generated from the script API)";
        def.category = CategoryFor(name);
        def.headerColor = Math::Vector3(0.25f, 0.45f, 0.45f);   // teal = generated
        def.keywords = { "api", "script" };

        // Pins: param names from the declaration when available.
        if (!pure) def.inputs.push_back(PinDefs::FlowIn());
        for (asUINT p = 0; p < paramCount; ++p) {
            const char* pname = nullptr;
            fn->GetParam(p, nullptr, nullptr, &pname);
            std::string pinName = (pname && pname[0]) ? pname : ("Arg" + std::to_string(p + 1));
            def.inputs.push_back(MakePin(pinName, PinKind::Input, params[p]));
        }
        if (!pure) def.outputs.push_back(PinDefs::FlowOut());
        if (ret != ApiType::Void)
            def.outputs.push_back(MakePin("Return", PinKind::Output, ret));

        // The call itself, shared by execute and evaluate. The function is
        // resolved BY DECLARATION at call time: registration may run against
        // a throwaway reflection engine (editor boot, for the palette) while
        // execution uses the live play engine - a captured asIScriptFunction*
        // would dangle across engines.
        std::string decl = fn->GetDeclaration(false, false, true);
        auto invoke = [decl, params, ret](const ExecutionContext& ctx,
                                          const std::vector<ECS::VariableValue>& inputs)
                -> ECS::VariableValue {
            ECS::VariableValue result = 0.0f;
            if (!ctx.scriptEngine) return result;
            asIScriptEngine* liveEngine = ctx.scriptEngine->GetASEngine();
            if (!liveEngine) return result;
            asIScriptFunction* fn = liveEngine->GetGlobalFunctionByDecl(decl.c_str());
            if (!fn) return result;
            asIScriptContext* c = ctx.scriptEngine->AcquireContext();
            if (!c) return result;

            // Stable storage for by-address args (strings/vectors) for the
            // duration of Execute.
            std::vector<std::string> strStore(params.size());
            std::vector<Math::Vector2> v2Store(params.size());
            std::vector<Math::Vector3> v3Store(params.size());

            c->Prepare(fn);
            for (usize p = 0; p < params.size(); ++p) {
                const ECS::VariableValue& v = (p < inputs.size()) ? inputs[p]
                                                                  : ECS::VariableValue(0.0f);
                asUINT idx = static_cast<asUINT>(p);
                switch (params[p]) {
                    case ApiType::String:
                        if (std::holds_alternative<std::string>(v)) strStore[p] = std::get<std::string>(v);
                        c->SetArgObject(idx, &strStore[p]);
                        break;
                    case ApiType::Vec2:
                        if (std::holds_alternative<Math::Vector2>(v)) v2Store[p] = std::get<Math::Vector2>(v);
                        c->SetArgObject(idx, &v2Store[p]);
                        break;
                    case ApiType::Vec3:
                        if (std::holds_alternative<Math::Vector3>(v)) v3Store[p] = std::get<Math::Vector3>(v);
                        c->SetArgObject(idx, &v3Store[p]);
                        break;
                    default:
                        SetArg(c, idx, params[p], v);
                        break;
                }
            }

            if (c->Execute() == asEXECUTION_FINISHED) {
                switch (ret) {
                    case ApiType::Bool:   result = (c->GetReturnByte() != 0); break;
                    case ApiType::Int:    result = static_cast<i32>(c->GetReturnDWord()); break;
                    case ApiType::UInt64: result = static_cast<ECS::Entity>(c->GetReturnQWord()); break;
                    case ApiType::Float:  result = c->GetReturnFloat(); break;
                    case ApiType::Double: result = static_cast<f32>(c->GetReturnDouble()); break;
                    case ApiType::String: {
                        void* obj = c->GetReturnObject();
                        result = obj ? *static_cast<std::string*>(obj) : std::string();
                        break;
                    }
                    case ApiType::Vec2: {
                        void* obj = c->GetReturnObject();
                        if (obj) result = *static_cast<Math::Vector2*>(obj);
                        break;
                    }
                    case ApiType::Vec3: {
                        void* obj = c->GetReturnObject();
                        if (obj) result = *static_cast<Math::Vector3*>(obj);
                        break;
                    }
                    default: break;
                }
            }
            ctx.scriptEngine->ReturnContext(c);
            return result;
        };

        if (pure) {
            def.flags = NodeDefFlags::Pure;
            def.evaluate = [invoke](const ExecutionContext& ctx,
                                    const std::vector<ECS::VariableValue>& inputs) {
                return invoke(ctx, inputs);
            };
        } else {
            def.execute = [invoke, ret](ExecutionContext& ctx,
                                        const std::vector<ECS::VariableValue>& inputs,
                                        std::vector<ECS::VariableValue>& outputs) {
                ECS::VariableValue r = invoke(ctx, inputs);
                if (ret != ApiType::Void) { outputs.resize(1); outputs[0] = r; }
            };
        }

        reg.RegisterNode(def);
        ++registered;
    }

    s_Done = true;
    ENJIN_LOG_INFO(Script, "VisualScript API codegen: %u nodes generated from %u bindings "
                   "(%u already hand-made, %u unmarshalable signatures)",
                   registered, fnCount, skippedDup, skippedTypes);
    return registered;
}

} // namespace VisualScript
} // namespace Enjin
