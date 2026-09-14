// Script can ask the registry what it holds.
//
// FR-0001 (Marty, 2026-09-13). There were nine DataAsset_* bindings and not one
// of them listed anything. `DataAsset_Load(name)` answers "does this one
// exist", so script could only ever confirm a name it already knew. A project
// that wanted its cast had to hand-maintain a manifest restating what the
// filesystem already knew, and forgetting to add a line to it was SILENT: the
// record loads, the schema validates, nothing warns, and the character simply
// never appears.
//
// The registry has answered this in C++ the whole time -- GetAssetsBySchema,
// GetAllAssets -- and the editor calls it in four places. Only the binding was
// missing, which is the entire asymmetry the request was about.
//
// The count-plus-indexed-getter shape is the physics API's, not a new idiom:
// no binding in this engine returns an array<T>@.

#include "EnjinTest.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"

#include <angelscript.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace Enjin;

namespace {

// Build a registry in memory rather than on disk: this is about enumeration,
// not about loading, and a file fixture would make the test depend on the
// loader it is not trying to check.
void SeedRegistry() {
    auto& reg = Assets::DataAssetRegistry::Get();
    reg.Clear();

    Assets::DataAssetSchema character;
    character.name = "Character";
    reg.RegisterSchema(character);

    Assets::DataAssetSchema activity;
    activity.name = "Activity";
    reg.RegisterSchema(activity);

    for (const char* n : {"cast/yuki", "cast/rin", "cast/aoi"}) {
        Assets::DataAsset a;
        a.name = n;
        a.schemaName = "Character";
        reg.CreateAsset(a);
    }
    {
        Assets::DataAsset a;
        a.name = "activity/drift";
        a.schemaName = "Activity";
        reg.CreateAsset(a);
    }
}

// Run one AngelScript expression and return what it produced as a string.
struct Runner {
    Scripting::ScriptEngine engine;

    Runner() {
        engine.Initialize();
        Scripting::RegisterAllBindings(engine.GetASEngine());
        engine.InvalidateContextPool();
    }
    ~Runner() { engine.Shutdown(); }

    std::string Eval(const std::string& body) {
        const std::string src =
            "string Probe() {\n"
            "    string acc = \"\";\n" + body +
            "    return acc;\n"
            "}\n";
        if (!engine.CompileScriptFromMemory("probe", src)) return "<compile failed>";

        asIScriptModule* mod = engine.GetASEngine()->GetModule("probe");
        if (!mod) return "<no module>";
        asIScriptFunction* fn = mod->GetFunctionByDecl("string Probe()");
        if (!fn) return "<no function>";

        asIScriptContext* ctx = engine.GetASEngine()->CreateContext();
        ctx->Prepare(fn);
        const int r = ctx->Execute();
        std::string acc = "<not finished>";
        if (r == asEXECUTION_FINISHED) {
            acc = *static_cast<std::string*>(ctx->GetReturnObject());
        }
        ctx->Release();
        return acc;
    }
};

}  // namespace

ENJIN_TEST(DataAssetEnumeration, ScriptCanListEveryAssetOfASchema) {
    SeedRegistry();
    Runner r;

    const std::string got = r.Eval(
        "    int n = DataAsset_ListBySchema(\"Character\");\n"
        "    acc += \"\" + n + \":\";\n"
        "    for (int i = 0; i < n; i++) { acc += DataAsset_GetListResult(i) + \",\"; }\n");

    std::printf("    %s\n", got.c_str());

    // Three characters, and the activity must not be among them.
    ENJIN_EXPECT_TRUE(got.rfind("3:", 0) == 0);
    ENJIN_EXPECT_TRUE(got.find("cast/yuki") != std::string::npos);
    ENJIN_EXPECT_TRUE(got.find("cast/rin") != std::string::npos);
    ENJIN_EXPECT_TRUE(got.find("cast/aoi") != std::string::npos);
    ENJIN_EXPECT_TRUE(got.find("activity/drift") == std::string::npos);
}

ENJIN_TEST(DataAssetEnumeration, ListAllSeesEverySchema) {
    SeedRegistry();
    Runner r;
    const std::string got = r.Eval("    acc += \"\" + DataAsset_ListAll();\n");
    std::printf("    all = %s\n", got.c_str());
    ENJIN_EXPECT_EQ(got, std::string("4"));
}

// So a loader can branch on what it found rather than being told in advance.
ENJIN_TEST(DataAssetEnumeration, ScriptCanAskWhatSchemaAnAssetUses) {
    SeedRegistry();
    Runner r;
    const std::string got = r.Eval(
        "    acc += DataAsset_GetSchemaName(\"cast/rin\") + \"|\";\n"
        "    acc += DataAsset_GetSchemaName(\"nope\") + \"|\";\n");
    std::printf("    %s\n", got.c_str());
    ENJIN_EXPECT_EQ(got, std::string("Character||"));
}

// An acc-of-range index returns empty rather than reading past the end. The
// count-plus-getter shape puts the bound in the caller's hands, so the getter
// has to be safe on its own.
ENJIN_TEST(DataAssetEnumeration, AnOutOfRangeIndexIsEmptyNotACrash) {
    SeedRegistry();
    Runner r;
    const std::string got = r.Eval(
        "    DataAsset_ListBySchema(\"Character\");\n"
        "    acc += \"[\" + DataAsset_GetListResult(-1) + \"]\";\n"
        "    acc += \"[\" + DataAsset_GetListResult(99) + \"]\";\n");
    std::printf("    %s\n", got.c_str());
    ENJIN_EXPECT_EQ(got, std::string("[][]"));
}

ENJIN_TEST_MAIN()
