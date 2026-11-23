# Mario Texture · Mariotronics for FM-1

A 44.1 kHz stereo guitar and tape instrument. Tape is stored at 14.7 kHz in RAM. The two guitar voices share the ADSR, delayed vibrato and pre-tape effects. The stereo plate receives the live guitar and the tape return after recording. Tap REC to record, overdub or hold. Hold REC for 1.2 seconds to erase.

## Panel

EDIT opens Guitar. LFO adjusts rate, depth, wait and rise. SEQ switches vibrato. SEL cycles through Phrasing and Scales. GLO cycles through MIDI and BLE MIDI. On hardware, enable the radio with K1 and confirm the K2 device row with OCT+. Row zero starts a scan. Select a system MIDI input in the browser emulator.

## Build

On Linux x86-64, use the JieLi toolchain and AC79 SDK for the firmware, and clang with wasm-ld for the emulator. Set JIELI_TOOLCHAIN and AC79_SDK before building.

```sh
./build.sh --release 0.2
clang --target=wasm32 -O2 -ffreestanding -nostdlib -fno-builtin -DGRANA_WEB -Ifirmware/src -Ifirmware/gen -Wl,--no-entry -Wl,--export-memory -Wl,-z,stack-size=262144 -Wl,--initial-memory=16777216 -o web/mariotronics.wasm firmware/src/mariotronics.c
python3 web/make_mariotronics.py
```

The FM-1 platform, USB/TRS MIDI and BLE central derive from VIRGO, GRANA, Felucca and Optimist. The guitar excitation and pickup derive from De-Pattern/Ravel_FM1 GUITAR. The Revox A77 display and MTXTronics intro use supplied reference artwork. See LICENSE, LICENSING.md and individual files for credits.

**Off-Tape Monitor.** Press FX until OFF-TAPE appears. K1 sets the level (0–100%, default 25%). In REC and DUB, the first head reads the newly written tape after 180 ms. Its level affects the monitor mix feeding the final plate.
