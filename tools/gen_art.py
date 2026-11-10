#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""BIOFORGE's pictures: firmware/gen/bio_art.h (Pillow). Drawn here, at four times the size and brought down, from
shapes and numbers only: no image of anyone else's goes in. The look (D26): biomechanical, after the feel of the
pictures the user gave (chrome and graphite, ribbed hoses, vertebrae, smooth capsules, airbrushed light), in the dark;
a pale green the only colour for sound, a dark blood red for recording. The logo (D30): the word cut into a ribbed
plate, dark flesh in the cuts, an eye at each end.

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
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "firmware" / "gen" / "bio_art.h"
S = 4                                           # drawn at 4x

BLACK = (5, 5, 6)
METAL = [(18, 18, 20), (40, 40, 44), (78, 78, 84), (132, 132, 138), (196, 196, 202), (236, 236, 240)]   # graphite to chrome
ON = (176, 214, 150)                            # sound: a pale green (bio_ui.h's COL_GREEN)

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


def shade(t):                                   # 0 dark .. 1 bright along the metal ramp
    t = max(0.0, min(1.0, t)) * (len(METAL) - 1)
    i = min(int(t), len(METAL) - 2)
    return lerp(METAL[i], METAL[i + 1], t - i)


def hose(d, pts, w, rnd, bright=1.0):
    """a ribbed hose: a dark rim, its body lit from the top left, a highlight, and ribs all along it"""
    path = catmull(pts, 40)
    for x, y in path:
        r = w / 2 + S
        d.ellipse([x - r, y - r, x + r, y + r], fill=BLACK)
    for layer, (k, off, t) in enumerate(((1.0, 0.0, 0.16), (0.84, 0.06, 0.30), (0.6, 0.13, 0.50), (0.3, 0.22, 0.78), (0.12, 0.27, 1.0))):
        for x, y in path:
            r = w / 2 * k
            d.ellipse([x - r - w * off, y - r - w * off, x + r - w * off, y + r - w * off], fill=shade(t * bright))
    step = max(4, int(w * 0.42))
    for i in range(0, len(path) - 1, step):    # the ribs: a dark groove and a lit edge
        (x0, y0), (x1, y1) = path[i], path[min(i + 1, len(path) - 1)]
        a = math.atan2(y1 - y0, x1 - x0) + math.pi / 2
        dx, dy = math.cos(a) * w / 2, math.sin(a) * w / 2
        d.line([(x0 - dx, y0 - dy), (x0 + dx, y0 + dy)], fill=lerp(shade(0.1), BLACK, 0.5), width=max(2, int(S * 1.2)))
        d.line([(x0 - dx + S, y0 - dy), (x0 + dx * 0.4 + S, y0 + dy * 0.4)], fill=shade(0.62 * bright), width=max(1, S // 2))


def spine(d, pts, size, rnd):
    """vertebrae along a curve: rounded bodies with a lit top, small processes"""
    path = catmull(pts, 60)
    step = int(size * 0.9)
    for i in range(0, len(path) - 1, max(1, step // 2)):
        x, y = path[i]
        x1, y1 = path[min(i + 1, len(path) - 1)]
        a = math.atan2(y1 - y, x1 - x)
        r = size / 2
        d.ellipse([x - r - S, y - r - S, x + r + S, y + r + S], fill=BLACK)
        d.ellipse([x - r, y - r, x + r, y + r], fill=shade(0.35))
        d.ellipse([x - r * 0.7 - S, y - r * 0.7 - S, x + r * 0.5, y + r * 0.5], fill=shade(0.6))
        d.ellipse([x - r * 0.35 - 2 * S, y - r * 0.35 - 2 * S, x - r * 0.05, y - r * 0.05], fill=shade(0.9))
        px, py = x + math.cos(a + math.pi / 2) * r * 1.5, y + math.sin(a + math.pi / 2) * r * 1.5
        d.line([(x, y), (px, py)], fill=shade(0.45), width=max(2, int(size * 0.18)))


def dome(d, x, y, r):
    """a smooth dome, a skull's or a capsule's: lit from the top left, two dark hollows"""
    for k in range(int(r), 0, -S):
        t = k / r
        d.ellipse([x - k - (1 - t) * r * 0.35, y - k - (1 - t) * r * 0.35, x + k - (1 - t) * r * 0.35, y + k - (1 - t) * r * 0.35],
                  fill=shade(0.18 + (1 - t) * 0.75))
    d.arc([x - r * 0.9, y - r * 0.2, x + r * 0.9, y + r * 1.1], 20, 160, fill=BLACK, width=int(2 * S))   # a seam


def ribs(d, x, y, rx, ry, n):
    for k in range(n):
        f = 1 - k * 0.12
        box = [x - rx * f, y - ry * f, x + rx * f, y + ry * f]
        d.arc(box, 200, 340, fill=BLACK, width=int(5 * S))
        d.arc(box, 200, 340, fill=shade(0.55), width=int(3 * S))
        d.arc([box[0] + S, box[1] + S, box[2] + S, box[3] + S], 210, 300, fill=shade(0.85), width=S)


def background(rnd):
    W = 240 * S
    im = Image.new("RGB", (W, W), BLACK)
    d = ImageDraw.Draw(im)
    for y in range(0, W, S):                    # an airbrushed ground: soft horizontal bands, ribbed, very dark
        t = 0.10 + 0.06 * math.sin(y / (9 * S)) + 0.04 * math.sin(y / (37 * S) + 1.3)
        d.line([(0, y), (W, y)], fill=shade(t))
    im = im.filter(ImageFilter.GaussianBlur(2 * S)); d = ImageDraw.Draw(im)
    for _ in range(7):                          # ribs (a cage)
        ribs(d, rnd.uniform(0, W), rnd.uniform(0, W), rnd.uniform(40, 110) * S, rnd.uniform(20, 60) * S, rnd.randint(3, 6))
    for _ in range(40):                         # hoses, thick and thin, crossing
        x, y = rnd.uniform(-80, W + 80), rnd.uniform(-80, W + 80); a = rnd.uniform(0, 2 * math.pi); pts = []
        for k in range(7):
            pts.append((x, y)); a += rnd.uniform(-0.7, 0.7)
            x += math.cos(a) * rnd.uniform(70, 170); y += math.sin(a) * rnd.uniform(70, 170)
        hose(d, pts, rnd.uniform(7, 24) * S / 2, rnd, bright=rnd.uniform(0.6, 1.0))
    for _ in range(5):                          # spines
        x, y = rnd.uniform(0, W), rnd.uniform(0, W); a = rnd.uniform(0, 2 * math.pi); pts = []
        for k in range(6):
            pts.append((x, y)); a += rnd.uniform(-0.5, 0.5); x += math.cos(a) * 120; y += math.sin(a) * 120
        spine(d, pts, rnd.uniform(7, 12) * S, rnd)
    for _ in range(6):                          # domes, half hidden
        dome(d, rnd.uniform(0, W), rnd.uniform(0, W), rnd.uniform(9, 20) * S)
    for _ in range(24):                         # more hoses over them
        x, y = rnd.uniform(-80, W + 80), rnd.uniform(-80, W + 80); a = rnd.uniform(0, 2 * math.pi); pts = []
        for k in range(6):
            pts.append((x, y)); a += rnd.uniform(-0.8, 0.8)
            x += math.cos(a) * rnd.uniform(60, 140); y += math.sin(a) * rnd.uniform(60, 140)
        hose(d, pts, rnd.uniform(5, 14) * S / 2, rnd, bright=rnd.uniform(0.7, 1.0))
    im = im.point(lambda v: int(v * 0.72))      # the whole of it in the dark
    dark = im.point(lambda v: int(v * 0.34))
    for box in (TOP, STATUS, MAIN, KNOBS):   # the panels: recesses, a chrome bevel, rivets
        x0, y0, x1, y1 = [v * S for v in box]
        m = Image.new("L", (W, W), 0)
        ImageDraw.Draw(m).rounded_rectangle([x0 + 2 * S, y0 + 2 * S, x1 - 2 * S, y1 - 2 * S], radius=6 * S, fill=255)
        im.paste(dark, (0, 0), m)
    d = ImageDraw.Draw(im)
    for box in (TOP, STATUS, MAIN, KNOBS):
        x0, y0, x1, y1 = [v * S for v in box]
        d.rounded_rectangle([x0, y0, x1, y1], radius=7 * S, outline=BLACK, width=2 * S)
        d.rounded_rectangle([x0 + S, y0 + S, x1 - S, y1 - S], radius=6 * S, outline=shade(0.55), width=2 * S)
        d.arc([x0 + S, y0 + S, x0 + 13 * S, y0 + 13 * S], 180, 270, fill=shade(0.95), width=S)
        d.line([(x0 + 7 * S, y0 + S), (x1 - 7 * S, y0 + S)], fill=shade(0.85), width=S)        # lit from above
        d.line([(x0 + 7 * S, y1 - S), (x1 - 7 * S, y1 - S)], fill=shade(0.25), width=S)
        for x in range(x0 + 9 * S, x1 - 8 * S, 4 * S):   # the frame ribbed, a hose laid round the panel
            for y in (y0 + S, y1 - S):
                d.line([(x, y - S), (x, y + S)], fill=BLACK, width=max(1, S // 2))
        for y in range(y0 + 9 * S, y1 - 8 * S, 4 * S):
            for x in (x0 + S, x1 - S):
                d.line([(x - S, y), (x + S, y)], fill=BLACK, width=max(1, S // 2))
        for cx, cy in ((x0 + 5 * S, y0 + 5 * S), (x1 - 5 * S, y0 + 5 * S), (x0 + 5 * S, y1 - 5 * S), (x1 - 5 * S, y1 - 5 * S)):
            if y1 - y0 > 30 * S:
                d.ellipse([cx - 2 * S, cy - 2 * S, cx + 2 * S, cy + 2 * S], fill=shade(0.4), outline=BLACK, width=S // 2)
                d.ellipse([cx - S, cy - S, cx, cy], fill=shade(0.9))
    return im.resize((240, 240), Image.LANCZOS)


def knob():
    """a valve: a ribbed chrome ring, a dark socket, a closed eye of steel in it"""
    N = 26 * S; im = Image.new("RGBA", (N, N), (0, 0, 0, 0)); d = ImageDraw.Draw(im); c = N / 2
    for k in range(int(12.8 * S), int(8 * S), -1):
        t = (k - 8 * S) / (4.8 * S)
        d.ellipse([c - k, c - k, c + k, c + k], fill=shade(0.25 + 0.6 * math.sin(t * math.pi)) + (255,))
    for i in range(24):                         # the ribbed bezel
        a = i * math.pi / 12
        d.line([(c + math.cos(a) * 9 * S, c + math.sin(a) * 9 * S), (c + math.cos(a) * 12.4 * S, c + math.sin(a) * 12.4 * S)],
               fill=shade(0.12) + (255,), width=S)
    d.ellipse([c - 8 * S, c - 8 * S, c + 8 * S, c + 8 * S], fill=BLACK + (255,))
    for k in range(int(6.5 * S), 0, -1):        # the eyeball: steel, lit at the top
        t = k / (6.5 * S)
        d.ellipse([c - k, c - k - (1 - t) * 2 * S, c + k, c + k - (1 - t) * 2 * S], fill=shade(0.2 + (1 - t) * 0.6) + (255,))
    d.chord([c - 6.6 * S, c - 6.6 * S, c + 6.6 * S, c + 6.6 * S], 180, 360, fill=shade(0.45) + (255,))   # the lid, closed
    d.arc([c - 6.6 * S, c - 3 * S, c + 6.6 * S, c + 3 * S], 0, 180, fill=BLACK + (255,), width=S)
    d.ellipse([c - 4 * S, c - 5.5 * S, c - 1.5 * S, c - 4 * S], fill=shade(1.0) + (210,))
    return im.resize((26, 26), Image.LANCZOS)


def cell(on):
    N = 12 * S; im = Image.new("RGBA", (N, N), (0, 0, 0, 0)); d = ImageDraw.Draw(im); c = N / 2
    d.ellipse([c - 5.6 * S, c - 5.6 * S, c + 5.6 * S, c + 5.6 * S], fill=BLACK + (255,))
    for k in range(int(5 * S), 0, -1):
        t = k / (5 * S)
        col = shade(0.12 + (1 - t) * 0.5) if not on else lerp(ON, (255, 255, 255), (1 - t) * 0.7) if t < 0.6 else lerp(shade(0.3), ON, 1 - t)
        d.ellipse([c - k - S * 0.5, c - k - S * 0.5, c + k - S * 0.5, c + k - S * 0.5], fill=col + (255,))
    if not on:
        d.ellipse([c - 3 * S, c - 3.2 * S, c - 1.2 * S, c - 1.6 * S], fill=shade(0.8) + (180,))
    return im.resize((12, 12), Image.LANCZOS)


def vertebra(on):
    W, H = 12 * S, 20 * S; im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.polygon([(W / 2, 0), (W / 2 + 2 * S, 4 * S), (W / 2 - 2 * S, 4 * S)], fill=shade(0.6) + (255,))
    d.rounded_rectangle([1.5 * S, 3 * S, W - 1.5 * S, H - 2 * S], radius=3 * S, fill=shade(0.45) + (255,), outline=BLACK + (255,), width=S)
    d.line([(2 * S, 4 * S), (W - 3 * S, 4 * S)], fill=shade(0.95) + (255,), width=S)
    for yy in (7, 11, 15):
        d.line([(2 * S, yy * S), (W - 2 * S, yy * S)], fill=shade(0.2) + (255,), width=max(1, S // 2))
    core = ON if on else shade(0.08)
    d.rounded_rectangle([3.5 * S, 8 * S, W - 3.5 * S, H - 7 * S], radius=1.5 * S, fill=core + (255,))
    if on:
        d.rectangle([4.5 * S, 8.5 * S, 5.5 * S, H - 7.5 * S], fill=(240, 255, 240, 255))
    return im.resize((12, 20), Image.LANCZOS)


# the logo's letters (D30): strokes in a box 10 wide and 16 high
LOGO_STROKES = {
    'B': [[(0, 0), (0, 16)], [(0, 0), (5.5, 0), (8.3, 1.6), (8.3, 6.2), (5.5, 8), (0, 8)],
          [(0, 8), (6.2, 8), (9.3, 9.8), (9.3, 14.2), (6.2, 16), (0, 16)]],
    'I': [[(4.6, 0), (4.6, 16)]],
    'O': [[(4.7 + 4.5 * math.cos(a * math.pi / 16), 8 + 8 * math.sin(a * math.pi / 16)) for a in range(33)]],
    'F': [[(0, 0), (0, 16)], [(0, 0), (9, 0)], [(0, 8), (6.8, 8)]],
    'R': [[(0, 0), (0, 16)], [(0, 0), (5.5, 0), (8.3, 1.6), (8.3, 6.2), (5.5, 8), (0, 8)], [(4.2, 8), (9.2, 16)]],
    'G': [[(9.2, 3), (7.4, 0.3), (2.6, 0.3), (0.3, 3.2), (0.3, 12.8), (2.6, 15.7), (7.2, 15.7), (9.2, 13.5), (9.2, 8.8),
           (5.0, 8.8)]],
    'E': [[(0, 0), (0, 16)], [(0, 0), (9, 0)], [(0, 8), (7, 8)], [(0, 16), (9, 16)]],
}
LOGO_SIZE = (103, 28)
FLESH = [(14, 5, 5), (44, 14, 14), (82, 28, 26), (128, 52, 44)]


def logo(scale=1):
    """BIOFORGE cut into a ribbed steel plate (D30), after the carved reliefs of the pictures: the cuts dark flesh, lit
    on their lower walls; the plate convex, ribbed along its top and its foot, a socket with an eye at each end, a hose
    from each into the frame. 103 x 28 (it overlaps the top panel's frame); `scale` for the page, drawn sharp"""
    K = scale * (8 if scale < 3 else 4)        # drawn at K times 1x and brought down
    W, H = LOGO_SIZE[0] * K, LOGO_SIZE[1] * K
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)

    def P(x, y):
        return (x * K, y * K)

    def tube(pts, w, lit):
        for x, y in pts:
            r = w / 2 + K * 0.5; d.ellipse([x - r, y - r, x + r, y + r], fill=BLACK + (255,))
        for k, off, t in ((1.0, 0, 0.16), (0.7, 0.1, 0.42), (0.35, 0.2, 0.8)):
            for x, y in pts:
                r = w / 2 * k; o = w * off
                d.ellipse([x - r - o, y - r - o, x + r - o, y + r - o], fill=shade(t * lit) + (255,))
        for i in range(0, len(pts) - 1, 3):
            (x0, y0), (x1, y1) = pts[i], pts[i + 1]
            a = math.atan2(y1 - y0, x1 - x0) + math.pi / 2
            dx, dy = math.cos(a) * w / 2, math.sin(a) * w / 2
            d.line([(x0 - dx, y0 - dy), (x0 + dx, y0 + dy)], fill=BLACK + (255,), width=max(1, int(K * 0.45)))

    def line_pts(p0, p1, step):
        n = max(2, int(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / step))
        return [(p0[0] + (p1[0] - p0[0]) * i / n, p0[1] + (p1[1] - p0[1]) * i / n) for i in range(n + 1)]

    # the hoses, from the sockets into the frame
    tube(line_pts(P(-4, 16.2), P(3, 14.4), K * 0.6), 2.4 * K, 0.6)
    tube(line_pts(P(100, 14.4), P(107, 16.2), K * 0.6), 2.4 * K, 0.6)
    # the plate: a capsule, convex, lit from above, a black rim
    x0, y0, x1, y1 = 1.0 * K, 2.2 * K, 102.0 * K, 25.8 * K
    d.rounded_rectangle([x0 - K * 0.7, y0 - K * 0.7, x1 + K * 0.7, y1 + K * 0.7], radius=int(12 * K), fill=BLACK + (255,))
    for k in range(48):
        t = k / 47
        ya, yb = y0 + (y1 - y0) * t * 0.5, y1 - (y1 - y0) * t * 0.5
        if yb - ya < K:                         # (the middle reached)
            break
        d.rounded_rectangle([x0 + t * 3 * K, ya, x1 - t * 3 * K, yb], radius=max(1, int((11.8 - t * 9) * K)),
                            fill=shade(0.2 + 0.52 * math.sin(t * math.pi / 2) ** 2) + (255,))
    d.arc([x0 + K, y0 + K * 0.6, x0 + 22 * K, y1 - K], 150, 260, fill=shade(0.95) + (255,), width=max(1, int(K * 0.5)))
    d.line([P(12, 2.9), P(91, 2.9)], fill=shade(0.9) + (255,), width=max(1, int(K * 0.45)))
    # its ribs, along the top and the foot; a groove each side of the word
    x = 13.2
    while x < 90.5:
        for ya, yb in ((2.8, 6.4), (21.6, 25.2)):
            d.line([P(x, ya), P(x, yb)], fill=shade(0.06) + (255,), width=max(1, int(K * 0.7)))
            d.line([P(x + 0.8, ya), P(x + 0.8, yb)], fill=shade(0.78) + (255,), width=max(1, int(K * 0.35)))
        x += 2.4
    for yy in (7.0, 21.0):
        d.line([P(13, yy), P(90.5, yy)], fill=shade(0.05) + (255,), width=max(1, int(K * 0.5)))
        d.line([P(13, yy + 0.6), P(90.5, yy + 0.6)], fill=shade(0.7) + (255,), width=max(1, int(K * 0.3)))
    # the word, cut: its mask, flesh in it (darker deep at the top), the upper walls in shadow, the lower ones lit
    mask = Image.new("L", (W, H), 0); md = ImageDraw.Draw(mask)
    for i, ch in enumerate("BIOFORGE"):
        for st in LOGO_STROKES[ch]:
            pts = [P(14.6 + i * 9.35 + x * 0.74, 8.0 + y * 0.75) for x, y in st]
            path = catmull([pts[-2]] + pts + [pts[1]], 8) if ch == 'O' else catmull([pts[0]] + pts + [pts[-1]], 12) if len(pts) > 2 else pts
            dense = []
            for (xa, ya), (xb, yb) in zip(path, path[1:]):
                dense += line_pts((xa, ya), (xb, yb), K * 0.25)
            for xx, yy in dense or path:
                r = 1.25 * K; md.ellipse([xx - r, yy - r, xx + r, yy + r], fill=255)
    flesh = Image.new("RGBA", (W, H), (0, 0, 0, 0)); fd = ImageDraw.Draw(flesh)
    for k in range(40):
        t = k / 39
        fd.rectangle([0, int((7.4 + 13.2 * t) * K), W, H], fill=lerp(FLESH[0], FLESH[2], t ** 1.3) + (255,))
    im.paste(flesh, (0, 0), mask)
    wall = ImageChops.subtract(mask, ImageChops.offset(mask, int(0.9 * K), int(1.1 * K)))
    lit = ImageChops.subtract(mask, ImageChops.offset(mask, -int(0.6 * K), -int(0.7 * K)))
    im.paste(Image.new("RGBA", (W, H), BLACK + (255,)), (0, 0), wall.point(lambda v: int(v * 0.85)))
    im.paste(Image.new("RGBA", (W, H), shade(0.92) + (255,)), (0, 0), lit.point(lambda v: int(v * 0.8)))
    d = ImageDraw.Draw(im)
    # the sockets: a ribbed bezel, flesh, an eye of steel lit from the top left
    for cx in (7.0, 96.0):
        c = (cx * K, 14 * K)
        d.ellipse([c[0] - 4.6 * K, c[1] - 4.6 * K, c[0] + 4.6 * K, c[1] + 4.6 * K], fill=BLACK + (255,))
        for k in range(int(4.1 * K), int(2.7 * K), -1):
            t = (k - 2.7 * K) / (1.4 * K)
            d.ellipse([c[0] - k, c[1] - k, c[0] + k, c[1] + k], fill=shade(0.22 + 0.68 * math.sin(t * math.pi)) + (255,))
        for j in range(16):
            a = j * math.pi / 8
            d.line([(c[0] + math.cos(a) * 2.9 * K, c[1] + math.sin(a) * 2.9 * K), (c[0] + math.cos(a) * 4.0 * K, c[1] + math.sin(a) * 4.0 * K)],
                   fill=shade(0.1) + (255,), width=max(1, int(K * 0.35)))
        for k in range(int(2.7 * K), 0, -1):
            t = k / (2.7 * K)
            d.ellipse([c[0] - k, c[1] - k, c[0] + k, c[1] + k], fill=lerp(FLESH[0], FLESH[2], (1 - t) * 0.7) + (255,))
        for k in range(int(1.7 * K), 0, -1):
            t = k / (1.7 * K)
            o = (1 - t) * 0.7 * K
            d.ellipse([c[0] + 0.2 * K - k - o, c[1] + 0.3 * K - k - o, c[0] + 0.2 * K + k - o, c[1] + 0.3 * K + k - o],
                      fill=shade(0.2 + (1 - t) * 0.8) + (255,))
    # a soft shadow under it all, down and right
    a = im.split()[3]
    sh = Image.new("RGBA", (W, H), BLACK + (0,)); sh.putalpha(a.point(lambda v: int(v * 0.8)))
    sh = sh.transform((W, H), Image.AFFINE, (1, 0, -0.8 * K, 0, 1, -1.0 * K)).filter(ImageFilter.GaussianBlur(K))
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0)); out.alpha_composite(sh); out.alpha_composite(im)
    return out.resize((LOGO_SIZE[0] * scale, LOGO_SIZE[1] * scale), Image.LANCZOS)


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
    rnd = random.Random(0x61E7)
    bg = background(rnd)
    q = bg.quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = [max(v, 8) for v in q.getpalette()[:64 * 3]]   # (no colour quite black: a drawn screen has none at 0)
    idx = list(q.get_flattened_data()) if hasattr(q, 'get_flattened_data') else list(q.getdata())
    sprites = {"EYE": knob(), "CELL_OFF": cell(False), "CELL_ON": cell(True),
               "VERT_OFF": vertebra(False), "VERT_ON": vertebra(True), "LOGO": logo()}
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
