// HazardSpike.as - damages the player on contact
//
// ONE SCRIPT CANNOT REACH INTO ANOTHER. Every .as file compiles as its own
// module, so a class defined in PlayerController.as is not a type this file can
// name, and Entity_GetBehavior -- which these scripts used to call -- is not a
// binding this engine has ever had. They therefore never compiled, and the
// template shipped teaching a pattern that cannot work.
//
// Scripts talk through EVENTS instead, which is the mechanism the engine
// actually provides: Events_Send with an EventData payload, Events_Listen with
// a handler taking (const string &in).
class HazardSpike : TegeBehavior {
    [Property] float damageAmount = 25.0f;
    [Property] float hazardRadius = 1.2f;
    [Property] float cooldownTime = 1.0f;

    private float _timer = 0.0f;

    void OnUpdate(float dt) {
        if (_timer > 0.0f) {
            _timer -= dt;
            return;
        }

        uint64 player = Scene_FindEntity("Player");
        if (player == 0) return;

        Vector3 diff = GetPosition() - Entity_GetPosition(player);
        if (diff.Length() > hazardRadius) return;

        EventData@ d = EventData();
        d.SetFloat("amount", damageAmount);
        Events_Send("player_damage", d);
        _timer = cooldownTime;
    }
}
