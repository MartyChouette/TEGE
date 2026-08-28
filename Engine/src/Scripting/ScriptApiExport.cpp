// ScriptApiExport.cpp
//
// Generates an AngelScript "stub" file (declarations only) from a fully-bound
// AngelScript engine, using AngelScript's own reflection API. The stub declares
// every registered enum, funcdef, typedef, value/ref type, global property, and
// global function — with exact signatures pulled from the live engine — so an
// external editor can index the TEGE scripting API and offer completion.
//
// It is NOT meant to be compiled into a game: the declarations have no bodies,
// and the real implementations live in C++.

#include "Enjin/Scripting/ScriptBindings.h"

#include <angelscript.h>

#include <map>
#include <sstream>
#include <string>

namespace Enjin {
namespace Scripting {

namespace {
    // "" is the global namespace.
    std::string NsKey(const char* ns) { return ns ? std::string(ns) : std::string(); }
}

std::string GenerateApiStub(asIScriptEngine* engine) {
    if (!engine) return {};

    std::ostringstream out;
    out << "// ============================================================\n";
    out << "// TEGE (Enjin) AngelScript API - auto-generated IntelliSense stub\n";
    out << "//\n";
    out << "// Declarations only; the real implementations live in the engine.\n";
    out << "// Regenerate from the editor: Tools > Export Script API (IntelliSense).\n";
    out << "// Do NOT hand-edit and do NOT compile this into your game.\n";
    out << "// ============================================================\n\n";

    // Accumulate declarations per-namespace so the emitted file is well-formed
    // AngelScript (namespaced symbols wrapped in `namespace X { ... }`).
    std::map<std::string, std::string> byNs;
    auto append = [&](const char* ns, const std::string& text) { byNs[NsKey(ns)] += text; };

    // ---- Enums ----
    for (asUINT i = 0; i < engine->GetEnumCount(); ++i) {
        asITypeInfo* e = engine->GetEnumByIndex(i);
        if (!e || !e->GetName()) continue;
        std::string body = "enum ";
        body += e->GetName();
        body += " {\n";
        asUINT count = e->GetEnumValueCount();
        for (asUINT v = 0; v < count; ++v) {
            int val = 0;
            const char* vn = e->GetEnumValueByIndex(v, &val);
            if (!vn) continue;
            body += "    ";
            body += vn;
            body += " = ";
            body += std::to_string(val);
            body += (v + 1 < count) ? ",\n" : "\n";
        }
        body += "}\n\n";
        append(e->GetNamespace(), body);
    }

    // ---- Funcdefs (callback signatures) ----
    for (asUINT i = 0; i < engine->GetFuncdefCount(); ++i) {
        asITypeInfo* fd = engine->GetFuncdefByIndex(i);
        if (!fd) continue;
        asIScriptFunction* sig = fd->GetFuncdefSignature();
        if (!sig) continue;
        const char* decl = sig->GetDeclaration(false, false, true);
        if (!decl) continue;
        std::string body = "funcdef ";
        body += decl;
        body += ";\n";
        append(fd->GetNamespace(), body);
    }

    // ---- Typedefs ----
    for (asUINT i = 0; i < engine->GetTypedefCount(); ++i) {
        asITypeInfo* td = engine->GetTypedefByIndex(i);
        if (!td || !td->GetName()) continue;
        const char* underlying = engine->GetTypeDeclaration(td->GetTypedefTypeId(), false);
        if (!underlying) continue;
        std::string body = "typedef ";
        body += underlying;
        body += " ";
        body += td->GetName();
        body += ";\n";
        append(td->GetNamespace(), body);
    }

    // ---- Object / value types (Vec3, Entity, ...) ----
    for (asUINT i = 0; i < engine->GetObjectTypeCount(); ++i) {
        asITypeInfo* t = engine->GetObjectTypeByIndex(i);
        if (!t || !t->GetName()) continue;
        std::string body = "class ";
        body += t->GetName();
        body += " {\n";
        for (asUINT p = 0; p < t->GetPropertyCount(); ++p) {
            const char* pd = t->GetPropertyDeclaration(p, false);
            if (!pd) continue;
            body += "    ";
            body += pd;
            body += ";\n";
        }
        for (asUINT m = 0; m < t->GetMethodCount(); ++m) {
            asIScriptFunction* mf = t->GetMethodByIndex(m);
            if (!mf) continue;
            const char* md = mf->GetDeclaration(false, false, true);
            if (!md) continue;
            body += "    ";
            body += md;
            body += ";\n";
        }
        body += "}\n\n";
        append(t->GetNamespace(), body);
    }

    // ---- Global properties ----
    for (asUINT i = 0; i < engine->GetGlobalPropertyCount(); ++i) {
        const char* name = nullptr;
        const char* ns = nullptr;
        int typeId = 0;
        bool isConst = false;
        if (engine->GetGlobalPropertyByIndex(i, &name, &ns, &typeId, &isConst) < 0) continue;
        if (!name) continue;
        const char* typeDecl = engine->GetTypeDeclaration(typeId, false);
        std::string body;
        if (isConst) body += "const ";
        body += (typeDecl ? typeDecl : "int");
        body += " ";
        body += name;
        body += ";\n";
        append(ns, body);
    }

    // ---- Global functions ----
    for (asUINT i = 0; i < engine->GetGlobalFunctionCount(); ++i) {
        asIScriptFunction* f = engine->GetGlobalFunctionByIndex(i);
        if (!f) continue;
        const char* decl = f->GetDeclaration(false, false, true);
        if (!decl) continue;
        std::string body = decl;
        body += ";\n";
        append(f->GetNamespace(), body);
    }

    // Emit the global namespace first, then each named namespace.
    auto globalIt = byNs.find("");
    if (globalIt != byNs.end() && !globalIt->second.empty()) {
        out << globalIt->second << "\n";
    }
    for (const auto& kv : byNs) {
        if (kv.first.empty()) continue;
        out << "namespace " << kv.first << " {\n";
        out << kv.second;
        out << "}\n\n";
    }

    return out.str();
}

} // namespace Scripting
} // namespace Enjin
