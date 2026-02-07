#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Logging/Log.h"
#include <scriptbuilder/scriptbuilder.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptarray/scriptarray.h>
#include <scriptmath/scriptmath.h>
#include <scriptdictionary/scriptdictionary.h>
#include <scripthandle/scripthandle.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cassert>
#include <cstring>

namespace Enjin {
namespace Scripting {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

ScriptEngine::ScriptEngine() = default;

ScriptEngine::~ScriptEngine()
{
    Shutdown();
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

bool ScriptEngine::Init()
{
    if (m_Engine) {
        ENJIN_LOG_WARN(Script, "ScriptEngine::Init called but engine already initialized");
        return true;
    }

    // Create the AngelScript engine
    m_Engine = asCreateScriptEngine();
    if (!m_Engine) {
        ENJIN_LOG_ERROR(Script, "Failed to create AngelScript engine");
        m_LastError = "Failed to create AngelScript engine";
        return false;
    }

    // Set the message callback so we capture compilation errors/warnings
    i32 r = m_Engine->SetMessageCallback(asFUNCTION(MessageCallback), this, asCALL_CDECL);
    if (r < 0) {
        ENJIN_LOG_ERROR(Script, "Failed to set AngelScript message callback (err=%d)", r);
        m_LastError = "Failed to set message callback";
        m_Engine->ShutDownAndRelease();
        m_Engine = nullptr;
        return false;
    }

    // ------------------------------------------------------------------
    // Register standard add-on types
    // ------------------------------------------------------------------

    // std::string binding
    RegisterStdString(m_Engine);

    // Script array (array<T>) - register as default array type
    RegisterScriptArray(m_Engine, true);

    // Math helpers (sin, cos, sqrt, etc.)
    RegisterScriptMath(m_Engine);

    // Dictionary type
    RegisterScriptDictionary(m_Engine);

    // Generic object handle (ref)
    RegisterScriptHandle(m_Engine);

    ENJIN_LOG_INFO(Script, "AngelScript %s initialized", asGetLibraryVersion());

    // ------------------------------------------------------------------
    // Pre-create a small pool of execution contexts
    // ------------------------------------------------------------------
    static constexpr u32 INITIAL_POOL_SIZE = 4;
    m_ContextPool.reserve(INITIAL_POOL_SIZE);
    for (u32 i = 0; i < INITIAL_POOL_SIZE; ++i) {
        asIScriptContext* ctx = m_Engine->CreateContext();
        if (ctx) {
            m_ContextPool.push_back(ctx);
        }
    }

    ENJIN_LOG_INFO(Script, "Context pool created with %u contexts",
                   static_cast<u32>(m_ContextPool.size()));

    return true;
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void ScriptEngine::Shutdown()
{
    if (!m_Engine) {
        return;
    }

    ENJIN_LOG_INFO(Script, "Shutting down ScriptEngine...");

    // Release all pooled contexts
    for (asIScriptContext* ctx : m_ContextPool) {
        if (ctx) {
            ctx->Release();
        }
    }
    m_ContextPool.clear();

    // Clear module tracking (modules are owned by the engine, released below)
    m_Modules.clear();

    // Shut down the AngelScript engine (releases all modules and types)
    m_Engine->ShutDownAndRelease();
    m_Engine = nullptr;

    m_LastError.clear();
    m_PollCounter = 0;
    m_World = nullptr;

    ENJIN_LOG_INFO(Script, "ScriptEngine shut down");
}

// ---------------------------------------------------------------------------
// CompileScript (from file)
// ---------------------------------------------------------------------------

bool ScriptEngine::CompileScript(const std::string& path)
{
    if (!m_Engine) {
        m_LastError = "ScriptEngine not initialized";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return false;
    }

    // Derive the module name from the filename (strip directory and extension)
    std::filesystem::path fsPath(path);
    std::string moduleName = fsPath.stem().string();

    ENJIN_LOG_INFO(Script, "Compiling script '%s' as module '%s'",
                   path.c_str(), moduleName.c_str());

    // If a module with this name already exists, discard it first
    asIScriptModule* existingMod = m_Engine->GetModule(moduleName.c_str(), asGM_ONLY_IF_EXISTS);
    if (existingMod) {
        m_Engine->DiscardModule(moduleName.c_str());
        ENJIN_LOG_INFO(Script, "Discarded existing module '%s' for recompilation",
                       moduleName.c_str());
    }

    // Use CScriptBuilder for #include support and metadata parsing
    CScriptBuilder builder;
    builder.SetIncludeCallback(IncludeCallback, this);

    i32 r = builder.StartNewModule(m_Engine, moduleName.c_str());
    if (r < 0) {
        m_LastError = "Failed to start new module '" + moduleName + "'";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return false;
    }

    r = builder.AddSectionFromFile(path.c_str());
    if (r < 0) {
        m_LastError = "Failed to load script file: " + path;
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        m_Engine->DiscardModule(moduleName.c_str());
        return false;
    }

    r = builder.BuildModule();
    if (r < 0) {
        m_LastError = "Compilation failed for module '" + moduleName + "'";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        m_Engine->DiscardModule(moduleName.c_str());
        return false;
    }

    // Track the module and its file timestamp for hot-reload
    std::error_code ec;
    auto lastWrite = std::filesystem::last_write_time(fsPath, ec);

    ModuleInfo info;
    info.filePath = path;
    info.moduleName = moduleName;
    info.lastModified = ec ? std::filesystem::file_time_type{} : lastWrite;
    info.needsReload = false;

    m_Modules[moduleName] = std::move(info);

    ENJIN_LOG_INFO(Script, "Module '%s' compiled successfully", moduleName.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// CompileScriptFromMemory
// ---------------------------------------------------------------------------

bool ScriptEngine::CompileScriptFromMemory(const std::string& moduleName,
                                           const std::string& source)
{
    if (!m_Engine) {
        m_LastError = "ScriptEngine not initialized";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return false;
    }

    ENJIN_LOG_INFO(Script, "Compiling in-memory script as module '%s' (%u bytes)",
                   moduleName.c_str(), static_cast<u32>(source.size()));

    // Discard existing module if present
    asIScriptModule* existingMod = m_Engine->GetModule(moduleName.c_str(), asGM_ONLY_IF_EXISTS);
    if (existingMod) {
        m_Engine->DiscardModule(moduleName.c_str());
    }

    CScriptBuilder builder;
    builder.SetIncludeCallback(IncludeCallback, this);

    i32 r = builder.StartNewModule(m_Engine, moduleName.c_str());
    if (r < 0) {
        m_LastError = "Failed to start new module '" + moduleName + "'";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return false;
    }

    // AddSectionFromMemory takes: name, code, length (0 = use strlen)
    r = builder.AddSectionFromMemory(moduleName.c_str(), source.c_str(),
                                     static_cast<u32>(source.size()));
    if (r < 0) {
        m_LastError = "Failed to add in-memory source for module '" + moduleName + "'";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        m_Engine->DiscardModule(moduleName.c_str());
        return false;
    }

    r = builder.BuildModule();
    if (r < 0) {
        m_LastError = "Compilation failed for in-memory module '" + moduleName + "'";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        m_Engine->DiscardModule(moduleName.c_str());
        return false;
    }

    // In-memory modules have no file path, so we don't track for hot-reload
    // but we still record them in the module map for lookups
    ModuleInfo info;
    info.filePath.clear(); // No file
    info.moduleName = moduleName;
    info.lastModified = std::filesystem::file_time_type{};
    info.needsReload = false;

    m_Modules[moduleName] = std::move(info);

    ENJIN_LOG_INFO(Script, "In-memory module '%s' compiled successfully", moduleName.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// CreateInstance
// ---------------------------------------------------------------------------

asIScriptObject* ScriptEngine::CreateInstance(const std::string& moduleName,
                                              const std::string& className)
{
    if (!m_Engine) {
        m_LastError = "ScriptEngine not initialized";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return nullptr;
    }

    asIScriptModule* mod = m_Engine->GetModule(moduleName.c_str(), asGM_ONLY_IF_EXISTS);
    if (!mod) {
        m_LastError = "Module '" + moduleName + "' not found";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return nullptr;
    }

    // Find the type by class name within the module
    asITypeInfo* typeInfo = mod->GetTypeInfoByName(className.c_str());
    if (!typeInfo) {
        m_LastError = "Class '" + className + "' not found in module '" + moduleName + "'";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return nullptr;
    }

    // Verify the type is a class (not interface or enum)
    u32 flags = typeInfo->GetFlags();
    if (!(flags & asOBJ_SCRIPT_OBJECT)) {
        m_LastError = "'" + className + "' is not a script class";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return nullptr;
    }

    // Create the script object using the default factory
    // The factory is the first behavior with asBEHAVE_FACTORY
    asIScriptFunction* factory = typeInfo->GetFactoryByIndex(0);
    if (!factory) {
        m_LastError = "No default factory found for class '" + className + "'";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return nullptr;
    }

    asIScriptContext* ctx = AcquireContext();
    if (!ctx) {
        m_LastError = "Failed to acquire execution context";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return nullptr;
    }

    i32 r = ctx->Prepare(factory);
    if (r < 0) {
        m_LastError = "Failed to prepare factory for class '" + className + "'";
        ENJIN_LOG_ERROR(Script, "%s (err=%d)", m_LastError.c_str(), r);
        ReturnContext(ctx);
        return nullptr;
    }

    r = ctx->Execute();
    if (r != asEXECUTION_FINISHED) {
        if (r == asEXECUTION_EXCEPTION) {
            const char* exMsg = ctx->GetExceptionString();
            m_LastError = "Exception creating '" + className + "': " +
                          (exMsg ? exMsg : "unknown");
            ENJIN_LOG_ERROR(Script, "%s (line %d)", m_LastError.c_str(),
                            ctx->GetExceptionLineNumber());
        } else {
            m_LastError = "Factory execution failed for class '" + className + "'";
            ENJIN_LOG_ERROR(Script, "%s (result=%d)", m_LastError.c_str(), r);
        }
        ReturnContext(ctx);
        return nullptr;
    }

    // Retrieve the newly created object from the return value
    asIScriptObject* obj = *reinterpret_cast<asIScriptObject**>(ctx->GetAddressOfReturnValue());
    if (obj) {
        obj->AddRef(); // We take ownership; caller must release
    }

    ReturnContext(ctx);

    if (!obj) {
        m_LastError = "Factory returned null for class '" + className + "'";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return nullptr;
    }

    ENJIN_LOG_INFO(Script, "Created instance of '%s::%s'",
                   moduleName.c_str(), className.c_str());
    return obj;
}

// ---------------------------------------------------------------------------
// ReleaseInstance
// ---------------------------------------------------------------------------

void ScriptEngine::ReleaseInstance(asIScriptObject* obj)
{
    if (obj) {
        obj->Release();
    }
}

// ---------------------------------------------------------------------------
// Context Pool
// ---------------------------------------------------------------------------

// Execution timeout callback — aborts scripts that exceed the instruction limit.
// The instruction counter is stored in the context's user data pointer.
void ScriptEngine::LineCallback(asIScriptContext* ctx, void* param)
{
    u32* counter = reinterpret_cast<u32*>(param);
    (*counter)++;
    if (*counter > MAX_INSTRUCTIONS) {
        ENJIN_LOG_ERROR(Script, "Script exceeded instruction limit (%u) — aborting", MAX_INSTRUCTIONS);
        ctx->Abort();
    }
}

asIScriptContext* ScriptEngine::AcquireContext()
{
    if (!m_Engine) {
        return nullptr;
    }

    asIScriptContext* ctx = nullptr;
    if (!m_ContextPool.empty()) {
        ctx = m_ContextPool.back();
        m_ContextPool.pop_back();
    } else {
        // Pool exhausted -- create a new context on demand
        ctx = m_Engine->CreateContext();
        if (!ctx) {
            ENJIN_LOG_WARN(Script, "Failed to create new execution context");
            return nullptr;
        }
    }

    // Allocate per-context instruction counter and install timeout callback
    u32* counter = new u32(0);
    ctx->SetUserData(counter, 0);
    int cbResult = ctx->SetLineCallback(asFUNCTION(ScriptEngine::LineCallback), counter, asCALL_CDECL);
    if (cbResult < 0) {
        ENJIN_LOG_WARN(Script, "Failed to set line callback on context (err=%d)", cbResult);
        delete counter;
        ctx->SetUserData(nullptr, 0);
        m_ContextPool.push_back(ctx);
        return nullptr;
    }

    return ctx;
}

void ScriptEngine::ReturnContext(asIScriptContext* ctx)
{
    if (!ctx) {
        return;
    }

    // Free the per-context instruction counter
    u32* counter = reinterpret_cast<u32*>(ctx->GetUserData(0));
    delete counter;
    ctx->SetUserData(nullptr, 0);
    ctx->ClearLineCallback();

    // Unprepare the context before returning to pool so it releases
    // references to functions/objects from the previous call
    ctx->Unprepare();
    m_ContextPool.push_back(ctx);
}

// ---------------------------------------------------------------------------
// FindMethod
// ---------------------------------------------------------------------------

asIScriptFunction* ScriptEngine::FindMethod(asIScriptObject* obj, const std::string& decl)
{
    if (!obj) {
        return nullptr;
    }

    asITypeInfo* type = obj->GetObjectType();
    if (!type) {
        return nullptr;
    }

    asIScriptFunction* func = type->GetMethodByDecl(decl.c_str());
    return func; // May be nullptr if method not declared
}

// ---------------------------------------------------------------------------
// ExecuteMethod
// ---------------------------------------------------------------------------

bool ScriptEngine::ExecuteMethod(asIScriptContext* ctx,
                                 asIScriptObject* obj,
                                 asIScriptFunction* func)
{
    if (!ctx || !obj || !func) {
        m_LastError = "ExecuteMethod: null context, object, or function";
        return false;
    }

    i32 r = ctx->Prepare(func);
    if (r < 0) {
        m_LastError = "ExecuteMethod: Prepare failed (err=" + std::to_string(r) + ")";
        ENJIN_LOG_ERROR(Script, "%s for %s", m_LastError.c_str(),
                        func->GetDeclaration());
        return false;
    }

    r = ctx->SetObject(obj);
    if (r < 0) {
        m_LastError = "ExecuteMethod: SetObject failed (err=" + std::to_string(r) + ")";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return false;
    }

    r = ctx->Execute();

    if (r == asEXECUTION_FINISHED) {
        return true;
    }

    if (r == asEXECUTION_EXCEPTION) {
        const char* exMsg = ctx->GetExceptionString();
        i32 exLine = ctx->GetExceptionLineNumber();
        const char* exFunc = nullptr;
        asIScriptFunction* exFuncObj = ctx->GetExceptionFunction();
        if (exFuncObj) {
            exFunc = exFuncObj->GetDeclaration();
        }

        std::ostringstream oss;
        oss << "Script exception in " << (exFunc ? exFunc : "unknown") << " line " << exLine;
        if (exMsg) {
            oss << ": " << exMsg;
        }
        m_LastError = oss.str();
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return false;
    }

    if (r == asEXECUTION_SUSPENDED) {
        // Coroutine-style suspension -- not an error, but the caller should
        // know execution did not complete
        m_LastError = "Execution suspended (coroutine)";
        return false;
    }

    m_LastError = "ExecuteMethod: unexpected result " + std::to_string(r);
    ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
    return false;
}

// ---------------------------------------------------------------------------
// Hot Reload -- directory & polling
// ---------------------------------------------------------------------------

void ScriptEngine::SetScriptDirectory(const std::string& dir)
{
    m_ScriptDirectory = dir;
    ENJIN_LOG_INFO(Script, "Script directory set to '%s'", dir.c_str());
}

void ScriptEngine::PollFileChanges()
{
    if (m_Modules.empty() || m_ScriptDirectory.empty()) {
        return;
    }

    ++m_PollCounter;
    if (m_PollCounter < POLL_INTERVAL) {
        return;
    }
    m_PollCounter = 0;

    std::error_code ec;

    for (auto& [name, info] : m_Modules) {
        // Skip in-memory modules (no file path to watch)
        if (info.filePath.empty()) {
            continue;
        }

        auto currentTime = std::filesystem::last_write_time(info.filePath, ec);
        if (ec) {
            // File may have been deleted; skip silently
            continue;
        }

        if (currentTime != info.lastModified) {
            info.needsReload = true;
            ENJIN_LOG_INFO(Script, "Change detected in '%s' -- scheduling reload",
                           info.filePath.c_str());
        }
    }
}

// ---------------------------------------------------------------------------
// Hot Reload -- recompilation
// ---------------------------------------------------------------------------

bool ScriptEngine::ProcessHotReload()
{
    bool anyReloaded = false;

    // Collect module names that need reload (iterate copy to allow safe mutation)
    std::vector<std::string> toReload;
    for (auto& [name, info] : m_Modules) {
        if (info.needsReload) {
            toReload.push_back(name);
        }
    }

    for (const std::string& modName : toReload) {
        auto it = m_Modules.find(modName);
        if (it == m_Modules.end()) {
            continue;
        }

        ModuleInfo& info = it->second;
        info.needsReload = false;

        ENJIN_LOG_INFO(Script, "Hot-reloading module '%s' from '%s'",
                       modName.c_str(), info.filePath.c_str());

        // Discard the old module
        m_Engine->DiscardModule(modName.c_str());

        // Recompile from file
        CScriptBuilder builder;
        builder.SetIncludeCallback(IncludeCallback, this);

        i32 r = builder.StartNewModule(m_Engine, modName.c_str());
        if (r < 0) {
            ENJIN_LOG_ERROR(Script, "Hot-reload: failed to start module '%s'", modName.c_str());
            m_LastError = "Hot-reload: failed to start module '" + modName + "'";
            continue;
        }

        r = builder.AddSectionFromFile(info.filePath.c_str());
        if (r < 0) {
            ENJIN_LOG_ERROR(Script, "Hot-reload: failed to load file '%s'",
                            info.filePath.c_str());
            m_LastError = "Hot-reload: failed to load file '" + info.filePath + "'";
            m_Engine->DiscardModule(modName.c_str());
            continue;
        }

        r = builder.BuildModule();
        if (r < 0) {
            ENJIN_LOG_ERROR(Script, "Hot-reload: compilation failed for '%s'",
                            modName.c_str());
            m_LastError = "Hot-reload: compilation failed for '" + modName + "'";
            m_Engine->DiscardModule(modName.c_str());
            continue;
        }

        // Update timestamp
        std::error_code ec;
        info.lastModified = std::filesystem::last_write_time(info.filePath, ec);

        ENJIN_LOG_INFO(Script, "Hot-reload: module '%s' recompiled successfully",
                       modName.c_str());
        anyReloaded = true;
    }

    return anyReloaded;
}

// ---------------------------------------------------------------------------
// GetBehaviorClasses
// ---------------------------------------------------------------------------

std::vector<std::string> ScriptEngine::GetBehaviorClasses() const
{
    std::vector<std::string> result;

    if (!m_Engine) {
        return result;
    }

    for (const auto& [modName, info] : m_Modules) {
        asIScriptModule* mod = m_Engine->GetModule(modName.c_str(), asGM_ONLY_IF_EXISTS);
        if (!mod) {
            continue;
        }

        u32 typeCount = static_cast<u32>(mod->GetObjectTypeCount());
        for (u32 i = 0; i < typeCount; ++i) {
            asITypeInfo* type = mod->GetObjectTypeByIndex(i);
            if (!type) {
                continue;
            }

            // Check if this type inherits from TegeBehavior (directly or indirectly)
            // Walk the base class chain
            bool isBehavior = false;
            asITypeInfo* baseType = type->GetBaseType();
            while (baseType) {
                const char* baseName = baseType->GetName();
                if (baseName && std::strcmp(baseName, "TegeBehavior") == 0) {
                    isBehavior = true;
                    break;
                }
                baseType = baseType->GetBaseType();
            }

            // Also check if the type itself is named TegeBehavior (the base class)
            // We typically don't want to instantiate the base itself, so skip it
            const char* typeName = type->GetName();
            if (typeName && std::strcmp(typeName, "TegeBehavior") == 0) {
                continue;
            }

            if (isBehavior) {
                result.push_back(typeName ? typeName : "");
            }
        }
    }

    // Sort alphabetically for stable ordering
    std::sort(result.begin(), result.end());
    return result;
}

// ---------------------------------------------------------------------------
// GetScriptFiles
// ---------------------------------------------------------------------------

std::vector<std::string> ScriptEngine::GetScriptFiles() const
{
    std::vector<std::string> result;

    if (m_ScriptDirectory.empty()) {
        return result;
    }

    std::error_code ec;
    if (!std::filesystem::exists(m_ScriptDirectory, ec) ||
        !std::filesystem::is_directory(m_ScriptDirectory, ec)) {
        return result;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             m_ScriptDirectory, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        auto ext = entry.path().extension().string();
        // Accept .as files (AngelScript source)
        if (ext == ".as") {
            result.push_back(entry.path().string());
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

// ---------------------------------------------------------------------------
// ApplyProperties
// ---------------------------------------------------------------------------

void ScriptEngine::ApplyProperties(asIScriptObject* obj,
                                   const std::vector<ECS::ScriptProperty>& properties)
{
    if (!obj) {
        return;
    }

    asITypeInfo* type = obj->GetObjectType();
    if (!type) {
        return;
    }

    for (const auto& prop : properties) {
        // Determine which value to write: overridden instance value or default
        const ECS::ScriptPropertyValue& val = prop.isOverridden
                                                  ? prop.instanceValue
                                                  : prop.defaultValue;

        // Find the member property by name
        u32 propCount = type->GetPropertyCount();
        bool found = false;

        for (u32 i = 0; i < propCount; ++i) {
            const char* propName = nullptr;
            i32 propTypeId = 0;
            bool isPrivate = false;
            bool isProtected = false;
            i32 offset = 0;
            bool isReference = false;

            type->GetProperty(i, &propName, &propTypeId, &isPrivate, &isProtected,
                              &offset, &isReference);

            if (!propName || prop.name != propName) {
                continue;
            }

            // Get pointer to the property in the object's memory
            void* addr = obj->GetAddressOfProperty(i);
            if (!addr) {
                ENJIN_LOG_WARN(Script, "ApplyProperties: null address for '%s'",
                               prop.name.c_str());
                break;
            }

            found = true;

            switch (prop.type) {
            case ECS::ScriptPropertyType::Int:
                if (propTypeId == asTYPEID_INT32) {
                    *reinterpret_cast<i32*>(addr) = val.intVal;
                } else if (propTypeId == asTYPEID_INT16) {
                    *reinterpret_cast<i16*>(addr) = static_cast<i16>(val.intVal);
                } else if (propTypeId == asTYPEID_INT8) {
                    *reinterpret_cast<i8*>(addr) = static_cast<i8>(val.intVal);
                } else if (propTypeId == asTYPEID_INT64) {
                    *reinterpret_cast<i64*>(addr) = static_cast<i64>(val.intVal);
                } else if (propTypeId == asTYPEID_UINT32) {
                    *reinterpret_cast<u32*>(addr) = static_cast<u32>(val.intVal);
                } else if (propTypeId == asTYPEID_UINT64) {
                    *reinterpret_cast<u64*>(addr) = static_cast<u64>(val.intVal);
                }
                break;

            case ECS::ScriptPropertyType::Float:
                if (propTypeId == asTYPEID_FLOAT) {
                    *reinterpret_cast<f32*>(addr) = val.floatVal;
                } else if (propTypeId == asTYPEID_DOUBLE) {
                    *reinterpret_cast<f64*>(addr) = static_cast<f64>(val.floatVal);
                }
                break;

            case ECS::ScriptPropertyType::Bool:
                if (propTypeId == asTYPEID_BOOL) {
                    *reinterpret_cast<bool*>(addr) = val.boolVal;
                }
                break;

            case ECS::ScriptPropertyType::String: {
                // AngelScript std::string binding stores std::string directly
                // Check that the type is the registered string type
                asITypeInfo* strType = m_Engine->GetTypeInfoByName("string");
                if (strType && propTypeId == strType->GetTypeId()) {
                    *reinterpret_cast<std::string*>(addr) = val.stringVal;
                }
                break;
            }

            case ECS::ScriptPropertyType::Vector2: {
                // Vector2 registered as value type with 2 floats
                asITypeInfo* v2Type = m_Engine->GetTypeInfoByName("Vector2");
                if (v2Type && propTypeId == v2Type->GetTypeId()) {
                    f32* dest = reinterpret_cast<f32*>(addr);
                    dest[0] = val.vec2Val.x;
                    dest[1] = val.vec2Val.y;
                }
                break;
            }

            case ECS::ScriptPropertyType::Vector3: {
                asITypeInfo* v3Type = m_Engine->GetTypeInfoByName("Vector3");
                if (v3Type && propTypeId == v3Type->GetTypeId()) {
                    f32* dest = reinterpret_cast<f32*>(addr);
                    dest[0] = val.vec3Val.x;
                    dest[1] = val.vec3Val.y;
                    dest[2] = val.vec3Val.z;
                }
                break;
            }

            case ECS::ScriptPropertyType::Vector4: {
                asITypeInfo* v4Type = m_Engine->GetTypeInfoByName("Vector4");
                if (v4Type && propTypeId == v4Type->GetTypeId()) {
                    f32* dest = reinterpret_cast<f32*>(addr);
                    dest[0] = val.vec4Val.x;
                    dest[1] = val.vec4Val.y;
                    dest[2] = val.vec4Val.z;
                    dest[3] = val.vec4Val.w;
                }
                break;
            }

            case ECS::ScriptPropertyType::Entity:
                // Entity IDs stored as uint64 in scripts
                if (propTypeId == asTYPEID_UINT64) {
                    *reinterpret_cast<u64*>(addr) = val.entityVal;
                }
                break;

            case ECS::ScriptPropertyType::Enum:
                // Enums are stored as i32 in AngelScript
                if (propTypeId == asTYPEID_INT32 ||
                    (propTypeId > asTYPEID_DOUBLE)) {
                    // Script enums have custom type IDs > asTYPEID_DOUBLE
                    *reinterpret_cast<i32*>(addr) = val.intVal;
                }
                break;
            }

            break; // Found the property, stop inner loop
        }

        if (!found) {
            ENJIN_LOG_WARN(Script, "ApplyProperties: property '%s' not found on object type '%s'",
                           prop.name.c_str(), type->GetName());
        }
    }
}

// ---------------------------------------------------------------------------
// ReadProperties
// ---------------------------------------------------------------------------

void ScriptEngine::ReadProperties(asIScriptObject* obj,
                                  std::vector<ECS::ScriptProperty>& properties)
{
    if (!obj) {
        return;
    }

    asITypeInfo* type = obj->GetObjectType();
    if (!type) {
        return;
    }

    for (auto& prop : properties) {
        u32 propCount = type->GetPropertyCount();

        for (u32 i = 0; i < propCount; ++i) {
            const char* propName = nullptr;
            i32 propTypeId = 0;
            bool isPrivate = false;
            bool isProtected = false;
            i32 offset = 0;
            bool isReference = false;

            type->GetProperty(i, &propName, &propTypeId, &isPrivate, &isProtected,
                              &offset, &isReference);

            if (!propName || prop.name != propName) {
                continue;
            }

            void* addr = obj->GetAddressOfProperty(i);
            if (!addr) {
                break;
            }

            // Mark as overridden since we're reading a live value
            prop.isOverridden = true;

            switch (prop.type) {
            case ECS::ScriptPropertyType::Int:
                if (propTypeId == asTYPEID_INT32) {
                    prop.instanceValue.intVal = *reinterpret_cast<i32*>(addr);
                } else if (propTypeId == asTYPEID_INT16) {
                    prop.instanceValue.intVal = static_cast<i32>(*reinterpret_cast<i16*>(addr));
                } else if (propTypeId == asTYPEID_INT8) {
                    prop.instanceValue.intVal = static_cast<i32>(*reinterpret_cast<i8*>(addr));
                } else if (propTypeId == asTYPEID_INT64) {
                    prop.instanceValue.intVal = static_cast<i32>(*reinterpret_cast<i64*>(addr));
                } else if (propTypeId == asTYPEID_UINT32) {
                    prop.instanceValue.intVal = static_cast<i32>(*reinterpret_cast<u32*>(addr));
                } else if (propTypeId == asTYPEID_UINT64) {
                    prop.instanceValue.intVal = static_cast<i32>(*reinterpret_cast<u64*>(addr));
                }
                break;

            case ECS::ScriptPropertyType::Float:
                if (propTypeId == asTYPEID_FLOAT) {
                    prop.instanceValue.floatVal = *reinterpret_cast<f32*>(addr);
                } else if (propTypeId == asTYPEID_DOUBLE) {
                    prop.instanceValue.floatVal = static_cast<f32>(*reinterpret_cast<f64*>(addr));
                }
                break;

            case ECS::ScriptPropertyType::Bool:
                if (propTypeId == asTYPEID_BOOL) {
                    prop.instanceValue.boolVal = *reinterpret_cast<bool*>(addr);
                }
                break;

            case ECS::ScriptPropertyType::String: {
                asITypeInfo* strType = m_Engine->GetTypeInfoByName("string");
                if (strType && propTypeId == strType->GetTypeId()) {
                    prop.instanceValue.stringVal = *reinterpret_cast<std::string*>(addr);
                }
                break;
            }

            case ECS::ScriptPropertyType::Vector2: {
                asITypeInfo* v2Type = m_Engine->GetTypeInfoByName("Vector2");
                if (v2Type && propTypeId == v2Type->GetTypeId()) {
                    const f32* src = reinterpret_cast<const f32*>(addr);
                    prop.instanceValue.vec2Val.x = src[0];
                    prop.instanceValue.vec2Val.y = src[1];
                }
                break;
            }

            case ECS::ScriptPropertyType::Vector3: {
                asITypeInfo* v3Type = m_Engine->GetTypeInfoByName("Vector3");
                if (v3Type && propTypeId == v3Type->GetTypeId()) {
                    const f32* src = reinterpret_cast<const f32*>(addr);
                    prop.instanceValue.vec3Val.x = src[0];
                    prop.instanceValue.vec3Val.y = src[1];
                    prop.instanceValue.vec3Val.z = src[2];
                }
                break;
            }

            case ECS::ScriptPropertyType::Vector4: {
                asITypeInfo* v4Type = m_Engine->GetTypeInfoByName("Vector4");
                if (v4Type && propTypeId == v4Type->GetTypeId()) {
                    const f32* src = reinterpret_cast<const f32*>(addr);
                    prop.instanceValue.vec4Val.x = src[0];
                    prop.instanceValue.vec4Val.y = src[1];
                    prop.instanceValue.vec4Val.z = src[2];
                    prop.instanceValue.vec4Val.w = src[3];
                }
                break;
            }

            case ECS::ScriptPropertyType::Entity:
                if (propTypeId == asTYPEID_UINT64) {
                    prop.instanceValue.entityVal = *reinterpret_cast<u64*>(addr);
                }
                break;

            case ECS::ScriptPropertyType::Enum:
                if (propTypeId == asTYPEID_INT32 ||
                    (propTypeId > asTYPEID_DOUBLE)) {
                    prop.instanceValue.intVal = *reinterpret_cast<i32*>(addr);
                }
                break;
            }

            break; // Found the property, stop inner loop
        }
    }
}

// ---------------------------------------------------------------------------
// MessageCallback (static)
// ---------------------------------------------------------------------------

void ScriptEngine::MessageCallback(const asSMessageInfo* msg, void* param)
{
    if (!msg) {
        return;
    }

    ScriptEngine* self = reinterpret_cast<ScriptEngine*>(param);

    // Build a formatted message: "file (line, col): message"
    std::ostringstream oss;
    if (msg->section && msg->section[0] != '\0') {
        oss << msg->section;
    } else {
        oss << "<unknown>";
    }
    oss << " (" << msg->row << ", " << msg->col << "): " << msg->message;

    std::string formatted = oss.str();

    switch (msg->type) {
    case asMSGTYPE_ERROR:
        ENJIN_LOG_ERROR(Script, "%s", formatted.c_str());
        if (self) {
            self->m_LastError = formatted;
        }
        break;

    case asMSGTYPE_WARNING:
        ENJIN_LOG_WARN(Script, "%s", formatted.c_str());
        break;

    case asMSGTYPE_INFORMATION:
        ENJIN_LOG_INFO(Script, "%s", formatted.c_str());
        break;

    default:
        ENJIN_LOG_INFO(Script, "%s", formatted.c_str());
        break;
    }
}

// ---------------------------------------------------------------------------
// IncludeCallback (static)
// ---------------------------------------------------------------------------

int ScriptEngine::IncludeCallback(const char* include,
                                  const char* from,
                                  CScriptBuilder* builder,
                                  void* userParam)
{
    ScriptEngine* self = reinterpret_cast<ScriptEngine*>(userParam);
    if (!self || !include || !builder) {
        return -1;
    }

    // Strategy 1: resolve relative to the file doing the including
    if (from && from[0] != '\0') {
        std::filesystem::path fromPath(from);
        std::filesystem::path resolved = fromPath.parent_path() / include;
        std::error_code ec;
        if (std::filesystem::exists(resolved, ec)) {
            std::string resolvedStr = resolved.lexically_normal().string();
            i32 r = builder->AddSectionFromFile(resolvedStr.c_str());
            if (r >= 0) {
                ENJIN_LOG_INFO(Script, "Included '%s' (relative to source)", resolvedStr.c_str());
                return 0;
            }
        }
    }

    // Strategy 2: resolve relative to the main script directory
    if (!self->m_ScriptDirectory.empty()) {
        std::filesystem::path resolved = std::filesystem::path(self->m_ScriptDirectory) / include;
        std::error_code ec;
        if (std::filesystem::exists(resolved, ec)) {
            std::string resolvedStr = resolved.lexically_normal().string();
            i32 r = builder->AddSectionFromFile(resolvedStr.c_str());
            if (r >= 0) {
                ENJIN_LOG_INFO(Script, "Included '%s' (script directory)", resolvedStr.c_str());
                return 0;
            }
        }
    }

    // Strategy 3: resolve relative to the enjin_api/ subdirectory inside the
    // script directory (engine-provided API headers for scripts)
    if (!self->m_ScriptDirectory.empty()) {
        std::filesystem::path apiPath =
            std::filesystem::path(self->m_ScriptDirectory) / "enjin_api" / include;
        std::error_code ec;
        if (std::filesystem::exists(apiPath, ec)) {
            std::string apiPathStr = apiPath.lexically_normal().string();
            i32 r = builder->AddSectionFromFile(apiPathStr.c_str());
            if (r >= 0) {
                ENJIN_LOG_INFO(Script, "Included '%s' (enjin_api directory)", apiPathStr.c_str());
                return 0;
            }
        }
    }

    ENJIN_LOG_ERROR(Script, "Failed to resolve #include \"%s\" (from '%s')",
                    include, from ? from : "<null>");
    return -1;
}

} // namespace Scripting
} // namespace Enjin
