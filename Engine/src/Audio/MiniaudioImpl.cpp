// Single translation unit that provides the miniaudio implementation.
// AudioEngine.cpp includes miniaudio.h without MINIAUDIO_IMPLEMENTATION and
// uses the symbols defined here. It is the only consumer now that the unused
// MiniaudioBackend has been retired; this stays a separate TU so the ~90k-line
// implementation is compiled once rather than on every edit to the engine.
#ifdef _WIN32
#define NOMINMAX
#endif

// Vorbis. miniaudio only compiles its Vorbis decoder `#ifdef
// STB_VORBIS_INCLUDE_STB_VORBIS_H`, and this TU did not include stb_vorbis, so
// every build shipped without one -- while the editor offered `.ogg` in every
// audio file filter, the thumbnailer accepted it, and BuildPipeline packed it
// into the game. LoadClip only checks that the file opens, so nothing failed at
// import: you picked an ogg, it showed as assigned, the build succeeded, and
// the sound was silent at play time. A comment in AudioEngine.cpp asserted
// "miniaudio supports Vorbis natively", which is true of miniaudio and was not
// true of this build.
//
// The decoder ships inside the miniaudio dependency we already fetch
// (extras/stb_vorbis.c) and miniaudio_SOURCE_DIR is on the include path, so
// this costs a dependency of nothing.
//
// The split include is miniaudio's documented requirement, not a style choice:
// the HEADER half has to be visible before miniaudio's implementation so its
// Vorbis path compiles, and the implementation half has to come after.
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

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

// The other half of stb_vorbis, after miniaudio as its docs require.
// Third-party C compiled as C++: its warnings are its own and are silenced here
// rather than repo-wide, the same way the ALSA one above is.
#undef STB_VORBIS_HEADER_ONLY
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4245 4456 4457 4701 4702 4100 4244)
#endif
#include "extras/stb_vorbis.c"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
