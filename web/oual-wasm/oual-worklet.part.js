// ---- the AudioWorklet part (appended to oual-core.mjs, with the `export` keywords removed, to make oual-worklet.js) ----
// Messages in:  {t:'set', v, p, value} {t:'reverb', value} {t:'master', value} {t:'env', v, points}
//               {t:'pack', id, bytes} {t:'o3', id, v, bytes} {t:'reset'}
// Messages out: {t:'ready'} {t:'done', id, n} {t:'error', id, message}
class OualProcessor extends AudioWorkletProcessor {
  constructor(opts) {
    super();
    const o = (opts && opts.processorOptions) || {};
    this.core = new OualCore(o.module, { rate: sampleRate, seed: o.seed || 1 });
    this.port.onmessage = (e) => this.onMessage(e.data);
    this.port.postMessage({ t: 'ready' });
  }
  onMessage(m) {
    const c = this.core;
    try {
      switch (m.t) {
        case 'set': c.set(m.v, m.p, m.value); break;
        case 'reverb': c.setReverb(m.value); break;
        case 'master': c.setMaster(m.value); break;
        case 'env': c.setEnvelope(m.v, m.points); break;
        case 'pack': this.port.postMessage({ t: 'done', id: m.id, n: c.loadPack(new Uint8Array(m.bytes)) }); break;
        case 'o3': c.loadO3(m.v, new Uint8Array(m.bytes)); this.port.postMessage({ t: 'done', id: m.id, n: 1 }); break;
        case 'reset': c.init(); break;
      }
    } catch (err) { this.port.postMessage({ t: 'error', id: m.id, message: String(err && err.message || err) }); }
  }
  process(inputs, outputs) {
    const out = outputs[0]; if (!out || !out[0]) return true;
    this.core.renderFloat(out[0], out[1] || out[0], out[0].length);
    return true;
  }
}
registerProcessor('oual-engine', OualProcessor);
