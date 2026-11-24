// The WebAssembly build against the native engine. usage: node test/test_wasm.mjs <wasm dir> <native dir>
//  - <wasm dir>: output of tools/build_wasm.sh.  <native dir>: WAVs rendered by the native tools/oual_host from test/wasm/patch_*.txt (name.wav).
// Run from the repo root after tools/make_test_pack.sh (it makes out/t01.o3 ... and out/ci_test.lfp). GPL-3.0-only.
import { readFileSync, existsSync } from 'node:fs';
import vm from 'node:vm';
import { pathToFileURL } from 'node:url';
const [wdir, ndir] = process.argv.slice(2);
const { OualCore, PARAMS, PARAM } = await import(pathToFileURL(`${wdir}/oual-core.mjs`).href);
const wasmBytes = readFileSync(`${wdir}/oual.wasm`), module = await WebAssembly.compile(wasmBytes);
let bad = 0; const check = (ok, name, extra = '') => { console.log((ok ? 'PASS  ' : 'FAIL  ') + name + (extra ? ' (' + extra + ')' : '')); if (!ok) bad++; };
const same = (a, b) => { if (a.length !== b.length) return false; for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false; return true; };
const lround = (x) => Math.sign(x) * Math.round(Math.abs(x));      // C's lround

// ---- the module itself ----
check(WebAssembly.Module.imports(module).length === 0, 'the module imports nothing (no libc, no host calls)');
const names = WebAssembly.Module.exports(module).map((e) => e.name);
check(['memory', 'oual_w_init', 'oual_w_render', 'oual_w_set', 'oual_w_load_pack', 'oual_w_encode'].every((n) => names.includes(n)), 'the expected exports are there', names.length + ' exports');
{ const c = new OualCore(module); check(c.x.oual_w_voices() === 10 && c.x.oual_w_params() === PARAMS.length && PARAMS.every((p, i) => p.id === i), 'voices and parameter table match the engine', c.x.oual_w_params() + ' params'); }

// ---- the same patch scripts as the native host tool: output must match the native render sample for sample ----
function readWav(path) { const b = readFileSync(path); return new Int16Array(b.buffer.slice(b.byteOffset + 44, b.byteOffset + b.length)); }
function applyKV(c, v, kv) {
  const m = kv.match(/^([^=]+)=(.+)$/); if (!m) return; const k = m[1], x = parseFloat(m[2]);
  const map = { on: ['on', 1], transpose: ['transpose', 100], rand: ['randPos', 1000], crush: ['crush', 1000], srr: ['srr', 1000], drive: ['drive', 1000], am: ['am', 1000],
    start: ['start', 65536], end: ['end', 65536], env: ['envRate', 1024], pan: ['pan', 1000], level_y: ['levelY', 1000] };
  if (map[k]) c.x.oual_w_set(v, PARAM[map[k][0]], k === 'on' ? Math.trunc(x) : lround(x * map[k][1])); }
