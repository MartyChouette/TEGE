"""Capture a hero still of a game for use as an embed poster.

The editor's --golden capture renders the GAME camera, but during play a
character controller drives that camera, so a raw capture just shows wherever the
player happens to be standing. This copies the project to a scratch directory,
strips the controllers so nothing hijacks the camera, points the camera at a
chosen framing, and captures from there. The real project is never touched.

  python tools/capture_poster.py <project.enjinproject> <out_base> \
      --pos X Y Z --look X Y Z [--fov 60] [--frames 240] [--scene Main]

Writes <out_base>.png (and .ppm) via the editor.
"""
import argparse, json, math, os, shutil, subprocess, sys, time

EDITOR = r'D:\GitHub\enjin\build\bin\Release\EnjinEditor.exe'

# Components that take over the camera during play.
CONTROLLERS = ('thirdPerson', 'firstPerson', 'topDown3D', 'topDown2D',
               'platformer2D', 'sideScroller', 'surfaceAligned')


def look_rotation(pos, target):
    """Quaternion [x,y,z,w] aiming the camera's -Z axis from pos at target.

    Matches the engine's ZYX intrinsic euler convention (pitch about X applied
    first, then yaw about Y), which is what Quaternion::FromEuler builds.
    """
    dx, dy, dz = (target[0] - pos[0], target[1] - pos[1], target[2] - pos[2])
    n = math.sqrt(dx * dx + dy * dy + dz * dz) or 1.0
    dx, dy, dz = dx / n, dy / n, dz / n
    pitch = math.asin(max(-1.0, min(1.0, dy)))
    yaw = math.atan2(-dx, -dz)
    sy, cy = math.sin(yaw / 2), math.cos(yaw / 2)
    sp, cp = math.sin(pitch / 2), math.cos(pitch / 2)
    # q = qYaw * qPitch
    return [cy * sp, sy * cp, -sy * sp, cy * cp]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('project')
    ap.add_argument('out_base')
    ap.add_argument('--pos', nargs=3, type=float)
    ap.add_argument('--look', nargs=3, type=float)
    # Keep the camera exactly where the scene author put it. Often the best
    # framing available for free, since it is the shot they set up.
    ap.add_argument('--scene-camera', action='store_true')
    ap.add_argument('--fov', type=float, default=60.0)
    ap.add_argument('--frames', type=int, default=240)
    ap.add_argument('--keep-controllers', action='store_true')
    ap.add_argument('--timeout', type=int, default=180)
    a = ap.parse_args()

    src_proj = os.path.abspath(a.project)
    src_dir = os.path.dirname(src_proj)
    tmp_dir = os.path.abspath(a.out_base) + '_proj'
    shutil.rmtree(tmp_dir, ignore_errors=True)
    shutil.copytree(src_dir, tmp_dir, ignore=shutil.ignore_patterns(
        'Build', '.git', '.enjin', '*.zip', '__pycache__'))

    proj_path = os.path.join(tmp_dir, os.path.basename(src_proj))
    proj = json.load(open(proj_path))
    start = next((s for s in proj['scenes'] if s.get('isStartScene')), proj['scenes'][0])
    scene_path = os.path.join(tmp_dir, start['path'])

    if not a.scene_camera and (not a.pos or not a.look):
        ap.error('give --pos and --look, or use --scene-camera')

    scene = json.load(open(scene_path))
    cam_q = look_rotation(a.pos, a.look) if not a.scene_camera else None
    cams, stripped = 0, 0
    for e in scene['entities']:
        if not a.keep_controllers:
            for c in CONTROLLERS:
                if c in e:
                    del e[c]
                    stripped += 1
        if 'camera' in e and e['camera'].get('isActive', True):
            if not a.scene_camera:
                e['transform']['position'] = [round(v, 3) for v in a.pos]
                e['transform']['rotation'] = [round(v, 6) for v in cam_q]
                e['camera']['fieldOfView'] = a.fov
            cams += 1
    json.dump(scene, open(scene_path, 'w'), separators=(',', ':'))
    print(f'  staged: {cams} camera(s) posed, {stripped} controller(s) stripped')

    png = os.path.abspath(a.out_base) + '.png'
    for stale in (png, os.path.abspath(a.out_base) + '.ppm'):
        if os.path.exists(stale):
            os.remove(stale)

    # WMI Create is the only launch that survives this tool's process cleanup.
    cmd = (f'"{EDITOR}" "{proj_path}" --play --golden "{os.path.abspath(a.out_base)}" '
           f'--golden-frames {a.frames}')
    ps = ('$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments '
          '@{ CommandLine = \'' + cmd.replace("'", "''") + '\'; CurrentDirectory = \'D:\\GitHub\\enjin\' }; '
          'Write-Output $r.ReturnValue')
    subprocess.run(['powershell', '-NoProfile', '-Command', ps],
                   capture_output=True, text=True, timeout=60)

    deadline = time.time() + a.timeout
    while time.time() < deadline:
        if os.path.exists(png) and os.path.getsize(png) > 0:
            time.sleep(1.0)   # let the write finish
            print(f'  captured: {png} ({os.path.getsize(png)//1024} KB)')
            shutil.rmtree(tmp_dir, ignore_errors=True)
            return 0
        time.sleep(2)
    print(f'  TIMEOUT: no capture at {png}', file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main())
