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

## Why there is no device image yet
- **No JieLi toolchain on this Mac.** The base builds with JieLi's clang 4.0.1 (a Windows installer, run under wine) and its own `tools/build_fw.py`; `fm1-base` offers a Linux toolchain
  that needs Docker or podman. Neither wine nor a container runtime is installed. Installing one is a download and needs Charlie's OK.
- **Size: the port is bigger than the free space.** On the host (arm64, -Os) `lofi_engine.c` grows from 21 KB to 35 KB of code and constants (+14 KB), and RAM by 46 KB (the engine is 41.9 KB:
  ten voices x three decoded blocks). The device image had about 6.5 KB free of 453,921 bytes. PI32v2 code density may differ, but expect the image to overflow unless the sampler's
  reverb, delay, sequencer and arpeggiator are compiled out of an oual build (most of the 21 KB), and the 1,200-entry pitch table (4.8 KB) is shrunk. That is the next change.
- Not yet done: a pack uploader in the web editor for version 2 packs (the pack is built on the host for now), a shared-sound format (voices that use the same sound each store a copy),
  the header still shows the sampler's tempo and STOP, the envelope shape editor, the reverb (main patch not read).

## The measurement build (needs Charlie's explicit "flash")
`app/board/audio.c` already keeps `statistics.peak_ticks` against `block_ticks`: the peak time used by one 128-frame render. A measurement build would show that figure with 1, 5 and 10 voices on,
which gives the real CPU per voice. Nothing is flashed without Charlie's "flash"; recovery stays `fm1_rescue.py` plus M-VAVE's stock V15 (the unit reports `FM-1_015`).
