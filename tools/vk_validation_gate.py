#!/usr/bin/env python3
"""Fail when a run produced Vulkan validation ERRORS.

CI's only Vulkan driver is lavapipe, a CPU renderer. No AMD, Intel or NVIDIA
driver is exercised anywhere, so vendor-specific bugs cannot be caught here at
all -- that gap is covered by hand, see _docs_internal/GPU_TEST_MATRIX.md.

API MISUSE is a different story. It is driver-independent, the validation layer
reports it identically on a software driver, and it is the class that produces
"works on my machine, black screen on theirs". So the smokes run with the layer
force-injected by the loader and hand their log to this.

    python3 tools/vk_validation_gate.py editor.log

Errors fail. Warnings print and do not.

Why warnings do not fail: measured 2026-09-06 against a real GPU, a 30-frame
golden run produced zero errors and ten warnings, every one of them
Undefined-Value-ShaderOutputNotConsumed -- the MRT velocity attachment, which
the shader writes and the blend state masks off with colorWriteMask = 0. That
is deliberate and understood. Gating on warnings would make this red on day one
for a thing nobody intends to change, and a gate that is red on day one gets
switched off. If a warning class ever earns a gate, promote that specific ID
into ERROR_IDS rather than the whole tier.

The caller is responsible for asserting the layer actually loaded. This script
cannot tell "clean run" from "layer never ran", and silently passing when the
layer is absent is exactly how the missing shader compiler hid for months.
"""

import re
import sys

# The layer prefixes every report with its severity.
ERROR_RE = re.compile(r"Validation Error")
WARN_RE = re.compile(r"Validation Warning")

# Bracketed VUID / message id, e.g. [ VUID-vkCmdDraw-None-08600 ].
ID_RE = re.compile(r"\[\s*([A-Za-z0-9_\-]+)\s*\]")

# Warning ids promoted to failures. Empty by design: add an id here only with a
# note saying what changed, so promotion is a deliberate act and not drift.
ERROR_IDS: "set[str]" = set()

# Error ids demoted to warnings, each with a reason. Baseline-pinned, the same
# shape tools/runtime_parity.py and tools/backend_parity.py use: known
# divergence is recorded with a reason and only NEW divergence fails.
#
# This exists for one specific case. The gate was measured on a real GPU (zero
# errors, editor and exported player both), but CI runs lavapipe, a CPU driver
# with its own extension support and limits. If lavapipe reports an error that
# hardware does not, pin THAT id here with a dated reason. Do not switch the
# gate off: an all-or-nothing gate that meets one false positive gets deleted,
# which is how this class went unwatched until 2026-09-06 in the first place.
#
# Anything pinned here is a debt, not a resolution. Say why, and say what would
# let it be removed.
IGNORED_IDS: "dict[str, str]" = {}


def main(argv):
    if len(argv) != 2:
        print("usage: vk_validation_gate.py <logfile>", file=sys.stderr)
        return 2

    try:
        with open(argv[1], "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except OSError as e:
        print(f"vk_validation_gate: cannot read {argv[1]}: {e}", file=sys.stderr)
        return 2

    def ids_after(line, match):
        # Only brackets to the RIGHT of the severity marker. The engine writes
        # its own log lines to the same stream and the layer's output lands
        # mid-line, so the FIRST bracket on the line is often the engine's
        # "[INFO ]" rather than the VUID.
        return ID_RE.findall(line[match.end():])

    errors, warnings, ignored = [], [], []
    for line in lines:
        m = ERROR_RE.search(line)
        if m:
            ids = ids_after(line, m)
            if any(i in IGNORED_IDS for i in ids):
                ignored.append((line.rstrip(), ids))
            else:
                errors.append((line.rstrip(), ids))
            continue
        m = WARN_RE.search(line)
        if m:
            ids = ids_after(line, m)
            if any(i in ERROR_IDS for i in ids):
                errors.append((line.rstrip(), ids))
            else:
                warnings.append((line.rstrip(), ids))

    def tally(entries):
        counts = {}
        for _, ids in entries:
            key = ids[0] if ids else "(no id)"
            counts[key] = counts.get(key, 0) + 1
        return sorted(counts.items(), key=lambda kv: -kv[1])

    if warnings:
        print(f"vk_validation_gate: {len(warnings)} warning(s), not fatal:")
        for name, n in tally(warnings):
            print(f"  {n:4d}  {name}")

    if ignored:
        print(f"vk_validation_gate: {len(ignored)} PINNED error(s), not fatal:")
        for name, n in tally(ignored):
            print(f"  {n:4d}  {name}  <- {IGNORED_IDS.get(name, 'no reason recorded')}")

    if not errors:
        print(f"vk_validation_gate: OK - no validation errors in {argv[1]}")
        return 0

    print(f"vk_validation_gate: {len(errors)} VALIDATION ERROR(S) in {argv[1]}")
    for name, n in tally(errors):
        print(f"  {n:4d}  {name}")
    print("")
    print("First 20 in full:")
    for line, _ in errors[:20]:
        print(f"  {line}")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
