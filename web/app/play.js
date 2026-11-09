// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 joakimskurk
// BIOFORGE on its own page: the firmware runs in the audio thread (worklet.js, bioforge.wasm); here its screen, its
// lights and the FM-1's controls: the knobs (drag, scroll or the arrow keys), the buttons (held while pressed; SEL
// latches, so a mouse can turn a knob with it held), the 27 keys (click or touch, several at once), the computer's
// keyboard, and a MIDI keyboard straight into the firmware (velocity, a clock). Nothing loads before Start (a browser
// plays sound only after a click). After ORBITRIX's and RoboSeq's web/app/play.js.
const $ = (id) => document.getElementById(id);
const fm1 = $('fm1');
if (fm1) {
  const lcd = $('lcd'), g = lcd.getContext('2d');
  const small = document.createElement('canvas');
  small.width = small.height = 240;
  const gs = small.getContext('2d');
  const img = gs.createImageData(240, 240);
  const px32 = new Uint32Array(img.data.buffer);
  const status = $('appstatus');
  let ac = null, node = null, gain = null, booting = false, held = 0;

  const say = (t) => { status.textContent = t; };
  const send = (m) => node && node.port.postMessage(m);

  function draw(p) {                             // RGB565 -> the canvas, at its size, sharp
    for (let i = 0; i < 57600; i++) {
      const c = p[i];
      const r = ((c >> 11) * 527 + 23) >> 6, gg = (((c >> 5) & 63) * 259 + 33) >> 6, b = ((c & 31) * 527 + 23) >> 6;
      px32[i] = 0xFF000000 | (b << 16) | (gg << 8) | r;
    }
    gs.putImageData(img, 0, 0);
    g.imageSmoothingEnabled = false;
    g.drawImage(small, 0, 0, lcd.width, lcd.height);
  }

  const capture = (el, e) => { try { el.setPointerCapture(e.pointerId); } catch (err) { /* (no such pointer) */ } };

  // ---- the keys: F3 .. G5, black where the octave has them
  const isBlack = (k) => (0x54A >> ((53 + k) % 12)) & 1;
  const NAME = ['F', 'F#', 'G', 'G#', 'A', 'A#', 'B', 'C', 'C#', 'D', 'D#', 'E'];
  const keysEl = $('keys'), keyEls = [];
  const whites = [...Array(27).keys()].filter((k) => !isBlack(k)).length;
  let wi = 0;
  for (let k = 0; k < 27; k++) {
    const el = document.createElement('button');
    el.type = 'button';
    el.tabIndex = -1;
    el.setAttribute('aria-label', NAME[k % 12] + (3 + Math.floor((k + 5) / 12)));
    if (isBlack(k)) {
      el.className = 'kb';
      el.style.left = `calc(${(100 * wi) / whites}% - ${(100 * 0.32) / whites}%)`;
      el.style.width = `${(100 * 0.64) / whites}%`;
    } else {
      el.className = 'kw';
      el.style.left = `${(100 * wi) / whites}%`;
      el.style.width = `${100 / whites}%`;
      wi++;
    }
    keysEl.appendChild(el);
    keyEls.push(el);
  }
  const press = (k, on) => {
    const was = held;
    held = on ? held | (1 << k) : held & ~(1 << k);
    keyEls[k].classList.toggle('down', !!(held & (1 << k)));
    if (held !== was) send({ type: 'keys', mask: held });
  };
  const touching = new Map();
  keysEl.addEventListener('pointerdown', (e) => {
    const k = keyEls.indexOf(e.target);
    if (k < 0) return;
    e.preventDefault();
    capture(keysEl, e);
    touching.set(e.pointerId, k);
    press(k, true);
  });
  const lift = (e) => { const k = touching.get(e.pointerId); if (k !== undefined) { touching.delete(e.pointerId); press(k, false); } };
  keysEl.addEventListener('pointerup', lift);
  keysEl.addEventListener('pointercancel', lift);
  keysEl.addEventListener('pointermove', (e) => {
    if (!touching.has(e.pointerId)) return;
    const el = document.elementFromPoint(e.clientX, e.clientY), k = keyEls.indexOf(el);
    const was = touching.get(e.pointerId);
    if (k >= 0 && k !== was) { press(was, false); touching.set(e.pointerId, k); press(k, true); }
  });

  // ---- the buttons: held while pressed; SEL latches (a click holds it, a click lets go), and Shift holds it too
  const SEL = 3;
  const btnEls = [...fm1.querySelectorAll('[data-btn]')];
  let selLatched = false, selShift = false;
  const selSet = () => {
    const on = selLatched || selShift;
    send({ type: on ? 'down' : 'up', id: SEL });
    btnEls.filter((b) => +b.dataset.btn === SEL).forEach((b) => b.classList.toggle('latched', on));
  };
  for (const b of btnEls) {
    const id = +b.dataset.btn;
    if (id === SEL) { b.addEventListener('click', () => { selLatched = !selLatched; selSet(); }); continue; }
    let down = false;
    b.addEventListener('pointerdown', (e) => { e.preventDefault(); down = true; capture(b, e); b.classList.add('down'); send({ type: 'down', id }); });
    const up = () => { if (!down) return; down = false; b.classList.remove('down'); send({ type: 'up', id }); };
    b.addEventListener('pointerup', up);
    b.addEventListener('pointercancel', up);
    b.addEventListener('keydown', (e) => { if (e.key === 'Enter' && !e.repeat) { send({ type: 'btn', id }); e.preventDefault(); } });
  }

  // the computer's keyboard: two rows as a piano's, the white keys Z .. / and Q .. Y, the black ones above them
  const CODE = { KeyZ: 0, KeyS: 1, KeyX: 2, KeyD: 3, KeyC: 4, KeyF: 5, KeyV: 6, KeyB: 7, KeyH: 8, KeyN: 9, KeyJ: 10,
                 KeyM: 11, Comma: 12, KeyL: 13, Period: 14, Semicolon: 15, Slash: 16, Quote: 17, KeyQ: 18, KeyW: 19,
                 Digit3: 20, KeyE: 21, Digit4: 22, KeyR: 23, KeyT: 24, Digit6: 25, KeyY: 26 };
  const BTNKEY = { Space: 12, Escape: 8, Enter: 13 };   // PLAY, HOME, REC
  let active = false;                            // (only while the FM-1 is the one played: a click on it)
  document.addEventListener('pointerdown', (e) => { active = fm1.contains(e.target); fm1.classList.toggle('active', active && !!node); });
  document.addEventListener('keydown', (e) => {
    if (!node || !active || e.metaKey || e.ctrlKey || e.altKey) return;
    if (e.key === 'Shift') { if (!selShift) { selShift = true; selSet(); } return; }
    if (e.code in CODE) { e.preventDefault(); if (!e.repeat) press(CODE[e.code], true); }
    else if (e.code in BTNKEY) { e.preventDefault(); if (!e.repeat) send({ type: 'btn', id: BTNKEY[e.code] }); }
  });
  document.addEventListener('keyup', (e) => {
    if (e.key === 'Shift' && selShift) { selShift = false; selSet(); }
    if (e.code in CODE) press(CODE[e.code], false);
  });
  window.addEventListener('blur', () => { for (let k = 0; k < 27; k++) if (held & (1 << k)) press(k, false); if (selShift) { selShift = false; selSet(); } });

  // ---- the knobs: a detent each 12 px dragged, each notch of a wheel, each arrow key; their mark turns with them
  const ANGLE = 18;
  for (const k of fm1.querySelectorAll('[data-enc], #vol')) {
    let turned = k.id === 'vol' ? 8 : 0, y0 = null, acc = 0;
    const show = () => { k.style.setProperty('--a', `${k.id === 'vol' ? -135 + turned * 27 : turned * ANGLE}deg`); };
    const turn = (n) => {
      if (k.id === 'vol') {                       // MASTER: the page's volume, 0 .. 10
        turned = Math.max(0, Math.min(10, turned + n));
        if (gain) gain.gain.value = (turned / 10) ** 2;
        k.setAttribute('aria-valuenow', String(turned * 10));
      } else {
        turned += n;
        send({ type: 'enc', id: +k.dataset.enc, n });
      }
      show();
    };
    k.addEventListener('pointerdown', (e) => { e.preventDefault(); capture(k, e); y0 = e.clientY; acc = 0; k.focus(); });
    k.addEventListener('pointermove', (e) => {
      if (y0 === null) return;
      acc += y0 - e.clientY; y0 = e.clientY;
      while (Math.abs(acc) >= 12) { const n = acc > 0 ? 1 : -1; turn(n); acc -= 12 * n; }
    });
    const end = () => { y0 = null; };
    k.addEventListener('pointerup', end);
    k.addEventListener('pointercancel', end);
    k.addEventListener('wheel', (e) => { e.preventDefault(); turn(e.deltaY < 0 ? 1 : -1); }, { passive: false });
    k.addEventListener('keydown', (e) => {
      const n = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1 }[e.key];
      if (n) { e.preventDefault(); e.stopPropagation(); turn(n); }
    });
    show();
  }

  // ---- a MIDI keyboard: its messages go into the firmware (notes with velocity, CC 1, pressure, a clock)
  $('midi').addEventListener('click', async () => {
    if (!navigator.requestMIDIAccess) { say('This browser cannot use a MIDI keyboard (Chrome or Edge can).'); return; }
    try {
      const midi = await navigator.requestMIDIAccess();
      const hook = () => {
        for (const input of midi.inputs.values())
          input.onmidimessage = ({ data }) => { if (data.length >= 1 && data[0] !== 0xF0) send({ type: 'midi', st: data[0], d1: data[1] || 0, d2: data[2] || 0 }); };
      };
      hook();
      midi.onstatechange = hook;
      $('midi').setAttribute('aria-pressed', 'true');
      say(midi.inputs.size ? 'A MIDI keyboard plays BIOFORGE: channel 1 the focus, 2..10 HEART..NOISE; its start and stop run the transport.' : 'No MIDI keyboard found yet: plug one in.');
    } catch (err) { say('MIDI was not allowed.'); }
  });

  // ---- the store BIOFORGE keeps (its settings, its four scenes in their slots), kept in this browser
  const STORE = 'bioforge-store-', NSTORE = 17;   // the settings, 8 blobs, 8 slots (the four scenes, A and B)
  const loadStore = () => {
    const out = [];
    try {
      for (let i = 0; i < NSTORE; i++) {
        const s = localStorage.getItem(STORE + i);
        if (s) out.push([i, Uint8Array.from(atob(s), (c) => c.charCodeAt(0))]);
      }
    } catch (err) { /* (no storage here: it starts as new) */ }
    return out;
  };
  const keepStore = (store) => {
    try {
      for (const [i, b] of store) {
        if (!b[0]) { localStorage.removeItem(STORE + i); continue; }
        let s = '';
        for (let j = 0; j < b.length; j += 0x8000) s += String.fromCharCode(...b.subarray(j, j + 0x8000));
        localStorage.setItem(STORE + i, btoa(s));
      }
    } catch (err) { /* (kept for this visit only) */ }
  };

  const keyLeds = (leds, glow) => {              // bits 14.. are the keys
    const lit = (w, i) => (w[(i / 32) | 0] >>> (i % 32)) & 1;
    for (let k = 0; k < 27; k++) {
      keyEls[k].classList.toggle('lit', !!lit(leds, 14 + k));
      keyEls[k].classList.toggle('glow', !lit(leds, 14 + k) && !!lit(glow, 14 + k));
    }
  };
  function show(leds, glow) {
    for (const b of btnEls) b.classList.toggle('lit', !!((leds[0] >>> +b.dataset.btn) & 1));
    keyLeds(leds, glow);
  }

  async function start() {
    if (booting) return;
    if (ac) {                                    // a pause, a go on
      if (ac.state === 'running') { await ac.suspend(); $('start').hidden = false; $('start').textContent = '▶ Go on'; say('Paused.'); }
      else { await ac.resume(); $('start').hidden = true; say('Running.'); }
      return;
    }
    booting = true;
    say('Loading the firmware…');
    try {
      ac = new AudioContext({ sampleRate: 44100, latencyHint: 'interactive' });
      const [wasm] = await Promise.all([
        fetch('app/bioforge.wasm').then((r) => { if (!r.ok) throw new Error(`bioforge.wasm: ${r.status}`); return r.arrayBuffer(); }),
        ac.audioWorklet.addModule('app/worklet.js'),
      ]);
      node = new AudioWorkletNode(ac, 'bioforge', { numberOfInputs: 0, outputChannelCount: [2] });
      gain = ac.createGain();
      gain.gain.value = 0.64;
      node.connect(gain).connect(ac.destination);
      node.port.onmessage = (e) => {
        const m = e.data;
        if (m.type === 'frame') { if (m.lcd) draw(m.lcd); show(m.leds, m.glow); if (m.store) keepStore(m.store); }
        else if (m.type === 'ready') {
          $('start').hidden = true; fm1.classList.add('on'); fm1.classList.toggle('active', active);
          say('Running: RITUAL 00, THE AWAKENING. Press PLAY (or Space). PRESETS turns the scene.');
        } else if (m.type === 'error') say(`It did not start: ${m.message}`);
      };
      node.port.postMessage({ type: 'boot', wasm, store: loadStore() }, [wasm]);
      await ac.resume();
    } catch (err) {
      say(`It did not start here: ${err.message || err}. It needs a browser with Web Audio worklets (any recent one).`);
      ac = null;
      node = null;
    }
    booting = false;
  }
  $('start').addEventListener('click', start);
  $('pause').addEventListener('click', () => { if (ac) start(); });
  $('forget').addEventListener('click', () => {
    try { for (let i = 0; i < NSTORE; i++) localStorage.removeItem(STORE + i); } catch (err) { /* (nothing kept) */ }
    say('What BIOFORGE kept in this browser (its settings and its saved scenes) is forgotten: the next visit starts as new.');
  });
}
