// CineSuite.as — Virtual Cinematography Suite for TEGE (The Enjin Game Engine)
// DAW-structured virtual film crew: Grip, Camera/Lens, Electric, Staging, Director, Monitoring.
// Works seamlessly with TegeBehavior and TEGE ECS.

enum DirectorStyle {
    Kubrick = 0,     // One-point perspective, constant velocity dolly, deep focus, pristine framing
    Hitchcock,       // Triad shot patterns, Vertigo dolly zoom, subject proximity holds
    Polanski,        // Claustrophobic tracking, wide lens proximity, locked eye-level
    Welles,          // Extreme low angles, deep focus crane oners, dynamic shadow ratios
    Spielberg,       // Retargeted oners, soft entrance/exit ramps, emotional zoom reveals
    Lucas,           // Graphic tableaux, dual-vcam wipe transitions, geometric staging
    Ford,            // Static frame holds, absolute look direction, subject frame exit
    Kurosawa         // Telephoto compression, A/B/C multicam coverage, dynamic group blocking
}

enum RigType {
    Fixed = 0,
    TrackedDolly,
    CraneArm,
    Handheld,
    OperatorHead
}

// -------------------------------------------------------------------
// SecondOrderDynamics — Deterministic spring-damper dynamics (f, zeta, r)
// Used across all TEGE camera rigs for natural inertia without physics engines.
// -------------------------------------------------------------------
class SecondOrderDynamics {
    float f = 2.0f;       // Frequency (speed of response)
    float zeta = 1.0f;    // Damping ratio (1.0 = critically damped, <1 = overshoot)
    float r = 0.0f;       // Initial response (0 = smooth start, >0 = immediate, <0 = anticipate)

    private Vector3 _xp;
    private Vector3 _y;
    private Vector3 _yd;

    private float _k1, _k2, _k3;

    void SetParams(float frequency, float dampingRatio, float initialResponse, Vector3 x0) {
        f = frequency;
        zeta = dampingRatio;
        r = initialResponse;

        float pi = 3.14159265f;
        float w = 2.0f * pi * f;
        _k1 = zeta / (pi * f);
        _k2 = 1.0f / (w * w);
        _k3 = r * zeta / (2.0f * pi * f);

        _xp = x0;
        _y = x0;
        _yd = Vector3(0, 0, 0);
    }

    Vector3 Update(float dt, Vector3 x, Vector3 xd) {
        if (dt <= 0.0001f) return _y;

        // Recalculate coefficients for dynamic stability
        float pi = 3.14159265f;
        float w = 2.0f * pi * f;
        _k1 = zeta / (pi * f);
        _k2 = 1.0f / (w * w);
        _k3 = r * zeta / (2.0f * pi * f);

        _y = _y + _yd * dt;
        _yd = _yd + (x + xd * _k3 - _y - _yd * _k1) / _k2 * dt;
        return _y;
    }
}

// -------------------------------------------------------------------
// CineFocusPuller — Virtual Focus Puller Agent
// -------------------------------------------------------------------
class CineFocusPuller {
    float focusDistance = 5.0f;
    float apertureTStop = 2.8f;
    float skillLevel = 0.9f; // 0.0 - 1.0 (imperfection model)

    private float _targetDistance = 5.0f;
    private float _currentVel = 0.0f;

    void TrackSubject(Vector3 camPos, Vector3 subjectPos, float dt) {
        Vector3 diff = subjectPos - camPos;
        _targetDistance = sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        // Lag and smooth rack based on skill level
        float smoothness = (1.1f - skillLevel) * 0.25f + 0.05f;
        focusDistance = focusDistance + (_targetDistance - focusDistance) * (dt / (dt + smoothness));
    }
}

// -------------------------------------------------------------------
// CineGaffer — Electric & Lighting Ratios
// -------------------------------------------------------------------
class CineGaffer {
    float keyIntensity = 1.0f;
    float fillRatio = 0.25f; // Key:Fill = 4:1
    float rimRatio = 0.50f;  // Key:Rim = 2:1

    float GetFillIntensity() { return keyIntensity * fillRatio; }
    float GetRimIntensity() { return keyIntensity * rimRatio; }

    float CalculateStopDelta(float lightA, float lightB) {
        if (lightB <= 0.0001f) return 0.0f;
        return log(lightA / lightB) / log(2.0f); // Ev stop delta
    }
}

// -------------------------------------------------------------------
// CineDirector — Canned Move Orchestrator
// -------------------------------------------------------------------
class CineDirector {
    DirectorStyle currentStyle = DirectorStyle::Kubrick;
    bool activeMove = false;
    float moveTime = 0.0f;
    float moveDuration = 3.0f;

    private Vector3 _startPos;
    private Vector3 _endPos;
    private float _startFocal = 50.0f;
    private float _endFocal = 50.0f;

    void Configure(DirectorStyle style) {
        currentStyle = style;
    }

    void TriggerVertigoDolly(Vector3 fromPos, Vector3 toPos, float startFocalLength, float endFocalLength, float duration) {
        _startPos = fromPos;
        _endPos = toPos;
        _startFocal = startFocalLength;
        _endFocal = endFocalLength;
        moveDuration = duration;
        moveTime = 0.0f;
        activeMove = true;
    }

    void Update(float dt, Vector3 &out currentPos, float &out currentFocal) {
        if (!activeMove) return;

        moveTime += dt;
        float t = moveTime / moveDuration;
        if (t >= 1.0f) {
            t = 1.0f;
            activeMove = false;
        }

        // Linear interpolation for strict Kubrick / Vertigo moves (no unintended ease-in/out)
        currentPos = _startPos + (_endPos - _startPos) * t;
        currentFocal = _startFocal + (_endFocal - _startFocal) * t;
    }
}

// -------------------------------------------------------------------
// TegeCineSuite — Master TEGE Behavior Component
// Attach to any entity with a Camera & Lens component in TEGE
// -------------------------------------------------------------------
class TegeCineSuite : TegeBehavior {
    [Property] int directorPreset = 0;
    [Property] uint64 primarySubjectId = 0;
    [Property] float defaultFocalLength = 35.0f;
    [Property] float keyLightRatio = 4.0f;
    [Property] bool enableDiagnosticOverlay = true;

    CineDirector director;
    CineFocusPuller focusPuller;
    CineGaffer gaffer;
    SecondOrderDynamics rigDynamics;

    void OnCreate() {
        director.Configure(DirectorStyle(directorPreset));
        Vector3 initialPos = GetPosition();
        rigDynamics.SetParams(2.0f, 1.0f, 0.0f, initialPos);
    }

    void SetDirector(DirectorStyle style) {
        directorPreset = int(style);
        director.Configure(style);
    }

    void OnUpdate(float dt) {
        // Auto focus puller tracking
        if (primarySubjectId != 0) {
            Vector3 subjPos = Entity_GetPosition(primarySubjectId);
            focusPuller.TrackSubject(GetPosition(), subjPos, dt);
        }

        // Live move updates
        Vector3 movePos;
        float focalLength = defaultFocalLength;
        if (director.activeMove) {
            director.Update(dt, movePos, focalLength);
            Vector3 dampedPos = rigDynamics.Update(dt, movePos, Vector3(0,0,0));
            SetPosition(dampedPos);
        }
    }

    void TriggerVertigoMove(Vector3 endPos, float endFocalLength, float duration) {
        director.TriggerVertigoDolly(GetPosition(), endPos, defaultFocalLength, endFocalLength, duration);
    }
}
