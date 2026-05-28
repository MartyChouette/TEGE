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
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/LOD.h"
#include <angelscript.h>
#include <string>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

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

static void Material_SetTransmission(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    if (mc) mc->transmission = val;
}

static f32 Material_GetTransmission(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    return mc ? mc->transmission : 0.0f;
}

static void Material_SetIOR(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    if (mc) mc->ior = val;
}

static f32 Material_GetIOR(u64 id) {
    if (!s_BindingsWorld) return 1.5f;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    return mc ? mc->ior : 1.5f;
}

static void Material_SetThickness(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    if (mc) mc->thickness = val;
}

static f32 Material_GetThickness(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    return mc ? mc->thickness : 0.0f;
}

static void Material_SetSSSIntensity(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    if (mc) mc->sssIntensity = val;
}

static f32 Material_GetSSSIntensity(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    return mc ? mc->sssIntensity : 0.0f;
}

static void Material_SetSSSRadius(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    if (mc) mc->sssRadius = val;
}

static f32 Material_GetSSSRadius(u64 id) {
    if (!s_BindingsWorld) return 1.0f;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    return mc ? mc->sssRadius : 1.0f;
}

static void Material_SetSSSColor(u64 id, const Vector3& color) {
    if (!s_BindingsWorld) return;
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    if (mc) mc->sssColor = color;
}

static Vector3 Material_GetSSSColor(u64 id) {
    if (!s_BindingsWorld) return Vector3(1.0f, 0.2f, 0.1f);
    auto* mc = s_BindingsWorld->GetComponent<MaterialComponent>(static_cast<Entity>(id));
    return mc ? mc->sssColor : Vector3(1.0f, 0.2f, 0.1f);
}

// ============================================================================
// Light component access
// ============================================================================

static void Light_SetColor(u64 id, const Vector3& color) {
    if (!s_BindingsWorld) return;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    if (lc) lc->color = color;
}

static Vector3 Light_GetColor(u64 id) {
    if (!s_BindingsWorld) return Vector3(1, 1, 1);
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    return lc ? lc->color : Vector3(1, 1, 1);
}

static void Light_SetIntensity(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    if (lc) lc->intensity = val;
}

static f32 Light_GetIntensity(u64 id) {
    if (!s_BindingsWorld) return 1.0f;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    return lc ? lc->intensity : 1.0f;
}

static void Light_SetRange(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    if (lc) lc->range = val;
}

static f32 Light_GetRange(u64 id) {
    if (!s_BindingsWorld) return 10.0f;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    return lc ? lc->range : 10.0f;
}

static void Light_SetType(u64 id, i32 type) {
    if (!s_BindingsWorld) return;
    if (type < 0 || type > 2) return;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    if (lc) lc->type = static_cast<LightType>(type);
}

static i32 Light_GetType(u64 id) {
    if (!s_BindingsWorld) return 1;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    return lc ? static_cast<i32>(lc->type) : 1;
}

static void Light_SetCastShadows(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    if (lc) lc->castShadows = val;
}

static bool Light_GetCastShadows(u64 id) {
    if (!s_BindingsWorld) return true;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    return lc ? lc->castShadows : true;
}

static void Light_SetSpotAngles(u64 id, f32 inner, f32 outer) {
    if (!s_BindingsWorld) return;
    auto* lc = s_BindingsWorld->GetComponent<LightComponent>(static_cast<Entity>(id));
    if (lc) { lc->innerConeAngle = inner; lc->outerConeAngle = outer; }
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

static void Camera_ApplyPreset(u64 id, int presetIndex) {
    if (!s_BindingsWorld) return;
    if (presetIndex < 0 || presetIndex >= static_cast<int>(CameraPreset::Count)) return;
    auto* cc = s_BindingsWorld->GetComponent<CameraComponent>(static_cast<Entity>(id));
    if (!cc) return;
    auto result = ApplyCameraPreset(static_cast<CameraPreset>(presetIndex));
    *cc = result.camera;
    auto* tc = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    if (tc) tc->rotation = Math::Quaternion::FromEuler(result.rotation);
}

static std::string Camera_GetPresetName(int presetIndex) {
    if (presetIndex < 0 || presetIndex >= static_cast<int>(CameraPreset::Count)) return "Unknown";
    return CameraPresetName(static_cast<CameraPreset>(presetIndex));
}

static void Camera_SetOrthoSize(u64 id, f32 size) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<CameraComponent>(static_cast<Entity>(id));
    if (cc) cc->orthoSize = size;
}

static f32 Camera_GetOrthoSize(u64 id) {
    if (!s_BindingsWorld) return 10.0f;
    auto* cc = s_BindingsWorld->GetComponent<CameraComponent>(static_cast<Entity>(id));
    return cc ? cc->orthoSize : 10.0f;
}

static void Camera_SetNearFar(u64 id, f32 nearPlane, f32 farPlane) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<CameraComponent>(static_cast<Entity>(id));
    if (cc) { cc->nearPlane = nearPlane; cc->farPlane = farPlane; }
}

static void Camera_SetProjectionType(u64 id, i32 type) {
    if (!s_BindingsWorld) return;
    if (type < 0 || type > 1) return;
    auto* cc = s_BindingsWorld->GetComponent<CameraComponent>(static_cast<Entity>(id));
    if (cc) cc->projectionType = static_cast<ProjectionType>(type);
}

