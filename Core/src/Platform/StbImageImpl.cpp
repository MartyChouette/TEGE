// The one translation unit that compiles stb_image.
//
// It lives in Core because Core is the lowest layer that calls it: Window.cpp
// decodes the window icon. It used to be compiled inside Engine
// (VulkanImage.cpp, and WebGPUTextureManager.cpp on web), which left Core
// depending on a symbol in the library above it. On Linux that only linked
// because Assimp 5 happened to export its own stbi_load further down the link
// line; Assimp 6 does not, and a test that pulled in Window.o without
// VulkanImage.o stopped linking (2026-10-06).
//
// Everything else includes stb_image.h WITHOUT the implementation define.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
