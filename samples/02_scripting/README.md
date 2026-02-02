# Sample 02: Scripting

Demonstrates AngelScript gameplay scripting: TegeBehavior lifecycle, movement, input, coroutines, and events.

## What This Demonstrates

- Writing a `TegeBehavior` script with `OnCreate()` and `OnUpdate()`
- Reading keyboard input from scripts
- Moving entities via `Entity_SetPosition()`
- Using coroutines for delayed actions
- Sending and receiving events between scripts

## Scene Contents

| Entity | Components | Purpose |
|--------|-----------|---------|
| Player | Transform, Mesh, Material, Script, Name | Movable cube controlled by a script |
| Spawner | Transform, Script, Name | Spawns effects using coroutines |
| Light | Transform, Light, Name | Scene illumination |
| Camera | Transform, Camera, Name | View camera |

## Scripts

### PlayerMovement.as

```angelscript
class PlayerMovement : TegeBehavior {
    float speed = 5.0f;

    void OnCreate() {
        Log("Player created!");
    }

    void OnUpdate(float dt) {
        Vector3 pos = Entity_GetPosition(entityId);

        if (Input_IsKeyDown(KEY_W)) pos.z -= speed * dt;
        if (Input_IsKeyDown(KEY_S)) pos.z += speed * dt;
        if (Input_IsKeyDown(KEY_A)) pos.x -= speed * dt;
        if (Input_IsKeyDown(KEY_D)) pos.x += speed * dt;

        Entity_SetPosition(entityId, pos);
    }
}
```

### SpawnerScript.as

```angelscript
class SpawnerScript : TegeBehavior {
    void OnCreate() {
        StartCoroutine("SpawnLoop");
        Events_Listen("player_moved", @OnPlayerMoved);
    }

    void SpawnLoop() {
        while (true) {
            YieldSeconds(2.0f);
            Log("Spawning effect...");
            Events_Send("spawn_effect", EventData());
        }
    }

    void OnPlayerMoved(EventData@ data) {
        Log("Player moved to new position");
    }
}
```

## Key Concepts

### TegeBehavior Lifecycle

1. `OnCreate()` - Called once when the entity is first created or the scene loads
2. `OnUpdate(float deltaTime)` - Called every frame
3. `OnDestroy()` - Called when the entity is destroyed

### Input

- `Input_IsKeyDown(keyCode)` - Returns true while key is held
- `Input_IsKeyPressed(keyCode)` - Returns true the frame key is first pressed
- Key codes: `KEY_W`, `KEY_A`, `KEY_S`, `KEY_D`, `KEY_SPACE`, `KEY_SHIFT`, etc.

### Coroutines

- `StartCoroutine("funcName")` - Start a coroutine on the current script
- `YieldSeconds(2.0f)` - Pause coroutine for 2 seconds
- `YieldFrames(5)` - Pause for 5 frames
- `YieldEndOfFrame()` - Resume next frame

### Events

- `Events_Listen("name", @callback)` - Register a listener
- `Events_Send("name", data)` - Send to listeners of that event name
- `Events_Broadcast(data)` - Send to all listeners
