#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/ECS/World.h"
#include <angelscript.h>
#include <string>

using namespace Enjin;
using namespace Enjin::Scripting;

// ===========================================================================
// Helpers
// ===========================================================================

// Initialize an engine and register all bindings in one step.
// Caller owns the engine and must call Shutdown().
static bool InitWithBindings(ScriptEngine& engine) {
    if (!engine.Initialize()) return false;
    RegisterAllBindings(engine.GetASEngine());
    return true;
}

// Returns true if the named global function is registered in the AS engine.
static bool HasGlobalFunction(asIScriptEngine* as, const char* decl) {
    return as->GetGlobalFunctionByDecl(decl) != nullptr;
}

// Returns true if the named type is registered (typeId >= 0).
static bool HasType(asIScriptEngine* as, const char* decl) {
    return as->GetTypeIdByDecl(decl) >= 0;
}

// Returns true if the named enum exists and has at least one value.
static bool HasEnum(asIScriptEngine* as, const char* name) {
    for (asUINT i = 0; i < as->GetEnumCount(); ++i) {
        asITypeInfo* ti = as->GetEnumByIndex(i);
        if (ti && std::string(ti->GetName()) == name) return true;
    }
    return false;
}

// ===========================================================================
// Binding Registration — individual subsystems
// ===========================================================================

ENJIN_TEST(BindingRegistration, RegisterMathTypesDoesNotCrash) {
    ScriptEngine engine;
    engine.Initialize();
    RegisterMathTypes(engine.GetASEngine()); // must not crash or assert
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterEntityTypesDoesNotCrash) {
    ScriptEngine engine;
    engine.Initialize();
    // Math must come first — EntityHandle methods return Vector3
    RegisterMathTypes(engine.GetASEngine());
    RegisterEntityTypes(engine.GetASEngine());
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterInputBindingsDoesNotCrash) {
    ScriptEngine engine;
    engine.Initialize();
    RegisterMathTypes(engine.GetASEngine());
    RegisterInputBindings(engine.GetASEngine());
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterAudioBindingsDoesNotCrash) {
    ScriptEngine engine;
    engine.Initialize();
    RegisterMathTypes(engine.GetASEngine());
    RegisterEntityTypes(engine.GetASEngine());
    RegisterAudioBindings(engine.GetASEngine());
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterDebugBindingsDoesNotCrash) {
    ScriptEngine engine;
    engine.Initialize();
    RegisterDebugBindings(engine.GetASEngine());
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterTimeBindingsDoesNotCrash) {
    ScriptEngine engine;
    engine.Initialize();
    RegisterTimeBindings(engine.GetASEngine());
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterAllBindingsDoesNotCrash) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    RegisterAllBindings(engine.GetASEngine());
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterAllBindingsTwiceOnSeparateEnginesDoesNotCrash) {
    // Two fully independent engines — registration must succeed on both
    ScriptEngine engine1;
    ScriptEngine engine2;
    ENJIN_ASSERT_TRUE(engine1.Initialize());
    ENJIN_ASSERT_TRUE(engine2.Initialize());
    RegisterAllBindings(engine1.GetASEngine());
    RegisterAllBindings(engine2.GetASEngine());
    engine1.Shutdown();
    engine2.Shutdown();
}

// ===========================================================================
// Type Registration — key types exist after binding
// ===========================================================================

ENJIN_TEST(TypeRegistration, Vector2Exists) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasType(engine.GetASEngine(), "Vector2"));
    engine.Shutdown();
}

ENJIN_TEST(TypeRegistration, Vector3Exists) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasType(engine.GetASEngine(), "Vector3"));
    engine.Shutdown();
}

ENJIN_TEST(TypeRegistration, Vector4Exists) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasType(engine.GetASEngine(), "Vector4"));
    engine.Shutdown();
}

ENJIN_TEST(TypeRegistration, QuaternionExists) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasType(engine.GetASEngine(), "Quaternion"));
    engine.Shutdown();
}

ENJIN_TEST(TypeRegistration, EntityTypedefExists) {
    // Entity is registered as a typedef for uint64
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasType(engine.GetASEngine(), "Entity"));
    engine.Shutdown();
}

