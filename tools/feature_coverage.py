"""Which engine features does the capture harness actually render?

WHY THIS EXISTS. On 2026-09-21 four features were listed as having no capture
coverage. Three of them turned out to be BROKEN the moment something finally
rendered them: animation LOD did nothing in an exported game, Water3D foam did
nothing outside the editor viewport, and a custom shader drew as the base
material. After each fix every pre-existing capture stayed byte-identical --
which is the same fact as "nothing authored it", and exactly why none were
caught.

So "what is uncovered?" is the highest-value question the harness can be asked,
and until now it was answered from memory, by whoever happened to remember. That
is how the list got to four in the first place. This makes it a measurement.

HOW IT WORKS. The serializer's registry is the authority on what a scene can
contain: every serializable component appears exactly once as an ENJIN_SERDES
row naming its JSON key. Scene files are JSON, so the keys a project authors are
readable directly. The intersection is coverage; the difference is the list.

WHAT IT CANNOT SEE, stated because a coverage tool that overstates its reach is
worse than none. It counts a component as covered when some captured project
AUTHORS it -- not when a capture would notice it changing. LODLadder authored
LODComponent for a day while every sphere resolved to level 0, and this tool
would have called that covered. Only a control run answers that question, and
only a person can decide what the control should be.

It also sees COMPONENTS, not material modes. The dither, palette, lightmapped,
surface-noise and vertex-snap modes are fields on MaterialComponent rather than
components of their own, so a scene authoring any material counts as authoring
the material. --modes reports those separately by scanning for the fields.

USAGE
    python tools/feature_coverage.py
    python tools/feature_coverage.py --sweep-dir <harness outdir>   # include templates
    python tools/feature_coverage.py --modes                        # material modes too
    python tools/feature_coverage.py --all-projects                 # ignore the manifest

Templates ship as generators rather than as scenes on disk -- `EnjinEditor
--new-from-template` builds them -- so they can only be scanned from a directory
a harness sweep already instantiated them into (`<outdir>/_tmpl/*`). Without
--sweep-dir the report says they were not scanned rather than counting them as
uncovered, because "not measured" and "absent" are different answers.
"""

import argparse
import io
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERIALIZER = os.path.join(ROOT, 'Engine', 'src', 'Scene', 'SceneSerializer.cpp')
MANIFEST = os.path.join(ROOT, 'tools', 'harness_manifest.json')

# Material fields that switch on a whole rendering MODE. Each is a feature in its
# own right with its own shader path, and none is a component, so the component
# sweep cannot see any of them. The value is what counts as "switched on".
MATERIAL_MODES = {
    'ditherGradient':        lambda v: bool(v),
    'ditherTransparency':    lambda v: bool(v),
    'paletteIndexed':        lambda v: bool(v),
    'lightmapped':           lambda v: bool(v),
    'surfaceNoiseScale':     lambda v: isinstance(v, (int, float)) and v > 0,
    'vertexSnapping':        lambda v: bool(v),
    'flatShading':           lambda v: bool(v),
    'sdfText':               lambda v: bool(v),
    'excludeFromCelShading': lambda v: bool(v),
    'triplanar':             lambda v: bool(v),
    'parallaxScale':         lambda v: isinstance(v, (int, float)) and v > 0,
}


def registry_components():
    """(json key, C++ type) for every serializable component, from the registry."""
    src = io.open(SERIALIZER, encoding='utf-8', errors='replace').read()
    rows = re.findall(r'ENJIN_SERDES\(\s*"([^"]+)"\s*,\s*ECS::(\w+)', src)
    # The #define itself matches nothing (its parameters are KEY/TYPE), so these
    # are the real table. Deduplicate loudly: a key registered twice is a
    # serializer bug, not something to quietly average over.
    seen, out = set(), []
    for key, typ in rows:
        if key in seen:
            print('WARNING: %s is registered twice in the serdes table' % key,
                  file=sys.stderr)
            continue
        seen.add(key)
        out.append((key, typ))
    return out


def manifest_projects():
    """(name, project path or None, template id or None) for every harness entry."""
    m = json.load(io.open(MANIFEST, encoding='utf-8'))
    return [(p['name'], p.get('project'), p.get('template')) for p in m['projects']]


def scene_files(project_file):
    """Every .enjin under the project's own directory."""
    base = os.path.dirname(project_file)
    out = []
    for dirpath, _dirs, files in os.walk(base):
        out += [os.path.join(dirpath, f) for f in files if f.endswith('.enjin')]
    return out


