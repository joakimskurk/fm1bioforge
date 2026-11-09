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
  screens/                    the logo, the icon, the link preview, the pages' screens
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
SCREENS = ("seq", "syn", "mut", "mix", "fx", "voice", "perc")
DEMOS = [   # build/audio/demos/N_name.wav, in order (tests/scripts/demo_N_name.txt plays each)
    ("1_awakening", "RITUAL 00 · THE AWAKENING",
     "The start scene, sixteen bars: HEART on the four, BONE on two and four, TEETH and CLAW between, the BASS "
     "sparingly in D Phrygian, the DRONE breathing under it all, SPASM's two nerve impulses, the VOICE held back."),
    ("2_static", "RITUAL 01 · STATIC CULT",
     "132 BPM, noise as percussion: a flam snare of noise, ticking sixteenths, metal off the beat, an open hiss, over "
     "a tight HEART and a rolling bass. PUMP ducks the rest under the kick; for the last eight bars it goes to 90 %."),
    ("3_splice", "RITUAL 02 · SPLICE",
     "100 BPM, broken and glitched: a 32-step body, a crushed snare, grains and zaps in ratchets and chances, a bit-hat "
     "on 12 steps against it, the bass gliding, the throat in stabs. After six bars LOOP goes to LOCK: it keeps what it "
     "plays."),
    ("4_void", "RITUAL 03 · VOID",
     "70 BPM and wide: a huge HEART twice a bar with the drone breathing under it, noise washes held for bars, crackle "
     "in the dark, the throat singing long notes, a long reverb."),
    ("5_kit", "THE NOISE KIT, SOUND BY SOUND",
     "STATIC CULT plays while TEETH's SOUND is turned one step a bar through the sixteen noise sounds: TICK, HISS, "
     "METAL, DIGI SNR, CLAP, ZAP, RISE, CRUSH, CRACKLE, GRAIN, PING, THUMP, WASH, FLAM SNR, BIT HAT, LASER; then its "
     "own body (TISSUE) again."),
    ("6_body", "THE BODY, ORGAN BY ORGAN",
     "Played on the keys. The kit: HEART at three tunings, BONE, TEETH closed then open, CLAW short and dragged. "
     "Then BASS, a DRONE held, SPASM, the VOICE's throat, NOISE breathing."),
    ("7_infection", "SLOW INFECTION, THEN UNWIND",
     "On MUT the LOOP is turned down from LOCK a little at a time and RATTLE comes in: the texture changes, the "
     "rhythm's anchors stay. Then RATTLE out and LOOP back to LOCK: a machine calming down."),
    ("8_build", "FROM NOTHING TO A LOOP",
     "An empty body. PLAY, REC: HEART and BONE played in, TEETH on the off-beats; then BONE becomes a CLAP and TEETH a "
     "TICK, the same notes in noise; a bass line in one bar; then the nerve moves the texture, and is locked again."),
]
SPEC_W, SPEC_H, SPEC_N = 1200, 160, 8192
SPEC_LO, SPEC_HI, SPEC_DB = 40.0, 16000.0, 75.0
FLESH = [(0.00, (10, 6, 7)), (0.30, (58, 14, 22)), (0.55, (150, 40, 52)), (0.75, (214, 200, 176)),
         (0.90, (140, 255, 42)), (1.00, (255, 255, 255))]


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
    """the piece's spectrum, left to right: a column a slice, the louder the brighter (black, flesh, bone, green)"""
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


def og_image(out):
    """the link preview, 1200 x 630: three screens on the tissue, the logo over them"""
    im = Image.new("RGB", (1200, 630), (10, 6, 7))
    d = ImageDraw.Draw(im)
    rng = np.random.default_rng(9)
    for _ in range(90):                         # veins
        x, y = rng.integers(0, 1200), rng.integers(0, 630)
        pts = [(x, y)]
        a = rng.uniform(0, 6.3)
        for _ in range(6):
            a += rng.uniform(-0.8, 0.8)
            x, y = x + np.cos(a) * 26, y + np.sin(a) * 26
            pts.append((x, y))
        d.line(pts, fill=(46, 14, 20), width=2)
    for i, n in enumerate(("seq", "syn", "mut")):
        s = Image.open(ROOT / "docs" / "screens" / f"{n}.png").convert("RGB").resize((340, 340), Image.NEAREST)
        im.paste(s, (45 + i * 380, 230))
    logo = Image.open(out.parent / "logo.png").convert("RGBA")
    im.paste(logo, ((1200 - logo.width) // 2, 40), logo)
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
    og_image(out / "screens" / "og.png")
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
