// Miniaudio C++ → C linkage shim.
// miniaudio.h is compiled as C++ in our build so its symbols get C++ name
// mangling. But miniaudio's Emscripten audio worklet JS code calls the
// C-linkage names (e.g. _ma_device__on_notification_unlocked).
// This shim runs after the WASM module is instantiated and aliases the
// mangled exports to their expected C names.
Module["onRuntimeInitialized"] = (function(prev) {
  return function() {
    var aliases = {
      "_ma_device__on_notification_unlocked": "__Z35ma_device__on_notification_unlockedP9ma_device"
    };
    for (var cName in aliases) {
      var mangled = aliases[cName];
      if (typeof Module[mangled] === "function") {
        Module[cName] = Module[mangled];
      }
    }
    if (prev) prev();
  };
})(Module["onRuntimeInitialized"]);