def keys_in_scene(path):
    """Top-level entity keys, plus which material modes are switched on."""
    try:
        doc = json.load(io.open(path, encoding='utf-8'))
    except (OSError, ValueError):
        return set(), set()
    keys, modes = set(), set()
    for ent in doc.get('entities', []):
        if not isinstance(ent, dict):
            continue
        keys |= set(ent.keys())
        mat = ent.get('material')
        if isinstance(mat, dict):
            for field, is_on in MATERIAL_MODES.items():
                if field in mat and is_on(mat[field]):
                    modes.add(field)
    return keys, modes


def collect_sources(args):
    """(label, scene path) for everything in scope, plus how many templates we got."""
    sources = []
    scanned_templates = 0

    if args.all_projects:
        examples = os.path.join(ROOT, 'Examples')
        for name in sorted(os.listdir(examples)):
            proj_dir = os.path.join(examples, name)
            if not os.path.isdir(proj_dir):
                continue
            for f in os.listdir(proj_dir):
                if f.endswith('.enjinproject'):
                    for s in scene_files(os.path.join(proj_dir, f)):
                        sources.append((name, s))
        return sources, scanned_templates

    for name, project, template in manifest_projects():
        if project:
            pf = os.path.join(ROOT, project)
            if os.path.isfile(pf):
                for s in scene_files(pf):
                    sources.append((name, s))
        elif template and args.sweep_dir:
            stage = os.path.join(args.sweep_dir, '_tmpl', name)
            if os.path.isdir(stage):
                scanned_templates += 1
                for dirpath, _d, files in os.walk(stage):
                    for f in files:
                        if f.endswith('.enjin'):
                            sources.append((name, os.path.join(dirpath, f)))
    return sources, scanned_templates


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--sweep-dir',
                    help='a harness outdir, so instantiated templates are scanned too')
    ap.add_argument('--modes', action='store_true', help='also report material modes')
    ap.add_argument('--all-projects', action='store_true',
                    help='scan every project in Examples/, not only the captured ones')
    ap.add_argument('--quiet', action='store_true',
                    help='counts and the uncovered list only')
    args = ap.parse_args()

    components = registry_components()
    sources, scanned_templates = collect_sources(args)

    authored = {}
    modes_authored = {}
    for label, scene in sources:
        keys, modes = keys_in_scene(scene)
        for k in keys:
            authored.setdefault(k, set()).add(label)
        for m in modes:
            modes_authored.setdefault(m, set()).add(label)

    covered = [(k, t) for k, t in components if k in authored]
    missing = [(k, t) for k, t in components if k not in authored]

    print('Feature coverage: %d of %d serializable components are authored by a '
          'captured project (%.0f%%)'
          % (len(covered), len(components),
             100.0 * len(covered) / max(len(components), 1)))
    print('  scenes scanned: %d' % len(sources))
    if not args.all_projects:
        total_templates = sum(1 for _n, _p, t in manifest_projects() if t)
        if args.sweep_dir:
            print('  templates scanned: %d of %d' % (scanned_templates, total_templates))
        elif total_templates:
            print('  templates NOT scanned (%d): pass --sweep-dir <harness outdir> to '
                  'include them. They are generated by the editor, not stored as scenes.'
                  % total_templates)

    print()
    print('UNCOVERED (%d) - nothing the harness captures authors these, so no capture '
          'can tell whether they still work:' % len(missing))
    for key, typ in missing:
        print('  %-34s %s' % (key, typ))

    if args.modes:
        print()
        on = sorted(m for m in MATERIAL_MODES if m in modes_authored)
        off = sorted(m for m in MATERIAL_MODES if m not in modes_authored)
        print('MATERIAL MODES - fields rather than components, so the sweep above '
              'cannot see them:')
        for m in on:
            print('  covered    %-24s %s' % (m, ', '.join(sorted(modes_authored[m]))))
        for m in off:
            print('  UNCOVERED  %s' % m)

    if not args.quiet and covered:
        print()
        print('Covered, with the projects that author them:')
        for key, _typ in covered:
            who = sorted(authored[key])
            shown = ', '.join(who[:4]) + (' +%d more' % (len(who) - 4) if len(who) > 4 else '')
            print('  %-34s %s' % (key, shown))

    # Non-zero exit is deliberately NOT used for "something is uncovered". Most
    # of these will stay uncovered for a long time, and a red CI on that trains
    # people to ignore it. This reports; a person decides what to do about it.
    return 0


if __name__ == '__main__':
    sys.exit(main())
