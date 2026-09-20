// CI canary: proves the AngelScript runtime compiles and runs scene scripts
// in the golden play-mode capture. The smoke step greps for the OnStart line.
class GoldenSmoke : TegeBehavior {
    void OnStart() {
        Debug_Log("GoldenSmoke: script runtime is alive");
    }
}
