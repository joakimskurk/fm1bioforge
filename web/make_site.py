#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 joakimskurk
"""BIOFORGE's site (GitHub Pages, https://joakimskurk.github.io/fm1bioforge/): its page, BIOFORGE to play in the
browser, the demos to hear, the installer.

  index.html                  web/index.html with fm1pkg.js and fm1ota.js (SLOOP's, unchanged) and the metadata
                              inlined: the installer (Web MIDI, Chrome or Edge), the demos
  app/                        BIOFORGE to play: bioforge.wasm (web/app/build.sh: the firmware's own code on
                              web/app/web_host.c), worklet.js (the audio thread), play.js (the FM-1 drawn)
  firmware/bioforge-VER.fwsc  the package the page installs (its SHA-256 in the page: a damaged download is never
                              offered)
  audio/                      the demos (tools/make_demos.sh: the firmware on the host, played by scripts), AAC, and
                              each one's spectrum as a picture (WebP), low to high
  screens/                    the logo, the icon, the link preview, three screens, the ground (the firmware's relief)
  .nojekyll                   (served as it is)

  sh tools/make_demos.sh       (first: the demos)
  sh web/app/build.sh          (BIOFORGE for the browser)
  python3 web/make_site.py build/bioforge.fwsc VERSION OUT_DIR

The package must be BIOFORGE's (its identity FM-1_908..) with its update loader in it (which the installer talks to).
The audio is encoded by ffmpeg. After ORBITRIX's, RoboSeq's and Star Distroyer's web/make_site.py, after SLOOP's
(Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; isod89).
"""
import hashlib
import json
import re
import shutil
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
APP = {"build/web/bioforge.wasm": "bioforge.wasm", "web/app/worklet.js": "worklet.js", "web/app/play.js": "play.js"}
SCREENS = ("seq", "land", "mut")            # (a few: the page is the FM-1 itself)
DEMOS = [   # build/audio/demos/N_name.wav, in order (tests/scripts/demo_N_name.txt plays each; tools/make_demos.sh
            # renders every script, the page carries these eight)
    ("2_rust", "RITUAL 01 · RUST",
     "Grim dark industrial ambience with a pulse and a melody, 108 BPM. A drone for a body (a MASS of octaves and a "
     "fifth on D1, a CLUSTER of semitones over it), a smouldering rumble and a wash of noise; a deep sub kick and its "
     "rumble, a bone breaking far off, dark slices of noise locked in 16ths; a bell the register plays from the scale. "
     "Made against a reference recording: its spectrum, its width, its weight."),
    ("3_abyss", "RITUAL 02 · ABYSS",
     "Dark ambient meets noise, 110 BPM. A deep kick with almost no top on the four and its RUMBLE under it; a fire "
     "crackling, the reverb and the delay breathing with it; ticks of six squares off the beat, sparks in time; wide, "
     "drifting dub chords off the beat into a dark delay, a bass on the off-beats, a wide drone and a choir in a cave."),
    ("5_frozen", "FROZEN",
     "Rhythm made of noise. Each 16th a slice of noise with its own band, length, shape and place; the register locks "
     "them, so the same noise comes back every bar and is the groove. LOOP down: slice after slice slips to new noise; "
     "back to LOCK: what it has become stays."),
    ("16_grinder", "GRINDER",
     "Industrial techno, 140 BPM, E Phrygian dominant: a hard driven kick, a crushed snare, metal hats, a rolling bass "
     "between the kicks, an iron stab, slices of metal noise locked; the crusher on. The hard, bright end."),
    ("20_lament", "LAMENT",
     "A melody, 92 BPM, C harmonic minor: a choir sings four bars (i, iv, V with its raised seventh, i), glass arped up "
     "and down through each chord, the bass on the roots, a soft kick and a clap far off, a hall."),
    ("21_shrine", "SHRINE",
     "Sweetness, ruptured: E Lydian, no beat but a clock. Glass arped up and down a major ninth, a choir on the sharp "
     "fourth, chimes; all of it through a wavering tape (WARP), and the nervous system cutting into it (RUPTURE): a "
     "silence, a stutter, a bar screwed down an octave, the same cuts each time round."),
    ("22_mirage", "MIRAGE",
     "A loop chopped and screwed by the nervous system: 64 BPM, C# Dorian. A choir's phrase, a mass, a bell sparkling "
     "from the chord; the tape wavering hard, cut up most of the time, and now and then crushed noise breaking in."),
    ("15_scar", "SCAR · ABYSS PLAYED",
     "ABYSS with SCAR, the output's last 0.37 s kept: ARP held, four white keys play it back while held. A roll of 16ths "
     "into the bar, darker each time round; the 8th just played, backwards; a repeat of 8ths; a bar smeared into "
     "crossing grains; let go, the music is where it would have been."),
]
SPEC_W, SPEC_H, SPEC_N = 1200, 160, 8192
SPEC_LO, SPEC_HI, SPEC_DB = 40.0, 16000.0, 75.0
FLESH = [(0.00, (7, 7, 9)), (0.30, (30, 30, 34)), (0.55, (74, 74, 80)), (0.75, (150, 150, 156)),
         (0.90, (176, 214, 150)), (1.00, (240, 248, 236))]   # graphite, steel, the firmware's pale green (D26)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def read_wav(path):
    with wave.open(str(path)) as f:
        ch, fs = f.getnchannels(), f.getframerate()
        x = np.frombuffer(f.readframes(f.getnframes()), dtype="<i2").astype(np.float32)
    return x.reshape(-1, ch).mean(axis=1) / 32768.0, fs


