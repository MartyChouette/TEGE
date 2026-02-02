// StateMachine.as — Simple finite state machine for game logic
// Usage:
//   class EnemyAI : TegeBehavior {
//       StateMachine fsm;
//
//       void OnStart() {
//           fsm.AddState("idle");
//           fsm.AddState("chase");
//           fsm.AddState("attack");
//           fsm.SetState("idle");
//       }
//
//       void OnUpdate(float dt) {
//           fsm.Update(dt);
//
//           if (fsm.IsState("idle")) {
//               // idle logic
//               if (playerNearby) fsm.SetState("chase");
//           } else if (fsm.IsState("chase")) {
//               // chase logic
//               if (inRange) fsm.SetState("attack");
//           }
//       }
//   }

class StateMachine {
    private string currentState;
    private string previousState;
    private float stateTime;
    private array<string> states;

    StateMachine() {
        stateTime = 0.0f;
    }

    // Add a named state
    void AddState(const string &in name) {
        states.insertLast(name);
    }

    // Transition to a new state
    void SetState(const string &in name) {
        if (currentState == name) return;
        previousState = currentState;
        currentState = name;
        stateTime = 0.0f;
    }

    // Get the current state name
    string GetState() const {
        return currentState;
    }

    // Get the previous state name
    string GetPreviousState() const {
        return previousState;
    }

    // Check if currently in a specific state
    bool IsState(const string &in name) const {
        return currentState == name;
    }

    // Check if just entered this state (first frame)
    bool JustEntered(const string &in name) const {
        return currentState == name && stateTime == 0.0f;
    }

    // Get time spent in current state (seconds)
    float GetStateTime() const {
        return stateTime;
    }

    // Check if a state exists
    bool HasState(const string &in name) const {
        for (uint i = 0; i < states.length(); i++) {
            if (states[i] == name) return true;
        }
        return false;
    }

    // Get number of registered states
    uint GetStateCount() const {
        return states.length();
    }

    // Call each frame to track time in state
    void Update(float dt) {
        stateTime += dt;
    }

    // Reset to no state
    void Reset() {
        previousState = currentState;
        currentState = "";
        stateTime = 0.0f;
    }
}
