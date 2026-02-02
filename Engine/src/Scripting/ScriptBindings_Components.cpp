#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include <angelscript.h>
#include <string>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// Health component access
// ============================================================================

static f32 Health_Get(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    return hc ? hc->currentHealth : 0.0f;
}

static f32 Health_GetMax(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    return hc ? hc->maxHealth : 0.0f;
}

static void Health_SetCurrent(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    if (hc) hc->currentHealth = Math::Clamp(val, 0.0f, hc->maxHealth);
}

static void Health_Damage(u64 id, f32 amount) {
    if (!s_BindingsWorld) return;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    if (!hc) return;

    // Apply to shield first
    if (hc->currentShield > 0.0f) {
        f32 shieldAbsorb = Math::Min(amount, hc->currentShield);
        hc->currentShield -= shieldAbsorb;
        amount -= shieldAbsorb;
    }

    // Check invulnerability
    if (hc->invulnerabilityTimer > 0.0f) return;

    hc->currentHealth = Math::Max(hc->currentHealth - amount, 0.0f);
    if (amount > 0.0f) {
        hc->onDamageNotify = true;
    }
    if (hc->currentHealth <= 0.0f) {
        hc->onDeathNotify = true;
    }
}

// ============================================================================
// Material component access
// ============================================================================

static void Material_SetBaseColor(u64 id, const Vector3& color) {
    if (!s_BindingsWorld) return;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    if (mc) mc->baseColor = color;
}

static Vector3 Material_GetBaseColor(u64 id) {
    if (!s_BindingsWorld) return Vector3(1.0f);
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    return mc ? mc->baseColor : Vector3(1.0f);
}

static void Material_SetMetallic(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    if (mc) mc->metallic = val;
}

static void Material_SetRoughness(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    if (mc) mc->roughness = val;
}

// ============================================================================
// Light component access
// ============================================================================

static void Light_SetColor(u64 id, const Vector3& color) {
    if (!s_BindingsWorld) return;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    if (lc) lc->color = color;
}

static void Light_SetIntensity(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    if (lc) lc->intensity = val;
}

// ============================================================================
// Camera component access
// ============================================================================

static void Camera_SetFOV(u64 id, f32 fov) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<CameraComponent>(static_cast<Entity>(id));
    if (cc) cc->fieldOfView = fov;
}

static f32 Camera_GetFOV(u64 id) {
    if (!s_BindingsWorld) return 60.0f;
    auto* cc = s_BindingsWorld->GetComponent<CameraComponent>(static_cast<Entity>(id));
    return cc ? cc->fieldOfView : 60.0f;
}

// ============================================================================
// AudioSource component access
// ============================================================================

static void AudioSource_Play(u64 id) {
    if (!s_BindingsWorld) return;
    auto* asc = s_BindingsWorld->GetComponent<AudioSourceComponent>(static_cast<Entity>(id));
    if (asc) asc->isPlaying = true;
}

static void AudioSource_Stop(u64 id) {
    if (!s_BindingsWorld) return;
    auto* asc = s_BindingsWorld->GetComponent<AudioSourceComponent>(static_cast<Entity>(id));
    if (asc) {
        asc->isPlaying = false;
        asc->soundHandle = 0;
    }
}

static void AudioSource_SetClip(u64 id, const std::string& path) {
    if (!s_BindingsWorld) return;
    auto* asc = s_BindingsWorld->GetComponent<AudioSourceComponent>(static_cast<Entity>(id));
    if (asc) asc->clipPath = path;
}

static void AudioSource_SetVolume(u64 id, f32 vol) {
    if (!s_BindingsWorld) return;
    auto* asc = s_BindingsWorld->GetComponent<AudioSourceComponent>(static_cast<Entity>(id));
    if (asc) asc->volume = vol;
}

// ============================================================================
// Animator component access
// ============================================================================

static void Animator_Play(u64 id, const std::string& animName) {
    if (!s_BindingsWorld) return;
    auto* ac = s_BindingsWorld->GetComponent<AnimatorComponent>(static_cast<Entity>(id));
    if (ac) {
        ac->animator.Play(animName);
    }
}

static void Animator_SetSpeed(u64 id, f32 speed) {
    if (!s_BindingsWorld) return;
    auto* ac = s_BindingsWorld->GetComponent<AnimatorComponent>(static_cast<Entity>(id));
    if (ac) {
        ac->animator.SetSpeed(speed);
    }
}

// ============================================================================
// CharacterController component access
// ============================================================================

static void Controller_SetMoveSpeed(u64 id, f32 speed) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(id);

    // Try each controller type
    if (auto* c = s_BindingsWorld->GetComponent<FirstPersonController>(entity)) { c->moveSpeed = speed; return; }
    if (auto* c = s_BindingsWorld->GetComponent<ThirdPersonController>(entity)) { c->moveSpeed = speed; return; }
    if (auto* c = s_BindingsWorld->GetComponent<TopDown3DController>(entity)) { c->moveSpeed = speed; return; }
    if (auto* c = s_BindingsWorld->GetComponent<TopDown2DController>(entity)) { c->moveSpeed = speed; return; }
    if (auto* c = s_BindingsWorld->GetComponent<Platformer2DController>(entity)) { c->moveSpeed = speed; return; }
}

