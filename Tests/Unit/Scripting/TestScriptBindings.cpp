#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Effects/SeasonalWeather.h"
#include <angelscript.h>
#include <filesystem>
#include <fstream>
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
//
// These used to be named *DoesNotCrash and assert nothing at all. That is not a
// pedantic complaint: this exact family covered the surface that shipped a web
// player with ZERO script bindings for months. On WASM, AngelScript forces
// AS_MAX_PORTABILITY and every raw asFUNCTION registration fails with
// asNOT_SUPPORTED (-7) -- a RETURN CODE, not a crash. So "it did not crash" was
// true the whole time, the tests were green the whole time, and every script in
// every browser build failed to compile.
//
// The assertion that catches that is: registration REGISTERED SOMETHING. Each
// test now counts the engine's global functions and types before and after, and
// names one symbol it must be able to find afterwards.
// ===========================================================================

namespace {

// Total registered surface: global functions + object types. Counting both
// matters because some subsystems register only types (math) and some only
// functions (time, debug).
struct RegisteredSurface {
    asUINT functions = 0;
    asUINT types = 0;
    asUINT Total() const { return functions + types; }
};

RegisteredSurface Surface(asIScriptEngine* as) {
    return { as->GetGlobalFunctionCount(), as->GetObjectTypeCount() };
}

} // namespace

ENJIN_TEST(BindingRegistration, RegisterMathTypesActuallyRegisters) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);

    const RegisteredSurface before = Surface(as);
    RegisterMathTypes(as);
    const RegisteredSurface after = Surface(as);

    ENJIN_EXPECT_TRUE(after.Total() > before.Total());
    ENJIN_EXPECT_TRUE(HasType(as, "Vector3"));
    ENJIN_EXPECT_TRUE(HasType(as, "Vector2"));
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterEntityTypesActuallyRegisters) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);

    // Math must come first — EntityHandle methods return Vector3
    RegisterMathTypes(as);
    const RegisteredSurface before = Surface(as);
    RegisterEntityTypes(as);
    const RegisteredSurface after = Surface(as);

    ENJIN_EXPECT_TRUE(after.Total() > before.Total());
    ENJIN_EXPECT_TRUE(HasType(as, "EntityHandle"));
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterInputBindingsActuallyRegisters) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);

    RegisterMathTypes(as);
    const asUINT before = as->GetGlobalFunctionCount();
    RegisterInputBindings(as);
    ENJIN_EXPECT_TRUE(as->GetGlobalFunctionCount() > before);
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterAudioBindingsActuallyRegisters) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);

    RegisterMathTypes(as);
    RegisterEntityTypes(as);
    const asUINT before = as->GetGlobalFunctionCount();
    RegisterAudioBindings(as);
    ENJIN_EXPECT_TRUE(as->GetGlobalFunctionCount() > before);
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterDebugBindingsActuallyRegisters) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);

    const asUINT before = as->GetGlobalFunctionCount();
    RegisterDebugBindings(as);
    ENJIN_EXPECT_TRUE(as->GetGlobalFunctionCount() > before);
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterTimeBindingsActuallyRegisters) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);

    const asUINT before = as->GetGlobalFunctionCount();
    RegisterTimeBindings(as);
    ENJIN_EXPECT_TRUE(as->GetGlobalFunctionCount() > before);
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, TheDateIsReadableFromScript) {
    // A calendar game has to be able to ask what day it is. Until these landed,
    // script could read the hour and the season and nothing else, so nothing
    // could be scheduled against the calendar the engine already kept: no
    // holiday, no visitor, no "the farm is ready on the ninth".
    //
    // Checking the exact declaration matters more than checking the count. A
    // typo in the decl string still registers a function, it just registers one
    // no script can call, and the registration itself reports success.
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);

    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "int WorldTime_GetDay()"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "int WorldTime_GetMonth()"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "int WorldTime_GetYear()"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "int WorldTime_GetDayOfYear()"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "void WorldTime_SetDate(int, int, int)"));
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, TheForecastIsReadableFromScript) {
    // Weather is a pure function of the world seed and the date, which is the
    // only reason a script can be told what Thursday looks like on Monday. If
    // this surface is missing, the forecast board cannot exist.
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);

    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "int WorldTime_GetWeatherOn(int)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "string WorldTime_GetWorldSeed()"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "void WorldTime_SetWorldSeed(const string&in)"));
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, AuthoredDaysAreSettableFromScript) {
    // The engine deliberately does not know the game's calendar file format, so
    // this is the whole seam between them: the game reads its own year and pushes
    // the authored days in through these four. If the seam is missing, an
    // authored calendar cannot reach the weather system at all.
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);

    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "void WorldTime_SetAuthoredWeather(int, int)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "void WorldTime_ClearAuthoredWeather()"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "bool WorldTime_IsAuthored(int)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "int WorldTime_GetAuthoredDayCount()"));
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterAllBindingsRegistersTheWholeSurface) {
    // The one that would have caught the web bug outright. Roughly 940 global
    // functions are registered today; the floor is deliberately far below that so
    // the test tracks "the registry is populated", not a number someone has to
    // maintain -- but it is far enough above zero that a wholesale
    // asNOT_SUPPORTED failure cannot slip past.
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);

    RegisterAllBindings(as);
    ENJIN_EXPECT_TRUE(as->GetGlobalFunctionCount() > 400);
    ENJIN_EXPECT_TRUE(as->GetObjectTypeCount() >= 8);
    engine.Shutdown();
}

