# SPDX-License-Identifier: GPL-3.0-only
# The feel of a recording, measured (D23): spectrum in third octaves, band energies, loudness, width, flatness, crest,
# a pulse's clarity and tempo, persistent pitches; and a spectrogram of the whole. To compare a render with a reference.
#   python3 -I tools/bio_feel.py IN.wav OUT_PREFIX      (16-bit stereo WAV; an MP3 first: ffmpeg -i in.mp3 -ac 2 in.wav)
import sys, wave, numpy as np
from PIL import Image
fn, out = sys.argv[1], sys.argv[2]
w = wave.open(fn); sr = w.getframerate(); n = w.getnframes()
raw = np.memmap(fn, dtype=np.int16, mode='r', offset=44, shape=(n, 2))
dur = n / sr
print(f"{fn}: {dur/60:.1f} min, {sr} Hz")
# frames every 0.5 s, FFT 8192
N = 8192; hop = sr // 2
idx = np.arange(0, n - N, hop)
win = np.hanning(N)
freqs = np.fft.rfftfreq(N, 1 / sr)
bands = [(20, 60, "sub"), (60, 200, "low"), (200, 800, "lowmid"), (800, 3000, "mid"), (3000, 8000, "high"), (8000, 20000, "air")]
F = []
spec_sum = np.zeros(len(freqs))
img_cols = []
for i in idx:
    x = raw[i:i + N].astype(np.float64) / 32768
    m = x.mean(1); s = (x[:, 0] - x[:, 1]) / 2
    P = np.abs(np.fft.rfft(m * win)) ** 2
    Ps = np.abs(np.fft.rfft(s * win)) ** 2
    spec_sum += P
    rms = 20 * np.log10(np.sqrt((m ** 2).mean()) + 1e-9)
    tot = P[1:].sum() + 1e-20
    be = [10 * np.log10(P[(freqs >= a) & (freqs < b)].sum() / tot + 1e-12) for a, b, _ in bands]
    cen = (P * freqs).sum() / tot
    sel = (freqs > 100) & (freqs < 10000)
    flat = np.exp(np.mean(np.log(P[sel] + 1e-20))) / (np.mean(P[sel]) + 1e-20)
    side = 10 * np.log10(Ps[1:].sum() / tot + 1e-12)
    crest = 20 * np.log10(np.abs(m).max() / (np.sqrt((m ** 2).mean()) + 1e-9) + 1e-9)
    F.append([i / sr, rms, cen, flat, side, crest] + be)
    img_cols.append(P)
F = np.array(F)
def stat(c, name, fmt="%.1f"):
    v = F[:, c]; print(f"  {name:10s} median " + fmt % np.median(v) + "  10%% " % () + fmt % np.percentile(v, 10) + "  90%% " % () + fmt % np.percentile(v, 90))
print("per 0.5 s:")
stat(1, "RMS dBFS"); stat(2, "centroid", "%.0f"); stat(3, "flatness", "%.3f"); stat(4, "side dB"); stat(5, "crest dB")
for j, (_, _, nm) in enumerate(bands): stat(6 + j, nm + " dB")
# the long-term spectrum in third octaves
spec = spec_sum / len(idx)
print("long-term spectrum (dB re its max, third octaves):")
fc = 25 * 2 ** (np.arange(0, 30) / 3)
lt = []
for f0 in fc:
    sel = (freqs >= f0 / 2 ** (1 / 6)) & (freqs < f0 * 2 ** (1 / 6))
    lt.append(10 * np.log10(spec[sel].sum() + 1e-20))
lt = np.array(lt); lt -= lt.max()
print("  " + "  ".join(f"{f:.0f}:{v:.0f}" for f, v in zip(fc, lt) if f < 20000))
# persistent peaks (drones): in the long-term spectrum at 5 Hz resolution, 30..1500 Hz
NB = 1 << 15
fr2 = np.fft.rfftfreq(NB, 1 / sr); acc = np.zeros(len(fr2)); cnt = 0
for i in np.linspace(0, n - NB - 1, 200).astype(int):
    m = raw[i:i + NB].astype(np.float64).mean(1) / 32768
    acc += np.abs(np.fft.rfft(m * np.hanning(NB))); cnt += 1