def spectrum(x, fs, pic):
    """the piece's spectrum, left to right: a column a slice, the louder the brighter (graphite, steel, green)"""
    win = np.hanning(SPEC_N).astype(np.float32)
    x = np.concatenate([x, np.zeros(SPEC_N, np.float32)])
    hop = (len(x) - 2 * SPEC_N) / (SPEC_W - 1)
    freqs = np.fft.rfftfreq(SPEC_N, 1.0 / fs)
    edges = np.geomspace(SPEC_LO, SPEC_HI, SPEC_H + 1)
    cut = np.append(np.searchsorted(freqs, edges[:-1]), np.searchsorted(freqs, edges[-1]))
    db = np.empty((SPEC_H, SPEC_W), np.float32)
    for c in range(SPEC_W):
        s = int(c * hop)
        m = np.abs(np.fft.rfft(x[s:s + SPEC_N] * win))
        db[:, c] = 20.0 * np.log10(np.maximum.reduceat(m, cut)[:-1] + 1e-9)
    ref = np.percentile(db, 99.8)
    t = np.clip((db - (ref - SPEC_DB)) / SPEC_DB, 0.0, 1.0)[::-1] ** 1.25
    rgb = np.zeros(t.shape + (3,), np.float32)
    for (t0, c0), (t1, c1) in zip(FLESH, FLESH[1:]):
        k = ((t >= t0) & (t <= t1))[..., None]
        f = ((t - t0) / (t1 - t0))[..., None]
        rgb = np.where(k, np.array(c0) + f * (np.array(c1) - np.array(c0)), rgb)
    Image.fromarray(rgb.astype(np.uint8), "RGB").save(pic, quality=84, method=6)


def encode(wav, m4a):
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", str(wav), "-c:a", "aac", "-b:a", "160k", "-movflags",
                    "+faststart", str(m4a)], check=True)


