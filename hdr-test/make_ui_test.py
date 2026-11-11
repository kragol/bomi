#!/usr/bin/env python3
"""Generate a still HDR10 (PQ, BT.2020) clip of four uniform grey bands, for
measuring how the UI is composited over HDR video (qt6-window --ui ...).

Bands, left to right: 10, 100, 203 (SDR white) and 1000 nits. Lossless, so a
frame grab decodes back to these exact values. Usage:
    python3 make_ui_test.py [output.mkv]
"""
import subprocess, sys
import numpy as np

W, H, FPS, SECONDS = 1920, 1080, 5, 20
OUT = sys.argv[1] if len(sys.argv) > 1 else 'ui-test.mkv'
BANDS = [10, 100, 203, 1000]

def pq(nits):
    """Absolute luminance in nits -> PQ signal 0..1 (SMPTE ST 2084)."""
    m1, m2 = 2610 / 16384, 2523 / 4096 * 128
    c1, c2, c3 = 3424 / 4096, 2413 / 4096 * 32, 2392 / 4096 * 32
    y = np.clip(np.asarray(nits, float) / 10000.0, 0, 1) ** m1
    return ((c1 + c2 * y) / (1 + c3 * y)) ** m2

nits = np.zeros((H, W))
for i, n in enumerate(BANDS):
    nits[:, i * W // len(BANDS):(i + 1) * W // len(BANDS)] = n
e = pq(nits)
# Grey: Y' = E', Cb = Cr = 0. Limited range, 10-bit.
y = np.clip(np.rint(64 + 876 * e), 0, 1023).astype('<u2')
c = np.full((H, W), 512, '<u2')
frame = np.stack([y, c, c]).tobytes()

cmd = ['ffmpeg', '-v', 'error', '-y',
       '-f', 'rawvideo', '-pix_fmt', 'yuv444p10le', '-s', f'{W}x{H}', '-r', str(FPS), '-i', '-',
       '-vf', 'setparams=color_primaries=bt2020:color_trc=smpte2084:colorspace=bt2020nc:range=tv',
       '-c:v', 'libx265', '-preset', 'fast', '-x265-params', 'lossless=1:log-level=error',
       '-pix_fmt', 'yuv444p10le',
       '-color_primaries', 'bt2020', '-color_trc', 'smpte2084', '-colorspace', 'bt2020nc',
       '-color_range', 'tv', OUT]
enc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
for _ in range(FPS * SECONDS):
    enc.stdin.write(frame)
enc.stdin.close()
sys.exit(enc.wait())