function runPatch(text, seconds, makeCore) {
  const rate = 44100; let c = makeCore(rate, 1); const evs = [];
  for (const raw of text.split('\n')) {
    const t = raw.trim().split(/\s+/); if (!t[0] || t[0][0] === '#') continue;
    if (t[0] === 'seed') c.init(rate, parseInt(t[1]));
    else if (t[0] === 'master') c.x.oual_w_set_master(lround(parseFloat(t[1]) * 32768));
    else if (t[0] === 'reverb') c.setReverb(parseInt(t[1]));
    else if (t[0] === 'voice') { const v = parseInt(t[1]) - 1; for (const kv of t.slice(2)) { if (kv.startsWith('sound=')) c.loadO3(v, new Uint8Array(readFileSync(kv.slice(6)))); else applyKV(c, v, kv); } }
    else if (t[0] === 'at') { const v = parseInt(t[3]) - 1; for (const kv of t.slice(4)) evs.push({ t: parseFloat(t[1]), v, kv }); } }
  const total = Math.round(seconds * rate), out = new Int16Array(total * 2);
  for (let done = 0; done < total;) { const n = Math.min(128, total - done);
    for (const e of evs) if (e.t * rate >= done && e.t * rate < done + n) applyKV(c, e.v, e.kv);
    out.set(c.render(n), done * 2); done += n; }
  return { out, c };
}
for (const [name, secs] of [['patch_a', 4], ['patch_b', 4]]) {
  const t0 = Date.now(), { out, c } = runPatch(readFileSync(`test/wasm/${name}.txt`, 'utf8'), secs, (r, s) => new OualCore(module, { rate: r, seed: s }));
  const ref = readWav(`${ndir}/${name}.wav`), ok = same(out, ref);
  let peak = 0; for (const s of out) peak = Math.max(peak, Math.abs(s));
  check(ok && peak > 1000, `${name}: ${secs} s of WebAssembly output equals the native engine's, sample for sample`, `${out.length / 2} frames, peak ${peak}, ${c.stats().wraps} wraps, ${Date.now() - t0} ms`);
  if (!ok) { let i = 0; while (i < out.length && out[i] === ref[i]) i++; console.log('      first difference at sample', i, out[i], ref[i]); }
}

// ---- the 3-bit encoder: the same bytes as the native enc3 tool ----
{ const raw = readFileSync('out/raw/t01.raw'), pcm = new Int16Array(raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.length)), c = new OualCore(module);
  const t0 = Date.now(), o3 = c.encode(pcm, 22050), ref = readFileSync('out/t01.o3');
  check(same(o3, new Uint8Array(ref)), 'encode(): bytes equal the native enc3 output', `${o3.length} bytes, ${pcm.length} samples, ${Date.now() - t0} ms`);
  // and the decoder side: an encoded sound plays
  c.loadO3(0, o3); c.set(0, 'on', 1); c.set(0, 'levelY', 0); let pk = 0; for (let i = 0; i < 20; i++) for (const s of c.render(128)) pk = Math.max(pk, Math.abs(s));
  check(pk > 1000, 'a sound encoded in the browser decodes and plays (3-bit codec)', 'peak ' + pk); }

// ---- packs ----
{ const pack = new Uint8Array(readFileSync('out/ci_test.lfp')), c = new OualCore(module);
  check(c.loadPack(pack) === 10, 'loadPack(): the ten sounds of a version 2 pack');
  const bad1 = (() => { try { new OualCore(module).loadPack(new Uint8Array(2000)); return false; } catch { return true; } })();
  const bad2 = (() => { try { new OualCore(module).loadO3(0, new Uint8Array(100)); return false; } catch { return true; } })();
  check(bad1 && bad2, 'garbage is refused: a pack of zeros and a .o3 with no header');
  // voice 1 from the pack equals voice 1 loaded as a .o3 with the pack's loop window
  const dv = new DataView(pack.buffer), frames = dv.getUint32(64 + 4, true), en = dv.getUint32(64 + 12, true), st = dv.getUint32(64 + 8, true);
  const d = new OualCore(module); d.loadO3(0, new Uint8Array(readFileSync('out/t01.o3'))); d.set(0, 'start', Math.floor(st * 65536 / frames)); d.set(0, 'end', Math.floor(en * 65536 / frames));
  for (const k of [c, d]) { k.set(0, 'on', 1); k.set(0, 'levelY', 100); }
  let eq = true; for (let i = 0; i < 100; i++) if (!same(c.render(128), d.render(128))) { eq = false; break; }
  check(eq, 'a pack voice plays exactly like the same .o3 with the pack\'s loop window'); }

