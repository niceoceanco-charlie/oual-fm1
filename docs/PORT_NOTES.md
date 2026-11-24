# Port notes: what was read from the Max patch, what is interpreted, what is not done

Source: github.com/pdmeyer/philip-meyer-max-tutorials, patchers/opn-oval/oual_qroccss (MIT, (c) 2025 Philip Meyer;
"Original: anonymous, 2007; Update 1: Philip Meyer, 2024"). Read: `oual_playback_poly.maxpat` (the voice, whole signal flow),
the parameter ranges in `oual_voice_ui.maxpat`, and the saved presets in `_DeletedItems/data/oual_qroccss_2024-10-30.json`.
The main patch `oual_qroccss.maxpat` was read for the reverb only (below); the rest of its routing and the preset system are not ported.
No Max code was copied; the DSP is written in C from the signal flow.

## The voice, as read (per voice, in order)
groove~ loop over one sound -> loop-edge window `clip(sin(pi*sync), 0.1)*10` -> overdrive~ (drive = 1 + 10*Drive) ->
degrade~ (sample-rate ratio `(1-SRR)^2`, bits `int((1-Crush)*16)+1`) -> AM -> gate (10 ms) -> level `(1-y)^2` (10 ms) ->
envelope -> equal-power pan (`sqrt(x)`, `sqrt(1-x)`, 10 ms). Speed = `2^(Transpose/12)`. The AM subpatch is a crossfade between
1 and `0.5*(1+cos(2*pi*12000 t))` by the AM amount.
**Rand Pos** (subpatch "grnish"): on every loop wrap the window is re-drawn around the stored one, with L = end - start:
`start' = start + L*rp*(r1 - 0.5)`, `end' = start' + L + L*rp*(r2 - 0.5)`, both clipped to the sound. Start and end are fractions of the sound
in the presets (0.386, 0.400 etc.), so many loops are only 10 to 50 ms long.
Parameter units in the presets: Transpose -12..12, Rand Pos / Crush / AM / Drive / SRR 0..100 (percent, divided by 100 in the patch),
envelope_rate -1..1.

## Interpretations (check these against Max by ear)
1. **groove~ sync is read as loop-relative** (0..1 over the loop). The patch needs that for the wrap detection that re-draws Rand Pos.
2. **overdrive~'s curve is not documented.** The engine uses: linear to 0.5, quadratic knee to full scale at 1.5, bypassed at Drive 0.
3. **degrade~**: sample-and-hold at the ratio, then rounding to the bit depth (bits 16 and up do nothing on 16-bit data).
4. **Envelope**: the patch has a drawn envelope (a `function` object) and `shape~`. Their exact behaviour was not decoded. The engine
   has a 64-point table per voice (default flat = no envelope) played once per `1/|envelope_rate|` loops, backwards when negative,
   and stopped at envelope_rate 0. Setting the table from a drawn curve is up to the UI.
5. **Smoothing**: a one-pole smoother of about 6 ms instead of line~'s 10 ms ramp; the pan is smoothed after the square root instead of before.
6. A voice that is switched off fades out and then costs nothing (the patch mutes it in poly~).
7. Interpolation is cubic Hermite with a 12-bit fraction (the patch's groove~ interpolation is not documented).

## What the codec choice means
3-bit LMS-ADPCM at 22.05 kHz: 9.4 KB/s, about 57 s in the 536,576-byte user flash. SNR on the six cepia_modular sounds: 32 to 41 dB.
Format and method follow `ferluht/fm1-lofi-sampler` (docs/CODEC.md); this is a separate implementation widened to 3 bits.

## Test results (host, Nov 23)
- `test/test_engine.c`: 24 checks, 0 failed, also clean under UBSan. Covers codec random access, transpose accuracy (within 1%), loop wraps without
  clicks, Crush bit counts, SRR run lengths, AM sidebands, Drive harmonics, Rand Pos ranges and determinism, envelope cycles, gate fade, and a 10-voice
  stress run with every control at its extreme (no overflow).
- `test/test_ref.c`: the integer engine against a floating-point model of the same chain, 30 random settings on the real sounds: worst 52.9 dB,
  the worst single sample off by 40 LSB (the 16-bit output rounds). This checks the integer maths, **not** parity with Max.
- `test/bench.c`: 10 voices with every effect on take about 3.7 ms of host CPU per second of audio (8 ns per voice-sample). This is **not** a device number.
- AddressSanitizer hangs at start-up on the development Mac, so only UBSan was used.

## Not done, and risks for the device
- **No device build, nothing flashed.** CPU per voice on the FM-1 is unmeasured; so is RAM headroom. The decoded-block cache is 3 x 1280 bytes per voice
  (38 KB for ten). Decoding at 1x costs 22,050 samples/s per voice.
- **Program flash**: the base has about 6.5 KB free. This engine is about 11 KB of host code at -Os plus about 7 KB of tables (the 1,200-entry pitch table
  could shrink to 100 entries plus interpolation). It has to replace the base's own sampler code, not sit beside it.
- The decoder keeps the base's 64-bit accumulator (4 taps); the base's CPU notes warn about 64-bit multiplies on this chip. The per-sample engine path
  avoids 64-bit multiplies except in the decoder; wrap-time and control-time code uses them.
- Not built: reverb, the 16-step sequencer, key/knob mapping, presets on the device, screen UI, the sample-pack format and uploader, the web editor.

## The reverb (read from the main patch, Nov 23 night)
The ten voices' stereo output goes through two identical `rvb` subpatches (left and right). Each: dry signal passes straight to the output; a copy goes
`*~ 0.14` -> `reson~ 10 1200 8` -> `allpass~ 100 65 0.7` -> `allpass~ 100 67 0.7` -> `allpass~ 100 85 0.7` -> `*~ 0.5`, and is added. There is no control for it.
Implemented in the engine as `oual_set_reverb` (0..1000 permille, default 1000 in the firmware = as in the patch; 0 costs nothing). Interpretations to check against Max:
`reson~ 10 1200 8` read as a two-pole resonator, peak gain 10 at 1200 Hz, Q 8 (bandwidth 150 Hz), so the wet path peaks at about 0.7 at 1.2 kHz; `allpass~ max delay gain` read as
H = (c + z^-D)/(1 + c z^-D) with c = 0.7. Coefficients are for 44.1 kHz. The wet path runs at 16 bits (the mix is shifted down 4 bits), so its noise floor is about -72 dBFS (the tail after
2 s of silence is 8 of 2^19). Memory: 41.7 KB of delay lines. Tests: integer chain against a floating-point model 51.5 dB; tail dies out; bounded on loud input.
A/B suggestion for Charlie: record the Max patch's output of one voice playing a short click or tone, with the same settings, and compare the tail.

## CPU readout
GLO page (oual build): K1 master, K2 reverb, tile 3 = CPU PEAK (the longest time the audio interrupt took for one 128-frame block, as a percent of the time a block lasts: the firmware already
kept `peak_ticks`; `fm1_audio_block_ticks()` and `fm1_audio_stats_reset_peak()` were added), tile 4 = overruns. K3 or K4 turns the peak back to zero. Polled twice a second on that page only.
On the host the harness fakes 25% and 2 overruns. The real numbers come from the first build on the device.
