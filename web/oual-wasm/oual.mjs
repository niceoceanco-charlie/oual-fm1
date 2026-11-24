// oual.mjs: the page-side API for the oual engine in the browser (the FM-1's C engine as WebAssembly, played through an AudioWorklet).
// GPL-3.0-only. Usage:
//   import { Oual, PARAMS } from './oual.mjs';
//   const ctx = new AudioContext();                         // create it from a user gesture (press Play); nothing makes sound before that
//   const oual = await Oual.create(ctx);                    // fetches oual.wasm and oual-worklet.js next to this file
//   oual.connect(ctx.destination);
//   await oual.loadPack(await (await fetch('pack.lfp')).arrayBuffer());   // a version 2 pack: sound n on voice n
//   oual.setOn(0, true); oual.set(0, 'transpose', 300); oual.setReverb(1000);
//   await oual.loadAudio(2, audioBuffer);                   // your own sound: encoded here with the device's 3-bit codec, nothing is uploaded
import { OualCore, PARAMS, PARAM, VOICES, resampleTo22k } from './oual-core.mjs';
export { PARAMS, PARAM, VOICES, resampleTo22k };

export class Oual {
  static async create(ctx, { wasmUrl = new URL('./oual.wasm', import.meta.url), workletUrl = new URL('./oual-worklet.js', import.meta.url), seed = 1 } = {}) {
    const module = await WebAssembly.compile(await (await fetch(wasmUrl)).arrayBuffer());
    await ctx.audioWorklet.addModule(workletUrl);
    const node = new AudioWorkletNode(ctx, 'oual-engine', { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2], processorOptions: { module, seed } });
    const o = new Oual(ctx, node, new OualCore(module, { rate: ctx.sampleRate, seed }));
    await new Promise((res, rej) => { node.port.onmessage = (e) => { if (e.data.t === 'ready') res(); else o._reply(e.data); }; node.onprocessorerror = () => rej(new Error('oual worklet failed')); });
    node.port.onmessage = (e) => o._reply(e.data);
    return o;
  }
  constructor(ctx, node, encoder) { this.ctx = ctx; this.node = node; this.encoder = encoder; this._id = 0; this._wait = new Map(); }
  connect(dest) { this.node.connect(dest); return this; }
  disconnect() { this.node.disconnect(); }
  _reply(m) { const w = this._wait.get(m.id); if (!w) return; this._wait.delete(m.id); if (m.t === 'error') w.rej(new Error(m.message)); else w.res(m.n); }
  _ask(msg, transfer) { const id = ++this._id; return new Promise((res, rej) => { this._wait.set(id, { res, rej }); this.node.port.postMessage({ ...msg, id }, transfer || []); }); }
  // Voice parameters (engine units: see PARAMS). param is a key ('crush') or a number.
  set(voice, param, value) { this.node.port.postMessage({ t: 'set', v: voice, p: typeof param === 'string' ? PARAM[param] : param, value: Math.round(value) }); }
  setOn(voice, on) { this.set(voice, 'on', on ? 1 : 0); }
  setReverb(permille) { this.node.port.postMessage({ t: 'reverb', value: permille }); }       // 0 (off, costs nothing) .. 1000
  setMaster(gain) { this.node.port.postMessage({ t: 'master', value: gain }); }               // 0..1, default 0.8
  setEnvelope(voice, points) { this.node.port.postMessage({ t: 'env', v: voice, points }); }
  // A version 2 pack (ArrayBuffer or Uint8Array). Resolves with the number of sounds.
  loadPack(bytes) { const b = new Uint8Array(bytes).slice(); return this._ask({ t: 'pack', bytes: b.buffer }, [b.buffer]); }
  loadO3(voice, bytes) { const b = new Uint8Array(bytes).slice(0); return this._ask({ t: 'o3', v: voice, bytes: b.buffer }, [b.buffer]); }
  // Your own sound: an AudioBuffer (first channel, resampled to 22.05 kHz) is encoded with the device's codec on this thread, then sent.
  encodeAudio(audioBuffer) { return this.encoder.encodeFloat(audioBuffer.getChannelData(0), audioBuffer.sampleRate); }
  loadAudio(voice, audioBuffer) { return this.loadO3(voice, this.encodeAudio(audioBuffer)); }
  async loadFile(voice, file) { return this.loadAudio(voice, await this.ctx.decodeAudioData(await file.arrayBuffer())); }
}
