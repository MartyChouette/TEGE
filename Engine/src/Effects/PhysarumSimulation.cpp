#include "Enjin/Effects/PhysarumSimulation.h"
#include <algorithm>
#include <cmath>
#include <chrono>

namespace Enjin {
namespace Effects {

// ============================================================================
// Constants
// ============================================================================

static constexpr f32 PI = 3.14159265358979323846f;
static constexpr f32 DEG_TO_RAD = PI / 180.0f;

// ============================================================================
// PRNG (xorshift32)
// ============================================================================

f32 PhysarumSimulation::RandomFloat() {
    m_RngState ^= m_RngState << 13;
    m_RngState ^= m_RngState >> 17;
    m_RngState ^= m_RngState << 5;
    return static_cast<f32>(m_RngState) / static_cast<f32>(0xFFFFFFFFu);
}

// ============================================================================
// Initialization
// ============================================================================

void PhysarumSimulation::Initialize(const PhysarumConfig& config) {
    m_Config = config;

    // Clamp resolution to sane bounds
    m_Config.width = std::max(16u, std::min(4096u, m_Config.width));
    m_Config.height = std::max(16u, std::min(4096u, m_Config.height));
    m_Config.agentCount = std::max(1u, std::min(1000000u, m_Config.agentCount));

    // Seed PRNG
    if (m_Config.seed != 0) {
        m_RngState = m_Config.seed;
    } else {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        m_RngState = static_cast<u32>(
            std::chrono::duration_cast<std::chrono::microseconds>(now).count() & 0xFFFFFFFF);
        if (m_RngState == 0) m_RngState = 12345;
    }

    u32 w = m_Config.width;
    u32 h = m_Config.height;

    // Allocate trail maps (zeroed)
    m_State.trailMap.assign(static_cast<usize>(w) * h, 0.0f);
    m_TempTrail.assign(static_cast<usize>(w) * h, 0.0f);
    m_State.width = w;
    m_State.height = h;
    m_State.stepCount = 0;

    // Create agents at random positions with random angles
    m_State.agents.resize(m_Config.agentCount);
    for (u32 i = 0; i < m_Config.agentCount; ++i) {
        PhysarumAgent& agent = m_State.agents[i];
        agent.x = RandomFloat() * static_cast<f32>(w);
        agent.y = RandomFloat() * static_cast<f32>(h);
        agent.angle = RandomFloat() * 2.0f * PI;
    }

    // Apply preset seeding patterns
    if (m_Config.preset != PhysarumPreset::Custom) {
        PhysarumConfig presetCfg = GetPresetConfig(m_Config.preset);
        m_Config.sensorAngle = presetCfg.sensorAngle;
        m_Config.sensorDistance = presetCfg.sensorDistance;
        m_Config.turnSpeed = presetCfg.turnSpeed;
        m_Config.moveSpeed = presetCfg.moveSpeed;
        m_Config.trailDecay = presetCfg.trailDecay;
        m_Config.trailDeposit = presetCfg.trailDeposit;
        m_Config.diffuseRadius = presetCfg.diffuseRadius;
    }
}

// ============================================================================
// Update / Step
// ============================================================================

void PhysarumSimulation::Update(f32 /*dt*/) {
    for (u32 s = 0; s < m_Config.stepsPerFrame; ++s) {
        Step();
    }
}

void PhysarumSimulation::Step() {
    if (m_State.agents.empty()) return;

    u32 w = m_State.width;
    u32 h = m_State.height;
    if (w == 0 || h == 0) return;

    f32 sensorAngleRad = m_Config.sensorAngle * DEG_TO_RAD;
    f32 turnSpeedRad = m_Config.turnSpeed * DEG_TO_RAD;

    for (u32 i = 0; i < static_cast<u32>(m_State.agents.size()); ++i) {
        PhysarumAgent& agent = m_State.agents[i];

        // 1. Sense: sample trail at three sensor positions
        f32 senseLeft = Sense(agent, -sensorAngleRad);
        f32 senseFront = Sense(agent, 0.0f);
        f32 senseRight = Sense(agent, sensorAngleRad);

        // 2. Turn based on sensor readings
        if (senseFront > senseLeft && senseFront > senseRight) {
            // Front is strongest — go straight (no turn)
        } else if (senseFront < senseLeft && senseFront < senseRight) {
            // Both sides stronger than front — random turn
            f32 r = RandomFloat();
            if (r < 0.5f) {
                agent.angle -= turnSpeedRad;
            } else {
                agent.angle += turnSpeedRad;
            }
        } else if (senseLeft > senseRight) {
            // Left is stronger — turn left
            agent.angle -= turnSpeedRad;
        } else if (senseRight > senseLeft) {
            // Right is stronger — turn right
            agent.angle += turnSpeedRad;
        }
        // If left == right and front >= both, go straight (already handled)

        // 3. Move forward
        f32 newX = agent.x + std::cos(agent.angle) * m_Config.moveSpeed;
        f32 newY = agent.y + std::sin(agent.angle) * m_Config.moveSpeed;

        // 4. Boundary handling
        if (m_Config.wrapEdges) {
            newX = WrapX(newX);
            newY = WrapY(newY);
        } else {
            // Bounce: if out of bounds, pick a random angle and stay in place
            if (newX < 0.0f || newX >= static_cast<f32>(w) ||
                newY < 0.0f || newY >= static_cast<f32>(h)) {
                agent.angle = RandomFloat() * 2.0f * PI;
                newX = std::max(0.0f, std::min(static_cast<f32>(w) - 0.001f, agent.x));
                newY = std::max(0.0f, std::min(static_cast<f32>(h) - 0.001f, agent.y));
            }
        }

        agent.x = newX;
        agent.y = newY;

        // 5. Deposit trail at new position
        u32 px = static_cast<u32>(agent.x);
        u32 py = static_cast<u32>(agent.y);
        if (px < w && py < h) {
            m_State.trailMap[static_cast<usize>(py) * w + px] += m_Config.trailDeposit;
        }
    }

    // 6. Diffuse and decay the trail map
    DiffuseAndDecay();

    ++m_State.stepCount;
}

// ============================================================================
// Sensing
// ============================================================================

f32 PhysarumSimulation::Sense(const PhysarumAgent& agent, f32 sensorAngleOffset) const {
    f32 sensorAngle = agent.angle + sensorAngleOffset;
    f32 sensorX = agent.x + std::cos(sensorAngle) * m_Config.sensorDistance;
    f32 sensorY = agent.y + std::sin(sensorAngle) * m_Config.sensorDistance;
    return SampleTrail(sensorX, sensorY);
}

f32 PhysarumSimulation::SampleTrail(f32 x, f32 y) const {
    u32 w = m_State.width;
    u32 h = m_State.height;
    if (w == 0 || h == 0) return 0.0f;

    // Wrap or clamp coordinates
    if (m_Config.wrapEdges) {
        x = WrapX(x);
        y = WrapY(y);
    } else {
        x = std::max(0.0f, std::min(static_cast<f32>(w) - 1.001f, x));
        y = std::max(0.0f, std::min(static_cast<f32>(h) - 1.001f, y));
    }

    // Bilinear interpolation
    u32 x0 = static_cast<u32>(x);
    u32 y0 = static_cast<u32>(y);
    u32 x1 = x0 + 1;
    u32 y1 = y0 + 1;

    // Handle wrap for the +1 samples
    if (m_Config.wrapEdges) {
        if (x1 >= w) x1 = 0;
        if (y1 >= h) y1 = 0;
    } else {
        if (x1 >= w) x1 = w - 1;
        if (y1 >= h) y1 = h - 1;
    }

    f32 fx = x - static_cast<f32>(x0);
    f32 fy = y - static_cast<f32>(y0);

    f32 v00 = m_State.trailMap[static_cast<usize>(y0) * w + x0];
    f32 v10 = m_State.trailMap[static_cast<usize>(y0) * w + x1];
    f32 v01 = m_State.trailMap[static_cast<usize>(y1) * w + x0];
    f32 v11 = m_State.trailMap[static_cast<usize>(y1) * w + x1];

    f32 top = v00 * (1.0f - fx) + v10 * fx;
    f32 bottom = v01 * (1.0f - fx) + v11 * fx;
    return top * (1.0f - fy) + bottom * fy;
}

// ============================================================================
// Diffusion and Decay
// ============================================================================

void PhysarumSimulation::DiffuseAndDecay() {
    u32 w = m_State.width;
    u32 h = m_State.height;
    if (w == 0 || h == 0) return;

    f32 decayFactor = 1.0f - m_Config.trailDecay;
    // Trail saturation ceiling, see the clamp below.
    const f32 steadyState = m_Config.trailDeposit /
                            std::max(m_Config.trailDecay, 0.0001f);
    const f32 maxTrail = steadyState * 20.0f;
    decayFactor = std::max(0.0f, std::min(1.0f, decayFactor));

    // Determine blur kernel size from diffuseRadius
    // 0 = no diffusion (just decay), 1 = 3x3, 2 = 5x5
    i32 kernelRadius = static_cast<i32>(m_Config.diffuseRadius);
    kernelRadius = std::max(0, std::min(2, kernelRadius));

    if (kernelRadius == 0) {
        // No diffusion — just decay in-place
        for (usize i = 0; i < m_State.trailMap.size(); ++i) {
            m_State.trailMap[i] *= decayFactor;
        }
        return;
    }

    // Box blur into temp buffer, then swap
    for (u32 py = 0; py < h; ++py) {
        for (u32 px = 0; px < w; ++px) {
            f32 sum = 0.0f;
            f32 count = 0.0f;

            for (i32 dy = -kernelRadius; dy <= kernelRadius; ++dy) {
                for (i32 dx = -kernelRadius; dx <= kernelRadius; ++dx) {
                    i32 sx = static_cast<i32>(px) + dx;
                    i32 sy = static_cast<i32>(py) + dy;

                    if (m_Config.wrapEdges) {
                        // Modular wrap
                        sx = ((sx % static_cast<i32>(w)) + static_cast<i32>(w)) % static_cast<i32>(w);
                        sy = ((sy % static_cast<i32>(h)) + static_cast<i32>(h)) % static_cast<i32>(h);
                    } else {
                        // Clamp to edges
                        sx = std::max(0, std::min(static_cast<i32>(w) - 1, sx));
                        sy = std::max(0, std::min(static_cast<i32>(h) - 1, sy));
                    }

                    sum += m_State.trailMap[static_cast<usize>(sy) * w + static_cast<u32>(sx)];
                    count += 1.0f;
                }
            }

            // Saturate the trail.
            //
            // Without a ceiling the map is an unbounded positive feedback loop:
            // more agents on a cell means more deposit, which makes it a
            // stronger attractor, which draws more agents. Whichever cell gets
            // slightly ahead first wins permanently. Measured on Classic Slime
            // at 192x192 with 60000 agents, the whole population ends up inside
            // a 20x16 pixel box (positional spread 3.4 px) and the peak trail
            // reaches 102442 against a single-agent steady state of 250. No
            // network forms at all, and no amount of seeding or steering
            // adjustment escapes it, because the runaway is in the field rather
            // than in the agents.
            //
            // The ceiling is derived from the preset rather than fixed, so it
            // scales with deposit and decay: deposit/decay is the level one
            // agent depositing continuously converges to, and twenty times that
            // leaves ample headroom for genuinely busy cells while capping the
            // runaway. Presets that never approached the ceiling are unchanged.
            f32 value = (sum / count) * decayFactor;
            m_TempTrail[static_cast<usize>(py) * w + px] = std::min(value, maxTrail);
        }
    }

    // Swap buffers
    std::swap(m_State.trailMap, m_TempTrail);
}

// ============================================================================
// Coordinate Wrapping
// ============================================================================

f32 PhysarumSimulation::WrapX(f32 x) const {
    f32 fw = static_cast<f32>(m_State.width);
    x = std::fmod(x, fw);
    if (x < 0.0f) x += fw;
    return x;
}

f32 PhysarumSimulation::WrapY(f32 y) const {
    f32 fh = static_cast<f32>(m_State.height);
    y = std::fmod(y, fh);
    if (y < 0.0f) y += fh;
    return y;
}

// ============================================================================
// Seeding Patterns
// ============================================================================

void PhysarumSimulation::Reset() {
    u32 w = m_State.width;
    u32 h = m_State.height;

    // Clear trail
    std::fill(m_State.trailMap.begin(), m_State.trailMap.end(), 0.0f);
    std::fill(m_TempTrail.begin(), m_TempTrail.end(), 0.0f);
    m_State.stepCount = 0;

    // Re-seed PRNG
    if (m_Config.seed != 0) {
        m_RngState = m_Config.seed;
    } else {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        m_RngState = static_cast<u32>(
            std::chrono::duration_cast<std::chrono::microseconds>(now).count() & 0xFFFFFFFF);
        if (m_RngState == 0) m_RngState = 12345;
    }

    // Randomize agent positions
    for (u32 i = 0; i < static_cast<u32>(m_State.agents.size()); ++i) {
        PhysarumAgent& agent = m_State.agents[i];
        agent.x = RandomFloat() * static_cast<f32>(w);
        agent.y = RandomFloat() * static_cast<f32>(h);
        agent.angle = RandomFloat() * 2.0f * PI;
    }
}

void PhysarumSimulation::SeedCircle(f32 cx, f32 cy, f32 radius, u32 count) {
    count = std::min(count, static_cast<u32>(m_State.agents.size()));

    for (u32 i = 0; i < count; ++i) {
        PhysarumAgent& agent = m_State.agents[i];

        // Random position within circle
        f32 r = radius * std::sqrt(RandomFloat());
        f32 theta = RandomFloat() * 2.0f * PI;
        agent.x = cx + r * std::cos(theta);
        agent.y = cy + r * std::sin(theta);

        // Aim inward, but spread the headings. Pointing every agent at exactly
        // the centre is a funnel, not a starting condition: with tens of
        // thousands of agents converging on one pixel the deposit there runs
        // hundreds of times above the analytic steady state, a third of the
        // population ends up stacked in a single cell, and no network forms at
        // all. Measured on Classic Slime: peak trail 102858 against a steady
        // state of deposit/decay = 250.
        //
        // A half-pi spread keeps the inward sweep that makes the ring seeding
        // worth having while letting agents pass the centre and interact, which
        // is where the trail network actually comes from.
        constexpr f32 kInwardSpread = PI * 0.5f;
        agent.angle = std::atan2(cy - agent.y, cx - agent.x)
                    + (RandomFloat() - 0.5f) * kInwardSpread;

        // Wrap if needed
        if (m_Config.wrapEdges) {
            agent.x = WrapX(agent.x);
            agent.y = WrapY(agent.y);
        } else {
            agent.x = std::max(0.0f, std::min(static_cast<f32>(m_State.width) - 0.001f, agent.x));
            agent.y = std::max(0.0f, std::min(static_cast<f32>(m_State.height) - 0.001f, agent.y));
        }
    }
}

void PhysarumSimulation::SeedPoint(f32 cx, f32 cy, u32 count) {
    count = std::min(count, static_cast<u32>(m_State.agents.size()));

    for (u32 i = 0; i < count; ++i) {
        PhysarumAgent& agent = m_State.agents[i];
        agent.x = cx;
        agent.y = cy;
        agent.angle = RandomFloat() * 2.0f * PI;

        if (m_Config.wrapEdges) {
            agent.x = WrapX(agent.x);
            agent.y = WrapY(agent.y);
        } else {
            agent.x = std::max(0.0f, std::min(static_cast<f32>(m_State.width) - 0.001f, agent.x));
            agent.y = std::max(0.0f, std::min(static_cast<f32>(m_State.height) - 0.001f, agent.y));
        }
    }
}

void PhysarumSimulation::SeedRing(f32 cx, f32 cy, f32 innerRadius, f32 outerRadius, u32 count) {
    count = std::min(count, static_cast<u32>(m_State.agents.size()));

    for (u32 i = 0; i < count; ++i) {
        PhysarumAgent& agent = m_State.agents[i];

        // Random position within ring (annulus)
        f32 rMin2 = innerRadius * innerRadius;
        f32 rMax2 = outerRadius * outerRadius;
        f32 r = std::sqrt(rMin2 + RandomFloat() * (rMax2 - rMin2));
        f32 theta = RandomFloat() * 2.0f * PI;
        agent.x = cx + r * std::cos(theta);
        agent.y = cy + r * std::sin(theta);

        // Aim inward, but spread the headings. Pointing every agent at exactly
        // the centre is a funnel, not a starting condition: with tens of
        // thousands of agents converging on one pixel the deposit there runs
        // hundreds of times above the analytic steady state, a third of the
        // population ends up stacked in a single cell, and no network forms at
        // all. Measured on Classic Slime: peak trail 102858 against a steady
        // state of deposit/decay = 250.
        //
        // A half-pi spread keeps the inward sweep that makes the ring seeding
        // worth having while letting agents pass the centre and interact, which
        // is where the trail network actually comes from.
        constexpr f32 kInwardSpread = PI * 0.5f;
        agent.angle = std::atan2(cy - agent.y, cx - agent.x)
                    + (RandomFloat() - 0.5f) * kInwardSpread;

        if (m_Config.wrapEdges) {
            agent.x = WrapX(agent.x);
            agent.y = WrapY(agent.y);
        } else {
            agent.x = std::max(0.0f, std::min(static_cast<f32>(m_State.width) - 0.001f, agent.x));
            agent.y = std::max(0.0f, std::min(static_cast<f32>(m_State.height) - 0.001f, agent.y));
        }
    }
}

void PhysarumSimulation::AddFoodSource(f32 fx, f32 fy, f32 radius, f32 strength) {
    u32 w = m_State.width;
    u32 h = m_State.height;
    if (w == 0 || h == 0) return;

    i32 r = static_cast<i32>(std::ceil(radius));
    i32 cx = static_cast<i32>(fx);
    i32 cy = static_cast<i32>(fy);

    for (i32 dy = -r; dy <= r; ++dy) {
        for (i32 dx = -r; dx <= r; ++dx) {
            f32 dist = std::sqrt(static_cast<f32>(dx * dx + dy * dy));
            if (dist > radius) continue;

            i32 px = cx + dx;
            i32 py = cy + dy;

            if (m_Config.wrapEdges) {
                px = ((px % static_cast<i32>(w)) + static_cast<i32>(w)) % static_cast<i32>(w);
                py = ((py % static_cast<i32>(h)) + static_cast<i32>(h)) % static_cast<i32>(h);
            } else {
                if (px < 0 || px >= static_cast<i32>(w)) continue;
                if (py < 0 || py >= static_cast<i32>(h)) continue;
            }

            f32 falloff = 1.0f - dist / radius;
            m_State.trailMap[static_cast<usize>(py) * w + static_cast<u32>(px)] += strength * falloff;
        }
    }
}

// ============================================================================
// Presets
// ============================================================================

PhysarumConfig PhysarumSimulation::GetPresetConfig(PhysarumPreset preset) {
    PhysarumConfig cfg;
    cfg.preset = preset;

    switch (preset) {
    // Note: turnSpeed exceeds sensorAngle here and in DenseWeb, so an agent
    // steering toward a sensor overshoots it and the other sensor wins on the
    // next step. Jones' original pairs them equal. Setting them equal was tried
    // and made no measurable difference once the seeding funnel was fixed, so
    // the shipped values stand until someone can show the change is an
    // improvement rather than a preference.
    case PhysarumPreset::ClassicSlime:
        cfg.sensorAngle = 22.5f;
        cfg.sensorDistance = 9.0f;
        cfg.turnSpeed = 45.0f;
        cfg.moveSpeed = 1.0f;
        cfg.trailDecay = 0.02f;
        cfg.trailDeposit = 5.0f;
        cfg.diffuseRadius = 1.0f;
        break;
    case PhysarumPreset::BranchingNetwork:
        cfg.sensorAngle = 45.0f;
        cfg.sensorDistance = 15.0f;
        cfg.turnSpeed = 22.0f;
        cfg.moveSpeed = 1.0f;
        cfg.trailDecay = 0.05f;
        cfg.trailDeposit = 3.0f;
        cfg.diffuseRadius = 1.0f;
        break;
    case PhysarumPreset::DenseWeb:
        cfg.sensorAngle = 30.0f;
        cfg.sensorDistance = 7.0f;
        cfg.turnSpeed = 60.0f;
        cfg.moveSpeed = 1.0f;
        cfg.trailDecay = 0.01f;
        cfg.trailDeposit = 8.0f;
        cfg.diffuseRadius = 1.0f;
        break;
    case PhysarumPreset::Tendrils:
        cfg.sensorAngle = 10.0f;
        cfg.sensorDistance = 20.0f;
        cfg.turnSpeed = 15.0f;
        cfg.moveSpeed = 1.0f;
        cfg.trailDecay = 0.04f;
        cfg.trailDeposit = 2.0f;
        cfg.diffuseRadius = 1.0f;
        break;
    case PhysarumPreset::Pulsating:
        cfg.sensorAngle = 35.0f;
        cfg.sensorDistance = 12.0f;
        cfg.turnSpeed = 40.0f;
        cfg.moveSpeed = 1.0f;
        cfg.trailDecay = 0.08f;
        cfg.trailDeposit = 10.0f;
        cfg.diffuseRadius = 1.0f;
        break;
    case PhysarumPreset::Custom:
        // Use defaults from PhysarumConfig
        break;
    }

    return cfg;
}

const char* PhysarumSimulation::GetPresetName(PhysarumPreset preset) {
    switch (preset) {
    case PhysarumPreset::ClassicSlime:      return "Classic Slime";
    case PhysarumPreset::BranchingNetwork:  return "Branching Network";
    case PhysarumPreset::DenseWeb:          return "Dense Web";
    case PhysarumPreset::Tendrils:          return "Tendrils";
    case PhysarumPreset::Pulsating:         return "Pulsating";
    case PhysarumPreset::Custom:            return "Custom";
    }
    return "Unknown";
}

// ============================================================================
// Baking / Export
// ============================================================================

std::vector<u8> PhysarumSimulation::BakeToRGBA8(const Math::Vector3& trailColor,
                                                  const Math::Vector3& bgColor) const {
    u32 w = m_State.width;
    u32 h = m_State.height;
    usize pixelCount = static_cast<usize>(w) * h;
    std::vector<u8> rgba(pixelCount * 4);

    if (pixelCount == 0) return rgba;

    // Tone-map against the mean of the OCCUPIED cells, with a saturating curve.
    //
    // Dividing by the maximum makes one cell set the scale for the whole image,
    // and a Physarum trail map is heavy-tailed by construction: agents pile up,
    // and where they do the deposit accumulates far above the network around it.
    // Measured on the shipped presets at 192x192 after 4000 steps, Classic Slime
    // reaches a maximum of 102858 against a mean of 399 over its occupied cells,
    // a 258:1 tail, while Branching Network sits at 19:1. Against the maximum the
    // entire Classic network divides down to background and the preset renders as
    // one bright dot on an empty field. A high percentile is not enough either:
    // the tail is long enough that even p99.5 stays orders of magnitude above the
    // trails you actually want to see.
    //
    // 1 - exp(-v/mean) has no tuning constant, maps the mean occupied cell to
    // 0.63 and three times the mean to 0.95, and compresses any outlier into the
    // top of the range instead of letting it define the range. Both presets read
    // correctly under it, and so does a sim that has run long enough to build
    // peaks the author never anticipated.
    f64 occupiedSum = 0.0;
    usize occupied = 0;
    for (usize i = 0; i < pixelCount; ++i) {
        f32 v = m_State.trailMap[i];
        if (v > 0.001f) { occupiedSum += v; ++occupied; }
    }

    f32 reference = (occupied > 0)
                      ? static_cast<f32>(occupiedSum / static_cast<f64>(occupied))
                      : 1.0f;
    if (reference < 0.0001f) reference = 1.0f;

    f32 invMax = 1.0f / reference;

    for (usize i = 0; i < pixelCount; ++i) {
        f32 t = 1.0f - std::exp(-m_State.trailMap[i] * invMax);

        // Lerp between background and trail color
        f32 r = bgColor.x + (trailColor.x - bgColor.x) * t;
        f32 g = bgColor.y + (trailColor.y - bgColor.y) * t;
        f32 b = bgColor.z + (trailColor.z - bgColor.z) * t;

        usize base = i * 4;
        rgba[base + 0] = static_cast<u8>(std::max(0.0f, std::min(255.0f, r * 255.0f)));
        rgba[base + 1] = static_cast<u8>(std::max(0.0f, std::min(255.0f, g * 255.0f)));
        rgba[base + 2] = static_cast<u8>(std::max(0.0f, std::min(255.0f, b * 255.0f)));
        rgba[base + 3] = 255;
    }

    return rgba;
}

std::vector<f32> PhysarumSimulation::BakeToHeightmap() const {
    u32 w = m_State.width;
    u32 h = m_State.height;
    usize pixelCount = static_cast<usize>(w) * h;
    std::vector<f32> heightmap(pixelCount);

    if (pixelCount == 0) return heightmap;

    // Find max for normalization
    f32 maxVal = 0.0f;
    for (usize i = 0; i < pixelCount; ++i) {
        if (m_State.trailMap[i] > maxVal) {
            maxVal = m_State.trailMap[i];
        }
    }
    if (maxVal < 0.0001f) maxVal = 1.0f;

    f32 invMax = 1.0f / maxVal;
    for (usize i = 0; i < pixelCount; ++i) {
        heightmap[i] = std::max(0.0f, std::min(1.0f, m_State.trailMap[i] * invMax));
    }

    return heightmap;
}

} // namespace Effects
} // namespace Enjin
