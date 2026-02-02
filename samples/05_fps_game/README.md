# Sample 05: FPS Game

A complete mini first-person shooter demonstrating all engine systems working together.

## What This Demonstrates

- First-person character controller with mouse look
- Physics-based projectiles
- Health and damage system
- Quest objectives
- HUD overlay (health bar, crosshair, ammo)
- 3D audio (gunshots, footsteps, ambient)
- Save/load game state
- Profiler integration

## Scene Contents

| Entity | Components | Purpose |
|--------|-----------|---------|
| Player | Transform, FPSController, Health, Resource, AudioListener, Footstep, HUDWidget, Script | The player character |
| PlayerCamera | Transform, Camera | First-person camera |
| Enemy_1-3 | Transform, Mesh, Health, AIController, AudioSource | Enemy NPCs |
| Weapon | Transform, Mesh, AudioSource, Script | Player's weapon |
| Ammo_Pickup_1-3 | Transform, Mesh, Pickup, Interactable | Ammo pickups |
| Health_Pickup | Transform, Mesh, Pickup, Interactable | Health restore |
| Quest_Objective | Transform, QuestState, Name | "Eliminate all enemies" quest |
| Ground | Transform, Mesh, BoxCollider, Rigidbody (Static) | Arena floor |
| Walls | Transform, Mesh, BoxCollider, Rigidbody (Static) | Arena walls |
| Ambient_Light | Transform, Light (Directional) | Scene lighting |
| Point_Lights | Transform, Light (Point) | Accent lighting |
| Music | Transform, AudioSource | Background music |

## Systems in Action

### Character Controller
The FPS controller provides:
- WASD movement with configurable speed
- Mouse look with raw input + smoothing
- Jump with gravity
- Sprint (hold Shift)

### Combat
- `HealthComponent` on player and enemies (max HP, regen, shields)
- `DamageComponent` on projectiles (damage amount, knockback)
- `DamageResistanceComponent` on enemies (per-type multipliers)
- Script fires raycasts for hitscan weapons

### Quest System
- `QuestStateComponent` with objectives ("Kill Enemy 1", "Kill Enemy 2", "Kill Enemy 3")
- Quest log overlay shows progress in top-right corner
- Quest auto-completes when all objectives are flagged done

### HUD
- Health bar (top-left)
- Resource/ammo bar (bottom-right)
- Crosshair (center)
- Quest log (top-right)

### Save/Load
- Quick save: F5
- Quick load: F9
- Save system captures entity states, health, quest progress, inventory

## AngelScript Integration

### WeaponScript.as

```angelscript
class WeaponScript : TegeBehavior {
    float fireRate = 0.15f;
    float damage = 25.0f;
    float fireTimer = 0.0f;

    void OnUpdate(float dt) {
        fireTimer -= dt;
        if (Input_IsMouseButtonDown(0) && fireTimer <= 0.0f) {
            Fire();
            fireTimer = fireRate;
        }
    }

    void Fire() {
        Audio_Play(entityId);

        // Raycast from camera
        if (Physics_Raycast(cameraPos, cameraForward, 100.0f)) {
            uint64 hitEntity = Physics_RaycastHit_Entity();
            if (HasComponent_Health(hitEntity)) {
                Health_Damage(hitEntity, damage);
            }
        }
    }
}
```

## Running the Sample

1. Open Enjin Editor
2. **File > Open Scene** and select `fps_game.enjin`
3. Press **Play**
4. WASD to move, mouse to look, left-click to shoot
5. Press **F5** to save, **F9** to load
6. Open **View > Profiler** to see performance breakdown
