#pragma once

// Desktop integration: handing a path, a URL or an executable to the host
// desktop. Every one of these had been hand-rolled at the call site, five
// different ways (ShellExecuteA, posix_spawnp, fork+execlp, a local spawnOpen
// lambda), and most of the hand-rolled copies had a Windows branch and an empty
// #else. On Linux that meant "Open Folder" did nothing, "Run in Browser" started
// a server and opened no browser, and "Launch game" printed "Launching game..."
// while launching nothing.
//
// These four functions are the whole surface. They return false when the action
// could not be started, so a caller can say so instead of claiming success.
// LinuxPlatform.h keeps its own OpenWithDefault for Linux-only code; this header
// is the portable entry point and is what editor and tooling code should call.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin::Platform {

// Hand a file, directory or URL to the desktop's default handler.
// Windows: ShellExecute "open". macOS: `open`. Linux: `xdg-open`.
ENJIN_API bool OpenInDesktop(const std::string& pathOrUrl);

// Show a file in the desktop file manager, selecting it where the platform
// supports that. Falls back to opening the containing directory.
ENJIN_API bool RevealInFileManager(const std::string& path);

// Open a URL, preferring an installed Chromium-family browser.
// WebGPU support is spotty outside Chrome and Edge, so web previews want a
// browser that can actually render them rather than whatever the OS default is.
// Falls back to OpenInDesktop when no such browser is found.
ENJIN_API bool OpenUrlPreferChromium(const std::string& url);

// Start an executable and return immediately, without a console window and
// without waiting. workingDir may be empty to inherit the current directory.
ENJIN_API bool LaunchDetached(const std::string& exePath, const std::string& workingDir);

} // namespace Enjin::Platform
