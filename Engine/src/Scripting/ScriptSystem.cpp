#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptPropertyParser.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Scripting {

ScriptSystem::ScriptSystem() = default;
ScriptSystem::~ScriptSystem() = default;

void ScriptSystem::CacheMethodIds(ECS::ScriptAttachment& script) {
    if (!script.instance) return;

    asIScriptObject* obj = static_cast<asIScriptObject*>(script.instance);
    asITypeInfo* type = obj->GetObjectType();
    if (!type) return;

    auto findMethod = [type](const char* decl) -> int {
        asIScriptFunction* func = type->GetMethodByDecl(decl);
        return func ? func->GetId() : -1;
    };

    script.methodOnCreate         = findMethod("void OnCreate()");
    script.methodOnStart          = findMethod("void OnStart()");
    script.methodOnUpdate         = findMethod("void OnUpdate(float)");
    script.methodOnFixedUpdate    = findMethod("void OnFixedUpdate(float)");
    script.methodOnLateUpdate     = findMethod("void OnLateUpdate(float)");
    script.methodOnDestroy        = findMethod("void OnDestroy()");
    script.methodOnEnable         = findMethod("void OnEnable()");
    script.methodOnDisable        = findMethod("void OnDisable()");
    script.methodOnCollisionEnter = findMethod("void OnCollisionEnter(uint64)");
    script.methodOnCollisionStay  = findMethod("void OnCollisionStay(uint64)");
    script.methodOnCollisionExit  = findMethod("void OnCollisionExit(uint64)");
    script.methodOnTriggerEnter   = findMethod("void OnTriggerEnter(uint64)");
    script.methodOnTriggerExit    = findMethod("void OnTriggerExit(uint64)");
}

void ScriptSystem::HandleScriptError(ECS::ScriptAttachment& script, const char* methodName) {
    script.hasError = true;
    if (m_ScriptEngine) {
        script.lastError = m_ScriptEngine->GetLastError();
    }
    if (script.lastError.empty()) {
        script.lastError = "Unknown error in " + std::string(methodName);
    }
    ENJIN_LOG_ERROR(Script, "Script error in %s::%s: %s",
        script.className.c_str(), methodName, script.lastError.c_str());
}

bool ScriptSystem::CallLifecycleMethod(ECS::ScriptAttachment& script, int methodId, const char* methodName) {
    if (methodId < 0 || !script.instance || script.hasError || !script.enabled) return true;
    if (!m_ScriptEngine) return false;

    asIScriptObject* obj = static_cast<asIScriptObject*>(script.instance);
    asIScriptContext* ctx = m_ScriptEngine->AcquireContext();
    if (!ctx) return false;

    asIScriptEngine* engine = m_ScriptEngine->GetASEngine();
    asIScriptFunction* func = engine->GetFunctionById(methodId);
    if (!func) {
        m_ScriptEngine->ReturnContext(ctx);
        return true; // Method doesn't exist, not an error
    }

    ctx->Prepare(func);
    ctx->SetObject(obj);

    int r = ctx->Execute();
    bool success = true;

    if (r == asEXECUTION_EXCEPTION) {
        script.lastError = ctx->GetExceptionString();
        HandleScriptError(script, methodName);
        success = false;
    } else if (r == asCONTEXT_NOT_PREPARED) {
        ENJIN_LOG_ERROR(Script, "Context not prepared for %s::%s", script.className.c_str(), methodName);
        success = false;
    }

    m_ScriptEngine->ReturnContext(ctx);
    return success;
}

