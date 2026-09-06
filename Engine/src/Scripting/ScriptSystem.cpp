#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"   // ClearBindingsEventListeners on teardown + mouse pick
#include "Enjin/Platform/Input.h"            // mouse position/buttons for OnMouseEnter/OnClick
#include "Enjin/ECS/Components/Skeleton.h"   // AnimatorComponent — animation-event wiring
#include "Enjin/Scripting/ScriptPropertyParser.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Scripting {

ScriptSystem::ScriptSystem() = default;
ScriptSystem::~ScriptSystem() {
    // The destroy observer captures `this`; leaving it registered on a World
    // that outlives this system is a dangling call on the next despawn.
    RemoveDestroyObserver();
}

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
    script.methodOnAnimationEvent = findMethod("void OnAnimationEvent(string)");
    if (script.methodOnAnimationEvent < 0)
        script.methodOnAnimationEvent = findMethod("void OnAnimationEvent(const string&in)");
    script.methodOnMouseEnter     = findMethod("void OnMouseEnter()");
    script.methodOnMouseExit      = findMethod("void OnMouseExit()");
    script.methodOnClick          = findMethod("void OnClick()");
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

bool ScriptSystem::ClassifyExecuteResult(ECS::ScriptAttachment& script, int result,
                                         const char* methodName, const char* exceptionText) {
    if (result == asEXECUTION_FINISHED) return true;

    if (result == asEXECUTION_EXCEPTION) {
        script.lastError = exceptionText ? exceptionText : "";
        HandleScriptError(script, methodName);
        return false;
    }

    if (result == asEXECUTION_ABORTED) {
        // ScriptEngine's line callback aborts a context that blows the
        // instruction budget. That abort used to land here as SUCCESS: every
        // dispatcher tested only for exceptions, so hasError was never set,
        // the script was never disabled, and the runaway method ran again on
        // the very next frame with a fresh counter (the budget is allocated
        // per AcquireContext). The limit stopped one call and bounded nothing
        // over time -- a while(true) in OnUpdate burned the full budget every
        // frame forever, logging one line each time.
        //
        // Latch the script off instead. A method that exhausts the budget once
        // will exhaust it every frame, and there was no other way to stop it.
        script.lastError = "Aborted: exceeded the script instruction budget in "
                           + std::string(methodName) + "(). This script is now disabled.";
        script.hasError = true;
        ENJIN_LOG_ERROR(Script, "Script aborted (instruction budget) in %s::%s -- script disabled",
                        script.className.c_str(), methodName);
        return false;
    }

    if (result == asCONTEXT_NOT_PREPARED) {
        ENJIN_LOG_ERROR(Script, "Context not prepared for %s::%s", script.className.c_str(), methodName);
        return false;
    }

    // Anything else (asEXECUTION_SUSPENDED, an error code) is reported rather
    // than passed off as a completed call. Not latched: only the abort above
    // is known to repeat every frame.
    ENJIN_LOG_WARN(Script, "Unexpected script execution result %d in %s::%s",
                   result, script.className.c_str(), methodName);
    return false;
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
    bool success = ClassifyExecuteResult(script, r, methodName,
        r == asEXECUTION_EXCEPTION ? ctx->GetExceptionString() : nullptr);

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
    bool success = ClassifyExecuteResult(script, r, methodName,
        r == asEXECUTION_EXCEPTION ? ctx->GetExceptionString() : nullptr);

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
    bool success = ClassifyExecuteResult(script, r, methodName,
        r == asEXECUTION_EXCEPTION ? ctx->GetExceptionString() : nullptr);

    m_ScriptEngine->ReturnContext(ctx);
    return success;
}

bool ScriptSystem::CallStringMethod(ECS::ScriptAttachment& script, int methodId, const char* methodName, const std::string& arg) {
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
    // AngelScript 'string' is registered as std::string (RegisterStdString), so the arg is
    // passed as a pointer to a std::string. The copy outlives Execute().
    std::string argCopy = arg;
    ctx->SetArgObject(0, &argCopy);

    int r = ctx->Execute();
    bool success = ClassifyExecuteResult(script, r, methodName,
        r == asEXECUTION_EXCEPTION ? ctx->GetExceptionString() : nullptr);

    m_ScriptEngine->ReturnContext(ctx);
    return success;
}

