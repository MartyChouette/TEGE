#!/usr/bin/env python3
"""Fail when an example or built-in template reads a raw key for gameplay.

Examples and templates are what people copy. A script that polls a physical
key (Input_GetKeyDown(Key::R)) cannot be rebound, does not appear in the
controls screen, the controls hint or How to Play, and gets no gamepad or touch
route. So every control in Examples/ and builtin_templates/ goes through a
named action: a built-in GameAction, or a project action declared in the
project's .enjinproject under "input" > "customActions" and looked up with
InputAction_Find("Name").

Two checks:
  1. Any call to a raw key, mouse-button or pad-button read is reported, unless
     the line carries `// raw-input-ok: <reason>` with a non-empty reason (for
     things that are not buttons, such as a pointer drag or a tap position).
  2. Every InputAction_Find("Name") must name a built-in action or a
     customActions entry in that example's .enjinproject (or an action the same
     script creates with InputAction_Define("Name")). A typo returns -1 and the
     control silently does nothing.

Usage: python tools/check_example_input.py    (exit 0 = clean, 1 = findings)
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIRS = ["Examples", "builtin_templates"]
SKIP_PARTS = {"Build", "build", "dist"}

# The script API's raw polling reads (ScriptBindings_Input.cpp). Pointer
# position, deltas, scroll and pinch are not buttons and are not listed.
RAW_READS = [
    "Input_GetKeyDown", "Input_GetKey", "Input_GetKeyUp",
    "Input_GetMouseButtonDown", "Input_GetMouseButton", "Input_GetMouseButtonUp",
    "Input_GetGamepadButtonDown", "Input_GetGamepadButton",
]
RAW_RE = re.compile(r"\b(" + "|".join(RAW_READS) + r")\s*\(")
OPT_OUT_RE = re.compile(r"//\s*raw-input-ok:\s*(\S.*)?$")
FIND_RE = re.compile(r'\bInputAction_Find\s*\(\s*"([^"]*)"\s*\)')
DEFINE_RE = re.compile(r'\bInputAction_Define\s*\(\s*"([^"]*)"\s*\)')

# Display names of the built-in actions (kActionInfo in InputActionMap.cpp);
# InputAction_Find matches on these.
BUILTIN_NAMES = {
    "Move Forward", "Move Back", "Move Left", "Move Right", "Jump", "Sprint",
    "Crouch", "Dash", "Interact", "Attack", "Block", "Pause",
    "Look Up", "Look Down", "Look Left", "Look Right",
    "Camera Zoom In", "Camera Zoom Out", "Confirm", "Cancel",
    "Menu Up", "Menu Down", "Menu Left", "Menu Right", "Advance Dialogue",
}


def strip_comment(line):
    """The code part of a line, ignoring // comments outside string literals."""
    in_str = False
    i = 0
    while i < len(line):
        c = line[i]
        if c == "\\" and in_str:
            i += 2
            continue
        if c == '"':
            in_str = not in_str
        elif not in_str and line.startswith("//", i):
            return line[:i]
        i += 1
    return line


def project_actions(script):
    """customActions names of the nearest .enjinproject above the script, or
    None when the script is not inside a project folder (a template)."""
    for d in script.parents:
        if d == ROOT:
            break
        projects = sorted(d.glob("*.enjinproject"))
        if projects:
            names = set()
            for p in projects:
                try:
                    data = json.loads(p.read_text(encoding="utf-8"))
                except (OSError, ValueError) as e:
                    names.add(None)
                    print(f"warning: cannot read {p.relative_to(ROOT)}: {e}", file=sys.stderr)
                    continue
                for a in data.get("input", {}).get("customActions", []):
                    if isinstance(a, dict) and a.get("name"):
                        names.add(a["name"])
            return names
    return None


def main():
    scripts = []
    for top in SCAN_DIRS:
        base = ROOT / top
        if not base.is_dir():
            continue
        for p in sorted(base.rglob("*.as")):
            if SKIP_PARTS.intersection(p.relative_to(ROOT).parts):
                continue
            scripts.append(p)

    findings = []
    for script in scripts:
        rel = script.relative_to(ROOT).as_posix()
        lines = script.read_text(encoding="utf-8", errors="replace").splitlines()
        text = "\n".join(lines)
        defined = set(DEFINE_RE.findall(text))
        declared = None  # loaded on first InputAction_Find

        for n, line in enumerate(lines, 1):
            code = strip_comment(line)
            if RAW_RE.search(code):
                m = OPT_OUT_RE.search(line)
                if not m:
                    findings.append(f"{rel}:{n}: raw input read: {line.strip()}")
                elif not (m.group(1) or "").strip():
                    findings.append(f"{rel}:{n}: raw-input-ok needs a reason: {line.strip()}")
            for name in FIND_RE.findall(code):
                if name in BUILTIN_NAMES or name in defined:
                    continue
                if declared is None:
                    declared = project_actions(script) or set()
                if name not in declared:
                    findings.append(f'{rel}:{n}: InputAction_Find("{name}") is not a built-in action '
                                    f"or a customActions name in the project file")

    if findings:
        print("\n".join(findings))
        print(f"\n{len(findings)} problem(s). Read a named action instead "
              "(InputAction_IsPressed(GameAction::...) or a project action from "
              "InputAction_Find), or mark a non-button read with // raw-input-ok: <reason>.")
        return 1
    print(f"ok: {len(scripts)} scripts, no raw gameplay input")
    return 0


if __name__ == "__main__":
    sys.exit(main())
