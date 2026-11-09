#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""BIOFORGE's pictures: firmware/gen/bio_art.h (Pillow). Drawn here, at four times the size and brought down, from
shapes and numbers only: no image of anyone else's goes in. The look is the idea document's palette: almost black,
burgundy flesh, bone, a little poison green.

  python3 tools/gen_art.py           write firmware/gen/bio_art.h (and build/art/*.png to look at)
  python3 tools/gen_art.py --check   fail if it differs

What is in it:
  BG_IDX, BG_PAL     the background, 240 x 240, 8-bit indices into 64 colours (RGB565)
  EYE, CELL_*, VERT_*, LOGO   sprites: RGB565 and an alpha (0..15) each
  F5                 a 5 x 7 pixel font, ASCII 32..95 (rows of 5 bits)"""
import math
import random
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "firmware" / "gen" / "bio_art.h"
S = 4                                           # drawn at 4x

BLACK = (10, 6, 7)
PANEL = (15, 9, 10)
FLESH = [(46, 16, 22), (78, 30, 38), (112, 52, 60), (150, 84, 88)]
BONE = [(92, 76, 70), (150, 128, 118), (196, 182, 160), (226, 216, 190)]
METAL = (52, 44, 44)
GREEN = (140, 255, 42)
RED = (212, 36, 48)

# the screen's areas (1x): what the firmware draws into
TOP = (1, 1, 239, 26)
STATUS = (1, 29, 239, 46)
MAIN = (1, 49, 239, 182)
KNOBS = (1, 185, 239, 239)

F5 = {
    ' ': ["     "] * 7,
    '!': ["  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "     ", "  #  "],
    '"': [" # # ", " # # ", "     ", "     ", "     ", "     ", "     "],
    '#': [" # # ", " # # ", "#####", " # # ", "#####", " # # ", " # # "],
    '$': ["  #  ", " ####", "# #  ", " ### ", "  # #", "#### ", "  #  "],
    '%': ["##   ", "##  #", "   # ", "  #  ", " #   ", "#  ##", "   ##"],
    '&': [" ##  ", "#  # ", "# #  ", " #   ", "# # #", "#  # ", " ## #"],
    "'": ["  #  ", "  #  ", " #   ", "     ", "     ", "     ", "     "],
    '(': ["   # ", "  #  ", " #   ", " #   ", " #   ", "  #  ", "   # "],
    ')': [" #   ", "  #  ", "   # ", "   # ", "   # ", "  #  ", " #   "],
    '*': ["     ", "  #  ", "# # #", " ### ", "# # #", "  #  ", "     "],
    '+': ["     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     "],
    ',': ["     ", "     ", "     ", "     ", " ##  ", "  #  ", " #   "],
    '-': ["     ", "     ", "     ", "#####", "     ", "     ", "     "],
    '.': ["     ", "     ", "     ", "     ", "     ", " ##  ", " ##  "],
    '/': ["     ", "    #", "   # ", "  #  ", " #   ", "#    ", "     "],
    '0': [" ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### "],
    '1': ["  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "],
    '2': [" ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####"],
    '3': ["#####", "   # ", "  #  ", "   # ", "    #", "#   #", " ### "],
    '4': ["   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # "],
    '5': ["#####", "#    ", "#### ", "    #", "    #", "#   #", " ### "],
    '6': ["  ## ", " #   ", "#    ", "#### ", "#   #", "#   #", " ### "],
    '7': ["#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   "],
    '8': [" ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "],
    '9': [" ### ", "#   #", "#   #", " ####", "    #", "   # ", " ##  "],
    ':': ["     ", " ##  ", " ##  ", "     ", " ##  ", " ##  ", "     "],
    ';': ["     ", " ##  ", " ##  ", "     ", " ##  ", "  #  ", " #   "],
    '<': ["   # ", "  #  ", " #   ", "#    ", " #   ", "  #  ", "   # "],
    '=': ["     ", "     ", "#####", "     ", "#####", "     ", "     "],
    '>': [" #   ", "  #  ", "   # ", "    #", "   # ", "  #  ", " #   "],
    '?': [" ### ", "#   #", "    #", "   # ", "  #  ", "     ", "  #  "],
    '@': [" ### ", "#   #", "    #", " ## #", "# # #", "# # #", " ### "],
    'A': [" ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"],
    'B': ["#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### "],
    'C': [" ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### "],
    'D': ["#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### "],
    'E': ["#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####"],
    'F': ["#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "],
    'G': [" ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ####"],
    'H': ["#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"],
    'I': [" ### ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "],
    'J': ["  ###", "   # ", "   # ", "   # ", "   # ", "#  # ", " ##  "],
    'K': ["#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #"],
    'L': ["#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####"],
    'M': ["#   #", "## ##", "# # #", "# # #", "#   #", "#   #", "#   #"],
    'N': ["#   #", "#   #", "##  #", "# # #", "#  ##", "#   #", "#   #"],
    'O': [" ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "],
    'P': ["#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "],
    'Q': [" ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #"],
    'R': ["#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #"],
    'S': [" ####", "#    ", "#    ", " ### ", "    #", "    #", "#### "],
    'T': ["#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "],
    'U': ["#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "],
    'V': ["#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  "],
    'W': ["#   #", "#   #", "#   #", "# # #", "# # #", "# # #", " # # "],
    'X': ["#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #"],
    'Y': ["#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  "],
    'Z': ["#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####"],
    '[': [" ### ", " #   ", " #   ", " #   ", " #   ", " #   ", " ### "],
    '\\': ["     ", "#    ", " #   ", "  #  ", "   # ", "    #", "     "],
    ']': [" ### ", "   # ", "   # ", "   # ", "   # ", "   # ", " ### "],
    '^': ["  #  ", " # # ", "#   #", "     ", "     ", "     ", "     "],
    '_': ["     ", "     ", "     ", "     ", "     ", "     ", "#####"],
}


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def catmull(pts, n=24):
    out = []
    for i in range(len(pts) - 3):
        p0, p1, p2, p3 = pts[i:i + 4]
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[j]) + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t2 +
                                    (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t3) for j in range(2)))
    return out


def tube(d, pts, w, cols, rnd):
    """a tube: dark rim, flesh, a highlight along one side, rings now and then (a trachea, a hose)"""
    path = catmull(pts)
    for i, (x, y) in enumerate(path):
        r = w / 2
        d.ellipse([x - r - S, y - r - S, x + r + S, y + r + S], fill=BLACK)
    for i, (x, y) in enumerate(path):
        r = w / 2
        d.ellipse([x - r, y - r, x + r, y + r], fill=cols[0])
    for i, (x, y) in enumerate(path):
        r = w / 2 * 0.72
        d.ellipse([x - r - w * 0.08, y - r - w * 0.08, x + r - w * 0.08, y + r - w * 0.08], fill=cols[1])
    for i, (x, y) in enumerate(path):
        r = w * 0.12
        d.ellipse([x - r - w * 0.22, y - r - w * 0.22, x + r - w * 0.22, y + r - w * 0.22], fill=cols[2])
    step = max(6, int(w * 0.9))
    for i in range(0, len(path) - 1, step):    # rings
        (x0, y0), (x1, y1) = path[i], path[min(i + 1, len(path) - 1)]
        a = math.atan2(y1 - y0, x1 - x0) + math.pi / 2
        dx, dy = math.cos(a) * w / 2, math.sin(a) * w / 2
        d.line([(x0 - dx, y0 - dy), (x0 + dx, y0 + dy)], fill=lerp(cols[0], BLACK, 0.5), width=max(2, S))


def bone_frame(d, box, rnd):
    """a frame of bone: segments with notches, as vertebrae laid along the edge"""
    x0, y0, x1, y1 = [v * S for v in box]
    d.rounded_rectangle([x0, y0, x1, y1], radius=5 * S, fill=PANEL, outline=BONE[0], width=3 * S)
    d.rounded_rectangle([x0 + S, y0 + S, x1 - S, y1 - S], radius=4 * S, outline=BONE[1], width=S)
    seg = 13 * S
    for x in range(x0 + 6 * S, x1 - 6 * S, seg):   # notches top and bottom
        for y in (y0, y1):
            d.rectangle([x, y - S, x + S, y + S], fill=BONE[2 if rnd.random() < 0.5 else 1])
    for y in range(y0 + 6 * S, y1 - 6 * S, seg):
        for x in (x0, x1):
            d.rectangle([x - S, y, x + S, y + S], fill=BONE[2 if rnd.random() < 0.5 else 1])


def flesh_texture(rnd, W):
    """living tissue, dark: soft lobes of burgundy, veins over them, a grain"""
    im = Image.new("RGB", (W, W), FLESH[0])
    d = ImageDraw.Draw(im)
    for _ in range(260):                        # lobes
        x, y, r = rnd.uniform(0, W), rnd.uniform(0, W), rnd.uniform(8, 40) * S
        d.ellipse([x - r, y - r, x + r, y + r], fill=lerp(FLESH[0], FLESH[1], rnd.uniform(0.0, 0.9)))
    im = im.filter(ImageFilter.GaussianBlur(6 * S))
    d = ImageDraw.Draw(im)
    for _ in range(90):                         # veins: dark, branching a little
        x, y = rnd.uniform(0, W), rnd.uniform(0, W)
        a = rnd.uniform(0, 2 * math.pi)
        wid = rnd.choice((1, 1, 2)) * S
        pts = [(x, y)]
        for k in range(rnd.randint(4, 9)):
            a += rnd.uniform(-0.8, 0.8)
            x += math.cos(a) * 10 * S
            y += math.sin(a) * 10 * S
            pts.append((x, y))
        d.line(pts, fill=lerp(FLESH[0], BLACK, 0.6), width=wid)
        d.line([(px - S * 0.6, py - S * 0.6) for px, py in pts], fill=lerp(FLESH[1], FLESH[2], 0.4), width=max(1, wid // 2))
    return im


def background(rnd):
    W = 240 * S
    im = flesh_texture(rnd, W)
    d = ImageDraw.Draw(im)
    for _ in range(16):                         # tubes between the panels
        x, y = rnd.uniform(-60, W + 60), rnd.uniform(-60, W + 60)
        a = rnd.uniform(0, 2 * math.pi)
        pts = []
        for k in range(7):
            pts.append((x, y))
            a += rnd.uniform(-0.9, 0.9)
            x += math.cos(a) * rnd.uniform(60, 160)
            y += math.sin(a) * rnd.uniform(60, 160)
        shade = rnd.random()
        cols = [lerp(FLESH[1], FLESH[2], shade), lerp(FLESH[2], FLESH[3], shade), lerp(FLESH[3], BONE[2], shade * 0.5)]
        if rnd.random() < 0.3:
            cols = [METAL, lerp(METAL, BONE[0], 0.5), lerp(METAL, BONE[1], 0.7)]
        tube(d, pts, rnd.uniform(10, 26), cols, rnd)
    # the panels: dark recesses of the same tissue, framed in bone
    dark = im.point(lambda v: int(v * 0.30))
    for box in (TOP, STATUS, MAIN, KNOBS):
        x0, y0, x1, y1 = [v * S for v in box]
        m = Image.new("L", (W, W), 0)
        ImageDraw.Draw(m).rounded_rectangle([x0 + 2 * S, y0 + 2 * S, x1 - 2 * S, y1 - 2 * S], radius=4 * S, fill=255)
        im.paste(dark, (0, 0), m)
    d = ImageDraw.Draw(im)
    for box in (TOP, STATUS, MAIN, KNOBS):
        x0, y0, x1, y1 = [v * S for v in box]
        d.rounded_rectangle([x0, y0, x1, y1], radius=5 * S, outline=BLACK, width=S)
        d.rounded_rectangle([x0 + S, y0 + S, x1 - S, y1 - S], radius=5 * S, outline=BONE[1], width=2 * S)
        d.rounded_rectangle([x0 + S, y0 + S, x1 - S, y1 - S], radius=5 * S, outline=BONE[2], width=S)
        x = x0 + 7 * S                          # vertebrae along the top and the bottom edge
        while x < x1 - 10 * S:
            ln = rnd.choice((9, 11, 13)) * S
            for y in (y0 + S, y1 - 3 * S):
                d.rectangle([x, y, x + S, y + 2 * S], fill=BONE[0])
            x += ln
    return im.resize((240, 240), Image.LANCZOS)


def eye(rnd):
    """an eye for a knob: a bone socket, a red iris with veins, a slit pupil, a wet highlight"""
    N = 26 * S
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c = N / 2
    d.ellipse([0, 0, N - 1, N - 1], fill=BONE[0] + (255,))
    d.ellipse([2 * S, 2 * S, N - 2 * S, N - 2 * S], fill=(24, 10, 12, 255))
    r0 = int(9.5 * S)
    for k in range(r0, 0, -S):                  # the iris: dark at its rim, red, darker in
        t = k / r0
        col = lerp((70, 8, 14), (196, 40, 52), 1 - abs(t - 0.6) * 1.6)
        d.ellipse([c - k, c - k, c + k, c + k], fill=col + (255,))
    for _ in range(20):                         # veins
        a = rnd.uniform(0, 2 * math.pi)
        r1, r2 = rnd.uniform(2, 4) * S, rnd.uniform(6.5, 9.5) * S
        d.line([(c + math.cos(a) * r1, c + math.sin(a) * r1), (c + math.cos(a + 0.2) * r2, c + math.sin(a + 0.2) * r2)],
               fill=(90, 6, 14, 255), width=S)
    d.ellipse([c - 1.6 * S, c - 8 * S, c + 1.6 * S, c + 8 * S], fill=(4, 2, 2, 255))   # the slit
    d.ellipse([c - 4.5 * S, c - 6.5 * S, c - 1.5 * S, c - 4 * S], fill=(255, 236, 220, 200))   # wet
    return im.resize((26, 26), Image.LANCZOS)


def cell(on, rnd):
    N = 12 * S
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c = N / 2
    for k in range(int(5.6 * S), 0, -S):        # a membrane sphere
        t = k / (5.6 * S)
        col = lerp((96, 24, 34), (40, 10, 14), t) if not on else lerp((40, 70, 20), (18, 26, 10), t)
        d.ellipse([c - k - S * 0.6, c - k - S * 0.6, c + k - S * 0.6, c + k - S * 0.6], fill=col + (255,))
    d.ellipse([c - 5.6 * S, c - 5.6 * S, c + 5.6 * S, c + 5.6 * S], outline=(20, 6, 8, 255), width=S)
    if on:                                      # a green four-pointed star, glowing
        pts = []
        for k in range(8):
            a = k * math.pi / 4
            r = 5.4 * S if k % 2 == 0 else 1.6 * S
            pts.append((c + math.cos(a) * r, c + math.sin(a) * r))
        d.polygon(pts, fill=GREEN + (255,))
        d.ellipse([c - 1.6 * S, c - 1.6 * S, c + 1.6 * S, c + 1.6 * S], fill=(230, 255, 200, 255))
    else:
        d.ellipse([c - 3.4 * S, c - 3.6 * S, c - 1.4 * S, c - 1.8 * S], fill=(170, 90, 96, 160))
    return im.resize((12, 12), Image.LANCZOS)


def vertebra(on):
    """a vertebra of the Turing's spine: a bone body with its processes, its core lit green when the bit is 1"""
    W, H = 12 * S, 20 * S
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.polygon([(W / 2, 0), (W / 2 + 2 * S, 4 * S), (W / 2 - 2 * S, 4 * S)], fill=BONE[1] + (255,))   # the spinous process
    d.rounded_rectangle([1.5 * S, 3 * S, W - 1.5 * S, H - 2 * S], radius=3 * S, fill=BONE[1] + (255,), outline=BONE[0] + (255,), width=S)
    d.line([(1.5 * S, 7 * S), (W - 1.5 * S, 7 * S)], fill=BONE[2] + (255,), width=S)
    d.line([(1.5 * S, H - 6 * S), (W - 1.5 * S, H - 6 * S)], fill=BONE[0] + (255,), width=S)
    core = GREEN if on else (40, 26, 24)
    d.rounded_rectangle([3.5 * S, 8.5 * S, W - 3.5 * S, H - 7.5 * S], radius=1.5 * S, fill=core + (255,))
    if on:
        d.rectangle([4.5 * S, 9 * S, 5.5 * S, H - 8 * S], fill=(225, 255, 200, 255))
    return im.resize((12, 20), Image.LANCZOS)


def logo(rnd):
    """BIOFORGE in the 5 x 7 font at twice its size, bone fading to flesh, dark edged, with drips"""
    text = "BIOFORGE"
    W, H = len(text) * 12 + 2, 22
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = im.load()
    mask = [[0] * W for _ in range(H)]
    for i, ch in enumerate(text):
        g = F5[ch]
        for gy, row in enumerate(g):
            for gx, v in enumerate(row):
                if v == '#':
                    for sy in range(2):
                        for sx in range(2):
                            mask[1 + gy * 2 + sy][1 + i * 12 + gx * 2 + sx] = 1
    for x in range(W):                          # drips from the bottom row
        if mask[14][x] and rnd.random() < 0.35:
            ln = rnd.randint(1, 6)
            for y in range(15, min(H - 1, 15 + ln)):
                mask[y][x] = 2
    for y in range(H):
        for x in range(W):
            if mask[y][x]:
                t = y / 14
                col = lerp(BONE[3], (170, 70, 78), min(1, t)) if mask[y][x] == 1 else (150, 30, 40)
                px[x, y] = col + (255,)
            elif any(0 <= y + dy < H and 0 <= x + dx < W and mask[y + dy][x + dx] for dy in (-1, 0, 1) for dx in (-1, 0, 1)):
                px[x, y] = (20, 6, 8, 230)
    return im


def rgb565(c):
    return ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)


def sprite_c(name, im):
    w, h = im.size
    px = im.load()
    cols, alph = [], []
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            cols.append(rgb565((r, g, b)))
            alph.append(min(15, (a + 8) // 17))
    pa = [alph[i] << 4 | (alph[i + 1] if i + 1 < len(alph) else 0) for i in range(0, len(alph), 2)]
    s = f"#define {name}_W {w}\n#define {name}_H {h}\n"
    s += f"static const uint16_t {name}[{w * h}] = {{" + ",".join(str(v) for v in cols) + "};\n"
    s += f"static const uint8_t {name}_A[{len(pa)}] = {{" + ",".join(str(v) for v in pa) + "};\n"
    return s


def main():
    rnd = random.Random(0xB10F0)
    bg = background(rnd)
    q = bg.quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:64 * 3]
    idx = list(q.get_flattened_data()) if hasattr(q, 'get_flattened_data') else list(q.getdata())
    sprites = {"EYE": eye(random.Random(7)), "CELL_OFF": cell(False, rnd), "CELL_ON": cell(True, rnd),
               "VERT_OFF": vertebra(False), "VERT_ON": vertebra(True), "LOGO": logo(random.Random(3))}
    out = ["/* SPDX-License-Identifier: GPL-3.0-only */\n/* generated by tools/gen_art.py: never edit (run it again) */\n",
           "#ifndef BIO_ART_H\n#define BIO_ART_H\n#include <stdint.h>\n\n"]
    out.append("static const uint16_t BG_PAL[64] = {" + ",".join(str(rgb565(pal[i * 3:i * 3 + 3])) for i in range(64)) + "};\n")
    out.append("static const uint8_t BG_IDX[57600] = {\n")
    for y in range(240):
        out.append(",".join(str(v) for v in idx[y * 240:(y + 1) * 240]) + ",\n")
    out.append("};\n")
    for n, im in sprites.items():
        out.append(sprite_c(n, im))
    rows = []
    for c in range(32, 96):
        g = F5.get(chr(c), F5['?'])
        rows.append("{" + ",".join(str(int(r.replace('#', '1').replace(' ', '0'), 2)) for r in g) + "}")
    out.append("/* the 5 x 7 font, ASCII 32..95, a row a byte (bit 4 the left) */\nstatic const uint8_t F5[64][7] = {" +
               ",".join(rows) + "};\n")
    out.append("\n#endif\n")
    text = "".join(out)
    if "--check" in sys.argv:
        if not OUT.exists() or OUT.read_text() != text:
            print(f"{OUT}: differs from what tools/gen_art.py makes (run it)")
            return 1
        print("bio_art.h: up to date")
        return 0
    OUT.write_text(text)
    look = ROOT / "build" / "art"
    look.mkdir(parents=True, exist_ok=True)
    q.convert("RGB").resize((720, 720), Image.NEAREST).save(look / "background.png")
    sheet = Image.new("RGBA", (200, 60), BLACK + (255,))
    x = 2
    for n, im in sprites.items():
        sheet.alpha_composite(im, (x, 2))
        x += im.size[0] + 4
    sheet.resize((800, 240), Image.NEAREST).save(look / "sprites.png")
    print(f"{OUT}: {len(text)} bytes; build/art/background.png, sprites.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