void ScriptSystem::OnAnimationEvent(ECS::Entity entity, const std::string& name) {
    if (!m_World || !m_World->HasComponent<ECS::ScriptComponent>(entity)) return;
    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
    if (!sc) return;
    for (auto& script : sc->scripts) {
        CallStringMethod(script, script.methodOnAnimationEvent, "OnAnimationEvent", name);
    }
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

    // Wire animation events to this script's optional OnAnimationEvent(string) hook.
    // The animator collects events during the parallel pose sample; RenderSystem fires
    // this callback on the main thread (FlushEvents), so calling AngelScript here is safe.
    // Captures the entity by value and re-looks-up the script on dispatch, so it stays
    // valid across script teardown/replay (no-ops when the instance is gone).
    if (script.methodOnAnimationEvent >= 0 && m_World) {
        if (auto* anim = m_World->GetComponent<ECS::AnimatorComponent>(entity)) {
            ECS::Entity e = entity;
            anim->animator.SetEventCallback([this, e](const std::string& name) {
                OnAnimationEvent(e, name);
            });
        }
    }

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

void ScriptSystem::InstallDestroyObserver() {
    if (!m_World || m_DestroyObserverToken != 0) return;
    m_WorldLife = m_World->LifeToken();
    m_DestroyObserverToken = m_World->AddEntityDestroyObserver([this](ECS::Entity e) {
        TeardownEntityScripts(e);
    });
}

void ScriptSystem::RemoveDestroyObserver() {
    if (m_DestroyObserverToken == 0) return;
    const ECS::World::DestroyObserverToken token = m_DestroyObserverToken;
    m_DestroyObserverToken = 0;

    // The World may already be gone. GamePlayer::Shutdown calls m_World.reset()
    // and only then lets its members destruct, so this system's destructor runs
    // with m_World pointing at freed memory -- dereferencing it to unregister
    // crashed the exported game on shutdown. There is nothing to unregister
    // from a World that no longer exists.
    if (m_WorldLife.expired()) return;
    if (m_World) m_World->RemoveEntityDestroyObserver(token);
}

void ScriptSystem::TeardownEntityScripts(ECS::Entity entity) {
    if (!m_World) return;
    auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
    if (!sc) return;

    for (auto& script : sc->scripts) {
        if (script.initialized && script.instance && !script.hasError) {
            if (script.enabled) {
                CallLifecycleMethod(script, script.methodOnDisable, "OnDisable");
            }
            CallLifecycleMethod(script, script.methodOnDestroy, "OnDestroy");
        }

        if (script.instance && m_ScriptEngine) {
            m_ScriptEngine->ReleaseInstance(static_cast<asIScriptObject*>(script.instance));
        }
        script.instance = nullptr;
        script.initialized = false;
        script.started = false;
        script.hasError = false;
        script.lastError.clear();
    }

    // A dead entity's coroutines and event listeners must go with it. Both of
    // these functions existed with zero callers: coroutines kept ticking on a
    // destroyed entity, and its listeners kept firing until the 1024-per-event
    // cap started rejecting new registrations.
    const u64 id = static_cast<u64>(entity);
    if (m_Scheduler) m_Scheduler->StopAllForEntity(id);
    Scripting::RemoveBindingsEventListenersForEntity(id);
}

void ScriptSystem::ShutdownAllScripts() {
    if (!m_World) return;

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::ScriptComponent>()) {
        TeardownEntityScripts(entity);
    }

    m_FixedTimeAccumulator = 0.0f;

    // Release every Events_Listen callback/object the bus AddRef'd. Without this the
    // delegates survive at ref count 1 to the engine's shutdown GC ("GC cannot
    // destroy $func" spam + a close hitch), and listeners leak across Play/Stop.
    Scripting::ClearBindingsEventListeners();
    // The script clock restarts with the scripts. Without this a second Play in
    // the editor continues the first run's Time_GetTime().
    Scripting::ResetBindingsTime();

    ENJIN_LOG_INFO(Script, "All scripts shut down");
}

