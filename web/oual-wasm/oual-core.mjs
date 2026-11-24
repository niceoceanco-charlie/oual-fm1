// oual-core: a synchronous wrapper around one instance of oual.wasm (the FM-1's C engine, built for the browser).
// Runs in Node, in a page (to encode sounds) and, copied into oual-worklet.js, inside an AudioWorklet. GPL-3.0-only.
// Units are the engine's own: cents, permille, 0..65536 for the loop window (see PARAMS).

// id = the engine's parameter number (core/oual_engine.h). min/max/def are in engine units.
export const PARAMS = [
  { id: 0,  key: 'on',        label: 'On',          min: 0,     max: 1,     def: 0,     unit: 'switch', note: '0 or 1' },
  { id: 1,  key: 'transpose', label: 'Transpose',   min: -1200, max: 1200,  def: 0,     unit: 'cents',  note: '+-12 semitones' },
  { id: 2,  key: 'randPos',   label: 'Rand Pos',    min: 0,     max: 1000,  def: 0,     unit: 'permille', note: 'random loop position per pass' },
  { id: 3,  key: 'crush',     label: 'Crush',       min: 0,     max: 1000,  def: 0,     unit: 'permille', note: 'bits = (1000-v)*16/1000 + 1' },
  { id: 4,  key: 'srr',       label: 'SRR',         min: 0,     max: 1000,  def: 0,     unit: 'permille', note: 'sample-rate reduction' },
  { id: 5,  key: 'drive',     label: 'Drive',       min: 0,     max: 1000,  def: 0,     unit: 'permille', note: 'drive factor = 1 + 10*v/1000' },
  { id: 6,  key: 'am',        label: 'AM',          min: 0,     max: 1000,  def: 0,     unit: 'permille', note: 'mix of the 12 kHz amplitude modulation' },
  { id: 7,  key: 'start',     label: 'Start',       min: 0,     max: 65536, def: 0,     unit: 'q16',    note: 'loop start, fraction of the sound' },
  { id: 8,  key: 'end',       label: 'End',         min: 0,     max: 65536, def: 65536, unit: 'q16',    note: 'loop end, fraction of the sound' },
  { id: 9,  key: 'envRate',   label: 'Envelope',    min: -1024, max: 1024,  def: 1024,  unit: 'q10',    note: 'envelope cycles per loop = 1/|v|; negative runs backwards' },
  { id: 10, key: 'pan',       label: 'Pan',         min: 0,     max: 1000,  def: 500,   unit: 'permille', note: '0 left .. 1000 right, equal power' },
  { id: 11, key: 'levelY',    label: 'Level (Y)',   min: 0,     max: 1000,  def: 500,   unit: 'permille', note: 'level = (1 - y)^2, so 0 is loudest' },
];
export const PARAM = Object.fromEntries(PARAMS.map(p => [p.key, p.id]));
export const VOICES = 10;
export const ENV_POINTS = 64;
export const SOURCE_RATE = 22050;       // the rate the device's sounds are stored at

// Mono Float32 at srcRate -> Int16 at 22050 Hz (area average when going down, linear interpolation when going up).
export function resampleTo22k(x, srcRate) {
  const n = Math.max(1, Math.floor(x.length * SOURCE_RATE / srcRate)), out = new Int16Array(n), r = srcRate / SOURCE_RATE;
  for (let i = 0; i < n; i++) {
    let s;
    if (r > 1) { const a = Math.floor(i * r), b = Math.min(x.length, Math.max(a + 1, Math.floor((i + 1) * r))); let t = 0; for (let k = a; k < b; k++) t += x[k]; s = t / (b - a); }
    else { const p = i * r, k = Math.floor(p), f = p - k; s = x[k] * (1 - f) + (x[Math.min(k + 1, x.length - 1)]) * f; }
    out[i] = Math.max(-32768, Math.min(32767, Math.round(s * 32767)));
  }
  return out;
}

