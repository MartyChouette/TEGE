// TegeBehavior — base class for all game scripts
// Inherit from this class to attach behavior to entities.
//
// Example:
//   class PlayerController : TegeBehavior {
//       [Property] float moveSpeed = 5.0f;
//
//       void OnUpdate(float dt) {
//           Vector3 move = Vector3(0, 0, 0);
//           if (Input_GetKey(Key::W)) move.z -= 1;
//           if (Input_GetKey(Key::S)) move.z += 1;
//           move = move.Normalized() * moveSpeed;
//           SetPosition(GetPosition() + move * dt);
//       }
//   }

class TegeBehavior {
    // Entity this behavior is attached to (set by the engine)
    uint64 _entityId;

    // Whether this behavior is enabled
    bool enabled = true;

    // --- Transform shortcuts ---

    Vector3 GetPosition() {
        return Entity_GetPosition(_entityId);
    }

    void SetPosition(const Vector3 &in pos) {
        Entity_SetPosition(_entityId, pos);
    }

    Vector3 GetRotation() {
        return Entity_GetRotation(_entityId);
    }

    void SetRotation(const Vector3 &in rot) {
        Entity_SetRotation(_entityId, rot);
    }

    Vector3 GetScale() {
        return Entity_GetScale(_entityId);
    }

    void SetScale(const Vector3 &in s) {
        Entity_SetScale(_entityId, s);
    }

    string GetName() {
        return Entity_GetName(_entityId);
    }

    uint64 GetEntity() {
        return _entityId;
    }

    // --- Coroutines ---

    void StartCoroutine(const string &in funcName) {
        ::StartCoroutine(funcName);
    }

    // --- Lifecycle callbacks (override in subclasses) ---
    // void OnCreate()                { }
    // void OnStart()                 { }
    // void OnUpdate(float dt)        { }
    // void OnFixedUpdate(float dt)   { }
    // void OnLateUpdate(float dt)    { }
    // void OnDestroy()               { }
    // void OnEnable()                { }
    // void OnDisable()               { }
    // void OnCollisionEnter(uint64 other) { }
    // void OnCollisionStay(uint64 other)  { }
    // void OnCollisionExit(uint64 other)  { }
    // void OnTriggerEnter(uint64 other)   { }
    // void OnTriggerExit(uint64 other)    { }
    // void OnMouseEnter()                 { }  // cursor ray first hits this entity (needs a collider)
    // void OnMouseExit()                  { }  // cursor ray leaves this entity
    // void OnClick()                      { }  // left click while the cursor is on this entity
}
