# oual in the browser: the FM-1 engine as WebAssembly

The same C engine the FM-1 runs (`core/oual_engine.c`, integer only) compiled to WebAssembly, with the real 3-bit codec decoder, the reverb, and the encoder, behind a small JavaScript API and an AudioWorklet. It is a **browser preview of the firmware engine**: the device build has no reverb and plays 2 voices; the browser can play all 10.

Built in CI (`.github/workflows/wasm.yml`) with plain clang and wasm-ld (no Emscripten, no libc). No device, no flashing.

## What you get (the CI artifact `oual-wasm-<version>`)
| File | What |
|---|---|
| `oual.wasm` | the engine. Imports nothing. |
| `oual.mjs` | page-side API (`Oual`), an ES module |
| `oual-core.mjs` | the synchronous core (`OualCore`) and the parameter table (`PARAMS`) |
| `oual-worklet.js` | the AudioWorklet processor `oual-engine` (a classic script: core + processor) |
| `manifest.json`, `SHA256SUMS` | version, commit, exports, sha256 of every file |

The tarball `oual-wasm-<version>.tar.gz` and its `.sha256` are in the same artifact. Keep the four files together in one folder (the page API finds `oual.wasm` and `oual-worklet.js` next to `oual.mjs`).

## Using it
```js
import { Oual, PARAMS } from './oual.mjs';
const ctx = new AudioContext();                 // create or resume it from a click: no sound before the user presses Play
const oual = await Oual.create(ctx);            // loads oual.wasm and oual-worklet.js
oual.connect(ctx.destination);
await oual.loadPack(await (await fetch('pack.lfp')).arrayBuffer());   // version 2 pack: sound n plays on voice n, with the pack's loop window
oual.setOn(0, true);
oual.set(0, 'transpose', 300);                  // cents; keys or numbers, see PARAMS
oual.setReverb(1000);                           // 0 = off (costs nothing) .. 1000 = as in the Max patch
await oual.loadFile(3, file);                   // your own audio file: decoded and encoded with the device's codec here, never uploaded
```
Parameters (engine units): `on` 0/1, `transpose` -1200..1200 cents, `randPos`, `crush`, `srr`, `drive`, `am`, `pan`, `levelY` 0..1000, `start`/`end` 0..65536 (fraction of the sound), `envRate` -1024..1024. `PARAMS` has the ranges, defaults and notes for building a UI. `setMaster(0..1)`, `setEnvelope(voice, 64 points)`.

Sound loading: `loadPack` (a `.lfp` from `tools/mkpack3`), `loadO3(voice, bytes)` (a `.o3`), `loadAudio(voice, audioBuffer)` / `loadFile(voice, file)` (first channel, resampled to 22.05 kHz, encoded in the page; a 20 s sound encodes in about a second).

Without the page API, `OualCore` runs the same engine in Node or on any thread: `new OualCore(await WebAssembly.compile(bytes), { rate: 44100 })`, then `render(frames)` (Int16 stereo) or `renderFloat(left, right)`.

## C ABI (all `oual_w_*`)
`init(rate, seed)`, `alloc(n)`, `load_pack(ptr, len)`, `load_o3(voice, ptr, len)`, `set_sound(voice, ptr, frames, rate)`, `set(voice, param, value)`, `set_reverb`, `set_master`, `env_buf` + `set_envelope`, `render(frames)` + `out()`, `encode(pcm, frames, rate)` + `encoded_len`, `stat_decoded`, `stat_wraps`. `init` releases everything allocated before; any call that allocates can grow the memory, so JS views must be re-made (the core does this).

## How it is tested (`test/run_wasm_tests.sh`)
- **Same output as the native engine, sample for sample.** `test/wasm/patch_*.txt` (the format of `tools/oual_host`) are rendered by the native host tool and by the WebAssembly module with the same events; the 16-bit outputs must be identical. Patch A has three voices with reverb and control changes; patch B has all ten voices at extreme settings.
- The encoder: the bytes of a sound encoded in the module equal the native `enc3` output.
- Packs: ten sounds load, garbage is refused, a pack voice plays exactly like the same `.o3` with the pack's loop window.
- Memory: a 20 s sound grows the memory and still plays; `init` releases it.
- The worklet file: run against a stub of the worklet API through its message handlers; it renders the same samples as the core.

## Limits (be honest in the page copy)
- Reverb delay lines are sized for 48 kHz: at a 96 kHz output they are shorter (the reverb sounds different).
- The engine plays sounds stored at their own rate (22.05 kHz from the device pack): that is the device's quality, not the browser's.
- Safari / iPhone: the module is plain WebAssembly (no threads, no SIMD) and the worklet is a classic script, but this has not been run on Safari yet.
- Encoding a long sound blocks the page's main thread for as long as it takes; do it before play, or show a progress state.

## Testing in a browser without making noise
Automated or by-hand browser checks must never open a real audio output. Use `OfflineAudioContext` (it renders to a buffer and plays nothing), do not connect to a live `AudioContext.destination`, and close the page, the browser tab and any local server when the check ends. The check used for this build (an `OfflineAudioContext` with the worklet, a pack and a tone) is the pattern: create `new OfflineAudioContext(2, frames, 44100)`, `await Oual.create(ctx)`, `oual.connect(ctx.destination)`, load, set voices on, `await ctx.startRendering()`, read the peak. A page that must play live sound needs a visible Stop control and starts only on a click.
