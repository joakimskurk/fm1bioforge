#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk)
"""BIOFORGE's pictures: firmware/gen/bio_art.h (Pillow). Drawn here, at four times the size and brought down, from
shapes and numbers only: no image of anyone else's goes in. The look (D26): biomechanical, after the feel of the
pictures the user gave (chrome and graphite, ribbed hoses, vertebrae, smooth capsules, airbrushed light), in the dark;
a pale green the only colour for sound, a dark blood red for recording.

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


def background(rnd, panels=True):
    """the screen's background; panels False: the relief alone, at 4x (the page's ground, web/make_site.py)"""
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
    if not panels:
        return im
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


def logo():
    text = "BIOFORGE"; W, H = len(text) * 12 + 2, 22
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); px = im.load()
    mask = [[0] * W for _ in range(H)]
    for i, ch in enumerate(text):
        for gy, row in enumerate(F5[ch]):
            for gx, v in enumerate(row):
                if v == '#':
                    for sy in range(2):
                        for sx in range(2):
                            mask[1 + gy * 2 + sy][1 + i * 12 + gx * 2 + sx] = 1
    for y in range(H):
        for x in range(W):
            if mask[y][x]:
                t = (y - 1) / 13                # chrome: bright, a soft band at the horizon (D30: no black line through
                v = 0.95 - 0.3 * math.exp(-((t - 0.55) / 0.12) ** 2) - 0.25 * t   # the word), bright again under it
                px[x, y] = shade(v) + (255,)
            elif any(0 <= y + dy < H and 0 <= x + dx < W and mask[y + dy][x + dx] for dy in (-1, 0, 1) for dx in (-1, 0, 1)):
                px[x, y] = BLACK + (230,)
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
    rnd = random.Random(0x61E7)
    bg = background(rnd)
    q = bg.quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = [max(v, 8) for v in q.getpalette()[:64 * 3]]   # (no colour quite black: a drawn screen has none at 0)
    idx = list(q.get_flattened_data()) if hasattr(q, 'get_flattened_data') else list(q.getdata())
    sprites = {"EYE": knob(), "CELL_OFF": cell(False), "CELL_ON": cell(True),
               "VERT_OFF": vertebra(False), "VERT_ON": vertebra(True), "LOGO": logo()}
    out = ["/* SPDX-License-Identifier: GPL-3.0-only */\n/* Copyright (C) 2026 Joakim (joakimskurk) */\n/* generated by tools/gen_art.py: never edit (run it again) */\n",
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