ENJIN_TEST(TypeRegistration, EntityHandleExists) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasType(engine.GetASEngine(), "EntityHandle"));
    engine.Shutdown();
}

ENJIN_TEST(TypeRegistration, TransformProxyExists) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasType(engine.GetASEngine(), "TransformProxy"));
    engine.Shutdown();
}

// ===========================================================================
// Input Binding Registration — enums and functions
// ===========================================================================

ENJIN_TEST(InputBindings, KeyEnumRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasEnum(engine.GetASEngine(), "Key"));
    engine.Shutdown();
}

ENJIN_TEST(InputBindings, MouseBtnEnumRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasEnum(engine.GetASEngine(), "MouseBtn"));
    engine.Shutdown();
}

ENJIN_TEST(InputBindings, GamepadBtnEnumRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasEnum(engine.GetASEngine(), "GamepadBtn"));
    engine.Shutdown();
}

ENJIN_TEST(InputBindings, GetKeyFunctionRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(engine.GetASEngine(), "bool Input_GetKey(int)"));
    engine.Shutdown();
}

ENJIN_TEST(InputBindings, GetKeyDownFunctionRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(engine.GetASEngine(), "bool Input_GetKeyDown(int)"));
    engine.Shutdown();
}

ENJIN_TEST(InputBindings, GetKeyUpFunctionRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(engine.GetASEngine(), "bool Input_GetKeyUp(int)"));
    engine.Shutdown();
}

ENJIN_TEST(InputBindings, GetMouseButtonFunctionRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(engine.GetASEngine(), "bool Input_GetMouseButton(int)"));
    engine.Shutdown();
}

// ===========================================================================
// Audio Binding Registration — functions exist
// ===========================================================================

ENJIN_TEST(AudioBindings, AudioPlayFunctionRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(engine.GetASEngine(), "void Audio_Play(uint64)"));
    engine.Shutdown();
}

ENJIN_TEST(AudioBindings, AudioStopFunctionRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(engine.GetASEngine(), "void Audio_Stop(uint64)"));
    engine.Shutdown();
}

ENJIN_TEST(AudioBindings, AudioStopAllFunctionRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(engine.GetASEngine(), "void Audio_StopAll()"));
    engine.Shutdown();
}

ENJIN_TEST(AudioBindings, AudioSetVolumeFunctionRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(engine.GetASEngine(), "void Audio_SetVolume(uint64, float)"));
    engine.Shutdown();
}

ENJIN_TEST(AudioBindings, AudioPlayAtPositionFunctionRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(engine.GetASEngine(),
        "void Audio_PlayAtPosition(const string &in, const Vector3 &in)"));
    engine.Shutdown();
}

// ===========================================================================
// Math Global Functions — registered via RegisterMathTypes
// ===========================================================================

ENJIN_TEST(MathBindings, GlobalMathFunctionsRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "float Sin(float)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "float Cos(float)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "float Sqrt(float)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "float Lerp(float, float, float)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "float Clamp(float, float, float)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "float PI()"));
    engine.Shutdown();
}

ENJIN_TEST(MathBindings, QuaternionGlobalHelpersRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "Quaternion Quaternion_Identity()"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as,
        "Quaternion Quaternion_FromEuler(const Vector3 &in)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as,
        "Quaternion Quaternion_Slerp(const Quaternion &in, const Quaternion &in, float)"));
    engine.Shutdown();
}

// ===========================================================================
// Script Compilation — using bound types
// ===========================================================================

ENJIN_TEST(Compilation, Vector3ScriptCompiles) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("vec3_test",
        "void main() {\n"
        "    Vector3 a(1.0f, 2.0f, 3.0f);\n"
        "    Vector3 b(4.0f, 5.0f, 6.0f);\n"
        "    Vector3 c = a + b;\n"
        "    float len = c.Length();\n"
        "}");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

ENJIN_TEST(Compilation, QuaternionScriptCompiles) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("quat_test",
        "void main() {\n"
        "    Quaternion q = Quaternion_Identity();\n"
        "    Vector3 euler = q.ToEuler();\n"
        "}");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