acc /= cnt
sel = (fr2 > 30) & (fr2 < 1500)
a = 20 * np.log10(acc + 1e-12)
loc = np.where(sel[1:-1] & (a[1:-1] > a[:-2]) & (a[1:-1] > a[2:]))[0] + 1
base = np.convolve(a, np.ones(41) / 41, 'same')
prom = a[loc] - base[loc]
top = loc[np.argsort(prom)[::-1][:12]]
NOTE = "C C# D Eb E F F# G Ab A Bb B".split()
print("persistent peaks (Hz, note, dB over the floor):")
for k in sorted(top, key=lambda k: fr2[k]):
    f = fr2[k]; mid = 69 + 12 * np.log2(f / 440); nn = int(round(mid))
    print(f"  {f:7.1f} {NOTE[nn % 12]}{nn // 12 - 1} ({(mid - nn) * 100:+.0f} c)  {a[k] - base[k]:.1f}")
# rhythm: an onset envelope at 100 a second (spectral flux, 40..8000 Hz), its periodicity over 20 s windows
hopo = sr // 100; No = 2048
wo = np.hanning(No); fo = np.fft.rfftfreq(No, 1 / sr); so = (fo > 40) & (fo < 8000)
prev = None; on = []
step = 1
for i in range(0, n - No, hopo):
    m = raw[i:i + No].astype(np.float32).mean(1)
    P = np.log1p(np.abs(np.fft.rfft(m * wo))[so])
    on.append(0 if prev is None else np.maximum(P - prev, 0).sum())
    prev = P
on = np.array(on); on = on - np.convolve(on, np.ones(50) / 50, 'same'); on = np.maximum(on, 0)
win_s = 2000
tempos = []; clar = []
for st in range(0, len(on) - win_s, win_s // 2):
    seg = on[st:st + win_s]; seg = seg - seg.mean()
    ac = np.correlate(seg, seg, 'full')[win_s - 1:]; ac /= ac[0] + 1e-9
    lags = np.arange(len(ac)); bpm = 6000 / np.maximum(lags, 1)
    sel = (bpm >= 50) & (bpm <= 180)
    k = np.argmax(ac * sel); tempos.append(bpm[k]); clar.append(ac[k])
tempos = np.array(tempos); clar = np.array(clar)
print(f"rhythm: pulse clarity median {np.median(clar):.2f} (0 none .. 1 a clock), 90% {np.percentile(clar, 90):.2f}")
strong = clar > 0.3
if strong.any():
    print(f"  where there is a pulse ({strong.mean()*100:.0f}% of the time): {np.median(tempos[strong]):.1f} BPM (or its multiple)")
print(f"  onsets a second (strong ones): {(on > np.percentile(on, 99)).sum() / dur:.2f}")
np.save(out + "_feat.npy", F)
# an image: the whole file's spectrogram (log frequency), its loudness under it
cols = np.array(img_cols)
lf = np.geomspace(30, 16000, 240)
ki = np.searchsorted(freqs, lf)
S = 10 * np.log10(cols[:, ki] + 1e-14)
W = 1500
S = S[np.linspace(0, len(S) - 1, W).astype(int)]
S = (S - S.max() + 70) / 70; S = np.clip(S, 0, 1)
im = (np.stack([S ** 0.8 * 230, S ** 1.6 * 150, S ** 3 * 90], -1)).astype(np.uint8)[:, ::-1].transpose(1, 0, 2)
Image.fromarray(im).resize((W, 300)).save(out + "_spec.png")
print("spectrogram ->", out + "_spec.png")
