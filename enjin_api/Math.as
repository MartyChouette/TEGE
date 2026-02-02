// Math utility functions for TegeBehavior scripts
// These wrap the engine's registered global math functions.

// Note: Vector2, Vector3, Vector4, Quaternion are registered as native types
// by the engine. The following are convenience functions.

// Linearly interpolate between two Vector3 values
Vector3 LerpVector3(const Vector3 &in a, const Vector3 &in b, float t) {
    return Vector3(
        Lerp(a.x, b.x, t),
        Lerp(a.y, b.y, t),
        Lerp(a.z, b.z, t)
    );
}

// Move a Vector3 toward a target, capped by maxDelta
Vector3 MoveTowardsVector3(const Vector3 &in current, const Vector3 &in target, float maxDelta) {
    Vector3 diff = target - current;
    float dist = diff.Length();
    if (dist <= maxDelta || dist < 0.0001f) {
        return target;
    }
    return current + diff * (maxDelta / dist);
}

// Distance between two points
float Distance(const Vector3 &in a, const Vector3 &in b) {
    return (b - a).Length();
}

// Direction from a to b (normalized)
Vector3 Direction(const Vector3 &in from, const Vector3 &in to) {
    return (to - from).Normalized();
}

// Convert degrees to radians
float Deg2Rad(float degrees) {
    return degrees * 3.14159265f / 180.0f;
}

// Convert radians to degrees
float Rad2Deg(float radians) {
    return radians * 180.0f / 3.14159265f;
}