static i32 Camera_GetProjectionType(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* cc = s_BindingsWorld->GetComponent<CameraComponent>(static_cast<Entity>(id));
    return cc ? static_cast<i32>(cc->projectionType) : 0;
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

static void Animator_Stop(u64 id) {
    if (!s_BindingsWorld) return;
    auto* ac = s_BindingsWorld->GetComponent<AnimatorComponent>(static_cast<Entity>(id));
    if (ac) ac->animator.Stop();
}

static void Animator_Pause(u64 id) {
    if (!s_BindingsWorld) return;
    auto* ac = s_BindingsWorld->GetComponent<AnimatorComponent>(static_cast<Entity>(id));
    if (ac) ac->animator.Pause();
}

static void Animator_Resume(u64 id) {
    if (!s_BindingsWorld) return;
    auto* ac = s_BindingsWorld->GetComponent<AnimatorComponent>(static_cast<Entity>(id));
    if (ac) ac->animator.Resume();
}

static bool Animator_IsPlaying(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* ac = s_BindingsWorld->GetComponent<AnimatorComponent>(static_cast<Entity>(id));
    return ac ? ac->animator.IsPlaying() : false;
}

static std::string Animator_GetCurrentAnimation(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* ac = s_BindingsWorld->GetComponent<AnimatorComponent>(static_cast<Entity>(id));
    return ac ? ac->animator.GetCurrentAnimationName() : "";
}

static f32 Animator_GetSpeed(u64 id) {
    if (!s_BindingsWorld) return 1.0f;
    auto* ac = s_BindingsWorld->GetComponent<AnimatorComponent>(static_cast<Entity>(id));
    return ac ? ac->animator.GetSpeed() : 1.0f;
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
// Rigidbody component get/set
// ============================================================================

static Vector3 Rigidbody_GetVelocity(u64 id) {
    if (!s_BindingsWorld) return Vector3();
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    return rb ? rb->velocity : Vector3();
}

static void Rigidbody_SetVelocity(u64 id, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    if (rb) rb->velocity = Vector3(x, y, z);
}

static Vector3 Rigidbody_GetAngularVelocity(u64 id) {
    if (!s_BindingsWorld) return Vector3();
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    return rb ? rb->angularVelocity : Vector3();
}

static void Rigidbody_SetAngularVelocity(u64 id, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    if (rb) rb->angularVelocity = Vector3(x, y, z);
}

static f32 Rigidbody_GetMass(u64 id) {
    if (!s_BindingsWorld) return 1.0f;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    return rb ? rb->mass : 1.0f;
}

static void Rigidbody_SetMass(u64 id, f32 mass) {
    if (!s_BindingsWorld) return;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    if (rb) rb->mass = mass;
}

static bool Rigidbody_GetUseGravity(u64 id) {
    if (!s_BindingsWorld) return true;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    return rb ? rb->useGravity : true;
}

static void Rigidbody_SetUseGravity(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    if (rb) rb->useGravity = val;
}

static bool Rigidbody_IsKinematic(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    return rb ? (rb->bodyType == RigidbodyComponent::BodyType::Kinematic) : false;
}

static void Rigidbody_SetKinematic(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    if (rb) rb->bodyType = val ? RigidbodyComponent::BodyType::Kinematic : RigidbodyComponent::BodyType::Dynamic;
}

static f32 Rigidbody_GetDrag(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    return rb ? rb->drag : 0.0f;
}

static void Rigidbody_SetDrag(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    if (rb) rb->drag = val;
}

static f32 Rigidbody_GetAngularDrag(u64 id) {
    if (!s_BindingsWorld) return 0.05f;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    return rb ? rb->angularDrag : 0.05f;
}

static void Rigidbody_SetAngularDrag(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    if (rb) rb->angularDrag = val;
}

static bool Rigidbody_IsGrounded(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    return rb ? rb->isGrounded : false;
}

static f32 Rigidbody_GetGravityScale(u64 id) {
    if (!s_BindingsWorld) return 1.0f;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    return rb ? rb->gravityScale : 1.0f;
}

static void Rigidbody_SetGravityScale(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(static_cast<Entity>(id));
    if (rb) rb->gravityScale = val;
}

// ============================================================================
// BoxCollider component get/set
// ============================================================================

static Vector3 BoxCollider_GetSize(u64 id) {
    if (!s_BindingsWorld) return Vector3(1, 1, 1);
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    return bc ? bc->size : Vector3(1, 1, 1);
}

static void BoxCollider_SetSize(u64 id, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    if (bc) bc->size = Vector3(x, y, z);
}

static Vector3 BoxCollider_GetCenter(u64 id) {
    if (!s_BindingsWorld) return Vector3();
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    return bc ? bc->center : Vector3();
}

static void BoxCollider_SetCenter(u64 id, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    if (bc) bc->center = Vector3(x, y, z);
}

static bool BoxCollider_IsTrigger(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    return bc ? bc->isTrigger : false;
}

static void BoxCollider_SetTrigger(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    if (bc) bc->isTrigger = val;
}

static u32 BoxCollider_GetCategoryBits(u64 id) {
    if (!s_BindingsWorld) return 1;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    return bc ? bc->categoryBits : 1;
}

static void BoxCollider_SetCategoryBits(u64 id, u32 val) {
    if (!s_BindingsWorld) return;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    if (bc) bc->categoryBits = val;
}

static u32 BoxCollider_GetCollisionMask(u64 id) {
    if (!s_BindingsWorld) return 0xFFFFFFFF;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    return bc ? bc->collisionMask : 0xFFFFFFFF;
}

static void BoxCollider_SetCollisionMask(u64 id, u32 val) {
    if (!s_BindingsWorld) return;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    if (bc) bc->collisionMask = val;
}

static f32 BoxCollider_GetFriction(u64 id) {
    if (!s_BindingsWorld) return 0.5f;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    return bc ? bc->friction : 0.5f;
}

static void BoxCollider_SetFriction(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    if (bc) bc->friction = val;
}

static f32 BoxCollider_GetBounciness(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    return bc ? bc->bounciness : 0.0f;
}

static void BoxCollider_SetBounciness(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(static_cast<Entity>(id));
    if (bc) bc->bounciness = val;
}

// ============================================================================
// SphereCollider component get/set
// ============================================================================

static bool HasComponent_SphereCollider(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<SphereColliderComponent>(static_cast<Entity>(id));
}

static f32 SphereCollider_GetRadius(u64 id) {
    if (!s_BindingsWorld) return 0.5f;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    return sc ? sc->radius : 0.5f;
}

static void SphereCollider_SetRadius(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    if (sc) sc->radius = val;
}

static Vector3 SphereCollider_GetCenter(u64 id) {
    if (!s_BindingsWorld) return Vector3();
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    return sc ? sc->center : Vector3();
}

static void SphereCollider_SetCenter(u64 id, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    if (sc) sc->center = Vector3(x, y, z);
}

static bool SphereCollider_IsTrigger(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    return sc ? sc->isTrigger : false;
}

static void SphereCollider_SetTrigger(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    if (sc) sc->isTrigger = val;
}

static u32 SphereCollider_GetCategoryBits(u64 id) {
    if (!s_BindingsWorld) return 1;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    return sc ? sc->categoryBits : 1;
}

static void SphereCollider_SetCategoryBits(u64 id, u32 val) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    if (sc) sc->categoryBits = val;
}

static u32 SphereCollider_GetCollisionMask(u64 id) {
    if (!s_BindingsWorld) return 0xFFFFFFFF;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    return sc ? sc->collisionMask : 0xFFFFFFFF;
}

static void SphereCollider_SetCollisionMask(u64 id, u32 val) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    if (sc) sc->collisionMask = val;
}

static f32 SphereCollider_GetFriction(u64 id) {
    if (!s_BindingsWorld) return 0.5f;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    return sc ? sc->friction : 0.5f;
}

static void SphereCollider_SetFriction(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    if (sc) sc->friction = val;
}

static f32 SphereCollider_GetBounciness(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    return sc ? sc->bounciness : 0.0f;
}

static void SphereCollider_SetBounciness(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<SphereColliderComponent>(static_cast<Entity>(id));
    if (sc) sc->bounciness = val;
}

// ============================================================================
// CapsuleCollider component get/set
// ============================================================================

static bool HasComponent_CapsuleCollider(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
}

static f32 CapsuleCollider_GetRadius(u64 id) {
    if (!s_BindingsWorld) return 0.5f;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    return cc ? cc->radius : 0.5f;
}

static void CapsuleCollider_SetRadius(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    if (cc) cc->radius = val;
}

static f32 CapsuleCollider_GetHeight(u64 id) {
    if (!s_BindingsWorld) return 2.0f;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    return cc ? cc->height : 2.0f;
}

static void CapsuleCollider_SetHeight(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    if (cc) cc->height = val;
}

static Vector3 CapsuleCollider_GetCenter(u64 id) {
    if (!s_BindingsWorld) return Vector3();
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    return cc ? cc->center : Vector3();
}

static void CapsuleCollider_SetCenter(u64 id, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    if (cc) cc->center = Vector3(x, y, z);
}

static bool CapsuleCollider_IsTrigger(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    return cc ? cc->isTrigger : false;
}

static void CapsuleCollider_SetTrigger(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    if (cc) cc->isTrigger = val;
}

static f32 CapsuleCollider_GetFriction(u64 id) {
    if (!s_BindingsWorld) return 0.5f;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    return cc ? cc->friction : 0.5f;
}

static void CapsuleCollider_SetFriction(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    if (cc) cc->friction = val;
}

static f32 CapsuleCollider_GetBounciness(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    return cc ? cc->bounciness : 0.0f;
}

static void CapsuleCollider_SetBounciness(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<CapsuleColliderComponent>(static_cast<Entity>(id));
    if (cc) cc->bounciness = val;
}

// ============================================================================
// Tilemap component get/set
// ============================================================================

static bool HasComponent_Tilemap(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<TilemapComponent>(static_cast<Entity>(id));
}

static i32 Tilemap_GetTile(u64 id, i32 x, i32 y) {
    if (!s_BindingsWorld || x < 0 || y < 0) return -1;
    auto* tm = s_BindingsWorld->GetComponent<TilemapComponent>(static_cast<Entity>(id));
    return tm ? tm->GetTile(static_cast<u32>(x), static_cast<u32>(y)) : -1;
}

static void Tilemap_SetTile(u64 id, i32 x, i32 y, i32 tileIndex) {
    if (!s_BindingsWorld || x < 0 || y < 0) return;
    auto* tm = s_BindingsWorld->GetComponent<TilemapComponent>(static_cast<Entity>(id));
    if (tm) tm->SetTile(static_cast<u32>(x), static_cast<u32>(y), tileIndex);
}

static i32 Tilemap_GetWidth(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* tm = s_BindingsWorld->GetComponent<TilemapComponent>(static_cast<Entity>(id));
    return tm ? static_cast<i32>(tm->width) : 0;
}

static i32 Tilemap_GetHeight(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* tm = s_BindingsWorld->GetComponent<TilemapComponent>(static_cast<Entity>(id));
    return tm ? static_cast<i32>(tm->height) : 0;
}

// ============================================================================
// TriggerZone component get/set
// ============================================================================

static bool HasComponent_TriggerZone(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<TriggerZoneComponent>(static_cast<Entity>(id));
}

static i32 TriggerZone_GetShape(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* tz = s_BindingsWorld->GetComponent<TriggerZoneComponent>(static_cast<Entity>(id));
    return tz ? static_cast<i32>(tz->shape) : 0;
}

static void TriggerZone_SetShape(u64 id, i32 val) {
    if (!s_BindingsWorld) return;
    auto* tz = s_BindingsWorld->GetComponent<TriggerZoneComponent>(static_cast<Entity>(id));
    if (tz && val >= 0 && val <= 1) tz->shape = static_cast<TriggerZoneComponent::Shape>(val);
}

static Vector3 TriggerZone_GetBoxSize(u64 id) {
    if (!s_BindingsWorld) return Vector3(2, 2, 2);
    auto* tz = s_BindingsWorld->GetComponent<TriggerZoneComponent>(static_cast<Entity>(id));
    return tz ? tz->boxSize : Vector3(2, 2, 2);
}

static void TriggerZone_SetBoxSize(u64 id, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    auto* tz = s_BindingsWorld->GetComponent<TriggerZoneComponent>(static_cast<Entity>(id));
    if (tz) tz->boxSize = Vector3(x, y, z);
}

static f32 TriggerZone_GetSphereRadius(u64 id) {
    if (!s_BindingsWorld) return 1.0f;
    auto* tz = s_BindingsWorld->GetComponent<TriggerZoneComponent>(static_cast<Entity>(id));
    return tz ? tz->sphereRadius : 1.0f;
}

