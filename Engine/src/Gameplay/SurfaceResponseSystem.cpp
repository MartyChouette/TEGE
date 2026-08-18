#include "Enjin/Gameplay/SurfaceResponseSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/GPUParticleEmitter.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"     // RigidbodyComponent
#include "Enjin/Physics/IPhysicsBackend.h"
#include <cmath>

namespace Enjin {
namespace Gameplay {

using namespace Enjin::ECS;

// Auto footstep cadence: a step every this many world units travelled. Running
// covers it faster, so steps naturally speed up. ~human stride.
static constexpr f32 kStepLength = 1.7f;
static constexpr f32 kWalkMinSpeed = 0.4f;   // horizontal speed below this = standing
static constexpr f32 kGroundProbe = 1.5f;    // how far below the walker to look for ground
static constexpr f32 kEventSuppress = 0.25f; // mute auto steps this long after an anim event

Audio::AudioClipHandle SurfaceResponseSystem::GetClip(const std::string& path) {
    if (path.empty() || !m_Audio) return Audio::INVALID_AUDIO_CLIP;
    auto it = m_ClipCache.find(path);
    if (it != m_ClipCache.end()) return it->second;
    Audio::AudioClipHandle clip = m_Audio->LoadClip(path);
    m_ClipCache[path] = clip;
    return clip;
}

// Play the given surface entity's footstep/impact sound + particle at a point.
void SurfaceResponseSystem::EmitSurface(World* world, Entity surface, const Math::Vector3& at,
                                        const Math::Vector3& normal, bool impact) {
    if (!world) return;
    auto* mat = world->GetComponent<MaterialComponent>(surface);
    if (!mat) return;

    const std::string& sound = impact ? mat->impactSound : mat->footstepSound;
    if (!sound.empty() && m_Audio) {
        Audio::AudioClipHandle clip = GetClip(sound);
        if (clip != Audio::INVALID_AUDIO_CLIP)
            m_Audio->PlayOneShot3D(clip, at, mat->footstepVolume);
    }

    // Burst the surface particle upward along the contact normal. Generic burst
    // for now (desktop GPU path); per-preset looks are a follow-up.
    if (mat->surfaceParticle != 0 && m_Render) {
        Math::Vector3 dir = normal;
        if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) dir = Math::Vector3(0, 1, 0);
        u32 count = impact ? 20u : 12u;
        m_Render->SpawnGPUParticles(count, at, dir);
    }
}

void SurfaceResponseSystem::EmitFootstep(World* world, Entity walker, const Math::Vector3& footPos) {
    if (!m_Physics) return;
    Physics::RaycastHit hit;
    if (!m_Physics->CheckGround(footPos, kGroundProbe, hit, 0xFFFFFFFF, walker)) return;
    if (!hit.hit || hit.entity == 0) return;
    EmitSurface(world, hit.entity, hit.point, hit.normal, /*impact=*/false);
}

void SurfaceResponseSystem::OnFootstepEvent(World* world, Entity entity) {
    auto* xf = world ? world->GetComponent<TransformComponent>(entity) : nullptr;
    if (!xf) return;
    // Fire immediately from the animation event, and mute the auto step briefly.
    WalkerState& st = m_State[EntityIndex(entity)];
    st.distanceSinceStep = 0.0f;
    st.eventSuppress = kEventSuppress;
    EmitFootstep(world, entity, xf->position);
}

void SurfaceResponseSystem::Update(World* world, f32 deltaTime) {
    m_Clock += deltaTime;

    // GPU particle impact sounds: the sim reports collider strikes (a couple of
    // frames late, inaudible); play the owning emitter's impact sound there.
    if (m_Render && m_Audio && world) {
        std::vector<ECS::RenderSystem::ParticleImpact> taken = m_Render->TakeParticleImpacts();
        for (const auto& hit : taken) {
            if (hit.emitter == INVALID_ENTITY) continue;
            auto* em = world->GetComponent<GPUParticleEmitterComponent>(hit.emitter);
            if (!em || em->impactSound.empty() || hit.speed < em->impactMinSpeed) continue;
            f32& next = m_ImpactNextAllowed[EntityIndex(hit.emitter)];
            if (m_Clock < next) continue;
            Audio::AudioClipHandle clip = GetClip(em->impactSound);
            if (clip == Audio::INVALID_AUDIO_CLIP) continue;
            m_Audio->PlayOneShot3D(clip, hit.position, em->impactVolume);
            next = m_Clock + em->impactCooldown;
        }
    }

    if (!world) return;

    // --- Footsteps: grounded, moving rigidbodies step on the surface below them ---
    for (Entity e : world->GetEntitiesWithComponent<RigidbodyComponent>()) {
        auto* rb = world->GetComponent<RigidbodyComponent>(e);
        auto* xf = world->GetComponent<TransformComponent>(e);
        if (!rb || !xf) continue;

        WalkerState& st = m_State[EntityIndex(e)];
        if (st.eventSuppress > 0.0f) st.eventSuppress -= deltaTime;

        f32 hspeed = std::sqrt(rb->velocity.x * rb->velocity.x + rb->velocity.z * rb->velocity.z);
        if (!rb->isGrounded || hspeed < kWalkMinSpeed) {
            st.distanceSinceStep = 0.0f;   // no steps while airborne or standing
            st.lastPos = xf->position;
            st.haveLast = true;
            continue;
        }

        if (st.haveLast) {
            f32 dx = xf->position.x - st.lastPos.x;
            f32 dz = xf->position.z - st.lastPos.z;
            st.distanceSinceStep += std::sqrt(dx * dx + dz * dz);
        }
        st.lastPos = xf->position;
        st.haveLast = true;

        if (st.eventSuppress <= 0.0f && st.distanceSinceStep >= kStepLength) {
            st.distanceSinceStep = 0.0f;
            EmitFootstep(world, e, xf->position);
        }
    }

    // --- Impacts: collision events strike a surface hard enough to be heard ---
    if (m_Physics) {
        const auto& events = m_Physics->GetPendingCollisionEvents();
        for (const auto& ev : events) {
            if (ev.type != Physics::CollisionEvent::Type::Enter || ev.isTrigger) continue;

            // Approach speed from the two bodies' velocities (0 if no rigidbody).
            Math::Vector3 va{0, 0, 0}, vb{0, 0, 0};
            if (auto* ra = world->GetComponent<RigidbodyComponent>(ev.entityA)) va = ra->velocity;
            if (auto* rb = world->GetComponent<RigidbodyComponent>(ev.entityB)) vb = rb->velocity;
            f32 rx = va.x - vb.x, ry = va.y - vb.y, rz = va.z - vb.z;
            f32 approach = std::sqrt(rx * rx + ry * ry + rz * rz);

            // Fire the impact for whichever surface has an impact sound, gated by its
            // threshold. Prefer entityB (usually the struck/static surface).
            for (Entity surf : { ev.entityB, ev.entityA }) {
                auto* mat = world->GetComponent<MaterialComponent>(surf);
                if (!mat || mat->impactSound.empty()) continue;
                if (approach < mat->impactThreshold) break;   // too gentle to hear
                EmitSurface(world, surf, ev.contactPoint, ev.normal, /*impact=*/true);
                break;   // one impact sound per collision
            }
        }
    }
}

} // namespace Gameplay
} // namespace Enjin
