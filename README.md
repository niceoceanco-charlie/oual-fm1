# oual-fm1 (work in progress, host only)

A port of Philip Meyer's `oual_qroccss` Max patch (ten looping sample voices with Crush, SRR, Drive, AM, Rand Pos, envelope) to the M-VAVE FM-1,
to run on the device with no computer in the loop. **Status: the 10-voice engine and a 3-bit codec run and are tested on the host. Nothing has been
built for or flashed to an FM-1.**

- `core/oual_engine.[ch]`: the engine, integer only (no float or libm). `core/oual_codec3.h`: the 3-bit decoder. `core/oual_tables.h`: generated tables.
- `tools/enc3.c`: encode a mono 16-bit raw file to `.o3`. `tools/oual_host.c`: render a patch file to WAV. `examples/`: two patch files.
- `test/run_tests.sh`: the tests. `docs/PORT_NOTES.md`: what was read from the patch, what is interpreted, what is missing.

Quick start (from this folder):

    cc -O2 -o enc3 tools/enc3.c -lm && ./enc3 sound_22050_mono.raw out/sound.o3
    cc -O2 -o oual_host tools/oual_host.c core/oual_engine.c -lm && ./oual_host examples/demo_10voice.txt out/demo.wav 40
    ./test/run_tests.sh

## Credits and licence
- Port of **oual_qroccss** by Philip Meyer (MIT, (c) 2025 Philip Meyer), whose credits read "Original: anonymous, 2007; Update 1: Philip Meyer, 2024". The MIT text is in `THIRD_PARTY.md`.
- The codec design follows **ferluht/fm1-lofi-sampler** (GPL-3.0), itself derived from the SEA codec by Dani Biro (MIT). The target platform and update system are M-VAVE's FM-1 and the open firmware `m-vave/fm1_open` (GPL-3.0 with an FM-1 SDK linking exception).
- This code: GPL-3.0-only (`LICENSE`). Not affiliated with M-VAVE or Philip Meyer.