static Vector3 Controller_GetVelocity(u64 id) {
    if (!s_BindingsWorld) return Vector3();
    Entity entity = static_cast<Entity>(id);

    if (auto* c = s_BindingsWorld->GetComponent<FirstPersonController>(entity)) return c->velocity;
    if (auto* c = s_BindingsWorld->GetComponent<ThirdPersonController>(entity)) return c->velocity;
    if (auto* c = s_BindingsWorld->GetComponent<TopDown3DController>(entity)) return c->velocity;
    if (auto* c = s_BindingsWorld->GetComponent<TopDown2DController>(entity)) return c->velocity;
    if (auto* c = s_BindingsWorld->GetComponent<Platformer2DController>(entity)) return c->velocity;
    return Vector3();
}

// ============================================================================
// HasComponent queries
// ============================================================================

static bool HasComponent_Health(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<HealthComponent>(static_cast<Entity>(id));
}

static bool HasComponent_Light(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<LightComponent>(static_cast<Entity>(id));
}

static bool HasComponent_Camera(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<CameraComponent>(static_cast<Entity>(id));
}

static bool HasComponent_Material(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<MaterialComponent>(static_cast<Entity>(id));
}

static bool HasComponent_AudioSource(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<AudioSourceComponent>(static_cast<Entity>(id));
}

static bool HasComponent_Rigidbody(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<RigidbodyComponent>(static_cast<Entity>(id));
}

static bool HasComponent_BoxCollider(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<BoxColliderComponent>(static_cast<Entity>(id));
}

static bool HasComponent_Animator(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<AnimatorComponent>(static_cast<Entity>(id));
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterComponentBindings(asIScriptEngine* engine) {
    // Health
    AS_CHECK(engine->RegisterGlobalFunction("float Health_Get(uint64)", asFUNCTION(Health_Get), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Health_GetMax(uint64)", asFUNCTION(Health_GetMax), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Health_SetCurrent(uint64, float)", asFUNCTION(Health_SetCurrent), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Health_Damage(uint64, float)", asFUNCTION(Health_Damage), asCALL_CDECL));

    // Material
    AS_CHECK(engine->RegisterGlobalFunction("void Material_SetBaseColor(uint64, const Vector3 &in)", asFUNCTION(Material_SetBaseColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 Material_GetBaseColor(uint64)", asFUNCTION(Material_GetBaseColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Material_SetMetallic(uint64, float)", asFUNCTION(Material_SetMetallic), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Material_SetRoughness(uint64, float)", asFUNCTION(Material_SetRoughness), asCALL_CDECL));

    // Light
    AS_CHECK(engine->RegisterGlobalFunction("void Light_SetColor(uint64, const Vector3 &in)", asFUNCTION(Light_SetColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Light_SetIntensity(uint64, float)", asFUNCTION(Light_SetIntensity), asCALL_CDECL));

    // Camera
    AS_CHECK(engine->RegisterGlobalFunction("void Camera_SetFOV(uint64, float)", asFUNCTION(Camera_SetFOV), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Camera_GetFOV(uint64)", asFUNCTION(Camera_GetFOV), asCALL_CDECL));

    // AudioSource
    AS_CHECK(engine->RegisterGlobalFunction("void AudioSource_Play(uint64)", asFUNCTION(AudioSource_Play), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AudioSource_Stop(uint64)", asFUNCTION(AudioSource_Stop), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AudioSource_SetClip(uint64, const string &in)", asFUNCTION(AudioSource_SetClip), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AudioSource_SetVolume(uint64, float)", asFUNCTION(AudioSource_SetVolume), asCALL_CDECL));

    // Animator
    AS_CHECK(engine->RegisterGlobalFunction("void Animator_Play(uint64, const string &in)", asFUNCTION(Animator_Play), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Animator_SetSpeed(uint64, float)", asFUNCTION(Animator_SetSpeed), asCALL_CDECL));

    // CharacterController
    AS_CHECK(engine->RegisterGlobalFunction("void Controller_SetMoveSpeed(uint64, float)", asFUNCTION(Controller_SetMoveSpeed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 Controller_GetVelocity(uint64)", asFUNCTION(Controller_GetVelocity), asCALL_CDECL));

    // HasComponent queries
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Health(uint64)", asFUNCTION(HasComponent_Health), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Light(uint64)", asFUNCTION(HasComponent_Light), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Camera(uint64)", asFUNCTION(HasComponent_Camera), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Material(uint64)", asFUNCTION(HasComponent_Material), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_AudioSource(uint64)", asFUNCTION(HasComponent_AudioSource), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Rigidbody(uint64)", asFUNCTION(HasComponent_Rigidbody), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_BoxCollider(uint64)", asFUNCTION(HasComponent_BoxCollider), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Animator(uint64)", asFUNCTION(HasComponent_Animator), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
