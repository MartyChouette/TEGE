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
    python tools/harness.py                     # every project, desktop only
    python tools/harness.py --web               # ... and in a browser, compared

Exit code is 0 only if every claim of every project passed.

--web exports each project for the browser as well, serves it, captures it in
headless Chrome, and compares the two runtimes. The comparison is of VERDICTS
and not pixels: the captures are different resolutions and go through different
post-process chains, so a pixel diff between them would be noise with a number
attached. What matters is whether the two AGREE -- a feature written on both
paths and switched on in only one draws a perfectly valid frame on both, passes
every single-runtime claim on both, and differs only in whether anything moves.
Both runtimes advance by the same fixed simulation step during a capture, or
frame N would be a different moment on each and the comparison would mean
nothing.

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
import contextlib
import functools
import http.server
import json
import os
import shutil
import socketserver
import subprocess
import sys
import tempfile
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capture_claims  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, 'tools', 'harness_manifest.json')
EDITOR = os.path.join(ROOT, 'build', 'bin', 'Release', 'EnjinEditor.exe')
PLAYER = os.path.join(ROOT, 'build', 'bin', 'Release', 'EnjinPlayer.exe')
WEB_CAPTURE = os.path.join(ROOT, 'tools', 'web_capture.mjs')

# The simulation step a capture run advances by, on BOTH runtimes. It has to be
# the same number or a desktop frame and a web frame of the same ordinal are
# different moments and comparing them means nothing. Desktop gets it from
# --golden; web gets it from ?fixedDelta= on the URL.
CAPTURE_DELTA = 1.0 / 60.0


@contextlib.contextmanager
def serve(directory):
    """A local HTTP server over one directory, for the length of a capture.

    A web build cannot be opened from file:// -- the wasm fetch is cross-origin
    against a file URL and the engine never boots -- so there has to be a server,
    and starting one per project keeps the harness a single command instead of a
    command plus a thing you must remember to run.
    """
    class Quiet(http.server.SimpleHTTPRequestHandler):
        # One line per request drowns the report it is printed next to, and none
        # of it is what the run is measuring.
        def log_message(self, *a):
            pass

    handler = functools.partial(Quiet, directory=directory)
    # Port 0 lets the OS pick a free one: a fixed port collides with whatever
    # the person at the machine already has serving, and that failure looks like
    # the web build being broken.
    with socketserver.TCPServer(('127.0.0.1', 0), handler) as httpd:
        httpd.allow_reuse_address = True
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()
        try:
            yield httpd.server_address[1]
        finally:
            httpd.shutdown()


def export_web(project, outdir):
    """Export for web through the real pipeline. Returns (ok, note, dir)."""
    web_dir = os.path.join(outdir, '_web', project['name'])
    if not os.path.isfile(EDITOR):
        return False, 'no EnjinEditor.exe', web_dir
    src = os.path.join(ROOT, project['project'])
    shutil.rmtree(web_dir, ignore_errors=True)
    os.makedirs(web_dir, exist_ok=True)
    try:
        proc = subprocess.run([EDITOR, '--build-web', src, web_dir],
                              capture_output=True, text=True,
                              timeout=project.get('export_timeout', 300))
    except subprocess.TimeoutExpired:
        return False, 'web export timed out', web_dir
    if proc.returncode != 0 or not os.path.isfile(os.path.join(web_dir, 'EnjinPlayer.wasm')):
        tail = (proc.stdout or '').strip().splitlines()[-1:] or ['no output']
        return False, 'web export failed: %s' % tail[0], web_dir
    return True, 'exported', web_dir


def run_web(project, web_dir, outdir):
    """Serve the web build and capture it in Chrome. Returns (ok, note, bases)."""
    if not os.path.isfile(WEB_CAPTURE):
        return False, 'no web_capture.mjs', []
    frames = project.get('frames', [30, 90, 240, 500])
    base = os.path.join(outdir, project['name'] + '.web')
    with serve(web_dir) as port:
        url = 'http://127.0.0.1:%d/index.html?fixedDelta=%.8f' % (port, CAPTURE_DELTA)
        cmd = ['node', WEB_CAPTURE, url, base + '.png',
               '--frames', ','.join(str(f) for f in frames), '--click']
        try:
            proc = subprocess.run(cmd, cwd=os.path.join(ROOT, 'tools'),
                                  capture_output=True, text=True,
                                  timeout=project.get('web_timeout', 300))
        except subprocess.TimeoutExpired:
            return False, 'web capture timed out', []
    if proc.returncode != 0:
        tail = (proc.stderr or proc.stdout or '').strip().splitlines()[-1:] or ['no output']
        return False, 'web capture failed: %s' % tail[0], []
    return True, 'captured', ['%s.f%04d' % (base, f) for f in frames]


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

    frames = project.get('frames', [30, 90, 240, 500])
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

    if claims.get('renders', True):
        for b in bases:
            try:
                ok, detail = capture_claims.renders(b + '.json')
            except (OSError, ValueError) as e:
                ok, detail = False, str(e)
            results.append(('renders@%s' % b.rsplit('.', 1)[-1], ok, detail))

    anim = claims.get('animates')
    if anim is not None:
        if len(bases) < 2:
            results.append(('animates', False, 'needs two frames, got %d' % len(bases)))
        else:
            pct = anim.get('min_changed_pct', 0.5) if isinstance(anim, dict) else 0.5
            try:
                # ALL the captures, not the first and last: two samples can land
                # on the same phase of a periodic motion or both land after a
                # scene has settled. See capture_claims.animates.
                ok, detail = capture_claims.animates([b + '.ppm' for b in bases],
                                                     min_changed_pct=pct)
            except (OSError, ValueError) as e:
                ok, detail = False, str(e)
            results.append(('animates', ok, detail))
    return results


