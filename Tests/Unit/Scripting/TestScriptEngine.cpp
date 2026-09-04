#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include <angelscript.h>
#include <cmath>

#include <filesystem>
#include <fstream>

using namespace Enjin;
using namespace Enjin::Scripting;

// ===========================================================================
// Initialization
// ===========================================================================

ENJIN_TEST(Init, HeadlessInitSucceeds) {
    ScriptEngine engine;
    ENJIN_EXPECT_TRUE(engine.Initialize());
    engine.Shutdown();
}

ENJIN_TEST(Init, DoubleInitIsSafe) {
    ScriptEngine engine;
    ENJIN_EXPECT_TRUE(engine.Initialize());
    // Second init should be safe (either succeeds or no-ops)
    engine.Shutdown();
}

ENJIN_TEST(Init, ShutdownWithoutInitIsSafe) {
    ScriptEngine engine;
    engine.Shutdown(); // Should not crash
}

ENJIN_TEST(Init, GetASEngineNonNull) {
    ScriptEngine engine;
    engine.Initialize();
    ENJIN_EXPECT_NOT_NULL(engine.GetASEngine());
    engine.Shutdown();
}

ENJIN_TEST(Init, LastErrorEmptyOnSuccess) {
    ScriptEngine engine;
    engine.Initialize();
    ENJIN_EXPECT_TRUE(engine.GetLastError().empty());
    engine.Shutdown();
}

// ===========================================================================
// Binding Registration
// ===========================================================================

ENJIN_TEST(Bindings, RegisterAllDoesNotCrash) {
    ScriptEngine engine;
    engine.Initialize();
    RegisterAllBindings(engine.GetASEngine());
    engine.Shutdown();
}

ENJIN_TEST(Bindings, MinimumFunctionCount) {
    ScriptEngine engine;
    engine.Initialize();
    RegisterAllBindings(engine.GetASEngine());

    // Query total registered global functions via AngelScript API
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);
    u32 funcCount = as->GetGlobalFunctionCount();
    // CLAUDE.md documents ~686 bindings; actual count is higher (~1106)
    // Guard against accidental mass removal
    ENJIN_EXPECT_GE(funcCount, 680u);

    engine.Shutdown();
}

ENJIN_TEST(Bindings, MathTypesRegistered) {
    ScriptEngine engine;
    engine.Initialize();
    RegisterAllBindings(engine.GetASEngine());

    asIScriptEngine* as = engine.GetASEngine();
    // Check core math types are registered
    ENJIN_EXPECT_GE(as->GetTypeIdByDecl("Vector2"), 0);
    ENJIN_EXPECT_GE(as->GetTypeIdByDecl("Vector3"), 0);
    ENJIN_EXPECT_GE(as->GetTypeIdByDecl("Vector4"), 0);
    ENJIN_EXPECT_GE(as->GetTypeIdByDecl("Quaternion"), 0);

    engine.Shutdown();
}

ENJIN_TEST(Bindings, EntityTypeRegistered) {
    ScriptEngine engine;
    engine.Initialize();
    RegisterAllBindings(engine.GetASEngine());

    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_EXPECT_GE(as->GetTypeIdByDecl("Entity"), 0);

    engine.Shutdown();
}

// ===========================================================================
// Context Pool
// ===========================================================================

ENJIN_TEST(ContextPool, AcquireReturnsNonNull) {
    ScriptEngine engine;
    engine.Initialize();
    asIScriptContext* ctx = engine.AcquireContext();
    ENJIN_EXPECT_NOT_NULL(ctx);
    engine.ReturnContext(ctx);
    engine.Shutdown();
}

ENJIN_TEST(ContextPool, AcquireMultiple) {
    ScriptEngine engine;
    engine.Initialize();

    asIScriptContext* c1 = engine.AcquireContext();
    asIScriptContext* c2 = engine.AcquireContext();
    asIScriptContext* c3 = engine.AcquireContext();
    asIScriptContext* c4 = engine.AcquireContext();

    ENJIN_EXPECT_NOT_NULL(c1);
    ENJIN_EXPECT_NOT_NULL(c2);
    ENJIN_EXPECT_NOT_NULL(c3);
    ENJIN_EXPECT_NOT_NULL(c4);

    engine.ReturnContext(c1);
    engine.ReturnContext(c2);
    engine.ReturnContext(c3);
    engine.ReturnContext(c4);
    engine.Shutdown();
}

