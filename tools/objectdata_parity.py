#!/usr/bin/env python3
"""The per-entity GPU layout is declared once. Check it has stayed that way.

Until 2026-09-15 this buffer was written out three times by hand -- the C++
`WebObjectDataUBO` and a `struct ObjectData` inside each of PBR_WGSL and
OUTLINE_WGSL, the latter two being string literals a compiler cannot see into.
Adding a field was a three-place edit, and missing one produced a green build, a
green shader compile, green tests, and every material on web read at shifted
offsets at once.

There is now one list, in WebObjectDataLayout.h. The C++ struct is generated
from it, and both shaders splice in a generated WGSL declaration instead of
carrying their own. They cannot drift, because there is nothing left to drift
from. This checks that nobody has quietly undone that:

  - no hand-written `struct ObjectData` in the shader header
  - both shaders still splice ENJIN_WEB_OBJECTDATA_WGSL
  - the C++ struct is still generated, not re-expanded by hand
  - the static_assert still matches what the field list adds up to

    python tools/objectdata_parity.py           # the report
    python tools/objectdata_parity.py --strict  # exit 1 on any of the above
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAYOUT = ROOT / "Engine/include/Enjin/Renderer/WebGPU/WebObjectDataLayout.h"
WGSL = ROOT / "Engine/include/Enjin/Renderer/WebGPU/WebShaderData.h"
CPP = ROOT / "Engine/src/ECS/Systems/RenderSystem.cpp"

# What each row costs in the buffer. WGSL and C++ agree on these only because
# the list states the alignment; a bare Math::Vector3 is not 16-byte aligned.
SIZES = {"mat4x4<f32>": 64, "vec4<f32>": 16, "vec3<f32>": 12, "vec2<f32>": 8,
         "f32": 4, "i32": 4, "u32": 4}


def rows():
    """(cpp_decl, name, wgsl_type) for every field in the one list."""
    src = LAYOUT.read_text(encoding="utf-8", errors="ignore")
    at = src.find("#define ENJIN_WEB_OBJECTDATA_FIELDS(X)")
    if at < 0:
        return None, "no ENJIN_WEB_OBJECTDATA_FIELDS in WebObjectDataLayout.h"
    body = []
    for line in src[at:].split("\n")[1:]:
        body.append(line)
        if not line.rstrip().endswith("\\"):
            break
    found = re.findall(r'X\(\s*([^,]+?)\s*,\s*(\w+)\s*,\s*"([^"]+)"', "\n".join(body))
    if not found:
        return None, "the field list has no rows"
    return found, None


def main():
    strict = "--strict" in sys.argv
    fields, err = rows()
    if err:
        print(f"FAIL  {err}")
        return 1

    unknown = [w for _, _, w in fields if w not in SIZES]
    if unknown:
        print(f"FAIL  unknown WGSL type(s) {sorted(set(unknown))} -- teach SIZES about them")
        return 1
    raw = sum(SIZES[w] for _, _, w in fields)
    aligned = (raw + 15) // 16 * 16
    print(f"One list: {len(fields)} fields, {raw} bytes -> sizeof {aligned}")

    problems = []

    cpp = CPP.read_text(encoding="utf-8", errors="ignore")
    if "ENJIN_WEB_OBJECTDATA_FIELDS(ENJIN_WEB_OBJECTDATA_MEMBER)" not in cpp:
        problems.append("WebObjectDataUBO is no longer generated from the list "
                        "(someone expanded the members by hand)")
    else:
        print("  ok  WebObjectDataUBO is generated from the list")

    m = re.search(r"static_assert\(sizeof\(WebObjectDataUBO\) == (\d+)", cpp)
    if not m:
        problems.append("no static_assert on sizeof(WebObjectDataUBO)")
    elif int(m.group(1)) != aligned:
        problems.append(f"static_assert says {m.group(1)}, the list adds up to {aligned}")
    else:
        print(f"  ok  static_assert agrees at {aligned} bytes")

    shaders = WGSL.read_text(encoding="utf-8", errors="ignore")
    hand = shaders.count("struct ObjectData {")
    if hand:
        problems.append(f"{hand} hand-written `struct ObjectData` back in WebShaderData.h -- "
                        "splice ENJIN_WEB_OBJECTDATA_WGSL instead")
    else:
        print("  ok  no hand-written ObjectData in the shader header")

    splices = shaders.count("ENJIN_WEB_OBJECTDATA_WGSL")
    if splices < 2:
        problems.append(f"{splices} shader(s) splice the generated struct, expected 2 "
                        "(PBR_WGSL and OUTLINE_WGSL)")
    else:
        print(f"  ok  {splices} shaders splice the generated declaration")

    if problems:
        print("\nThe layout has been un-singled:")
        for p in problems:
            print(f"  {p}")
        print("\nAdd a field by adding ONE row to ENJIN_WEB_OBJECTDATA_FIELDS in")
        print("WebObjectDataLayout.h. The struct and both shaders follow from it.")
        return 1 if strict else 0

    print("\nOne declaration, two generated readers. Nothing to drift.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
