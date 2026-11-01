#!/usr/bin/env python3
"""Measure how mpv's tone-mapping curves treat the brightness ladder of
hdr-test.mkv (section 1), the way the Qt 6 HDR test window renders it.

Each curve renders one ladder frame with PQ/BT.2020 output on a private Xvfb
server, mpv takes a window screenshot, and the patches are decoded back to
nits. The 8-bit code shows how many distinct levels survive in an 8-bit
window (NVIDIA's Wayland EGL offers no 10-bit window config).

The default --peak 294 is what KWin needs on the test display: it maps PQ's
203-nit SDR white to its own reference luminance (the SDR brightness, 400),
so a 580-nit display peak is 580 * 203 / 400 = 294 in PQ's scale. A
passthrough row (target peak = source peak, no tone mapping in mpv) shows the
source values, i.e. what KWin receives in the route bomi would use.

Usage: python3 compare_tone_mapping.py [--peak N] [--curves a,b,...]
           [--gpu-next] [--keep DIR] [--video FILE] [-- extra mpv options]
"""
import argparse, os, subprocess, sys, tempfile
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))

# Ladder layout, copied from brightness_ladder() in make_hdr_test.py (which
# cannot be imported: it generates the video at import time).
W, H = 1920, 1080
LEVELS = [100, 203, 300, 500, 750, 1000, 1500, 2000, 4000]
PW, GAP, PY, PH = 170, 26, 380, 320
X0 = (W - len(LEVELS) * PW - (len(LEVELS) - 1) * GAP) / 2
LADDER_TIME = 6          # seconds; the ladder runs from 3 to 13
SOURCE_PEAK = 4000       # PEAK in make_hdr_test.py

SHOT_LUA = """
local done = false
mp.register_event("playback-restart", function()
    if done then return end
    done = true
    mp.add_timeout(1.0, function()
        mp.commandv("screenshot-to-file", mp.get_opt("shot"), "window")
        mp.command("quit")
    end)
end)
"""


def inv_pq(e):
    """PQ signal 0..1 -> nits (SMPTE ST 2084)."""
    m1, m2 = 2610 / 16384, 2523 / 4096 * 128
    c1, c2, c3 = 3424 / 4096, 2413 / 4096 * 32, 2392 / 4096 * 32
    p = np.clip(e, 0, 1) ** (1 / m2)
    return 10000 * (np.maximum(p - c1, 0) / (c2 - c3 * p)) ** (1 / m1)


def measure(png):
    im = np.asarray(Image.open(png).convert('RGB')).astype(float)
    if im.shape[:2] != (H, W):
        sys.exit(f'{png}: screenshot is {im.shape[1]}x{im.shape[0]}, expected {W}x{H}')
    cy = int(PY + PH / 2)
    rows = []
    for i in range(len(LEVELS)):
        cx = int(X0 + i * (PW + GAP) + PW / 2)
        code = im[cy - 10:cy + 10, cx - 20:cx + 20].mean()
        rows.append((code, float(inv_pq(code / 255))))
    return rows


def start_xvfb():
    r, w = os.pipe()
    xvfb = subprocess.Popen(['Xvfb', '-displayfd', str(w), '-screen', '0', f'{W}x{H}x24', '-nolisten', 'tcp'],
                            pass_fds=[w], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.close(w)
    with os.fdopen(r) as f:
        display = f.readline().strip()
    if not display:
        xvfb.kill()
        sys.exit('Xvfb failed to start')
    return xvfb, ':' + display


def render(display, vo, peak, curve, out, lua, video, extra):
    env = dict(os.environ, DISPLAY=display)
    env.pop('WAYLAND_DISPLAY', None)
    cmd = ['mpv', '--no-config', f'--vo={vo}', '--gpu-context=x11egl',
           f'--geometry={W}x{H}+0+0', '--no-border', '--no-audio', f'--start={LADDER_TIME}',
           f'--script={lua}', f'--script-opts=shot={out}', '--msg-level=all=warn',
           '--target-trc=pq', '--target-prim=bt.2020', f'--target-peak={peak}']
    if curve:
        cmd.append(f'--tone-mapping={curve}')
    if vo == 'gpu-next':
        cmd.append('--gpu-sw')   # Xvfb only has llvmpipe, which libplacebo refuses otherwise
    cmd += extra + [video]
    # mpv prints its log to stdout when that is not a terminal, so merge both.
    p = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=60)
    if not os.path.exists(out):
        msg = '\n'.join(l for l in p.stdout.splitlines() if 'DRI3' not in l)
        sys.exit(f'mpv produced no screenshot for {vo} {curve}:\n{msg}')
    # vo_gpu whitelists its curves and silently uses auto (bt.2390) for the
    # rest (spline, bt.2446a, st2094-*), with only this warning to show for it.
    return 'Tone mapping curve unsupported' in p.stdout


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--peak', type=int, default=294, help='mpv target-peak in nits (default 294)')
    ap.add_argument('--curves', default='bt.2390,hable,mobius,reinhard',
                    help='vo_gpu tone-mapping curves to compare (default: bt.2390,hable,mobius,reinhard)')
    ap.add_argument('--gpu-next', action='store_true',
                    help='also render with gpu-next (software only under Xvfb; earlier results were not believable)')
    ap.add_argument('--keep', metavar='DIR', help='keep the screenshots in DIR')
    ap.add_argument('--video', default=os.path.join(HERE, 'hdr-test.mkv'))
    args, extra = ap.parse_known_args()
    extra = [a for a in extra if a != '--']

    runs = [('passthrough', 'gpu', SOURCE_PEAK, None)]
    runs += [(c, 'gpu', args.peak, c) for c in args.curves.split(',') if c]
    if args.gpu_next:
        runs.append(('gpu-next', 'gpu-next', args.peak, None))

    with tempfile.TemporaryDirectory() as tmp:
        outdir = args.keep or tmp
        os.makedirs(outdir, exist_ok=True)
        lua = os.path.join(tmp, 'shot.lua')
        with open(lua, 'w') as f:
            f.write(SHOT_LUA)
        xvfb, display = start_xvfb()
        results = {}
        try:
            for name, vo, peak, curve in runs:
                out = os.path.join(outdir, f'ladder-{name}.png')
                if os.path.exists(out):
                    os.remove(out)
                fell_back = render(display, vo, peak, curve, out, lua, args.video, extra)
                results[name] = (measure(out), fell_back)
        finally:
            xvfb.terminate()
            xvfb.wait()

    print(f'Ladder frame at {LADDER_TIME} s, PQ/BT.2020 output, target peak {args.peak} '
          f'(passthrough: {SOURCE_PEAK}). Each cell: output nits (8-bit code).\n')
    width = max(len(n) for n in results) + 2
    print(' ' * width + ''.join(f'{n:>11}' for n in LEVELS) + '   codes 1000-4000')
    for name, (rows, fell_back) in results.items():
        cells = ''.join(f'{f"{nits:.0f} ({code:.0f})":>11}' for code, nits in rows)
        spread = rows[-1][0] - rows[LEVELS.index(1000)][0]
        note = ''
        if fell_back:
            note = '  unsupported by this vo: mpv fell back to auto (bt.2390)'
        if name == 'gpu-next':
            note += '  (software rendering, treat with suspicion)'
        print(f'{name:<{width}}{cells}   {spread:5.1f}{note}')
    if args.keep:
        print(f'\nScreenshots in {os.path.abspath(args.keep)}')


if __name__ == '__main__':
    main()
