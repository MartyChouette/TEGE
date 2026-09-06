// HazardSpike.as — Bite-sized behavior attached to the Hazard object
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
        if (player != 0) {
            Vector3 diff = GetPosition() - Entity_GetPosition(player);
            if (diff.Length() <= hazardRadius) {
                PlayerController@ controller = cast<PlayerController>(Entity_GetBehavior(player, "PlayerController"));
                if (controller !is null) {
                    controller.TakeDamage(damageAmount);
                    _timer = cooldownTime;
                }
            }
        }
    }
}
