// PickupItem.as — Bite-sized behavior attached to each Pickup object
class PickupItem : TegeBehavior {
    [Property] float spinSpeed = 90.0f;
    [Property] float pickupRadius = 1.2f;

    void OnUpdate(float dt) {
        Vector3 rot = GetRotation();
        rot.y += spinSpeed * dt;
        SetRotation(rot);

        uint64 player = Scene_FindEntity("Player");
        if (player != 0) {
            Vector3 diff = GetPosition() - Entity_GetPosition(player);
            if (diff.Length() <= pickupRadius) {
                PlayerController@ controller = cast<PlayerController>(Entity_GetBehavior(player, "PlayerController"));
                if (controller !is null) {
                    controller.AddPickup();
                }
                Scene_DestroyEntity(GetEntity());
            }
        }
    }
}
