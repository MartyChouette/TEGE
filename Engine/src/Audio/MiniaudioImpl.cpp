// Single translation unit that provides the miniaudio implementation.
// AudioEngine.cpp includes miniaudio.h without MINIAUDIO_IMPLEMENTATION and
// uses the symbols defined here. It is the only consumer now that the unused
// MiniaudioBackend has been retired; this stays a separate TU so the ~90k-line
// implementation is compiled once rather than on every edit to the engine.
#ifdef _WIN32
#define NOMINMAX
#endif
#define MINIAUDIO_IMPLEMENTATION
// miniaudio's ALSA backend drops the return of two read() calls on its
// self-pipe wakeups. That is its code, not ours, and it is the only thing
// -Wunused-result flags in the whole build — silenced precisely here so the
// warning stays worth reading everywhere else.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif
#include "miniaudio.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
