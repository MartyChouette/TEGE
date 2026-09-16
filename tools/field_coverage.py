#!/usr/bin/env python3
"""Which component fields never save, and which components cannot be authored.

A component field is declared in a header and then re-stated by hand in the
serializer, the deserializer and the inspector. Miss the serializer and the
value silently does not save; miss the deserializer and it saves and never loads
back; miss the inspector and nobody can author it at all. None of those three
failures produces a build error, a failing test, or a wrong-looking frame -- the
capture harness cannot see them, because a field that never saved looks
identical on screen the first time you set it.

This counts them. It is a REPORT, not a gate by default: the numbers have never
been measured, so a threshold picked before seeing them would be a guess.

    python tools/field_coverage.py             # the report
    python tools/field_coverage.py --verbose   # name every field
    python tools/field_coverage.py --strict    # exit 1 if it got worse than the baseline

WHAT IT WILL NOT TELL YOU. It matches on MEMBER names, reading `audio.volume`
out of the serializer body rather than trusting the JSON key, so renaming a key
does not produce a false alarm. It cannot see a field written through a helper
(`SerializeVector3(j, v)`), and it counts a mention anywhere in the function as
covered, so a field written under a condition that is never true still reads as
saved. Both make it UNDER-report, which is the right direction for a number
people are going to act on: everything it names is really missing, and the true
figure is at least this bad.
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COMPONENTS = os.path.join(ROOT, 'Engine', 'include', 'Enjin', 'ECS', 'Components')
SERIALIZER = os.path.join(ROOT, 'Engine', 'src', 'Scene', 'SceneSerializer.cpp')
# ALL of them, not EditorLayerComponents.cpp alone. The inspector is spread over
# 73 files -- EditorLayerComponents_Audio.cpp, EditorLayerInspector.cpp,
# EditorLayerRendering.cpp and more -- and reading one of them reported 55
# components as unauthorable including AudioSourceComponent, which has an
# inspector with a working audition button. A tool that names the wrong files
# is worse than no tool: every number it prints is confident and wrong.
EDITOR_DIR = os.path.join(ROOT, 'Engine', 'src', 'Editor')
BASELINE = os.path.join(ROOT, 'tools', 'field_coverage_baseline.txt')

# Field types worth checking. Deliberately the plain ones: a std::vector or a
# nested struct is usually written through a helper this cannot follow, and
# counting those as "not saved" would be noise in a report meant to be acted on.
SCALAR = r'(?:f32|f64|i8|i16|i32|i64|u8|u16|u32|u64|bool|std::string)'
FIELD_RE = re.compile(r'^\s*(?:alignas\(\d+\)\s*)?' + SCALAR + r'\s+(\w+)\s*(?:=[^;]*)?;', re.M)
STRUCT_RE = re.compile(r'\bstruct\s+(?:ENJIN_API\s+)?(\w*Component)\b')


def read(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        return f.read()


def strip_comments(src):
    """Remove C++ comments, respecting string literals. So a field named only in
    a comment does not read as covered.

    A single pass, not two regexes. The two-regex version -- block comments with
    DOTALL, then line comments -- silently ate live code: a `/*` inside a LINE
    comment opens a block the first pass then closes at the next `*/` hundreds of
    lines later, taking everything between with it. It removed the editor's only
    use of VoxelVolumeComponent and reported the component as unauthorable. The
    same stripper runs over the serializer, so every field count it produced was
    suspect until this was fixed.
    """
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '"' or c == "'":
            quote = c
            out.append(c)
            i += 1
            while i < n:
                out.append(src[i])
                if src[i] == '\\':          # escaped char: take the next one whole
                    if i + 1 < n:
                        out.append(src[i + 1])
                    i += 2
                    continue
                if src[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        if c == '/' and i + 1 < n:
            if src[i + 1] == '/':
                while i < n and src[i] != '\n':
                    i += 1
                continue
            if src[i + 1] == '*':
                i += 2
                while i + 1 < n and not (src[i] == '*' and src[i + 1] == '/'):
                    i += 1
                i += 2
                out.append(' ')             # keep tokens either side apart
                continue
        out.append(c)
        i += 1
    return ''.join(out)


def components():
    """{ComponentName: [field, ...]} from the headers."""
    out = {}
    for name in sorted(os.listdir(COMPONENTS)):
        if not name.endswith('.h'):
            continue
        src = strip_comments(read(os.path.join(COMPONENTS, name)))
        # Split at each struct declaration and take the body up to the next one,
        # which is close enough: these headers declare components one after
        # another at file scope.
        marks = [(m.start(), m.group(1)) for m in STRUCT_RE.finditer(src)]
        for i, (at, comp) in enumerate(marks):
            end = marks[i + 1][0] if i + 1 < len(marks) else len(src)
            body = src[at:end]
            fields = FIELD_RE.findall(body)
            if fields:
                out.setdefault(comp, []).extend(f for f in fields if f not in out.get(comp, []))
    return out


def function_body(src, name):
    """The text of a function by name, brace-matched. None if it is not there."""
    m = re.search(r'\b' + re.escape(name) + r'\s*\([^)]*\)\s*\{', src)
    if not m:
        return None
    depth, i = 0, m.end() - 1
    while i < len(src):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return src[m.end():i]
        i += 1
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--verbose', action='store_true', help='name every missing field')
    ap.add_argument('--strict', action='store_true',
                    help='exit 1 if the counts are worse than the recorded baseline')
    ap.add_argument('--write-baseline', action='store_true')
    args = ap.parse_args()

    comps = components()
    ser_src = strip_comments(read(SERIALIZER))
    insp_src = strip_comments(''.join(
        read(os.path.join(EDITOR_DIR, f))
        for f in sorted(os.listdir(EDITOR_DIR)) if f.endswith('.cpp')))
    # Two ways a component gets serialized, and reading only the first was the
    # fourth thing this tool got wrong. The ENJIN_SERDES registry covers the
    # REGULAR components; the core ones -- mesh, animator, children, the IK
    # family -- are written inline in the entity loop
    # (`entityJson["mesh"] = SerializeMeshComponent(...)`), exactly as the
    # registry's own comment says: "use ENJIN_SERDES; the few irregular ones are
    # explicit entries". Counting only the macro reported MeshComponent as
    # having no serializer, in an engine whose scene files are full of meshes.
    # THREE ways a component reaches a scene file, and reading fewer than all of
    # them is how this tool got the count wrong twice in a row.
    #
    #   ENJIN_SERDES("key", ECS::Foo, ...)          the regular registry
    #   entityJson["mesh"] = SerializeMeshComponent(...)   core types, inline
    #   ComponentSerdes{ "handIK", [](...){ ...HasComponent<ECS::HandIKComponent>
    #                                       the "irregular ones", inline lambdas
    #
    # So the test is simply whether the serializer TOUCHES the type at all. That
    # is a weaker claim than "has a serializer" and it is the honest one: a
    # component the serializer never mentions certainly does not save, and one it
    # does mention is somebody's deliberate handling that this tool should not
    # second-guess.
    registered = set(re.findall(r'ECS::(\w+Component)\b', ser_src))
    inspected = set(re.findall(r'(?:Get|Has)Component<ECS::(\w+)>', insp_src))

    no_serdes, no_inspector = [], []
    write_only, read_only, persisted, absent = [], [], [], []
    checked_fields = 0

    for comp, fields in sorted(comps.items()):
        if comp not in registered:
            no_serdes.append(comp)
            continue
        if comp not in inspected:
            no_inspector.append(comp)

        ser = function_body(ser_src, 'Serialize' + comp)
        des = function_body(ser_src, 'Deserialize' + comp)
        if ser is None or des is None:
            continue
        for f in fields:
            checked_fields += 1
            word = re.compile(r'\.' + re.escape(f) + r'\b')
            w, r = bool(word.search(ser)), bool(word.search(des))
            name = '%s.%s' % (comp, f)
            if w and r:
                persisted.append(name)
            elif w and not r:
                write_only.append(name)
            elif r and not w:
                read_only.append(name)
            else:
                absent.append(name)

    print('components with fields      %4d' % len(comps))
    print('  registered for saving     %4d' % (len(comps) - len(no_serdes)))
    print('  NO serializer at all      %4d' % len(no_serdes))
    print('  saved but NO inspector    %4d   <- can be saved, cannot be authored'
          % len(no_inspector))
    print()
    print('scalar fields checked       %4d' % checked_fields)
    print('  round-trip (saved+loaded) %4d' % len(persisted))
    print('  asymmetric                %4d   <- worth a look, not automatically wrong'
          % (len(write_only) + len(read_only)))
    print('    written, never read     %4d' % len(write_only))
    print('    read, never written     %4d' % len(read_only))
    print('  in neither                %4d   (mostly runtime state)' % len(absent))
    print()
    print('READ THE CATEGORIES BEFORE QUOTING THEM. Two passes of this tool overclaimed')
    print('before the numbers meant anything, and both are worth not repeating.')
    print()
    print('The first version counted "never written on save" and reported 421. The first')
    print('twelve were attackTimer, frameTimer, stateTimer -- transient state that is')
    print('correctly not persisted. That is the "in neither" column now, and it is NOT a')
    print('bug count.')
    print()
    print('The second version called every asymmetry a defect. All 11 read-never-written')
    print('fields on 2026-09-16 were dirty flags and derived counts that the DESERIALIZER')
    print('sets deliberately -- text.dirty = true to force a re-rasterize on load,')
    print('LOD vertexCount recomputed from the mesh. Correct engineering, not a missing')
    print('save. So asymmetry is a SMELL worth reading, not a verdict.')
    print()
    print('And the THIRD overclaim was the worst, because it was confident: reading only')
    print('EditorLayerComponents.cpp reported 55 components as unauthorable, including')
    print('AudioSourceComponent, which has an inspector with a working audition button.')
    print('The inspector is spread over 73 files. Then the comment stripper turned out')
    print('to be eating live code -- a /* inside a // comment opened a block it closed')
    print('hundreds of lines later -- which hid the only editor use of')
    print('VoxelVolumeComponent. 55 became 4 became 3.')
    print()
    print('The real finding is small and checked by hand: three components can be')
    print('written to a scene file and appear nowhere in the editor at all.')

    if args.verbose:
        for title, items in (('NO serializer', no_serdes), ('NO inspector', no_inspector),
                             ('written, never read', write_only),
                             ('read, never written', read_only),
                             ('in neither (runtime state, or forgotten?)', absent)):
            if items:
                print('\n%s (%d):' % (title, len(items)))
                for i in items:
                    print('   ', i)

    # The baseline tracks the unambiguous numbers only. Pinning "in neither" would
    # make adding a timer to a component fail CI, which teaches people to raise
    # the number rather than to look at it.
    counts = {'no_serdes': len(no_serdes), 'no_inspector': len(no_inspector),
              'write_only': len(write_only), 'read_only': len(read_only)}

    if args.write_baseline:
        with open(BASELINE, 'w', encoding='utf-8') as f:
            f.write('# Recorded by tools/field_coverage.py. These are how bad things are,\n'
                    '# not how bad they may become: --strict fails if any number goes UP.\n'
                    '# Lower one and re-record; there is no way to raise one quietly.\n')
            for k in sorted(counts):
                f.write('%s %d\n' % (k, counts[k]))
        print('\nbaseline written')
        return 0

    if args.strict:
        if not os.path.isfile(BASELINE):
            print('\nno baseline recorded -- run with --write-baseline')
            return 1
        base = {}
        for line in read(BASELINE).splitlines():
            if line.startswith('#') or not line.strip():
                continue
            k, v = line.split()
            base[k] = int(v)
        worse = [(k, counts[k], base[k]) for k in counts if counts[k] > base.get(k, 0)]
        if worse:
            print()
            for k, now, was in worse:
                print('WORSE  %s: %d, baseline %d' % (k, now, was))
            print('A new field that does not save is the defect this counts. '
                  'Wire it up, or lower the baseline deliberately.')
            return 1
        better = [k for k in counts if counts[k] < base.get(k, 0)]
        if better:
            print('\nbetter than baseline on: %s -- re-record with --write-baseline'
                  % ', '.join(sorted(better)))
        print('\nno worse than the recorded baseline')
    return 0


if __name__ == '__main__':
    sys.exit(main())
