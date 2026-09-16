#!/usr/bin/env python3
"""Boot each project, photograph it, and check what the photographs prove.

WHY THIS EXISTS. About 115 of the items on the backlog are not work, they are
"go and check whether this claim is true", and each one costs a person a
session. Worse, the class of bug that has dominated recently is invisible to
every automatic check the project already has: a feature written on both render
paths and switched on in neither compiles clean, passes every test, and compiles
through Dawn without complaint. Water waves, the procedural sky, shore foam, the
fluid sim, orthographic cameras and the camera clear colour were all found that
way, by eye, one at a time.

A capture is the only thing that sees it. So: run the game, take two pictures
far apart, and assert claims about them (tools/capture_claims.py).

    python tools/harness.py                 # run everything in the manifest
    python tools/harness.py --only playground
    python tools/harness.py --list

Exit code is 0 only if every claim of every project passed.

COVERAGE IS THE OPEN PART. A project can only be run here once it has been
EXPORTED, because the desktop runtime under test is the exported game -- that is
deliberate: it is the render path a player takes and the one path that had no
capture at all until EnjinPlayer grew --golden. Adding the 31 Examples and 16
templates means exporting each one, which is the next piece of this and is not
done yet. The manifest lists what is wired today and nothing more, so the report
never implies coverage it does not have.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capture_claims  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, 'tools', 'harness_manifest.json')


def run_desktop(project, outdir):
    """Boot the exported game, capture its frames. Returns (ok, note, basepaths)."""
    exe = os.path.join(ROOT, project['dist'], 'Game.exe')
    if not os.path.isfile(exe):
        return False, 'no Game.exe at %s (export it first)' % project['dist'], []

    # The freshly built player, not the one the export copied in: BuildPipeline
    # copies a PREBUILT EnjinPlayer.exe, so an exported game on disk can be an
    # engine or three old and would be tested instead of the tree.
    fresh = os.path.join(ROOT, 'build', 'bin', 'Release', 'EnjinPlayer.exe')
    if os.path.isfile(fresh):
        exe = os.path.join(ROOT, project['dist'], '_harness_player.exe')
        with open(fresh, 'rb') as src, open(exe, 'wb') as dst:
            dst.write(src.read())

    frames = project.get('frames', [90, 400])
    base = os.path.join(outdir, project['name'])
    cmd = [exe, '--golden', base, '--golden-frames', ','.join(str(f) for f in frames)]
    try:
        proc = subprocess.run(cmd, cwd=os.path.join(ROOT, project['dist']),
                              capture_output=True, timeout=project.get('timeout', 120))
    except subprocess.TimeoutExpired:
        return False, 'timed out', []
    if proc.returncode != 0:
        return False, 'exit code %d' % proc.returncode, []
    return True, 'booted', ['%s.f%04d' % (base, f) for f in frames]


def check(project, bases):
    """Evaluate this project's claims against its captures. Returns [(name, ok, detail)]."""
    results = []
    claims = project.get('claims', {})

    if claims.get('draws', True):
        for b in bases:
            try:
                ok, detail = capture_claims.draws(b + '.ppm')
            except (OSError, ValueError) as e:
                ok, detail = False, str(e)
            results.append(('draws@%s' % os.path.basename(b).split('.')[-1], ok, detail))

    anim = claims.get('animates')
    if anim is not None and len(bases) >= 2:
        pct = anim.get('min_changed_pct', 0.5) if isinstance(anim, dict) else 0.5
        try:
            ok, detail = capture_claims.animates(bases[0] + '.ppm', bases[-1] + '.ppm',
                                                 min_changed_pct=pct)
        except (OSError, ValueError) as e:
            ok, detail = False, str(e)
        results.append(('animates', ok, detail))
    elif anim is not None:
        results.append(('animates', False, 'needs two capture frames, got %d' % len(bases)))

    return results


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--only', help='run just this project by name')
    ap.add_argument('--list', action='store_true', help='show what is wired and exit')
    ap.add_argument('--outdir', help='keep captures here instead of a temp dir')
    args = ap.parse_args()

    with open(MANIFEST, encoding='utf-8') as f:
        manifest = json.load(f)
    projects = manifest['projects']
    if args.only:
        projects = [p for p in projects if p['name'] == args.only]
        if not projects:
            print('no project named %r in the manifest' % args.only)
            return 2

    if args.list:
        print('%d project(s) wired:' % len(manifest['projects']))
        for p in manifest['projects']:
            print('  %-18s %s  frames=%s' % (p['name'], p['dist'], p.get('frames')))
        return 0

    outdir = args.outdir or tempfile.mkdtemp(prefix='enjin_harness_')
    os.makedirs(outdir, exist_ok=True)

    failures = 0
    for p in projects:
        ok, note, bases = run_desktop(p, outdir)
        if not ok:
            print('FAIL %-18s boot: %s' % (p['name'], note))
            failures += 1
            continue
        for name, passed, detail in check(p, bases):
            print('%s %-18s %-14s %s' % ('pass' if passed else 'FAIL', p['name'], name, detail))
            if not passed:
                failures += 1

    print()
    print('captures in %s' % outdir)
    print('%d project(s), %d failing claim(s)' % (len(projects), failures))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
