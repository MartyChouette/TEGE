// SpinningCube.as — spins unless Reduced Motion is enabled.
class SpinningCube : TegeBehavior {
    [Property] float spinSpeed = 60.0f;

    void OnUpdate(float dt) {
        if (Accessibility_GetReducedMotion()) return;
        Vector3 rot = GetRotation();
        rot.y += spinSpeed * dt;
        SetRotation(rot);
    }
}