// ---- memory growth: a 20 s sound grows the memory; views are re-made; rendering still works ----
{ const c = new OualCore(module), n = 22050 * 20, pcm = new Int16Array(n); let s = 1; for (let i = 0; i < n; i++) { s = (s * 1664525 + 1013904223) >>> 0; pcm[i] = Math.round(8000 * Math.sin(i * 0.05) + ((s >>> 20) - 2048) * 2); }
  const before = c.x.memory.buffer.byteLength, t0 = Date.now(), o3 = c.encode(pcm, 22050);
  check(o3.length === 12 + Math.ceil(n / 640) * 272, 'a 20 s sound encodes to the expected size', `${o3.length} bytes, memory ${before >> 10} KB -> ${c.x.memory.buffer.byteLength >> 10} KB, ${Date.now() - t0} ms`);
  c.loadO3(4, o3); c.set(4, 'on', 1); c.set(4, 'levelY', 0); let pk = 0; for (let i = 0; i < 50; i++) for (const v of c.render(128)) pk = Math.max(pk, Math.abs(v)); check(pk > 1000, 'and it plays after the memory grew', 'peak ' + pk);
  c.init(); let sil = true; for (const v of c.render(512)) if (v) sil = false; check(sil, 'init() releases everything and the output is silent'); }

// ---- the AudioWorklet file, run against a stub of the worklet API: same samples as the core ----
{ let proc = null; const posted = [];
  class AudioWorkletProcessor { constructor() { this.port = { postMessage: (m) => posted.push(m), onmessage: null }; } }
  const ctx = vm.createContext({ AudioWorkletProcessor, registerProcessor: (n, cls) => { proc = { n, cls }; }, sampleRate: 44100, Int16Array, Float32Array, Uint8Array, WebAssembly, Math, Number, String, Object, Error });
  vm.runInContext(readFileSync(`${wdir}/oual-worklet.js`, 'utf8'), ctx);
  check(proc && proc.n === 'oual-engine', 'oual-worklet.js registers the processor "oual-engine"');
  const p = new proc.cls({ processorOptions: { module, seed: 1 } }); const pack = readFileSync('out/ci_test.lfp');
  p.port.onmessage({ data: { t: 'pack', id: 1, bytes: pack.buffer.slice(pack.byteOffset, pack.byteOffset + pack.length) } });
  for (const m of [{ t: 'set', v: 0, p: PARAM.on, value: 1 }, { t: 'set', v: 0, p: PARAM.levelY, value: 100 }, { t: 'set', v: 0, p: PARAM.crush, value: 600 }, { t: 'reverb', value: 800 }, { t: 'set', v: 1, p: PARAM.on, value: 1 }]) p.port.onmessage({ data: m });
  const ref = new OualCore(module, { rate: 44100, seed: 1 }); ref.loadPack(new Uint8Array(pack)); ref.set(0, 'on', 1); ref.set(0, 'levelY', 100); ref.set(0, 'crush', 600); ref.setReverb(800); ref.set(1, 'on', 1);
  let eq = true, pk = 0; for (let b = 0; b < 200; b++) { const L = new Float32Array(128), R = new Float32Array(128); p.process([], [[L, R]]); const r = ref.render(128);
    for (let i = 0; i < 128; i++) { if (L[i] !== r[2 * i] / 32768 || R[i] !== r[2 * i + 1] / 32768) eq = false; pk = Math.max(pk, Math.abs(L[i])); } }
  check(eq && pk > 0.02, 'the worklet renders 200 blocks identical to the core, through its message handlers', 'peak ' + pk.toFixed(3));
  check(posted.some((m) => m.t === 'ready') && posted.some((m) => m.t === 'done' && m.id === 1 && m.n === 10), 'it reports ready, and the pack as loaded (10 sounds)');
  p.port.onmessage({ data: { t: 'pack', id: 2, bytes: new ArrayBuffer(64) } }); check(posted.some((m) => m.t === 'error' && m.id === 2), 'a bad pack is reported as an error, the engine keeps running'); }

console.log(bad ? `${bad} FAILED` : 'ALL WASM CHECKS PASSED'); process.exit(bad ? 1 : 0);
