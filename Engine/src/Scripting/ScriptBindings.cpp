#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Math/Math.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Platform/Input.h"
#include <angelscript.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptarray/scriptarray.h>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <filesystem>
#include <string>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

// ---------------------------------------------------------------------------
// Active world pointer for entity/scene operations
// Shared across ScriptBindings translation units via extern linkage.
// ---------------------------------------------------------------------------
ECS::World* s_BindingsWorld = nullptr;

// ---------------------------------------------------------------------------
// SC-C2/C3: Centralized asset path validator for script bindings.
// Rejects absolute paths and directory traversal. Returns true if safe.
// ---------------------------------------------------------------------------
bool ValidateScriptAssetPath(const std::string& path, const char* funcName) {
    if (path.empty()) return false;
    // SC-2: Reject excessively long paths to prevent abuse
    if (path.size() > 1024) {
        ENJIN_LOG_WARN(Script, "%s: path too long (%zu chars, max 1024)", funcName, path.size());
        return false;
    }
    // Reject absolute paths
    if (path.size() >= 2 && path[1] == ':') { // Windows drive letter
        ENJIN_LOG_WARN(Script, "%s: absolute path rejected", funcName);
        return false;
    }
    if (path[0] == '/' || path[0] == '\\') {
        ENJIN_LOG_WARN(Script, "%s: absolute path rejected", funcName);
        return false;
    }
    // Reject directory traversal
    auto normalized = std::filesystem::path(path).lexically_normal().string();
    if (normalized.find("..") != std::string::npos) {
        ENJIN_LOG_WARN(Script, "%s: path traversal rejected", funcName);
        return false;
    }
    return true;
}

static Scripting::CoroutineScheduler* s_BindingsCoroutineScheduler = nullptr;

// Accessor for other translation units (e.g. ScriptEngine hot-reload)
Scripting::CoroutineScheduler* GetBindingsCoroutineScheduler() {
    return s_BindingsCoroutineScheduler;
}
Scripting::ScriptEventBus* s_BindingsEventBus = nullptr;
static Scripting::ScriptEngine* s_BindingsScriptEngine = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsWorld(ECS::World* world) {
    s_BindingsWorld = world;
}

void SetBindingsCoroutineScheduler(CoroutineScheduler* scheduler) {
    s_BindingsCoroutineScheduler = scheduler;
}

void SetBindingsEventBus(ScriptEventBus* bus) {
    s_BindingsEventBus = bus;
}

