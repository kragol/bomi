#!/usr/bin/env python3
"""Generate a short HDR10 (PQ, BT.2020) test video to compare HDR output with
tone-mapped SDR, e.g. bomi against mpv --vo=gpu-next --target-colorspace-hint=yes.

Everything is drawn in absolute nits, so each patch is exactly the brightness
its label says. Usage: python3 make_hdr_test.py [output.mkv]
"""
import subprocess, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

W, H, FPS = 1920, 1080, 24
OUT = sys.argv[1] if len(sys.argv) > 1 else 'hdr-test.mkv'
FONT = '/usr/share/fonts/TTF/DejaVuSans-Bold.ttf'
LABEL_NITS = 80          # below SDR white, so labels do not light up the screen themselves
PEAK = 4000              # brightest pixel in the video; written as MaxCLL / mastering peak

# --- colour ------------------------------------------------------------------

def pq(nits):
    """Absolute luminance in nits -> PQ signal 0..1 (SMPTE ST 2084)."""
    m1, m2 = 2610 / 16384, 2523 / 4096 * 128
    c1, c2, c3 = 3424 / 4096, 2413 / 4096 * 32, 2392 / 4096 * 32
    y = np.clip(nits / 10000.0, 0, 1) ** m1
    return ((c1 + c2 * y) / (1 + c3 * y)) ** m2

def to_yuv444p10(rgb_nits):
    """Linear BT.2020 RGB in nits (H, W, 3) -> limited-range 10-bit Y'CbCr planes."""
    e = pq(rgb_nits)
    y = 0.2627 * e[..., 0] + 0.6780 * e[..., 1] + 0.0593 * e[..., 2]
    cb = (e[..., 2] - y) / 1.8814
    cr = (e[..., 0] - y) / 1.4746
    planes = [64 + 876 * y, 512 + 896 * cb, 512 + 896 * cr]
    return np.stack([np.clip(np.rint(p), 0, 1023) for p in planes]).astype('<u2').tobytes()

# BT.709 primaries expressed in BT.2020 linear RGB (ITU-R BT.2087)
M709_2020 = np.array([[0.6274, 0.3293, 0.0433],
                      [0.0691, 0.9195, 0.0114],
                      [0.0164, 0.0880, 0.8956]])

# --- drawing -----------------------------------------------------------------

_fonts = {}
def font(size):
    if size not in _fonts:
        _fonts[size] = ImageFont.truetype(FONT, size)
    return _fonts[size]

def text(img, xy, s, size=34, nits=LABEL_NITS, anchor='la'):
    mask = Image.new('L', (W, H))
    ImageDraw.Draw(mask).text(xy, s, font=font(size), fill=255, anchor=anchor)
    a = np.asarray(mask, np.float32)[..., None] / 255
    img[:] = img * (1 - a) + nits * a

def header(img, title, hint):
    text(img, (60, 40), title, 46)
    text(img, (60, 100), hint, 28, nits=LABEL_NITS * 0.8)

def blank():
    return np.zeros((H, W, 3), np.float32)

def rect(img, x, y, w, h, nits):
    img[int(y):int(y + h), int(x):int(x + w)] = nits

def disc(img, cx, cy, r, nits):
    yy, xx = np.ogrid[:H, :W]
    img[(xx - cx) ** 2 + (yy - cy) ** 2 <= r * r] = nits

# --- segments: each yields linear-nits frames --------------------------------

def title_card(sec):
    img = blank()
    text(img, (W / 2, 420), 'HDR10 test pattern', 72, anchor='mm')
    text(img, (W / 2, 520), 'PQ / BT.2020, values in nits. SDR white is about 203 nits.', 34, anchor='mm')
    text(img, (W / 2, 580), 'Compare tone-mapped SDR (bomi) with real HDR output (mpv gpu-next).', 30, anchor='mm')
    for _ in range(sec * FPS):
        yield img

def brightness_ladder(sec):
    img = blank()
    header(img, '1. Brightness ladder',
           'HDR: patches keep getting brighter up to your monitor\'s peak. SDR: everything above ~203 is squeezed together.')
    levels = [100, 203, 300, 500, 750, 1000, 1500, 2000, 4000]
    pw, gap = 170, 26
    x0 = (W - len(levels) * pw - (len(levels) - 1) * gap) / 2
    for i, n in enumerate(levels):
        x = x0 + i * (pw + gap)
        rect(img, x, 380, pw, 320, n)
        text(img, (x + pw / 2, 740), f'{n}', 38, anchor='ma')
    text(img, (W / 2, 820), 'nits', 30, anchor='ma')
    for _ in range(sec * FPS):
        yield img

def shadow_ladder(sec):
    img = blank()
    header(img, '2. Shadow ladder (background is 0 nits)',
           'Each step should be distinguishable from its neighbour; the first patch is pure black.')
    levels = [0, 0.01, 0.05, 0.1, 0.25, 0.5, 1, 2, 5]
    pw, gap = 170, 26
    x0 = (W - len(levels) * pw - (len(levels) - 1) * gap) / 2
    rect(img, x0 - 40, 340, len(levels) * (pw + gap) - gap + 80, 400, 0)
    for i, n in enumerate(levels):
        x = x0 + i * (pw + gap)
        rect(img, x, 380, pw, 320, n)
        text(img, (x + pw / 2, 740), f'{n:g}', 38, anchor='ma')
    text(img, (W / 2, 820), 'nits', 30, anchor='ma')
    for _ in range(sec * FPS):
        yield img

