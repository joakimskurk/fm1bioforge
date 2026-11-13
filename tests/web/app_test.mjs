// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Joakim (joakimskurk)
// BIOFORGE for the browser (web/app/: the firmware compiled to WebAssembly, web/app/build.sh) in Node, as the page's
// worklet runs it: no imports; faster than real time; the grid on the screen at power-on; PLAY: the awakening sounds,
// the playhead moves and PLAY's light is lit; a key plays the kit; HOME: MIX; STOP: the sound dies away; an edit and
// SAVE reach the store (the page keeps it); after a restart from that store the scene comes back.
//
//   node tests/web/app_test.mjs build/web/bioforge.wasm
import { readFileSync } from 'fs';

const bytes = readFileSync(process.argv[2] || 'build/web/bioforge.wasm');
const mod = await WebAssembly.compile(bytes);
let fail = 0;
const check = (ok, what) => { console.log(`${ok ? 'ok  ' : 'FAIL'} ${what}`); if (!ok) fail = 1; };

const imports = WebAssembly.Module.imports(mod);
check(imports.length === 0, `no imports (${imports.map((i) => i.name).join(', ') || 'none'})`);

async function boot(store) {
  const { exports: x } = await WebAssembly.instantiate(mod, {});
  const size = x.web_store_size();
  for (const [i, b] of store || []) new Uint8Array(x.memory.buffer, x.web_store(i), size).set(b);
  x.web_init();
  return x;
}
const BTN = { HOME: 8, SEQ: 11, PLAY: 12, SAVE: 9, GLO: 7 };

const x = await boot();
function run(x, sec) {                           // the loudest sample of each 128 frames
  const peaks = [];
  for (let i = 0; i < Math.round((sec * 44100) / 128); i++) {
    x.web_run(128);
    const o = new Int32Array(x.memory.buffer, x.web_out_ptr(), 256);
    let p = 0;
    for (let k = 0; k < 256; k++) p = Math.max(p, Math.abs(o[k]));
    peaks.push(p);
  }
  return peaks;
}
const lcd = (x) => new Uint16Array(x.memory.buffer, x.web_lcd(), 240 * 240).slice();
const differ = (a, b) => a.reduce((n, v, i) => n + (v !== b[i]), 0);
const lit = (x, id) => ((id < 32 ? x.web_leds_lo() >>> id : x.web_leds_hi() >>> (id - 32)) & 1) === 1;

const t0 = performance.now();
const quiet = run(x, 1);
const speed = 1000 / (performance.now() - t0);
check(speed > 1.5, `1 s of BIOFORGE (stopped) in ${(1000 / speed).toFixed(0)} ms: ${speed.toFixed(1)} x real time`);
check(Math.max(...quiet) < 200, `stopped at power-on: silent (its loudest ${Math.max(...quiet)})`);
const grid = lcd(x);
check(grid.filter((c) => c !== 0).length > 50000, 'the screen: the grid drawn at power-on');
check((x.web_state() & 15) === 1, `the first page is SEQ (state ${x.web_state()})`);

x.web_btn(BTN.PLAY);
const t1 = performance.now();
const play = run(x, 4);
const speed2 = 4000 / (performance.now() - t1);
check(speed2 > 1.2, `4 s of the awakening in ${(4000 / speed2).toFixed(0)} ms: ${speed2.toFixed(1)} x real time`);
const loud = play.filter((p) => p > 3000).length / play.length;
check(loud > 0.3, `PLAY: the awakening sounds (${(100 * loud).toFixed(0)} % of it over -20 dBFS)`);
check(Math.max(...play) < 32767, `and does not clip (its loudest ${Math.max(...play)})`);
check(((x.web_state() >> 4) & 1) === 1 && lit(x, BTN.PLAY), "playing: the state says so, PLAY's light is lit");
const g1 = lcd(x);
run(x, 0.3);
check(differ(g1, lcd(x)) > 50, `the playhead moves (${differ(g1, lcd(x))} pixels changed in 0.3 s)`);

x.web_btn(BTN.HOME);
run(x, 0.3);
check((x.web_state() & 15) === 0 && differ(grid, lcd(x)) > 10000, `HOME: MIX (${differ(grid, lcd(x))} pixels changed)`);

x.web_btn(BTN.PLAY);                             // stop: the sound dies away (the delay and the reverb's tails)
run(x, 6);
const after = run(x, 1);
check(Math.max(...after) < 400, `stopped: silent again within 6 s (its loudest ${Math.max(...after)})`);
x.web_keys(1 << 3);                              // the kit's HEART, its middle key
const hit = run(x, 0.3);
x.web_keys(0);
check(Math.max(...hit) > 3000, `a key: the kit's HEART struck (its loudest ${Math.max(...hit)})`);
run(x, 2);

x.web_btn(BTN.GLO);                              // an edit (BPM up), then SAVE: the store moves
x.web_enc(2, 3);
run(x, 0.5);
const rev0 = x.web_store_rev();
x.web_btn(BTN.SAVE);
run(x, 1.5);
check(x.web_store_rev() > rev0, `SAVE: the scene and the settings reach the store (rev ${rev0} -> ${x.web_store_rev()})`);
const size = x.web_store_size(), store = [];
for (let i = 0; i < x.web_store_n(); i++) store.push([i, new Uint8Array(x.memory.buffer, x.web_store(i), size).slice()]);
const y = await boot(store);
run(y, 0.3);
y.web_btn(BTN.GLO);
run(y, 0.5);
const a = lcd(y);
y.web_enc(2, -3);
run(y, 0.5);
check(differ(a, lcd(y)) > 0, 'restarted from the store: the scene is back (its tempo turned back down redraws GLO)');
console.log(fail ? 'app_test: FAILED' : 'app_test: all passed');
process.exit(fail);