ENJIN_TEST(BindingRegistration, RegisterAllBindingsTwiceOnSeparateEnginesBothPopulated) {
    // Two fully independent engines — registration must succeed on BOTH, not
    // merely not crash on both.
    ScriptEngine engine1;
    ScriptEngine engine2;
    ENJIN_ASSERT_TRUE(engine1.Initialize());
    ENJIN_ASSERT_TRUE(engine2.Initialize());
    RegisterAllBindings(engine1.GetASEngine());
    RegisterAllBindings(engine2.GetASEngine());

    const asUINT c1 = engine1.GetASEngine()->GetGlobalFunctionCount();
    const asUINT c2 = engine2.GetASEngine()->GetGlobalFunctionCount();
    ENJIN_EXPECT_TRUE(c1 > 400);
    ENJIN_EXPECT_EQ(c1, c2);   // the second engine is not short-changed
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

// Raw keys follow input focus, like InputAction_*. They used to read the
// keyboard directly, so a script still saw W while the player typed it into
// the console.
ENJIN_TEST(Execution, RawKeysAreSilentOutsideGameplayFocus) {
    // Arrange: a script that reads Space, and Space held down.
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_ASSERT_TRUE(engine.CompileScriptFromMemory("rawkey_exec",
        "bool gHeld = false;\n"
        "void Read() { gHeld = Input_GetKey(Key::Space); }\n"));
    asIScriptModule* mod = engine.GetASEngine()->GetModule("rawkey_exec");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* func = mod->GetFunctionByName("Read");
    ENJIN_ASSERT_NOT_NULL(func);
    bool* held = static_cast<bool*>(mod->GetAddressOfGlobalVar(mod->GetGlobalVarIndexByName("gHeld")));
    ENJIN_ASSERT_NOT_NULL(held);

    bool keys[512] = {};
    bool mouse[8] = {};
    keys[static_cast<int>(KeyCode::Space)] = true;
    Input::SetReplayInjection(true);
    Input::InjectFrameState(keys, mouse, Math::Vector2(0.0f, 0.0f));
    Input::Update();

    auto read = [&](Input::InputFocus focus) {
        Input::SetInputFocus(focus);
        asIScriptContext* ctx = engine.AcquireContext();
        ctx->Prepare(func);
        ctx->Execute();
        engine.ReturnContext(ctx);
        return *held;
    };

    // Act / Assert
    ENJIN_EXPECT_TRUE(read(Input::InputFocus::Gameplay));
    ENJIN_EXPECT_FALSE(read(Input::InputFocus::Console));
    ENJIN_EXPECT_FALSE(read(Input::InputFocus::Menu));
    ENJIN_EXPECT_FALSE(read(Input::InputFocus::Dialogue));

    Input::SetInputFocus(Input::InputFocus::Gameplay);
    Input::SetReplayInjection(false);
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

// ===========================================================================
// Water interaction bindings (splash / wake / sustained pressure / height query)
// ===========================================================================

ENJIN_TEST(WaterBindings, RegisteredWithCorrectSignatures) {
    // The script compiles only if Water_* are registered with these exact signatures
    // (also exercises the ENJIN_AS macro path that WASM requires). No execution needed.
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    const char* src =
        "void Use() {\n"
        "    uint64 e = 1;\n"
        "    Water_Splash(e, 0.0f, 0.0f, 1.0f);\n"
        "    Water_Wake(e, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f);\n"
        "    Water_SustainedPressure(e, 0.0f, 0.0f, 2.0f, 1.0f);\n"
        "    float h = Water_GetHeight(e, 0.0f, 0.0f);\n"
        "}\n";
    ENJIN_ASSERT_TRUE(engine.CompileScriptFromMemory("water_bindings_test", src));

    engine.Shutdown();
}

// ===========================================================================
// Gameplay-component bindings (parity gap: previously-unbound components)
// ===========================================================================

ENJIN_TEST(GameplayComponentBindings, RegisteredWithCorrectSignatures) {
    // Compiles only if the newly-bound component accessors are registered with these
    // signatures (exercises the ENJIN_AS macro path that WASM requires).
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    const char* src =
        "void Use() {\n"
        "    uint64 e = 1;\n"
        "    LookAt_SetTarget(e, 2);\n"
        "    LookAt_SetTargetPosition(e, 0.0f, 1.0f, 0.0f);\n"
        "    LookAt_SetSpeed(e, 90.0f);\n"
        "    float s = LookAt_GetSpeed(e);\n"
        "    DamageResist_Set(e, \"fire\", 0.5f);\n"
        "    float r = DamageResist_Get(e, \"fire\");\n"
        "    Ragdoll_SetActive(e, true);\n"
        "    bool rg = Ragdoll_IsActive(e);\n"
        "    Pushable_SetAxes(e, true, false, true);\n"
        "    TempZone_SetTemperature(e, -5.0f);\n"
        "    ReflectionProbe_SetIntensity(e, 0.8f);\n"
        "    Billboard_SetLockY(e, true);\n"
        "    bool p = Possessable_IsPossessed(e);\n"
        "    SavePoint_SetSlot(e, 2);\n"
        "    Footstep_SetVolume(e, 0.7f);\n"
        "    Reverb_SetWetDryMix(e, 0.4f);\n"
        "    Lens_SetVignette(e, 0.3f, 0.5f);\n"
        "    SpringJoint_SetStiffness(e, 80.0f);\n"
        "    SliderJoint_SetMotor(e, true, 2.0f, 100.0f);\n"
        "    BallSocket_SetConeLimit(e, true, 30.0f);\n"
        "}\n";
    ENJIN_ASSERT_TRUE(engine.CompileScriptFromMemory("gameplay_component_bindings", src));

    engine.Shutdown();
}

// ===========================================================================
// Loose files, JSON, and the shape of the year
//
// The seam a game uses to read a calendar it did not pack. Shells keeps its year
// in years/*.json beside the exe so a player can drop their own in; before
// these existed a script could not open a file or read JSON at all.
// ===========================================================================

namespace {

// Compile `source` and run its `int main()`. Returns the value, or -9999 when
// the script did not compile or did not finish.
int RunIntMain(ScriptEngine& engine, const char* module, const std::string& source) {
    if (!engine.CompileScriptFromMemory(module, source)) return -9999;
    asIScriptModule* mod = engine.GetASEngine()->GetModule(module);
    if (!mod) return -9999;
    asIScriptFunction* fn = mod->GetFunctionByDecl("int main()");
    if (!fn) return -9999;
    asIScriptContext* ctx = engine.AcquireContext();
    int result = -9999;
    if (ctx->Prepare(fn) >= 0 && ctx->Execute() == asEXECUTION_FINISHED)
        result = static_cast<int>(ctx->GetReturnDWord());
    engine.ReturnContext(ctx);
    return result;
}

} // namespace

ENJIN_TEST(BindingRegistration, FilesJsonAndCalendarAreRegistered) {
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    asIScriptEngine* as = engine.GetASEngine();

    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "string File_ReadText(const string&in)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "bool File_Exists(const string&in)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "int Json_Parse(const string&in)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "string Json_GetKeyAt(int, const string&in, int)"));
    ENJIN_EXPECT_TRUE(HasGlobalFunction(as, "void WorldTime_SetCalendar(int, int)"));
    engine.Shutdown();
}

ENJIN_TEST(FileBindings, JsonReadsByPointerAndFallsBackOnTypos) {
    // A hand-edited year file: a day keyed by number, a string where a number
    // belongs. The typo must come back as the fallback, not as garbage.
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));

    const int r = RunIntMain(engine, "json_test", R"(
        int main() {
            int doc = Json_Parse("""{"seed":"shells-1","yearLength":"120","days":{"5":{"weather":"rain"},"22":{"weather":"clear"}}}""");
            if (doc <= 0) return 1;
            if (Json_GetString(doc, "/seed", "") != "shells-1") return 2;
            if (Json_GetInt(doc, "/yearLength", -1) != -1) return 3;
            if (Json_GetCount(doc, "/days") != 2) return 4;
            if (Json_GetKeyAt(doc, "/days", 0) != "22") return 5;   // sorted, not file order
            if (Json_GetString(doc, "/days/5/weather", "") != "rain") return 6;
            if (Json_Has(doc, "/days/6")) return 7;
            if (Json_GetString(doc, "/days/6/weather", "none") != "none") return 8;
            Json_Free(doc);
            if (Json_Has(doc, "/seed")) return 9;
            if (Json_Parse("{ not json") != 0) return 10;
            return 0;
        }
    )");
    ENJIN_EXPECT_EQ(r, 0);
    engine.Shutdown();
}

