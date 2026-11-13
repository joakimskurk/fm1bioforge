#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk)
"""tools/emu_check.sh's checks of an fm1-emu run (numpy, Pillow).
  isr REPORT MAX         the interrupts' share of the chip, under MAX %
  load REPORT            the engine's own isr_load (from the dump), printed
  sound WAV T0 T1        sound between T0 and T1 s (RMS over -50 dBFS)
  screen BMP...          each screen drawn (not black)"""
import re
import sys
import wave
from pathlib import Path


def main():
    what = sys.argv[1]
    if what == "isr":
        txt = Path(sys.argv[2]).read_text(errors="replace")
        m = re.search(r"cpu0: ([\d.]+)% in an ISR, ([\d.]+)% nested", txt)
        if not m:
            print(txt[-2000:])
            print("no ISR report (did the firmware start?)")
            return 1
        tot = float(m.group(1)) + float(m.group(2))
        print(f"fm1-emu: the interrupts take {tot:.1f} % of the chip (want under {sys.argv[3]})")
        return 0 if tot < float(sys.argv[3]) else 1
    if what == "load":
        txt = Path(sys.argv[2]).read_text(errors="replace")
        m = re.search(r"^\s*0x[0-9a-f]+: ([0-9a-f]{8})", txt, re.M)
        if not m:
            print("(no dump of isr_load: build/bioforge.syms?)")
            return 0
        print(f"fm1-emu: the engine measures itself at {int(m.group(1), 16)} % of the audio interrupt's 5.8 ms")
        return 0
    if what == "sound":
        import numpy as np
        w = wave.open(sys.argv[2])
        fs, ch = w.getframerate(), w.getnchannels()
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64).reshape(-1, ch) / 32768
        a, b = int(float(sys.argv[3]) * fs), int(float(sys.argv[4]) * fs)
        seg = x[a:b]
        rms = 20 * np.log10(np.sqrt(np.mean(seg ** 2)) + 1e-12) if len(seg) else -999
        print(f"fm1-emu: {len(x) / fs:.2f} s of sound, RMS {rms:.1f} dBFS between {sys.argv[3]} and {sys.argv[4]} s")
        return 0 if rms > -50 else 1
    if what == "screen":
        from PIL import Image
        ok = 1
        for p in sys.argv[2:]:
            if not Path(p).exists():
                print(f"{p}: no screen")
                ok = 0
                continue
            im = Image.open(p).convert("L")
            lit = sum(1 for v in im.getdata() if v > 24) / (im.width * im.height)
            print(f"{p}: {100 * lit:.0f} % of it drawn")
            ok &= lit > 0.2
        return 0 if ok else 1
    print("what?", what)
    return 2


if __name__ == "__main__":
    sys.exit(main())