static void TriggerZone_SetSphereRadius(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* tz = s_BindingsWorld->GetComponent<TriggerZoneComponent>(static_cast<Entity>(id));
    if (tz) tz->sphereRadius = val;
}

static bool TriggerZone_GetTriggerOnce(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* tz = s_BindingsWorld->GetComponent<TriggerZoneComponent>(static_cast<Entity>(id));
    return tz ? tz->triggerOnce : false;
}

static void TriggerZone_SetTriggerOnce(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* tz = s_BindingsWorld->GetComponent<TriggerZoneComponent>(static_cast<Entity>(id));
    if (tz) tz->triggerOnce = val;
}

// ============================================================================
// Interactable component get/set
// ============================================================================

static bool HasComponent_Interactable(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<InteractableComponent>(static_cast<Entity>(id));
}

static std::string Interactable_GetPrompt(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* ic = s_BindingsWorld->GetComponent<InteractableComponent>(static_cast<Entity>(id));
    return ic ? ic->promptText : "";
}

static void Interactable_SetPrompt(u64 id, const std::string& val) {
    if (!s_BindingsWorld) return;
    auto* ic = s_BindingsWorld->GetComponent<InteractableComponent>(static_cast<Entity>(id));
    if (ic) ic->promptText = val;
}

static f32 Interactable_GetRange(u64 id) {
    if (!s_BindingsWorld) return 2.0f;
    auto* ic = s_BindingsWorld->GetComponent<InteractableComponent>(static_cast<Entity>(id));
    return ic ? ic->interactionRange : 2.0f;
}

static void Interactable_SetRange(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* ic = s_BindingsWorld->GetComponent<InteractableComponent>(static_cast<Entity>(id));
    if (ic) ic->interactionRange = val;
}

static bool Interactable_IsEnabled(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* ic = s_BindingsWorld->GetComponent<InteractableComponent>(static_cast<Entity>(id));
    return ic ? ic->isEnabled : false;
}

static void Interactable_SetEnabled(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* ic = s_BindingsWorld->GetComponent<InteractableComponent>(static_cast<Entity>(id));
    if (ic) ic->isEnabled = val;
}

static bool Interactable_HasBeenUsed(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* ic = s_BindingsWorld->GetComponent<InteractableComponent>(static_cast<Entity>(id));
    return ic ? ic->hasBeenUsed : false;
}

// ============================================================================
// Pickup component get/set
// ============================================================================

static bool HasComponent_Pickup(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<PickupComponent>(static_cast<Entity>(id));
}

static i32 Pickup_GetType(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* pc = s_BindingsWorld->GetComponent<PickupComponent>(static_cast<Entity>(id));
    return pc ? static_cast<i32>(pc->type) : 0;
}

static void Pickup_SetType(u64 id, i32 val) {
    if (!s_BindingsWorld) return;
    auto* pc = s_BindingsWorld->GetComponent<PickupComponent>(static_cast<Entity>(id));
    if (pc && val >= 0 && val <= 5) pc->type = static_cast<PickupComponent::PickupType>(val);
}

static f32 Pickup_GetValue(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* pc = s_BindingsWorld->GetComponent<PickupComponent>(static_cast<Entity>(id));
    return pc ? pc->value : 0.0f;
}

static void Pickup_SetValue(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* pc = s_BindingsWorld->GetComponent<PickupComponent>(static_cast<Entity>(id));
    if (pc) pc->value = val;
}

static std::string Pickup_GetCustomId(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* pc = s_BindingsWorld->GetComponent<PickupComponent>(static_cast<Entity>(id));
    return pc ? pc->customId : "";
}

static void Pickup_SetCustomId(u64 id, const std::string& val) {
    if (!s_BindingsWorld) return;
    auto* pc = s_BindingsWorld->GetComponent<PickupComponent>(static_cast<Entity>(id));
    if (pc) pc->customId = val;
}

static f32 Pickup_GetRange(u64 id) {
    if (!s_BindingsWorld) return 1.0f;
    auto* pc = s_BindingsWorld->GetComponent<PickupComponent>(static_cast<Entity>(id));
    return pc ? pc->pickupRange : 1.0f;
}

static void Pickup_SetRange(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* pc = s_BindingsWorld->GetComponent<PickupComponent>(static_cast<Entity>(id));
    if (pc) pc->pickupRange = val;
}

static bool Pickup_GetDestroyOnPickup(u64 id) {
    if (!s_BindingsWorld) return true;
    auto* pc = s_BindingsWorld->GetComponent<PickupComponent>(static_cast<Entity>(id));
    return pc ? pc->destroyOnPickup : true;
}

static void Pickup_SetDestroyOnPickup(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* pc = s_BindingsWorld->GetComponent<PickupComponent>(static_cast<Entity>(id));
    if (pc) pc->destroyOnPickup = val;
}

// ============================================================================
// Inventory component access
// ============================================================================

static bool HasComponent_Inventory(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<InventoryComponent>(static_cast<Entity>(id));
}

static i32 Inventory_GetItemCount(u64 id, const std::string& itemId) {
    if (!s_BindingsWorld) return 0;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    if (!inv) return 0;
    for (const auto& slot : inv->slots) {
        if (slot.itemId == itemId) return slot.quantity;
    }
    return 0;
}

static bool Inventory_AddItem(u64 id, const std::string& itemId, i32 count) {
    if (!s_BindingsWorld) return false;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    if (!inv) return false;
    // Try to stack
    for (auto& slot : inv->slots) {
        if (slot.itemId == itemId) {
            slot.quantity += count;
            if (slot.quantity > slot.maxStack) slot.quantity = slot.maxStack;
            return true;
        }
    }
    // New slot
    if (inv->slots.size() >= inv->maxSlots) return false;
    InventoryComponent::InventorySlot newSlot;
    newSlot.itemId = itemId;
    newSlot.quantity = count;
    inv->slots.push_back(newSlot);
    return true;
}

static bool Inventory_RemoveItem(u64 id, const std::string& itemId, i32 count) {
    if (!s_BindingsWorld) return false;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    if (!inv) return false;
    for (auto it = inv->slots.begin(); it != inv->slots.end(); ++it) {
        if (it->itemId == itemId) {
            if (it->quantity < count) return false;
            it->quantity -= count;
            if (it->quantity <= 0) inv->slots.erase(it);
            return true;
        }
    }
    return false;
}

static bool Inventory_HasItem(u64 id, const std::string& itemId) {
    if (!s_BindingsWorld) return false;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    if (!inv) return false;
    for (const auto& slot : inv->slots) {
        if (slot.itemId == itemId && slot.quantity > 0) return true;
    }
    return false;
}

static void Inventory_Clear(u64 id) {
    if (!s_BindingsWorld) return;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    if (inv) inv->slots.clear();
}

static i32 Inventory_GetCoins(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    return inv ? inv->coins : 0;
}

static void Inventory_SetCoins(u64 id, i32 val) {
    if (!s_BindingsWorld) return;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    if (inv) inv->coins = val;
}

static i32 Inventory_GetGems(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    return inv ? inv->gems : 0;
}

static void Inventory_SetGems(u64 id, i32 val) {
    if (!s_BindingsWorld) return;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    if (inv) inv->gems = val;
}

static bool Inventory_HasKey(u64 id, const std::string& keyId) {
    if (!s_BindingsWorld) return false;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    if (!inv) return false;
    for (const auto& k : inv->keys) {
        if (k == keyId) return true;
    }
    return false;
}

static void Inventory_AddKey(u64 id, const std::string& keyId) {
    if (!s_BindingsWorld) return;
    auto* inv = s_BindingsWorld->GetComponent<InventoryComponent>(static_cast<Entity>(id));
    if (!inv) return;
    for (const auto& k : inv->keys) {
        if (k == keyId) return; // Already has it
    }
    inv->keys.push_back(keyId);
}

// ============================================================================
// Timer component get/set
// ============================================================================

static bool HasComponent_Timer(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<TimerComponent>(static_cast<Entity>(id));
}

static f32 Timer_GetDuration(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    return tc ? tc->duration : 0.0f;
}

static void Timer_SetDuration(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    if (tc) tc->duration = val;
}

static f32 Timer_GetElapsed(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    return tc ? tc->elapsed : 0.0f;
}

static void Timer_SetElapsed(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    if (tc) tc->elapsed = val;
}

static bool Timer_IsRunning(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    return tc ? tc->isRunning : false;
}

static void Timer_SetRunning(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    if (tc) tc->isRunning = val;
}

static bool Timer_GetLoop(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    return tc ? tc->loop : false;
}

static void Timer_SetLoop(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    if (tc) tc->loop = val;
}

static f32 Timer_GetProgress(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    return tc ? tc->GetProgress() : 0.0f;
}

static f32 Timer_GetRemaining(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    return tc ? tc->GetRemaining() : 0.0f;
}

static bool Timer_IsComplete(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* tc = s_BindingsWorld->GetComponent<TimerComponent>(static_cast<Entity>(id));
    return tc ? tc->IsComplete() : false;
}

// ============================================================================
// Extended Health component bindings
// ============================================================================

static f32 Health_GetShield(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    return hc ? hc->currentShield : 0.0f;
}

