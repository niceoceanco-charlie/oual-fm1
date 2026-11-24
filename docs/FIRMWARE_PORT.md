# Firmware port (host-verified, NOT built for the device, nothing flashed)

The engine is integrated into a fork of `ferluht/fm1-lofi-sampler` in `fw/` (a separate git repo, branch `oual`, gitignored here). `tools/sync_fw.sh`
copies the engine from `core/` into `fw/firmware/app/`.

## What was done
- **Pack version 2** (`fw/firmware/app/lofi_pack.h`): the existing pack image with version 2 = 3-bit sounds (272 bytes per 640 samples), source rate at byte 32.
  Slot n is voice n, played by key n; the slot's loop start/end are the voice's window. The upload protocol carries the pack unchanged. `tools/mkpack3.c` builds one from `.o3` files.
- **Engine in the audio path** (`lofi_engine.c`): `le_bind` binds the oual engine for a version 2 pack; `le_render` renders the ten voices and converts to Q23.
  The engine's `.c` is included as `oual_engine.inc` so its code and tables get `FM1_AUDIO_CODE` / `FM1_AUDIO_DATA` (audio runs while flash is being written).
  Settings travel control task -> audio side through the base's existing event ring (`EV_OUAL`). Version 1 packs behave exactly as before.
- **Panel** (`oual_ui.inc`, hooks in `lofi_ui.c`): keys 1..10 switch voices (LED lit = on); SELECT picks the voice; HOME = state, transpose, rand pos, level;
  FX = crush, SRR, drive, AM; EDIT = start, end, pan, envelope rate (with the real waveform and window markers); GLO = master; SEL = state; SAVE stores
  all ten voices' settings (`LFST` version 3) in the same two flash sectors as before.
- **Checks** (`fw/firmware/hosttest/oual_host.c`, the base's own host harness running the real UI code): 18 checks pass. The pack uploads through the firmware glue and
  binds; key 1 switches voice 1; **the firmware's output equals the standalone engine's sample for sample**; every page edits what it says; ten keys toggle ten voices; nine
  voices stay in range; voices off fade to silence; SAVE and a power cycle restore all settings; screen work per tick stays within the base's limit (38 calls, 11,520 pixels).
  The base's original 25+ checks (`lofi_host`) still pass.
- Screens rendered by the harness: `out/screens/oual_*.png`.

## Host builds and tests
    cd fw/firmware
    gcc -O1 -g -DFM1_HOST_TEST -Wno-misleading-indentation -Wno-unused-result -Wno-unused-function -Isdk/include -Iapp -Iapp/synth -Iapp/board -Ihosttest \
        -o hosttest/oual_host hosttest/oual_host.c app/lofi_engine.c -lm
    cd hosttest && ./oual_host ../../../out/oual_demo.lfp        # make the pack with tools/mkpack3 (see tools/mkpack3.c)

## Repo layout and the cloud build (updated Nov 23, evening)
The firmware now lives in this repo (`firmware/`, a copy of the fork; `fw/` is only a local scratch clone). `tools/sync_fw.sh` copies the engine from `core/` into `firmware/app/`.
Device images are built on GitHub's Linux machines, not on a Mac: `ci/build.yml` runs the host tests and the firmware harness, downloads JieLi's Linux toolchain (pinned by SHA-256),
builds the upstream sampler (for a size baseline) and the oual firmware with `build/build_fw.py` (the base's wine build script changed to run the Linux tools natively), and uploads the package
and flash page as an artifact. **The workflow is parked in `ci/`** because the GitHub token on the Mac may not push workflow files; moving it to `.github/workflows/` needs the `workflow` scope.
Nothing built there is to be used on a device until Charlie says "flash".

## Size: the oual build is smaller than the sampler it replaces (host proxy)
`-DOUAL_ONLY` compiles out the sampler's voices, reverb/delay/crush/filter, sequencer, arpeggiator and their pictures. Host arm64 -Os, engine plus panel code:
upstream sampler 48.3 KB of code and constants, RAM 141 KB; oual full (both modes) 64.6 KB, 188 KB; **oual only 37.0 KB, 106 KB**. So it should fit the roughly 6.5 KB headroom with room to spare
(11 KB under the sampler), but PI32v2 code density is unknown until the real build prints `app image N bytes (limit 453921)`. All 18 harness checks pass with and without the switch;
the sampler harness still passes. The 4.8 KB pitch table can still shrink.

## Still to do
The reverb is in (see PORT_NOTES.md) and the GLO page shows the CPU load. The web-editor uploader for version 2 packs (the pack is built on the host), voices that share one stored sound, the envelope shape editor, the measurement run on the device, and the first device build in CI.

## The measurement build (needs Charlie's explicit "flash")
`app/board/audio.c` already keeps `statistics.peak_ticks` against `block_ticks`: the peak time used by one 128-frame render. A measurement build would show that figure with 1, 5 and 10 voices on,
which gives the real CPU per voice. Nothing is flashed without Charlie's "flash"; recovery stays `fm1_rescue.py` plus M-VAVE's stock V15 (the unit reports `FM-1_015`).
