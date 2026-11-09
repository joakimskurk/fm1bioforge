#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A look at a render (numpy, Pillow): a spectrogram on a log-frequency axis (30 Hz..20 kHz) over a waveform's envelope,
as a PNG, and the numbers: peak, RMS, DC, the spectral centroid, the energy above 16 kHz, the largest jump between
frames (a click).
  python3 tools/bio_look.py IN.wav OUT.png [SECONDS]"""
import sys, wave
import numpy as np
from PIL import Image, ImageDraw

def load(p):
    w = wave.open(p)
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768.0
    return x.reshape(-1, w.getnchannels()).mean(axis=1), w.getframerate()

def main():
    x, fs = load(sys.argv[1])
    if len(sys.argv) > 3: x = x[: int(float(sys.argv[3]) * fs)]
    n = 2048
    hop = max(16, (len(x) - n) // 1000)
    win = np.hanning(n)
    frames = [x[i:i + n] * win for i in range(0, max(1, len(x) - n), hop)]
    S = np.abs(np.fft.rfft(np.array(frames), axis=1)) + 1e-9
    db = 20 * np.log10(S / n * 4)
    freqs = np.fft.rfftfreq(n, 1 / fs)
    W, H = min(1200, len(frames)), 300
    ys = np.geomspace(30, 20000, H)[::-1]
    idx = np.clip(np.searchsorted(freqs, ys), 0, len(freqs) - 1)
    cols = np.linspace(0, len(frames) - 1, W).astype(int)
    img = db[cols][:, idx].T
    img = np.clip((img + 100) / 100, 0, 1)
    rgb = np.stack([img ** 0.7 * 255, img ** 1.6 * 255, img ** 3 * 140 + 20 * img], axis=-1).astype(np.uint8)
    env = np.array([np.max(np.abs(x[c * hop: c * hop + n])) for c in cols])
    out = Image.new("RGB", (W, H + 80), (8, 6, 6))
    out.paste(Image.fromarray(rgb), (0, 0))
    d = ImageDraw.Draw(out)
    for f in (50, 100, 200, 500, 1000, 2000, 5000, 10000):
        y = int(np.argmin(np.abs(ys - f)))
        d.line([(0, y), (6, y)], fill=(200, 200, 200)); d.text((8, y - 6), str(f), fill=(160, 160, 160))
    for i in range(W - 1):
        d.line([(i, H + 79 - env[i] * 78), (i + 1, H + 79 - env[i + 1] * 78)], fill=(140, 255, 42))
    out.save(sys.argv[2])
    spec = S.mean(axis=0)
    cen = float((spec * freqs).sum() / spec.sum())
    hi = float((spec[freqs > 16000] ** 2).sum() / (spec ** 2).sum())
    jump = float(np.max(np.abs(np.diff(x)))) if len(x) > 1 else 0
    print(f"{sys.argv[1]}: {len(x)/fs:.2f} s, peak {20*np.log10(np.max(np.abs(x))+1e-9):.1f} dBFS, "
          f"RMS {20*np.log10(np.sqrt(np.mean(x**2))+1e-9):.1f} dBFS, DC {np.mean(x):+.5f}, centroid {cen:.0f} Hz, "
          f">16 kHz {10*np.log10(hi+1e-12):.1f} dB, largest step {jump:.3f}")

main()