ENJIN_TEST(Compilation, MathGlobalsScriptCompiles) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("math_globals_test",
        "void main() {\n"
        "    float s = Sin(PI() * 0.5f);\n"
        "    float c = Cos(0.0f);\n"
        "    float r = Sqrt(2.0f);\n"
        "    float v = Clamp(1.5f, 0.0f, 1.0f);\n"
        "    float l = Lerp(0.0f, 10.0f, 0.5f);\n"
        "}");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

ENJIN_TEST(Compilation, InputEnumUsageCompiles) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("input_enum_test",
        "void main() {\n"
        "    bool pressed = Input_GetKey(Key::Space);\n"
        "    bool mouseDown = Input_GetMouseButton(MouseBtn::Left);\n"
        "}");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

ENJIN_TEST(Compilation, EntityHandleScriptCompiles) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("entity_handle_test",
        "void main() {\n"
        "    EntityHandle@ h = EntityHandle();\n"
        "    bool valid = h.IsValid();\n"
        "    uint64 id = h.GetID();\n"
        "}");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

ENJIN_TEST(Compilation, AudioCallsCompile) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("audio_call_test",
        "void main() {\n"
        "    Audio_StopAll();\n"
        "    Audio_SetMasterVolume(1.0f);\n"
        "}");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

// ===========================================================================
// Script Execution — compile and run math operations
// ===========================================================================

ENJIN_TEST(Execution, Vector3AdditionResult) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    const char* src =
        "float gResult = 0.0f;\n"
        "void ComputeLength() {\n"
        "    Vector3 a(3.0f, 0.0f, 0.0f);\n"
        "    Vector3 b(0.0f, 4.0f, 0.0f);\n"
        "    Vector3 c = a + b;\n"
        "    gResult = c.Length();\n"  // sqrt(9+16) = 5
        "}";

    bool ok = engine.CompileScriptFromMemory("vec3_exec", src);
    ENJIN_ASSERT_TRUE(ok);

    asIScriptModule* mod = engine.GetASEngine()->GetModule("vec3_exec");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* func = mod->GetFunctionByName("ComputeLength");
    ENJIN_ASSERT_NOT_NULL(func);

    asIScriptContext* ctx = engine.AcquireContext();
    ctx->Prepare(func);
    int r = ctx->Execute();
    ENJIN_EXPECT_EQ(r, (int)asEXECUTION_FINISHED);
    engine.ReturnContext(ctx);

    int idx = mod->GetGlobalVarIndexByName("gResult");
    ENJIN_ASSERT_TRUE(idx >= 0);
    float* result = static_cast<float*>(mod->GetAddressOfGlobalVar(idx));
    ENJIN_ASSERT_NOT_NULL(result);
    ENJIN_EXPECT_FLOAT_NEAR(*result, 5.0f, 0.001f);

    engine.Shutdown();
}

ENJIN_TEST(Execution, MathLerpResult) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    const char* src =
        "float gResult = 0.0f;\n"
        "void ComputeLerp() {\n"
        "    gResult = Lerp(0.0f, 100.0f, 0.25f);\n"
        "}";

    bool ok = engine.CompileScriptFromMemory("lerp_exec", src);
    ENJIN_ASSERT_TRUE(ok);

    asIScriptModule* mod = engine.GetASEngine()->GetModule("lerp_exec");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* func = mod->GetFunctionByName("ComputeLerp");
    ENJIN_ASSERT_NOT_NULL(func);

    asIScriptContext* ctx = engine.AcquireContext();
    ctx->Prepare(func);
    int r = ctx->Execute();
    ENJIN_EXPECT_EQ(r, (int)asEXECUTION_FINISHED);
    engine.ReturnContext(ctx);

    int idx = mod->GetGlobalVarIndexByName("gResult");
    ENJIN_ASSERT_TRUE(idx >= 0);
    float* result = static_cast<float*>(mod->GetAddressOfGlobalVar(idx));
    ENJIN_ASSERT_NOT_NULL(result);
    ENJIN_EXPECT_FLOAT_NEAR(*result, 25.0f, 0.001f);

    engine.Shutdown();
}

