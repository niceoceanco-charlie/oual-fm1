# oual-fm1 (work in progress)

A port of Philip Meyer's `oual_qroccss` Max patch (ten looping sample voices with Crush, SRR, Drive, AM, Rand Pos, envelope) to the M-VAVE FM-1,
to run on the device with no computer in the loop. Hear the sounds and try the blind codec test on the Lab page: **https://niceocean.studio/lab/oual**

**Status.** v0.2 runs on one FM-1 (the author's): up to 2 voices reliably, a third can overrun, and restoring the stock V15 firmware was tested. The 10-voice engine and a 3-bit codec are also tested on a computer. The device build has no reverb. Sound is mono and heavily compressed (storage is small), so it will not match the original patch. It is not yet meant for others to flash; if you do, be ready to recover.

**Risk.** Custom firmware is at your own risk. Back up your stock firmware first. Recovery routes are in `docs/INSTALL_OUAL.md` and `docs/UPDATE_MODE_RESEARCH.md`. This chip has no proven safe recovery for every failure.

## Give feedback
- Sound or codec preference: [Sound feedback](../../issues/new?template=sound-feedback.yml)
- Tried it on your FM-1: [Tester report](../../issues/new?template=tester-report.yml)
- [Bug](../../issues/new?template=bug.yml) or [Idea](../../issues/new?template=idea.yml)
- Share what you made: Discussions, "Show and tell"

## What is here
- `core/oual_engine.[ch]`: the engine, integer only (no float or libm). `core/oual_codec3.h`: the 3-bit decoder. `core/oual_tables.h`: generated tables.
- `tools/enc3.c`: encode a mono 16-bit raw file to `.o3`. `tools/oual_host.c`: render a patch file to WAV. `examples/`: two patch files.
- `test/run_tests.sh`: the tests. `docs/PORT_NOTES.md`: what was read from the patch, what is interpreted, what is missing.

Quick start (from this folder):

    cc -O2 -o enc3 tools/enc3.c -lm && ./enc3 sound_22050_mono.raw out/sound.o3
    cc -O2 -o oual_host tools/oual_host.c core/oual_engine.c -lm && ./oual_host examples/demo_10voice.txt out/demo.wav 40
    ./test/run_tests.sh

## Credits and licence
- Port of **oual_qroccss** by Philip Meyer (MIT, (c) 2025 Philip Meyer), whose credits read "Original: anonymous, 2007; Update 1: Philip Meyer, 2024". The MIT text is in `THIRD_PARTY.md`. Inspired by Markus Popp's ovalprocess (2000).
- Base firmware and codec design: **ferluht/fm1-lofi-sampler** (GPL-3.0), itself derived from the SEA codec by Dani Biro (MIT) and from M-VAVE's open firmware `m-vave/fm1_open` (GPL-3.0 with an FM-1 SDK linking exception).
- M-VAVE's boot/update files and recovery image (`firmware/sdk/`) are Apache-2.0; the closed engine library is redistributed under M-VAVE's FM-1-only grant (see `firmware/THIRD_PARTY.md`).
- Sounds: **cepia modular**.
- This code: GPL-3.0-only (`LICENSE`). Not affiliated with M-VAVE or Philip Meyer.