ENJIN_TEST(FileBindings, ReadsUnderTheRootAndNothingOutsideIt) {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "enjin_file_bindings_test";
    fs::create_directories(root / "years");
    { std::ofstream(root / "years" / "y.json") << "{\"seed\":\"kelp-7\"}"; }
    { std::ofstream(root.parent_path() / "enjin_outside_root.txt") << "secret"; }
    SetBindingsFileRoot(root.string());

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    const int r = RunIntMain(engine, "file_test", R"(
        int main() {
            if (!File_Exists("years/y.json")) return 1;
            int doc = Json_Parse(File_ReadText("years/y.json"));
            if (Json_GetString(doc, "/seed", "") != "kelp-7") return 2;
            if (File_Exists("years/missing.json")) return 3;
            if (File_ReadText("../enjin_outside_root.txt") != "") return 4;
            if (File_Exists("../enjin_outside_root.txt")) return 5;
            return 0;
        }
    )");
    ENJIN_EXPECT_EQ(r, 0);
    engine.Shutdown();

    SetBindingsFileRoot("");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::remove(root.parent_path() / "enjin_outside_root.txt", ec);
}

ENJIN_TEST(FileBindings, AFourSeasonYearNumbersItsDaysOneToOneTwenty) {
    // Shells' year is four seasons of thirty days. On the default twelve-month
    // calendar its day 31 was the engine's first of February; with the calendar
    // set to four months of thirty it is the first of summer, as the file means.
    Effects::WorldTimeSystem time;
    Effects::SeasonalWeatherSystem seasonal;
    SetBindingsWorldTime(&time, &seasonal);

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    const int r = RunIntMain(engine, "calendar_test", R"(
        int main() {
            WorldTime_SetCalendar(30, 4);
            if (WorldTime_GetMonthsPerYear() != 4) return 1;
            WorldTime_SetDate(1, 2, 1);
            if (WorldTime_GetDayOfYear() != 31) return 2;
            if (WorldTime_GetSeason() != 1) return 3;
            WorldTime_SetDate(30, 4, 1);
            if (WorldTime_GetDayOfYear() != 120) return 4;
            if (WorldTime_GetSeason() != 3) return 5;
            WorldTime_SetSeason(1);                       // was month 6 of 4
            if (WorldTime_GetMonth() != 2) return 6;
            WorldTime_SetCalendar(30, 12);
            WorldTime_SetSeason(1);                       // default calendar unchanged
            if (WorldTime_GetMonth() != 6) return 7;
            return 0;
        }
    )");
    ENJIN_EXPECT_EQ(r, 0);
    engine.Shutdown();
    SetBindingsWorldTime(nullptr, nullptr);
}

