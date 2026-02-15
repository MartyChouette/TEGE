// Single translation unit that provides the miniaudio implementation.
// Both MiniaudioBackend.cpp and SimpleAudio.cpp include miniaudio.h
// (without MINIAUDIO_IMPLEMENTATION) and use the symbols defined here.
#ifdef _WIN32
#define NOMINMAX
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