def moving_dot(sec):
    base = blank()
    header(base, '3. Bright dot on black (local dimming)',
           'The black around the moving 1000-nit dot should stay black. A grey halo that follows it is the monitor\'s backlight.')
    text(base, (1500, 860), '203 nits (SDR white)', 26, anchor='ma')
    n = sec * FPS
    for f in range(n):
        img = base.copy()
        a = 2 * np.pi * f / (n / 2)
        disc(img, 700 + 380 * np.cos(a), 580 + 260 * np.sin(a), 22, 1000)
        disc(img, 1500, 800, 22, 203)
        yield img

def growing_square(sec):
    n = sec * FPS
    for f in range(n):
        img = blank()
        frac = 0.01 + 0.99 * min(1, f / (n * 0.85))
        w, h = W * np.sqrt(frac), H * np.sqrt(frac)
        rect(img, (W - w) / 2, (H - h) / 2, w, h, 1000)
        # labels stay readable once the square covers them: 80 nits on 1000 is dark text
        header(img, '4. Growing 1000-nit window (brightness limiting)',
               'HDR: many monitors dim the window as it grows. Tone-mapped SDR: it stays the same.')
        text(img, (W / 2, H - 70), f'{100 * frac:.0f}% of screen', 32, anchor='ma')
        yield img

def dynamic_tone_mapping(sec):
    base = blank()
    xs = np.linspace(0, 30, W, dtype=np.float32)
    base[200:900] = xs[None, :, None]                      # 0..30 nit gradient
    for x, nits in [(360, 5), (760, 15), (1160, 50)]:
        rect(base, x, 360, 260, 380, nits)
        text(base, (x + 130, 770), f'{nits} nits', 30, anchor='ma')
    header(base, '5. Dim scene, then a 4000-nit logo (seconds 3 to 7)',
           'Do the dim patches get darker while the logo is up? That is the player re-fitting its tone curve.')
    logo = base.copy()
    disc(logo, 1640, 360, 70, PEAK)
    text(logo, (1640, 460), f'{PEAK} nits', 30, anchor='ma')
    n = sec * FPS
    for f in range(n):
        yield logo if 3 * FPS <= f < 7 * FPS else base

def gamut(sec):
    img = blank()
    header(img, '6. Colour gamut (each primary driven at 100 nits)',
           'Wide-gamut HDR: the bottom row is deeper and more saturated. SDR can only approximate it, often with a hue shift.')
    names = ['red', 'green', 'blue']
    eye = np.eye(3)
    rows = [('BT.709 primaries', eye @ M709_2020.T), ('BT.2020 primaries', eye)]
    for r, (label, cols) in enumerate(rows):
        y = 260 + r * 360
        text(img, (120, y + 140), label, 34)
        for i in range(3):
            c = cols[i] * 100       # the primary at 100 nits, below SDR white, so SDR can show it unclipped
            img[y:y + 300, 620 + i * 420:620 + i * 420 + 360] = c
            if r == 1:
                text(img, (620 + i * 420 + 180, y + 320), names[i], 30, anchor='ma')
    for _ in range(sec * FPS):
        yield img

SEGMENTS = [(title_card, 3), (brightness_ladder, 10), (shadow_ladder, 8), (moving_dot, 10),
            (growing_square, 10), (dynamic_tone_mapping, 10), (gamut, 8)]

# --- encode ------------------------------------------------------------------

x265 = ':'.join([
    'hdr10=1', 'repeat-headers=1', 'range=limited',
    'colorprim=bt2020', 'transfer=smpte2084', 'colormatrix=bt2020nc',
    f'master-display=G(8500,39850)B(6550,2300)R(35400,14600)WP(15635,16450)L({PEAK * 10000},50)',
    f'max-cll={PEAK},1000', 'log-level=error'])
cmd = ['ffmpeg', '-v', 'error', '-y',
       '-f', 'rawvideo', '-pix_fmt', 'yuv444p10le', '-s', f'{W}x{H}', '-r', str(FPS), '-i', '-',
       '-vf', 'setparams=color_primaries=bt2020:color_trc=smpte2084:colorspace=bt2020nc:range=tv,format=yuv420p10le',
       '-c:v', 'libx265', '-preset', 'fast', '-crf', '12', '-x265-params', x265,
       '-color_primaries', 'bt2020', '-color_trc', 'smpte2084', '-colorspace', 'bt2020nc',
       '-color_range', 'tv', OUT]
enc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
cache = {}
for seg, sec in SEGMENTS:
    for img in seg(sec):
        key = id(img)
        if key not in cache:                 # static segments repeat one frame object
            cache.clear()
            cache[key] = (img, to_yuv444p10(img))
        enc.stdin.write(cache[key][1])
    print(f'{seg.__name__}: {sec}s', flush=True)
enc.stdin.close()
sys.exit(enc.wait())