static void Health_SetShield(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    if (hc) hc->currentShield = Math::Clamp(val, 0.0f, hc->maxShield);
}

static void Health_SetMaxHealth(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    if (hc) hc->maxHealth = val;
}

static bool Health_IsDead(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    return hc ? hc->isDead : false;
}

static bool Health_IsInvulnerable(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    return hc ? hc->isInvulnerable : false;
}

static void Health_SetInvulnerable(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    if (hc) hc->isInvulnerable = val;
}

static f32 Health_GetPercent(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    return hc ? hc->GetHealthPercent() : 0.0f;
}

static void Health_Heal(u64 id, f32 amount) {
    if (!s_BindingsWorld) return;
    auto* hc = s_BindingsWorld->GetComponent<HealthComponent>(static_cast<Entity>(id));
    if (hc) hc->currentHealth = Math::Min(hc->currentHealth + amount, hc->maxHealth);
}

// ============================================================================
// Lock component get/set
// ============================================================================

static bool HasComponent_Lock(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<LockComponent>(static_cast<Entity>(id));
}

static bool Lock_IsLocked(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* lc = s_BindingsWorld->GetComponent<LockComponent>(static_cast<Entity>(id));
    return lc ? lc->isLocked : false;
}

static void Lock_SetLocked(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* lc = s_BindingsWorld->GetComponent<LockComponent>(static_cast<Entity>(id));
    if (lc) lc->isLocked = val;
}

static std::string Lock_GetRequiredKey(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* lc = s_BindingsWorld->GetComponent<LockComponent>(static_cast<Entity>(id));
    return lc ? lc->requiredKey : "";
}

static bool Lock_IsOpen(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* lc = s_BindingsWorld->GetComponent<LockComponent>(static_cast<Entity>(id));
    return lc ? lc->isOpen : false;
}

static void Lock_SetOpen(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* lc = s_BindingsWorld->GetComponent<LockComponent>(static_cast<Entity>(id));
    if (lc) lc->isOpen = val;
}

// ============================================================================
// Switch component get/set
// ============================================================================

static bool HasComponent_Switch(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<SwitchComponent>(static_cast<Entity>(id));
}

static bool Switch_IsActive(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* sc = s_BindingsWorld->GetComponent<SwitchComponent>(static_cast<Entity>(id));
    return sc ? sc->isActive : false;
}

static void Switch_SetActive(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<SwitchComponent>(static_cast<Entity>(id));
    if (sc) sc->isActive = val;
}

static i32 Switch_GetLinkedCount(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* sc = s_BindingsWorld->GetComponent<SwitchComponent>(static_cast<Entity>(id));
    return sc ? static_cast<i32>(sc->linkedEntities.size()) : 0;
}

static u64 Switch_GetLinkedEntity(u64 id, i32 index) {
    if (!s_BindingsWorld) return 0;
    auto* sc = s_BindingsWorld->GetComponent<SwitchComponent>(static_cast<Entity>(id));
    if (!sc || index < 0 || index >= static_cast<i32>(sc->linkedEntities.size())) return 0;
    return static_cast<u64>(sc->linkedEntities[index]);
}

static std::string Switch_GetPrompt(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* sc = s_BindingsWorld->GetComponent<SwitchComponent>(static_cast<Entity>(id));
    return sc ? sc->promptText : "";
}

// ============================================================================
// GoalZone component get/set
// ============================================================================

static bool HasComponent_GoalZone(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<GoalZoneComponent>(static_cast<Entity>(id));
}

static bool GoalZone_IsSatisfied(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* gz = s_BindingsWorld->GetComponent<GoalZoneComponent>(static_cast<Entity>(id));
    return gz ? gz->isSatisfied : false;
}

static std::string GoalZone_GetRequiredTag(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* gz = s_BindingsWorld->GetComponent<GoalZoneComponent>(static_cast<Entity>(id));
    return gz ? gz->requiredTag : "";
}

static i32 GoalZone_GetGoalGroup(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* gz = s_BindingsWorld->GetComponent<GoalZoneComponent>(static_cast<Entity>(id));
    return gz ? gz->goalGroup : 0;
}

static std::string GoalZone_GetNextScene(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* gz = s_BindingsWorld->GetComponent<GoalZoneComponent>(static_cast<Entity>(id));
    return gz ? gz->nextScene : "";
}

// ============================================================================
// Conveyor component get/set
// ============================================================================

static bool HasComponent_Conveyor(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<ConveyorComponent>(static_cast<Entity>(id));
}

static Vector3 Conveyor_GetDirection(u64 id) {
    if (!s_BindingsWorld) return Vector3(1, 0, 0);
    auto* cc = s_BindingsWorld->GetComponent<ConveyorComponent>(static_cast<Entity>(id));
    return cc ? cc->direction : Vector3(1, 0, 0);
}

static void Conveyor_SetDirection(u64 id, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<ConveyorComponent>(static_cast<Entity>(id));
    if (cc) cc->direction = Vector3(x, y, z);
}

static f32 Conveyor_GetSpeed(u64 id) {
    if (!s_BindingsWorld) return 3.0f;
    auto* cc = s_BindingsWorld->GetComponent<ConveyorComponent>(static_cast<Entity>(id));
    return cc ? cc->speed : 3.0f;
}

static void Conveyor_SetSpeed(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<ConveyorComponent>(static_cast<Entity>(id));
    if (cc) cc->speed = val;
}

static bool Conveyor_IsActive(u64 id) {
    if (!s_BindingsWorld) return true;
    auto* cc = s_BindingsWorld->GetComponent<ConveyorComponent>(static_cast<Entity>(id));
    return cc ? cc->isActive : true;
}

static void Conveyor_SetActive(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* cc = s_BindingsWorld->GetComponent<ConveyorComponent>(static_cast<Entity>(id));
    if (cc) cc->isActive = val;
}

// ============================================================================
// Teleporter component get/set
// ============================================================================

static bool HasComponent_Teleporter(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<TeleporterComponent>(static_cast<Entity>(id));
}

static Vector3 Teleporter_GetDestination(u64 id) {
    if (!s_BindingsWorld) return Vector3();
    auto* tp = s_BindingsWorld->GetComponent<TeleporterComponent>(static_cast<Entity>(id));
    return tp ? tp->targetPosition : Vector3();
}

static void Teleporter_SetDestination(u64 id, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    auto* tp = s_BindingsWorld->GetComponent<TeleporterComponent>(static_cast<Entity>(id));
    if (tp) tp->targetPosition = Vector3(x, y, z);
}

static f32 Teleporter_GetCooldown(u64 id) {
    if (!s_BindingsWorld) return 1.0f;
    auto* tp = s_BindingsWorld->GetComponent<TeleporterComponent>(static_cast<Entity>(id));
    return tp ? tp->cooldown : 1.0f;
}

static void Teleporter_SetCooldown(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* tp = s_BindingsWorld->GetComponent<TeleporterComponent>(static_cast<Entity>(id));
    if (tp) tp->cooldown = val;
}

static bool Teleporter_GetPreserveVelocity(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* tp = s_BindingsWorld->GetComponent<TeleporterComponent>(static_cast<Entity>(id));
    return tp ? tp->preserveVelocity : false;
}

static void Teleporter_SetPreserveVelocity(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* tp = s_BindingsWorld->GetComponent<TeleporterComponent>(static_cast<Entity>(id));
    if (tp) tp->preserveVelocity = val;
}

// ============================================================================
// MovingPlatform component get/set
// ============================================================================

static bool HasComponent_MovingPlatform(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<MovingPlatformComponent>(static_cast<Entity>(id));
}

static f32 MovingPlatform_GetSpeed(u64 id) {
    if (!s_BindingsWorld) return 2.0f;
    auto* mp = s_BindingsWorld->GetComponent<MovingPlatformComponent>(static_cast<Entity>(id));
    return mp ? mp->speed : 2.0f;
}

static void MovingPlatform_SetSpeed(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* mp = s_BindingsWorld->GetComponent<MovingPlatformComponent>(static_cast<Entity>(id));
    if (mp) mp->speed = val;
}

static bool MovingPlatform_IsMoving(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* mp = s_BindingsWorld->GetComponent<MovingPlatformComponent>(static_cast<Entity>(id));
    return mp ? mp->isMoving : false;
}

static void MovingPlatform_SetMoving(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* mp = s_BindingsWorld->GetComponent<MovingPlatformComponent>(static_cast<Entity>(id));
    if (mp) mp->isMoving = val;
}

static i32 MovingPlatform_GetWaypointCount(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* mp = s_BindingsWorld->GetComponent<MovingPlatformComponent>(static_cast<Entity>(id));
    return mp ? static_cast<i32>(mp->waypoints.size()) : 0;
}

static f32 MovingPlatform_GetWaitTime(u64 id) {
    if (!s_BindingsWorld) return 1.0f;
    auto* mp = s_BindingsWorld->GetComponent<MovingPlatformComponent>(static_cast<Entity>(id));
    return mp ? mp->waitTime : 1.0f;
}

static void MovingPlatform_SetWaitTime(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* mp = s_BindingsWorld->GetComponent<MovingPlatformComponent>(static_cast<Entity>(id));
    if (mp) mp->waitTime = val;
}

