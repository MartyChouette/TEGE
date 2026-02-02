# Sample 03: Physics

Demonstrates physics joints, ragdolls, collision detection, and rigidbody dynamics.

## What This Demonstrates

- Rigidbody components with gravity and collision
- Box and sphere colliders
- 6 joint types (Distance, Hinge, BallSocket, Spring, Fixed, Slider)
- Ragdoll configuration
- Raycasting from scripts

## Scene Contents

| Entity | Components | Purpose |
|--------|-----------|---------|
| Ground | Transform, Mesh, Material, BoxCollider, Rigidbody (Static) | Static ground plane |
| Pendulum_Anchor | Transform, Rigidbody (Static) | Fixed point for hinge joint |
| Pendulum_Weight | Transform, Mesh, Rigidbody, HingeJoint | Swinging weight |
| Bridge_Plank_1-5 | Transform, Mesh, Rigidbody, DistanceJoint | Chain of planks forming a bridge |
| Spring_Box | Transform, Mesh, Rigidbody, SpringJoint | Box connected by spring |
| Ragdoll_Root | Transform, Skeleton, Animator, Ragdoll | Character with ragdoll physics |
| Ball | Transform, Mesh, SphereCollider, Rigidbody, Script | Player-controllable ball |

## Joint Types Demonstrated

### Hinge Joint (Pendulum)
The pendulum uses a `HingeJointComponent` with:
- Axis: (0, 0, 1) - swings in the XY plane
- Limits: -60 to 60 degrees

### Distance Joint (Bridge)
Bridge planks are chained with `DistanceJointComponent`:
- Rest distance: 1.5 units
- Stiffness: 0.8 (slightly flexible)

### Spring Joint
The spring box uses `SpringJointComponent`:
- Spring constant: 15.0
- Damping: 0.5

## Key Concepts

### Rigidbody Setup

Every dynamic physics entity needs:
1. `TransformComponent` - Position in world
2. `RigidbodyComponent` - Mass, velocity, gravity settings
3. A collider (`BoxColliderComponent` or `SphereColliderComponent`)

Set `bodyType` to `Static` for immovable objects, `Dynamic` for simulated objects, `Kinematic` for script-driven objects.

### Joint Setup

Every joint needs:
1. `entityA` and `entityB` - The two connected entities
2. `anchorA` and `anchorB` - Local-space connection points
3. Type-specific settings (limits, motors, spring constants)

### Scripted Physics Queries

```angelscript
// Raycast
bool hit = Physics_Raycast(origin, direction, maxDist);

// Check sphere overlap
bool overlapping = Physics_CheckSphere(center, radius);

// Apply force
Physics_AddForce(entityId, Vector3(0, 100, 0));

// Apply instant impulse
Physics_AddImpulse(entityId, Vector3(10, 0, 0));
```