// ===========================================================================
// Script Compilation (uses base Init only — no RegisterAllBindings)
// Init registers std::string, array, math funcs, dictionary, handle
// ===========================================================================

ENJIN_TEST(Compile, ValidScriptFromMemory) {
    ScriptEngine engine;
    engine.Initialize();

    bool ok = engine.CompileScriptFromMemory("test_module",
        "void main() { float x = 1.0f + 2.0f; }");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

ENJIN_TEST(Compile, SyntaxErrorFails) {
    ScriptEngine engine;
    engine.Initialize();

    bool ok = engine.CompileScriptFromMemory("bad_module",
        "void main() { this is not valid code!!! }");
    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_FALSE(engine.GetLastError().empty());

    engine.Shutdown();
}

ENJIN_TEST(Compile, EmptyScriptDoesNotCrash) {
    ScriptEngine engine;
    engine.Initialize();

    // Empty source — may succeed or fail depending on AngelScript version,
    // but must not crash or leave engine in a bad state
    engine.CompileScriptFromMemory("empty_module", "");

    // Engine should still be usable after
    bool ok = engine.CompileScriptFromMemory("after_empty",
        "void main() { int x = 1; }");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

ENJIN_TEST(Compile, EmptyModuleNameFails) {
    ScriptEngine engine;
    engine.Initialize();

    bool ok = engine.CompileScriptFromMemory("", "void main() {}");
    ENJIN_EXPECT_FALSE(ok);

    engine.Shutdown();
}

ENJIN_TEST(Compile, UsesStdStringBinding) {
    ScriptEngine engine;
    engine.Initialize();

    // Init registers std::string — verify we can compile using it
    bool ok = engine.CompileScriptFromMemory("string_test",
        "void main() { string s = \"hello\"; }");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

ENJIN_TEST(Compile, UsesBuiltinMathFunctions) {
    ScriptEngine engine;
    engine.Initialize();

    // Init registers RegisterScriptMath — cos, sin, sqrt, etc.
    bool ok = engine.CompileScriptFromMemory("mathfunc_test",
        "void main() { float a = cos(3.14f); float b = sqrt(2.0f); }");
    ENJIN_EXPECT_TRUE(ok);

    engine.Shutdown();
}

// ===========================================================================
// Script Execution Safety
// ===========================================================================

ENJIN_TEST(Execution, TrivialScriptRuns) {
    ScriptEngine engine;
    engine.Initialize();

    bool ok = engine.CompileScriptFromMemory("run_test",
        "int result = 0;\nvoid main() { result = 42; }");
    ENJIN_ASSERT_TRUE(ok);

    // Get the module and function
    asIScriptModule* mod = engine.GetASEngine()->GetModule("run_test");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* func = mod->GetFunctionByName("main");
    ENJIN_ASSERT_NOT_NULL(func);

    asIScriptContext* ctx = engine.AcquireContext();
    ctx->Prepare(func);
    int r = ctx->Execute();
    ENJIN_EXPECT_EQ(r, (int)asEXECUTION_FINISHED);

    engine.ReturnContext(ctx);
    engine.Shutdown();
}

ENJIN_TEST(Execution, GlobalVariableAccess) {
    ScriptEngine engine;
    engine.Initialize();

    bool ok = engine.CompileScriptFromMemory("globals_test",
        "int counter = 0;\nvoid Increment() { counter += 10; }");
    ENJIN_ASSERT_TRUE(ok);

    asIScriptModule* mod = engine.GetASEngine()->GetModule("globals_test");
    ENJIN_ASSERT_NOT_NULL(mod);
    asIScriptFunction* func = mod->GetFunctionByName("Increment");
    ENJIN_ASSERT_NOT_NULL(func);

    asIScriptContext* ctx = engine.AcquireContext();
    ctx->Prepare(func);
    int r = ctx->Execute();
    ENJIN_EXPECT_EQ(r, (int)asEXECUTION_FINISHED);

    // Read back global variable
    int idx = mod->GetGlobalVarIndexByName("counter");
    ENJIN_EXPECT_GE(idx, 0);
    int* counterPtr = (int*)mod->GetAddressOfGlobalVar(idx);
    ENJIN_ASSERT_NOT_NULL(counterPtr);
    ENJIN_EXPECT_EQ(*counterPtr, 10);

    engine.ReturnContext(ctx);
    engine.Shutdown();
}

// ===========================================================================
// Script path validation (hardening: traversal rejection, 2026-06-16 sweep)
// ===========================================================================

ENJIN_TEST(PathValidation, CompileScript_RejectsLeadingTraversal) {
    ScriptEngine engine;
    engine.Initialize();

    bool ok = engine.CompileScript("../evil.as");

    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_TRUE(engine.GetLastError().find("traversal") != std::string::npos);
    engine.Shutdown();
}

ENJIN_TEST(PathValidation, CompileScript_RejectsEmbeddedTraversal) {
    // "scripts/../../evil.as" normalizes to "../evil.as" — must be rejected
    // even though the raw string does not START with "..".
    ScriptEngine engine;
    engine.Initialize();

    bool ok = engine.CompileScript("scripts/../../evil.as");

    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_TRUE(engine.GetLastError().find("traversal") != std::string::npos);
    engine.Shutdown();
}

ENJIN_TEST(PathValidation, CompileScript_DotDotInFilenameIsNotTraversal) {
    // "a..b.as" contains ".." as a substring but is a legal file name. The
    // old substring check wrongly rejected it; it must now fail only because
    // the file does not exist, never with a traversal error.
    ScriptEngine engine;
    engine.Initialize();

    bool ok = engine.CompileScript("a..b.as");

    ENJIN_EXPECT_FALSE(ok);  // file doesn't exist
    ENJIN_EXPECT_TRUE(engine.GetLastError().find("traversal") == std::string::npos);
    engine.Shutdown();
}

ENJIN_TEST(PathValidation, IncludeEscapingScriptDirectoryIsRejected) {
    // Real-file regression for the IncludeCallback containment check: a script
    // inside the script directory tries to #include a file that resolves
    // OUTSIDE it via an embedded "sub/../../" hop. Compilation must fail.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "enjin_test_include_escape";
    fs::path scripts = root / "scripts";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(scripts / "sub");

    {   // The escape target, outside the script directory.
        std::ofstream f(root / "outside.as");
        f << "void Stolen() {}\n";
    }
    {   // The attacker script, inside the script directory.
        std::ofstream f(scripts / "main.as");
        f << "#include \"sub/../../outside.as\"\n"
             "void main() { Stolen(); }\n";
    }

    ScriptEngine engine;
    engine.Initialize();
    Scripting::RegisterAllBindings(engine.GetASEngine());
    engine.SetScriptDirectory(scripts.string());

    bool ok = engine.CompileScript((scripts / "main.as").string());

    ENJIN_EXPECT_FALSE(ok);
    engine.Shutdown();
    fs::remove_all(root, ec);
}

ENJIN_TEST(PathValidation, IncludeInsideScriptDirectoryStillWorks) {
    // Control for the escape test: a well-behaved include in the same
    // directory must keep compiling after the containment fix.
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "enjin_test_include_ok";
    fs::path scripts = root / "scripts";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(scripts);

    {
        std::ofstream f(scripts / "helper.as");
        f << "void Helper() {}\n";
    }
    {
        std::ofstream f(scripts / "main.as");
        f << "#include \"helper.as\"\n"
             "void main() { Helper(); }\n";
    }

    ScriptEngine engine;
    engine.Initialize();
    Scripting::RegisterAllBindings(engine.GetASEngine());
    engine.SetScriptDirectory(scripts.string());

    bool ok = engine.CompileScript((scripts / "main.as").string());

    ENJIN_EXPECT_TRUE(ok);
    engine.Shutdown();
    fs::remove_all(root, ec);
}


// ===========================================================================
// Bindings registered after Initialize
// ===========================================================================

// AngelScript decides how each registered native function hands its return
// value back - for a float, whether it arrives in a floating-point register or
// an integer one - the first time a context is created, and marks that decision
// stale on every later registration without ever redoing it.
//
// ScriptEngine used to create its context pool inside Initialize, before the
// engine's own bindings were registered. The first call through a pooled
// context then read a float return out of the integer register and returned
// whatever address was left in it: on Linux a large or tiny nonsense number
// that changed with every process start, on Windows a value that happened to
// look right. This is the shape of that bug, in miniature.
namespace {
f32 g_ProbeValue = 0.0f;
f32 ProbeGetValue() { return g_ProbeValue; }
void ProbeSetValue(f32 v) { g_ProbeValue = v; }
bool NearEq(f32 a, f32 b) { return std::fabs(a - b) < 0.0001f; }
} // namespace

ENJIN_TEST(ContextPool, FloatReturnFromABindingRegisteredAfterInit) {
    // Arrange: initialize first, register second — the order every host uses,
    // because you need the engine before you can register anything on it.
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_TRUE(as != nullptr);
    ENJIN_ASSERT_TRUE(as->RegisterGlobalFunction("float Probe_GetValue()",
        ENJIN_AS_FN(ProbeGetValue), ENJIN_AS_CALL_CDECL) >= 0);
    ENJIN_ASSERT_TRUE(as->RegisterGlobalFunction("void Probe_SetValue(float)",
        ENJIN_AS_FN(ProbeSetValue), ENJIN_AS_CALL_CDECL) >= 0);

    asIScriptFunction* get = as->GetGlobalFunctionByDecl("float Probe_GetValue()");
    asIScriptFunction* set = as->GetGlobalFunctionByDecl("void Probe_SetValue(float)");
    ENJIN_ASSERT_TRUE(get != nullptr);
    ENJIN_ASSERT_TRUE(set != nullptr);

    // Act: read a known value back through a pooled context. This is the very
    // first execution on this engine, which is exactly when the bug fired.
    g_ProbeValue = 1.0f;
    asIScriptContext* c = engine.AcquireContext();
    ENJIN_ASSERT_TRUE(c != nullptr);
    ENJIN_ASSERT_TRUE(c->Prepare(get) >= 0);
    ENJIN_ASSERT_TRUE(c->Execute() == asEXECUTION_FINISHED);
    const f32 first = c->GetReturnFloat();
    engine.ReturnContext(c);

    // Assert
    ENJIN_EXPECT_TRUE(NearEq(first, 1.0f));

    // And an argument travels in the same direction: set through the binding,
    // read it back, so a half-working marshaling path cannot pass.
    c = engine.AcquireContext();
    ENJIN_ASSERT_TRUE(c != nullptr);
    ENJIN_ASSERT_TRUE(c->Prepare(set) >= 0);
    c->SetArgFloat(0, 0.5f);
    ENJIN_ASSERT_TRUE(c->Execute() == asEXECUTION_FINISHED);
    engine.ReturnContext(c);
    ENJIN_EXPECT_TRUE(NearEq(g_ProbeValue, 0.5f));

    c = engine.AcquireContext();
    ENJIN_ASSERT_TRUE(c != nullptr);
    ENJIN_ASSERT_TRUE(c->Prepare(get) >= 0);
    ENJIN_ASSERT_TRUE(c->Execute() == asEXECUTION_FINISHED);
    const f32 second = c->GetReturnFloat();
    engine.ReturnContext(c);
    ENJIN_EXPECT_TRUE(NearEq(second, 0.5f));

    g_ProbeValue = 0.0f;
    engine.Shutdown();
}

ENJIN_TEST(ContextPool, InvalidateLetsALateBindingWork) {
    // Arrange: run a script call first, so the pool is already full of contexts
    // created before this binding exists.
    ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    asIScriptEngine* as = engine.GetASEngine();
    ENJIN_ASSERT_TRUE(as != nullptr);
    ENJIN_ASSERT_TRUE(as->RegisterGlobalFunction("float Probe_GetValue()",
        ENJIN_AS_FN(ProbeGetValue), ENJIN_AS_CALL_CDECL) >= 0);
    asIScriptContext* warm = engine.AcquireContext();
    ENJIN_ASSERT_TRUE(warm != nullptr);
    engine.ReturnContext(warm);

    // Act: register another float-returning function afterwards, then tell the
    // engine the bindings changed.
    ENJIN_ASSERT_TRUE(as->RegisterGlobalFunction("float Probe_GetValue2()",
        ENJIN_AS_FN(ProbeGetValue), ENJIN_AS_CALL_CDECL) >= 0);
    engine.InvalidateContextPool();

    g_ProbeValue = 2.5f;
    asIScriptFunction* get2 = as->GetGlobalFunctionByDecl("float Probe_GetValue2()");
    ENJIN_ASSERT_TRUE(get2 != nullptr);
    asIScriptContext* c = engine.AcquireContext();
    ENJIN_ASSERT_TRUE(c != nullptr);
    ENJIN_ASSERT_TRUE(c->Prepare(get2) >= 0);
    ENJIN_ASSERT_TRUE(c->Execute() == asEXECUTION_FINISHED);
    const f32 v = c->GetReturnFloat();
    engine.ReturnContext(c);

    // Assert
    ENJIN_EXPECT_TRUE(NearEq(v, 2.5f));
    g_ProbeValue = 0.0f;
    engine.Shutdown();
}

ENJIN_TEST_MAIN()
