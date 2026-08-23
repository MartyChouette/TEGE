# Your Work Outlives This Engine

TEGE's position on data longevity and vendor lock-in, stated as policy. Games and
their makers should never lose work to a format, a service, or a company.

## What we commit to

**Every format you author in is human-readable.** Scenes (`.enjin`), projects
(`.enjinproject`), prefabs (`.enjprefab`), and shader graphs (`.enjshader`) are
plain JSON. You can read them in a text editor, diff them in version control,
and parse them with thirty lines of any language. No authored work is ever
trapped in an opaque binary.

**The one packed format is openly unpackable.** Exported games bundle assets
into `.enjpak` for convenience, not for capture: the packer AND unpacker ship in
this repository's source, the obfuscation is documented as non-cryptographic,
and exports also emit loose `assets/` and `scripts/` folders beside the
executable. Nothing about your shipped game is unrecoverable.

**No required services, accounts, or phone-home.** The engine and editor run
entirely offline. Optional integrations with proprietary SDKs (Steamworks,
DLSS, XeSS, OIDN) are exactly that - optional, OFF by default at build time,
and the engine is fully functional without every one of them. Protocol
integrations (like the editor's MCP server) use open, published protocols any
implementation can speak.

**The source stops being restricted on a schedule.** TEGE is Business Source
License 1.1: you can read, modify, and build the full source today, and each
release automatically converts to Apache 2.0 four years after it ships. The
restrictions expire; the code does not. Games you make are yours outright from
day one - sell them, port them, do anything.

**Your games ship self-contained.** An exported game is an executable plus data
files in a folder. It needs no launcher, no runtime service, and no blessing
from us to keep working after we are gone.

## Why this is written down

Engines die, companies pivot, services sunset. The measure of respect for the
people who build on a tool is what happens to their work when the tool's maker
disappears. Everything above is designed so the honest answer is: nothing.