def logo_png(out, scale):
    """the logo, as the firmware draws it (bio_art.h's LOGO), scaled up, sharp"""
    src = (ROOT / "firmware" / "gen" / "bio_art.h").read_text()
    w = int(re.search(r"#define LOGO_W (\d+)", src).group(1))
    h = int(re.search(r"#define LOGO_H (\d+)", src).group(1))
    cols = [int(v) for v in re.search(r"static const uint16_t LOGO\[\d+\] = \{([^}]*)\}", src).group(1).split(",")]
    alph = [int(v) for v in re.search(r"static const uint8_t LOGO_A\[\d+\] = \{([^}]*)\}", src).group(1).split(",")]
    im = Image.new("RGBA", (w, h))
    px = im.load()
    for i, c in enumerate(cols):
        a = (alph[i >> 1] >> 4) if not i & 1 else alph[i >> 1] & 15
        r, g, b = (c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3
        px[i % w, i // w] = (r | r >> 5, g | g >> 6, b | b >> 5, a * 17)
    im.resize((w * scale, h * scale), Image.NEAREST).save(out)


def tissue(out):
    """the page's ground: the firmware's own relief (tools/gen_art.py, without its panels), darker, to tile"""
    sys.path.insert(0, str(ROOT / "tools"))
    import gen_art
    import random
    im = gen_art.background(random.Random(0x61E7), panels=False).point(lambda v: int(v * 0.55))
    im.save(out, quality=78, method=6)
    return im


def og_image(out, ground):
    """the link preview, 1200 x 630: three screens on the relief, the logo over them, EARLY ALPHA"""
    im = Image.new("RGB", (1200, 630), (7, 7, 9))
    g = ground.resize((960, 960))
    for x in (0, 960):
        im.paste(g, (x, -165))
    im = Image.blend(im, Image.new("RGB", im.size, (7, 7, 9)), 0.35)
    for i, n in enumerate(SCREENS):
        sc = Image.open(ROOT / "docs" / "screens" / f"{n}.png").convert("RGB").resize((340, 340), Image.NEAREST)
        im.paste(sc, (45 + i * 380, 240))
    logo = Image.open(out.parent / "logo.png").convert("RGBA")
    im.paste(logo, ((1200 - logo.width) // 2, 40), logo)
    d = ImageDraw.Draw(im)                      # EARLY ALPHA, in the firmware's 5 x 7 font, blood red, a frame round it
    sys.path.insert(0, str(ROOT / "tools"))
    import gen_art
    text, k, x0, y0 = "EARLY ALPHA", 4, 0, 0
    w = len(text) * 6 * k - k
    x0, y0 = 1200 - 60 - w, 176
    d.rectangle([x0 - 12, y0 - 10, x0 + w + 12, y0 + 7 * k + 10], fill=(10, 4, 5), outline=(168, 28, 34), width=3)
    for i, ch in enumerate(text):
        for gy, row in enumerate(gen_art.F5[ch]):
            for gx, v in enumerate(row):
                if v == "#":
                    x, y = x0 + (i * 6 + gx) * k, y0 + gy * k
                    d.rectangle([x, y, x + k - 1, y + k - 1], fill=(168, 28, 34))
    im.save(out)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not product.startswith("FM-1_908"):
        raise SystemExit(f"{pkg}: identity {product!r} is not BIOFORGE's (FM-1_908..)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it: the installer could not write it")
    for src in APP:
        if not (ROOT / src).exists():
            raise SystemExit(f"{src}: missing (web/app/build.sh)")
    wavs = {w.stem: w for w in (ROOT / "build" / "audio" / "demos").glob("*.wav")}
    missing = [d[0] for d in DEMOS if d[0] not in wavs]
    if missing:
        raise SystemExit(f"build/audio/demos: {', '.join(missing)} missing (tools/make_demos.sh renders them)")
    name = f"bioforge-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for sub in ("firmware", "screens", "audio", "app"):
        (out / sub).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    for src, dst in APP.items():
        shutil.copy(ROOT / src, out / "app" / dst)
    for n in SCREENS:
        shutil.copy(ROOT / "docs" / "screens" / f"{n}.png", out / "screens" / f"{n}.png")
    logo_png(out / "screens" / "logo.png", 6)
    logo_png(out / "screens" / "icon.png", 2)
    ground = tissue(out / "screens" / "tissue.webp")
    og_image(out / "screens" / "og.png", ground)
    demos = []
    for stem, title, about in DEMOS:
        x, fs = read_wav(wavs[stem])
        spectrum(x, fs, out / "audio" / f"{stem}.webp")
        encode(wavs[stem], out / "audio" / f"{stem}.m4a")
        demos.append({"id": stem, "title": title, "about": about, "src": f"audio/{stem}.m4a",
                      "spectrum": f"audio/{stem}.webp", "secs": round(len(x) / fs, 1)})
    meta = json.dumps({"version": version, "product": product, "pkg": "firmware/" + name,
                       "sha256": hashlib.sha256(raw).hexdigest(), "size": len(raw)})
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8"))
    html = (HERE / "index.html").read_text(encoding="utf-8")
    for mark in ("/*LIB*/", "/*META*/", "/*DEMOS*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"web/index.html must hold {mark} once")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta).replace("/*DEMOS*/", json.dumps(demos))
    (out / "index.html").write_text(html, encoding="utf-8")
    (out / ".nojekyll").write_text("")
    size = sum(p.stat().st_size for p in out.rglob("*") if p.is_file())
    print(f"site: {out}: index.html ({len(html)} B), firmware/{name} ({len(raw)} B, {product}), {len(demos)} demos, "
          f"BIOFORGE to play ({(out / 'app' / 'bioforge.wasm').stat().st_size // 1024} KB), {size // 1024} KB in all")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