ENJIN_TEST(Execution, Vector3DotProduct) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    const char* src =
        "float gDot = -999.0f;\n"
        "void ComputeDot() {\n"
        "    Vector3 a(1.0f, 0.0f, 0.0f);\n"
        "    Vector3 b(0.0f, 1.0f, 0.0f);\n"
        "    gDot = a.Dot(b);\n"  // perpendicular => 0
        "}";

    bool ok = engine.CompileScriptFromMemory("dot_exec", src);
    ENJIN_ASSERT_TRUE(ok);

    asIScriptModule* mod = engine.GetASEngine()->GetModule("dot_exec");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* func = mod->GetFunctionByName("ComputeDot");
    ENJIN_ASSERT_NOT_NULL(func);

    asIScriptContext* ctx = engine.AcquireContext();
    ctx->Prepare(func);
    int r = ctx->Execute();
    ENJIN_EXPECT_EQ(r, (int)asEXECUTION_FINISHED);
    engine.ReturnContext(ctx);

    int idx = mod->GetGlobalVarIndexByName("gDot");
    ENJIN_ASSERT_TRUE(idx >= 0);
    float* result = static_cast<float*>(mod->GetAddressOfGlobalVar(idx));
    ENJIN_ASSERT_NOT_NULL(result);
    ENJIN_EXPECT_FLOAT_NEAR(*result, 0.0f, 0.001f);

    engine.Shutdown();
}

// ===========================================================================
// Entity Handle Operations via Script
// ===========================================================================

ENJIN_TEST(EntityScript, EntityHandleDefaultInvalidWithoutWorld) {
    // Without a world bound, EntityHandle.IsValid() must return false
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    const char* src =
        "bool gValid = true;\n"
        "void CheckHandle() {\n"
        "    EntityHandle@ h = EntityHandle();\n"
        "    gValid = h.IsValid();\n"
        "}";

    bool ok = engine.CompileScriptFromMemory("entity_valid_test", src);
    ENJIN_ASSERT_TRUE(ok);

    asIScriptModule* mod = engine.GetASEngine()->GetModule("entity_valid_test");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* func = mod->GetFunctionByName("CheckHandle");
    ENJIN_ASSERT_NOT_NULL(func);

    asIScriptContext* ctx = engine.AcquireContext();
    ctx->Prepare(func);
    int r = ctx->Execute();
    ENJIN_EXPECT_EQ(r, (int)asEXECUTION_FINISHED);
    engine.ReturnContext(ctx);

    int idx = mod->GetGlobalVarIndexByName("gValid");
    ENJIN_ASSERT_TRUE(idx >= 0);
    bool* valid = static_cast<bool*>(mod->GetAddressOfGlobalVar(idx));
    ENJIN_ASSERT_NOT_NULL(valid);
    ENJIN_EXPECT_FALSE(*valid);

    engine.Shutdown();
}

ENJIN_TEST(EntityScript, EntityHandleWithWorldIsValid) {
    // Bind a world and create a real entity, then verify IsValid from script
    ECS::World world;
    ECS::Entity e = world.CreateEntity();

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    SetBindingsWorld(&world);

    // Pass the entity id into the script module as a global
    std::string src =
        "bool gValid = false;\n"
        "uint64 gEntityId = " + std::to_string((u64)e) + ";\n"
        "void CheckHandle() {\n"
        "    EntityHandle@ h = EntityHandle(gEntityId);\n"
        "    gValid = h.IsValid();\n"
        "}";

    bool ok = engine.CompileScriptFromMemory("entity_world_test", src);
    ENJIN_ASSERT_TRUE(ok);

    asIScriptModule* mod = engine.GetASEngine()->GetModule("entity_world_test");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* func = mod->GetFunctionByName("CheckHandle");
    ENJIN_ASSERT_NOT_NULL(func);

    asIScriptContext* ctx = engine.AcquireContext();
    ctx->Prepare(func);
    int r = ctx->Execute();
    ENJIN_EXPECT_EQ(r, (int)asEXECUTION_FINISHED);
    engine.ReturnContext(ctx);

    int idx = mod->GetGlobalVarIndexByName("gValid");
    ENJIN_ASSERT_TRUE(idx >= 0);
    bool* valid = static_cast<bool*>(mod->GetAddressOfGlobalVar(idx));
    ENJIN_ASSERT_NOT_NULL(valid);
    ENJIN_EXPECT_TRUE(*valid);

    // Clean up global state
    SetBindingsWorld(nullptr);
    engine.Shutdown();
}

