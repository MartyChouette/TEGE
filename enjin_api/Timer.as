// Timer.as — Countdown and repeating timer utility
// Usage:
//   class Spawner : TegeBehavior {
//       Timer spawnTimer;
//
//       void OnStart() {
//           spawnTimer.Start(2.0f, true); // 2 seconds, repeating
//       }
//
//       void OnUpdate(float dt) {
//           spawnTimer.Update(dt);
//           if (spawnTimer.JustFinished()) {
//               SpawnEnemy();
//           }
//       }
//   }

class Timer {
    private float duration;
    private float elapsed;
    private bool running;
    private bool repeating;
    private bool finished;
    private bool justFinishedFlag;
    private int repeatCount;
    private int maxRepeats; // -1 = infinite

    Timer() {
        duration = 0.0f;
        elapsed = 0.0f;
        running = false;
        repeating = false;
        finished = false;
        justFinishedFlag = false;
        repeatCount = 0;
        maxRepeats = -1;
    }

    // Start a one-shot timer
    void Start(float dur) {
        duration = dur;
        elapsed = 0.0f;
        running = true;
        repeating = false;
        finished = false;
        justFinishedFlag = false;
        repeatCount = 0;
        maxRepeats = -1;
    }

    // Start a repeating timer
    void Start(float dur, bool repeat) {
        Start(dur);
        repeating = repeat;
    }

    // Start a repeating timer with max repeat count
    void Start(float dur, bool repeat, int maxReps) {
        Start(dur);
        repeating = repeat;
        maxRepeats = maxReps;
    }

    // Stop the timer
    void Stop() {
        running = false;
    }

    // Resume a stopped timer
    void Resume() {
        if (!finished) running = true;
    }

    // Reset and stop
    void Reset() {
        elapsed = 0.0f;
        running = false;
        finished = false;
        justFinishedFlag = false;
        repeatCount = 0;
    }

    // Reset and immediately restart
    void Restart() {
        elapsed = 0.0f;
        running = true;
        finished = false;
        justFinishedFlag = false;
        repeatCount = 0;
    }

    // Call every frame
    void Update(float dt) {
        justFinishedFlag = false;
        if (!running || finished) return;

        elapsed += dt;
        if (elapsed >= duration) {
            justFinishedFlag = true;
            repeatCount++;

            if (repeating) {
                if (maxRepeats >= 0 && repeatCount >= maxRepeats) {
                    finished = true;
                    running = false;
                } else {
                    // Carry over excess time for accurate repeating
                    elapsed -= duration;
                }
            } else {
                finished = true;
                running = false;
                elapsed = duration;
            }
        }
    }

    // True only on the frame the timer completes (or each repeat cycle)
    bool JustFinished() const {
        return justFinishedFlag;
    }

    // True if the timer has completed (and is not repeating, or max repeats reached)
    bool IsFinished() const {
        return finished;
    }

    // True if the timer is actively counting
    bool IsRunning() const {
        return running;
    }

    // Progress from 0 to 1
    float GetProgress() const {
        if (duration <= 0.0f) return 1.0f;
        return Clamp(elapsed / duration, 0.0f, 1.0f);
    }

    // Time remaining
    float GetRemaining() const {
        float r = duration - elapsed;
        if (r < 0.0f) return 0.0f;
        return r;
    }

    // Time elapsed in current cycle
    float GetElapsed() const {
        return elapsed;
    }

    // Total duration
    float GetDuration() const {
        return duration;
    }

    // Number of times the timer has completed
    int GetRepeatCount() const {
        return repeatCount;
    }
}