bool ScriptSystem::CallLifecycleMethodFloat(ECS::ScriptAttachment& script, int methodId, const char* methodName, f32 value) {
    if (methodId < 0 || !script.instance || script.hasError || !script.enabled) return true;
    if (!m_ScriptEngine) return false;

    asIScriptObject* obj = static_cast<asIScriptObject*>(script.instance);
    asIScriptContext* ctx = m_ScriptEngine->AcquireContext();
    if (!ctx) return false;

    asIScriptEngine* engine = m_ScriptEngine->GetASEngine();
    asIScriptFunction* func = engine->GetFunctionById(methodId);
    if (!func) {
        m_ScriptEngine->ReturnContext(ctx);
        return true;
    }

    ctx->Prepare(func);
    ctx->SetObject(obj);
    ctx->SetArgFloat(0, value);

    int r = ctx->Execute();
    bool success = true;

    if (r == asEXECUTION_EXCEPTION) {
        script.lastError = ctx->GetExceptionString();
        HandleScriptError(script, methodName);
        success = false;
    }

    m_ScriptEngine->ReturnContext(ctx);
    return success;
}

bool ScriptSystem::CallCollisionMethod(ECS::ScriptAttachment& script, int methodId, const char* methodName, ECS::Entity other) {
    if (methodId < 0 || !script.instance || script.hasError || !script.enabled) return true;
    if (!m_ScriptEngine) return false;

    asIScriptObject* obj = static_cast<asIScriptObject*>(script.instance);
    asIScriptContext* ctx = m_ScriptEngine->AcquireContext();
    if (!ctx) return false;

    asIScriptEngine* engine = m_ScriptEngine->GetASEngine();
    asIScriptFunction* func = engine->GetFunctionById(methodId);
    if (!func) {
        m_ScriptEngine->ReturnContext(ctx);
        return true;
    }

    ctx->Prepare(func);
    ctx->SetObject(obj);
    ctx->SetArgQWord(0, static_cast<asQWORD>(other));

    int r = ctx->Execute();
    bool success = true;

    if (r == asEXECUTION_EXCEPTION) {
        script.lastError = ctx->GetExceptionString();
        HandleScriptError(script, methodName);
        success = false;
    }

    m_ScriptEngine->ReturnContext(ctx);
    return success;
}

