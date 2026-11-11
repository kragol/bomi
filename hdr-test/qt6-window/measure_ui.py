#!/usr/bin/env python3
"""Measure a qt6-window frame grab (--grab) of ui-test.mkv (make_ui_test.py):
decode the PQ window back to nits at the UI's test patches and compare with
what the same UI shows in an SDR window, i.e. SDR white at 203 nits and
blending on gamma 2.2 values.

    python3 measure_ui.py grab.png [grab2.png ...]
"""
import sys
import numpy as np
from PIL import Image

LOGICAL_W = 1280                     # Main.qml's window width
BANDS = [10, 100, 203, 1000]         # ui-test.mkv, left to right
WHITE = 203.0

def nits_from_pq(e):
    m1, m2 = 2610 / 16384, 2523 / 4096 * 128
    c1, c2, c3 = 3424 / 4096, 2413 / 4096 * 32, 2392 / 4096 * 32
    p = np.clip(e, 0, 1) ** (1 / m2)
    return 10000 * (np.maximum(p - c1, 0) / (c2 - c3 * p)) ** (1 / m1)

def sdr_blend(video_nits, ui, alpha):
    """What an SDR window shows: blend on gamma 2.2 values, white = 203."""
    g = (1 - alpha) * (video_nits / WHITE) ** (1 / 2.2) + alpha * ui
    return WHITE * g ** 2.2

def measure(path):
    img = np.asarray(Image.open(path).convert('RGB'), float) / 255
    s = img.shape[1] / LOGICAL_W
    def at(x, y):                     # mean luminance-ish (green) of a 5x5 spot
        px, py = int(x * s), int(y * s)
        return nits_from_pq(img[py - 2:py + 3, px - 2:px + 3, 1].mean())
    rows = []
    for i in range(11):
        v = round(255 * i / 10) / 255
        rows.append((f'ramp sRGB {v:.2f}', at(40 + 50 * i, 40), WHITE * v ** 2.2))
    for k, band in enumerate(BANDS):
        x = 160 + 320 * k
        rows.append((f'{band:>4} nit video, no UI', at(x, 230), band))
        rows.append((f'{band:>4} nit video + black 75%', at(x, 325), sdr_blend(band, 0, 192 / 255)))
        rows.append((f'{band:>4} nit video + black 55%', at(x, 385), sdr_blend(band, 0, 0.55)))
        rows.append((f'{band:>4} nit video + white 30%', at(x, 445), sdr_blend(band, 1, 0.3)))
    print(f'== {path}')
    print(f'{"patch":32} {"measured":>9} {"as SDR":>9} {"error":>7}')
    for name, got, want in rows:
        err = (got - want) / want * 100 if want > 0.05 else float('nan')
        print(f'{name:32} {got:9.2f} {want:9.2f} {err:6.0f}%')

for p in sys.argv[1:]:
    measure(p)