// ===========================================================================
// Script Compilation Errors
// ===========================================================================

ENJIN_TEST(CompilationErrors, UndeclaredTypeIsError) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("bad_type_test",
        "void main() { NonExistentType x; }");
    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_FALSE(engine.GetLastError().empty());

    engine.Shutdown();
}

ENJIN_TEST(CompilationErrors, MissingReturnTypeIsError) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("missing_return_test",
        "int GetValue() { }"); // no return statement for non-void
    ENJIN_EXPECT_FALSE(ok);

    engine.Shutdown();
}

// ===========================================================================
// Script Events — delegate dispatch (regression: Accessibility Demo UI)
// ===========================================================================

// Regression for the "UI does nothing" bug (2026-08-08): listeners registered
// with EventCallback(this.Method) delegates never fired — dispatch prepared
// the delegate then also called SetObject, and Execute failed with
// asCONTEXT_NOT_PREPARED. This test drives the EXACT authored-UI path:
// Events_Listen with a method delegate, then a payload dispatch like the
// UI forwarder sends.
ENJIN_TEST(ScriptEvents, DelegateListenerFiresAndReadsPayload) {
    // Arrange: engine + bindings + event bus wired the way PlayMode wires them
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ScriptEventBus bus;
    bus.SetScriptEngine(&engine);
    SetBindingsEventBus(&bus);

    const char* src =
        "int gHits = 0;\n"
        "float gValue = 0.0f;\n"
        "class Demo {\n"
        "    void Register() {\n"
        "        Events_Listen(\"ui_test\", EventCallback(this.OnEvent));\n"
        "    }\n"
        "    void OnEvent(const string &in ev) {\n"
        "        gHits++;\n"
        "        gValue = Events_CurrentFloat(\"value\");\n"
        "    }\n"
        "}\n"
        "Demo gDemo;\n"
        "void Setup() { gDemo.Register(); }\n";
    ENJIN_ASSERT_TRUE(engine.CompileScriptFromMemory("delegate_event_test", src));

    asIScriptModule* mod = engine.GetASEngine()->GetModule("delegate_event_test");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* setup = mod->GetFunctionByDecl("void Setup()");
    ENJIN_ASSERT_NOT_NULL(setup);
    {
        asIScriptContext* ctx = engine.AcquireContext();
        ENJIN_ASSERT_NOT_NULL(ctx);
        ENJIN_ASSERT_TRUE(ctx->Prepare(setup) >= 0);
        ENJIN_ASSERT_TRUE(ctx->Execute() == asEXECUTION_FINISHED);
        engine.ReturnContext(ctx);
    }

    // Act: dispatch with a payload exactly like the UI slider forwarder
    EventData data;
    data.SetFloat("value", 0.75f);
    bus.Send("ui_test", data);

    // Assert: the delegate ran and read the payload
    int hitsIdx = mod->GetGlobalVarIndexByName("gHits");
    int valueIdx = mod->GetGlobalVarIndexByName("gValue");
    ENJIN_ASSERT_TRUE(hitsIdx >= 0);
    ENJIN_ASSERT_TRUE(valueIdx >= 0);
    int* hits = static_cast<int*>(mod->GetAddressOfGlobalVar(hitsIdx));
    float* value = static_cast<float*>(mod->GetAddressOfGlobalVar(valueIdx));
    ENJIN_ASSERT_NOT_NULL(hits);
    ENJIN_ASSERT_NOT_NULL(value);
    ENJIN_EXPECT_EQ(*hits, 1);
    ENJIN_EXPECT_TRUE(*value > 0.74f && *value < 0.76f);

    // Clean up global state
    bus.Clear();
    SetBindingsEventBus(nullptr);
    engine.Shutdown();
}

ENJIN_TEST(CompilationErrors, UnterminatedStringIsError) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("unterminated_str_test",
        "void main() { string s = \"unterminated; }");
    ENJIN_EXPECT_FALSE(ok);

    engine.Shutdown();
}

