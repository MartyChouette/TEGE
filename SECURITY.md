# Security Policy

## Reporting a vulnerability

If you find a security issue in the engine, the editor, the web player, or an
exported game's runtime, please report it privately first:

- **Preferred:** open a private report via
  [GitHub Security Advisories](https://github.com/MartyChouette/TEGE/security/advisories/new)
- **Or email:** greatmartyscott@gmail.com with `[TEGE SECURITY]` in the subject

Please include what you found, where (file/system), and how to reproduce it.
You'll get an acknowledgment as soon as I see it, this is a single-developer
project, so response time is best-effort, but security reports go to the front
of the queue.

Please don't open public issues for unpatched vulnerabilities.

## Scope notes

Things that are **by design** and not vulnerabilities:
- `.enjpak` obfuscation is XOR + CRC32 and documented as non-cryptographic
  it is casual asset protection, not DRM or encryption.
- The editor's MCP server and dev web server bind localhost only and are off
  by default; enabling them is an explicit local choice.

Things that are **very much in scope**:
- Memory-safety issues reachable through loaded content (scenes, models,
  images, audio, packs), the engine loads community-made assets.
- Script sandbox escapes (AngelScript is sandboxed with an instruction limit
  and confined `#include` paths).
- Path traversal anywhere a file path comes from content.
- Network-facing code (LAN multiplayer, HTTP client).

## Supported versions

The latest release receives fixes. There is no backporting to older previews.