ENJIN_TEST(FileBindings, ShrinkingTheCalendarClampsTheDate) {
    // The default date is June; a four-month year has no June. The date has to
    // land somewhere real on the same frame, not at the next midnight.
    Effects::WorldTimeSystem time;
    time.SetTime(12.0f, 15, 6, 1);
    SetBindingsWorldTime(&time, nullptr);

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    const int r = RunIntMain(engine, "clamp_test", R"(
        int main() {
            WorldTime_SetCalendar(30, 4);
            if (WorldTime_GetMonth() != 4) return 1;
            if (WorldTime_GetDay() != 15) return 2;
            if (WorldTime_GetDayOfYear() != 105) return 3;
            return 0;
        }
    )");
    ENJIN_EXPECT_EQ(r, 0);
    engine.Shutdown();
    SetBindingsWorldTime(nullptr, nullptr);
}

// SD-25: Health_Damage from script had no source, so every script hit was
// Physical and resistances and weaknesses were out of its reach. The typed
// overload carries the type through the shared damage path.
ENJIN_TEST(Execution, ScriptDamageCanBeTyped) {
    // Arrange: a target that takes half damage from fire and double from ice.
    ECS::World world;
    const ECS::Entity target = world.CreateEntity();
    auto& hp = world.AddComponent<ECS::HealthComponent>(target);
    hp.maxHealth = hp.currentHealth = 100.0f;
    auto& resist = world.AddComponent<ECS::DamageResistanceComponent>(target);
    resist.fireMult = 0.5f;
    resist.iceMult = 2.0f;
    SetBindingsWorld(&world);

    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(InitWithBindings(engine));
    ENJIN_ASSERT_TRUE(engine.CompileScriptFromMemory("typed_damage",
        "uint64 gTarget = 0;\n"
        "void Burn()   { Health_Damage(gTarget, 10.0f, DamageType::Fire); }\n"
        "void Freeze() { Health_Damage(gTarget, 10.0f, DamageType::Ice); }\n"
        "void Hit()    { Health_Damage(gTarget, 10.0f); }\n"));
    asIScriptModule* mod = engine.GetASEngine()->GetModule("typed_damage");
    ENJIN_ASSERT_NOT_NULL(mod);
    *static_cast<u64*>(mod->GetAddressOfGlobalVar(mod->GetGlobalVarIndexByName("gTarget"))) = target;
    auto run = [&](const char* fn) {
        asIScriptContext* ctx = engine.AcquireContext();
        ctx->Prepare(mod->GetFunctionByName(fn));
        ctx->Execute();
        engine.ReturnContext(ctx);
        return world.GetComponent<ECS::HealthComponent>(target)->currentHealth;
    };

    // Act / Assert
    ENJIN_EXPECT_FLOAT_EQ(run("Burn"), 95.0f);     // fire at half
    ENJIN_EXPECT_FLOAT_EQ(run("Freeze"), 75.0f);   // ice at double
    ENJIN_EXPECT_FLOAT_EQ(run("Hit"), 65.0f);      // untyped stays physical

    SetBindingsWorld(nullptr);
    engine.Shutdown();
}

ENJIN_TEST_MAIN()