void SetBindingsScriptEngine(ScriptEngine* engine) {
    s_BindingsScriptEngine = engine;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Helper macro to verify registration calls
// ============================================================================
#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

// ============================================================================
// VECTOR2 WRAPPERS
// ============================================================================

static void Vector2_DefaultConstruct(Vector2* self) {
    new(self) Vector2();
}

static void Vector2_CopyConstruct(const Vector2& other, Vector2* self) {
    new(self) Vector2(other);
}

static void Vector2_InitConstruct(f32 x, f32 y, Vector2* self) {
    new(self) Vector2(x, y);
}

static Vector2 Vector2_opAdd(const Vector2& a, const Vector2& b) {
    return a + b;
}

static Vector2 Vector2_opSub(const Vector2& a, const Vector2& b) {
    return a - b;
}

static Vector2 Vector2_opMulF(const Vector2& a, f32 s) {
    return a * s;
}

static Vector2 Vector2_opDivF(const Vector2& a, f32 s) {
    if (std::abs(s) < 1e-7f) return Vector2(0.0f, 0.0f);
    return a / s;
}

static Vector2 Vector2_opNeg(const Vector2& a) {
    return -a;
}

static f32 Vector2_Length(const Vector2& self) {
    return self.Length();
}

static Vector2 Vector2_Normalized(const Vector2& self) {
    return self.Normalized();
}

static f32 Vector2_Dot(const Vector2& self, const Vector2& other) {
    return self.Dot(other);
}

// ============================================================================
// VECTOR3 WRAPPERS
// ============================================================================

static void Vector3_DefaultConstruct(Vector3* self) {
    new(self) Vector3();
}

static void Vector3_CopyConstruct(const Vector3& other, Vector3* self) {
    new(self) Vector3(other);
}

static void Vector3_InitConstruct(f32 x, f32 y, f32 z, Vector3* self) {
    new(self) Vector3(x, y, z);
}

static Vector3 Vector3_opAdd(const Vector3& a, const Vector3& b) {
    return a + b;
}

static Vector3 Vector3_opSub(const Vector3& a, const Vector3& b) {
    return a - b;
}

static Vector3 Vector3_opMulF(const Vector3& a, f32 s) {
    return a * s;
}

static Vector3 Vector3_opDivF(const Vector3& a, f32 s) {
    if (std::abs(s) < 1e-7f) return Vector3(0.0f, 0.0f, 0.0f);
    return a / s;
}

static Vector3 Vector3_opNeg(const Vector3& a) {
    return -a;
}

static f32 Vector3_Length(const Vector3& self) {
    return self.Length();
}

static Vector3 Vector3_Normalized(const Vector3& self) {
    return self.Normalized();
}

static f32 Vector3_Dot(const Vector3& self, const Vector3& other) {
    return self.Dot(other);
}

static Vector3 Vector3_Cross(const Vector3& self, const Vector3& other) {
    return self.Cross(other);
}

// ============================================================================
// VECTOR4 WRAPPERS
// ============================================================================

static void Vector4_DefaultConstruct(Vector4* self) {
    new(self) Vector4();
}

static void Vector4_CopyConstruct(const Vector4& other, Vector4* self) {
    new(self) Vector4(other);
}

static void Vector4_InitConstruct(f32 x, f32 y, f32 z, f32 w, Vector4* self) {
    new(self) Vector4(x, y, z, w);
}

// ============================================================================
// QUATERNION WRAPPERS
// ============================================================================

static void Quat_DefaultConstruct(Quaternion* self) {
    new(self) Quaternion();
}

static void Quat_CopyConstruct(const Quaternion& other, Quaternion* self) {
    new(self) Quaternion(other);
}

static void Quat_ComponentConstruct(f32 x, f32 y, f32 z, f32 w, Quaternion* self) {
    new(self) Quaternion(x, y, z, w);
}

static void Quat_AxisAngleConstruct(const Vector3& axis, f32 angle, Quaternion* self) {
    new(self) Quaternion(axis, angle);
}

static Vector3 Quat_Rotate(const Quaternion& self, const Vector3& v) {
    return self.Rotate(v);
}

static Quaternion Quat_Slerp(const Quaternion& a, const Quaternion& b, f32 t) {
    return Quaternion::Slerp(a, b, t);
}

static Quaternion Quat_Identity() {
    return Quaternion::Identity();
}

static Quaternion Quat_FromEuler(const Vector3& euler) {
    return Quaternion::FromEuler(euler);
}

static Vector3 Quat_ToEuler(const Quaternion& self) {
    return self.ToEuler();
}

static Quaternion Quat_Normalized(const Quaternion& self) {
    return self.Normalized();
}

static Quaternion Quat_Inverse(const Quaternion& self) {
    return self.Inverse();
}

static Quaternion Quat_opMul(const Quaternion& a, const Quaternion& b) {
    return a * b;
}

// ============================================================================
// ENTITY HANDLE (Reference type for script access to ECS entities)
// ============================================================================

class EntityHandle {
public:
    EntityHandle() : m_Entity(INVALID_ENTITY), m_RefCount(1) {}
    EntityHandle(Entity e) : m_Entity(e), m_RefCount(1) {}

    void AddRef() { m_RefCount.fetch_add(1, std::memory_order_relaxed); }
    void Release() {
        if (m_RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete this;
    }

    bool IsValid() const {
        return s_BindingsWorld && m_Entity != INVALID_ENTITY &&
               s_BindingsWorld->IsValid(m_Entity);
    }

    Entity GetID() const { return m_Entity; }

    Vector3 GetPosition() const {
        if (!IsValid()) return Vector3();
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(m_Entity);
        return t ? t->position : Vector3();
    }

    void SetPosition(const Vector3& pos) {
        if (!IsValid()) return;
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(m_Entity);
        if (t) t->position = pos;
    }

    Vector3 GetRotation() const {
        if (!IsValid()) return Vector3();
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(m_Entity);
        if (!t) return Vector3();
        Vector3 euler = t->rotation.ToEuler();
        return Vector3(Degrees(euler.x), Degrees(euler.y), Degrees(euler.z));
    }

    void SetRotation(const Vector3& eulerDeg) {
        if (!IsValid()) return;
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(m_Entity);
        if (t) {
            t->rotation = Quaternion::FromEuler(
                Vector3(Radians(eulerDeg.x), Radians(eulerDeg.y), Radians(eulerDeg.z)));
        }
    }

    Vector3 GetScale() const {
        if (!IsValid()) return Vector3(1.0f);
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(m_Entity);
        return t ? t->scale : Vector3(1.0f);
    }

    void SetScale(const Vector3& s) {
        if (!IsValid()) return;
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(m_Entity);
        if (t) t->scale = s;
    }

    std::string GetName() const {
        if (!IsValid()) return "";
        auto* n = s_BindingsWorld->GetComponent<NameComponent>(m_Entity);
        return n ? n->name : "";
    }

    bool HasTag(const std::string& tag) const {
        if (!IsValid()) return false;
        auto* tc = s_BindingsWorld->GetComponent<TagComponent>(m_Entity);
        return tc ? tc->HasTag(tag) : false;
    }

private:
    Entity m_Entity;
    std::atomic<int> m_RefCount;
};

static EntityHandle* EntityHandle_Factory() {
    return new EntityHandle();
}

static EntityHandle* EntityHandle_FactoryFromID(u64 id) {
    return new EntityHandle(static_cast<Entity>(id));
}

// ============================================================================
// TRANSFORM PROXY (value type returned by transform property)
// ============================================================================

struct TransformProxy {
    Entity entity;

    TransformProxy() : entity(INVALID_ENTITY) {}
    TransformProxy(Entity e) : entity(e) {}

    // SC-H6: Validate entity before component access to prevent stale data
    Vector3 GetPosition() const {
        if (!s_BindingsWorld || !s_BindingsWorld->IsValid(entity)) return Vector3();
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(entity);
        return t ? t->position : Vector3();
    }

    void SetPosition(const Vector3& pos) {
        if (!s_BindingsWorld || !s_BindingsWorld->IsValid(entity)) return;
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(entity);
        if (t) t->position = pos;
    }

    Vector3 GetRotation() const {
        if (!s_BindingsWorld || !s_BindingsWorld->IsValid(entity)) return Vector3();
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(entity);
        if (!t) return Vector3();
        Vector3 euler = t->rotation.ToEuler();
        return Vector3(Degrees(euler.x), Degrees(euler.y), Degrees(euler.z));
    }

    void SetRotation(const Vector3& eulerDeg) {
        if (!s_BindingsWorld || !s_BindingsWorld->IsValid(entity)) return;
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(entity);
        if (t) {
            t->rotation = Quaternion::FromEuler(
                Vector3(Radians(eulerDeg.x), Radians(eulerDeg.y), Radians(eulerDeg.z)));
        }
    }

    Vector3 GetScale() const {
        if (!s_BindingsWorld || !s_BindingsWorld->IsValid(entity)) return Vector3(1.0f);
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(entity);
        return t ? t->scale : Vector3(1.0f);
    }

    void SetScale(const Vector3& s) {
        if (!s_BindingsWorld || !s_BindingsWorld->IsValid(entity)) return;
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(entity);
        if (t) t->scale = s;
    }

    Vector3 GetForward() const {
        if (!s_BindingsWorld || !s_BindingsWorld->IsValid(entity)) return Vector3(0, 0, -1);
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(entity);
        if (!t) return Vector3(0, 0, -1);
        return t->rotation.Rotate(Vector3(0, 0, -1));
    }

    Vector3 GetRight() const {
        if (!s_BindingsWorld || !s_BindingsWorld->IsValid(entity)) return Vector3(1, 0, 0);
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(entity);
        if (!t) return Vector3(1, 0, 0);
        return t->rotation.Rotate(Vector3(1, 0, 0));
    }

    Vector3 GetUp() const {
        if (!s_BindingsWorld || !s_BindingsWorld->IsValid(entity)) return Vector3(0, 1, 0);
        auto* t = s_BindingsWorld->GetComponent<TransformComponent>(entity);
        if (!t) return Vector3(0, 1, 0);
        return t->rotation.Rotate(Vector3(0, 1, 0));
    }
};

static void TransformProxy_DefaultConstruct(TransformProxy* self) {
    new(self) TransformProxy();
}

static void TransformProxy_CopyConstruct(const TransformProxy& other, TransformProxy* self) {
    new(self) TransformProxy(other);
}

// ============================================================================
// TIME GLOBALS
// ============================================================================

static f32 s_DeltaTime = 0.0f;
static f32 s_FixedDeltaTime = 1.0f / 60.0f;
static f32 s_TotalTime = 0.0f;
static f32 s_TimeScale = 1.0f;
static u32 s_FrameCount = 0;

static f32 Time_GetDeltaTime()      { return s_DeltaTime; }
static f32 Time_GetFixedDeltaTime() { return s_FixedDeltaTime; }
static f32 Time_GetTime()           { return s_TotalTime; }
static f32 Time_GetTimeScale()      { return s_TimeScale; }
static void Time_SetTimeScale(f32 scale) { s_TimeScale = scale; }
static u32 Time_GetFrameCount()     { return s_FrameCount; }

// ============================================================================
// DEBUG FUNCTIONS
// ============================================================================

// Cap script log messages to prevent excessively long strings from consuming memory
static constexpr usize MAX_SCRIPT_LOG_LENGTH = 4096;

static void Debug_Log(const std::string& msg) {
    if (msg.size() > MAX_SCRIPT_LOG_LENGTH) {
        ENJIN_LOG_INFO(Script, "%s", msg.substr(0, MAX_SCRIPT_LOG_LENGTH).c_str());
        return;
    }
    ENJIN_LOG_INFO(Script, "%s", msg.c_str());
}

static void Debug_LogWarning(const std::string& msg) {
    if (msg.size() > MAX_SCRIPT_LOG_LENGTH) {
        ENJIN_LOG_WARN(Script, "%s", msg.substr(0, MAX_SCRIPT_LOG_LENGTH).c_str());
        return;
    }
    ENJIN_LOG_WARN(Script, "%s", msg.c_str());
}

static void Debug_LogError(const std::string& msg) {
    if (msg.size() > MAX_SCRIPT_LOG_LENGTH) {
        ENJIN_LOG_ERROR(Script, "%s", msg.substr(0, MAX_SCRIPT_LOG_LENGTH).c_str());
        return;
    }
    ENJIN_LOG_ERROR(Script, "%s", msg.c_str());
}

// ============================================================================
// MATH GLOBAL FUNCTIONS
// ============================================================================

static f32 Math_Abs(f32 v) { return Math::Abs(v); }
static f32 Math_Sin(f32 v) { return Math::Sin(v); }
static f32 Math_Cos(f32 v) { return Math::Cos(v); }
static f32 Math_Tan(f32 v) { return Math::Tan(v); }
static f32 Math_Asin(f32 v) { return Math::Asin(v); }
static f32 Math_Acos(f32 v) { return Math::Acos(v); }
static f32 Math_Atan2(f32 y, f32 x) { return Math::Atan2(y, x); }
static f32 Math_Sqrt(f32 v) { return Math::Sqrt(v); }
static f32 Math_Pow(f32 b, f32 e) { return Math::Pow(b, e); }
static f32 Math_Floor(f32 v) { return Math::Floor(v); }
static f32 Math_Ceil(f32 v) { return Math::Ceil(v); }
static f32 Math_Round(f32 v) { return Math::Round(v); }
static f32 Math_Min(f32 a, f32 b) { return Math::Min(a, b); }
static f32 Math_Max(f32 a, f32 b) { return Math::Max(a, b); }
static f32 Math_Clamp(f32 v, f32 lo, f32 hi) { return Math::Clamp(v, lo, hi); }
static f32 Math_Lerp(f32 a, f32 b, f32 t) { return Math::Lerp(a, b, t); }
static f32 Math_MoveTowards(f32 current, f32 target, f32 maxDelta) {
    return Math::MoveTowards(current, target, maxDelta);
}
static f32 Math_Sign(f32 v) { return Math::Sign(v); }

static f32 Math_Random() { return Math::Random01(); }
static f32 Math_RandomRange(f32 lo, f32 hi) { return Math::Random(lo, hi); }
static i32 Math_RandomInt(i32 lo, i32 hi) { return Math::RandomInt(lo, hi); }

static f32 Math_Radians(f32 deg) { return Math::Radians(deg); }
static f32 Math_Degrees(f32 rad) { return Math::Degrees(rad); }

static f32 Math_PI() { return Math::PI; }

// ============================================================================
// COROUTINE WRAPPERS
// ============================================================================

static void Script_StartCoroutine(const std::string& funcName) {
    if (!s_BindingsCoroutineScheduler || !s_BindingsScriptEngine) return;

    // Get the calling context to find the script object and entity ID
    asIScriptContext* callingCtx = asGetActiveContext();
    if (!callingCtx) return;

    // Get the calling object (TegeBehavior subclass)
    asIScriptObject* obj = static_cast<asIScriptObject*>(callingCtx->GetThisPointer());
    if (!obj) return;

    // Find the coroutine method on the object
    asIScriptFunction* func = s_BindingsScriptEngine->FindMethod(obj, "void " + funcName + "()");
    if (!func) {
        ENJIN_LOG_WARN(Script, "StartCoroutine: method '%s' not found", funcName.c_str());
        return;
    }

    // Get entity ID from the _entityId property
    asITypeInfo* type = obj->GetObjectType();
    u32 propCount = type->GetPropertyCount();
    u64 entityId = 0;
    for (u32 i = 0; i < propCount; i++) {
        const char* propName = nullptr;
        type->GetProperty(i, &propName);
        if (propName && std::string(propName) == "_entityId") {
            entityId = *static_cast<u64*>(obj->GetAddressOfProperty(i));
            break;
        }
    }

    // Create a new context for the coroutine
    asIScriptContext* coCtx = s_BindingsScriptEngine->AcquireContext();
    if (!coCtx) return;

    coCtx->Prepare(func);
    coCtx->SetObject(obj);
    obj->AddRef();

    // Execute until first yield or completion
    int r = coCtx->Execute();
    if (r == asEXECUTION_SUSPENDED) {
        s_BindingsCoroutineScheduler->StartCoroutine(coCtx, entityId);
    } else {
        // Completed immediately or errored
        if (r == asEXECUTION_EXCEPTION) {
            ENJIN_LOG_ERROR(Script, "Coroutine '%s' exception: %s",
                funcName.c_str(), coCtx->GetExceptionString());
        }
        s_BindingsScriptEngine->ReturnContext(coCtx);
    }
}

static void Script_YieldSeconds(f32 seconds) {
    Scripting::CoroutineScheduler::YieldSeconds(seconds);
}

static void Script_YieldFrames(u32 frames) {
    Scripting::CoroutineScheduler::YieldFrames(frames);
}

static void Script_YieldEndOfFrame() {
    Scripting::CoroutineScheduler::YieldEndOfFrame();
}

// ============================================================================
// EVENT DATA WRAPPERS
// ============================================================================

struct ScriptEventData {
    Scripting::EventData data;
    std::atomic<int> refCount{1};

    void AddRef() { refCount.fetch_add(1, std::memory_order_relaxed); }
    void Release() { if (refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this; }
};

static ScriptEventData* EventData_Factory() {
    return new ScriptEventData();
}

static void EventData_SetFloat(ScriptEventData* self, const std::string& key, f32 val) {
    if (!self) return;
    self->data.SetFloat(key, val);
}
static f32 EventData_GetFloat(ScriptEventData* self, const std::string& key) {
    if (!self) return 0.0f;
    return self->data.GetFloat(key);
}
static void EventData_SetInt(ScriptEventData* self, const std::string& key, i32 val) {
    if (!self) return;
    self->data.SetInt(key, val);
}
static i32 EventData_GetInt(ScriptEventData* self, const std::string& key) {
    if (!self) return 0;
    return self->data.GetInt(key);
}
static void EventData_SetString(ScriptEventData* self, const std::string& key, const std::string& val) {
    if (!self) return;
    self->data.SetString(key, val);
}
static std::string EventData_GetString(ScriptEventData* self, const std::string& key) {
    if (!self) return "";
    return self->data.GetString(key);
}
static void EventData_SetEntity(ScriptEventData* self, const std::string& key, u64 val) {
    if (!self) return;
    self->data.SetEntity(key, val);
}
static u64 EventData_GetEntity(ScriptEventData* self, const std::string& key) {
    if (!self) return 0;
    return self->data.GetEntity(key);
}

static u32 Events_Listen(const std::string& eventName, asIScriptFunction* callback) {
    if (!s_BindingsEventBus) return 0;

    asIScriptContext* ctx = asGetActiveContext();
    if (!ctx) return 0;

    asIScriptObject* obj = static_cast<asIScriptObject*>(ctx->GetThisPointer());

    // Get entity ID
    u64 entityId = 0;
    if (obj) {
        asITypeInfo* type = obj->GetObjectType();
        u32 propCount = type->GetPropertyCount();
        for (u32 i = 0; i < propCount; i++) {
            const char* propName = nullptr;
            type->GetProperty(i, &propName);
            if (propName && std::string(propName) == "_entityId") {
                entityId = *static_cast<u64*>(obj->GetAddressOfProperty(i));
                break;
            }
        }
    }

    return s_BindingsEventBus->Listen(eventName, obj, callback, entityId);
}

static void Events_Send(const std::string& eventName, ScriptEventData* data) {
    if (!s_BindingsEventBus || !data) return;
    s_BindingsEventBus->Send(eventName, data->data);
}

static void Events_Broadcast(ScriptEventData* data) {
    if (!s_BindingsEventBus || !data) return;
    s_BindingsEventBus->Broadcast(data->data);
}

// Payload of the event currently being dispatched. EventCallback only receives
// the event name; these read the payload keys (UI bridge sets "value" for
// sliders, "checked" for toggles, "text" for buttons).
static float Events_CurrentFloat(const std::string& key) {
    const Enjin::Scripting::EventData* d = Enjin::Scripting::GetCurrentDispatchEventData();
    return d ? d->GetFloat(key) : 0.0f;
}

static int Events_CurrentInt(const std::string& key) {
    const Enjin::Scripting::EventData* d = Enjin::Scripting::GetCurrentDispatchEventData();
    return d ? d->GetInt(key) : 0;
}

static std::string Events_CurrentString(const std::string& key) {
    const Enjin::Scripting::EventData* d = Enjin::Scripting::GetCurrentDispatchEventData();
    return d ? d->GetString(key) : "";
}

// ============================================================================
// REGISTRATION FUNCTIONS
// ============================================================================

namespace Enjin {
namespace Scripting {

// ---------------------------------------------------------------------------
// RegisterMathTypes
// ---------------------------------------------------------------------------
void RegisterMathTypes(asIScriptEngine* engine) {
    // ---- Vector2 ----
    AS_CHECK(engine->RegisterObjectType("Vector2", sizeof(Vector2),
        asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<Vector2>()));

    AS_CHECK(engine->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT,
        "void f()", ENJIN_AS_OBJ_LAST(Vector2_DefaultConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT,
        "void f(const Vector2 &in)", ENJIN_AS_OBJ_LAST(Vector2_CopyConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT,
        "void f(float, float)", ENJIN_AS_OBJ_LAST(Vector2_InitConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));

    AS_CHECK(engine->RegisterObjectProperty("Vector2", "float x", asOFFSET(Vector2, x)));
    AS_CHECK(engine->RegisterObjectProperty("Vector2", "float y", asOFFSET(Vector2, y)));

    AS_CHECK(engine->RegisterObjectMethod("Vector2", "Vector2 opAdd(const Vector2 &in) const",
        ENJIN_AS_OBJ_FIRST(Vector2_opAdd), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector2", "Vector2 opSub(const Vector2 &in) const",
        ENJIN_AS_OBJ_FIRST(Vector2_opSub), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector2", "Vector2 opMul(float) const",
        ENJIN_AS_OBJ_FIRST(Vector2_opMulF), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector2", "Vector2 opDiv(float) const",
        ENJIN_AS_OBJ_FIRST(Vector2_opDivF), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector2", "Vector2 opNeg() const",
        ENJIN_AS_OBJ_FIRST(Vector2_opNeg), ENJIN_AS_CALL_CDECL_OBJFIRST));

    AS_CHECK(engine->RegisterObjectMethod("Vector2", "float Length() const",
        ENJIN_AS_OBJ_FIRST(Vector2_Length), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector2", "Vector2 Normalized() const",
        ENJIN_AS_OBJ_FIRST(Vector2_Normalized), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector2", "float Dot(const Vector2 &in) const",
        ENJIN_AS_OBJ_FIRST(Vector2_Dot), ENJIN_AS_CALL_CDECL_OBJFIRST));

    // ---- Vector3 ----
    AS_CHECK(engine->RegisterObjectType("Vector3", sizeof(Vector3),
        asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<Vector3>()));

    AS_CHECK(engine->RegisterObjectBehaviour("Vector3", asBEHAVE_CONSTRUCT,
        "void f()", ENJIN_AS_OBJ_LAST(Vector3_DefaultConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("Vector3", asBEHAVE_CONSTRUCT,
        "void f(const Vector3 &in)", ENJIN_AS_OBJ_LAST(Vector3_CopyConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("Vector3", asBEHAVE_CONSTRUCT,
        "void f(float, float, float)", ENJIN_AS_OBJ_LAST(Vector3_InitConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));

    AS_CHECK(engine->RegisterObjectProperty("Vector3", "float x", asOFFSET(Vector3, x)));
    AS_CHECK(engine->RegisterObjectProperty("Vector3", "float y", asOFFSET(Vector3, y)));
    AS_CHECK(engine->RegisterObjectProperty("Vector3", "float z", asOFFSET(Vector3, z)));

    AS_CHECK(engine->RegisterObjectMethod("Vector3", "Vector3 opAdd(const Vector3 &in) const",
        ENJIN_AS_OBJ_FIRST(Vector3_opAdd), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector3", "Vector3 opSub(const Vector3 &in) const",
        ENJIN_AS_OBJ_FIRST(Vector3_opSub), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector3", "Vector3 opMul(float) const",
        ENJIN_AS_OBJ_FIRST(Vector3_opMulF), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector3", "Vector3 opDiv(float) const",
        ENJIN_AS_OBJ_FIRST(Vector3_opDivF), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector3", "Vector3 opNeg() const",
        ENJIN_AS_OBJ_FIRST(Vector3_opNeg), ENJIN_AS_CALL_CDECL_OBJFIRST));

    AS_CHECK(engine->RegisterObjectMethod("Vector3", "float Length() const",
        ENJIN_AS_OBJ_FIRST(Vector3_Length), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector3", "Vector3 Normalized() const",
        ENJIN_AS_OBJ_FIRST(Vector3_Normalized), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector3", "float Dot(const Vector3 &in) const",
        ENJIN_AS_OBJ_FIRST(Vector3_Dot), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Vector3", "Vector3 Cross(const Vector3 &in) const",
        ENJIN_AS_OBJ_FIRST(Vector3_Cross), ENJIN_AS_CALL_CDECL_OBJFIRST));

    // ---- Vector4 ----
    AS_CHECK(engine->RegisterObjectType("Vector4", sizeof(Vector4),
        asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<Vector4>()));

    AS_CHECK(engine->RegisterObjectBehaviour("Vector4", asBEHAVE_CONSTRUCT,
        "void f()", ENJIN_AS_OBJ_LAST(Vector4_DefaultConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("Vector4", asBEHAVE_CONSTRUCT,
        "void f(const Vector4 &in)", ENJIN_AS_OBJ_LAST(Vector4_CopyConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("Vector4", asBEHAVE_CONSTRUCT,
        "void f(float, float, float, float)", ENJIN_AS_OBJ_LAST(Vector4_InitConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));

    AS_CHECK(engine->RegisterObjectProperty("Vector4", "float x", asOFFSET(Vector4, x)));
    AS_CHECK(engine->RegisterObjectProperty("Vector4", "float y", asOFFSET(Vector4, y)));
    AS_CHECK(engine->RegisterObjectProperty("Vector4", "float z", asOFFSET(Vector4, z)));
    AS_CHECK(engine->RegisterObjectProperty("Vector4", "float w", asOFFSET(Vector4, w)));

    // ---- Quaternion ----
    AS_CHECK(engine->RegisterObjectType("Quaternion", sizeof(Quaternion),
        asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<Quaternion>()));

    AS_CHECK(engine->RegisterObjectBehaviour("Quaternion", asBEHAVE_CONSTRUCT,
        "void f()", ENJIN_AS_OBJ_LAST(Quat_DefaultConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("Quaternion", asBEHAVE_CONSTRUCT,
        "void f(const Quaternion &in)", ENJIN_AS_OBJ_LAST(Quat_CopyConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("Quaternion", asBEHAVE_CONSTRUCT,
        "void f(float, float, float, float)", ENJIN_AS_OBJ_LAST(Quat_ComponentConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("Quaternion", asBEHAVE_CONSTRUCT,
        "void f(const Vector3 &in, float)", ENJIN_AS_OBJ_LAST(Quat_AxisAngleConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));

    AS_CHECK(engine->RegisterObjectProperty("Quaternion", "float x", asOFFSET(Quaternion, x)));
    AS_CHECK(engine->RegisterObjectProperty("Quaternion", "float y", asOFFSET(Quaternion, y)));
    AS_CHECK(engine->RegisterObjectProperty("Quaternion", "float z", asOFFSET(Quaternion, z)));
    AS_CHECK(engine->RegisterObjectProperty("Quaternion", "float w", asOFFSET(Quaternion, w)));

    AS_CHECK(engine->RegisterObjectMethod("Quaternion", "Vector3 Rotate(const Vector3 &in) const",
        ENJIN_AS_OBJ_FIRST(Quat_Rotate), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Quaternion", "Quaternion Normalized() const",
        ENJIN_AS_OBJ_FIRST(Quat_Normalized), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Quaternion", "Quaternion Inverse() const",
        ENJIN_AS_OBJ_FIRST(Quat_Inverse), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Quaternion", "Vector3 ToEuler() const",
        ENJIN_AS_OBJ_FIRST(Quat_ToEuler), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("Quaternion", "Quaternion opMul(const Quaternion &in) const",
        ENJIN_AS_OBJ_FIRST(Quat_opMul), ENJIN_AS_CALL_CDECL_OBJFIRST));

    // Quaternion static / global helpers
    AS_CHECK(engine->RegisterGlobalFunction("Quaternion Quaternion_Identity()",
        ENJIN_AS_FN(Quat_Identity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Quaternion Quaternion_FromEuler(const Vector3 &in)",
        ENJIN_AS_FN(Quat_FromEuler), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Quaternion Quaternion_Slerp(const Quaternion &in, const Quaternion &in, float)",
        ENJIN_AS_FN(Quat_Slerp), ENJIN_AS_CALL_CDECL));

    // ---- Global math functions ----
    AS_CHECK(engine->RegisterGlobalFunction("float Abs(float)",  ENJIN_AS_FN(Math_Abs),  ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Sin(float)",  ENJIN_AS_FN(Math_Sin),  ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Cos(float)",  ENJIN_AS_FN(Math_Cos),  ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Tan(float)",  ENJIN_AS_FN(Math_Tan),  ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Asin(float)", ENJIN_AS_FN(Math_Asin), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Acos(float)", ENJIN_AS_FN(Math_Acos), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Atan2(float, float)", ENJIN_AS_FN(Math_Atan2), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Sqrt(float)", ENJIN_AS_FN(Math_Sqrt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Pow(float, float)", ENJIN_AS_FN(Math_Pow), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Floor(float)", ENJIN_AS_FN(Math_Floor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Ceil(float)",  ENJIN_AS_FN(Math_Ceil),  ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Round(float)", ENJIN_AS_FN(Math_Round), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Min(float, float)", ENJIN_AS_FN(Math_Min), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Max(float, float)", ENJIN_AS_FN(Math_Max), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Clamp(float, float, float)", ENJIN_AS_FN(Math_Clamp), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Lerp(float, float, float)",  ENJIN_AS_FN(Math_Lerp),  ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float MoveTowards(float, float, float)", ENJIN_AS_FN(Math_MoveTowards), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Sign(float)",    ENJIN_AS_FN(Math_Sign),    ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Random()",       ENJIN_AS_FN(Math_Random),      ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float RandomRange(float, float)", ENJIN_AS_FN(Math_RandomRange), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int   RandomInt(int, int)",       ENJIN_AS_FN(Math_RandomInt),   ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Radians(float)", ENJIN_AS_FN(Math_Radians), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Degrees(float)", ENJIN_AS_FN(Math_Degrees), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float PI()",           ENJIN_AS_FN(Math_PI),      ENJIN_AS_CALL_CDECL));
}

// ---------------------------------------------------------------------------
// RegisterEntityTypes
// ---------------------------------------------------------------------------
void RegisterEntityTypes(asIScriptEngine* engine) {
    // Entity is a uint64 value type alias
    AS_CHECK(engine->RegisterTypedef("Entity", "uint64"));

    // EntityHandle - reference type
    AS_CHECK(engine->RegisterObjectType("EntityHandle", 0, asOBJ_REF));
    AS_CHECK(engine->RegisterObjectBehaviour("EntityHandle", asBEHAVE_FACTORY,
        "EntityHandle@ f()", ENJIN_AS_FN(EntityHandle_Factory), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterObjectBehaviour("EntityHandle", asBEHAVE_FACTORY,
        "EntityHandle@ f(uint64)", ENJIN_AS_FN(EntityHandle_FactoryFromID), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterObjectBehaviour("EntityHandle", asBEHAVE_ADDREF,
        "void f()", ENJIN_AS_MFN(EntityHandle, AddRef), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectBehaviour("EntityHandle", asBEHAVE_RELEASE,
        "void f()", ENJIN_AS_MFN(EntityHandle, Release), ENJIN_AS_CALL_THISCALL));

    AS_CHECK(engine->RegisterObjectMethod("EntityHandle", "bool IsValid() const",
        ENJIN_AS_MFN(EntityHandle, IsValid), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("EntityHandle", "uint64 GetID() const",
        ENJIN_AS_MFN(EntityHandle, GetID), ENJIN_AS_CALL_THISCALL));

    AS_CHECK(engine->RegisterObjectMethod("EntityHandle", "Vector3 GetPosition() const",
        ENJIN_AS_MFN(EntityHandle, GetPosition), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("EntityHandle", "void SetPosition(const Vector3 &in)",
        ENJIN_AS_MFN(EntityHandle, SetPosition), ENJIN_AS_CALL_THISCALL));

    AS_CHECK(engine->RegisterObjectMethod("EntityHandle", "Vector3 GetRotation() const",
        ENJIN_AS_MFN(EntityHandle, GetRotation), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("EntityHandle", "void SetRotation(const Vector3 &in)",
        ENJIN_AS_MFN(EntityHandle, SetRotation), ENJIN_AS_CALL_THISCALL));

    AS_CHECK(engine->RegisterObjectMethod("EntityHandle", "Vector3 GetScale() const",
        ENJIN_AS_MFN(EntityHandle, GetScale), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("EntityHandle", "void SetScale(const Vector3 &in)",
        ENJIN_AS_MFN(EntityHandle, SetScale), ENJIN_AS_CALL_THISCALL));

    AS_CHECK(engine->RegisterObjectMethod("EntityHandle", "string GetName() const",
        ENJIN_AS_MFN(EntityHandle, GetName), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("EntityHandle", "bool HasTag(const string &in) const",
        ENJIN_AS_MFN(EntityHandle, HasTag), ENJIN_AS_CALL_THISCALL));

    // TransformProxy - value type for transform access on TegeBehavior
    AS_CHECK(engine->RegisterObjectType("TransformProxy", sizeof(TransformProxy),
        asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<TransformProxy>()));

    AS_CHECK(engine->RegisterObjectBehaviour("TransformProxy", asBEHAVE_CONSTRUCT,
        "void f()", ENJIN_AS_OBJ_LAST(TransformProxy_DefaultConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("TransformProxy", asBEHAVE_CONSTRUCT,
        "void f(const TransformProxy &in)", ENJIN_AS_OBJ_LAST(TransformProxy_CopyConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));

    AS_CHECK(engine->RegisterObjectMethod("TransformProxy", "Vector3 get_position() const",
        ENJIN_AS_MFN(TransformProxy, GetPosition), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("TransformProxy", "void set_position(const Vector3 &in)",
        ENJIN_AS_MFN(TransformProxy, SetPosition), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("TransformProxy", "Vector3 get_rotation() const",
        ENJIN_AS_MFN(TransformProxy, GetRotation), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("TransformProxy", "void set_rotation(const Vector3 &in)",
        ENJIN_AS_MFN(TransformProxy, SetRotation), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("TransformProxy", "Vector3 get_scale() const",
        ENJIN_AS_MFN(TransformProxy, GetScale), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("TransformProxy", "void set_scale(const Vector3 &in)",
        ENJIN_AS_MFN(TransformProxy, SetScale), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("TransformProxy", "Vector3 get_forward() const",
        ENJIN_AS_MFN(TransformProxy, GetForward), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("TransformProxy", "Vector3 get_right() const",
        ENJIN_AS_MFN(TransformProxy, GetRight), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectMethod("TransformProxy", "Vector3 get_up() const",
        ENJIN_AS_MFN(TransformProxy, GetUp), ENJIN_AS_CALL_THISCALL));
}

// ---------------------------------------------------------------------------
// RegisterTimeBindings
// ---------------------------------------------------------------------------
void RegisterTimeBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction("float Time_GetDeltaTime()",
        ENJIN_AS_FN(Time_GetDeltaTime), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Time_GetFixedDeltaTime()",
        ENJIN_AS_FN(Time_GetFixedDeltaTime), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Time_GetTime()",
        ENJIN_AS_FN(Time_GetTime), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Time_GetTimeScale()",
        ENJIN_AS_FN(Time_GetTimeScale), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Time_SetTimeScale(float)",
        ENJIN_AS_FN(Time_SetTimeScale), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint Time_GetFrameCount()",
        ENJIN_AS_FN(Time_GetFrameCount), ENJIN_AS_CALL_CDECL));
}

// ---------------------------------------------------------------------------
// RegisterDebugBindings
// ---------------------------------------------------------------------------
void RegisterDebugBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction("void Debug_Log(const string &in)",
        ENJIN_AS_FN(Debug_Log), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Debug_LogWarning(const string &in)",
        ENJIN_AS_FN(Debug_LogWarning), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Debug_LogError(const string &in)",
        ENJIN_AS_FN(Debug_LogError), ENJIN_AS_CALL_CDECL));
}

// ---------------------------------------------------------------------------
// RegisterCoroutineBindings
// ---------------------------------------------------------------------------
void RegisterCoroutineBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction(
        "void StartCoroutine(const string &in)",
        ENJIN_AS_FN(Script_StartCoroutine), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void YieldSeconds(float)",
        ENJIN_AS_FN(Script_YieldSeconds), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void YieldFrames(uint)",
        ENJIN_AS_FN(Script_YieldFrames), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void YieldEndOfFrame()",
        ENJIN_AS_FN(Script_YieldEndOfFrame), ENJIN_AS_CALL_CDECL));
}

// ---------------------------------------------------------------------------
// RegisterEventBindings
// ---------------------------------------------------------------------------
void RegisterEventBindings(asIScriptEngine* engine) {
    // EventData reference type
    AS_CHECK(engine->RegisterObjectType("EventData", 0, asOBJ_REF));
    AS_CHECK(engine->RegisterObjectBehaviour("EventData", asBEHAVE_FACTORY,
        "EventData@ f()", ENJIN_AS_FN(EventData_Factory), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterObjectBehaviour("EventData", asBEHAVE_ADDREF,
        "void f()", ENJIN_AS_MFN(ScriptEventData, AddRef), ENJIN_AS_CALL_THISCALL));
    AS_CHECK(engine->RegisterObjectBehaviour("EventData", asBEHAVE_RELEASE,
        "void f()", ENJIN_AS_MFN(ScriptEventData, Release), ENJIN_AS_CALL_THISCALL));

    AS_CHECK(engine->RegisterObjectMethod("EventData", "void SetFloat(const string &in, float)",
        ENJIN_AS_OBJ_FIRST(EventData_SetFloat), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("EventData", "float GetFloat(const string &in)",
        ENJIN_AS_OBJ_FIRST(EventData_GetFloat), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("EventData", "void SetInt(const string &in, int)",
        ENJIN_AS_OBJ_FIRST(EventData_SetInt), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("EventData", "int GetInt(const string &in)",
        ENJIN_AS_OBJ_FIRST(EventData_GetInt), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("EventData", "void SetString(const string &in, const string &in)",
        ENJIN_AS_OBJ_FIRST(EventData_SetString), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("EventData", "string GetString(const string &in)",
        ENJIN_AS_OBJ_FIRST(EventData_GetString), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("EventData", "void SetEntity(const string &in, uint64)",
        ENJIN_AS_OBJ_FIRST(EventData_SetEntity), ENJIN_AS_CALL_CDECL_OBJFIRST));
    AS_CHECK(engine->RegisterObjectMethod("EventData", "uint64 GetEntity(const string &in)",
        ENJIN_AS_OBJ_FIRST(EventData_GetEntity), ENJIN_AS_CALL_CDECL_OBJFIRST));

    // Event system functions
    AS_CHECK(engine->RegisterFuncdef("void EventCallback(const string &in)"));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint Events_Listen(const string &in, EventCallback@)",
        ENJIN_AS_FN(Events_Listen), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Events_Send(const string &in, EventData@)",
        ENJIN_AS_FN(Events_Send), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Events_Broadcast(EventData@)",
        ENJIN_AS_FN(Events_Broadcast), ENJIN_AS_CALL_CDECL));

    // Read the payload of the event currently being dispatched (valid inside
    // an EventCallback). UI bridge keys: "value" (slider), "checked" (toggle),
    // "text" (button label).
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Events_CurrentFloat(const string &in)",
        ENJIN_AS_FN(Events_CurrentFloat), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Events_CurrentInt(const string &in)",
        ENJIN_AS_FN(Events_CurrentInt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "string Events_CurrentString(const string &in)",
        ENJIN_AS_FN(Events_CurrentString), ENJIN_AS_CALL_CDECL));
}

// ---------------------------------------------------------------------------
// VisualScript Interop Bindings
// ---------------------------------------------------------------------------

static void VisualScript_SendEvent(u64 entity, const std::string& eventName) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<ECS::Entity>(entity))) return;
    auto* script = s_BindingsWorld->GetComponent<ECS::VisualScriptComponent>(static_cast<ECS::Entity>(entity));
    if (!script) return;

    // Verify the custom event exists; log if not found
    auto nodeId = script->GetCustomEventNode(eventName);
    if (nodeId == 0) {
        ENJIN_LOG_WARN(Script, "VisualScript_SendEvent: No handler for event '%s' on entity %llu",
                       eventName.c_str(), entity);
    }
}

static void VisualScript_SetVariable(u64 entity, const std::string& varName, float value) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<ECS::Entity>(entity))) return;
    auto* script = s_BindingsWorld->GetComponent<ECS::VisualScriptComponent>(static_cast<ECS::Entity>(entity));
    if (!script) return;

    auto* var = script->FindVariable(varName);
    if (var) {
        var->value = value;
    } else {
        ENJIN_LOG_WARN(Script, "VisualScript_SetVariable: Variable '%s' not found on entity %llu",
                       varName.c_str(), entity);
    }
}

static float VisualScript_GetVariable(u64 entity, const std::string& varName) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<ECS::Entity>(entity))) return 0.0f;
    auto* script = s_BindingsWorld->GetComponent<ECS::VisualScriptComponent>(static_cast<ECS::Entity>(entity));
    if (!script) return 0.0f;

    const auto* var = script->FindVariable(varName);
    if (var && std::holds_alternative<f32>(var->value)) {
        return std::get<f32>(var->value);
    }
    return 0.0f;
}

// ---------------------------------------------------------------------------
// DataAsset bindings
// ---------------------------------------------------------------------------

static bool DataAsset_Load(const std::string& assetName) {
    return Enjin::Assets::DataAssetRegistry::Get().FindAsset(assetName) != nullptr;
}

static float DataAsset_GetFloat(const std::string& assetName, const std::string& field) {
    return Enjin::Assets::DataAssetRegistry::Get().GetFloat(assetName, field);
}

static int DataAsset_GetInt(const std::string& assetName, const std::string& field) {
    return Enjin::Assets::DataAssetRegistry::Get().GetInt(assetName, field);
}

static bool DataAsset_GetBool(const std::string& assetName, const std::string& field) {
    return Enjin::Assets::DataAssetRegistry::Get().GetBool(assetName, field);
}

static std::string DataAsset_GetString(const std::string& assetName, const std::string& field) {
    return Enjin::Assets::DataAssetRegistry::Get().GetString(assetName, field);
}

static Vector3 DataAsset_GetVector3(const std::string& assetName, const std::string& field) {
    return Enjin::Assets::DataAssetRegistry::Get().GetVector3(assetName, field);
}

static void RegisterDataAssetBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool DataAsset_Load(const string &in)",
        ENJIN_AS_FN(DataAsset_Load), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float DataAsset_GetFloat(const string &in, const string &in)",
        ENJIN_AS_FN(DataAsset_GetFloat), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int DataAsset_GetInt(const string &in, const string &in)",
        ENJIN_AS_FN(DataAsset_GetInt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool DataAsset_GetBool(const string &in, const string &in)",
        ENJIN_AS_FN(DataAsset_GetBool), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "string DataAsset_GetString(const string &in, const string &in)",
        ENJIN_AS_FN(DataAsset_GetString), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 DataAsset_GetVector3(const string &in, const string &in)",
        ENJIN_AS_FN(DataAsset_GetVector3), ENJIN_AS_CALL_CDECL));
}

static void RegisterVisualScriptBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction(
        "void VisualScript_SendEvent(uint64, const string &in)",
        ENJIN_AS_FN(VisualScript_SendEvent), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void VisualScript_SetVariable(uint64, const string &in, float)",
        ENJIN_AS_FN(VisualScript_SetVariable), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float VisualScript_GetVariable(uint64, const string &in)",
        ENJIN_AS_FN(VisualScript_GetVariable), ENJIN_AS_CALL_CDECL));
}

// Forward declarations for binding registration functions defined in other TUs
void RegisterAIBindings(asIScriptEngine* engine);
void RegisterAccessibilityBindings(asIScriptEngine* engine);
void RegisterFlashAPIBindings(asIScriptEngine* engine);

// ---------------------------------------------------------------------------
// RegisterAllBindings
// ---------------------------------------------------------------------------
void RegisterAllBindings(asIScriptEngine* engine) {
    // S14: Seeding handled by xorshift32 in Math.h — no srand() needed

    RegisterMathTypes(engine);
    RegisterEntityTypes(engine);
    RegisterInputBindings(engine);
    RegisterPhysicsBindings(engine);
    RegisterAudioBindings(engine);
    RegisterSceneBindings(engine);
    RegisterTimeBindings(engine);
    RegisterDebugBindings(engine);
    RegisterComponentBindings(engine);
    RegisterCoroutineBindings(engine);
    RegisterEventBindings(engine);
    RegisterTweenBindings(engine);
    RegisterStateMachineBindings(engine);
    RegisterDialogueBindings(engine);
    RegisterRenderBindings(engine);
    RegisterNoiseBindings(engine);
    RegisterVisualScriptBindings(engine);
    RegisterDataAssetBindings(engine);
    RegisterSaveBindings(engine);
    RegisterWeatherBindings(engine);
    RegisterGameplayBindings(engine);
    RegisterUIBindings(engine);
    RegisterParticleBindings(engine);
    RegisterPrefabBindings(engine);
    RegisterStreamingBindings(engine);
    RegisterFlowerBindings(engine);
    RegisterPhysics2DBindings(engine);
    RegisterNetworkBindings(engine);
    RegisterSpriteBindings(engine);
    RegisterAIBindings(engine);
    RegisterAccessibilityBindings(engine);
    RegisterProceduralBindings(engine);
    RegisterPluginBindings(engine);
    RegisterAudioGraphBindings(engine);
    RegisterFlashAPIBindings(engine);
    RegisterMIDIBindings(engine);
    RegisterInputActionBindings(engine);
    RegisterHUDBindings(engine);
    RegisterTextBindings(engine);
    RegisterElementalBindings(engine);
    RegisterRewindBindings(engine);
    RegisterAudioReactiveBindings(engine);

    ENJIN_LOG_INFO(Script, "AngelScript bindings registered successfully");
}

} // namespace Scripting
} // namespace Enjin
