// PickupItem.as - one collectable
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
class PickupItem : TegeBehavior {
    [Property] float spinSpeed = 90.0f;
    [Property] float pickupRadius = 1.2f;

    void OnUpdate(float dt) {
        Vector3 rot = GetRotation();
        rot.y += spinSpeed * dt;
        SetRotation(rot);

        uint64 player = Scene_FindEntity("Player");
        if (player == 0) return;

        Vector3 diff = GetPosition() - Entity_GetPosition(player);
        if (diff.Length() > pickupRadius) return;

        Events_Send("player_pickup", EventData());
        Scene_DestroyEntity(GetEntity());
    }
}