// ============================================================================
// Damage component get/set (DamageZone equivalent)
// ============================================================================

static bool HasComponent_Damage(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<DamageComponent>(static_cast<Entity>(id));
}

static f32 Damage_GetDamage(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* dc = s_BindingsWorld->GetComponent<DamageComponent>(static_cast<Entity>(id));
    return dc ? dc->damage : 0.0f;
}

static void Damage_SetDamage(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* dc = s_BindingsWorld->GetComponent<DamageComponent>(static_cast<Entity>(id));
    if (dc) dc->damage = val;
}

static f32 Damage_GetKnockback(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* dc = s_BindingsWorld->GetComponent<DamageComponent>(static_cast<Entity>(id));
    return dc ? dc->knockbackForce : 0.0f;
}

static void Damage_SetKnockback(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* dc = s_BindingsWorld->GetComponent<DamageComponent>(static_cast<Entity>(id));
    if (dc) dc->knockbackForce = val;
}

static f32 Damage_GetInterval(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* dc = s_BindingsWorld->GetComponent<DamageComponent>(static_cast<Entity>(id));
    return dc ? dc->damageInterval : 0.0f;
}

static void Damage_SetInterval(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* dc = s_BindingsWorld->GetComponent<DamageComponent>(static_cast<Entity>(id));
    if (dc) dc->damageInterval = val;
}

// ============================================================================
// Resource component get/set (Stamina equivalent)
// ============================================================================

static bool HasComponent_Resource(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<ResourceComponent>(static_cast<Entity>(id));
}

static f32 Resource_GetValue(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* rc = s_BindingsWorld->GetComponent<ResourceComponent>(static_cast<Entity>(id));
    return rc ? rc->currentValue : 0.0f;
}

static void Resource_SetValue(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* rc = s_BindingsWorld->GetComponent<ResourceComponent>(static_cast<Entity>(id));
    if (rc) rc->currentValue = Math::Clamp(val, 0.0f, rc->maxValue);
}

static f32 Resource_GetMax(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* rc = s_BindingsWorld->GetComponent<ResourceComponent>(static_cast<Entity>(id));
    return rc ? rc->maxValue : 0.0f;
}

static void Resource_SetMax(u64 id, f32 val) {
    if (!s_BindingsWorld) return;
    auto* rc = s_BindingsWorld->GetComponent<ResourceComponent>(static_cast<Entity>(id));
    if (rc) rc->maxValue = val;
}

static f32 Resource_GetPercent(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* rc = s_BindingsWorld->GetComponent<ResourceComponent>(static_cast<Entity>(id));
    return rc ? rc->GetPercent() : 0.0f;
}

static bool Resource_TryConsume(u64 id, f32 amount) {
    if (!s_BindingsWorld) return false;
    auto* rc = s_BindingsWorld->GetComponent<ResourceComponent>(static_cast<Entity>(id));
    return rc ? rc->TryConsume(amount) : false;
}

static bool Resource_IsDepleted(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* rc = s_BindingsWorld->GetComponent<ResourceComponent>(static_cast<Entity>(id));
    return rc ? rc->depleted : false;
}

static std::string Resource_GetName(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* rc = s_BindingsWorld->GetComponent<ResourceComponent>(static_cast<Entity>(id));
    return rc ? rc->resourceName : "";
}

// ============================================================================
// LOD component access
// ============================================================================

static bool HasComponent_LOD(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<LODComponent>(static_cast<Entity>(id));
}

static i32 LOD_GetCurrentLOD(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* lod = s_BindingsWorld->GetComponent<LODComponent>(static_cast<Entity>(id));
    return lod ? lod->activeLOD : 0;
}

static i32 LOD_GetLevelCount(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* lod = s_BindingsWorld->GetComponent<LODComponent>(static_cast<Entity>(id));
    return lod ? lod->levelCount : 0;
}

static bool LOD_IsEnabled(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* lod = s_BindingsWorld->GetComponent<LODComponent>(static_cast<Entity>(id));
    return lod ? lod->enabled : false;
}

static void LOD_SetEnabled(u64 id, bool val) {
    if (!s_BindingsWorld) return;
    auto* lod = s_BindingsWorld->GetComponent<LODComponent>(static_cast<Entity>(id));
    if (lod) lod->enabled = val;
}

// ============================================================================
// Layer component access
// ============================================================================

static bool HasComponent_Layer(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<LayerComponent>(static_cast<Entity>(id));
}

static u32 Layer_GetLayer(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* lc = s_BindingsWorld->GetComponent<LayerComponent>(static_cast<Entity>(id));
    return lc ? lc->layer : 0;
}

static void Layer_SetLayer(u64 id, u32 val) {
    if (!s_BindingsWorld) return;
    auto* lc = s_BindingsWorld->GetComponent<LayerComponent>(static_cast<Entity>(id));
    if (lc) lc->layer = val;
}

static std::string Layer_GetName(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* lc = s_BindingsWorld->GetComponent<LayerComponent>(static_cast<Entity>(id));
    return lc ? lc->layerName : "";
}

// ============================================================================
// Notes component access
// ============================================================================

static bool HasComponent_Notes(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<NotesComponent>(static_cast<Entity>(id));
}

static std::string Notes_Get(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* nc = s_BindingsWorld->GetComponent<NotesComponent>(static_cast<Entity>(id));
    return nc ? nc->notes : "";
}

static void Notes_Set(u64 id, const std::string& val) {
    if (!s_BindingsWorld) return;
    auto* nc = s_BindingsWorld->GetComponent<NotesComponent>(static_cast<Entity>(id));
    if (nc) nc->notes = val;
}

// ============================================================================
// Tag component access
// ============================================================================

static bool HasComponent_Tag(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<TagComponent>(static_cast<Entity>(id));
}

static i32 Tag_GetCount(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* tc = s_BindingsWorld->GetComponent<TagComponent>(static_cast<Entity>(id));
    return tc ? static_cast<i32>(tc->tags.size()) : 0;
}

static std::string Tag_GetAt(u64 id, i32 index) {
    if (!s_BindingsWorld) return "";
    auto* tc = s_BindingsWorld->GetComponent<TagComponent>(static_cast<Entity>(id));
    if (!tc || index < 0 || index >= static_cast<i32>(tc->tags.size())) return "";
    return tc->tags[index];
}

static void Tag_Add(u64 id, const std::string& tag) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<TagComponent>(static_cast<Entity>(id));
    if (tc) tc->AddTag(tag);
}

static void Tag_Remove(u64 id, const std::string& tag) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<TagComponent>(static_cast<Entity>(id));
    if (tc) tc->RemoveTag(tag);
}