export class OualCore {
  // module: a WebAssembly.Module (compile oual.wasm once and share it). rate: the output sample rate.
  constructor(module, { rate = 44100, seed = 1 } = {}) {
    this.x = new WebAssembly.Instance(module, {}).exports;
    if (this.x.oual_w_api() !== 1) throw new Error('oual.wasm has a different API version');
    this.rate = rate; this.seed = seed; this._buf = null; this.init(rate, seed);
  }
  static async compile(bytesOrResponse) { return WebAssembly.compile(bytesOrResponse); }
  get mem() { const b = this.x.memory.buffer; if (b !== this._buf) { this._buf = b; this._u8 = new Uint8Array(b); } return this._u8; }
  // Start over: every voice off, no sounds, reverb 0, master 0.8. Memory handed out by alloc() is gone.
  init(rate = this.rate, seed = this.seed) { if (this.x.oual_w_init(rate, seed) < 0) throw new Error('bad sample rate'); this.rate = rate; }
  alloc(n) { const p = this.x.oual_w_alloc(n); if (!p) throw new Error('out of memory'); return p; }
  _put(bytes) { const p = this.alloc(bytes.length); this.mem.set(bytes, p); return p; }
  // A version 2 pack (.lfp): sound n plays on voice n with the pack's loop window. Returns the number of sounds.
  loadPack(bytes) { const p = this._put(bytes), n = this.x.oual_w_load_pack(p, bytes.length); if (n < 0) throw new Error('not a version 2 oual pack (' + n + ')'); return n; }
  // One .o3 file (as made by encode()) onto a voice.
  loadO3(voice, bytes) { const p = this._put(bytes), r = this.x.oual_w_load_o3(voice, p, bytes.length); if (r < 0) throw new Error('bad .o3 sound (' + r + ')'); }
  set(voice, param, value) { this.x.oual_w_set(voice, typeof param === 'string' ? PARAM[param] : param, Math.round(value)); }
  setReverb(permille) { this.x.oual_w_set_reverb(Math.round(permille)); }
  setMaster(gain) { this.x.oual_w_set_master(Math.round(gain * 32768)); }       // 0..1 (the engine's own default is 0.8)
  // 64 points, 0..1 (Float32Array) or -32768..32767 (Int16Array); the envelope is looped at the Envelope rate.
  setEnvelope(voice, points) {
    const e = new Int16Array(this.x.memory.buffer, this.x.oual_w_env_buf(), ENV_POINTS), f = points instanceof Int16Array;
    for (let i = 0; i < ENV_POINTS; i++) e[i] = f ? points[i] : Math.max(-32768, Math.min(32767, Math.round(points[i] * 32767)));
    this.x.oual_w_set_envelope(voice);
  }
  // Render `frames` (1..4096) frames; returns interleaved stereo Int16 (a view, valid until the next call).
  render(frames) { if (this.x.oual_w_render(frames) < 0) throw new Error('frames must be 1..4096'); return new Int16Array(this.x.memory.buffer, this.x.oual_w_out(), frames * 2); }
  // The same, as floats into two arrays (an AudioWorklet's output channels).
  renderFloat(left, right, frames = left.length) {
    for (let done = 0; done < frames;) {
      const n = Math.min(4096, frames - done), s = this.render(n);
      for (let i = 0; i < n; i++) { left[done + i] = s[2 * i] / 32768; right[done + i] = s[2 * i + 1] / 32768; }
      done += n; }
  }
  stats() { return { decoded: this.x.oual_w_stat_decoded(), wraps: this.x.oual_w_stat_wraps() }; }
  // Mono Int16 at `rate` (the device uses 22050) -> a .o3 file (Uint8Array copy).
  encode(pcm, rate = SOURCE_RATE) {
    const p = this._put(new Uint8Array(pcm.buffer, pcm.byteOffset, pcm.byteLength)), o = this.x.oual_w_encode(p, pcm.length, rate);
    if (!o) throw new Error('could not encode'); return this.mem.slice(o, o + this.x.oual_w_encoded_len());
  }
  // Mono (or the first channel) Float32 at any rate -> a .o3 file at 22050 Hz.
  encodeFloat(samples, srcRate) { return this.encode(resampleTo22k(samples, srcRate), SOURCE_RATE); }
}
