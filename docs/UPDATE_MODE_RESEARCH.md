# Research: a reliable way into the chip's update mode from a running app (Nov 24, research only; nothing was sent to any FM-1)

Question: the oual firmware runs but its "Install original V15" failed twice ("The device stopped requesting data"). What gets the unit into the chip's update mode (UBOOT, USB 4C4A:8057) from the running app?

## Short answer
The **soft key**: SysEx `F0 22 24 35 7D F7`, which USB-MIDI carries as one 8-byte packet `04 F0 22 24 07 35 7D F7`. Plausible on oual, **not proven on it**; a first test writes nothing.

## Sources
1. Mechanism, from disassembly of stock V15 and FM-1_092: github.com/ip2k/lunar-modulator, notes/2026-10-05-softkey-efuse.md. The USB-MIDI receive callback, on a single 8-byte transfer equal to that packet, writes the marker string `usb_update_mode` to RAM 0x01C7FD80 and resets the chip through P33. No flash, VM or update-parameter write. The mask ROM then offers UBOOT and waits; a power cycle exits it and flash boots again. `7F` (the upgrade command, the one our page sends) goes through the same check.
2. "Stock V15 also enters UBOOT on the USB-MIDI packet `04 F0 22 24 07 35 7D F7`": github.com/kurogedelic/FM-1-transporter, docs/PROTOCOL.md; src/pio_host.c ("xx = 7D calls the mask ROM UBOOT1.00").
3. A ready tool: github.com/Quixotic7/MvaveFM1Unbricker, `fm1_unbrick.py softkey`: for a unit that still runs, asks its identity over USB-MIDI, then sends the key; writes nothing; works on macOS (needs python-rtmidi). It says the key drops "stock V15 and the Felucca family" into UBOOT. oual is neither: it is built on M-VAVE's Official Open SDK.
4. Apps that implement the same key themselves: Felucca firmware/src/main.c, felucca-bsp chip/usb.c, fm1-x0x firmware/src/usb.c; update command in felucca-bsp chip/ota.c.

## Evidence that the handler is still in oual (byte scan, mine)
`firmware/sdk/lib/libfm1_engine.a` contains the SysEx `F0 22 24 35 7D F7` once. The oual package (`FM1_OFFICIAL_OPEN.fwsc`, CI run 45318505778) contains the 8-byte USB packet once, exactly as the stock V15 and V16 packages do (once each). Our own app code has no update-mode entry (grep of `firmware/app`: none). So the SDK handler is most likely still there. It lives in the USB receive path, so it should not need the heap the OTA stage may need. No one has tested it on an Official-Open-based app.

## Routes that do not fit oual
- "Two failed boots, then UBOOT" is not unchanged JieLi bootloader behaviour. Quixotic7's troubleshooting says Felucca-family firmware arms a watchdog and a boot-loop guard, and that a half-written firmware gets no fallback from the chip's own SPL. Our app boots fine, so it never triggers.
- "Hold OCT- and OCT+ at power-on" is a Felucca / X0X / MONK-1 app feature (`fm1_rescue.py` lines 402-403; Quixotic7). Absent from our code.
- "Hold REC while switching on" (fm1_rescue.py hint for stock): the Desk reports it failed on this unit's stock V15.
- Hardware fallback if the soft key does nothing: FM-1 Transporter + RP2040 (github.com/kurogedelic/FM-1-transporter; Quixotic7 TRANSPORTER-GUIDE.md; github.com/jvitkauskas/cardputer-synth-rescue; github.com/jvitkauskas/m-unbricker).

## Same symptom elsewhere
- fm1-x0x#46 (UC25HQ80 unit): "the device stopped after 0 requests: nothing was written" while the app ran. There the app did not recognise the newer flash chip; fixed by fm1_rescue on macOS. Less likely here (steps A and B updated this same unit through the vendor loader), but our flash ID is still unread; fm1_rescue prints it, read-only.
- fm1-x0x#55: after an Official Open install the flash head can differ from V15/V16 in isd_config.ini; fm1_rescue accepts exactly that difference, anything else it refuses. Fallback: Quixotic7 `full-restore`.

## Heap (Desk, confirmed in source)
ferluht docs/CHANGELOG.md v0.9.3: the Bluetooth/USB stack allocates from the heap the statics leave: 105,740 B in v0.9/v0.9.3, 40,140 B in the bad v0.9.1/v0.9.2, stock V14 244,972; "not yet known whether even that is enough". Rule added: CI fails any oual build under 105,740 B (run 45331081859 is red at 99,052 B). Quarter-rate reverb would add about 31 KB.

## What is on Charlie's Mac
`~/fm1-rescue`: fm1_rescue.py, FM-1-V15.fwsc, FM-1-V16.fwsc, a venv with pyusb + libusb-package. No python-rtmidi, so it cannot send the soft key; a small Web MIDI page (like the uploader, about 30 lines, typed confirmation) can, with nothing installed. Not built.

## Proposed (each step needs Charlie's OK)
**P1, writes nothing:** start `sudo ~/fm1-rescue/env/bin/python fm1_rescue.py FM-1-V15.fwsc FM-1-V16.fwsc` with NO `--write` (it waits for update mode), then send the soft key. Expect: screen black, the Mac lists WL82 UBOOT1.00, the script prints chip key and flash ID, saves a full-flash backup (the first this unit has) and says it is not writing. Power-cycle the FM-1: it must boot oual again (proves nothing was written). macOS drops UBOOT after about 30 s unless grabbed, so the script runs first. If the soft key does nothing, oual just stays running; go to the Transporter.
**P2, writes, only after P1 worked and Charlie says so:** the same with `--write`: only the app sectors of the matching stock package; head and bootloader never written. Then the normal force-flash flow again.

Unknowns: soft key on oual; flash ID; the 30 s window; sudo on this Mac.
