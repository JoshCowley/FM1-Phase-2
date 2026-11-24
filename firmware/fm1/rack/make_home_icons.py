#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""HOME's icons (update 40): one 24 x 24 icon for each kind of source, filter, effect, modulation, the drums and JAM, drawn in the FM1 Rack
(MODFM1) HOME's style: fine 1 px strokes, soft anti-aliased edges, the glyph's own motif from the page pictures (a string, a ring, the filter curve,
echoes falling, rings spreading, strands woven, a slope, a trigger).  Each icon is vectors rasterised here into 4-bit coverage (288 bytes) and written
to home_icons.h; the firmware only blends the coverage over the tile in the stage's colour.

    python3 make_home_icons.py            writes home_icons.h
    python3 make_home_icons.py --sheet F  writes F (every icon at 1x and 4x with its name; needs Pillow)
"""
import math, sys

N = 24
SS = 4                                   # supersampling per axis for the coverage of a stroke's edge
W = 1.15                                 # stroke width in pixels

# ---------------------------------------------------------------- primitives: lists of (kind, args, alpha)
def L(x0, y0, x1, y1, a=1.0): return ('seg', (x0, y0, x1, y1), a)
def P(pts, a=1.0): return [L(*pts[i], *pts[i + 1], a=a) for i in range(len(pts) - 1)]
def ARC(cx, cy, r, a0, a1, a=1.0, n=None):
    n = n or max(6, int(abs(a1 - a0) / 12))
    pts = [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
    return P(pts, a)
def CIRC(cx, cy, r, a=1.0): return ARC(cx, cy, r, 0, 360, a, 36)
def DOT(cx, cy, r=1.2, a=1.0): return [('dot', (cx, cy, r), a)]
def FUNC(f, x0, x1, a=1.0, n=48): return P([(x0 + (x1 - x0) * i / n, f(x0 + (x1 - x0) * i / n)) for i in range(n + 1)], a)
def RECT(x, y, w, h, a=1.0): return P([(x, y), (x + w, y), (x + w, y + h), (x, y + h), (x, y)], a)
def FILL(x, y, w, h, a=1.0): return [('box', (x, y, x + w, y + h), a)]

def cloud(a=1.0, y=0.0):                 # the outline of a cloud, 5 px of base
    return (ARC(7.0, 13.5 + y, 3.4, 90, 270, a) + ARC(11.5, 10.5 + y, 4.6, 180, 360, a) + ARC(16.2, 12.5 + y, 3.6, 270, 450, a)
            + [L(7.0, 16.9 + y, 16.2, 16.1 + y, a)])

ICONS = []
def icon(name, kind, *prims):
    flat = []
    for p in prims: flat += p if isinstance(p, list) else [p]
    ICONS.append((name, kind, flat))

# SOURCES -----------------------------------------------------------------------------------------------
icon('BRAIDS', 'src', FILL(2, 6, 0.9, 12), FILL(21.1, 6, 0.9, 12),                   # a string fixed at both ends, its octave above
     FUNC(lambda x: 12 + 5.2 * math.sin((x - 2) / 20 * math.pi), 2.5, 21.5),
     FUNC(lambda x: 12 - 3.2 * math.sin((x - 2) / 20 * 2 * math.pi), 2.5, 21.5, 0.42))
icon('RINGS', 'src', P([(12 + (7.6 + 1.4 * math.sin(3 * math.radians(a))) * math.cos(math.radians(a)),
                        12 + (7.6 + 1.4 * math.sin(3 * math.radians(a))) * math.sin(math.radians(a))) for a in range(0, 361, 10)]),
     CIRC(12, 12, 3.4, 0.45), DOT(12, 12, 1.1))
# THE FILTER ---------------------------------------------------------------------------------------------
icon('FILTER', 'flt', L(2, 20, 22, 20, 0.35), L(14.5, 4.5, 14.5, 20, 0.35),           # a low-pass with its resonance, the cutoff marked
     P([(2, 9), (8, 9), (11, 8.6), (13.2, 6.2), (14.6, 4.6), (16, 7.5), (18, 12.5), (20, 17.5), (22, 19.2)]))
# EFFECTS ------------------------------------------------------------------------------------------------
icon('CLOUDS', 'fx', cloud(1.0, -1.5), DOT(8, 20, 1.0), DOT(12, 18.7, 1.0, 0.8), DOT(15.5, 20.3, 1.0), DOT(19, 18.8, 0.9, 0.6), DOT(10, 22, 0.8, 0.5))     # grains falling
icon('CLOUDS LOOP', 'fx', cloud(1.0, -3.5),                                                                                                       # a loop going round
     ARC(12, 19.2, 3.3, 20, 320, 1.0), P([(15.4, 17.0), (15.3, 20.0), (12.6, 18.6)]))
icon('CLOUDS SPEC', 'fx', cloud(1.0, -3.0), *[FILL(5.6 + i * 2.7, 20 - h, 1.1, h, 0.9) for i, h in enumerate([2, 4, 6, 3, 5, 2, 4])])                 # bars of a spectrum
icon('REVERB', 'fx', *[ARC(4.5, 12, r, -52, 52, a) for r, a in ((4.5, 1.0), (8.5, 0.8), (12.5, 0.6), (16.5, 0.42))], DOT(4.5, 12, 1.3))          # wavefronts, thinning
icon('DELAY', 'fx', L(2, 20.5, 22, 20.5, 0.35), *[FILL(3 + i * 5.2, 20 - h, 1.3, h, a) for i, (h, a) in enumerate([(15, 1.0), (10.5, 0.75), (6.8, 0.52), (4, 0.34)])])  # echoes, each lower
icon('CHORUS', 'fx', *[FUNC(lambda x, p=p: 12 + 4.4 * math.sin(x * 0.52 + p), 2, 22, a) for p, a in ((0, 1.0), (1.9, 0.7), (3.8, 0.45))])           # strands woven
# MODULATION ---------------------------------------------------------------------------------------------
icon('LFO', 'mod', L(2, 12, 22, 12, 0.3), FUNC(lambda x: 12 - 6 * math.sin((x - 2) / 20 * 2 * math.pi * 1.5), 2, 22))
icon('TIDES', 'mod', L(2, 20, 22, 20, 0.35),                                                                                                        # a slope: rise, fall, rise
     P([(2, 19), (5, 15), (8, 9), (10, 5.5), (12.5, 8), (15, 13.5), (17, 17), (19, 12), (21, 6)]))
icon('PEAKS', 'mod', L(2, 20, 22, 20, 0.35),                                                                                                        # a strike: fast up, falling curve, the trigger below
     P([(2, 19), (5.5, 19), (6.6, 4)] + [(6.6 + i * 1.1, 4 + 15 * (1 - math.exp(-i * 0.33))) for i in range(1, 15)]),
     FILL(5.8, 21.2, 1.0, 1.8, 0.8))
icon('FUNC', 'mod', P([(2, 17), (9, 17), (9, 6), (21, 6)], 0.4),                                                                                    # a slew: the step, and the rounded answer
     FUNC(lambda x: 17 - 11 * (1 - math.exp(-(x - 9) / 3.0)) if x >= 9 else 17, 2, 21))
icon('GRIDS', 'mod', *[DOT(4 + i * 5.3, 4 + j * 5.3, 1.5 if (i * 5 + j * 3) % 7 in (0, 2, 5) else 0.7, 1.0 if (i * 5 + j * 3) % 7 in (0, 2, 5) else 0.45)
                       for i in range(4) for j in range(4)])
icon('MSEQ', 'mod', P([(2, 15), (6, 15), (6, 8), (10, 8), (10, 17), (14, 17), (14, 11), (18, 11), (18, 5), (22, 5)]), DOT(8, 8, 1.2))
icon('GATE', 'mod', P([(2, 9), (4, 9), (4, 4), (9, 4), (9, 9), (13, 9), (13, 4), (15, 4), (15, 9), (22, 9)]),                                          # two lanes of gates
     P([(2, 20), (7, 20), (7, 15), (10, 15), (10, 20), (17, 20), (17, 15), (22, 15)], 0.7))
icon('KNOB', 'mod', *[ARC(12, 12, 8.6, 135 + k * 45, 135 + k * 45 + 0.01, 0.5) for k in range(7)], ARC(12, 12, 5.4, 0, 360, 0.85), L(12, 12, 16.2, 6.4), DOT(12, 12, 1.0))
# BESIDES THE PATH ---------------------------------------------------------------------------------------
icon('DRUMS', 'drum', P([(4.4 + 15.2 * (0.5 + 0.5 * math.cos(math.radians(a))), 11 + 3.2 * math.sin(math.radians(a))) for a in range(0, 361, 15)]),   # a drum and its sticks
     P([(4.4, 11), (4.4, 18)]), P([(19.6, 11), (19.6, 18)]),
     P([(4.4 + 15.2 * (0.5 + 0.5 * math.cos(math.radians(a))), 18 + 3.2 * math.sin(math.radians(a))) for a in range(0, 181, 15)]),
     L(7, 3.5, 12.6, 8.0, 0.9), L(17, 3.5, 11.4, 8.0, 0.9))
icon('JAM', 'jam', P([(14, 2.5), (6.5, 13), (11.6, 13), (9.5, 21.5), (17.5, 10), (12.3, 10), (14, 2.5)]), P([(3, 17), (5.5, 17)], 0.5), P([(19.5, 6), (21.5, 6)], 0.5))

# ---------------------------------------------------------------------------------------------- rasterise
def dist_seg(px, py, x0, y0, x1, y1):
    dx, dy = x1 - x0, y1 - y0
    d2 = dx * dx + dy * dy
    t = 0.0 if d2 == 0 else max(0.0, min(1.0, ((px - x0) * dx + (py - y0) * dy) / d2))
    return math.hypot(px - (x0 + t * dx), py - (y0 + t * dy))

def raster(prims):
    cov = [[0.0] * N for _ in range(N)]
    for kind, args, a in prims:
        if a <= 0: continue
        if kind == 'seg':
            x0, y0, x1, y1 = args
            bx0, bx1 = int(max(0, min(x0, x1) - 2)), int(min(N - 1, max(x0, x1) + 2))
            by0, by1 = int(max(0, min(y0, y1) - 2)), int(min(N - 1, max(y0, y1) + 2))
            for y in range(by0, by1 + 1):
                for x in range(bx0, bx1 + 1):
                    d = dist_seg(x + 0.5, y + 0.5, x0, y0, x1, y1)
                    c = max(0.0, min(1.0, W / 2 + 0.5 - d))
                    cov[y][x] = max(cov[y][x], c * a)
        elif kind == 'dot':
            cx, cy, r = args
            for y in range(max(0, int(cy - r - 2)), min(N, int(cy + r + 3))):
                for x in range(max(0, int(cx - r - 2)), min(N, int(cx + r + 3))):
                    c = max(0.0, min(1.0, r + 0.5 - math.hypot(x + 0.5 - cx, y + 0.5 - cy)))
                    cov[y][x] = max(cov[y][x], c * a)
        elif kind == 'box':
            x0, y0, x1, y1 = args
            for y in range(N):
                for x in range(N):
                    ox = max(0.0, min(x + 1, x1) - max(x, x0)); oy = max(0.0, min(y + 1, y1) - max(y, y0))
                    cov[y][x] = max(cov[y][x], ox * oy * a)
    return [[min(15, int(round(v * 15))) for v in row] for row in cov]

def build():
    out = []
    for name, kind, prims in ICONS:
        out.append((name, kind, raster(prims)))
    return out

def header(icons):
    lines = ['// SPDX-License-Identifier: GPL-3.0',
             '// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.',
             '/* Generated by make_home_icons.py: do not edit. HOME\'s 24 x 24 icons, 4-bit coverage, two pixels a byte (high nibble first), 12 bytes a row. */',
             'enum { ' + ', '.join('HI_' + n.replace(' ', '_') for n, _, _ in icons) + ', HI_N };',
             'static const uint8_t HICON[HI_N][288] = {']
    for n, k, g in icons:
        by = []
        for row in g:
            for x in range(0, N, 2): by.append(row[x] << 4 | row[x + 1])
        lines.append('    {' + ', '.join('0x%02X' % b for b in by) + '},   /* ' + n + ' */')
    lines.append('};')
    return '\n'.join(lines) + '\n'

# each icon's colour = its page's colour: theme.h PN_C_* (keep the two in step). name -> (RGB, the page that uses it)
COLOURS = {'BRAIDS': ((240, 184, 104), 'SRC: BRAIDS'), 'RINGS': ((232, 108, 40), 'SHP: RINGS'), 'FILTER': ((50, 205, 70), 'ENV: FILTER'),
           'CLOUDS': ((125, 180, 255), 'FX: CLOUDS (all three modes)'), 'CLOUDS LOOP': ((125, 180, 255), 'FX: CLOUDS LOOPDLY'), 'CLOUDS SPEC': ((125, 180, 255), 'FX: CLOUDS SPECTRL'),
           'REVERB': ((232, 238, 255), 'FX: REVERB'), 'DELAY': ((70, 100, 240), 'FX: DELAY'), 'CHORUS': ((20, 165, 185), 'FX: CHORUS'),
           'LFO': ((170, 110, 245), 'LFO: THREE LFOS'), 'TIDES': ((0, 235, 255), 'MOD: TIDES'), 'PEAKS': ((255, 74, 82), 'MOD: PEAKS'),
           'FUNC': ((232, 226, 64), 'MOD: FUNC'), 'GRIDS': ((176, 226, 84), 'MOD: GRIDS'), 'MSEQ': ((246, 86, 198), 'LFO menu: MSEQ'),
           'GATE': ((150, 150, 255), 'LFO menu: GATE PATTERN'), 'KNOB': ((255, 120, 160), 'MATRIX (the knob layer)'),
           'DRUMS': ((255, 196, 170), 'SEQ: the DRUM pages'), 'JAM': ((255, 244, 184), 'FX menu: JAM')}

def rgb565(c): return (c[0] >> 3) << 11 | (c[1] >> 2) << 5 | c[2] >> 3

def sheet(icons, path):
    from PIL import Image, ImageDraw
    cw = 150
    cols = 5
    rows = (len(icons) + cols - 1) // cols
    im = Image.new('RGB', (cols * cw, rows * 150), (0, 0, 0))
    d = ImageDraw.Draw(im)
    for i, (n, k, g) in enumerate(icons):
        ox, oy = (i % cols) * cw, (i // cols) * 150
        c = COLOURS[n][0]
        for z, (px, py) in ((4, (ox + 12, oy + 8)), (1, (ox + 12 + 96 - 24, oy + 8))):
            for y in range(N):
                for x in range(N):
                    a = g[y][x] / 15.0
                    if a > 0:
                        col = tuple(int(6 + (cc - 6) * a) for cc in c)
                        d.rectangle([px + x * z, py + y * z, px + x * z + z - 1, py + y * z + z - 1], fill=col)
        d.text((ox + 12, oy + 112), n, fill=(210, 214, 230))
        d.text((ox + 12, oy + 126), '%04X  %s' % (rgb565(c), COLOURS[n][1]), fill=c)
    im.save(path)

if __name__ == '__main__':
    ic = build()
    if '--colours' in sys.argv:
        for n, (c, pg) in COLOURS.items(): print('%-12s %04X  (%3d,%3d,%3d)  %s' % (n, rgb565(c), *c, pg))
    elif '--sheet' in sys.argv:
        sheet(ic, sys.argv[sys.argv.index('--sheet') + 1])
    else:
        open(__file__.rsplit('/', 1)[0] + '/home_icons.h' if '/' in __file__ else 'home_icons.h', 'w').write(header(ic))
        print('home_icons.h:', len(ic), 'icons,', len(ic) * 288, 'bytes')