void ScriptSystem::InitScript(ECS::Entity entity, ECS::ScriptAttachment& script) {
    if (!m_ScriptEngine || script.initialized) return;

    // Resolve relative script paths against the script root (project dir).
    // The process CWD is the exe directory, so scene-stored paths like
    // "scripts/Foo.as" never resolve without this.
    std::string resolvedPath = script.scriptPath;
    if (!m_ScriptRoot.empty() && !std::filesystem::path(script.scriptPath).is_absolute()) {
        resolvedPath = (std::filesystem::path(m_ScriptRoot) / script.scriptPath).string();
    }

    // Derive module name the same way ScriptEngine::CompileScript does
    // (SC-3: parentDir_stem). A bare filename stem never matches for scripts
    // in subdirectories and CreateInstance fails with "module not found".
    std::filesystem::path fsPath(resolvedPath);
    std::string stem = fsPath.stem().string();
    std::string parentDir = fsPath.parent_path().filename().string();
    std::string moduleName = parentDir.empty() ? stem : (parentDir + "_" + stem);

    // Compile if not already compiled
    m_ScriptEngine->CompileScript(resolvedPath);

    // Parse [Property] annotations from source for editor metadata
    {
        std::ifstream file(resolvedPath);
        if (file.is_open()) {
            std::string source((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            auto parsed = Scripting::ParseProperties(source);
            for (const auto& pp : parsed) {
                auto sp = Scripting::ToScriptProperty(pp);
                bool found = false;
                for (auto& existing : script.properties) {
                    if (existing.name == sp.name) {
                        // Refresh metadata from source, preserve overridden instance values
                        existing.hasRange = sp.hasRange;
                        existing.rangeMin = sp.rangeMin;
                        existing.rangeMax = sp.rangeMax;
                        existing.tooltip = sp.tooltip;
                        existing.header = sp.header;
                        existing.type = sp.type;
                        existing.defaultValue = sp.defaultValue;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    script.properties.push_back(sp);
                }
            }
        }
    }

    // Create instance
    asIScriptObject* obj = m_ScriptEngine->CreateInstance(moduleName, script.className);
    if (!obj) {
        script.hasError = true;
        script.lastError = "Failed to create instance of " + script.className;
        // Carry the compiler's actual message so the editor can show WHY
        // (missing file, syntax error with line number) instead of just "failed"
        const std::string& engineErr = m_ScriptEngine->GetLastError();
        if (!engineErr.empty()) {
            script.lastError += " — " + engineErr;
        }
        ENJIN_LOG_ERROR(Script, "%s", script.lastError.c_str());
        return;
    }

    script.instance = obj;
    script.hasError = false;
    script.lastError.clear();

    // Cache method IDs
    CacheMethodIds(script);

    // Apply property overrides
    m_ScriptEngine->ApplyProperties(obj, script.properties);

    // Set the entity ID on the object (the 'entity' member from TegeBehavior)
    asITypeInfo* type = obj->GetObjectType();
    for (asUINT i = 0; i < type->GetPropertyCount(); ++i) {
        const char* propName = nullptr;
        int typeId = 0;
        type->GetProperty(i, &propName, &typeId);
        if (std::string(propName) == "_entityId" && typeId == asTYPEID_UINT64) {
            *reinterpret_cast<u64*>(obj->GetAddressOfProperty(i)) = entity;
            break;
        }
    }

    script.initialized = true;

    // Call OnCreate
    CallLifecycleMethod(script, script.methodOnCreate, "OnCreate");

    // Call OnEnable if enabled
    if (script.enabled) {
        CallLifecycleMethod(script, script.methodOnEnable, "OnEnable");
    }
}

void ScriptSystem::InitializeAllScripts() {
    if (!m_World || !m_ScriptEngine) return;

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::ScriptComponent>()) {
        auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
        if (!sc) continue;

        for (auto& script : sc->scripts) {
            if (!script.initialized) {
                InitScript(entity, script);
            }
        }
    }

    ENJIN_LOG_INFO(Script, "All scripts initialized");
}

void ScriptSystem::ShutdownAllScripts() {
    if (!m_World) return;

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::ScriptComponent>()) {
        auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
        if (!sc) continue;

        for (auto& script : sc->scripts) {
            if (script.initialized && script.instance && !script.hasError) {
                // Call OnDisable then OnDestroy
                if (script.enabled) {
                    CallLifecycleMethod(script, script.methodOnDisable, "OnDisable");
                }
                CallLifecycleMethod(script, script.methodOnDestroy, "OnDestroy");
            }

            // Release instance
            if (script.instance && m_ScriptEngine) {
                m_ScriptEngine->ReleaseInstance(static_cast<asIScriptObject*>(script.instance));
            }
            script.instance = nullptr;
            script.initialized = false;
            script.started = false;
            script.hasError = false;
            script.lastError.clear();
        }
    }

    m_FixedTimeAccumulator = 0.0f;
    ENJIN_LOG_INFO(Script, "All scripts shut down");
}

void ScriptSystem::Update(f32 deltaTime) {
    if (!m_Enabled || !m_World || !m_ScriptEngine) return;

    // Query script entities once per frame — reused by all phases including FixedUpdate/LateUpdate
    m_CachedScriptEntities = m_World->GetEntitiesWithComponent<ECS::ScriptComponent>();

    // 1. Poll for file changes and process hot reload
    m_ScriptEngine->PollFileChanges();
    if (m_ScriptEngine->ProcessHotReload()) {
        // Re-initialize scripts whose modules were reloaded
        for (ECS::Entity entity : m_CachedScriptEntities) {
            auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
            if (!sc) continue;

            for (auto& script : sc->scripts) {
                if (script.hasError) {
                    // Retry: release old instance and re-init
                    if (script.instance) {
                        m_ScriptEngine->ReleaseInstance(static_cast<asIScriptObject*>(script.instance));
                        script.instance = nullptr;
                    }
                    script.initialized = false;
                    script.started = false;
                    script.hasError = false;
                    script.lastError.clear();
                    InitScript(entity, script);
                }
            }
        }
    }

    // 2. Initialize new scripts + 3. Call OnStart — single pass
    for (ECS::Entity entity : m_CachedScriptEntities) {
        auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
        if (!sc) continue;

        for (auto& script : sc->scripts) {
            if (!script.initialized) {
                InitScript(entity, script);
            }
            if (script.initialized && !script.started && !script.hasError && script.enabled) {
                CallLifecycleMethod(script, script.methodOnStart, "OnStart");
                script.started = true;
            }
        }
    }

    // 4. Fixed update (accumulate time, run at fixed intervals)
    m_FixedTimeAccumulator += deltaTime;
    while (m_FixedTimeAccumulator >= FIXED_TIMESTEP) {
        FixedUpdate(FIXED_TIMESTEP);
        m_FixedTimeAccumulator -= FIXED_TIMESTEP;
    }

    // 5. OnUpdate
    for (ECS::Entity entity : m_CachedScriptEntities) {
        auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
        if (!sc) continue;

        for (auto& script : sc->scripts) {
            if (script.initialized && script.started && !script.hasError && script.enabled) {
                CallLifecycleMethodFloat(script, script.methodOnUpdate, "OnUpdate", deltaTime);
            }
        }
    }

    // 6. Update coroutines
    if (m_Scheduler) {
        m_Scheduler->Update(deltaTime);
    }

    // 7. OnLateUpdate
    LateUpdate(deltaTime);
}

void ScriptSystem::FixedUpdate(f32 fixedDeltaTime) {
    if (!m_Enabled || !m_World) return;

    for (ECS::Entity entity : m_CachedScriptEntities) {
        auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
        if (!sc) continue;

        for (auto& script : sc->scripts) {
            if (script.initialized && script.started && !script.hasError && script.enabled) {
                CallLifecycleMethodFloat(script, script.methodOnFixedUpdate, "OnFixedUpdate", fixedDeltaTime);
            }
        }
    }
}

void ScriptSystem::LateUpdate(f32 deltaTime) {
    if (!m_Enabled || !m_World) return;

    for (ECS::Entity entity : m_CachedScriptEntities) {
        auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
        if (!sc) continue;

        for (auto& script : sc->scripts) {
            if (script.initialized && script.started && !script.hasError && script.enabled) {
                CallLifecycleMethodFloat(script, script.methodOnLateUpdate, "OnLateUpdate", deltaTime);
            }
        }
    }
}

void ScriptSystem::OnCollisionEnter(ECS::Entity entity, ECS::Entity other) {
    if (!m_World || !m_World->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
    if (!sc) return;
    for (auto& script : sc->scripts) {
        CallCollisionMethod(script, script.methodOnCollisionEnter, "OnCollisionEnter", other);
    }
}

void ScriptSystem::OnCollisionStay(ECS::Entity entity, ECS::Entity other) {
    if (!m_World || !m_World->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
    if (!sc) return;
    for (auto& script : sc->scripts) {
        CallCollisionMethod(script, script.methodOnCollisionStay, "OnCollisionStay", other);
    }
}

void ScriptSystem::OnCollisionExit(ECS::Entity entity, ECS::Entity other) {
    if (!m_World || !m_World->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
    if (!sc) return;
    for (auto& script : sc->scripts) {
        CallCollisionMethod(script, script.methodOnCollisionExit, "OnCollisionExit", other);
    }
}

void ScriptSystem::OnTriggerEnter(ECS::Entity entity, ECS::Entity other) {
    if (!m_World || !m_World->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
    if (!sc) return;
    for (auto& script : sc->scripts) {
        CallCollisionMethod(script, script.methodOnTriggerEnter, "OnTriggerEnter", other);
    }
}

void ScriptSystem::OnTriggerExit(ECS::Entity entity, ECS::Entity other) {
    if (!m_World || !m_World->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
    if (!sc) return;
    for (auto& script : sc->scripts) {
        CallCollisionMethod(script, script.methodOnTriggerExit, "OnTriggerExit", other);
    }
}

} // namespace Scripting
} // namespace Enjin
