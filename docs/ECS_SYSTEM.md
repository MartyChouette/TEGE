# ECS (Entity Component System) Implementation

## Overview

A complete Entity Component System implementation following data-oriented design principles for cache efficiency.

## Architecture

### Core Components

1. **Entity** - Just an ID (u64)
2. **Component** - Plain data structures
3. **System** - Logic that operates on components
4. **World** - Container that manages everything

### Key Design Decisions

- **Structure of Arrays (SoA)**: Components are stored in separate arrays for cache efficiency
- **Type-safe Component Storage**: Template-based storage with type erasure
- **System-based Logic**: All game logic lives in systems
- **No Inheritance**: Components are plain structs, not classes

## Usage

### Creating Entities

```cpp
World world;
Entity entity = world.CreateEntity();
```

### Adding Components

```cpp
// Add transform component
TransformComponent& transform = world.AddComponent<TransformComponent>(entity);
transform.position = Vector3(0, 0, 0);
transform.scale = Vector3(1, 1, 1);

// Add mesh component
MeshComponent& mesh = world.AddComponent<MeshComponent>(entity);
mesh.vertices = { /* vertex data */ };
mesh.indices = { /* index data */ };
```

### Creating Systems

```cpp
class RenderSystem : public ISystem {
public:
    RenderSystem(World* world, Renderer* renderer);
    void Update(f32 deltaTime) override;
};

// Register system
RenderSystem* renderSystem = world.RegisterSystem<RenderSystem>(&world, &renderer);
```

### Updating the World

```cpp
world.Update(deltaTime); // Updates all systems
```

## Implemented Components

### TransformComponent
- Position (Vector3)
- Rotation (Quaternion)
- Scale (Vector3)
- `ToMatrix()` - Converts to transformation matrix

### MeshComponent
- Vertices (vector of Vertex structs: position, normal, UV, color, tangent, boneWeights, boneIndices)
- Indices (vector of u32)
- `IsValid()` - Checks if mesh has data

### SkeletonComponent
- `skeleton` (shared_ptr to `Animation::Skeleton`) - Shared skeleton data with bones, bind poses, inverse bind matrices

### AnimatorComponent
- `animator` (`SkeletalAnimator`) - Drives animation sampling and pose computation
- `stateMachine` (`AnimationStateMachine`) - State-based animation transitions
- `matricesDirty` (bool) - Signals that skinning matrices need GPU upload
- `Initialize(skeleton)` - Binds skeleton to animator and state machine
- `Update(deltaTime)` - Ticks state machine and animator, marks matrices dirty

## Implemented Systems

### RenderSystem
- Renders entities with Transform and Mesh components
- Currently creates a test triangle entity
- Ready for Vulkan pipeline integration

## Performance Benefits

### Cache Efficiency
Components are stored in contiguous arrays, improving cache locality when iterating:

```cpp
// All transforms are in one array
for (const auto& transform : transformStorage.GetComponents()) {
    // Cache-friendly iteration
}
```

### No Virtual Calls
Components are plain structs, no virtual function overhead.

### Component Filtering
Systems can efficiently query for entities with specific component combinations.

## Future Enhancements

1. **Archetype-based Storage** - Group entities by component signature
2. **Query System** - Efficient entity queries (e.g., "all entities with Transform and Mesh")
3. **Component Pools** - Pre-allocated component pools
4. **Multi-threading** - Parallel system execution
5. **Component Serialization** - Save/load component data

## Example: Complete Entity Setup

```cpp
World world;
Renderer renderer;

// Create entity
Entity triangle = world.CreateEntity();

// Add components
TransformComponent& transform = world.AddComponent<TransformComponent>(triangle);
transform.position = Vector3(0, 0, -5);
transform.rotation = Quaternion::Identity();
transform.scale = Vector3(1, 1, 1);

MeshComponent& mesh = world.AddComponent<MeshComponent>(triangle);
mesh.vertices = {
    { Vector3(-0.5f, -0.5f, 0.0f), Vector3(0, 0, 1), Vector2(0, 0) },
    { Vector3(0.5f, -0.5f, 0.0f), Vector3(0, 0, 1), Vector2(1, 0) },
    { Vector3(0.0f, 0.5f, 0.0f), Vector3(0, 0, 1), Vector2(0.5f, 1) }
};
mesh.indices = { 0, 1, 2 };

// Register render system
world.RegisterSystem<RenderSystem>(&world, &renderer);

// Update loop
while (running) {
    world.Update(deltaTime);
}
```

## Component Guidelines

1. **Keep components small** - They're copied frequently
2. **No logic in components** - Only data
3. **Use composition** - Prefer multiple small components over one large component
4. **Make components POD** - Plain Old Data for optimal performance

## System Guidelines

1. **One responsibility** - Each system does one thing
2. **Query efficiently** - Use component storage directly
3. **No dependencies** - Systems should be independent when possible
4. **Update order** - Consider system execution order