static bool Tag_Has(u64 id, const std::string& tag) {
    if (!s_BindingsWorld) return false;
    auto* tc = s_BindingsWorld->GetComponent<TagComponent>(static_cast<Entity>(id));
    return tc ? tc->HasTag(tag) : false;
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
// ============================================================================
// Camera2D component access
// ============================================================================

static void Camera2D_Shake(u64 id, f32 intensity, f32 duration) {
    if (!s_BindingsWorld) return;
    auto* cam = s_BindingsWorld->GetComponent<Camera2DBoundsComponent>(static_cast<Entity>(id));
    if (cam) cam->TriggerShake(intensity, duration);
}

static f32 Camera2D_GetZoom(u64 id) {
    if (!s_BindingsWorld) return 1.0f;
    auto* cam = s_BindingsWorld->GetComponent<Camera2DBoundsComponent>(static_cast<Entity>(id));
    return cam ? cam->currentZoom : 1.0f;
}

static void Camera2D_SetZoom(u64 id, f32 zoom) {
    if (!s_BindingsWorld) return;
    auto* cam = s_BindingsWorld->GetComponent<Camera2DBoundsComponent>(static_cast<Entity>(id));
    if (cam) cam->targetZoom = zoom;
}

static void Camera2D_AddTarget(u64 cameraId, u64 targetId) {
    if (!s_BindingsWorld) return;
    auto* cam = s_BindingsWorld->GetComponent<Camera2DBoundsComponent>(static_cast<Entity>(cameraId));
    if (cam) {
        Entity target = static_cast<Entity>(targetId);
        if (std::find(cam->additionalTargets.begin(), cam->additionalTargets.end(), target) == cam->additionalTargets.end()) {
            cam->additionalTargets.push_back(target);
        }
    }
}

static void Camera2D_RemoveTarget(u64 cameraId, u64 targetId) {
    if (!s_BindingsWorld) return;
    auto* cam = s_BindingsWorld->GetComponent<Camera2DBoundsComponent>(static_cast<Entity>(cameraId));
    if (cam) {
        Entity target = static_cast<Entity>(targetId);
        auto it = std::find(cam->additionalTargets.begin(), cam->additionalTargets.end(), target);
        if (it != cam->additionalTargets.end()) {
            cam->additionalTargets.erase(it);
        }
    }
}

static void Camera2D_ClearTargets(u64 id) {
    if (!s_BindingsWorld) return;
    auto* cam = s_BindingsWorld->GetComponent<Camera2DBoundsComponent>(static_cast<Entity>(id));
    if (cam) cam->additionalTargets.clear();
}

static void Camera2D_SetDeadZone(u64 id, f32 width, f32 height) {
    if (!s_BindingsWorld) return;
    auto* cam = s_BindingsWorld->GetComponent<Camera2DBoundsComponent>(static_cast<Entity>(id));
    if (cam) {
        cam->deadZoneSize.x = width;
        cam->deadZoneSize.y = height;
    }
}

static void Camera2D_SetLookAhead(u64 id, f32 distance, f32 smoothing) {
    if (!s_BindingsWorld) return;
    auto* cam = s_BindingsWorld->GetComponent<Camera2DBoundsComponent>(static_cast<Entity>(id));
    if (cam) {
        cam->lookAheadDistance = distance;
        cam->lookAheadSmoothing = smoothing;
    }
}

static void Camera2D_SetFollowTarget(u64 cameraId, u64 targetId) {
    if (!s_BindingsWorld) return;
    auto* cam = s_BindingsWorld->GetComponent<Camera2DBoundsComponent>(static_cast<Entity>(cameraId));
    if (cam) cam->followTarget = static_cast<Entity>(targetId);
}

static u64 Camera2D_GetFollowTarget(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* cam = s_BindingsWorld->GetComponent<Camera2DBoundsComponent>(static_cast<Entity>(id));
    return cam ? static_cast<u64>(cam->followTarget) : 0;
}

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
    AS_CHECK(engine->RegisterGlobalFunction("void Material_SetTransmission(uint64, float)", asFUNCTION(Material_SetTransmission), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Material_GetTransmission(uint64)", asFUNCTION(Material_GetTransmission), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Material_SetIOR(uint64, float)", asFUNCTION(Material_SetIOR), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Material_GetIOR(uint64)", asFUNCTION(Material_GetIOR), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Material_SetThickness(uint64, float)", asFUNCTION(Material_SetThickness), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Material_GetThickness(uint64)", asFUNCTION(Material_GetThickness), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Material_SetSSSIntensity(uint64, float)", asFUNCTION(Material_SetSSSIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Material_GetSSSIntensity(uint64)", asFUNCTION(Material_GetSSSIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Material_SetSSSRadius(uint64, float)", asFUNCTION(Material_SetSSSRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Material_GetSSSRadius(uint64)", asFUNCTION(Material_GetSSSRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Material_SetSSSColor(uint64, const Vector3 &in)", asFUNCTION(Material_SetSSSColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 Material_GetSSSColor(uint64)", asFUNCTION(Material_GetSSSColor), asCALL_CDECL));

    // Light
    AS_CHECK(engine->RegisterGlobalFunction("void Light_SetColor(uint64, const Vector3 &in)", asFUNCTION(Light_SetColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 Light_GetColor(uint64)", asFUNCTION(Light_GetColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Light_SetIntensity(uint64, float)", asFUNCTION(Light_SetIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Light_GetIntensity(uint64)", asFUNCTION(Light_GetIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Light_SetRange(uint64, float)", asFUNCTION(Light_SetRange), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Light_GetRange(uint64)", asFUNCTION(Light_GetRange), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Light_SetType(uint64, int)", asFUNCTION(Light_SetType), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Light_GetType(uint64)", asFUNCTION(Light_GetType), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Light_SetCastShadows(uint64, bool)", asFUNCTION(Light_SetCastShadows), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Light_GetCastShadows(uint64)", asFUNCTION(Light_GetCastShadows), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Light_SetSpotAngles(uint64, float, float)", asFUNCTION(Light_SetSpotAngles), asCALL_CDECL));

    // Camera
    AS_CHECK(engine->RegisterGlobalFunction("void Camera_SetFOV(uint64, float)", asFUNCTION(Camera_SetFOV), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Camera_GetFOV(uint64)", asFUNCTION(Camera_GetFOV), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera_ApplyPreset(uint64, int)", asFUNCTION(Camera_ApplyPreset), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Camera_GetPresetName(int)", asFUNCTION(Camera_GetPresetName), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera_SetOrthoSize(uint64, float)", asFUNCTION(Camera_SetOrthoSize), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Camera_GetOrthoSize(uint64)", asFUNCTION(Camera_GetOrthoSize), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera_SetNearFar(uint64, float, float)", asFUNCTION(Camera_SetNearFar), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera_SetProjectionType(uint64, int)", asFUNCTION(Camera_SetProjectionType), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Camera_GetProjectionType(uint64)", asFUNCTION(Camera_GetProjectionType), asCALL_CDECL));

    // AudioSource
    AS_CHECK(engine->RegisterGlobalFunction("void AudioSource_Play(uint64)", asFUNCTION(AudioSource_Play), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AudioSource_Stop(uint64)", asFUNCTION(AudioSource_Stop), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AudioSource_SetClip(uint64, const string &in)", asFUNCTION(AudioSource_SetClip), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AudioSource_SetVolume(uint64, float)", asFUNCTION(AudioSource_SetVolume), asCALL_CDECL));

    // Animator
    AS_CHECK(engine->RegisterGlobalFunction("void Animator_Play(uint64, const string &in)", asFUNCTION(Animator_Play), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Animator_SetSpeed(uint64, float)", asFUNCTION(Animator_SetSpeed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Animator_Stop(uint64)", asFUNCTION(Animator_Stop), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Animator_Pause(uint64)", asFUNCTION(Animator_Pause), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Animator_Resume(uint64)", asFUNCTION(Animator_Resume), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Animator_IsPlaying(uint64)", asFUNCTION(Animator_IsPlaying), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Animator_GetCurrentAnimation(uint64)", asFUNCTION(Animator_GetCurrentAnimation), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Animator_GetSpeed(uint64)", asFUNCTION(Animator_GetSpeed), asCALL_CDECL));

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

    // Camera2D
    AS_CHECK(engine->RegisterGlobalFunction("void Camera2D_Shake(uint64, float, float)", asFUNCTION(Camera2D_Shake), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Camera2D_GetZoom(uint64)", asFUNCTION(Camera2D_GetZoom), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera2D_SetZoom(uint64, float)", asFUNCTION(Camera2D_SetZoom), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera2D_AddTarget(uint64, uint64)", asFUNCTION(Camera2D_AddTarget), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera2D_RemoveTarget(uint64, uint64)", asFUNCTION(Camera2D_RemoveTarget), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera2D_ClearTargets(uint64)", asFUNCTION(Camera2D_ClearTargets), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera2D_SetDeadZone(uint64, float, float)", asFUNCTION(Camera2D_SetDeadZone), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera2D_SetLookAhead(uint64, float, float)", asFUNCTION(Camera2D_SetLookAhead), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Camera2D_SetFollowTarget(uint64, uint64)", asFUNCTION(Camera2D_SetFollowTarget), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint64 Camera2D_GetFollowTarget(uint64)", asFUNCTION(Camera2D_GetFollowTarget), asCALL_CDECL));

    // Rigidbody
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 Rigidbody_GetVelocity(uint64)", asFUNCTION(Rigidbody_GetVelocity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rigidbody_SetVelocity(uint64, float, float, float)", asFUNCTION(Rigidbody_SetVelocity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 Rigidbody_GetAngularVelocity(uint64)", asFUNCTION(Rigidbody_GetAngularVelocity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rigidbody_SetAngularVelocity(uint64, float, float, float)", asFUNCTION(Rigidbody_SetAngularVelocity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Rigidbody_GetMass(uint64)", asFUNCTION(Rigidbody_GetMass), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rigidbody_SetMass(uint64, float)", asFUNCTION(Rigidbody_SetMass), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Rigidbody_GetUseGravity(uint64)", asFUNCTION(Rigidbody_GetUseGravity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rigidbody_SetUseGravity(uint64, bool)", asFUNCTION(Rigidbody_SetUseGravity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Rigidbody_IsKinematic(uint64)", asFUNCTION(Rigidbody_IsKinematic), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rigidbody_SetKinematic(uint64, bool)", asFUNCTION(Rigidbody_SetKinematic), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Rigidbody_GetDrag(uint64)", asFUNCTION(Rigidbody_GetDrag), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rigidbody_SetDrag(uint64, float)", asFUNCTION(Rigidbody_SetDrag), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Rigidbody_GetAngularDrag(uint64)", asFUNCTION(Rigidbody_GetAngularDrag), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rigidbody_SetAngularDrag(uint64, float)", asFUNCTION(Rigidbody_SetAngularDrag), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Rigidbody_IsGrounded(uint64)", asFUNCTION(Rigidbody_IsGrounded), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Rigidbody_GetGravityScale(uint64)", asFUNCTION(Rigidbody_GetGravityScale), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rigidbody_SetGravityScale(uint64, float)", asFUNCTION(Rigidbody_SetGravityScale), asCALL_CDECL));

    // BoxCollider
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 BoxCollider_GetSize(uint64)", asFUNCTION(BoxCollider_GetSize), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BoxCollider_SetSize(uint64, float, float, float)", asFUNCTION(BoxCollider_SetSize), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 BoxCollider_GetCenter(uint64)", asFUNCTION(BoxCollider_GetCenter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BoxCollider_SetCenter(uint64, float, float, float)", asFUNCTION(BoxCollider_SetCenter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool BoxCollider_IsTrigger(uint64)", asFUNCTION(BoxCollider_IsTrigger), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BoxCollider_SetTrigger(uint64, bool)", asFUNCTION(BoxCollider_SetTrigger), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint BoxCollider_GetCategoryBits(uint64)", asFUNCTION(BoxCollider_GetCategoryBits), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BoxCollider_SetCategoryBits(uint64, uint)", asFUNCTION(BoxCollider_SetCategoryBits), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint BoxCollider_GetCollisionMask(uint64)", asFUNCTION(BoxCollider_GetCollisionMask), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BoxCollider_SetCollisionMask(uint64, uint)", asFUNCTION(BoxCollider_SetCollisionMask), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float BoxCollider_GetFriction(uint64)", asFUNCTION(BoxCollider_GetFriction), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BoxCollider_SetFriction(uint64, float)", asFUNCTION(BoxCollider_SetFriction), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float BoxCollider_GetBounciness(uint64)", asFUNCTION(BoxCollider_GetBounciness), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BoxCollider_SetBounciness(uint64, float)", asFUNCTION(BoxCollider_SetBounciness), asCALL_CDECL));

    // SphereCollider
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_SphereCollider(uint64)", asFUNCTION(HasComponent_SphereCollider), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float SphereCollider_GetRadius(uint64)", asFUNCTION(SphereCollider_GetRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SphereCollider_SetRadius(uint64, float)", asFUNCTION(SphereCollider_SetRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 SphereCollider_GetCenter(uint64)", asFUNCTION(SphereCollider_GetCenter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SphereCollider_SetCenter(uint64, float, float, float)", asFUNCTION(SphereCollider_SetCenter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool SphereCollider_IsTrigger(uint64)", asFUNCTION(SphereCollider_IsTrigger), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SphereCollider_SetTrigger(uint64, bool)", asFUNCTION(SphereCollider_SetTrigger), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint SphereCollider_GetCategoryBits(uint64)", asFUNCTION(SphereCollider_GetCategoryBits), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SphereCollider_SetCategoryBits(uint64, uint)", asFUNCTION(SphereCollider_SetCategoryBits), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint SphereCollider_GetCollisionMask(uint64)", asFUNCTION(SphereCollider_GetCollisionMask), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SphereCollider_SetCollisionMask(uint64, uint)", asFUNCTION(SphereCollider_SetCollisionMask), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float SphereCollider_GetFriction(uint64)", asFUNCTION(SphereCollider_GetFriction), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SphereCollider_SetFriction(uint64, float)", asFUNCTION(SphereCollider_SetFriction), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float SphereCollider_GetBounciness(uint64)", asFUNCTION(SphereCollider_GetBounciness), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SphereCollider_SetBounciness(uint64, float)", asFUNCTION(SphereCollider_SetBounciness), asCALL_CDECL));

    // CapsuleCollider
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_CapsuleCollider(uint64)", asFUNCTION(HasComponent_CapsuleCollider), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float CapsuleCollider_GetRadius(uint64)", asFUNCTION(CapsuleCollider_GetRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void CapsuleCollider_SetRadius(uint64, float)", asFUNCTION(CapsuleCollider_SetRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float CapsuleCollider_GetHeight(uint64)", asFUNCTION(CapsuleCollider_GetHeight), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void CapsuleCollider_SetHeight(uint64, float)", asFUNCTION(CapsuleCollider_SetHeight), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 CapsuleCollider_GetCenter(uint64)", asFUNCTION(CapsuleCollider_GetCenter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void CapsuleCollider_SetCenter(uint64, float, float, float)", asFUNCTION(CapsuleCollider_SetCenter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool CapsuleCollider_IsTrigger(uint64)", asFUNCTION(CapsuleCollider_IsTrigger), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void CapsuleCollider_SetTrigger(uint64, bool)", asFUNCTION(CapsuleCollider_SetTrigger), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float CapsuleCollider_GetFriction(uint64)", asFUNCTION(CapsuleCollider_GetFriction), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void CapsuleCollider_SetFriction(uint64, float)", asFUNCTION(CapsuleCollider_SetFriction), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float CapsuleCollider_GetBounciness(uint64)", asFUNCTION(CapsuleCollider_GetBounciness), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void CapsuleCollider_SetBounciness(uint64, float)", asFUNCTION(CapsuleCollider_SetBounciness), asCALL_CDECL));

    // TriggerZone
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_TriggerZone(uint64)", asFUNCTION(HasComponent_TriggerZone), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int TriggerZone_GetShape(uint64)", asFUNCTION(TriggerZone_GetShape), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void TriggerZone_SetShape(uint64, int)", asFUNCTION(TriggerZone_SetShape), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 TriggerZone_GetBoxSize(uint64)", asFUNCTION(TriggerZone_GetBoxSize), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void TriggerZone_SetBoxSize(uint64, float, float, float)", asFUNCTION(TriggerZone_SetBoxSize), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float TriggerZone_GetSphereRadius(uint64)", asFUNCTION(TriggerZone_GetSphereRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void TriggerZone_SetSphereRadius(uint64, float)", asFUNCTION(TriggerZone_SetSphereRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool TriggerZone_GetTriggerOnce(uint64)", asFUNCTION(TriggerZone_GetTriggerOnce), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void TriggerZone_SetTriggerOnce(uint64, bool)", asFUNCTION(TriggerZone_SetTriggerOnce), asCALL_CDECL));

    // Interactable
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Interactable(uint64)", asFUNCTION(HasComponent_Interactable), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Interactable_GetPrompt(uint64)", asFUNCTION(Interactable_GetPrompt), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Interactable_SetPrompt(uint64, const string &in)", asFUNCTION(Interactable_SetPrompt), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Interactable_GetRange(uint64)", asFUNCTION(Interactable_GetRange), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Interactable_SetRange(uint64, float)", asFUNCTION(Interactable_SetRange), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Interactable_IsEnabled(uint64)", asFUNCTION(Interactable_IsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Interactable_SetEnabled(uint64, bool)", asFUNCTION(Interactable_SetEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Interactable_HasBeenUsed(uint64)", asFUNCTION(Interactable_HasBeenUsed), asCALL_CDECL));

    // Pickup
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Pickup(uint64)", asFUNCTION(HasComponent_Pickup), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Pickup_GetType(uint64)", asFUNCTION(Pickup_GetType), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Pickup_SetType(uint64, int)", asFUNCTION(Pickup_SetType), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Pickup_GetValue(uint64)", asFUNCTION(Pickup_GetValue), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Pickup_SetValue(uint64, float)", asFUNCTION(Pickup_SetValue), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Pickup_GetCustomId(uint64)", asFUNCTION(Pickup_GetCustomId), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Pickup_SetCustomId(uint64, const string &in)", asFUNCTION(Pickup_SetCustomId), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Pickup_GetRange(uint64)", asFUNCTION(Pickup_GetRange), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Pickup_SetRange(uint64, float)", asFUNCTION(Pickup_SetRange), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Pickup_GetDestroyOnPickup(uint64)", asFUNCTION(Pickup_GetDestroyOnPickup), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Pickup_SetDestroyOnPickup(uint64, bool)", asFUNCTION(Pickup_SetDestroyOnPickup), asCALL_CDECL));

    // Inventory
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Inventory(uint64)", asFUNCTION(HasComponent_Inventory), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Inventory_GetItemCount(uint64, const string &in)", asFUNCTION(Inventory_GetItemCount), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Inventory_AddItem(uint64, const string &in, int)", asFUNCTION(Inventory_AddItem), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Inventory_RemoveItem(uint64, const string &in, int)", asFUNCTION(Inventory_RemoveItem), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Inventory_HasItem(uint64, const string &in)", asFUNCTION(Inventory_HasItem), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Inventory_Clear(uint64)", asFUNCTION(Inventory_Clear), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Inventory_GetCoins(uint64)", asFUNCTION(Inventory_GetCoins), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Inventory_SetCoins(uint64, int)", asFUNCTION(Inventory_SetCoins), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Inventory_GetGems(uint64)", asFUNCTION(Inventory_GetGems), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Inventory_SetGems(uint64, int)", asFUNCTION(Inventory_SetGems), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Inventory_HasKey(uint64, const string &in)", asFUNCTION(Inventory_HasKey), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Inventory_AddKey(uint64, const string &in)", asFUNCTION(Inventory_AddKey), asCALL_CDECL));

    // Timer
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Timer(uint64)", asFUNCTION(HasComponent_Timer), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Timer_GetDuration(uint64)", asFUNCTION(Timer_GetDuration), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Timer_SetDuration(uint64, float)", asFUNCTION(Timer_SetDuration), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Timer_GetElapsed(uint64)", asFUNCTION(Timer_GetElapsed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Timer_SetElapsed(uint64, float)", asFUNCTION(Timer_SetElapsed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Timer_IsRunning(uint64)", asFUNCTION(Timer_IsRunning), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Timer_SetRunning(uint64, bool)", asFUNCTION(Timer_SetRunning), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Timer_GetLoop(uint64)", asFUNCTION(Timer_GetLoop), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Timer_SetLoop(uint64, bool)", asFUNCTION(Timer_SetLoop), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Timer_GetProgress(uint64)", asFUNCTION(Timer_GetProgress), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Timer_GetRemaining(uint64)", asFUNCTION(Timer_GetRemaining), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Timer_IsComplete(uint64)", asFUNCTION(Timer_IsComplete), asCALL_CDECL));

    // Extended Health
    AS_CHECK(engine->RegisterGlobalFunction("float Health_GetShield(uint64)", asFUNCTION(Health_GetShield), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Health_SetShield(uint64, float)", asFUNCTION(Health_SetShield), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Health_SetMaxHealth(uint64, float)", asFUNCTION(Health_SetMaxHealth), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Health_IsDead(uint64)", asFUNCTION(Health_IsDead), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Health_IsInvulnerable(uint64)", asFUNCTION(Health_IsInvulnerable), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Health_SetInvulnerable(uint64, bool)", asFUNCTION(Health_SetInvulnerable), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Health_GetPercent(uint64)", asFUNCTION(Health_GetPercent), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Health_Heal(uint64, float)", asFUNCTION(Health_Heal), asCALL_CDECL));

    // Lock
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Lock(uint64)", asFUNCTION(HasComponent_Lock), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Lock_IsLocked(uint64)", asFUNCTION(Lock_IsLocked), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Lock_SetLocked(uint64, bool)", asFUNCTION(Lock_SetLocked), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Lock_GetRequiredKey(uint64)", asFUNCTION(Lock_GetRequiredKey), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Lock_IsOpen(uint64)", asFUNCTION(Lock_IsOpen), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Lock_SetOpen(uint64, bool)", asFUNCTION(Lock_SetOpen), asCALL_CDECL));

    // Switch
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Switch(uint64)", asFUNCTION(HasComponent_Switch), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Switch_IsActive(uint64)", asFUNCTION(Switch_IsActive), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Switch_SetActive(uint64, bool)", asFUNCTION(Switch_SetActive), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Switch_GetLinkedCount(uint64)", asFUNCTION(Switch_GetLinkedCount), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint64 Switch_GetLinkedEntity(uint64, int)", asFUNCTION(Switch_GetLinkedEntity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Switch_GetPrompt(uint64)", asFUNCTION(Switch_GetPrompt), asCALL_CDECL));

    // GoalZone
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_GoalZone(uint64)", asFUNCTION(HasComponent_GoalZone), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool GoalZone_IsSatisfied(uint64)", asFUNCTION(GoalZone_IsSatisfied), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string GoalZone_GetRequiredTag(uint64)", asFUNCTION(GoalZone_GetRequiredTag), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int GoalZone_GetGoalGroup(uint64)", asFUNCTION(GoalZone_GetGoalGroup), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string GoalZone_GetNextScene(uint64)", asFUNCTION(GoalZone_GetNextScene), asCALL_CDECL));

    // Conveyor
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Conveyor(uint64)", asFUNCTION(HasComponent_Conveyor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 Conveyor_GetDirection(uint64)", asFUNCTION(Conveyor_GetDirection), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Conveyor_SetDirection(uint64, float, float, float)", asFUNCTION(Conveyor_SetDirection), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Conveyor_GetSpeed(uint64)", asFUNCTION(Conveyor_GetSpeed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Conveyor_SetSpeed(uint64, float)", asFUNCTION(Conveyor_SetSpeed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Conveyor_IsActive(uint64)", asFUNCTION(Conveyor_IsActive), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Conveyor_SetActive(uint64, bool)", asFUNCTION(Conveyor_SetActive), asCALL_CDECL));

    // Teleporter
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Teleporter(uint64)", asFUNCTION(HasComponent_Teleporter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 Teleporter_GetDestination(uint64)", asFUNCTION(Teleporter_GetDestination), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Teleporter_SetDestination(uint64, float, float, float)", asFUNCTION(Teleporter_SetDestination), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Teleporter_GetCooldown(uint64)", asFUNCTION(Teleporter_GetCooldown), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Teleporter_SetCooldown(uint64, float)", asFUNCTION(Teleporter_SetCooldown), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Teleporter_GetPreserveVelocity(uint64)", asFUNCTION(Teleporter_GetPreserveVelocity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Teleporter_SetPreserveVelocity(uint64, bool)", asFUNCTION(Teleporter_SetPreserveVelocity), asCALL_CDECL));

    // MovingPlatform
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_MovingPlatform(uint64)", asFUNCTION(HasComponent_MovingPlatform), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float MovingPlatform_GetSpeed(uint64)", asFUNCTION(MovingPlatform_GetSpeed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void MovingPlatform_SetSpeed(uint64, float)", asFUNCTION(MovingPlatform_SetSpeed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool MovingPlatform_IsMoving(uint64)", asFUNCTION(MovingPlatform_IsMoving), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void MovingPlatform_SetMoving(uint64, bool)", asFUNCTION(MovingPlatform_SetMoving), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int MovingPlatform_GetWaypointCount(uint64)", asFUNCTION(MovingPlatform_GetWaypointCount), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float MovingPlatform_GetWaitTime(uint64)", asFUNCTION(MovingPlatform_GetWaitTime), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void MovingPlatform_SetWaitTime(uint64, float)", asFUNCTION(MovingPlatform_SetWaitTime), asCALL_CDECL));

    // Damage
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Damage(uint64)", asFUNCTION(HasComponent_Damage), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Damage_GetDamage(uint64)", asFUNCTION(Damage_GetDamage), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Damage_SetDamage(uint64, float)", asFUNCTION(Damage_SetDamage), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Damage_GetKnockback(uint64)", asFUNCTION(Damage_GetKnockback), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Damage_SetKnockback(uint64, float)", asFUNCTION(Damage_SetKnockback), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Damage_GetInterval(uint64)", asFUNCTION(Damage_GetInterval), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Damage_SetInterval(uint64, float)", asFUNCTION(Damage_SetInterval), asCALL_CDECL));

    // Resource (Stamina)
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Resource(uint64)", asFUNCTION(HasComponent_Resource), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Resource_GetValue(uint64)", asFUNCTION(Resource_GetValue), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Resource_SetValue(uint64, float)", asFUNCTION(Resource_SetValue), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Resource_GetMax(uint64)", asFUNCTION(Resource_GetMax), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Resource_SetMax(uint64, float)", asFUNCTION(Resource_SetMax), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Resource_GetPercent(uint64)", asFUNCTION(Resource_GetPercent), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Resource_TryConsume(uint64, float)", asFUNCTION(Resource_TryConsume), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Resource_IsDepleted(uint64)", asFUNCTION(Resource_IsDepleted), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Resource_GetName(uint64)", asFUNCTION(Resource_GetName), asCALL_CDECL));

    // LOD
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_LOD(uint64)", asFUNCTION(HasComponent_LOD), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int LOD_GetCurrentLOD(uint64)", asFUNCTION(LOD_GetCurrentLOD), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int LOD_GetLevelCount(uint64)", asFUNCTION(LOD_GetLevelCount), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool LOD_IsEnabled(uint64)", asFUNCTION(LOD_IsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void LOD_SetEnabled(uint64, bool)", asFUNCTION(LOD_SetEnabled), asCALL_CDECL));

    // Layer
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Layer(uint64)", asFUNCTION(HasComponent_Layer), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint Layer_GetLayer(uint64)", asFUNCTION(Layer_GetLayer), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Layer_SetLayer(uint64, uint)", asFUNCTION(Layer_SetLayer), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Layer_GetName(uint64)", asFUNCTION(Layer_GetName), asCALL_CDECL));

    // Notes
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Notes(uint64)", asFUNCTION(HasComponent_Notes), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Notes_Get(uint64)", asFUNCTION(Notes_Get), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Notes_Set(uint64, const string &in)", asFUNCTION(Notes_Set), asCALL_CDECL));

    // Tag
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Tag(uint64)", asFUNCTION(HasComponent_Tag), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Tag_GetCount(uint64)", asFUNCTION(Tag_GetCount), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Tag_GetAt(uint64, int)", asFUNCTION(Tag_GetAt), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Tag_Add(uint64, const string &in)", asFUNCTION(Tag_Add), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Tag_Remove(uint64, const string &in)", asFUNCTION(Tag_Remove), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Tag_Has(uint64, const string &in)", asFUNCTION(Tag_Has), asCALL_CDECL));

    // Tilemap
    AS_CHECK(engine->RegisterGlobalFunction("bool HasComponent_Tilemap(uint64)", asFUNCTION(HasComponent_Tilemap), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Tilemap_GetTile(uint64, int, int)", asFUNCTION(Tilemap_GetTile), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Tilemap_SetTile(uint64, int, int, int)", asFUNCTION(Tilemap_SetTile), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Tilemap_GetWidth(uint64)", asFUNCTION(Tilemap_GetWidth), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Tilemap_GetHeight(uint64)", asFUNCTION(Tilemap_GetHeight), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