ENJIN_TEST(CompilationErrors, UndeclaredFunctionCallIsError) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("bad_call_test",
        "void main() { DoSomethingThatDoesNotExist(42); }");
    ENJIN_EXPECT_FALSE(ok);

    engine.Shutdown();
}

ENJIN_TEST(CompilationErrors, EngineRemainsUsableAfterError) {
    // After a compile error the engine must still accept valid scripts
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    engine.CompileScriptFromMemory("bad_module", "void main() { ??? }");

    bool ok = engine.CompileScriptFromMemory("good_after_bad",
        "void main() { float x = Sin(0.0f); }");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

// ===========================================================================
// Sandbox — instruction limit enforcement
// ===========================================================================

ENJIN_TEST(Sandbox, InfiniteLoopIsAborted) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("infinite_loop",
        "void Run() { while (true) { int x = 1; } }");
    ENJIN_ASSERT_TRUE(ok);

    asIScriptModule* mod = engine.GetASEngine()->GetModule("infinite_loop");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* func = mod->GetFunctionByName("Run");
    ENJIN_ASSERT_NOT_NULL(func);

    asIScriptContext* ctx = engine.AcquireContext();
    ctx->Prepare(func);
    int r = ctx->Execute();
    // Must NOT return asEXECUTION_FINISHED — should be aborted or exception
    ENJIN_EXPECT_NE(r, (int)asEXECUTION_FINISHED);
    engine.ReturnContext(ctx);

    engine.Shutdown();
}

ENJIN_TEST(Sandbox, BoundedLoopFinishes) {
    // A loop well under the 1M instruction limit must complete normally
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    bool ok = engine.CompileScriptFromMemory("bounded_loop",
        "int gSum = 0;\n"
        "void Run() { for (int i = 0; i < 1000; i++) { gSum += i; } }");
    ENJIN_ASSERT_TRUE(ok);

    asIScriptModule* mod = engine.GetASEngine()->GetModule("bounded_loop");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* func = mod->GetFunctionByName("Run");
    ENJIN_ASSERT_NOT_NULL(func);

    asIScriptContext* ctx = engine.AcquireContext();
    ctx->Prepare(func);
    int r = ctx->Execute();
    ENJIN_EXPECT_EQ(r, (int)asEXECUTION_FINISHED);
    engine.ReturnContext(ctx);

    engine.Shutdown();
}

// ===========================================================================
// Multiple Engines — independent registration state
// ===========================================================================

ENJIN_TEST(MultipleEngines, BothEnginesCompileIndependently) {
    ScriptEngine engine1;
    ScriptEngine engine2;
    ENJIN_ASSERT_TRUE(engine1.Initialize());
    ENJIN_ASSERT_TRUE(engine2.Initialize());

    RegisterAllBindings(engine1.GetASEngine());
    RegisterAllBindings(engine2.GetASEngine());

    // Both should compile the same script successfully
    bool ok1 = engine1.CompileScriptFromMemory("test_mod",
        "void main() { Vector3 v(1.0f, 2.0f, 3.0f); }");
    bool ok2 = engine2.CompileScriptFromMemory("test_mod",
        "void main() { Vector3 v(1.0f, 2.0f, 3.0f); }");

    ENJIN_EXPECT_TRUE(ok1);
    ENJIN_EXPECT_TRUE(ok2);

    engine1.Shutdown();
    engine2.Shutdown();
}

ENJIN_TEST(MultipleEngines, ErrorInOneDoesNotAffectOther) {
    ScriptEngine engine1;
    ScriptEngine engine2;
    ENJIN_ASSERT_TRUE(engine1.Initialize());
    ENJIN_ASSERT_TRUE(engine2.Initialize());

    RegisterAllBindings(engine1.GetASEngine());
    RegisterAllBindings(engine2.GetASEngine());

    // Force an error in engine1
    engine1.CompileScriptFromMemory("bad", "void main() { ??? }");

    // engine2 must be unaffected
    bool ok2 = engine2.CompileScriptFromMemory("good",
        "void main() { float x = Sqrt(4.0f); }");
    ENJIN_EXPECT_TRUE(ok2);

    engine1.Shutdown();
    engine2.Shutdown();
}

ENJIN_TEST_MAIN()
