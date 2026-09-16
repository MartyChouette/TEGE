#!/usr/bin/env python3
"""Export each project, boot it, photograph it, and check what the pictures prove.

WHY THIS EXISTS. About 115 of the items on the backlog are not work, they are
"go and check whether this claim is true", and each one costs a person a
session. Worse, the bug class that has dominated recently is invisible to every
automatic check the project already has: a feature written on both render paths
and switched on in neither compiles clean, passes the suite, and goes through
Dawn without complaint. Water waves, the procedural sky, shore foam, the fluid
sim, orthographic cameras and the camera clear colour were all found by eye, one
at a time. A capture is the only thing that sees them.

    python tools/harness.py --list
    python tools/harness.py --only WaterVolume
    python tools/harness.py                     # everything in the manifest

Exit code is 0 only if every claim of every project passed.

Each project is EXPORTED first, through the same pipeline a person uses, and the
exported game is what gets booted. That is deliberate: the exported game is the
render path that had no capture coverage at all, and testing a project folder
instead would measure a path no player runs.

A NOTE ON WINDOWS: the player has no offscreen mode, so every project opens a
real window for a few seconds. A full sweep therefore takes over the screen for
several minutes and is not something to start while someone is working. Giving
the player a headless surface would fix that and would also let this run on a CI
box with no display; it is not done yet.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capture_claims  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, 'tools', 'harness_manifest.json')
EDITOR = os.path.join(ROOT, 'build', 'bin', 'Release', 'EnjinEditor.exe')
PLAYER = os.path.join(ROOT, 'build', 'bin', 'Release', 'EnjinPlayer.exe')


def export(project, outdir):
    """Run the real export pipeline. Returns (ok, note, game_dir)."""
    game_dir = os.path.join(outdir, '_export', project['name'])
    if not os.path.isfile(EDITOR):
        return False, 'no EnjinEditor.exe (build it first)', game_dir
    src = os.path.join(ROOT, project['project'])
    if not os.path.isfile(src):
        return False, 'no such project: %s' % project['project'], game_dir

    shutil.rmtree(game_dir, ignore_errors=True)
    os.makedirs(game_dir, exist_ok=True)
    try:
        proc = subprocess.run([EDITOR, '--build-desktop', src, game_dir],
                              capture_output=True, text=True,
                              timeout=project.get('export_timeout', 300))
    except subprocess.TimeoutExpired:
        return False, 'export timed out', game_dir
    if proc.returncode != 0:
        tail = (proc.stdout or '').strip().splitlines()[-1:] or ['no output']
        return False, 'export failed: %s' % tail[0], game_dir
    return True, 'exported', game_dir


def run(project, game_dir, outdir):
    """Boot the exported game and capture its frames. Returns (ok, note, basepaths)."""
    # The FRESHLY BUILT player, not the one the export copied in: BuildPipeline
    # ships a prebuilt EnjinPlayer, so an exported game can carry an engine three
    # versions old and testing it would measure the wrong tree.
    exe = os.path.join(game_dir, '_harness_player.exe')
    if not os.path.isfile(PLAYER):
        return False, 'no EnjinPlayer.exe (build it first)', []
    shutil.copyfile(PLAYER, exe)

    frames = project.get('frames', [90, 400])
    base = os.path.join(outdir, project['name'])
    cmd = [exe, '--golden', base, '--golden-frames', ','.join(str(f) for f in frames)]
    try:
        proc = subprocess.run(cmd, cwd=game_dir, capture_output=True,
                              timeout=project.get('timeout', 180))
    except subprocess.TimeoutExpired:
        return False, 'timed out', []
    if proc.returncode != 0:
        return False, 'exit code %d' % proc.returncode, []
    return True, 'booted', ['%s.f%04d' % (base, f) for f in frames]


def check(project, bases):
    """Evaluate this project's claims. Returns [(claim, ok, detail)]."""
    results = []
    claims = project.get('claims', {})

    if claims.get('draws', True):
        for b in bases:
            try:
                ok, detail = capture_claims.draws(b + '.ppm')
            except (OSError, ValueError) as e:
                ok, detail = False, str(e)
            results.append(('draws@%s' % b.rsplit('.', 1)[-1], ok, detail))

    anim = claims.get('animates')
    if anim is not None:
        if len(bases) < 2:
            results.append(('animates', False, 'needs two frames, got %d' % len(bases)))
        else:
            pct = anim.get('min_changed_pct', 0.5) if isinstance(anim, dict) else 0.5
            try:
                ok, detail = capture_claims.animates(bases[0] + '.ppm', bases[-1] + '.ppm',
                                                     min_changed_pct=pct)
            except (OSError, ValueError) as e:
                ok, detail = False, str(e)
            results.append(('animates', ok, detail))
    return results


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--only', action='append', help='run just this project (repeatable)')
    ap.add_argument('--list', action='store_true', help='show what is wired and exit')
    ap.add_argument('--outdir', help='keep captures here instead of a temp dir')
    ap.add_argument('--skip-export', action='store_true',
                    help='reuse the exports already in outdir (needs --outdir)')
    args = ap.parse_args()

    with open(MANIFEST, encoding='utf-8') as f:
        manifest = json.load(f)
    all_projects = manifest['projects']

    if args.list:
        print('%d project(s) wired:' % len(all_projects))
        for p in all_projects:
            print('  %-22s %-52s frames=%s' % (p['name'], p['project'], p.get('frames')))
        return 0

    projects = all_projects
    if args.only:
        wanted = set(args.only)
        projects = [p for p in all_projects if p['name'] in wanted]
        missing = wanted - {p['name'] for p in projects}
        if missing:
            print('not in the manifest: %s' % ', '.join(sorted(missing)))
            return 2

    outdir = args.outdir or tempfile.mkdtemp(prefix='enjin_harness_')
    os.makedirs(outdir, exist_ok=True)

    failures, ran = 0, 0
    for p in projects:
        game_dir = os.path.join(outdir, '_export', p['name'])
        if args.skip_export:
            if not os.path.isdir(game_dir):
                print('FAIL %-22s no existing export to reuse' % p['name'])
                failures += 1
                continue
        else:
            ok, note, game_dir = export(p, outdir)
            if not ok:
                print('FAIL %-22s %s' % (p['name'], note))
                failures += 1
                continue

        ok, note, bases = run(p, game_dir, outdir)
        if not ok:
            print('FAIL %-22s boot: %s' % (p['name'], note))
            failures += 1
            continue

        ran += 1
        for name, passed, detail in check(p, bases):
            print('%s %-22s %-14s %s' % ('pass' if passed else 'FAIL', p['name'], name, detail))
            if not passed:
                failures += 1

    print()
    print('captures in %s' % outdir)
    print('%d project(s) attempted, %d booted, %d failing check(s)'
          % (len(projects), ran, failures))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