def check_web(project, bases):
    """Claims that a browser capture can support. Returns [(claim, ok, detail)]."""
    results = []
    claims = project.get('claims', {})
    for b in bases:
        try:
            ok, detail = capture_claims.draws(b + '.png')
        except (OSError, ValueError) as e:
            ok, detail = False, str(e)
        results.append(('web draws@%s' % b.rsplit('.', 1)[-1], ok, detail))

    # No `renders` on web: the sidecar with the draw-call count is written by the
    # desktop player's --golden and the browser has no equivalent. The web build
    # does export getDrawCallCount, so this is wireable later; it is simply not
    # wired, and claiming it here would mean claiming a number nobody read.
    if claims.get('animates') is not None and len(bases) >= 2:
        pct = claims['animates'].get('min_changed_pct', 0.5)             if isinstance(claims['animates'], dict) else 0.5
        try:
            ok, detail = capture_claims.animates([b + '.png' for b in bases],
                                                 min_changed_pct=pct)
        except (OSError, ValueError) as e:
            ok, detail = False, str(e)
        results.append(('web animates', ok, detail))
    return results


def parity(project, desktop_results, web_results):
    """Do the two runtimes AGREE about this project?

    This is the check the whole desktop/web comparison exists for. A feature that
    is written on both paths and switched on in only one renders a perfectly
    valid frame on both, passes every single-runtime claim on both, and differs
    only in whether anything MOVES. Water wave displacement sat behind an unset
    flag bit on web for months exactly like that.

    Compares verdicts, not pixels. The two captures are different resolutions and
    go through different post-process chains, so a pixel diff between them would
    be noise with a number attached.
    """
    def verdict(results, name):
        hits = [ok for claim, ok, _ in results if claim.endswith(name)]
        return all(hits) if hits else None

    out = []
    for name in ('draws', 'animates'):
        d = verdict(desktop_results, name)
        w = verdict(web_results, name)
        if d is None or w is None:
            continue
        out.append(('parity:%s' % name, d == w,
                    'desktop %s, web %s' % ('yes' if d else 'no', 'yes' if w else 'no')))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--only', action='append', help='run just this project (repeatable)')
    ap.add_argument('--list', action='store_true', help='show what is wired and exit')
    ap.add_argument('--outdir', help='keep captures here instead of a temp dir')
    ap.add_argument('--ci', action='store_true',
                    help='run only the projects marked "ci" in the manifest')
    ap.add_argument('--web', action='store_true',
                    help='also export and capture each project in a browser, and compare')
    ap.add_argument('--skip-export', action='store_true',
                    help='reuse the exports already in outdir (needs --outdir)')
    args = ap.parse_args()

    with open(MANIFEST, encoding='utf-8') as f:
        manifest = json.load(f)
    all_projects = manifest['projects']

    projects = all_projects
    if args.ci:
        # A gate has to finish. The full sweep is 26 projects at 500 simulation
        # frames each, and CI renders on lavapipe in software, so gating on all
        # of it would trade a useful check for a slow one nobody waits for.
        projects = [p for p in projects if p.get('ci')]
    if args.only:
        wanted = set(args.only)
        projects = [p for p in all_projects if p['name'] in wanted]
        missing = wanted - {p['name'] for p in projects}
        if missing:
            print('not in the manifest: %s' % ', '.join(sorted(missing)))
            return 2

    # After the filters, so --list --ci shows what the GATE runs rather than
    # everything. A flag that changes the run but not the listing is a flag that
    # lies about what it does.
    if args.list:
        print('%d project(s):' % len(projects))
        for p in projects:
            print('  %-22s %-52s frames=%s%s' % (p['name'], p['project'], p.get('frames'),
                                                 '  [ci]' if p.get('ci') else ''))
        return 0

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
        desktop_results = check(p, bases)
        for name, passed, detail in desktop_results:
            print('%s %-22s %-14s %s' % ('pass' if passed else 'FAIL', p['name'], name, detail))
            if not passed:
                failures += 1

        if not args.web:
            continue

        ok, note, web_dir = export_web(p, outdir)
        if not ok:
            print('FAIL %-22s %s' % (p['name'], note))
            failures += 1
            continue
        ok, note, web_bases = run_web(p, web_dir, outdir)
        if not ok:
            print('FAIL %-22s %s' % (p['name'], note))
            failures += 1
            continue

        web_results = check_web(p, web_bases)
        for name, passed, detail in web_results:
            print('%s %-22s %-14s %s' % ('pass' if passed else 'FAIL', p['name'], name, detail))
            if not passed:
                failures += 1

        for name, passed, detail in parity(p, desktop_results, web_results):
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