void ScriptSystem::Update(f32 deltaTime) {
    if (!m_Enabled || !m_World || !m_ScriptEngine) return;

    // Query script entities once per frame — reused by all phases including FixedUpdate/LateUpdate
    // Advance the script-visible clock first, so anything a script reads this
    // frame already reflects this frame. Doing it here means every runtime gets
    // it -- all three call Update -- rather than three places remembering to.
    Scripting::TickBindingsTime(deltaTime);

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
            // hasError gates the retry: a failed init used to re-run EVERY
            // frame (full compile attempt + file IO + 4 error lines — 50k log
            // lines in a minute when a script file is missing). Hot reload
            // clears hasError when the source changes, and play-mode stop
            // resets it, so those remain the retry paths.
            if (!script.initialized && !script.hasError) {
                InitScript(entity, script);
            }
            if (script.initialized && !script.started && !script.hasError && script.enabled) {
                CallLifecycleMethod(script, script.methodOnStart, "OnStart");
                script.started = true;
            }
        }
    }

    // 4. Fixed update (accumulate time, run at fixed intervals). When the
    // SimulationClock drives ticks (fixed-timestep projects), the runtime calls
    // FixedUpdate inside the physics step loop instead — skip the internal
    // accumulator so scripts don't tick twice.
    if (!m_ExternalFixedClock) {
        m_FixedTimeAccumulator += deltaTime;
        while (m_FixedTimeAccumulator >= FIXED_TIMESTEP) {
            FixedUpdate(FIXED_TIMESTEP);
            m_FixedTimeAccumulator -= FIXED_TIMESTEP;
        }
    }

    // 4b. Mouse-over callbacks (before OnUpdate so enter/click state is
    // visible to the same frame's OnUpdate)
    UpdateMouseCallbacks();

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

    // Time_GetFixedDeltaTime() should report the step actually being run, not
    // the 1/60 the binding was initialised with.
    Scripting::TickBindingsTime(0.0f, fixedDeltaTime);

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

void ScriptSystem::UpdateMouseCallbacks() {
    // Only pay for the raycast when a loaded script actually listens.
    bool anyListener = false;
    for (ECS::Entity entity : m_CachedScriptEntities) {
        auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
        if (!sc) continue;
        for (auto& script : sc->scripts) {
            if (script.methodOnMouseEnter >= 0 || script.methodOnMouseExit >= 0 ||
                script.methodOnClick >= 0) {
                anyListener = true;
                break;
            }
        }
        if (anyListener) break;
    }
    if (!anyListener && m_MouseHoverEntity == ECS::INVALID_ENTITY) return;

    ECS::Entity hovered = ECS::INVALID_ENTITY;
    if (anyListener && !Input::IsMouseCaptured()) {
        Math::Vector2 mouse = Input::GetMousePosition();
        hovered = static_cast<ECS::Entity>(BindingsPickEntityAtScreen(mouse.x, mouse.y));
        if (hovered != ECS::INVALID_ENTITY && !m_World->IsValid(hovered))
            hovered = ECS::INVALID_ENTITY;
    }

    auto dispatch = [this](ECS::Entity entity, int ECS::ScriptAttachment::*method, const char* name) {
        if (entity == ECS::INVALID_ENTITY || !m_World->IsValid(entity)) return;
        auto* sc = m_World->GetComponent<ECS::ScriptComponent>(entity);
        if (!sc) return;
        for (auto& script : sc->scripts) {
            if (script.initialized && script.started)
                CallLifecycleMethod(script, script.*method, name);
        }
    };

    if (hovered != m_MouseHoverEntity) {
        dispatch(m_MouseHoverEntity, &ECS::ScriptAttachment::methodOnMouseExit, "OnMouseExit");
        dispatch(hovered, &ECS::ScriptAttachment::methodOnMouseEnter, "OnMouseEnter");
        m_MouseHoverEntity = hovered;
    }

    if (hovered != ECS::INVALID_ENTITY && Input::IsMouseButtonPressed(MouseButton::Left)) {
        dispatch(hovered, &ECS::ScriptAttachment::methodOnClick, "OnClick");
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
